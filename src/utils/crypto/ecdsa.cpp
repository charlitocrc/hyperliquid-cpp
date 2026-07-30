// The EC_KEY API is deprecated in OpenSSL 3.0 but not removed, and it still
// works. Porting to EVP_PKEY/OSSL_PARAM buys nothing here, so silence the
// deprecation notes rather than drown real warnings in them.
// ponytail: revisit if OpenSSL 4.x actually drops EC_KEY.
#define OPENSSL_SUPPRESS_DEPRECATED

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

    // Every byte, including leading zeros: signature r/s are fixed-width
    // 32-byte quantities. Stripping a leading zero byte yields a 62-char hex
    // string that the API rejects, which happens to ~1 in 256 signatures.
    for (size_t i = 0; i < bytes.size(); i++) {
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

    BIGNUM* x_coord = BN_new();
    BIGNUM* y_coord = BN_new();
    EC_POINT_get_affine_coordinates(group, kG, x_coord, y_coord, ctx);

    // The recovery id is a property of the point kG, which we have right here,
    // so read it off directly rather than recovering the public key to find it.
    //
    //   bit 0 = parity of kG.y
    //   bit 1 = whether kG.x had to be reduced mod order
    //
    // Bit 1 needs the comparison before the reduction below. It is only
    // reachable when kG.x lands in [order, p), which on secp256k1 has
    // probability ~2^-128 -- but the check is two instructions, and a wrong
    // recovery id means the signature attributes to the wrong address.
    int recovery_id = (BN_cmp(x_coord, order) >= 0) ? 2 : 0;
    if (BN_is_odd(y_coord)) {
        recovery_id |= 1;
    }

    BIGNUM* r = BN_new();
    BN_mod(r, x_coord, order, ctx);

    // Calculate s = k^-1 * (hash + r * priv_key) mod order
    BIGNUM* k_inv = BN_new();
    BN_mod_inverse(k_inv, k, order, ctx);

    BIGNUM* e = BN_bin2bn(hash.data(), hash.size(), nullptr);
    BIGNUM* s = BN_new();
    BIGNUM* tmp = BN_new();

    BN_mod_mul(tmp, r, priv_key, order, ctx);  // r * priv_key
    BN_mod_add(tmp, e, tmp, order, ctx);        // hash + r * priv_key
    BN_mod_mul(s, k_inv, tmp, order, ctx);      // k^-1 * (hash + r * priv_key)

    // Ensure s is in the lower half (ETH requirement for non-malleability).
    // Replacing s with order - s mirrors R over the x-axis, flipping the y
    // parity the recovery id encodes, so bit 0 has to flip with it.
    BIGNUM* half_order = BN_new();
    BN_rshift1(half_order, order);
    if (BN_cmp(s, half_order) > 0) {
        BN_sub(s, order, s);
        recovery_id ^= 1;
    }

    Signature result;
    result.r = "0x" + bnToHex(r, 32);
    result.s = "0x" + bnToHex(s, 32);
    result.v = recovery_id + 27;  // Ethereum uses 27/28

    // Cleanup
    EC_POINT_free(kG);
    BN_free(k);
    BN_free(order);
    BN_free(x_coord);
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
