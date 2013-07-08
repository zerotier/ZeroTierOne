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
			default:
				if ((!_protocol)&&(!_port))
					return true; // match other ethertypes if protocol and port are ANY, since we don't know what to do with them
				break;

			case ZT_ETHERTYPE_IPV4:
				if (len > 20) {
					if ((!_protocol)||(_protocol(((const uint8_t *)data)[9]))) { // IP protocol
						if (!_port)
							return true; // protocol matches or is ANY, port is ANY

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
					int nextHeader = ((const uint8_t *)data)[6];
					unsigned int pos = 40;
					while ((pos < len)&&(nextHeader >= 0)&&(nextHeader != 59)) { // 59 == no next header
						fprintf(stderr,"[rule] V6: start header parse, header %.2x pos %d\n",nextHeader,pos);

						switch(nextHeader) {
							case 0: // hop-by-hop options
							case 60: // destination options
							case 43: // routing
							case 135: // mobility (mobile IPv6 options)
								if (_protocol((unsigned int)nextHeader))
									return true; // match if our goal was to match any of these
								nextHeader = ((const uint8_t *)data)[pos];
								pos += 8 + (8 * ((const uint8_t *)data)[pos + 1]);
								break;
							case 44: // fragment
								if (_protocol(44))
									return true; // match if our goal was to match fragments
								nextHeader = ((const uint8_t *)data)[pos];
								pos += 8;
								break;
							case ZT_IPPROTO_AH: // AH
								return _protocol(ZT_IPPROTO_AH); // true if AH is matched protocol, otherwise false since packet will be IPsec
							case ZT_IPPROTO_ESP: // ESP
								return _protocol(ZT_IPPROTO_ESP); // true if ESP is matched protocol, otherwise false since packet will be IPsec
							case ZT_IPPROTO_ICMPV6:
								if (_protocol(ZT_IPPROTO_ICMPV6)) { // only match ICMPv6 if specified
									if ((!_port)||(_port(((const uint8_t *)data)[pos])))
										return true; // protocol matches, port is ANY or matches ICMP type
								}
								break;
							case ZT_IPPROTO_TCP:
							case ZT_IPPROTO_UDP:
							case ZT_IPPROTO_SCTP:
							case ZT_IPPROTO_UDPLITE:
								// If we encounter any of these, match if protocol matches or is wildcard as
								// we'll consider these the "real payload" if present.
								if ((!_protocol)||(_protocol(nextHeader))) {
									if ((!_port)||(_port(((const uint16_t *)data)[(pos / 2) + 1])))
										return true; // protocol matches or is ANY, port is ANY or matches
								}
								break;
						}

						fprintf(stderr,"[rule] V6: end header parse, next header %.2x, new pos %d\n",nextHeader,pos);
					}
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

std::string Filter::toString(const char *sep) const
{
	char buf[256];

	if (!sep)
		sep = ",";

	std::string s;

	Mutex::Lock _l(_chain_m);
	for(std::vector<Entry>::const_iterator i(_chain.begin());i!=_chain.end();++i) {
		bool first = (i == _chain.begin());

		s.push_back('[');

		if (i->rule.etherType()) {
			sprintf(buf,"%u-%u",i->rule.etherType().start,i->rule.etherType().end);
			s.append(buf);
		} else s.push_back('*');

		s.push_back(';');

		if (i->rule.protocol()) {
			sprintf(buf,"%u-%u",i->rule.protocol().start,i->rule.protocol().end);
			s.append(buf);
		} else s.push_back('*');

		s.push_back(';');

		if (i->rule.port()) {
			sprintf(buf,"%u-%u",i->rule.port().start,i->rule.port().end);
			s.append(buf);
		} else s.push_back('*');

		s.append("]:");

		switch(i->action) {
			case ACTION_DENY:
				s.append("DENY");
				break;
			case ACTION_ALLOW:
				s.append("ALLOW");
				break;
			case ACTION_LOG:
				s.append("LOG");
				break;
		}

		if (!first)
			s.append(sep);
	}

	return s;
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
