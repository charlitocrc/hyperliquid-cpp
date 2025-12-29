# Quick Start Guide

## Setup (One Time)

```bash
# Install dependencies
brew install curl openssl@3 cmake  # macOS
# OR
sudo apt-get install libcurl4-openssl-dev libssl-dev cmake build-essential  # Linux

# Build the library
make build
```

## Compile Your Code

### Method 1: Use the compile script (Easiest)

```bash
# Create your trading bot
./compile.sh your_bot.cpp

# Run it
export HYPERLIQUID_PRIVATE_KEY="0x..."
./your_bot
```

### Method 2: Add to examples/ and use CMake

```bash
# 1. Move your file to examples/
mv your_bot.cpp examples/

# 2. Edit examples/CMakeLists.txt and add "your_bot" to the EXAMPLES list

# 3. Build
cd build
make your_bot

# 4. Run
./examples/your_bot
```

## Common Commands

```bash
make help           # Show all available commands
make build          # Build the library
make examples       # Build all examples
make clean          # Clean build directory
make rebuild        # Clean and rebuild everything

./compile.sh file.cpp [output_name]  # Compile any .cpp file
```

## Quick Examples

### Market Order
```cpp
#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <cstdlib>
#include <iostream>

int main() {
    auto wallet = hyperliquid::Wallet::fromPrivateKey(
        std::getenv("HYPERLIQUID_PRIVATE_KEY"));

    hyperliquid::Exchange exchange(wallet, hyperliquid::TESTNET_API_URL);

    // Buy 0.1 ETH at market
    auto result = exchange.marketOpen("ETH", true, 0.1);
    std::cout << result.dump(2) << "\n";

    return 0;
}
```

### Limit Order
```cpp
hyperliquid::OrderType order_type;
order_type.limit = hyperliquid::LimitOrderType{"Gtc"};

auto result = exchange.order(
    "BTC",      // coin
    true,       // buy
    0.01,       // size
    50000.0,    // price
    order_type,
    false       // reduce_only
);
```

### Query Positions
```cpp
#include <hyperliquid/info.hpp>

int main() {
    hyperliquid::Info info(hyperliquid::TESTNET_API_URL);

    auto state = info.userState("0xYourAddress");
    std::cout << state.dump(2) << "\n";

    return 0;
}
```

## Environment Variables

```bash
# Required for trading operations
export HYPERLIQUID_PRIVATE_KEY="0x1234..."

# Optional: Use testnet
# (or specify hyperliquid::TESTNET_API_URL in code)
```

## File Structure

```
hyperliquid-cpp/
├── compile.sh           # Quick compile script
├── Makefile            # Common tasks
├── order.cpp           # Your custom files go here
├── examples/           # Example programs
│   ├── basic_order.cpp
│   ├── basic_market_order.cpp
│   ├── query_positions.cpp
│   └── bulk_orders.cpp
├── include/            # Header files
│   └── hyperliquid/
└── build/              # Compiled binaries
    ├── libhyperliquid.a
    └── examples/
```

## Troubleshooting

**Error: "library 'ssl' not found"**
- macOS: `brew install openssl@3`
- Linux: `sudo apt-get install libssl-dev`

**Error: "HYPERLIQUID_PRIVATE_KEY not set"**
```bash
export HYPERLIQUID_PRIVATE_KEY="0x..."
```

**Rebuild after making changes to src/**
```bash
cd build
make hyperliquid
cd ..
./compile.sh your_bot.cpp
```

## Next Steps

- Read full documentation: [README.md](README.md)
- Check examples: `ls examples/`
- API reference: [README.md#api-documentation](README.md#api-documentation)
- Test on testnet first!
