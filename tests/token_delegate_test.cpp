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

        nlohmann::json action = {
            {"type", "tokenDelegate"},
            {"validator", VALIDATOR},
            {"wei", ONE_HYPE},
            {"isUndelegate", false},
            {"nonce", 1583838}
        };

        std::vector<hyperliquid::EIP712Type> payload_types = {
            {"hyperliquidChain", "string"},
            {"validator", "address"},
            {"wei", "uint64"},
            {"isUndelegate", "bool"},
            {"nonce", "uint64"}
        };

        auto sig = hyperliquid::signUserSignedAction(
            *wallet, action, payload_types,
            "HyperliquidTransaction:TokenDelegate", /*is_mainnet=*/true);

        const auto json = sig.toJson();
        assert(json.at("r") == "0xec05cea1ef6651967651e74358beb7e81db63c5bd2b2b964a1196382d775488e");
        assert(json.at("s") == "0x0854284ca873049f0b38e641dbcf425bc20ac87399cca2c5367857d412ad2ace");
        assert(json.at("v") == 27);
    }
    {
        TestExchange exchange;

        exchange.tokenDelegate("0xB8F45222A3246A2B0104696A1DF26842007C5BC5", ONE_HYPE, false);

        assert(exchange.lastAction().at("validator") == VALIDATOR);
    }
    {
        TestExchange exchange;
        exchange.tokenDelegate(VALIDATOR, ONE_HYPE, /*is_undelegate=*/true);
        assert(exchange.requests().size() == 1);
        const auto& delegate = exchange.requests()[0].payload.at("action");
        assert(delegate.at("type") == "tokenDelegate");
        assert(delegate.at("validator") == VALIDATOR);
        assert(delegate.at("wei") == ONE_HYPE);
        assert(delegate.at("isUndelegate") == true);

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
            exchange.tokenDelegate(VALIDATOR, 0, false);
        } catch (const std::invalid_argument&) {
            threw = true;
    }
    assert(threw);
    threw = false;
    try {
        exchange.tokenDelegate("0xnope", ONE_HYPE, false);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
    assert(exchange.requests().empty());
    }
}
