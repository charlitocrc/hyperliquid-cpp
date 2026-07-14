#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <cstdlib>
#include <iostream>

// Approve an agent (API) wallet, then trade with it.
//
// An agent can sign orders and cancels on behalf of the account but cannot
// transfer funds. Approval must be signed by the account's own wallet.

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

        // Approve a fresh agent. The private key is generated locally and
        // returned here only — persist it if you want to reuse the agent.
        std::cout << "Approving agent \"example_agent\"...\n";
        auto [result, agent_key] = exchange.approveAgent("example_agent");
        std::cout << result.dump(2) << "\n\n";

        if (result.value("status", "") != "ok") {
            std::cerr << "✗ Approval failed\n";
            return 1;
        }
        std::cout << "✓ Agent approved\n";
        std::cout << "Agent private key (store it securely): " << agent_key << "\n\n";

        // Trade as the agent: its wallet signs, account_address says whose
        // account the actions apply to.
        auto agent_wallet = hyperliquid::Wallet::fromPrivateKey(agent_key);
        std::cout << "Agent address: " << agent_wallet->address() << "\n";

        hyperliquid::Exchange agent_exchange(agent_wallet,
                                             hyperliquid::TESTNET_API_URL,
                                             nullptr,
                                             /*vault_address=*/"",
                                             /*account_address=*/address);
        std::cout << "Agent exchange ready - it can now place orders for "
                  << address << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
