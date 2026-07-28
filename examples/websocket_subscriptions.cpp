// Live websocket subscriptions against the Hyperliquid mainnet feed.
//
// Subscribes to the order book, the trade tape and all mid prices, prints the
// first few messages of each, then shuts down cleanly. No API key needed:
// none of these are user-specific subscriptions.
//
// Build: cmake -S . -B build -DBUILD_EXAMPLES=ON && cmake --build build
// Run:   ./build/examples/websocket_subscriptions

#include "hyperliquid/websocket_manager.hpp"
#include "hyperliquid/ws_types.hpp"
#include "hyperliquid/utils/constants.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

using hyperliquid::WebSocketManager;
using nlohmann::json;

namespace {

// Callbacks run on the websocket thread, so std::cout needs a lock to keep
// lines from interleaving.
std::mutex print_mutex;

void printLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << line << std::endl;
}

} // namespace

int main() {
    WebSocketManager ws(hyperliquid::MAINNET_API_URL);

    std::atomic<int> book_messages{0};
    std::atomic<int> trade_messages{0};
    std::atomic<int> mids_messages{0};
    std::atomic<int> fast_ctx_messages{0};

    ws.setErrorCallback([](const json& message) {
        printLine("[error] " + message.dump());
    });
    ws.setUnhandledCallback([](const json& message) {
        // Worth printing: this is what a routing mismatch looks like, as
        // opposed to no data at all.
        printLine("[unhandled] " + message.dump().substr(0, 200));
    });

    // Raw JSON: the default, and all you need for a couple of fields.
    const json book_subscription{{"type", "l2Book"}, {"coin", "ETH"}};
    const int book_id = ws.subscribe(book_subscription, [&](const json& message) {
        if (++book_messages > 3) {
            return;
        }
        const auto& levels = message["data"]["levels"];
        printLine("[l2Book ETH] best bid " + levels[0][0]["px"].get<std::string>() +
                  "  best ask " + levels[1][0]["px"].get<std::string>());
    });

    // Same feed, read through the typed view in ws_types.hpp instead of raw
    // JSON. Conversion is opt-in and per-message: the callback still receives
    // nlohmann::json, and nothing is converted unless you ask.
    ws.subscribe(json{{"type", "trades"}, {"coin", "ETH"}}, [&](const json& message) {
        if (++trade_messages > 3) {
            return;
        }
        for (const auto& element : message["data"]) {
            const auto trade = element.get<hyperliquid::WsTrade>();
            printLine("[trade " + trade.coin + "] " + trade.side + " " + trade.sz + " @ " +
                      trade.px + "  tid=" + std::to_string(trade.tid));
        }
    });

    ws.subscribe(json{{"type", "allMids"}}, [&](const json& message) {
        if (++mids_messages > 1) {
            return;
        }
        printLine("[allMids] " + std::to_string(message["data"]["mids"].size()) + " markets");
    });

    // fastAssetCtxs arrives base64-wrapped around a raw DEFLATE stream. The
    // manager decodes it before this runs, so "data" is ordinary JSON here.
    // The first message is a full snapshot; later ones carry only changed coins.
    ws.subscribe(json{{"type", "fastAssetCtxs"}}, [&](const json& message) {
        const int seen = ++fast_ctx_messages;
        if (seen > 2) {
            return;
        }
        const auto& coins = message["data"];
        std::string sample;
        for (auto it = coins.begin(); it != coins.end() && sample.size() < 60; ++it) {
            sample += it.key() + "=" + it.value().value("markPx", "?") + " ";
        }
        printLine("[fastAssetCtxs] " + std::string(seen == 1 ? "snapshot" : "update") + ", " +
                  std::to_string(coins.size()) + " coins: " + sample);
    });

    // Subscriptions registered before start() are sent as soon as the socket
    // opens, and re-sent on every reconnect.
    printLine("connecting to " + hyperliquid::MAINNET_API_URL + " ...");
    ws.start();

    std::this_thread::sleep_for(std::chrono::seconds(15));

    // Unsubscribing a feed stops its messages while the others keep flowing.
    ws.unsubscribe(book_subscription, book_id);
    printLine("unsubscribed from l2Book");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    ws.stop();
    printLine("stopped: " + std::to_string(book_messages.load()) + " book, " +
              std::to_string(trade_messages.load()) + " trade, " +
              std::to_string(mids_messages.load()) + " mids, " +
              std::to_string(fast_ctx_messages.load()) + " fastAssetCtxs messages");
    return 0;
}
