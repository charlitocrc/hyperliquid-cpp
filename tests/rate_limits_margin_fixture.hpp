#pragma once

#include "test_exchange.hpp"

#include <cassert>
#include <stdexcept>

using hyperliquid_test::TestExchange;

namespace {

// The action hash is keccak(msgpack(action)), so key order in the serialized
// action is load-bearing. Assert the literal serialization, not just json
// equality (which ignores order).

} // namespace
