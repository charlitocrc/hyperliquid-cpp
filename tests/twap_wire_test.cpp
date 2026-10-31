#include "twap_orders_fixture.hpp"

namespace {

void twapWireSerializesKeysInSpecOrder() {
    TwapWire twap;
    twap.asset = 0;
    twap.is_buy = true;
    twap.size = "1.5";
    twap.reduce_only = false;
    twap.minutes = 30;
    twap.randomize = false;

    const std::string expected = R"({"a":0,"b":true,"s":"1.5","r":false,"m":30,"t":false})";
    assert(twap.toJson().dump() == expected);
}

} // namespace

int main() {
    twapWireSerializesKeysInSpecOrder();
    return 0;
}
