#!/bin/bash
# Simple script to compile any .cpp file using the hyperliquid library

set -e

if [ $# -eq 0 ]; then
    echo "Usage: ./compile.sh <file.cpp> [output_name]"
    echo ""
    echo "Examples:"
    echo "  ./compile.sh order.cpp          # Creates ./order"
    echo "  ./compile.sh my_bot.cpp bot     # Creates ./bot"
    exit 1
fi

INPUT_FILE="$1"
OUTPUT_NAME="${2:-${INPUT_FILE%.cpp}}"

# Check if input file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: File '$INPUT_FILE' not found"
    exit 1
fi

# Build the library first if needed
if [ ! -f "build/libhyperliquid.a" ]; then
    echo "Building hyperliquid library..."
    mkdir -p build
    cd build
    cmake .. > /dev/null
    make hyperliquid
    cd ..
fi

# Detect OpenSSL path
if [ -d "/opt/homebrew/opt/openssl@3" ]; then
    # macOS with Homebrew
    OPENSSL_INCLUDE="/opt/homebrew/opt/openssl@3/include"
    OPENSSL_LIB="/opt/homebrew/opt/openssl@3/lib"
elif [ -d "/usr/local/opt/openssl@3" ]; then
    # macOS with older Homebrew
    OPENSSL_INCLUDE="/usr/local/opt/openssl@3/include"
    OPENSSL_LIB="/usr/local/opt/openssl@3/lib"
else
    # Linux or system OpenSSL
    OPENSSL_INCLUDE="/usr/include"
    OPENSSL_LIB="/usr/lib"
fi

echo "Compiling $INPUT_FILE -> $OUTPUT_NAME"

g++ -std=c++17 \
  -I./include \
  -I./build/_deps/json-src/include \
  -I./build/_deps/msgpack-src/include \
  -I"$OPENSSL_INCLUDE" \
  "$INPUT_FILE" \
  -L./build \
  -L"$OPENSSL_LIB" \
  -lhyperliquid \
  -lcurl \
  -lssl -lcrypto \
  -o "$OUTPUT_NAME"

echo "✓ Compiled successfully: ./$OUTPUT_NAME"
echo ""
echo "Run with: ./$OUTPUT_NAME"
