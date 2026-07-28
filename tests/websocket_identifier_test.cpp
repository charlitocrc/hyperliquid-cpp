// Pins websocket subscription routing.
//
// Every message the SDK receives is delivered by matching the identifier built
// from the inbound message against the identifier built from the subscription
// that asked for it. If those two ever disagree, the symptom is silence: the
// socket connects, the server streams, and no callback fires. This test is the
// thing that fails instead.
//
// Sample payloads are the wire shapes from the Hyperliquid websocket docs.

#include "hyperliquid/websocket_manager.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using hyperliquid::messageToIdentifier;
using hyperliquid::subscriptionToIdentifier;
using nlohmann::json;

namespace {

int checks_run = 0;

// A subscription and a message that subscription should receive must produce
// the same identifier.
void expectRoundTrip(const json& subscription, const json& message, const std::string& expected) {
    const auto from_subscription = subscriptionToIdentifier(subscription);
    const auto from_message = messageToIdentifier(message);

    assert(from_subscription.has_value() && "subscription produced no identifier");
    assert(from_message.has_value() && "message produced no identifier");
    assert(*from_subscription == expected);
    assert(*from_message == expected);
    ++checks_run;
}

// Types that route on their name alone: only the subscription side has fields.
void expectChannelOnly(const std::string& type, const std::string& channel) {
    const auto from_subscription = subscriptionToIdentifier(
        json{{"type", type}, {"user", "0xABC"}, {"dex", "test"}});
    const auto from_message = messageToIdentifier(
        json{{"channel", channel}, {"data", json::object()}});

    assert(from_subscription.has_value());
    assert(from_message.has_value());
    assert(*from_subscription == type);
    assert(*from_message == type);
    ++checks_run;
}

void testCoinKeyedFeeds() {
    expectRoundTrip(
        json{{"type", "l2Book"}, {"coin", "ETH"}},
        json{{"channel", "l2Book"}, {"data", {{"coin", "ETH"}, {"time", 1}}}},
        "l2Book:eth");

    // trades sends an array; routing reads element 0.
    expectRoundTrip(
        json{{"type", "trades"}, {"coin", "SOL"}},
        json{{"channel", "trades"}, {"data", json::array({{{"coin", "SOL"}, {"px", "100"}}})}},
        "trades:sol");

    expectRoundTrip(
        json{{"type", "bbo"}, {"coin", "BTC"}},
        json{{"channel", "bbo"}, {"data", {{"coin", "BTC"}}}},
        "bbo:btc");

    // candle names the same two fields differently on each side: coin/interval
    // in the subscription, s/i in the message.
    expectRoundTrip(
        json{{"type", "candle"}, {"coin", "ETH"}, {"interval", "1m"}},
        json{{"channel", "candle"}, {"data", {{"s", "ETH"}, {"i", "1m"}}}},
        "candle:eth,1m");

    expectRoundTrip(
        json{{"type", "activeAssetCtx"}, {"coin", "ETH"}},
        json{{"channel", "activeAssetCtx"}, {"data", {{"coin", "ETH"}}}},
        "activeAssetCtx:eth");

    expectRoundTrip(
        json{{"type", "activeAssetData"}, {"coin", "ETH"}, {"user", "0xAbC"}},
        json{{"channel", "activeAssetData"}, {"data", {{"coin", "ETH"}, {"user", "0xabc"}}}},
        "activeAssetData:eth,0xabc");
}

void testUserKeyedFeeds() {
    const std::string user = "0xCd5051944f780a621eE62E39e493c489668aCF4d";
    const std::string lowered = "0xcd5051944f780a621ee62e39e493c489668acf4d";

    // webData3 is deliberately absent: its messages carry no top-level user.
    for (const std::string& type : {std::string("userFills"),
                                    std::string("userFundings"),
                                    std::string("userNonFundingLedgerUpdates")}) {
        expectRoundTrip(
            json{{"type", type}, {"user", user}},
            json{{"channel", type}, {"data", {{"user", lowered}, {"isSnapshot", true}}}},
            type + ":" + lowered);
    }
}

void testChannelOnlyFeeds() {
    expectChannelOnly("allMids", "allMids");
    expectChannelOnly("orderUpdates", "orderUpdates");

    // The docs are explicit: userEvents messages arrive on channel "user".
    expectChannelOnly("userEvents", "user");

    // The 11 types whose payload shape is unverified route on the name alone.
    expectChannelOnly("notification", "notification");
    expectChannelOnly("twapStates", "twapStates");
    expectChannelOnly("clearinghouseState", "clearinghouseState");
    expectChannelOnly("openOrders", "openOrders");
    expectChannelOnly("spotState", "spotState");
    expectChannelOnly("userTwapHistory", "userTwapHistory");
    expectChannelOnly("userTwapSliceFills", "userTwapSliceFills");
    expectChannelOnly("allDexsClearinghouseState", "allDexsClearinghouseState");
    expectChannelOnly("allDexsAssetCtxs", "allDexsAssetCtxs");
    expectChannelOnly("outcomeMetaUpdates", "outcomeMetaUpdates");
    expectChannelOnly("fastAssetCtxs", "fastAssetCtxs");
}

void testCaseInsensitivity() {
    // The user supplies whatever casing they like; the server echoes its own.
    // Both sides lowercase, so they still meet.
    const auto subscription = subscriptionToIdentifier(json{{"type", "l2Book"}, {"coin", "eth"}});
    const auto message = messageToIdentifier(
        json{{"channel", "l2Book"}, {"data", {{"coin", "ETH"}}}});
    assert(subscription.has_value() && message.has_value());
    assert(*subscription == *message);
    ++checks_run;
}

void testSpotCtxChannelAlias() {
    // Spot assets answer on activeSpotAssetCtx but belong to the same feed.
    const auto subscription = subscriptionToIdentifier(
        json{{"type", "activeAssetCtx"}, {"coin", "@107"}});
    const auto message = messageToIdentifier(
        json{{"channel", "activeSpotAssetCtx"}, {"data", {{"coin", "@107"}}}});
    assert(subscription.has_value() && message.has_value());
    assert(*subscription == *message);
    assert(*message == "activeAssetCtx:@107");
    ++checks_run;
}

// Regression: webData3 takes a "user" in its subscription but its messages
// nest the address under "userState" instead of repeating it at the top level.
// Keying it on a flat data["user"] matched nothing and every message was
// dropped, so it routes on the channel name alone.
void testWebData3RoutesOnChannelAlone() {
    const auto from_subscription =
        subscriptionToIdentifier(json{{"type", "webData3"}, {"user", "0xABC"}});
    const auto from_message = messageToIdentifier(json{
        {"channel", "webData3"},
        {"data", {{"perpDexStates", json::array()},
                  {"userState", {{"user", "0xabc"}}}}}});

    assert(from_subscription.has_value() && from_message.has_value());
    assert(*from_subscription == "webData3");
    assert(*from_message == "webData3");
    ++checks_run;
}

// A keyed type whose message omits the routing field must fall back to the bare
// type rather than returning nothing. dispatch() then range-scans every
// subscription of that type, so a wrong guess about a payload over-delivers
// instead of silently dropping.
void testMissingRoutingFieldDegradesToType() {
    const auto no_coin = messageToIdentifier(json{
        {"channel", "l2Book"}, {"data", {{"time", 1}}}});
    assert(no_coin.has_value() && *no_coin == "l2Book");

    const auto no_data = messageToIdentifier(json{{"channel", "l2Book"}});
    assert(no_data.has_value() && *no_data == "l2Book");

    const auto empty_batch = messageToIdentifier(json{
        {"channel", "trades"}, {"data", json::array()}});
    assert(empty_batch.has_value() && *empty_batch == "trades");

    // Wrong shapes degrade the same way instead of throwing.
    const auto bad_shape = messageToIdentifier(json{
        {"channel", "l2Book"}, {"data", "a string"}});
    assert(bad_shape.has_value() && *bad_shape == "l2Book");
    ++checks_run;
}

// Nothing here may throw: this code runs on the network thread, where an
// escaping exception is std::terminate.
void testMalformedInputIsRejectedNotThrown() {
    assert(!subscriptionToIdentifier(json::array()).has_value());
    assert(!subscriptionToIdentifier(json{{"coin", "ETH"}}).has_value());          // no type
    assert(!subscriptionToIdentifier(json{{"type", "nopeNotAType"}}).has_value());
    assert(!subscriptionToIdentifier(json{{"type", "l2Book"}}).has_value());       // no coin
    assert(!subscriptionToIdentifier(json{{"type", 7}}).has_value());

    assert(!messageToIdentifier(json::array()).has_value());
    assert(!messageToIdentifier(json{{"data", json::object()}}).has_value());      // no channel
    assert(!messageToIdentifier(json{{"channel", "pong"}}).has_value());
    assert(!messageToIdentifier(json{{"channel", "subscriptionResponse"}}).has_value());

    // An unknown channel has no type to fall back to, so it stays nullopt.
    assert(!messageToIdentifier(json{{"channel", "notAChannel"},
                                     {"data", json::object()}}).has_value());

    // A known channel with unusable data degrades to the bare type instead of
    // raising json::type_error. See testMissingRoutingFieldDegradesToType.
    assert(*messageToIdentifier(json{{"channel", "trades"},
                                     {"data", json::object()}}) == "trades");
    assert(*messageToIdentifier(json{{"channel", "candle"},
                                     {"data", {{"s", "ETH"}}}}) == "candle");
    ++checks_run;
}

// fastAssetCtxs is the only channel whose data is not plain JSON: base64
// wrapping a raw DEFLATE stream. The docs publish a payload and its expected
// decoding specifically so implementations can check themselves against it.
void testFastAssetCtxsDecoding() {
    const std::string payload =
        "q1ZyCnFWsqpWyk0syg6oULJSsjQ3NTDQM1Wq1VFyDfFAkTI2MzXQMwJLVVRWWfmFuTiiyBuamOoZKdXWAgA=";

    const auto decoded = hyperliquid::decodeFastAssetCtxs(payload);
    assert(decoded.has_value() && "documented fastAssetCtxs payload failed to decode");

    const json expected{
        {"BTC", {{"markPx", "97500.5"}}},
        {"ETH", {{"markPx", "3650.25"}}},
        {"xyz:NVDA", {{"markPx", "145.2"}}},
    };
    assert(*decoded == expected);
    ++checks_run;
}

// Runs on the socket thread against network input, so malformed data has to
// come back as nullopt rather than an exception or a hang.
void testFastAssetCtxsRejectsBadInput() {
    assert(!hyperliquid::decodeFastAssetCtxs("").has_value());
    assert(!hyperliquid::decodeFastAssetCtxs("not!valid!base64!").has_value());
    assert(!hyperliquid::decodeFastAssetCtxs("abc").has_value());       // length not a multiple of 4
    assert(!hyperliquid::decodeFastAssetCtxs("AAAAAAAA").has_value());  // valid base64, not DEFLATE

    // Valid base64 of a truncated DEFLATE stream: the first 8 chars of the
    // documented payload decode cleanly but the stream never terminates.
    assert(!hyperliquid::decodeFastAssetCtxs("q1ZyCnFW").has_value());
    ++checks_run;
}

} // namespace

int main() {
    testFastAssetCtxsDecoding();
    testFastAssetCtxsRejectsBadInput();
    testCoinKeyedFeeds();
    testUserKeyedFeeds();
    testChannelOnlyFeeds();
    testWebData3RoutesOnChannelAlone();
    testMissingRoutingFieldDegradesToType();
    testCaseInsensitivity();
    testSpotCtxChannelAlias();
    testMalformedInputIsRejectedNotThrown();

    std::cout << "websocket identifier routing: " << checks_run << " checks passed\n";
    return 0;
}
