//! Quantum proof generation and verification.
//!
//! ## Proof layout (256 bytes)
//!
//! ```text
//! [0..4)    magic "QSP1"
//! [4]       format version (= 1)
//! [5]       qubit count (e.g. 27)
//! [6]       reserved (0)
//! [7]       flags (bit0: computed on GPU)
//! [8..40)   state fingerprint: SHA-256 over 1024 stride-sampled
//!           final-state amplitudes (re,im f32 LE pairs)
//! [40..44)  LE32 measurement (dominant computational basis state)
//! [44..76)  zero padding (reserved for future checkpoint data)
//! [76..108) difficulty hash: SHA-256("QDIFF" || fingerprint ||
//!           LE32(measurement) || LE32(nonce))  -- the value compared
//!           against the quantum target by CheckQuantumDifficulty()
//! [108..256) zero padding
//! ```
//!
//! Verification re-runs the identical simulation and compares every
//! committed field. This is the quantum analogue of PoW hash checking:
//! the work is the simulation itself.

use crate::circuit::simulate;
use crate::sha256::{sha256, Sha256};
use crate::PROOF_SIZE;

pub const PROOF_MAGIC: &[u8; 4] = b"QSP1";
pub const PROOF_VERSION: u8 = 1;
pub const FLAG_GPU: u8 = 1;
pub const FINGERPRINT_SAMPLES: usize = 1024;
pub const OFF_FINGERPRINT: usize = 8;
pub const OFF_MEASUREMENT: usize = 40;
pub const OFF_DIFFHASH: usize = 76;

/// A parsed quantum proof.
#[derive(Clone, Debug, PartialEq)]
pub struct QuantumProof {
    pub qubits: u8,
    pub flags: u8,
    pub fingerprint: [u8; 32],
    pub measurement: u32,
    pub diff_hash: [u8; 32],
}

impl QuantumProof {
    pub fn to_bytes(&self) -> [u8; PROOF_SIZE] {
        let mut out = [0u8; PROOF_SIZE];
        out[0..4].copy_from_slice(PROOF_MAGIC);
        out[4] = PROOF_VERSION;
        out[5] = self.qubits;
        out[7] = self.flags;
        out[OFF_FINGERPRINT..OFF_FINGERPRINT + 32].copy_from_slice(&self.fingerprint);
        out[OFF_MEASUREMENT..OFF_MEASUREMENT + 4].copy_from_slice(&self.measurement.to_le_bytes());
        out[OFF_DIFFHASH..OFF_DIFFHASH + 32].copy_from_slice(&self.diff_hash);
        out
    }

    pub fn parse(raw: &[u8]) -> Option<Self> {
        if raw.len() != PROOF_SIZE { return None; }
        if &raw[0..4] != PROOF_MAGIC { return None; }
        if raw[4] != PROOF_VERSION { return None; }
        let mut fingerprint = [0u8; 32];
        fingerprint.copy_from_slice(&raw[OFF_FINGERPRINT..OFF_FINGERPRINT + 32]);
        let mut diff_hash = [0u8; 32];
        diff_hash.copy_from_slice(&raw[OFF_DIFFHASH..OFF_DIFFHASH + 32]);
        Some(QuantumProof {
            qubits: raw[5],
            flags: raw[7],
            fingerprint,
            measurement: u32::from_le_bytes([raw[40], raw[41], raw[42], raw[43]]),
            diff_hash,
        })
    }
}

/// Compute the difficulty hash from the committed fields.
pub fn compute_diff_hash(fingerprint: &[u8; 32], measurement: u32, nonce: u32) -> [u8; 32] {
    let mut h = Sha256::new();
    h.update(b"QDIFF");
    h.update(fingerprint);
    h.update(&measurement.to_le_bytes());
    h.update(&nonce.to_le_bytes());
    h.finish()
}

/// Generate a quantum proof for (block_hash, nonce) by simulating the
/// derived circuit. `gpu_flag` records which backend was used.
pub fn generate_proof(block_hash: &[u8; 32], nonce: u32, qubits: u8, gpu_flag: bool) -> [u8; PROOF_SIZE] {
    let (measurement, sv) = simulate(block_hash, nonce, qubits);
    let fingerprint = sha256(&sv.sampled_bytes(FINGERPRINT_SAMPLES));
    let measurement32 = measurement as u32;
    let diff_hash = compute_diff_hash(&fingerprint, measurement32, nonce);
    let proof = QuantumProof {
        qubits,
        flags: if gpu_flag { FLAG_GPU } else { 0 },
        fingerprint,
        measurement: measurement32,
        diff_hash,
    };
    proof.to_bytes()
}

/// Verify a quantum proof: re-simulate and compare all committed fields.
/// Returns true iff the proof is internally consistent for the given
/// (block_hash, nonce). Difficulty checking is done separately by the
/// caller (CheckQuantumDifficulty in consensus code).
pub fn verify_proof(block_hash: &[u8; 32], nonce: u32, raw: &[u8], qubits: u8) -> bool {
    let parsed = match QuantumProof::parse(raw) {
        Some(p) => p,
        None => return false,
    };
    if parsed.qubits != qubits { return false; }
    let (measurement, sv) = simulate(block_hash, nonce, qubits);
    let fingerprint = sha256(&sv.sampled_bytes(FINGERPRINT_SAMPLES));
    if fingerprint != parsed.fingerprint { return false; }
    if measurement as u32 != parsed.measurement { return false; }
    compute_diff_hash(&fingerprint, measurement as u32, nonce) == parsed.diff_hash
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn proof_roundtrip_and_tamper() {
        let h = [3u8; 32];
        let raw = generate_proof(&h, 42, 12, false);
        assert!(verify_proof(&h, 42, &raw, 12));
        // wrong nonce
        assert!(!verify_proof(&h, 43, &raw, 12));
        // wrong block hash
        let h2 = [4u8; 32];
        assert!(!verify_proof(&h2, 42, &raw, 12));
        // tampered fingerprint
        let mut bad = raw;
        bad[9] ^= 1;
        assert!(!verify_proof(&h, 42, &bad, 12));
        // tampered measurement
        let mut bad2 = raw;
        bad2[40] ^= 1;
        assert!(!verify_proof(&h, 42, &bad2, 12));
        // wrong qubit count
        assert!(!verify_proof(&h, 42, &raw, 11));
    }

    #[test]
    fn parse_rejects_garbage() {
        assert!(QuantumProof::parse(&[0u8; 256]).is_none());
        assert!(QuantumProof::parse(&[0u8; 10]).is_none());
    }
}
