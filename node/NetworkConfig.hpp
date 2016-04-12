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

#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

#include "../include/ZeroTierOne.h"

#include "Constants.hpp"
#include "Dictionary.hpp"
#include "Buffer.hpp"
#include "InetAddress.hpp"
#include "MulticastGroup.hpp"
#include "Address.hpp"
#include "CertificateOfMembership.hpp"

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
	 * Create an instance of a NetworkConfig for the test network ID
	 *
	 * The test network ID is defined as ZT_TEST_NETWORK_ID. This is a
	 * "fake" network with no real controller and default options.
	 *
	 * @param self This node's ZT address
	 * @return Configuration for test network ID
	 */
	static NetworkConfig createTestNetworkConfig(const Address &self);

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
		for(unsigned int i=0;i<_ruleCount;++i) {
			if ((_rules[i].etherType < 0)||((unsigned int)_rules[i].etherType == etherType))
				return (_rules[i].action == ZT_NETWORK_RULE_ACTION_ACCEPT);
		}
		return false;
	}

#ifdef ZT_SUPPORT_OLD_STYLE_NETCONF
	/**
	 * Parse an old-style dictionary and fill in structure
	 *
	 * @throws std::invalid_argument Invalid dictionary
	 */
	void fromDictionary(const Dictionary &d);
#endif

	inline uint64_t networkId() const throw() { return _nwid; }
	inline uint64_t timestamp() const throw() { return _timestamp; }
	inline uint64_t revision() const throw() { return _revision; }
	inline const Address &issuedTo() const throw() { return _issuedTo; }
	inline unsigned int multicastLimit() const throw() { return _multicastLimit; }
	inline bool allowPassiveBridging() const throw() { return _allowPassiveBridging; }
	inline bool enableBroadcast() const throw() { return _enableBroadcast; }
	inline ZT_VirtualNetworkType type() const throw() { return _type; }
	inline bool isPublic() const throw() { return (_type == ZT_NETWORK_TYPE_PUBLIC); }
	inline bool isPrivate() const throw() { return (_type == ZT_NETWORK_TYPE_PRIVATE); }
	inline const char *name() const throw() { return _name; }
	inline const CertificateOfMembership &com() const throw() { return _com; }

	inline std::vector<InetAddress> localRoutes() const
	{
		std::vector<InetAddress> r;
		for(unsigned int i=0;i<_localRouteCount;++i)
			r.push_back(_localRoutes[i]);
		return r;
	}

	inline std::vector<InetAddress> staticIps() const
	{
		std::vector<InetAddress> r;
		for(unsigned int i=0;i<_staticIpCount;++i)
			r.push_back(_staticIps[i]);
		return r;
	}

	inline std::vector<InetAddress> gateways() const
	{
		std::vector<InetAddress> r;
		for(unsigned int i=0;i<_gatewayCount;++i)
			r.push_back(_gateways[i]);
		return r;
	}

	inline std::vector<Address> activeBridges() const
	{
		std::vector<Address> r;
		for(unsigned int i=0;i<_activeBridgeCount;++i)
			r.push_back(_activeBridges[i]);
		return r;
	}

	/**
	 * @param fromPeer Peer attempting to bridge other Ethernet peers onto network
	 * @return True if this network allows bridging
	 */
	inline bool permitsBridging(const Address &fromPeer) const
	{
		if (_allowPassiveBridging)
			return true;
		for(unsigned int i=0;i<_activeBridgeCount;++i) {
			if (_activeBridges[i] == fromPeer)
				return true;
		}
		return false;
	}

	inline operator bool() const throw() { return (_nwid != 0); }

	inline bool operator==(const NetworkConfig &nc) const { return (memcmp(this,&nc,sizeof(NetworkConfig)) == 0); }
	inline bool operator!=(const NetworkConfig &nc) const { return (!(*this == nc)); }

protected: // protected so that a subclass can fill this out in network controller code
	uint64_t _nwid;
	uint64_t _timestamp;
	uint64_t _revision;
	Address _issuedTo;
	unsigned int _multicastLimit;
	bool _allowPassiveBridging;
	bool _enableBroadcast;
	ZT_VirtualNetworkType _type;

	char _name[ZT_MAX_NETWORK_SHORT_NAME_LENGTH + 1];

	Address _activeBridges[ZT_MAX_NETWORK_ACTIVE_BRIDGES];
	InetAddress _localRoutes[ZT_MAX_NETWORK_LOCAL_ROUTES];
	InetAddress _staticIps[ZT_MAX_ZT_ASSIGNED_ADDRESSES];
	InetAddress _gateways[ZT_MAX_NETWORK_GATEWAYS];
	ZT_VirtualNetworkStaticDevice _static[ZT_MAX_NETWORK_STATIC_DEVICES];
	ZT_VirtualNetworkRule _rules[ZT_MAX_NETWORK_RULES];

	unsigned int _activeBridgeCount;
	unsigned int _localRouteCount;
	unsigned int _staticIpCount;
	unsigned int _gatewayCount;
	unsigned int _staticCount;
	unsigned int _ruleCount;

	CertificateOfMembership _com;
};

} // namespace ZeroTier

#endif
