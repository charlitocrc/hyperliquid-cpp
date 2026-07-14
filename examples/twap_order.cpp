#include <hyperliquid/exchange.hpp>
#include <hyperliquid/info.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

// Place a TWAP order, watch its slice fills, then cancel it.
//
// A TWAP splits one large order into suborders executed every 30 seconds over
// the requested duration, each with a maximum slippage of 3%.

int main() {
    try {
        const char* private_key = std::getenv("HYPERLIQUID_PRIVATE_KEY");
        if (!private_key) {
            std::cerr << "Error: Set HYPERLIQUID_PRIVATE_KEY environment variable\n";
            std::cerr << "Usage: export HYPERLIQUID_PRIVATE_KEY=\"0x...\"\n";
            return 1;
        }

        auto wallet = hyperliquid::Wallet::fromPrivateKey(private_key);
        std::string address = wallet->address();
        std::cout << "Using address: " << address << "\n\n";

        hyperliquid::Exchange exchange(wallet, hyperliquid::TESTNET_API_URL);

        const std::string coin = "HYPE";
        const double size = 5.0;
        const int minutes = 30;

        // Spread the buy over 30 minutes (one suborder every 30 seconds).
        std::cout << "Placing TWAP: Buy " << size << " " << coin
                  << " over " << minutes << " minutes\n";

        auto result = exchange.twapOrder(
            coin,
            true,     // is_buy
            size,     // total size across the whole TWAP
            minutes,
            false,    // reduce_only
            false     // randomize suborder timing
        );

        std::cout << "TWAP result:\n" << result.dump(2) << "\n\n";

        // A rejected TWAP still comes back as HTTP 200 with status "ok".
        // The real outcome is nested under response.data.status, which is either
        // {"running": {"twapId": N}} or {"error": "..."}. Check it explicitly.
        auto status = result["response"]["data"]["status"];

        if (status.contains("error")) {
            std::cerr << "✗ TWAP rejected: " << status["error"].get<std::string>() << "\n";
            return 1;
        }

        if (!status.contains("running")) {
            std::cerr << "✗ Unexpected TWAP response shape\n";
            return 1;
        }

        int64_t twap_id = status["running"]["twapId"];
        std::cout << "✓ TWAP running with id: " << twap_id << "\n\n";

        // Let a couple of slices execute. Suborders fire every 30 seconds.
        std::cout << "Waiting 65s for the first slices to fill...\n";
        std::this_thread::sleep_for(std::chrono::seconds(65));

        std::cout << "Fetching TWAP slice fills...\n";
        auto slice_fills = exchange.info_.userTwapSliceFills(address);
        std::cout << "Slice fills:\n" << slice_fills.dump(2) << "\n\n";

        // Cancel the remainder.
        std::cout << "Canceling TWAP id " << twap_id << "...\n";
        auto cancel_result = exchange.twapCancel(coin, twap_id);
        std::cout << "Cancel result:\n" << cancel_result.dump(2) << "\n\n";

        // twapCancel reports failure the same way: status is either the string
        // "success" or an object carrying an error.
        auto cancel_status = cancel_result["response"]["data"]["status"];
        if (cancel_status.is_string() && cancel_status == "success") {
            std::cout << "✓ TWAP canceled\n";
        } else {
            std::cerr << "✗ Cancel failed: " << cancel_status.dump() << "\n";
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
