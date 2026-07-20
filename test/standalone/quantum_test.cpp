// Copyright (c) 2026 The HCScoin developers
// HCScoin: Standalone tests for consensus/quantum.cpp.
//
// 1. Cross-implementation parity: the proofs computed here must match the
//    vectors produced by panta-sim (cargo run --release --example vector).
// 2. Functional checks: generate/validate round-trip, tamper detection,
//    difficulty checking.
//
// Build (from repo root):
//   c++ -O2 -std=c++20 -I src src/consensus/quantum.cpp src/crypto/sha256.cpp \
//       test/standalone/quantum_test.cpp -o quantum_test
//
// Exit code 0 = all tests passed.

#include <consensus/quantum.h>
#include <uint256.h>

#include <cstdio>
#include <cstring>

static int failures = 0;

#define CHECK(cond, name) do { \
    if (cond) { std::printf("  [PASS] %s\n", name); } \
    else { std::printf("  [FAIL] %s\n", name); failures++; } \
} while (0)

static std::string HexStr(const unsigned char* p, size_t n)
{
    static const char* d = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) { s += d[p[i] >> 4]; s += d[p[i] & 15]; }
    return s;
}

int main()
{
    std::printf("== consensus/quantum (dual PoW) self-tests ==\n");

    struct Vector {
        unsigned char hashfill;
        uint32_t nonce;
        int qubits;
        const char* fingerprint;
        uint32_t measurement;
        const char* diffhash;
    };
    // Vectors from panta-sim (Rust) - see panta-sim/examples/vector.rs
    static const Vector VECTORS[] = {
        {0x03, 42, 12,
         "cccafb70cf891fedd88219c48bba99e5bf841f4b0a4256bca80bee28a053cf34",
         288, "2b11d9f555e8c19f99fe3a5c5bdf1fda7985fd86cfd0b44c645d9560431215bb"},
        {0x07, 0, 10,
         "ae589d0b20d5739d77e6d30a38252273d6254b97770dd02cae93c5eab1c01ef3",
         576, "f8f40f07608ff356d922d1f899aae480f6c2a8d8d4fd0008177352a1f92bdc6d"},
        {0xab, 123456, 12,
         "5d66bfb379bb2abbb9ad2eddf0f81e63bf6511188b52f9df8e28a7df30328a71",
         533, "9863fb119427c20ef8ecb80f3bd258bb41ad1f7be510de8726d372c31231a698"},
    };

    for (const auto& v : VECTORS) {
        uint256 h;
        std::memset(h.begin(), v.hashfill, 32);
        std::vector<unsigned char> proof = GenerateQuantumProof(h, v.nonce, v.qubits);
        char name[128];

        std::snprintf(name, sizeof(name), "parity fp (fill=%02x, nonce=%u, q=%d)",
                      v.hashfill, v.nonce, v.qubits);
        CHECK(HexStr(proof.data() + QUANTUM_OFF_FINGERPRINT, 32) == v.fingerprint, name);

        uint32_t meas = 0;
        std::memcpy(&meas, proof.data() + QUANTUM_OFF_MEASUREMENT, 4);
        std::snprintf(name, sizeof(name), "parity measurement (fill=%02x)", v.hashfill);
        CHECK(meas == v.measurement, name);

        std::snprintf(name, sizeof(name), "parity diffhash (fill=%02x)", v.hashfill);
        CHECK(HexStr(proof.data() + QUANTUM_OFF_DIFFHASH, 32) == v.diffhash, name);

        std::snprintf(name, sizeof(name), "validate (fill=%02x)", v.hashfill);
        CHECK(ValidateQuantumProof(proof, h, v.nonce, v.qubits), name);

        std::snprintf(name, sizeof(name), "validate rejects wrong nonce (fill=%02x)", v.hashfill);
        CHECK(!ValidateQuantumProof(proof, h, v.nonce + 1, v.qubits), name);

        std::snprintf(name, sizeof(name), "validate rejects tamper (fill=%02x)", v.hashfill);
        std::vector<unsigned char> bad = proof;
        bad[9] ^= 1;
        CHECK(!ValidateQuantumProof(bad, h, v.nonce, v.qubits), name);

        std::snprintf(name, sizeof(name), "wellformed (fill=%02x)", v.hashfill);
        CHECK(IsQuantumProofWellFormed(proof, v.qubits) &&
              !IsQuantumProofWellFormed(proof, v.qubits + 1), name);
    }

    /* Difficulty checking */
    {
        uint256 h;
        std::memset(h.begin(), 0x03, 32);
        std::vector<unsigned char> proof = GenerateQuantumProof(h, 42, 12);
        uint256 target = GetQuantumDifficultyHash(proof);
        CHECK(CheckQuantumDifficulty(proof, target), "difficulty: equal target passes");
        // target - 1 must fail (decrement last byte with borrow)
        std::vector<unsigned char> t(target.begin(), target.end());
        int i = 0;
        for (; i < 32; ++i) { if (t[i] > 0) { t[i]--; break; } t[i] = 0xff; }
        uint256 below;
        std::memcpy(below.begin(), t.data(), 32);
        CHECK(!CheckQuantumDifficulty(proof, below), "difficulty: target-1 fails");
        uint256 max;
        std::memset(max.begin(), 0xff, 32);
        CHECK(CheckQuantumDifficulty(proof, max), "difficulty: max target passes");
    }

    if (failures == 0) std::printf("ALL QUANTUM TESTS PASSED\n");
    else std::printf("%d TEST(S) FAILED\n", failures);
    return failures ? 1 : 0;
}
