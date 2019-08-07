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

#ifndef ZT_LOCATOR_HPP
#define ZT_LOCATOR_HPP

#include "Constants.hpp"
#include "Identity.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <vector>

namespace ZeroTier {

/**
 * Signed information about a node's location on the network
 * 
 * A locator can be stored in DNS as a series of TXT records with a DNS name
 * that includes a public key that can be used to validate the locator's
 * signature. That way DNS records can't be spoofed even if no DNSSEC or
 * anything else is present to secure DNS.
 */
class Locator
{
public:
	Locator() :
		_signatureLength(0) {}

	inline const std::vector<InetAddress> &phy() const { return _physical; }
	inline const std::vector<Identity> &virt() const { return _virtual; }

	inline bool sign(const Identity &signingId)
	{
		std::sort(_physical.begin(),_physical.end());
		std::sort(_virtual.begin(),_virtual.end());
		_id = signingId;
	}

private:
	int64_t _ts;
	Identity _id;
	std::vector<InetAddress> _physical;
	std::vector<Identity> _virtual;
	unsigned int _signatureLength;
	uint8_t _signature[ZT_SIGNATURE_BUFFER_SIZE];
};

} // namespace ZeroTier

#endif
