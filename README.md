# HCScoin

HCScoin is an experimental Bitcoin-Core-derived cryptocurrency project focused on post-quantum transaction primitives, dual-consensus research, and reproducible headless builds.

> Status: research/proof-of-concept. Do not treat this repository as production-ready money software without an independent security and consensus review.

## What is included

- Post-quantum research code around Dilithium / ML-DSA style signatures.
- Quantum-proof-of-work validation experiments.
- Bitcoin Core style daemon and RPC client.
- Headless Linux build path for CI and Docker.
- GitHub Actions workflows for source checks, local-style Linux builds, tests, and Docker image smoke tests.

## Repository layout

- `src/` — HCScoin/Bitcoin-Core-derived C++ source tree.
- `src/consensus/` — consensus and quantum PoW related code.
- `src/crypto/` — cryptographic primitives and post-quantum experiments.
- `panta-sim/` — Rust quantum simulator experiments.
- `quantum-miner/` — miner experiments.
- `test/` — unit, functional, fuzz, lint, and standalone tests.
- `docs/` and `doc/` — project and upstream-style documentation.
- `.github/workflows/` — GitHub Actions CI and cloud build workflows.
- `Dockerfile` / `docker-compose.yml` — local container build and node runtime.

## Build locally

The maintained default build is a headless Linux build that creates:

- `build/bin/hcscoin`
- `build/bin/hcscoind`
- `build/bin/hcscoin-cli`

### Ubuntu 24.04 / 22.04

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

git clone https://github.com/timfromhcs/hcscoin.git
cd hcscoin

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_GUI=OFF \
  -DENABLE_WALLET=OFF \
  -DENABLE_IPC=OFF \
  -DWITH_ZMQ=OFF

cmake --build build --parallel 2
```

### Smoke test

```bash
build/bin/hcscoind --version
build/bin/hcscoin-cli --version
```

## Run a node

```bash
mkdir -p ~/.hcscoin
cat > ~/.hcscoin/hcscoin.conf <<'EOF'
server=1
rpcuser=hcscoin
rpcpassword=change-this-password
EOF

build/bin/hcscoind -daemon=0 -printtoconsole
```

In another terminal:

```bash
build/bin/hcscoin-cli getblockchaininfo
```

## Docker

Build and run a local image:

```bash
docker build -t hcscoin:local .
docker run --rm hcscoin:local --version
```

Run via Compose:

```bash
docker compose up --build
```

RPC is bound to `127.0.0.1:28332` by default in `docker-compose.yml`.

## Network defaults

- Mainnet P2P: `28333`
- Mainnet RPC: `28332`
- Regtest P2P: `28335`
- Testnet/Testnet4/Signet ports are inherited from the project chain parameter configuration.

## CI / cloud build

Two GitHub Actions workflows are maintained:

- `.github/workflows/ci.yml` — fast source/configure checks and binary-name verification.
- `.github/workflows/build.yml` — full Linux headless build, unit tests, artifact packaging, and Docker image smoke test.

Both workflows run on pushes and pull requests to `master` and `main`.

## Security notes

- This project changes consensus and cryptography-sensitive code.
- Keep wallet support disabled unless you are actively working on wallet code and have reviewed the dependency requirements.
- Keep IPC disabled for minimal builds unless Cap'n Proto/libmultiprocess support is explicitly needed.
- Never run public RPC without authentication and network restrictions.

## License

HCScoin is released under the MIT license. See [`COPYING`](COPYING).

This repository is derived from Bitcoin Core, which is also MIT licensed.
