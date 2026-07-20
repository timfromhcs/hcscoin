#!/usr/bin/env python3
"""
HCScoin Testnet Faucet
=======================
Simple HTTP server that distributes small amounts of testnet HCS coins
to requestors.

Features:
  - Proof-of-work anti-spam (client must solve a small PoW challenge)
  - Per-address rate limiting (1 request per 24h)
  - Posts raw transaction via hcscoin-cli
  - Configurable faucet amount (default 10 test HCS)

Run:
  python faucet.py --cli=/usr/local/bin/hcscoin-cli --testnet4 --amount=10
"""

import argparse
import hashlib
import http.server
import json
import os
import random
import subprocess
import time
import threading
from urllib.parse import urlparse, parse_qs

# In-memory rate limit table { address: last_timestamp }
_ratelimit: dict[str, float] = {}
_ratelimit_lock = threading.Lock()

CHALLENGE_LEN = 32

def generate_challenge() -> str:
    return random.randbytes(CHALLENGE_LEN).hex()

def check_pow(challenge: str, nonce: int, difficulty: int = 20) -> bool:
    """Verify the client solved: SHA256(challenge || nonce) has `difficulty` leading zero bits."""
    data = (challenge + str(nonce)).encode()
    h = hashlib.sha256(data).digest()
    val = int.from_bytes(h, 'big')
    return val < (1 << (256 - difficulty))

class FaucetHandler(http.server.BaseHTTPRequestHandler):
    cli_path = "hcscoin-cli"
    is_testnet4 = True
    amount = 10  # HCS
    challenge_store: dict[str, float] = {}
    challenge_lock = threading.Lock()

    def do_GET(self):
        parsed = urlparse(self.path)
        params = parse_qs(parsed.query)

        if parsed.path == "/":
            self._serve_html()
        elif parsed.path == "/challenge":
            self._serve_challenge()
        elif parsed.path == "/claim":
            self._serve_claim(params)
        else:
            self.send_error(404)

    def _cli_args(self) -> list[str]:
        args = [self.cli_path]
        if self.is_testnet4:
            args.append("-testnet4")
        return args

    def _send_json(self, data: dict, status: int = 200):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def _serve_html(self):
        html = """<!DOCTYPE html>
<html><head><title>HCScoin Testnet Faucet</title>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font-family:sans-serif;max-width:600px;margin:40px auto;padding:0 20px}
input,button{font-size:16px;padding:8px;width:100%;margin:4px 0}
</style></head><body>
<h1>HCScoin Testnet Faucet</h1>
<p>Get free testnet HCS coins for development and testing.</p>
<form action="/claim" method="get">
<label>Your hcs1... address:</label>
<input type="text" name="address" placeholder="hcs1..." required>
<input type="hidden" name="challenge" value="placeholder">
<input type="hidden" name="nonce" value="0">
<button type="submit">Get HCS</button>
</form>
<p>Note: The faucet includes a JavaScript PoW solver (disabled when JS is off).
Use the CLI for automatic claiming.</p>
</body></html>"""
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.end_headers()
        self.wfile.write(html.encode())

    def _serve_challenge(self):
        chal = generate_challenge()
        with self.challenge_lock:
            self.challenge_store[chal] = time.time()
        self._send_json({"challenge": chal, "difficulty": 20})

    def _serve_claim(self, params: dict):
        address = params.get("address", [""])[0]
        challenge = params.get("challenge", [""])[0]
        nonce_str = params.get("nonce", ["0"])[0]
        if not address.startswith("hcs1") and not address.startswith("thcs1"):
            return self._send_json({"error": "Invalid address format"}, 400)
        try:
            nonce = int(nonce_str)
        except ValueError:
            return self._send_json({"error": "Invalid nonce"}, 400)
        with self.challenge_lock:
            if challenge not in self.challenge_store:
                return self._send_json({"error": "Invalid challenge"}, 400)
            del self.challenge_store[challenge]
        if not check_pow(challenge, nonce):
            return self._send_json({"error": "PoW not satisfied"}, 400)
        # Rate limit
        with _ratelimit_lock:
            last = _ratelimit.get(address, 0.0)
            if time.time() - last < 86400:
                return self._send_json({"error": "Rate limited, try again in 24h"}, 429)
            _ratelimit[address] = time.time()
        # Send transaction
        try:
            result = subprocess.run(
                self._cli_args() + ["sendtoaddress", address, str(self.amount)],
                capture_output=True, text=True, timeout=30,
            )
            if result.returncode != 0:
                return self._send_json({"error": f"RPC failed: {result.stderr}"}, 500)
            txid = result.stdout.strip()
            self._send_json({"txid": txid, "amount": self.amount, "address": address})
        except Exception as e:
            self._send_json({"error": str(e)}, 500)

def main():
    parser = argparse.ArgumentParser(description="HCScoin Testnet Faucet")
    parser.add_argument("--cli", default="hcscoin-cli", help="Path to hcscoin-cli")
    parser.add_argument("--testnet4", action="store_true", help="Use testnet4")
    parser.add_argument("--amount", type=float, default=10.0, help="HCS per claim")
    parser.add_argument("--port", type=int, default=28380, help="HTTP listen port")
    parser.add_argument("--bind", default="0.0.0.0", help="Bind address")
    args = parser.parse_args()
    FaucetHandler.cli_path = args.cli
    FaucetHandler.is_testnet4 = args.testnet4
    FaucetHandler.amount = args.amount
    server = http.server.HTTPServer((args.bind, args.port), FaucetHandler)
    print(f"HCScoin Faucet listening on http://{args.bind}:{args.port}")
    server.serve_forever()

if __name__ == "__main__":
    main()
