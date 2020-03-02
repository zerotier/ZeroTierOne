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
#include <ctime>

#ifdef __WINDOWS__
#include <process.h>
#else
#include <unistd.h>
#endif

namespace ZeroTier {
namespace Protocol {

// The counter used to assign packet IDs / cryptographic nonces.
std::atomic<uint64_t> _s_packetIdCtr((uint64_t)time(nullptr) << 32U);

uint64_t createProbe(const Identity &sender,const Identity &recipient,const uint8_t key[ZT_PEER_SECRET_KEY_LENGTH]) noexcept
{
	uint8_t tmp[ZT_IDENTITY_HASH_SIZE + ZT_IDENTITY_HASH_SIZE];
	memcpy(tmp,sender.fingerprint().hash(),ZT_IDENTITY_HASH_SIZE);
	memcpy(tmp + ZT_IDENTITY_HASH_SIZE,recipient.fingerprint().hash(),ZT_IDENTITY_HASH_SIZE);
	uint64_t hash[6];
	SHA384(hash,tmp,sizeof(tmp),key,ZT_PEER_SECRET_KEY_LENGTH);
	return hash[0];
}

void armor(Buf &pkt,int packetSize,const uint8_t key[ZT_PEER_SECRET_KEY_LENGTH],uint8_t cipherSuite) noexcept
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
			poly1305(mac,pkt.unsafeData + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,packetSize - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,macKey);
			ph.mac = mac[0];
		} break;

		case ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012: {
			uint8_t perPacketKey[ZT_PEER_SECRET_KEY_LENGTH];
			salsa2012DeriveKey(key,perPacketKey,pkt,packetSize);
			Salsa20 s20(perPacketKey,&ph.packetId);

			uint8_t macKey[ZT_POLY1305_KEY_LEN];
			s20.crypt12(Utils::ZERO256,macKey,ZT_POLY1305_KEY_LEN);

			const unsigned int encLen = packetSize - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START;
			s20.crypt12(pkt.unsafeData + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,pkt.unsafeData + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,encLen);

			uint64_t mac[2];
			poly1305(mac,pkt.unsafeData + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,encLen,macKey);
			ph.mac = mac[0];
		} break;

		case ZT_PROTO_CIPHER_SUITE__AES_GCM_NRH: {
		} break;
	}
}

int compress(SharedPtr<Buf> &pkt,int packetSize) noexcept
{
	if (packetSize <= 128)
		return packetSize;

	SharedPtr<Buf> pkt2(new Buf());
	if (!pkt2) return packetSize;

	const int uncompressedLen = packetSize - ZT_PROTO_PACKET_PAYLOAD_START;
	const int compressedLen = LZ4_compress_fast(reinterpret_cast<const char *>(pkt->unsafeData + ZT_PROTO_PACKET_PAYLOAD_START),reinterpret_cast<char *>(pkt2->unsafeData + ZT_PROTO_PACKET_PAYLOAD_START),uncompressedLen,ZT_BUF_MEM_SIZE - ZT_PROTO_PACKET_PAYLOAD_START);
	if ((compressedLen > 0)&&(compressedLen < uncompressedLen)) {
		memcpy(pkt2->unsafeData,pkt->unsafeData,ZT_PROTO_PACKET_PAYLOAD_START);
		pkt.swap(pkt2);
		pkt->as<Protocol::Header>().verb |= ZT_PROTO_VERB_FLAG_COMPRESSED;
		return compressedLen + ZT_PROTO_PACKET_PAYLOAD_START;
	}

	return packetSize;
}

} // namespace Protocol
} // namespace ZeroTier
