#pragma once

// Pins the two actions that move money off the master account: vaultTransfer
// and withdraw3.
//
// Both are money paths with a silent failure mode. vaultTransfer encodes its
// amount as an integer with 6 decimals, so a missing or doubled scaling is a
// factor of a million, and it names its target vault inside the action, so a
// vault configured on the Exchange must not also ride in the envelope -- a
// signature that does not cover the vault it is sent with cannot be verified.
// withdraw3 sends USDC to an Arbitrum address that cannot be taken back.
//
// Reference signatures come from the Python SDK run against the same key and
// action, so a change to the EIP-712 or msgpack encoding fails here rather than
// at the exchange.

#include "test_exchange.hpp"

#include "hyperliquid/utils/conversions.hpp"
#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using hyperliquid::Wallet;
using hyperliquid_test::TEST_PRIVATE_KEY;
using hyperliquid_test::TestExchange;

namespace {

constexpr const char* HLP_VAULT = "0xdfc24b077bc1425ad1dea75bcb6f8158e10df303";
constexpr const char* DESTINATION = "0x1234567890123456789012345678901234567890";

// 5 USD must reach the wire as 5000000, matching the Python SDK example that
// passes 5_000_000 for the same transfer.

} // namespace
