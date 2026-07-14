#include "test_exchange.hpp"

#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <string>
#include <vector>

using hyperliquid_test::TEST_PRIVATE_KEY;
using hyperliquid_test::TestExchange;

namespace {

constexpr const char* AGENT_ADDRESS = "0x5e9ee1089755c3435139848e47e6635505d5a13a";

hyperliquid::Signature signApproveAgent(const nlohmann::json& action) {
    auto wallet = hyperliquid::Wallet::fromPrivateKey(TEST_PRIVATE_KEY);

    std::vector<hyperliquid::EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"agentAddress", "address"},
        {"agentName", "string"},
        {"nonce", "uint64"}
    };

    return hyperliquid::signUserSignedAction(
        *wallet, action, payload_types,
        "HyperliquidTransaction:ApproveAgent", /*is_mainnet=*/true);
}

// Pin both signatures (unnamed signs agentName as "") to ones produced by the
// Python SDK with the same key and action:
//   sign_agent(wallet, {"type": "approveAgent", "agentAddress": AGENT_ADDRESS,
//       "agentName": <"" | "mybot">, "nonce": 1583838}, is_mainnet=True)
void approveAgentSignatureMatchesPythonSdk() {
    nlohmann::json action = {
        {"type", "approveAgent"},
        {"agentAddress", AGENT_ADDRESS},
        {"agentName", ""},
        {"nonce", 1583838}
    };

    auto unnamed = signApproveAgent(action).toJson();
    assert(unnamed.at("r") == "0x58338707c9a87f3e1aa5530b4b36b61569939f0f5f1de90c7a9cbd640d302484");
    assert(unnamed.at("s") == "0x232ee5d4a03b6f669df7b952b94eb8eee3641731f84893c571d62ea815793fea");
    assert(unnamed.at("v") == 28);

    action["agentName"] = "mybot";
    auto named = signApproveAgent(action).toJson();
    assert(named.at("r") == "0x19cde336459ab1b23c804184218dc5e8fee50dc1132d39e99796b5663ce43a44");
    assert(named.at("s") == "0x5c9109761b06952d58b164a9c377fecc67350769b7ebda3e1332e42fed197239");
    assert(named.at("v") == 28);
}

void approveAgentUnnamedOmitsAgentNameFromWire() {
    TestExchange exchange;

    auto [response, agent_key] = exchange.approveAgent();

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "approveAgent");
    // Signed with agentName "" but the wire action must not carry the field.
    assert(!action.contains("agentName"));
    assert(action.at("nonce").is_number_integer());

    const auto& payload = exchange.lastPayload();
    assert(payload.at("nonce") == action.at("nonce"));
    assert(payload.contains("signature"));

    // The returned key is "0x" + 64 hex chars and derives the approved address.
    assert(agent_key.size() == 66);
    assert(agent_key.substr(0, 2) == "0x");
    auto agent_wallet = hyperliquid::Wallet::fromPrivateKey(agent_key);
    assert(action.at("agentAddress") == agent_wallet->address());
}

void approveAgentNamedSendsAgentName() {
    TestExchange exchange;

    auto [response, agent_key] = exchange.approveAgent("mybot");

    const auto& action = exchange.lastAction();
    assert(action.at("agentName") == "mybot");
}

void approveAgentGeneratesFreshKeys() {
    TestExchange exchange;

    auto [r1, key1] = exchange.approveAgent();
    auto [r2, key2] = exchange.approveAgent();

    assert(key1 != key2);
}

} // namespace

int main() {
    approveAgentSignatureMatchesPythonSdk();
    approveAgentUnnamedOmitsAgentNameFromWire();
    approveAgentNamedSendsAgentName();
    approveAgentGeneratesFreshKeys();

    return 0;
}
