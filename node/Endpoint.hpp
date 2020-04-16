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

#define ZT_ENDPOINT_MARSHAL_SIZE_MAX 64

namespace ZeroTier {

/**
 * Endpoint variant specifying some form of network endpoint.
 */
class Endpoint : public TriviallyCopyable
{
public:
	/**
	 * Endpoint type
	 *
	 * These are set to be the same as the IDs used for trace events in ZeroTierCore.h.
	 */
	enum Type
	{
		TYPE_NIL =          ZT_ENDPOINT_TYPE_NIL,
		TYPE_ZEROTIER =     ZT_ENDPOINT_TYPE_ZEROTIER,
		TYPE_ETHERNET =     ZT_ENDPOINT_TYPE_ETHERNET,
		TYPE_INETADDR_V4 =  ZT_ENDPOINT_TYPE_INETADDR_V4,
		TYPE_INETADDR_V6 =  ZT_ENDPOINT_TYPE_INETADDR_V6
	};

	/**
	 * Protocol identifier bits.
	 */
	enum Protocol
	{
		PROTO_DGRAM =       ZT_ENDPOINT_PROTO_DGRAM,
		PROTO_STREAM  =     ZT_ENDPOINT_PROTO_STREAM,
		PROTO_HTTP2 =       ZT_ENDPOINT_PROTO_HTTP2,
		PROTO_HTTPS2 =      ZT_ENDPOINT_PROTO_HTTPS2,
		PROTO_WS =          ZT_ENDPOINT_PROTO_WS,
		PROTO_WEBRTC =      ZT_ENDPOINT_PROTO_WEBRTC,
		PROTO_WIREGUARD =   ZT_ENDPOINT_PROTO_WIREGUARD
	};

	ZT_INLINE Endpoint() noexcept { memoryZero(this); } // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)

	explicit Endpoint(const InetAddress &sa,Protocol proto = PROTO_DGRAM) noexcept;

	/**
	 * @return True if this is an IPv4 or IPv6 IP address
	 */
	ZT_INLINE bool isInetAddr() const noexcept { return ((_t == TYPE_INETADDR_V4)||(_t == TYPE_INETADDR_V6)); }

	/**
	 * @return InetAddress or NIL if not of this type
	 */
	ZT_INLINE const InetAddress &inetAddr() const noexcept { return ((_t == TYPE_INETADDR_V4) || (_t == TYPE_INETADDR_V6)) ? asInetAddress(_v.sa) : InetAddress::NIL; }

	/**
	 * @return Protocol bit mask
	 */
	ZT_INLINE Protocol protocol() const noexcept { return _proto; }

	/**
	 * @return 384-bit hash of identity keys or NULL if not of this type
	 */
	ZT_INLINE const Fingerprint &fingerprint() const noexcept { return *reinterpret_cast<const Fingerprint *>(&_v.zt); }

	/**
	 * @return Ethernet address or NIL if not of this type
	 */
	ZT_INLINE MAC ethernet() const noexcept { return (_t == TYPE_ETHERNET) ? MAC(_v.eth) : MAC(); }

	/**
	 * @return Endpoint type or NIL if unset/empty
	 */
	ZT_INLINE Type type() const noexcept { return _t; }

	ZT_INLINE operator bool() const noexcept { return _t != TYPE_NIL; } // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

	bool operator==(const Endpoint &ep) const noexcept;
	ZT_INLINE bool operator!=(const Endpoint &ep) const noexcept { return (!(*this == ep)); }
	bool operator<(const Endpoint &ep) const noexcept;
	ZT_INLINE bool operator>(const Endpoint &ep) const noexcept { return (ep < *this); }
	ZT_INLINE bool operator<=(const Endpoint &ep) const noexcept { return !(ep < *this); }
	ZT_INLINE bool operator>=(const Endpoint &ep) const noexcept { return !(*this < ep); }

	static constexpr int marshalSizeMax() noexcept { return ZT_ENDPOINT_MARSHAL_SIZE_MAX; }
	int marshal(uint8_t data[ZT_ENDPOINT_MARSHAL_SIZE_MAX]) const noexcept;
	int unmarshal(const uint8_t *restrict data,int len) noexcept;

private:
	Type _t;
	Protocol _proto;
	int _l[3]; // X,Y,Z location in kilometers from the nearest gravitational center of mass (e.g. Earth)
	union {
		sockaddr_storage sa;
		ZT_Fingerprint zt;
		uint8_t eth[6];
	} _v;
};

} // namespace ZeroTier

#endif
