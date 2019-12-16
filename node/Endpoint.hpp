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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Address.hpp"
#include "Utils.hpp"

#define ZT_ENDPOINT_MARSHAL_SIZE_MAX (ZT_ENDPOINT_MAX_NAME_SIZE+4)

namespace ZeroTier {

/**
 * Endpoint variant specifying some form of network endpoint
 */
class Endpoint
{
public:
	enum Type
	{
		NIL =      0, // NIL value
		INETADDR = 1, // InetAddress (v4 or v6)
		DNSNAME =  2, // DNS name and port that resolves to InetAddress
		ZEROTIER = 3, // ZeroTier Address (for relaying and meshy behavior)
		URL =      4, // URL for http/https/ws/etc. (not implemented yet)
		ETHERNET = 5  // 48-bit LAN-local Ethernet address
	};

	inline Endpoint() { memset(reinterpret_cast<void *>(this),0,sizeof(Endpoint)); }

	inline Endpoint(const InetAddress &sa) : _t(INETADDR) { _v.sa = sa; }
	inline Endpoint(const Address &zt,const uint8_t identityHash[ZT_IDENTITY_HASH_SIZE]) : _t(ZEROTIER) { _v.zt.a = zt.toInt(); memcpy(_v.zt.idh,identityHash,ZT_IDENTITY_HASH_SIZE); }
	inline Endpoint(const char *name,const int port) : _t(DNSNAME) { Utils::scopy(_v.dns.name,sizeof(_v.dns.name),name); _v.dns.port = port; }
	inline Endpoint(const char *url) : _t(URL) { Utils::scopy(_v.url,sizeof(_v.url),url); }

	inline const InetAddress *sockaddr() const { return (_t == INETADDR) ? reinterpret_cast<const InetAddress *>(&_v.sa) : nullptr; }
	inline const char *dnsName() const { return (_t == DNSNAME) ? _v.dns.name : nullptr; }
	inline const int dnsPort() const { return (_t == DNSNAME) ? _v.dns.port : -1; }
	inline Address ztAddress() const { return (_t == ZEROTIER) ? Address(_v.zt.a) : Address(); }
	inline const uint8_t *ztIdentityHash() const { return (_t == ZEROTIER) ? _v.zt.idh : nullptr; }
	inline const char *url() const { return (_t == URL) ? _v.url : nullptr; }
	inline MAC ethernet() const { return (_t == ETHERNET) ? MAC(_v.eth) : MAC(); }

	inline Type type() const { return _t; }

	static inline int marshalSizeMax() { return ZT_ENDPOINT_MARSHAL_SIZE_MAX; }
	inline int marshal(uint8_t data[ZT_ENDPOINT_MARSHAL_SIZE_MAX])
	{
		int p;
		switch(_t) {
			case INETADDR:
				data[0] = (uint8_t)INETADDR;
				return 1 + reinterpret_cast<const InetAddress *>(&_v.sa)->marshal(data+1);
			case DNSNAME:
				data[0] = (uint8_t)DNSNAME;
				p = 1;
				for (;;) {
					if ((data[p] = (uint8_t)_v.dns.name[p-1]) == 0)
						break;
					++p;
					if (p == (ZT_ENDPOINT_MAX_NAME_SIZE+1))
						return -1;
				}
				data[p++] = (uint8_t)((_v.dns.port >> 8) & 0xff);
				data[p++] = (uint8_t)(_v.dns.port & 0xff);
				return p;
			case ZEROTIER:
				data[0] = (uint8_t)ZEROTIER;
				data[1] = (uint8_t)((_v.zt.a >> 32) & 0xff);
				data[2] = (uint8_t)((_v.zt.a >> 24) & 0xff);
				data[3] = (uint8_t)((_v.zt.a >> 16) & 0xff);
				data[4] = (uint8_t)((_v.zt.a >> 8) & 0xff);
				data[5] = (uint8_t)(_v.zt.a & 0xff);
				memcpy(data + 6,_v.zt.idh,ZT_IDENTITY_HASH_SIZE);
				return (ZT_IDENTITY_HASH_SIZE + 6);
			case URL:
				data[0] = (uint8_t)URL;
				p = 1;
				for (;;) {
					if ((data[p] = (uint8_t)_v.url[p-1]) == 0)
						break;
					++p;
					if (p == (ZT_ENDPOINT_MAX_NAME_SIZE+1))
						return -1;
				}
				return p;
			case ETHERNET:
				data[0] = (uint8_t)ETHERNET;
				data[1] = (uint8_t)((_v.eth >> 40) & 0xff);
				data[2] = (uint8_t)((_v.eth >> 32) & 0xff);
				data[3] = (uint8_t)((_v.eth >> 24) & 0xff);
				data[4] = (uint8_t)((_v.eth >> 16) & 0xff);
				data[5] = (uint8_t)((_v.eth >> 8) & 0xff);
				data[6] = (uint8_t)(_v.eth & 0xff);
				return 7;
			default:
				data[0] = (uint8_t)NIL;
				return 1;
		}
	}
	inline int unmarshal(const uint8_t *restrict data,const int len)
	{
		if (len <= 0)
			return -1;
		int p;
		switch((Type)data[0]) {
			case NIL:
				_t = NIL;
				return 1;
			case INETADDR:
				_t = INETADDR;
				return reinterpret_cast<InetAddress *>(&_v.sa)->unmarshal(data+1,len-1);
			case DNSNAME:
				if (len < 4)
					return -1;
				_t = DNSNAME;
				p = 1;
				for (;;) {
					if ((_v.dns.name[p-1] = (char)data[p]) == 0) {
						++p;
						break;
					}
					++p;
					if ((p >= (ZT_ENDPOINT_MAX_NAME_SIZE+1))||(p >= (len-2)))
						return;
				}
				_v.dns.port = ((int)data[p++]) << 8;
				_v.dns.port |= (int)data[p++];
				return p;
			case ZEROTIER:
				if (len < (ZT_IDENTITY_HASH_SIZE + 6))
					return -1;
				_t = ZEROTIER;
				_v.zt.a = ((uint64_t)data[1]) << 32;
				_v.zt.a |= ((uint64_t)data[2]) << 24;
				_v.zt.a |= ((uint64_t)data[3]) << 16;
				_v.zt.a |= ((uint64_t)data[4]) << 8;
				_v.zt.a |= (uint64_t)data[5];
				memcpy(_v.zt.idh,data + 6,ZT_IDENTITY_HASH_SIZE);
				return (ZT_IDENTITY_HASH_SIZE + 6);
			case URL:
				if (len < 2)
					return -1;
				_t = URL;
				p = 1;
				for (;;) {
					if ((_v.url[p-1] = (char)data[p]) == 0) {
						++p;
						break;
					}
					++p;
					if ((p >= (ZT_ENDPOINT_MAX_NAME_SIZE+1))||(p >= len))
						return -1;
				}
				return p;
			case ETHERNET:
				if (len < 7)
					return -1;
				_t = ZEROTIER;
				_v.eth = ((uint64_t)data[1]) << 40;
				_v.eth |= ((uint64_t)data[2]) << 32;
				_v.eth |= ((uint64_t)data[3]) << 24;
				_v.eth |= ((uint64_t)data[4]) << 16;
				_v.eth |= ((uint64_t)data[5]) << 8;
				_v.eth |= (uint64_t)data[6];
				return 7;
		}
		return false;
	}

private:
	Type _t;
	union {
		struct sockaddr_storage sa;
		struct {
			char name[ZT_ENDPOINT_MAX_NAME_SIZE];
			int port;
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
