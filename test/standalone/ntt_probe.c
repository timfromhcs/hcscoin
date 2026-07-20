/* Quick NTT sanity probe for the Dilithium module (not a unit test). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crypto/dilithium.h"

void dilithium_randombytes(uint8_t* out, size_t n)
{
    static uint64_t s = 12345;
    for (size_t i = 0; i < n; ++i) { s = s * 6364136223846793005ULL + 1; out[i] = (uint8_t)(s >> 33); }
}

/* access internals by including the C file directly */
#include "../../src/crypto/dilithium.c"

int main(void)
{
    dil_init_zetas();
    printf("zetas[1]=%d (expect 25847)  zetas[2]=%d (expect -2608894)\n",
           dil_zetas[1], dil_zetas[2]);
    fflush(stdout);

    /* round-trip: invntt(ntt(a)) == a * R (mod q)  -- "_tomont" semantics */
    dil_poly a, b;
    for (int i = 0; i < DIL_N; ++i) a.coeffs[i] = (int32_t)((i * 2654435761u) % DIL_Q) - DIL_Q / 2;
    memcpy(&b, &a, sizeof(a));
    dil_poly_ntt(&b);
    dil_poly_invntt(&b);
    int bad = 0;
    for (int i = 0; i < DIL_N; ++i) {
        int32_t x = dil_caddq(dil_reduce32(b.coeffs[i]));
        int32_t y = (int32_t)(((int64_t)dil_caddq(dil_reduce32(a.coeffs[i])) * (1LL << 32)) % DIL_Q);
        if (x != y) { if (bad < 5) printf("mismatch i=%d: got %d want %d\n", i, x, y); bad++; }
    }
    printf(bad ? "NTT ROUNDTRIP FAILED (%d mismatches)\n" : "NTT ROUNDTRIP OK\n", bad);
    fflush(stdout);

    /* staged keygen probe */
    uint8_t seedbuf[3 * DIL_SEEDBYTES + DIL_CRHBYTES];
    uint8_t seed[DIL_SEEDBYTES];
    memset(seed, 0x42, sizeof(seed));
    printf("stage: shake256 expand...\n"); fflush(stdout);
    shake256(seedbuf, sizeof(seedbuf), seed, DIL_SEEDBYTES);
    printf("stage: expand_mat...\n"); fflush(stdout);
    dil_polyvecl mat[DIL_K], s1, s1hat;
    dil_polyveck s2, t1, t0;
    dil_expand_mat(mat, seedbuf);
    printf("stage: uniform_eta s1...\n"); fflush(stdout);
    for (unsigned int i = 0; i < DIL_L; ++i)
        dil_poly_uniform_eta(&s1.vec[i], seedbuf + DIL_SEEDBYTES, (uint16_t)i);
    printf("stage: uniform_eta s2...\n"); fflush(stdout);
    for (unsigned int i = 0; i < DIL_K; ++i)
        dil_poly_uniform_eta(&s2.vec[i], seedbuf + DIL_SEEDBYTES, (uint16_t)(DIL_L + i));
    printf("stage: ntt + matvec...\n"); fflush(stdout);
    s1hat = s1;
    dil_polyvecl_ntt(&s1hat);
    dil_poly t;
    for (unsigned int i = 0; i < DIL_K; ++i) {
        dil_polyvecl_pointwise_acc(&t, &mat[i], &s1hat);
        dil_poly_reduce(&t);
        dil_poly_invntt(&t);
        t1.vec[i] = t;
    }
    printf("stage: add/caddq/power2round/pack...\n"); fflush(stdout);
    dil_polyveck_add(&t1, &t1, &s2);
    dil_polyveck_caddq(&t1);
    dil_polyveck_power2round(&t1, &t0, &t1);
    printf("ALL STAGES OK\n"); fflush(stdout);
    return bad != 0;
}
