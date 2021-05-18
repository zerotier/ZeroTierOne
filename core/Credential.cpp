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

#include "Credential.hpp"

#include "CapabilityCredential.hpp"
#include "Constants.hpp"
#include "Context.hpp"
#include "MembershipCredential.hpp"
#include "Network.hpp"
#include "OwnershipCredential.hpp"
#include "RevocationCredential.hpp"
#include "TagCredential.hpp"
#include "Topology.hpp"

// These are compile-time asserts to make sure temporary marshal buffers here and
// also in NtworkConfig.cpp are always large enough to marshal all credential types.
#if ZT_TAG_MARSHAL_SIZE_MAX > ZT_BUF_MEM_SIZE
#error ZT_TAG_MARSHAL_SIZE_MAX exceeds maximum buffer size
#endif
#if ZT_CAPABILITY_MARSHAL_SIZE_MAX > ZT_BUF_MEM_SIZE
#error ZT_CAPABILITY_MARSHAL_SIZE_MAX exceeds maximum buffer size
#endif
#if ZT_REVOCATION_MARSHAL_SIZE_MAX > ZT_BUF_MEM_SIZE
#error ZT_REVOCATION_MARSHAL_SIZE_MAX exceeds maximum buffer size
#endif
#if ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX > ZT_BUF_MEM_SIZE
#error ZT_CERTIFICATEOFOWNERSHIP_MARSHAL_SIZE_MAX exceeds maximum buffer size
#endif
#if ZT_MEMBERSHIP_CREDENTIAL_MARSHAL_SIZE_MAX > ZT_BUF_MEM_SIZE
#error ZT_MEMBERSHIP_CREDENTIAL_MARSHAL_SIZE_MAX exceeds maximum buffer size
#endif

namespace ZeroTier {

template <typename CRED>
static ZT_INLINE Credential::VerifyResult p_credVerify(const Context &ctx, const CallContext &cc, CRED credential)
{
    uint8_t tmp[ZT_BUF_MEM_SIZE + 16];

    const Address signedBy(credential.signer());
    const uint64_t networkId = credential.networkId();
    if ((!signedBy) || (signedBy != Network::controllerFor(networkId)))
        return Credential::VERIFY_BAD_SIGNATURE;

    const SharedPtr<Peer> peer(ctx.topology->peer(cc, signedBy));
    if (!peer)
        return Credential::VERIFY_NEED_IDENTITY;

    try {
        int l = credential.marshal(tmp, true);
        if (l <= 0)
            return Credential::VERIFY_BAD_SIGNATURE;
        return (
            peer->identity().verify(tmp, (unsigned int)l, credential.signature(), credential.signatureLength())
                ? Credential::VERIFY_OK
                : Credential::VERIFY_BAD_SIGNATURE);
    }
    catch (...) {
    }

    return Credential::VERIFY_BAD_SIGNATURE;
}

Credential::VerifyResult
Credential::s_verify(const Context &ctx, const CallContext &cc, const RevocationCredential &credential)
{
    return p_credVerify(ctx, cc, credential);
}

Credential::VerifyResult
Credential::s_verify(const Context &ctx, const CallContext &cc, const TagCredential &credential)
{
    return p_credVerify(ctx, cc, credential);
}

Credential::VerifyResult
Credential::s_verify(const Context &ctx, const CallContext &cc, const CapabilityCredential &credential)
{
    return p_credVerify(ctx, cc, credential);
}

Credential::VerifyResult
Credential::s_verify(const Context &ctx, const CallContext &cc, const OwnershipCredential &credential)
{
    return p_credVerify(ctx, cc, credential);
}

Credential::VerifyResult
Credential::s_verify(const Context &ctx, const CallContext &cc, const MembershipCredential &credential)
{
    // Sanity check network ID.
    if ((!credential.m_signedBy) || (credential.m_signedBy != Network::controllerFor(credential.m_networkId)))
        return Credential::VERIFY_BAD_SIGNATURE;

    // If we don't know the peer, get its identity. This shouldn't happen here but should be handled.
    const SharedPtr<Peer> peer(ctx.topology->peer(cc, credential.m_signedBy));
    if (!peer)
        return Credential::VERIFY_NEED_IDENTITY;

    // Now verify the controller's signature.
    uint64_t buf[ZT_MEMBERSHIP_CREDENTIAL_MARSHAL_SIZE_MAX / 8];
    const unsigned int bufSize = credential.m_fillSigningBuf(buf);
    return peer->identity().verify(buf, bufSize, credential.m_signature, credential.m_signatureLength)
               ? Credential::VERIFY_OK
               : Credential::VERIFY_BAD_SIGNATURE;
}

}   // namespace ZeroTier
