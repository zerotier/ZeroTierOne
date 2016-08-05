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

namespace ZeroTier {

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

static bool _doZtFilter(
	const RuntimeEnvironment *RR,
	const uint64_t nwid,
	const bool inbound,
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
	const Tag *localTags,
	const unsigned int localTagCount,
	const uint32_t *remoteTagIds,
	const uint32_t *remoteTagValues,
	const unsigned int remoteTagCount,
	const Tag **relevantLocalTags, // pointer array must be at least [localTagCount] in size
	unsigned int &relevantLocalTagCount)
{
	// For each set of rules we start by assuming that they match (since no constraints
	// yields a 'match all' rule).
	uint8_t thisSetMatches = 1;

	for(unsigned int rn=0;rn<ruleCount;++rn) {
		const ZT_VirtualNetworkRuleType rt = (ZT_VirtualNetworkRuleType)(rules[rn].t & 0x7f);
		uint8_t thisRuleMatches = 0;

		switch(rt) {
			// Actions -------------------------------------------------------------

			case ZT_NETWORK_RULE_ACTION_DROP:
				if (thisSetMatches) {
					return false;
				} else {
					thisSetMatches = 1; // continue parsing next set of rules
				}
				break;
			case ZT_NETWORK_RULE_ACTION_ACCEPT:
				if (thisSetMatches) {
					return true;
				} else {
					thisSetMatches = 1; // continue parsing next set of rules
				}
				break;
			case ZT_NETWORK_RULE_ACTION_TEE:
			case ZT_NETWORK_RULE_ACTION_REDIRECT: {
				Packet outp(Address(rules[rn].v.zt),RR->identity.address(),Packet::VERB_EXT_FRAME);
				outp.append(nwid);
				outp.append((uint8_t)((rt == ZT_NETWORK_RULE_ACTION_REDIRECT) ? 0x04 : 0x02));
				macDest.appendTo(outp);
				macSource.appendTo(outp);
				outp.append((uint16_t)etherType);
				outp.append(frameData,frameLen);
				outp.compress();
				RR->sw->send(outp,true,nwid);

				if (rt == ZT_NETWORK_RULE_ACTION_REDIRECT) {
					return false;
				} else {
					thisSetMatches = 1; // TEE does not terminate parsing
				}
			}	break;

			// Rules ---------------------------------------------------------------

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
								unsigned int pos = headerLen + ((rt == ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE) ? 2 : 0);
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
									if (rt == ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE) pos += 2;
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
			}	break;
			case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
				thisRuleMatches = (uint8_t)((frameLen >= (unsigned int)rules[rn].v.frameSize[0])&&(frameLen <= (unsigned int)rules[rn].v.frameSize[1]));
				break;
			case ZT_NETWORK_RULE_MATCH_TAGS_SAMENESS:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR: {
				const Tag *lt = (const Tag *)0;
				for(unsigned int i=0;i<localTagCount;++i) {
					if (rules[rn].v.tag.id == localTags[i].id()) {
						lt = &(localTags[i]);
						break;
					}
				}
				if (!lt) {
					thisRuleMatches = 0;
				} else {
					const uint32_t *rtv = (const uint32_t *)0;
					for(unsigned int i=0;i<remoteTagCount;++i) {
						if (rules[rn].v.tag.id == remoteTagIds[i]) {
							rtv = &(remoteTagValues[i]);
							break;
						}
					}
					if (!rtv) {
						thisRuleMatches = 0;
					} else {
						if (rt == ZT_NETWORK_RULE_MATCH_TAGS_SAMENESS) {
							const uint32_t sameness = (lt->value() > *rtv) ? (lt->value() - *rtv) : (*rtv - lt->value());
							thisRuleMatches = (uint8_t)(sameness <= rules[rn].v.tag.value);
						} else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND) {
							thisRuleMatches = (uint8_t)((lt->value() & *rtv) <= rules[rn].v.tag.value);
						} else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR) {
							thisRuleMatches = (uint8_t)((lt->value() | *rtv) <= rules[rn].v.tag.value);
						} else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR) {
							thisRuleMatches = (uint8_t)((lt->value() ^ *rtv) <= rules[rn].v.tag.value);
						} else { // sanity check, can't really happen
							thisRuleMatches = 0;
						}
						if (thisRuleMatches) {
							relevantLocalTags[relevantLocalTagCount++] = lt;
						}
					}
				}
			}	break;
		}

		// thisSetMatches remains true if the current rule matched... or does NOT match if not bit (0x80) is 1
		thisSetMatches &= (thisRuleMatches ^ ((rules[rn].t & 0x80) >> 7));

		//TRACE("[%u] %u result==%u set==%u",rn,(unsigned int)rt,(unsigned int)thisRuleMatches,(unsigned int)thisSetMatches);
	}

	return false;
}

const ZeroTier::MulticastGroup Network::BROADCAST(ZeroTier::MAC(0xffffffffffffULL),0);

Network::Network(const RuntimeEnvironment *renv,uint64_t nwid,void *uptr) :
	RR(renv),
	_uPtr(uptr),
	_id(nwid),
	_mac(renv->identity.address(),nwid),
	_portInitialized(false),
	_lastConfigUpdate(0),
	_destroyed(false),
	_netconfFailure(NETCONF_FAILURE_NONE),
	_portError(0)
{
	char confn[128],mcdbn[128];
	Utils::snprintf(confn,sizeof(confn),"networks.d/%.16llx.conf",_id);

	if (_id == ZT_TEST_NETWORK_ID) {
		applyConfiguration(NetworkConfig::createTestNetworkConfig(RR->identity.address()));

		// Save a one-byte CR to persist membership in the test network
		RR->node->dataStorePut(confn,"\n",1,false);
	} else {
		bool gotConf = false;
		try {
			std::string conf(RR->node->dataStoreGet(confn));
			if (conf.length()) {
				Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> dconf(conf.c_str());
				NetworkConfig nconf;
				if (nconf.fromDictionary(dconf)) {
					this->setConfiguration(nconf,false);
					_lastConfigUpdate = 0; // we still want to re-request a new config from the network
					gotConf = true;
				}
			}
		} catch ( ... ) {} // ignore invalids, we'll re-request

		if (!gotConf) {
			// Save a one-byte CR to persist membership while we request a real netconf
			RR->node->dataStorePut(confn,"\n",1,false);
		}
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
	const Address &ztSource,
	const Address &ztDest,
	const MAC &macSource,
	const MAC &macDest,
	const uint8_t *frameData,
	const unsigned int frameLen,
	const unsigned int etherType,
	const unsigned int vlanId)
{
	uint32_t remoteTagIds[ZT_MAX_NETWORK_TAGS];
	uint32_t remoteTagValues[ZT_MAX_NETWORK_TAGS];
	const Tag *relevantLocalTags[ZT_MAX_NETWORK_TAGS];
	unsigned int relevantLocalTagCount = 0;

	Mutex::Lock _l(_lock);

	Membership &m = _memberships[ztDest];
	const unsigned int remoteTagCount = m.getAllTags(_config,remoteTagIds,remoteTagValues,ZT_MAX_NETWORK_TAGS);

	if (_doZtFilter(
		RR,
		_id,
		false,
		ztSource,
		ztDest,
		macSource,
		macDest,
		frameData,
		frameLen,
		etherType,
		vlanId,
		_config.rules,
		_config.ruleCount,
		_config.tags,
		_config.tagCount,
		remoteTagIds,
		remoteTagValues,
		remoteTagCount,
		relevantLocalTags,
		relevantLocalTagCount
	)) {
		m.sendCredentialsIfNeeded(RR,RR->node->now(),ztDest,_config.com,(const Capability *)0,relevantLocalTags,relevantLocalTagCount);
		return true;
	}

	for(unsigned int c=0;c<_config.capabilityCount;++c) {
		relevantLocalTagCount = 0;
		if (_doZtFilter(
			RR,
			_id,
			false,
			ztSource,
			ztDest,
			macSource,
			macDest,
			frameData,
			frameLen,
			etherType,
			vlanId,
			_config.capabilities[c].rules(),
			_config.capabilities[c].ruleCount(),
			_config.tags,
			_config.tagCount,
			remoteTagIds,
			remoteTagValues,
			remoteTagCount,
			relevantLocalTags,
			relevantLocalTagCount
		)) {
			m.sendCredentialsIfNeeded(RR,RR->node->now(),ztDest,_config.com,&(_config.capabilities[c]),relevantLocalTags,relevantLocalTagCount);
			return true;
		}
	}

	return false;
}

bool Network::filterIncomingPacket(
	const SharedPtr<Peer> &sourcePeer,
	const Address &ztDest,
	const MAC &macSource,
	const MAC &macDest,
	const uint8_t *frameData,
	const unsigned int frameLen,
	const unsigned int etherType,
	const unsigned int vlanId)
{
}

bool Network::subscribedToMulticastGroup(const MulticastGroup &mg,bool includeBridgedGroups) const
{
	Mutex::Lock _l(_lock);
	if (std::binary_search(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg))
		return true;
	else if (includeBridgedGroups)
		return _multicastGroupsBehindMe.contains(mg);
	else return false;
}

void Network::multicastSubscribe(const MulticastGroup &mg)
{
	{
		Mutex::Lock _l(_lock);
		if (std::binary_search(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg))
			return;
		_myMulticastGroups.push_back(mg);
		std::sort(_myMulticastGroups.begin(),_myMulticastGroups.end());
	}
	_announceMulticastGroups();
}

void Network::multicastUnsubscribe(const MulticastGroup &mg)
{
	Mutex::Lock _l(_lock);
	std::vector<MulticastGroup> nmg;
	for(std::vector<MulticastGroup>::const_iterator i(_myMulticastGroups.begin());i!=_myMulticastGroups.end();++i) {
		if (*i != mg)
			nmg.push_back(*i);
	}
	if (nmg.size() != _myMulticastGroups.size())
		_myMulticastGroups.swap(nmg);
}

bool Network::tryAnnounceMulticastGroupsTo(const SharedPtr<Peer> &peer)
{
	Mutex::Lock _l(_lock);
	if (
	    (_isAllowed(peer)) ||
	    (peer->address() == this->controller()) ||
	    (RR->topology->isUpstream(peer->identity()))
	   ) {
		_announceMulticastGroupsTo(peer,_allMulticastGroups());
		return true;
	}
	return false;
}

bool Network::applyConfiguration(const NetworkConfig &conf)
{
	if (_destroyed) // sanity check
		return false;
	try {
		if ((conf.networkId == _id)&&(conf.issuedTo == RR->identity.address())) {
			ZT_VirtualNetworkConfig ctmp;
			bool portInitialized;
			{
				Mutex::Lock _l(_lock);
				_config = conf;
				_lastConfigUpdate = RR->node->now();
				_netconfFailure = NETCONF_FAILURE_NONE;
				_externalConfig(&ctmp);
				portInitialized = _portInitialized;
				_portInitialized = true;
			}
			_portError = RR->node->configureVirtualNetworkPort(_id,&_uPtr,(portInitialized) ? ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE : ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP,&ctmp);
			return true;
		} else {
			TRACE("ignored invalid configuration for network %.16llx (configuration contains mismatched network ID or issued-to address)",(unsigned long long)_id);
		}
	} catch (std::exception &exc) {
		TRACE("ignored invalid configuration for network %.16llx (%s)",(unsigned long long)_id,exc.what());
	} catch ( ... ) {
		TRACE("ignored invalid configuration for network %.16llx (unknown exception)",(unsigned long long)_id);
	}
	return false;
}

int Network::setConfiguration(const NetworkConfig &nconf,bool saveToDisk)
{
	try {
		{
			Mutex::Lock _l(_lock);
			if (_config == nconf)
				return 1; // OK config, but duplicate of what we already have
		}
		if (applyConfiguration(nconf)) {
			if (saveToDisk) {
				char n[64];
				Utils::snprintf(n,sizeof(n),"networks.d/%.16llx.conf",_id);
				Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> d;
				if (nconf.toDictionary(d,false))
					RR->node->dataStorePut(n,(const void *)d.data(),d.sizeBytes(),true);
			}
			return 2; // OK and configuration has changed
		}
	} catch ( ... ) {
		TRACE("ignored invalid configuration for network %.16llx",(unsigned long long)_id);
	}
	return 0;
}

void Network::requestConfiguration()
{
	if (_id == ZT_TEST_NETWORK_ID) // pseudo-network-ID, uses locally generated static config
		return;

	Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> rmd;
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_VERSION,(uint64_t)ZT_NETWORKCONFIG_VERSION);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_PROTOCOL_VERSION,(uint64_t)ZT_PROTO_VERSION);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MAJOR_VERSION,(uint64_t)ZEROTIER_ONE_VERSION_MAJOR);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MINOR_VERSION,(uint64_t)ZEROTIER_ONE_VERSION_MINOR);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_REVISION,(uint64_t)ZEROTIER_ONE_VERSION_REVISION);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_NETWORK_RULES,(uint64_t)ZT_MAX_NETWORK_RULES);
	rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_CAPABILITY_RULES,(uint64_t)ZT_MAX_CAPABILITY_RULES);

	if (controller() == RR->identity.address()) {
		if (RR->localNetworkController) {
			NetworkConfig nconf;
			switch(RR->localNetworkController->doNetworkConfigRequest(InetAddress(),RR->identity,RR->identity,_id,rmd,nconf)) {
				case NetworkController::NETCONF_QUERY_OK:
					this->setConfiguration(nconf,true);
					return;
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

	TRACE("requesting netconf for network %.16llx from controller %s",(unsigned long long)_id,controller().toString().c_str());

	Packet outp(controller(),RR->identity.address(),Packet::VERB_NETWORK_CONFIG_REQUEST);
	outp.append((uint64_t)_id);
	const unsigned int rmdSize = rmd.sizeBytes();
	outp.append((uint16_t)rmdSize);
	outp.append((const void *)rmd.data(),rmdSize);
	outp.append((_config) ? (uint64_t)_config.revision : (uint64_t)0);
	outp.compress();
	RR->sw->send(outp,true,0);
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
			if ((now - m->clean(now)) > ZT_MEMBERSHIP_EXPIRATION_TIME)
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
		_announceMulticastGroups();
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

bool Network::_isAllowed(const SharedPtr<Peer> &peer) const
{
	// Assumes _lock is locked
	try {
		if (_config) {
			if (_config.isPublic()) {
				return true;
			} else {
				LockingPtr<Membership> m(peer->membership(_id,false));
				if (m) {
					return _config.com.agreesWith(m->com());
				}
			}
		}
	} catch ( ... ) {
		TRACE("isAllowed() check failed for peer %s: unexpected exception: unexpected exception",peer->address().toString().c_str());
	}
	return false;
}

class _MulticastAnnounceAll
{
public:
	_MulticastAnnounceAll(const RuntimeEnvironment *renv,Network *nw) :
		_now(renv->node->now()),
		_controller(nw->controller()),
		_network(nw),
		_anchors(nw->config().anchors()),
		_upstreamAddresses(renv->topology->upstreamAddresses())
	{}
	inline void operator()(Topology &t,const SharedPtr<Peer> &p)
	{
		if ( (_network->_isAllowed(p)) || // FIXME: this causes multicast LIKEs for public networks to get spammed, which isn't terrible but is a bit stupid
		     (p->address() == _controller) ||
		     (std::find(_upstreamAddresses.begin(),_upstreamAddresses.end(),p->address()) != _upstreamAddresses.end()) ||
				 (std::find(_anchors.begin(),_anchors.end(),p->address()) != _anchors.end()) ) {
			peers.push_back(p);
		}
	}
	std::vector< SharedPtr<Peer> > peers;
private:
	const uint64_t _now;
	const Address _controller;
	Network *const _network;
	const std::vector<Address> _anchors;
	const std::vector<Address> _upstreamAddresses;
};
void Network::_announceMulticastGroups()
{
	// Assumes _lock is locked
	std::vector<MulticastGroup> allMulticastGroups(_allMulticastGroups());
	_MulticastAnnounceAll gpfunc(RR,this);
	RR->topology->eachPeer<_MulticastAnnounceAll &>(gpfunc);
	for(std::vector< SharedPtr<Peer> >::const_iterator i(gpfunc.peers.begin());i!=gpfunc.peers.end();++i)
		_announceMulticastGroupsTo(*i,allMulticastGroups);
}

void Network::_announceMulticastGroupsTo(const SharedPtr<Peer> &peer,const std::vector<MulticastGroup> &allMulticastGroups) const
{
	// Assumes _lock is locked

	// Anyone we announce multicast groups to will need our COM to authenticate GATHER requests.
	{
		LockingPtr<Membership> m(peer->membership(_id,false));
		if (m) m->sendCredentialsIfNeeded(RR,RR->node->now(),*peer,_config);
	}

	Packet outp(peer->address(),RR->identity.address(),Packet::VERB_MULTICAST_LIKE);

	for(std::vector<MulticastGroup>::const_iterator mg(allMulticastGroups.begin());mg!=allMulticastGroups.end();++mg) {
		if ((outp.size() + 24) >= ZT_PROTO_MAX_PACKET_LENGTH) {
			outp.compress();
			RR->sw->send(outp,true,0);
			outp.reset(peer->address(),RR->identity.address(),Packet::VERB_MULTICAST_LIKE);
		}

		// network ID, MAC, ADI
		outp.append((uint64_t)_id);
		mg->mac().appendTo(outp);
		outp.append((uint32_t)mg->adi());
	}

	if (outp.size() > ZT_PROTO_MIN_PACKET_LENGTH) {
		outp.compress();
		RR->sw->send(outp,true,0);
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

} // namespace ZeroTier
