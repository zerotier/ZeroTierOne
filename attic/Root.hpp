/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

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
 */
class Root
{
public:
	ZT_ALWAYS_INLINE Root() : _dnsPublicKeySize(0) {}

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
	ZT_ALWAYS_INLINE Root(S dn,const uint8_t *const dnspk,const unsigned int dnspksize,const Identity &dflId,const std::vector<InetAddress> &dflAddrs) :
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

	/**
	 * @return Current identity (either default or latest locator)
	 */
	ZT_ALWAYS_INLINE const Identity id() const
	{
		if (_lastFetchedLocator.id())
			return _lastFetchedLocator.id();
		return _defaultIdentity;
	}

	/**
	 * @param id Identity to check
	 * @return True if identity equals this root's current identity
	 */
	ZT_ALWAYS_INLINE bool is(const Identity &id) const
	{
		return ((_lastFetchedLocator.id()) ? (id == _lastFetchedLocator.id()) : (id == _defaultIdentity));
	}

	/**
	 * @return Current ZeroTier address (either default or latest locator)
	 */
	ZT_ALWAYS_INLINE const Address address() const
	{
		if (_lastFetchedLocator.id())
			return _lastFetchedLocator.id().address();
		return _defaultIdentity.address();
	}

	/**
	 * @return DNS name for this root or empty string if static entry with no DNS
	 */
	ZT_ALWAYS_INLINE const Str dnsName() const { return _dnsName; }

	/**
	 * @return Latest locator or NIL locator object if none
	 */
	ZT_ALWAYS_INLINE Locator locator() const { return _lastFetchedLocator; }

	/**
	 * @return Timestamp of latest retrieved locator or 0 if none
	 */
	ZT_ALWAYS_INLINE int64_t locatorTimestamp() const { return _lastFetchedLocator.timestamp(); }

	/**
	 * Update locator, returning true if new locator is valid and newer than existing
	 */
	ZT_ALWAYS_INLINE bool updateLocator(const Locator &loc)
	{
		if (!loc.verify())
			return false;
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
	ZT_ALWAYS_INLINE bool updateLocatorFromTxt(I start,I end)
	{
		try {
			if (_dnsPublicKeySize != ZT_ECC384_PUBLIC_KEY_SIZE)
				return false;
			Locator loc;
			if (!loc.decodeTxtRecords(start,end,_dnsPublicKey)) // also does verify()
				return false;
			if ((loc.phy().size() > 0)&&(loc.timestamp() > _lastFetchedLocator.timestamp())) {
				_lastFetchedLocator = loc;
				return true;
			}
			return false;
		} catch ( ... ) {}
		return false;
	}

	/**
	 * Pick a random physical IP for this root with the given address family
	 *
	 * @param addressFamily AF_INET or AF_INET6
	 * @return Address or InetAddress::NIL if no addresses exist for the given family
	 */
	ZT_ALWAYS_INLINE const InetAddress &pickPhysical(const int addressFamily) const
	{
		std::vector<const InetAddress *> pickList;
		const std::vector<InetAddress> *const av = (_lastFetchedLocator) ? &(_lastFetchedLocator.phy()) : &_defaultAddresses;
		for(std::vector<InetAddress>::const_iterator i(av->begin());i!=av->end();++i) {
			if (addressFamily == (int)i->ss_family) {
				pickList.push_back(&(*i));
			}
		}
		if (pickList.size() == 1)
			return *pickList[0];
		else if (pickList.size() > 1)
			return *pickList[(unsigned long)Utils::random() % (unsigned long)pickList.size()];
		return InetAddress::NIL;
	}

private:
	Identity _defaultIdentity;
	std::vector<InetAddress> _defaultAddresses;
	Str _dnsName;
	Locator _lastFetchedLocator;
	unsigned int _dnsPublicKeySize;
	uint8_t _dnsPublicKey[ZT_ECC384_PUBLIC_KEY_SIZE];
};

} // namespace ZeroTier

#endif
