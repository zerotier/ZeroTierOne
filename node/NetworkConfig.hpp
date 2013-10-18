/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_NETWORKCONFIG_HPP
#define _ZT_NETWORKCONFIG_HPP

#include <set>
#include <string>
#include <stdexcept>

#include "Dictionary.hpp"
#include "InetAddress.hpp"
#include "AtomicCounter.hpp"
#include "SharedPtr.hpp"

namespace ZeroTier {

// These are short to fit in packets with plenty of room to spare
#define ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES "et"
#define ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID "nwid"
#define ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP "ts"
#define ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO "id"
#define ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_PREFIX_BITS "mpb"
#define ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_DEPTH "md"
#define ZT_NETWORKCONFIG_DICT_KEY_ARP_CACHE_TTL "cARP"
#define ZT_NETWORKCONFIG_DICT_KEY_NDP_CACHE_TTL "cNDP"
#define ZT_NETWORKCONFIG_DICT_KEY_EMULATE_ARP "eARP"
#define ZT_NETWORKCONFIG_DICT_KEY_EMULATE_NDP "eNDP"
#define ZT_NETWORKCONFIG_DICT_KEY_IS_OPEN "o"
#define ZT_NETWORKCONFIG_DICT_KEY_NAME "name"
#define ZT_NETWORKCONFIG_DICT_KEY_DESC "desc"
#define ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC "v4s"
#define ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC "v6s"
#define ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES "mr"

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
	 * @param d Dictionary containing configuration
	 * @throws std::invalid_argument Invalid configuration
	 */
	NetworkConfig(const Dictionary &d)
		throw(std::invalid_argument)
	{
		_fromDictionary(d);
	}

	/**
	 * @param etherType Ethernet frame type to check
	 * @return True if allowed on this network
	 */
	inline bool allowsEtherType(unsigned int etherType)
		throw()
	{
		if ((!etherType)||(etherType > 0xffff)) // sanity checks
			return false;
		else if ((_etWhitelist[0] & 1)) // prsence of 0 in set inverts sense: whitelist becomes blacklist
			return (!(_etWhitelist[etherType >> 3] & (1 << (etherType & 7))));
		else return ((_etWhitelist[etherType >> 3] & (1 << (etherType & 7))));
	}

	std::set<unsigned int> allowedEtherTypes() const;
	inline uint64_t networkId() const throw() { return _nwid; }
	inline uint64_t timestamp() const throw() { return _timestamp; }
	inline const Address &issuedTo() const throw() { return _issuedTo; }
	inline unsigned int multicastPrefixBits() const throw() { return _multicastPrefixBits; }
	inline unsigned int multicastDepth() const throw() { return _multicastDepth; }
	inline unsigned int arpCacheTtl() const throw() { return _arpCacheTtl; }
	inline unsigned int ndpCacheTtl() const throw() { return _ndpCacheTtl; }
	inline bool emulateArp() const throw() { return _emulateArp; }
	inline bool emulateNdp() const throw() { return _emulateNdp; }
	inline bool isOpen() const throw() { return _isOpen; }
	inline const std::string &name() const throw() { return _name; }
	inline const std::string &description() const throw() { return _description; }
	inline const std::set<InetAddress> &staticIps() const throw() { return _staticIps; }
	inline const MulticastRateTable &multicastRates() const throw() { return _multicastRates; }

private:
	NetworkConfig() {}
	~NetworkConfig() {}

	void _fromDictionary(const Dictionary &d)
		throw(std::invalid_argument);

	unsigned char _etWhitelist[65536 / 8];
	uint64_t _nwid;
	uint64_t _timestamp;
	Address _issuedTo;
	unsigned int _multicastPrefixBits;
	unsigned int _multicastDepth;
	unsigned int _arpCacheTtl;
	unsigned int _ndpCacheTtl;
	bool _emulateArp;
	bool _emulateNdp;
	bool _isOpen;
	std::string _name;
	std::string _description;
	std::set<InetAddress> _staticIps;
	MulticastRateTable _multicastRates;

	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif
