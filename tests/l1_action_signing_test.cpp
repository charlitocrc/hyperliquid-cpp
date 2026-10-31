#include "l1_action_signing_fixture.hpp"

namespace {

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

} // namespace

int main() {
    signatureComponentsAreFixedWidth();
    signatureVectorsAreStable();
    subAccountExclusionDoesNotLeakToOtherActions();
    vaultAddressIsSignedOver();
    expiresAfterIsSignedOver();
    vaultAndExpiresAfterTogether();
    return 0;
}
