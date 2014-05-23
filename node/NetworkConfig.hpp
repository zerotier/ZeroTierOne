/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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
#include <set>
#include <string>
#include <stdexcept>

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
#define ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO "id"
#define ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_PREFIX_BITS "mpb"
#define ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_DEPTH "md"
#define ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES "mr"
#define ZT_NETWORKCONFIG_DICT_KEY_PRIVATE "p"
#define ZT_NETWORKCONFIG_DICT_KEY_NAME "n"
#define ZT_NETWORKCONFIG_DICT_KEY_DESC "d"
#define ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC "v4s"
#define ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC "v6s"
#define ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP "com"
#define ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST "eb"

/**
 * Network configuration received from netconf master nodes
 *
 * This is designed to work as an immutable value object held in a shared
 * pointer so that it can be both updated and used without too much mutex
 * boogie.
 */
class NetworkConfig
{
public:
	friend class SharedPtr<NetworkConfig>;

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
	};

	/**
	 * A hard-coded default multicast rate for networks that don't specify
	 */
	static const MulticastRate DEFAULT_MULTICAST_RATE;

	/**
	 * @param d Dictionary containing configuration
	 * @throws std::invalid_argument Invalid configuration
	 */
	NetworkConfig(const Dictionary &d) { _fromDictionary(d); }

	/**
	 * @param etherType Ethernet frame type to check
	 * @return True if allowed on this network
	 */
	inline bool permitsEtherType(unsigned int etherType)
		throw()
	{
		if ((!etherType)||(etherType > 0xffff)) // sanity checks
			return false;
		else if ((_etWhitelist[0] & 1)) // prsence of 0 in set inverts sense: whitelist becomes blacklist
			return ((_etWhitelist[etherType >> 3] & (1 << (etherType & 7))) == 0);
		else return ((_etWhitelist[etherType >> 3] & (1 << (etherType & 7))) != 0);
	}

	std::set<unsigned int> allowedEtherTypes() const;
	inline uint64_t networkId() const throw() { return _nwid; }
	inline uint64_t timestamp() const throw() { return _timestamp; }
	inline const Address &issuedTo() const throw() { return _issuedTo; }
	inline unsigned int multicastPrefixBits() const throw() { return _multicastPrefixBits; }
	inline unsigned int multicastDepth() const throw() { return _multicastDepth; }
	inline const std::map<MulticastGroup,MulticastRate> &multicastRates() const throw() { return _multicastRates; }
	inline bool isOpen() const throw() { return (!_private); }
	inline bool isPrivate() const throw() { return _private; }
	inline const std::string &name() const throw() { return _name; }
	inline const std::string &description() const throw() { return _description; }
	inline const std::set<InetAddress> &staticIps() const throw() { return _staticIps; }
	inline const CertificateOfMembership &com() const throw() { return _com; }
	inline bool enableBroadcast() const throw() { return _enableBroadcast; }

	/**
	 * @param fromPeer Peer attempting to bridge other Ethernet peers onto network
	 * @return True if this network allows bridging
	 */
	inline bool permitsBridging(const Address &fromPeer) const
		throw()
	{
		return false; // TODO: bridging not implemented yet
	}

	/**
	 * @param mg Multicast group
	 * @return Multicast rate or DEFAULT_MULTICAST_RATE if not set
	 */
	const MulticastRate &multicastRate(const MulticastGroup &mg) const
		throw();

private:
	NetworkConfig() {}
	~NetworkConfig() {}

	void _fromDictionary(const Dictionary &d);

	unsigned char _etWhitelist[65536 / 8];
	uint64_t _nwid;
	uint64_t _timestamp;
	Address _issuedTo;
	unsigned int _multicastPrefixBits;
	unsigned int _multicastDepth;
	bool _private;
	bool _enableBroadcast;
	std::string _name;
	std::string _description;
	std::set<InetAddress> _staticIps;
	std::map<MulticastGroup,MulticastRate> _multicastRates;
	CertificateOfMembership _com;

	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif

