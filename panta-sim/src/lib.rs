//! # Panta-Sim — HCScoin quantum circuit emulator
//!
//! Deterministic statevector simulator used for HCScoin Quantum
//! Proof-of-Work. A 27-qubit Clifford+T circuit is derived from the block
//! hash; miners simulate it, and the resulting measurement + state
//! fingerprint form the quantum proof stored in the block header.
//!
//! Determinism contract (critical for consensus):
//! * Only gates whose amplitudes are exactly representable in `f32` are
//!   used (X, Z, S, T, H with the constant `FRAC_1_SQRT_2`, CNOT).
//! * No `sin`/`cos`, no FMA-dependent expressions, fixed evaluation order.
//! * The C++ fallback in `src/consensus/quantum.cpp` reproduces this file
//!   bit-for-bit and is continuously cross-checked by the test suite.
//!
//! The optional `gpu` feature adds a wgpu/Vulkan backend with identical
//! numerics (f32, same op order); the CPU backend is always available and
//! is used automatically when no suitable GPU is present.

// unsafe is confined to ffi.rs (raw pointer marshalling for the C ABI).

pub mod sha256;
pub mod statevector;
pub mod circuit;
pub mod proof;
pub mod ffi;

#[cfg(feature = "gpu")]
pub mod gpu;

/// Number of qubits used for mainnet quantum proofs (2^27 statevector).
pub const DEFAULT_QUBITS: u8 = 27;
/// Hard upper bound: 2^30 complex f32 amplitudes = 8 GiB.
pub const MAX_QUBITS: u8 = 30;
/// Layers applied per circuit (circuit depth = 3 * qubits).
pub const LAYERS_PER_QUBIT: u32 = 3;
/// Proof size in bytes (must match QUANTUM_PROOF_SIZE on the C++ side).
pub const PROOF_SIZE: usize = 256;

pub use proof::{generate_proof, verify_proof, QuantumProof};
