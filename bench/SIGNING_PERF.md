# Signing Performance: Before and After

Reproduce with:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCH=ON
cmake --build build --target signing_bench
./build/bench/signing_bench
```

**Environment:** Apple M4 (10 cores), macOS 24.6.0, Apple clang 17.0.0,
OpenSSL 3.6.2 (Homebrew), `-O3 -DNDEBUG`, single thread.

**Method:** each stage timed per-iteration over 2000–5000 iterations after a
200-iteration warmup. Throughput is derived from the **mean**; before the change,
latency was bimodal and a median reported only the more common mode.

## Result

| Path | Before | After | Speedup |
|---|---|---|---|
| `signL1Action()`, 1 order | 833 µs (~1,200 sig/s) | **175 µs (~5,710 sig/s)** | **4.8×** |
| `signL1Action()`, 20 orders | 860 µs (~1,155 sig/s) | 184 µs (~5,440 sig/s) | 4.7× |
| `signHash` (ECDSA only) | ~819 µs | **157 µs** | 5.2× |
| Raw `ECDSA_do_sign` (reference floor) | 148 µs | 148 µs | — |

Signing went from **5.7× slower than OpenSSL's own signing call** to **18%
slower** — and that remaining 18% is the msgpack serialization, keccak, EIP-712
encoding, and hex formatting that `ECDSA_do_sign` does not do at all. The ECDSA
work itself is now at the floor.

Latency also stopped being bimodal. Before: a coin flip between ~596 µs and
~1001 µs. After: mean 175 µs, median 171 µs, p95 190 µs, min 154 µs — a ~36 µs
band.

## What changed

`src/utils/crypto/ecdsa.cpp`. The recovery id is a property of the point `kG`,
which `signHash` already computes. It was calling `calculateRecoveryId()`, which
recovered the public key from scratch to rediscover it:

- per attempt, a modular square root (`BN_mod_sqrt`, a full modexp) plus three EC
  scalar multiplications — **425 µs, about three full signatures**
- it tried recovery id 0 then 1, exiting early on a match, so 1 or 2 attempts,
  roughly 50/50 (209/191 over 400 hashes → 1.48 average)
- that averaged **~629 µs, ~75% of total signing time**

It now reads the value directly off `kG`:

```cpp
int recovery_id = (BN_cmp(x_coord, order) >= 0) ? 2 : 0;
if (BN_is_odd(y_coord)) {
    recovery_id |= 1;
}
// ... and after the low-s normalization, which mirrors R over the x-axis:
if (BN_cmp(s, half_order) > 0) {
    BN_sub(s, order, s);
    recovery_id ^= 1;
}
```

`y_coord` was previously computed and then freed without ever being read, so the
answer was already sitting in a local variable. `calculateRecoveryId()` and its
98 lines of point recovery are deleted, along with the `ECDSA_SIG` that existed
only to feed it.

## Accuracy

This is strictly more correct than what it replaced, not a trade.

**1. Byte-identical output.** 2000 signatures over distinct hashes, before vs
after — all 2000 `(r, s, v)` triples match exactly, covering both parities
(1069 × v=27, 931 × v=28).

**2. Independently verified, not just unchanged.** "Matches the old code" would
be worthless if the old code were wrong, so each of the 2000 signatures was run
through standard ECDSA public-key recovery — reconstruct `R` from `(r, v)`, solve
`Q = r⁻¹(sR − eG)`, keccak the result to an address — and compared against the
real signer address. **2000/2000 recovered correctly.**

**3. Two failure modes removed.**

- The old loop `return 0`d when neither recovery id matched, silently producing a
  signature that recovers to the wrong address. The direct computation cannot
  fail.
- The old loop only ever tried ids 0 and 1, ignoring the `x >= n` case (ids 2/3).
  The new code handles it via the `BN_cmp(x_coord, order)` check. Probability is
  ~2⁻¹²⁸ on secp256k1, so this will never fire in practice — but it costs two
  instructions and the failure would be silent.

**4. Pinned against regression.** `signatureVectorsAreStable()` in
`tests/l1_action_signing_test.cpp` holds four pre-change `(r, s, v)` vectors, two
of each parity, so the v=28 low-s negation path is covered. Verified non-vacuous:
removing the `recovery_id ^= 1` flip makes it fail on `actual.v == expected.v`.

This test matters more than it looks. Every other case in that file recomputes
the expected signature with the same code it is checking, so both sides move
together and a change in signing output is invisible to them. These vectors are
the only thing pinning `v`, which is what decides the address an order is
attributed to.

## What is left

### Not worth doing for speed

**Replace the hand-rolled ECDSA core with `ECDSA_do_sign`.** The remaining
~148 µs of ECDSA already equals OpenSSL's 148 µs for a whole signature, so there
is no throughput left here. The reason to still do it is safety: a hand-rolled
RFC 6979 nonce bug leaks the private key, and this implementation skips RFC 6979's
`bits2octets` reduction.

**Non-ECDSA stages.** msgpack + keccak + EIP-712 + hex now total ~18 µs, which is
10% of the new budget rather than 1.7% of the old one — so this is where the next
increment would come from, but the absolute numbers are small. `l1Payload`
rebuilding the same static EIP-712 type declarations every call (5.6 µs) is the
largest single item.

| Stage | After |
|---|---|
| `actionHash` (msgpack + keccak) | 0.90 µs |
| `constructPhantomAgent` | 2.22 µs |
| `l1Payload` (build EIP-712 json) | 5.62 µs |
| `encodeTypedData` (EIP-712 hash) | 5.64 µs |
| `generateDeterministicK` (RFC 6979) | 4.67 µs |
| `bnToHex` ×2 | 3.64 µs |
| `signHash` total | 157 µs |

### Open items, unrelated to speed

- **Side channel:** the nonce `k` is never marked
  `BN_set_flags(k, BN_FLG_CONSTTIME)`, so the scalar multiplication is not
  guaranteed constant-time with respect to the nonce. Lives in the code the
  `ECDSA_do_sign` switch would delete.
- **Parallelism:** signing is CPU-bound with no shared mutable state beyond the
  `EC_KEY`, so bulk signing should scale near-linearly across threads. Now worth
  doing, since the per-signature work is no longer redundant.
- **Testnet acceptance:** not run. `HL_TESTNET_ADDRESS` / `HL_TESTNET_KEY` are
  not exported into the shell used for this work. The byte-identical output plus
  address recovery above is stronger evidence about the recovery id than an
  exchange round-trip would be, but if you want the round-trip too, export them
  (`! export HL_TESTNET_KEY=...`) and it can be added.
