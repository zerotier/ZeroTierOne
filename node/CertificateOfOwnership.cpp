/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "CertificateOfOwnership.hpp"
#include "RuntimeEnvironment.hpp"
#include "Identity.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Network.hpp"
#include "Node.hpp"

namespace ZeroTier {

int CertificateOfOwnership::verify(const RuntimeEnvironment *RR,void *tPtr) const
{
	if ((!_signedBy)||(_signedBy != Network::controllerFor(_networkId)))
		return -1;
	const Identity id(RR->topology->getIdentity(tPtr,_signedBy));
	if (!id) {
		RR->sw->requestWhois(tPtr,RR->node->now(),_signedBy);
		return 1;
	}
	try {
		Buffer<(sizeof(CertificateOfOwnership) + 64)> tmp;
		this->serialize(tmp,true);
		return (id.verify(tmp.data(),tmp.size(),_signature) ? 0 : -1);
	} catch ( ... ) {
		return -1;
	}
}

bool CertificateOfOwnership::_owns(const CertificateOfOwnership::Thing &t,const void *v,unsigned int l) const
{
	for(unsigned int i=0,j=_thingCount;i<j;++i) {
		if (_thingTypes[i] == (uint8_t)t) {
			unsigned int k = 0;
			while (k < l) {
				if (reinterpret_cast<const uint8_t *>(v)[k] != _thingValues[i][k])
					break;
				++k;
			}
			if (k == l)
				return true;
		}
	}
	return false;
}

} // namespace ZeroTier
