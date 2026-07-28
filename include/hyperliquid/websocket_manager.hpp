#pragma once

#include "hyperliquid/utils/constants.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <nlohmann/json.hpp>

// ix::WebSocket is forward declared so IXWebSocket's headers stay out of the
// SDK's public include surface; consumers link against hyperliquid only.
namespace ix { class WebSocket; }

namespace hyperliquid {

/**
 * Build the routing identifier for an outbound subscription object.
 *
 * The identifier is what a subscription and the messages it produces have in
 * common, e.g. {"type":"l2Book","coin":"ETH"} -> "l2Book:eth". Coin symbols and
 * addresses are lowercased so the two directions always agree.
 *
 * @return std::nullopt if the subscription is malformed, has an unknown "type",
 *         or is missing a field that type needs for routing.
 */
std::optional<std::string> subscriptionToIdentifier(const nlohmann::json& subscription);

/**
 * Build the routing identifier for an inbound websocket message.
 *
 * Mirrors subscriptionToIdentifier() using the same table, reading the routing
 * fields out of msg["data"]. Handles the two channel names that differ from
 * their subscription type: "user" (userEvents) and "activeSpotAssetCtx"
 * (activeAssetCtx).
 *
 * @return std::nullopt for control channels (pong, subscriptionResponse, error)
 *         and for unknown channels. Never throws, whatever the payload shape.
 */
std::optional<std::string> messageToIdentifier(const nlohmann::json& ws_msg);

/**
 * Decode a fastAssetCtxs payload into JSON.
 *
 * fastAssetCtxs is the only subscription whose "data" is not plain JSON: it
 * arrives as base64 wrapping a raw DEFLATE stream (RFC 1951 — no zlib RFC 1950
 * or gzip RFC 1952 header, so zlib needs windowBits = -15). Applied in order:
 * base64 decode, raw inflate, parse as UTF-8 JSON.
 *
 * WebSocketManager already applies this to fastAssetCtxs messages before they
 * reach a callback, so subscribers receive decoded JSON like every other
 * channel. This is exposed for decoding a payload captured elsewhere.
 *
 * @param encoded the base64 text from msg["data"].
 * @return std::nullopt if the input is not valid base64, is not a well-formed
 *         DEFLATE stream, inflates past the size cap, or is not valid JSON.
 *         Network input is untrusted, so every failure is a return value here
 *         rather than an exception.
 */
std::optional<nlohmann::json> decodeFastAssetCtxs(const std::string& encoded);

/**
 * Manages a websocket connection to the Hyperliquid API, routing subscription
 * messages to per-subscription callbacks.
 *
 * Threading contract, read before use:
 *
 *   - Callbacks are invoked on the websocket's network thread, not the thread
 *     that called subscribe().
 *   - Do not block in a callback. A slow callback stalls the socket and the
 *     server will drop the connection after 60s of silence.
 *   - Do not call stop() from inside a callback: it joins the very thread the
 *     callback is running on.
 *   - subscribe() and unsubscribe() are safe to call from a callback and from
 *     any thread.
 *
 * Reconnects are automatic. Every time the socket opens, whether for the first
 * time or after a drop, all active subscriptions are re-sent, because the
 * server keeps no record of them across connections.
 */
class WebSocketManager {
public:
    using Callback = std::function<void(const nlohmann::json&)>;

    /**
     * @param base_url HTTP(S) API URL, e.g. MAINNET_API_URL. Rewritten to the
     *                 websocket URL internally ("https://host" -> "wss://host/ws").
     * @throws Error if base_url is not an http:// or https:// URL.
     */
    explicit WebSocketManager(const std::string& base_url = MAINNET_API_URL);

    /** Calls stop(); safe even if start() was never called. */
    ~WebSocketManager();

    WebSocketManager(const WebSocketManager&) = delete;
    WebSocketManager& operator=(const WebSocketManager&) = delete;

    /**
     * Connect in the background. Returns immediately, before the socket is up.
     * Subscriptions registered before or after this call are both fine.
     * Calling start() on an already-started manager does nothing.
     */
    void start();

    /**
     * Disconnect and join the network and ping threads. Idempotent.
     * Must not be called from inside a callback (see the threading contract).
     */
    void stop();

    /** True once the socket is open, false while disconnected or reconnecting. */
    bool connected() const { return connected_.load(std::memory_order_relaxed); }

    /**
     * Register a subscription and deliver its messages to callback.
     *
     * The subscription object is the wire format from the docs, e.g.
     * {"type":"l2Book","coin":"ETH"}. If the socket is not up yet the
     * subscription is recorded and sent as soon as it connects.
     *
     * @return an id for unsubscribe(). Ids are unique within this manager.
     * @throws Error if callback is null or the subscription is malformed or of
     *         an unknown type. Programmer error, so it fails loudly here rather
     *         than silently never delivering.
     */
    int subscribe(const nlohmann::json& subscription, Callback callback);

    /**
     * Remove one subscription registered by subscribe(). The unsubscribe frame
     * is only sent once the last callback for that feed is gone, since one feed
     * can back several subscriptions.
     *
     * @return true if subscription_id was found and removed.
     */
    bool unsubscribe(const nlohmann::json& subscription, int subscription_id);

    /** Called for {"channel":"error"} messages from the server. */
    void setErrorCallback(Callback callback);

    /**
     * Called for any message that matched no subscription, and for payloads
     * that failed to parse (delivered as {"channel":"parseError","raw":<text>}).
     * Without this, a routing mismatch looks exactly like "no data".
     */
    void setUnhandledCallback(Callback callback);

private:
    struct ActiveSubscription {
        nlohmann::json subscription;  // kept verbatim so it can be re-sent on reconnect
        Callback callback;
        int id;
    };

    void onOpen();
    void onMessage(const std::string& text);
    void dispatch(const nlohmann::json& message, const std::string& identifier);
    void pingLoop();
    void send(const nlohmann::json& payload);

    std::string ws_url_;
    std::unique_ptr<ix::WebSocket> ws_;

    // Guards active_subscriptions_, id_counter_, and the two loose callbacks.
    // Never held while a user callback runs: a callback calling subscribe()
    // would deadlock on this non-recursive mutex.
    mutable std::mutex mutex_;
    // Ordered so an exact-identifier miss can fall back to a cheap range scan
    // over every identifier sharing the same subscription type.
    std::map<std::string, std::vector<ActiveSubscription>> active_subscriptions_;
    int id_counter_ = 0;
    Callback error_callback_;
    Callback unhandled_callback_;

    std::atomic<bool> connected_{false};
    std::atomic<bool> started_{false};
    std::atomic<bool> stopping_{false};

    std::thread ping_thread_;
    std::mutex ping_mutex_;
    std::condition_variable ping_cv_;
};

} // namespace hyperliquid
