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

#include "Endpoint.hpp"

namespace ZeroTier {

bool Endpoint::operator==(const Endpoint &ep) const
{
	if (_t == ep._t) {
		switch(_t) {
			case INETADDR: return (*sockaddr() == *ep.sockaddr());
			case DNSNAME:  return ((_v.dns.port == ep._v.dns.port)&&(strcmp(_v.dns.name,ep._v.dns.name) == 0));
			case ZEROTIER: return ((_v.zt.a == ep._v.zt.a)&&(memcmp(_v.zt.idh,ep._v.zt.idh,sizeof(_v.zt.idh)) == 0));
			case URL:      return (strcmp(_v.url,ep._v.url) == 0);
			case ETHERNET: return (_v.eth == ep._v.eth);
			default:       return true;
		}
	}
	return false;
}

bool Endpoint::operator<(const Endpoint &ep) const
{
	if ((int)_t < (int)ep._t) {
		return true;
	} else if (_t == ep._t) {
		int ncmp;
		switch(_t) {
			case INETADDR: return (*sockaddr() < *ep.sockaddr());
			case DNSNAME:
				ncmp = strcmp(_v.dns.name,ep._v.dns.name);
				return ((ncmp < 0) ? true : (ncmp == 0)&&(_v.dns.port < ep._v.dns.port));
			case ZEROTIER: return (_v.zt.a < ep._v.zt.a) ? true : ((_v.zt.a == ep._v.zt.a)&&(memcmp(_v.zt.idh,ep._v.zt.idh,sizeof(_v.zt.idh)) < 0));
			case URL:      return (strcmp(_v.url,ep._v.url) < 0);
			case ETHERNET: return (_v.eth < ep._v.eth);
			default:       return false;
		}
	}
	return false;
}

int Endpoint::marshal(uint8_t data[ZT_ENDPOINT_MARSHAL_SIZE_MAX]) const
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
			data[p++] = (uint8_t)(_v.dns.port >> 8U);
			data[p++] = (uint8_t)_v.dns.port;
			return p;
		case ZEROTIER:
			data[0] = (uint8_t)ZEROTIER;
			data[1] = (uint8_t)(_v.zt.a >> 32U);
			data[2] = (uint8_t)(_v.zt.a >> 24U);
			data[3] = (uint8_t)(_v.zt.a >> 16U);
			data[4] = (uint8_t)(_v.zt.a >> 8U);
			data[5] = (uint8_t)_v.zt.a;
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
			data[1] = (uint8_t)(_v.eth >> 40U);
			data[2] = (uint8_t)(_v.eth >> 32U);
			data[3] = (uint8_t)(_v.eth >> 24U);
			data[4] = (uint8_t)(_v.eth >> 16U);
			data[5] = (uint8_t)(_v.eth >> 8U);
			data[6] = (uint8_t)_v.eth;
			return 7;
		default:
			data[0] = (uint8_t)NIL;
			return 1;
	}
}

int Endpoint::unmarshal(const uint8_t *restrict data,const int len)
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
					return -1;
			}
			_v.dns.port = (uint16_t)(((unsigned int)data[p++]) << 8U);
			_v.dns.port |= (uint16_t)data[p++];
			return p;
		case ZEROTIER:
			if (len < (ZT_IDENTITY_HASH_SIZE + 6))
				return -1;
			_t = ZEROTIER;
			_v.zt.a = ((uint64_t)data[1]) << 32U;
			_v.zt.a |= ((uint64_t)data[2]) << 24U;
			_v.zt.a |= ((uint64_t)data[3]) << 16U;
			_v.zt.a |= ((uint64_t)data[4]) << 8U;
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
			_v.eth = ((uint64_t)data[1]) << 40U;
			_v.eth |= ((uint64_t)data[2]) << 32U;
			_v.eth |= ((uint64_t)data[3]) << 24U;
			_v.eth |= ((uint64_t)data[4]) << 16U;
			_v.eth |= ((uint64_t)data[5]) << 8U;
			_v.eth |= (uint64_t)data[6];
			return 7;
		default:
			// Unrecognized endpoint types not yet specified must start with a byte
			// length size so that older versions of ZeroTier can skip them.
			if (len < 2)
				return -1;
			_t = UNRECOGNIZED;
			return 1 + (int)data[1];
	}
	return false;
}

} // namespace ZeroTier
