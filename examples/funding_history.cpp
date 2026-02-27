#include <hyperliquid/info.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <iostream>
#include <chrono>

int main() {
    const std::string address = "0x0000000000000000000000000000000000000000";
    const std::string coin = "ETH";

    // 30 days ago in milliseconds
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    int64_t start_time = now_ms - (30LL * 24 * 60 * 60 * 1000);

    hyperliquid::Info info(hyperliquid::MAINNET_API_URL, true);

    // --- fundingHistory: historical rates for a coin ---
    std::cout << "=== Funding Rate History for " << coin << " (last 30 days) ===\n\n";
    auto rates = info.fundingHistory(coin, start_time);

    if (rates.is_array() && !rates.empty()) {
        std::cout << "Total records: " << rates.size() << "\n\n";
        std::cout << "Most recent 5 entries:\n";
        int show = std::min((int)rates.size(), 5);
        for (int i = (int)rates.size() - show; i < (int)rates.size(); ++i) {
            const auto& r = rates[i];
            std::cout << "  time=" << r["time"]
                      << "  fundingRate=" << r["fundingRate"].get<std::string>()
                      << "  premium=" << r["premium"].get<std::string>()
                      << "\n";
        }
    } else {
        std::cout << "No funding rate records returned.\n";
    }

    std::cout << "\n";

    // --- userFundingHistory: funding payments for a user ---
    std::cout << "=== User Funding History for " << address << " (last 30 days) ===\n\n";
    auto payments = info.userFundingHistory(address, start_time);

    if (payments.is_array() && !payments.empty()) {
        std::cout << "Total records: " << payments.size() << "\n\n";
        std::cout << "Most recent 5 entries:\n";
        int show = std::min((int)payments.size(), 5);
        for (int i = (int)payments.size() - show; i < (int)payments.size(); ++i) {
            const auto& p = payments[i];
            const auto& delta = p["delta"];
            std::cout << "  time=" << p["time"]
                      << "  coin=" << delta["coin"].get<std::string>()
                      << "  usdc=" << delta["usdc"].get<std::string>()
                      << "  rate=" << delta["fundingRate"].get<std::string>()
                      << "\n";
        }
    } else {
        std::cout << "No user funding records in this period.\n";
    }

    return 0;
}
