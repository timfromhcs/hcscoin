/*
 * HCScoin: Standalone unit tests for the ML-DSA-87 (Dilithium) module.
 *
 * Build (from repo root):
 *   cc -O2 -DDILITHIUM_CUSTOM_RANDOMBYTES \
 *      -I src src/crypto/dilithium.c test/standalone/dilithium_test.c \
 *      -o dilithium_test
 *
 * Exit code 0 = all tests passed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crypto/dilithium.h"

/* Deterministic RNG for reproducible tests (xorshift64*). */
static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;

void dilithium_randombytes(uint8_t* out, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        rng_state ^= rng_state >> 12;
        rng_state ^= rng_state << 25;
        rng_state ^= rng_state >> 27;
        out[i] = (uint8_t)((rng_state * 0x2545F4914F6CDD1DULL) >> 56);
    }
}

static int failures = 0;

#define CHECK(cond, name) do { \
    if (cond) { printf("  [PASS] %s\n", name); } \
    else { printf("  [FAIL] %s\n", name); failures++; } \
} while (0)

int main(void)
{
    static uint8_t pk[DILITHIUM_PUBLICKEYBYTES];
    static uint8_t sk[DILITHIUM_SECRETKEYBYTES];
    static uint8_t sig[DILITHIUM_SIGNATUREBYTES];
    static uint8_t pk2[DILITHIUM_PUBLICKEYBYTES];
    static uint8_t sk2[DILITHIUM_SECRETKEYBYTES];
    size_t siglen = 0;

    const char* msg1 = "HCScoin: quantum-resistant transaction test vector #1";
    const char* msg2 = "HCScoin: quantum-resistant transaction test vector #2";

    printf("== dilithium (ML-DSA-87) self-tests ==\n");
    fflush(stdout);

    /* 1. keygen produces correctly sized, non-trivial material */
    if (dilithium_keygen(pk, sk) != 0) { printf("  [FAIL] keygen rc\n"); return 1; }
    {
        int nonzero = 0;
        for (size_t i = 0; i < sizeof(pk); ++i) nonzero |= pk[i];
        CHECK(nonzero != 0, "keygen produces non-zero public key");
        CHECK(sizeof(pk) == 2592, "public key size is 2592 bytes");
        CHECK(sizeof(sk) == 4896, "secret key size is 4896 bytes");
    }

    /* 2. sign + verify round trip */
    if (dilithium_sign(sig, &siglen, (const uint8_t*)msg1, strlen(msg1), sk) != 0) {
        printf("  [FAIL] sign rc\n"); return 1;
    }
    CHECK(siglen == DILITHIUM_SIGNATUREBYTES, "signature size is 4627 bytes");
    CHECK(dilithium_verify(sig, siglen, (const uint8_t*)msg1, strlen(msg1), pk) == 0,
          "verify(sig, msg, pk) succeeds");

    /* 3. wrong message must fail */
    CHECK(dilithium_verify(sig, siglen, (const uint8_t*)msg2, strlen(msg2), pk) != 0,
          "verify rejects wrong message");

    /* 4. wrong key must fail */
    if (dilithium_keygen(pk2, sk2) != 0) return 1;
    CHECK(dilithium_verify(sig, siglen, (const uint8_t*)msg1, strlen(msg1), pk2) != 0,
          "verify rejects wrong public key");

    /* 5. bit-flipped signature must fail */
    sig[100] ^= 0x01;
    CHECK(dilithium_verify(sig, siglen, (const uint8_t*)msg1, strlen(msg1), pk) != 0,
          "verify rejects corrupted signature");
    sig[100] ^= 0x01; /* restore */

    /* 6. truncated/extended lengths must fail */
    CHECK(dilithium_verify(sig, siglen - 1, (const uint8_t*)msg1, strlen(msg1), pk) != 0,
          "verify rejects truncated signature");

    /* 6b. public key re-derivation from secret key */
    {
        uint8_t pk_derived[DILITHIUM_PUBLICKEYBYTES];
        CHECK(dilithium_pk_from_sk(pk_derived, sk) == 0 && memcmp(pk_derived, pk, sizeof(pk)) == 0,
              "pk re-derived from sk matches");
    }

    /* 7. deterministic keygen from seed is stable */
    {
        uint8_t seed[32];
        memset(seed, 0x42, sizeof(seed));
        dilithium_keygen_from_seed(pk, sk, seed);
        dilithium_keygen_from_seed(pk2, sk2, seed);
        CHECK(memcmp(pk, pk2, sizeof(pk)) == 0 && memcmp(sk, sk2, sizeof(sk)) == 0,
              "seeded keygen is deterministic");
        /* and the seeded key still signs/verifies */
        if (dilithium_sign(sig, &siglen, (const uint8_t*)msg1, strlen(msg1), sk) == 0)
            CHECK(dilithium_verify(sig, siglen, (const uint8_t*)msg1, strlen(msg1), pk) == 0,
                  "seeded key sign/verify round trip");
    }

    /* 8. stress: 20 random messages, sign & verify each */
    {
        int ok = 1;
        uint8_t m[64];
        for (int i = 0; i < 20 && ok; ++i) {
            dilithium_randombytes(m, sizeof(m));
            if (dilithium_sign(sig, &siglen, m, sizeof(m), sk2) != 0) ok = 0;
            else if (dilithium_verify(sig, siglen, m, sizeof(m), pk2) != 0) ok = 0;
        }
        CHECK(ok, "20x random message sign/verify");
    }

    if (failures == 0) printf("ALL DILITHIUM TESTS PASSED\n");
    else printf("%d TEST(S) FAILED\n", failures);
    return failures ? 1 : 0;
}
