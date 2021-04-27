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

#ifndef ZT_CERTIFICATEOFMEMBERSHIP_HPP
#define ZT_CERTIFICATEOFMEMBERSHIP_HPP

#include "Address.hpp"
#include "C25519.hpp"
#include "Constants.hpp"
#include "Credential.hpp"
#include "FCV.hpp"
#include "Identity.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

// Maximum number of additional tuples beyond the standard always-present three.
#define ZT_MEMBERSHIP_CREDENTIAL_MAX_ADDITIONAL_QUALIFIERS 8

// version + qualifier count + three required qualifiers + additional qualifiers +
#define ZT_MEMBERSHIP_CREDENTIAL_MARSHAL_SIZE_MAX (1 + 2 + (3 * 3 * 8) + (ZT_MEMBERSHIP_CREDENTIAL_MAX_ADDITIONAL_QUALIFIERS * 3 * 8) + 144 + 5 + 2 + 96)

namespace ZeroTier {

class Context;

/**
 * Certificate of network membership
 *
 * This is the fundamental permission object issued by network controllers to members of networks
 * to admit them into networks.
 *
 * A certificate of membership (COM) consists of a series of tuples called qualifiers as well
 * as the full identity fingerprint of the node being admitted, the address of the controller
 * (for sanity checking), and a signature.
 *
 * A qualifier is a tuple of three 64-bit unsigned integers: an id, a value, and a delta.
 *
 * Certiciates are checked between peers by determining if they agree. If the absolute value
 * of the difference between any two qualifier values exceeds its delta, the certificates do
 * not agree. A delta if 1 for example means that the values of two peers may differ by no more
 * than one. A delta of 0 indicates values that must be the same. A delta of uint64_max is for
 * informational tuples that are not included in certificate checking, as this means they may
 * differ by any amount.
 *
 * All COMs contain three initial tuples: timestamp, network ID, and the address of the
 * issued-to node. The latter is informational. The network ID must equal exactly, though in
 * theory a controller could allow a delta there to e.g. allow cross-communication between all
 * of its networks. (This has never been done in practice.) The most important field is the
 * timestamp, whose delta defines a moving window within which certificates must be timestamped
 * by the network controller to agree. A certificate that is too old will fall out of this
 * window vs its peers and will no longer be considered valid.
 *
 * (Revocations are a method to rapidly revoke access that works alongside this slower but
 * more definitive method.)
 *
 * Certificate of membership wire format:
 *
 * This wire format comes in two versions: version 1 for ZeroTier 1.x, which will
 * eventually go away once 1.x is out of support, and version 2 for ZeroTier 2.x and later.
 *
 * Version 2:
 *
 * <[1] wire format type byte: 1 or 2>
 * <[2] 16-bit number of qualifier tuples>
 * <[...] qualifier tuples>
 * <[48] fingerprint hash of identity of peer to whom COM was issued>
 * <[5] address of network controller>
 * <[2] 16-bit size of signature>
 * <[...] signature>
 *
 * Version 1 is identical except the fingerprint hash is omitted and is instead loaded
 * into a series of six informational tuples. The signature size is also omitted and a
 * 96-byte signature field is assumed.
 *
 * Qualifier tuples must appear in numeric order of ID, and the first three tuples
 * must have IDs 0, 1, and 2 being the timestamp, network ID, and issued-to address
 * respectively. In version 1 COMs the IDs 3-8 are used to pack in the full identity
 * fingerprint, so these are reserved as well. Optional additional tuples (not currently
 * used) must use ID 65536 or higher.
 *
 * Signatures are computed over tuples only for backward compatibility with v1, and we
 * don't plan to change this. Tuples are emitted into a buffer in ascending numeric
 * order with the fingerprint hash being packed into tuple IDs 3-8 and this buffer is
 * then signed.
 */
class MembershipCredential : public Credential {
    friend class Credential;

  public:
    static constexpr ZT_CredentialType credentialType() noexcept
    {
        return ZT_CREDENTIAL_TYPE_COM;
    }

    /**
     * Create an empty certificate of membership
     */
    ZT_INLINE MembershipCredential() noexcept
    {
        memoryZero(this);
    }

    /**
     * Create from required fields common to all networks
     *
     * @param timestamp Timestamp of certificate
     * @param timestampMaxDelta Maximum variation between timestamps on this net
     * @param nwid Network ID
     * @param issuedTo Certificate recipient
     */
    MembershipCredential(int64_t timestamp, int64_t timestampMaxDelta, uint64_t nwid, const Identity& issuedTo) noexcept;

    /**
     * @return True if there's something here
     */
    ZT_INLINE operator bool() const noexcept
    {
        return (m_networkId != 0);
    }

    /**
     * @return Credential ID, always 0 for COMs
     */
    ZT_INLINE uint32_t id() const noexcept
    {
        return 0;
    }

    /**
     * @return Timestamp for this cert and maximum delta for timestamp
     */
    ZT_INLINE int64_t timestamp() const noexcept
    {
        return m_timestamp;
    }

    ZT_INLINE int64_t revision() const noexcept
    {
        return m_timestamp;
    }

    /**
     * @return Maximum allowed difference between timestamps
     */
    ZT_INLINE int64_t timestampMaxDelta() const noexcept
    {
        return m_timestampMaxDelta;
    }

    /**
     * @return Fingerprint of identity to which this cert was issued
     */
    ZT_INLINE const Fingerprint& issuedTo() const noexcept
    {
        return m_issuedTo;
    }

    /**
     * @return Network ID for which this cert was issued
     */
    ZT_INLINE uint64_t networkId() const noexcept
    {
        return m_networkId;
    }

    /**
     * Compare two certificates for parameter agreement
     *
     * This compares this certificate with the other and returns true if all
     * parameters in this cert are present in the other and if they agree to
     * within this cert's max delta value for each given parameter.
     *
     * Tuples present in other but not in this cert are ignored, but any
     * tuples present in this cert but not in other result in 'false'.
     *
     * @param other Cert to compare with
     * @return True if certs agree and 'other' may be communicated with
     */
    bool agreesWith(const MembershipCredential& other) const noexcept;

    /**
     * Sign this certificate
     *
     * @param with Identity to sign with, must include private key
     * @return True if signature was successful
     */
    bool sign(const Identity& with) noexcept;

    /**
     * Verify this COM and its signature
     *
     * @param RR Runtime environment for looking up peers
     * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
     */
    ZT_INLINE Credential::VerifyResult verify(const Context& ctx, const CallContext& cc) const
    {
        return s_verify(ctx, cc, *this);
    }

    static constexpr int marshalSizeMax() noexcept
    {
        return ZT_MEMBERSHIP_CREDENTIAL_MARSHAL_SIZE_MAX;
    }

    int marshal(uint8_t data[ZT_MEMBERSHIP_CREDENTIAL_MARSHAL_SIZE_MAX], bool v2 = false) const noexcept;
    int unmarshal(const uint8_t* data, int len) noexcept;

  private:
    unsigned int m_fillSigningBuf(uint64_t* buf) const noexcept;

    struct p_Qualifier {
        ZT_INLINE p_Qualifier() noexcept
            : id(0)
            , value(0)
            , delta(0)
        {
        }

        ZT_INLINE p_Qualifier(const uint64_t id_, const uint64_t value_, const uint64_t delta_) noexcept
            : id(id_)
            , value(value_)
            , delta(delta_)
        {
        }

        uint64_t id;
        uint64_t value;
        uint64_t delta;
        ZT_INLINE bool operator<(const p_Qualifier& q) const noexcept
        {
            return (id < q.id);
        }   // sort order
    };

    FCV<p_Qualifier, ZT_MEMBERSHIP_CREDENTIAL_MAX_ADDITIONAL_QUALIFIERS> m_additionalQualifiers;
    int64_t m_timestamp;
    int64_t m_timestampMaxDelta;
    uint64_t m_networkId;
    Fingerprint m_issuedTo;
    Address m_signedBy;
    unsigned int m_signatureLength;
    uint8_t m_signature[ZT_SIGNATURE_BUFFER_SIZE];
};

}   // namespace ZeroTier

#endif
