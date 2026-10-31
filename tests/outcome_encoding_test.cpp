#include "outcomes_fixture.hpp"

int main() {
    // Docs example: outcome 1, side 0 -> #10 / asset 100000010.
    assert(hyperliquid::outcomeCoin(1, 0) == "#10");
    assert(hyperliquid::outcomeAsset(1, 0) == 100000010);
    assert(hyperliquid::outcomeCoin(14200, 1) == "#142001");
    assert(throws([] { hyperliquid::outcomeCoin(1, 2); }));
    assert(throws([] { hyperliquid::outcomeCoin(-1, 0); }));
    assert(throws([] { hyperliquid::outcomeAsset(300000000, 0); }));  // past int

    TestExchange exchange;
    const auto& info = exchange.info_;
    assert(info.nameToAsset("#10") == 100000010);
    assert(info.nameToAsset("#0") == 100000000);
    assert(info.nameToCoin("#142001") == "#142001");
    for (const char* bad : {"#", "#12", "#1a0", "#-10", "#99999999990"}) {
        assert(throws([&] { info.nameToAsset(bad); }));
        assert(throws([&] { info.nameToCoin(bad); }));
    }
}
