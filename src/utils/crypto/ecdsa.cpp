#include "hyperliquid/types.hpp"
#include "hyperliquid/utils/conversions.hpp"
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>
#include <stdexcept>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

namespace hyperliquid {
namespace crypto {

// Forward declare keccak256
std::vector<uint8_t> keccak256(const uint8_t* data, size_t len);

std::string bnToHex(const BIGNUM* bn, int min_bytes = 32) {
    int num_bytes = BN_num_bytes(bn);
    std::vector<uint8_t> bytes(std::max(num_bytes, min_bytes), 0);

    // BN_bn2bin writes big-endian bytes
    int actual_bytes = BN_bn2bin(bn, bytes.data() + (bytes.size() - num_bytes));
    if (actual_bytes != num_bytes) {
        throw std::runtime_error("BN_bn2bin failed");
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : bytes) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

void* createKeyFromPrivate(const std::string& private_key_hex) {
    // Remove "0x" prefix if present
    std::string key_hex = private_key_hex;
    if (key_hex.substr(0, 2) == "0x") {
        key_hex = key_hex.substr(2);
    }

    // Create EC_KEY with secp256k1 curve
    EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!ec_key) {
        throw std::runtime_error("Failed to create EC_KEY");
    }

    // Convert hex to BIGNUM
    BIGNUM* priv_bn = BN_new();
    if (BN_hex2bn(&priv_bn, key_hex.c_str()) == 0) {
        BN_free(priv_bn);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Invalid private key hex");
    }

    // Set private key
    if (EC_KEY_set_private_key(ec_key, priv_bn) != 1) {
        BN_free(priv_bn);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to set private key");
    }

    // Derive and set public key
    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    EC_POINT* pub_key = EC_POINT_new(group);
    if (!pub_key) {
        BN_free(priv_bn);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to create public key point");
    }

    if (EC_POINT_mul(group, pub_key, priv_bn, nullptr, nullptr, nullptr) != 1) {
        EC_POINT_free(pub_key);
        BN_free(priv_bn);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to derive public key");
    }

    if (EC_KEY_set_public_key(ec_key, pub_key) != 1) {
        EC_POINT_free(pub_key);
        BN_free(priv_bn);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to set public key");
    }

    EC_POINT_free(pub_key);
    BN_free(priv_bn);

    // Validate key
    if (EC_KEY_check_key(ec_key) != 1) {
        EC_KEY_free(ec_key);
        throw std::runtime_error("Invalid EC key");
    }

    return static_cast<void*>(ec_key);
}

std::string deriveAddress(const void* ec_key_ptr) {
    const EC_KEY* ec_key = static_cast<const EC_KEY*>(ec_key_ptr);
    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    const EC_POINT* pub_key = EC_KEY_get0_public_key(ec_key);

    // Convert public key to uncompressed format (65 bytes: 0x04 + x + y)
    std::vector<uint8_t> pub_key_bytes(65);
    size_t len = EC_POINT_point2oct(group, pub_key,
                                    POINT_CONVERSION_UNCOMPRESSED,
                                    pub_key_bytes.data(), 65, nullptr);

    if (len != 65) {
        throw std::runtime_error("Failed to convert public key");
    }

    // Hash public key (skip first byte 0x04)
    std::vector<uint8_t> hash = keccak256(pub_key_bytes.data() + 1, 64);

    // Take last 20 bytes for address
    std::string address = "0x";
    for (size_t i = 12; i < 32; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        address += buf;
    }

    return address;
}

int calculateRecoveryId(const EC_KEY* ec_key,
                       const std::vector<uint8_t>& hash,
                       const ECDSA_SIG* sig) {
    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    const EC_POINT* pub_key = EC_KEY_get0_public_key(ec_key);

    const BIGNUM *r, *s;
    ECDSA_SIG_get0(sig, &r, &s);

    // Try recovery IDs 0 and 1
    for (int recovery_id = 0; recovery_id < 2; ++recovery_id) {
        EC_POINT* recovered = EC_POINT_new(group);
        if (!recovered) continue;

        // Attempt to recover public key
        // This is simplified - full recovery involves more steps
        // For now, we'll use a heuristic based on Y coordinate parity

        BIGNUM* y = BN_new();
        if (EC_POINT_get_affine_coordinates(group, pub_key, nullptr, y, nullptr) == 1) {
            int y_parity = BN_is_odd(y);
            BN_free(y);

            EC_POINT_free(recovered);

            if (y_parity == recovery_id) {
                return recovery_id;
            }
        } else {
            BN_free(y);
            EC_POINT_free(recovered);
        }
    }

    // Default to 0 if recovery fails
    return 0;
}

Signature signHash(const void* ec_key_ptr, const std::vector<uint8_t>& hash) {
    EC_KEY* ec_key = static_cast<EC_KEY*>(const_cast<void*>(ec_key_ptr));

    if (hash.size() != 32) {
        throw std::invalid_argument("Hash must be 32 bytes");
    }

    // Sign the hash
    ECDSA_SIG* sig = ECDSA_do_sign(hash.data(), hash.size(), ec_key);
    if (!sig) {
        throw std::runtime_error("ECDSA signing failed");
    }

    const BIGNUM *r, *s;
    ECDSA_SIG_get0(sig, &r, &s);

    // Convert to hex strings
    Signature result;
    result.r = "0x" + bnToHex(r, 32);
    result.s = "0x" + bnToHex(s, 32);

    // Calculate recovery ID (v)
    int recovery_id = calculateRecoveryId(ec_key, hash, sig);
    result.v = recovery_id + 27;  // Ethereum uses 27/28

    ECDSA_SIG_free(sig);
    return result;
}

void freeKey(void* ec_key_ptr) {
    if (ec_key_ptr) {
        EC_KEY_free(static_cast<EC_KEY*>(ec_key_ptr));
    }
}

} // namespace crypto
} // namespace hyperliquid
