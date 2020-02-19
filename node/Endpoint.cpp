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

#include "Endpoint.hpp"

namespace ZeroTier {

bool Endpoint::operator==(const Endpoint &ep) const
{
	if (_t == ep._t) {
		switch(_t) {
			default:               return true;
			case TYPE_ZEROTIER:    return ((_v.zt.a == ep._v.zt.a)&&(memcmp(_v.zt.idh,ep._v.zt.idh,sizeof(_v.zt.idh)) == 0));
			case TYPE_DNSNAME:     return ((_v.dns.port == ep._v.dns.port)&&(strcmp(_v.dns.name,ep._v.dns.name) == 0));
			case TYPE_URL:         return (strcmp(_v.url,ep._v.url) == 0);
			case TYPE_ETHERNET:    return (_v.eth == ep._v.eth);
			case TYPE_INETADDR_V4:
			case TYPE_INETADDR_V6: return (inetAddr() == ep.inetAddr());
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
			case TYPE_ZEROTIER:    return (_v.zt.a < ep._v.zt.a) ? true : ((_v.zt.a == ep._v.zt.a)&&(memcmp(_v.zt.idh,ep._v.zt.idh,sizeof(_v.zt.idh)) < 0));
			case TYPE_DNSNAME:
				ncmp = strcmp(_v.dns.name,ep._v.dns.name);
				                     return ((ncmp < 0) ? true : (ncmp == 0)&&(_v.dns.port < ep._v.dns.port));
			case TYPE_URL:         return (strcmp(_v.url,ep._v.url) < 0);
			case TYPE_ETHERNET:    return (_v.eth < ep._v.eth);
			case TYPE_INETADDR_V4:
			case TYPE_INETADDR_V6: return (inetAddr() < ep.inetAddr());
			default:               return false;
		}
	}
	return false;
}

int Endpoint::marshal(uint8_t data[ZT_ENDPOINT_MARSHAL_SIZE_MAX]) const noexcept
{
	int p;
	data[0] = (uint8_t)_t;
	Utils::storeBigEndian(data + 1,(int16_t)_l[0]);
	Utils::storeBigEndian(data + 3,(int16_t)_l[1]);
	Utils::storeBigEndian(data + 5,(int16_t)_l[2]);
	switch(_t) {
		case TYPE_ZEROTIER:
			data[7] = (uint8_t)(_v.zt.a >> 32U);
			data[8] = (uint8_t)(_v.zt.a >> 24U);
			data[9] = (uint8_t)(_v.zt.a >> 16U);
			data[10] = (uint8_t)(_v.zt.a >> 8U);
			data[11] = (uint8_t)_v.zt.a;
			memcpy(data + 12,_v.zt.idh,ZT_IDENTITY_HASH_SIZE);
			return ZT_IDENTITY_HASH_SIZE + 12;
		case TYPE_DNSNAME:
			p = 7;
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
		case TYPE_URL:
			p = 7;
			for (;;) {
				if ((data[p] = (uint8_t)_v.url[p-1]) == 0)
					break;
				++p;
				if (p == (ZT_ENDPOINT_MAX_NAME_SIZE+1))
					return -1;
			}
			return p;
		case TYPE_ETHERNET:
			data[7] = (uint8_t)(_v.eth >> 40U);
			data[8] = (uint8_t)(_v.eth >> 32U);
			data[9] = (uint8_t)(_v.eth >> 24U);
			data[10] = (uint8_t)(_v.eth >> 16U);
			data[11] = (uint8_t)(_v.eth >> 8U);
			data[12] = (uint8_t)_v.eth;
			return 13;
		case TYPE_INETADDR_V4:
		case TYPE_INETADDR_V6:
			p = asInetAddress(_v.sa).marshal(data + 7);
			if (p < 0)
				return p;
			return 7 + p;
		default:
			data[0] = (uint8_t)TYPE_NIL;
			return 7;
	}
}

int Endpoint::unmarshal(const uint8_t *restrict data,const int len) noexcept
{
	if (len < 7)
		return -1;
	int p;
	_t = (Type)data[0];
	_l[0] = Utils::loadBigEndian<int16_t>(data + 1);
	_l[1] = Utils::loadBigEndian<int16_t>(data + 3);
	_l[2] = Utils::loadBigEndian<int16_t>(data + 5);
  switch(_t) {
		case TYPE_NIL:
			return 7;
	  case TYPE_ZEROTIER:
		  if (len < (12 + ZT_IDENTITY_HASH_SIZE))
			  return -1;
		  _v.zt.a = ((uint64_t)data[7]) << 32U;
		  _v.zt.a |= ((uint64_t)data[8]) << 24U;
		  _v.zt.a |= ((uint64_t)data[9]) << 16U;
		  _v.zt.a |= ((uint64_t)data[10]) << 8U;
		  _v.zt.a |= (uint64_t)data[11];
		  memcpy(_v.zt.idh,data + 12,ZT_IDENTITY_HASH_SIZE);
		  return 60;
	  case TYPE_DNSNAME:
		  if (len < 10)
			  return -1;
		  p = 7;
		  for (;;) {
			  if ((_v.dns.name[p-1] = (char)data[p]) == 0) {
				  ++p;
				  break;
			  }
			  ++p;
			  if ((p >= (ZT_ENDPOINT_MARSHAL_SIZE_MAX-2))||(p >= (len-2)))
				  return -1;
		  }
		  _v.dns.port = (uint16_t)(((unsigned int)data[p++]) << 8U);
		  _v.dns.port |= (uint16_t)data[p++];
		  return p;
	  case TYPE_URL:
		  if (len < 8)
			  return -1;
		  p = 7;
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
	  case TYPE_ETHERNET:
		  if (len < 13)
			  return -1;
		  _v.eth = ((uint64_t)data[7]) << 40U;
		  _v.eth |= ((uint64_t)data[8]) << 32U;
		  _v.eth |= ((uint64_t)data[9]) << 24U;
		  _v.eth |= ((uint64_t)data[10]) << 16U;
		  _v.eth |= ((uint64_t)data[11]) << 8U;
		  _v.eth |= (uint64_t)data[12];
		  return 13;
		case TYPE_INETADDR_V4:
		case TYPE_INETADDR_V6:
			p = asInetAddress(_v.sa).unmarshal(data + 7,len - 7);
			if (p <= 0)
				return -1;
			return 7 + p;
		default:
			// Unrecognized endpoint types not yet specified must start with a 16-bit
			// length so that older versions of ZeroTier can skip them.
			if (len < 9)
				return -1;
			p = 9 + (int)Utils::loadBigEndian<uint16_t>(data + 7);
			return (p > len) ? -1 : p;
	}
}

} // namespace ZeroTier
