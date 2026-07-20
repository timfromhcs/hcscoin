/*
 * HCScoin Genesis Block Miner
 * ===========================
 * Constructs the HCScoin genesis block exactly like CreateGenesisBlock() in
 * src/kernel/chainparams.cpp and grinds the nonce until the block hash
 * satisfies the target encoded in nBits.
 *
 * Build:  cc -O2 -o genesis_miner genesis_miner.c
 * Run:    ./genesis_miner
 *
 * Printed hashes use the display convention of uint256::ToString() so they
 * can be pasted directly into chainparams.cpp asserts.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- SHA-256 ---------------- */
typedef struct {
    uint32_t s[8];
    uint64_t bytes;
    unsigned char buf[64];
} sha256_ctx;

static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha256_transform(sha256_ctx* c, const unsigned char* p)
{
    uint32_t w[64], a,b,d,e,f,g,h,t1,t2,cc;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1 = rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);
        w[i] = w[i-16]+s0+w[i-7]+s1;
    }
    a=c->s[0];b=c->s[1];cc=c->s[2];d=c->s[3];e=c->s[4];f=c->s[5];g=c->s[6];h=c->s[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e,6)^rotr(e,11)^rotr(e,25);
        uint32_t ch = (e&f)^((~e)&g);
        t1 = h + S1 + ch + K256[i] + w[i];
        uint32_t S0 = rotr(a,2)^rotr(a,13)^rotr(a,22);
        uint32_t maj = (a&b)^(a&cc)^(b&cc);
        t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->s[0]+=a;c->s[1]+=b;c->s[2]+=cc;c->s[3]+=d;c->s[4]+=e;c->s[5]+=f;c->s[6]+=g;c->s[7]+=h;
}

static void sha256_init(sha256_ctx* c)
{
    static const uint32_t iv[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                                   0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memcpy(c->s, iv, sizeof(iv));
    c->bytes = 0;
}

static void sha256_write(sha256_ctx* c, const unsigned char* data, size_t len)
{
    size_t used = (size_t)(c->bytes & 63);
    size_t n;
    c->bytes += len;
    if (used) {
        n = 64 - used;
        if (n > len) n = len;
        memcpy(c->buf + used, data, n);
        data += n; len -= n;
        if (used + n == 64) sha256_transform(c, c->buf);
    }
    while (len >= 64) { sha256_transform(c, data); data += 64; len -= 64; }
    if (len) memcpy(c->buf, data, len);
}

static void sha256_final(sha256_ctx* c, unsigned char out[32])
{
    uint64_t bits = c->bytes * 8;
    unsigned char pad = 0x80, zero = 0;
    int i;
    sha256_write(c, &pad, 1);
    while ((c->bytes & 63) != 56) sha256_write(c, &zero, 1);
    for (i = 7; i >= 0; i--) { unsigned char b = (unsigned char)(bits >> (i*8)); sha256_write(c, &b, 1); }
    for (i = 0; i < 8; i++) {
        out[i*4]   = (unsigned char)(c->s[i] >> 24);
        out[i*4+1] = (unsigned char)(c->s[i] >> 16);
        out[i*4+2] = (unsigned char)(c->s[i] >> 8);
        out[i*4+3] = (unsigned char)(c->s[i]);
    }
}

static void dsha256(const unsigned char* data, size_t len, unsigned char out[32])
{
    sha256_ctx c;
    unsigned char tmp[32];
    sha256_init(&c); sha256_write(&c, data, len); sha256_final(&c, tmp);
    sha256_init(&c); sha256_write(&c, tmp, 32); sha256_final(&c, out);
}


/* ---------------- Genesis construction (mirrors CreateGenesisBlock) ---------------- */
static const char* PSZ_TIMESTAMP = "HCScoin Genesis - 20 July 2026";
/* Same well-known pubkey as Bitcoin's genesis; coinbase output is unspendable. */
static const char* GENESIS_PUBKEY_HEX =
    "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f"
    "4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f";

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Serialize the genesis coinbase transaction; returns length in bytes. */
static size_t build_genesis_tx(unsigned char* out)
{
    unsigned char* p = out;
    size_t tslen = strlen(PSZ_TIMESTAMP);
    size_t pklen = strlen(GENESIS_PUBKEY_HEX) / 2;
    size_t i;
    unsigned char scriptsig[256];
    unsigned char* s = scriptsig;

    *p++ = 1; *p++ = 0; *p++ = 0; *p++ = 0;          /* version = 1 (LE) */
    *p++ = 1;                                        /* vin count */
    memset(p, 0, 32); p += 32;                       /* prevout hash = 0 */
    *p++ = 0xff; *p++ = 0xff; *p++ = 0xff; *p++ = 0xff; /* prevout n = -1 */

    /* scriptSig = push(486604799) ++ OP_4 ++ push(timestamp) */
    *s++ = 0x04; *s++ = 0xff; *s++ = 0xff; *s++ = 0x00; *s++ = 0x1d;
    *s++ = 0x54;                                     /* OP_4 (CScriptNum(4)) */
    *s++ = (unsigned char)tslen;
    memcpy(s, PSZ_TIMESTAMP, tslen); s += tslen;
    *p++ = (unsigned char)(s - scriptsig);
    memcpy(p, scriptsig, (size_t)(s - scriptsig)); p += (size_t)(s - scriptsig);

    *p++ = 0xff; *p++ = 0xff; *p++ = 0xff; *p++ = 0xff; /* sequence */
    *p++ = 1;                                        /* vout count */
    {
        uint64_t v = 5000000000ULL;                  /* 50 * COIN */
        for (i = 0; i < 8; i++) *p++ = (unsigned char)(v >> (i*8));
    }
    *p++ = (unsigned char)(1 + pklen + 1);           /* scriptPubKey length */
    *p++ = (unsigned char)pklen;                     /* push 65-byte pubkey */
    for (i = 0; i < pklen; i++)
        *p++ = (unsigned char)((hexval(GENESIS_PUBKEY_HEX[i*2]) << 4) |
                               hexval(GENESIS_PUBKEY_HEX[i*2+1]));
    *p++ = 0xac;                                     /* OP_CHECKSIG */
    *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;          /* locktime */
    return (size_t)(p - out);
}

/* nBits compact -> 32-byte little-endian target. */
static void target_from_bits(uint32_t nBits, unsigned char target[32])
{
    uint32_t exponent = nBits >> 24;
    uint32_t mantissa = nBits & 0x007fffff;
    int base = (int)exponent - 3;
    int i;
    memset(target, 0, 32);
    for (i = 0; i < 3; i++) {
        int pos = base + i;
        if (pos >= 0 && pos < 32)
            target[pos] = (unsigned char)(mantissa >> (8 * i));
    }
}

static int hash_meets_target(const unsigned char hash[32], const unsigned char target[32])
{
    int i;
    for (i = 31; i >= 0; i--) {
        if (hash[i] < target[i]) return 1;
        if (hash[i] > target[i]) return 0;
    }
    return 1;
}

static void print_display_hex(const char* label, const unsigned char internal[32])
{
    int i;
    printf("%s", label);
    for (i = 31; i >= 0; i--) printf("%02x", internal[i]);
    printf("\n");
}

static void mine_genesis(const char* netname, uint32_t nTime, uint32_t nBits)
{
    unsigned char tx[1024];
    size_t txlen = build_genesis_tx(tx);
    unsigned char merkle[32], hash[32], target[32];
    /* HCScoin extended header: 80 legacy bytes + nQuantumNonce(4) +
     * quantumFingerprint(32 bytes of zeros for genesis). */
    unsigned char header[116];
    uint32_t nNonce = 0;

    dsha256(tx, txlen, merkle);
    target_from_bits(nBits, target);

    header[0] = 1; header[1] = 0; header[2] = 0; header[3] = 0;
    memset(header + 4, 0, 32);
    memcpy(header + 36, merkle, 32);
    header[68] = (unsigned char)(nTime);
    header[69] = (unsigned char)(nTime >> 8);
    header[70] = (unsigned char)(nTime >> 16);
    header[71] = (unsigned char)(nTime >> 24);
    header[72] = (unsigned char)(nBits);
    header[73] = (unsigned char)(nBits >> 8);
    header[74] = (unsigned char)(nBits >> 16);
    header[75] = (unsigned char)(nBits >> 24);
    /* nQuantumNonce = 0 (4 bytes LE) */
    header[80] = 0; header[81] = 0; header[82] = 0; header[83] = 0;
    /* quantumFingerprint = 32 zero bytes (null for genesis) */
    memset(header + 84, 0, 32);

    do {
        header[76] = (unsigned char)(nNonce);
        header[77] = (unsigned char)(nNonce >> 8);
        header[78] = (unsigned char)(nNonce >> 16);
        header[79] = (unsigned char)(nNonce >> 24);
        dsha256(header, 116, hash);
        if (hash_meets_target(hash, target)) break;
        nNonce++;
    } while (nNonce != 0);

    printf("=== %s ===\n", netname);
    printf("nTime    = %u\n", nTime);
    printf("nBits    = 0x%08x\n", nBits);
    printf("nNonce   = %u\n", nNonce);
    print_display_hex("merkle   = 0x", merkle);
    print_display_hex("hash     = 0x", hash);
    printf("\n");
}

int main(void)
{
    /* Fixed UTC timestamps:
     * 1784505600 = 2026-07-20 00:00:00 UTC (verified with Python). */
    mine_genesis("HCScoin MAINNET",  1784505600U, 0x1e0fffffU);
    mine_genesis("HCScoin TESTNET3", 1784505900U, 0x1e0fffffU);
    mine_genesis("HCScoin TESTNET4", 1784506800U, 0x1e0fffffU);
    mine_genesis("HCScoin SIGNET",   1784506500U, 0x1e0377aeU);
    mine_genesis("HCScoin REGTEST",  1784506200U, 0x207fffffU);
    return 0;
}
