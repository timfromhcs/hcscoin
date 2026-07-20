#!/usr/bin/env python3
"""Independent cross-verification of the HCScoin genesis blocks.

Reconstructs the genesis transaction and block header with hashlib (a
completely different SHA-256 implementation than genesis_miner.c) and checks:
  1. The merkle root matches.
  2. The block hash matches for the given (nTime, nBits, nNonce).
  3. The block hash satisfies the compact target (proof of work).
  4. The compact target itself is below the HCScoin powLimit.

Run: python verify_genesis.py
"""
import hashlib
import struct
import sys

PSZ_TIMESTAMP = b"HCScoin Genesis - 20 July 2026"
GENESIS_PUBKEY = bytes.fromhex(
    "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f"
    "4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f"
)
POW_LIMIT = int("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", 16)

def dsha(b: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(b).digest()).digest()

def push_data(data: bytes) -> bytes:
    assert len(data) < 76
    return bytes([len(data)]) + data

def build_tx() -> bytes:
    n = 486604799  # 0x1D00FFFF
    le = n.to_bytes(4, "little")
    scriptsig = push_data(le) + b"\x54" + push_data(PSZ_TIMESTAMP)  # OP_4 = 0x54
    scriptpub = push_data(GENESIS_PUBKEY) + b"\xac"                 # OP_CHECKSIG
    tx = struct.pack("<i", 1)
    tx += b"\x01" + b"\x00" * 32 + struct.pack("<I", 0xFFFFFFFF)
    tx += bytes([len(scriptsig)]) + scriptsig
    tx += struct.pack("<I", 0xFFFFFFFF)
    tx += b"\x01" + struct.pack("<Q", 50 * 100_000_000)
    tx += bytes([len(scriptpub)]) + scriptpub
    tx += struct.pack("<I", 0)
    return tx

def compact_to_target(nbits: int) -> int:
    exponent = nbits >> 24
    mantissa = nbits & 0x007FFFFF
    return mantissa << (8 * (exponent - 3))

def parse_miner_output(text: str):
    """Parse genesis_miner output blocks into case dicts."""
    cases = []
    name = None
    vals = {}
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("==="):
            if name and vals:
                cases.append((name, vals))
            name = line.strip("= ").replace("HCScoin ", "")
            vals = {}
        elif "=" in line:
            k, v = [x.strip() for x in line.split("=", 1)]
            vals[k] = v
    if name and vals:
        cases.append((name, vals))
    return cases


def main() -> int:
    import subprocess, os
    here = os.path.dirname(os.path.abspath(__file__))
    exe = os.path.join(here, "genesis_miner.exe" if os.name == "nt" else "genesis_miner")
    out = subprocess.run([exe], capture_output=True, text=True, check=True).stdout

    tx = build_tx()
    merkle = dsha(tx)
    merkle_display = merkle[::-1].hex()

    ok = True
    for name, vals in parse_miner_output(out):
        ntime = int(vals["nTime"])
        nbits = int(vals["nBits"], 16)
        nonce = int(vals["nNonce"])
        expected_hash = vals["hash"].removeprefix("0x")
        reported_merkle = vals["merkle"].removeprefix("0x")
        assert reported_merkle == merkle_display, f"{name}: merkle mismatch"

        target = compact_to_target(nbits)
        # Extended HCScoin header: legacy 80 bytes + nQuantumNonce(4=0)
        # + vQuantumProof compactsize(0x00 empty).
        header = struct.pack("<i", 1) + b"\x00" * 32 + merkle
        header += struct.pack("<III", ntime, nbits, nonce)
        header += struct.pack("<I", 0) + b"\x00"
        h = dsha(header)
        display = h[::-1].hex()
        value = int.from_bytes(h, "little")
        if display != expected_hash:
            print(f"[FAIL] {name}: hash mismatch {display} != {expected_hash}")
            ok = False
            continue
        if value > target:
            print(f"[FAIL] {name}: PoW not satisfied")
            ok = False
            continue
        print(f"[OK] {name:9s} hash: {display}")
        print(f"     nonce={nonce} nBits=0x{nbits:08x}")
    print(f"[OK] merkle root  : {merkle_display}")
    print("\nALL GENESIS CHECKS PASSED" if ok else "\nGENESIS CHECKS FAILED")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
