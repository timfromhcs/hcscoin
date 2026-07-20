# HCScoin Quantum Proof-of-Work — Technical Specification

## Dual-Consensus Architecture

HCScoin uses a **dual consensus** mechanism: every block must satisfy both:

1. **SHA-256d proof-of-work** (identical to Bitcoin Core) — computed over the
   legacy 80-byte header (`CBlockHeader::GetHashProof()`).
2. **Quantum proof-of-work** — a 27-qubit Clifford+T circuit determined by
   the block's PoW hash and `nQuantumNonce` must produce a difficulty hash
   that meets the quantum target.

## Quantum Circuit Specification

### Circuit Derivation

The circuit is a deterministic function of `(hashProof, nQuantumNonce, qubits)`:

```
seed ← hashProof(32) || LE32(nQuantumNonce) || "HCScoin-QuantumPoW-v1"
stream ← expand_seed(seed, 4 * layers)  // SHA-256-based XOF
layers ← 3 * qubits

for l in 0..layers:
    g ← stream[4l+0] % 5  // gate selector (H, X, Z, S, T)
    w ← stream[4l+1] % qubits  // single-qubit target wire
    c ← stream[4l+2] % qubits  // CNOT control
    t ← (c + 1 + stream[4l+3] % (qubits-1)) % qubits  // CNOT target
    apply gate g on wire w
    apply CNOT(c, t)
```

### Gate Set (Clifford+T)

Only gates with **exactly representable f32 amplitudes** are used:

| Gate | Matrix | Expr (f32) |
|------|--------|-----------|
| H | 1/√2 [1 1; 1 -1] | k = 0.70710677 (0x3F3504F3) |
| X | [0 1; 1 0] | swap |
| Z | [1 0; 0 -1] | b ← -b |
| S | [1 0; 0 i] | b ← i·b |
| T | [1 0; 0 exp(iπ/4)] | b.re ← (b.re-b.im)·k, b.im ← (b.re+b.im)·k |
| CNOT | [I 0; 0 X] | conditional swap on target |

### Statevector Simulation

- **2²⁷ ≈ 134M complex f32 amplitudes** → 1 GiB RAM
- Each gate → O(2²⁷) work: single-qubit gates via pair iteration,
  CNOT via swap iteration.
- **GPU acceleration**: wgpu/Vulkan compute shader (optional, feature-gated).
  Detects compatible devices automatically, falls back to CPU.
- **Difficulty check**: `SHA256("QDIFF" || fingerprint || LE32(measurement) || LE32(nonce))`
  treated as a 256-bit integer and compared against `quantumPowLimit`.

## Proof Format (256 bytes)

```
[0..4)    magic "QSP1"
[4]       format version (=1)
[5]       qubit count
[6]       reserved
[7]       flags (bit0=GPU)
[8..40)   state fingerprint = SHA-256(1024 stride-sampled amplitudes)
[40..44)  LE32 measurement (argmax computational basis state)
[44..76)  reserved
[76..108) difficulty hash
[108..256) reserved
```

The header commits to the proof via `quantumFingerprint = SHA-256(proof)`.

## Cross-Implementation Determinism

Both the Rust (`panta-sim/`) and C++ (`src/consensus/quantum.cpp`)
implementations produce bit-identical outputs, verified by
`test/standalone/quantum_test.cpp` against vectors from `panta-sim/examples/vector.rs`.

## Performance Targets

| Metric | CPU (Ryzen 9) | GPU (RTX 4090) |
|--------|---------------|----------------|
| 27-qubit sim (1 attempt) | ~30 seconds | ~0.8 seconds |
| 12-qubit sim (regtest) | ~15 milliseconds | ~2 ms |
| Expected attempts/block | 16 (mainnet) | 16 (same target) |
| Block header validation | 30s CPU / 0.8s GPU | |
| Full block validation | same (adds tx checks) | |

## Airdrop Economic Model

50% of transaction fees → miner  
50% of transaction fees → airdrop contract  
Monthly lottery: 10 random active addresses split the pool  
See `CAirdropContract` in `src/consensus/airdrop.h`.
