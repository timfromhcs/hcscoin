# HCScoin Build & Install Guide

## Prerequisites

- **OS**: Ubuntu 22.04 / 24.04 x86_64 (recommended), macOS 14+, Windows 10+ (WSL2 or MSVC)
- **Compiler**: GCC 11+ or Clang 16+, or MSVC 17.x (VS BuildTools 2024+)
- **CMake** ≥ 3.22
- **Rust** (for the Panta-Sim quantum emulator): `curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh`
- **GPU Driver** (optional, for quantum proof acceleration):
  - Vulkan 1.2+ driver for your GPU (NVIDIA ≥ 535, AMD ≥ 23.10, Intel ≥ 107.8316)
  - Apple Metal supported via MoltenVK

## Quick Build

### From source (Ubuntu)

```bash
# Dependencies
sudo apt install build-essential cmake libevent-dev libsqlite3-dev \
    libboost-dev libboost-multi-index-dev

# Clone and build
cd /workspace
git clone https://github.com/hcscoin/hcscoin
cd hcscoin
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_GUI=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON \
    -DENABLE_EXTERNAL_SIGNER=OFF -DBUILD_TESTS=OFF
cmake --build build -j$(nproc)

# Build the Panta-Sim quantum backend
cd panta-sim
cargo build --release
cd ..

# Create symlinks for the daemon and CLI
ln -sf build/src/hcscoind hcscoind
ln -sf build/src/hcscoin-cli hcscoin-cli
```

### Windows (MSVC + vcpkg)

Open "x64 Native Tools Command Prompt for VS" (or Developer PowerShell):

```powershell
cd C:\hcscoin
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DBUILD_GUI=OFF -DWITH_ZMQ=OFF -DENABLE_WALLET=ON `
    -DENABLE_EXTERNAL_SIGNER=OFF -DBUILD_TESTS=OFF
ninja -C build
```

### Windows (WSL2 Ubuntu)

Same as Linux build above; GPU passthrough requires the Vulkan loader:

```bash
sudo apt install mesa-vulkan-drivers libvulkan1
```

## Configuration

Create `~/.hcscoin/hcscoin.conf`:

```
# HCScoin mainnet configuration
rpcuser=hcscoin
rpcpassword=<your-strong-password>
server=1
daemon=1
```

## Running

```bash
# Start daemon
./hcscoind -daemon

# Check status
./hcscoin-cli getblockchaininfo

# Generate a new post-quantum Dilithium address
./hcscoin-cli getnewaddress

# Send test transaction
./hcscoin-cli sendtoaddress <address> 0.1
```

## Testnet

```bash
./hcscoind -testnet -daemon
./hcscoin-cli -testnet getblockchaininfo
```

## GPU Miner Setup

See [MINING.md](MINING.md).
