# Makefile for common hyperliquid-cpp tasks

.PHONY: help build examples clean rebuild test run-examples

help:
	@echo "Hyperliquid C++ SDK - Common Commands"
	@echo ""
	@echo "  make build         - Build the hyperliquid library"
	@echo "  make examples      - Build all examples"
	@echo "  make clean         - Clean build directory"
	@echo "  make rebuild       - Clean and rebuild everything"
	@echo "  make run-examples  - Show how to run examples"
	@echo ""
	@echo "To compile a custom .cpp file:"
	@echo "  ./compile.sh your_file.cpp"

build:
	@mkdir -p build
	@cd build && cmake .. && make hyperliquid
	@echo "✓ Library built successfully"

examples: build
	@cd build && make
	@echo "✓ All examples built successfully"
	@echo ""
	@echo "Run examples from build/examples/ directory:"
	@echo "  ./build/examples/basic_order"
	@echo "  ./build/examples/basic_market_order"
	@echo "  ./build/examples/query_positions <address>"
	@echo "  ./build/examples/bulk_orders"
	@echo "  ./build/examples/order"

clean:
	@rm -rf build
	@echo "✓ Build directory cleaned"

rebuild: clean build
	@echo "✓ Rebuild complete"

run-examples:
	@echo "Available examples:"
	@echo ""
	@echo "1. Basic Order (limit order):"
	@echo "   export HYPERLIQUID_PRIVATE_KEY='0x...'"
	@echo "   ./build/examples/basic_order"
	@echo ""
	@echo "2. Market Order:"
	@echo "   export HYPERLIQUID_PRIVATE_KEY='0x...'"
	@echo "   ./build/examples/basic_market_order"
	@echo ""
	@echo "3. Query Positions:"
	@echo "   ./build/examples/query_positions 0xYourAddress"
	@echo ""
	@echo "4. Bulk Orders:"
	@echo "   export HYPERLIQUID_PRIVATE_KEY='0x...'"
	@echo "   ./build/examples/bulk_orders"
