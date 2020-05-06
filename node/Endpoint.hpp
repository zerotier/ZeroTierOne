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
#include "Fingerprint.hpp"
#include "MAC.hpp"

#define ZT_ENDPOINT_MARSHAL_SIZE_MAX 128

static_assert(ZT_ENDPOINT_MARSHAL_SIZE_MAX > (ZT_INETADDRESS_MARSHAL_SIZE_MAX + 1),"ZT_ENDPOINT_MARSHAL_SIZE_MAX not large enough");
static_assert(ZT_ENDPOINT_MARSHAL_SIZE_MAX > (sizeof(ZT_Fingerprint) + 1),"ZT_ENDPOINT_MARSHAL_SIZE_MAX not large enough");

namespace ZeroTier {

/**
 * Endpoint variant specifying some form of network endpoint.
 * 
 * This is sort of a superset of InetAddress and for the standard UDP
 * protocol marshals and unmarshals to a compatible format. This makes
 * it backward compatible with older node versions' protocol fields
 * where InetAddress was used as long as only the UDP type is exchanged
 * with those nodes.
 */
class Endpoint : public TriviallyCopyable
{
public:
	/**
	 * Endpoint type (defined in the API)
	 */
	typedef ZT_EndpointType Type;

	/**
	 * Create a NIL/empty endpoint
	 */
	ZT_INLINE Endpoint() noexcept { memoryZero(this); }

	/**
	 * Create an endpoint for a type that uses an IP
	 * 
	 * @param a IP/port
	 * @param et Endpoint type (default: IP_UDP)
	 */
	ZT_INLINE Endpoint(const InetAddress &a,const Type et = ZT_ENDPOINT_TYPE_IP_UDP) noexcept
	{
		if (a) {
			Utils::copy<sizeof(InetAddress)>(m_value,&a);
			m_value[ZT_ENDPOINT_MARSHAL_SIZE_MAX-1] = (uint8_t)et;
		} else {
			memoryZero(this);
		}
	}

	/**
	 * Create an endpoint for ZeroTier relaying (ZEROTIER type)
	 * 
	 * @param zt_ ZeroTier identity fingerprint
	 */
	ZT_INLINE Endpoint(const Fingerprint &zt_) noexcept
	{
		if (zt_) {
			Utils::copy<sizeof(Fingerprint)>(m_value,&zt_);
			m_value[ZT_ENDPOINT_MARSHAL_SIZE_MAX-1] = (uint8_t)ZT_ENDPOINT_TYPE_ZEROTIER;
		} else {
			memoryZero(this);
		}
	}

	/**
	 * Create an endpoint for a type that uses a MAC address
	 * 
	 * @param eth_ Ethernet address
	 * @param et Endpoint type (default: ETHERNET)
	 */
	ZT_INLINE Endpoint(const MAC &eth_,const Type et = ZT_ENDPOINT_TYPE_ETHERNET) noexcept
	{
		if (eth_) {
			Utils::copy<sizeof(MAC)>(m_value,&eth_);
			m_value[ZT_ENDPOINT_MARSHAL_SIZE_MAX-1] = (uint8_t)et;
		} else {
			memoryZero(this);
		}
	}

	/**
	 * @return Endpoint type
	 */
	ZT_INLINE Type type() const noexcept { return (Type)m_value[ZT_ENDPOINT_MARSHAL_SIZE_MAX-1]; }

	/**
	 * @return True if endpoint type isn't NIL
	 */
	ZT_INLINE operator bool() const noexcept { return (m_value[ZT_ENDPOINT_MARSHAL_SIZE_MAX] != (uint8_t)ZT_ENDPOINT_TYPE_NIL); }

	/**
	 * @return True if this endpoint type has an InetAddress address type and thus ip() is valid
	 */
	ZT_INLINE bool isInetAddr() const noexcept
	{
		switch(this->type()) {
			case ZT_ENDPOINT_TYPE_IP:
			case ZT_ENDPOINT_TYPE_IP_UDP:
			case ZT_ENDPOINT_TYPE_IP_TCP:
			case ZT_ENDPOINT_TYPE_IP_HTTP2:
				return true;
			default:
				return false;
		}
	}

	/**
	 * Get InetAddress if this type uses IPv4 or IPv6 addresses (undefined otherwise)
	 * 
	 * @return InetAddress instance
	 */
	ZT_INLINE const InetAddress &ip() const noexcept { return *reinterpret_cast<const InetAddress *>(m_value); }

	/**
	 * Get MAC if this is an Ethernet, WiFi direct, or Bluetooth type (undefined otherwise)
	 *
	 * @return Ethernet MAC
	 */
	ZT_INLINE const MAC &eth() const noexcept { return *reinterpret_cast<const MAC *>(m_value); }

	/**
	 * Get fingerprint if this is a ZeroTier endpoint type (undefined otherwise)
	 * 
	 * @return ZeroTier fingerprint
	 */
	ZT_INLINE const Fingerprint &zt() const noexcept { return *reinterpret_cast<const Fingerprint *>(m_value); }

	static constexpr int marshalSizeMax() noexcept { return ZT_ENDPOINT_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_ENDPOINT_MARSHAL_SIZE_MAX]) const noexcept;
	int unmarshal(const uint8_t *restrict data,int len) noexcept;

private:
	uint8_t m_value[ZT_ENDPOINT_MARSHAL_SIZE_MAX]; // the last byte in this buffer is the type
};

} // namespace ZeroTier

#endif
