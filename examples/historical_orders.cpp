#include <hyperliquid/info.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <iostream>
#include <unordered_map>

int main() {
    const std::string address = "0x0000000000000000000000000000000000000000";

    hyperliquid::Info info(hyperliquid::MAINNET_API_URL, true);

    std::cout << "=== Historical Orders for " << address << " ===\n\n";

    auto orders = info.historicalOrders(address);

    if (!orders.is_array() || orders.empty()) {
        std::cout << "No historical orders found.\n";
        return 0;
    }

    std::cout << "Total records (max 2000): " << orders.size() << "\n\n";

    // Tally by status
    std::unordered_map<std::string, int> by_status;
    for (const auto& rec : orders) {
        by_status[rec["status"].get<std::string>()]++;
    }
    std::cout << "Breakdown by status:\n";
    for (const auto& [status, count] : by_status) {
        std::cout << "  " << status << ": " << count << "\n";
    }

    // Show most recent 5
    std::cout << "\nMost recent 5 entries:\n";
    int show = std::min((int)orders.size(), 5);
    for (int i = (int)orders.size() - show; i < (int)orders.size(); ++i) {
        const auto& rec = orders[i];
        const auto& ord = rec["order"];
        std::cout << "  oid=" << ord["oid"]
                  << "  coin=" << ord["coin"].get<std::string>()
                  << "  side=" << ord["side"].get<std::string>()
                  << "  sz=" << ord["sz"].get<std::string>()
                  << "  origSz=" << ord["origSz"].get<std::string>()
                  << "  limitPx=" << ord["limitPx"].get<std::string>()
                  << "  type=" << ord["orderType"].get<std::string>()
                  << "  status=" << rec["status"].get<std::string>()
                  << "\n";
    }

    return 0;
}
