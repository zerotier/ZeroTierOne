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

int Endpoint::marshal(uint8_t data[ZT_ENDPOINT_MARSHAL_SIZE_MAX]) const noexcept
{
	switch(m_value[ZT_ENDPOINT_MARSHAL_SIZE_MAX-1]) {
		default:
		//case ZT_ENDPOINT_TYPE_NIL:
			data[0] = 0;
			return 1;

		case ZT_ENDPOINT_TYPE_ZEROTIER:
			data[0] = 16 + ZT_ENDPOINT_TYPE_ZEROTIER;
			reinterpret_cast<const Fingerprint *>(m_value)->address().copyTo(data + 1);
			Utils::copy<ZT_FINGERPRINT_HASH_SIZE>(data + 1 + ZT_ADDRESS_LENGTH,reinterpret_cast<const Fingerprint *>(m_value)->hash());
			return 1 + ZT_ADDRESS_LENGTH + ZT_FINGERPRINT_HASH_SIZE;

		case ZT_ENDPOINT_TYPE_ETHERNET:
		case ZT_ENDPOINT_TYPE_WIFI_DIRECT:
		case ZT_ENDPOINT_TYPE_BLUETOOTH:
			data[0] = 16 + m_value[ZT_ENDPOINT_MARSHAL_SIZE_MAX-1];
			reinterpret_cast<const MAC *>(m_value)->copyTo(data + 1);
			return 7;

		case ZT_ENDPOINT_TYPE_IP_UDP:
			return reinterpret_cast<const InetAddress *>(m_value)->marshal(data);

		case ZT_ENDPOINT_TYPE_IP:
		case ZT_ENDPOINT_TYPE_IP_TCP:
		case ZT_ENDPOINT_TYPE_IP_HTTP2:
			data[0] = 16 + m_value[ZT_ENDPOINT_MARSHAL_SIZE_MAX-1];
			return 1 + reinterpret_cast<const InetAddress *>(m_value)->marshal(data + 1);
	}
}

int Endpoint::unmarshal(const uint8_t *restrict data,int len) noexcept
{
	memoryZero(this);
	if (unlikely(len <= 0))
		return -1;

	// Serialized endpoints with type bytes less than 16 are passed through
	// to the unmarshal method of InetAddress and considered UDP endpoints.
	// This allows backward compatibility with old endpoint fields in the
	// protocol that were serialized InetAddress instances.
	if (data[0] < 16) {
		switch(data[0]) {
			case 0:
				return 1;
			case 4:
			case 6:
				m_value[ZT_ENDPOINT_MARSHAL_SIZE_MAX-1] = (uint8_t)ZT_ENDPOINT_TYPE_IP_UDP;
				return reinterpret_cast<InetAddress *>(m_value)->unmarshal(data,len);
		}
		return -1;
	}

	switch((m_value[ZT_ENDPOINT_MARSHAL_SIZE_MAX-1] = (data[0] - 16))) {
		case ZT_ENDPOINT_TYPE_NIL:
			return 1;

		case ZT_ENDPOINT_TYPE_ZEROTIER:
			if (len >= (1 + ZT_ADDRESS_LENGTH + ZT_FINGERPRINT_HASH_SIZE)) {
				reinterpret_cast<Fingerprint *>(m_value)->apiFingerprint()->address = Address(data + 1).toInt();
				Utils::copy<ZT_FINGERPRINT_HASH_SIZE>(reinterpret_cast<Fingerprint *>(m_value)->apiFingerprint()->hash,data + 1 + ZT_ADDRESS_LENGTH);
				return 1 + ZT_ADDRESS_LENGTH + ZT_FINGERPRINT_HASH_SIZE;
			}
			return -1;

		case ZT_ENDPOINT_TYPE_ETHERNET:
		case ZT_ENDPOINT_TYPE_WIFI_DIRECT:
		case ZT_ENDPOINT_TYPE_BLUETOOTH:
			if (len >= 7) {
				reinterpret_cast<MAC *>(m_value)->setTo(data + 1);
				return 7;
			}
			return -1;

		case ZT_ENDPOINT_TYPE_IP:
		case ZT_ENDPOINT_TYPE_IP_UDP:
		case ZT_ENDPOINT_TYPE_IP_TCP:
		case ZT_ENDPOINT_TYPE_IP_HTTP2:
			return reinterpret_cast<InetAddress *>(m_value)->unmarshal(data + 1,len - 1);

		default:
			break;
	}

	// Unrecognized types can still be passed over in a valid stream if they are
	// prefixed by a 16-bit size. This allows forward compatibility with future
	// endpoint types.
	m_value[ZT_ENDPOINT_MARSHAL_SIZE_MAX-1] = (uint8_t)ZT_ENDPOINT_TYPE_NIL;
	if (len < 3)
		return -1;
	const int unrecLen = 1 + (int)Utils::loadBigEndian<uint16_t>(data + 1);
	return (unrecLen > len) ? -1 : unrecLen;
}

} // namespace ZeroTier
