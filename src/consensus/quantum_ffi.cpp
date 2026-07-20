// Copyright (c) 2026 The HCScoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
//
// HCScoin: C++ bridge to the panta-sim Rust FFI library.
//
// When USE_PANTA_SIM is defined and the library is linked at build time,
// this module calls the Rust functions directly. When not available,
// the portable C++ implementation in consensus/quantum.cpp is used via
// the quantum.h interface (both implementations are cross-tested to be
// bit-identical).

#include <consensus/quantum_ffi.h>
#include <consensus/quantum.h>

#include <cstring>
#include <cstdio>
#include <logging.h>

#ifdef USE_PANTA_SIM

// Link against libpanta_sim (static or CDYLIB).
// The symbols are declared in quantum_ffi.h.

#else // !USE_PANTA_SIM

// Stub: the FFI is not available, so every call is forwarded to the
// portable C++ implementation. Both implementations are bit-identical,
// verified by test/standalone/quantum_test.cpp.

int32_t panta_sim_version(void) { return (1 << 16) | (0 << 8) | 0; }

int32_t panta_sim_gpu_available(void) { return 0; }

uint8_t panta_sim_default_qubits(void) { return 27; }

int32_t panta_sim_generate_proof(const uint8_t* block_hash,
                                  uint32_t nonce,
                                  uint8_t qubits,
                                  int32_t use_gpu,
                                  uint8_t* out)
{
    (void)use_gpu;
    uint256 hash;
    std::memcpy(hash.begin(), block_hash, 32);
    auto proof = GenerateQuantumProof(hash, nonce, qubits);
    if (proof.size() != 256) return 3;
    std::memcpy(out, proof.data(), 256);
    return 0;
}

int32_t panta_sim_verify_proof(const uint8_t* block_hash,
                                uint32_t nonce,
                                const uint8_t* proof_ptr,
                                uint8_t qubits)
{
    uint256 hash;
    std::memcpy(hash.begin(), block_hash, 32);
    std::vector<uint8_t> proof(proof_ptr, proof_ptr + 256);
    return ValidateQuantumProof(proof, hash, nonce, qubits) ? 0 : 1;
}

#endif // USE_PANTA_SIM
