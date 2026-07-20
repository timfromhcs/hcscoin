//! C ABI for linking panta-sim into hcscoind (see
//! `src/consensus/quantum_ffi.h`). All functions are thread-safe and
//! perform no I/O.
//!
//! Return codes: 0 = success, 1 = verification failed, 2 = bad arguments,
//! 3 = allocation/simulation error.

use crate::proof;
use crate::{DEFAULT_QUBITS, MAX_QUBITS, PROOF_SIZE};

/// Library version (semver packed: major<<16 | minor<<8 | patch).
#[no_mangle]
pub extern "C" fn panta_sim_version() -> u32 {
    (1 << 16) | (0 << 8) | 0
}

/// Returns 1 if a GPU (wgpu/Vulkan) backend is available, else 0.
#[no_mangle]
pub extern "C" fn panta_sim_gpu_available() -> i32 {
    #[cfg(feature = "gpu")]
    {
        crate::gpu::gpu_available() as i32
    }
    #[cfg(not(feature = "gpu"))]
    {
        0
    }
}

/// Default qubit count used by HCScoin mainnet.
#[no_mangle]
pub extern "C" fn panta_sim_default_qubits() -> u8 {
    DEFAULT_QUBITS
}

/// Generate a 256-byte quantum proof for (block_hash, nonce).
///
/// * `block_hash`: pointer to 32 bytes (internal uint256 byte order).
/// * `out`: pointer to a writable 256-byte buffer.
/// * `use_gpu`: nonzero to request the GPU backend (falls back to CPU).
#[no_mangle]
pub extern "C" fn panta_sim_generate_proof(
    block_hash: *const u8,
    nonce: u32,
    qubits: u8,
    use_gpu: i32,
    out: *mut u8,
) -> i32 {
    if block_hash.is_null() || out.is_null() {
        return 2;
    }
    if qubits == 0 || qubits > MAX_QUBITS {
        return 2;
    }
    let mut hash = [0u8; 32];
    unsafe {
        core::ptr::copy_nonoverlapping(block_hash, hash.as_mut_ptr(), 32);
    }

    #[cfg(feature = "gpu")]
    let used_gpu = use_gpu != 0 && crate::gpu::gpu_available();
    #[cfg(not(feature = "gpu"))]
    let used_gpu = false;
    let _ = use_gpu;

    let result = std::panic::catch_unwind(|| {
        #[cfg(feature = "gpu")]
        if used_gpu {
            if let Some(p) = crate::gpu::generate_proof_gpu(&hash, nonce, qubits) {
                return p;
            }
        }
        proof::generate_proof(&hash, nonce, qubits, used_gpu)
    });
    match result {
        Ok(p) => {
            unsafe {
                core::ptr::copy_nonoverlapping(p.as_ptr(), out, PROOF_SIZE);
            }
            0
        }
        Err(_) => 3,
    }
}

/// Verify a 256-byte quantum proof for (block_hash, nonce).
/// Returns 0 if valid, 1 if invalid, 2 on bad arguments, 3 on error.
#[no_mangle]
pub extern "C" fn panta_sim_verify_proof(
    block_hash: *const u8,
    nonce: u32,
    proof_ptr: *const u8,
    qubits: u8,
) -> i32 {
    if block_hash.is_null() || proof_ptr.is_null() {
        return 2;
    }
    if qubits == 0 || qubits > MAX_QUBITS {
        return 2;
    }
    let mut hash = [0u8; 32];
    let mut raw = [0u8; PROOF_SIZE];
    unsafe {
        core::ptr::copy_nonoverlapping(block_hash, hash.as_mut_ptr(), 32);
        core::ptr::copy_nonoverlapping(proof_ptr, raw.as_mut_ptr(), PROOF_SIZE);
    }
    let result = std::panic::catch_unwind(|| proof::verify_proof(&hash, nonce, &raw, qubits));
    match result {
        Ok(true) => 0,
        Ok(false) => 1,
        Err(_) => 3,
    }
}
