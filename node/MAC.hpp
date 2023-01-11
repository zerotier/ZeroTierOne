/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_MAC_HPP
#define ZT_MAC_HPP

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "Constants.hpp"
#include "Utils.hpp"
#include "Address.hpp"
#include "Buffer.hpp"

namespace ZeroTier {

/**
 * 48-byte Ethernet MAC address
 */
class MAC
{
public:
	MAC() : _m(0ULL) {}
	MAC(const MAC &m) : _m(m._m) {}

	MAC(const unsigned char a,const unsigned char b,const unsigned char c,const unsigned char d,const unsigned char e,const unsigned char f) :
		_m( ((((uint64_t)a) & 0xffULL) << 40) |
		    ((((uint64_t)b) & 0xffULL) << 32) |
		    ((((uint64_t)c) & 0xffULL) << 24) |
		    ((((uint64_t)d) & 0xffULL) << 16) |
		    ((((uint64_t)e) & 0xffULL) << 8) |
		    (((uint64_t)f) & 0xffULL) ) {}
	MAC(const void *bits,unsigned int len) { setTo(bits,len); }
	MAC(const Address &ztaddr,uint64_t nwid) { fromAddress(ztaddr,nwid); }
	MAC(const uint64_t m) : _m(m & 0xffffffffffffULL) {}

	/**
	 * @return MAC in 64-bit integer
	 */
	inline uint64_t toInt() const { return _m; }

	/**
	 * Set MAC to zero
	 */
	inline void zero() { _m = 0ULL; }

	/**
	 * @return True if MAC is non-zero
	 */
	inline operator bool() const { return (_m != 0ULL); }

	/**
	 * @param bits Raw MAC in big-endian byte order
	 * @param len Length, must be >= 6 or result is zero
	 */
	inline void setTo(const void *bits,unsigned int len)
	{
		if (len < 6) {
			_m = 0ULL;
			return;
		}
		const unsigned char *b = (const unsigned char *)bits;
		_m =  ((((uint64_t)*b) & 0xff) << 40); ++b;
		_m |= ((((uint64_t)*b) & 0xff) << 32); ++b;
		_m |= ((((uint64_t)*b) & 0xff) << 24); ++b;
		_m |= ((((uint64_t)*b) & 0xff) << 16); ++b;
		_m |= ((((uint64_t)*b) & 0xff) << 8); ++b;
		_m |= (((uint64_t)*b) & 0xff);
	}

	/**
	 * @param buf Destination buffer for MAC in big-endian byte order
	 * @param len Length of buffer, must be >= 6 or nothing is copied
	 */
	inline void copyTo(void *buf,unsigned int len) const
	{
		if (len < 6)
			return;
		unsigned char *b = (unsigned char *)buf;
		*(b++) = (unsigned char)((_m >> 40) & 0xff);
		*(b++) = (unsigned char)((_m >> 32) & 0xff);
		*(b++) = (unsigned char)((_m >> 24) & 0xff);
		*(b++) = (unsigned char)((_m >> 16) & 0xff);
		*(b++) = (unsigned char)((_m >> 8) & 0xff);
		*b = (unsigned char)(_m & 0xff);
	}

	/**
	 * Append to a buffer in big-endian byte order
	 *
	 * @param b Buffer to append to
	 */
	template<unsigned int C>
	inline void appendTo(Buffer<C> &b) const
	{
		unsigned char *p = (unsigned char *)b.appendField(6);
		*(p++) = (unsigned char)((_m >> 40) & 0xff);
		*(p++) = (unsigned char)((_m >> 32) & 0xff);
		*(p++) = (unsigned char)((_m >> 24) & 0xff);
		*(p++) = (unsigned char)((_m >> 16) & 0xff);
		*(p++) = (unsigned char)((_m >> 8) & 0xff);
		*p = (unsigned char)(_m & 0xff);
	}

	/**
	 * @return True if this is broadcast (all 0xff)
	 */
	inline bool isBroadcast() const { return (_m == 0xffffffffffffULL); }

	/**
	 * @return True if this is a multicast MAC
	 */
	inline bool isMulticast() const { return ((_m & 0x010000000000ULL) != 0ULL); }

	/**
	 * @param True if this is a locally-administered MAC
	 */
	inline bool isLocallyAdministered() const { return ((_m & 0x020000000000ULL) != 0ULL); }

	/**
	 * Set this MAC to a MAC derived from an address and a network ID
	 *
	 * @param ztaddr ZeroTier address
	 * @param nwid 64-bit network ID
	 */
	inline void fromAddress(const Address &ztaddr,uint64_t nwid)
	{
		uint64_t m = ((uint64_t)firstOctetForNetwork(nwid)) << 40;
		m |= ztaddr.toInt(); // a is 40 bits
		m ^= ((nwid >> 8) & 0xff) << 32;
		m ^= ((nwid >> 16) & 0xff) << 24;
		m ^= ((nwid >> 24) & 0xff) << 16;
		m ^= ((nwid >> 32) & 0xff) << 8;
		m ^= (nwid >> 40) & 0xff;
		_m = m;
	}

	/**
	 * Get the ZeroTier address for this MAC on this network (assuming no bridging of course, basic unicast)
	 *
	 * This just XORs the next-least-significant 5 bytes of the network ID again to unmask.
	 *
	 * @param nwid Network ID
	 */
	inline Address toAddress(uint64_t nwid) const
	{
		uint64_t a = _m & 0xffffffffffULL; // least significant 40 bits of MAC are formed from address
		a ^= ((nwid >> 8) & 0xff) << 32; // ... XORed with bits 8-48 of the nwid in little-endian byte order, so unmask it
		a ^= ((nwid >> 16) & 0xff) << 24;
		a ^= ((nwid >> 24) & 0xff) << 16;
		a ^= ((nwid >> 32) & 0xff) << 8;
		a ^= (nwid >> 40) & 0xff;
		return Address(a);
	}

	/**
	 * @param nwid Network ID
	 * @return First octet of MAC for this network
	 */
	static inline unsigned char firstOctetForNetwork(uint64_t nwid)
	{
		unsigned char a = ((unsigned char)(nwid & 0xfe) | 0x02); // locally administered, not multicast, from LSB of network ID
		return ((a == 0x52) ? 0x32 : a); // blacklist 0x52 since it's used by KVM, libvirt, and other popular virtualization engines... seems de-facto standard on Linux
	}

	/**
	 * @param i Value from 0 to 5 (inclusive)
	 * @return Byte at said position (address interpreted in big-endian order)
	 */
	inline unsigned char operator[](unsigned int i) const { return (unsigned char)((_m >> (40 - (i * 8))) & 0xff); }

	/**
	 * @return 6, which is the number of bytes in a MAC, for container compliance
	 */
	inline unsigned int size() const { return 6; }

	inline unsigned long hashCode() const { return (unsigned long)_m; }

	inline char *toString(char buf[18]) const
	{
		buf[0] = Utils::HEXCHARS[(_m >> 44) & 0xf];
		buf[1] = Utils::HEXCHARS[(_m >> 40) & 0xf];
		buf[2] = ':';
		buf[3] = Utils::HEXCHARS[(_m >> 36) & 0xf];
		buf[4] = Utils::HEXCHARS[(_m >> 32) & 0xf];
		buf[5] = ':';
		buf[6] = Utils::HEXCHARS[(_m >> 28) & 0xf];
		buf[7] = Utils::HEXCHARS[(_m >> 24) & 0xf];
		buf[8] = ':';
		buf[9] = Utils::HEXCHARS[(_m >> 20) & 0xf];
		buf[10] = Utils::HEXCHARS[(_m >> 16) & 0xf];
		buf[11] = ':';
		buf[12] = Utils::HEXCHARS[(_m >> 12) & 0xf];
		buf[13] = Utils::HEXCHARS[(_m >> 8) & 0xf];
		buf[14] = ':';
		buf[15] = Utils::HEXCHARS[(_m >> 4) & 0xf];
		buf[16] = Utils::HEXCHARS[_m & 0xf];
		buf[17] = (char)0;
		return buf;
	}

	inline MAC &operator=(const MAC &m)
	{
		_m = m._m;
		return *this;
	}
	inline MAC &operator=(const uint64_t m)
	{
		_m = m;
		return *this;
	}

	inline bool operator==(const MAC &m) const { return (_m == m._m); }
	inline bool operator!=(const MAC &m) const { return (_m != m._m); }
	inline bool operator<(const MAC &m) const { return (_m < m._m); }
	inline bool operator<=(const MAC &m) const { return (_m <= m._m); }
	inline bool operator>(const MAC &m) const { return (_m > m._m); }
	inline bool operator>=(const MAC &m) const { return (_m >= m._m); }

private:
	uint64_t _m;
};

} // namespace ZeroTier

#endif
