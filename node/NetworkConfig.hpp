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

// These dictionary keys are short so they don't take up much room in
// netconf response packets.
#define ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES "et"
#define ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID "nwid"
#define ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP "ts"
#define ZT_NETWORKCONFIG_DICT_KEY_REVISION "r"
#define ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO "id"
#define ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT "ml"
#define ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES "mr"
#define ZT_NETWORKCONFIG_DICT_KEY_PRIVATE "p"
#define ZT_NETWORKCONFIG_DICT_KEY_NAME "n"
#define ZT_NETWORKCONFIG_DICT_KEY_DESC "d"
#define ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC "v4s"
#define ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC "v6s"
#define ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP "com"
#define ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST "eb"
#define ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING "pb"
#define ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES "ab"

/**
 * Network configuration received from netconf master nodes
 *
 * This is an immutable value object created from a dictionary received from netconf master.
 */
class NetworkConfig
{
	friend class SharedPtr<NetworkConfig>;

public:
	/**
	 * Tuple of multicast rate parameters
	 */
	struct MulticastRate
	{
		MulticastRate() throw() {}
		MulticastRate(uint32_t pl,uint32_t maxb,uint32_t acc) throw() : preload(pl),maxBalance(maxb),accrual(acc) {}

		uint32_t preload;
		uint32_t maxBalance;
		uint32_t accrual;

		inline bool operator==(const MulticastRate &mr) const { return ((preload == mr.preload)&&(maxBalance == mr.maxBalance)&&(accrual == mr.accrual)); }
		inline bool operator!=(const MulticastRate &mr) const { return (!(*this == mr)); }
	};

	/**
	 * A hard-coded default multicast rate for networks that don't specify
	 */
	static const MulticastRate DEFAULT_MULTICAST_RATE;

	/**
	 * Create an instance of a NetworkConfig for the test network ID
	 *
	 * The test network ID is defined as ZT_TEST_NETWORK_ID. This is a
	 * "fake" network with no real netconf master and default options.
	 *
	 * @param self This node's ZT address
	 * @return Configured instance of netconf for test network ID
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
	inline const std::map<MulticastGroup,MulticastRate> &multicastRates() const throw() { return _multicastRates; }
	inline bool allowPassiveBridging() const throw() { return _allowPassiveBridging; }
	inline bool isPublic() const throw() { return (!_private); }
	inline bool isPrivate() const throw() { return _private; }
	inline const std::string &name() const throw() { return _name; }
	inline const std::string &description() const throw() { return _description; }
	inline const std::vector<InetAddress> &staticIps() const throw() { return _staticIps; }
	inline const std::vector<Address> &activeBridges() const throw() { return _activeBridges; }
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

	/**
	 * @param mg Multicast group
	 * @return Multicast rate or DEFAULT_MULTICAST_RATE if not set
	 */
	const MulticastRate &multicastRate(const MulticastGroup &mg) const
		throw();

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
	std::string _description;
	std::vector<InetAddress> _staticIps;
	std::vector<Address> _activeBridges;
	std::map<MulticastGroup,MulticastRate> _multicastRates;
	CertificateOfMembership _com;

	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif

