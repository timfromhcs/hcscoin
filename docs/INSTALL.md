# Build and install HCScoin

This guide documents the maintained headless build used by local development, Docker, and GitHub Actions.

## Supported build

- OS: Ubuntu 22.04 or 24.04 x86_64
- Compiler: GCC 11+ or Clang 16+
- Build system: CMake 3.22+ with Ninja
- Default deliverables:
  - `build/bin/hcscoin`
  - `build/bin/hcscoind`
  - `build/bin/hcscoin-cli`

Wallet, GUI, ZMQ, and IPC can be enabled later, but the default reliable build keeps them disabled to minimize external dependencies.

## Dependencies

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential \
  ccache \
  cmake \
  git \
  libboost-dev \
  libevent-dev \
  libsqlite3-dev \
  ninja-build \
  pkg-config \
  python3
```

Optional features require extra packages:

- ZMQ: `libzmq3-dev`
- Wallet: Berkeley DB / SQLite wallet dependencies depending on configuration
- IPC: Cap'n Proto and libmultiprocess dependencies
- GUI: Qt 6 development packages

## Configure

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_GUI=OFF \
  -DENABLE_WALLET=OFF \
  -DENABLE_IPC=OFF \
  -DWITH_ZMQ=OFF
```

## Build

```bash
cmake --build build --parallel 2
```

Use a larger `--parallel` value on machines with enough RAM.

## Verify

```bash
build/bin/hcscoind --version
build/bin/hcscoin-cli --version
```

## Install locally

```bash
sudo install -m 0755 build/bin/hcscoin /usr/local/bin/hcscoin
sudo install -m 0755 build/bin/hcscoind /usr/local/bin/hcscoind
sudo install -m 0755 build/bin/hcscoin-cli /usr/local/bin/hcscoin-cli
```

## Run

```bash
mkdir -p ~/.hcscoin
cat > ~/.hcscoin/hcscoin.conf <<'EOF'
server=1
rpcuser=hcscoin
rpcpassword=change-this-password
EOF

hcscoind -daemon=0 -printtoconsole
```

Query from another terminal:

```bash
hcscoin-cli getblockchaininfo
```

## Docker

```bash
docker build -t hcscoin:local .
docker run --rm hcscoin:local --version
docker compose up --build
```
