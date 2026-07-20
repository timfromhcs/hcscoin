/* Probe: pack/unpack roundtrip + bounded Dilithium signing diagnostics. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crypto/dilithium.h"

void dilithium_randombytes(uint8_t* out, size_t n)
{
    static uint64_t s = 999;
    for (size_t i = 0; i < n; ++i) { s = s * 6364136223846793005ULL + 1; out[i] = (uint8_t)(s >> 33); }
}

#include "../../src/crypto/dilithium.c"

static int maxnorm(const dil_poly* a)
{
    int32_t m = 0;
    for (int i = 0; i < DIL_N; ++i) {
        int32_t t = a->coeffs[i];
        t = t - ((t >> 31) & (2 * t));
        if (t > m) m = t;
    }
    return m;
}

int main(void)
{
    static uint8_t pk[DILITHIUM_PUBLICKEYBYTES], sk[DILITHIUM_SECRETKEYBYTES];
    uint8_t seed[32];
    memset(seed, 0x42, sizeof(seed));
    printf("keygen...\n"); fflush(stdout);
    dilithium_keygen_from_seed(pk, sk, seed);

    /* 1. sk roundtrip: unpack and re-pack, compare bytes */
    {
        uint8_t rho[32], key[32], tr[64];
        dil_polyvecl s1;
        dil_polyveck s2, t0;
        static uint8_t sk2[DILITHIUM_SECRETKEYBYTES];
        dil_unpack_sk(rho, key, tr, &s1, &s2, &t0, sk);
        dil_pack_sk(sk2, rho, key, tr, &s1, &s2, &t0);
        printf("sk roundtrip: %s\n", memcmp(sk, sk2, sizeof(sk)) == 0 ? "OK" : "MISMATCH");
        fflush(stdout);
    }

    /* 2. replicate sign flow once with diagnostics */
    {
        uint8_t rho[32], key[32], tr[64], mu[64], rhoprime[64], rnd[32], ctilde[64];
        const char* msg = "probe message";
        dil_polyvecl mat[DIL_K], s1, y, z;
        dil_polyveck s2, t0, w, w1, w0, h;
        dil_poly cp;
        uint16_t nonce = 0;

        dil_unpack_sk(rho, key, tr, &s1, &s2, &t0, sk);
        shake_ctx c;
        shake_init(&c, SHAKE256_RATE);
        shake_absorb(&c, tr, 64);
        shake_absorb(&c, (const uint8_t*)msg, strlen(msg));
        shake_finalize(&c);
        shake_squeeze(&c, mu, 64);
        memset(rnd, 1, 32);
        shake_init(&c, SHAKE256_RATE);
        shake_absorb(&c, key, 32);
        shake_absorb(&c, rnd, 32);
        shake_absorb(&c, mu, 64);
        shake_finalize(&c);
        shake_squeeze(&c, rhoprime, 64);
        dil_expand_mat(mat, rho);
        dil_polyvecl_ntt(&s1);

        for (int attempt = 0; attempt < 5; ++attempt) {
            printf("attempt %d...\n", attempt); fflush(stdout);
            dil_polyvecl_uniform_gamma1(&y, rhoprime, nonce++);
            z = y;
            dil_polyvecl_ntt(&z);
            for (unsigned int i = 0; i < DIL_K; ++i) {
                dil_polyvecl_pointwise_acc(&w.vec[i], &mat[i], &z);
                dil_poly_reduce(&w.vec[i]);
                dil_poly_invntt(&w.vec[i]);
            }
            dil_polyveck_caddq(&w);
            dil_polyveck_decompose(&w1, &w0, &w);
            {
                uint8_t w1packed[DIL_K * DIL_POLYW1_PACKEDBYTES];
                for (unsigned int i = 0; i < DIL_K; ++i)
                    dil_polyw1_pack(w1packed + i * DIL_POLYW1_PACKEDBYTES, &w1.vec[i]);
                shake_init(&c, SHAKE256_RATE);
                shake_absorb(&c, mu, 64);
                shake_absorb(&c, w1packed, sizeof(w1packed));
                shake_finalize(&c);
                shake_squeeze(&c, ctilde, 64);
            }
            dil_poly_challenge(&cp, ctilde);
            dil_poly_ntt(&cp);

            dil_polyvecl_pointwise_poly(&z, &cp, &s1);
            dil_polyvecl_invntt(&z);
            dil_polyvecl_add(&z, &z, &y);
            dil_polyvecl_reduce(&z);
            printf("  max|z| = %d  (bound %d)\n", maxnorm(&z.vec[0]), DIL_GAMMA1 - DIL_BETA);
            fflush(stdout);

            dil_polyveck_pointwise_poly(&h, &cp, &s2);
            dil_polyveck_invntt(&h);
            dil_polyveck_sub(&w0, &w0, &h);
            dil_polyveck_reduce(&w0);
            printf("  max|w0-cs2| = %d  (bound %d)\n", maxnorm(&w0.vec[0]), DIL_GAMMA2 - DIL_BETA);
            fflush(stdout);

            dil_polyveck_pointwise_poly(&h, &cp, &t0);
            dil_polyveck_invntt(&h);
            dil_polyveck_reduce(&h);
            printf("  max|ct0| = %d  (bound %d)\n", maxnorm(&h.vec[0]), DIL_GAMMA2);
            fflush(stdout);

            dil_polyveck_add(&w0, &w0, &h);
            unsigned int nhints = dil_polyveck_make_hint(&h, &w0, &w1);
            printf("  hints = %u (max %d)\n", nhints, DIL_OMEGA);
            fflush(stdout);
        }
    }
    printf("PROBE DONE\n"); fflush(stdout);
    return 0;
}
