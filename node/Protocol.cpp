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

uintptr_t _checkSizes()
{
	// These are compiled time checked assertions that make sure our platform/compiler is sane
	// and that packed structures are working properly.
	if (ZT_PROTO_MAX_PACKET_LENGTH > ZT_BUF_MEM_SIZE)
		throw std::runtime_error("ZT_PROTO_MAX_PACKET_LENGTH > ZT_BUF_MEM_SIZE");
	if (sizeof(Header) != ZT_PROTO_MIN_PACKET_LENGTH)
		throw std::runtime_error("sizeof(Header) != ZT_PROTO_MIN_PACKET_LENGTH");
	if (sizeof(FragmentHeader) != ZT_PROTO_MIN_FRAGMENT_LENGTH)
		throw std::runtime_error("sizeof(FragmentHeader) != ZT_PROTO_MIN_FRAGMENT_LENGTH");
	return (uintptr_t)Utils::getSecureRandomU64(); // also prevents compiler from optimizing out
}

} // anonymous namespace

// Make compiler compile and "run" _checkSizes()
volatile uintptr_t _checkSizesIMeanIt = _checkSizes();

uint64_t createProbe(const Identity &sender,const Identity &recipient,const uint8_t key[ZT_PEER_SECRET_KEY_LENGTH]) noexcept
{
	uint8_t tmp[ZT_IDENTITY_HASH_SIZE + ZT_IDENTITY_HASH_SIZE];
	memcpy(tmp,sender.hash(),ZT_IDENTITY_HASH_SIZE);
	memcpy(tmp + ZT_IDENTITY_HASH_SIZE,recipient.hash(),ZT_IDENTITY_HASH_SIZE);
	uint64_t hash[6];
	SHA384(hash,tmp,sizeof(tmp),key,ZT_PEER_SECRET_KEY_LENGTH);
	return hash[0];
}

uint64_t getPacketId() noexcept
{
#ifdef ZT_PACKET_USE_ATOMIC_INTRINSICS
	return __sync_add_and_fetch(&_packetIdCtr,1ULL);
#else
	return ++_packetIdCtr;
#endif
}

void armor(Buf &pkt,unsigned int packetSize,const uint8_t key[ZT_PEER_SECRET_KEY_LENGTH],uint8_t cipherSuite) noexcept
{
	Protocol::Header &ph = pkt.as<Protocol::Header>();
	ph.flags = (ph.flags & 0xc7U) | ((cipherSuite << 3U) & 0x38U); // flags: FFCCCHHH where CCC is cipher

	switch(cipherSuite) {
		case ZT_PROTO_CIPHER_SUITE__POLY1305_NONE: {
			uint8_t perPacketKey[ZT_PEER_SECRET_KEY_LENGTH];
			salsa2012DeriveKey(key,perPacketKey,pkt,packetSize);
			Salsa20 s20(perPacketKey,&ph.packetId);

			uint8_t macKey[ZT_POLY1305_KEY_LEN];
			s20.crypt12(Utils::ZERO256,macKey,ZT_POLY1305_KEY_LEN);

			// only difference here is that we don't encrypt the payload

			uint64_t mac[2];
			poly1305(mac,pkt.b + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,packetSize - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,macKey);
			ph.mac = mac[0];
		} break;

		case ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012: {
			uint8_t perPacketKey[ZT_PEER_SECRET_KEY_LENGTH];
			salsa2012DeriveKey(key,perPacketKey,pkt,packetSize);
			Salsa20 s20(perPacketKey,&ph.packetId);

			uint8_t macKey[ZT_POLY1305_KEY_LEN];
			s20.crypt12(Utils::ZERO256,macKey,ZT_POLY1305_KEY_LEN);

			const unsigned int encLen = packetSize - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START;
			s20.crypt12(pkt.b + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,pkt.b + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,encLen);

			uint64_t mac[2];
			poly1305(mac,pkt.b + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,encLen,macKey);
			ph.mac = mac[0];
		} break;

		case ZT_PROTO_CIPHER_SUITE__AES_GCM_NRH: {
		} break;
	}
}

unsigned int compress(SharedPtr<Buf> &pkt,unsigned int packetSize) noexcept
{
	if (packetSize <= 128)
		return packetSize;

	SharedPtr<Buf> pkt2(Buf::get());
	if (!pkt2) return packetSize;

	const unsigned int uncompressedLen = packetSize - ZT_PROTO_PACKET_PAYLOAD_START;
	const int compressedLen = LZ4_compress_fast(
		reinterpret_cast<const char *>(pkt->b + ZT_PROTO_PACKET_PAYLOAD_START),
		reinterpret_cast<char *>(pkt2->b + ZT_PROTO_PACKET_PAYLOAD_START),
		(int)uncompressedLen,
		ZT_BUF_MEM_SIZE - ZT_PROTO_PACKET_PAYLOAD_START);
	if ((compressedLen > 0)&&(compressedLen < (int)uncompressedLen)) {
		memcpy(pkt2->b,pkt->b,ZT_PROTO_PACKET_PAYLOAD_START);
		pkt.swap(pkt2);
		pkt->as<Protocol::Header>().verb |= ZT_PROTO_VERB_FLAG_COMPRESSED;
		return (unsigned int)compressedLen + ZT_PROTO_PACKET_PAYLOAD_START;
	}

	return packetSize;
}

} // namespace Protocol
} // namespace ZeroTier
