#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace hyperliquid {

/**
 * Base API client for HTTP communication with Hyperliquid
 */
class API {
public:
    explicit API(const std::string& base_url = "", int timeout_ms = 30000);
    virtual ~API();

protected:
    /**
     * POST request to API endpoint
     */
    nlohmann::json post(const std::string& url_path,
                       const nlohmann::json& payload = nlohmann::json::object());

    std::string base_url_;
    int timeout_ms_;

private:
    void initCurl();
    void cleanupCurl();
    void handleException(long response_code, const std::string& response_body);

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);

    void* curl_handle_;  // CURL* hidden in implementation
};

} // namespace hyperliquid
