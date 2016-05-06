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

#ifndef ZT_NETWORKCONFIG_HPP
#define ZT_NETWORKCONFIG_HPP

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <vector>
#include <stdexcept>
#include <algorithm>

#include "../include/ZeroTierOne.h"

#include "Constants.hpp"
#include "Buffer.hpp"
#include "InetAddress.hpp"
#include "MulticastGroup.hpp"
#include "Address.hpp"
#include "CertificateOfMembership.hpp"

#ifdef ZT_SUPPORT_OLD_STYLE_NETCONF
#include "Dictionary.hpp"
#include <string>
#endif

/**
 * Flag: allow passive bridging (experimental)
 */
#define ZT_NETWORKCONFIG_FLAG_ALLOW_PASSIVE_BRIDGING 0x0001

/**
 * Flag: enable broadcast
 */
#define ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST 0x0002

/**
 * Device is a network preferred relay
 */
#define ZT_NETWORKCONFIG_SPECIALIST_TYPE_NETWORK_PREFERRED_RELAY 0x0000010000000000ULL

/**
 * Device is an active bridge
 */
#define ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE 0x0000020000000000ULL

/**
 * An anchor is a device that is willing to be one and has been online/stable for a long time on this network
 */
#define ZT_NETWORKCONFIG_SPECIALIST_TYPE_ANCHOR 0x0000040000000000ULL

namespace ZeroTier {

#ifdef ZT_SUPPORT_OLD_STYLE_NETCONF

// Fields for meta-data sent with network config requests
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MAJOR_VERSION "majv"
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MINOR_VERSION "minv"
#define ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_REVISION "revv"

// These dictionary keys are short so they don't take up much room in
// netconf response packets.

// integer(hex)[,integer(hex),...]
#define ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES "et"
// network ID
#define ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID "nwid"
// integer(hex)
#define ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP "ts"
// integer(hex)
#define ZT_NETWORKCONFIG_DICT_KEY_REVISION "r"
// address of member
#define ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO "id"
// integer(hex)
#define ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT "ml"
// 0/1
#define ZT_NETWORKCONFIG_DICT_KEY_PRIVATE "p"
// text
#define ZT_NETWORKCONFIG_DICT_KEY_NAME "n"
// text
#define ZT_NETWORKCONFIG_DICT_KEY_DESC "d"
// IP/bits[,IP/bits,...]
// Note that IPs that end in all zeroes are routes with no assignment in them.
#define ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC "v4s"
// IP/bits[,IP/bits,...]
// Note that IPs that end in all zeroes are routes with no assignment in them.
#define ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC "v6s"
// serialized CertificateOfMembership
#define ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP "com"
// 0/1
#define ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST "eb"
// 0/1
#define ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING "pb"
// node[,node,...]
#define ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES "ab"
// node;IP/port[,node;IP/port]
#define ZT_NETWORKCONFIG_DICT_KEY_RELAYS "rl"
// IP/metric[,IP/metric,...]
#define ZT_NETWORKCONFIG_DICT_KEY_GATEWAYS "gw"

#endif // ZT_SUPPORT_OLD_STYLE_NETCONF

/**
 * Network configuration received from network controller nodes
 *
 * This is a memcpy()'able structure and is safe (in a crash sense) to modify
 * without locks.
 */
class NetworkConfig
{
public:
	/**
	 * Network preferred relay with optional physical endpoint addresses
	 */
	struct Relay
	{
		Address address;
		InetAddress phy4,phy6;
	};

	/**
	 * Create an instance of a NetworkConfig for the test network ID
	 *
	 * The test network ID is defined as ZT_TEST_NETWORK_ID. This is a
	 * "fake" network with no real controller and default options.
	 *
	 * @param self This node's ZT address
	 * @return Configuration for test network ID
	 */
	static inline NetworkConfig createTestNetworkConfig(const Address &self)
	{
		NetworkConfig nc;

		nc._nwid = ZT_TEST_NETWORK_ID;
		nc._timestamp = 1;
		nc._revision = 1;
		nc._issuedTo = self;
		nc._multicastLimit = ZT_MULTICAST_DEFAULT_LIMIT;
		nc._flags = ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST;
		nc._type = ZT_NETWORK_TYPE_PUBLIC;

		nc._rules[nc._ruleCount].t = ZT_NETWORK_RULE_ACTION_ACCEPT;
		nc._ruleCount = 1;

		Utils::snprintf(nc._name,sizeof(nc._name),"ZT_TEST_NETWORK");

		// Make up a V4 IP from 'self' in the 10.0.0.0/8 range -- no
		// guarantee of uniqueness but collisions are unlikely.
		uint32_t ip = (uint32_t)((self.toInt() & 0x00ffffff) | 0x0a000000); // 10.x.x.x
		if ((ip & 0x000000ff) == 0x000000ff) ip ^= 0x00000001; // but not ending in .255
		if ((ip & 0x000000ff) == 0x00000000) ip ^= 0x00000001; // or .0
		nc._staticIps[0] = InetAddress(Utils::hton(ip),8);

		// Assign an RFC4193-compliant IPv6 address -- will never collide
		nc._staticIps[1] = InetAddress::makeIpv6rfc4193(ZT_TEST_NETWORK_ID,self.toInt());

		nc._staticIpCount = 2;

		return nc;
	}

	NetworkConfig()
	{
		memset(this,0,sizeof(NetworkConfig));
	}

	NetworkConfig(const NetworkConfig &nc)
	{
		memcpy(this,&nc,sizeof(NetworkConfig));
	}

	inline NetworkConfig &operator=(const NetworkConfig &nc)
	{
		memcpy(this,&nc,sizeof(NetworkConfig));
		return *this;
	}

	/**
	 * @param etherType Ethernet frame type to check
	 * @return True if allowed on this network
	 */
	inline bool permitsEtherType(unsigned int etherType) const
	{
		unsigned int et = 0;
		for(unsigned int i=0;i<_ruleCount;++i) {
			ZT_VirtualNetworkRuleType rt = (ZT_VirtualNetworkRuleType)(_rules[i].t & 0x7f);
			if (rt == ZT_NETWORK_RULE_MATCH_ETHERTYPE) {
				et = _rules[i].v.etherType;
			} else if (rt == ZT_NETWORK_RULE_ACTION_ACCEPT) {
				if ((!et)||(et == etherType))
					return true;
				et = 0;
			}
		}
		return false;
	}

	/**
	 * @return Network ID that this config applies to
	 */
	inline uint64_t networkId() const throw() { return _nwid; }

	/**
	 * @return Timestamp of this config (controller-side)
	 */
	inline uint64_t timestamp() const throw() { return _timestamp; }

	/**
	 * @return Config revision number
	 */
	inline uint64_t revision() const throw() { return _revision; }

	/**
	 * @return ZeroTier address of device to which this config was issued
	 */
	inline const Address &issuedTo() const throw() { return _issuedTo; }

	/**
	 * @return Maximum number of multicast recipients or 0 to disable multicast
	 */
	inline unsigned int multicastLimit() const throw() { return _multicastLimit; }

	/**
	 * @return True if passive bridging is allowed (experimental)
	 */
	inline bool allowPassiveBridging() const throw() { return ((_flags & ZT_NETWORKCONFIG_FLAG_ALLOW_PASSIVE_BRIDGING) != 0); }

	/**
	 * @return True if broadcast (ff:ff:ff:ff:ff:ff) address should work on this network
	 */
	inline bool enableBroadcast() const throw() { return ((_flags & ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST) != 0); }

	/**
	 * @return Type of network (currently public or private)
	 */
	inline ZT_VirtualNetworkType type() const throw() { return _type; }

	/**
	 * @return Network type is public (no access control)
	 */
	inline bool isPublic() const throw() { return (_type == ZT_NETWORK_TYPE_PUBLIC); }

	/**
	 * @return Network type is private (certificate access control)
	 */
	inline bool isPrivate() const throw() { return (_type == ZT_NETWORK_TYPE_PRIVATE); }

	/**
	 * @return Short network name
	 */
	inline const char *name() const throw() { return _name; }

	/**
	 * @return Network certificate of membership or NULL COM object if none (public network)
	 */
	inline const CertificateOfMembership &com() const throw() { return _com; }

	/**
	 * @return ZeroTier-managed static IPs assigned to this device on this network
	 */
	inline std::vector<InetAddress> staticIps() const
	{
		std::vector<InetAddress> r;
		for(unsigned int i=0;i<_staticIpCount;++i)
			r.push_back(_staticIps[i]);
		return r;
	}

	/**
	 * @return ZeroTier addresses of devices on this network designated as active bridges
	 */
	inline std::vector<Address> activeBridges() const
	{
		std::vector<Address> r;
		for(unsigned int i=0;i<_specialistCount;++i) {
			if ((_specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE) != 0)
				r.push_back(Address(_specialists[i]));
		}
		return r;
	}

	/**
	 * @return ZeroTier addresses of "anchor" devices on this network
	 */
	inline std::vector<Address> anchors() const
	{
		std::vector<Address> r;
		for(unsigned int i=0;i<_specialistCount;++i) {
			if ((_specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_ANCHOR) != 0)
				r.push_back(Address(_specialists[i]));
		}
		return r;
	}

	/**
	 * Look up a static physical address for a given ZeroTier address
	 *
	 * @param zt ZeroTier address
	 * @param af Address family (e.g. AF_INET) or 0 for the first we find of any type
	 * @return Physical address, if any
	 */
	inline InetAddress staticPhysicalAddress(const Address &zt,unsigned int af) const
	{
		for(unsigned int i=0;i<_staticCount;++i) {
			if (_static[i].zt == zt) {
				if ((af == 0)||((unsigned int)_static[i].phy.ss_family == af))
					return _static[i].phy;
			}
		}
		return InetAddress();
	}

	/**
	 * This gets network preferred relays with their static physical address if one is defined
	 *
	 * @return Network-preferred relays for this network (if none, only roots will be used)
	 */
	inline std::vector<Relay> relays() const
	{
		std::vector<Relay> r;
		for(unsigned int i=0;i<_specialistCount;++i) {
			if ((_specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_NETWORK_PREFERRED_RELAY) != 0) {
				r.push_back(Relay());
				r.back().address = _specialists[i];
				r.back().phy4 = staticPhysicalAddress(r.back().address,AF_INET);
				r.back().phy6 = staticPhysicalAddress(r.back().address,AF_INET6);
			}
		}
		return r;
	}

	/**
	 * @param fromPeer Peer attempting to bridge other Ethernet peers onto network
	 * @return True if this network allows bridging
	 */
	inline bool permitsBridging(const Address &fromPeer) const
	{
		if ((_flags & ZT_NETWORKCONFIG_FLAG_ALLOW_PASSIVE_BRIDGING) != 0)
			return true;
		for(unsigned int i=0;i<_specialistCount;++i) {
			if ((fromPeer == _specialists[i])&&((_specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE) != 0))
				return true;
		}
		return false;
	}

	/**
	 * Iterate through relays efficiently
	 *
	 * @param ptr Value-result parameter -- start by initializing with zero, then call until return is null
	 * @return Address of relay or NULL if no more
	 */
	Address nextRelay(unsigned int &ptr) const
	{
		while (ptr < _specialistCount) {
			if ((_specialists[ptr] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_NETWORK_PREFERRED_RELAY) != 0) {
				return Address(_specialists[ptr]);
			}
			++ptr;
		}
		return Address();
	}

	/**
	 * @param zt ZeroTier address
	 * @return True if this address is a relay
	 */
	bool isRelay(const Address &zt) const
	{
		for(unsigned int i=0;i<_specialistCount;++i) {
			if ((zt == _specialists[i])&&((_specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_NETWORK_PREFERRED_RELAY) != 0))
				return true;
		}
		return false;
	}

	/**
	 * @return True if this network config is non-NULL
	 */
	inline operator bool() const throw() { return (_nwid != 0); }

	inline bool operator==(const NetworkConfig &nc) const { return (memcmp(this,&nc,sizeof(NetworkConfig)) == 0); }
	inline bool operator!=(const NetworkConfig &nc) const { return (!(*this == nc)); }

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		b.append((uint16_t)1); // version

		b.append((uint64_t)_nwid);
		b.append((uint64_t)_timestamp);
		b.append((uint64_t)_revision);
		_issuedTo.appendTo(b);
		b.append((uint32_t)_multicastLimit);
		b.append((uint32_t)_flags);
		b.append((uint8_t)_type);

		unsigned int nl = (unsigned int)strlen(_name);
		if (nl > 255) nl = 255; // sanity check
		b.append((uint8_t)nl);
		b.append((const void *)_name,nl);

		b.append((uint16_t)_specialistCount);
		for(unsigned int i=0;i<_specialistCount;++i)
			b.append((uint64_t)_specialists[i]);

		b.append((uint16_t)_routeCount);
		for(unsigned int i=0;i<_routeCount;++i) {
			reinterpret_cast<const InetAddress *>(&(_routes[i].target))->serialize(b);
			reinterpret_cast<const InetAddress *>(&(_routes[i].via))->serialize(b);
		}

		b.append((uint16_t)_staticIpCount);
		for(unsigned int i=0;i<_staticIpCount;++i)
			_staticIps[i].serialize(b);

		b.append((uint16_t)_staticCount);
		for(unsigned int i=0;i<_staticCount;++i) {
			_static[i].zt.appendTo(b);
			_static[i].phy.serialize(b);
		}

		b.append((uint16_t)_ruleCount);
		for(unsigned int i=0;i<_ruleCount;++i) {
			b.append((uint8_t)_rules[i].t);
			switch((ZT_VirtualNetworkRuleType)(_rules[i].t & 0x7f)) {
				//case ZT_NETWORK_RULE_ACTION_DROP:
				//case ZT_NETWORK_RULE_ACTION_ACCEPT:
				default:
					b.append((uint8_t)0);
					break;
				case ZT_NETWORK_RULE_ACTION_TEE:
				case ZT_NETWORK_RULE_ACTION_REDIRECT:
				case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
				case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS:
					b.append((uint8_t)5);
					Address(_rules[i].v.zt).appendTo(b);
					break;
				case ZT_NETWORK_RULE_MATCH_VLAN_ID:
					b.append((uint8_t)2);
					b.append((uint16_t)_rules[i].v.vlanId);
					break;
				case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
					b.append((uint8_t)1);
					b.append((uint8_t)_rules[i].v.vlanPcp);
					break;
				case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
					b.append((uint8_t)1);
					b.append((uint8_t)_rules[i].v.vlanDei);
					break;
				case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
					b.append((uint8_t)2);
					b.append((uint16_t)_rules[i].v.etherType);
					break;
				case ZT_NETWORK_RULE_MATCH_MAC_SOURCE:
				case ZT_NETWORK_RULE_MATCH_MAC_DEST:
					b.append((uint8_t)6);
					b.append(_rules[i].v.mac,6);
					break;
				case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
				case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
					b.append((uint8_t)5);
					b.append(&(_rules[i].v.ipv4.ip),4);
					b.append((uint8_t)_rules[i].v.ipv4.mask);
					break;
				case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
				case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
					b.append((uint8_t)17);
					b.append(_rules[i].v.ipv6.ip,16);
					b.append((uint8_t)_rules[i].v.ipv6.mask);
					break;
				case ZT_NETWORK_RULE_MATCH_IP_TOS:
					b.append((uint8_t)1);
					b.append((uint8_t)_rules[i].v.ipTos);
					break;
				case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
					b.append((uint8_t)1);
					b.append((uint8_t)_rules[i].v.ipProtocol);
					break;
				case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE:
				case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE:
					b.append((uint8_t)4);
					b.append((uint16_t)_rules[i].v.port[0]);
					b.append((uint16_t)_rules[i].v.port[1]);
					break;
				case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS:
					b.append((uint8_t)8);
					b.append((uint64_t)_rules[i].v.characteristics);
					break;
				case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
					b.append((uint8_t)4);
					b.append((uint16_t)_rules[i].v.frameSize[0]);
					b.append((uint16_t)_rules[i].v.frameSize[1]);
					break;
			}
		}

		_com.serialize(b);

		b.append((uint16_t)0); // extended bytes, currently 0 since unused
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		memset(this,0,sizeof(NetworkConfig));

		unsigned int p = startAt;

		if (b.template at<uint16_t>(p) != 1)
			throw std::invalid_argument("unrecognized version");
		p += 2;

		_nwid = b.template at<uint64_t>(p); p += 8;
		_timestamp = b.template at<uint64_t>(p); p += 8;
		_revision = b.template at<uint64_t>(p); p += 8;
		_issuedTo.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
		_multicastLimit = (unsigned int)b.template at<uint32_t>(p); p += 4;
		_flags = (unsigned int)b.template at<uint32_t>(p); p += 4;
		_type = (ZT_VirtualNetworkType)b[p++];

		unsigned int nl = (unsigned int)b[p++];
		memcpy(_name,b.field(p,nl),std::max(nl,(unsigned int)ZT_MAX_NETWORK_SHORT_NAME_LENGTH));
		p += nl;
		// _name will always be null terminated since field size is ZT_MAX_NETWORK_SHORT_NAME_LENGTH + 1

		_specialistCount = (unsigned int)b.template at<uint16_t>(p); p += 2;
		if (_specialistCount > ZT_MAX_NETWORK_SPECIALISTS)
			throw std::invalid_argument("overflow (specialists)");
		for(unsigned int i=0;i<_specialistCount;++i) {
			_specialists[i] = b.template at<uint64_t>(p); p += 8;
		}

		_routeCount = (unsigned int)b.template at<uint16_t>(p); p += 2;
		if (_routeCount > ZT_MAX_NETWORK_ROUTES)
			throw std::invalid_argument("overflow (routes)");
		for(unsigned int i=0;i<_routeCount;++i) {
			p += reinterpret_cast<InetAddress *>(&(_routes[i].target))->deserialize(b,p);
			p += reinterpret_cast<InetAddress *>(&(_routes[i].via))->deserialize(b,p);
		}

		_staticIpCount = (unsigned int)b.template at<uint16_t>(p); p += 2;
		if (_staticIpCount > ZT_MAX_ZT_ASSIGNED_ADDRESSES)
			throw std::invalid_argument("overflow (static IPs)");
		for(unsigned int i=0;i<_staticIpCount;++i) {
			p += _staticIps[i].deserialize(b,p);
		}

		_staticCount = (unsigned int)b.template at<uint16_t>(p); p += 2;
		if (_staticCount > ZT_MAX_NETWORK_STATIC_PHYSICAL_ADDRESSES)
			throw std::invalid_argument("overflow (static addresses)");
		for(unsigned int i=0;i<_staticCount;++i) {
			_static[i].zt.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
			p += _static[i].phy.deserialize(b,p);
		}

		_ruleCount = (unsigned int)b.template at<uint16_t>(p); p += 2;
		if (_ruleCount > ZT_MAX_NETWORK_RULES)
			throw std::invalid_argument("overflow (rules)");
		for(unsigned int i=0;i<_ruleCount;++i) {
			_rules[i].t = (uint8_t)b[p++];
			unsigned int rlen = (unsigned int)b[p++];
			switch((ZT_VirtualNetworkRuleType)(_rules[i].t & 0x7f)) {
				//case ZT_NETWORK_RULE_ACTION_DROP:
				//case ZT_NETWORK_RULE_ACTION_ACCEPT:
				default:
					break;
				case ZT_NETWORK_RULE_ACTION_TEE:
				case ZT_NETWORK_RULE_ACTION_REDIRECT:
				case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
				case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS: {
					Address tmp;
					tmp.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
					_rules[i].v.zt = tmp.toInt();
				}	break;
				case ZT_NETWORK_RULE_MATCH_VLAN_ID:
					_rules[i].v.vlanId = b.template at<uint16_t>(p);
					break;
				case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
					_rules[i].v.vlanPcp = (uint8_t)b[p];
					break;
				case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
					_rules[i].v.vlanDei = (uint8_t)b[p];
					break;
				case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
					_rules[i].v.etherType = b.template at<uint16_t>(p);
					break;
				case ZT_NETWORK_RULE_MATCH_MAC_SOURCE:
				case ZT_NETWORK_RULE_MATCH_MAC_DEST:
					memcpy(_rules[i].v.mac,b.field(p,6),6);
					break;
				case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
				case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
					memcpy(&(_rules[i].v.ipv4.ip),b.field(p,4),4);
					_rules[i].v.ipv4.mask = (uint8_t)b[p+4];
					break;
				case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
				case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
					memcpy(_rules[i].v.ipv6.ip,b.field(p,16),16);
					_rules[i].v.ipv6.mask = (uint8_t)b[p+16];
					break;
				case ZT_NETWORK_RULE_MATCH_IP_TOS:
					_rules[i].v.ipTos = (uint8_t)b[p];
					break;
				case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
					_rules[i].v.ipProtocol = (uint8_t)b[p];
					break;
				case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE:
				case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE:
					_rules[i].v.port[0] = b.template at<uint16_t>(p);
					_rules[i].v.port[1] = b.template at<uint16_t>(p+2);
					break;
				case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS:
					_rules[i].v.characteristics = b.template at<uint64_t>(p);
					break;
				case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
					_rules[i].v.frameSize[0] = b.template at<uint16_t>(p);
					_rules[i].v.frameSize[1] = b.template at<uint16_t>(p+2);
					break;
			}
			p += rlen;
		}

		p += _com.deserialize(b,p);

		p += b.template at<uint16_t>(p) + 2;

		return (p - startAt);
	}

#ifdef ZT_SUPPORT_OLD_STYLE_NETCONF
	void fromDictionary(const char *ds,unsigned int dslen);
#endif

protected: // protected so that a subclass can fill this out in network controller code
	uint64_t _nwid;
	uint64_t _timestamp;
	uint64_t _revision;
	Address _issuedTo;
	unsigned int _multicastLimit;
	unsigned int _flags;
	ZT_VirtualNetworkType _type;

	char _name[ZT_MAX_NETWORK_SHORT_NAME_LENGTH + 1];

	// Special ZeroTier addresses -- most significant 40 bits are address, least 24 are specialist type flags
	uint64_t _specialists[ZT_MAX_NETWORK_SPECIALISTS];

	// ZeroTier-managed IPs and routing table entries and stuff
	ZT_VirtualNetworkRoute _routes[ZT_MAX_NETWORK_ROUTES];
	InetAddress _staticIps[ZT_MAX_ZT_ASSIGNED_ADDRESSES];

	// ZeroTier to physical static mappings
	struct {
		Address zt;
		InetAddress phy;
	} _static[ZT_MAX_NETWORK_STATIC_PHYSICAL_ADDRESSES];

	// Network rules (only Ethernet type filtering is currently supported)
	ZT_VirtualNetworkRule _rules[ZT_MAX_NETWORK_RULES];

	unsigned int _specialistCount;
	unsigned int _routeCount;
	unsigned int _staticIpCount;
	unsigned int _staticCount;
	unsigned int _ruleCount;

	CertificateOfMembership _com;
};

} // namespace ZeroTier

#endif
