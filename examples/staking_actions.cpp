#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <cstdlib>
#include <iostream>

// Stake HYPE: move it from spot into the staking balance, then delegate it to
// a validator.
//
// Staking is two steps and neither one alone earns anything:
//   spot balance --cDeposit()--> staking balance --tokenDelegate()--> validator
//
// The reverse trip is slow on purpose: undelegating is blocked for 1 day after
// the last delegation to that validator, and cWithdraw() then puts the amount
// through a 7-day unstaking queue before it reappears in spot. Plan around it.
//
// Read-only staking queries live in examples/staking_lending_and_outcomes.cpp.

int main(int argc, char** argv) {
    try {
        const char* private_key = std::getenv("HYPERLIQUID_PRIVATE_KEY");
        if (!private_key) {
            std::cerr << "Error: Set HYPERLIQUID_PRIVATE_KEY environment variable\n";
            std::cerr << "Usage: export HYPERLIQUID_PRIVATE_KEY=\"0x...\"\n";
            return 1;
        }

        // Pass a validator address as argv[1]. Delegating to a jailed or
        // unreliable validator earns nothing, so choose deliberately rather
        // than by advertised yield. A jailed validator produces no rewards
        // for its delegators; the SDK has no validator listing, so check the
        // staking UI or an explorer first.
        const std::string validator =
            argc > 1 ? argv[1] : "0xb8f45222a3246a2b0104696a1df26842007c5bc5";

        auto wallet = hyperliquid::Wallet::fromPrivateKey(private_key);
        std::string address = wallet->address();
        std::cout << "Using address: " << address << "\n\n";

        hyperliquid::Exchange exchange(wallet, hyperliquid::TESTNET_API_URL);

        // Amounts are wei. HYPE has 8 decimals, so 1 HYPE = 100000000 wei --
        // the single easiest thing to get wrong here.
        const uint64_t one_hype = 100000000;

        std::cout << "Before:\n";
        auto before = exchange.info_.userStakingSummary(address);
        std::cout << "  delegated:   " << before.value("delegated", "?") << "\n";
        std::cout << "  undelegated: " << before.value("undelegated", "?") << "\n\n";

        std::cout << "Depositing 1 HYPE from spot into the staking balance...\n";
        auto deposit = exchange.cDeposit(one_hype);
        std::cout << deposit.dump(2) << "\n\n";

        if (deposit.value("status", "") != "ok") {
            std::cerr << "✗ cDeposit failed -- is there 1 HYPE in the spot balance?\n";
            return 1;
        }

        std::cout << "Delegating it to " << validator << "...\n";
        auto delegated = exchange.tokenDelegate(validator, one_hype, /*is_undelegate=*/false);
        std::cout << delegated.dump(2) << "\n\n";

        std::cout << "After:\n";
        auto after = exchange.info_.userStakingSummary(address);
        std::cout << "  delegated:   " << after.value("delegated", "?") << "\n";
        std::cout << "  undelegated: " << after.value("undelegated", "?") << "\n\n";

        for (const auto& delegation : exchange.info_.userStakingDelegations(address)) {
            std::cout << "  " << delegation.value("validator", "?")
                      << "  " << delegation.value("amount", "?")
                      << "  locked until " << delegation["lockedUntilTimestamp"] << "\n";
        }

        // The way back out, left commented because the lockup makes it fail
        // for a day after the delegation above, and the withdrawal then takes
        // a week to land in spot:
        //
        // exchange.tokenDelegate(validator, one_hype, /*is_undelegate=*/true);
        // exchange.cWithdraw(one_hype);

        std::cout << "\n✓ Done\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
