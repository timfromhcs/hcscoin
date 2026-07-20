// Copyright (c) 2026 The HCScoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
//
// HCScoin: Quantum Proof-of-Work - portable C++ implementation.
//
// This file is a bit-exact port of panta-sim (Rust):
//   panta-sim/src/statevector.rs  -> QSim::Statevector
//   panta-sim/src/circuit.rs      -> QuantumSimulate()
//   panta-sim/src/proof.rs        -> GenerateQuantumProof / ValidateQuantumProof
//
// DETERMINISM CONTRACT (consensus-critical):
//   * Clifford+T gate set only (X, Z, S, T, H, CNOT); all constants are
//     exactly representable f32 values (FRAC_1_SQRT_2 = 0x3F3504F3).
//   * Identical operand order and iteration order as the Rust code.
//   * Compile this file with -ffp-contract=off on GCC/Clang so that no
//     FMA fusion can change f32/f64 rounding (MSVC does not auto-fuse).
//   * Cross-implementation parity is enforced by
//     test/standalone/quantum_test.cpp against vectors produced by
//     panta-sim (see panta-sim/examples/vector.rs).

#include <consensus/quantum.h>

#include <crypto/sha256.h>
#include <uint256.h>

#include <cmath>
#include <cstring>
#include <memory>

/* ------------------------------------------------------------------ */
/* Seed expansion (identical to panta-sim/src/sha256.rs)              */
/* ------------------------------------------------------------------ */
static void QuantumExpandSeed(const std::vector<unsigned char>& seed, size_t n, std::vector<unsigned char>& out)
{
    out.clear();
    out.reserve(n + 32);
    for (uint32_t i = 0; out.size() < n; ++i) {
        CSHA256 h;
        h.Write(seed.data(), seed.size());
        unsigned char le[4] = {(unsigned char)i, (unsigned char)(i >> 8),
                               (unsigned char)(i >> 16), (unsigned char)(i >> 24)};
        h.Write(le, 4);
        unsigned char block[32];
        h.Finalize(block);
        out.insert(out.end(), block, block + 32);
    }
    out.resize(n);
}

/* ------------------------------------------------------------------ */
/* Statevector (identical to panta-sim/src/statevector.rs)            */
/* ------------------------------------------------------------------ */
namespace QSim {

static constexpr float FRAC_1_SQRT_2 = 0.70710677f; // 0x3F3504F3

struct C32 {
    float re{0.f}, im{0.f};
};

class Statevector
{
public:
    int nqubits;
    std::vector<C32> amps;

    explicit Statevector(int n) : nqubits(n), amps(size_t{1} << n) { amps[0] = C32{1.f, 0.f}; }

    template <typename F>
    void ForPairs(int q, F&& f)
    {
        const size_t step = size_t{1} << (q + 1);
        const size_t half = size_t{1} << q;
        for (size_t base = 0; base < amps.size(); base += step) {
            for (size_t off = 0; off < half; ++off) {
                const size_t i0 = base + off;
                const size_t i1 = i0 + half;
                f(amps[i0], amps[i1]);
            }
        }
    }

    void H(int q)
    {
        const float k = FRAC_1_SQRT_2;
        ForPairs(q, [k](C32& a, C32& b) {
            const C32 s{a.re + b.re, a.im + b.im};
            const C32 d{a.re - b.re, a.im - b.im};
            a = C32{s.re * k, s.im * k};
            b = C32{d.re * k, d.im * k};
        });
    }

    void X(int q)
    {
        ForPairs(q, [](C32& a, C32& b) { std::swap(a, b); });
    }

    void Z(int q)
    {
        ForPairs(q, [](C32&, C32& b) { b = C32{-b.re, -b.im}; });
    }

    void S(int q)
    {
        ForPairs(q, [](C32&, C32& b) { b = C32{-b.im, b.re}; });
    }

    void T(int q)
    {
        const float k = FRAC_1_SQRT_2;
        ForPairs(q, [k](C32&, C32& b) {
            const float re = (b.re - b.im) * k;
            const float im = (b.re + b.im) * k;
            b = C32{re, im};
        });
    }

    void CNOT(int c, int t)
    {
        const size_t cmask = size_t{1} << c;
        const size_t tmask = size_t{1} << t;
        for (size_t i = 0; i < amps.size(); ++i) {
            if ((i & cmask) != 0 && (i & tmask) == 0) {
                std::swap(amps[i], amps[i | tmask]);
            }
        }
    }

    uint64_t ArgmaxMeasurement() const
    {
        uint64_t best = 0;
        double bestp = -1.0;
        for (size_t i = 0; i < amps.size(); ++i) {
            const double re = amps[i].re;
            const double im = amps[i].im;
            const double p = re * re + im * im;
            if (p > bestp) {
                bestp = p;
                best = i;
            }
        }
        return best;
    }

    void SampledBytes(std::vector<unsigned char>& out, size_t count) const
    {
        if (count > amps.size()) count = amps.size();
        const size_t stride = amps.size() / count;
        out.clear();
        out.reserve(count * 8);
        for (size_t k = 0, i = 0; k < count; ++k, i += stride) {
            unsigned char b[4];
            std::memcpy(b, &amps[i].re, 4);
            out.insert(out.end(), b, b + 4);
            std::memcpy(b, &amps[i].im, 4);
            out.insert(out.end(), b, b + 4);
        }
    }
};

} // namespace QSim

/* ------------------------------------------------------------------ */
/* Circuit simulation (identical to panta-sim/src/circuit.rs)         */
/* ------------------------------------------------------------------ */
static constexpr int QUANTUM_LAYERS_PER_QUBIT = 3;
static constexpr size_t QUANTUM_FINGERPRINT_SAMPLES = 1024;
static const char* QUANTUM_CIRCUIT_DOMAIN = "HCScoin-QuantumPoW-v1";

/* Returns (measurement, fingerprint[32]) for (blockHash, nonce, qubits). */
static void QuantumSimulate(const uint256& blockHash, uint32_t nQuantumNonce, int nQubits,
                            uint32_t& measurementOut, unsigned char fingerprintOut[32])
{
    const int layers = nQubits * QUANTUM_LAYERS_PER_QUBIT;

    // seed = blockHash(32) || LE32(nonce) || domain (22 bytes)
    std::vector<unsigned char> seed;
    seed.reserve(32 + 4 + 22);
    seed.insert(seed.end(), blockHash.begin(), blockHash.end());
    seed.push_back((unsigned char)(nQuantumNonce));
    seed.push_back((unsigned char)(nQuantumNonce >> 8));
    seed.push_back((unsigned char)(nQuantumNonce >> 16));
    seed.push_back((unsigned char)(nQuantumNonce >> 24));
    const unsigned char* dom = (const unsigned char*)QUANTUM_CIRCUIT_DOMAIN;
    seed.insert(seed.end(), dom, dom + std::strlen(QUANTUM_CIRCUIT_DOMAIN));

    std::vector<unsigned char> stream;
    QuantumExpandSeed(seed, (size_t)4 * layers, stream);

    QSim::Statevector sv(nQubits);
    for (int l = 0; l < layers; ++l) {
        const unsigned int b0 = stream[4 * l + 0];
        const unsigned int b1 = stream[4 * l + 1];
        const unsigned int b2 = stream[4 * l + 2];
        const unsigned int b3 = stream[4 * l + 3];

        const int w = (int)(b1 % (unsigned)nQubits);
        switch (b0 % 5) {
        case 0: sv.H(w); break;
        case 1: sv.X(w); break;
        case 2: sv.Z(w); break;
        case 3: sv.S(w); break;
        default: sv.T(w); break;
        }

        const int c = (int)(b2 % (unsigned)nQubits);
        const int t = (int)(((b2 % (unsigned)nQubits) + 1 + (b3 % (unsigned)(nQubits - 1))) % (unsigned)nQubits);
        sv.CNOT(c, t);
    }

    measurementOut = (uint32_t)sv.ArgmaxMeasurement();
    std::vector<unsigned char> sampled;
    sv.SampledBytes(sampled, QUANTUM_FINGERPRINT_SAMPLES);
    CSHA256().Write(sampled.data(), sampled.size()).Finalize(fingerprintOut);
}

/* Difficulty hash: SHA256("QDIFF" || fp || LE32(meas) || LE32(nonce)). */
static void QuantumComputeDiffHash(const unsigned char fingerprint[32], uint32_t measurement,
                                   uint32_t nQuantumNonce, unsigned char out[32])
{
    CSHA256 h;
    h.Write((const unsigned char*)"QDIFF", 5);
    h.Write(fingerprint, 32);
    unsigned char le[4];
    le[0] = (unsigned char)measurement;         le[1] = (unsigned char)(measurement >> 8);
    le[2] = (unsigned char)(measurement >> 16); le[3] = (unsigned char)(measurement >> 24);
    h.Write(le, 4);
    le[0] = (unsigned char)(nQuantumNonce);       le[1] = (unsigned char)(nQuantumNonce >> 8);
    le[2] = (unsigned char)(nQuantumNonce >> 16); le[3] = (unsigned char)(nQuantumNonce >> 24);
    h.Write(le, 4);
    h.Finalize(out);
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */
uint256 GetQuantumDifficultyHash(const std::vector<unsigned char>& quantumProof)
{
    uint256 r;
    if (quantumProof.size() != QUANTUM_PROOF_SIZE) return r; // null
    std::memcpy(r.begin(), quantumProof.data() + QUANTUM_OFF_DIFFHASH, 32);
    return r;
}

bool IsQuantumProofWellFormed(const std::vector<unsigned char>& quantumProof, int nQubits)
{
    if (quantumProof.size() != QUANTUM_PROOF_SIZE) return false;
    if (std::memcmp(quantumProof.data(), QUANTUM_PROOF_MAGIC, 4) != 0) return false;
    if (quantumProof[4] != QUANTUM_PROOF_VERSION) return false;
    if (quantumProof[5] != (unsigned char)nQubits) return false;
    return true;
}

std::vector<unsigned char> GenerateQuantumProof(const uint256& blockHash,
                                                uint32_t nQuantumNonce,
                                                int nQubits)
{
    uint32_t measurement = 0;
    unsigned char fingerprint[32];
    QuantumSimulate(blockHash, nQuantumNonce, nQubits, measurement, fingerprint);

    std::vector<unsigned char> proof(QUANTUM_PROOF_SIZE, 0);
    std::memcpy(proof.data(), QUANTUM_PROOF_MAGIC, 4);
    proof[4] = QUANTUM_PROOF_VERSION;
    proof[5] = (unsigned char)nQubits;
    proof[7] = 0; // CPU backend
    std::memcpy(proof.data() + QUANTUM_OFF_FINGERPRINT, fingerprint, 32);
    proof[QUANTUM_OFF_MEASUREMENT + 0] = (unsigned char)(measurement);
    proof[QUANTUM_OFF_MEASUREMENT + 1] = (unsigned char)(measurement >> 8);
    proof[QUANTUM_OFF_MEASUREMENT + 2] = (unsigned char)(measurement >> 16);
    proof[QUANTUM_OFF_MEASUREMENT + 3] = (unsigned char)(measurement >> 24);
    unsigned char diffhash[32];
    QuantumComputeDiffHash(fingerprint, measurement, nQuantumNonce, diffhash);
    std::memcpy(proof.data() + QUANTUM_OFF_DIFFHASH, diffhash, 32);
    return proof;
}

bool ValidateQuantumProof(const std::vector<unsigned char>& quantumProof,
                          const uint256& blockHash,
                          uint32_t nQuantumNonce,
                          int nQubits)
{
    if (!IsQuantumProofWellFormed(quantumProof, nQubits)) return false;

    uint32_t measurement = 0;
    unsigned char fingerprint[32];
    QuantumSimulate(blockHash, nQuantumNonce, nQubits, measurement, fingerprint);

    if (std::memcmp(quantumProof.data() + QUANTUM_OFF_FINGERPRINT, fingerprint, 32) != 0)
        return false;
    unsigned char le4[4] = {(unsigned char)(measurement), (unsigned char)(measurement >> 8),
                            (unsigned char)(measurement >> 16), (unsigned char)(measurement >> 24)};
    if (std::memcmp(quantumProof.data() + QUANTUM_OFF_MEASUREMENT, le4, 4) != 0)
        return false;
    unsigned char diffhash[32];
    QuantumComputeDiffHash(fingerprint, measurement, nQuantumNonce, diffhash);
    return std::memcmp(quantumProof.data() + QUANTUM_OFF_DIFFHASH, diffhash, 32) == 0;
}

bool CheckQuantumDifficulty(const std::vector<unsigned char>& proof, const uint256& target)
{
    const uint256 diffhash = GetQuantumDifficultyHash(proof);
    if (diffhash.IsNull()) return false;
    // Same ordering convention as PoW: hash interpreted as little-endian
    // 256-bit number must be <= target.
    return !(target < diffhash);
}

