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

#include <stdint.h>

#include "Constants.hpp"
#include "Filter.hpp"
#include "InetAddress.hpp"

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

namespace ZeroTier {

bool Filter::run(
	const uint64_t nwid,
	const bool receiving,
	const Address &ztSource,
	const Address &ztDest,
	const MAC &macSource,
	const MAC &macDest,
	const uint8_t *frameData,
	const unsigned int frameLen,
	const unsigned int etherType,
	const unsigned int vlanId,
	const ZT_VirtualNetworkRule *rules,
	const unsigned int ruleCount,
	const uint32_t *tagKeys,
	const uint32_t *tagValues,
	const unsigned int tagCount,
	Address &sendCopyOfPacketTo)
{
	sendCopyOfPacketTo.zero();

	// For each set of rules we start by assuming that they match (since no constraints
	// yields a 'match all' rule).
	uint8_t thisSetMatches = 1;

	for(unsigned int rn=0;rn<ruleCount;++rn) {
		const ZT_VirtualNetworkRuleType rt = (ZT_VirtualNetworkRuleType)(rules[rn].t & 0x7f);
		uint8_t thisRuleMatches = 0;

		switch(rt) {
			// Actions end a set of ANDed rules
			case ZT_NETWORK_RULE_ACTION_DROP:
			case ZT_NETWORK_RULE_ACTION_ACCEPT:
			case ZT_NETWORK_RULE_ACTION_TEE:
			case ZT_NETWORK_RULE_ACTION_REDIRECT:
				if (thisSetMatches) {
					// This set did match, so perform action!
					if (rt != ZT_NETWORK_RULE_ACTION_DROP) {
						if ((rt == ZT_NETWORK_RULE_ACTION_TEE)||(rt == ZT_NETWORK_RULE_ACTION_REDIRECT)) {
							sendCopyOfPacketTo = rules[rn].v.zt;
							/*
							// Tee and redirect both want this frame copied to somewhere else.
							Packet outp(Address(rules[rn].v.zt),RR->identity.address(),Packet::VERB_EXT_FRAME);
							outp.append(nwid);
							outp.append((unsigned char)0x00); // TODO: should maybe include COM if needed
							macDest.appendTo(outp);
							macSource.appendTo(outp);
							outp.append((uint16_t)etherType);
							outp.append(frameData,frameLen);
							outp.compress();
							RR->sw->send(outp,true,nwid);
							*/
						}
						// For REDIRECT we will want to DROP at this node. For TEE we ACCEPT at this node but
						// also forward it along as we just did.
						return (rt != ZT_NETWORK_RULE_ACTION_REDIRECT);
					}
					return false;
				} else {
					// Otherwise start a new set, assuming that it will match
					//TRACE("[%u] %u previous set did not match, starting next",rn,(unsigned int)rt);
					thisSetMatches = 1;
				}
				continue;

			// A rule can consist of one or more MATCH criterion
			case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
				thisRuleMatches = (uint8_t)(rules[rn].v.zt == ztSource.toInt());
				break;
			case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS:
				thisRuleMatches = (uint8_t)(rules[rn].v.zt == ztDest.toInt());
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_ID:
				thisRuleMatches = (uint8_t)(rules[rn].v.vlanId == (uint16_t)vlanId);
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
				// NOT SUPPORTED YET
				thisRuleMatches = (uint8_t)(rules[rn].v.vlanPcp == 0);
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
				// NOT SUPPORTED YET
				thisRuleMatches = (uint8_t)(rules[rn].v.vlanDei == 0);
				break;
			case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
				thisRuleMatches = (uint8_t)(rules[rn].v.etherType == (uint16_t)etherType);
				break;
			case ZT_NETWORK_RULE_MATCH_MAC_SOURCE:
				thisRuleMatches = (uint8_t)(MAC(rules[rn].v.mac,6) == macSource);
				break;
			case ZT_NETWORK_RULE_MATCH_MAC_DEST:
				thisRuleMatches = (uint8_t)(MAC(rules[rn].v.mac,6) == macDest);
				break;
			case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
				if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)) {
					thisRuleMatches = (uint8_t)(InetAddress((const void *)&(rules[rn].v.ipv4.ip),4,rules[rn].v.ipv4.mask).containsAddress(InetAddress((const void *)(frameData + 12),4,0)));
				} else {
					thisRuleMatches = 0;
				}
				break;
			case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
				if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)) {
					thisRuleMatches = (uint8_t)(InetAddress((const void *)&(rules[rn].v.ipv4.ip),4,rules[rn].v.ipv4.mask).containsAddress(InetAddress((const void *)(frameData + 16),4,0)));
				} else {
					thisRuleMatches = 0;
				}
				break;
			case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
				if ((etherType == ZT_ETHERTYPE_IPV6)&&(frameLen >= 40)) {
					thisRuleMatches = (uint8_t)(InetAddress((const void *)rules[rn].v.ipv6.ip,16,rules[rn].v.ipv6.mask).containsAddress(InetAddress((const void *)(frameData + 8),16,0)));
				} else {
					thisRuleMatches = 0;
				}
				break;
			case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
				if ((etherType == ZT_ETHERTYPE_IPV6)&&(frameLen >= 40)) {
					thisRuleMatches = (uint8_t)(InetAddress((const void *)rules[rn].v.ipv6.ip,16,rules[rn].v.ipv6.mask).containsAddress(InetAddress((const void *)(frameData + 24),16,0)));
				} else {
					thisRuleMatches = 0;
				}
				break;
			case ZT_NETWORK_RULE_MATCH_IP_TOS:
				if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)) {
					thisRuleMatches = (uint8_t)(rules[rn].v.ipTos == ((frameData[1] & 0xfc) >> 2));
				} else if ((etherType == ZT_ETHERTYPE_IPV6)&&(frameLen >= 40)) {
					const uint8_t trafficClass = ((frameData[0] << 4) & 0xf0) | ((frameData[1] >> 4) & 0x0f);
					thisRuleMatches = (uint8_t)(rules[rn].v.ipTos == ((trafficClass & 0xfc) >> 2));
				} else {
					thisRuleMatches = 0;
				}
				break;
			case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
				if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)) {
					thisRuleMatches = (uint8_t)(rules[rn].v.ipProtocol == frameData[9]);
				} else if (etherType == ZT_ETHERTYPE_IPV6) {
					unsigned int pos = 0,proto = 0;
					if (_ipv6GetPayload(frameData,frameLen,pos,proto)) {
						thisRuleMatches = (uint8_t)(rules[rn].v.ipProtocol == (uint8_t)proto);
					} else {
						thisRuleMatches = 0;
					}
				} else {
					thisRuleMatches = 0;
				}
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
								unsigned int pos = headerLen + (((unsigned int)(rt == ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE)) << 1); // headerLen or +2 for destination port
								p = (int)frameData[pos++] << 8;
								p |= (int)frameData[pos];
							}
							break;
					}
					thisRuleMatches = (p > 0) ? (uint8_t)((p >= (int)rules[rn].v.port[0])&&(p <= (int)rules[rn].v.port[1])) : (uint8_t)0;
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
									p = (int)frameData[pos++] << 8;
									p |= (int)frameData[pos];
								}
								break;
						}
						thisRuleMatches = (p > 0) ? (uint8_t)((p >= (int)rules[rn].v.port[0])&&(p <= (int)rules[rn].v.port[1])) : (uint8_t)0;
					} else {
						thisRuleMatches = 0;
					}
				} else {
					thisRuleMatches = 0;
				}
				break;
			case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS: {
				uint64_t cf = (receiving) ? ZT_RULE_PACKET_CHARACTERISTICS_0_INBOUND : 0ULL;
				thisRuleMatches = (uint8_t)((cf & rules[rn].v.characteristics[0]) == rules[rn].v.characteristics[1]);
			}	break;
			case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
				thisRuleMatches = (uint8_t)((frameLen >= (unsigned int)rules[rn].v.frameSize[0])&&(frameLen <= (unsigned int)rules[rn].v.frameSize[1]));
				break;
			case ZT_NETWORK_RULE_MATCH_TAG_VALUE_RANGE:
			case ZT_NETWORK_RULE_MATCH_TAG_VALUE_BITS_ALL:
			case ZT_NETWORK_RULE_MATCH_TAG_VALUE_BITS_ANY:
				for(unsigned int i=0;i<tagCount;++i) { // sequential scan is probably fastest since this is going to be <64 entries (usually only one or two)
					if (tagKeys[i] == rules[rn].v.tag.id) {
						if (rt == ZT_NETWORK_RULE_MATCH_TAG_VALUE_RANGE) {
							thisRuleMatches = (uint8_t)((tagValues[i] >= rules[rn].v.tag.value[0])&&(tagValues[i] <= rules[rn].v.tag.value[1]));
						} else if (rt == ZT_NETWORK_RULE_MATCH_TAG_VALUE_BITS_ALL) {
							thisRuleMatches = (uint8_t)((tagValues[i] & rules[rn].v.tag.value[0]) == rules[rn].v.tag.value[0]);
						} else if (rt == ZT_NETWORK_RULE_MATCH_TAG_VALUE_BITS_ANY) {
							thisRuleMatches = (uint8_t)((tagValues[i] & rules[rn].v.tag.value[0]) != 0);
						}
						break;
					}
				}
				break;
		}

		// thisSetMatches remains true if the current rule matched... or does NOT match if not bit (0x80) is 1
		thisSetMatches &= (thisRuleMatches ^ ((rules[rn].t & 0x80) >> 7));

		//TRACE("[%u] %u result==%u set==%u",rn,(unsigned int)rt,(unsigned int)thisRuleMatches,(unsigned int)thisSetMatches);
	}

	return false; // no matches, no rules, default action is therefore DROP
}

} // namespace ZeroTier
