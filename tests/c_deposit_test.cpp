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

        nlohmann::json deposit = {
            {"type", "cDeposit"}, {"wei", ONE_HYPE}, {"nonce", 1583838}};
        auto deposit_sig = hyperliquid::signUserSignedAction(
            *wallet, deposit, payload_types,
            "HyperliquidTransaction:CDeposit", /*is_mainnet=*/true).toJson();
        assert(deposit_sig.at("r") == "0xdd1352cd82455f8231a1914db0ad672cb5f2e05021e69bfb76fb965c55c055c3");
        assert(deposit_sig.at("s") == "0x12ad67cfff4924b19badfc8714df90090722db848ab0f33c7d515d6d615c1132");
        assert(deposit_sig.at("v") == 27);

    }
    {
        TestExchange exchange;
        exchange.cDeposit(ONE_HYPE);
        assert(exchange.requests().size() == 1);
        const auto& deposit = exchange.requests()[0].payload.at("action");
        assert(exchange.requests()[0].path == "/exchange");
        assert(deposit.at("type") == "cDeposit");
        assert(deposit.at("wei") == ONE_HYPE);
        assert(deposit.at("wei").is_number());  // a number on the wire, not a string
        assert(deposit.at("nonce").is_number_integer());

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
            exchange.cDeposit(0);
        } catch (const std::invalid_argument&) {
            threw = true;
    }
    assert(threw);
    assert(exchange.requests().empty());
    }
}
