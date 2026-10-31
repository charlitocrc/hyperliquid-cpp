#include "l1_action_signing_fixture.hpp"

namespace {

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

} // namespace

int main() {
    noopUsesSuppliedNonce();
    noopWithoutNonceGeneratesOne();
    return 0;
}
