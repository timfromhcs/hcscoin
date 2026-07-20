# HCScoin API Reference

## Standard Bitcoin RPC Commands

All [Bitcoin Core RPC commands](https://developer.bitcoin.org/reference/rpc/)
are supported. The currency unit is **HCS** (1 HCS = 10^8 sat).

## HCScoin-Specific RPC Commands

### Quantum Proof-of-Work

| Command | Description |
|---------|-------------|
| `getquantuminfo` | Returns quantum difficulty, last proof time, GPU backend status |
| `generatequantumproof <nonce>` | Manually generate a quantum proof for the block under construction |
| `getquantumproof <blockhash>` | Retrieve the 256-byte quantum proof for a specific block |

### Post-Quantum Wallets (Dilithium)

| Command | Description |
|---------|-------------|
| `getnewdilithiumaddress` | Generate a new hcs1... Bech32m Dilithium address |
| `dilithiumgenkeypair` | Generate (public, private) Dilithium keypair (returns hex) |
| `signwithmessage <address> <message>` | Sign a message using the Dilithium private key |
| `verifymessage <address> <signature> <message>` | Verify a Dilithium signature |
| `listdilithiumkeys` | List all Dilithium public keys in the wallet |

### Airdrop & Economy

| Command | Description |
|---------|-------------|
| `getairdropinfo` | Airdrop balance, month counter, last draw time |
| `listlotterywinners <height>` | Winners of the monthly lottery at a given block height |

### GPU Miner

| Command | Description |
|---------|-------------|
| `setminingparams <gpu|cpu> <quantum_qubits=27> <gpu_id=0>` | Configure mining backend |
| `getminestatus` | Whether node is mining, GPU vs CPU, hash rate |

## Example Queries

```bash
# Quantum info
./hcscoin-cli getquantuminfo

# Generate a Dilithium address
./hcscoin-cli getnewdilithiumaddress

# Check airdrop contract
./hcscoin-cli getairdropinfo
```

## JSON-RPC Details

Bitcoin's standard API extends:
- `getblockchaininfo` adds: `quantumheight`, `quantumpowlimit`, `quantumqubits`
- `getblock` adds: `quantumnonce`, `quantumfingerprint` fields
- `getrawmempool` no change

## WebSocket Notifications

HCScoin optionally publishes quantum proof events via ZeroMQ:

| Topic | Payload |
|-------|---------|
| `quantumproof` | 256-byte proof for the latest block |
| `airdrop` | JSON with winners and amounts |
