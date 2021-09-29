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

#include "Capability.hpp"
#include "RuntimeEnvironment.hpp"
#include "Identity.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Network.hpp"
#include "Node.hpp"

namespace ZeroTier {

int Capability::verify(const RuntimeEnvironment *RR,void *tPtr) const
{
	try {
		// There must be at least one entry, and sanity check for bad chain max length
		if ((_maxCustodyChainLength < 1)||(_maxCustodyChainLength > ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH))
			return -1;

		// Validate all entries in chain of custody
		Buffer<(sizeof(Capability) * 2)> tmp;
		this->serialize(tmp,true);
		for(unsigned int c=0;c<_maxCustodyChainLength;++c) {
			if (c == 0) {
				if ((!_custody[c].to)||(!_custody[c].from)||(_custody[c].from != Network::controllerFor(_nwid)))
					return -1; // the first entry must be present and from the network's controller
			} else {
				if (!_custody[c].to)
					return 0; // all previous entries were valid, so we are valid
				else if ((!_custody[c].from)||(_custody[c].from != _custody[c-1].to))
					return -1; // otherwise if we have another entry it must be from the previous holder in the chain
			}

			const Identity id(RR->topology->getIdentity(tPtr,_custody[c].from));
			if (id) {
				if (!id.verify(tmp.data(),tmp.size(),_custody[c].signature))
					return -1;
			} else {
				RR->sw->requestWhois(tPtr,RR->node->now(),_custody[c].from);
				return 1;
			}
		}

		// We reached max custody chain length and everything was valid
		return 0;
	} catch ( ... ) {}
	return -1;
}

} // namespace ZeroTier
