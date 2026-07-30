// Signing throughput benchmark for the Hyperliquid C++ SDK.
//
// Measures the full signL1Action() path a caller actually hits, then breaks it
// into stages, then compares the ECDSA core against raw OpenSSL to establish
// the floor. Purely local -- no network, no funds.

// Same reason as src/utils/crypto/ecdsa.cpp: EC_KEY works fine in OpenSSL 3.x,
// and this file times it directly.
#define OPENSSL_SUPPRESS_DEPRECATED

#include <hyperliquid/utils/signing.hpp>
#include <hyperliquid/types.hpp>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// Internal crypto entry points. Not in any header, but external linkage.
namespace hyperliquid {
namespace crypto {
void* createKeyFromPrivate(const std::string& private_key_hex);
void freeKey(void* ec_key);
Signature signHash(const void* ec_key, const std::vector<uint8_t>& hash);
std::vector<uint8_t> encodeTypedData(const nlohmann::json& typed_data);
std::vector<uint8_t> keccak256(const std::vector<uint8_t>& data);
BIGNUM* generateDeterministicK(const BIGNUM* priv_key,
                               const std::vector<uint8_t>& hash,
                               const EC_GROUP* group);
std::string bnToHex(const BIGNUM* bn, int min_bytes);
}  // namespace crypto
}  // namespace hyperliquid

using hyperliquid::Signature;
using hyperliquid::Wallet;
using Clock = std::chrono::steady_clock;

namespace {

constexpr const char* TEST_KEY = "";

struct Stats {
    double mean_us;
    double median_us;
    double p95_us;
    double min_us;
    double ops_per_sec;
};

// Both mean and median are reported, and the gap between them matters here:
// signing latency is bimodal, because the recovery-id search exits after one
// attempt or two depending on the hash. A median hides that by landing in
// whichever mode is more common, so throughput is derived from the mean.
Stats measure(int iterations, const std::function<void()>& body) {
    // Warm up: first calls fault in pages and prime OpenSSL's internal tables.
    for (int i = 0; i < 200; ++i) {
        body();
    }

    std::vector<double> samples;
    samples.reserve(iterations);
    for (int i = 0; i < iterations; ++i) {
        const auto start = Clock::now();
        body();
        const auto end = Clock::now();
        samples.push_back(
            std::chrono::duration<double, std::micro>(end - start).count());
    }

    double total = 0.0;
    for (double sample : samples) {
        total += sample;
    }

    std::sort(samples.begin(), samples.end());
    Stats stats;
    stats.mean_us = total / static_cast<double>(samples.size());
    stats.median_us = samples[samples.size() / 2];
    stats.p95_us = samples[static_cast<size_t>(samples.size() * 0.95)];
    stats.min_us = samples.front();
    stats.ops_per_sec = 1e6 / stats.mean_us;
    return stats;
}

void report(const char* label, const Stats& s) {
    std::printf("  %-34s %9.2f %9.2f %9.2f %9.2f %11.0f\n",
                label, s.mean_us, s.median_us, s.p95_us, s.min_us, s.ops_per_sec);
}

void header(const char* title) {
    std::printf("\n%s\n", title);
    std::printf("  %-34s %9s %9s %9s %9s %11s\n",
                "stage", "mean us", "med us", "p95 us", "min us", "ops/sec");
    std::printf("  %s\n", std::string(88, '-').c_str());
}

nlohmann::ordered_json orderAction() {
    nlohmann::ordered_json order;
    order["a"] = 0;
    order["b"] = true;
    order["p"] = "65000.0";
    order["s"] = "0.01";
    order["r"] = false;
    nlohmann::ordered_json limit;
    limit["tif"] = "Gtc";
    order["t"] = nlohmann::ordered_json{{"limit", limit}};

    nlohmann::ordered_json action;
    action["type"] = "order";
    action["orders"] = nlohmann::ordered_json::array({order});
    action["grouping"] = "na";
    return action;
}

// A 20-order bulk action, to see how the fixed ECDSA cost compares with the
// action-serialization cost that scales with order count.
nlohmann::ordered_json bulkOrderAction(int count) {
    nlohmann::ordered_json orders = nlohmann::ordered_json::array();
    for (int i = 0; i < count; ++i) {
        nlohmann::ordered_json order;
        order["a"] = i;
        order["b"] = (i % 2) == 0;
        order["p"] = "65000.0";
        order["s"] = "0.01";
        order["r"] = false;
        nlohmann::ordered_json limit;
        limit["tif"] = "Gtc";
        order["t"] = nlohmann::ordered_json{{"limit", limit}};
        orders.push_back(order);
    }

    nlohmann::ordered_json action;
    action["type"] = "order";
    action["orders"] = orders;
    action["grouping"] = "na";
    return action;
}

}  // namespace

int main() {
    auto wallet = Wallet::fromPrivateKey(TEST_KEY);
    const auto action = orderAction();
    const auto bulk20 = bulkOrderAction(20);

    std::printf("Signing benchmark -- Release (-O3), single thread\n");
    std::printf("address: %s\n", wallet->address().c_str());

    // ---- End to end, the path every exchange action takes.
    int64_t nonce = 1700000000000;

    header("End-to-end signL1Action()");
    report("1 order", measure(2000, [&] {
        volatile auto sig = hyperliquid::signL1Action(
            *wallet, action, std::nullopt, nonce++, std::nullopt, false);
        (void)sig;
    }));
    report("20 orders", measure(2000, [&] {
        volatile auto sig = hyperliquid::signL1Action(
            *wallet, bulk20, std::nullopt, nonce++, std::nullopt, false);
        (void)sig;
    }));

    // ---- Stage breakdown for the single-order case.
    const auto act_hash =
        hyperliquid::actionHash(action, std::nullopt, nonce, std::nullopt);
    const auto phantom = hyperliquid::constructPhantomAgent(act_hash, false);
    const auto payload = hyperliquid::l1Payload(phantom);
    const auto msg_hash = hyperliquid::crypto::encodeTypedData(payload);

    // Wallet keeps its EC_KEY private, so use an independent handle on the
    // same key for the stage-level measurements.
    void* key = hyperliquid::crypto::createKeyFromPrivate(TEST_KEY);
    EC_KEY* ec_key = static_cast<EC_KEY*>(key);
    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    const BIGNUM* priv = EC_KEY_get0_private_key(ec_key);

    header("Stage breakdown (1 order)");
    report("actionHash (msgpack + keccak)", measure(5000, [&] {
        volatile auto h =
            hyperliquid::actionHash(action, std::nullopt, nonce, std::nullopt);
        (void)h;
    }));
    report("constructPhantomAgent", measure(5000, [&] {
        volatile auto p = hyperliquid::constructPhantomAgent(act_hash, false);
        (void)p;
    }));
    report("l1Payload (build EIP-712 json)", measure(5000, [&] {
        volatile auto p = hyperliquid::l1Payload(phantom);
        (void)p;
    }));
    report("encodeTypedData (EIP-712 hash)", measure(5000, [&] {
        volatile auto h = hyperliquid::crypto::encodeTypedData(payload);
        (void)h;
    }));
    report("signHash (ECDSA total)", measure(2000, [&] {
        volatile auto s = hyperliquid::crypto::signHash(key, msg_hash);
        (void)s;
    }));

    header("Inside signHash (1 signature)");
    report("generateDeterministicK (RFC 6979)", measure(3000, [&] {
        BIGNUM* k = hyperliquid::crypto::generateDeterministicK(priv, msg_hash, group);
        BN_free(k);
    }));
    // The recovery id is now derived inline from kG inside signHash -- it is a
    // BN_cmp plus a parity test, so there is nothing separable left to time.
    report("bnToHex x2 (r and s)", measure(5000, [&] {
        volatile size_t n = hyperliquid::crypto::bnToHex(priv, 32).size() +
                            hyperliquid::crypto::bnToHex(priv, 32).size();
        (void)n;
    }));

    // ---- The floor: what OpenSSL costs for the same curve and hash, with no
    // recovery id and no hex formatting.
    header("Reference floor (raw OpenSSL, same curve)");
    report("ECDSA_do_sign", measure(3000, [&] {
        ECDSA_SIG* sig = ECDSA_do_sign(msg_hash.data(),
                                       static_cast<int>(msg_hash.size()), ec_key);
        ECDSA_SIG_free(sig);
    }));

    hyperliquid::crypto::freeKey(key);
    return 0;
}
