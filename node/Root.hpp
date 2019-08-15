/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_ROOT_HPP
#define ZT_ROOT_HPP

#include "Constants.hpp"
#include "Str.hpp"
#include "ECC384.hpp"
#include "Locator.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"
#include "Identity.hpp"
#include "Mutex.hpp"

namespace ZeroTier {

/**
 * A root entry pointing to a node capable of global identity lookup and indirect transit
 * 
 * Root entries point to DNS records that contain TXT entries that decode to Locator objects
 * pointing to actual root nodes. A default root identity and static addresses can also be
 * provided as fallback if DNS is not available.
 * 
 * Note that root identities can change if DNS returns a different result, but that DNS entries
 * are authenticated using their own signature scheme. This allows a root DNS name to serve
 * up different roots based on factors like location or relative load of different roots.
 * 
 * It's also possible to create a root with no DNS and no DNS validator public key. This root
 * will be a static entry pointing to a single root identity and set of physical addresses.
 * 
 * This object is thread-safe and may be concurrently accessed and updated.
 */
class Root
{
public:
	inline Root() : _dnsPublicKeySize(0) {}
	inline Root(const Root &r) { *this = r; }

	/**
	 * Create a new root entry
	 * 
	 * @param dn DNS name
	 * @param dnspk DNS public key for record validation
	 * @param dnspksize Size of DNS public key (currently always the size of a NIST P-384 point compressed public key)
	 * @param dflId Default identity if DNS is not available
	 * @param dflAddrs Default IP addresses if DNS is not available
	 */
	template<typename S>
	inline Root(S dn,const uint8_t *const dnspk,const unsigned int dnspksize,const Identity &dflId,const std::vector<InetAddress> &dflAddrs) :
		_defaultIdentity(dflId),
		_defaultAddresses(dflAddrs),
		_dnsName(dn),
		_dnsPublicKeySize(dnspksize)
	{
		if (dnspksize != 0) {
			if (dnspksize > sizeof(_dnsPublicKey))
				throw ZT_EXCEPTION_INVALID_ARGUMENT;
			memcpy(_dnsPublicKey,dnspk,dnspksize);
		}
	}

	inline Root &operator=(const Root &r)
	{
		Mutex::Lock l(_lock);
		Mutex::Lock rl(r._lock);
		_defaultIdentity = r._defaultIdentity;
		_defaultAddresses = r._defaultAddresses;
		_dnsName = r._dnsName;
		_lastFetchedLocator = r._lastFetchedLocator;
		_dnsPublicKeySize = r._dnsPublicKeySize;
		memcpy(_dnsPublicKey,r._dnsPublicKey,_dnsPublicKeySize);
		return *this;
	}

	/**
	 * @return Current identity (either default or latest locator)
	 */
	inline const Identity id() const
	{
		Mutex::Lock l(_lock);
		if (_lastFetchedLocator.id())
			return _lastFetchedLocator.id();
		return _defaultIdentity;
	}

	/**
	 * @param id Identity to check
	 * @return True if identity equals this root's current identity
	 */
	inline bool is(const Identity &id) const
	{
		Mutex::Lock l(_lock);
		return ((_lastFetchedLocator.id()) ? (id == _lastFetchedLocator.id()) : (id == _defaultIdentity));
	}

	/**
	 * @return Current ZeroTier address (either default or latest locator)
	 */
	inline const Address address() const
	{
		Mutex::Lock l(_lock);
		if (_lastFetchedLocator.id())
			return _lastFetchedLocator.id().address();
		return _defaultIdentity.address();
	}

	/**
	 * @return DNS name for this root (or empty string if none)
	 */
	inline const Str dnsName() const
	{
		Mutex::Lock l(_lock);
		return _dnsName;
	}

	/**
	 * @return Latest locator
	 */
	inline Locator locator() const
	{
		Mutex::Lock l(_lock);
		return _lastFetchedLocator;
	}

	/**
	 * @return Timestamp of latest retrieved locator
	 */
	inline int64_t locatorTimestamp() const
	{
		Mutex::Lock l(_lock);
		return _lastFetchedLocator.timestamp();
	}

	/**
	 * Pick a random physical address
	 * 
	 * @return Physical address or InetAddress::NIL if none are available
	 */
	inline const InetAddress randomPhysicalAddress() const
	{
		Mutex::Lock l(_lock);
		if (_lastFetchedLocator.phy().empty()) {
			if (_defaultAddresses.empty())
				return InetAddress::NIL;
			return _defaultAddresses[(unsigned long)Utils::random() % (unsigned long)_defaultAddresses.size()];
		}
		return _lastFetchedLocator.phy()[(unsigned long)Utils::random() % (unsigned long)_lastFetchedLocator.phy().size()];
	}

	/**
	 * Update locator, returning true if new locator is valid and newer than existing
	 */
	inline bool updateLocator(const Locator &loc)
	{
		if (!loc.verify())
			return false;
		Mutex::Lock l(_lock);
		if ((loc.phy().size() > 0)&&(loc.timestamp() > _lastFetchedLocator.timestamp())) {
			_lastFetchedLocator = loc;
			return true;
		}
		return false;
	}

	/**
	 * Update this root's locator from a series of TXT records
	 */
	template<typename I>
	inline bool updateLocatorFromTxt(I start,I end)
	{
		try {
			Mutex::Lock l(_lock);
			if (_dnsPublicKeySize != ZT_ECC384_PUBLIC_KEY_SIZE)
				return false;
			Locator loc;
			if (!loc.decodeTxtRecords(start,end,_dnsPublicKey))
				return false;
			if ((loc.phy().size() > 0)&&(loc.timestamp() > _lastFetchedLocator.timestamp())) {
				_lastFetchedLocator = loc;
				return true;
			}
			return false;
		} catch ( ... ) {}
		return false;
	}

private:
	Identity _defaultIdentity;
	std::vector<InetAddress> _defaultAddresses;
	Str _dnsName;
	Locator _lastFetchedLocator;
	unsigned int _dnsPublicKeySize;
	uint8_t _dnsPublicKey[ZT_ECC384_PUBLIC_KEY_SIZE];
	Mutex _lock;
};

} // namespace ZeroTier

#endif
