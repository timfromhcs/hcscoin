/*
 * HCScoin: CRYSTALS-Dilithium / NIST ML-DSA-87 (FIPS 204) implementation.
 *
 * Self-contained public-domain-style reference implementation following the
 * FIPS 204 / Dilithium round-3.1 algorithms for parameter set 87
 * (K=8, L=7, ETA=2, TAU=60, BETA=120, GAMMA1=2^19, GAMMA2=(Q-1)/32,
 * OMEGA=75, CTILDEBYTES=64), producing:
 *
 *   pk  = 2592 bytes,  sk = 4896 bytes,  sig = 4627 bytes
 *
 * The Keccak/SHAKE code is included below so the module has no external
 * dependencies. Only portable C99 is used.
 *
 * WARNING: This is a constant-time-conscious but NOT side-channel-hardened
 * reference implementation. For production HSM use, swap in a certified
 * implementation (e.g. liboqs/PQClean) behind the same API.
 */
#include "dilithium.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Parameters (ML-DSA-87)                                             */
/* ------------------------------------------------------------------ */
#define DIL_N      256
#define DIL_Q      8380417
#define DIL_QINV   58728449 /* q^(-1) mod 2^32 */
#define DIL_D      13
#define DIL_K      8
#define DIL_L      7
#define DIL_ETA    2
#define DIL_TAU    60
#define DIL_BETA   120
#define DIL_GAMMA1 (1 << 19)
#define DIL_GAMMA2 ((DIL_Q - 1) / 32)
#define DIL_OMEGA  75
#define DIL_CTILDEBYTES 64
#define DIL_CRHBYTES    64
#define DIL_SEEDBYTES   32

#define DIL_POLYETA_PACKEDBYTES  96
#define DIL_POLYT1_PACKEDBYTES   320
#define DIL_POLYT0_PACKEDBYTES   416
#define DIL_POLYZ_PACKEDBYTES    640
#define DIL_POLYW1_PACKEDBYTES   128

/* Compile-time size sanity checks against the public header. */
typedef char dil_pk_size_check[(DIL_SEEDBYTES + DIL_K * DIL_POLYT1_PACKEDBYTES) == DILITHIUM_PUBLICKEYBYTES ? 1 : -1];
typedef char dil_sk_size_check[(2 * DIL_SEEDBYTES + DIL_CRHBYTES \
        + (DIL_L + DIL_K) * DIL_POLYETA_PACKEDBYTES + DIL_K * DIL_POLYT0_PACKEDBYTES) == DILITHIUM_SECRETKEYBYTES ? 1 : -1];
typedef char dil_sig_size_check[(DIL_CTILDEBYTES + DIL_L * DIL_POLYZ_PACKEDBYTES + DIL_OMEGA + DIL_K) == DILITHIUM_SIGNATUREBYTES ? 1 : -1];

/* ------------------------------------------------------------------ */
/* Keccak-f[1600] and SHAKE128/256 (FIPS 202)                         */
/* ------------------------------------------------------------------ */
#define KECCAK_ROUNDS 24
#define SHAKE128_RATE 168
#define SHAKE256_RATE 136

static uint64_t keccak_load64(const uint8_t* x)
{
    uint64_t r = 0;
    for (unsigned int i = 0; i < 8; ++i) r |= (uint64_t)x[i] << (8 * i);
    return r;
}

static const uint8_t keccakf_rndc[KECCAK_ROUNDS] = {
    0x01, 0x82, 0x8a, 0x80, 0x8b, 0x01, 0x81, 0x09,
    0x8a, 0x88, 0x09, 0x0a, 0x8b, 0x8b, 0x81, 0x89,
    0x03, 0x02, 0x80, 0x0b, 0x0a, 0x8a, 0x82, 0x83
};

static const uint8_t keccakf_rotc[KECCAK_ROUNDS] = {
    1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14,
    27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44
};

static const uint8_t keccakf_piln[KECCAK_ROUNDS] = {
    10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1
};

static uint64_t rol64(uint64_t x, int s) { return (x << s) | (x >> (64 - s)); }

static void keccakf1600(uint64_t st[25])
{
    uint64_t t, bc[5];
    for (int round = 0; round < KECCAK_ROUNDS; ++round) {
        /* Theta */
        for (int i = 0; i < 5; ++i)
            bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
        for (int i = 0; i < 5; ++i) {
            t = bc[(i + 4) % 5] ^ rol64(bc[(i + 1) % 5], 1);
            for (int j = 0; j < 25; j += 5) st[j + i] ^= t;
        }
        /* Rho Pi */
        t = st[1];
        for (int i = 0; i < 24; ++i) {
            int j = keccakf_piln[i];
            bc[0] = st[j];
            st[j] = rol64(t, keccakf_rotc[i]);
            t = bc[0];
        }
        /* Chi */
        for (int j = 0; j < 25; j += 5) {
            for (int i = 0; i < 5; ++i) bc[i] = st[j + i];
            for (int i = 0; i < 5; ++i)
                st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }
        /* Iota */
        st[0] ^= keccakf_rndc[round];
    }
}


/* Incremental SHAKE context: absorbed input + squeeze position. */
typedef struct {
    uint64_t st[25];
    unsigned int rate;
    unsigned int pos; /* position within current block for squeeze */
} shake_ctx;

static void shake_init(shake_ctx* c, unsigned int rate)
{
    memset(c->st, 0, sizeof(c->st));
    c->rate = rate;
    c->pos = 0;
}

static void shake_absorb(shake_ctx* c, const uint8_t* in, size_t inlen)
{
    while (inlen > 0) {
        size_t take = c->rate - c->pos;
        if (take > inlen) take = inlen;
        for (size_t i = 0; i < take; ++i) {
            size_t p = c->pos + i;
            c->st[p / 8] ^= (uint64_t)in[i] << (8 * (p % 8));
        }
        c->pos += (unsigned int)take;
        in += take;
        inlen -= take;
        if (c->pos == c->rate) {
            keccakf1600(c->st);
            c->pos = 0;
        }
    }
}

static void shake_finalize(shake_ctx* c)
{
    c->st[c->pos / 8] ^= (uint64_t)0x1F << (8 * (c->pos % 8));
    c->st[(c->rate - 1) / 8] ^= (uint64_t)0x80 << (8 * ((c->rate - 1) % 8));
    keccakf1600(c->st);
    c->pos = 0;
}

static void shake_squeeze(shake_ctx* c, uint8_t* out, size_t outlen)
{
    while (outlen > 0) {
        if (c->pos == c->rate) {
            keccakf1600(c->st);
            c->pos = 0;
        }
        size_t take = c->rate - c->pos;
        if (take > outlen) take = outlen;
        for (size_t i = 0; i < take; ++i) {
            size_t p = c->pos + i;
            out[i] = (uint8_t)(c->st[p / 8] >> (8 * (p % 8)));
        }
        c->pos += (unsigned int)take;
        out += take;
        outlen -= take;
    }
}

static void shake256(uint8_t* out, size_t outlen, const uint8_t* in, size_t inlen)
{
    shake_ctx c;
    shake_init(&c, SHAKE256_RATE);
    shake_absorb(&c, in, inlen);
    shake_finalize(&c);
    shake_squeeze(&c, out, outlen);
}

/* ------------------------------------------------------------------ */
/* Field arithmetic modulo Q (Montgomery domain)                      */
/* ------------------------------------------------------------------ */
static int32_t dil_montgomery_reduce(int64_t a)
{
    int32_t t;
    t = (int32_t)((uint32_t)a * (uint32_t)DIL_QINV);
    t = (int32_t)((a - (int64_t)t * DIL_Q) >> 32);
    return t;
}

static int32_t dil_reduce32(int32_t a)
{
    int32_t t;
    t = (a + (1 << 22)) >> 23;
    t = a - t * DIL_Q;
    return t;
}

static int32_t dil_caddq(int32_t a)
{
    a += (a >> 31) & DIL_Q;
    return a;
}

static uint32_t dil_powmod(uint32_t base, uint32_t exp)
{
    uint64_t r = 1, b = base % DIL_Q;
    while (exp) {
        if (exp & 1) r = (r * b) % DIL_Q;
        b = (b * b) % DIL_Q;
        exp >>= 1;
    }
    return (uint32_t)r;
}

static uint8_t dil_brv8(uint8_t x)
{
    x = (uint8_t)(((x & 0x55) << 1) | ((x >> 1) & 0x55));
    x = (uint8_t)(((x & 0x33) << 2) | ((x >> 2) & 0x33));
    x = (uint8_t)((x << 4) | (x >> 4));
    return x;
}

/* NTT twiddle table, generated at init:
 *   zetas[k] = 1753^brv8(k) * 2^32  (mod Q)   -- Montgomery domain,
 * centered to (-Q/2, Q/2]. zetas[0] is unused (0). This reproduces the
 * reference table exactly: zetas[1] = 25847, zetas[2] = -2608894. */
static int32_t dil_zetas[DIL_N];
static int dil_zetas_ready = 0;

static void dil_init_zetas(void)
{
    if (dil_zetas_ready) return;
    dil_zetas[0] = 0;
    for (int k = 1; k < DIL_N; ++k) {
        uint32_t t = (uint32_t)(((uint64_t)dil_powmod(1753, dil_brv8((uint8_t)k)) * (1ULL << 32)) % DIL_Q);
        dil_zetas[k] = (t > (uint32_t)(DIL_Q / 2)) ? (int32_t)t - DIL_Q : (int32_t)t;
    }
    dil_zetas_ready = 1;
}

static void dil_ntt(int32_t a[DIL_N])
{
    unsigned int k = 0;
    for (unsigned int len = 128; len > 0; len >>= 1) {
        for (unsigned int start = 0; start < DIL_N; ) {
            int32_t zeta = dil_zetas[++k];
            unsigned int j;
            for (j = start; j < start + len; ++j) {
                int32_t t = dil_montgomery_reduce((int64_t)zeta * a[j + len]);
                a[j + len] = a[j] - t;
                a[j] = a[j] + t;
            }
            start = j + len;
        }
    }
}

static void dil_invntt_tomont(int32_t a[DIL_N])
{
    const int32_t f = 41978; /* mont^2/256 */
    unsigned int k = DIL_N;
    for (unsigned int len = 1; len < DIL_N; len <<= 1) {
        for (unsigned int start = 0; start < DIL_N; ) {
            int32_t zeta = -dil_zetas[--k];
            unsigned int j;
            for (j = start; j < start + len; ++j) {
                int32_t t = a[j];
                a[j] = t + a[j + len];
                a[j + len] = t - a[j + len];
                a[j + len] = dil_montgomery_reduce((int64_t)zeta * a[j + len]);
            }
            start = j + len;
        }
    }
    for (unsigned int j = 0; j < DIL_N; ++j)
        a[j] = dil_montgomery_reduce((int64_t)f * a[j]);
}


/* ------------------------------------------------------------------ */
/* Rounding                                                           */
/* ------------------------------------------------------------------ */
static int32_t dil_power2round(int32_t* a0, int32_t a)
{
    int32_t a1;
    a1 = (a + (1 << (DIL_D - 1)) - 1) >> DIL_D;
    *a0 = a - (a1 << DIL_D);
    return a1;
}

static int32_t dil_decompose(int32_t* a0, int32_t a)
{
    int32_t a1;
    /* GAMMA2 == (Q-1)/32 */
    a1 = (a + 127) >> 7;
    a1 = (a1 * 1025 + (1 << 21)) >> 22;
    a1 &= 15;
    *a0 = a - a1 * 2 * DIL_GAMMA2;
    *a0 -= (((DIL_Q - 1) / 2 - *a0) >> 31) & DIL_Q;
    return a1;
}

static unsigned int dil_make_hint(int32_t a0, int32_t a1)
{
    if (a0 > DIL_GAMMA2 || a0 < -DIL_GAMMA2 || (a0 == -DIL_GAMMA2 && a1 != 0))
        return 1;
    return 0;
}

static int32_t dil_use_hint(int32_t a, unsigned int hint)
{
    int32_t a0, a1;
    a1 = dil_decompose(&a0, a);
    if (hint == 0) return a1;
    if (a0 > 0) return (a1 == 15) ? 0 : a1 + 1;
    return (a1 == 0) ? 15 : a1 - 1;
}

/* ------------------------------------------------------------------ */
/* Polynomials and vectors                                            */
/* ------------------------------------------------------------------ */
typedef struct { int32_t coeffs[DIL_N]; } dil_poly;
typedef struct { dil_poly vec[DIL_L]; } dil_polyvecl;
typedef struct { dil_poly vec[DIL_K]; } dil_polyveck;

static int dil_poly_chknorm(const dil_poly* a, int32_t bound)
{
    for (unsigned int i = 0; i < DIL_N; ++i) {
        int32_t t = a->coeffs[i];
        t = t - ((t >> 31) & (2 * t)); /* |t| */
        if (t >= bound) return 1;
    }
    return 0;
}

static int dil_polyvecl_chknorm(const dil_polyvecl* v, int32_t bound)
{
    for (unsigned int i = 0; i < DIL_L; ++i)
        if (dil_poly_chknorm(&v->vec[i], bound)) return 1;
    return 0;
}

static int dil_polyveck_chknorm(const dil_polyveck* v, int32_t bound)
{
    for (unsigned int i = 0; i < DIL_K; ++i)
        if (dil_poly_chknorm(&v->vec[i], bound)) return 1;
    return 0;
}

static void dil_poly_ntt(dil_poly* a) { dil_ntt(a->coeffs); }
static void dil_poly_invntt(dil_poly* a) { dil_invntt_tomont(a->coeffs); }

static void dil_poly_pointwise(dil_poly* c, const dil_poly* a, const dil_poly* b)
{
    for (unsigned int i = 0; i < DIL_N; ++i)
        c->coeffs[i] = dil_montgomery_reduce((int64_t)a->coeffs[i] * b->coeffs[i]);
}

static void dil_polyvecl_ntt(dil_polyvecl* v)
{
    for (unsigned int i = 0; i < DIL_L; ++i) dil_poly_ntt(&v->vec[i]);
}

static void dil_polyveck_ntt(dil_polyveck* v)
{
    for (unsigned int i = 0; i < DIL_K; ++i) dil_poly_ntt(&v->vec[i]);
}

static void dil_polyvecl_invntt(dil_polyvecl* v)
{
    for (unsigned int i = 0; i < DIL_L; ++i) dil_poly_invntt(&v->vec[i]);
}

static void dil_polyveck_invntt(dil_polyveck* v)
{
    for (unsigned int i = 0; i < DIL_K; ++i) dil_poly_invntt(&v->vec[i]);
}

static void dil_polyvecl_pointwise_poly(dil_polyvecl* r, const dil_poly* a, const dil_polyvecl* v)
{
    for (unsigned int i = 0; i < DIL_L; ++i)
        dil_poly_pointwise(&r->vec[i], a, &v->vec[i]);
}

static void dil_polyveck_pointwise_poly(dil_polyveck* r, const dil_poly* a, const dil_polyveck* v)
{
    for (unsigned int i = 0; i < DIL_K; ++i)
        dil_poly_pointwise(&r->vec[i], a, &v->vec[i]);
}

static void dil_polyvecl_pointwise_acc(dil_poly* w, const dil_polyvecl* u, const dil_polyvecl* v)
{
    dil_poly t;
    dil_poly_pointwise(w, &u->vec[0], &v->vec[0]);
    for (unsigned int i = 1; i < DIL_L; ++i) {
        dil_poly_pointwise(&t, &u->vec[i], &v->vec[i]);
        for (unsigned int j = 0; j < DIL_N; ++j) w->coeffs[j] += t.coeffs[j];
    }
}

static void dil_polyveck_add(dil_polyveck* r, const dil_polyveck* a, const dil_polyveck* b)
{
    for (unsigned int i = 0; i < DIL_K; ++i)
        for (unsigned int j = 0; j < DIL_N; ++j)
            r->vec[i].coeffs[j] = a->vec[i].coeffs[j] + b->vec[i].coeffs[j];
}

static void dil_polyveck_sub(dil_polyveck* r, const dil_polyveck* a, const dil_polyveck* b)
{
    for (unsigned int i = 0; i < DIL_K; ++i)
        for (unsigned int j = 0; j < DIL_N; ++j)
            r->vec[i].coeffs[j] = a->vec[i].coeffs[j] - b->vec[i].coeffs[j];
}

static void dil_polyvecl_add(dil_polyvecl* r, const dil_polyvecl* a, const dil_polyvecl* b)
{
    for (unsigned int i = 0; i < DIL_L; ++i)
        for (unsigned int j = 0; j < DIL_N; ++j)
            r->vec[i].coeffs[j] = a->vec[i].coeffs[j] + b->vec[i].coeffs[j];
}

static void dil_polyvecl_reduce(dil_polyvecl* v)
{
    for (unsigned int i = 0; i < DIL_L; ++i)
        for (unsigned int j = 0; j < DIL_N; ++j)
            v->vec[i].coeffs[j] = dil_reduce32(v->vec[i].coeffs[j]);
}

static void dil_polyveck_reduce(dil_polyveck* v)
{
    for (unsigned int i = 0; i < DIL_K; ++i)
        for (unsigned int j = 0; j < DIL_N; ++j)
            v->vec[i].coeffs[j] = dil_reduce32(v->vec[i].coeffs[j]);
}

static void dil_polyveck_caddq(dil_polyveck* v)
{
    for (unsigned int i = 0; i < DIL_K; ++i)
        for (unsigned int j = 0; j < DIL_N; ++j)
            v->vec[i].coeffs[j] = dil_caddq(v->vec[i].coeffs[j]);
}

static void dil_polyveck_power2round(dil_polyveck* v1, dil_polyveck* v0, const dil_polyveck* v)
{
    for (unsigned int i = 0; i < DIL_K; ++i)
        for (unsigned int j = 0; j < DIL_N; ++j)
            v1->vec[i].coeffs[j] = dil_power2round(&v0->vec[i].coeffs[j], v->vec[i].coeffs[j]);
}

static void dil_polyveck_decompose(dil_polyveck* v1, dil_polyveck* v0, const dil_polyveck* v)
{
    for (unsigned int i = 0; i < DIL_K; ++i)
        for (unsigned int j = 0; j < DIL_N; ++j)
            v1->vec[i].coeffs[j] = dil_decompose(&v0->vec[i].coeffs[j], v->vec[i].coeffs[j]);
}

static unsigned int dil_polyveck_make_hint(dil_polyveck* h, const dil_polyveck* v0, const dil_polyveck* v1)
{
    unsigned int s = 0;
    for (unsigned int i = 0; i < DIL_K; ++i)
        for (unsigned int j = 0; j < DIL_N; ++j) {
            h->vec[i].coeffs[j] = (int32_t)dil_make_hint(v0->vec[i].coeffs[j], v1->vec[i].coeffs[j]);
            s += (unsigned int)h->vec[i].coeffs[j];
        }
    return s;
}

static void dil_polyveck_use_hint(dil_polyveck* w, const dil_polyveck* u, const dil_polyveck* h)
{
    for (unsigned int i = 0; i < DIL_K; ++i)
        for (unsigned int j = 0; j < DIL_N; ++j)
            w->vec[i].coeffs[j] = dil_use_hint(u->vec[i].coeffs[j], (unsigned int)h->vec[i].coeffs[j]);
}

/* ------------------------------------------------------------------ */
/* Sampling                                                           */
/* ------------------------------------------------------------------ */
/* Rejection-sample a polynomial with coefficients in [0, Q-1] from
 * SHAKE128(rho || nonce_LE16). Used to expand the public matrix A. */
static void dil_poly_uniform(dil_poly* a, const uint8_t rho[DIL_SEEDBYTES], uint16_t nonce)
{
    shake_ctx c;
    uint8_t inbuf[DIL_SEEDBYTES + 2];
    uint8_t outbuf[SHAKE128_RATE];
    unsigned int ctr = 0;

    memcpy(inbuf, rho, DIL_SEEDBYTES);
    inbuf[DIL_SEEDBYTES] = (uint8_t)(nonce & 0xff);
    inbuf[DIL_SEEDBYTES + 1] = (uint8_t)(nonce >> 8);
    shake_init(&c, SHAKE128_RATE);
    shake_absorb(&c, inbuf, sizeof(inbuf));
    shake_finalize(&c);

    while (ctr < DIL_N) {
        unsigned int pos;
        shake_squeeze(&c, outbuf, SHAKE128_RATE);
        for (pos = 0; pos + 3 <= SHAKE128_RATE && ctr < DIL_N; pos += 3) {
            uint32_t t = (uint32_t)outbuf[pos]
                       | ((uint32_t)outbuf[pos + 1] << 8)
                       | ((uint32_t)outbuf[pos + 2] << 16);
            t &= 0x7FFFFF;
            if (t < DIL_Q) a->coeffs[ctr++] = (int32_t)t;
        }
    }
}

/* Rejection-sample a polynomial with coefficients in [-ETA, ETA] from
 * SHAKE256(rhoprime || nonce_LE16). ETA = 2. */
static void dil_poly_uniform_eta(dil_poly* a, const uint8_t rhoprime[DIL_CRHBYTES], uint16_t nonce)
{
    shake_ctx c;
    uint8_t inbuf[DIL_CRHBYTES + 2];
    uint8_t outbuf[SHAKE256_RATE];
    unsigned int ctr = 0;

    memcpy(inbuf, rhoprime, DIL_CRHBYTES);
    inbuf[DIL_CRHBYTES] = (uint8_t)(nonce & 0xff);
    inbuf[DIL_CRHBYTES + 1] = (uint8_t)(nonce >> 8);
    shake_init(&c, SHAKE256_RATE);
    shake_absorb(&c, inbuf, sizeof(inbuf));
    shake_finalize(&c);

    while (ctr < DIL_N) {
        unsigned int pos;
        shake_squeeze(&c, outbuf, SHAKE256_RATE);
        for (pos = 0; pos < SHAKE256_RATE && ctr < DIL_N; ++pos) {
            uint32_t t0 = outbuf[pos] & 0x0F;
            uint32_t t1 = outbuf[pos] >> 4;
            if (t0 < 15) {
                t0 = t0 - (205 * t0 >> 10) * 5;
                a->coeffs[ctr++] = DIL_ETA - (int32_t)t0;
            }
            if (t1 < 15 && ctr < DIL_N) {
                t1 = t1 - (205 * t1 >> 10) * 5;
                a->coeffs[ctr++] = DIL_ETA - (int32_t)t1;
            }
        }
    }
}

/* Sample a polynomial with coefficients in [-GAMMA1+1, GAMMA1] from
 * SHAKE256(rhoprime || nonce_LE16). GAMMA1 = 2^19. */
static void dil_poly_uniform_gamma1(dil_poly* a, const uint8_t rhoprime[DIL_CRHBYTES], uint16_t nonce)
{
    uint8_t inbuf[DIL_CRHBYTES + 2];
    uint8_t outbuf[DIL_POLYZ_PACKEDBYTES];
    shake_ctx c;

    memcpy(inbuf, rhoprime, DIL_CRHBYTES);
    inbuf[DIL_CRHBYTES] = (uint8_t)(nonce & 0xff);
    inbuf[DIL_CRHBYTES + 1] = (uint8_t)(nonce >> 8);
    shake_init(&c, SHAKE256_RATE);
    shake_absorb(&c, inbuf, sizeof(inbuf));
    shake_finalize(&c);
    shake_squeeze(&c, outbuf, sizeof(outbuf));

    for (unsigned int i = 0; i < DIL_N / 2; ++i) {
        uint32_t t0 = (uint32_t)outbuf[5 * i + 0]
                    | ((uint32_t)outbuf[5 * i + 1] << 8)
                    | ((uint32_t)outbuf[5 * i + 2] << 16);
        t0 &= 0xFFFFF;
        uint32_t t1 = ((uint32_t)outbuf[5 * i + 2] >> 4)
                    | ((uint32_t)outbuf[5 * i + 3] << 4)
                    | ((uint32_t)outbuf[5 * i + 4] << 12);
        a->coeffs[2 * i + 0] = DIL_GAMMA1 - (int32_t)t0;
        a->coeffs[2 * i + 1] = DIL_GAMMA1 - (int32_t)t1;
    }
}

static void dil_polyvecl_uniform_gamma1(dil_polyvecl* v, const uint8_t rhoprime[DIL_CRHBYTES], uint16_t nonce)
{
    for (unsigned int i = 0; i < DIL_L; ++i)
        dil_poly_uniform_gamma1(&v->vec[i], rhoprime, (uint16_t)(DIL_L * nonce + i));
}

/* Sample the challenge polynomial c with exactly TAU +-1 coefficients from
 * the challenge seed ctilde. */
static void dil_poly_challenge(dil_poly* c, const uint8_t seed[DIL_CTILDEBYTES])
{
    shake_ctx ctx;
    uint8_t buf[SHAKE256_RATE];
    uint64_t signs;
    unsigned int pos = 0;

    memset(c->coeffs, 0, sizeof(c->coeffs));
    shake_init(&ctx, SHAKE256_RATE);
    shake_absorb(&ctx, seed, DIL_CTILDEBYTES);
    shake_finalize(&ctx);
    shake_squeeze(&ctx, buf, SHAKE256_RATE);

    signs = keccak_load64(buf);
    pos = 8;
    for (unsigned int i = DIL_N - DIL_TAU; i < DIL_N; ++i) {
        unsigned int j;
        for (;;) {
            if (pos >= SHAKE256_RATE) {
                shake_squeeze(&ctx, buf, SHAKE256_RATE);
                pos = 0;
            }
            j = buf[pos++];
            if (j <= i) break;
        }
        c->coeffs[i] = c->coeffs[j];
        c->coeffs[j] = (signs & 1) ? -1 : 1;
        signs >>= 1;
    }
}

/* Expand the K x L public matrix A from rho. */
static void dil_expand_mat(dil_polyvecl mat[DIL_K], const uint8_t rho[DIL_SEEDBYTES])
{
    for (unsigned int i = 0; i < DIL_K; ++i)
        for (unsigned int j = 0; j < DIL_L; ++j)
            dil_poly_uniform(&mat[i].vec[j], rho, (uint16_t)((i << 8) + j));
}

/* ------------------------------------------------------------------ */
/* Packing (canonical contiguous LSB-first bit layout, as in the      */
/* reference implementation)                                          */
/* ------------------------------------------------------------------ */
static void dil_pack_bits(uint8_t* out, const uint32_t* vals, unsigned int n, unsigned int bits)
{
    uint64_t acc = 0;
    unsigned int accbits = 0;
    size_t pos = 0;
    const uint32_t mask = (bits == 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    for (unsigned int i = 0; i < n; ++i) {
        acc |= (uint64_t)(vals[i] & mask) << accbits;
        accbits += bits;
        while (accbits >= 8) {
            out[pos++] = (uint8_t)(acc & 0xff);
            acc >>= 8;
            accbits -= 8;
        }
    }
    if (accbits) out[pos++] = (uint8_t)(acc & 0xff);
}

static void dil_unpack_bits(uint32_t* vals, const uint8_t* in, unsigned int n, unsigned int bits)
{
    uint64_t acc = 0;
    unsigned int accbits = 0;
    size_t pos = 0;
    const uint32_t mask = (bits == 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    for (unsigned int i = 0; i < n; ++i) {
        while (accbits < bits) {
            acc |= (uint64_t)in[pos++] << accbits;
            accbits += 8;
        }
        vals[i] = (uint32_t)(acc & mask);
        acc >>= bits;
        accbits -= bits;
    }
}

static void dil_polyt1_pack(uint8_t* out, const dil_poly* a)
{
    uint32_t v[DIL_N];
    for (unsigned int i = 0; i < DIL_N; ++i) v[i] = (uint32_t)a->coeffs[i];
    dil_pack_bits(out, v, DIL_N, 10);
}

static void dil_polyt1_unpack(dil_poly* a, const uint8_t* in)
{
    uint32_t v[DIL_N];
    dil_unpack_bits(v, in, DIL_N, 10);
    for (unsigned int i = 0; i < DIL_N; ++i) a->coeffs[i] = (int32_t)v[i];
}

static void dil_polyeta_pack(uint8_t* out, const dil_poly* a)
{
    uint32_t v[DIL_N];
    for (unsigned int i = 0; i < DIL_N; ++i) v[i] = (uint32_t)(DIL_ETA - a->coeffs[i]);
    dil_pack_bits(out, v, DIL_N, 3);
}

static void dil_polyeta_unpack(dil_poly* a, const uint8_t* in)
{
    uint32_t v[DIL_N];
    dil_unpack_bits(v, in, DIL_N, 3);
    for (unsigned int i = 0; i < DIL_N; ++i) a->coeffs[i] = DIL_ETA - (int32_t)v[i];
}

static void dil_polyt0_pack(uint8_t* out, const dil_poly* a)
{
    uint32_t v[DIL_N];
    for (unsigned int i = 0; i < DIL_N; ++i) v[i] = (uint32_t)((1 << (DIL_D - 1)) - a->coeffs[i]);
    dil_pack_bits(out, v, DIL_N, 13);
}

static void dil_polyt0_unpack(dil_poly* a, const uint8_t* in)
{
    uint32_t v[DIL_N];
    dil_unpack_bits(v, in, DIL_N, 13);
    for (unsigned int i = 0; i < DIL_N; ++i) a->coeffs[i] = (1 << (DIL_D - 1)) - (int32_t)v[i];
}

static void dil_polyz_pack(uint8_t* out, const dil_poly* a)
{
    uint32_t v[DIL_N];
    for (unsigned int i = 0; i < DIL_N; ++i) v[i] = (uint32_t)(DIL_GAMMA1 - a->coeffs[i]);
    dil_pack_bits(out, v, DIL_N, 20);
}

static void dil_polyz_unpack(dil_poly* a, const uint8_t* in)
{
    uint32_t v[DIL_N];
    dil_unpack_bits(v, in, DIL_N, 20);
    for (unsigned int i = 0; i < DIL_N; ++i) a->coeffs[i] = DIL_GAMMA1 - (int32_t)v[i];
}

static void dil_polyw1_pack(uint8_t* out, const dil_poly* a)
{
    uint32_t v[DIL_N];
    for (unsigned int i = 0; i < DIL_N; ++i) v[i] = (uint32_t)a->coeffs[i];
    dil_pack_bits(out, v, DIL_N, 4);
}

/* ------------------------------------------------------------------ */
/* Key / signature (un)packing                                        */
/* ------------------------------------------------------------------ */
static void dil_pack_pk(uint8_t* pk, const uint8_t rho[DIL_SEEDBYTES], const dil_polyveck* t1)
{
    memcpy(pk, rho, DIL_SEEDBYTES);
    for (unsigned int i = 0; i < DIL_K; ++i)
        dil_polyt1_pack(pk + DIL_SEEDBYTES + i * DIL_POLYT1_PACKEDBYTES, &t1->vec[i]);
}

static void dil_unpack_pk(uint8_t rho[DIL_SEEDBYTES], dil_polyveck* t1, const uint8_t* pk)
{
    memcpy(rho, pk, DIL_SEEDBYTES);
    for (unsigned int i = 0; i < DIL_K; ++i)
        dil_polyt1_unpack(&t1->vec[i], pk + DIL_SEEDBYTES + i * DIL_POLYT1_PACKEDBYTES);
}

static void dil_pack_sk(uint8_t* sk,
                        const uint8_t rho[DIL_SEEDBYTES],
                        const uint8_t key[DIL_SEEDBYTES],
                        const uint8_t tr[DIL_CRHBYTES],
                        const dil_polyvecl* s1,
                        const dil_polyveck* s2,
                        const dil_polyveck* t0)
{
    uint8_t* p = sk;
    memcpy(p, rho, DIL_SEEDBYTES); p += DIL_SEEDBYTES;
    memcpy(p, key, DIL_SEEDBYTES); p += DIL_SEEDBYTES;
    memcpy(p, tr, DIL_CRHBYTES); p += DIL_CRHBYTES;
    for (unsigned int i = 0; i < DIL_L; ++i) { dil_polyeta_pack(p, &s1->vec[i]); p += DIL_POLYETA_PACKEDBYTES; }
    for (unsigned int i = 0; i < DIL_K; ++i) { dil_polyeta_pack(p, &s2->vec[i]); p += DIL_POLYETA_PACKEDBYTES; }
    for (unsigned int i = 0; i < DIL_K; ++i) { dil_polyt0_pack(p, &t0->vec[i]); p += DIL_POLYT0_PACKEDBYTES; }
}

static void dil_unpack_sk(uint8_t rho[DIL_SEEDBYTES],
                          uint8_t key[DIL_SEEDBYTES],
                          uint8_t tr[DIL_CRHBYTES],
                          dil_polyvecl* s1,
                          dil_polyveck* s2,
                          dil_polyveck* t0,
                          const uint8_t* sk)
{
    const uint8_t* p = sk;
    memcpy(rho, p, DIL_SEEDBYTES); p += DIL_SEEDBYTES;
    memcpy(key, p, DIL_SEEDBYTES); p += DIL_SEEDBYTES;
    memcpy(tr, p, DIL_CRHBYTES); p += DIL_CRHBYTES;
    for (unsigned int i = 0; i < DIL_L; ++i) { dil_polyeta_unpack(&s1->vec[i], p); p += DIL_POLYETA_PACKEDBYTES; }
    for (unsigned int i = 0; i < DIL_K; ++i) { dil_polyeta_unpack(&s2->vec[i], p); p += DIL_POLYETA_PACKEDBYTES; }
    for (unsigned int i = 0; i < DIL_K; ++i) { dil_polyt0_unpack(&t0->vec[i], p); p += DIL_POLYT0_PACKEDBYTES; }
}

/* Elementwise reduce32 for a single poly (helper for sign/verify). */
static void dil_poly_reduce(dil_poly* a)
{
    for (unsigned int i = 0; i < DIL_N; ++i)
        a->coeffs[i] = dil_reduce32(a->coeffs[i]);
}

static void dil_pack_sig(uint8_t* sig, const uint8_t ctilde[DIL_CTILDEBYTES],
                         const dil_polyvecl* z, const dil_polyveck* h)
{
    uint8_t* p = sig;
    memcpy(p, ctilde, DIL_CTILDEBYTES); p += DIL_CTILDEBYTES;
    for (unsigned int i = 0; i < DIL_L; ++i) { dil_polyz_pack(p, &z->vec[i]); p += DIL_POLYZ_PACKEDBYTES; }

    /* Encode hint h: indices of set coefficients, then per-poly offsets. */
    memset(p, 0, DIL_OMEGA + DIL_K);
    unsigned int k = 0;
    for (unsigned int i = 0; i < DIL_K; ++i) {
        for (unsigned int j = 0; j < DIL_N; ++j)
            if (h->vec[i].coeffs[j] != 0) p[k++] = (uint8_t)j;
        p[DIL_OMEGA + i] = (uint8_t)k;
    }
}

/* Returns 0 on success, 1 on malformed input (strict decoding). */
static int dil_unpack_sig(uint8_t ctilde[DIL_CTILDEBYTES],
                          dil_polyvecl* z, dil_polyveck* h, const uint8_t* sig)
{
    const uint8_t* p = sig;
    memcpy(ctilde, p, DIL_CTILDEBYTES); p += DIL_CTILDEBYTES;
    for (unsigned int i = 0; i < DIL_L; ++i) { dil_polyz_unpack(&z->vec[i], p); p += DIL_POLYZ_PACKEDBYTES; }

    memset(h, 0, sizeof(*h));
    unsigned int k = 0;
    for (unsigned int i = 0; i < DIL_K; ++i) {
        if (p[DIL_OMEGA + i] < k || p[DIL_OMEGA + i] > DIL_OMEGA) return 1;
        for (unsigned int j = k; j < p[DIL_OMEGA + i]; ++j) {
            if (j > k && p[j] <= p[j - 1]) return 1; /* strictly increasing */
            h->vec[i].coeffs[p[j]] = 1;
        }
        k = p[DIL_OMEGA + i];
    }
    for (unsigned int j = k; j < DIL_OMEGA; ++j)
        if (p[j] != 0) return 1; /* padding must be zero */
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */
int dilithium_keygen_from_seed(uint8_t* pk, uint8_t* sk,
                               const uint8_t seed[DILITHIUM_SEEDBYTES])
{
    uint8_t seedbuf[3 * DIL_SEEDBYTES + DIL_CRHBYTES];
    const uint8_t* rho = seedbuf;
    const uint8_t* rhoprime = seedbuf + DIL_SEEDBYTES;
    const uint8_t* key = seedbuf + DIL_SEEDBYTES + DIL_CRHBYTES;
    dil_polyvecl mat[DIL_K], s1, s1hat;
    dil_polyveck s2, t1, t0;
    dil_poly t;
    uint8_t tr[DIL_CRHBYTES];

    dil_init_zetas();

    /* Expand 32 + 64 + 32 bytes from the seed */
    shake256(seedbuf, sizeof(seedbuf), seed, DIL_SEEDBYTES);

    dil_expand_mat(mat, rho);

    /* Sample secret vectors s1, s2 */
    for (unsigned int i = 0; i < DIL_L; ++i)
        dil_poly_uniform_eta(&s1.vec[i], rhoprime, (uint16_t)i);
    for (unsigned int i = 0; i < DIL_K; ++i)
        dil_poly_uniform_eta(&s2.vec[i], rhoprime, (uint16_t)(DIL_L + i));

    /* t = NTT^{-1}(A * NTT(s1)) + s2 */
    s1hat = s1;
    dil_polyvecl_ntt(&s1hat);
    for (unsigned int i = 0; i < DIL_K; ++i) {
        dil_polyvecl_pointwise_acc(&t, &mat[i], &s1hat);
        dil_poly_reduce(&t);
        dil_poly_invntt(&t);
        t1.vec[i] = t;
    }
    dil_polyveck_add(&t1, &t1, &s2);

    /* t1, t0 = Power2Round(t) */
    dil_polyveck_caddq(&t1);
    dil_polyveck_power2round(&t1, &t0, &t1);

    dil_pack_pk(pk, rho, &t1);
    shake256(tr, DIL_CRHBYTES, pk, DILITHIUM_PUBLICKEYBYTES);
    dil_pack_sk(sk, rho, key, tr, &s1, &s2, &t0);
    return 0;
}

int dilithium_keygen(uint8_t* pk, uint8_t* sk)
{
    uint8_t seed[DIL_SEEDBYTES];
    dilithium_randombytes(seed, DIL_SEEDBYTES);
    return dilithium_keygen_from_seed(pk, sk, seed);
}

int dilithium_pk_from_sk(uint8_t* pk, const uint8_t* sk)
{
    uint8_t rho[DIL_SEEDBYTES], key[DIL_SEEDBYTES], tr[DIL_CRHBYTES];
    dil_polyvecl mat[DIL_K], s1, s1hat;
    dil_polyveck s2, t0, t1;
    dil_poly t;

    dil_init_zetas();
    dil_unpack_sk(rho, key, tr, &s1, &s2, &t0, sk);

    /* Recompute t = A*s1 + s2 exactly as in keygen. */
    dil_expand_mat(mat, rho);
    s1hat = s1;
    dil_polyvecl_ntt(&s1hat);
    for (unsigned int i = 0; i < DIL_K; ++i) {
        dil_polyvecl_pointwise_acc(&t, &mat[i], &s1hat);
        dil_poly_reduce(&t);
        dil_poly_invntt(&t);
        t1.vec[i] = t;
    }
    dil_polyveck_add(&t1, &t1, &s2);
    dil_polyveck_caddq(&t1);
    dil_polyveck_power2round(&t1, &t0, &t1);

    dil_pack_pk(pk, rho, &t1);
    return 0;
}

int dilithium_sign(uint8_t* sig, size_t* siglen,
                   const uint8_t* msg, size_t msglen,
                   const uint8_t* sk)
{
    uint8_t rho[DIL_SEEDBYTES], key[DIL_SEEDBYTES], tr[DIL_CRHBYTES];
    uint8_t mu[DIL_CRHBYTES], rhoprime[DIL_CRHBYTES], rnd[DIL_SEEDBYTES];
    uint8_t ctilde[DIL_CTILDEBYTES];
    dil_polyvecl mat[DIL_K], s1, y, z;
    dil_polyveck s2, t0, w, w1, w0, h;
    dil_poly cp;
    uint16_t nonce = 0;

    dil_init_zetas();
    dil_unpack_sk(rho, key, tr, &s1, &s2, &t0, sk);

    /* mu = CRH(tr || M) */
    {
        shake_ctx c;
        shake_init(&c, SHAKE256_RATE);
        shake_absorb(&c, tr, DIL_CRHBYTES);
        shake_absorb(&c, msg, msglen);
        shake_finalize(&c);
        shake_squeeze(&c, mu, DIL_CRHBYTES);
    }

    /* rhoprime = CRH(key || rnd || mu)  (hedged/randomized signing) */
    dilithium_randombytes(rnd, DIL_SEEDBYTES);
    {
        shake_ctx c;
        shake_init(&c, SHAKE256_RATE);
        shake_absorb(&c, key, DIL_SEEDBYTES);
        shake_absorb(&c, rnd, DIL_SEEDBYTES);
        shake_absorb(&c, mu, DIL_CRHBYTES);
        shake_finalize(&c);
        shake_squeeze(&c, rhoprime, DIL_CRHBYTES);
    }

    dil_expand_mat(mat, rho);
    /* Transform all secret-key vectors into the NTT domain (as in the
     * reference implementation): s1, s2 AND t0. */
    dil_polyvecl_ntt(&s1);
    dil_polyveck_ntt(&s2);
    dil_polyveck_ntt(&t0);

rej:
    /* y <- S^L_{GAMMA1} */
    dil_polyvecl_uniform_gamma1(&y, rhoprime, nonce++);
    z = y;
    dil_polyvecl_ntt(&z);

    /* w = A*y ; w1,w0 = Decompose(w) */
    for (unsigned int i = 0; i < DIL_K; ++i) {
        dil_polyvecl_pointwise_acc(&w.vec[i], &mat[i], &z);
        dil_poly_reduce(&w.vec[i]);
        dil_poly_invntt(&w.vec[i]);
    }
    dil_polyveck_caddq(&w);
    dil_polyveck_decompose(&w1, &w0, &w);

    /* ctilde = H(mu || packw1(w1)) */
    {
        shake_ctx c;
        uint8_t w1packed[DIL_K * DIL_POLYW1_PACKEDBYTES];
        for (unsigned int i = 0; i < DIL_K; ++i)
            dil_polyw1_pack(w1packed + i * DIL_POLYW1_PACKEDBYTES, &w1.vec[i]);
        shake_init(&c, SHAKE256_RATE);
        shake_absorb(&c, mu, DIL_CRHBYTES);
        shake_absorb(&c, w1packed, sizeof(w1packed));
        shake_finalize(&c);
        shake_squeeze(&c, ctilde, DIL_CTILDEBYTES);
    }

    dil_poly_challenge(&cp, ctilde);
    dil_poly_ntt(&cp);

    /* z = y + c*s1 ; reject if ||z|| >= GAMMA1 - BETA */
    dil_polyvecl_pointwise_poly(&z, &cp, &s1);
    dil_polyvecl_invntt(&z);
    dil_polyvecl_add(&z, &z, &y);
    dil_polyvecl_reduce(&z);
    if (dil_polyvecl_chknorm(&z, DIL_GAMMA1 - DIL_BETA)) goto rej;

    /* w0 = w0 - c*s2 ; reject if ||w0|| >= GAMMA2 - BETA */
    dil_polyveck_pointwise_poly(&h, &cp, &s2);
    dil_polyveck_invntt(&h);
    dil_polyveck_sub(&w0, &w0, &h);
    dil_polyveck_reduce(&w0);
    if (dil_polyveck_chknorm(&w0, DIL_GAMMA2 - DIL_BETA)) goto rej;

    /* h = c*t0 ; reject if ||h|| >= GAMMA2 */
    dil_polyveck_pointwise_poly(&h, &cp, &t0);
    dil_polyveck_invntt(&h);
    dil_polyveck_reduce(&h);
    if (dil_polyveck_chknorm(&h, DIL_GAMMA2)) goto rej;

    /* hints */
    dil_polyveck_add(&w0, &w0, &h);
    {
        unsigned int n = dil_polyveck_make_hint(&h, &w0, &w1);
        if (n > DIL_OMEGA) goto rej;
    }

    dil_pack_sig(sig, ctilde, &z, &h);
    *siglen = DILITHIUM_SIGNATUREBYTES;
    return 0;
}

int dilithium_verify(const uint8_t* sig, size_t siglen,
                     const uint8_t* msg, size_t msglen,
                     const uint8_t* pk)
{
    uint8_t rho[DIL_SEEDBYTES], tr[DIL_CRHBYTES], mu[DIL_CRHBYTES];
    uint8_t ctilde[DIL_CTILDEBYTES], ctilde2[DIL_CTILDEBYTES];
    dil_polyvecl mat[DIL_K], z;
    dil_polyveck t1, w1, h;
    dil_poly cp, t;

    if (siglen != DILITHIUM_SIGNATUREBYTES) return -1;
    dil_init_zetas();

    dil_unpack_pk(rho, &t1, pk);
    if (dil_unpack_sig(ctilde, &z, &h, sig) != 0) return -1;
    if (dil_polyvecl_chknorm(&z, DIL_GAMMA1 - DIL_BETA)) return -1;

    /* mu = CRH(CRH(pk) || M) */
    shake256(tr, DIL_CRHBYTES, pk, DILITHIUM_PUBLICKEYBYTES);
    {
        shake_ctx c;
        shake_init(&c, SHAKE256_RATE);
        shake_absorb(&c, tr, DIL_CRHBYTES);
        shake_absorb(&c, msg, msglen);
        shake_finalize(&c);
        shake_squeeze(&c, mu, DIL_CRHBYTES);
    }

    /* w' = A*z - c*t1*2^D ; w1' = UseHint(h, w') */
    dil_poly_challenge(&cp, ctilde);
    dil_expand_mat(mat, rho);
    dil_polyvecl_ntt(&z);
    dil_poly_ntt(&cp);

    for (unsigned int i = 0; i < DIL_K; ++i)
        for (unsigned int j = 0; j < DIL_N; ++j)
            t1.vec[i].coeffs[j] <<= DIL_D;
    for (unsigned int i = 0; i < DIL_K; ++i) dil_poly_ntt(&t1.vec[i]);

    for (unsigned int i = 0; i < DIL_K; ++i) {
        dil_polyvecl_pointwise_acc(&w1.vec[i], &mat[i], &z);
        dil_poly_pointwise(&t, &cp, &t1.vec[i]);
        for (unsigned int j = 0; j < DIL_N; ++j)
            w1.vec[i].coeffs[j] = dil_reduce32(w1.vec[i].coeffs[j] - t.coeffs[j]);
        dil_poly_invntt(&w1.vec[i]);
    }
    dil_polyveck_caddq(&w1);
    dil_polyveck_use_hint(&w1, &w1, &h);

    /* ctilde' = H(mu || packw1(w1')) */
    {
        shake_ctx c;
        uint8_t w1packed[DIL_K * DIL_POLYW1_PACKEDBYTES];
        for (unsigned int i = 0; i < DIL_K; ++i)
            dil_polyw1_pack(w1packed + i * DIL_POLYW1_PACKEDBYTES, &w1.vec[i]);
        shake_init(&c, SHAKE256_RATE);
        shake_absorb(&c, mu, DIL_CRHBYTES);
        shake_absorb(&c, w1packed, sizeof(w1packed));
        shake_finalize(&c);
        shake_squeeze(&c, ctilde2, DIL_CTILDEBYTES);
    }

    for (unsigned int i = 0; i < DIL_CTILDEBYTES; ++i)
        if (ctilde[i] != ctilde2[i]) return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Default OS CSPRNG for dilithium_randombytes(). Tests override this */
/* by defining DILITHIUM_CUSTOM_RANDOMBYTES and providing their own.  */
/* ------------------------------------------------------------------ */
#ifndef DILITHIUM_CUSTOM_RANDOMBYTES
#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
void dilithium_randombytes(uint8_t* out, size_t n)
{
    if (BCryptGenRandom(NULL, out, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        abort(); /* CSPRNG failure is fatal */
    }
}
#else
#include <stdio.h>
void dilithium_randombytes(uint8_t* out, size_t n)
{
    FILE* f = fopen("/dev/urandom", "rb");
    if (!f) abort();
    if (fread(out, 1, n, f) != n) { fclose(f); abort(); }
    fclose(f);
}
#endif
#endif /* DILITHIUM_CUSTOM_RANDOMBYTES */
