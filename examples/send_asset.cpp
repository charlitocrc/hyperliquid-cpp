#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <cstdlib>
#include <iostream>

// Transfer a token between dexes with sendAsset.
//
// Dex names: "" is the default perp dex, "spot" is spot. When a perp dex is
// involved, the token must be that dex's collateral token.

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

        // Testnet USDC token id; find token ids via info_.spotMeta().
        const std::string token = "USDC:0xeb62eee3685fc4c43992febcd9e75443";
        const double amount = 1.0;

        // Move USDC from the default perp dex to spot, to yourself.
        std::cout << "Sending " << amount << " " << token
                  << " from perp dex to spot...\n";
        auto result = exchange.sendAsset(address, "", "spot", token, amount);
        std::cout << result.dump(2) << "\n";

        if (result.value("status", "") != "ok") {
            std::cerr << "✗ Transfer failed\n";
            return 1;
        }
        std::cout << "✓ Transfer sent\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
