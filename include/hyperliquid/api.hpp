#pragma once

#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace hyperliquid {

namespace detail {
/**
 * One HTTP connection to the API host: a libcurl handle, the headers it reuses,
 * and the lock that serializes transfers over it. Opaque on purpose -- nothing
 * outside src/api.cpp needs the curl types.
 *
 * Held by shared_ptr so an Exchange and the Info it owns can point at the same
 * connection instead of opening one each.
 */
class HttpConnection;
}  // namespace detail

/**
 * Base API client for HTTP communication with Hyperliquid.
 *
 * Each API object owns one connection, kept alive between calls: TLS is
 * negotiated once, not per request. Exchange shares its connection with the
 * Info it exposes as info_, so a trading client opens one connection in total.
 *
 * Thread safety: every API object is safe to call from multiple threads -- the
 * connection serializes transfers internally. That makes the obvious usage
 * correct, at the cost of concurrent calls on one object queueing rather than
 * overlapping. For genuinely parallel requests, give each thread its own
 * Info/Exchange; the objects are independent and each gets its own connection.
 */
class API {
public:
    explicit API(const std::string& base_url = "", int timeout_ms = 30000);
    virtual ~API();

    API(const API&) = delete;
    API& operator=(const API&) = delete;

protected:
    /**
     * Adopt an existing connection instead of opening one. A null connection
     * means "open your own", so this doubles as the normal constructor.
     */
    API(const std::string& base_url,
        int timeout_ms,
        std::shared_ptr<detail::HttpConnection> connection);

    /**
     * This object's connection, for handing to a subobject that should share
     * it rather than open a second one.
     */
    const std::shared_ptr<detail::HttpConnection>& connection() const {
        return connection_;
    }

    /**
     * POST request to API endpoint
     */
    virtual nlohmann::json post(const std::string& url_path,
                                const nlohmann::json& payload = nlohmann::json::object());

    std::string base_url_;
    int timeout_ms_;

private:
    void handleException(long response_code, const std::string& response_body);

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);

    std::shared_ptr<detail::HttpConnection> connection_;
};

} // namespace hyperliquid
