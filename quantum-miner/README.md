# HCScoin Quantum Miner — Vulkan GPU Backend

This directory contains the Vulkan-based GPU miner for the HCScoin dual
consensus (SHA-256d PoW + Quantum PoW). It is a port of the rpow2
Vulkan miner adapted for the HCScoin quantum circuit (Clifford+T,
statevector simulation on the GPU).

## Build

```bash
cd quantum-miner
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**Prerequisites**: Vulkan SDK 1.3+, glslangValidator, CMake 3.22+

## Usage

```bash
./vulkan_miner [options]

Options:
  -m, --mode <mainnet|testnet4|signet|regtest>  Network mode (default: mainnet)
  -p, --port <port>                               RPC port (auto-detect if omitted)
  -u, --rpc-url <url>                             Full RPC URL e.g. http://localhost:28332
  -U, --rpcuser <user>
  -P, --rpcpassword <pass>
  -g, --gpu <id>                                  GPU device ID (default: 0)
  -q, --qubits <n>                                Quantum qubits (default: 27)
  -t, --threads <n>                               CPU threads for fallback (default: all)
  -h, --help
```

## Shaders

| File | Purpose |
|------|---------|
| `shaders/sha256d.comp` | SHA-256d PoW hashing (one grinds nNonce on GPU) |
| `shaders/quantum_gates.comp` | Panta-Sim quantum circuit (Clifford+T gate applications) |
| `shaders/common.glsl` | Shared definitions (constants, types) |

## GPU Requirements

- Vulkan 1.2 or higher
- 8 GB VRAM minimum for 27-qubit simulation (1 GiB statevector + buffers)
- NVIDIA RTX 2060+, AMD RX 5700+, Intel ARC A750+
- Apple Silicon with Metal via MoltenVK

## Fallback

If no GPU is detected, the miner transparently falls back to the CPU
implementation (`src/consensus/quantum.cpp`). Use `-t <n>` to control
CPU thread count.
