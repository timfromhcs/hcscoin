// Copyright (c) 2026 The HCScoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
//
// HCScoin: Dilithium (ML-DSA-87) key types.
//
// CDilithiumPubKey / CDilithiumKey are the post-quantum counterparts of
// CPubKey / CKey. They wrap the constant-size ML-DSA-87 key material
// (2592-byte public keys, 4896-byte secret keys, 4627-byte signatures)
// with the same value-type ergonomics as the secp256k1 types so they can
// be used in wallets, scripts and RPC code.
//
// Address format: Bech32m with HRP "hcs" and witness version 2, i.e.
// hcs1d... addresses (see doc/QUANTUM.md).

#ifndef HCSCOIN_DILITHIUM_KEY_H
#define HCSCOIN_DILITHIUM_KEY_H

#include <crypto/dilithium.h>
#include <uint256.h>

#include <array>
#include <cstring>
#include <vector>

/** Size of a Dilithium public key in bytes. */
static constexpr unsigned int DILITHIUM_PUBLIC_KEY_SIZE = DILITHIUM_PUBLICKEYBYTES;
/** Size of a Dilithium secret key in bytes. */
static constexpr unsigned int DILITHIUM_SECRET_KEY_SIZE = DILITHIUM_SECRETKEYBYTES;
/** Size of a Dilithium signature in bytes. */
static constexpr unsigned int DILITHIUM_SIGNATURE_SIZE = DILITHIUM_SIGNATUREBYTES;

/**
 * A Dilithium (ML-DSA-87) public key: 2592 bytes.
 */
class CDilithiumPubKey
{
public:
    std::array<unsigned char, DILITHIUM_PUBLIC_KEY_SIZE> m_data{};

    CDilithiumPubKey() = default;
    explicit CDilithiumPubKey(const std::vector<unsigned char>& vch)
    {
        if (vch.size() == m_data.size()) std::memcpy(m_data.data(), vch.data(), m_data.size());
    }

    bool IsValid() const
    {
        static const std::array<unsigned char, DILITHIUM_PUBLIC_KEY_SIZE> zero{};
        return m_data != zero;
    }

    const unsigned char* data() const { return m_data.data(); }
    unsigned char* data() { return m_data.data(); }
    static constexpr size_t size() { return DILITHIUM_PUBLIC_KEY_SIZE; }

    std::vector<unsigned char> ToVector() const { return {m_data.begin(), m_data.end()}; }

    /** Short key ID used for address payloads: first 32 bytes of SHA256(pk). */
    uint256 GetID() const;

    friend bool operator==(const CDilithiumPubKey& a, const CDilithiumPubKey& b) { return a.m_data == b.m_data; }
    friend bool operator!=(const CDilithiumPubKey& a, const CDilithiumPubKey& b) { return !(a == b); }

    template <typename Stream>
    void Serialize(Stream& s) const { s.write(MakeByteSpan(m_data)); }
    template <typename Stream>
    void Unserialize(Stream& s) { s.read(MakeByteSpan(m_data)); }
};

/**
 * A Dilithium (ML-DSA-87) secret key: 4896 bytes (includes public key).
 */
class CDilithiumKey
{
public:
    std::array<unsigned char, DILITHIUM_SECRET_KEY_SIZE> m_data{};

    CDilithiumKey() = default;

    bool IsValid() const
    {
        static const std::array<unsigned char, DILITHIUM_SECRET_KEY_SIZE> zero{};
        return m_data != zero;
    }

    const unsigned char* data() const { return m_data.data(); }
    unsigned char* data() { return m_data.data(); }
    static constexpr size_t size() { return DILITHIUM_SECRET_KEY_SIZE; }

    std::vector<unsigned char> ToVector() const { return {m_data.begin(), m_data.end()}; }

    /** Generate a fresh random keypair. */
    void MakeNewKey();
    /** Load keypair from a 32-byte seed (deterministic, for tests/HD). */
    void SetFromSeed(const unsigned char seed[DILITHIUM_SEEDBYTES]);
    /** Extract the public key. */
    CDilithiumPubKey GetPubKey() const;
    /** Sign a message; returns false on failure. */
    bool Sign(const std::vector<unsigned char>& msg, std::vector<unsigned char>& sig) const;
    /** Sign a 32-byte hash. */
    bool Sign(const uint256& hash, std::vector<unsigned char>& sig) const;

    friend bool operator==(const CDilithiumKey& a, const CDilithiumKey& b) { return a.m_data == b.m_data; }

    template <typename Stream>
    void Serialize(Stream& s) const { s.write(MakeByteSpan(m_data)); }
    template <typename Stream>
    void Unserialize(Stream& s) { s.read(MakeByteSpan(m_data)); }
};

/** Verify a Dilithium signature against a public key. */
bool VerifyDilithiumSignature(const CDilithiumPubKey& pubkey,
                              const std::vector<unsigned char>& msg,
                              const std::vector<unsigned char>& sig);

#endif // HCSCOIN_DILITHIUM_KEY_H
