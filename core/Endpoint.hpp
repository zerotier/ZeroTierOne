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

#define ZT_ENDPOINT_STRING_SIZE_MAX 256
#define ZT_ENDPOINT_MARSHAL_SIZE_MAX 192

namespace ZeroTier {

static_assert((ZT_ENDPOINT_MARSHAL_SIZE_MAX - 1) > ZT_INETADDRESS_MARSHAL_SIZE_MAX, "ZT_ENDPOINT_MARSHAL_SIZE_MAX not large enough");
static_assert((ZT_ENDPOINT_MARSHAL_SIZE_MAX - 1) > sizeof(ZT_Fingerprint), "ZT_ENDPOINT_MARSHAL_SIZE_MAX not large enough");
static_assert((ZT_ENDPOINT_MARSHAL_SIZE_MAX - 1) > sizeof(InetAddress), "ZT_ENDPOINT_MARSHAL_SIZE_MAX not large enough");
static_assert((ZT_ENDPOINT_MARSHAL_SIZE_MAX - 1) > sizeof(MAC), "ZT_ENDPOINT_MARSHAL_SIZE_MAX not large enough");
static_assert((ZT_ENDPOINT_MARSHAL_SIZE_MAX - 1) > sizeof(Fingerprint), "ZT_ENDPOINT_MARSHAL_SIZE_MAX not large enough");

/**
 * Endpoint variant specifying some form of network endpoint.
 * 
 * This is sort of a superset of InetAddress and for the standard UDP
 * protocol marshals and unmarshals to a compatible format. This makes
 * it backward compatible with older node versions' protocol fields
 * where InetAddress was used as long as only the UDP type is exchanged
 * with those nodes.
 */
class Endpoint : public ZT_Endpoint, public TriviallyCopyable
{
public:
	/**
	 * Create a NIL/empty endpoint
	 */
	ZT_INLINE Endpoint() noexcept
	{ memoryZero(this); }

	ZT_INLINE Endpoint(const ZT_Endpoint &ep) noexcept
	{ Utils::copy< sizeof(ZT_Endpoint) >((ZT_Endpoint *)this, &ep); }

	/**
	 * Create an endpoint for a type that uses an IP
	 * 
	 * @param a IP/port
	 * @param et Endpoint type (default: IP_UDP)
	 */
	ZT_INLINE Endpoint(const InetAddress &inaddr, const ZT_EndpointType et = ZT_ENDPOINT_TYPE_IP_UDP) noexcept
	{
		if (inaddr) {
			this->type = et;
			Utils::copy< sizeof(struct sockaddr_storage) >(&(this->value.ss), &(inaddr.as.ss));
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
			this->type = ZT_ENDPOINT_TYPE_ZEROTIER;
			this->value.fp = zt_;
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
	ZT_INLINE Endpoint(const MAC &eth_, const ZT_EndpointType et = ZT_ENDPOINT_TYPE_ETHERNET) noexcept
	{
		if (eth_) {
			this->type = et;
			this->value.mac = eth_.toInt();
		} else {
			memoryZero(this);
		}
	}

	/**
	 * @return True if endpoint type isn't NIL
	 */
	ZT_INLINE operator bool() const noexcept
	{ return this->type != ZT_ENDPOINT_TYPE_NIL; }

	/**
	 * @return True if this endpoint type has an InetAddress address type and thus ip() is valid
	 */
	ZT_INLINE bool isInetAddr() const noexcept
	{
		switch (this->type) {
			case ZT_ENDPOINT_TYPE_IP:
			case ZT_ENDPOINT_TYPE_IP_UDP:
			case ZT_ENDPOINT_TYPE_IP_TCP:
			case ZT_ENDPOINT_TYPE_IP_HTTP:
				return true;
			default:
				return false;
		}
	}

	/**
	 * Check whether this endpoint's address is the same as another.
	 *
	 * Right now this checks whether IPs are equal if both are IP based endpoints.
	 * Otherwise it checks for simple equality.
	 *
	 * @param ep Endpoint to check
	 * @return True if endpoints seem to refer to the same address/host
	 */
	ZT_INLINE bool isSameAddress(const Endpoint &ep) const noexcept
	{
		switch (this->type) {
			case ZT_ENDPOINT_TYPE_IP:
			case ZT_ENDPOINT_TYPE_IP_UDP:
			case ZT_ENDPOINT_TYPE_IP_TCP:
			case ZT_ENDPOINT_TYPE_IP_HTTP:
				switch (ep.type) {
					case ZT_ENDPOINT_TYPE_IP:
					case ZT_ENDPOINT_TYPE_IP_UDP:
					case ZT_ENDPOINT_TYPE_IP_TCP:
					case ZT_ENDPOINT_TYPE_IP_HTTP:
						return ip().ipsEqual(ep.ip());
					default:
						break;
				}
				break;
			default:
				break;
		}
		return (*this) == ep;
	}

	/**
	 * Get InetAddress if this type uses IPv4 or IPv6 addresses (undefined otherwise)
	 * 
	 * @return InetAddress instance
	 */
	ZT_INLINE const InetAddress &ip() const noexcept
	{ return asInetAddress(this->value.ss); }

	/**
	 * Get MAC if this is an Ethernet, WiFi direct, or Bluetooth type (undefined otherwise)
	 *
	 * @return Ethernet MAC
	 */
	ZT_INLINE MAC eth() const noexcept
	{ return MAC(this->value.mac); }

	/**
	 * Get fingerprint if this is a ZeroTier endpoint type (undefined otherwise)
	 * 
	 * @return ZeroTier fingerprint
	 */
	ZT_INLINE Fingerprint zt() const noexcept
	{ return Fingerprint(this->value.fp); }

	ZT_INLINE unsigned long hashCode() const noexcept
	{
		switch (this->type) {
			default:
				return 1;
			case ZT_ENDPOINT_TYPE_ZEROTIER:
				return (unsigned long)this->value.fp.address;
			case ZT_ENDPOINT_TYPE_ETHERNET:
			case ZT_ENDPOINT_TYPE_WIFI_DIRECT:
			case ZT_ENDPOINT_TYPE_BLUETOOTH:
				return (unsigned long)Utils::hash64(this->value.mac);
			case ZT_ENDPOINT_TYPE_IP:
			case ZT_ENDPOINT_TYPE_IP_UDP:
			case ZT_ENDPOINT_TYPE_IP_TCP:
			case ZT_ENDPOINT_TYPE_IP_HTTP:
				return ip().hashCode();
		}
	}

	char *toString(char s[ZT_ENDPOINT_STRING_SIZE_MAX]) const noexcept;

	ZT_INLINE String toString() const
	{
		char tmp[ZT_ENDPOINT_STRING_SIZE_MAX];
		return String(toString(tmp));
	}

	bool fromString(const char *s) noexcept;

	static constexpr int marshalSizeMax() noexcept
	{ return ZT_ENDPOINT_MARSHAL_SIZE_MAX; }

	int marshal(uint8_t data[ZT_ENDPOINT_MARSHAL_SIZE_MAX]) const noexcept;

	int unmarshal(const uint8_t *restrict data, int len) noexcept;

	bool operator==(const Endpoint &ep) const noexcept;

	ZT_INLINE bool operator!=(const Endpoint &ep) const noexcept
	{ return !((*this) == ep); }

	bool operator<(const Endpoint &ep) const noexcept;

	ZT_INLINE bool operator>(const Endpoint &ep) const noexcept
	{ return (ep < *this); }

	ZT_INLINE bool operator<=(const Endpoint &ep) const noexcept
	{ return !(ep < *this); }

	ZT_INLINE bool operator>=(const Endpoint &ep) const noexcept
	{ return !(*this < ep); }
};

static_assert(sizeof(Endpoint) == sizeof(ZT_Endpoint), "size mismatch");

} // namespace ZeroTier

#endif
