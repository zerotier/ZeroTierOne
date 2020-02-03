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

#include "Protocol.hpp"
#include "Buf.hpp"
#include "Utils.hpp"

#if defined(__GCC__) && (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64))
#define ZT_PACKET_USE_ATOMIC_INTRINSICS
#endif
#ifndef ZT_PACKET_USE_ATOMIC_INTRINSICS
#include <atomic>
#endif

namespace ZeroTier {
namespace Protocol {

namespace {

const uint64_t ZEROES32[4] = { 0,0,0,0 };

/**
 * Deterministically mangle a 256-bit crypto key based on packet
 *
 * This uses extra data from the packet to mangle the secret, giving us an
 * effective IV that is somewhat more than 64 bits. This is "free" for
 * Salsa20 since it has negligible key setup time so using a different
 * key each time is fine.
 *
 * @param in Input key (32 bytes)
 * @param out Output buffer (32 bytes)
 */
ZT_ALWAYS_INLINE void _salsa20MangleKey(const uint8_t *const in,uint8_t *const out,const Buf< Header > &packet,const unsigned int packetSize)
{
	// IV and source/destination addresses. Using the addresses divides the
	// key space into two halves-- A->B and B->A (since order will change).
	for(int i=0;i<18;++i) // 8 + (ZT_ADDRESS_LENGTH * 2) == 18
		out[i] = in[i] ^ packet.data.bytes[i];

	// Flags, but with hop count masked off. Hop count is altered by forwarding
	// nodes. It's one of the only parts of a packet modifiable by people
	// without the key.
	out[18] = in[18] ^ (packet.data.fields.flags & 0xf8U);

	// Raw packet size in bytes -- thus each packet size defines a new
	// key space.
	out[19] = in[19] ^ (uint8_t)packetSize;
	out[20] = in[20] ^ (uint8_t)(packetSize >> 8U); // little endian

	// Rest of raw key is used unchanged
	for(int i=21;i<32;++i)
		out[i] = in[i];
}

unsigned long long _initPacketID()
{
	unsigned long long tmp = 0;
	Utils::getSecureRandom(&tmp,sizeof(tmp));
	tmp >>= 31U;
	tmp |= (((uint64_t)time(nullptr)) & 0xffffffffULL) << 33U;
	return tmp;
}
#ifdef ZT_PACKET_USE_ATOMIC_INTRINSICS
unsigned long long _packetIdCtr = _initPacketID();
#else
static std::atomic<unsigned long long> _packetIdCtr(_initPacketID());
#endif

} // anonymous namespace

void _armor(Buf< Header > &packet,const unsigned int packetSize,const uint8_t key[ZT_PEER_SECRET_KEY_LENGTH],const uint8_t cipherSuite)
{
	packet.data.fields.flags = (packet.data.fields.flags & 0xc7U) | ((cipherSuite << 3U) & 0x38U); // FFCCCHHH
	if (cipherSuite == ZT_PROTO_CIPHER_SUITE__AES_GCM) {
		// TODO
	} else if (cipherSuite != ZT_PROTO_CIPHER_SUITE__NONE) {
		uint8_t mangledKey[ZT_PEER_SECRET_KEY_LENGTH],macKey[ZT_POLY1305_KEY_LEN];
		uint64_t mac[2];

		_salsa20MangleKey(key,mangledKey,packet,packetSize);
		Salsa20 s20(mangledKey,&(packet.data.fields.packetId));
		s20.crypt12(ZEROES32,macKey,sizeof(macKey));

		uint8_t *payload = packet.data.bytes + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START;
		const unsigned int payloadLen = packetSize - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START;

		if (cipherSuite == ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012)
			s20.crypt12(payload,payload,payloadLen);

		poly1305(mac,payload,payloadLen,macKey);
		packet.data.fields.mac = mac[0];
	}
}

int _dearmor(Buf< Header > &packet,const unsigned int packetSize,const uint8_t key[ZT_PEER_SECRET_KEY_LENGTH])
{
	const int cipherSuite = (int)(packet.data.fields.flags & 0x38U);
	if (cipherSuite == ZT_PROTO_CIPHER_SUITE__AES_GCM) {
		// TODO
	} else if (cipherSuite != ZT_PROTO_CIPHER_SUITE__NONE) {
		uint8_t mangledKey[ZT_PEER_SECRET_KEY_LENGTH],macKey[ZT_POLY1305_KEY_LEN];
		uint64_t mac[2];

		_salsa20MangleKey(key,mangledKey,packet,packetSize);
		Salsa20 s20(mangledKey,&(packet.data.fields.packetId));
		s20.crypt12(ZEROES32,macKey,sizeof(macKey));

		uint8_t *payload = packet.data.bytes + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START;
		const unsigned int payloadLen = packetSize - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START;

		if (cipherSuite == ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012)
			s20.crypt12(payload,payload,payloadLen);

		poly1305(mac,payload,payloadLen,macKey);
		if (packet.data.fields.mac != mac[0])
			return -1;
	}
	return cipherSuite;
}

unsigned int _compress(Buf< Header > &packet,const unsigned int packetSize)
{
	uint8_t tmp[ZT_BUF_MEM_SIZE + 32];

	if ((packet.data.fields.verb & ZT_PROTO_VERB_FLAG_COMPRESSED) != 0) // sanity check for multiple calls to compress()
		return packetSize;

	const unsigned int uncompressedLen = packetSize - ZT_PROTO_PACKET_PAYLOAD_START;
	const int compressedLen = LZ4_compress_fast(
		reinterpret_cast<const char *>(packet.data.bytes + ZT_PROTO_PACKET_PAYLOAD_START),
	  reinterpret_cast<char *>(tmp),
		(int)uncompressedLen,
		sizeof(tmp) - ZT_PROTO_PACKET_PAYLOAD_START);
	if ((compressedLen > 0)&&(compressedLen < uncompressedLen)) {
		packet.data.fields.verb |= ZT_PROTO_VERB_FLAG_COMPRESSED;
		memcpy(packet.data.bytes + ZT_PROTO_PACKET_PAYLOAD_START,tmp,compressedLen);
		return (unsigned int)compressedLen + ZT_PROTO_PACKET_PAYLOAD_START;
	}

	return packetSize;
}

int _uncompress(Buf< Header > &packet,const unsigned int packetSize)
{
	uint8_t tmp[ZT_BUF_MEM_SIZE];

	if ((packet.data.fields.verb & ZT_PROTO_VERB_FLAG_COMPRESSED) == 0)
		return (int)packetSize;

	const int uncompressedLen = LZ4_decompress_safe(
		reinterpret_cast<const char *>(packet.data.bytes + ZT_PROTO_PACKET_PAYLOAD_START),
		reinterpret_cast<char *>(tmp),
		(int)(packetSize - ZT_PROTO_PACKET_PAYLOAD_START),
		sizeof(tmp) - ZT_PROTO_PACKET_PAYLOAD_START);

	if ((uncompressedLen > 0)&&(uncompressedLen <= (sizeof(tmp) - ZT_PROTO_PACKET_PAYLOAD_START))) {
		packet.data.fields.verb &= (uint8_t)(~((uint8_t)ZT_PROTO_VERB_FLAG_COMPRESSED));
		memcpy(packet.data.bytes + ZT_PROTO_PACKET_PAYLOAD_START,tmp,uncompressedLen);
		return uncompressedLen + ZT_PROTO_PACKET_PAYLOAD_START;
	}
	return -1;
}

uint64_t getPacketId()
{
#ifdef ZT_PACKET_USE_ATOMIC_INTRINSICS
	return __sync_add_and_fetch(&_packetIdCtr,1ULL);
#else
	return ++_packetIdCtr;
#endif
}

} // namespace Protocol
} // namespace ZeroTier
