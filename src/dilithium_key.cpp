// Copyright (c) 2026 The HCScoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
//
// HCScoin: Dilithium (ML-DSA-87) key type implementations.

#include <dilithium_key.h>

#include <hash.h>

uint256 CDilithiumPubKey::GetID() const
{
    return Hash(m_data);
}

void CDilithiumKey::MakeNewKey()
{
    CDilithiumPubKey pk;
    dilithium_keygen(pk.data(), m_data.data());
}

void CDilithiumKey::SetFromSeed(const unsigned char seed[DILITHIUM_SEEDBYTES])
{
    CDilithiumPubKey pk;
    dilithium_keygen_from_seed(pk.data(), m_data.data(), seed);
}

CDilithiumPubKey CDilithiumKey::GetPubKey() const
{
    /* ML-DSA secret keys embed rho, s1 and s2, so the public key
     * (rho || t1) is exactly re-derivable (t = A*s1 + s2). */
    CDilithiumPubKey pk;
    if (dilithium_pk_from_sk(pk.data(), m_data.data()) != 0) return CDilithiumPubKey{};
    return pk;
}

bool CDilithiumKey::Sign(const std::vector<unsigned char>& msg, std::vector<unsigned char>& sig) const
{
    sig.resize(DILITHIUM_SIGNATURE_SIZE);
    size_t siglen = 0;
    if (dilithium_sign(sig.data(), &siglen, msg.data(), msg.size(), m_data.data()) != 0) {
        sig.clear();
        return false;
    }
    sig.resize(siglen);
    return true;
}

bool CDilithiumKey::Sign(const uint256& hash, std::vector<unsigned char>& sig) const
{
    return Sign(std::vector<unsigned char>(hash.begin(), hash.end()), sig);
}

bool VerifyDilithiumSignature(const CDilithiumPubKey& pubkey,
                              const std::vector<unsigned char>& msg,
                              const std::vector<unsigned char>& sig)
{
    if (sig.size() != DILITHIUM_SIGNATURE_SIZE) return false;
    return dilithium_verify(sig.data(), sig.size(), msg.data(), msg.size(), pubkey.data()) == 0;
}
