/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "Identity.hpp"

#include "Constants.hpp"
#include "Endpoint.hpp"
#include "MIMC52.hpp"
#include "SHA512.hpp"
#include "Salsa20.hpp"
#include "Utils.hpp"

#include <memory>
#include <utility>

namespace ZeroTier {

namespace {

// This is the memory-intensive hash function used to compute v0 identities from v0 public keys.
#define ZT_V0_IDENTITY_GEN_MEMORY 2097152

void identityV0ProofOfWorkFrankenhash(
    const void* const restrict c25519CombinedPublicKey,
    void* const restrict digest,
    void* const restrict genmem) noexcept
{
    // Digest publicKey[] to obtain initial digest
    SHA512(digest, c25519CombinedPublicKey, ZT_C25519_COMBINED_PUBLIC_KEY_SIZE);

    // Initialize genmem[] using Salsa20 in a CBC-like configuration since
    // ordinary Salsa20 is randomly seek-able. This is good for a cipher
    // but is not what we want for sequential memory-hardness.
    Utils::zero<ZT_V0_IDENTITY_GEN_MEMORY>(genmem);
    Salsa20 s20(digest, (char*)digest + 32);
    s20.crypt20((char*)genmem, (char*)genmem, 64);
    for (unsigned long i = 64; i < ZT_V0_IDENTITY_GEN_MEMORY; i += 64) {
        unsigned long k = i - 64;
        *((uint64_t*)((char*)genmem + i)) = *((uint64_t*)((char*)genmem + k));
        *((uint64_t*)((char*)genmem + i + 8)) = *((uint64_t*)((char*)genmem + k + 8));
        *((uint64_t*)((char*)genmem + i + 16)) = *((uint64_t*)((char*)genmem + k + 16));
        *((uint64_t*)((char*)genmem + i + 24)) = *((uint64_t*)((char*)genmem + k + 24));
        *((uint64_t*)((char*)genmem + i + 32)) = *((uint64_t*)((char*)genmem + k + 32));
        *((uint64_t*)((char*)genmem + i + 40)) = *((uint64_t*)((char*)genmem + k + 40));
        *((uint64_t*)((char*)genmem + i + 48)) = *((uint64_t*)((char*)genmem + k + 48));
        *((uint64_t*)((char*)genmem + i + 56)) = *((uint64_t*)((char*)genmem + k + 56));
        s20.crypt20((char*)genmem + i, (char*)genmem + i, 64);
    }

    // Render final digest using genmem as a lookup table
    for (unsigned long i = 0; i < (ZT_V0_IDENTITY_GEN_MEMORY / sizeof(uint64_t));) {
        unsigned long idx1 = (unsigned long)(Utils::ntoh(((uint64_t*)genmem)[i++]) % (64 / sizeof(uint64_t)));
        unsigned long idx2 =
            (unsigned long)(Utils::ntoh(((uint64_t*)genmem)[i++]) % (ZT_V0_IDENTITY_GEN_MEMORY / sizeof(uint64_t)));
        uint64_t tmp = ((uint64_t*)genmem)[idx2];
        ((uint64_t*)genmem)[idx2] = ((uint64_t*)digest)[idx1];
        ((uint64_t*)digest)[idx1] = tmp;
        s20.crypt20(digest, digest, 64);
    }
}

struct identityV0ProofOfWorkCriteria {
    ZT_INLINE identityV0ProofOfWorkCriteria(unsigned char* restrict sb, char* restrict gm) noexcept
        : digest(sb)
        , genmem(gm)
    {
    }

    ZT_INLINE bool operator()(const uint8_t pub[ZT_C25519_COMBINED_PUBLIC_KEY_SIZE]) const noexcept
    {
        identityV0ProofOfWorkFrankenhash(pub, digest, genmem);
        return (digest[0] < 17);
    }

    unsigned char* restrict digest;
    char* restrict genmem;
};

void v1ChallengeFromPub(const uint8_t pub[ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE], uint64_t challenge[4])
{
    // This builds a 256-bit challenge by XORing the two public keys together. This doesn't need to be
    // a hash, just different for different public keys. Public keys are basically kind of hashes of
    // private keys, so that's good enough. This is only used to seed a PRNG in MIMC52 for a proof of
    // sequential work. It's not used for authentication beyond checking PoW.
    Utils::copy<32>(challenge, pub + 7);
    challenge[0] ^= Utils::loadMachineEndian<uint64_t>(pub + 40);
    challenge[1] ^= Utils::loadMachineEndian<uint64_t>(pub + 48);
    challenge[2] ^= Utils::loadMachineEndian<uint64_t>(pub + 56);
    challenge[3] ^= Utils::loadMachineEndian<uint64_t>(pub + 64);
    challenge[0] ^= Utils::loadMachineEndian<uint64_t>(pub + 72);
    challenge[1] ^= Utils::loadMachineEndian<uint64_t>(pub + 80);
    challenge[2] ^= Utils::loadMachineEndian<uint64_t>(pub + 88);
    challenge[3] ^= Utils::loadMachineEndian<uint64_t>(pub + 96);
    challenge[0] ^= Utils::loadMachineEndian<uint64_t>(pub + 104);
    challenge[1] ^= Utils::loadMachineEndian<uint64_t>(pub + 112);
}

}   // anonymous namespace

const Identity Identity::NIL;

bool Identity::generate(const Type t)
{
    m_type = t;
    m_hasPrivate = true;

    switch (t) {
        case C25519: {
            // Generate C25519/Ed25519 key pair whose hash satisfies a "hashcash" criterion and generate the
            // address from the last 40 bits of this hash. This is different from the fingerprint hash for V0.
            uint8_t digest[64];
            char* const genmem = new char[ZT_V0_IDENTITY_GEN_MEMORY];
            Address address;
            do {
                C25519::generateSatisfying(identityV0ProofOfWorkCriteria(digest, genmem), m_pub, m_priv);
                address.setTo(digest + 59);
            } while (address.isReserved());
            delete[] genmem;
            m_fp.address = address;   // address comes from PoW hash for type 0 identities
            m_computeHash();
        } break;

        case P384:
            for (;;) {
                C25519::generateCombined(m_pub + 7, m_priv);
                ECC384GenerateKey(
                    m_pub + 7 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE,
                    m_priv + ZT_C25519_COMBINED_PRIVATE_KEY_SIZE);

                uint64_t challenge[4];
                v1ChallengeFromPub(m_pub, challenge);
                const uint64_t proof =
                    MIMC52::delay(reinterpret_cast<const uint8_t*>(challenge), ZT_IDENTITY_TYPE1_MIMC52_ROUNDS);
                m_pub[0] = (uint8_t)(proof >> 48U);
                m_pub[1] = (uint8_t)(proof >> 40U);
                m_pub[2] = (uint8_t)(proof >> 32U);
                m_pub[3] = (uint8_t)(proof >> 24U);
                m_pub[4] = (uint8_t)(proof >> 16U);
                m_pub[5] = (uint8_t)(proof >> 8U);
                m_pub[6] = (uint8_t)proof;

                m_computeHash();
                const Address addr(m_fp.hash);
                if (! addr.isReserved()) {
                    m_fp.address = addr;
                    break;
                }
            }
            break;

        default:
            return false;
    }

    return true;
}

bool Identity::locallyValidate() const noexcept
{
    try {
        if ((m_fp) && ((! Address(m_fp.address).isReserved()))) {
            switch (m_type) {
                case C25519: {
                    uint8_t digest[64];
                    char* const genmem = (char*)malloc(ZT_V0_IDENTITY_GEN_MEMORY);
                    if (! genmem)
                        return false;
                    identityV0ProofOfWorkFrankenhash(m_pub, digest, genmem);
                    free(genmem);
                    return ((Address(digest + 59) == m_fp.address) && (digest[0] < 17));
                }

                case P384:
                    if (Address(m_fp.hash) == m_fp.address) {
                        uint64_t challenge[4];
                        v1ChallengeFromPub(m_pub, challenge);
                        return MIMC52::verify(
                            reinterpret_cast<const uint8_t*>(challenge),
                            ZT_IDENTITY_TYPE1_MIMC52_ROUNDS,
                            ((uint64_t)m_pub[0] << 48U) | ((uint64_t)m_pub[1] << 40U) | ((uint64_t)m_pub[2] << 32U)
                                | ((uint64_t)m_pub[3] << 24U) | ((uint64_t)m_pub[4] << 16U) | ((uint64_t)m_pub[5] << 8U)
                                | (uint64_t)m_pub[6]);
                    }
                    return false;
            }
        }
    }
    catch (...) {
    }
    return false;
}

void Identity::hashWithPrivate(uint8_t h[ZT_FINGERPRINT_HASH_SIZE]) const
{
    if (m_hasPrivate) {
        switch (m_type) {
            case C25519:
                SHA384(h, m_pub, ZT_C25519_COMBINED_PUBLIC_KEY_SIZE, m_priv, ZT_C25519_COMBINED_PRIVATE_KEY_SIZE);
                return;

            case P384:
                SHA384(h, m_pub, sizeof(m_pub), m_priv, sizeof(m_priv));
                return;
        }
    }
    Utils::zero<ZT_FINGERPRINT_HASH_SIZE>(h);
}

unsigned int Identity::sign(const void* data, unsigned int len, void* sig, unsigned int siglen) const
{
    if (m_hasPrivate) {
        switch (m_type) {
            case C25519:
                if (siglen >= ZT_C25519_SIGNATURE_LEN) {
                    C25519::sign(m_priv, m_pub, data, len, sig);
                    return ZT_C25519_SIGNATURE_LEN;
                }
                break;

            case P384:
                if (siglen >= ZT_ECC384_SIGNATURE_SIZE) {
                    static_assert(ZT_ECC384_SIGNATURE_HASH_SIZE == ZT_SHA384_DIGEST_SIZE, "weird!");
                    uint8_t h[ZT_ECC384_SIGNATURE_HASH_SIZE];
                    SHA384(h, data, len, m_pub, ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE);
                    ECC384ECDSASign(m_priv + ZT_C25519_COMBINED_PRIVATE_KEY_SIZE, h, (uint8_t*)sig);
                    return ZT_ECC384_SIGNATURE_SIZE;
                }
                break;
        }
    }
    return 0;
}

bool Identity::verify(const void* data, unsigned int len, const void* sig, unsigned int siglen) const
{
    switch (m_type) {
        case C25519:
            return C25519::verify(m_pub, data, len, sig, siglen);

        case P384:
            if (siglen == ZT_ECC384_SIGNATURE_SIZE) {
                uint8_t h[ZT_ECC384_SIGNATURE_HASH_SIZE];
                SHA384(h, data, len, m_pub, ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE);
                return ECC384ECDSAVerify(m_pub + 7 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE, h, (const uint8_t*)sig);
            }
            break;
    }
    return false;
}

bool Identity::agree(const Identity& id, uint8_t key[ZT_SYMMETRIC_KEY_SIZE]) const
{
    uint8_t rawkey[128], h[64];
    if (m_hasPrivate) {
        if ((m_type == C25519) || (id.m_type == C25519)) {
            // If we are a C25519 key we can agree with another C25519 key or with only the
            // C25519 portion of a type 1 P-384 key.
            C25519::agree(m_priv, id.m_pub, rawkey);
            SHA512(h, rawkey, ZT_C25519_ECDH_SHARED_SECRET_SIZE);
            Utils::copy<ZT_SYMMETRIC_KEY_SIZE>(key, h);
            return true;
        }
        else if ((m_type == P384) && (id.m_type == P384)) {
            // For another P384 identity we execute DH agreement with BOTH keys and then
            // hash the results together. For those (cough FIPS cough) who only consider
            // P384 to be kosher, the C25519 secret can be considered a "salt"
            // or something. For those who don't trust P384 this means the privacy of
            // your traffic is also protected by C25519.
            C25519::agree(m_priv, id.m_pub, rawkey);
            ECC384ECDH(
                id.m_pub + 7 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE,
                m_priv + ZT_C25519_COMBINED_PRIVATE_KEY_SIZE,
                rawkey + ZT_C25519_ECDH_SHARED_SECRET_SIZE);
            SHA384(key, rawkey, ZT_C25519_ECDH_SHARED_SECRET_SIZE + ZT_ECC384_SHARED_SECRET_SIZE);
            return true;
        }
    }
    return false;
}

char* Identity::toString(bool includePrivate, char buf[ZT_IDENTITY_STRING_BUFFER_LENGTH]) const
{
    char* p = buf;
    Address(m_fp.address).toString(p);
    p += 10;
    *(p++) = ':';

    switch (m_type) {
        case C25519: {
            *(p++) = '0';
            *(p++) = ':';
            Utils::hex(m_pub, ZT_C25519_COMBINED_PUBLIC_KEY_SIZE, p);
            p += ZT_C25519_COMBINED_PUBLIC_KEY_SIZE * 2;
            if ((m_hasPrivate) && (includePrivate)) {
                *(p++) = ':';
                Utils::hex(m_priv, ZT_C25519_COMBINED_PRIVATE_KEY_SIZE, p);
                p += ZT_C25519_COMBINED_PRIVATE_KEY_SIZE * 2;
            }
            *p = (char)0;
            return buf;
        }
        case P384: {
            *(p++) = '1';
            *(p++) = ':';
            int el =
                Utils::b32e(m_pub, sizeof(m_pub), p, (int)(ZT_IDENTITY_STRING_BUFFER_LENGTH - (uintptr_t)(p - buf)));
            if (el <= 0)
                return nullptr;
            p += el;
            if ((m_hasPrivate) && (includePrivate)) {
                *(p++) = ':';
                el = Utils::b32e(
                    m_priv,
                    sizeof(m_priv),
                    p,
                    (int)(ZT_IDENTITY_STRING_BUFFER_LENGTH - (uintptr_t)(p - buf)));
                if (el <= 0)
                    return nullptr;
                p += el;
            }
            *p = (char)0;
            return buf;
        }
        default:
            buf[0] = 0;
    }

    return nullptr;
}

bool Identity::fromString(const char* str)
{
    char tmp[ZT_IDENTITY_STRING_BUFFER_LENGTH];
    memoryZero(this);
    if ((! str) || (! Utils::scopy(tmp, sizeof(tmp), str)))
        return false;

    int fno = 0;
    char* saveptr = nullptr;
    for (char* f = Utils::stok(tmp, ":", &saveptr); ((f) && (fno < 4)); f = Utils::stok(nullptr, ":", &saveptr)) {
        switch (fno++) {
            case 0:
                m_fp.address = Utils::hexStrToU64(f) & ZT_ADDRESS_MASK;
                if (Address(m_fp.address).isReserved())
                    return false;
                break;

            case 1:
                if ((f[0] == '0') && (! f[1])) {
                    m_type = C25519;
                }
                else if ((f[0] == '1') && (! f[1])) {
                    m_type = P384;
                }
                else {
                    return false;
                }
                break;

            case 2:
                switch (m_type) {
                    case C25519:
                        if (Utils::unhex(f, strlen(f), m_pub, ZT_C25519_COMBINED_PUBLIC_KEY_SIZE)
                            != ZT_C25519_COMBINED_PUBLIC_KEY_SIZE)
                            return false;
                        break;

                    case P384:
                        if (Utils::b32d(f, m_pub, sizeof(m_pub)) != sizeof(m_pub))
                            return false;
                        break;
                }
                break;

            case 3:
                if (strlen(f) > 1) {
                    switch (m_type) {
                        case C25519:
                            if (Utils::unhex(f, strlen(f), m_priv, ZT_C25519_COMBINED_PRIVATE_KEY_SIZE)
                                != ZT_C25519_COMBINED_PRIVATE_KEY_SIZE) {
                                return false;
                            }
                            else {
                                m_hasPrivate = true;
                            }
                            break;

                        case P384:
                            if (Utils::b32d(f, m_priv, sizeof(m_priv)) != sizeof(m_priv)) {
                                return false;
                            }
                            else {
                                m_hasPrivate = true;
                            }
                            break;
                    }
                    break;
                }
        }
    }

    if (fno < 3)
        return false;

    m_computeHash();
    return ! ((m_type == P384) && (Address(m_fp.hash) != m_fp.address));
}

int Identity::marshal(uint8_t data[ZT_IDENTITY_MARSHAL_SIZE_MAX], const bool includePrivate) const noexcept
{
    Address(m_fp.address).copyTo(data);
    switch (m_type) {
        case C25519:
            data[ZT_ADDRESS_LENGTH] = (uint8_t)C25519;
            Utils::copy<ZT_C25519_COMBINED_PUBLIC_KEY_SIZE>(data + ZT_ADDRESS_LENGTH + 1, m_pub);
            if ((includePrivate) && (m_hasPrivate)) {
                data[ZT_ADDRESS_LENGTH + 1 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE] = ZT_C25519_COMBINED_PRIVATE_KEY_SIZE;
                Utils::copy<ZT_C25519_COMBINED_PRIVATE_KEY_SIZE>(
                    data + ZT_ADDRESS_LENGTH + 1 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE + 1,
                    m_priv);
                return ZT_ADDRESS_LENGTH + 1 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE + 1
                       + ZT_C25519_COMBINED_PRIVATE_KEY_SIZE;
            }
            data[ZT_ADDRESS_LENGTH + 1 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE] = 0;
            return ZT_ADDRESS_LENGTH + 1 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE + 1;

        case P384:
            data[ZT_ADDRESS_LENGTH] = (uint8_t)P384;
            Utils::copy<ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE>(data + ZT_ADDRESS_LENGTH + 1, m_pub);
            if ((includePrivate) && (m_hasPrivate)) {
                data[ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE] =
                    ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE;
                Utils::copy<ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE>(
                    data + ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1,
                    m_priv);
                return ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1
                       + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE;
            }
            data[ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE] = 0;
            return ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1;
    }
    return -1;
}

int Identity::unmarshal(const uint8_t* data, const int len) noexcept
{
    memoryZero(this);

    if (len < (1 + ZT_ADDRESS_LENGTH))
        return -1;
    m_fp.address = Address(data);

    unsigned int privlen;
    switch ((m_type = (Type)data[ZT_ADDRESS_LENGTH])) {
        case C25519:
            if (len < (ZT_ADDRESS_LENGTH + 1 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE + 1))
                return -1;

            Utils::copy<ZT_C25519_COMBINED_PUBLIC_KEY_SIZE>(m_pub, data + ZT_ADDRESS_LENGTH + 1);
            m_computeHash();

            privlen = data[ZT_ADDRESS_LENGTH + 1 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE];
            if (privlen == ZT_C25519_COMBINED_PRIVATE_KEY_SIZE) {
                if (len
                    < (ZT_ADDRESS_LENGTH + 1 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE + 1
                       + ZT_C25519_COMBINED_PRIVATE_KEY_SIZE))
                    return -1;
                m_hasPrivate = true;
                Utils::copy<ZT_C25519_COMBINED_PRIVATE_KEY_SIZE>(
                    m_priv,
                    data + ZT_ADDRESS_LENGTH + 1 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE + 1);
                return ZT_ADDRESS_LENGTH + 1 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE + 1
                       + ZT_C25519_COMBINED_PRIVATE_KEY_SIZE;
            }
            else if (privlen == 0) {
                m_hasPrivate = false;
                return ZT_ADDRESS_LENGTH + 1 + ZT_C25519_COMBINED_PUBLIC_KEY_SIZE + 1;
            }
            break;

        case P384:
            if (len < (ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1))
                return -1;

            Utils::copy<ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE>(m_pub, data + ZT_ADDRESS_LENGTH + 1);
            m_computeHash();                          // this sets the address for P384
            if (Address(m_fp.hash) != m_fp.address)   // this sanity check is possible with V1 identities
                return -1;

            privlen = data[ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE];
            if (privlen == 0) {
                m_hasPrivate = false;
                return ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1;
            }
            else if (privlen == ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE) {
                if (len
                    < (ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1
                       + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE))
                    return -1;
                m_hasPrivate = true;
                Utils::copy<ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE>(
                    &m_priv,
                    data + ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1);
                return ZT_ADDRESS_LENGTH + 1 + ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE + 1
                       + ZT_IDENTITY_P384_COMPOUND_PRIVATE_KEY_SIZE;
            }
            break;
    }

    return -1;
}

void Identity::m_computeHash()
{
    switch (m_type) {
        default:
            m_fp.zero();
            break;

        case C25519:
            SHA384(m_fp.hash, m_pub, ZT_C25519_COMBINED_PUBLIC_KEY_SIZE);
            break;

        case P384:
            SHA384(m_fp.hash, m_pub, ZT_IDENTITY_P384_COMPOUND_PUBLIC_KEY_SIZE);
            break;
    }
}

}   // namespace ZeroTier
