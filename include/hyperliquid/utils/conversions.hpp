#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace hyperliquid {

/**
 * Convert float to wire format string (8 decimal precision)
 * Removes trailing zeros and validates no significant rounding occurred
 */
std::string floatToWire(double value);

/**
 * Convert float to USD integer (6 decimals)
 */
int64_t floatToUsdInt(double value);

/**
 * Convert float to integer with specified decimals
 */
int64_t floatToInt(double value, int decimals);

/**
 * Convert hex string to bytes
 * Handles both "0x..." and raw hex strings
 */
std::vector<uint8_t> hexToBytes(const std::string& hex);

/**
 * Convert bytes to hex string with "0x" prefix
 */
std::string bytesToHex(const std::vector<uint8_t>& bytes, bool with_prefix = true);

/**
 * Convert bytes to hex string with "0x" prefix
 */
std::string bytesToHex(const uint8_t* data, size_t len, bool with_prefix = true);

/**
 * Convert address to lowercase and validate format
 */
std::string normalizeAddress(const std::string& address);

/**
 * Get current timestamp in milliseconds
 */
int64_t getTimestampMs();

/**
 * Round price to Hyperliquid tick size requirements
 * - Prices can have up to 5 significant figures
 * - But no more than MAX_DECIMALS - szDecimals decimal places
 * - MAX_DECIMALS = 6 for perps, 8 for spot
 * - Integer prices > 100k are always allowed
 */
double roundPrice(double price, int sz_decimals, bool is_spot);

/**
 * Round size to Hyperliquid lot size requirements
 * - Sizes are rounded to szDecimals
 */
double roundSize(double size, int sz_decimals);

} // namespace hyperliquid
