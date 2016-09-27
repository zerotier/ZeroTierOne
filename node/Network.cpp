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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "Constants.hpp"
#include "../version.h"
#include "Network.hpp"
#include "RuntimeEnvironment.hpp"
#include "MAC.hpp"
#include "Address.hpp"
#include "InetAddress.hpp"
#include "Switch.hpp"
#include "Buffer.hpp"
#include "Packet.hpp"
#include "NetworkController.hpp"
#include "Node.hpp"
#include "Peer.hpp"

// Uncomment to make the rules engine dump trace info to stdout
//#define ZT_RULES_ENGINE_DEBUGGING 1

namespace ZeroTier {

namespace {

#ifdef ZT_RULES_ENGINE_DEBUGGING
#define FILTER_TRACE(f,...) { Utils::snprintf(dpbuf,sizeof(dpbuf),f,##__VA_ARGS__); dlog.push_back(std::string(dpbuf)); }
static const char *_rtn(const ZT_VirtualNetworkRuleType rt)
{
	switch(rt) {
		case ZT_NETWORK_RULE_ACTION_DROP: return "ACTION_DROP";
		case ZT_NETWORK_RULE_ACTION_ACCEPT: return "ACTION_ACCEPT";
		case ZT_NETWORK_RULE_ACTION_TEE: return "ACTION_TEE";
		case ZT_NETWORK_RULE_ACTION_WATCH: return "ACTION_WATCH";
		case ZT_NETWORK_RULE_ACTION_REDIRECT: return "ACTION_REDIRECT";
		case ZT_NETWORK_RULE_ACTION_DEBUG_LOG: return "ACTION_DEBUG_LOG";
		case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS: return "MATCH_SOURCE_ZEROTIER_ADDRESS";
		case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS: return "MATCH_DEST_ZEROTIER_ADDRESS";
		case ZT_NETWORK_RULE_MATCH_VLAN_ID: return "MATCH_VLAN_ID";
		case ZT_NETWORK_RULE_MATCH_VLAN_PCP: return "MATCH_VLAN_PCP";
		case ZT_NETWORK_RULE_MATCH_VLAN_DEI: return "MATCH_VLAN_DEI";
		case ZT_NETWORK_RULE_MATCH_ETHERTYPE: return "MATCH_ETHERTYPE";
		case ZT_NETWORK_RULE_MATCH_MAC_SOURCE: return "MATCH_MAC_SOURCE";
		case ZT_NETWORK_RULE_MATCH_MAC_DEST: return "MATCH_MAC_DEST";
		case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE: return "MATCH_IPV4_SOURCE";
		case ZT_NETWORK_RULE_MATCH_IPV4_DEST: return "MATCH_IPV4_DEST";
		case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE: return "MATCH_IPV6_SOURCE";
		case ZT_NETWORK_RULE_MATCH_IPV6_DEST: return "MATCH_IPV6_DEST";
		case ZT_NETWORK_RULE_MATCH_IP_TOS: return "MATCH_IP_TOS";
		case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL: return "MATCH_IP_PROTOCOL";
		case ZT_NETWORK_RULE_MATCH_ICMP: return "MATCH_ICMP";
		case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE: return "MATCH_IP_SOURCE_PORT_RANGE";
		case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE: return "MATCH_IP_DEST_PORT_RANGE";
		case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS: return "MATCH_CHARACTERISTICS";
		case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE: return "MATCH_FRAME_SIZE_RANGE";
		case ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE: return "MATCH_TAGS_DIFFERENCE";
		case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND: return "MATCH_TAGS_BITWISE_AND";
		case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR: return "MATCH_TAGS_BITWISE_OR";
		case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR: return "MATCH_TAGS_BITWISE_XOR";
		default: return "???";
	}
}
static const void _dumpFilterTrace(const char *ruleName,uint8_t thisSetMatches,bool inbound,const Address &ztSource,const Address &ztDest,const MAC &macSource,const MAC &macDest,const std::vector<std::string> &dlog,unsigned int frameLen,unsigned int etherType,const char *msg)
{
	static volatile unsigned long cnt = 0;
	printf("%.6lu %c %s %s frameLen=%u etherType=%u" ZT_EOL_S,
		cnt++,
		((thisSetMatches) ? 'Y' : '.'),
		ruleName,
		((inbound) ? "INBOUND" : "OUTBOUND"),
		frameLen,
		etherType
	);
	for(std::vector<std::string>::const_iterator m(dlog.begin());m!=dlog.end();++m)
		printf("     | %s" ZT_EOL_S,m->c_str());
	printf("     + %c %s->%s %.2x:%.2x:%.2x:%.2x:%.2x:%.2x->%.2x:%.2x:%.2x:%.2x:%.2x:%.2x" ZT_EOL_S,
		((thisSetMatches) ? 'Y' : '.'),
		ztSource.toString().c_str(),
		ztDest.toString().c_str(),
		(unsigned int)macSource[0],
		(unsigned int)macSource[1],
		(unsigned int)macSource[2],
		(unsigned int)macSource[3],
		(unsigned int)macSource[4],
		(unsigned int)macSource[5],
		(unsigned int)macDest[0],
		(unsigned int)macDest[1],
		(unsigned int)macDest[2],
		(unsigned int)macDest[3],
		(unsigned int)macDest[4],
		(unsigned int)macDest[5]
	);
	if (msg)
		printf("     +   (%s)" ZT_EOL_S,msg);
}
#else
#define FILTER_TRACE(f,...) {}
#endif // ZT_RULES_ENGINE_DEBUGGING

// Returns true if packet appears valid; pos and proto will be set
static bool _ipv6GetPayload(const uint8_t *frameData,unsigned int frameLen,unsigned int &pos,unsigned int &proto)
{
	if (frameLen < 40)
		return false;
	pos = 40;
	proto = frameData[6];
	while (pos <= frameLen) {
		switch(proto) {
			case 0: // hop-by-hop options
			case 43: // routing
			case 60: // destination options
			case 135: // mobility options
				if ((pos + 8) > frameLen)
					return false; // invalid!
				proto = frameData[pos];
				pos += ((unsigned int)frameData[pos + 1] * 8) + 8;
				break;

			//case 44: // fragment -- we currently can't parse these and they are deprecated in IPv6 anyway
			//case 50:
			//case 51: // IPSec ESP and AH -- we have to stop here since this is encrypted stuff
			default:
				return true;
		}
	}
	return false; // overflow == invalid
}

enum _doZtFilterResult
{
	DOZTFILTER_NO_MATCH,
	DOZTFILTER_DROP,
	DOZTFILTER_REDIRECT,
	DOZTFILTER_ACCEPT,
	DOZTFILTER_SUPER_ACCEPT
};
static _doZtFilterResult _doZtFilter(
	const RuntimeEnvironment *RR,
	const NetworkConfig &nconf,
	const Membership *membership, // can be NULL
	const bool inbound,
	const Address &ztSource,
	Address &ztDest, // MUTABLE -- is changed on REDIRECT actions
	const MAC &macSource,
	const MAC &macDest,
	const uint8_t *const frameData,
	const unsigned int frameLen,
	const unsigned int etherType,
	const unsigned int vlanId,
	const ZT_VirtualNetworkRule *rules, // cannot be NULL
	const unsigned int ruleCount,
	Address &cc, // MUTABLE -- set to TEE destination if TEE action is taken or left alone otherwise
	unsigned int &ccLength, // MUTABLE -- set to length of packet payload to TEE
	bool &ccWatch) // MUTABLE -- set to true for WATCH target as opposed to normal TEE
{
#ifdef ZT_RULES_ENGINE_DEBUGGING
	char dpbuf[1024]; // used by FILTER_TRACE macro
	std::vector<std::string> dlog;
#endif // ZT_RULES_ENGINE_DEBUGGING

	// The default match state for each set of entries starts as 'true' since an
	// ACTION with no MATCH entries preceding it is always taken.
	uint8_t thisSetMatches = 1;

	for(unsigned int rn=0;rn<ruleCount;++rn) {
		const ZT_VirtualNetworkRuleType rt = (ZT_VirtualNetworkRuleType)(rules[rn].t & 0x7f);

		// First check if this is an ACTION
		if ((unsigned int)rt <= (unsigned int)ZT_NETWORK_RULE_ACTION__MAX_ID) {
			if (thisSetMatches) {
				switch(rt) {
					case ZT_NETWORK_RULE_ACTION_DROP:
#ifdef ZT_RULES_ENGINE_DEBUGGING
						_dumpFilterTrace("ACTION_DROP",thisSetMatches,inbound,ztSource,ztDest,macSource,macDest,dlog,frameLen,etherType,(const char *)0);
#endif // ZT_RULES_ENGINE_DEBUGGING
						return DOZTFILTER_DROP;

					case ZT_NETWORK_RULE_ACTION_ACCEPT:
#ifdef ZT_RULES_ENGINE_DEBUGGING
						_dumpFilterTrace("ACTION_ACCEPT",thisSetMatches,inbound,ztSource,ztDest,macSource,macDest,dlog,frameLen,etherType,(const char *)0);
#endif // ZT_RULES_ENGINE_DEBUGGING
						return DOZTFILTER_ACCEPT; // match, accept packet

					// These are initially handled together since preliminary logic is common
					case ZT_NETWORK_RULE_ACTION_TEE:
					case ZT_NETWORK_RULE_ACTION_WATCH:
					case ZT_NETWORK_RULE_ACTION_REDIRECT:	{
						const Address fwdAddr(rules[rn].v.fwd.address);
						if (fwdAddr == ztSource) {
#ifdef ZT_RULES_ENGINE_DEBUGGING
							_dumpFilterTrace(_rtn(rt),thisSetMatches,inbound,ztSource,ztDest,macSource,macDest,dlog,frameLen,etherType,"skipped as no-op since source is target");
							dlog.clear();
#endif // ZT_RULES_ENGINE_DEBUGGING
						} else if (fwdAddr == RR->identity.address()) {
							if (inbound) {
#ifdef ZT_RULES_ENGINE_DEBUGGING
								_dumpFilterTrace(_rtn(rt),thisSetMatches,inbound,ztSource,ztDest,macSource,macDest,dlog,frameLen,etherType,"interpreted as super-ACCEPT on inbound since we are target");
#endif // ZT_RULES_ENGINE_DEBUGGING
								return DOZTFILTER_SUPER_ACCEPT;
							} else {
#ifdef ZT_RULES_ENGINE_DEBUGGING
								_dumpFilterTrace(_rtn(rt),thisSetMatches,inbound,ztSource,ztDest,macSource,macDest,dlog,frameLen,etherType,"skipped as no-op on outbound since we are target");
								dlog.clear();
#endif // ZT_RULES_ENGINE_DEBUGGING
							}
						} else if (fwdAddr == ztDest) {
#ifdef ZT_RULES_ENGINE_DEBUGGING
							_dumpFilterTrace(_rtn(rt),thisSetMatches,inbound,ztSource,ztDest,macSource,macDest,dlog,frameLen,etherType,"skipped as no-op because destination is already target");
							dlog.clear();
#endif // ZT_RULES_ENGINE_DEBUGGING
						} else {
							if (rt == ZT_NETWORK_RULE_ACTION_REDIRECT) {
#ifdef ZT_RULES_ENGINE_DEBUGGING
								_dumpFilterTrace("ACTION_REDIRECT",thisSetMatches,inbound,ztSource,ztDest,macSource,macDest,dlog,frameLen,etherType,(const char *)0);
#endif // ZT_RULES_ENGINE_DEBUGGING
								ztDest = fwdAddr;
								return DOZTFILTER_REDIRECT;
							} else {
#ifdef ZT_RULES_ENGINE_DEBUGGING
								_dumpFilterTrace("ACTION_TEE",thisSetMatches,inbound,ztSource,ztDest,macSource,macDest,dlog,frameLen,etherType,(const char *)0);
								dlog.clear();
#endif // ZT_RULES_ENGINE_DEBUGGING
								cc = fwdAddr;
								ccLength = (rules[rn].v.fwd.length != 0) ? ((frameLen < (unsigned int)rules[rn].v.fwd.length) ? frameLen : (unsigned int)rules[rn].v.fwd.length) : frameLen;
								ccWatch = (rt == ZT_NETWORK_RULE_ACTION_WATCH);
							}
						}
					}	continue;

					// This is a no-op that exists for use with rules engine tracing and isn't for use in production
					case ZT_NETWORK_RULE_ACTION_DEBUG_LOG: // a no-op target specifically for debugging purposes
#ifdef ZT_RULES_ENGINE_DEBUGGING
						_dumpFilterTrace("ACTION_DEBUG_LOG",thisSetMatches,inbound,ztSource,ztDest,macSource,macDest,dlog,frameLen,etherType,(const char *)0);
						dlog.clear();
#endif // ZT_RULES_ENGINE_DEBUGGING
						continue;

					// Unrecognized ACTIONs are ignored as no-ops
					default:
#ifdef ZT_RULES_ENGINE_DEBUGGING
						_dumpFilterTrace(_rtn(rt),thisSetMatches,inbound,ztSource,ztDest,macSource,macDest,dlog,frameLen,etherType,(const char *)0);
						dlog.clear();
#endif // ZT_RULES_ENGINE_DEBUGGING
						continue;
				}
			} else {
#ifdef ZT_RULES_ENGINE_DEBUGGING
				_dumpFilterTrace(_rtn(rt),thisSetMatches,inbound,ztSource,ztDest,macSource,macDest,dlog,frameLen,etherType,(const char *)0);
				dlog.clear();
#endif // ZT_RULES_ENGINE_DEBUGGING
				thisSetMatches = 1; // reset to default true for next batch of entries
				continue;
			}
		}

		// Circuit breaker: skip further MATCH entries up to next ACTION if match state is false
		if (!thisSetMatches)
			continue;

		// If this was not an ACTION evaluate next MATCH and update thisSetMatches with (AND [result])
		uint8_t thisRuleMatches = 0;
		switch(rt) {
			case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
				thisRuleMatches = (uint8_t)(rules[rn].v.zt == ztSource.toInt());
				FILTER_TRACE("%u %s %c %.10llx==%.10llx -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),rules[rn].v.zt,ztSource.toInt(),(unsigned int)thisRuleMatches);
				break;
			case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS:
				thisRuleMatches = (uint8_t)(rules[rn].v.zt == ztDest.toInt());
				FILTER_TRACE("%u %s %c %.10llx==%.10llx -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),rules[rn].v.zt,ztDest.toInt(),(unsigned int)thisRuleMatches);
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_ID:
				thisRuleMatches = (uint8_t)(rules[rn].v.vlanId == (uint16_t)vlanId);
				FILTER_TRACE("%u %s %c %u==%u -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.vlanId,(unsigned int)vlanId,(unsigned int)thisRuleMatches);
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
				// NOT SUPPORTED YET
				thisRuleMatches = (uint8_t)(rules[rn].v.vlanPcp == 0);
				FILTER_TRACE("%u %s %c %u==%u -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.vlanPcp,0,(unsigned int)thisRuleMatches);
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
				// NOT SUPPORTED YET
				thisRuleMatches = (uint8_t)(rules[rn].v.vlanDei == 0);
				FILTER_TRACE("%u %s %c %u==%u -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.vlanDei,0,(unsigned int)thisRuleMatches);
				break;
			case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
				thisRuleMatches = (uint8_t)(rules[rn].v.etherType == (uint16_t)etherType);
				FILTER_TRACE("%u %s %c %u==%u -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.etherType,etherType,(unsigned int)thisRuleMatches);
				break;
			case ZT_NETWORK_RULE_MATCH_MAC_SOURCE:
				thisRuleMatches = (uint8_t)(MAC(rules[rn].v.mac,6) == macSource);
				FILTER_TRACE("%u %s %c %.12llx=%.12llx -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),rules[rn].v.mac,macSource.toInt(),(unsigned int)thisRuleMatches);
				break;
			case ZT_NETWORK_RULE_MATCH_MAC_DEST:
				thisRuleMatches = (uint8_t)(MAC(rules[rn].v.mac,6) == macDest);
				FILTER_TRACE("%u %s %c %.12llx=%.12llx -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),rules[rn].v.mac,macDest.toInt(),(unsigned int)thisRuleMatches);
				break;
			case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
				if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)) {
					thisRuleMatches = (uint8_t)(InetAddress((const void *)&(rules[rn].v.ipv4.ip),4,rules[rn].v.ipv4.mask).containsAddress(InetAddress((const void *)(frameData + 12),4,0)));
					FILTER_TRACE("%u %s %c %s contains %s -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),InetAddress((const void *)&(rules[rn].v.ipv4.ip),4,rules[rn].v.ipv4.mask).toString().c_str(),InetAddress((const void *)(frameData + 12),4,0).toIpString().c_str(),(unsigned int)thisRuleMatches);
				} else {
					thisRuleMatches = 0;
					FILTER_TRACE("%u %s %c [frame not IPv4] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
				}
				break;
			case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
				if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)) {
					thisRuleMatches = (uint8_t)(InetAddress((const void *)&(rules[rn].v.ipv4.ip),4,rules[rn].v.ipv4.mask).containsAddress(InetAddress((const void *)(frameData + 16),4,0)));
					FILTER_TRACE("%u %s %c %s contains %s -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),InetAddress((const void *)&(rules[rn].v.ipv4.ip),4,rules[rn].v.ipv4.mask).toString().c_str(),InetAddress((const void *)(frameData + 16),4,0).toIpString().c_str(),(unsigned int)thisRuleMatches);
				} else {
					thisRuleMatches = 0;
					FILTER_TRACE("%u %s %c [frame not IPv4] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
				}
				break;
			case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
				if ((etherType == ZT_ETHERTYPE_IPV6)&&(frameLen >= 40)) {
					thisRuleMatches = (uint8_t)(InetAddress((const void *)rules[rn].v.ipv6.ip,16,rules[rn].v.ipv6.mask).containsAddress(InetAddress((const void *)(frameData + 8),16,0)));
					FILTER_TRACE("%u %s %c %s contains %s -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),InetAddress((const void *)rules[rn].v.ipv6.ip,16,rules[rn].v.ipv6.mask).toString().c_str(),InetAddress((const void *)(frameData + 8),16,0).toIpString().c_str(),(unsigned int)thisRuleMatches);
				} else {
					thisRuleMatches = 0;
					FILTER_TRACE("%u %s %c [frame not IPv6] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
				}
				break;
			case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
				if ((etherType == ZT_ETHERTYPE_IPV6)&&(frameLen >= 40)) {
					thisRuleMatches = (uint8_t)(InetAddress((const void *)rules[rn].v.ipv6.ip,16,rules[rn].v.ipv6.mask).containsAddress(InetAddress((const void *)(frameData + 24),16,0)));
					FILTER_TRACE("%u %s %c %s contains %s -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),InetAddress((const void *)rules[rn].v.ipv6.ip,16,rules[rn].v.ipv6.mask).toString().c_str(),InetAddress((const void *)(frameData + 24),16,0).toIpString().c_str(),(unsigned int)thisRuleMatches);
				} else {
					thisRuleMatches = 0;
					FILTER_TRACE("%u %s %c [frame not IPv6] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
				}
				break;
			case ZT_NETWORK_RULE_MATCH_IP_TOS:
				if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)) {
					thisRuleMatches = (uint8_t)(rules[rn].v.ipTos == ((frameData[1] & 0xfc) >> 2));
					FILTER_TRACE("%u %s %c (IPv4) %u==%u -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.ipTos,(unsigned int)((frameData[1] & 0xfc) >> 2),(unsigned int)thisRuleMatches);
				} else if ((etherType == ZT_ETHERTYPE_IPV6)&&(frameLen >= 40)) {
					const uint8_t trafficClass = ((frameData[0] << 4) & 0xf0) | ((frameData[1] >> 4) & 0x0f);
					thisRuleMatches = (uint8_t)(rules[rn].v.ipTos == ((trafficClass & 0xfc) >> 2));
					FILTER_TRACE("%u %s %c (IPv6) %u==%u -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.ipTos,(unsigned int)((trafficClass & 0xfc) >> 2),(unsigned int)thisRuleMatches);
				} else {
					thisRuleMatches = 0;
					FILTER_TRACE("%u %s %c [frame not IP] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
				}
				break;
			case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
				if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)) {
					thisRuleMatches = (uint8_t)(rules[rn].v.ipProtocol == frameData[9]);
					FILTER_TRACE("%u %s %c (IPv4) %u==%u -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.ipProtocol,(unsigned int)frameData[9],(unsigned int)thisRuleMatches);
				} else if (etherType == ZT_ETHERTYPE_IPV6) {
					unsigned int pos = 0,proto = 0;
					if (_ipv6GetPayload(frameData,frameLen,pos,proto)) {
						thisRuleMatches = (uint8_t)(rules[rn].v.ipProtocol == (uint8_t)proto);
						FILTER_TRACE("%u %s %c (IPv6) %u==%u -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.ipProtocol,proto,(unsigned int)thisRuleMatches);
					} else {
						thisRuleMatches = 0;
						FILTER_TRACE("%u %s %c [invalid IPv6] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
					}
				} else {
					thisRuleMatches = 0;
					FILTER_TRACE("%u %s %c [frame not IP] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
				}
				break;
			case ZT_NETWORK_RULE_MATCH_ICMP:
				if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)) {
					if (frameData[9] == 0x01) {
						const unsigned int ihl = (frameData[0] & 0xf) * 32;
						if (frameLen >= (ihl + 2)) {
							if (rules[rn].v.icmp.type == frameData[ihl]) {
								if ((rules[rn].v.icmp.flags & 0x01) != 0) {
									thisRuleMatches = (uint8_t)(frameData[ihl+1] == rules[rn].v.icmp.code);
								} else {
									thisRuleMatches = 1;
								}
							} else {
								thisRuleMatches = 0;
							}
							FILTER_TRACE("%u %s %c (IPv4) icmp-type:%d==%d icmp-code:%d==%d -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(int)frameData[ihl],(int)rules[rn].v.icmp.type,(int)frameData[ihl+1],(((rules[rn].v.icmp.flags & 0x01) != 0) ? (int)rules[rn].v.icmp.code : -1),(unsigned int)thisRuleMatches);
						} else {
							thisRuleMatches = 0;
							FILTER_TRACE("%u %s %c [IPv4 frame invalid] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
						}
					} else {
						thisRuleMatches = 0;
						FILTER_TRACE("%u %s %c [frame not ICMP] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
					}
				} else if (etherType == ZT_ETHERTYPE_IPV6) {
					unsigned int pos = 0,proto = 0;
					if (_ipv6GetPayload(frameData,frameLen,pos,proto)) {
						if ((proto == 0x3a)&&(frameLen >= (pos+2))) {
							if (rules[rn].v.icmp.type == frameData[pos]) {
								if ((rules[rn].v.icmp.flags & 0x01) != 0) {
									thisRuleMatches = (uint8_t)(frameData[pos+1] == rules[rn].v.icmp.code);
								} else {
									thisRuleMatches = 1;
								}
							} else {
								thisRuleMatches = 0;
							}
							FILTER_TRACE("%u %s %c (IPv4) icmp-type:%d==%d icmp-code:%d==%d -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(int)frameData[pos],(int)rules[rn].v.icmp.type,(int)frameData[pos+1],(((rules[rn].v.icmp.flags & 0x01) != 0) ? (int)rules[rn].v.icmp.code : -1),(unsigned int)thisRuleMatches);
						} else {
							thisRuleMatches = 0;
							FILTER_TRACE("%u %s %c [frame not ICMPv6] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
						}
					} else {
						thisRuleMatches = 0;
						FILTER_TRACE("%u %s %c [invalid IPv6] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
					}
				} else {
					thisRuleMatches = 0;
					FILTER_TRACE("%u %s %c [frame not IP] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
				}
				break;
				break;
			case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE:
			case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE:
				if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)) {
					const unsigned int headerLen = 4 * (frameData[0] & 0xf);
					int p = -1;
					switch(frameData[9]) { // IP protocol number
						// All these start with 16-bit source and destination port in that order
						case 0x06: // TCP
						case 0x11: // UDP
						case 0x84: // SCTP
						case 0x88: // UDPLite
							if (frameLen > (headerLen + 4)) {
								unsigned int pos = headerLen + ((rt == ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE) ? 2 : 0);
								p = (int)frameData[pos++] << 8;
								p |= (int)frameData[pos];
							}
							break;
					}

					thisRuleMatches = (p >= 0) ? (uint8_t)((p >= (int)rules[rn].v.port[0])&&(p <= (int)rules[rn].v.port[1])) : (uint8_t)0;
					FILTER_TRACE("%u %s %c (IPv4) %d in %d-%d -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),p,(int)rules[rn].v.port[0],(int)rules[rn].v.port[1],(unsigned int)thisRuleMatches);
				} else if (etherType == ZT_ETHERTYPE_IPV6) {
					unsigned int pos = 0,proto = 0;
					if (_ipv6GetPayload(frameData,frameLen,pos,proto)) {
						int p = -1;
						switch(proto) { // IP protocol number
							// All these start with 16-bit source and destination port in that order
							case 0x06: // TCP
							case 0x11: // UDP
							case 0x84: // SCTP
							case 0x88: // UDPLite
								if (frameLen > (pos + 4)) {
									if (rt == ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE) pos += 2;
									p = (int)frameData[pos++] << 8;
									p |= (int)frameData[pos];
								}
								break;
						}
						thisRuleMatches = (p > 0) ? (uint8_t)((p >= (int)rules[rn].v.port[0])&&(p <= (int)rules[rn].v.port[1])) : (uint8_t)0;
						FILTER_TRACE("%u %s %c (IPv6) %d in %d-%d -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),p,(int)rules[rn].v.port[0],(int)rules[rn].v.port[1],(unsigned int)thisRuleMatches);
					} else {
						thisRuleMatches = 0;
						FILTER_TRACE("%u %s %c [invalid IPv6] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
					}
				} else {
					thisRuleMatches = 0;
					FILTER_TRACE("%u %s %c [frame not IP] -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='));
				}
				break;
			case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS: {
				uint64_t cf = (inbound) ? ZT_RULE_PACKET_CHARACTERISTICS_INBOUND : 0ULL;
				if (macDest.isMulticast()) cf |= ZT_RULE_PACKET_CHARACTERISTICS_MULTICAST;
				if (macDest.isBroadcast()) cf |= ZT_RULE_PACKET_CHARACTERISTICS_BROADCAST;
				if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)&&(frameData[9] == 0x06)) {
					const unsigned int headerLen = 4 * (frameData[0] & 0xf);
					cf |= (uint64_t)frameData[headerLen + 13];
					cf |= (((uint64_t)(frameData[headerLen + 12] & 0x0f)) << 8);
				} else if (etherType == ZT_ETHERTYPE_IPV6) {
					unsigned int pos = 0,proto = 0;
					if (_ipv6GetPayload(frameData,frameLen,pos,proto)) {
						if ((proto == 0x06)&&(frameLen > (pos + 14))) {
							cf |= (uint64_t)frameData[pos + 13];
							cf |= (((uint64_t)(frameData[pos + 12] & 0x0f)) << 8);
						}
					}
				}
				thisRuleMatches = (uint8_t)((cf & rules[rn].v.characteristics[0]) == rules[rn].v.characteristics[1]);
				FILTER_TRACE("%u %s %c (%.16llx & %.16llx)==%.16llx -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),cf,rules[rn].v.characteristics[0],rules[rn].v.characteristics[1],(unsigned int)thisRuleMatches);
			}	break;
			case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
				thisRuleMatches = (uint8_t)((frameLen >= (unsigned int)rules[rn].v.frameSize[0])&&(frameLen <= (unsigned int)rules[rn].v.frameSize[1]));
				FILTER_TRACE("%u %s %c %u in %u-%u -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),frameLen,(unsigned int)rules[rn].v.frameSize[0],(unsigned int)rules[rn].v.frameSize[1],(unsigned int)thisRuleMatches);
				break;
			case ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR: {
				const Tag *const localTag = std::lower_bound(&(nconf.tags[0]),&(nconf.tags[nconf.tagCount]),rules[rn].v.tag.id,Tag::IdComparePredicate());
				if ((localTag != &(nconf.tags[nconf.tagCount]))&&(localTag->id() == rules[rn].v.tag.id)) {
					const Tag *const remoteTag = ((membership) ? membership->getTag(nconf,rules[rn].v.tag.id) : (const Tag *)0);
					if (remoteTag) {
						const uint32_t ltv = localTag->value();
						const uint32_t rtv = remoteTag->value();
						if (rt == ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE) {
							const uint32_t diff = (ltv > rtv) ? (ltv - rtv) : (rtv - ltv);
							thisRuleMatches = (uint8_t)(diff <= rules[rn].v.tag.value);
							FILTER_TRACE("%u %s %c TAG %u local:%u remote:%u difference:%u<=%u -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.tag.id,ltv,rtv,diff,(unsigned int)rules[rn].v.tag.value,thisRuleMatches);
						} else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND) {
							thisRuleMatches = (uint8_t)((ltv & rtv) == rules[rn].v.tag.value);
							FILTER_TRACE("%u %s %c TAG %u local:%.8x & remote:%.8x == %.8x -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.tag.id,ltv,rtv,(unsigned int)rules[rn].v.tag.value,(unsigned int)thisRuleMatches);
						} else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR) {
							thisRuleMatches = (uint8_t)((ltv | rtv) == rules[rn].v.tag.value);
							FILTER_TRACE("%u %s %c TAG %u local:%.8x | remote:%.8x == %.8x -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.tag.id,ltv,rtv,(unsigned int)rules[rn].v.tag.value,(unsigned int)thisRuleMatches);
						} else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR) {
							thisRuleMatches = (uint8_t)((ltv ^ rtv) == rules[rn].v.tag.value);
							FILTER_TRACE("%u %s %c TAG %u local:%.8x ^ remote:%.8x == %.8x -> %u",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.tag.id,ltv,rtv,(unsigned int)rules[rn].v.tag.value,(unsigned int)thisRuleMatches);
						} else { // sanity check, can't really happen
							thisRuleMatches = 0;
						}
					} else {
						if (inbound) {
							thisRuleMatches = 0;
							FILTER_TRACE("%u %s %c remote tag %u not found -> 0 (inbound side is strict)",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.tag.id);
						} else {
							thisRuleMatches = 1;
							FILTER_TRACE("%u %s %c remote tag %u not found -> 1 (outbound side is not strict)",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.tag.id);
						}
					}
				} else {
					thisRuleMatches = 0;
					FILTER_TRACE("%u %s %c local tag %u not found -> 0",rn,_rtn(rt),(((rules[rn].t & 0x80) != 0) ? '!' : '='),(unsigned int)rules[rn].v.tag.id);
				}
			}	break;

			// The result of an unsupported MATCH is configurable at the network
			// level via a flag.
			default:
				thisRuleMatches = (uint8_t)((nconf.flags & ZT_NETWORKCONFIG_FLAG_RULES_RESULT_OF_UNSUPPORTED_MATCH) != 0);
				break;
		}

		// State of equals state AND result of last MATCH (possibly NOTed depending on bit 0x80)
		thisSetMatches &= (thisRuleMatches ^ ((rules[rn].t >> 7) & 1));
	}

	return DOZTFILTER_NO_MATCH;
}

} // anonymous namespace

const ZeroTier::MulticastGroup Network::BROADCAST(ZeroTier::MAC(0xffffffffffffULL),0);

Network::Network(const RuntimeEnvironment *renv,uint64_t nwid,void *uptr) :
	RR(renv),
	_uPtr(uptr),
	_id(nwid),
	_lastAnnouncedMulticastGroupsUpstream(0),
	_mac(renv->identity.address(),nwid),
	_portInitialized(false),
	_lastConfigUpdate(0),
	_destroyed(false),
	_netconfFailure(NETCONF_FAILURE_NONE),
	_portError(0)
{
	for(int i=0;i<ZT_NETWORK_MAX_INCOMING_UPDATES;++i)
		_incomingConfigChunks[i].ts = 0;

	char confn[128];
	Utils::snprintf(confn,sizeof(confn),"networks.d/%.16llx.conf",_id);

	bool gotConf = false;
	Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> *dconf = new Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY>();
	NetworkConfig *nconf = new NetworkConfig();
	try {
		std::string conf(RR->node->dataStoreGet(confn));
		if (conf.length()) {
			dconf->load(conf.c_str());
			if (nconf->fromDictionary(*dconf)) {
				this->_setConfiguration(*nconf,false);
				_lastConfigUpdate = 0; // we still want to re-request a new config from the network
				gotConf = true;
			}
		}
	} catch ( ... ) {} // ignore invalids, we'll re-request
	delete nconf;
	delete dconf;

	if (!gotConf) {
		// Save a one-byte CR to persist membership while we request a real netconf
		RR->node->dataStorePut(confn,"\n",1,false);
	}

	if (!_portInitialized) {
		ZT_VirtualNetworkConfig ctmp;
		_externalConfig(&ctmp);
		_portError = RR->node->configureVirtualNetworkPort(_id,&_uPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP,&ctmp);
		_portInitialized = true;
	}
}

Network::~Network()
{
	ZT_VirtualNetworkConfig ctmp;
	_externalConfig(&ctmp);

	char n[128];
	if (_destroyed) {
		RR->node->configureVirtualNetworkPort(_id,&_uPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY,&ctmp);
		Utils::snprintf(n,sizeof(n),"networks.d/%.16llx.conf",_id);
		RR->node->dataStoreDelete(n);
	} else {
		RR->node->configureVirtualNetworkPort(_id,&_uPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN,&ctmp);
	}
}

bool Network::filterOutgoingPacket(
	const bool noTee,
	const Address &ztSource,
	const Address &ztDest,
	const MAC &macSource,
	const MAC &macDest,
	const uint8_t *frameData,
	const unsigned int frameLen,
	const unsigned int etherType,
	const unsigned int vlanId)
{
	const uint64_t now = RR->node->now();
	Address ztFinalDest(ztDest);
	int localCapabilityIndex = -1;
	bool accept = false;

	Mutex::Lock _l(_lock);

	Membership *const membership = (ztDest) ? _memberships.get(ztDest) : (Membership *)0;

	Address cc;
	unsigned int ccLength = 0;
	bool ccWatch = false;
	switch(_doZtFilter(RR,_config,membership,false,ztSource,ztFinalDest,macSource,macDest,frameData,frameLen,etherType,vlanId,_config.rules,_config.ruleCount,cc,ccLength,ccWatch)) {

		case DOZTFILTER_NO_MATCH:
			for(unsigned int c=0;c<_config.capabilityCount;++c) {
				ztFinalDest = ztDest; // sanity check, shouldn't be possible if there was no match
				Address cc2;
				unsigned int ccLength2 = 0;
				bool ccWatch2 = false;
				switch (_doZtFilter(RR,_config,membership,false,ztSource,ztFinalDest,macSource,macDest,frameData,frameLen,etherType,vlanId,_config.capabilities[c].rules(),_config.capabilities[c].ruleCount(),cc2,ccLength2,ccWatch2)) {
					case DOZTFILTER_NO_MATCH:
					case DOZTFILTER_DROP: // explicit DROP in a capability just terminates its evaluation and is an anti-pattern
						break;

					case DOZTFILTER_REDIRECT: // interpreted as ACCEPT but ztFinalDest will have been changed in _doZtFilter()
					case DOZTFILTER_ACCEPT:
					case DOZTFILTER_SUPER_ACCEPT: // no difference in behavior on outbound side
						localCapabilityIndex = (int)c;
						accept = true;

						if ((!noTee)&&(cc2)) {
							Membership &m2 = _membership(cc2);
							m2.pushCredentials(RR,now,cc2,_config,localCapabilityIndex,false);

							Packet outp(cc2,RR->identity.address(),Packet::VERB_EXT_FRAME);
							outp.append(_id);
							outp.append((uint8_t)(ccWatch2 ? 0x16 : 0x02));
							macDest.appendTo(outp);
							macSource.appendTo(outp);
							outp.append((uint16_t)etherType);
							outp.append(frameData,ccLength2);
							outp.compress();
							RR->sw->send(outp,true);
						}

						break;
				}
				if (accept)
					break;
			}
			break;

		case DOZTFILTER_DROP:
			return false;

		case DOZTFILTER_REDIRECT: // interpreted as ACCEPT but ztFinalDest will have been changed in _doZtFilter()
		case DOZTFILTER_ACCEPT:
		case DOZTFILTER_SUPER_ACCEPT: // no difference in behavior on outbound side
			accept = true;
			break;
	}

	if (accept) {
		if (membership)
			membership->pushCredentials(RR,now,ztDest,_config,localCapabilityIndex,false);

		if ((!noTee)&&(cc)) {
			Membership &m2 = _membership(cc);
			m2.pushCredentials(RR,now,cc,_config,localCapabilityIndex,false);

			Packet outp(cc,RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(_id);
			outp.append((uint8_t)(ccWatch ? 0x16 : 0x02));
			macDest.appendTo(outp);
			macSource.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(frameData,ccLength);
			outp.compress();
			RR->sw->send(outp,true);
		}

		if ((ztDest != ztFinalDest)&&(ztFinalDest)) {
			Membership &m2 = _membership(ztFinalDest);
			m2.pushCredentials(RR,now,ztFinalDest,_config,localCapabilityIndex,false);

			Packet outp(ztFinalDest,RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(_id);
			outp.append((uint8_t)0x04);
			macDest.appendTo(outp);
			macSource.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(frameData,frameLen);
			outp.compress();
			RR->sw->send(outp,true);

			return false; // DROP locally, since we redirected
		} else {
			return true;
		}
	} else {
		return false;
	}
}

int Network::filterIncomingPacket(
	const SharedPtr<Peer> &sourcePeer,
	const Address &ztDest,
	const MAC &macSource,
	const MAC &macDest,
	const uint8_t *frameData,
	const unsigned int frameLen,
	const unsigned int etherType,
	const unsigned int vlanId)
{
	Address ztFinalDest(ztDest);
	int accept = 0;

	Mutex::Lock _l(_lock);

	Membership &membership = _membership(sourcePeer->address());

	Address cc;
	unsigned int ccLength = 0;
	bool ccWatch = false;
	switch (_doZtFilter(RR,_config,&membership,true,sourcePeer->address(),ztFinalDest,macSource,macDest,frameData,frameLen,etherType,vlanId,_config.rules,_config.ruleCount,cc,ccLength,ccWatch)) {

		case DOZTFILTER_NO_MATCH: {
			Membership::CapabilityIterator mci(membership,_config);
			const Capability *c;
			while ((c = mci.next())) {
				ztFinalDest = ztDest; // sanity check, should be unmodified if there was no match
				Address cc2;
				unsigned int ccLength2 = 0;
				bool ccWatch2 = false;
				switch(_doZtFilter(RR,_config,&membership,true,sourcePeer->address(),ztFinalDest,macSource,macDest,frameData,frameLen,etherType,vlanId,c->rules(),c->ruleCount(),cc2,ccLength2,ccWatch2)) {
					case DOZTFILTER_NO_MATCH:
					case DOZTFILTER_DROP: // explicit DROP in a capability just terminates its evaluation and is an anti-pattern
						break;
					case DOZTFILTER_REDIRECT: // interpreted as ACCEPT but ztDest will have been changed in _doZtFilter()
					case DOZTFILTER_ACCEPT:
						accept = 1; // ACCEPT
						break;
					case DOZTFILTER_SUPER_ACCEPT:
						accept = 2; // super-ACCEPT
						break;
				}

				if (accept) {
					if (cc2) {
						_membership(cc2).pushCredentials(RR,RR->node->now(),cc2,_config,-1,false);

						Packet outp(cc2,RR->identity.address(),Packet::VERB_EXT_FRAME);
						outp.append(_id);
						outp.append((uint8_t)(ccWatch2 ? 0x1c : 0x08));
						macDest.appendTo(outp);
						macSource.appendTo(outp);
						outp.append((uint16_t)etherType);
						outp.append(frameData,ccLength2);
						outp.compress();
						RR->sw->send(outp,true);
					}
					break;
				}
			}
		}	break;

		case DOZTFILTER_DROP:
			return 0; // DROP

		case DOZTFILTER_REDIRECT: // interpreted as ACCEPT but ztFinalDest will have been changed in _doZtFilter()
		case DOZTFILTER_ACCEPT:
			accept = 1; // ACCEPT
			break;
		case DOZTFILTER_SUPER_ACCEPT:
			accept = 2; // super-ACCEPT
			break;
	}

	if (accept) {
		if (cc) {
			_membership(cc).pushCredentials(RR,RR->node->now(),cc,_config,-1,false);

			Packet outp(cc,RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(_id);
			outp.append((uint8_t)(ccWatch ? 0x1c : 0x08));
			macDest.appendTo(outp);
			macSource.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(frameData,ccLength);
			outp.compress();
			RR->sw->send(outp,true);
		}

		if ((ztDest != ztFinalDest)&&(ztFinalDest)) {
			_membership(ztFinalDest).pushCredentials(RR,RR->node->now(),ztFinalDest,_config,-1,false);

			Packet outp(ztFinalDest,RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(_id);
			outp.append((uint8_t)0x0a);
			macDest.appendTo(outp);
			macSource.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(frameData,frameLen);
			outp.compress();
			RR->sw->send(outp,true);

			return 0; // DROP locally, since we redirected
		}
	}

	return accept;
}

bool Network::subscribedToMulticastGroup(const MulticastGroup &mg,bool includeBridgedGroups) const
{
	Mutex::Lock _l(_lock);
	if (std::binary_search(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg))
		return true;
	else if (includeBridgedGroups)
		return _multicastGroupsBehindMe.contains(mg);
	return false;
}

void Network::multicastSubscribe(const MulticastGroup &mg)
{
	Mutex::Lock _l(_lock);
	if (!std::binary_search(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg)) {
		_myMulticastGroups.insert(std::upper_bound(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg),mg);
		_sendUpdatesToMembers(&mg);
	}
}

void Network::multicastUnsubscribe(const MulticastGroup &mg)
{
	Mutex::Lock _l(_lock);
	std::vector<MulticastGroup>::iterator i(std::lower_bound(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg));
	if ( (i != _myMulticastGroups.end()) && (*i == mg) )
		_myMulticastGroups.erase(i);
}

uint64_t Network::handleConfigChunk(const Packet &chunk,unsigned int ptr)
{
	const unsigned int start = ptr;

	ptr += 8; // skip network ID, which is already obviously known
	const unsigned int chunkLen = chunk.at<uint16_t>(ptr); ptr += 2;
	const void *chunkData = chunk.field(ptr,chunkLen); ptr += chunkLen;

	Mutex::Lock _l(_lock);

	_IncomingConfigChunk *c = (_IncomingConfigChunk *)0;
	uint64_t chunkId = 0;
	uint64_t configUpdateId;
	unsigned long totalLength,chunkIndex;
	if (ptr < chunk.size()) {
		const bool fastPropagate = ((chunk[ptr++] & 0x01) != 0);
		configUpdateId = chunk.at<uint64_t>(ptr); ptr += 8;
		totalLength = chunk.at<uint32_t>(ptr); ptr += 4;
		chunkIndex = chunk.at<uint32_t>(ptr); ptr += 4;

		if (((chunkIndex + chunkLen) > totalLength)||(totalLength >= ZT_NETWORKCONFIG_DICT_CAPACITY)) { // >= since we need room for a null at the end
			TRACE("discarded chunk from %s: invalid length or length overflow",chunk.source().toString().c_str());
			return 0;
		}

		if ((chunk[ptr] != 1)||(chunk.at<uint16_t>(ptr + 1) != ZT_C25519_SIGNATURE_LEN)) {
			TRACE("discarded chunk from %s: unrecognized signature type",chunk.source().toString().c_str());
			return 0;
		}
		const uint8_t *sig = reinterpret_cast<const uint8_t *>(chunk.field(ptr + 3,ZT_C25519_SIGNATURE_LEN));

		// We can use the signature, which is unique per chunk, to get a per-chunk ID for local deduplication use
		for(unsigned int i=0;i<16;++i)
			reinterpret_cast<uint8_t *>(&chunkId)[i & 7] ^= sig[i];

		// Find existing or new slot for this update and check if this is a duplicate chunk
		for(int i=0;i<ZT_NETWORK_MAX_INCOMING_UPDATES;++i) {
			if (_incomingConfigChunks[i].updateId == configUpdateId) {
				c = &(_incomingConfigChunks[i]);

				for(unsigned long j=0;j<c->haveChunks;++j) {
					if (c->haveChunkIds[j] == chunkId)
						return 0;
				}

				break;
			} else if ((!c)||(_incomingConfigChunks[i].ts < c->ts)) {
				c = &(_incomingConfigChunks[i]);
			}
		}

		// If it's not a duplicate, check chunk signature
		const Identity controllerId(RR->topology->getIdentity(controller()));
		if (!controllerId) { // we should always have the controller identity by now, otherwise how would we have queried it the first time?
			TRACE("unable to verify chunk from %s: don't have controller identity",chunk.source().toString().c_str());
			return 0;
		}
		if (!controllerId.verify(chunk.field(start,ptr - start),ptr - start,sig,ZT_C25519_SIGNATURE_LEN)) {
			TRACE("discarded chunk from %s: signature check failed",chunk.source().toString().c_str());
			return 0;
		}

		// New properly verified chunks can be flooded "virally" through the network
		if (fastPropagate) {
			Address *a = (Address *)0;
			Membership *m = (Membership *)0;
			Hashtable<Address,Membership>::Iterator i(_memberships);
			while (i.next(a,m)) {
				if ((*a != chunk.source())&&(*a != controller())) {
					Packet outp(*a,RR->identity.address(),Packet::VERB_NETWORK_CONFIG);
					outp.append(reinterpret_cast<const uint8_t *>(chunk.data()) + start,chunk.size() - start);
					RR->sw->send(outp,true);
				}
			}
		}
	} else if (chunk.source() == controller()) {
		// Legacy support for OK(NETWORK_CONFIG_REQUEST) from older controllers
		chunkId = chunk.packetId();
		configUpdateId = chunkId;
		totalLength = chunkLen;
		chunkIndex = 0;

		if (totalLength >= ZT_NETWORKCONFIG_DICT_CAPACITY)
			return 0;

		for(int i=0;i<ZT_NETWORK_MAX_INCOMING_UPDATES;++i) {
			if ((!c)||(_incomingConfigChunks[i].ts < c->ts))
				c = &(_incomingConfigChunks[i]);
		}
	} else {
		TRACE("discarded single-chunk unsigned legacy config: this is only allowed if the sender is the controller itself");
		return 0;
	}

	++c->ts; // newer is higher, that's all we need

	if (c->updateId != configUpdateId) {
		c->updateId = configUpdateId;
		c->haveChunks = 0;
		c->haveBytes = 0;
	}
	if (c->haveChunks >= ZT_NETWORK_MAX_UPDATE_CHUNKS)
		return false;
	c->haveChunkIds[c->haveChunks++] = chunkId;

	memcpy(c->data.unsafeData() + chunkIndex,chunkData,chunkLen);
	c->haveBytes += chunkLen;

	if (c->haveBytes == totalLength) {
		c->data.unsafeData()[c->haveBytes] = (char)0; // ensure null terminated

		NetworkConfig *const nc = new NetworkConfig();
		try {
			if (nc->fromDictionary(c->data)) {
				this->_setConfiguration(*nc,true);
				return configUpdateId;
			}
			delete nc;
		} catch ( ... ) {
			delete nc;
		}
	}

	return 0;
}

void Network::requestConfiguration()
{
	const Address ctrl(controller());

	Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> rmd;
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_VERSION,(uint64_t)ZT_NETWORKCONFIG_VERSION);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_VENDOR,(uint64_t)ZT_VENDOR_ZEROTIER);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_PROTOCOL_VERSION,(uint64_t)ZT_PROTO_VERSION);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MAJOR_VERSION,(uint64_t)ZEROTIER_ONE_VERSION_MAJOR);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MINOR_VERSION,(uint64_t)ZEROTIER_ONE_VERSION_MINOR);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_REVISION,(uint64_t)ZEROTIER_ONE_VERSION_REVISION);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_NETWORK_RULES,(uint64_t)ZT_MAX_NETWORK_RULES);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_NETWORK_CAPABILITIES,(uint64_t)ZT_MAX_NETWORK_CAPABILITIES);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_CAPABILITY_RULES,(uint64_t)ZT_MAX_CAPABILITY_RULES);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_NETWORK_TAGS,(uint64_t)ZT_MAX_NETWORK_TAGS);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_FLAGS,(uint64_t)0);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_RULES_ENGINE_REV,(uint64_t)ZT_RULES_ENGINE_REVISION);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_RELAY_POLICY,(uint64_t)RR->node->relayPolicy());

	if (ctrl == RR->identity.address()) {
		if (RR->localNetworkController) {
			NetworkConfig nconf;
			switch(RR->localNetworkController->doNetworkConfigRequest(InetAddress(),RR->identity,RR->identity,_id,rmd,nconf)) {
				case NetworkController::NETCONF_QUERY_OK: {
					Mutex::Lock _l(_lock);
					this->_setConfiguration(nconf,true);
				}	return;
				case NetworkController::NETCONF_QUERY_OBJECT_NOT_FOUND:
					this->setNotFound();
					return;
				case NetworkController::NETCONF_QUERY_ACCESS_DENIED:
					this->setAccessDenied();
					return;
				default:
					return;
			}
		} else {
			this->setNotFound();
			return;
		}
	}

	TRACE("requesting netconf for network %.16llx from controller %s",(unsigned long long)_id,ctrl.toString().c_str());

	Packet outp(ctrl,RR->identity.address(),Packet::VERB_NETWORK_CONFIG_REQUEST);
	outp.append((uint64_t)_id);
	const unsigned int rmdSize = rmd.sizeBytes();
	outp.append((uint16_t)rmdSize);
	outp.append((const void *)rmd.data(),rmdSize);
	if (_config) {
		outp.append((uint64_t)_config.revision);
		outp.append((uint64_t)_config.timestamp);
	} else {
		outp.append((unsigned char)0,16);
	}
	RR->node->expectReplyTo(outp.packetId());
	outp.compress();
	RR->sw->send(outp,true);
}

bool Network::gate(const SharedPtr<Peer> &peer)
{
	const uint64_t now = RR->node->now();
	Mutex::Lock _l(_lock);
	try {
		if (_config) {
			Membership *m = _memberships.get(peer->address());
			if ( (_config.isPublic()) || ((m)&&(m->isAllowedOnNetwork(_config))) ) {
				if (!m)
					m = &(_membership(peer->address()));
				m->pushCredentials(RR,now,peer->address(),_config,-1,false);
				if (m->shouldLikeMulticasts(now)) {
					_announceMulticastGroupsTo(peer->address(),_allMulticastGroups());
					m->likingMulticasts(now);
				}
				return true;
			}
		}
	} catch ( ... ) {
		TRACE("gate() check failed for peer %s: unexpected exception",peer->address().toString().c_str());
	}
	return false;
}

void Network::clean()
{
	const uint64_t now = RR->node->now();
	Mutex::Lock _l(_lock);

	if (_destroyed)
		return;

	{
		Hashtable< MulticastGroup,uint64_t >::Iterator i(_multicastGroupsBehindMe);
		MulticastGroup *mg = (MulticastGroup *)0;
		uint64_t *ts = (uint64_t *)0;
		while (i.next(mg,ts)) {
			if ((now - *ts) > (ZT_MULTICAST_LIKE_EXPIRE * 2))
				_multicastGroupsBehindMe.erase(*mg);
		}
	}

	{
		Address *a = (Address *)0;
		Membership *m = (Membership *)0;
		Hashtable<Address,Membership>::Iterator i(_memberships);
		while (i.next(a,m)) {
			if (!RR->topology->getPeerNoCache(*a))
				_memberships.erase(*a);
		}
	}
}

void Network::learnBridgeRoute(const MAC &mac,const Address &addr)
{
	Mutex::Lock _l(_lock);
	_remoteBridgeRoutes[mac] = addr;

	// Anti-DOS circuit breaker to prevent nodes from spamming us with absurd numbers of bridge routes
	while (_remoteBridgeRoutes.size() > ZT_MAX_BRIDGE_ROUTES) {
		Hashtable< Address,unsigned long > counts;
		Address maxAddr;
		unsigned long maxCount = 0;

		MAC *m = (MAC *)0;
		Address *a = (Address *)0;

		// Find the address responsible for the most entries
		{
			Hashtable<MAC,Address>::Iterator i(_remoteBridgeRoutes);
			while (i.next(m,a)) {
				const unsigned long c = ++counts[*a];
				if (c > maxCount) {
					maxCount = c;
					maxAddr = *a;
				}
			}
		}

		// Kill this address from our table, since it's most likely spamming us
		{
			Hashtable<MAC,Address>::Iterator i(_remoteBridgeRoutes);
			while (i.next(m,a)) {
				if (*a == maxAddr)
					_remoteBridgeRoutes.erase(*m);
			}
		}
	}
}

void Network::learnBridgedMulticastGroup(const MulticastGroup &mg,uint64_t now)
{
	Mutex::Lock _l(_lock);
	const unsigned long tmp = (unsigned long)_multicastGroupsBehindMe.size();
	_multicastGroupsBehindMe.set(mg,now);
	if (tmp != _multicastGroupsBehindMe.size())
		_sendUpdatesToMembers(&mg);
}

Membership::AddCredentialResult Network::addCredential(const CertificateOfMembership &com)
{
	if (com.networkId() != _id)
		return Membership::ADD_REJECTED;
	const Address a(com.issuedTo());
	Mutex::Lock _l(_lock);
	Membership &m = _membership(a);
	const Membership::AddCredentialResult result = m.addCredential(RR,_config,com);
	if ((result == Membership::ADD_ACCEPTED_NEW)||(result == Membership::ADD_ACCEPTED_REDUNDANT)) {
		m.pushCredentials(RR,RR->node->now(),a,_config,-1,false);
		RR->mc->addCredential(com,true);
	}
	return result;
}

Membership::AddCredentialResult Network::addCredential(const Address &sentFrom,const Revocation &rev)
{
	if (rev.networkId() != _id)
		return Membership::ADD_REJECTED;

	Mutex::Lock _l(_lock);
	Membership &m = _membership(rev.target());

	const Membership::AddCredentialResult result = m.addCredential(RR,_config,rev);

	if ((result == Membership::ADD_ACCEPTED_NEW)&&(rev.fastPropagate())) {
		Address *a = (Address *)0;
		Membership *m = (Membership *)0;
		Hashtable<Address,Membership>::Iterator i(_memberships);
		while (i.next(a,m)) {
			if ((*a != sentFrom)&&(*a != rev.signer())) {
				Packet outp(*a,RR->identity.address(),Packet::VERB_NETWORK_CREDENTIALS);
				outp.append((uint8_t)0x00); // no COM
				outp.append((uint16_t)0); // no capabilities
				outp.append((uint16_t)0); // no tags
				outp.append((uint16_t)1); // one revocation!
				rev.serialize(outp);
				RR->sw->send(outp,true);
			}
		}
	}

	return result;
}

void Network::destroy()
{
	Mutex::Lock _l(_lock);
	_destroyed = true;
}

ZT_VirtualNetworkStatus Network::_status() const
{
	// assumes _lock is locked
	if (_portError)
		return ZT_NETWORK_STATUS_PORT_ERROR;
	switch(_netconfFailure) {
		case NETCONF_FAILURE_ACCESS_DENIED:
			return ZT_NETWORK_STATUS_ACCESS_DENIED;
		case NETCONF_FAILURE_NOT_FOUND:
			return ZT_NETWORK_STATUS_NOT_FOUND;
		case NETCONF_FAILURE_NONE:
			return ((_config) ? ZT_NETWORK_STATUS_OK : ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION);
		default:
			return ZT_NETWORK_STATUS_PORT_ERROR;
	}
}

int Network::_setConfiguration(const NetworkConfig &nconf,bool saveToDisk)
{
	// assumes _lock is locked
	try {
		if ((nconf.issuedTo != RR->identity.address())||(nconf.networkId != _id))
			return 0;
		if (_config == nconf)
			return 1; // OK config, but duplicate of what we already have

		ZT_VirtualNetworkConfig ctmp;
		_config = nconf;
		_lastConfigUpdate = RR->node->now();
		_netconfFailure = NETCONF_FAILURE_NONE;
		_externalConfig(&ctmp);
		const bool oldPortInitialized = _portInitialized;
		_portInitialized = true;
		_portError = RR->node->configureVirtualNetworkPort(_id,&_uPtr,(oldPortInitialized) ? ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE : ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP,&ctmp);

		if (saveToDisk) {
			char n[64];
			Utils::snprintf(n,sizeof(n),"networks.d/%.16llx.conf",_id);
			Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> d;
			if (nconf.toDictionary(d,false))
				RR->node->dataStorePut(n,(const void *)d.data(),d.sizeBytes(),true);
		}

		return 2; // OK and configuration has changed
	} catch ( ... ) {
		TRACE("ignored invalid configuration for network %.16llx",(unsigned long long)_id);
	}
	return 0;
}

void Network::_externalConfig(ZT_VirtualNetworkConfig *ec) const
{
	// assumes _lock is locked
	ec->nwid = _id;
	ec->mac = _mac.toInt();
	if (_config)
		Utils::scopy(ec->name,sizeof(ec->name),_config.name);
	else ec->name[0] = (char)0;
	ec->status = _status();
	ec->type = (_config) ? (_config.isPrivate() ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC) : ZT_NETWORK_TYPE_PRIVATE;
	ec->mtu = ZT_IF_MTU;
	ec->physicalMtu = ZT_UDP_DEFAULT_PAYLOAD_MTU - (ZT_PACKET_IDX_PAYLOAD + 16);
	ec->dhcp = 0;
	std::vector<Address> ab(_config.activeBridges());
	ec->bridge = ((_config.allowPassiveBridging())||(std::find(ab.begin(),ab.end(),RR->identity.address()) != ab.end())) ? 1 : 0;
	ec->broadcastEnabled = (_config) ? (_config.enableBroadcast() ? 1 : 0) : 0;
	ec->portError = _portError;
	ec->netconfRevision = (_config) ? (unsigned long)_config.revision : 0;

	ec->assignedAddressCount = 0;
	for(unsigned int i=0;i<ZT_MAX_ZT_ASSIGNED_ADDRESSES;++i) {
		if (i < _config.staticIpCount) {
			memcpy(&(ec->assignedAddresses[i]),&(_config.staticIps[i]),sizeof(struct sockaddr_storage));
			++ec->assignedAddressCount;
		} else {
			memset(&(ec->assignedAddresses[i]),0,sizeof(struct sockaddr_storage));
		}
	}

	ec->routeCount = 0;
	for(unsigned int i=0;i<ZT_MAX_NETWORK_ROUTES;++i) {
		if (i < _config.routeCount) {
			memcpy(&(ec->routes[i]),&(_config.routes[i]),sizeof(ZT_VirtualNetworkRoute));
			++ec->routeCount;
		} else {
			memset(&(ec->routes[i]),0,sizeof(ZT_VirtualNetworkRoute));
		}
	}
}

void Network::_sendUpdatesToMembers(const MulticastGroup *const newMulticastGroup)
{
	// Assumes _lock is locked
	const uint64_t now = RR->node->now();

	std::vector<MulticastGroup> groups;
	if (newMulticastGroup)
		groups.push_back(*newMulticastGroup);
	else groups = _allMulticastGroups();

	if ((newMulticastGroup)||((now - _lastAnnouncedMulticastGroupsUpstream) >= ZT_MULTICAST_ANNOUNCE_PERIOD)) {
		if (!newMulticastGroup)
			_lastAnnouncedMulticastGroupsUpstream = now;

		// Announce multicast groups to upstream peers (roots, etc.) and also send
		// them our COM so that MULTICAST_GATHER can be authenticated properly.
		const std::vector<Address> upstreams(RR->topology->upstreamAddresses());
		for(std::vector<Address>::const_iterator a(upstreams.begin());a!=upstreams.end();++a) {
			if (_config.com) {
				Packet outp(*a,RR->identity.address(),Packet::VERB_NETWORK_CREDENTIALS);
				_config.com.serialize(outp);
				outp.append((uint8_t)0x00);
				RR->sw->send(outp,true);
			}
			_announceMulticastGroupsTo(*a,groups);
		}

		// Also announce to controller, and send COM to simplify and generalize behavior even though in theory it does not need it
		const Address c(controller());
		if ( (std::find(upstreams.begin(),upstreams.end(),c) == upstreams.end()) && (!_memberships.contains(c)) ) {
			if (_config.com) {
				Packet outp(c,RR->identity.address(),Packet::VERB_NETWORK_CREDENTIALS);
				_config.com.serialize(outp);
				outp.append((uint8_t)0x00);
				RR->sw->send(outp,true);
			}
			_announceMulticastGroupsTo(c,groups);
		}
	}

	// Make sure that all "network anchors" have Membership records so we will
	// push multicasts to them. Note that _membership() also does this but in a
	// piecemeal on-demand fashion.
	const std::vector<Address> anchors(_config.anchors());
	for(std::vector<Address>::const_iterator a(anchors.begin());a!=anchors.end();++a)
		_membership(*a);

	// Send credentials and multicast LIKEs to members, upstreams, and controller
	{
		Address *a = (Address *)0;
		Membership *m = (Membership *)0;
		Hashtable<Address,Membership>::Iterator i(_memberships);
		while (i.next(a,m)) {
			m->pushCredentials(RR,now,*a,_config,-1,false);
			if ( ((newMulticastGroup)||(m->shouldLikeMulticasts(now))) && (m->isAllowedOnNetwork(_config)) ) {
				if (!newMulticastGroup)
					m->likingMulticasts(now);
				_announceMulticastGroupsTo(*a,groups);
			}
		}
	}
}

void Network::_announceMulticastGroupsTo(const Address &peer,const std::vector<MulticastGroup> &allMulticastGroups)
{
	// Assumes _lock is locked
	Packet outp(peer,RR->identity.address(),Packet::VERB_MULTICAST_LIKE);

	for(std::vector<MulticastGroup>::const_iterator mg(allMulticastGroups.begin());mg!=allMulticastGroups.end();++mg) {
		if ((outp.size() + 24) >= ZT_PROTO_MAX_PACKET_LENGTH) {
			outp.compress();
			RR->sw->send(outp,true);
			outp.reset(peer,RR->identity.address(),Packet::VERB_MULTICAST_LIKE);
		}

		// network ID, MAC, ADI
		outp.append((uint64_t)_id);
		mg->mac().appendTo(outp);
		outp.append((uint32_t)mg->adi());
	}

	if (outp.size() > ZT_PROTO_MIN_PACKET_LENGTH) {
		outp.compress();
		RR->sw->send(outp,true);
	}
}

std::vector<MulticastGroup> Network::_allMulticastGroups() const
{
	// Assumes _lock is locked
	std::vector<MulticastGroup> mgs;
	mgs.reserve(_myMulticastGroups.size() + _multicastGroupsBehindMe.size() + 1);
	mgs.insert(mgs.end(),_myMulticastGroups.begin(),_myMulticastGroups.end());
	_multicastGroupsBehindMe.appendKeys(mgs);
	if ((_config)&&(_config.enableBroadcast()))
		mgs.push_back(Network::BROADCAST);
	std::sort(mgs.begin(),mgs.end());
	mgs.erase(std::unique(mgs.begin(),mgs.end()),mgs.end());
	return mgs;
}

Membership &Network::_membership(const Address &a)
{
	// assumes _lock is locked
	return _memberships[a];
}

} // namespace ZeroTier
