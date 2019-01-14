/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

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
