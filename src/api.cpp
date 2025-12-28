#include "hyperliquid/api.hpp"
#include "hyperliquid/errors.hpp"
#include "hyperliquid/utils/constants.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <sstream>

namespace hyperliquid {

size_t API::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total_size);
    return total_size;
}

API::API(const std::string& base_url, int timeout_ms)
    : base_url_(base_url.empty() ? MAINNET_API_URL : base_url),
      timeout_ms_(timeout_ms),
      curl_handle_(nullptr) {
    initCurl();
}

API::~API() {
    cleanupCurl();
}

void API::initCurl() {
    curl_handle_ = curl_easy_init();
    if (!curl_handle_) {
        throw std::runtime_error("Failed to initialize libcurl");
    }
}

void API::cleanupCurl() {
    if (curl_handle_) {
        curl_easy_cleanup(static_cast<CURL*>(curl_handle_));
        curl_handle_ = nullptr;
    }
}

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
    CURL* curl = static_cast<CURL*>(curl_handle_);
    std::string response_body;

    std::string url = base_url_ + url_path;
    std::string json_str = payload.dump();

    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // Set POST data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_str.length());

    // Set write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms_));

    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    // Clean up headers
    curl_slist_free_all(headers);

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
