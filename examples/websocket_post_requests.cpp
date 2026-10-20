// Sending requests over the websocket instead of HTTP.
//
// Anything that can go to /info or /exchange can go over an open socket
// instead. One round trip is about the same either way -- the HTTP connection
// is kept alive too -- so the gain is not in the single-request latency printed
// below. It is that one socket serves both streaming and requests, that the
// requests carry no HTTP headers, and that several can be in flight at once:
// the HTTP path serializes concurrent calls on one curl handle, the socket does
// not. Two ways to use it, both shown here:
//
//   1. The transport swap. useWebsocketTransport() and every existing method
//      keeps its signature and its return shape, only the wire changes.
//   2. The raw async API, for firing several requests without waiting on each.
//
// Read-only, so no API key is needed.
//
// Build: cmake -S . -B build -DBUILD_EXAMPLES=ON && cmake --build build
// Run:   ./build/examples/websocket_post_requests

#include "hyperliquid/errors.hpp"
#include "hyperliquid/info.hpp"
#include "hyperliquid/utils/constants.hpp"
#include "hyperliquid/websocket_manager.hpp"

#include <chrono>
#include <iostream>
#include <vector>

using hyperliquid::Info;
using nlohmann::json;

int main() {
    Info info(hyperliquid::MAINNET_API_URL);

    // Over HTTP, as always.
    const auto http_start = std::chrono::steady_clock::now();
    json book = info.l2Snapshot("ETH");
    const auto http_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - http_start).count();
    std::cout << "http  l2Snapshot(ETH) best bid " << book["levels"][0][0]["px"]
              << "  (" << http_ms << "ms)\n";

    // One call brings the socket up, waits for it, and switches the transport.
    info.useWebsocketTransport();

    // Identical call, identical return shape. Only the transport changed.
    const auto ws_start = std::chrono::steady_clock::now();
    book = info.l2Snapshot("ETH");
    const auto ws_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - ws_start).count();
    std::cout << "ws    l2Snapshot(ETH) best bid " << book["levels"][0][0]["px"]
              << "  (" << ws_ms << "ms)\n";

    // So does everything else built on the same transport.
    std::cout << "ws    allMids has " << info.allMids().size() << " markets\n";

    // An Exchange would be the same one call, covering its actions and the
    // queries made through its info_, over one shared socket:
    //
    //     Exchange exchange(wallet, MAINNET_API_URL);
    //     exchange.useWebsocketTransport();
    //     exchange.order(...);   // signed and sent over the websocket

    // The raw API, for requests you do not want to wait on one at a time. The
    // transport swap above is blocking because the methods it replaces are.
    auto& ws = *info.websocketTransport();

    std::vector<std::future<json>> inflight;
    for (const char* coin : {"BTC", "ETH", "SOL"}) {
        inflight.push_back(ws.post("info", json{{"type", "l2Book"}, {"coin", coin}}));
    }
    for (auto& pending : inflight) {
        const json response = pending.get();
        std::cout << "async " << response["coin"] << " best bid "
                  << response["levels"][0][0]["px"] << "\n";
    }

    // A rejected request throws the same exception type it would over HTTP, so
    // error handling does not change with the transport either.
    try {
        info.userState("not-an-address");
        std::cout << "unexpected: a malformed address was accepted\n";
    } catch (const hyperliquid::Error& error) {
        std::cout << "error " << error.what() << "\n";
    }

    info.disconnectWebsocket();
    return 0;
}
