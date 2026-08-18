// Regression cover for the L1 action signing path shared by every exchange
// action (order, cancel, modify, leverage, scheduleCancel, twap, ...).
//
// Signing is RFC 6979 deterministic, so for a given (key, action, vault, nonce,
// expiresAfter, is_mainnet) the signature is reproducible. Each case below
// recomputes the expected signature independently and compares it to what the
// Exchange actually sent. That pins every input to signL1Action -- not just the
// action body, but the vault address, the expires-after, and the mainnet flag.
//
// This exists so the shared postL1Action() helper cannot silently drop or
// reorder a signing input.

#include "hyperliquid/exchange.hpp"
#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <string>
#include <vector>

using hyperliquid::AssetInfo;
using hyperliquid::Exchange;
using hyperliquid::Meta;
using hyperliquid::Signature;
using hyperliquid::SpotAssetInfo;
using hyperliquid::SpotMeta;
using hyperliquid::SpotTokenInfo;
using hyperliquid::Wallet;

namespace {

constexpr const char* TEST_PRIVATE_KEY =
    "0x0123456789012345678901234567890123456789012345678901234567890123";

constexpr const char* TEST_VAULT = "0x1234567890abcdef1234567890abcdef12345678";

// Not MAINNET_API_URL, so is_mainnet must be false in the signature.
constexpr const char* TEST_URL = "http://localhost";

struct CapturedRequest {
    std::string path;
    nlohmann::json payload;
};

class TestExchange : public Exchange {
public:
    explicit TestExchange(const std::string& vault_address = "")
        : Exchange(Wallet::fromPrivateKey(TEST_PRIVATE_KEY),
                   TEST_URL,
                   &perpMeta(),
                   vault_address,
                   "",
                   &spotMetaFixture(),
                   &perpDexsFixture(),
                   1000) {}

    const CapturedRequest& lastRequest() const { return requests_.back(); }
    size_t requestCount() const { return requests_.size(); }

protected:
    nlohmann::json post(const std::string& url_path,
                        const nlohmann::json& payload = nlohmann::json::object()) override {
        requests_.push_back({url_path, payload});
        return nlohmann::json{{"status", "ok"}};
    }

private:
    static const Meta& perpMeta() {
        static const Meta meta{{AssetInfo{"BTC", 5}}};
        return meta;
    }

    static const SpotMeta& spotMetaFixture() {
        static const SpotMeta spot_meta{
            {SpotAssetInfo{"PURR/USDC", {1, 0}, 0, true}},
            {
                SpotTokenInfo{"USDC", 8, 8, 0, "0x0", true},
                SpotTokenInfo{"PURR", 0, 5, 1, "0x1", true},
            },
        };
        return spot_meta;
    }

    static const std::vector<std::string>& perpDexsFixture() {
        static const std::vector<std::string> dexs{""};
        return dexs;
    }

    std::vector<CapturedRequest> requests_;
};

// Recompute the signature the Exchange should have produced, from the nonce it
// actually used, and assert the envelope matches in full.
void assertSignedCorrectly(const TestExchange& exchange,
                           const nlohmann::ordered_json& expected_action,
                           const std::string& vault_address,
                           std::optional<int64_t> expires_after) {
    assert(exchange.requestCount() == 1);

    const auto& request = exchange.lastRequest();
    assert(request.path == "/exchange");

    const auto& payload = request.payload;
    assert(payload.at("action") == nlohmann::json(expected_action));

    // The envelope reports the vault and expiry that were signed over.
    if (vault_address.empty()) {
        assert(payload.at("vaultAddress").is_null());
    } else {
        assert(payload.at("vaultAddress") == vault_address);
    }
    if (expires_after.has_value()) {
        assert(payload.at("expiresAfter") == expires_after.value());
    } else {
        assert(payload.at("expiresAfter").is_null());
    }

    const int64_t nonce = payload.at("nonce").get<int64_t>();
    assert(nonce > 0);

    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    std::optional<std::string> vault_opt =
        vault_address.empty() ? std::nullopt : std::optional<std::string>(vault_address);

    const Signature expected = signL1Action(*wallet,
                                            expected_action,
                                            vault_opt,
                                            nonce,
                                            expires_after,
                                            /*is_mainnet=*/false);

    assert(payload.at("signature") == expected.toJson());
}

void cancelSignsExpectedAction() {
    TestExchange exchange;

    exchange.cancel("BTC", 12345);

    nlohmann::ordered_json cancel_obj;
    cancel_obj["a"] = 0;
    cancel_obj["o"] = 12345;

    nlohmann::ordered_json action;
    action["type"] = "cancel";
    action["cancels"] = nlohmann::ordered_json::array({cancel_obj});

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

// Its own action type, with spelled-out field names -- not "cancel" carrying a
// cloid where the oid goes. The exchange types the cancel action's o as a
// uint64, so that form fails to deserialize and no order is ever cancelled.
// Pinned to the Python SDK, which signs the same action as:
//   sign_l1_action(wallet, {"type": "cancelByCloid", "cancels":
//       [{"asset": 0, "cloid": "0x00000000000000000000000000000001"}]},
//       None, 1583838, None, is_mainnet=False)
void cancelByCloidSignsItsOwnAction() {
    TestExchange exchange;

    exchange.cancelByCloid("BTC", hyperliquid::Cloid::fromInt(1));

    const auto& action = exchange.lastRequest().payload.at("action");
    assert(action.at("type") == "cancelByCloid");
    assert(action.at("cancels")[0].at("asset") == 0);
    assert(action.at("cancels")[0].at("cloid") == "0x00000000000000000000000000000001");
    assert(!action.at("cancels")[0].contains("a"));
    assert(!action.at("cancels")[0].contains("o"));

    nlohmann::ordered_json cancel_obj;
    cancel_obj["asset"] = 0;
    cancel_obj["cloid"] = "0x00000000000000000000000000000001";

    nlohmann::ordered_json expected_action;
    expected_action["type"] = "cancelByCloid";
    expected_action["cancels"] = nlohmann::ordered_json::array({cancel_obj});

    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    auto expected = hyperliquid::signL1Action(
        *wallet, expected_action, std::nullopt,
        exchange.lastRequest().payload.at("nonce").get<int64_t>(), std::nullopt,
        /*is_mainnet=*/false);
    assert(exchange.lastRequest().payload.at("signature") == expected.toJson());

    // Same action and nonce as the Python SDK reference above.
    auto pinned = hyperliquid::signL1Action(
        *wallet, expected_action, std::nullopt, 1583838, std::nullopt,
        /*is_mainnet=*/false).toJson();
    assert(pinned.at("r") == "0x923c4ea3515e7bc8243033f91c25dd1b8fb53f4c566179d66b8ab5f80f9fccf7");
    assert(pinned.at("s") == "0x2abfa94e0be12b0dbb0b49f0a1a3643ba74263505e1caa2daaf9a0a476fc381e");
    assert(pinned.at("v") == 27);
}

void updateLeverageSignsExpectedAction() {
    TestExchange exchange;

    exchange.updateLeverage(10, "BTC", true);

    nlohmann::ordered_json action;
    action["type"] = "updateLeverage";
    action["asset"] = 0;
    action["isCross"] = true;
    action["leverage"] = 10;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

void scheduleCancelSignsExpectedAction() {
    TestExchange exchange;

    exchange.scheduleCancel(1700000000000);

    nlohmann::ordered_json action;
    action["type"] = "scheduleCancel";
    action["time"] = 1700000000000;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

// scheduleCancel() with no time omits the field entirely -- a different action
// body, so a different hash.
void scheduleCancelWithoutTimeOmitsField() {
    TestExchange exchange;

    exchange.scheduleCancel();

    nlohmann::ordered_json action;
    action["type"] = "scheduleCancel";

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

void twapOrderSignsExpectedAction() {
    TestExchange exchange;

    exchange.twapOrder("BTC", true, 1.5, 30);

    nlohmann::ordered_json twap;
    twap["a"] = 0;
    twap["b"] = true;
    twap["s"] = "1.5";
    twap["r"] = false;
    twap["m"] = 30;
    twap["t"] = false;

    nlohmann::ordered_json action;
    action["type"] = "twapOrder";
    action["twap"] = twap;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

void twapCancelSignsExpectedAction() {
    TestExchange exchange;

    exchange.twapCancel("BTC", 77738308);

    nlohmann::ordered_json action;
    action["type"] = "twapCancel";
    action["a"] = 0;
    action["t"] = 77738308;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

// The vault address is signed over, not just attached to the envelope. If the
// shared helper ever forgets to forward it, the recomputed signature diverges.
void vaultAddressIsSignedOver() {
    TestExchange exchange(TEST_VAULT);

    exchange.cancel("BTC", 999);

    nlohmann::ordered_json cancel_obj;
    cancel_obj["a"] = 0;
    cancel_obj["o"] = 999;

    nlohmann::ordered_json action;
    action["type"] = "cancel";
    action["cancels"] = nlohmann::ordered_json::array({cancel_obj});

    assertSignedCorrectly(exchange, action, TEST_VAULT, std::nullopt);
}

// Likewise expiresAfter: it feeds the action hash.
void expiresAfterIsSignedOver() {
    TestExchange exchange;
    exchange.setExpiresAfter(1800000000000);

    exchange.updateLeverage(5, "BTC", false);

    nlohmann::ordered_json action;
    action["type"] = "updateLeverage";
    action["asset"] = 0;
    action["isCross"] = false;
    action["leverage"] = 5;

    assertSignedCorrectly(exchange, action, "", 1800000000000);
}

// Both at once, which is the combination most likely to be dropped by a
// refactor that only threads one of them through.
void vaultAndExpiresAfterTogether() {
    TestExchange exchange(TEST_VAULT);
    exchange.setExpiresAfter(1800000000000);

    exchange.twapCancel("BTC", 42);

    nlohmann::ordered_json action;
    action["type"] = "twapCancel";
    action["a"] = 0;
    action["t"] = 42;

    assertSignedCorrectly(exchange, action, TEST_VAULT, 1800000000000);
}

// Exact (r, s, v) output for fixed hashes, captured BEFORE the recovery-id
// optimization replaced calculateRecoveryId()'s brute-force public-key recovery
// with a direct read of kG's y-parity.
//
// The rest of this file recomputes signatures with the same code it is checking,
// so it cannot catch a change in signing output -- both sides would move
// together. These vectors are the only thing pinning v, and v is what decides
// which address the exchange attributes an order to.
//
// Two of each parity: v=28 exercises the low-s negation path that flips the
// recovery id, v=27 does not.
void signatureVectorsAreStable() {
    struct Vector {
        uint8_t counter;  // written into the last byte of an otherwise-zero hash
        const char* r;
        const char* s;
        int v;
    };

    // clang-format off
    static const Vector vectors[] = {
        {0, "0x615a373bb368a656e667bbe6766b8f546f87698567af8c51c2080c7d1b62e868",
            "0x5f14b74c1fccfaea8c1b229e8f437c64a06fad53e4abaf92cc1f6b7391d93ab5", 28},
        {1, "0x09196c9b3816d85e856b124ecd4736a6b09bb968bbe084db1ac86cf5ab37d94b",
            "0x3d5cf1759aff01bd204197ca059e5ad5abb16344bac6fd4b964ea266025b6eac", 27},
        {2, "0x7f27ca3db8056788dcf67186cdf62b13d6d40ac6529c1eb90b9259d5c7e34e0b",
            "0x2cde2a23b7f0481d24ddf3a2a65d4d04f33745d73506834270481dac19f61efb", 28},
        {3, "0x77369e545b9b9b1082e4883eb21889db3b868bc5066b17fc9ed3443cc5999ad9",
            "0x3256c0910bd2d06e7cff611abef1e44ae58d507100dab4dbcdacd34e4278bf33", 27},
    };
    // clang-format on

    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);

    for (const auto& expected : vectors) {
        std::vector<uint8_t> hash(32, 0);
        hash[31] = expected.counter;

        const Signature actual = wallet->signMessage(hash);

        assert(actual.r == expected.r);
        assert(actual.s == expected.s);
        assert(actual.v == expected.v);
    }
}

// noop() is the only action that takes a caller-supplied nonce, which is the
// entire point of it: it burns the nonce of an order still in flight. If the
// override were dropped, postL1Action would substitute a fresh timestamp and
// the call would silently stop racing the order it was aimed at.
//
// assertSignedCorrectly recomputes from whatever nonce the payload carries, so
// it alone cannot catch that -- hence the exact-value assert here.
void noopUsesSuppliedNonce() {
    TestExchange exchange(TEST_VAULT);

    const int64_t target_nonce = 1700000000000;
    exchange.noop(target_nonce);

    assert(exchange.lastRequest().payload.at("nonce").get<int64_t>() == target_nonce);

    nlohmann::ordered_json action;
    action["type"] = "noop";

    // Unlike the subAccount* actions, noop is an ordinary L1 action: a
    // configured vault is signed over and sent.
    assertSignedCorrectly(exchange, action, TEST_VAULT, std::nullopt);
}

// With no argument it falls back to a generated timestamp, so it is usable as
// a plain nonce burn.
void noopWithoutNonceGeneratesOne() {
    TestExchange exchange;

    exchange.noop();

    // Milliseconds since epoch, so comfortably past this bound and nowhere
    // near the sentinel the explicit-nonce case uses.
    const int64_t nonce = exchange.lastRequest().payload.at("nonce").get<int64_t>();
    assert(nonce > 1700000000000);

    nlohmann::ordered_json action;
    action["type"] = "noop";

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

// setReferrer and the subAccount* actions act on the master account. Each is
// driven from an Exchange that HAS a vault configured, but asserted against a
// signature recomputed with no vault -- so if postL1Action ever starts signing
// these with vault_address_, or postAction starts putting it in the envelope,
// these fail.

void setReferrerIgnoresConfiguredVault() {
    TestExchange exchange(TEST_VAULT);

    exchange.setReferrer("HYPERLIQUID");

    nlohmann::ordered_json action;
    action["type"] = "setReferrer";
    action["code"] = "HYPERLIQUID";

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

void createSubAccountIgnoresConfiguredVault() {
    TestExchange exchange(TEST_VAULT);

    exchange.createSubAccount("desk-1");

    nlohmann::ordered_json action;
    action["type"] = "createSubAccount";
    action["name"] = "desk-1";

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

void subAccountTransferIgnoresConfiguredVault() {
    TestExchange exchange(TEST_VAULT);

    exchange.subAccountTransfer(TEST_VAULT, true, 1000000);

    nlohmann::ordered_json action;
    action["type"] = "subAccountTransfer";
    action["subAccountUser"] = TEST_VAULT;
    action["isDeposit"] = true;
    action["usd"] = 1000000;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

void subAccountSpotTransferIgnoresConfiguredVault() {
    TestExchange exchange(TEST_VAULT);

    exchange.subAccountSpotTransfer(TEST_VAULT, false, "PURR:0x1", 1.5);

    nlohmann::ordered_json action;
    action["type"] = "subAccountSpotTransfer";
    action["subAccountUser"] = TEST_VAULT;
    action["isDeposit"] = false;
    action["token"] = "PURR:0x1";
    action["amount"] = "1.5";

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

// The vault exclusion is per action type, not a global switch: an ordinary
// action from the same Exchange must still sign and send the vault.
void subAccountExclusionDoesNotLeakToOtherActions() {
    TestExchange exchange(TEST_VAULT);

    exchange.cancel("BTC", 7);

    nlohmann::ordered_json cancel_obj;
    cancel_obj["a"] = 0;
    cancel_obj["o"] = 7;

    nlohmann::ordered_json action;
    action["type"] = "cancel";
    action["cancels"] = nlohmann::ordered_json::array({cancel_obj});

    assertSignedCorrectly(exchange, action, TEST_VAULT, std::nullopt);
}

// r and s are fixed-width 32-byte quantities. A value whose top byte is zero
// must still serialize to 64 hex chars -- an encoder that strips the leading
// zero yields a 62-char component the API rejects, on roughly 1 signature in
// 256. Sign a fixed sweep of hashes: deterministic signing makes this a fixed
// pass/fail, and 600 hashes cover the leading-zero case several times over.
void signatureComponentsAreFixedWidth() {
    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);

    int leading_zero_cases = 0;

    for (int i = 0; i < 600; ++i) {
        std::vector<uint8_t> hash(32, 0);
        hash[28] = static_cast<uint8_t>(i >> 24);
        hash[29] = static_cast<uint8_t>(i >> 16);
        hash[30] = static_cast<uint8_t>(i >> 8);
        hash[31] = static_cast<uint8_t>(i);

        const Signature sig = wallet->signMessage(hash);

        // "0x" + 64 hex chars.
        assert(sig.r.size() == 66);
        assert(sig.s.size() == 66);

        if (sig.r.compare(2, 2, "00") == 0 || sig.s.compare(2, 2, "00") == 0) {
            leading_zero_cases++;
        }
    }

    // Guards the guard: if the sweep stops producing leading-zero components,
    // the asserts above are no longer exercising the padding path.
    assert(leading_zero_cases > 0);
}

} // namespace

int main() {
    signatureComponentsAreFixedWidth();
    signatureVectorsAreStable();
    noopUsesSuppliedNonce();
    noopWithoutNonceGeneratesOne();
    setReferrerIgnoresConfiguredVault();
    createSubAccountIgnoresConfiguredVault();
    subAccountTransferIgnoresConfiguredVault();
    subAccountSpotTransferIgnoresConfiguredVault();
    subAccountExclusionDoesNotLeakToOtherActions();
    cancelSignsExpectedAction();
    cancelByCloidSignsItsOwnAction();
    updateLeverageSignsExpectedAction();
    scheduleCancelSignsExpectedAction();
    scheduleCancelWithoutTimeOmitsField();
    twapOrderSignsExpectedAction();
    twapCancelSignsExpectedAction();
    vaultAddressIsSignedOver();
    expiresAfterIsSignedOver();
    vaultAndExpiresAfterTogether();

    return 0;
}
