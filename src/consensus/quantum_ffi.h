// Copyright (c) 2026 The HCScoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
//
// HCScoin: FFI bridge to the panta-sim Rust library.
//
// When HCScoin is built with -DUSE_PANTA_SIM and linked against
// panta-sim/target/release/panta_sim (staticlib/cdylib), the quantum
// proof generation and verification are delegated to the Rust
// implementation (which can use the wgpu/Vulkan GPU backend). Without
// it, the portable C++ implementation in consensus/quantum.cpp is used;
// both are bit-identical (see test/standalone/quantum_test.cpp).

#ifndef HCSCOIN_CONSENSUS_QUANTUM_FFI_H
#define HCSCOIN_CONSENSUS_QUANTUM_FFI_H

#include <cstdint>

#ifdef USE_PANTA_SIM
extern "C" {

/** Semver packed: major<<16 | minor<<8 | patch. */
uint32_t panta_sim_version(void);

/** 1 if a GPU (wgpu/Vulkan) backend is available, else 0. */
int32_t panta_sim_gpu_available(void);

/** Default qubit count used by HCScoin mainnet. */
uint8_t panta_sim_default_qubits(void);

/**
 * Generate a 256-byte quantum proof for (block_hash, nonce).
 * Returns 0 on success; out receives 256 bytes.
 */
int32_t panta_sim_generate_proof(const uint8_t* block_hash,
                                 uint32_t nonce,
                                 uint8_t qubits,
                                 int32_t use_gpu,
                                 uint8_t* out);

/**
 * Verify a 256-byte quantum proof for (block_hash, nonce).
 * Returns 0 if valid, 1 if invalid, 2/3 on error.
 */
int32_t panta_sim_verify_proof(const uint8_t* block_hash,
                               uint32_t nonce,
                               const uint8_t* proof,
                               uint8_t qubits);

} // extern "C"
#endif // USE_PANTA_SIM

#endif // HCSCOIN_CONSENSUS_QUANTUM_FFI_H
