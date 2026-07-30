#include "hyperliquid/api.hpp"
#include "hyperliquid/errors.hpp"
#include "hyperliquid/utils/constants.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <sstream>

namespace hyperliquid {

namespace detail {

// See api.hpp. Owns the curl handle, the header list it reuses, and the lock
// that keeps concurrent callers from interleaving transfers on one handle.
class HttpConnection {
public:
    HttpConnection() {
        handle_ = curl_easy_init();
        if (!handle_) {
            throw std::runtime_error("Failed to initialize libcurl");
        }

        // Built once and kept: every request sends the same headers, and
        // rebuilding the list per call allocates for no reason.
        headers_ = curl_slist_append(nullptr, "Content-Type: application/json");
        curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, headers_);

        // Without this, curl may use signals for timeouts, which is unsafe in a
        // multi-threaded process. Required for the thread-safety guarantee.
        curl_easy_setopt(handle_, CURLOPT_NOSIGNAL, 1L);

        // Keep the connection alive across idle gaps. A trading client can sit
        // quiet between actions, and a load balancer or NAT that drops an idle
        // connection costs a full TLS handshake (~52ms measured) on the next
        // request. Probe well before the 60s idle timeouts commonly deployed.
        curl_easy_setopt(handle_, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(handle_, CURLOPT_TCP_KEEPIDLE, 30L);
        curl_easy_setopt(handle_, CURLOPT_TCP_KEEPINTVL, 15L);

        // Advertise the encodings this build of curl can decode. Metadata
        // responses (spotMeta, l2Book) are large and compress well; curl
        // decompresses transparently, so callers see no difference.
        curl_easy_setopt(handle_, CURLOPT_ACCEPT_ENCODING, "");
    }

    ~HttpConnection() {
        if (handle_) {
            curl_easy_cleanup(handle_);
        }
        if (headers_) {
            curl_slist_free_all(headers_);
        }
    }

    HttpConnection(const HttpConnection&) = delete;
    HttpConnection& operator=(const HttpConnection&) = delete;

    CURL* handle() { return handle_; }
    std::mutex& mutex() { return mutex_; }

private:
    CURL* handle_ = nullptr;
    curl_slist* headers_ = nullptr;
    std::mutex mutex_;
};

}  // namespace detail

size_t API::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total_size);
    return total_size;
}

API::API(const std::string& base_url, int timeout_ms)
    : API(base_url, timeout_ms, nullptr) {
}

API::API(const std::string& base_url,
         int timeout_ms,
         std::shared_ptr<detail::HttpConnection> connection)
    : base_url_(base_url.empty() ? MAINNET_API_URL : base_url),
      timeout_ms_(timeout_ms),
      connection_(connection ? std::move(connection)
                             : std::make_shared<detail::HttpConnection>()) {
}

// Out of line so HttpConnection only has to be complete here.
API::~API() = default;

void API::handleException(long response_code, const std::string& response_body) {
    if (response_code >= 200 && response_code < 300) {
        return;  // Success
    }

    // Try to parse JSON error response
    try {
        auto json_response = nlohmann::json::parse(response_body);

        if (response_code >= 400 && response_code < 500) {
            // Client error
            std::string error_code = json_response.value("error", "Unknown");
            std::string error_message = json_response.value("message", response_body);
            std::string error_data = json_response.value("data", "");
            throw ClientError(response_code, error_code, error_message, error_data);
        } else if (response_code >= 500) {
            // Server error
            std::string error_message = json_response.value("message", response_body);
            throw ServerError(response_code, error_message);
        }
    } catch (const nlohmann::json::parse_error&) {
        // Not JSON, use raw response body
        if (response_code >= 400 && response_code < 500) {
            throw ClientError(response_code, "ParseError", response_body);
        } else if (response_code >= 500) {
            throw ServerError(response_code, response_body);
        }
    }
}

nlohmann::json API::post(const std::string& url_path, const nlohmann::json& payload) {
    const std::string url = base_url_ + url_path;
    const std::string json_str = payload.dump();
    std::string response_body;

    // Held for the whole exchange, not just the perform: the per-request
    // options below live on the shared handle, so another thread setting its
    // own URL between our setopt and our perform would send our body to its
    // endpoint.
    std::lock_guard<std::mutex> lock(connection_->mutex());
    CURL* curl = connection_->handle();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_str.length()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms_));

    // curl defaults to a 300s connect timeout, which turns an unreachable host
    // into a hang. Cap it, but never above the caller's overall timeout.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                     static_cast<long>(std::min(timeout_ms_, 10000)));

    const CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::string error_msg = "HTTP request failed: ";
        error_msg += curl_easy_strerror(res);
        throw std::runtime_error(error_msg);
    }

    // Get response code
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    // Handle errors
    handleException(response_code, response_body);

    // Parse and return JSON
    try {
        return nlohmann::json::parse(response_body);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error(std::string("Failed to parse JSON response: ") + e.what());
    }
}

} // namespace hyperliquid
