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

#include <cstdlib>
#include <stdexcept>

#if defined(__GCC__) && (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64))
#define ZT_PACKET_USE_ATOMIC_INTRINSICS
#endif
#ifndef ZT_PACKET_USE_ATOMIC_INTRINSICS
#include <atomic>
#endif

namespace ZeroTier {
namespace Protocol {

namespace {

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

uintptr_t _checkStructureSizing()
{
	if (sizeof(Header) != ZT_PROTO_MIN_PACKET_LENGTH)
		throw std::runtime_error("sizeof(Header) != ZT_PROTO_MIN_PACKET_LENGTH");
	if (sizeof(FragmentHeader) != ZT_PROTO_MIN_FRAGMENT_LENGTH)
		throw std::runtime_error("sizeof(FragmentHeader) != ZT_PROTO_MIN_FRAGMENT_LENGTH");
	return (uintptr_t)Utils::getSecureRandomU64(); // also prevents compiler from optimizing out
}

} // anonymous namespace

volatile uintptr_t _compileTimeStructCheckHappened = _checkStructureSizing();

uint64_t getPacketId()
{
#ifdef ZT_PACKET_USE_ATOMIC_INTRINSICS
	return __sync_add_and_fetch(&_packetIdCtr,1ULL);
#else
	return ++_packetIdCtr;
#endif
}

#if 0
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
#endif

} // namespace Protocol
} // namespace ZeroTier
