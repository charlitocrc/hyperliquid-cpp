#pragma once

#include "hyperliquid/utils/constants.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
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
 * Unwrap the "data" object of a {"channel":"post"} message into the body the
 * same request would have returned over HTTP.
 *
 * Info and action responses are wrapped differently, verified against mainnet:
 *
 *   info    response.payload is {"type":"l2Book","data":{...}} -- an envelope
 *           the HTTP endpoint does not have, so "data" is what comes back.
 *   action  response.payload is {"status":...,"response":...} already, i.e.
 *           byte-identical to the HTTP /exchange body, and is returned as is.
 *           A rejected action is still a success here: it arrives as
 *           status "err", exactly as over HTTP.
 *
 * @throws ClientError/ServerError for a response of type "error", parsing the
 *         leading HTTP status out of the server's message so that a caller's
 *         catch(ClientError) keeps working after switching transports. Plain
 *         Error when there is no leading status, or the shape is unusable.
 *
 * Exposed for testing; WebSocketManager applies it before fulfilling a post.
 */
nlohmann::json unwrapPostResponse(const nlohmann::json& data);

/**
 * Recover the post id from a {"channel":"error"} message's text.
 *
 * A post the server cannot deserialize is rejected before its id is read, so
 * the error carries no id field -- but the text echoes the request verbatim:
 *
 *   Error parsing JSON into valid websocket request: {"method": "post",
 *   "id": 102, "request": {"type": "info", "payload": {...}}}
 *
 * Scanning the id back out of that echo is what turns a malformed request into
 * an immediate exception rather than a wait for the deadline sweep. Verified
 * against mainnet, but it reads the server's prose rather than a documented
 * field: if the wording changes this returns nullopt and the request falls
 * through to the sweep, which is the same outcome as not scanning at all.
 *
 * @return the first "id": <number> in the text, or nullopt if there is none.
 */
std::optional<int> postIdFromErrorText(const std::string& text);

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

    /**
     * Send an info request or a signed action over the socket instead of HTTP.
     *
     * The payload is the same body the HTTP endpoint takes -- an info request
     * object for "info", or the {action, nonce, signature, ...} envelope for
     * "action" -- and the result is the same body the HTTP endpoint returns.
     * See unwrapPostResponse() for how the two are reconciled.
     *
     * Returns before the server has answered. The future is fulfilled on the
     * websocket thread, so waiting on it from inside a subscription callback
     * deadlocks the connection; wait from any other thread.
     *
     * @param request_type "info" or "action". "explorer" is not supported over
     *                     the socket by the server.
     * @param deadline how long the request may stay outstanding before the ping
     *                 thread fails it. Bounds the pending map when a response
     *                 never arrives, which the server does for a request it
     *                 could not deserialize.
     * @return a future holding the response body, or carrying the exception
     *         raised by unwrapPostResponse() if the server reported an error.
     * @throws Error if the frame could not be handed to the socket at all, i.e.
     *         the connection is down. Distinct from a failed future, which means
     *         the request was sent and the server refused it.
     */
    std::future<nlohmann::json> post(const std::string& request_type,
                                     const nlohmann::json& payload,
                                     std::chrono::milliseconds deadline = std::chrono::seconds(60));

    /**
     * Blocking post(), for use as a drop-in replacement for an HTTP request.
     *
     * @return the response body, or std::nullopt when the socket is not usable
     *         right now -- the signal for a caller to fall back to HTTP. A
     *         request that was actually sent never reports nullopt.
     * @throws Error on timeout. A request that reached the server is not retried
     *         on another transport: re-sending an action that may already have
     *         executed is worse than failing.
     * @throws Error if called on the websocket thread, where waiting for a reply
     *         the same thread has to deliver would hang until the socket drops.
     */
    std::optional<nlohmann::json> tryPost(const std::string& request_type,
                                          const nlohmann::json& payload,
                                          std::chrono::milliseconds timeout);

    /**
     * Block until the socket is open.
     *
     * Only needed when connecting and immediately issuing a request: without it
     * the first few posts find the socket still opening and fall back to HTTP.
     *
     * @return false on timeout or if the manager is stopping.
     */
    bool waitUntilConnected(std::chrono::milliseconds timeout);

    /** True when called from the thread that delivers websocket messages. */
    bool onWebSocketThread() const;

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

    /** One outstanding post(), waiting for the reply carrying its id. */
    struct PendingPost {
        std::promise<nlohmann::json> promise;
        std::chrono::steady_clock::time_point deadline;
    };

    void onOpen();
    void onMessage(const std::string& text);
    void dispatch(const nlohmann::json& message, const std::string& identifier);
    void pingLoop();
    bool send(const nlohmann::json& payload);

    /** Fulfil the post whose id the message carries. No-op if it is unknown. */
    void completePost(const nlohmann::json& data);

    /**
     * Fail the post that an id-less {"channel":"error"} refers to.
     * @return true if a pending request was claimed, so the error is handled
     *         and should not also reach the error callback.
     */
    bool failPostFromErrorText(const std::string& text);

    /** Fail every post past its deadline. Run from the ping thread. */
    void failStalePosts();

    /** Fail every outstanding post, on disconnect and on stop(). */
    void failAllPosts(const std::string& reason);

    std::string ws_url_;
    std::unique_ptr<ix::WebSocket> ws_;

    // Guards active_subscriptions_, id_counter_, and the two loose callbacks.
    // Never held while a user callback runs: a callback calling subscribe()
    // would deadlock on this non-recursive mutex.
    mutable std::mutex mutex_;
    // Ordered so an exact-identifier miss can fall back to a cheap range scan
    // over every identifier sharing the same subscription type.
    std::map<std::string, std::vector<ActiveSubscription>> active_subscriptions_;
    // Outstanding post() requests, keyed by the id sent on the wire. Guarded by
    // mutex_ alongside the subscriptions; both are touched only briefly, and a
    // promise is always moved out before it is fulfilled so a woken caller can
    // take the lock again straight away.
    std::unordered_map<int, PendingPost> pending_posts_;
    // Shared by subscriptions and posts. They live in different maps, so one
    // counter is enough and makes every id unique across the manager.
    int id_counter_ = 0;
    Callback error_callback_;
    Callback unhandled_callback_;

    std::atomic<bool> connected_{false};
    std::atomic<bool> started_{false};
    std::atomic<bool> stopping_{false};
    // Set on the first message; compared against to catch a blocking post()
    // issued from a callback, which would wait for a reply only this thread can
    // deliver. Default-constructed until then, which equals no real thread.
    std::atomic<std::thread::id> ws_thread_id_{};

    std::thread ping_thread_;
    std::mutex ping_mutex_;
    std::condition_variable ping_cv_;

    // Separate from ping_mutex_: waitUntilConnected() must not be woken by the
    // ping timer, and onOpen() must not block behind a ping in flight.
    std::mutex connect_mutex_;
    std::condition_variable connect_cv_;
};

} // namespace hyperliquid
