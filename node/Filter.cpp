/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdint.h>

#include "RuntimeEnvironment.hpp"
#include "Logger.hpp"
#include "Filter.hpp"
#include "Utils.hpp"

namespace ZeroTier {

bool Filter::Rule::operator()(unsigned int etype,const void *data,unsigned int len) const
{
	if ((!_etherType)||(_etherType(etype))) { // ethertype is ANY, or matches
		// Ethertype determines meaning of protocol and port
		switch(etype) {
			case ZT_ETHERTYPE_IPV4:
				if (len > 20) {
					if ((!_protocol)||(_protocol(((const uint8_t *)data)[9]))) { // IP protocol
						if (!_port)
							return true; // protocol matches, port is ANY

						// Don't match on fragments beyond fragment 0. If we've blocked
						// fragment 0, further fragments will fall on deaf ears anyway.
						if ((Utils::ntoh(((const uint16_t *)data)[3]) & 0x1fff))
							return false;

						// Internet header length determines where data begins, in multiples of 32 bits
						unsigned int ihl = 4 * (((const uint8_t *)data)[0] & 0x0f);

						switch(((const uint8_t *)data)[9]) { // port's meaning depends on IP protocol
							case ZT_IPPROTO_ICMP:
								return _port(((const uint8_t *)data)[ihl]); // port = ICMP type
							case ZT_IPPROTO_TCP:
							case ZT_IPPROTO_UDP:
							case ZT_IPPROTO_SCTP:
							case ZT_IPPROTO_UDPLITE:
								return _port(((const uint16_t *)data)[(ihl / 2) + 1]); // destination port
						}

						return false; // no match on port
					}
				}
				break;

			case ZT_ETHERTYPE_IPV6:
				if (len > 40) {
					// see: http://stackoverflow.com/questions/17518951/is-the-ipv6-header-really-this-nutty
				}
				break;
		}
	}

	return false;
}

Filter::Filter(const RuntimeEnvironment *renv) :
	_r(renv)
{
}

Filter::~Filter()
{
}

void Filter::add(const Rule &r,const Action &a)
{
	Mutex::Lock _l(_chain_m);
	for(std::vector<Entry>::iterator i(_chain.begin());i!=_chain.end();++i) {
		if (i->rule == r) {
			_chain.erase(i);
			break;
		}
	}
	_chain.push_back(Entry(r,a));
}

const char *Filter::etherTypeName(const unsigned int etherType)
	throw()
{
	static char tmp[6];
	switch(etherType) {
		case ZT_ETHERTYPE_IPV4:
			return "IPV4";
		case ZT_ETHERTYPE_ARP:
			return "ARP";
		case ZT_ETHERTYPE_RARP:
			return "RARP";
		case ZT_ETHERTYPE_ATALK:
			return "ATALK";
		case ZT_ETHERTYPE_AARP:
			return "AARP";
		case ZT_ETHERTYPE_IPX_A:
			return "IPX_A";
		case ZT_ETHERTYPE_IPX_B:
			return "IPX_B";
		case ZT_ETHERTYPE_IPV6:
			return "IPV6";
	}
	sprintf(tmp,"%.4x",etherType);
	return tmp; // technically not thread safe, but we're only going to see this in debugging if ever
}

} // namespace ZeroTier
