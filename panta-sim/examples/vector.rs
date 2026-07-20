//! Prints quantum proof test vectors for cross-implementation parity with
//! the C++ fallback (src/consensus/quantum.cpp).
//!
//! Run: cargo run --release --example vector

use panta_sim::proof::{compute_diff_hash, generate_proof, verify_proof};

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{:02x}", x)).collect()
}

fn main() {
    for (hash_byte, nonce, qubits) in [(3u8, 42u32, 12u8), (7u8, 0u32, 10u8), (0xabu8, 123456u32, 12u8)] {
        let hash = [hash_byte; 32];
        let proof = generate_proof(&hash, nonce, qubits, false);
        assert!(verify_proof(&hash, nonce, &proof, qubits));
        let fp = &proof[8..40];
        let meas = u32::from_le_bytes([proof[40], proof[41], proof[42], proof[43]]);
        let dh = &proof[76..108];
        println!("VECTOR hash={:02x}.. nonce={} qubits={}", hash_byte, nonce, qubits);
        println!("  fingerprint={}", hex(fp));
        println!("  measurement={}", meas);
        println!("  diffhash={}", hex(dh));
        println!("  proof={}", hex(&proof));
        // sanity: diffhash matches helper
        assert_eq!(dh, &compute_diff_hash(&proof[8..40].try_into().unwrap(), meas, nonce));
    }
}
