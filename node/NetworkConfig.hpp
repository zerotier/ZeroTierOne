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

#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

#include "Constants.hpp"
#include "Dictionary.hpp"
#include "InetAddress.hpp"
#include "AtomicCounter.hpp"
#include "SharedPtr.hpp"
#include "MulticastGroup.hpp"
#include "Address.hpp"
#include "CertificateOfMembership.hpp"

namespace ZeroTier {

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

/**
 * Network configuration received from network controller nodes
 *
 * This is an immutable value object created from a dictionary received from controller.
 */
class NetworkConfig
{
	friend class SharedPtr<NetworkConfig>;

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
	static SharedPtr<NetworkConfig> createTestNetworkConfig(const Address &self);

	/**
	 * @param d Dictionary containing configuration
	 * @throws std::invalid_argument Invalid configuration
	 */
	NetworkConfig(const Dictionary &d) { _fromDictionary(d); }

	/**
	 * @param etherType Ethernet frame type to check
	 * @return True if allowed on this network
	 */
	inline bool permitsEtherType(unsigned int etherType) const
		throw()
	{
		if ((etherType <= 0)||(etherType > 0xffff)) // sanity checks
			return false;
		if ((_etWhitelist[0] & 1)) // presence of 0 means allow all
			return true;
		return ((_etWhitelist[etherType >> 3] & (1 << (etherType & 7))) != 0);
	}

	/**
	 * @return Allowed ethernet types or a vector containing only 0 if "all"
	 */
	std::vector<unsigned int> allowedEtherTypes() const;

	inline uint64_t networkId() const throw() { return _nwid; }
	inline uint64_t timestamp() const throw() { return _timestamp; }
	inline uint64_t revision() const throw() { return _revision; }
	inline const Address &issuedTo() const throw() { return _issuedTo; }
	inline unsigned int multicastLimit() const throw() { return _multicastLimit; }
	inline bool allowPassiveBridging() const throw() { return _allowPassiveBridging; }
	inline bool isPublic() const throw() { return (!_private); }
	inline bool isPrivate() const throw() { return _private; }
	inline const std::string &name() const throw() { return _name; }
	inline const std::vector<InetAddress> &localRoutes() const throw() { return _localRoutes; }
	inline const std::vector<InetAddress> &staticIps() const throw() { return _staticIps; }
	inline const std::vector<InetAddress> &gateways() const throw() { return _gateways; }
	inline const std::vector<Address> &activeBridges() const throw() { return _activeBridges; }
	inline const std::vector< std::pair<Address,InetAddress> > &relays() const throw() { return _relays; }
	inline const CertificateOfMembership &com() const throw() { return _com; }
	inline bool enableBroadcast() const throw() { return _enableBroadcast; }

	/**
	 * @param fromPeer Peer attempting to bridge other Ethernet peers onto network
	 * @return True if this network allows bridging
	 */
	inline bool permitsBridging(const Address &fromPeer) const
	{
		return ( (_allowPassiveBridging) || (std::find(_activeBridges.begin(),_activeBridges.end(),fromPeer) != _activeBridges.end()) );
	}

	bool operator==(const NetworkConfig &nc) const;
	inline bool operator!=(const NetworkConfig &nc) const { return (!(*this == nc)); }

private:
	NetworkConfig() {}
	~NetworkConfig() {}

	void _fromDictionary(const Dictionary &d);

	uint64_t _nwid;
	uint64_t _timestamp;
	uint64_t _revision;
	unsigned char _etWhitelist[65536 / 8];
	Address _issuedTo;
	unsigned int _multicastLimit;
	bool _allowPassiveBridging;
	bool _private;
	bool _enableBroadcast;
	std::string _name;
	std::vector<InetAddress> _localRoutes;
	std::vector<InetAddress> _staticIps;
	std::vector<InetAddress> _gateways;
	std::vector<Address> _activeBridges;
	std::vector< std::pair<Address,InetAddress> > _relays;
	CertificateOfMembership _com;

	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif
