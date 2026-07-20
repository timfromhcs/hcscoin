/*
 * HCScoin: CRYSTALS-Dilithium (NIST ML-DSA-87, FIPS 204) post-quantum
 * signatures.
 *
 * This module provides the parameter set "ML-DSA-87" (Dilithium mode 5,
 * NIST security category 5) with the exact byte sizes required by the
 * HCScoin consensus rules:
 *
 *   public key : 2592 bytes
 *   secret key : 4896 bytes
 *   signature  : 4627 bytes
 *
 * The implementation is self-contained (Keccak/SHAKE included) and has no
 * third-party dependencies. Randomness is supplied through a pluggable
 * hook, dilithium_randombytes(), which by default uses the operating
 * system CSPRNG (BCryptGenRandom on Windows, /dev/urandom elsewhere) and
 * can be overridden by tests.
 */
#ifndef HCSCOIN_CRYPTO_DILITHIUM_H
#define HCSCOIN_CRYPTO_DILITHIUM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DILITHIUM_PUBLICKEYBYTES 2592
#define DILITHIUM_SECRETKEYBYTES 4896
#define DILITHIUM_SIGNATUREBYTES 4627
#define DILITHIUM_SEEDBYTES 32

/** Pluggable randomness source (override in tests; OS CSPRNG default). */
void dilithium_randombytes(uint8_t* out, size_t n);

/** Generate a Dilithium keypair. Returns 0 on success. */
int dilithium_keygen(uint8_t* pk, uint8_t* sk);

/** Generate a keypair deterministically from a 32-byte seed (tests). */
int dilithium_keygen_from_seed(uint8_t* pk, uint8_t* sk,
                               const uint8_t seed[DILITHIUM_SEEDBYTES]);

/** Re-derive the public key from a secret key (rho, s1, s2 are in sk). */
int dilithium_pk_from_sk(uint8_t* pk, const uint8_t* sk);

/**
 * Sign message with Dilithium (randomized, FIPS 204 hedged signing).
 * sig must have room for DILITHIUM_SIGNATUREBYTES bytes; *siglen is set.
 * Returns 0 on success.
 */
int dilithium_sign(uint8_t* sig, size_t* siglen,
                   const uint8_t* msg, size_t msglen,
                   const uint8_t* sk);

/** Verify Dilithium signature. Returns 0 if valid, non-zero otherwise. */
int dilithium_verify(const uint8_t* sig, size_t siglen,
                     const uint8_t* msg, size_t msglen,
                     const uint8_t* pk);

#ifdef __cplusplus
}
#endif

#endif // HCSCOIN_CRYPTO_DILITHIUM_H
