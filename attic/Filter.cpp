/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <algorithm>

#include "RuntimeEnvironment.hpp"
#include "Logger.hpp"
#include "Filter.hpp"
#include "Utils.hpp"

namespace ZeroTier {

const char *const Filter::UNKNOWN_NAME = "(unknown)";
const Range<unsigned int> Filter::ANY;

static inline Range<unsigned int> __parseRange(char *r)
	throw(std::invalid_argument)
{
	char *saveptr = (char *)0;
	unsigned int a = 0;
	unsigned int b = 0;
	unsigned int fn = 0;
	for(char *f=Utils::stok(r,"-",&saveptr);(f);f=Utils::stok((char *)0,"-",&saveptr)) {
		if (*f) {
			switch(fn++) {
				case 0:
					if (*f != '*')
						a = b = (unsigned int)strtoul(f,(char **)0,10);
					break;
				case 1:
					if (*f != '*')
						b = (unsigned int)strtoul(f,(char **)0,10);
					break;
				default:
					throw std::invalid_argument("rule range must be <int>, <int>-<int>, or *");
			}
		}
	}
	return Range<unsigned int>(a,b);
}

Filter::Rule::Rule(const char *s)
	throw(std::invalid_argument)
{
	char *saveptr = (char *)0;
	char tmp[256];
	if (!Utils::scopy(tmp,sizeof(tmp),s))
		throw std::invalid_argument("rule string too long");
	unsigned int fn = 0;
	for(char *f=Utils::stok(tmp,";",&saveptr);(f);f=Utils::stok((char *)0,";",&saveptr)) {
		if (*f) {
			switch(fn++) {
				case 0:
					_etherType = __parseRange(f);
					break;
				case 1:
					_protocol = __parseRange(f);
					break;
				case 2:
					_port = __parseRange(f);
					break;
				default:
					throw std::invalid_argument("rule string has unknown extra fields");
			}
		}
	}
	if (fn != 3)
		throw std::invalid_argument("rule string must contain 3 fields");
}

bool Filter::Rule::operator()(unsigned int etype,const void *data,unsigned int len) const
	throw(std::invalid_argument)
{
	if ((!_etherType)||(_etherType(etype))) { // ethertype is ANY, or matches
		// Ethertype determines meaning of protocol and port
		switch(etype) {
			case ZT_ETHERTYPE_IPV4:
				if (len > 20) {
					if ((!_protocol)||(_protocol(((const uint8_t *)data)[9]))) { // protocol is ANY or match
						if (!_port) // port is ANY
							return true;

						// Don't match on fragments beyond fragment 0. If we've blocked
						// fragment 0, further fragments will fall on deaf ears anyway.
						if ((Utils::ntoh(((const uint16_t *)data)[3]) & 0x1fff))
							return false;

						// Internet header length determines where data begins, in multiples of 32 bits
						unsigned int ihl = 4 * (((const uint8_t *)data)[0] & 0x0f);

						switch(((const uint8_t *)data)[9]) { // port's meaning depends on IP protocol
							case ZT_IPPROTO_ICMP:
								// For ICMP, port is ICMP type
								return _port(((const uint8_t *)data)[ihl]);
							case ZT_IPPROTO_TCP:
							case ZT_IPPROTO_UDP:
							case ZT_IPPROTO_SCTP:
							case ZT_IPPROTO_UDPLITE:
								// For these, port is destination port. Protocol designers were
								// nice enough to put the field in the same place.
								return _port(((const uint16_t *)data)[(ihl / 2) + 1]);
							default:
								// port has no meaning for other IP types, so ignore it
								return true;
						}

						return false; // no match on port
					}
				} else throw std::invalid_argument("undersized IPv4 packet");
				break;

			case ZT_ETHERTYPE_IPV6:
				if (len > 40) {
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
								// Only match ICMPv6 if we've selected it specifically
								if (_protocol(ZT_IPPROTO_ICMPV6)) {
									// Port is interpreted as ICMPv6 type
									if ((!_port)||(_port(((const uint8_t *)data)[pos])))
										return true;
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
							default: {
								char foo[128];
								Utils::snprintf(foo,sizeof(foo),"unrecognized IPv6 header type %d",(int)nextHeader);
								throw std::invalid_argument(foo);
							}
						}

						fprintf(stderr,"[rule] V6: end header parse, next header %.2x, new pos %d\n",nextHeader,pos);
					}
				} else throw std::invalid_argument("undersized IPv6 packet");
				break;

			default:
				// For other ethertypes, protocol and port are ignored. What would they mean?
				return true;
		}
	}

	return false;
}

std::string Filter::Rule::toString() const
{
	char buf[128];
	std::string s;

	switch(_etherType.magnitude()) {
		case 0:
			s.push_back('*');
			break;
		case 1:
			Utils::snprintf(buf,sizeof(buf),"%u",_etherType.start);
			s.append(buf);
			break;
		default:
			Utils::snprintf(buf,sizeof(buf),"%u-%u",_etherType.start,_etherType.end);
			s.append(buf);
			break;
	}
	s.push_back(';');
	switch(_protocol.magnitude()) {
		case 0:
			s.push_back('*');
			break;
		case 1:
			Utils::snprintf(buf,sizeof(buf),"%u",_protocol.start);
			s.append(buf);
			break;
		default:
			Utils::snprintf(buf,sizeof(buf),"%u-%u",_protocol.start,_protocol.end);
			s.append(buf);
			break;
	}
	s.push_back(';');
	switch(_port.magnitude()) {
		case 0:
			s.push_back('*');
			break;
		case 1:
			Utils::snprintf(buf,sizeof(buf),"%u",_port.start);
			s.append(buf);
			break;
		default:
			Utils::snprintf(buf,sizeof(buf),"%u-%u",_port.start,_port.end);
			s.append(buf);
			break;
	}

	return s;
}

Filter::Filter(const char *s)
	throw(std::invalid_argument)
{
	char tmp[16384];
	if (!Utils::scopy(tmp,sizeof(tmp),s))
		throw std::invalid_argument("filter string too long");
	char *saveptr = (char *)0;
	unsigned int fn = 0;
	for(char *f=Utils::stok(tmp,",",&saveptr);(f);f=Utils::stok((char *)0,",",&saveptr)) {
		try {
			_rules.push_back(Rule(f));
			++fn;
		} catch (std::invalid_argument &exc) {
			char tmp[256];
			Utils::snprintf(tmp,sizeof(tmp),"invalid rule at index %u: %s",fn,exc.what());
			throw std::invalid_argument(tmp);
		}
	}
	std::sort(_rules.begin(),_rules.end());
}

std::string Filter::toString() const
{
	std::string s;

	for(std::vector<Rule>::const_iterator r(_rules.begin());r!=_rules.end();++r) {
		if (s.length() > 0)
			s.push_back(',');
		s.append(r->toString());
	}

	return s;
}

void Filter::add(const Rule &r)
{
	for(std::vector<Rule>::iterator rr(_rules.begin());rr!=_rules.end();++rr) {
		if (r == *rr)
			return;
	}
	_rules.push_back(r);
	std::sort(_rules.begin(),_rules.end());
}

const char *Filter::etherTypeName(const unsigned int etherType)
	throw()
{
	switch(etherType) {
		case ZT_ETHERTYPE_IPV4:  return "ETHERTYPE_IPV4";
		case ZT_ETHERTYPE_ARP:   return "ETHERTYPE_ARP";
		case ZT_ETHERTYPE_RARP:  return "ETHERTYPE_RARP";
		case ZT_ETHERTYPE_ATALK: return "ETHERTYPE_ATALK";
		case ZT_ETHERTYPE_AARP:  return "ETHERTYPE_AARP";
		case ZT_ETHERTYPE_IPX_A: return "ETHERTYPE_IPX_A";
		case ZT_ETHERTYPE_IPX_B: return "ETHERTYPE_IPX_B";
		case ZT_ETHERTYPE_IPV6:  return "ETHERTYPE_IPV6";
	}
	return UNKNOWN_NAME;
}

const char *Filter::ipProtocolName(const unsigned int ipp)
	throw()
{
	switch(ipp) {
		case ZT_IPPROTO_ICMP:    return "IPPROTO_ICMP";
		case ZT_IPPROTO_IGMP:    return "IPPROTO_IGMP";
		case ZT_IPPROTO_TCP:     return "IPPROTO_TCP";
		case ZT_IPPROTO_UDP:     return "IPPROTO_UDP";
		case ZT_IPPROTO_GRE:     return "IPPROTO_GRE";
		case ZT_IPPROTO_ESP:     return "IPPROTO_ESP";
		case ZT_IPPROTO_AH:      return "IPPROTO_AH";
		case ZT_IPPROTO_ICMPV6:  return "IPPROTO_ICMPV6";
		case ZT_IPPROTO_OSPF:    return "IPPROTO_OSPF";
		case ZT_IPPROTO_IPIP:    return "IPPROTO_IPIP";
		case ZT_IPPROTO_IPCOMP:  return "IPPROTO_IPCOMP";
		case ZT_IPPROTO_L2TP:    return "IPPROTO_L2TP";
		case ZT_IPPROTO_SCTP:    return "IPPROTO_SCTP";
		case ZT_IPPROTO_FC:      return "IPPROTO_FC";
		case ZT_IPPROTO_UDPLITE: return "IPPROTO_UDPLITE";
		case ZT_IPPROTO_HIP:     return "IPPROTO_HIP";
	}
	return UNKNOWN_NAME;
}

const char *Filter::icmpTypeName(const unsigned int icmpType)
	throw()
{
	switch(icmpType) {
		case ZT_ICMP_ECHO_REPLY:                  return "ICMP_ECHO_REPLY";
		case ZT_ICMP_DESTINATION_UNREACHABLE:     return "ICMP_DESTINATION_UNREACHABLE";
		case ZT_ICMP_SOURCE_QUENCH:               return "ICMP_SOURCE_QUENCH";
		case ZT_ICMP_REDIRECT:                    return "ICMP_REDIRECT";
		case ZT_ICMP_ALTERNATE_HOST_ADDRESS:      return "ICMP_ALTERNATE_HOST_ADDRESS";
		case ZT_ICMP_ECHO_REQUEST:                return "ICMP_ECHO_REQUEST";
		case ZT_ICMP_ROUTER_ADVERTISEMENT:        return "ICMP_ROUTER_ADVERTISEMENT";
		case ZT_ICMP_ROUTER_SOLICITATION:         return "ICMP_ROUTER_SOLICITATION";
		case ZT_ICMP_TIME_EXCEEDED:               return "ICMP_TIME_EXCEEDED";
		case ZT_ICMP_BAD_IP_HEADER:               return "ICMP_BAD_IP_HEADER";
		case ZT_ICMP_TIMESTAMP:                   return "ICMP_TIMESTAMP";
		case ZT_ICMP_TIMESTAMP_REPLY:             return "ICMP_TIMESTAMP_REPLY";
		case ZT_ICMP_INFORMATION_REQUEST:         return "ICMP_INFORMATION_REQUEST";
		case ZT_ICMP_INFORMATION_REPLY:           return "ICMP_INFORMATION_REPLY";
		case ZT_ICMP_ADDRESS_MASK_REQUEST:        return "ICMP_ADDRESS_MASK_REQUEST";
		case ZT_ICMP_ADDRESS_MASK_REPLY:          return "ICMP_ADDRESS_MASK_REPLY";
		case ZT_ICMP_TRACEROUTE:                  return "ICMP_TRACEROUTE";
		case ZT_ICMP_MOBILE_HOST_REDIRECT:        return "ICMP_MOBILE_HOST_REDIRECT";
		case ZT_ICMP_MOBILE_REGISTRATION_REQUEST: return "ICMP_MOBILE_REGISTRATION_REQUEST";
		case ZT_ICMP_MOBILE_REGISTRATION_REPLY:   return "ICMP_MOBILE_REGISTRATION_REPLY";
	}
	return UNKNOWN_NAME;
}

const char *Filter::icmp6TypeName(const unsigned int icmp6Type)
	throw()
{
	switch(icmp6Type) {
		case ZT_ICMP6_DESTINATION_UNREACHABLE:              return "ICMP6_DESTINATION_UNREACHABLE";
		case ZT_ICMP6_PACKET_TOO_BIG:                       return "ICMP6_PACKET_TOO_BIG";
		case ZT_ICMP6_TIME_EXCEEDED:                        return "ICMP6_TIME_EXCEEDED";
		case ZT_ICMP6_PARAMETER_PROBLEM:                    return "ICMP6_PARAMETER_PROBLEM";
		case ZT_ICMP6_ECHO_REQUEST:                         return "ICMP6_ECHO_REQUEST";
		case ZT_ICMP6_ECHO_REPLY:                           return "ICMP6_ECHO_REPLY";
		case ZT_ICMP6_MULTICAST_LISTENER_QUERY:             return "ICMP6_MULTICAST_LISTENER_QUERY";
		case ZT_ICMP6_MULTICAST_LISTENER_REPORT:            return "ICMP6_MULTICAST_LISTENER_REPORT";
		case ZT_ICMP6_MULTICAST_LISTENER_DONE:              return "ICMP6_MULTICAST_LISTENER_DONE";
		case ZT_ICMP6_ROUTER_SOLICITATION:                  return "ICMP6_ROUTER_SOLICITATION";
		case ZT_ICMP6_ROUTER_ADVERTISEMENT:                 return "ICMP6_ROUTER_ADVERTISEMENT";
		case ZT_ICMP6_NEIGHBOR_SOLICITATION:                return "ICMP6_NEIGHBOR_SOLICITATION";
		case ZT_ICMP6_NEIGHBOR_ADVERTISEMENT:               return "ICMP6_NEIGHBOR_ADVERTISEMENT";
		case ZT_ICMP6_REDIRECT_MESSAGE:                     return "ICMP6_REDIRECT_MESSAGE";
		case ZT_ICMP6_ROUTER_RENUMBERING:                   return "ICMP6_ROUTER_RENUMBERING";
		case ZT_ICMP6_NODE_INFORMATION_QUERY:               return "ICMP6_NODE_INFORMATION_QUERY";
		case ZT_ICMP6_NODE_INFORMATION_RESPONSE:            return "ICMP6_NODE_INFORMATION_RESPONSE";
		case ZT_ICMP6_INV_NEIGHBOR_SOLICITATION:            return "ICMP6_INV_NEIGHBOR_SOLICITATION";
		case ZT_ICMP6_INV_NEIGHBOR_ADVERTISEMENT:           return "ICMP6_INV_NEIGHBOR_ADVERTISEMENT";
		case ZT_ICMP6_MLDV2:                                return "ICMP6_MLDV2";
		case ZT_ICMP6_HOME_AGENT_ADDRESS_DISCOVERY_REQUEST: return "ICMP6_HOME_AGENT_ADDRESS_DISCOVERY_REQUEST";
		case ZT_ICMP6_HOME_AGENT_ADDRESS_DISCOVERY_REPLY:   return "ICMP6_HOME_AGENT_ADDRESS_DISCOVERY_REPLY";
		case ZT_ICMP6_MOBILE_PREFIX_SOLICITATION:           return "ICMP6_MOBILE_PREFIX_SOLICITATION";
		case ZT_ICMP6_MOBILE_PREFIX_ADVERTISEMENT:          return "ICMP6_MOBILE_PREFIX_ADVERTISEMENT";
		case ZT_ICMP6_CERTIFICATION_PATH_SOLICITATION:      return "ICMP6_CERTIFICATION_PATH_SOLICITATION";
		case ZT_ICMP6_CERTIFICATION_PATH_ADVERTISEMENT:     return "ICMP6_CERTIFICATION_PATH_ADVERTISEMENT";
		case ZT_ICMP6_MULTICAST_ROUTER_ADVERTISEMENT:       return "ICMP6_MULTICAST_ROUTER_ADVERTISEMENT";
		case ZT_ICMP6_MULTICAST_ROUTER_SOLICITATION:        return "ICMP6_MULTICAST_ROUTER_SOLICITATION";
		case ZT_ICMP6_MULTICAST_ROUTER_TERMINATION:         return "ICMP6_MULTICAST_ROUTER_TERMINATION";
		case ZT_ICMP6_RPL_CONTROL_MESSAGE:                  return "ICMP6_RPL_CONTROL_MESSAGE";
	}
	return UNKNOWN_NAME;
}

} // namespace ZeroTier
