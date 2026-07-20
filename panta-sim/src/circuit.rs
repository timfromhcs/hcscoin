//! Deterministic circuit derivation and simulation.
//!
//! The circuit is a function of (block_hash, nonce, qubits) only. All
//! nodes derive the identical Clifford+T circuit. Layout per layer `l`
//! (of `3 * qubits` layers), with `stream = expand_seed(seed, 4*layers)`:
//!
//! ```text
//! g = stream[4l+0] % 5        -> gate {H, X, Z, S, T}
//! w = stream[4l+1] % qubits   -> target wire of the single-qubit gate
//! c = stream[4l+2] % qubits   -> CNOT control
//! t = (c + 1 + stream[4l+3] % (qubits-1)) % qubits -> CNOT target
//! ```
//!
//! This specification is implemented identically in
//! `src/consensus/quantum.cpp` (see QuantumSimulateCpu).

use crate::sha256::expand_seed;
use crate::statevector::Statevector;
use crate::LAYERS_PER_QUBIT;

/// Domain separation tag for circuit derivation.
pub const CIRCUIT_DOMAIN: &[u8] = b"HCScoin-QuantumPoW-v1";

/// Build the full seed stream for a (block_hash, nonce) pair.
pub fn circuit_stream(block_hash: &[u8; 32], nonce: u32, qubits: u8) -> Vec<u8> {
    let layers = (qubits as usize) * (LAYERS_PER_QUBIT as usize);
    let mut seed = Vec::with_capacity(32 + 4 + CIRCUIT_DOMAIN.len());
    seed.extend_from_slice(block_hash);
    seed.extend_from_slice(&nonce.to_le_bytes());
    seed.extend_from_slice(CIRCUIT_DOMAIN);
    expand_seed(&seed, 4 * layers)
}

/// Simulate the derived circuit; returns (measurement, sampled_amplitudes).
pub fn simulate(block_hash: &[u8; 32], nonce: u32, qubits: u8) -> (u64, Statevector) {
    let stream = circuit_stream(block_hash, nonce, qubits);
    let n = qubits as usize;
    let layers = n * (LAYERS_PER_QUBIT as usize);
    let mut sv = Statevector::new(qubits).expect("qubits within limit");

    for l in 0..layers {
        let b0 = stream[4 * l] as usize;
        let b1 = stream[4 * l + 1] as usize;
        let b2 = stream[4 * l + 2] as usize;
        let b3 = stream[4 * l + 3] as usize;

        let w = (b1 % n) as u8;
        match b0 % 5 {
            0 => sv.h(w),
            1 => sv.x(w),
            2 => sv.z(w),
            3 => sv.s(w),
            _ => sv.t(w),
        }

        let c = (b2 % n) as u8;
        let t = ((b2 % n) + 1 + (b3 % (n - 1))) % n;
        sv.cnot(c, t as u8);
    }

    (sv.argmax_measurement(), sv)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn deterministic_and_nonce_sensitive() {
        let h = [7u8; 32];
        let (m1, _s1) = simulate(&h, 0, 10);
        let (m1b, _s1b) = simulate(&h, 0, 10);
        assert_eq!(m1, m1b);
        let (_m2, s2) = simulate(&h, 1, 10);
        let (_m1c, s1c) = simulate(&h, 0, 10);
        // Different nonce -> different final state (with overwhelming prob.)
        assert_ne!(s1c.sampled_bytes(64), s2.sampled_bytes(64));
        // Unitarity preserved
        assert!((s2.total_prob() - 1.0).abs() < 1e-3);
    }
}
