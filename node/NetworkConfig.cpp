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

#include "NetworkConfig.hpp"
#include "Utils.hpp"

namespace ZeroTier {

bool NetworkConfig::toDictionary(Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> &d,bool includeLegacy) const
{
	Buffer<ZT_NETWORKCONFIG_DICT_CAPACITY> tmp;

	d.clear();

	// Try to put the more human-readable fields first

	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_VERSION,(uint64_t)ZT_NETWORKCONFIG_VERSION)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID,this->networkId)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP,this->timestamp)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_REVISION,this->revision)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO,this->issuedTo)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_FLAGS,this->flags)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,(uint64_t)this->multicastLimit)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_TYPE,(uint64_t)this->type)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_NAME,this->name)) return false;

#ifdef ZT_SUPPORT_OLD_STYLE_NETCONF
	if (includeLegacy) {
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING_OLD,this->allowPassiveBridging())) return false;
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST_OLD,this->enableBroadcast())) return false;
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_PRIVATE_OLD,this->isPrivate())) return false;

		std::string v4s;
		for(unsigned int i=0;i<staticIpCount;++i) {
			if (this->staticIps[i].ss_family == AF_INET) {
				if (v4s.length() > 0)
					v4s.push_back(',');
				v4s.append(this->staticIps[i].toString());
			}
		}
		if (v4s.length() > 0) {
			if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC_OLD,v4s.c_str())) return false;
		}
		std::string v6s;
		for(unsigned int i=0;i<staticIpCount;++i) {
			if (this->staticIps[i].ss_family == AF_INET6) {
				if (v6s.length() > 0)
					v6s.push_back(',');
				v6s.append(this->staticIps[i].toString());
			}
		}
		if (v6s.length() > 0) {
			if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC_OLD,v6s.c_str())) return false;
		}

		std::string ets;
		unsigned int et = 0;
		ZT_VirtualNetworkRuleType lastrt = ZT_NETWORK_RULE_ACTION_ACCEPT;
		for(unsigned int i=0;i<ruleCount;++i) {
			ZT_VirtualNetworkRuleType rt = (ZT_VirtualNetworkRuleType)(rules[i].t & 0x7f);
			if (rt == ZT_NETWORK_RULE_MATCH_ETHERTYPE) {
				et = rules[i].v.etherType;
			} else if (rt == ZT_NETWORK_RULE_ACTION_ACCEPT) {
				if (((int)lastrt < 32)||(lastrt == ZT_NETWORK_RULE_MATCH_ETHERTYPE)) {
					if (ets.length() > 0)
						ets.push_back(',');
					char tmp[16];
					Utils::snprintf(tmp,sizeof(tmp),"%x",et);
					ets.append(tmp);
				}
				et = 0;
			}
			lastrt = rt;
		}
		if (ets.length() > 0) {
			if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES_OLD,ets.c_str())) return false;
		}

		if (this->com) {
			if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP_OLD,this->com.toString().c_str())) return false;
		}

		std::string ab;
		for(unsigned int i=0;i<this->specialistCount;++i) {
			if ((this->specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE) != 0) {
				if (ab.length() > 0)
					ab.push_back(',');
				ab.append(Address(this->specialists[i]).toString().c_str());
			}
		}
		if (ab.length() > 0) {
			if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES_OLD,ab.c_str())) return false;
		}

		std::vector<Relay> rvec(this->relays());
		std::string rl;
		for(std::vector<Relay>::const_iterator i(rvec.begin());i!=rvec.end();++i) {
			if (rl.length() > 0)
				rl.push_back(',');
			rl.append(i->address.toString());
			if (i->phy4) {
				rl.push_back(';');
				rl.append(i->phy4.toString());
			} else if (i->phy6) {
				rl.push_back(';');
				rl.append(i->phy6.toString());
			}
		}
		if (rl.length() > 0) {
			if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_RELAYS_OLD,rl.c_str())) return false;
		}
	}
#endif // ZT_SUPPORT_OLD_STYLE_NETCONF

	// Then add binary blobs

	if (this->com) {
		tmp.clear();
		this->com.serialize(tmp);
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_COM,tmp)) return false;
	}

	tmp.clear();
	for(unsigned int i=0;i<this->specialistCount;++i) {
		tmp.append((uint64_t)this->specialists[i]);
	}
	if (tmp.size()) {
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_SPECIALISTS,tmp)) return false;
	}

	tmp.clear();
	for(unsigned int i=0;i<this->routeCount;++i) {
		reinterpret_cast<const InetAddress *>(&(this->routes[i].target))->serialize(tmp);
		reinterpret_cast<const InetAddress *>(&(this->routes[i].via))->serialize(tmp);
		tmp.append((uint16_t)this->routes[i].flags);
		tmp.append((uint16_t)this->routes[i].metric);
	}
	if (tmp.size()) {
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_ROUTES,tmp)) return false;
	}

	tmp.clear();
	for(unsigned int i=0;i<this->staticIpCount;++i) {
		this->staticIps[i].serialize(tmp);
	}
	if (tmp.size()) {
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_STATIC_IPS,tmp)) return false;
	}

	tmp.clear();
	for(unsigned int i=0;i<this->pinnedCount;++i) {
		this->pinned[i].zt.appendTo(tmp);
		this->pinned[i].phy.serialize(tmp);
	}
	if (tmp.size()) {
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_PINNED,tmp)) return false;
	}

	tmp.clear();
	for(unsigned int i=0;i<this->ruleCount;++i) {
		tmp.append((uint8_t)rules[i].t);
		switch((ZT_VirtualNetworkRuleType)(rules[i].t & 0x7f)) {
			//case ZT_NETWORK_RULE_ACTION_DROP:
			//case ZT_NETWORK_RULE_ACTION_ACCEPT:
			default:
				tmp.append((uint8_t)0);
				break;
			case ZT_NETWORK_RULE_ACTION_TEE:
			case ZT_NETWORK_RULE_ACTION_REDIRECT:
			case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
			case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS:
				tmp.append((uint8_t)5);
				Address(rules[i].v.zt).appendTo(tmp);
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_ID:
				tmp.append((uint8_t)2);
				tmp.append((uint16_t)rules[i].v.vlanId);
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
				tmp.append((uint8_t)1);
				tmp.append((uint8_t)rules[i].v.vlanPcp);
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
				tmp.append((uint8_t)1);
				tmp.append((uint8_t)rules[i].v.vlanDei);
				break;
			case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
				tmp.append((uint8_t)2);
				tmp.append((uint16_t)rules[i].v.etherType);
				break;
			case ZT_NETWORK_RULE_MATCH_MAC_SOURCE:
			case ZT_NETWORK_RULE_MATCH_MAC_DEST:
				tmp.append((uint8_t)6);
				tmp.append(rules[i].v.mac,6);
				break;
			case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
			case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
				tmp.append((uint8_t)5);
				tmp.append(&(rules[i].v.ipv4.ip),4);
				tmp.append((uint8_t)rules[i].v.ipv4.mask);
				break;
			case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
			case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
				tmp.append((uint8_t)17);
				tmp.append(rules[i].v.ipv6.ip,16);
				tmp.append((uint8_t)rules[i].v.ipv6.mask);
				break;
			case ZT_NETWORK_RULE_MATCH_IP_TOS:
				tmp.append((uint8_t)1);
				tmp.append((uint8_t)rules[i].v.ipTos);
				break;
			case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
				tmp.append((uint8_t)1);
				tmp.append((uint8_t)rules[i].v.ipProtocol);
				break;
			case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE:
			case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE:
				tmp.append((uint8_t)4);
				tmp.append((uint16_t)rules[i].v.port[0]);
				tmp.append((uint16_t)rules[i].v.port[1]);
				break;
			case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS:
				tmp.append((uint8_t)8);
				tmp.append((uint64_t)rules[i].v.characteristics);
				break;
			case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
				tmp.append((uint8_t)4);
				tmp.append((uint16_t)rules[i].v.frameSize[0]);
				tmp.append((uint16_t)rules[i].v.frameSize[1]);
				break;
			case ZT_NETWORK_RULE_MATCH_TCP_RELATIVE_SEQUENCE_NUMBER_RANGE:
				tmp.append((uint8_t)8);
				tmp.append((uint32_t)rules[i].v.tcpseq[0]);
				tmp.append((uint32_t)rules[i].v.tcpseq[1]);
				break;
			case ZT_NETWORK_RULE_MATCH_COM_FIELD_GE:
			case ZT_NETWORK_RULE_MATCH_COM_FIELD_LE:
				tmp.append((uint8_t)16);
				tmp.append((uint64_t)rules[i].v.comIV[0]);
				tmp.append((uint64_t)rules[i].v.comIV[1]);
				break;
		}
	}
	if (tmp.size()) {
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_RULES,tmp)) return false;
	}

	return true;
}

bool NetworkConfig::fromDictionary(const Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> &d)
{
	try {
		Buffer<ZT_NETWORKCONFIG_DICT_CAPACITY> tmp;
		char tmp2[ZT_NETWORKCONFIG_DICT_CAPACITY];

		memset(this,0,sizeof(NetworkConfig));

		// Fields that are always present, new or old
		this->networkId = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID,0);
		if (!this->networkId)
			return false;
		this->timestamp = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP,0);
		this->revision = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_REVISION,0);
		this->issuedTo = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO,0);
		if (!this->issuedTo)
			return false;
		this->multicastLimit = (unsigned int)d.getUI(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,0);
		d.get(ZT_NETWORKCONFIG_DICT_KEY_NAME,this->name,sizeof(this->name));

		if (d.getUI(ZT_NETWORKCONFIG_DICT_KEY_VERSION,0) < 6) {
	#ifdef ZT_SUPPORT_OLD_STYLE_NETCONF
			// Decode legacy fields if version is old
			if (d.getB(ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING_OLD))
				this->flags |= ZT_NETWORKCONFIG_FLAG_ALLOW_PASSIVE_BRIDGING;
			if (d.getB(ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST_OLD))
				this->flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST;
			this->flags |= ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION; // always enable for old-style netconf
			this->type = (d.getB(ZT_NETWORKCONFIG_DICT_KEY_PRIVATE_OLD,true)) ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC;

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC_OLD,tmp2,sizeof(tmp2)) > 0) {
				char *saveptr = (char *)0;
				for(char *f=Utils::stok(tmp2,",",&saveptr);(f);f=Utils::stok((char *)0,",",&saveptr)) {
					if (this->staticIpCount >= ZT_MAX_ZT_ASSIGNED_ADDRESSES) break;
					InetAddress ip(f);
					if (!ip.isNetwork())
						this->staticIps[this->staticIpCount++] = ip;
				}
			}
			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC_OLD,tmp2,sizeof(tmp2)) > 0) {
				char *saveptr = (char *)0;
				for(char *f=Utils::stok(tmp2,",",&saveptr);(f);f=Utils::stok((char *)0,",",&saveptr)) {
					if (this->staticIpCount >= ZT_MAX_ZT_ASSIGNED_ADDRESSES) break;
					InetAddress ip(f);
					if (!ip.isNetwork())
						this->staticIps[this->staticIpCount++] = ip;
				}
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP_OLD,tmp2,sizeof(tmp2)) > 0) {
				this->com.fromString(tmp2);
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES_OLD,tmp2,sizeof(tmp2)) > 0) {
				char *saveptr = (char *)0;
				for(char *f=Utils::stok(tmp2,",",&saveptr);(f);f=Utils::stok((char *)0,",",&saveptr)) {
					unsigned int et = Utils::hexStrToUInt(f) & 0xffff;
					if ((this->ruleCount + 2) > ZT_MAX_NETWORK_RULES) break;
					if (et > 0) {
						this->rules[this->ruleCount].t = (uint8_t)ZT_NETWORK_RULE_MATCH_ETHERTYPE;
						this->rules[this->ruleCount].v.etherType = (uint16_t)et;
						++this->ruleCount;
					}
					this->rules[this->ruleCount++].t = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;
				}
			} else {
				this->rules[0].t = ZT_NETWORK_RULE_ACTION_ACCEPT;
				this->ruleCount = 1;
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES_OLD,tmp2,sizeof(tmp2)) > 0) {
				char *saveptr = (char *)0;
				for(char *f=Utils::stok(tmp2,",",&saveptr);(f);f=Utils::stok((char *)0,",",&saveptr)) {
					this->addSpecialist(Address(f),ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE);
				}
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_RELAYS_OLD,tmp2,sizeof(tmp2)) > 0) {
				char *saveptr = (char *)0;
				for(char *f=Utils::stok(tmp2,",",&saveptr);(f);f=Utils::stok((char *)0,",",&saveptr)) {
					char tmp3[256];
					Utils::scopy(tmp3,sizeof(tmp3),f);

					InetAddress phy;
					char *semi = tmp3;
					while (*semi) {
						if (*semi == ';') {
							*semi = (char)0;
							++semi;
							phy = InetAddress(semi);
						} else ++semi;
					}
					Address zt(tmp3);

					this->addSpecialist(zt,ZT_NETWORKCONFIG_SPECIALIST_TYPE_NETWORK_PREFERRED_RELAY);
					if ((phy)&&(this->pinnedCount < ZT_MAX_NETWORK_PINNED)) {
						this->pinned[this->pinnedCount].zt = zt;
						this->pinned[this->pinnedCount].phy = phy;
						++this->pinnedCount;
					}
				}
			}
	#else
			return false;
	#endif // ZT_SUPPORT_OLD_STYLE_NETCONF
		} else {
			// Otherwise we can use the new fields
			this->flags = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_FLAGS,0);
			this->type = (ZT_VirtualNetworkType)d.getUI(ZT_NETWORKCONFIG_DICT_KEY_TYPE,(uint64_t)ZT_NETWORK_TYPE_PRIVATE);

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_COM,tmp)) {
				this->com.deserialize(tmp,0);
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_SPECIALISTS,tmp)) {
				unsigned int p = 0;
				while (((p + 8) <= tmp.size())&&(specialistCount < ZT_MAX_NETWORK_SPECIALISTS)) {
					this->specialists[this->specialistCount++] = tmp.at<uint64_t>(p);
					p += 8;
				}
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_ROUTES,tmp)) {
				unsigned int p = 0;
				while ((p < tmp.size())&&(routeCount < ZT_MAX_NETWORK_ROUTES)) {
					p += reinterpret_cast<InetAddress *>(&(this->routes[this->routeCount].target))->deserialize(tmp,p);
					p += reinterpret_cast<InetAddress *>(&(this->routes[this->routeCount].via))->deserialize(tmp,p);
					this->routes[this->routeCount].flags = tmp.at<uint16_t>(p); p += 2;
					this->routes[this->routeCount].metric = tmp.at<uint16_t>(p); p += 2;
					++this->routeCount;
				}
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_STATIC_IPS,tmp)) {
				unsigned int p = 0;
				while ((p < tmp.size())&&(staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)) {
					p += this->staticIps[this->staticIpCount++].deserialize(tmp,p);
				}
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_PINNED,tmp)) {
				unsigned int p = 0;
				while ((p < tmp.size())&&(pinnedCount < ZT_MAX_NETWORK_PINNED)) {
					this->pinned[this->pinnedCount].zt.setTo(tmp.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
					p += this->pinned[this->pinnedCount].phy.deserialize(tmp,p);
					++this->pinnedCount;
				}
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_RULES,tmp)) {
				unsigned int p = 0;
				while ((p < tmp.size())&&(ruleCount < ZT_MAX_NETWORK_RULES)) {
					rules[ruleCount].t = (uint8_t)tmp[p++];
					unsigned int fieldLen = (unsigned int)tmp[p++];
					switch((ZT_VirtualNetworkRuleType)(rules[ruleCount].t & 0x7f)) {
						default:
							break;
						case ZT_NETWORK_RULE_ACTION_TEE:
						case ZT_NETWORK_RULE_ACTION_REDIRECT:
						case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
						case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS:
							rules[ruleCount].v.zt = Address(tmp.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH).toInt();
							break;
						case ZT_NETWORK_RULE_MATCH_VLAN_ID:
							rules[ruleCount].v.vlanId = tmp.at<uint16_t>(p);
							break;
						case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
							rules[ruleCount].v.vlanPcp = (uint8_t)tmp[p];
							break;
						case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
							rules[ruleCount].v.vlanDei = (uint8_t)tmp[p];
							break;
						case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
							rules[ruleCount].v.etherType = tmp.at<uint16_t>(p);
							break;
						case ZT_NETWORK_RULE_MATCH_MAC_SOURCE:
						case ZT_NETWORK_RULE_MATCH_MAC_DEST:
							memcpy(rules[ruleCount].v.mac,tmp.field(p,6),6);
							break;
						case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
						case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
							memcpy(&(rules[ruleCount].v.ipv4.ip),tmp.field(p,4),4);
							rules[ruleCount].v.ipv4.mask = (uint8_t)tmp[p + 4];
							break;
						case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
						case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
							memcpy(rules[ruleCount].v.ipv6.ip,tmp.field(p,16),16);
							rules[ruleCount].v.ipv6.mask = (uint8_t)tmp[p + 16];
							break;
						case ZT_NETWORK_RULE_MATCH_IP_TOS:
							rules[ruleCount].v.ipTos = (uint8_t)tmp[p];
							break;
						case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
							rules[ruleCount].v.ipProtocol = (uint8_t)tmp[p];
							break;
						case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE:
						case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE:
							rules[ruleCount].v.port[0] = tmp.at<uint16_t>(p);
							rules[ruleCount].v.port[1] = tmp.at<uint16_t>(p + 2);
							break;
						case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS:
							rules[ruleCount].v.characteristics = tmp.at<uint64_t>(p);
							break;
						case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
							rules[ruleCount].v.frameSize[0] = tmp.at<uint16_t>(p);
							rules[ruleCount].v.frameSize[0] = tmp.at<uint16_t>(p + 2);
							break;
						case ZT_NETWORK_RULE_MATCH_TCP_RELATIVE_SEQUENCE_NUMBER_RANGE:
							rules[ruleCount].v.tcpseq[0] = tmp.at<uint32_t>(p);
							rules[ruleCount].v.tcpseq[1] = tmp.at<uint32_t>(p + 4);
							break;
						case ZT_NETWORK_RULE_MATCH_COM_FIELD_GE:
						case ZT_NETWORK_RULE_MATCH_COM_FIELD_LE:
							rules[ruleCount].v.comIV[0] = tmp.at<uint64_t>(p);
							rules[ruleCount].v.comIV[1] = tmp.at<uint64_t>(p + 8);
							break;
					}
					p += fieldLen;
					++ruleCount;
				}
			}
		}

		//printf("~~~\n%s\n~~~\n",d.data());
		//dump();
		//printf("~~~\n");

		return true;
	} catch ( ... ) {
		return false;
	}
}

} // namespace ZeroTier
