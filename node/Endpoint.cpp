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
#include "Utils.hpp"

namespace ZeroTier {

Endpoint::Endpoint(const InetAddress &sa,const Protocol proto) noexcept // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
{
	switch (sa.family()) {
		case AF_INET:
			_t = TYPE_INETADDR_V4;
			break;
		case AF_INET6:
			_t = TYPE_INETADDR_V6;
			break;
		default:
			_t = TYPE_NIL;
			return;
	}
	_proto = proto;
	asInetAddress(_v.sa) = sa;
}

bool Endpoint::operator==(const Endpoint &ep) const noexcept
{
	if ((_t == ep._t)&&(_proto == ep._proto)) {
		switch(_t) {
			default:
				return true;
			case TYPE_ZEROTIER:
				return ((_v.zt.address == ep._v.zt.address) && (memcmp(_v.zt.hash,ep._v.zt.hash,sizeof(_v.zt.hash)) == 0));
			case TYPE_ETHERNET:
				return memcmp(_v.eth,ep._v.eth,6) == 0;
			case TYPE_INETADDR_V4:
			case TYPE_INETADDR_V6:
				return asInetAddress(_v.sa) == asInetAddress(ep._v.sa);
		}
	}
	return false;
}

bool Endpoint::operator<(const Endpoint &ep) const noexcept
{
	if ((int)_t < (int)ep._t) {
		return true;
	} else if (_t == ep._t) {
		if ((int)_proto < (int)ep._proto) {
			return true;
		} else {
			switch (_t) {
				case TYPE_ZEROTIER:
					return (_v.zt.address < ep._v.zt.address) ? true : ((_v.zt.address == ep._v.zt.address) && (memcmp(_v.zt.hash,ep._v.zt.hash,sizeof(_v.zt.hash)) < 0));
				case TYPE_ETHERNET:
					return memcmp(_v.eth,ep._v.eth,6) < 0;
				case TYPE_INETADDR_V4:
				case TYPE_INETADDR_V6:
					return asInetAddress(_v.sa) < asInetAddress(ep._v.sa);
				default:
					return false;
			}
		}
	}
	return false;
}

int Endpoint::marshal(uint8_t data[ZT_ENDPOINT_MARSHAL_SIZE_MAX]) const noexcept
{
	data[0] = (uint8_t)_t;
	Utils::storeBigEndian(data + 1,(uint16_t)_proto);
	Utils::storeBigEndian(data + 3,(uint16_t)_l[0]);
	Utils::storeBigEndian(data + 5,(uint16_t)_l[1]);
	Utils::storeBigEndian(data + 7,(uint16_t)_l[2]);

	int p;
	switch(_t) {
		case TYPE_ZEROTIER:
			data[9] = (uint8_t)(_v.zt.address >> 32U);
			data[10] = (uint8_t)(_v.zt.address >> 24U);
			data[11] = (uint8_t)(_v.zt.address >> 16U);
			data[12] = (uint8_t)(_v.zt.address >> 8U);
			data[13] = (uint8_t)_v.zt.address;
			Utils::copy<ZT_FINGERPRINT_HASH_SIZE>(data + 14,_v.zt.hash);
			return ZT_FINGERPRINT_HASH_SIZE + 14;
		case TYPE_ETHERNET:
			Utils::copy<6>(data + 9,_v.eth);
			return 15;
		case TYPE_INETADDR_V4:
		case TYPE_INETADDR_V6:
			p = 9 + asInetAddress(_v.sa).marshal(data + 7);
			if (p <= 9)
				return -1;
			return p;
		default:
			data[0] = (uint8_t)TYPE_NIL;
			return 1;
	}
}

int Endpoint::unmarshal(const uint8_t *restrict data,const int len) noexcept
{
	if (len < 1)
		return -1;

	_t = (Type)data[0];
	if (_t == TYPE_NIL)
		return 1;

	_proto = (Protocol)Utils::loadBigEndian<uint16_t>(data + 1);
	_l[0] = (int)Utils::loadBigEndian<uint16_t>(data + 3);
	_l[1] = (int)Utils::loadBigEndian<uint16_t>(data + 5);
	_l[2] = (int)Utils::loadBigEndian<uint16_t>(data + 7);

	int p;
  switch(_t) {
	  case TYPE_ZEROTIER:
		  if (len < (14 + ZT_FINGERPRINT_HASH_SIZE))
			  return -1;
		  _v.zt.address = ((uint64_t)data[9]) << 32U;
		  _v.zt.address |= ((uint64_t)data[10]) << 24U;
		  _v.zt.address |= ((uint64_t)data[11]) << 16U;
		  _v.zt.address |= ((uint64_t)data[12]) << 8U;
		  _v.zt.address |= (uint64_t)data[13];
		  Utils::copy<ZT_FINGERPRINT_HASH_SIZE>(_v.zt.hash,data + 14);
		  return ZT_FINGERPRINT_HASH_SIZE + 14;
	  case TYPE_ETHERNET:
		  if (len < 15)
			  return -1;
		  Utils::copy<6>(_v.eth,data + 9);
		  return 15;
		case TYPE_INETADDR_V4:
		case TYPE_INETADDR_V6:
			if (len <= 9)
				return -1;
			p = 9 + asInetAddress(_v.sa).unmarshal(data + 9,len - 9);
			if ((p <= 9)||(p >= len))
				return -1;
			return p;
		default:
			// Unrecognized endpoint types not yet specified must start with a 16-bit
			// length so that older versions of ZeroTier can skip them.
			if (len < 11)
				return -1;
			p = 11 + (int)Utils::loadBigEndian<uint16_t>(data + 9);
			return (p > len) ? -1 : p;
	}
}

} // namespace ZeroTier
