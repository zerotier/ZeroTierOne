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

#ifndef ZT_TAG_HPP
#define ZT_TAG_HPP

#include "Address.hpp"
#include "C25519.hpp"
#include "Constants.hpp"
#include "Credential.hpp"
#include "Identity.hpp"

#define ZT_TAG_MARSHAL_SIZE_MAX (8 + 8 + 4 + 4 + 5 + 5 + 1 + 2 + ZT_SIGNATURE_BUFFER_SIZE + 2)

namespace ZeroTier {

class Context;

/**
 * A tag that can be associated with members and matched in rules
 *
 * Capabilities group rules, while tags group members subject to those
 * rules. Tag values can be matched in rules, and tags relevant to a
 * capability are presented along with it.
 *
 * E.g. a capability might be "can speak Samba/CIFS within your
 * department." This cap might have a rule to allow TCP/137 but
 * only if a given tag ID's value matches between two peers. The
 * capability is what members can do, while the tag is who they are.
 * Different departments might have tags with the same ID but different
 * values.
 *
 * Unlike capabilities tags are signed only by the issuer and are never
 * transferable.
 */
class TagCredential : public Credential {
    friend class Credential;

  public:
    static constexpr ZT_CredentialType credentialType() noexcept
    {
        return ZT_CREDENTIAL_TYPE_TAG;
    }

    ZT_INLINE TagCredential() noexcept
    {
        memoryZero(this);
    }

    /**
     * @param nwid Network ID
     * @param ts Timestamp
     * @param issuedTo Address to which this tag was issued
     * @param id Tag ID
     * @param value Tag value
     */
    ZT_INLINE TagCredential(const uint64_t nwid, const int64_t ts, const Address& issuedTo, const uint32_t id, const uint32_t value) noexcept
        : m_id(id)
        , m_value(value)
        , m_networkId(nwid)
        , m_ts(ts)
        , m_issuedTo(issuedTo)
        , m_signedBy()
        , m_signatureLength(0)
    {
    }

    ZT_INLINE uint32_t id() const noexcept
    {
        return m_id;
    }

    ZT_INLINE const uint32_t& value() const noexcept
    {
        return m_value;
    }

    ZT_INLINE uint64_t networkId() const noexcept
    {
        return m_networkId;
    }

    ZT_INLINE int64_t timestamp() const noexcept
    {
        return m_ts;
    }

    ZT_INLINE int64_t revision() const noexcept
    {
        return m_ts;
    }

    ZT_INLINE const Address& issuedTo() const noexcept
    {
        return m_issuedTo;
    }

    ZT_INLINE const Address& signer() const noexcept
    {
        return m_signedBy;
    }

    ZT_INLINE const uint8_t* signature() const noexcept
    {
        return m_signature;
    }

    ZT_INLINE unsigned int signatureLength() const noexcept
    {
        return m_signatureLength;
    }

    /**
     * Sign this tag
     *
     * @param signer Signing identity, must have private key
     * @return True if signature was successful
     */
    bool sign(const Identity& signer) noexcept;

    /**
     * Check this tag's signature
     *
     * @param RR Runtime environment to allow identity lookup for signedBy
     * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
     */
    ZT_INLINE Credential::VerifyResult verify(const Context& ctx, const CallContext& cc) const noexcept
    {
        return s_verify(ctx, cc, *this);
    }

    static constexpr int marshalSizeMax() noexcept
    {
        return ZT_TAG_MARSHAL_SIZE_MAX;
    }

    int marshal(uint8_t data[ZT_TAG_MARSHAL_SIZE_MAX], bool forSign = false) const noexcept;

    int unmarshal(const uint8_t* data, int len) noexcept;

    // Provides natural sort order by ID
    ZT_INLINE bool operator<(const TagCredential& t) const noexcept
    {
        return (m_id < t.m_id);
    }

    ZT_INLINE bool operator==(const TagCredential& t) const noexcept
    {
        return (memcmp(this, &t, sizeof(TagCredential)) == 0);
    }

    ZT_INLINE bool operator!=(const TagCredential& t) const noexcept
    {
        return (memcmp(this, &t, sizeof(TagCredential)) != 0);
    }

    // For searching sorted arrays or lists of Tags by ID
    struct IdComparePredicate {
        ZT_INLINE bool operator()(const TagCredential& a, const TagCredential& b) const noexcept
        {
            return (a.id() < b.id());
        }

        ZT_INLINE bool operator()(const uint32_t a, const TagCredential& b) const noexcept
        {
            return (a < b.id());
        }

        ZT_INLINE bool operator()(const TagCredential& a, const uint32_t b) const noexcept
        {
            return (a.id() < b);
        }

        ZT_INLINE bool operator()(const TagCredential* a, const TagCredential* b) const noexcept
        {
            return (a->id() < b->id());
        }

        ZT_INLINE bool operator()(const TagCredential* a, const TagCredential& b) const noexcept
        {
            return (a->id() < b.id());
        }

        ZT_INLINE bool operator()(const TagCredential& a, const TagCredential* b) const noexcept
        {
            return (a.id() < b->id());
        }

        ZT_INLINE bool operator()(const uint32_t a, const TagCredential* b) const noexcept
        {
            return (a < b->id());
        }

        ZT_INLINE bool operator()(const TagCredential* a, const uint32_t b) const noexcept
        {
            return (a->id() < b);
        }

        ZT_INLINE bool operator()(const uint32_t a, const uint32_t b) const noexcept
        {
            return (a < b);
        }
    };

  private:
    uint32_t m_id;
    uint32_t m_value;
    uint64_t m_networkId;
    int64_t m_ts;
    Address m_issuedTo;
    Address m_signedBy;
    unsigned int m_signatureLength;
    uint8_t m_signature[ZT_SIGNATURE_BUFFER_SIZE];
};

}   // namespace ZeroTier

#endif
