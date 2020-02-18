/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_ENDPOINT_HPP
#define ZT_ENDPOINT_HPP

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Address.hpp"
#include "Utils.hpp"
#include "TriviallyCopyable.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

// max name size + type byte + port (for DNS name/port) + 3x 16-bit coordinate for location
#define ZT_ENDPOINT_MARSHAL_SIZE_MAX (ZT_ENDPOINT_MAX_NAME_SIZE+1+2+2+2+2)

namespace ZeroTier {

/**
 * Endpoint variant specifying some form of network endpoint
 *
 * This data structure supports a number of types that are not yet actually used:
 * DNSNAME, URL, and ETHERNET. These are present to reserve them for future use.
 */
class Endpoint : public TriviallyCopyable
{
public:
	enum Type
	{
		NIL =          0,   // NIL value
		INETADDR_V4 =  1,   // IPv4
		INETADDR_V6 =  2,   // IPv6
		DNSNAME =      3,   // DNS name and port that resolves to InetAddress
		ZEROTIER =     4,   // ZeroTier Address (for relaying and meshy behavior)
		URL =          5,   // URL for http/https/ws/etc. (not implemented yet)
		ETHERNET =     6,   // 48-bit LAN-local Ethernet address
		WEBRTC =       7,   // WebRTC data channels
		UNRECOGNIZED = 255  // Unrecognized endpoint type encountered in stream
	};

	ZT_ALWAYS_INLINE Endpoint() noexcept { memoryZero(this); }

	explicit ZT_ALWAYS_INLINE Endpoint(const InetAddress &sa) { *this = sa; }

	ZT_ALWAYS_INLINE Endpoint(const Address &zt,const uint8_t identityHash[ZT_IDENTITY_HASH_SIZE]) :
		_t(ZEROTIER)
	{
		_v.zt.a = zt.toInt();
		memcpy(_v.zt.idh,identityHash,ZT_IDENTITY_HASH_SIZE);
	}

	ZT_ALWAYS_INLINE Endpoint(const char *name,const int port) :
		_t(DNSNAME)
	{
		_v.dns.port = port;
		Utils::scopy(_v.dns.name,sizeof(_v.dns.name),name);
	}

	explicit ZT_ALWAYS_INLINE Endpoint(const char *url) :
		_t(URL)
	{ Utils::scopy(_v.url,sizeof(_v.url),url); }

	ZT_ALWAYS_INLINE Endpoint &operator=(const InetAddress &sa)
	{
		switch(sa.ss_family) {
			case AF_INET:
				_t = INETADDR_V4;
				break;
			case AF_INET6:
				_t = INETADDR_V6;
				break;
			default:
				_t = NIL;
				return *this;
		}
		_v.sa = sa;
		return *this;
	}

	/**
	 * @return InetAddress or NIL if not of this type
	 */
	ZT_ALWAYS_INLINE const InetAddress &inetAddr() const noexcept { return ((_t == INETADDR_V4)||(_t == INETADDR_V6)) ? *reinterpret_cast<const InetAddress *>(&_v.sa) : InetAddress::NIL; }

	/**
	 * @return DNS name or empty string if not of this type
	 */
	ZT_ALWAYS_INLINE const char *dnsName() const noexcept { return (_t == DNSNAME) ? _v.dns.name : ""; }

	/**
	 * @return Port associated with DNS name or -1 if not of this type
	 */
	ZT_ALWAYS_INLINE int dnsPort() const noexcept { return (_t == DNSNAME) ? _v.dns.port : -1; }

	/**
	 * @return ZeroTier address or NIL if not of this type
	 */
	ZT_ALWAYS_INLINE Address ztAddress() const noexcept { return Address((_t == ZEROTIER) ? _v.zt.a : (uint64_t)0); }

	/**
	 * @return 384-bit hash of identity keys or NULL if not of this type
	 */
	ZT_ALWAYS_INLINE const uint8_t *ztIdentityHash() const noexcept { return (_t == ZEROTIER) ? _v.zt.idh : nullptr; }

	/**
	 * @return URL or empty string if not of this type
	 */
	ZT_ALWAYS_INLINE const char *url() const noexcept { return (_t == URL) ? _v.url : ""; }

	/**
	 * @return Ethernet address or NIL if not of this type
	 */
	ZT_ALWAYS_INLINE MAC ethernet() const noexcept { return (_t == ETHERNET) ? MAC(_v.eth) : MAC(); }

	/**
	 * @return Endpoint type or NIL if unset/empty
	 */
	ZT_ALWAYS_INLINE Type type() const noexcept { return _t; }

	explicit ZT_ALWAYS_INLINE operator bool() const noexcept { return _t != NIL; }

	bool operator==(const Endpoint &ep) const;
	ZT_ALWAYS_INLINE bool operator!=(const Endpoint &ep) const { return (!(*this == ep)); }
	bool operator<(const Endpoint &ep) const;
	ZT_ALWAYS_INLINE bool operator>(const Endpoint &ep) const { return (ep < *this); }
	ZT_ALWAYS_INLINE bool operator<=(const Endpoint &ep) const { return !(ep < *this); }
	ZT_ALWAYS_INLINE bool operator>=(const Endpoint &ep) const { return !(*this < ep); }

	static constexpr int marshalSizeMax() noexcept { return ZT_ENDPOINT_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_ENDPOINT_MARSHAL_SIZE_MAX]) const noexcept;
	int unmarshal(const uint8_t *restrict data,int len) noexcept;

private:
	Type _t;
	int _l[3]; // X,Y,Z location in kilometers from the nearest gravitational center of mass
	union {
		struct sockaddr_storage sa;
		struct {
			uint16_t port;
			char name[ZT_ENDPOINT_MAX_NAME_SIZE];
		} dns;
		struct {
			uint16_t offerLen;
			uint8_t offer[ZT_ENDPOINT_MAX_NAME_SIZE];
		} webrtc;
		struct {
			uint64_t a;
			uint8_t idh[ZT_IDENTITY_HASH_SIZE];
		} zt;
		char url[ZT_ENDPOINT_MAX_NAME_SIZE];
		uint64_t eth;
	} _v;
};

} // namespace ZeroTier

#endif
