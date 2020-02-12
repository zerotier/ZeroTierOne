/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "Credential.hpp"
#include "Capability.hpp"
#include "Tag.hpp"
#include "CertificateOfMembership.hpp"
#include "CertificateOfOwnership.hpp"
#include "Revocation.hpp"
#include "Network.hpp"
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
#if ZT_CERTIFICATEOFMEMBERSHIP_MARSHAL_SIZE_MAX > ZT_BUF_MEM_SIZE
#error ZT_CERTIFICATEOFMEMBERSHIP_MARSHAL_SIZE_MAX exceeds maximum buffer size
#endif

namespace ZeroTier {

template<typename CRED>
static ZT_ALWAYS_INLINE Credential::VerifyResult _credVerify(const RuntimeEnvironment *RR,void *tPtr,CRED credential)
{
	uint8_t tmp[ZT_BUF_MEM_SIZE + 16];

	const Address signedBy(credential.signer());
	const uint64_t networkId = credential.networkId();
	if ((!signedBy)||(signedBy != Network::controllerFor(networkId)))
		return Credential::VERIFY_BAD_SIGNATURE;

	const SharedPtr<Peer> peer(RR->topology->peer(tPtr,signedBy));
	if (!peer) {
		RR->sw->requestWhois(tPtr,RR->node->now(),signedBy);
		return Credential::VERIFY_NEED_IDENTITY;
	}

	try {
		int l = credential.marshal(tmp,true);
		if (l <= 0)
			return Credential::VERIFY_BAD_SIGNATURE;
		return (peer->identity().verify(tmp,(unsigned int)l,credential.signature(),credential.signatureLength()) ? Credential::VERIFY_OK : Credential::VERIFY_BAD_SIGNATURE);
	} catch ( ... ) {}

	return Credential::VERIFY_BAD_SIGNATURE;
}

Credential::VerifyResult Credential::_verify(const RuntimeEnvironment *const RR,void *tPtr,const Revocation &credential) const { return _credVerify(RR,tPtr,credential); }
Credential::VerifyResult Credential::_verify(const RuntimeEnvironment *const RR,void *tPtr,const Tag &credential) const { return _credVerify(RR,tPtr,credential); }
Credential::VerifyResult Credential::_verify(const RuntimeEnvironment *const RR,void *tPtr,const CertificateOfOwnership &credential) const { return _credVerify(RR,tPtr,credential); }

Credential::VerifyResult Credential::_verify(const RuntimeEnvironment *const RR,void *tPtr,const CertificateOfMembership &credential) const
{
	if ((!credential._signedBy)||(credential._signedBy != Network::controllerFor(credential.networkId()))||(credential._qualifierCount > ZT_NETWORK_COM_MAX_QUALIFIERS))
		return Credential::VERIFY_BAD_SIGNATURE;

	const SharedPtr<Peer> peer(RR->topology->peer(tPtr,credential._signedBy));
	if (!peer) {
		RR->sw->requestWhois(tPtr,RR->node->now(),credential._signedBy);
		return Credential::VERIFY_NEED_IDENTITY;
	}

	uint64_t buf[ZT_NETWORK_COM_MAX_QUALIFIERS * 3];
	unsigned int ptr = 0;
	for(unsigned int i=0;i<credential._qualifierCount;++i) {
		buf[ptr++] = Utils::hton(credential._qualifiers[i].id);
		buf[ptr++] = Utils::hton(credential._qualifiers[i].value);
		buf[ptr++] = Utils::hton(credential._qualifiers[i].maxDelta);
	}

	return (peer->identity().verify(buf,ptr * sizeof(uint64_t),credential._signature,credential._signatureLength) ? Credential::VERIFY_OK : Credential::VERIFY_BAD_SIGNATURE);
}

Credential::VerifyResult Credential::_verify(const RuntimeEnvironment *RR,void *tPtr,const Capability &credential) const
{
	uint8_t tmp[ZT_CAPABILITY_MARSHAL_SIZE_MAX + 16];
	try {
		// There must be at least one entry, and sanity check for bad chain max length
		if ((credential._maxCustodyChainLength < 1)||(credential._maxCustodyChainLength > ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH))
			return Credential::VERIFY_BAD_SIGNATURE;

		int l = credential.marshal(tmp,true);
		if (l <= 0)
			return Credential::VERIFY_BAD_SIGNATURE;

		// Validate all entries in chain of custody
		for(unsigned int c=0;c<credential._maxCustodyChainLength;++c) {
			if (c == 0) {
				if ((!credential._custody[c].to)||(!credential._custody[c].from)||(credential._custody[c].from != Network::controllerFor(credential._nwid)))
					return Credential::VERIFY_BAD_SIGNATURE; // the first entry must be present and from the network's controller
			} else {
				if (!credential._custody[c].to)
					return Credential::VERIFY_OK; // all previous entries were valid, so we are valid
				else if ((!credential._custody[c].from)||(credential._custody[c].from != credential._custody[c-1].to))
					return Credential::VERIFY_BAD_SIGNATURE; // otherwise if we have another entry it must be from the previous holder in the chain
			}

			const SharedPtr<Peer> peer(RR->topology->peer(tPtr,credential._custody[c].from));
			if (peer) {
				if (!peer->identity().verify(tmp,(unsigned int)l,credential._custody[c].signature,credential._custody[c].signatureLength))
					return Credential::VERIFY_BAD_SIGNATURE;
			} else {
				RR->sw->requestWhois(tPtr,RR->node->now(),credential._custody[c].from);
				return Credential::VERIFY_NEED_IDENTITY;
			}
		}

		// We reached max custody chain length and everything was valid
		return Credential::VERIFY_OK;
	} catch ( ... ) {}
	return Credential::VERIFY_BAD_SIGNATURE;
}

} // namespace ZeroTier
