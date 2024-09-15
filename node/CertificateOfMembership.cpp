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

#include "CertificateOfMembership.hpp"
#include "ECC.hpp"
#include "RuntimeEnvironment.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Network.hpp"
#include "Node.hpp"

namespace ZeroTier {

CertificateOfMembership::CertificateOfMembership(uint64_t timestamp,uint64_t timestampMaxDelta,uint64_t nwid,const Identity &issuedTo)
{
	_qualifiers[0].id = COM_RESERVED_ID_TIMESTAMP;
	_qualifiers[0].value = timestamp;
	_qualifiers[0].maxDelta = timestampMaxDelta;
	_qualifiers[1].id = COM_RESERVED_ID_NETWORK_ID;
	_qualifiers[1].value = nwid;
	_qualifiers[1].maxDelta = 0;
	_qualifiers[2].id = COM_RESERVED_ID_ISSUED_TO;
	_qualifiers[2].value = issuedTo.address().toInt();
	_qualifiers[2].maxDelta = 0xffffffffffffffffULL;

	// Include hash of full identity public key in COM for hardening purposes. Pack it in
	// using the original COM format. Format may be revised in the future to make this cleaner.
	uint64_t idHash[6];
	issuedTo.publicKeyHash(idHash);
	for(unsigned long i=0;i<4;++i) {
		_qualifiers[i + 3].id = (uint64_t)(i + 3);
		_qualifiers[i + 3].value = Utils::ntoh(idHash[i]);
		_qualifiers[i + 3].maxDelta = 0xffffffffffffffffULL;
	}

	_qualifierCount = 7;
	memset(_signature.data,0,ZT_ECC_SIGNATURE_LEN);
}

bool CertificateOfMembership::agreesWith(const CertificateOfMembership &other, const Identity &otherIdentity) const
{
	if ((_qualifierCount == 0)||(other._qualifierCount == 0)) {
		return false;
	}

	std::map< uint64_t, uint64_t > otherFields;
	for(unsigned int i=0;i<other._qualifierCount;++i) {
		otherFields[other._qualifiers[i].id] = other._qualifiers[i].value;
	}

	bool fullIdentityVerification = false;
	for(unsigned int i=0;i<_qualifierCount;++i) {
		const uint64_t qid = _qualifiers[i].id;
		if ((qid >= 3)&&(qid <= 6)) {
			fullIdentityVerification = true;
		}
		std::map< uint64_t, uint64_t >::iterator otherQ(otherFields.find(qid));
		if (otherQ == otherFields.end()) {
			return false;
		}
		const uint64_t a = _qualifiers[i].value;
		const uint64_t b = otherQ->second;
		if (((a >= b) ? (a - b) : (b - a)) > _qualifiers[i].maxDelta) {
			return false;
		}
	}

	// If this COM has a full hash of its identity, assume the other must have this as well.
	// Otherwise we are on a controller that does not incorporate these.
	if (fullIdentityVerification) {
		uint64_t idHash[6];
		otherIdentity.publicKeyHash(idHash);
		for(unsigned long i=0;i<4;++i) {
			std::map< uint64_t, uint64_t >::iterator otherQ(otherFields.find((uint64_t)(i + 3)));
			if (otherQ == otherFields.end()) {
				return false;
			}
			if (otherQ->second != Utils::ntoh(idHash[i])) {
				return false;
			}
		}
	}

	return true;
}

bool CertificateOfMembership::sign(const Identity &with)
{
	uint64_t buf[ZT_NETWORK_COM_MAX_QUALIFIERS * 3];
	unsigned int ptr = 0;
	for(unsigned int i=0;i<_qualifierCount;++i) {
		buf[ptr++] = Utils::hton(_qualifiers[i].id);
		buf[ptr++] = Utils::hton(_qualifiers[i].value);
		buf[ptr++] = Utils::hton(_qualifiers[i].maxDelta);
	}

	try {
		_signature = with.sign(buf,ptr * sizeof(uint64_t));
		_signedBy = with.address();
		return true;
	} catch ( ... ) {
		_signedBy.zero();
		return false;
	}
}

int CertificateOfMembership::verify(const RuntimeEnvironment *RR,void *tPtr) const
{
	if ((!_signedBy)||(_signedBy != Network::controllerFor(networkId()))||(_qualifierCount > ZT_NETWORK_COM_MAX_QUALIFIERS)) {
		return -1;
	}

	const Identity id(RR->topology->getIdentity(tPtr,_signedBy));
	if (!id) {
		RR->sw->requestWhois(tPtr,RR->node->now(),_signedBy);
		return 1;
	}

	uint64_t buf[ZT_NETWORK_COM_MAX_QUALIFIERS * 3];
	unsigned int ptr = 0;
	for(unsigned int i=0;i<_qualifierCount;++i) {
		buf[ptr++] = Utils::hton(_qualifiers[i].id);
		buf[ptr++] = Utils::hton(_qualifiers[i].value);
		buf[ptr++] = Utils::hton(_qualifiers[i].maxDelta);
	}
	return (id.verify(buf,ptr * sizeof(uint64_t),_signature) ? 0 : -1);
}

} // namespace ZeroTier
