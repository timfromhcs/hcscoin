# HCScoin Testnet Guide

## Overview

HCScoin maintains four test networks:

| Network | P2P Port | RPC Port | Genesis | Qubits |
|---------|----------|----------|---------|--------|
| Testnet3 (deprecated) | 28334 | 28336 | 00000266bff9... (nTime 1784505900) | 27 |
| Testnet4 | 28338 | 28339 | 0000018e8772... (nTime 1784506800) | 27 |
| Signet | 28340 | 28341 | 000001af6eeb... (nTime 1784506500) | 27 |
| Regtest | 28335 | 28337 | 025cd4a4693b... (nTime 1784506200) | 12 |

## Quick Start (Testnet4)

```bash
# Start node
./hcscoind -testnet4 -daemon -miner=auto -quantumgpu=1

# Monitor sync
./hcscoin-cli -testnet4 getblockchaininfo

# Generate testnet addresses
./hcscoin-cli -testnet4 getnewaddress "post-quantum wallet"
```

## Faucet

The **HCScoin testnet faucet** is available at:  
**`https://faucet.hcscoin.org`** (CLI alternative below)

### CLI Faucet Request

```bash
./hcscoin-cli -testnet4 faucet-request --address=<hcs1...address>
```

The faucet server (runs on port 28380) validates proof-of-work to prevent
abuse. See `testnet/faucet.py` for the server-side implementation.

## Mining on Testnet

GPU mining is available for testnet. The quantum difficulty is set
generously (2^255 target → ~2 simulations per block).

```bash
./quantum-miner/vulkan_miner -m testnet4 -p 28338 -u http://localhost:28339
```

## Wallet

Generate a wallet and fund it via the faucet, then send:

```bash
./hcscoin-cli -testnet4 getbalance
./hcscoin-cli -testnet4 sendtoaddress <address> 5.0 "test payment"
```

Use `-regtest` for local development (12 qubits → fast):
```bash
./hcscoind -regtest -daemon -gen=1 -quantumqubits=12
```
