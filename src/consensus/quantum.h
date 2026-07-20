// Copyright (c) 2026 The HCScoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
//
// HCScoin: Quantum Proof-of-Work consensus interface.
//
// Each HCScoin block must satisfy BOTH the classical SHA-256d PoW and a
// quantum proof produced by simulating a 27-qubit Clifford+T circuit in
// the Panta-Sim emulator (Rust, wgpu/Vulkan GPU backend with CPU
// fallback). The proof is committed to the block header via
// CBlockHeader::vQuantumProof / nQuantumNonce (see primitives/block.h).
//
// The verification algorithm in this file is a portable C++
// re-implementation of panta-sim/src/*.rs and must remain bit-identical
// (see test/standalone/quantum_test.cpp for the cross-implementation
// parity vectors).

#ifndef HCSCOIN_CONSENSUS_QUANTUM_H
#define HCSCOIN_CONSENSUS_QUANTUM_H

#include <cstddef>
#include <cstdint>
#include <vector>

class uint256;

/** Size of a quantum proof in bytes (must match panta-sim PROOF_SIZE). */
static constexpr size_t QUANTUM_PROOF_SIZE = 256;

/** Default qubit count for the quantum simulation (2^27 amplitudes). */
static constexpr int QUANTUM_DEFAULT_QUBITS = 27;

/** Proof magic bytes "QSP1" and format version. */
inline constexpr unsigned char QUANTUM_PROOF_MAGIC[4] = {'Q', 'S', 'P', '1'};
static constexpr unsigned char QUANTUM_PROOF_VERSION = 1;

/** Byte offsets inside a quantum proof. */
static constexpr size_t QUANTUM_OFF_FINGERPRINT = 8;
static constexpr size_t QUANTUM_OFF_MEASUREMENT = 40;
static constexpr size_t QUANTUM_OFF_DIFFHASH = 76;

/** Extract the 32-byte difficulty hash committed in a proof. */
uint256 GetQuantumDifficultyHash(const std::vector<unsigned char>& quantumProof);

/** Basic structural validation (magic, version, qubit count). */
bool IsQuantumProofWellFormed(const std::vector<unsigned char>& quantumProof, int nQubits);

/**
 * Validate a quantum proof for a block: re-simulate the circuit derived
 * from (blockHash, nQuantumNonce) and compare all committed fields.
 * Uses the panta-sim GPU backend when linked & available, else CPU.
 */
bool ValidateQuantumProof(const std::vector<unsigned char>& quantumProof,
                          const uint256& blockHash,
                          uint32_t nQuantumNonce,
                          int nQubits);

/**
 * Generate a quantum proof for mining (full simulation).
 * Returns a 256-byte proof (empty vector on failure).
 */
std::vector<unsigned char> GenerateQuantumProof(const uint256& blockHash,
                                                uint32_t nQuantumNonce,
                                                int nQubits);

/**
 * Check whether the proof's difficulty hash meets the quantum target
 * (interpreted like a PoW target: hash must be <= target).
 */
bool CheckQuantumDifficulty(const std::vector<unsigned char>& proof,
                            const uint256& target);

#endif // HCSCOIN_CONSENSUS_QUANTUM_H
