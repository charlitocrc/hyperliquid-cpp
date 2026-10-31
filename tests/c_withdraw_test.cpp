#include "test_exchange.hpp"

#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <string>
#include <vector>

using hyperliquid_test::TEST_PRIVATE_KEY;
using hyperliquid_test::TestExchange;

constexpr const char* VALIDATOR = "0xb8f45222a3246a2b0104696a1df26842007c5bc5";
constexpr uint64_t ONE_HYPE = 100000000;  // HYPE has 8 decimals


int main() {
    {
        auto wallet = hyperliquid::Wallet::fromPrivateKey(TEST_PRIVATE_KEY);

        std::vector<hyperliquid::EIP712Type> payload_types = {
            {"hyperliquidChain", "string"},
            {"wei", "uint64"},
            {"nonce", "uint64"}
        };

        nlohmann::json withdraw = {
            {"type", "cWithdraw"}, {"wei", ONE_HYPE}, {"nonce", 1583838}};
        auto withdraw_sig = hyperliquid::signUserSignedAction(
            *wallet, withdraw, payload_types,
            "HyperliquidTransaction:CWithdraw", /*is_mainnet=*/true).toJson();
        assert(withdraw_sig.at("r") == "0x2e689a729096ec76194af44d430300e4f14cb1d35f3a3c1dc9bf633ef43c72bd");
        assert(withdraw_sig.at("s") == "0x34987338c63c01806b039a48e52443b8a098c87b30462aba8fe3d00eb7a4dcdb");
        assert(withdraw_sig.at("v") == 28);
    }
    {
        TestExchange exchange;
        exchange.cWithdraw(250000000);
        assert(exchange.requests().size() == 1);
        const auto& withdraw = exchange.requests()[0].payload.at("action");
        assert(withdraw.at("type") == "cWithdraw");
        assert(withdraw.at("wei") == 250000000);

        // User-signed: the action nonce is the envelope nonce. The envelope still
        // carries a vaultAddress field (null here, with no vault configured), like
        // usdSend and every other user-signed action -- unlike usdClassTransfer
        // and sendAsset, which omit the field entirely. A vault is not part of an
        // EIP-712 payload, so there is nothing here for the signature to cover.
        for (const auto& request : exchange.requests()) {
            assert(request.payload.at("nonce") == request.payload.at("action").at("nonce"));
            assert(request.payload.contains("signature"));
            assert(request.payload.at("vaultAddress").is_null());
    }
    }
    {
        TestExchange exchange;
        bool threw = false;
        try {
            exchange.cWithdraw(0);
        } catch (const std::invalid_argument&) {
            threw = true;
    }
    assert(threw);
    assert(exchange.requests().empty());
    }
}
