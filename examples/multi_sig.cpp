#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <hyperliquid/utils/conversions.hpp>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Send actions on behalf of a multi-sig account.
//
// The flow has three parties and one rule: every authorized user signs the
// *same* inner action, nonce and leader address, and the leader then sends the
// collected signatures wrapped in a multiSig action.
//
//   1. Each authorized user signs the inner action
//      (signMultiSigL1ActionPayload for L1 actions such as an order,
//       signMultiSigUserSignedActionPayload for user-signed ones such as
//       sendAsset or convertToMultiSigUser).
//   2. The leader -- an authorized user, or an agent of one -- calls
//      exchange.multiSig() with those signatures. Its nonce is the one the
//      exchange consumes, so it is the nonce everyone signed over.
//
// Environment:
//   HYPERLIQUID_PRIVATE_KEY   the leader's key, which sends the action
//   HYPERLIQUID_MULTI_SIG_KEYS comma-separated keys of the authorized users
//   HYPERLIQUID_MULTI_SIG_USER the multi-sig account being acted on
//
// Runs against testnet. The order below is priced far from the market so it
// rests instead of filling; cancel it from the UI when done.

namespace {

std::vector<std::string> splitKeys(const std::string& csv) {
    std::vector<std::string> keys;
    std::stringstream ss(csv);
    std::string key;
    while (std::getline(ss, key, ',')) {
        if (!key.empty()) {
            keys.push_back(key);
        }
    }
    return keys;
}

const char* env(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : "";
}

} // namespace

int main() {
    try {
        const std::string leader_key = env("HYPERLIQUID_PRIVATE_KEY");
        const std::string signer_keys = env("HYPERLIQUID_MULTI_SIG_KEYS");
        const std::string multi_sig_user = env("HYPERLIQUID_MULTI_SIG_USER");

        if (leader_key.empty() || signer_keys.empty() || multi_sig_user.empty()) {
            std::cerr << "Error: set HYPERLIQUID_PRIVATE_KEY, "
                         "HYPERLIQUID_MULTI_SIG_KEYS and HYPERLIQUID_MULTI_SIG_USER\n";
            std::cerr << "Usage: export HYPERLIQUID_MULTI_SIG_KEYS=\"0xkey1,0xkey2\"\n";
            return 1;
        }

        auto leader = hyperliquid::Wallet::fromPrivateKey(leader_key);
        std::cout << "Leader (sends the action): " << leader->address() << "\n";
        std::cout << "Multi-sig account:         " << multi_sig_user << "\n\n";

        hyperliquid::Exchange exchange(leader, hyperliquid::TESTNET_API_URL);
        const bool is_mainnet = false;  // TESTNET_API_URL above

        // Who can sign for this account, and how many signatures it takes.
        // null means the address is not a multi-sig account.
        auto signers = exchange.info_.queryUserToMultiSigSigners(multi_sig_user);
        std::cout << "Authorized signers:\n" << signers.dump(2) << "\n\n";
        if (signers.is_null()) {
            std::cerr << "✗ " << multi_sig_user << " is not a multi-sig account.\n"
                      << "  Convert it first (from the account's own wallet):\n"
                      << "    exchange.convertToMultiSigUser({user1, user2}, 2);\n";
            return 1;
        }

        // ---------------------------------------------------------------
        // 1. An L1 inner action: a resting buy order.
        // ---------------------------------------------------------------
        // Build the wire form through the SDK so the key order matches what the
        // exchange hashes. Rounding is normally done by Exchange::order(); here
        // the action is built by hand, so round explicitly.
        const std::string coin = "ETH";
        const int asset = exchange.info_.nameToAsset(coin);
        const int sz_decimals = exchange.info_.asset_to_sz_decimals_[asset];

        hyperliquid::OrderRequest order;
        order.coin = coin;
        order.is_buy = true;
        order.sz = hyperliquid::roundSize(0.01, sz_decimals);
        order.limit_px = hyperliquid::roundPrice(1000.0, sz_decimals, /*is_spot=*/false);
        order.order_type.limit = hyperliquid::LimitOrderType{"Gtc"};
        order.reduce_only = false;

        auto inner_action = hyperliquid::orderWiresToOrderAction(
            {hyperliquid::orderRequestToOrderWire(order, asset)}, std::nullopt, "na");

        // One nonce for the whole action: every signer signs it, and the leader
        // sends it. Reusing one already consumed gets the action rejected.
        const int64_t nonce = hyperliquid::getTimestampMs();

        std::vector<hyperliquid::Signature> signatures;
        for (const auto& key : splitKeys(signer_keys)) {
            auto signer = hyperliquid::Wallet::fromPrivateKey(key);
            std::cout << "Collecting signature from " << signer->address() << "\n";

            // In practice each authorized user runs this on their own machine
            // and returns only the signature.
            signatures.push_back(hyperliquid::signMultiSigL1ActionPayload(
                *signer,
                inner_action,
                std::nullopt,   // vault: must match the Exchange's, none here
                nonce,
                std::nullopt,   // expires_after: must match exchange.setExpiresAfter()
                multi_sig_user,
                leader->address(),
                is_mainnet));
        }

        std::cout << "\nSending order through multiSig()...\n";
        auto result = exchange.multiSig(multi_sig_user, inner_action, signatures, nonce);
        std::cout << result.dump(2) << "\n\n";

        // ---------------------------------------------------------------
        // 2. A user-signed inner action: sendAsset.
        // ---------------------------------------------------------------
        // Two differences from the L1 case: the action must carry
        // signatureChainId and hyperliquidChain itself (nothing adds them on
        // this path), and the signing types are the action's own, which
        // signMultiSigUserSignedActionPayload() enriches.
        //
        // const int64_t transfer_nonce = hyperliquid::getTimestampMs();
        // nlohmann::ordered_json transfer;
        // transfer["type"] = "sendAsset";
        // transfer["signatureChainId"] = "0x66eee";
        // transfer["hyperliquidChain"] = is_mainnet ? "Mainnet" : "Testnet";
        // transfer["destination"] = "0x...";
        // transfer["sourceDex"] = "";
        // transfer["destinationDex"] = "spot";
        // transfer["token"] = "USDC:0x...";
        // transfer["amount"] = "1";
        // transfer["fromSubAccount"] = "";
        // transfer["nonce"] = transfer_nonce;
        //
        // std::vector<hyperliquid::EIP712Type> send_asset_types = {
        //     {"hyperliquidChain", "string"}, {"destination", "string"},
        //     {"sourceDex", "string"},        {"destinationDex", "string"},
        //     {"token", "string"},            {"amount", "string"},
        //     {"fromSubAccount", "string"},   {"nonce", "uint64"}};
        //
        // std::vector<hyperliquid::Signature> transfer_signatures;
        // for (const auto& key : splitKeys(signer_keys)) {
        //     auto signer = hyperliquid::Wallet::fromPrivateKey(key);
        //     transfer_signatures.push_back(
        //         hyperliquid::signMultiSigUserSignedActionPayload(
        //             *signer, transfer, send_asset_types,
        //             "HyperliquidTransaction:SendAsset",
        //             multi_sig_user, leader->address(), is_mainnet));
        // }
        // exchange.multiSig(multi_sig_user, transfer, transfer_signatures, transfer_nonce);

        // ---------------------------------------------------------------
        // 3. Changing the signer set, or leaving multi-sig entirely.
        // ---------------------------------------------------------------
        // A converted account can no longer send anything on its own, so both
        // travel through multiSig() as a convertToMultiSigUser inner action --
        // signed like case 2, with CONVERT_TO_MULTI_SIG_USER types.
        //
        // New signer set:
        //   convert["signers"] = hyperliquid::convertToMultiSigUserSigners(
        //       {"0xuser1", "0xuser2", "0xuser3"}, 2);
        // Back to a normal user (empty signer set):
        //   convert["signers"] = "null";
        //
        // std::vector<hyperliquid::EIP712Type> convert_types = {
        //     {"hyperliquidChain", "string"}, {"signers", "string"}, {"nonce", "uint64"}};

        std::cout << "✓ Done\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
