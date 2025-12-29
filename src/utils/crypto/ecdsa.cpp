#include "hyperliquid/types.hpp"
#include "hyperliquid/utils/conversions.hpp"
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/core_names.h>
#include <openssl/hmac.h>
#include <stdexcept>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>

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

    // Skip leading zeros (but keep at least one byte)
    size_t start = 0;
    while (start < bytes.size() - 1 && bytes[start] == 0) {
        start++;
    }

    for (size_t i = start; i < bytes.size(); i++) {
        oss << std::setw(2) << static_cast<int>(bytes[i]);
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

// RFC 6979 deterministic k generation
BIGNUM* generateDeterministicK(const BIGNUM* priv_key, const std::vector<uint8_t>& hash, const EC_GROUP* group) {
    // Get curve order
    BIGNUM* order = BN_new();
    EC_GROUP_get_order(group, order, nullptr);

    // Convert private key and hash to bytes
    std::vector<uint8_t> priv_bytes(32);
    std::vector<uint8_t> hash_bytes = hash;

    BN_bn2binpad(priv_key, priv_bytes.data(), 32);

    // Ensure hash is 32 bytes
    if (hash_bytes.size() > 32) {
        hash_bytes.resize(32);
    } else while (hash_bytes.size() < 32) {
        hash_bytes.insert(hash_bytes.begin(), 0);
    }

    // RFC 6979 Section 3.2
    // Step a: hash message (already done)
    // Step b: h1 = H(m) truncated to qlen bits

    // Step c: V = 0x01 0x01 ...0x01 (32 bytes)
    std::vector<uint8_t> V(32, 0x01);

    // Step d: K = 0x00 0x00 ... 0x00 (32 bytes)
    std::vector<uint8_t> K(32, 0x00);

    // Step e: K = HMAC_K(V || 0x00 || priv || hash)
    std::vector<uint8_t> data;
    data.insert(data.end(), V.begin(), V.end());
    data.push_back(0x00);
    data.insert(data.end(), priv_bytes.begin(), priv_bytes.end());
    data.insert(data.end(), hash_bytes.begin(), hash_bytes.end());

    unsigned int len;
    HMAC(EVP_sha256(), K.data(), K.size(), data.data(), data.size(), K.data(), &len);

    // Step f: V = HMAC_K(V)
    HMAC(EVP_sha256(), K.data(), K.size(), V.data(), V.size(), V.data(), &len);

    // Step g: K = HMAC_K(V || 0x01 || priv || hash)
    data.clear();
    data.insert(data.end(), V.begin(), V.end());
    data.push_back(0x01);
    data.insert(data.end(), priv_bytes.begin(), priv_bytes.end());
    data.insert(data.end(), hash_bytes.begin(), hash_bytes.end());

    HMAC(EVP_sha256(), K.data(), K.size(), data.data(), data.size(), K.data(), &len);

    // Step h: V = HMAC_K(V)
    HMAC(EVP_sha256(), K.data(), K.size(), V.data(), V.size(), V.data(), &len);

    // Step h3: Generate k
    BIGNUM* k = nullptr;
    while (true) {
        // T = V = HMAC_K(V)
        HMAC(EVP_sha256(), K.data(), K.size(), V.data(), V.size(), V.data(), &len);

        k = BN_bin2bn(V.data(), V.size(), k);

        // Check if k is in [1, order-1]
        if (BN_is_zero(k) || BN_cmp(k, order) >= 0) {
            // K = HMAC_K(V || 0x00)
            data.clear();
            data.insert(data.end(), V.begin(), V.end());
            data.push_back(0x00);
            HMAC(EVP_sha256(), K.data(), K.size(), data.data(), data.size(), K.data(), &len);

            // V = HMAC_K(V)
            HMAC(EVP_sha256(), K.data(), K.size(), V.data(), V.size(), V.data(), &len);
        } else {
            break;
        }
    }

    BN_free(order);
    return k;
}

int calculateRecoveryId(const EC_KEY* ec_key,
                       const std::vector<uint8_t>& hash,
                       const ECDSA_SIG* sig) {
    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    const EC_POINT* pub_key = EC_KEY_get0_public_key(ec_key);

    const BIGNUM *r, *s;
    ECDSA_SIG_get0(sig, &r, &s);

    // Get the order of the curve
    BIGNUM* order = BN_new();
    EC_GROUP_get_order(group, order, nullptr);

    // Get curve parameters
    BIGNUM* p = BN_new();
    EC_GROUP_get_curve(group, p, nullptr, nullptr, nullptr);

    BN_CTX* ctx = BN_CTX_new();

    // Try both recovery IDs (0 and 1)
    for (int recovery_id = 0; recovery_id < 2; ++recovery_id) {
        // Calculate x coordinate of R (which is r)
        BIGNUM* x = BN_dup(r);

        // Calculate y from x
        // y^2 = x^3 + 7 (for secp256k1)
        BIGNUM* y_squared = BN_new();
        BIGNUM* y = BN_new();
        BIGNUM* tmp = BN_new();

        // y_squared = x^3
        BN_mod_mul(tmp, x, x, p, ctx);
        BN_mod_mul(y_squared, tmp, x, p, ctx);

        // y_squared = x^3 + 7
        BN_add_word(y_squared, 7);
        BN_mod(y_squared, y_squared, p, ctx);

        // y = sqrt(y_squared) mod p
        BN_mod_sqrt(y, y_squared, p, ctx);

        // Choose y based on recovery_id (odd/even)
        if ((BN_is_odd(y) ? 1 : 0) != recovery_id) {
            BN_sub(y, p, y);
        }

        // Create point R = (x, y)
        EC_POINT* R = EC_POINT_new(group);
        EC_POINT_set_affine_coordinates(group, R, x, y, ctx);

        // Recover public key: Q = r^-1 * (s*R - e*G)
        BIGNUM* r_inv = BN_new();
        BN_mod_inverse(r_inv, r, order, ctx);

        BIGNUM* e = BN_bin2bn(hash.data(), hash.size(), nullptr);

        EC_POINT* sR = EC_POINT_new(group);
        EC_POINT_mul(group, sR, nullptr, R, s, ctx);

        EC_POINT* eG = EC_POINT_new(group);
        EC_POINT_mul(group, eG, e, nullptr, nullptr, ctx);

        EC_POINT* Q = EC_POINT_new(group);
        EC_POINT_invert(group, eG, ctx);
        EC_POINT_add(group, Q, sR, eG, ctx);
        EC_POINT_mul(group, Q, nullptr, Q, r_inv, ctx);

        // Compare recovered public key with actual public key
        int match = (EC_POINT_cmp(group, Q, pub_key, ctx) == 0);

        // Cleanup
        EC_POINT_free(R);
        EC_POINT_free(sR);
        EC_POINT_free(eG);
        EC_POINT_free(Q);
        BN_free(x);
        BN_free(y);
        BN_free(y_squared);
        BN_free(tmp);
        BN_free(r_inv);
        BN_free(e);

        if (match) {
            BN_free(order);
            BN_free(p);
            BN_CTX_free(ctx);
            return recovery_id;
        }
    }

    BN_free(order);
    BN_free(p);
    BN_CTX_free(ctx);

    // Default to 0 if recovery fails
    return 0;
}

Signature signHash(const void* ec_key_ptr, const std::vector<uint8_t>& hash) {
    EC_KEY* ec_key = static_cast<EC_KEY*>(const_cast<void*>(ec_key_ptr));

    if (hash.size() != 32) {
        throw std::invalid_argument("Hash must be 32 bytes");
    }

    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    const BIGNUM* priv_key = EC_KEY_get0_private_key(ec_key);
    if (!priv_key) {
        throw std::runtime_error("Failed to get private key");
    }

    // Generate deterministic k using RFC 6979
    BIGNUM* k = generateDeterministicK(priv_key, hash, group);

    // Get curve order
    BIGNUM* order = BN_new();
    EC_GROUP_get_order(group, order, nullptr);

    BN_CTX* ctx = BN_CTX_new();

    // Calculate r = (k * G).x mod order
    EC_POINT* kG = EC_POINT_new(group);
    EC_POINT_mul(group, kG, k, nullptr, nullptr, ctx);

    BIGNUM* r = BN_new();
    BIGNUM* y_coord = BN_new();
    EC_POINT_get_affine_coordinates(group, kG, r, y_coord, ctx);
    BN_mod(r, r, order, ctx);

    // Calculate s = k^-1 * (hash + r * priv_key) mod order
    BIGNUM* k_inv = BN_new();
    BN_mod_inverse(k_inv, k, order, ctx);

    BIGNUM* e = BN_bin2bn(hash.data(), hash.size(), nullptr);
    BIGNUM* s = BN_new();
    BIGNUM* tmp = BN_new();

    BN_mod_mul(tmp, r, priv_key, order, ctx);  // r * priv_key
    BN_mod_add(tmp, e, tmp, order, ctx);        // hash + r * priv_key
    BN_mod_mul(s, k_inv, tmp, order, ctx);      // k^-1 * (hash + r * priv_key)

    // Ensure s is in the lower half (ETH requirement for non-malleability)
    BIGNUM* half_order = BN_new();
    BN_rshift1(half_order, order);
    if (BN_cmp(s, half_order) > 0) {
        BN_sub(s, order, s);
    }

    // Create signature
    ECDSA_SIG* sig = ECDSA_SIG_new();
    ECDSA_SIG_set0(sig, BN_dup(r), BN_dup(s));

    // Convert to hex strings
    Signature result;
    result.r = "0x" + bnToHex(r, 32);
    result.s = "0x" + bnToHex(s, 32);

    // Calculate recovery ID (v)
    int recovery_id = calculateRecoveryId(ec_key, hash, sig);
    result.v = recovery_id + 27;  // Ethereum uses 27/28

    // Cleanup
    ECDSA_SIG_free(sig);
    EC_POINT_free(kG);
    BN_free(k);
    BN_free(order);
    BN_free(r);
    BN_free(s);
    BN_free(y_coord);
    BN_free(k_inv);
    BN_free(e);
    BN_free(tmp);
    BN_free(half_order);
    BN_CTX_free(ctx);

    return result;
}

void freeKey(void* ec_key_ptr) {
    if (ec_key_ptr) {
        EC_KEY_free(static_cast<EC_KEY*>(ec_key_ptr));
    }
}

} // namespace crypto
} // namespace hyperliquid
