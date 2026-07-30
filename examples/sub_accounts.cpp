#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <cstdlib>
#include <iostream>

// Create a sub-account, then move perp USDC into it and back out.
//
// All three actions act on the master account, so they are signed by the
// account's own wallet and ignore any vault/subaccount configured on the
// Exchange. setReferrer() is shown too -- it shares that property, but it is
// commented out because an account can only be referred once, before it has
// traded, so running it here would burn that one chance.

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

        // Set the referral code credited for this account's fees. One shot per
        // account, and only before it has traded.
        //
        // auto referral = exchange.setReferrer("YOURCODE");
        // std::cout << referral.dump(2) << "\n\n";

        std::cout << "Creating sub-account \"example-desk\"...\n";
        auto created = exchange.createSubAccount("example-desk");
        std::cout << created.dump(2) << "\n\n";

        if (created.value("status", "") != "ok") {
            std::cerr << "✗ Sub-account creation failed\n";
            return 1;
        }

        // The response carries the new sub-account's address, which is what
        // the transfer calls address.
        std::string sub_account = created["response"]["data"].get<std::string>();
        std::cout << "✓ Sub-account created: " << sub_account << "\n\n";

        // usd is micro-USDC: 1 USDC = 1000000.
        const int64_t one_usdc = 1000000;

        std::cout << "Depositing 1 USDC into the sub-account...\n";
        auto deposit = exchange.subAccountTransfer(sub_account, true, one_usdc);
        std::cout << deposit.dump(2) << "\n\n";

        std::cout << "Withdrawing it back to the master account...\n";
        auto withdraw = exchange.subAccountTransfer(sub_account, false, one_usdc);
        std::cout << withdraw.dump(2) << "\n\n";

        // The spot counterpart takes a token identifier ("NAME:0x<token id>",
        // as listed by info_.spotMeta()) and a float amount instead of usd.
        //
        // auto spot = exchange.subAccountSpotTransfer(
        //     sub_account, true, "PURR:0xc1fb593aeffbeb02f85e0308e9956a90", 1.5);
        // std::cout << spot.dump(2) << "\n";

        std::cout << "✓ Done\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
