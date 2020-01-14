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

#define ZT_ENDPOINT_MARSHAL_SIZE_MAX (ZT_ENDPOINT_MAX_NAME_SIZE+3)

namespace ZeroTier {

/**
 * Endpoint variant specifying some form of network endpoint
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

	ZT_ALWAYS_INLINE Endpoint() { memset(reinterpret_cast<void *>(this),0,sizeof(Endpoint)); }

	explicit ZT_ALWAYS_INLINE Endpoint(const InetAddress &sa) : _t(INETADDR) { _v.sa = sa; }
	ZT_ALWAYS_INLINE Endpoint(const Address &zt,const uint8_t identityHash[ZT_IDENTITY_HASH_SIZE]) : _t(ZEROTIER) { _v.zt.a = zt.toInt(); memcpy(_v.zt.idh,identityHash,ZT_IDENTITY_HASH_SIZE); }
	ZT_ALWAYS_INLINE Endpoint(const char *name,const int port) : _t(DNSNAME) { Utils::scopy(_v.dns.name,sizeof(_v.dns.name),name); _v.dns.port = port; }
	explicit ZT_ALWAYS_INLINE Endpoint(const char *url) : _t(URL) { Utils::scopy(_v.url,sizeof(_v.url),url); }

	ZT_ALWAYS_INLINE const InetAddress *sockaddr() const { return (_t == INETADDR) ? reinterpret_cast<const InetAddress *>(&_v.sa) : nullptr; }
	ZT_ALWAYS_INLINE const char *dnsName() const { return (_t == DNSNAME) ? _v.dns.name : nullptr; }
	ZT_ALWAYS_INLINE int dnsPort() const { return (_t == DNSNAME) ? _v.dns.port : -1; }
	ZT_ALWAYS_INLINE Address ztAddress() const { return (_t == ZEROTIER) ? Address(_v.zt.a) : Address(); }
	ZT_ALWAYS_INLINE const uint8_t *ztIdentityHash() const { return (_t == ZEROTIER) ? _v.zt.idh : nullptr; }
	ZT_ALWAYS_INLINE const char *url() const { return (_t == URL) ? _v.url : nullptr; }
	ZT_ALWAYS_INLINE MAC ethernet() const { return (_t == ETHERNET) ? MAC(_v.eth) : MAC(); }

	ZT_ALWAYS_INLINE Type type() const { return _t; }

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
	union {
		struct sockaddr_storage sa;
		struct {
			char name[ZT_ENDPOINT_MAX_NAME_SIZE];
			uint16_t port;
		} dns;
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
