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

ZT_ALWAYS_INLINE const Identity &identityFromPeerPtr(const SharedPtr<Peer> &p)
{
	if (p)
		return p->identity();
	return Identity::NIL;
}

} // anonymous namespace

VL1::VL1(const RuntimeEnvironment *renv) :
	RR(renv)
{
}

VL1::~VL1()
{
}

void VL1::onRemotePacket(void *const tPtr,const int64_t localSocket,const InetAddress &fromAddr,SharedPtr<Buf> &data,const unsigned int len)
{
	// Get canonical Path object for this originating address and local socket pair.
	const SharedPtr<Path> path(RR->topology->path(localSocket,fromAddr));

	const int64_t now = RR->node->now();

	// Update path's last receive time (this is updated when anything is received at all, even if invalid or a keepalive)
	path->received(now);

	try {
		// Handle 8-byte short probes, which are used as a low-bandwidth way to initiate a real handshake.
		// These are only minimally "secure" in the sense that they are unique per graph edge (sender->recipient)
		// to within 1/2^64 but can easily be replayed. We rate limit this to prevent ZeroTier being used as
		// a vector in DDOS amplification attacks, then send a larger fully authenticated message to initiate
		// a handshake. We do not send HELLO since we don't want this to be a vector for third parties to
		// mass-probe for ZeroTier nodes and obtain all of the information in a HELLO. This isn't a huge risk
		// but we might as well avoid it. When the peer receives NOP on a path that hasn't been handshaked yet
		// it will send its own HELLO to which we will respond with a fully encrypted OK(HELLO).
		if (len == ZT_PROTO_PROBE_LENGTH) {
			const SharedPtr<Peer> peer(RR->topology->peerByProbe(Utils::loadAsIsEndian<uint64_t>(data->b)));
			if ((peer)&&(peer->rateGateInboundProbe(now))) {
				peer->sendNOP(tPtr,path->localSocket(),path->address(),now);
				path->sent(now);
			}
			return;
		}

		// Discard any other runt packets that aren't probes. These are likely to be keepalives.
		// No reason to bother even logging them. Note that the last receive time for the path
		// was still updated, so tiny keepalives do keep the path alive.
		if (len < ZT_PROTO_MIN_FRAGMENT_LENGTH)
			return;

		// A vector of slices of buffers that aspires to eventually hold an assembled packet.
		// These are reassembled into a single contiguous buffer at the same time as decryption
		// and authentication.
		FCV<Buf::Slice,ZT_MAX_PACKET_FRAGMENTS> pktv;

		// Destination address of packet (filled below)
		Address destination;

		if (data->b[ZT_PROTO_PACKET_FRAGMENT_INDICATOR_INDEX] == ZT_PROTO_PACKET_FRAGMENT_INDICATOR) {
			// Fragment -----------------------------------------------------------------------------------------------------

			const Protocol::FragmentHeader &fragmentHeader = data->as<Protocol::FragmentHeader>();
			destination.setTo(fragmentHeader.destination);

			if (destination != RR->identity.address()) {
				_relay(tPtr,path,destination,data,len);
				return;
			}

			switch (_inputPacketAssembler.assemble(
				fragmentHeader.packetId,
				pktv,
				data,
				ZT_PROTO_PACKET_FRAGMENT_PAYLOAD_START_AT,
				(unsigned int)(len - ZT_PROTO_PACKET_FRAGMENT_PAYLOAD_START_AT),
				fragmentHeader.counts & 0xfU, // fragment number
				fragmentHeader.counts >> 4U,  // total number of fragments in message is specified in each fragment
				now,
				path,
				ZT_MAX_INCOMING_FRAGMENTS_PER_PATH)) {
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
			// Not fragment, meaning whole packet or head of series with fragments ------------------------------------------

			if (len < ZT_PROTO_MIN_PACKET_LENGTH)
				return;
			const Protocol::Header &packetHeader = data->as<Protocol::Header>();
			destination.setTo(packetHeader.destination);

			if (destination != RR->identity.address()) {
				_relay(tPtr,path,destination,data,len);
				return;
			}

			if ((packetHeader.flags & ZT_PROTO_FLAG_FRAGMENTED) != 0) {
				switch (_inputPacketAssembler.assemble(
					packetHeader.packetId,
					pktv,
					data,
					0,
					len,
					0, // always the zero'eth fragment
					0, // this is specified in fragments, not in the head
					now,
					path,
					ZT_MAX_INCOMING_FRAGMENTS_PER_PATH)) {
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
			} else { // packet isn't fragmented, so skip the Defragmenter logic completely.
				Buf::Slice &s = pktv.push();
				s.b.swap(data);
				s.s = 0;
				s.e = len;
			}
		}

		// Packet defragmented and apparently addressed to this node ------------------------------------------------------

		// Subject pktv to a few sanity checks just to make sure Defragmenter worked correctly and
		// there is enough room in each slice to shift their contents to sizes that are multiples
		// of 64 if needed for crypto.
		if ((pktv.empty()) || (((int)pktv[0].e - (int)pktv[0].s) < sizeof(Protocol::Header))) {
			RR->t->unexpectedError(tPtr,0x3df19990,"empty or undersized packet vector after parsing packet from %s of length %d",Trace::str(path->address()).s,(int)len);
			return;
		}
		for(FCV<Buf::Slice,ZT_MAX_PACKET_FRAGMENTS>::const_iterator s(pktv.begin());s!=pktv.end();++s) {
			if ((s->e > (ZT_BUF_MEM_SIZE - 64))||(s->s > s->e))
				return;
		}

		Protocol::Header *ph = &(pktv[0].b->as<Protocol::Header>(pktv[0].s));
		const Address source(ph->source);

		if (source == RR->identity.address())
			return;
		SharedPtr<Peer> peer(RR->topology->peer(tPtr,source));

		Buf::Slice pkt;
		bool authenticated = false;

		const uint8_t hops = Protocol::packetHops(*ph);
		const uint8_t cipher = Protocol::packetCipher(*ph);

		unsigned int packetSize = pktv[0].e - pktv[0].s;
		for(FCV<Buf::Slice,ZT_MAX_PACKET_FRAGMENTS>::const_iterator s(pktv.begin()+1);s!=pktv.end();++s)
			packetSize += s->e - s->s;
		if (packetSize > ZT_PROTO_MAX_PACKET_LENGTH) {
			RR->t->incomingPacketDropped(tPtr,0x010348da,ph->packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return;
		}

		// If we don't know this peer and this is not a HELLO, issue a WHOIS and enqueue this packet to try again.
		if ((!peer)&&(!(((cipher == ZT_PROTO_CIPHER_SUITE__POLY1305_NONE)||(cipher == ZT_PROTO_CIPHER_SUITE__NONE))&&((ph->verb & 0x1fU) == Protocol::VERB_HELLO)))) {
			pkt = Buf::assembleSliceVector(pktv);
			if (pkt.e < ZT_PROTO_MIN_PACKET_LENGTH) {
				RR->t->incomingPacketDropped(tPtr,0xbada9366,ph->packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
				return;
			}
			{
				Mutex::Lock wl(_whoisQueue_l);
				_WhoisQueueItem &wq = _whoisQueue[source];
				wq.inboundPackets.push_back(pkt);
			}
			_sendPendingWhois(tPtr,now);
			return;
		}

		switch(cipher) {
			case ZT_PROTO_CIPHER_SUITE__POLY1305_NONE:
				if (peer) {
					pkt = Buf::assembleSliceVector(pktv);
					if (pkt.e < ZT_PROTO_MIN_PACKET_LENGTH) {
						RR->t->incomingPacketDropped(tPtr,0x432aa9da,ph->packetId,0,peer->identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
						return;
					}
					ph = &(pkt.b->as<Protocol::Header>());

					// Generate one-time-use MAC key using Salsa20.
					uint8_t perPacketKey[ZT_PEER_SECRET_KEY_LENGTH];
					uint8_t macKey[ZT_POLY1305_KEY_LEN];
					Protocol::salsa2012DeriveKey(peer->key(),perPacketKey,*pktv[0].b,packetSize);
					Salsa20(perPacketKey,&ph->packetId).crypt12(Utils::ZERO256,macKey,ZT_POLY1305_KEY_LEN);

					// Verify packet MAC.
					uint64_t mac[2];
					poly1305(mac,pkt.b->b + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,packetSize - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,macKey);
					if (ph->mac != mac[0]) {
						RR->t->incomingPacketDropped(tPtr,0xcc89c812,ph->packetId,0,peer->identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
						return;
					}
					authenticated = true;
				}
				break;

			case ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012:
				if (peer) {
					// Derive per-packet key using symmetric key plus some data from the packet header.
					uint8_t perPacketKey[ZT_PEER_SECRET_KEY_LENGTH];
					Protocol::salsa2012DeriveKey(peer->key(),perPacketKey,*pktv[0].b,packetSize);
					Salsa20 s20(perPacketKey,&ph->packetId);

					// Do one Salsa20 block to generate the one-time-use Poly1305 key.
					uint8_t macKey[ZT_POLY1305_KEY_LEN];
					s20.crypt12(Utils::ZERO256,macKey,ZT_POLY1305_KEY_LEN);

					// Get a buffer to store the decrypted and fully contiguous packet.
					pkt.b.set(new Buf());

					// Salsa20 is a stream cipher but it's only seekable to multiples of 64 bytes.
					// This moves data in slices around so that all slices have sizes that are
					// multiples of 64 except the last slice. Note that this does not corrupt
					// the assembled slice vector, just moves data around.
					if (pktv.size() > 1) {
						unsigned int prevOverflow,i;
						for (typename FCV<Buf::Slice,ZT_MAX_PACKET_FRAGMENTS>::iterator ps(pktv.begin()),s(ps + 1);s!=pktv.end();) {
							prevOverflow = (ps->e - ps->s) & 63U; // amount by which previous exceeds a multiple of 64
							for(i=0;i<prevOverflow;++i) {
								if (s->s >= s->e)
									goto next_slice;
								ps->b->b[ps->e++] = s->b->b[s->s++]; // move from head of current to end of previous
							}
							next_slice: ps = s++;
						}
					}

					// Simultaneously decrypt and assemble packet into a contiguous buffer.
					// Since we moved data around above all slices will have sizes that are
					// multiples of 64.
					memcpy(pkt.b->b,ph,sizeof(Protocol::Header));
					pkt.e = sizeof(Protocol::Header);
					for(FCV<Buf::Slice,ZT_MAX_PACKET_FRAGMENTS>::iterator s(pktv.begin());s!=pktv.end();++s) {
						const unsigned int sliceSize = s->e - s->s;
						s20.crypt12(s->b->b + s->s,pkt.b->b + pkt.e,sliceSize);
						pkt.e += sliceSize;
					}
					ph = &(pkt.b->as<Protocol::Header>());

					// Verify packet MAC.
					uint64_t mac[2];
					poly1305(mac,pkt.b->b + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,packetSize - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,macKey);
					if (ph->mac != mac[0]) {
						RR->t->incomingPacketDropped(tPtr,0xbc881231,ph->packetId,0,peer->identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
						return;
					}
					authenticated = true;
				} else {
					RR->t->incomingPacketDropped(tPtr,0xb0b01999,ph->packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
					return;
				}
				break;

			case ZT_PROTO_CIPHER_SUITE__NONE: {
				// CIPHER_SUITE__NONE is only used with trusted paths. Verification is performed by
				// checking the address and the presence of its corresponding trusted path ID in the
				// packet header's MAC field.

				pkt = Buf::assembleSliceVector(pktv);
				if (pkt.e < ZT_PROTO_MIN_PACKET_LENGTH)
					RR->t->incomingPacketDropped(tPtr,0x3d3337df,ph->packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
				ph = &(pkt.b->as<Protocol::Header>());

				if (RR->topology->shouldInboundPathBeTrusted(path->address(),Utils::ntoh(ph->mac))) {
					authenticated = true;
				} else {
					RR->t->incomingPacketDropped(tPtr,0x2dfa910b,ph->packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_NOT_TRUSTED_PATH);
					return;
				}
			} break;

			//case ZT_PROTO_CIPHER_SUITE__AES_GCM_NRH:
			//	if (peer) {
			//	}
			//	break;

			default:
				RR->t->incomingPacketDropped(tPtr,0x5b001099,ph->packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
				return;
		}

		// Packet fully assembled, authenticated 'true' if already authenticated via MAC ----------------------------------

		// Return any still held buffers in pktv to the buffer pool.
		pktv.clear();

		const Protocol::Verb verb = (Protocol::Verb)(ph->verb & ZT_PROTO_VERB_MASK);

		// All verbs except HELLO require authentication before being handled. The HELLO
		// handler does its own authentication.
		if (((!authenticated)||(!peer))&&(verb != Protocol::VERB_HELLO)) {
			RR->t->incomingPacketDropped(tPtr,0x5b001099,ph->packetId,0,identityFromPeerPtr(peer),path->address(),hops,verb,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
			return;
		}

		// Decompress packet payload if compressed. For additional safety decompression is
		// only performed on packets whose MACs have already been validated. (Only HELLO is
		// sent without this, and HELLO doesn't benefit from compression.)
		if ((ph->verb & ZT_PROTO_VERB_FLAG_COMPRESSED) != 0) {
			if (!authenticated) {
				RR->t->incomingPacketDropped(tPtr,0x390bcd0a,ph->packetId,0,identityFromPeerPtr(peer),path->address(),hops,verb,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
				return;
			}

			SharedPtr<Buf> nb(new Buf());
			const int uncompressedLen = LZ4_decompress_safe(
				reinterpret_cast<const char *>(pkt.b->b + ZT_PROTO_PACKET_PAYLOAD_START),
				reinterpret_cast<char *>(nb->b),
				(int)(packetSize - ZT_PROTO_PACKET_PAYLOAD_START),
				ZT_BUF_MEM_SIZE - ZT_PROTO_PACKET_PAYLOAD_START);

			if ((uncompressedLen > 0)&&(uncompressedLen <= (ZT_BUF_MEM_SIZE - ZT_PROTO_PACKET_PAYLOAD_START))) {
				pkt.b.swap(nb);
				pkt.e = packetSize = (unsigned int)uncompressedLen;
			} else {
				RR->t->incomingPacketDropped(tPtr,0xee9e4392,ph->packetId,0,identityFromPeerPtr(peer),path->address(),hops,verb,ZT_TRACE_PACKET_DROP_REASON_INVALID_COMPRESSED_DATA);
				return;
			}
		}

		/*
		 * Important notes:
		 *
		 * All verbs except HELLO assume that authenticated is true and peer is non-NULL.
		 * This is checked above. HELLO will accept either case and always performs its
		 * own secondary validation. The path argument is never NULL.
		 *
		 * VL1 and VL2 are conceptually separate layers of the ZeroTier protocol. In the
		 * code they are almost entirely logically separate. To make the code easier to
		 * understand the handlers for VL2 data paths have been moved to a VL2 class.
		 */

		bool ok = true; // set to false if a packet turns out to be invalid
		Protocol::Verb inReVerb = Protocol::VERB_NOP; // set via result parameter to _ERROR and _OK
		switch(verb) {
			case Protocol::VERB_NOP:                        break;
			case Protocol::VERB_HELLO:                      ok = _HELLO(tPtr,path,peer,*pkt.b,(int)packetSize,authenticated); break;
			case Protocol::VERB_ERROR:                      ok = _ERROR(tPtr,path,peer,*pkt.b,(int)packetSize,inReVerb); break;
			case Protocol::VERB_OK:                         ok = _OK(tPtr,path,peer,*pkt.b,(int)packetSize,inReVerb); break;
			case Protocol::VERB_WHOIS:                      ok = _WHOIS(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_RENDEZVOUS:                 ok = _RENDEZVOUS(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_FRAME:                      ok = RR->vl2->_FRAME(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_EXT_FRAME:                  ok = RR->vl2->_EXT_FRAME(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_ECHO:                       ok = _ECHO(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_MULTICAST_LIKE:             ok = RR->vl2->_MULTICAST_LIKE(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_NETWORK_CREDENTIALS:        ok = RR->vl2->_NETWORK_CREDENTIALS(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_NETWORK_CONFIG_REQUEST:     ok = RR->vl2->_NETWORK_CONFIG_REQUEST(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_NETWORK_CONFIG:             ok = RR->vl2->_NETWORK_CONFIG(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_MULTICAST_GATHER:           ok = RR->vl2->_MULTICAST_GATHER(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_MULTICAST_FRAME_deprecated: ok = RR->vl2->_MULTICAST_FRAME_deprecated(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_PUSH_DIRECT_PATHS:          ok = _PUSH_DIRECT_PATHS(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_USER_MESSAGE:               ok = _USER_MESSAGE(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_MULTICAST:                  ok = RR->vl2->_MULTICAST(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			case Protocol::VERB_ENCAP:                      ok = _ENCAP(tPtr,path,peer,*pkt.b,(int)packetSize); break;
			default:
				RR->t->incomingPacketDropped(tPtr,0xdeadeff0,ph->packetId,0,identityFromPeerPtr(peer),path->address(),hops,verb,ZT_TRACE_PACKET_DROP_REASON_UNRECOGNIZED_VERB);
				break;
		}
		if (ok)
			peer->received(tPtr,path,hops,ph->packetId,packetSize - ZT_PROTO_PACKET_PAYLOAD_START,verb,inReVerb);
	} catch ( ... ) {
		RR->t->unexpectedError(tPtr,0xea1b6dea,"unexpected exception in onRemotePacket() parsing packet from %s",Trace::str(path->address()).s);
	}
}

void VL1::_relay(void *tPtr,const SharedPtr<Path> &path,const Address &destination,SharedPtr<Buf> &data,unsigned int len)
{
	const uint8_t newHopCount = (data->b[ZT_PROTO_PACKET_FLAGS_INDEX] & 7U) + 1;
	if (newHopCount >= ZT_RELAY_MAX_HOPS)
		return;
	data->b[ZT_PROTO_PACKET_FLAGS_INDEX] = (data->b[ZT_PROTO_PACKET_FLAGS_INDEX] & 0xf8U) | newHopCount;

	const SharedPtr<Peer> toPeer(RR->topology->peer(tPtr,destination,false));
	if (!toPeer)
		return;
	const uint64_t now = RR->node->now();
	const SharedPtr<Path> toPath(toPeer->path(now));
	if (!toPath)
		return;

	toPath->send(RR,tPtr,data->b,len,now);
}

void VL1::_sendPendingWhois(void *const tPtr,const int64_t now)
{
	SharedPtr<Peer> root(RR->topology->root());
	if (!root)
		return;
	SharedPtr<Path> rootPath(root->path(now));
	if (!rootPath)
		return;

	std::vector<Address> toSend;
	{
		Mutex::Lock wl(_whoisQueue_l);
		Hashtable<Address,_WhoisQueueItem>::Iterator wi(_whoisQueue);
		Address *a = nullptr;
		_WhoisQueueItem *wq = nullptr;
		while (wi.next(a,wq)) {
			if ((now - wq->lastRetry) >= ZT_WHOIS_RETRY_DELAY) {
				wq->lastRetry = now;
				++wq->retries;
				toSend.push_back(*a);
			}
		}
	}

	Buf outp;
	Protocol::Header &ph = outp.as<Protocol::Header>();

	std::vector<Address>::iterator a(toSend.begin());
	while (a != toSend.end()) {
		ph.packetId = Protocol::getPacketId();
		root->address().copyTo(ph.destination);
		RR->identity.address().copyTo(ph.source);
		ph.flags = 0;
		ph.verb = Protocol::VERB_OK;

		int outl = sizeof(Protocol::Header);
		while ((a != toSend.end())&&(outl < ZT_PROTO_MAX_PACKET_LENGTH)) {
			a->copyTo(outp.b + outl);
			++a;
			outl += ZT_ADDRESS_LENGTH;
		}

		if (outl > sizeof(Protocol::Header)) {
			Protocol::armor(outp,outl,root->key(),peer->cipher());
			RR->expect->sending(ph.packetId,now);
			rootPath->send(RR,tPtr,outp.b,outl,now);
		}
	}
}

bool VL1::_HELLO(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize,const bool authenticated)
{
	if (packetSize < sizeof(Protocol::HELLO)) {
		RR->t->incomingPacketDropped(tPtr,0x2bdb0001,0,0,identityFromPeerPtr(peer),path->address(),0,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return false;
	}
	Protocol::HELLO &p = pkt.as<Protocol::HELLO>();
	const uint8_t hops = Protocol::packetHops(p.h);
	p.h.flags &= (uint8_t)~ZT_PROTO_FLAG_FIELD_HOPS_MASK; // mask off hops for MAC calculation
	int ptr = sizeof(Protocol::HELLO);

	if (p.versionProtocol < ZT_PROTO_VERSION_MIN) {
		RR->t->incomingPacketDropped(tPtr,0xe8d12bad,p.h.packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_PEER_TOO_OLD);
		return false;
	}

	Identity id;
	if (pkt.rO(ptr,id) < 0) {
		RR->t->incomingPacketDropped(tPtr,0x707a9810,p.h.packetId,0,identityFromPeerPtr(peer),path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
		return false;
	}
	if (Address(p.h.source) != id.address()) {
		RR->t->incomingPacketDropped(tPtr,0x06aa9ff1,p.h.packetId,0,Identity::NIL,path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
		return false;
	}

	// Packet is basically valid and identity unmarshaled successfully --------------------------------------------------

	uint8_t key[ZT_PEER_SECRET_KEY_LENGTH];
	if ((peer) && (id == peer->identity())) {
		memcpy(key,peer->key(),ZT_PEER_SECRET_KEY_LENGTH);
	} else {
		peer.zero();
		if (!RR->identity.agree(id,key)) {
			RR->t->incomingPacketDropped(tPtr,0x46db8010,p.h.packetId,0,id,path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
			return false;
		}
	}

	if ((!peer)||(!authenticated)) {
		uint8_t perPacketKey[ZT_PEER_SECRET_KEY_LENGTH];
		uint8_t macKey[ZT_POLY1305_KEY_LEN];
		Protocol::salsa2012DeriveKey(peer->key(),perPacketKey,pkt,packetSize);
		Salsa20(perPacketKey,&p.h.packetId).crypt12(Utils::ZERO256,macKey,ZT_POLY1305_KEY_LEN);
		uint64_t mac[2];
		poly1305(mac,pkt.b + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,packetSize - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,macKey);
		if (p.h.mac != mac[0]) {
			RR->t->incomingPacketDropped(tPtr,0x11bfff81,p.h.packetId,0,id,path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
			return false;
		}
	}

	// Packet has passed Poly1305 MAC authentication --------------------------------------------------------------------

	uint8_t hmacKey[ZT_PEER_SECRET_KEY_LENGTH],hmac[ZT_HMACSHA384_LEN];
	if (peer->remoteVersionProtocol() >= 11) {
		if (packetSize <= ZT_HMACSHA384_LEN) { // sanity check, should be impossible
			RR->t->incomingPacketDropped(tPtr,0x1000662a,p.h.packetId,0,id,path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
			return false;
		}
		packetSize -= ZT_HMACSHA384_LEN;
		KBKDFHMACSHA384(key,ZT_PROTO_KDF_KEY_LABEL_HELLO_HMAC,0,0,hmacKey); // iter == 0 for HELLO, 1 for OK(HELLO)
		HMACSHA384(hmacKey,pkt.b,packetSize,hmac);
		if (!Utils::secureEq(pkt.b + packetSize,hmac,ZT_HMACSHA384_LEN)) {
			RR->t->incomingPacketDropped(tPtr,0x1000662a,p.h.packetId,0,id,path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
			return false;
		}
	}

	// Packet has passed HMAC-SHA384 (if present) -----------------------------------------------------------------------

	InetAddress externalSurfaceAddress;
	Dictionary nodeMetaData;

	// Get external surface address if present.
	if (ptr < packetSize) {
		if (pkt.rO(ptr,externalSurfaceAddress) < 0) {
			RR->t->incomingPacketDropped(tPtr,0x10001003,p.h.packetId,0,id,path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
			return false;
		}
	}

	if ((ptr < packetSize)&&(peer->remoteVersionProtocol() >= 11)) {
		// Everything after this point is encrypted with Salsa20/12. This is only a privacy measure
		// since there's nothing truly secret in a HELLO packet. It also means that an observer
		// can't even get ephemeral public keys without first knowing the long term secret key,
		// adding a little defense in depth.
		uint8_t iv[8];
		for (int i = 0; i < 8; ++i) iv[i] = pkt.b[i];
		iv[7] &= 0xf8U; // this exists for pure legacy reasons, meh...
		Salsa20 s20(key,iv);
		s20.crypt12(pkt.b + ptr,pkt.b + ptr,packetSize - ptr);

		ptr += pkt.rI16(ptr); // skip length field which currently is always zero in v2.0+

		if (ptr < packetSize) {
			const unsigned int dictionarySize = pkt.rI16(ptr);
			const void *const dictionaryBytes = pkt.rBnc(ptr,dictionarySize);
			if (Buf::readOverflow(ptr,packetSize)) {
				RR->t->incomingPacketDropped(tPtr,0x0d0f0112,p.h.packetId,0,id,path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
				return false;
			}
			if (dictionarySize) {
				if (!nodeMetaData.decode(dictionaryBytes,dictionarySize)) {
					RR->t->incomingPacketDropped(tPtr,0x67192344,p.h.packetId,0,id,path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
					return false;
				}
			}

			ptr += pkt.rI16(ptr); // skip any additional fields, currently always 0
		}
	}

	if (Buf::readOverflow(ptr,packetSize)) { // sanity check, should be impossible
		RR->t->incomingPacketDropped(tPtr,0x50003470,0,p.h.packetId,id,path->address(),0,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return false;
	}

	// Packet is fully decoded and has passed all tests -----------------------------------------------------------------

	const int64_t now = RR->node->now();

	if (!peer) {
		if (!id.locallyValidate()) {
			RR->t->incomingPacketDropped(tPtr,0x2ff7a909,p.h.packetId,0,id,path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
			return false;
		}
		peer.set(new Peer(RR));
		if (!peer)
			return false;
		peer->init(id);
		peer = RR->topology->add(tPtr,peer);
	}

	// All validation steps complete, peer learned if not yet known -----------------------------------------------------

	if ((hops == 0) && (externalSurfaceAddress))
		RR->sa->iam(tPtr,id,path->localSocket(),path->address(),externalSurfaceAddress,RR->topology->isRoot(id),now);

	peer->setRemoteVersion(p.versionProtocol,p.versionMajor,p.versionMinor,Utils::ntoh(p.versionRev));

	// Compose and send OK(HELLO) ---------------------------------------------------------------------------------------

	std::vector<uint8_t> myNodeMetaDataBin;
	{
		Dictionary myNodeMetaData;
		myNodeMetaData.encode(myNodeMetaDataBin);
	}
	if (myNodeMetaDataBin.size() > ZT_PROTO_MAX_PACKET_LENGTH) {
		RR->t->unexpectedError(tPtr,0xbc8861e0,"node meta-data dictionary exceeds maximum packet length while composing OK(HELLO) to %s",Trace::str(id.address(),path).s);
		return false;
	}

	Buf outp;
	Protocol::OK::HELLO &ok = outp.as<Protocol::OK::HELLO>();

	ok.h.h.packetId = Protocol::getPacketId();
	id.address().copyTo(ok.h.h.destination);
	RR->identity.address().copyTo(ok.h.h.source);
	ok.h.h.flags = 0;
	ok.h.h.verb = Protocol::VERB_OK;

	ok.h.inReVerb = Protocol::VERB_HELLO;
	ok.h.inRePacketId = p.h.packetId;

	ok.timestampEcho = p.timestamp;
	ok.versionProtocol = ZT_PROTO_VERSION;
	ok.versionMajor = ZEROTIER_ONE_VERSION_MAJOR;
	ok.versionMinor = ZEROTIER_ONE_VERSION_MINOR;
	ok.versionRev = ZT_CONST_TO_BE_UINT16(ZEROTIER_ONE_VERSION_REVISION);

	int outl = sizeof(Protocol::OK::HELLO);
	outp.wO(outl,path->address());

	outp.wI(outl,(uint16_t)0); // legacy field, always 0

	if (p.versionProtocol >= 11) {
		outp.wI(outl,(uint16_t)myNodeMetaDataBin.size());
		outp.wB(outl,myNodeMetaDataBin.data(),(unsigned int)myNodeMetaDataBin.size());
		outp.wI(outl,(uint16_t)0); // length of additional fields, currently 0

		if ((outl + ZT_HMACSHA384_LEN) > ZT_PROTO_MAX_PACKET_LENGTH) // sanity check, shouldn't be possible
			return false;

		KBKDFHMACSHA384(key,ZT_PROTO_KDF_KEY_LABEL_HELLO_HMAC,0,1,hmacKey); // iter == 1 for OK
		HMACSHA384(hmacKey,outp.b + sizeof(ok.h),outl - sizeof(ok.h),outp.b + outl);
		outl += ZT_HMACSHA384_LEN;
	}

	Protocol::armor(outp,outl,peer->key(),peer->cipher());
	path->send(RR,tPtr,outp.b,outl,now);

	return true;
}

bool VL1::_ERROR(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize,Protocol::Verb &inReVerb)
{
	if (packetSize < sizeof(Protocol::ERROR::Header)) {
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
}

bool VL1::_OK(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize,Protocol::Verb &inReVerb)
{
	if (packetSize < sizeof(Protocol::OK::Header)) {
		RR->t->incomingPacketDropped(tPtr,0x4c1f1ff7,0,0,identityFromPeerPtr(peer),path->address(),0,Protocol::VERB_OK,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return false;
	}
	Protocol::OK::Header &oh = pkt.as<Protocol::OK::Header>();
	inReVerb = (Protocol::Verb)oh.inReVerb;

	const int64_t now = RR->node->now();
	if (!RR->expect->expecting(oh.inRePacketId,now)) {
		RR->t->incomingPacketDropped(tPtr,0x4c1f1ff7,0,0,identityFromPeerPtr(peer),path->address(),0,Protocol::VERB_OK,ZT_TRACE_PACKET_DROP_REASON_REPLY_NOT_EXPECTED);
		return false;
	}

	switch(oh.inReVerb) {

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

bool VL1::_WHOIS(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
	if (packetSize < sizeof(Protocol::OK::Header)) {
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
			const SharedPtr<Peer> &wp(RR->topology->peer(tPtr,Address(pkt.b + ptr)));
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

		if (outl > sizeof(Protocol::OK::WHOIS)) {
			Protocol::armor(outp,outl,peer->key(),peer->cipher());
			path->send(RR,tPtr,outp.b,outl,RR->node->now());
		}
	}

	return true;
}

bool VL1::_RENDEZVOUS(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
	if (RR->topology->isRoot(peer->identity())) {
		if (packetSize < sizeof(Protocol::RENDEZVOUS)) {
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
						if ((sizeof(Protocol::RENDEZVOUS) + rdv.addressLength) <= packetSize) {
							const InetAddress atAddr(pkt.b + sizeof(Protocol::RENDEZVOUS),rdv.addressLength,port);
							peer->contact(tPtr,Endpoint(atAddr),now,false,false);
							RR->t->tryingNewPath(tPtr,0x55a19aaa,with->identity(),atAddr,path->address(),Protocol::packetId(pkt,packetSize),Protocol::VERB_RENDEZVOUS,peer->address(),peer->identity().hash(),ZT_TRACE_TRYING_NEW_PATH_REASON_RENDEZVOUS);
						}
						break;
					case 255:
						if ((sizeof(Protocol::RENDEZVOUS) + 1) <= packetSize) {
							Endpoint ep;
							int epl = ep.unmarshal(pkt.b + sizeof(Protocol::RENDEZVOUS),packetSize - (int)sizeof(Protocol::RENDEZVOUS));
							if ((epl > 0) && (ep)) {
								switch (ep.type()) {
									case Endpoint::INETADDR_V4:
									case Endpoint::INETADDR_V6:
										peer->contact(tPtr,ep,now,false,false);
										RR->t->tryingNewPath(tPtr,0x55a19aab,with->identity(),ep.inetAddr(),path->address(),Protocol::packetId(pkt,packetSize),Protocol::VERB_RENDEZVOUS,peer->address(),peer->identity().hash(),ZT_TRACE_TRYING_NEW_PATH_REASON_RENDEZVOUS);
										break;
									default:
										break;
								}
							}
						}
						break;
				}
			}
		}
	}
	return true;
}

bool VL1::_ECHO(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
	const uint64_t packetId = Protocol::packetId(pkt,packetSize);
	const uint64_t now = RR->node->now();
	if (packetSize < sizeof(Protocol::Header)) {
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
		outp.wB(outl,pkt.b + sizeof(Protocol::Header),packetSize - sizeof(Protocol::Header));

		if (Buf::writeOverflow(outl)) {
			RR->t->incomingPacketDropped(tPtr,0x14d70bb0,packetId,0,peer->identity(),path->address(),Protocol::packetHops(pkt,packetSize),Protocol::VERB_ECHO,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return false;
		}

		Protocol::armor(outp,outl,peer->key(),peer->cipher());
		path->send(RR,tPtr,outp.b,outl,now);
	} else {
		RR->t->incomingPacketDropped(tPtr,0x27878bc1,packetId,0,peer->identity(),path->address(),Protocol::packetHops(pkt,packetSize),Protocol::VERB_ECHO,ZT_TRACE_PACKET_DROP_REASON_RATE_LIMIT_EXCEEDED);
	}

	return true;
}

bool VL1::_PUSH_DIRECT_PATHS(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
	if (packetSize < sizeof(Protocol::PUSH_DIRECT_PATHS)) {
		RR->t->incomingPacketDropped(tPtr,0x1bb1bbb1,Protocol::packetId(pkt,packetSize),0,peer->identity(),path->address(),Protocol::packetHops(pkt,packetSize),Protocol::VERB_PUSH_DIRECT_PATHS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return false;
	}
	Protocol::PUSH_DIRECT_PATHS &pdp = pkt.as<Protocol::PUSH_DIRECT_PATHS>();

	const uint64_t now = RR->node->now();
	if (!peer->rateGateInboundPushDirectPaths(now)) {
		RR->t->incomingPacketDropped(tPtr,0x35b1aaaa,pdp.h.packetId,0,peer->identity(),path->address(),Protocol::packetHops(pdp.h),Protocol::VERB_PUSH_DIRECT_PATHS,ZT_TRACE_PACKET_DROP_REASON_RATE_LIMIT_EXCEEDED);
		return true;
	}

	int ptr = sizeof(Protocol::PUSH_DIRECT_PATHS);
	const unsigned int numPaths = Utils::ntoh(pdp.numPaths);
	InetAddress a;
	Endpoint ep;
	for(unsigned int pi=0;pi<numPaths;++pi) {
		/*const uint8_t flags = pkt.rI8(ptr);*/ ++ptr; // flags are not presently used
		ptr += pkt.rI16(ptr); // extended attributes size, currently always 0
		const unsigned int addrType = pkt.rI8(ptr);
		const unsigned int addrRecordLen = pkt.rI8(ptr);
		if (addrRecordLen == 0) {
			RR->t->incomingPacketDropped(tPtr,0xaed00118,pdp.h.packetId,0,peer->identity(),path->address(),Protocol::packetHops(pdp.h),Protocol::VERB_PUSH_DIRECT_PATHS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
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
			default: break;
		}

		if (Buf::readOverflow(ptr,packetSize)) {
			RR->t->incomingPacketDropped(tPtr,0xbad0f10f,pdp.h.packetId,0,peer->identity(),path->address(),Protocol::packetHops(pdp.h),Protocol::VERB_PUSH_DIRECT_PATHS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
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
				case Endpoint::INETADDR_V4:
				case Endpoint::INETADDR_V6:
					a = ep.inetAddr();
					break;
				default: // other types are not supported yet
					break;
			}
		}

		if (a) {
		}

		ptr += (int)addrRecordLen;
	}

	return true;
}

bool VL1::_USER_MESSAGE(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
	// TODO
}

bool VL1::_ENCAP(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize)
{
	// TODO: not implemented yet
	return true;
}

} // namespace ZeroTier
