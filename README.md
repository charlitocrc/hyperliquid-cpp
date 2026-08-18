# Hyperliquid C++ SDK

A C++ SDK for interacting with the Hyperliquid decentralized exchange, supporting core trading features including order placement, cancellation, position management, and market data queries.

**New to this SDK?** → See [QUICKSTART.md](QUICKSTART.md) for a quick 2-minute setup guide.

## Features

- ✅ Place and cancel limit orders
- ✅ Execute market orders with slippage protection
- ✅ Modify existing orders
- ✅ Bulk order operations
- ✅ Query positions and account state
- ✅ Transfer USD and spot tokens
- ✅ Leverage management
- ✅ Multi-signature accounts
- ✅ Staking, borrow/lend and prediction-market queries
- ✅ Full EIP-712 signing support
- ✅ Testnet and Mainnet support

## Quick Start for Developers

### 1. Install Dependencies

**macOS:**
```bash
brew install curl openssl@3 cmake
```

**Ubuntu/Debian:**
```bash
sudo apt-get install -y libcurl4-openssl-dev libssl-dev cmake build-essential
```

### 2. Build the Library

```bash
make build
```

### 3. Compile Your Code

**Easy way** - Use the compile script:
```bash
./compile.sh your_file.cpp
./your_file
```

**Or** add your file to `examples/` and use CMake:
```bash
# Move your file to examples/
mv your_file.cpp examples/

# Add to examples/CMakeLists.txt, then:
cd build
make your_file
```

### 4. Run Examples

```bash
make examples
export HYPERLIQUID_PRIVATE_KEY="0x..."
./build/examples/basic_order
```

## Requirements

- C++17 or later
- CMake 3.15+
- libcurl
- OpenSSL 3.0+
- zlib (websocket builds only, for `fastAssetCtxs` payloads)
- nlohmann/json (auto-downloaded via CMake)
- msgpack-c (auto-downloaded via CMake)
- IXWebSocket (auto-downloaded via CMake; disable with `-DBUILD_WEBSOCKET=OFF`)

## Installation

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y libcurl4-openssl-dev libssl-dev cmake build-essential
```

### macOS

```bash
brew install curl openssl@3 cmake
```

### Building from Source

```bash
# Clone or navigate to the repository
cd hyperliquid-cpp

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
make -j$(nproc)

# Optional: Install system-wide
sudo make install
```

## Usage Guide

### Writing Your First Bot

Create a file `my_bot.cpp`:

```cpp
#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <iostream>
#include <cstdlib>

int main() {
    const char* key = std::getenv("HYPERLIQUID_PRIVATE_KEY");
    if (!key) {
        std::cerr << "Set HYPERLIQUID_PRIVATE_KEY\n";
        return 1;
    }

    auto wallet = hyperliquid::Wallet::fromPrivateKey(key);
    hyperliquid::Exchange exchange(wallet, hyperliquid::TESTNET_API_URL);

    // Your trading logic here
    auto result = exchange.marketOpen("ETH", true, 0.1);
    std::cout << result.dump(2) << "\n";

    return 0;
}
```

Compile and run:
```bash
./compile.sh my_bot.cpp
export HYPERLIQUID_PRIVATE_KEY="0x..."
./my_bot
```

### Basic Code Examples

```cpp
#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>

int main() {
    // Create wallet from private key
    auto wallet = hyperliquid::Wallet::fromPrivateKey("0x...");

    // Create exchange client (testnet)
    hyperliquid::Exchange exchange(wallet, hyperliquid::TESTNET_API_URL);

    // Place a limit order
    hyperliquid::OrderType order_type;
    order_type.limit = hyperliquid::LimitOrderType{"Gtc"};

    auto result = exchange.order(
        "ETH",      // coin
        true,       // is_buy
        0.2,        // size
        1100.0,     // limit_px
        order_type,
        false       // reduce_only
    );

    return 0;
}
```

### Environment Setup

Set your private key as an environment variable:

```bash
export HYPERLIQUID_PRIVATE_KEY="0x..."
```

## Examples

Working examples live in the `examples/` directory. The main ones:

### 1. Basic Order (`basic_order.cpp`)

Place and cancel a limit order:

```bash
export HYPERLIQUID_PRIVATE_KEY="0x..."
./build/examples/basic_order
```

### 2. Market Order (`basic_market_order.cpp`)

Execute a market order and close the position:

```bash
./build/examples/basic_market_order
```

### 3. Query Positions (`query_positions.cpp`)

Query positions and open orders for any address:

```bash
./build/examples/query_positions 0x1234567890abcdef...
```

### 4. Bulk Orders (`bulk_orders.cpp`)

Place and cancel multiple orders in a single request:

```bash
./build/examples/bulk_orders
```

### 5. TWAP Order (`twap_order.cpp`)

Place a TWAP, watch its slice fills, then cancel the remainder. Note this example
sleeps 65 seconds to let the first suborders execute:

```bash
./build/examples/twap_order
```

Other examples cover funding history, historical orders, user fees, candles, account
queries, and perp deployment queries.

## API Documentation

### Exchange Class

#### Order Operations

```cpp
// Place single order
nlohmann::json order(
    const std::string& coin,
    bool is_buy,
    double sz,
    double limit_px,
    const OrderType& order_type,
    bool reduce_only = false,
    const std::optional<Cloid>& cloid = std::nullopt,
    const std::optional<BuilderInfo>& builder = std::nullopt
);

// Place multiple orders
nlohmann::json bulkOrders(
    const std::vector<OrderRequest>& orders,
    const std::optional<BuilderInfo>& builder = std::nullopt,
    const std::string& grouping = "na"
);

// Market orders
nlohmann::json marketOpen(const std::string& coin, bool is_buy, double sz,
                         std::optional<double> px = std::nullopt,
                         double slippage = 0.05);

nlohmann::json marketClose(const std::string& coin,
                          std::optional<double> sz = std::nullopt);
```

#### Cancel Operations

```cpp
// Cancel by OID
nlohmann::json cancel(const std::string& coin, int64_t oid);

// Cancel by Client Order ID
// Cancel by client order id. Its own action ("cancelByCloid"), not the plain
// cancel action with a cloid in the oid field.
nlohmann::json cancelByCloid(const std::string& coin, const Cloid& cloid);

// Bulk cancel
nlohmann::json bulkCancel(const std::vector<CancelRequest>& cancels);
```

#### Modify Operations

```cpp
nlohmann::json modifyOrder(const OidOrCloid& oid,
                          const std::string& coin,
                          bool is_buy,
                          double sz,
                          double limit_px,
                          const OrderType& order_type,
                          bool reduce_only = false);

nlohmann::json bulkModifyOrders(const std::vector<ModifyRequest>& modifies);
```

#### TWAP Operations

A TWAP splits one large order into suborders executed every 30 seconds over the
requested duration, each with a maximum slippage of 3%.

```cpp
// Place a TWAP: spread the order over `minutes`
nlohmann::json twapOrder(const std::string& coin,
                        bool is_buy,
                        double sz,          // total size across the whole TWAP
                        int minutes,        // duration to spread it over
                        bool reduce_only = false,
                        bool randomize = false);  // randomize suborder timing

// Cancel a running TWAP by its id
nlohmann::json twapCancel(const std::string& coin, int64_t twap_id);
```

`sz` is rounded to the asset's `szDecimals`, same as `order()`.

**Checking the result is not optional.** A rejected TWAP still returns HTTP 200 with
`"status": "ok"` at the top level — the real outcome lives in `response.data.status`,
which is either `{"running": {"twapId": N}}` or `{"error": "..."}`:

```cpp
auto result = exchange.twapOrder("ETH", true, 0.1, 30);

auto status = result["response"]["data"]["status"];
if (status.contains("error")) {
    std::cerr << "TWAP rejected: " << status["error"].get<std::string>() << "\n";
    return 1;
}

int64_t twap_id = status["running"]["twapId"];

// Watch it execute
auto fills = exchange.info_.userTwapSliceFills(address);

// Cancel the remainder. Failure surfaces the same way.
auto cancel_result = exchange.twapCancel("ETH", twap_id);
auto cancel_status = cancel_result["response"]["data"]["status"];
bool ok = cancel_status.is_string() && cancel_status == "success";
```

Valid TWAP durations are enforced by the API, not by the SDK — an out-of-range
duration comes back as `"Invalid TWAP duration: N min(s)"`.

#### Transfer Operations

```cpp
// Transfer USD
nlohmann::json usdTransfer(double amount, const std::string& destination);

// Transfer spot tokens
nlohmann::json spotTransfer(double amount,
                           const std::string& destination,
                           const std::string& token);

// Move USDC between your own spot and perp balances
// (to_perp=true: spot -> perp, false: perp -> spot)
nlohmann::json usdClassTransfer(double amount, bool to_perp);

// Transfer a token between dexes ("" = default perp dex, "spot" = spot).
// With a perp dex involved, token must be that dex's collateral token.
nlohmann::json sendAsset(const std::string& destination,
                        const std::string& source_dex,
                        const std::string& destination_dex,
                        const std::string& token,
                        double amount);
```

#### Agents

```cpp
// Approve an agent (API) wallet that can sign orders/cancels for this account
// but cannot transfer funds. Generates the agent key locally and returns it
// alongside the response - persist it, it is shown nowhere else.
// Must be signed by the account's own wallet.
std::pair<nlohmann::json, std::string> approveAgent(
    const std::optional<std::string>& name = std::nullopt);

// Later: trade as the agent
auto [result, agent_key] = exchange.approveAgent("mybot");
auto agent_wallet = Wallet::fromPrivateKey(agent_key);
Exchange agent_exchange(agent_wallet, MAINNET_API_URL, nullptr, "",
                        /*account_address=*/master_address);
```

#### Builder Fees

```cpp
// Approve a max builder fee rate (percent string) for a builder address.
// Must be signed by the account's own wallet, not an agent wallet.
// Verify with info_.maxBuilderFee(user, builder).
nlohmann::json approveBuilderFee(const std::string& builder,
                                const std::string& max_fee_rate);  // e.g. "0.001%"
```

#### Leverage & Margin Management

```cpp
nlohmann::json updateLeverage(int leverage,
                             const std::string& coin,
                             bool is_cross = true);

// Add (positive) or remove (negative) a fixed USDC amount of isolated margin
nlohmann::json updateIsolatedMargin(double amount, const std::string& coin);

// Top up an isolated position so its effective leverage drops to the target.
// The exchange computes the margin to add; this never removes margin.
nlohmann::json topUpIsolatedOnlyMargin(const std::string& coin, double leverage);
```

#### Multi-Signature

A multi-sig account's actions must all arrive wrapped in `multiSig()`, carrying
signatures from at least `threshold` of its authorized users. The wallet that
sends it is the leader: it must itself be an authorized user (or an agent of
one), and its nonce is the one the exchange consumes.

```cpp
// Convert an account to multi-sig. Signed by the account's own wallet;
// agent wallets are rejected, and every authorized user must already exist
// on Hyperliquid. At most 10 users, threshold in [1, users].
nlohmann::json convertToMultiSigUser(const std::vector<std::string>& authorized_users,
                                    int threshold);

// Send an action on behalf of a multi-sig account.
nlohmann::json multiSig(const std::string& multi_sig_user,
                       const nlohmann::ordered_json& inner_action,
                       const std::vector<Signature>& signatures,
                       int64_t nonce);
```

Every signer signs the *same* inner action, nonce and leader address. The
signing entry point depends on the inner action:

```cpp
// One nonce for the whole action - everyone signs it, the leader sends it.
int64_t nonce = getTimestampMs();

auto inner = orderWiresToOrderAction({orderRequestToOrderWire(order, asset)},
                                     std::nullopt, "na");

std::vector<Signature> signatures;
for (auto& signer : authorized_wallets) {
    // L1 actions (order, cancel, updateLeverage, ...)
    signatures.push_back(signMultiSigL1ActionPayload(
        *signer, inner, /*vault=*/std::nullopt, nonce, /*expires_after=*/std::nullopt,
        multi_sig_user, leader->address(), is_mainnet));

    // User-signed actions (sendAsset, convertToMultiSigUser, ...) instead use
    // signMultiSigUserSignedActionPayload(). Those inner actions must carry
    // their own "signatureChainId" and "hyperliquidChain" - nothing adds them
    // on this path.
}

exchange.multiSig(multi_sig_user, inner, signatures, nonce);
```

`inner_action` is an `ordered_json` because its key order feeds the signed
msgpack hash. Build it with `orderWiresToOrderAction()` or in the order the
exchange uses. The nonce, vault and `expiresAfter` must match across every
signer and the leader.

Inspect an account's signer set with
`info_.queryUserToMultiSigSigners(address)`, which returns `null` for an
address that is not multi-sig. Changing the signer set, or converting back to
a normal user, also goes through `multiSig()` — see `examples/multi_sig.cpp`.

Note: converting to multi-sig does not move the HyperEVM side of the account,
which stays controlled by the original key. Multi-sig accounts should not
interact with HyperEVM.

#### Rate Limits

```cpp
// Buy extra request weight instead of being throttled.
// Check current usage with exchange.info_.userRateLimit(address).
nlohmann::json reserveRequestWeight(int64_t weight);
```

### Info Class

```cpp
// User state (positions, margin)
nlohmann::json userState(const std::string& address, const std::string& dex = "");

// Open orders
nlohmann::json openOrders(const std::string& address, const std::string& dex = "");

// All mid prices
nlohmann::json allMids(const std::string& dex = "");

// User fills
nlohmann::json userFills(const std::string& address);

// Perpetuals metadata
Meta meta(const std::string& dex = "");

// Spot metadata
SpotMeta spotMeta();

// Order book snapshot
nlohmann::json l2Snapshot(const std::string& name);

// Query order
nlohmann::json queryOrderByOid(const std::string& user, int64_t oid);

// Multi-sig signer set, or null if the address is not a multi-sig account
nlohmann::json queryUserToMultiSigSigners(const std::string& multi_sig_user);

// Metadata for every perp dex at once, index-aligned with perpDexs().
// Note: returns bare metas, not the [meta, ctxs] pairs the docs show.
nlohmann::json allPerpMetas();

// Leverage, max trade size and available balance for one user and coin (perps
// only). maxTradeSzs and availableToTrade are [buy, sell] pairs.
nlohmann::json activeAssetData(const std::string& user, const std::string& coin);

// Agent (API) wallets approved for a user. validUntil is null when the agent
// does not expire, despite the docs typing it as an int.
nlohmann::json extraAgents(const std::string& user);
```

#### Staking, Borrow/Lend and Outcomes

Read-only so far — the matching exchange actions (`cDeposit`, `tokenDelegate`,
borrow/lend operations, `userOutcome`) are not implemented yet. Amounts come
back as decimal strings throughout.

```cpp
// Staking. The wire types are named for the question rather than the answer,
// so these send "delegatorSummary", "delegations" and "delegatorRewards".
nlohmann::json userStakingSummary(const std::string& user);
nlohmann::json userStakingDelegations(const std::string& user);
nlohmann::json userStakingRewards(const std::string& user);
nlohmann::json delegatorHistory(const std::string& user);

// Borrow/lend. token is a numeric token index (0 is USDC); an ltv of "0.0"
// means the token is not accepted as collateral.
nlohmann::json borrowLendUserState(const std::string& user);
nlohmann::json borrowLendReserveState(int64_t token);
nlohmann::json allBorrowLendReserveStates();

// Prediction markets. settledOutcome returns null while an outcome is still
// open, and also for an index that does not exist.
nlohmann::json outcomeMeta();
nlohmann::json settledOutcome(int64_t outcome);

// Spot deploy auctions. spotDeployState also reports the deployments the given
// deployer has in flight.
nlohmann::json spotDeployState(const std::string& user);
nlohmann::json spotPairDeployAuctionStatus();
```

Run `./build/examples/staking_lending_and_outcomes [address]` for a live tour
of all of these — no private key needed.

### Wallet Class

```cpp
// Create wallet from private key
static std::shared_ptr<Wallet> fromPrivateKey(const std::string& private_key_hex);

// Get Ethereum address
std::string address() const;

// Sign message
Signature signMessage(const std::vector<uint8_t>& message_hash) const;
```

## Order Types

### Limit Orders

```cpp
// Good Til Canceled
OrderType gtc;
gtc.limit = LimitOrderType{"Gtc"};

// Immediate or Cancel
OrderType ioc;
ioc.limit = LimitOrderType{"Ioc"};

// Add Liquidity Only (post-only)
OrderType alo;
alo.limit = LimitOrderType{"Alo"};
```

### Trigger Orders

```cpp
OrderType trigger;
trigger.trigger = TriggerOrderType{
    .trigger_px = 2000.0,
    .is_market = true,
    .tpsl = "tp"  // "tp" for take profit, "sl" for stop loss
};
```

### TWAP Orders

TWAP is not an `OrderType` — it is a separate action with its own method. See
[TWAP Operations](#twap-operations).

```cpp
// Buy 0.1 ETH spread over 30 minutes
exchange.twapOrder("ETH", true, 0.1, 30);
```

## Asset IDs

The SDK automatically handles asset ID mapping:

- **Perpetuals**: 0-9999 (e.g., BTC = 0, ETH = 1)
- **Spot**: 10000+ (e.g., PURR/USDC = 10000)
- **Builder Perps**: 110000+ (e.g., test:ABC = 110000)

## Error Handling

```cpp
try {
    auto result = exchange.order(...);
    if (result["status"] == "ok") {
        // Success
    }
} catch (const hyperliquid::ClientError& e) {
    // 4xx client error
    std::cerr << "Client error: " << e.what() << "\n";
    std::cerr << "Code: " << e.errorCode() << "\n";
} catch (const hyperliquid::ServerError& e) {
    // 5xx server error
    std::cerr << "Server error: " << e.what() << "\n";
} catch (const std::exception& e) {
    // Other errors
    std::cerr << "Error: " << e.what() << "\n";
}
```

## Architecture

The SDK is organized into several layers:

### Cryptography Layer
- **Keccak-256**: OpenSSL-based hashing
- **ECDSA**: secp256k1 signing and key management
- **EIP-712**: Typed data encoding and signing

### Signing Layer
- Action hash computation (msgpack + nonce + vault + expires)
- L1 action signing (orders, cancels)
- User-signed action signing (transfers)

### API Layer
- **API**: Base HTTP client using libcurl
- **Info**: Read-only market data queries
- **Exchange**: Trading operations

### Utilities
- Float-to-wire conversion (8-decimal precision)
- Hex/bytes conversion
- Timestamp generation

## Testing

The SDK has been designed to work with both testnet and mainnet:

```cpp
// Testnet
hyperliquid::Exchange exchange(wallet, hyperliquid::TESTNET_API_URL);

// Mainnet
hyperliquid::Exchange exchange(wallet, hyperliquid::MAINNET_API_URL);
// or simply
hyperliquid::Exchange exchange(wallet);  // defaults to mainnet
```

### Running the unit tests

Tests mock the HTTP layer, so they need no network and no private key:

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build
cd build && ctest --output-on-failure
```

Tests assert with `assert()`, and the build defaults to `Release` (which defines
`NDEBUG` and would compile every assert away). Test targets therefore force
`-UNDEBUG` in `tests/CMakeLists.txt` — keep that on any new test target, or the
test will pass no matter what it claims to check.

## Troubleshooting

### Build Issues

**OpenSSL not found:**
```bash
# Ubuntu/Debian
sudo apt-get install libssl-dev

# macOS
brew install openssl@3
export OPENSSL_ROOT_DIR=/usr/local/opt/openssl@3
```

**libcurl not found:**
```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev

# macOS
brew install curl
```

### Runtime Issues

**"KECCAK-256 not available":**
- Ensure OpenSSL 3.0+ is installed
- Check OpenSSL version: `openssl version`

**"Invalid private key":**
- Ensure private key starts with "0x" or is 64 hex characters
- Private key should be 32 bytes (64 hex chars)

**"User or API Wallet does not exist":**
- Signature verification failed
- Check that the private key matches the address
- Ensure correct network (mainnet vs testnet)

## Performance Considerations

- **Nonce Management**: Uses millisecond timestamps for nonces
- **Connection Pooling**: libcurl reuses connections for efficiency
- **Batch Operations**: Use bulk methods for multiple orders
- **Metadata Caching**: Info class caches coin-to-asset mappings


## Resources

- [Hyperliquid Documentation](https://hyperliquid.gitbook.io/hyperliquid-docs)
- [Official Python SDK](https://github.com/hyperliquid-dex/hyperliquid-python-sdk)
- [API Documentation](https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api)

## Sponsors

- [Hyperlens.io](https://hyperlens.io) - Hyperliquid data platform with actionable analytics on traders, dexes, builders, and more.

## Contributing

Contributions are welcome! Please ensure:
- Code follows C++17 standards
- Examples compile without warnings
- All tests pass on both testnet and mainnet
- Documentation is updated for new features

## Support

For issues and questions:
- Check the [official Hyperliquid docs](https://hyperliquid.gitbook.io/hyperliquid-docs)
- Review example code in `examples/` directory
- Test on testnet before mainnet

---

**Disclaimer**: This SDK is for educational and development purposes. Always test thoroughly on testnet before using real funds on mainnet. Trading carries risk.
