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

#include <cstdint>
#include <cstring>

#include "Packet.hpp"
#include "Mutex.hpp"
#include "LZ4.hpp"

namespace ZeroTier {

const unsigned char Packet::ZERO_KEY[32] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

void Packet::armor(const void *key,bool encryptPayload)
{
	uint8_t mangledKey[32];
	uint8_t *const data = reinterpret_cast<uint8_t *>(unsafeData());

	// Set flag now, since it affects key mangle function
	setCipher(encryptPayload ? ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012 : ZT_PROTO_CIPHER_SUITE__POLY1305_NONE);

	_salsa20MangleKey((const unsigned char *)key,mangledKey);

	Salsa20 s20(mangledKey,data + ZT_PACKET_IDX_IV);
	uint64_t macKey[4];
	s20.crypt12(ZERO_KEY,macKey,sizeof(macKey));
	uint8_t *const payload = data + ZT_PACKET_IDX_VERB;
	const unsigned int payloadLen = size() - ZT_PACKET_IDX_VERB;
	if (encryptPayload)
		s20.crypt12(payload,payload,payloadLen);
	uint64_t mac[2];
	poly1305(mac,payload,payloadLen,macKey);
	memcpy(data + ZT_PACKET_IDX_MAC,mac,8);
}

bool Packet::dearmor(const void *key)
{
	uint8_t mangledKey[32];
	uint8_t *const data = reinterpret_cast<uint8_t *>(unsafeData());
	const unsigned int payloadLen = size() - ZT_PACKET_IDX_VERB;
	unsigned char *const payload = data + ZT_PACKET_IDX_VERB;
	const unsigned int cs = cipher();

	if ((cs == ZT_PROTO_CIPHER_SUITE__POLY1305_NONE)||(cs == ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012)) {
		_salsa20MangleKey((const unsigned char *)key,mangledKey);
		Salsa20 s20(mangledKey,data + ZT_PACKET_IDX_IV);
		uint64_t macKey[4];
		s20.crypt12(ZERO_KEY,macKey,sizeof(macKey));
		uint64_t mac[2];
		poly1305(mac,payload,payloadLen,macKey);
#ifdef ZT_NO_TYPE_PUNNING
		if (!Utils::secureEq(mac,data + ZT_PACKET_IDX_MAC,8))
			return false;
#else
		if ((*reinterpret_cast<const uint64_t *>(data + ZT_PACKET_IDX_MAC)) != mac[0]) // also secure, constant time
			return false;
#endif
		if (cs == ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012)
			s20.crypt12(payload,payload,payloadLen);
		return true;
	} else {
		return false; // unrecognized cipher suite
	}
}

bool Packet::compress()
{
	char *const data = reinterpret_cast<char *>(unsafeData());
	char buf[ZT_PROTO_MAX_PACKET_LENGTH * 2];

	if ((!compressed())&&(size() > (ZT_PACKET_IDX_PAYLOAD + 64))) { // don't bother compressing tiny packets
		int pl = (int)(size() - ZT_PACKET_IDX_PAYLOAD);
		int cl = LZ4_compress_fast(data + ZT_PACKET_IDX_PAYLOAD,buf,pl,ZT_PROTO_MAX_PACKET_LENGTH * 2,1);
		if ((cl > 0)&&(cl < pl)) {
			data[ZT_PACKET_IDX_VERB] |= (char)ZT_PROTO_VERB_FLAG_COMPRESSED;
			setSize((unsigned int)cl + ZT_PACKET_IDX_PAYLOAD);
			memcpy(data + ZT_PACKET_IDX_PAYLOAD,buf,cl);
			return true;
		}
	}
	data[ZT_PACKET_IDX_VERB] &= (char)(~ZT_PROTO_VERB_FLAG_COMPRESSED);

	return false;
}

bool Packet::uncompress()
{
	char *const data = reinterpret_cast<char *>(unsafeData());
	char buf[ZT_PROTO_MAX_PACKET_LENGTH];

	if ((compressed())&&(size() >= ZT_PROTO_MIN_PACKET_LENGTH)) {
		if (size() > ZT_PACKET_IDX_PAYLOAD) {
			unsigned int compLen = size() - ZT_PACKET_IDX_PAYLOAD;
			int ucl = LZ4_decompress_safe((const char *)data + ZT_PACKET_IDX_PAYLOAD,buf,compLen,sizeof(buf));
			if ((ucl > 0)&&(ucl <= (int)(capacity() - ZT_PACKET_IDX_PAYLOAD))) {
				setSize((unsigned int)ucl + ZT_PACKET_IDX_PAYLOAD);
				memcpy(data + ZT_PACKET_IDX_PAYLOAD,buf,ucl);
			} else {
				return false;
			}
		}
		data[ZT_PACKET_IDX_VERB] &= (char)(~ZT_PROTO_VERB_FLAG_COMPRESSED);
	}

	return true;
}

uint64_t Packet::nextPacketId()
{
	// The packet ID which is also the packet's nonce/IV can be sequential but
	// it should never repeat. This scheme minimizes the chance of nonce
	// repetition if (as will usually be the case) the clock is relatively
	// accurate.

	static uint64_t ctr = 0;
	static Mutex lock;
	lock.lock();
	while (ctr == 0) {
		Utils::getSecureRandom(&ctr,sizeof(ctr));
		ctr >>= 32;
		ctr |= (((uint64_t)time(nullptr)) & 0xffffffffULL) << 32;
	}
	const uint64_t i = ctr++;
	lock.unlock();
	return i;
}

} // namespace ZeroTier
