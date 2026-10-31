#pragma once

#include "test_exchange.hpp"

#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

using hyperliquid::TwapWire;
using hyperliquid_test::TestExchange;

namespace {

const nlohmann::json kTwapRunningResponse = {
    {"status", "ok"},
    {"response", {{"type", "twapOrder"},
                  {"data", {{"status", {{"running", {{"twapId", 77738308}}}}}}}}},
};

// The action hash is keccak(msgpack(action)), so the key order of the twap
// object is load-bearing: a wrong order still serializes to valid JSON but
// signs a different action and the API rejects the signature. Assert the
// literal serialization, not just json equality (which ignores order).

} // namespace
