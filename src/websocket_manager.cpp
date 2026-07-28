#include "hyperliquid/websocket_manager.hpp"
#include "hyperliquid/errors.hpp"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <openssl/evp.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <vector>

namespace hyperliquid {
namespace {

using nlohmann::json;

// The server closes any connection it has not sent a message on for 60s and
// expects an application-level {"method":"ping"} as the keepalive, not an
// RFC 6455 ping frame. 50s leaves a margin, matching the Python SDK.
constexpr std::chrono::seconds kPingInterval{50};

// First frame the server sends is this bare string, not JSON.
constexpr std::string_view kHandshakeGreeting = "Websocket connection established.";

/**
 * One row per subscription type: how to build its routing identifier from a
 * subscription object and from an inbound message.
 *
 * Single source of truth for both directions. The Python SDK keeps two parallel
 * if-chains (subscription_to_identifier / ws_msg_to_identifier) that have to
 * agree by hand across 13 types; this table cannot disagree with itself.
 *
 * sub_key/msg_key name the same routing value on each side, which is why they
 * differ for candle (subscription says "coin"/"interval", the message says
 * "s"/"i"). An empty sub_key means the type routes on its name alone.
 */
struct ChannelSpec {
    std::string_view type;      // subscription "type", and the identifier prefix
    std::string_view channel;   // inbound "channel", differs for userEvents
    std::string_view sub_key;   // first routing field, as named in the subscription
    std::string_view msg_key;   // first routing field, as named in msg["data"]
    std::string_view sub_key2;  // second routing field, "" if the type has one
    std::string_view msg_key2;
    bool data_is_array;         // trades sends an array; route on element 0
};

// Only types whose messages are known to echo a routing field carry one. For
// the 11 newer types the docs do not show whether the payload repeats "user" or
// "dex", and guessing wrong drops every message; routing on the type name alone
// can only over-deliver to a second subscriber of the same type, never drop.
//
// ponytail: channel-only routing for the unverified types. Promote one to a
// keyed row by filling in sub_key/msg_key once a live payload confirms the field.
constexpr std::array<ChannelSpec, 24> kChannels{{
    // Routing fields confirmed against the Python SDK's message handler.
    {"l2Book",                      "l2Book",                      "coin", "coin", "",     "",     false},
    {"trades",                      "trades",                      "coin", "coin", "",     "",     true},
    {"candle",                      "candle",                      "coin", "s",    "interval", "i", false},
    {"bbo",                         "bbo",                         "coin", "coin", "",     "",     false},
    {"activeAssetCtx",              "activeAssetCtx",              "coin", "coin", "",     "",     false},
    {"activeAssetData",             "activeAssetData",             "coin", "coin", "user", "user", false},
    {"userFills",                   "userFills",                   "user", "user", "",     "",     false},
    {"userFundings",                "userFundings",                "user", "user", "",     "",     false},
    {"userNonFundingLedgerUpdates", "userNonFundingLedgerUpdates", "user", "user", "",     "",     false},

    // Confirmed to carry no usable routing field.
    // webData3 belongs here despite taking a "user" in its subscription: the
    // message nests the address under "userState" rather than repeating it at
    // the top level, so there is no flat field to key on.
    {"webData3",                    "webData3",                    "", "", "", "", false},
    {"allMids",                     "allMids",                     "", "", "", "", false},
    {"userEvents",                  "user",                        "", "", "", "", false},
    {"orderUpdates",                "orderUpdates",                "", "", "", "", false},

    // Payload shape unverified: route on type name only.
    {"notification",                "notification",                "", "", "", "", false},
    {"twapStates",                  "twapStates",                  "", "", "", "", false},
    {"clearinghouseState",          "clearinghouseState",          "", "", "", "", false},
    {"openOrders",                  "openOrders",                  "", "", "", "", false},
    {"spotState",                   "spotState",                   "", "", "", "", false},
    {"userTwapHistory",             "userTwapHistory",             "", "", "", "", false},
    {"userTwapSliceFills",          "userTwapSliceFills",          "", "", "", "", false},
    {"allDexsClearinghouseState",   "allDexsClearinghouseState",   "", "", "", "", false},
    {"allDexsAssetCtxs",            "allDexsAssetCtxs",            "", "", "", "", false},
    {"outcomeMetaUpdates",          "outcomeMetaUpdates",          "", "", "", "", false},
    {"fastAssetCtxs",               "fastAssetCtxs",               "", "", "", "", false},
}};

const ChannelSpec* specForType(std::string_view type) {
    for (const auto& spec : kChannels) {
        if (spec.type == type) {
            return &spec;
        }
    }
    return nullptr;
}

const ChannelSpec* specForChannel(std::string_view channel) {
    // Spot assets arrive on their own channel but belong to the same feed as
    // the perp ones, exactly as the Python SDK treats them.
    if (channel == "activeSpotAssetCtx") {
        return specForType("activeAssetCtx");
    }
    for (const auto& spec : kChannels) {
        if (spec.channel == channel) {
            return &spec;
        }
    }
    return nullptr;
}

// Coin symbols and hex addresses only, so byte-wise ASCII lowering is correct
// and locale-independent.
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

/**
 * Read one routing field as a lowercased string.
 *
 * Accepts numbers as well as strings because interval-like fields are not
 * guaranteed to be quoted. Returns nullopt when the field is absent or of a
 * shape with no sensible text form, never throws.
 */
std::optional<std::string> routingField(const json& source, std::string_view key) {
    if (!source.is_object()) {
        return std::nullopt;
    }
    const auto it = source.find(std::string(key));
    if (it == source.end()) {
        return std::nullopt;
    }
    if (it->is_string()) {
        return toLower(it->get<std::string>());
    }
    if (it->is_number()) {
        return it->dump();
    }
    return std::nullopt;
}

/**
 * Assemble "type", "type:a" or "type:a,b" from the routing fields found in
 * source. Returns nullopt if a field the type routes on is missing.
 */
std::optional<std::string> buildIdentifier(const ChannelSpec& spec,
                                           const json& source,
                                           std::string_view key1,
                                           std::string_view key2) {
    std::string identifier(spec.type);
    if (key1.empty()) {
        return identifier;
    }

    const auto first = routingField(source, key1);
    if (!first) {
        return std::nullopt;
    }
    identifier.reserve(identifier.size() + first->size() + 2);
    identifier += ':';
    identifier += *first;

    if (key2.empty()) {
        return identifier;
    }

    const auto second = routingField(source, key2);
    if (!second) {
        return std::nullopt;
    }
    identifier += ',';
    identifier += *second;
    return identifier;
}

// A fastAssetCtxs snapshot covers roughly a thousand markets and runs well
// under a megabyte. The cap exists because inflating attacker-controlled bytes
// is otherwise unbounded work; it is generous enough that only a malformed or
// hostile stream can reach it.
constexpr std::size_t kMaxInflatedBytes = 32u * 1024u * 1024u;
constexpr std::size_t kInflateChunkBytes = 64u * 1024u;

/**
 * Standard base64 decode. Returns nullopt on anything that is not well-formed
 * base64, rather than guessing at the intent of malformed input.
 */
std::optional<std::vector<unsigned char>> base64Decode(const std::string& encoded) {
    if (encoded.empty() || encoded.size() % 4 != 0) {
        return std::nullopt;
    }

    std::vector<unsigned char> decoded(encoded.size() / 4 * 3);
    const int written = EVP_DecodeBlock(decoded.data(),
                                        reinterpret_cast<const unsigned char*>(encoded.data()),
                                        static_cast<int>(encoded.size()));
    if (written < 0) {
        return std::nullopt;
    }

    // EVP_DecodeBlock always reports a multiple of three: it decodes '=' padding
    // into trailing zero bytes and does not subtract them. Each '=' in the final
    // quartet stands for one byte that is not part of the payload.
    std::size_t size = static_cast<std::size_t>(written);
    if (encoded[encoded.size() - 1] == '=') {
        --size;
        if (encoded[encoded.size() - 2] == '=') {
            --size;
        }
    }
    decoded.resize(size);
    return decoded;
}

/** Owns a z_stream so inflateEnd() runs on every exit path. */
class InflateStream {
public:
    InflateStream() {
        // windowBits = -15 selects a raw DEFLATE stream (RFC 1951). A positive
        // value would expect a zlib header and 16+ a gzip header; this feed
        // sends neither, so anything else fails immediately with Z_DATA_ERROR.
        initialized_ = inflateInit2(&stream_, -15) == Z_OK;
    }
    ~InflateStream() {
        if (initialized_) {
            inflateEnd(&stream_);
        }
    }
    InflateStream(const InflateStream&) = delete;
    InflateStream& operator=(const InflateStream&) = delete;

    bool ok() const { return initialized_; }
    z_stream& get() { return stream_; }

private:
    z_stream stream_{};
    bool initialized_ = false;
};

/**
 * Inflate a complete raw DEFLATE stream held in memory.
 *
 * @return nullopt if the stream is malformed, truncated, or would exceed
 *         kMaxInflatedBytes.
 */
std::optional<std::string> inflateRaw(const std::vector<unsigned char>& compressed) {
    InflateStream inflater;
    if (!inflater.ok()) {
        return std::nullopt;
    }

    z_stream& stream = inflater.get();
    // zlib's next_in is non-const for historical reasons; it does not write here.
    stream.next_in = const_cast<Bytef*>(compressed.data());
    stream.avail_in = static_cast<uInt>(compressed.size());

    std::string inflated;
    std::vector<char> chunk(kInflateChunkBytes);
    int status = Z_OK;
    do {
        stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());

        // The whole input is already in memory, so Z_FINISH lets zlib decode in
        // one pass, returning Z_BUF_ERROR only when the output chunk fills up.
        status = inflate(&stream, Z_FINISH);
        if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR) {
            return std::nullopt;
        }

        const std::size_t produced = chunk.size() - stream.avail_out;
        if (inflated.size() + produced > kMaxInflatedBytes) {
            return std::nullopt;
        }
        inflated.append(chunk.data(), produced);

        // No output and no progress left to make means a truncated stream;
        // without this the loop would spin forever on Z_BUF_ERROR.
        if (produced == 0 && status == Z_BUF_ERROR) {
            return std::nullopt;
        }
    } while (status != Z_STREAM_END);

    return inflated;
}

} // namespace

std::optional<json> decodeFastAssetCtxs(const std::string& encoded) {
    const auto compressed = base64Decode(encoded);
    if (!compressed) {
        return std::nullopt;
    }
    const auto inflated = inflateRaw(*compressed);
    if (!inflated) {
        return std::nullopt;
    }
    // Not exceptions: a malformed frame from the network is an expected outcome,
    // and this runs on the socket thread where a throw would be fatal.
    json parsed = json::parse(*inflated, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded()) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<std::string> subscriptionToIdentifier(const json& subscription) {
    if (!subscription.is_object()) {
        return std::nullopt;
    }
    const auto type = subscription.find("type");
    if (type == subscription.end() || !type->is_string()) {
        return std::nullopt;
    }
    const ChannelSpec* spec = specForType(type->get<std::string>());
    if (!spec) {
        return std::nullopt;
    }
    return buildIdentifier(*spec, subscription, spec->sub_key, spec->sub_key2);
}

std::optional<std::string> messageToIdentifier(const json& ws_msg) {
    if (!ws_msg.is_object()) {
        return std::nullopt;
    }
    const auto channel = ws_msg.find("channel");
    if (channel == ws_msg.end() || !channel->is_string()) {
        return std::nullopt;
    }
    const ChannelSpec* spec = specForChannel(channel->get<std::string>());
    if (!spec) {
        return std::nullopt;
    }
    const std::string bare_type(spec->type);
    if (spec->sub_key.empty()) {
        return bare_type;
    }

    const auto data = ws_msg.find("data");
    if (data == ws_msg.end()) {
        return bare_type;
    }

    // Degrading to the bare type rather than returning nullopt is what keeps a
    // wrong guess about a payload's routing field from silently dropping the
    // message: dispatch() then range-scans every subscription of this type.
    // webData3 was exactly this case — it carries no top-level "user" (the
    // address is nested under "userState"), so a keyed lookup found nobody.
    if (spec->data_is_array) {
        // An empty batch carries no coin, so there is nothing to route on.
        if (!data->is_array() || data->empty()) {
            return bare_type;
        }
        return buildIdentifier(*spec, data->front(), spec->msg_key, spec->msg_key2)
            .value_or(bare_type);
    }
    return buildIdentifier(*spec, *data, spec->msg_key, spec->msg_key2).value_or(bare_type);
}

WebSocketManager::WebSocketManager(const std::string& base_url) {
    if (base_url.rfind("http", 0) != 0) {
        throw Error("WebSocketManager requires an http:// or https:// base URL, got: " + base_url);
    }
    // "https://host" -> "wss://host/ws", "http://host" -> "ws://host/ws".
    ws_url_ = "ws" + base_url.substr(4) + "/ws";

    // No-op outside Windows, where it initialises Winsock. Once per process.
    static const bool net_initialized = ix::initNetSystem();
    (void)net_initialized;

    ws_ = std::make_unique<ix::WebSocket>();
    ws_->setUrl(ws_url_);
    ws_->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        switch (msg->type) {
        case ix::WebSocketMessageType::Open:
            onOpen();
            break;
        case ix::WebSocketMessageType::Close:
        case ix::WebSocketMessageType::Error:
            connected_.store(false, std::memory_order_relaxed);
            break;
        case ix::WebSocketMessageType::Message:
            onMessage(msg->str);
            break;
        default:
            break;  // Ping/Pong/Fragment are handled by the library.
        }
    });
}

WebSocketManager::~WebSocketManager() {
    stop();
}

void WebSocketManager::start() {
    if (started_.exchange(true)) {
        return;
    }
    ws_->start();  // spawns IXWebSocket's network thread, reconnects on its own
    ping_thread_ = std::thread(&WebSocketManager::pingLoop, this);
}

void WebSocketManager::stop() {
    if (stopping_.exchange(true)) {
        return;
    }
    ping_cv_.notify_all();
    if (ping_thread_.joinable()) {
        ping_thread_.join();
    }
    ws_->stop();
    connected_.store(false, std::memory_order_relaxed);
}

void WebSocketManager::pingLoop() {
    std::unique_lock<std::mutex> lock(ping_mutex_);
    // wait_for returns true only when the predicate holds, i.e. on shutdown;
    // a plain timeout means it is time for another ping.
    while (!ping_cv_.wait_for(lock, kPingInterval, [this] {
        return stopping_.load(std::memory_order_relaxed);
    })) {
        if (connected()) {
            send(json{{"method", "ping"}});
        }
    }
}

void WebSocketManager::send(const json& payload) {
    ws_->send(payload.dump());  // ix::WebSocket::send is thread-safe
}

void WebSocketManager::onOpen() {
    connected_.store(true, std::memory_order_relaxed);

    // The server keeps no record of subscriptions across connections, so an
    // open is always a full re-subscribe. This is why there is no separate
    // "queued before connect" list: first connect and reconnect are one path.
    std::vector<json> to_send;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [identifier, subscriptions] : active_subscriptions_) {
            if (!subscriptions.empty()) {
                to_send.push_back(subscriptions.front().subscription);
            }
        }
    }
    for (const auto& subscription : to_send) {
        send(json{{"method", "subscribe"}, {"subscription", subscription}});
    }
}

void WebSocketManager::onMessage(const std::string& text) {
    // This runs on IXWebSocket's thread. An exception escaping here is
    // std::terminate, so nothing below is allowed to propagate.
    try {
        if (text == kHandshakeGreeting) {
            return;
        }

        json message;
        try {
            message = json::parse(text);
        } catch (const json::parse_error&) {
            Callback unhandled;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                unhandled = unhandled_callback_;
            }
            if (unhandled) {
                unhandled(json{{"channel", "parseError"}, {"raw", text}});
            }
            return;
        }

        const std::string channel = message.value("channel", std::string{});
        if (channel == "pong" || channel == "subscriptionResponse") {
            return;
        }
        if (channel == "error") {
            Callback on_error;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                on_error = error_callback_;
            }
            if (on_error) {
                on_error(message);
            }
            return;
        }

        // fastAssetCtxs is the one channel whose data is not plain JSON. Decode
        // it here so subscribers see the same shape as every other channel.
        // contains() first: operator[] on a missing key would insert a null.
        if (channel == "fastAssetCtxs" && message.contains("data") && message["data"].is_string()) {
            auto decoded = decodeFastAssetCtxs(message["data"].get<std::string>());
            if (!decoded) {
                // Keep the undecodable payload rather than dropping it, so the
                // unhandled callback can show what actually arrived.
                dispatch(message, std::string{});
                return;
            }
            message["data"] = std::move(*decoded);
        }

        const auto identifier = messageToIdentifier(message);
        if (!identifier) {
            dispatch(message, std::string{});
            return;
        }
        dispatch(message, *identifier);
    } catch (...) {
        // A user callback threw, or a payload had a shape nothing anticipated.
        // Dropping one message beats killing the caller's process.
    }
}

void WebSocketManager::dispatch(const json& message, const std::string& identifier) {
    std::vector<Callback> targets;
    Callback unhandled;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        unhandled = unhandled_callback_;

        if (!identifier.empty()) {
            const auto exact = active_subscriptions_.find(identifier);
            if (exact != active_subscriptions_.end()) {
                for (const auto& subscription : exact->second) {
                    targets.push_back(subscription.callback);
                }
            }

            // Exact miss: the message did not carry the routing field we keyed
            // the subscription on. Fall back to every subscription of the same
            // type rather than dropping. Identifiers are "type" or "type:...",
            // and ':' < ';', so one range covers both. Types are a closed set,
            // so no other type can share the prefix.
            if (targets.empty()) {
                const std::string type = identifier.substr(0, identifier.find(':'));
                const auto end = active_subscriptions_.lower_bound(type + ';');
                for (auto it = active_subscriptions_.lower_bound(type); it != end; ++it) {
                    for (const auto& subscription : it->second) {
                        targets.push_back(subscription.callback);
                    }
                }
            }
        }
    }

    if (targets.empty()) {
        if (unhandled) {
            unhandled(message);
        }
        return;
    }
    // Callbacks run with the lock released so one can call subscribe() without
    // deadlocking on this thread.
    for (const auto& callback : targets) {
        callback(message);
    }
}

int WebSocketManager::subscribe(const json& subscription, Callback callback) {
    if (!callback) {
        throw Error("subscribe() requires a non-null callback");
    }
    const auto identifier = subscriptionToIdentifier(subscription);
    if (!identifier) {
        throw Error("unsupported or malformed subscription: " + subscription.dump());
    }

    int subscription_id = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        subscription_id = ++id_counter_;
        active_subscriptions_[*identifier].push_back(
            ActiveSubscription{subscription, std::move(callback), subscription_id});
    }

    // Not connected yet is fine: onOpen() re-sends everything registered.
    if (connected()) {
        send(json{{"method", "subscribe"}, {"subscription", subscription}});
    }
    return subscription_id;
}

bool WebSocketManager::unsubscribe(const json& subscription, int subscription_id) {
    const auto identifier = subscriptionToIdentifier(subscription);
    if (!identifier) {
        return false;
    }

    bool removed = false;
    bool feed_now_empty = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto feed = active_subscriptions_.find(*identifier);
        if (feed == active_subscriptions_.end()) {
            return false;
        }
        auto& subscriptions = feed->second;
        const auto match = std::find_if(subscriptions.begin(), subscriptions.end(),
                                        [subscription_id](const ActiveSubscription& active) {
                                            return active.id == subscription_id;
                                        });
        if (match != subscriptions.end()) {
            subscriptions.erase(match);
            removed = true;
        }
        if (subscriptions.empty()) {
            active_subscriptions_.erase(feed);
            feed_now_empty = true;
        }
    }

    // Several subscriptions can share one feed, so only tell the server to stop
    // once the last of them is gone.
    if (removed && feed_now_empty && connected()) {
        send(json{{"method", "unsubscribe"}, {"subscription", subscription}});
    }
    return removed;
}

void WebSocketManager::setErrorCallback(Callback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_callback_ = std::move(callback);
}

void WebSocketManager::setUnhandledCallback(Callback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    unhandled_callback_ = std::move(callback);
}

} // namespace hyperliquid
