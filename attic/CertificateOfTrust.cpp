/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "CertificateOfTrust.hpp"

#include "RuntimeEnvironment.hpp"
#include "Topology.hpp"
#include "Switch.hpp"

namespace ZeroTier {

bool CertificateOfTrust::create(uint64_t ts,uint64_t rls,const Identity &iss,const Identity &tgt,Level l)
{
	if ((!iss)||(!iss.hasPrivate()))
		return false;

	_timestamp = ts;
	_roles = rls;
	_issuer = iss.address();
	_target = tgt;
	_level = l;

	Buffer<sizeof(Identity) + 64> tmp;
	tmp.append(_timestamp);
	tmp.append(_roles);
	_issuer.appendTo(tmp);
	_target.serialize(tmp,false);
	tmp.append((uint16_t)_level);
	_signature = iss.sign(tmp.data(),tmp.size());

	return true;
}

int CertificateOfTrust::verify(const RuntimeEnvironment *RR) const
{
	const Identity id(RR->topology->getIdentity(_issuer));
	if (!id) {
		RR->sw->requestWhois(_issuer);
		return 1;
	}

	Buffer<sizeof(Identity) + 64> tmp;
	tmp.append(_timestamp);
	tmp.append(_roles);
	_issuer.appendTo(tmp);
	_target.serialize(tmp,false);
	tmp.append((uint16_t)_level);

	return (id.verify(tmp.data(),tmp.size(),_signature) ? 0 : -1);
}

} // namespace ZeroTier
