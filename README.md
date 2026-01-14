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
- nlohmann/json (auto-downloaded via CMake)
- msgpack-c (auto-downloaded via CMake)

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

The SDK includes four working examples in the `examples/` directory:

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

#### Transfer Operations

```cpp
// Transfer USD
nlohmann::json usdTransfer(double amount, const std::string& destination);

// Transfer spot tokens
nlohmann::json spotTransfer(double amount,
                           const std::string& destination,
                           const std::string& token);
```

#### Leverage Management

```cpp
nlohmann::json updateLeverage(int leverage,
                             const std::string& coin,
                             bool is_cross = true);
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
```

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

- [Hyperlens.io](https://hyperlens.io) - Hyperliquid data platform with analytics on traders, dexes, builders, and more.

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
