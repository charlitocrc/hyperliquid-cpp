#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <cstdlib>
#include <iostream>

// Move USDC between the spot and perp balances of the same account.

int main() {
    try {
        const char* private_key = std::getenv("HYPERLIQUID_PRIVATE_KEY");
        if (!private_key) {
            std::cerr << "Error: Set HYPERLIQUID_PRIVATE_KEY environment variable\n";
            std::cerr << "Usage: export HYPERLIQUID_PRIVATE_KEY=\"0x...\"\n";
            return 1;
        }

        auto wallet = hyperliquid::Wallet::fromPrivateKey(private_key);
        std::cout << "Using address: " << wallet->address() << "\n\n";

        hyperliquid::Exchange exchange(wallet, hyperliquid::TESTNET_API_URL);

        const double amount = 1.0;

        // Spot -> perp.
        std::cout << "Transferring " << amount << " USDC from spot to perp...\n";
        auto result = exchange.usdClassTransfer(amount, /*to_perp=*/true);
        std::cout << result.dump(2) << "\n\n";

        if (result.value("status", "") != "ok") {
            std::cerr << "✗ Transfer failed\n";
            return 1;
        }

        // And back: perp -> spot.
        std::cout << "Transferring " << amount << " USDC from perp to spot...\n";
        auto back = exchange.usdClassTransfer(amount, /*to_perp=*/false);
        std::cout << back.dump(2) << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
