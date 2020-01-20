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

#ifndef ZT_ENDPOINT_HPP
#define ZT_ENDPOINT_HPP

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Address.hpp"
#include "Utils.hpp"

// max name size + type byte + port (for DNS name/port) + 3x 16-bit coordinate for location
#define ZT_ENDPOINT_MARSHAL_SIZE_MAX (ZT_ENDPOINT_MAX_NAME_SIZE+1+2+2+2+2)

namespace ZeroTier {

/**
 * Endpoint variant specifying some form of network endpoint
 *
 * This data structure supports a number of types that are not yet actually used:
 * DNSNAME, URL, and ETHERNET. These are present to reserve them for future use.
 */
class Endpoint
{
public:
	enum Type
	{
		NIL =      0,      // NIL value
		INETADDR = 1,      // InetAddress (v4 or v6)
		DNSNAME =  2,      // DNS name and port that resolves to InetAddress
		ZEROTIER = 3,      // ZeroTier Address (for relaying and meshy behavior)
		URL =      4,      // URL for http/https/ws/etc. (not implemented yet)
		ETHERNET = 5,      // 48-bit LAN-local Ethernet address
		UNRECOGNIZED = 255 // Unrecognized endpoint type encountered in stream
	};

	ZT_ALWAYS_INLINE Endpoint()
	{
		memset(reinterpret_cast<void *>(this),0,sizeof(Endpoint));
	}

	ZT_ALWAYS_INLINE Endpoint(const Endpoint &ep)
	{
		memcpy(reinterpret_cast<void *>(this),&ep,sizeof(Endpoint));
	}

	explicit ZT_ALWAYS_INLINE Endpoint(const InetAddress &sa) :
		_t(INETADDR)
	{
		_v.sa = sa;
	}

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
	{
		Utils::scopy(_v.url,sizeof(_v.url),url);
	}

	ZT_ALWAYS_INLINE Endpoint &operator=(const Endpoint &ep)
	{
		memcpy(reinterpret_cast<void *>(this),&ep,sizeof(Endpoint));
		return *this;
	}

	ZT_ALWAYS_INLINE Endpoint &operator=(const InetAddress &sa)
	{
		_t = INETADDR;
		_v.sa = sa;
		return *this;
	}

	/**
	 * @return InetAddress or NIL if not of this type
	 */
	ZT_ALWAYS_INLINE const InetAddress &inetAddr() const { return (_t == INETADDR) ? *reinterpret_cast<const InetAddress *>(&_v.sa) : InetAddress::NIL; }

	/**
	 * @return DNS name or empty string if not of this type
	 */
	ZT_ALWAYS_INLINE const char *dnsName() const { return (_t == DNSNAME) ? _v.dns.name : ""; }

	/**
	 * @return Port associated with DNS name or -1 if not of this type
	 */
	ZT_ALWAYS_INLINE int dnsPort() const { return (_t == DNSNAME) ? _v.dns.port : -1; }

	/**
	 * @return ZeroTier address or NIL if not of this type
	 */
	ZT_ALWAYS_INLINE Address ztAddress() const { return Address((_t == ZEROTIER) ? _v.zt.a : (uint64_t)0); }

	/**
	 * @return 384-bit hash of identity keys or NULL if not of this type
	 */
	ZT_ALWAYS_INLINE const uint8_t *ztIdentityHash() const { return (_t == ZEROTIER) ? _v.zt.idh : nullptr; }

	/**
	 * @return URL or empty string if not of this type
	 */
	ZT_ALWAYS_INLINE const char *url() const { return (_t == URL) ? _v.url : ""; }

	/**
	 * @return Ethernet address or NIL if not of this type
	 */
	ZT_ALWAYS_INLINE MAC ethernet() const { return (_t == ETHERNET) ? MAC(_v.eth) : MAC(); }

	/**
	 * @return Endpoint type or NIL if unset/empty
	 */
	ZT_ALWAYS_INLINE Type type() const { return _t; }

	explicit ZT_ALWAYS_INLINE operator bool() const { return _t != NIL; }

	bool operator==(const Endpoint &ep) const;
	ZT_ALWAYS_INLINE bool operator!=(const Endpoint &ep) const { return (!(*this == ep)); }
	bool operator<(const Endpoint &ep) const;
	ZT_ALWAYS_INLINE bool operator>(const Endpoint &ep) const { return (ep < *this); }
	ZT_ALWAYS_INLINE bool operator<=(const Endpoint &ep) const { return !(ep < *this); }
	ZT_ALWAYS_INLINE bool operator>=(const Endpoint &ep) const { return !(*this < ep); }

	static ZT_ALWAYS_INLINE int marshalSizeMax() { return ZT_ENDPOINT_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_ENDPOINT_MARSHAL_SIZE_MAX]) const;
	int unmarshal(const uint8_t *restrict data,const int len);

private:
	Type _t;
	int _l[3]; // X,Y,Z location in kilometers from the nearest gravitational center of mass
	union {
		struct sockaddr_storage sa;
		ZT_PACKED_STRUCT(struct {
			uint16_t port;
			char name[ZT_ENDPOINT_MAX_NAME_SIZE];
		}) dns;
		ZT_PACKED_STRUCT(struct {
			uint64_t a;
			uint8_t idh[ZT_IDENTITY_HASH_SIZE];
		}) zt;
		char url[ZT_ENDPOINT_MAX_NAME_SIZE];
		uint64_t eth;
	} _v;
};

} // namespace ZeroTier

#endif
