// Pins the websocket post envelope: how a reply is unwrapped into the body the
// HTTP endpoint would have returned, and how a rejection becomes an exception.
//
// Both matter because the transport swap is meant to be invisible. If the
// unwrapping is wrong, every method on Info returns a differently shaped object
// the moment useWebsocketTransport() is called, and the caller's parsing breaks
// with no clue that the transport is why. This test is the thing that fails
// instead.
//
// Sample payloads are real responses captured from mainnet, not the docs: the
// docs show an error arriving on the post channel as type "error", which is not
// what the server actually does.

#include "hyperliquid/errors.hpp"
#include "hyperliquid/websocket_manager.hpp"

#include <cassert>
#include <iostream>
#include <string>

using hyperliquid::ClientError;
using hyperliquid::Error;
using hyperliquid::postIdFromErrorText;
using hyperliquid::ServerError;
using hyperliquid::unwrapPostResponse;
using nlohmann::json;

namespace {

int checks_run = 0;

// An info reply carries a {"type","data"} envelope the HTTP endpoint does not.
// What comes back has to be the inner "data", whatever its JSON type.
void testInfoUnwrapping() {
    const json book = json::parse(R"({
        "id": 123,
        "response": {
            "type": "info",
            "payload": {
                "type": "l2Book",
                "data": {"coin": "ETH", "time": 1757000000000,
                         "levels": [[{"px":"3007.1","sz":"2.7954","n":1}],
                                    [{"px":"3040.1","sz":"3.9499","n":1}]]}
            }
        }
    })");
    const json unwrapped = unwrapPostResponse(book);
    assert(unwrapped["coin"] == "ETH");
    assert(unwrapped["levels"][0][0]["px"] == "3007.1");
    assert(!unwrapped.contains("type") && "the info envelope must not survive");
    ++checks_run;

    // candleSnapshot and openOrders wrap an array rather than an object.
    const json candles = json::parse(R"({
        "id": 5,
        "response": {"type": "info",
                     "payload": {"type": "candleSnapshot",
                                 "data": [{"t": 1757000000000, "c": "4321.0"}]}}
    })");
    const json list = unwrapPostResponse(candles);
    assert(list.is_array());
    assert(list[0]["c"] == "4321.0");
    ++checks_run;

    // An unknown coin is not an error: the server answers with a null body,
    // exactly as the HTTP endpoint does.
    const json empty = json::parse(R"({
        "id": 101,
        "response": {"type": "info", "payload": {"type": "l2Book", "data": null}}
    })");
    assert(unwrapPostResponse(empty).is_null());
    ++checks_run;
}

// An action reply is already the HTTP /exchange body. Unwrapping it any further
// would strip the status the caller checks.
void testActionPassthrough() {
    const json resting = json::parse(R"({
        "id": 256,
        "response": {
            "type": "action",
            "payload": {"status": "ok",
                        "response": {"type": "order",
                                     "data": {"statuses": [{"resting": {"oid": 88383}}]}}}
        }
    })");
    const json body = unwrapPostResponse(resting);
    assert(body["status"] == "ok");
    assert(body["response"]["data"]["statuses"][0]["resting"]["oid"] == 88383);
    ++checks_run;

    // A rejected action is a successful response carrying status "err", same as
    // over HTTP. It must not be turned into an exception.
    const json rejected = json::parse(R"({
        "id": 105,
        "response": {"type": "action",
                     "payload": {"status": "err",
                                 "response": "User or API Wallet 0xef1a does not exist."}}
    })");
    const json failed = unwrapPostResponse(rejected);
    assert(failed["status"] == "err");
    assert(failed["response"].is_string());
    ++checks_run;
}

// The status code has to survive so that one catch(ClientError) covers both
// transports.
void testErrorResponses() {
    const json client_error = json::parse(R"({
        "id": 7,
        "response": {"type": "error",
                     "payload": "422 Unprocessable Entity: Failed to deserialize the JSON body"}
    })");
    bool threw = false;
    try {
        unwrapPostResponse(client_error);
    } catch (const ClientError& e) {
        threw = true;
        assert(e.statusCode() == 422);
        assert(e.errorCode() == "Unprocessable Entity");
        assert(std::string(e.what()) == "Failed to deserialize the JSON body");
    }
    assert(threw && "an error response must not come back as a value");
    ++checks_run;

    const json server_error = json::parse(R"({
        "id": 8,
        "response": {"type": "error", "payload": "502 Bad Gateway: upstream is down"}
    })");
    threw = false;
    try {
        unwrapPostResponse(server_error);
    } catch (const ServerError& e) {
        threw = true;
        assert(e.statusCode() == 502);
    }
    assert(threw);
    ++checks_run;

    // No leading status: a plain Error keeping the whole message.
    const json prose = json::parse(R"({
        "id": 9, "response": {"type": "error", "payload": "something went wrong"}
    })");
    threw = false;
    try {
        unwrapPostResponse(prose);
    } catch (const ClientError&) {
        assert(false && "prose must not be read as a status code");
    } catch (const Error& e) {
        threw = true;
        assert(std::string(e.what()) == "something went wrong");
    }
    assert(threw);
    ++checks_run;

    // A body that opens with a number is not a status line.
    threw = false;
    try {
        unwrapPostResponse(json::parse(
            R"({"id": 10, "response": {"type": "error", "payload": "1713825891591 is not a valid nonce"}})"));
    } catch (const ClientError&) {
        assert(false && "a 13-digit nonce is not an HTTP status");
    } catch (const Error&) {
        threw = true;
    }
    assert(threw);
    ++checks_run;
}

// Shapes that should fail loudly rather than return something plausible.
void testMalformedResponses() {
    const char* bad[] = {
        R"({"id": 1})",                                          // no response
        R"({"id": 1, "response": "oops"})",                      // response not an object
        R"({"id": 1, "response": {"type": "info"}})",            // no payload
        R"(["not", "an", "object"])",
    };
    for (const char* text : bad) {
        bool threw = false;
        try {
            unwrapPostResponse(json::parse(text));
        } catch (const Error&) {
            threw = true;
        }
        assert(threw && "a malformed post reply must throw");
        ++checks_run;
    }
}

// The server rejects a request it cannot deserialize with an id-less error that
// quotes the request back. Recovering the id is what fails the right caller
// immediately instead of leaving it to the deadline sweep.
void testOrphanErrorIdRecovery() {
    // Verbatim from mainnet.
    const std::string echoed =
        R"(Error parsing JSON into valid websocket request: {"method": "post", "id": 102, )"
        R"("request": {"type": "info", "payload": {"type": "clearinghouseState", "user": "0xnothex"}}})";
    const auto id = postIdFromErrorText(echoed);
    assert(id.has_value() && *id == 102);
    ++checks_run;

    // Whitespace around the colon is the server's formatting, not a guarantee.
    assert(postIdFromErrorText(R"({"id":7})") == 7);
    assert(postIdFromErrorText("{\"id\"  :\t 7}") == 7);
    ++checks_run;

    // Nothing to recover: these must not claim someone else's request.
    assert(!postIdFromErrorText("Subscription limit reached").has_value());
    assert(!postIdFromErrorText(R"({"id": "abc"})").has_value());
    assert(!postIdFromErrorText(R"({"identifier": 5})").has_value());
    assert(!postIdFromErrorText(R"({"id": 99999999999999999999})").has_value());
    ++checks_run;
}

} // namespace

int main() {
    testInfoUnwrapping();
    testActionPassthrough();
    testErrorResponses();
    testMalformedResponses();
    testOrphanErrorIdRecovery();

    std::cout << "ws_post_test: " << checks_run << " checks passed" << std::endl;
    return 0;
}
