/*
 * Copyright (c)2019 ZeroTier, Inc.
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
#include "Buffer.hpp"
#include "Constants.hpp"
#include "Credential.hpp"
#include "ECC.hpp"
#include "Identity.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace ZeroTier {

class RuntimeEnvironment;

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
class Tag : public Credential {
  public:
    static inline Credential::Type credentialType()
    {
        return Credential::CREDENTIAL_TYPE_TAG;
    }

    Tag() : _id(0), _value(0), _networkId(0), _ts(0)
    {
        memset(_signature.data, 0, sizeof(_signature.data));
    }

    /**
     * @param nwid Network ID
     * @param ts Timestamp
     * @param issuedTo Address to which this tag was issued
     * @param id Tag ID
     * @param value Tag value
     */
    Tag(const uint64_t nwid, const int64_t ts, const Address& issuedTo, const uint32_t id, const uint32_t value) : _id(id), _value(value), _networkId(nwid), _ts(ts), _issuedTo(issuedTo), _signedBy()
    {
        memset(_signature.data, 0, sizeof(_signature.data));
    }

    inline uint32_t id() const
    {
        return _id;
    }
    inline const uint32_t& value() const
    {
        return _value;
    }
    inline uint64_t networkId() const
    {
        return _networkId;
    }
    inline int64_t timestamp() const
    {
        return _ts;
    }
    inline const Address& issuedTo() const
    {
        return _issuedTo;
    }
    inline const Address& signedBy() const
    {
        return _signedBy;
    }

    /**
     * Sign this tag
     *
     * @param signer Signing identity, must have private key
     * @return True if signature was successful
     */
    inline bool sign(const Identity& signer)
    {
        if (signer.hasPrivate()) {
            Buffer<sizeof(Tag) + 64> tmp;
            _signedBy = signer.address();
            this->serialize(tmp, true);
            _signature = signer.sign(tmp.data(), tmp.size());
            return true;
        }
        return false;
    }

    /**
     * Check this tag's signature
     *
     * @param RR Runtime environment to allow identity lookup for signedBy
     * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
     * @return 0 == OK, 1 == waiting for WHOIS, -1 == BAD signature or tag
     */
    int verify(const RuntimeEnvironment* RR, void* tPtr) const;

    template <unsigned int C> inline void serialize(Buffer<C>& b, const bool forSign = false) const
    {
        if (forSign) {
            b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);
        }

        b.append(_networkId);
        b.append(_ts);
        b.append(_id);
        b.append(_value);

        _issuedTo.appendTo(b);
        _signedBy.appendTo(b);
        if (! forSign) {
            b.append((uint8_t)1);                       // 1 == Ed25519
            b.append((uint16_t)ZT_ECC_SIGNATURE_LEN);   // length of signature
            b.append(_signature.data, ZT_ECC_SIGNATURE_LEN);
        }

        b.append((uint16_t)0);   // length of additional fields, currently 0

        if (forSign) {
            b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);
        }
    }

    template <unsigned int C> inline unsigned int deserialize(const Buffer<C>& b, unsigned int startAt = 0)
    {
        unsigned int p = startAt;

        *this = Tag();

        _networkId = b.template at<uint64_t>(p);
        p += 8;
        _ts = b.template at<uint64_t>(p);
        p += 8;
        _id = b.template at<uint32_t>(p);
        p += 4;

        _value = b.template at<uint32_t>(p);
        p += 4;

        _issuedTo.setTo(b.field(p, ZT_ADDRESS_LENGTH), ZT_ADDRESS_LENGTH);
        p += ZT_ADDRESS_LENGTH;
        _signedBy.setTo(b.field(p, ZT_ADDRESS_LENGTH), ZT_ADDRESS_LENGTH);
        p += ZT_ADDRESS_LENGTH;
        if (b[p++] == 1) {
            if (b.template at<uint16_t>(p) != ZT_ECC_SIGNATURE_LEN) {
                throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN;
            }
            p += 2;
            memcpy(_signature.data, b.field(p, ZT_ECC_SIGNATURE_LEN), ZT_ECC_SIGNATURE_LEN);
            p += ZT_ECC_SIGNATURE_LEN;
        }
        else {
            p += 2 + b.template at<uint16_t>(p);
        }

        p += 2 + b.template at<uint16_t>(p);
        if (p > b.size()) {
            throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;
        }

        return (p - startAt);
    }

    // Provides natural sort order by ID
    inline bool operator<(const Tag& t) const
    {
        return (_id < t._id);
    }

    inline bool operator==(const Tag& t) const
    {
        return (memcmp(this, &t, sizeof(Tag)) == 0);
    }
    inline bool operator!=(const Tag& t) const
    {
        return (memcmp(this, &t, sizeof(Tag)) != 0);
    }

    // For searching sorted arrays or lists of Tags by ID
    struct IdComparePredicate {
        inline bool operator()(const Tag& a, const Tag& b) const
        {
            return (a.id() < b.id());
        }
        inline bool operator()(const uint32_t a, const Tag& b) const
        {
            return (a < b.id());
        }
        inline bool operator()(const Tag& a, const uint32_t b) const
        {
            return (a.id() < b);
        }
        inline bool operator()(const Tag* a, const Tag* b) const
        {
            return (a->id() < b->id());
        }
        inline bool operator()(const Tag* a, const Tag& b) const
        {
            return (a->id() < b.id());
        }
        inline bool operator()(const Tag& a, const Tag* b) const
        {
            return (a.id() < b->id());
        }
        inline bool operator()(const uint32_t a, const Tag* b) const
        {
            return (a < b->id());
        }
        inline bool operator()(const Tag* a, const uint32_t b) const
        {
            return (a->id() < b);
        }
        inline bool operator()(const uint32_t a, const uint32_t b) const
        {
            return (a < b);
        }
    };

  private:
    uint32_t _id;
    uint32_t _value;
    uint64_t _networkId;
    int64_t _ts;
    Address _issuedTo;
    Address _signedBy;
    ECC::Signature _signature;
};

}   // namespace ZeroTier

#endif
