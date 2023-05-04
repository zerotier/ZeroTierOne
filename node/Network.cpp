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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../include/ZeroTierDebug.h"

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
#include "Trace.hpp"
#include "Metrics.hpp"

#include <set>

namespace ZeroTier {

namespace {

// Returns true if packet appears valid; pos and proto will be set
static inline bool _ipv6GetPayload(const uint8_t *frameData,unsigned int frameLen,unsigned int &pos,unsigned int &proto)
{
	if (frameLen < 40) {
		return false;
	}
	pos = 40;
	proto = frameData[6];
	while (pos <= frameLen) {
		switch(proto) {
			case 0: // hop-by-hop options
			case 43: // routing
			case 60: // destination options
			case 135: // mobility options
				if ((pos + 8) > frameLen) {
					return false; // invalid!
				}
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
	Trace::RuleResultLog &rrl,
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
	bool &ccWatch, // MUTABLE -- set to true for WATCH target as opposed to normal TEE
	uint8_t &qosBucket) // MUTABLE -- set to the value of the argument provided to PRIORITY
{
	// Set to true if we are a TEE/REDIRECT/WATCH target
	bool superAccept = false;

	// The default match state for each set of entries starts as 'true' since an
	// ACTION with no MATCH entries preceding it is always taken.
	uint8_t thisSetMatches = 1;

	rrl.clear();

	for(unsigned int rn=0;rn<ruleCount;++rn) {
		const ZT_VirtualNetworkRuleType rt = (ZT_VirtualNetworkRuleType)(rules[rn].t & 0x3f);

		// First check if this is an ACTION
		if ((unsigned int)rt <= (unsigned int)ZT_NETWORK_RULE_ACTION__MAX_ID) {
			if (thisSetMatches) {
				switch(rt) {
					case ZT_NETWORK_RULE_ACTION_PRIORITY:
						qosBucket = (rules[rn].v.qosBucket >= 0 || rules[rn].v.qosBucket <= 8) ? rules[rn].v.qosBucket : 4; // 4 = default bucket (no priority)
						return DOZTFILTER_ACCEPT;

					case ZT_NETWORK_RULE_ACTION_DROP:
						return DOZTFILTER_DROP;

					case ZT_NETWORK_RULE_ACTION_ACCEPT:
						return (superAccept ? DOZTFILTER_SUPER_ACCEPT : DOZTFILTER_ACCEPT); // match, accept packet

					// These are initially handled together since preliminary logic is common
					case ZT_NETWORK_RULE_ACTION_TEE:
					case ZT_NETWORK_RULE_ACTION_WATCH:
					case ZT_NETWORK_RULE_ACTION_REDIRECT:	{
						const Address fwdAddr(rules[rn].v.fwd.address);
						if (fwdAddr == ztSource) {
							// Skip as no-op since source is target
						} else if (fwdAddr == RR->identity.address()) {
							if (inbound) {
								return DOZTFILTER_SUPER_ACCEPT;
							} else {
							}
						} else if (fwdAddr == ztDest) {
						} else {
							if (rt == ZT_NETWORK_RULE_ACTION_REDIRECT) {
								ztDest = fwdAddr;
								return DOZTFILTER_REDIRECT;
							} else {
								cc = fwdAddr;
								ccLength = (rules[rn].v.fwd.length != 0) ? ((frameLen < (unsigned int)rules[rn].v.fwd.length) ? frameLen : (unsigned int)rules[rn].v.fwd.length) : frameLen;
								ccWatch = (rt == ZT_NETWORK_RULE_ACTION_WATCH);
							}
						}
					}	continue;

					case ZT_NETWORK_RULE_ACTION_BREAK:
						return DOZTFILTER_NO_MATCH;

					// Unrecognized ACTIONs are ignored as no-ops
					default:
						continue;
				}
			} else {
				// If this is an incoming packet and we are a TEE or REDIRECT target, we should
				// super-accept if we accept at all. This will cause us to accept redirected or
				// tee'd packets in spite of MAC and ZT addressing checks.
				if (inbound) {
					switch(rt) {
						case ZT_NETWORK_RULE_ACTION_TEE:
						case ZT_NETWORK_RULE_ACTION_WATCH:
						case ZT_NETWORK_RULE_ACTION_REDIRECT:
							if (RR->identity.address() == rules[rn].v.fwd.address) {
								superAccept = true;
							}
							break;
						default:
							break;
					}
				}

				thisSetMatches = 1; // reset to default true for next batch of entries
				continue;
			}
		}

		// Circuit breaker: no need to evaluate an AND if the set's match state
		// is currently false since anything AND false is false.
		if ((!thisSetMatches)&&(!(rules[rn].t & 0x40))) {
			rrl.logSkipped(rn,thisSetMatches);
			continue;
		}

		// If this was not an ACTION evaluate next MATCH and update thisSetMatches with (AND [result])
		uint8_t thisRuleMatches = 0;
		uint64_t ownershipVerificationMask = 1; // this magic value means it hasn't been computed yet -- this is done lazily the first time it's needed
		switch(rt) {
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
					const uint8_t tosMasked = frameData[1] & rules[rn].v.ipTos.mask;
					thisRuleMatches = (uint8_t)((tosMasked >= rules[rn].v.ipTos.value[0])&&(tosMasked <= rules[rn].v.ipTos.value[1]));
				} else if ((etherType == ZT_ETHERTYPE_IPV6)&&(frameLen >= 40)) {
					const uint8_t tosMasked = (((frameData[0] << 4) & 0xf0) | ((frameData[1] >> 4) & 0x0f)) & rules[rn].v.ipTos.mask;
					thisRuleMatches = (uint8_t)((tosMasked >= rules[rn].v.ipTos.value[0])&&(tosMasked <= rules[rn].v.ipTos.value[1]));
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
			case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
				thisRuleMatches = (uint8_t)(rules[rn].v.etherType == (uint16_t)etherType);
				break;
			case ZT_NETWORK_RULE_MATCH_ICMP:
				if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)) {
					if (frameData[9] == 0x01) { // IP protocol == ICMP
						const unsigned int ihl = (frameData[0] & 0xf) * 4;
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
						} else {
							thisRuleMatches = 0;
						}
					} else {
						thisRuleMatches = 0;
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
						} else {
							thisRuleMatches = 0;
						}
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
								unsigned int pos = headerLen + ((rt == ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE) ? 2 : 0);
								p = (int)frameData[pos++] << 8;
								p |= (int)frameData[pos];
							}
							break;
					}

					thisRuleMatches = (p >= 0) ? (uint8_t)((p >= (int)rules[rn].v.port[0])&&(p <= (int)rules[rn].v.port[1])) : (uint8_t)0;
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
									if (rt == ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE) {
										pos += 2;
									}
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
				uint64_t cf = (inbound) ? ZT_RULE_PACKET_CHARACTERISTICS_INBOUND : 0ULL;
				if (macDest.isMulticast()) {
					cf |= ZT_RULE_PACKET_CHARACTERISTICS_MULTICAST;
				}
				if (macDest.isBroadcast()) {
					cf |= ZT_RULE_PACKET_CHARACTERISTICS_BROADCAST;
				}
				if (ownershipVerificationMask == 1) {
					ownershipVerificationMask = 0;
					InetAddress src;
					if ((etherType == ZT_ETHERTYPE_IPV4)&&(frameLen >= 20)) {
						src.set((const void *)(frameData + 12),4,0);
					} else if ((etherType == ZT_ETHERTYPE_IPV6)&&(frameLen >= 40)) {
						// IPv6 NDP requires special handling, since the src and dest IPs in the packet are empty or link-local.
						if ( (frameLen >= (40 + 8 + 16)) && (frameData[6] == 0x3a) && ((frameData[40] == 0x87)||(frameData[40] == 0x88)) ) {
							if (frameData[40] == 0x87) {
								// Neighbor solicitations contain no reliable source address, so we implement a small
								// hack by considering them authenticated. Otherwise you would pretty much have to do
								// this manually in the rule set for IPv6 to work at all.
								ownershipVerificationMask |= ZT_RULE_PACKET_CHARACTERISTICS_SENDER_IP_AUTHENTICATED;
							} else {
								// Neighbor advertisements on the other hand can absolutely be authenticated.
								src.set((const void *)(frameData + 40 + 8),16,0);
							}
						} else {
							// Other IPv6 packets can be handled normally
							src.set((const void *)(frameData + 8),16,0);
						}
					} else if ((etherType == ZT_ETHERTYPE_ARP)&&(frameLen >= 28)) {
						src.set((const void *)(frameData + 14),4,0);
					}
					if (inbound) {
						if (membership) {
							if ((src)&&(membership->hasCertificateOfOwnershipFor<InetAddress>(nconf,src))) {
								ownershipVerificationMask |= ZT_RULE_PACKET_CHARACTERISTICS_SENDER_IP_AUTHENTICATED;
							}
							if (membership->hasCertificateOfOwnershipFor<MAC>(nconf,macSource)) {
								ownershipVerificationMask |= ZT_RULE_PACKET_CHARACTERISTICS_SENDER_MAC_AUTHENTICATED;
							}
						}
					} else {
						for(unsigned int i=0;i<nconf.certificateOfOwnershipCount;++i) {
							if ((src)&&(nconf.certificatesOfOwnership[i].owns(src))) {
								ownershipVerificationMask |= ZT_RULE_PACKET_CHARACTERISTICS_SENDER_IP_AUTHENTICATED;
							}
							if (nconf.certificatesOfOwnership[i].owns(macSource)) {
								ownershipVerificationMask |= ZT_RULE_PACKET_CHARACTERISTICS_SENDER_MAC_AUTHENTICATED;
							}
						}
					}
				}
				cf |= ownershipVerificationMask;
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
				thisRuleMatches = (uint8_t)((cf & rules[rn].v.characteristics) != 0);
			}	break;
			case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
				thisRuleMatches = (uint8_t)((frameLen >= (unsigned int)rules[rn].v.frameSize[0])&&(frameLen <= (unsigned int)rules[rn].v.frameSize[1]));
				break;
			case ZT_NETWORK_RULE_MATCH_RANDOM:
				thisRuleMatches = (uint8_t)((uint32_t)(RR->node->prng() & 0xffffffffULL) <= rules[rn].v.randomProbability);
				break;
			case ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR:
			case ZT_NETWORK_RULE_MATCH_TAGS_EQUAL: {
				const Tag *const localTag = std::lower_bound(&(nconf.tags[0]),&(nconf.tags[nconf.tagCount]),rules[rn].v.tag.id,Tag::IdComparePredicate());
				if ((localTag != &(nconf.tags[nconf.tagCount]))&&(localTag->id() == rules[rn].v.tag.id)) {
					const Tag *const remoteTag = ((membership) ? membership->getTag(nconf,rules[rn].v.tag.id) : (const Tag *)0);
					if (remoteTag) {
						const uint32_t ltv = localTag->value();
						const uint32_t rtv = remoteTag->value();
						if (rt == ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE) {
							const uint32_t diff = (ltv > rtv) ? (ltv - rtv) : (rtv - ltv);
							thisRuleMatches = (uint8_t)(diff <= rules[rn].v.tag.value);
						} else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND) {
							thisRuleMatches = (uint8_t)((ltv & rtv) == rules[rn].v.tag.value);
						} else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR) {
							thisRuleMatches = (uint8_t)((ltv | rtv) == rules[rn].v.tag.value);
						} else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR) {
							thisRuleMatches = (uint8_t)((ltv ^ rtv) == rules[rn].v.tag.value);
						} else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_EQUAL) {
							thisRuleMatches = (uint8_t)((ltv == rules[rn].v.tag.value)&&(rtv == rules[rn].v.tag.value));
						} else { // sanity check, can't really happen
							thisRuleMatches = 0;
						}
					} else {
						if ((inbound)&&(!superAccept)) {
							thisRuleMatches = 0;
						} else {
							// Outbound side is not strict since if we have to match both tags and
							// we are sending a first packet to a recipient, we probably do not know
							// about their tags yet. They will filter on inbound and we will filter
							// once we get their tag. If we are a tee/redirect target we are also
							// not strict since we likely do not have these tags.
							thisRuleMatches = 1;
						}
					}
				} else {
					thisRuleMatches = 0;
				}
			}	break;
			case ZT_NETWORK_RULE_MATCH_TAG_SENDER:
			case ZT_NETWORK_RULE_MATCH_TAG_RECEIVER: {
				if (superAccept) {
					thisRuleMatches = 1;
				} else if ( ((rt == ZT_NETWORK_RULE_MATCH_TAG_SENDER)&&(inbound)) || ((rt == ZT_NETWORK_RULE_MATCH_TAG_RECEIVER)&&(!inbound)) ) {
					const Tag *const remoteTag = ((membership) ? membership->getTag(nconf,rules[rn].v.tag.id) : (const Tag *)0);
					if (remoteTag) {
						thisRuleMatches = (uint8_t)(remoteTag->value() == rules[rn].v.tag.value);
					} else {
						if (rt == ZT_NETWORK_RULE_MATCH_TAG_RECEIVER) {
							// If we are checking the receiver and this is an outbound packet, we
							// can't be strict since we may not yet know the receiver's tag.
							thisRuleMatches = 1;
						} else {
							thisRuleMatches = 0;
						}
					}
				} else { // sender and outbound or receiver and inbound
					const Tag *const localTag = std::lower_bound(&(nconf.tags[0]),&(nconf.tags[nconf.tagCount]),rules[rn].v.tag.id,Tag::IdComparePredicate());
					if ((localTag != &(nconf.tags[nconf.tagCount]))&&(localTag->id() == rules[rn].v.tag.id)) {
						thisRuleMatches = (uint8_t)(localTag->value() == rules[rn].v.tag.value);
					} else {
						thisRuleMatches = 0;
					}
				}
			}	break;
			case ZT_NETWORK_RULE_MATCH_INTEGER_RANGE: {
				uint64_t integer = 0;
				const unsigned int bits = (rules[rn].v.intRange.format & 63) + 1;
				const unsigned int bytes = ((bits + 8 - 1) / 8); // integer ceiling of division by 8
				if ((rules[rn].v.intRange.format & 0x80) == 0) {
					// Big-endian
					unsigned int idx = rules[rn].v.intRange.idx + (8 - bytes);
					const unsigned int eof = idx + bytes;
					if (eof <= frameLen) {
						while (idx < eof) {
							integer <<= 8;
							integer |= frameData[idx++];
						}
					}
					integer &= 0xffffffffffffffffULL >> (64 - bits);
				} else {
					// Little-endian
					unsigned int idx = rules[rn].v.intRange.idx;
					const unsigned int eof = idx + bytes;
					if (eof <= frameLen) {
						while (idx < eof) {
							integer >>= 8;
							integer |= ((uint64_t)frameData[idx++]) << 56;
						}
					}
					integer >>= (64 - bits);
				}
				thisRuleMatches = (uint8_t)((integer >= rules[rn].v.intRange.start)&&(integer <= (rules[rn].v.intRange.start + (uint64_t)rules[rn].v.intRange.end)));
			}	break;

			// The result of an unsupported MATCH is configurable at the network
			// level via a flag.
			default:
				thisRuleMatches = (uint8_t)((nconf.flags & ZT_NETWORKCONFIG_FLAG_RULES_RESULT_OF_UNSUPPORTED_MATCH) != 0);
				break;
		}

		rrl.log(rn,thisRuleMatches,thisSetMatches);

		if ((rules[rn].t & 0x40)) {
			thisSetMatches |= (thisRuleMatches ^ ((rules[rn].t >> 7) & 1));
		} else {
			thisSetMatches &= (thisRuleMatches ^ ((rules[rn].t >> 7) & 1));
		}
	}

	return DOZTFILTER_NO_MATCH;
}

} // anonymous namespace

const ZeroTier::MulticastGroup Network::BROADCAST(ZeroTier::MAC(0xffffffffffffULL),0);

Network::Network(const RuntimeEnvironment *renv,void *tPtr,uint64_t nwid,void *uptr,const NetworkConfig *nconf) :
	RR(renv),
	_uPtr(uptr),
	_id(nwid),
	_nwidStr(OSUtils::networkIDStr(nwid)),
	_lastAnnouncedMulticastGroupsUpstream(0),
	_mac(renv->identity.address(),nwid),
	_portInitialized(false),
	_lastConfigUpdate(0),
	_destroyed(false),
	_netconfFailure(NETCONF_FAILURE_NONE),
	_portError(0),
	_num_multicast_groups{Metrics::network_num_multicast_groups.Add({{"network_id", _nwidStr}})},
	_incoming_packets_accepted{Metrics::network_packets.Add({{"direction","rx"},{"network_id", _nwidStr},{"accepted","yes"}})},
	_incoming_packets_dropped{Metrics::network_packets.Add({{"direction","rx"},{"network_id", _nwidStr},{"accepted","no"}})},
	_outgoing_packets_accepted{Metrics::network_packets.Add({{"direction","tx"},{"network_id", _nwidStr},{"accepted","yes"}})},
	_outgoing_packets_dropped{Metrics::network_packets.Add({{"direction","tx"},{"network_id", _nwidStr},{"accepted","no"}})}
{
	for(int i=0;i<ZT_NETWORK_MAX_INCOMING_UPDATES;++i) {
		_incomingConfigChunks[i].ts = 0;
	}

	if (nconf) {
		this->setConfiguration(tPtr,*nconf,false);
		_lastConfigUpdate = 0; // still want to re-request since it's likely outdated
	} else {
		uint64_t tmp[2];
		tmp[0] = nwid;
		tmp[1] = 0;

		bool got = false;
		Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> *dict = new Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY>();
		try {
			int n = RR->node->stateObjectGet(tPtr,ZT_STATE_OBJECT_NETWORK_CONFIG,tmp,dict->unsafeData(),ZT_NETWORKCONFIG_DICT_CAPACITY - 1);
			if (n > 1) {
				NetworkConfig *nconf = new NetworkConfig();
				try {
					if (nconf->fromDictionary(*dict)) {
						this->setConfiguration(tPtr,*nconf,false);
						_lastConfigUpdate = 0; // still want to re-request an update since it's likely outdated
						got = true;
					}
				} catch ( ... ) {}
				delete nconf;
			}
		} catch ( ... ) {}
		delete dict;

		if (!got) {
			RR->node->stateObjectPut(tPtr,ZT_STATE_OBJECT_NETWORK_CONFIG,tmp,"\n",1);
		}
	}

	if (!_portInitialized) {
		ZT_VirtualNetworkConfig ctmp;
		memset(&ctmp, 0, sizeof(ZT_VirtualNetworkConfig));
		_externalConfig(&ctmp);
		_portError = RR->node->configureVirtualNetworkPort(tPtr,_id,&_uPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP,&ctmp);
		_portInitialized = true;
	}

	Metrics::network_num_joined++;
}

Network::~Network()
{
	ZT_VirtualNetworkConfig ctmp;
	_externalConfig(&ctmp);
	Metrics::network_num_joined--;
	if (_destroyed) {
		// This is done in Node::leave() so we can pass tPtr properly
		//RR->node->configureVirtualNetworkPort((void *)0,_id,&_uPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY,&ctmp);
	} else {
		RR->node->configureVirtualNetworkPort((void *)0,_id,&_uPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN,&ctmp);
	}
}

bool Network::filterOutgoingPacket(
	void *tPtr,
	const bool noTee,
	const Address &ztSource,
	const Address &ztDest,
	const MAC &macSource,
	const MAC &macDest,
	const uint8_t *frameData,
	const unsigned int frameLen,
	const unsigned int etherType,
	const unsigned int vlanId,
	uint8_t &qosBucket)
{
	Address ztFinalDest(ztDest);
	int localCapabilityIndex = -1;
	int accept = 0;
	Trace::RuleResultLog rrl,crrl;
	Address cc;
	unsigned int ccLength = 0;
	bool ccWatch = false;

	Mutex::Lock _l(_lock);

	Membership *const membership = (ztDest) ? _memberships.get(ztDest) : (Membership *)0;

	switch(_doZtFilter(RR,rrl,_config,membership,false,ztSource,ztFinalDest,macSource,macDest,frameData,frameLen,etherType,vlanId,_config.rules,_config.ruleCount,cc,ccLength,ccWatch,qosBucket)) {

		case DOZTFILTER_NO_MATCH: {
			for(unsigned int c=0;c<_config.capabilityCount;++c) {
				ztFinalDest = ztDest; // sanity check, shouldn't be possible if there was no match
				Address cc2;
				unsigned int ccLength2 = 0;
				bool ccWatch2 = false;
				switch (_doZtFilter(RR,crrl,_config,membership,false,ztSource,ztFinalDest,macSource,macDest,frameData,frameLen,etherType,vlanId,_config.capabilities[c].rules(),_config.capabilities[c].ruleCount(),cc2,ccLength2,ccWatch2,qosBucket)) {
					case DOZTFILTER_NO_MATCH:
					case DOZTFILTER_DROP: // explicit DROP in a capability just terminates its evaluation and is an anti-pattern
						break;

					case DOZTFILTER_REDIRECT: // interpreted as ACCEPT but ztFinalDest will have been changed in _doZtFilter()
					case DOZTFILTER_ACCEPT:
					case DOZTFILTER_SUPER_ACCEPT: // no difference in behavior on outbound side in capabilities
						localCapabilityIndex = (int)c;
						accept = 1;

						if ((!noTee)&&(cc2)) {
							Packet outp(cc2,RR->identity.address(),Packet::VERB_EXT_FRAME);
							outp.append(_id);
							outp.append((uint8_t)(ccWatch2 ? 0x16 : 0x02));
							macDest.appendTo(outp);
							macSource.appendTo(outp);
							outp.append((uint16_t)etherType);
							outp.append(frameData,ccLength2);
							outp.compress();
							RR->sw->send(tPtr,outp,true);
						}

						break;
				}
				if (accept) {
					break;
				}
			}
		}	break;

		case DOZTFILTER_DROP:
			if (_config.remoteTraceTarget) {
				RR->t->networkFilter(tPtr,*this,rrl,(Trace::RuleResultLog *)0,(Capability *)0,ztSource,ztDest,macSource,macDest,frameData,frameLen,etherType,vlanId,noTee,false,0);
			}
			return false;

		case DOZTFILTER_REDIRECT: // interpreted as ACCEPT but ztFinalDest will have been changed in _doZtFilter()
		case DOZTFILTER_ACCEPT:
			accept = 1;
			break;

		case DOZTFILTER_SUPER_ACCEPT:
			accept = 2;
			break;
	}

	if (accept) {
		_outgoing_packets_accepted++;
		if ((!noTee)&&(cc)) {
			Packet outp(cc,RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(_id);
			outp.append((uint8_t)(ccWatch ? 0x16 : 0x02));
			macDest.appendTo(outp);
			macSource.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(frameData,ccLength);
			outp.compress();
			RR->sw->send(tPtr,outp,true);
		}

		if ((ztDest != ztFinalDest)&&(ztFinalDest)) {
			Packet outp(ztFinalDest,RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(_id);
			outp.append((uint8_t)0x04);
			macDest.appendTo(outp);
			macSource.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(frameData,frameLen);
			outp.compress();
			RR->sw->send(tPtr,outp,true);

			if (_config.remoteTraceTarget) {
				RR->t->networkFilter(tPtr,*this,rrl,(localCapabilityIndex >= 0) ? &crrl : (Trace::RuleResultLog *)0,(localCapabilityIndex >= 0) ? &(_config.capabilities[localCapabilityIndex]) : (Capability *)0,ztSource,ztDest,macSource,macDest,frameData,frameLen,etherType,vlanId,noTee,false,0);
			}
			return false; // DROP locally, since we redirected
		} else {
			if (_config.remoteTraceTarget) {
				RR->t->networkFilter(tPtr,*this,rrl,(localCapabilityIndex >= 0) ? &crrl : (Trace::RuleResultLog *)0,(localCapabilityIndex >= 0) ? &(_config.capabilities[localCapabilityIndex]) : (Capability *)0,ztSource,ztDest,macSource,macDest,frameData,frameLen,etherType,vlanId,noTee,false,1);
			}
			return true;
		}
	} else {
		_outgoing_packets_dropped++;
		if (_config.remoteTraceTarget) {
			RR->t->networkFilter(tPtr,*this,rrl,(localCapabilityIndex >= 0) ? &crrl : (Trace::RuleResultLog *)0,(localCapabilityIndex >= 0) ? &(_config.capabilities[localCapabilityIndex]) : (Capability *)0,ztSource,ztDest,macSource,macDest,frameData,frameLen,etherType,vlanId,noTee,false,0);
		}
		return false;
	}
}

int Network::filterIncomingPacket(
	void *tPtr,
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
	Trace::RuleResultLog rrl,crrl;
	int accept = 0;
	Address cc;
	unsigned int ccLength = 0;
	bool ccWatch = false;
	const Capability *c = (Capability *)0;

	uint8_t qosBucket = 255; // For incoming packets this is a dummy value

	Mutex::Lock _l(_lock);

	Membership &membership = _membership(sourcePeer->address());

	switch (_doZtFilter(RR,rrl,_config,&membership,true,sourcePeer->address(),ztFinalDest,macSource,macDest,frameData,frameLen,etherType,vlanId,_config.rules,_config.ruleCount,cc,ccLength,ccWatch,qosBucket)) {

		case DOZTFILTER_NO_MATCH: {
			Membership::CapabilityIterator mci(membership,_config);
			while ((c = mci.next())) {
				ztFinalDest = ztDest; // sanity check, should be unmodified if there was no match
				Address cc2;
				unsigned int ccLength2 = 0;
				bool ccWatch2 = false;
				switch(_doZtFilter(RR,crrl,_config,&membership,true,sourcePeer->address(),ztFinalDest,macSource,macDest,frameData,frameLen,etherType,vlanId,c->rules(),c->ruleCount(),cc2,ccLength2,ccWatch2,qosBucket)) {
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
						Packet outp(cc2,RR->identity.address(),Packet::VERB_EXT_FRAME);
						outp.append(_id);
						outp.append((uint8_t)(ccWatch2 ? 0x1c : 0x08));
						macDest.appendTo(outp);
						macSource.appendTo(outp);
						outp.append((uint16_t)etherType);
						outp.append(frameData,ccLength2);
						outp.compress();
						RR->sw->send(tPtr,outp,true);
					}
					break;
				}
			}
		}	break;

		case DOZTFILTER_DROP:
			if (_config.remoteTraceTarget) {
				RR->t->networkFilter(tPtr,*this,rrl,(Trace::RuleResultLog *)0,(Capability *)0,sourcePeer->address(),ztDest,macSource,macDest,frameData,frameLen,etherType,vlanId,false,true,0);
			}
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
		_incoming_packets_accepted++;
		if (cc) {
			Packet outp(cc,RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(_id);
			outp.append((uint8_t)(ccWatch ? 0x1c : 0x08));
			macDest.appendTo(outp);
			macSource.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(frameData,ccLength);
			outp.compress();
			RR->sw->send(tPtr,outp,true);
		}

		if ((ztDest != ztFinalDest)&&(ztFinalDest)) {
			Packet outp(ztFinalDest,RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(_id);
			outp.append((uint8_t)0x0a);
			macDest.appendTo(outp);
			macSource.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(frameData,frameLen);
			outp.compress();
			RR->sw->send(tPtr,outp,true);

			if (_config.remoteTraceTarget) {
				RR->t->networkFilter(tPtr,*this,rrl,(c) ? &crrl : (Trace::RuleResultLog *)0,c,sourcePeer->address(),ztDest,macSource,macDest,frameData,frameLen,etherType,vlanId,false,true,0);
			}
			return 0; // DROP locally, since we redirected
		}
	} else {
		_incoming_packets_dropped++;
	}

	if (_config.remoteTraceTarget) {
		RR->t->networkFilter(tPtr,*this,rrl,(c) ? &crrl : (Trace::RuleResultLog *)0,c,sourcePeer->address(),ztDest,macSource,macDest,frameData,frameLen,etherType,vlanId,false,true,accept);
	}
	return accept;
}

bool Network::subscribedToMulticastGroup(const MulticastGroup &mg,bool includeBridgedGroups) const
{
	Mutex::Lock _l(_lock);
	if (std::binary_search(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg)) {
		return true;
	} else if (includeBridgedGroups) {
		return _multicastGroupsBehindMe.contains(mg);
	}
	return false;
}

void Network::multicastSubscribe(void *tPtr,const MulticastGroup &mg)
{
	Mutex::Lock _l(_lock);
	if (!std::binary_search(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg)) {
		_myMulticastGroups.insert(std::upper_bound(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg),mg);
		_sendUpdatesToMembers(tPtr,&mg);
		_num_multicast_groups++;
	}
}

void Network::multicastUnsubscribe(const MulticastGroup &mg)
{
	Mutex::Lock _l(_lock);
	std::vector<MulticastGroup>::iterator i(std::lower_bound(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg));
	if ( (i != _myMulticastGroups.end()) && (*i == mg) ) {
		_myMulticastGroups.erase(i);
		_num_multicast_groups--;
	}
}

uint64_t Network::handleConfigChunk(void *tPtr,const uint64_t packetId,const Address &source,const Buffer<ZT_PROTO_MAX_PACKET_LENGTH> &chunk,unsigned int ptr)
{
	if (_destroyed) {
		return 0;
	}

	const unsigned int start = ptr;

	ptr += 8; // skip network ID, which is already obviously known
	const unsigned int chunkLen = chunk.at<uint16_t>(ptr);
	ptr += 2;
	const void *chunkData = chunk.field(ptr,chunkLen);
	ptr += chunkLen;

	NetworkConfig *nc = (NetworkConfig *)0;
	uint64_t configUpdateId;
	{
		Mutex::Lock _l(_lock);

		_IncomingConfigChunk *c = (_IncomingConfigChunk *)0;
		uint64_t chunkId = 0;
		unsigned long totalLength,chunkIndex;
		if (ptr < chunk.size()) {
			const bool fastPropagate = ((chunk[ptr++] & 0x01) != 0);
			configUpdateId = chunk.at<uint64_t>(ptr);
			ptr += 8;
			totalLength = chunk.at<uint32_t>(ptr);
			ptr += 4;
			chunkIndex = chunk.at<uint32_t>(ptr);
			ptr += 4;

			if (((chunkIndex + chunkLen) > totalLength)||(totalLength >= ZT_NETWORKCONFIG_DICT_CAPACITY)) { // >= since we need room for a null at the end
				return 0;
			}
			if ((chunk[ptr] != 1)||(chunk.at<uint16_t>(ptr + 1) != ZT_C25519_SIGNATURE_LEN)) {
				return 0;
			}
			const uint8_t *sig = reinterpret_cast<const uint8_t *>(chunk.field(ptr + 3,ZT_C25519_SIGNATURE_LEN));

			// We can use the signature, which is unique per chunk, to get a per-chunk ID for local deduplication use
			for(unsigned int i=0;i<16;++i) {
				reinterpret_cast<uint8_t *>(&chunkId)[i & 7] ^= sig[i];
			}

			// Find existing or new slot for this update and check if this is a duplicate chunk
			for(int i=0;i<ZT_NETWORK_MAX_INCOMING_UPDATES;++i) {
				if (_incomingConfigChunks[i].updateId == configUpdateId) {
					c = &(_incomingConfigChunks[i]);

					for(unsigned long j=0;j<c->haveChunks;++j) {
						if (c->haveChunkIds[j] == chunkId) {
							return 0;
						}
					}

					break;
				} else if ((!c)||(_incomingConfigChunks[i].ts < c->ts)) {
					c = &(_incomingConfigChunks[i]);
				}
			}

			// If it's not a duplicate, check chunk signature
			const Identity controllerId(RR->topology->getIdentity(tPtr,controller()));
			if (!controllerId) { // we should always have the controller identity by now, otherwise how would we have queried it the first time?
				return 0;
			}
			if (!controllerId.verify(chunk.field(start,ptr - start),ptr - start,sig,ZT_C25519_SIGNATURE_LEN)) {
				return 0;
			}

			// New properly verified chunks can be flooded "virally" through the network
			if (fastPropagate) {
				Address *a = (Address *)0;
				Membership *m = (Membership *)0;
				Hashtable<Address,Membership>::Iterator i(_memberships);
				while (i.next(a,m)) {
					if ((*a != source)&&(*a != controller())) {
						Packet outp(*a,RR->identity.address(),Packet::VERB_NETWORK_CONFIG);
						outp.append(reinterpret_cast<const uint8_t *>(chunk.data()) + start,chunk.size() - start);
						RR->sw->send(tPtr,outp,true);
					}
				}
			}
		} else if ((source == controller())||(!source)) { // since old chunks aren't signed, only accept from controller itself (or via cluster backplane)
			// Legacy support for OK(NETWORK_CONFIG_REQUEST) from older controllers
			chunkId = packetId;
			configUpdateId = chunkId;
			totalLength = chunkLen;
			chunkIndex = 0;

			if (totalLength >= ZT_NETWORKCONFIG_DICT_CAPACITY) {
				return 0;
			}

			for(int i=0;i<ZT_NETWORK_MAX_INCOMING_UPDATES;++i) {
				if ((!c)||(_incomingConfigChunks[i].ts < c->ts)) {
					c = &(_incomingConfigChunks[i]);
				}
			}
		} else {
			// Single-chunk unsigned legacy configs are only allowed from the controller itself
			return 0;
		}

		++c->ts; // newer is higher, that's all we need

		if (c->updateId != configUpdateId) {
			c->updateId = configUpdateId;
			c->haveChunks = 0;
			c->haveBytes = 0;
		}
		if (c->haveChunks >= ZT_NETWORK_MAX_UPDATE_CHUNKS) {
			return false;
		}
		c->haveChunkIds[c->haveChunks++] = chunkId;

		memcpy(c->data.unsafeData() + chunkIndex,chunkData,chunkLen);
		c->haveBytes += chunkLen;

		if (c->haveBytes == totalLength) {
			c->data.unsafeData()[c->haveBytes] = (char)0; // ensure null terminated

			nc = new NetworkConfig();
			try {
				if (!nc->fromDictionary(c->data)) {
					delete nc;
					nc = (NetworkConfig *)0;
				}
			} catch ( ... ) {
				delete nc;
				nc = (NetworkConfig *)0;
			}
		}
	}

	if (nc) {
		this->setConfiguration(tPtr, *nc, true);
		delete nc;
		return configUpdateId;
	} else {
		return 0;
	}

	return 0;
}

int Network::setConfiguration(void *tPtr,const NetworkConfig &nconf,bool saveToDisk)
{
	if (_destroyed) {
		return 0;
	}

	// _lock is NOT locked when this is called
	try {
		if ((nconf.issuedTo != RR->identity.address())||(nconf.networkId != _id)) {
			return 0; // invalid config that is not for us or not for this network
		}
		if (_config == nconf) {
			return 1; // OK config, but duplicate of what we already have
		}

		ZT_VirtualNetworkConfig ctmp;
		bool oldPortInitialized;
		{	// do things that require lock here, but unlock before calling callbacks
			Mutex::Lock _l(_lock);

			_config = nconf;
			_lastConfigUpdate = RR->node->now();
			_netconfFailure = NETCONF_FAILURE_NONE;

			oldPortInitialized = _portInitialized;
			_portInitialized = true;

			_externalConfig(&ctmp);
		}

		_portError = RR->node->configureVirtualNetworkPort(tPtr,_id,&_uPtr,(oldPortInitialized) ? ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE : ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP,&ctmp);
		_authenticationURL = nconf.authenticationURL;

		if (saveToDisk) {
			Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> *const d = new Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY>();
			try {
				if (nconf.toDictionary(*d,false)) {
					uint64_t tmp[2];
					tmp[0] = _id;
					tmp[1] = 0;
					RR->node->stateObjectPut(tPtr,ZT_STATE_OBJECT_NETWORK_CONFIG,tmp,d->data(),d->sizeBytes());
				}
			} catch ( ... ) {}
			delete d;
		}

		return 2; // OK and configuration has changed
	} catch ( ... ) {} // ignore invalid configs
	return 0;
}

void Network::requestConfiguration(void *tPtr)
{
	if (_destroyed) {
		return;
	}

	if ((_id >> 56) == 0xff) {
		if ((_id & 0xffffff) == 0) {
			const uint16_t startPortRange = (uint16_t)((_id >> 40) & 0xffff);
			const uint16_t endPortRange = (uint16_t)((_id >> 24) & 0xffff);
			if (endPortRange >= startPortRange) {
				NetworkConfig *const nconf = new NetworkConfig();

				nconf->networkId = _id;
				nconf->timestamp = RR->node->now();
				nconf->credentialTimeMaxDelta = ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_MAX_MAX_DELTA;
				nconf->revision = 1;
				nconf->issuedTo = RR->identity.address();
				nconf->flags = ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION;
				nconf->mtu = ZT_DEFAULT_MTU;
				nconf->multicastLimit = 0;
				nconf->staticIpCount = 1;
				nconf->ruleCount = 14;
				nconf->staticIps[0] = InetAddress::makeIpv66plane(_id,RR->identity.address().toInt());

				// Drop everything but IPv6
				nconf->rules[0].t = (uint8_t)ZT_NETWORK_RULE_MATCH_ETHERTYPE | 0x80; // NOT
				nconf->rules[0].v.etherType = 0x86dd; // IPv6
				nconf->rules[1].t = (uint8_t)ZT_NETWORK_RULE_ACTION_DROP;

				// Allow ICMPv6
				nconf->rules[2].t = (uint8_t)ZT_NETWORK_RULE_MATCH_IP_PROTOCOL;
				nconf->rules[2].v.ipProtocol = 0x3a; // ICMPv6
				nconf->rules[3].t = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;

				// Allow destination ports within range
				nconf->rules[4].t = (uint8_t)ZT_NETWORK_RULE_MATCH_IP_PROTOCOL;
				nconf->rules[4].v.ipProtocol = 0x11; // UDP
				nconf->rules[5].t = (uint8_t)ZT_NETWORK_RULE_MATCH_IP_PROTOCOL | 0x40; // OR
				nconf->rules[5].v.ipProtocol = 0x06; // TCP
				nconf->rules[6].t = (uint8_t)ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE;
				nconf->rules[6].v.port[0] = startPortRange;
				nconf->rules[6].v.port[1] = endPortRange;
				nconf->rules[7].t = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;

				// Allow non-SYN TCP packets to permit non-connection-initiating traffic
				nconf->rules[8].t = (uint8_t)ZT_NETWORK_RULE_MATCH_CHARACTERISTICS | 0x80; // NOT
				nconf->rules[8].v.characteristics = ZT_RULE_PACKET_CHARACTERISTICS_TCP_SYN;
				nconf->rules[9].t = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;

				// Also allow SYN+ACK which are replies to SYN
				nconf->rules[10].t = (uint8_t)ZT_NETWORK_RULE_MATCH_CHARACTERISTICS;
				nconf->rules[10].v.characteristics = ZT_RULE_PACKET_CHARACTERISTICS_TCP_SYN;
				nconf->rules[11].t = (uint8_t)ZT_NETWORK_RULE_MATCH_CHARACTERISTICS;
				nconf->rules[11].v.characteristics = ZT_RULE_PACKET_CHARACTERISTICS_TCP_ACK;
				nconf->rules[12].t = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;

				nconf->rules[13].t = (uint8_t)ZT_NETWORK_RULE_ACTION_DROP;

				nconf->type = ZT_NETWORK_TYPE_PUBLIC;

				nconf->name[0] = 'a';
				nconf->name[1] = 'd';
				nconf->name[2] = 'h';
				nconf->name[3] = 'o';
				nconf->name[4] = 'c';
				nconf->name[5] = '-';
				Utils::hex((uint16_t)startPortRange,nconf->name + 6);
				nconf->name[10] = '-';
				Utils::hex((uint16_t)endPortRange,nconf->name + 11);
				nconf->name[15] = (char)0;

				this->setConfiguration(tPtr,*nconf,false);
				delete nconf;
			} else {
				this->setNotFound(tPtr);
			}
		} else if ((_id & 0xff) == 0x01) {
			// ffAAaaaaaaaaaa01 -- where AA is the IPv4 /8 to use and aaaaaaaaaa is the anchor node for multicast gather and replication
			const uint64_t myAddress = RR->identity.address().toInt();
			const uint64_t networkHub = (_id >> 8) & 0xffffffffffULL;

			uint8_t ipv4[4];
			ipv4[0] = (uint8_t)((_id >> 48) & 0xff);
			ipv4[1] = (uint8_t)((myAddress >> 16) & 0xff);
			ipv4[2] = (uint8_t)((myAddress >> 8) & 0xff);
			ipv4[3] = (uint8_t)(myAddress & 0xff);

			char v4ascii[24];
			Utils::decimal(ipv4[0],v4ascii);

			NetworkConfig *const nconf = new NetworkConfig();

			nconf->networkId = _id;
			nconf->timestamp = RR->node->now();
			nconf->credentialTimeMaxDelta = ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_MAX_MAX_DELTA;
			nconf->revision = 1;
			nconf->issuedTo = RR->identity.address();
			nconf->flags = ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION;
			nconf->mtu = ZT_DEFAULT_MTU;
			nconf->multicastLimit = 1024;
			nconf->specialistCount = (networkHub == 0) ? 0 : 1;
			nconf->staticIpCount = 2;
			nconf->ruleCount = 1;

			if (networkHub != 0) {
				nconf->specialists[0] = networkHub;
			}

			nconf->staticIps[0] = InetAddress::makeIpv66plane(_id,myAddress);
			nconf->staticIps[1].set(ipv4,4,8);

			nconf->rules[0].t = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;

			nconf->type = ZT_NETWORK_TYPE_PUBLIC;

			nconf->name[0] = 'a';
			nconf->name[1] = 'd';
			nconf->name[2] = 'h';
			nconf->name[3] = 'o';
			nconf->name[4] = 'c';
			nconf->name[5] = '-';
			unsigned long nn = 6;
			while ((nconf->name[nn] = v4ascii[nn - 6])) {
				++nn;
			}
			nconf->name[nn++] = '.';
			nconf->name[nn++] = '0';
			nconf->name[nn++] = '.';
			nconf->name[nn++] = '0';
			nconf->name[nn++] = '.';
			nconf->name[nn++] = '0';
			nconf->name[nn++] = (char)0;

			this->setConfiguration(tPtr,*nconf,false);
			delete nconf;
		}
		return;
	}

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

	RR->t->networkConfigRequestSent(tPtr,*this,ctrl);

	if (ctrl == RR->identity.address()) {
		if (RR->localNetworkController) {
			RR->localNetworkController->request(_id,InetAddress(),0xffffffffffffffffULL,RR->identity,rmd);
		} else {
			this->setNotFound(tPtr);
		}
		return;
	}

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
	outp.compress();
	RR->node->expectReplyTo(outp.packetId());
	RR->sw->send(tPtr,outp,true);
}

bool Network::gate(void *tPtr,const SharedPtr<Peer> &peer)
{
	const int64_t now = RR->node->now();
	//int64_t comTimestamp = 0;
	//int64_t comRevocationThreshold = 0;
	Mutex::Lock _l(_lock);
	try {
		if (_config) {
			Membership *m = _memberships.get(peer->address());
			//if (m) {
			//	comTimestamp = m->comTimestamp();
			//	comRevocationThreshold = m->comRevocationThreshold();
			//}
			if ( (_config.isPublic()) || ((m)&&(m->isAllowedOnNetwork(_config, peer->identity()))) ) {
				if (!m) {
					m = &(_membership(peer->address()));
				}
				if (m->multicastLikeGate(now)) {
					_announceMulticastGroupsTo(tPtr,peer->address(),_allMulticastGroups());
				}
				return true;
			}
		}
	} catch ( ... ) {}
	//printf("%.16llx %.10llx not allowed, COM ts %lld revocation %lld\n", _id, peer->address().toInt(), comTimestamp, comRevocationThreshold); fflush(stdout);

	return false;
}

bool Network::recentlyAssociatedWith(const Address &addr)
{
	Mutex::Lock _l(_lock);
	const Membership *m = _memberships.get(addr);
	return ((m)&&(m->recentlyAssociated(RR->node->now())));
}

void Network::clean()
{
	const int64_t now = RR->node->now();
	Mutex::Lock _l(_lock);

	if (_destroyed) {
		return;
	}

	{
		Hashtable< MulticastGroup,uint64_t >::Iterator i(_multicastGroupsBehindMe);
		MulticastGroup *mg = (MulticastGroup *)0;
		uint64_t *ts = (uint64_t *)0;
		while (i.next(mg,ts)) {
			if ((now - *ts) > (ZT_MULTICAST_LIKE_EXPIRE * 2)) {
				_multicastGroupsBehindMe.erase(*mg);
			}
		}
	}

	{
		Address *a = (Address *)0;
		Membership *m = (Membership *)0;
		Hashtable<Address,Membership>::Iterator i(_memberships);
		while (i.next(a,m)) {
			if (!RR->topology->getPeerNoCache(*a)) {
				_memberships.erase(*a);
			} else {
				m->clean(now,_config);
			}
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
				if (*a == maxAddr) {
					_remoteBridgeRoutes.erase(*m);
				}
			}
		}
	}
}

void Network::learnBridgedMulticastGroup(void *tPtr,const MulticastGroup &mg,int64_t now)
{
	Mutex::Lock _l(_lock);
	const unsigned long tmp = (unsigned long)_multicastGroupsBehindMe.size();
	_multicastGroupsBehindMe.set(mg,now);
	if (tmp != _multicastGroupsBehindMe.size()) {
		_sendUpdatesToMembers(tPtr,&mg);
	}
}

Membership::AddCredentialResult Network::addCredential(void *tPtr,const CertificateOfMembership &com)
{
	if (com.networkId() != _id) {
		return Membership::ADD_REJECTED;
	}
	Mutex::Lock _l(_lock);
	return _membership(com.issuedTo()).addCredential(RR,tPtr,_config,com);
}

Membership::AddCredentialResult Network::addCredential(void *tPtr,const Address &sentFrom,const Revocation &rev)
{
	if (rev.networkId() != _id) {
		return Membership::ADD_REJECTED;
	}

	Mutex::Lock _l(_lock);
	Membership &m = _membership(rev.target());

	const Membership::AddCredentialResult result = m.addCredential(RR,tPtr,_config,rev);

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
				outp.append((uint16_t)0); // no certificates of ownership
				RR->sw->send(tPtr,outp,true);
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
	if (_portError) {
		return ZT_NETWORK_STATUS_PORT_ERROR;
	}
	switch(_netconfFailure) {
		case NETCONF_FAILURE_ACCESS_DENIED:
			return ZT_NETWORK_STATUS_ACCESS_DENIED;
		case NETCONF_FAILURE_NOT_FOUND:
			return ZT_NETWORK_STATUS_NOT_FOUND;
		case NETCONF_FAILURE_NONE:
			return ((_config) ? ZT_NETWORK_STATUS_OK : ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION);
		case NETCONF_FAILURE_AUTHENTICATION_REQUIRED:
			return ZT_NETWORK_STATUS_AUTHENTICATION_REQUIRED;
		default:
			return ZT_NETWORK_STATUS_PORT_ERROR;
	}
}

void Network::_externalConfig(ZT_VirtualNetworkConfig *ec) const
{
	// assumes _lock is locked
	ec->nwid = _id;
	ec->mac = _mac.toInt();
	if (_config) {
		Utils::scopy(ec->name,sizeof(ec->name),_config.name);
	} else {
		ec->name[0] = (char)0;
	}
	ec->status = _status();
	ec->type = (_config) ? (_config.isPrivate() ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC) : ZT_NETWORK_TYPE_PRIVATE;
	ec->mtu = (_config) ? _config.mtu : ZT_DEFAULT_MTU;
	ec->dhcp = 0;
	std::vector<Address> ab(_config.activeBridges());
	ec->bridge = (std::find(ab.begin(),ab.end(),RR->identity.address()) != ab.end()) ? 1 : 0;
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

	ec->multicastSubscriptionCount = (unsigned int)_myMulticastGroups.size();
	for(unsigned long i=0;i<(unsigned long)_myMulticastGroups.size();++i) {
		ec->multicastSubscriptions[i].mac = _myMulticastGroups[i].mac().toInt();
		ec->multicastSubscriptions[i].adi = _myMulticastGroups[i].adi();
	}

	memcpy(&ec->dns, &_config.dns, sizeof(ZT_VirtualNetworkDNS));

	Utils::scopy(ec->authenticationURL, sizeof(ec->authenticationURL), _authenticationURL.c_str());
	ec->ssoVersion = _config.ssoVersion;
	ec->authenticationExpiryTime = _config.authenticationExpiryTime;
	ec->ssoEnabled = _config.ssoEnabled;
	Utils::scopy(ec->centralAuthURL, sizeof(ec->centralAuthURL), _config.centralAuthURL);
	Utils::scopy(ec->issuerURL, sizeof(ec->issuerURL), _config.issuerURL);
	Utils::scopy(ec->ssoNonce, sizeof(ec->ssoNonce), _config.ssoNonce);
	Utils::scopy(ec->ssoState, sizeof(ec->ssoState), _config.ssoState);
	Utils::scopy(ec->ssoClientID, sizeof(ec->ssoClientID), _config.ssoClientID);
	Utils::scopy(ec->ssoProvider, sizeof(ec->ssoProvider), _config.ssoProvider);
}

void Network::_sendUpdatesToMembers(void *tPtr,const MulticastGroup *const newMulticastGroup)
{
	// Assumes _lock is locked
	const int64_t now = RR->node->now();

	std::vector<MulticastGroup> groups;
	if (newMulticastGroup) {
		groups.push_back(*newMulticastGroup);
	} else {
		groups = _allMulticastGroups();
	}

	std::vector<Address> alwaysAnnounceTo;

	if ((newMulticastGroup)||((now - _lastAnnouncedMulticastGroupsUpstream) >= ZT_MULTICAST_ANNOUNCE_PERIOD)) {
		if (!newMulticastGroup) {
			_lastAnnouncedMulticastGroupsUpstream = now;
		}

		alwaysAnnounceTo = _config.alwaysContactAddresses();
		if (std::find(alwaysAnnounceTo.begin(),alwaysAnnounceTo.end(),controller()) == alwaysAnnounceTo.end()) {
			alwaysAnnounceTo.push_back(controller());
		}
		const std::vector<Address> upstreams(RR->topology->upstreamAddresses());
		for(std::vector<Address>::const_iterator a(upstreams.begin());a!=upstreams.end();++a) {
			if (std::find(alwaysAnnounceTo.begin(),alwaysAnnounceTo.end(),*a) == alwaysAnnounceTo.end()) {
				alwaysAnnounceTo.push_back(*a);
			}
		}
		std::sort(alwaysAnnounceTo.begin(),alwaysAnnounceTo.end());

		for(std::vector<Address>::const_iterator a(alwaysAnnounceTo.begin());a!=alwaysAnnounceTo.end();++a) {
			/*
			// push COM to non-members so they can do multicast request auth
			if ( (_config.com) && (!_memberships.contains(*a)) && (*a != RR->identity.address()) ) {
				Packet outp(*a,RR->identity.address(),Packet::VERB_NETWORK_CREDENTIALS);
				_config.com.serialize(outp);
				outp.append((uint8_t)0x00);
				outp.append((uint16_t)0); // no capabilities
				outp.append((uint16_t)0); // no tags
				outp.append((uint16_t)0); // no revocations
				outp.append((uint16_t)0); // no certificates of ownership
				RR->sw->send(tPtr,outp,true);
			}
			*/
			_announceMulticastGroupsTo(tPtr,*a,groups);
		}
	}

	{
		Address *a = (Address *)0;
		Membership *m = (Membership *)0;
		Hashtable<Address,Membership>::Iterator i(_memberships);
		while (i.next(a,m)) {
			const Identity remoteIdentity(RR->topology->getIdentity(tPtr, *a));
			if (remoteIdentity) {
				if ( ( m->multicastLikeGate(now) || (newMulticastGroup) ) && (m->isAllowedOnNetwork(_config, remoteIdentity)) && (!std::binary_search(alwaysAnnounceTo.begin(),alwaysAnnounceTo.end(),*a)) ) {
					_announceMulticastGroupsTo(tPtr,*a,groups);
				}
			}
		}
	}
}

void Network::_announceMulticastGroupsTo(void *tPtr,const Address &peer,const std::vector<MulticastGroup> &allMulticastGroups)
{
	// Assumes _lock is locked
	Packet *const outp = new Packet(peer,RR->identity.address(),Packet::VERB_MULTICAST_LIKE);

	for(std::vector<MulticastGroup>::const_iterator mg(allMulticastGroups.begin());mg!=allMulticastGroups.end();++mg) {
		if ((outp->size() + 24) >= ZT_PROTO_MAX_PACKET_LENGTH) {
			outp->compress();
			RR->sw->send(tPtr,*outp,true);
			outp->reset(peer,RR->identity.address(),Packet::VERB_MULTICAST_LIKE);
		}

		// network ID, MAC, ADI
		outp->append((uint64_t)_id);
		mg->mac().appendTo(*outp);
		outp->append((uint32_t)mg->adi());
	}

	if (outp->size() > ZT_PROTO_MIN_PACKET_LENGTH) {
		outp->compress();
		RR->sw->send(tPtr,*outp,true);
	}

	delete outp;
}

std::vector<MulticastGroup> Network::_allMulticastGroups() const
{
	// Assumes _lock is locked
	std::vector<MulticastGroup> mgs;
	mgs.reserve(_myMulticastGroups.size() + _multicastGroupsBehindMe.size() + 1);
	mgs.insert(mgs.end(),_myMulticastGroups.begin(),_myMulticastGroups.end());
	_multicastGroupsBehindMe.appendKeys(mgs);
	if ((_config)&&(_config.enableBroadcast())) {
		mgs.push_back(Network::BROADCAST);
	}
	std::sort(mgs.begin(),mgs.end());
	mgs.erase(std::unique(mgs.begin(),mgs.end()),mgs.end());
	return mgs;
}

Membership &Network::_membership(const Address &a)
{
	// assumes _lock is locked
	return _memberships[a];
}

void Network::setAuthenticationRequired(void *tPtr, const char* issuerURL, const char* centralEndpoint, const char* clientID, const char *ssoProvider, const char* nonce, const char* state)
{
	Mutex::Lock _l(_lock);
	_netconfFailure = NETCONF_FAILURE_AUTHENTICATION_REQUIRED;
	_config.ssoEnabled = true;
	_config.ssoVersion = 1;

	Utils::scopy(_config.issuerURL, sizeof(_config.issuerURL), issuerURL);
	Utils::scopy(_config.centralAuthURL, sizeof(_config.centralAuthURL), centralEndpoint);
	Utils::scopy(_config.ssoClientID, sizeof(_config.ssoClientID), clientID);
	Utils::scopy(_config.ssoNonce, sizeof(_config.ssoNonce), nonce);
	Utils::scopy(_config.ssoState, sizeof(_config.ssoState), state);
	Utils::scopy(_config.ssoProvider, sizeof(_config.ssoProvider), ssoProvider);
	_sendUpdateEvent(tPtr);
}

void Network::_sendUpdateEvent(void *tPtr) {
	ZT_VirtualNetworkConfig ctmp;
	_externalConfig(&ctmp);
	RR->node->configureVirtualNetworkPort(tPtr, _id, &_uPtr, (_portInitialized) ? ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE : ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP, &ctmp);
}

} // namespace ZeroTier
