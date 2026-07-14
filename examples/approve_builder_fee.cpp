#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>

// Approve a maximum builder fee for a builder address, then verify the
// approval with the maxBuilderFee query.
//
// Must be signed by the account's own wallet, not an agent (API) wallet.

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

        const std::string builder = "0x6B41600ce9d883eaE12e2Dd64CdCBeb6dc87c0fb";
        const std::string max_fee_rate = "0.001%";

        std::cout << "Approving builder " << builder
                  << " up to " << max_fee_rate << "...\n";
        auto result = exchange.approveBuilderFee(builder, max_fee_rate);
        std::cout << result.dump(2) << "\n\n";

        if (result.value("status", "") != "ok") {
            std::cerr << "✗ Approval failed\n";
            return 1;
        }
        std::cout << "✓ Builder fee approved\n\n";

        // Verify: query the approved rate back (tenths of a basis point,
        // so 0.001% comes back as 10).
        std::cout << "Querying approved max builder fee...\n";
        auto max_fee = exchange.info_.maxBuilderFee(address, builder);
        std::cout << "maxBuilderFee: " << max_fee.dump() << "\n\n";

        // Orders can now carry the builder fee. BuilderInfo wants the address
        // lowercase and the fee in tenths of a basis point.
        std::string builder_lower = builder;
        std::transform(builder_lower.begin(), builder_lower.end(),
                       builder_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        hyperliquid::BuilderInfo builder_info{builder_lower, 10};
        std::cout << "Example: pass BuilderInfo{\"" << builder_info.b << "\", "
                  << builder_info.f << "} to order() to attach the fee.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
