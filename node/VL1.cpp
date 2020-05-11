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

#include "VL1.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"
#include "Topology.hpp"
#include "VL2.hpp"
#include "Salsa20.hpp"
#include "LZ4.hpp"
#include "Poly1305.hpp"
#include "Identity.hpp"
#include "SelfAwareness.hpp"
#include "SHA512.hpp"
#include "Peer.hpp"
#include "Path.hpp"
#include "Expect.hpp"

namespace ZeroTier {

namespace {

ZT_INLINE const Identity &identityFromPeerPtr(const SharedPtr<Peer> &p)
{
	return (p) ? p->identity() : Identity::NIL;
}

struct p_SalsaPolyCopyFunction
{
	Salsa20 s20;
	Poly1305 poly1305;
	ZT_INLINE p_SalsaPolyCopyFunction(const void *salsaKey,const void *salsaIv) :
		s20(salsaKey,salsaIv),
		poly1305()
	{
		uint8_t macKey[ZT_POLY1305_KEY_SIZE];
		s20.crypt12(Utils::ZERO256,macKey,ZT_POLY1305_KEY_SIZE);
		poly1305.init(macKey);
	}
	ZT_INLINE void operator()(void *dest,const void *src,const unsigned int len) noexcept
	{
		poly1305.update(src,len);
		s20.crypt12(src,dest,len);
	}
};

struct p_PolyCopyFunction
{
	Poly1305 poly1305;
	ZT_INLINE p_PolyCopyFunction(const void *salsaKey,const void *salsaIv) :
		poly1305()
	{
		uint8_t macKey[ZT_POLY1305_KEY_SIZE];
		Salsa20(salsaKey,salsaIv).crypt12(Utils::ZERO256,macKey,ZT_POLY1305_KEY_SIZE);
		poly1305.init(macKey);
	}
	ZT_INLINE void operator()(void *dest,const void *src,const unsigned int len) noexcept
	{
		poly1305.update(src,len);
		Utils::copy(dest,src,len);
	}
};

} // anonymous namespace

VL1::VL1(const RuntimeEnvironment *renv) :
	RR(renv)
{
}

void VL1::onRemotePacket(void *const tPtr,const int64_t localSocket,const InetAddress &fromAddr,SharedPtr<Buf> &data,const unsigned int len)
{
	const SharedPtr<Path> path(RR->topology->path(localSocket,fromAddr));
	const int64_t now = RR->node->now();

	ZT_SPEW("%u bytes from %s (local socket %lld)",len,fromAddr.toString().c_str(),localSocket);
	path->received(now,len);

	// NOTE: likely/unlikely are used here to highlight the most common code path
	// for valid data packets. This may allow the compiler to generate very slightly
	// faster code for that path.

	/*
	 * Packet format:
	 *   <[8] 64-bit packet ID / crypto IV>
	 *   <[5] destination ZT address>
	 *   <[5] source ZT address>
	 *   <[1] outer visible flags, cipher, and hop count (bits: FFCCHHH)>
	 *   <[8] 64-bit MAC (or trusted path ID in trusted path mode)>
	 *   [... -- begin encryption envelope -- ...]
	 *   <[1] inner envelope flags (MS 3 bits) and verb (LS 5 bits)>
	 *   [... verb-specific payload ...]
	 */

	try {
		// If this is too short to be a packet or fragment, check if it's a probe and if not simply drop it.
		if (unlikely(len < ZT_PROTO_MIN_FRAGMENT_LENGTH)) {
			if (len == ZT_PROTO_PROBE_LENGTH) {
				const uint32_t probeToken = data->lI32(0);
				PeerList peers(RR->topology->peersByProbeToken(probeToken));
				ZT_SPEW("probe %.8lx matches %u peers",(unsigned long)probeToken,peers.size());
				for(unsigned int pi=0;pi<peers.size();++pi) {
					if (peers[pi]->rateGateProbeRequest(now)) {
						ZT_SPEW("HELLO -> %s(%s)",peers[pi]->address().toString().c_str(),fromAddr.toString().c_str());
						peers[pi]->hello(tPtr,localSocket,fromAddr,now);
					}
				}
			}
			return;
		}

		static_assert((ZT_PROTO_PACKET_ID_INDEX + sizeof(uint64_t)) < ZT_PROTO_MIN_FRAGMENT_LENGTH,"overflow");
		const uint64_t packetId = Utils::loadAsIsEndian<uint64_t>(data->unsafeData + ZT_PROTO_PACKET_ID_INDEX);

		static_assert((ZT_PROTO_PACKET_DESTINATION_INDEX + ZT_ADDRESS_LENGTH) < ZT_PROTO_MIN_FRAGMENT_LENGTH,"overflow");
		Address destination(data->unsafeData + ZT_PROTO_PACKET_DESTINATION_INDEX);
		if (destination != RR->identity.address()) {
			m_relay(tPtr,path,destination,data,len);
			return;
		}

		// ----------------------------------------------------------------------------------------------------------------
		// If we made it this far, the packet is at least MIN_FRAGMENT_LENGTH and is addressed to this node's ZT address
		// ----------------------------------------------------------------------------------------------------------------

		Buf::PacketVector pktv;

		static_assert(ZT_PROTO_PACKET_FRAGMENT_INDICATOR_INDEX <= ZT_PROTO_MIN_FRAGMENT_LENGTH,"overflow");
		if (data->unsafeData[ZT_PROTO_PACKET_FRAGMENT_INDICATOR_INDEX] == ZT_PROTO_PACKET_FRAGMENT_INDICATOR) {
			// This looks like a fragment (excluding the head) of a larger packet.
			static_assert(ZT_PROTO_PACKET_FRAGMENT_COUNTS < ZT_PROTO_MIN_FRAGMENT_LENGTH,"overflow");
			const unsigned int totalFragments = (data->unsafeData[ZT_PROTO_PACKET_FRAGMENT_COUNTS] >> 4U) & 0x0fU;
			const unsigned int fragmentNo = data->unsafeData[ZT_PROTO_PACKET_FRAGMENT_COUNTS] & 0x0fU;
			switch (m_inputPacketAssembler.assemble(
				packetId,
				pktv,
				data,
				ZT_PROTO_PACKET_FRAGMENT_PAYLOAD_START_AT,
				len - ZT_PROTO_PACKET_FRAGMENT_PAYLOAD_START_AT,
				fragmentNo,
				totalFragments,
				now,
				path)) {
				case Defragmenter<ZT_MAX_PACKET_FRAGMENTS>::COMPLETE:
					break;
				default:
					//case Defragmenter<ZT_MAX_PACKET_FRAGMENTS>::OK:
					//case Defragmenter<ZT_MAX_PACKET_FRAGMENTS>::ERR_DUPLICATE_FRAGMENT:
					//case Defragmenter<ZT_MAX_PACKET_FRAGMENTS>::ERR_INVALID_FRAGMENT:
					//case Defragmenter<ZT_MAX_PACKET_FRAGMENTS>::ERR_TOO_MANY_FRAGMENTS_FOR_PATH:
					//case Defragmenter<ZT_MAX_PACKET_FRAGMENTS>::ERR_OUT_OF_MEMORY:
					return;
			}
		} else {
			if (unlikely(len < ZT_PROTO_MIN_PACKET_LENGTH))
				return;
			static_assert(ZT_PROTO_PACKET_FLAGS_INDEX < ZT_PROTO_MIN_PACKET_LENGTH,"overflow");
			if ((data->unsafeData[ZT_PROTO_PACKET_FLAGS_INDEX] & ZT_PROTO_FLAG_FRAGMENTED) != 0) {
				// This is the head of a series of fragments that we may or may not already have.
				switch (m_inputPacketAssembler.assemble(
					packetId,
					pktv,
					data,
					0, // fragment index is 0 since this is the head
					len,
					0, // always the zero'eth fragment
					0, // this is specified in fragments, not in the head
					now,
					path)) {
					case Defragmenter<ZT_MAX_PACKET_FRAGMENTS>::COMPLETE:
						break;
					default:
						//case Defragmenter<ZT_MAX_PACKET_FRAGMENTS>::OK:
						//case Defragmenter<ZT_MAX_PACKET_FRAGMENTS>::ERR_DUPLICATE_FRAGMENT:
						//case Defragmenter<ZT_MAX_PACKET_FRAGMENTS>::ERR_INVALID_FRAGMENT:
						//case Defragmenter<ZT_MAX_PACKET_FRAGMENTS>::ERR_TOO_MANY_FRAGMENTS_FOR_PATH:
						//case Defragmenter<ZT_MAX_PACKET_FRAGMENTS>::ERR_OUT_OF_MEMORY:
						return;
				}
			} else {
				// This is a single whole packet with no fragments.
				Buf::Slice s = pktv.push();
				s.b.swap(data);
				s.s = 0;
				s.e = len;
			}
		}

		// ----------------------------------------------------------------------------------------------------------------
		// If we made it this far without returning, a packet is fully assembled and ready to process.
		// ----------------------------------------------------------------------------------------------------------------

		const uint8_t *const hdr = pktv[0].b->unsafeData + pktv[0].s;
		static_assert((ZT_PROTO_PACKET_SOURCE_INDEX + ZT_ADDRESS_LENGTH) < ZT_PROTO_MIN_PACKET_LENGTH,"overflow");
		const Address source(hdr + ZT_PROTO_PACKET_SOURCE_INDEX);
		static_assert(ZT_PROTO_PACKET_FLAGS_INDEX < ZT_PROTO_MIN_PACKET_LENGTH,"overflow");
		const uint8_t hops = hdr[ZT_PROTO_PACKET_FLAGS_INDEX] & ZT_PROTO_FLAG_FIELD_HOPS_MASK;
		const uint8_t cipher = (hdr[ZT_PROTO_PACKET_FLAGS_INDEX] >> 3U) & 3U;

		SharedPtr<Buf> pkt(new Buf());
		int pktSize = 0;

		static_assert(ZT_PROTO_PACKET_VERB_INDEX < ZT_PROTO_MIN_PACKET_LENGTH,"overflow");
		if (unlikely( ((cipher == ZT_PROTO_CIPHER_SUITE__POLY1305_NONE)||(cipher == ZT_PROTO_CIPHER_SUITE__NONE)) && ((hdr[ZT_PROTO_PACKET_VERB_INDEX] & ZT_PROTO_VERB_MASK) == Protocol::VERB_HELLO) )) {
			// Handle unencrypted HELLO packets.
			pktSize = pktv.mergeCopy(*pkt);
			if (unlikely(pktSize < ZT_PROTO_MIN_PACKET_LENGTH)) {
				ZT_SPEW("discarding packet %.16llx from %s(%s): assembled packet size: %d",packetId,source.toString().c_str(),fromAddr.toString().c_str(),pktSize);
				return;
			}
			const SharedPtr<Peer> peer(m_HELLO(tPtr, path, *pkt, pktSize));
			if (peer)
				peer->received(tPtr,path,hops,packetId,pktSize - ZT_PROTO_PACKET_PAYLOAD_START,Protocol::VERB_HELLO,Protocol::VERB_NOP);
			return;
		}

		// This remains zero if authentication fails. Otherwise it gets set to a bit mask
		// indicating authentication and other security flags like encryption and forward
		// secrecy status.
		unsigned int auth = 0;

		SharedPtr<Peer> peer(RR->topology->peer(tPtr,source));
		if (peer) {
			switch(cipher) {

				case ZT_PROTO_CIPHER_SUITE__POLY1305_NONE: {
					uint8_t perPacketKey[ZT_SALSA20_KEY_SIZE];
					Protocol::salsa2012DeriveKey(peer->rawIdentityKey(),perPacketKey,*pktv[0].b,pktv.totalSize());
					p_PolyCopyFunction s20cf(perPacketKey,&packetId);

					pktSize = pktv.mergeMap<p_PolyCopyFunction &>(*pkt,ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,s20cf);
					if (unlikely(pktSize < ZT_PROTO_MIN_PACKET_LENGTH)) {
						ZT_SPEW("discarding packet %.16llx from %s(%s): assembled packet size: %d",packetId,source.toString().c_str(),fromAddr.toString().c_str(),pktSize);
						return;
					}

					uint64_t mac[2];
					s20cf.poly1305.finish(mac);
					static_assert((ZT_PROTO_PACKET_MAC_INDEX + 8) < ZT_PROTO_MIN_PACKET_LENGTH,"overflow");
					if (unlikely(Utils::loadAsIsEndian<uint64_t>(hdr + ZT_PROTO_PACKET_MAC_INDEX) != mac[0])) {
						ZT_SPEW("discarding packet %.16llx from %s(%s): packet MAC failed (none/poly1305)",packetId,source.toString().c_str(),fromAddr.toString().c_str());
						RR->t->incomingPacketDropped(tPtr,0xcc89c812,packetId,0,peer->identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
						return;
					}

					auth = ZT_VL1_AUTH_RESULT_FLAG_AUTHENTICATED;
				}	break;

				case ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012: {
					uint8_t perPacketKey[ZT_SALSA20_KEY_SIZE];
					Protocol::salsa2012DeriveKey(peer->rawIdentityKey(),perPacketKey,*pktv[0].b,pktv.totalSize());
					p_SalsaPolyCopyFunction s20cf(perPacketKey,&packetId);

					pktSize = pktv.mergeMap<p_SalsaPolyCopyFunction &>(*pkt,ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,s20cf);
					if (unlikely(pktSize < ZT_PROTO_MIN_PACKET_LENGTH)) {
						ZT_SPEW("discarding packet %.16llx from %s(%s): assembled packet size: %d",packetId,source.toString().c_str(),fromAddr.toString().c_str(),pktSize);
						return;
					}

					uint64_t mac[2];
					s20cf.poly1305.finish(mac);
					static_assert((ZT_PROTO_PACKET_MAC_INDEX + 8) < ZT_PROTO_MIN_PACKET_LENGTH,"overflow");
					if (unlikely(Utils::loadAsIsEndian<uint64_t>(hdr + ZT_PROTO_PACKET_MAC_INDEX) != mac[0])) {
						ZT_SPEW("discarding packet %.16llx from %s(%s): packet MAC failed (salsa/poly1305)",packetId,source.toString().c_str(),fromAddr.toString().c_str());
						RR->t->incomingPacketDropped(tPtr,0xcc89c812,packetId,0,peer->identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
						return;
					}

					auth = ZT_VL1_AUTH_RESULT_FLAG_AUTHENTICATED | ZT_VL1_AUTH_RESULT_FLAG_ENCRYPTED;
				}	break;

				case ZT_PROTO_CIPHER_SUITE__NONE: {
					// TODO
				} break;

				case ZT_PROTO_CIPHER_SUITE__AES_GMAC_SIV: {
					// TODO
				}	break;

				default:
					RR->t->incomingPacketDropped(tPtr,0x5b001099,packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
					return;
			}
		}

		if (likely(auth != 0)) {
			// If authentication was successful go on and process the packet.

			if (unlikely(pktSize < ZT_PROTO_MIN_PACKET_LENGTH)) {
				ZT_SPEW("discarding packet %.16llx from %s(%s): assembled packet size %d is smaller than minimum packet length",packetId,source.toString().c_str(),fromAddr.toString().c_str(),pktSize);
				return;
			}

			// TODO: should take instance ID into account here once that is fully implemented.
			if (unlikely(peer->deduplicateIncomingPacket(packetId))) {
				ZT_SPEW("discarding packet %.16llx from %s(%s): duplicate!",packetId,source.toString().c_str(),fromAddr.toString().c_str());
				return;
			}

			static_assert(ZT_PROTO_PACKET_VERB_INDEX < ZT_PROTO_MIN_PACKET_LENGTH,"overflow");
			const uint8_t verbFlags = pkt->unsafeData[ZT_PROTO_PACKET_VERB_INDEX];
			const Protocol::Verb verb = (Protocol::Verb)(verbFlags & ZT_PROTO_VERB_MASK);

			// Decompress packet payload if compressed. For additional safety decompression is
			// only performed on packets whose MACs have already been validated. (Only HELLO is
			// sent without this, and HELLO doesn't benefit from compression.)
			if (((verbFlags & ZT_PROTO_VERB_FLAG_COMPRESSED) != 0)&&(pktSize > ZT_PROTO_PACKET_PAYLOAD_START)) {
				SharedPtr<Buf> dec(new Buf());
				Utils::copy<ZT_PROTO_PACKET_PAYLOAD_START>(dec->unsafeData,pkt->unsafeData);
				const int uncompressedLen = LZ4_decompress_safe(
					reinterpret_cast<const char *>(pkt->unsafeData + ZT_PROTO_PACKET_PAYLOAD_START),
					reinterpret_cast<char *>(dec->unsafeData + ZT_PROTO_PACKET_PAYLOAD_START),
					pktSize - ZT_PROTO_PACKET_PAYLOAD_START,
					ZT_BUF_MEM_SIZE - ZT_PROTO_PACKET_PAYLOAD_START);
				if (likely((uncompressedLen >= 0)&&(uncompressedLen <= (ZT_BUF_MEM_SIZE - ZT_PROTO_PACKET_PAYLOAD_START)))) {
					pkt.swap(dec);
					ZT_SPEW("decompressed packet: %d -> %d",pktSize,ZT_PROTO_PACKET_PAYLOAD_START + uncompressedLen);
					pktSize = ZT_PROTO_PACKET_PAYLOAD_START + uncompressedLen;
				} else {
					RR->t->incomingPacketDropped(tPtr,0xee9e4392,packetId,0,identityFromPeerPtr(peer),path->address(),hops,verb,ZT_TRACE_PACKET_DROP_REASON_INVALID_COMPRESSED_DATA);
					return;
				}
			}

			ZT_SPEW("%s from %s(%s) (%d bytes)",Protocol::verbName(verb),source.toString().c_str(),fromAddr.toString().c_str(),pktSize);

			// NOTE: HELLO is normally sent in the clear (in terms of our usual AEAD modes) and is handled
			// above. We will try to process it here, but if so it'll still get re-authenticated via HELLO's
			// own internal authentication logic as usual. It would be abnormal to make it here with HELLO
			// but not invalid.

			bool ok = true;
			Protocol::Verb inReVerb = Protocol::VERB_NOP;
			switch(verb) {
				case Protocol::VERB_NOP:                        break;
				case Protocol::VERB_HELLO:                      ok = (bool)(m_HELLO(tPtr, path, *pkt, pktSize)); break;
				case Protocol::VERB_ERROR:                      ok = m_ERROR(tPtr, packetId, auth, path, peer, *pkt, pktSize, inReVerb); break;
				case Protocol::VERB_OK:                         ok = m_OK(tPtr, packetId, auth, path, peer, *pkt, pktSize, inReVerb); break;
				case Protocol::VERB_WHOIS:                      ok = m_WHOIS(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_RENDEZVOUS:                 ok = m_RENDEZVOUS(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_FRAME:                      ok = RR->vl2->m_FRAME(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_EXT_FRAME:                  ok = RR->vl2->m_EXT_FRAME(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_ECHO:                       ok = m_ECHO(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_MULTICAST_LIKE:             ok = RR->vl2->m_MULTICAST_LIKE(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_NETWORK_CREDENTIALS:        ok = RR->vl2->m_NETWORK_CREDENTIALS(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_NETWORK_CONFIG_REQUEST:     ok = RR->vl2->m_NETWORK_CONFIG_REQUEST(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_NETWORK_CONFIG:             ok = RR->vl2->m_NETWORK_CONFIG(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_MULTICAST_GATHER:           ok = RR->vl2->m_MULTICAST_GATHER(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_MULTICAST_FRAME_deprecated: ok = RR->vl2->m_MULTICAST_FRAME_deprecated(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_PUSH_DIRECT_PATHS:          ok = m_PUSH_DIRECT_PATHS(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_USER_MESSAGE:               ok = m_USER_MESSAGE(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_MULTICAST:                  ok = RR->vl2->m_MULTICAST(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;
				case Protocol::VERB_ENCAP:                      ok = m_ENCAP(tPtr, packetId, auth, path, peer, *pkt, pktSize); break;

				default:
					RR->t->incomingPacketDropped(tPtr,0xeeeeeff0,packetId,0,identityFromPeerPtr(peer),path->address(),hops,verb,ZT_TRACE_PACKET_DROP_REASON_UNRECOGNIZED_VERB);
					break;
			}
			if (likely(ok))
				peer->received(tPtr,path,hops,packetId,pktSize - ZT_PROTO_PACKET_PAYLOAD_START,verb,inReVerb);
		} else {
			// If decryption and authentication were not successful, try to look up identities.
			// This is rate limited by virtue of the retry rate limit timer.
			if (pktSize <= 0)
				pktSize = pktv.mergeCopy(*pkt);
			if (pktSize >= ZT_PROTO_MIN_PACKET_LENGTH) {
				ZT_SPEW("authentication failed or no peers match, queueing WHOIS for %s",source.toString().c_str());
				bool sendPending;
				{
					Mutex::Lock wl(m_whoisQueue_l);
					p_WhoisQueueItem &wq = m_whoisQueue[source];
					const unsigned int wpidx = wq.waitingPacketCount++ % ZT_VL1_MAX_WHOIS_WAITING_PACKETS;
					wq.waitingPacketSize[wpidx] = (unsigned int)pktSize;
					wq.waitingPacket[wpidx] = pkt;
					sendPending = (now - wq.lastRetry) >= ZT_WHOIS_RETRY_DELAY;
				}
				if (sendPending)
					m_sendPendingWhois(tPtr,now);
			}
		}
	} catch ( ... ) {
		RR->t->unexpectedError(tPtr,0xea1b6dea,"unexpected exception in onRemotePacket() parsing packet from %s",path->address().toString().c_str());
	}
}

void VL1::m_relay(void *tPtr, const SharedPtr<Path> &path, Address destination, SharedPtr<Buf> &pkt, int pktSize)
{
}

void VL1::m_sendPendingWhois(void *tPtr, int64_t now)
{
	const SharedPtr<Peer> root(RR->topology->root());
	if (unlikely(!root))
		return;
	const SharedPtr<Path> rootPath(root->path(now));
	if (unlikely(!rootPath))
		return;

	std::vector<Address> toSend;
	{
		Mutex::Lock wl(m_whoisQueue_l);
		for(Map<Address,p_WhoisQueueItem>::iterator wi(m_whoisQueue.begin());wi!=m_whoisQueue.end();++wi) {
			if ((now - wi->second.lastRetry) >= ZT_WHOIS_RETRY_DELAY) {
				wi->second.lastRetry = now;
				++wi->second.retries;
				toSend.push_back(wi->first);
			}
		}
	}

	if (toSend.empty())
		return;

	const SharedPtr<SymmetricKey> key(root->key());
	uint8_t outp[ZT_DEFAULT_UDP_MTU - ZT_PROTO_MIN_PACKET_LENGTH];
	std::vector<Address>::iterator a(toSend.begin());
	while (a != toSend.end()) {
		const uint64_t packetId = key->nextMessage(RR->identity.address(),root->address());
		int p = Protocol::newPacket(outp,packetId,root->address(),RR->identity.address(),Protocol::VERB_WHOIS);
		while ((a != toSend.end())&&(p < (sizeof(outp) - ZT_ADDRESS_LENGTH))) {
			a->copyTo(outp + p);
			++a;
			p += ZT_ADDRESS_LENGTH;
		}
		Protocol::armor(outp,p,key,root->cipher());
		RR->expect->sending(packetId,now);
		root->send(tPtr,now,outp,p,rootPath);
	}
}

SharedPtr<Peer> VL1::m_HELLO(void *tPtr, const SharedPtr<Path> &path, Buf &pkt, int packetSize)
{
	const uint64_t packetId = Utils::loadAsIsEndian<uint64_t>(pkt.unsafeData + ZT_PROTO_PACKET_ID_INDEX);
	const uint64_t mac = Utils::loadAsIsEndian<uint64_t>(pkt.unsafeData + ZT_PROTO_PACKET_MAC_INDEX);
	const uint8_t hops = pkt.unsafeData[ZT_PROTO_PACKET_FLAGS_INDEX] & ZT_PROTO_FLAG_FIELD_HOPS_MASK;

	const uint8_t protoVersion = pkt.lI8<ZT_PROTO_PACKET_PAYLOAD_START>();
	unsigned int versionMajor = pkt.lI8<ZT_PROTO_PACKET_PAYLOAD_START + 1>(); // LEGACY
	unsigned int versionMinor = pkt.lI8<ZT_PROTO_PACKET_PAYLOAD_START + 2>(); // LEGACY
	unsigned int versionRev = pkt.lI16<ZT_PROTO_PACKET_PAYLOAD_START + 3>(); // LEGACY
	const uint64_t timestamp = pkt.lI64<ZT_PROTO_PACKET_PAYLOAD_START + 5>();

	int ii = ZT_PROTO_PACKET_PAYLOAD_START + 13;

	// Get identity and verify that it matches the sending address in the packet.
	Identity id;
	if (unlikely(pkt.rO(ii,id) < 0)) {
		RR->t->incomingPacketDropped(tPtr,0x707a9810,packetId,0,Identity::NIL,path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
		return SharedPtr<Peer>();
	}
	if (unlikely(id.address() != Address(pkt.unsafeData + ZT_PROTO_PACKET_SOURCE_INDEX))) {
		RR->t->incomingPacketDropped(tPtr,0x707a9010,packetId,0,Identity::NIL,path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
		return SharedPtr<Peer>();
	}

	// Get the peer that matches this identity, or learn a new one if we don't know it.
	SharedPtr<Peer> peer(RR->topology->peer(tPtr,id.address(),true));
	if (peer) {
		if (peer->identity() != id) {
			RR->t->incomingPacketDropped(tPtr,0x707a9891,packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
			return SharedPtr<Peer>();
		}
		if (peer->deduplicateIncomingPacket(packetId)) {
			ZT_SPEW("discarding packet %.16llx from %s(%s): duplicate!",packetId,id.address().toString().c_str(),path->address().toString().c_str());
			return SharedPtr<Peer>();
		}
	} else {
		if (unlikely(!id.locallyValidate())) {
			RR->t->incomingPacketDropped(tPtr,0x707a9892,packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
			return SharedPtr<Peer>();
		}
		peer.set(new Peer(RR));
		if (unlikely(!peer->init(id))) {
			RR->t->incomingPacketDropped(tPtr,0x707a9893,packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_UNSPECIFIED);
			return SharedPtr<Peer>();
		}
		peer = RR->topology->add(tPtr,peer);
	}

	// ------------------------------------------------------------------------------------------------------------------
	// If we made it this far, peer is non-NULL and the identity is valid and matches it.
	// ------------------------------------------------------------------------------------------------------------------

	if (protoVersion >= 11) {
		// V2.x and newer use HMAC-SHA384 for HELLO, which offers a larger security margin
		// to guard key exchange and connection setup than typical AEAD. The packet MAC
		// field is ignored, and eventually it'll be undefined.
		uint8_t hmac[ZT_HMACSHA384_LEN];
		if (unlikely(packetSize < ZT_HMACSHA384_LEN)) {
			RR->t->incomingPacketDropped(tPtr,0xab9c9891,packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
			return SharedPtr<Peer>();
		}
		packetSize -= ZT_HMACSHA384_LEN;
		pkt.unsafeData[ZT_PROTO_PACKET_FLAGS_INDEX] &= ~ZT_PROTO_FLAG_FIELD_HOPS_MASK; // mask hops to 0
		Utils::storeAsIsEndian<uint64_t>(pkt.unsafeData + ZT_PROTO_PACKET_MAC_INDEX,0); // set MAC field to 0
		HMACSHA384(peer->identityHelloHmacKey(),pkt.unsafeData,packetSize,hmac);
		if (unlikely(!Utils::secureEq(hmac,pkt.unsafeData + packetSize,ZT_HMACSHA384_LEN))) {
			RR->t->incomingPacketDropped(tPtr,0x707a9891,packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
			return SharedPtr<Peer>();
		}
	} else {
		// Older versions use Poly1305 MAC (but no whole packet encryption) for HELLO.
		if (likely(packetSize > ZT_PROTO_PACKET_ENCRYPTED_SECTION_START)) {
			uint8_t perPacketKey[ZT_SALSA20_KEY_SIZE];
			Protocol::salsa2012DeriveKey(peer->rawIdentityKey(),perPacketKey,pkt,packetSize);
			uint8_t macKey[ZT_POLY1305_KEY_SIZE];
			Salsa20(perPacketKey,&packetId).crypt12(Utils::ZERO256,macKey,ZT_POLY1305_KEY_SIZE);
			Poly1305 poly1305(macKey);
			poly1305.update(pkt.unsafeData + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,packetSize - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START);
			uint64_t polyMac[2];
			poly1305.finish(polyMac);
			if (unlikely(mac != polyMac[0])) {
				RR->t->incomingPacketDropped(tPtr,0x11bfff82,packetId,0,id,path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
				return SharedPtr<Peer>();
			}
		} else {
			RR->t->incomingPacketDropped(tPtr,0x11bfff81,packetId,0,id,path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
			return SharedPtr<Peer>();
		}
	}

	// ------------------------------------------------------------------------------------------------------------------
	// This far means we passed MAC (Poly1305 or HMAC-SHA384 for newer peers)
	// ------------------------------------------------------------------------------------------------------------------

	// LEGACY: this is superseded by the sent-to field in the meta-data dictionary if present.
	InetAddress sentTo;
	if (unlikely(pkt.rO(ii,sentTo) < 0)) {
		RR->t->incomingPacketDropped(tPtr,0x707a9811,packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
		return SharedPtr<Peer>();
	}

	const SharedPtr<SymmetricKey> key(peer->identityKey());

	if (protoVersion >= 11) {
		// V2.x and newer supports an encrypted section and has a new OK format.
		if (likely((ii + 12) < packetSize)) {
			uint64_t ctrNonce[2];
			ctrNonce[0] = Utils::loadAsIsEndian<uint64_t>(pkt.unsafeData + ii);
#if __BYTE_ORDER == __BIG_ENDIAN
			ctrNonce[1] = ((uint64_t)Utils::loadAsIsEndian<uint32_t>(pkt.unsafeData + ii + 8)) << 32U;
#else
			ctrNonce[1] = Utils::loadAsIsEndian<uint32_t>(pkt.unsafeData + ii + 8);
#endif
			ii += 12;
			AES::CTR ctr(peer->identityHelloDictionaryEncryptionCipher());
			ctr.init(reinterpret_cast<uint8_t *>(ctrNonce),pkt.unsafeData + ii);
			ctr.crypt(pkt.unsafeData + ii,packetSize - ii);
			ctr.finish();

			const unsigned int dictSize = pkt.rI16(ii);
			if (unlikely((ii + dictSize) > packetSize)) {
				RR->t->incomingPacketDropped(tPtr,0x707a9815,packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
				return peer;
			}
			Dictionary md;
			if (!md.decode(pkt.unsafeData + ii,dictSize)) {
				RR->t->incomingPacketDropped(tPtr,0x707a9816,packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
				return peer;
			}

			if (!md.empty()) {
				InetAddress sentTo2;
				if (md.getO(ZT_PROTO_HELLO_NODE_META_PHYSICAL_DEST,sentTo2))
					sentTo = sentTo2;
				const uint64_t packedVer = md.getUI(ZT_PROTO_HELLO_NODE_META_SOFTWARE_VERSION);
				if (packedVer != 0) {
					versionMajor = (unsigned int)(packedVer >> 48U) & 0xffffU;
					versionMinor = (unsigned int)(packedVer >> 32U) & 0xffffU;
					versionRev = (unsigned int)(packedVer >> 16U) & 0xffffU;
				}
				const uint32_t probeToken = (uint32_t)md.getUI(ZT_PROTO_HELLO_NODE_META_PROBE_TOKEN);
				if (probeToken != 0)
					peer->setProbeToken(probeToken);
			}
		}

		Protocol::newPacket(pkt,key->nextMessage(RR->identity.address(),peer->address()),peer->address(),RR->identity.address(),Protocol::VERB_OK);
		ii = ZT_PROTO_PACKET_PAYLOAD_START;
		pkt.wI8(ii,Protocol::VERB_HELLO);
		pkt.wI64(ii,packetId);
		pkt.wI64(ii,timestamp);
		pkt.wI8(ii,(uint8_t)protoVersion);

		FCV<uint8_t,1024> okmd;
		pkt.wI16(ii,(uint16_t)okmd.size());
		pkt.wB(ii,okmd.data(),okmd.size());
	} else {
		// V1.x has nothing more for this version to parse, and has an older OK format.
		Protocol::newPacket(pkt,key->nextMessage(RR->identity.address(),peer->address()),peer->address(),RR->identity.address(),Protocol::VERB_OK);
		ii = ZT_PROTO_PACKET_PAYLOAD_START;
		pkt.wI8(ii,Protocol::VERB_HELLO);
		pkt.wI64(ii,packetId);
		pkt.wI64(ii,timestamp);
		pkt.wI8(ii,(uint8_t)protoVersion);
		pkt.wI8(ii,(uint8_t)versionMajor);
		pkt.wI8(ii,(uint8_t)versionMinor);
		pkt.wI16(ii,(uint16_t)versionRev);
		pkt.wO(ii,path->address());
		pkt.wI16(ii,0);
	}

	peer->setRemoteVersion(protoVersion,versionMajor,versionMinor,versionRev);
	peer->send(tPtr,RR->node->now(),pkt.unsafeData,ii,path);
}

bool VL1::m_ERROR(void *tPtr,const uint64_t packetId,const unsigned int auth, const SharedPtr<Path> &path, const SharedPtr<Peer> &peer, Buf &pkt, int packetSize, Protocol::Verb &inReVerb)
{
#if 0
	if (packetSize < (int)sizeof(Protocol::ERROR::Header)) {
		RR->t->incomingPacketDropped(tPtr,0x3beb1947,0,0,identityFromPeerPtr(peer),path->address(),0,Protocol::VERB_ERROR,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return false;
	}
	Protocol::ERROR::Header &eh = pkt.as<Protocol::ERROR::Header>();
	inReVerb = (Protocol::Verb)eh.inReVerb;

	const int64_t now = RR->node->now();
	if (!RR->expect->expecting(eh.inRePacketId,now)) {
		RR->t->incomingPacketDropped(tPtr,0x4c1f1ff7,0,0,identityFromPeerPtr(peer),path->address(),0,Protocol::VERB_OK,ZT_TRACE_PACKET_DROP_REASON_REPLY_NOT_EXPECTED);
		return false;
	}

	switch(eh.error) {

		//case Protocol::ERROR_INVALID_REQUEST:
		//case Protocol::ERROR_BAD_PROTOCOL_VERSION:
		//case Protocol::ERROR_CANNOT_DELIVER:
		default:
			break;

		case Protocol::ERROR_OBJ_NOT_FOUND:
			if (eh.inReVerb == Protocol::VERB_NETWORK_CONFIG_REQUEST) {
			}
			break;

		case Protocol::ERROR_UNSUPPORTED_OPERATION:
			if (eh.inReVerb == Protocol::VERB_NETWORK_CONFIG_REQUEST) {
			}
			break;

		case Protocol::ERROR_NEED_MEMBERSHIP_CERTIFICATE:
			break;

		case Protocol::ERROR_NETWORK_ACCESS_DENIED_:
			if (eh.inReVerb == Protocol::VERB_NETWORK_CONFIG_REQUEST) {
			}
			break;

	}
	return true;
#endif
}

bool VL1::m_OK(void *tPtr,const uint64_t packetId,const unsigned int auth, const SharedPtr<Path> &path, const SharedPtr<Peer> &peer, Buf &pkt, int packetSize, Protocol::Verb &inReVerb)
{
	int ii = ZT_PROTO_PACKET_PAYLOAD_START + 13;

	inReVerb = (Protocol::Verb)pkt.rI8(ii);
	const uint64_t inRePacketId = pkt.rI64(ii);
	if (unlikely(Buf::readOverflow(ii,packetSize))) {
		RR->t->incomingPacketDropped(tPtr,0x4c1f1ff7,packetId,0,identityFromPeerPtr(peer),path->address(),0,Protocol::VERB_OK,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return false;
	}

	const int64_t now = RR->node->now();
	if (!RR->expect->expecting(inRePacketId,now)) {
		RR->t->incomingPacketDropped(tPtr,0x4c1f1ff8,packetId,0,identityFromPeerPtr(peer),path->address(),0,Protocol::VERB_OK,ZT_TRACE_PACKET_DROP_REASON_REPLY_NOT_EXPECTED);
		return false;
	}

	ZT_SPEW("got OK in-re %s (packet ID %.16llx) from %s(%s)",Protocol::verbName(inReVerb),inRePacketId,peer->address().toString().c_str(),path->address().toString().c_str());

	switch(inReVerb) {

		case Protocol::VERB_HELLO:
			break;

		case Protocol::VERB_WHOIS:
			break;

		case Protocol::VERB_NETWORK_CONFIG_REQUEST:
			break;

		case Protocol::VERB_MULTICAST_GATHER:
			break;

	}

	return true;
}

bool VL1::m_WHOIS(void *tPtr,const uint64_t packetId,const unsigned int auth, const SharedPtr<Path> &path, const SharedPtr<Peer> &peer, Buf &pkt, int packetSize)
{
#if 0
	if (packetSize < (int)sizeof(Protocol::OK::Header)) {
		RR->t->incomingPacketDropped(tPtr,0x4c1f1ff7,0,0,identityFromPeerPtr(peer),path->address(),0,Protocol::VERB_OK,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return false;
	}
	Protocol::Header &ph = pkt.as<Protocol::Header>();

	if (!peer->rateGateInboundWhoisRequest(RR->node->now())) {
		RR->t->incomingPacketDropped(tPtr,0x19f7194a,ph.packetId,0,peer->identity(),path->address(),Protocol::packetHops(ph),Protocol::VERB_WHOIS,ZT_TRACE_PACKET_DROP_REASON_RATE_LIMIT_EXCEEDED);
		return true;
	}

	Buf outp;
	Protocol::OK::WHOIS &outh = outp.as<Protocol::OK::WHOIS>();
	int ptr = sizeof(Protocol::Header);
	while ((ptr + ZT_ADDRESS_LENGTH) <= packetSize) {
		outh.h.h.packetId = Protocol::getPacketId();
		peer->address().copyTo(outh.h.h.destination);
		RR->identity.address().copyTo(outh.h.h.source);
		outh.h.h.flags = 0;
		outh.h.h.verb = Protocol::VERB_OK;

		outh.h.inReVerb = Protocol::VERB_WHOIS;
		outh.h.inRePacketId = ph.packetId;

		int outl = sizeof(Protocol::OK::WHOIS);
		while ( ((ptr + ZT_ADDRESS_LENGTH) <= packetSize) && ((outl + ZT_IDENTITY_MARSHAL_SIZE_MAX + ZT_LOCATOR_MARSHAL_SIZE_MAX) < ZT_PROTO_MAX_PACKET_LENGTH) ) {
			const SharedPtr<Peer> &wp(RR->topology->peer(tPtr,Address(pkt.unsafeData + ptr)));
			if (wp) {
				outp.wO(outl,wp->identity());
				if (peer->remoteVersionProtocol() >= 11) { // older versions don't know what a locator is
					const Locator loc(wp->locator());
					outp.wO(outl,loc);
				}
				if (Buf::writeOverflow(outl)) { // sanity check, shouldn't be possible
					RR->t->unexpectedError(tPtr,0xabc0f183,"Buf write overflow building OK(WHOIS) to reply to %s",Trace::str(peer->address(),path).s);
					return false;
				}
			}
			ptr += ZT_ADDRESS_LENGTH;
		}

		if (outl > (int)sizeof(Protocol::OK::WHOIS)) {
			Protocol::armor(outp,outl,peer->key(),peer->cipher());
			path->send(RR,tPtr,outp.unsafeData,outl,RR->node->now());
		}
	}

	return true;
#endif
}

bool VL1::m_RENDEZVOUS(void *tPtr,const uint64_t packetId,const unsigned int auth, const SharedPtr<Path> &path, const SharedPtr<Peer> &peer, Buf &pkt, int packetSize)
{
#if 0
	if (RR->topology->isRoot(peer->identity())) {
		if (packetSize < (int)sizeof(Protocol::RENDEZVOUS)) {
			RR->t->incomingPacketDropped(tPtr,0x43e90ab3,Protocol::packetId(pkt,packetSize),0,peer->identity(),path->address(),Protocol::packetHops(pkt,packetSize),Protocol::VERB_RENDEZVOUS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return false;
		}
		Protocol::RENDEZVOUS &rdv = pkt.as<Protocol::RENDEZVOUS>();

		const SharedPtr<Peer> with(RR->topology->peer(tPtr,Address(rdv.peerAddress)));
		if (with) {
			const int64_t now = RR->node->now();
			const unsigned int port = Utils::ntoh(rdv.port);
			if (port != 0) {
				switch(rdv.addressLength) {
					case 4:
					case 16:
						if ((int)(sizeof(Protocol::RENDEZVOUS) + rdv.addressLength) <= packetSize) {
							const InetAddress atAddr(pkt.unsafeData + sizeof(Protocol::RENDEZVOUS),rdv.addressLength,port);
							peer->tryToContactAt(tPtr,Endpoint(atAddr),now,false);
							RR->t->tryingNewPath(tPtr,0x55a19aaa,with->identity(),atAddr,path->address(),Protocol::packetId(pkt,packetSize),Protocol::VERB_RENDEZVOUS,peer->identity(),ZT_TRACE_TRYING_NEW_PATH_REASON_RENDEZVOUS);
						}
						break;
					case 255: {
						Endpoint ep;
						int p = sizeof(Protocol::RENDEZVOUS);
						int epl = pkt.rO(p,ep);
						if ((epl > 0) && (ep) && (!Buf::readOverflow(p,packetSize))) {
							switch (ep.type()) {
								case Endpoint::TYPE_INETADDR_V4:
								case Endpoint::TYPE_INETADDR_V6:
									peer->tryToContactAt(tPtr,ep,now,false);
									RR->t->tryingNewPath(tPtr,0x55a19aab,with->identity(),ep.inetAddr(),path->address(),Protocol::packetId(pkt,packetSize),Protocol::VERB_RENDEZVOUS,peer->identity(),ZT_TRACE_TRYING_NEW_PATH_REASON_RENDEZVOUS);
									break;
								default:
									break;
							}
						}
					} break;
				}
			}
		}
	}
	return true;
#endif
}

bool VL1::m_ECHO(void *tPtr,const uint64_t packetId,const unsigned int auth, const SharedPtr<Path> &path, const SharedPtr<Peer> &peer, Buf &pkt, int packetSize)
{
#if 0
	const uint64_t packetId = Protocol::packetId(pkt,packetSize);
	const uint64_t now = RR->node->now();
	if (packetSize < (int)sizeof(Protocol::Header)) {
		RR->t->incomingPacketDropped(tPtr,0x14d70bb0,packetId,0,peer->identity(),path->address(),Protocol::packetHops(pkt,packetSize),Protocol::VERB_ECHO,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return false;
	}

	if (peer->rateGateEchoRequest(now)) {
		Buf outp;
		Protocol::OK::ECHO &outh = outp.as<Protocol::OK::ECHO>();
		outh.h.h.packetId = Protocol::getPacketId();
		peer->address().copyTo(outh.h.h.destination);
		RR->identity.address().copyTo(outh.h.h.source);
		outh.h.h.flags = 0;
		outh.h.h.verb = Protocol::VERB_OK;
		outh.h.inReVerb = Protocol::VERB_ECHO;
		outh.h.inRePacketId = packetId;
		int outl = sizeof(Protocol::OK::ECHO);
		outp.wB(outl,pkt.unsafeData + sizeof(Protocol::Header),packetSize - sizeof(Protocol::Header));

		if (Buf::writeOverflow(outl)) {
			RR->t->incomingPacketDropped(tPtr,0x14d70bb0,packetId,0,peer->identity(),path->address(),Protocol::packetHops(pkt,packetSize),Protocol::VERB_ECHO,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return false;
		}

		Protocol::armor(outp,outl,peer->key(),peer->cipher());
		path->send(RR,tPtr,outp.unsafeData,outl,now);
	} else {
		RR->t->incomingPacketDropped(tPtr,0x27878bc1,packetId,0,peer->identity(),path->address(),Protocol::packetHops(pkt,packetSize),Protocol::VERB_ECHO,ZT_TRACE_PACKET_DROP_REASON_RATE_LIMIT_EXCEEDED);
	}

	return true;
#endif
}

bool VL1::m_PUSH_DIRECT_PATHS(void *tPtr,const uint64_t packetId,const unsigned int auth, const SharedPtr<Path> &path, const SharedPtr<Peer> &peer, Buf &pkt, int packetSize)
{
#if 0
	if (packetSize < (int)sizeof(Protocol::PUSH_DIRECT_PATHS)) {
		RR->t->incomingPacketDropped(tPtr,0x1bb1bbb1,Protocol::packetId(pkt,packetSize),0,peer->identity(),path->address(),Protocol::packetHops(pkt,packetSize),Protocol::VERB_PUSH_DIRECT_PATHS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return false;
	}
	Protocol::PUSH_DIRECT_PATHS &pdp = pkt.as<Protocol::PUSH_DIRECT_PATHS>();

	int ptr = sizeof(Protocol::PUSH_DIRECT_PATHS);
	const unsigned int numPaths = Utils::ntoh(pdp.numPaths);
	InetAddress a;
	Endpoint ep;
	for(unsigned int pi=0;pi<numPaths;++pi) {
		/*const uint8_t flags = pkt.rI8(ptr);*/ ++ptr; // flags are not presently used

		const int xas = (int)pkt.rI16(ptr);
		//const uint8_t *const extendedAttrs = pkt.rBnc(ptr,xas);
		ptr += xas;

		const unsigned int addrType = pkt.rI8(ptr);
		const unsigned int addrRecordLen = pkt.rI8(ptr);
		if (addrRecordLen == 0) {
			RR->t->incomingPacketDropped(tPtr,0xaed00118,pdp.h.packetId,0,peer->identity(),path->address(),Protocol::packetHops(pdp.h),Protocol::VERB_PUSH_DIRECT_PATHS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return false;
		}
		if (Buf::readOverflow(ptr,packetSize)) {
			RR->t->incomingPacketDropped(tPtr,0xb450e10f,pdp.h.packetId,0,peer->identity(),path->address(),Protocol::packetHops(pdp.h),Protocol::VERB_PUSH_DIRECT_PATHS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return false;
		}

		const void *addrBytes = nullptr;
		unsigned int addrLen = 0;
		unsigned int addrPort = 0;
		switch(addrType) {
			case 0:
				addrBytes = pkt.rBnc(ptr,addrRecordLen);
				addrLen = addrRecordLen;
				break;
			case 4:
				addrBytes = pkt.rBnc(ptr,4);
				addrLen = 4;
				addrPort = pkt.rI16(ptr);
				break;
			case 6:
				addrBytes = pkt.rBnc(ptr,16);
				addrLen = 16;
				addrPort = pkt.rI16(ptr);
				break;
			//case 200:
				// TODO: this would be a WebRTC SDP offer contained in the extended attrs field
				//break;
			default: break;
		}

		if (Buf::readOverflow(ptr,packetSize)) {
			RR->t->incomingPacketDropped(tPtr,0xb4d0f10f,pdp.h.packetId,0,peer->identity(),path->address(),Protocol::packetHops(pdp.h),Protocol::VERB_PUSH_DIRECT_PATHS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return false;
		}

		if (addrPort) {
			a.set(addrBytes,addrLen,addrPort);
		} else if (addrLen) {
			if (ep.unmarshal(reinterpret_cast<const uint8_t *>(addrBytes),(int)addrLen) <= 0) {
				RR->t->incomingPacketDropped(tPtr,0x00e0f00d,pdp.h.packetId,0,peer->identity(),path->address(),Protocol::packetHops(pdp.h),Protocol::VERB_PUSH_DIRECT_PATHS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
				return false;
			}

			switch(ep.type()) {
				case Endpoint::TYPE_INETADDR_V4:
				case Endpoint::TYPE_INETADDR_V6:
					a = ep.inetAddr();
					break;
				default: // other types are not supported yet
					break;
			}
		}

		if (a) {
			RR->t->tryingNewPath(tPtr,0xa5ab1a43,peer->identity(),a,path->address(),Protocol::packetId(pkt,packetSize),Protocol::VERB_RENDEZVOUS,peer->identity(),ZT_TRACE_TRYING_NEW_PATH_REASON_RECEIVED_PUSH_DIRECT_PATHS);
		}

		ptr += (int)addrRecordLen;
	}

	// TODO: add to a peer try-queue

	return true;
#endif
}

bool VL1::m_USER_MESSAGE(void *tPtr,const uint64_t packetId,const unsigned int auth, const SharedPtr<Path> &path, const SharedPtr<Peer> &peer, Buf &pkt, int packetSize)
{
	// TODO
	return true;
}

bool VL1::m_ENCAP(void *tPtr,const uint64_t packetId,const unsigned int auth, const SharedPtr<Path> &path, const SharedPtr<Peer> &peer, Buf &pkt, int packetSize)
{
	// TODO: not implemented yet
	return true;
}

} // namespace ZeroTier
