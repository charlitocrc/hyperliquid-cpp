#include "multi_sig_fixture.hpp"

namespace {

void convertToMultiSigUserPostsSortedSigners() {
    TestExchange exchange;

    exchange.convertToMultiSigUser({"0x0000000000000000000000000000000000000001",
                                    "0x0000000000000000000000000000000000000000"},
                                   2);

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "convertToMultiSigUser");
    assert(action.at("signers") ==
           "{\"authorizedUsers\": [\"0x0000000000000000000000000000000000000000\", "
           "\"0x0000000000000000000000000000000000000001\"], \"threshold\": 2}");
    assert(action.at("nonce").is_number_integer());

    const auto& payload = exchange.lastPayload();
    assert(payload.at("nonce") == action.at("nonce"));
    assert(payload.contains("signature"));
}

} // namespace

int main() {
    convertToMultiSigUserPostsSortedSigners();
    return 0;
}
