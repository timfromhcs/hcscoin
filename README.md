# ⚡ HCScoin — Post-Quantum Dual-Consensus Cryptocurrency

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](COPYING)

**HCScoin** is a fully featured cryptocurrency based on Bitcoin Core with massive modifications:

- **Post-Quantum Cryptography** — CRYSTALS-Dilithium Level 5 (ML-DSA-87) instead of ECDSA
- **Dual-Consensus** — SHA-256d PoW + Quantum Proof-of-Work (27-qubit statevector simulation)
- **Panta-Sim Quantum Emulator** — GPU-accelerated (Vulkan/wgpu) Clifford+T circuit verification
- **GPU Mining** — Vulkan-based miner with automatic CPU fallback
- **50/50 Economic Model** — Half miner rewards, half airdrop lottery

---

## Quick Start

```bash
# Build from source (Ubuntu 22.04+)
cd hcscoin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run tests
cd build && ctest --output-on-failure -j$(nproc)

# Start mainnet node
./build/src/hcscoind

# Query the chain
./build/src/hcscoin-cli getblockchaininfo
```

### Docker

```bash
docker compose up -d
```

---

## Network Configuration

| Network | P2P Port | RPC Port | Magic Bytes |
|---------|----------|----------|-------------|
| Mainnet | 28333 | 28332 | `F2 E5 D4 C3` |
| Testnet3 | 28336 | 28337 | `F2 E5 D4 C3` |
| Testnet4 | 28338 | 28339 | `F2 E5 D4 C3` |
| Signet | 28340 | 28341 | `F2 E5 D4 C3` |
| Regtest | 28335 | 28337 | `F2 E5 D4 C3` |

Data directory: `~/.hcscoin/` · Config file: `~/.hcscoin/hcscoin.conf`

---

## Key Features

### 🔐 Post-Quantum Security

HCScoin replaces ECDSA with **CRYSTALS-Dilithium Level 5** (ML-DSA-87, NIST FIPS 204):

| Parameter | Value |
|-----------|-------|
| Public Key | 2,592 bytes |
| Secret Key | 4,896 bytes |
| Signature | 4,627 bytes |
| Security Level | ≥ 256-bit classical, ≥ 128-bit quantum |
| NIST Level | 5 (highest) |

Address format: `hcs1...` (Bech32m variant)

### ⚛️ Dual-Consensus (SHA-256d + Quantum PoW)

Every block satisfies **both**:
1. SHA-256d hash below target (Bitcoin-compatible)
2. Quantum proof fingerprint from 27-qubit Clifford+T simulation

### 📊 Economic Model

| Destination | Share |
|-------------|-------|
| Miner (reduced block reward + Tx fees) | 50% |
| Airdrop Contract (lottery pool) | 50% |

**Monthly Airdrop Lottery**: 10 active addresses randomly selected on-chain.

---

## Repository Structure

| Path | Description |
|------|-------------|
| `src/` | C++ source (Bitcoin Core modified) |
| `src/crypto/dilithium.c` | ML-DSA-87 implementation |
| `src/consensus/quantum.cpp` | Quantum PoW validation |
| `src/consensus/quantum_ffi.cpp` | Rust FFI bridge |
| `src/consensus/airdrop.cpp` | Airdrop lottery contract |
| `panta-sim/` | Quantum simulator (Rust) |
| `quantum-miner/` | Vulkan GPU miner |
| `test/standalone/` | Cross-implementation tests |
| `docs/` | Documentation |
| `agents/` | AI agent configs |
| `testnet/` | Testnet config and faucet |

---

## Testing

```bash
# Standalone Dilithium tests (13 tests)
cc -O2 -DDILITHIUM_CUSTOM_RANDOMBYTES -I src \
    src/crypto/dilithium.c test/standalone/dilithium_test.c \
    -o dilithium_test && ./dilithium_test

# Standalone Quantum PoW tests (24 tests)
c++ -O2 -std=c++20 -msse4.1 -I src \
    src/consensus/quantum.cpp src/crypto/sha256.cpp \
    src/crypto/sha256_sse4.cpp test/standalone/quantum_test.cpp \
    -o quantum_test && ./quantum_test

# Panta-Sim Rust tests (8 tests)
cd panta-sim && cargo test --release
```

**Test results**: Dilithium 13/13 ✅ · Quantum PoW 24/24 ✅ · Panta-Sim 8/8 ✅

---

## License

HCScoin is released under the terms of the MIT license. See [COPYING](COPYING).

---

*Based on Bitcoin Core (MIT), CRYSTALS-Dilithium (public domain).*



