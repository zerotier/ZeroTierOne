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
#include "Topology.hpp"
#include "VL2.hpp"
#include "Salsa20.hpp"
#include "LZ4.hpp"
#include "Poly1305.hpp"

namespace ZeroTier {

namespace {

#if 0
ZT_ALWAYS_INLINE bool _doHELLO(SharedPtr<Buf> &pkt,int len,const RuntimeEnvironment *const RR,void *const tPtr,const bool alreadyAuthenticated)
{
	if (p.size < sizeof(Protocol::HELLO)) {
		RR->t->incomingPacketDropped(tPtr,p.idBE,0,Identity(),p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return true;
	}
	Buf< Protocol::HELLO > &pkt = reinterpret_cast<Buf< Protocol::HELLO > &>(*p.pkt);

	Identity id;
	int ptr = sizeof(Protocol::HELLO);
	if (pkt.rO(ptr,id) < 0) {
		RR->t->incomingPacketDropped(tPtr,p.idBE,0,Identity(),p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
		return true;
	}

	if (pkt.data.fields.versionProtocol < ZT_PROTO_VERSION_MIN) {
		RR->t->incomingPacketDropped(tPtr,p.idBE,0,id,p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_PEER_TOO_OLD);
		return true;
	}
	if (Address(pkt.data.fields.h.source) != id.address()) {
		RR->t->incomingPacketDropped(tPtr,p.idBE,0,id,p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return true;
	}

	const int64_t now = RR->node->now();

	SharedPtr<Peer> peer(RR->topology->get(tPtr,id.address()));
	if (peer) {
		// We already have an identity with this address -- check for collisions
		if (!alreadyAuthenticated) {
			if (peer->identity() != id) {
				// Identity is different from the one we already have -- address collision

				// Check rate limits
				if (!RR->node->rateGateIdentityVerification(now,p.path->address()))
					return true;

				uint8_t key[ZT_PEER_SECRET_KEY_LENGTH];
				if (RR->identity.agree(id,key)) {
					if (Protocol::dearmor(pkt,p.size,key) < 0) { // ensure packet is authentic, otherwise drop
						RR->t->incomingPacketDropped(tPtr,p.idBE,0,id,p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
						return true;
					} else {
						// TODO: we handle identity collisions differently now
					}
				} else {
					RR->t->incomingPacketDropped(tPtr,p.idBE,0,id,p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
					return true;
				}

				return true;
			} else {
				// Identity is the same as the one we already have -- check packet integrity

				if (Protocol::dearmor(pkt,p.size,peer->key()) < 0) {
					RR->t->incomingPacketDropped(tPtr,p.idBE,0,id,p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
					return true;
				}

				// Continue at // VALID
			}
		} // else if alreadyAuthenticated then continue at // VALID
	} else {
		// We don't already have an identity with this address -- validate and learn it

		// Sanity check: this basically can't happen
		if (alreadyAuthenticated) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,Identity(),p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_UNSPECIFIED);
			return true;
		}

		// Check rate limits
		if (!RR->node->rateGateIdentityVerification(now,p.path->address())) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,id,p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_RATE_LIMIT_EXCEEDED);
			return true;
		}

		// Check packet integrity and MAC (this is faster than locallyValidate() so do it first to filter out total crap)
		SharedPtr<Peer> newPeer(new Peer(RR));
		if (!newPeer->init(RR->identity,id)) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,id,p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
			return true;
		}
		if (Protocol::dearmor(pkt,p.size,newPeer->key())) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,id,p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
			return true;
		}

		// Check that identity's address is valid as per the derivation function
		if (!id.locallyValidate()) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,id,p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
			return true;
		}

		peer = RR->topology->add(tPtr,newPeer);

		// Continue at // VALID
	}

	// VALID -- if we made it here, packet passed identity and authenticity checks!

	// Get address to which this packet was sent to learn our external surface address if packet was direct.
	InetAddress externalSurfaceAddress;
	if (ptr < p.size) {
		if (pkt.rO(ptr,externalSurfaceAddress) < 0) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,id,p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
			return true;
		}
		if ((p.hops == 0)&&(externalSurfaceAddress))
			RR->sa->iam(tPtr,id,p.path->localSocket(),p.path->address(),externalSurfaceAddress,RR->topology->isRoot(id),now);
	}

	// Send OK(HELLO) with an echo of the packet's timestamp and some of the same
	// information about us: version, sent-to address, etc.

	ZT_GET_NEW_BUF(outp,Protocol::OK::HELLO);

	outp->data.fields.h.packetId = Protocol::getPacketId();
	peer->address().copyTo(outp->data.fields.h.destination);
	RR->identity.address().copyTo(outp->data.fields.h.source);
	outp->data.fields.h.flags = 0;
	outp->data.fields.h.verb = Protocol::VERB_OK;

	outp->data.fields.oh.inReVerb = Protocol::VERB_HELLO;
	outp->data.fields.oh.inRePacketId = p.idBE;

	outp->data.fields.timestampEcho = pkt.data.fields.timestamp;
	outp->data.fields.versionProtocol = ZT_PROTO_VERSION;
	outp->data.fields.versionMajor = ZEROTIER_ONE_VERSION_MAJOR;
	outp->data.fields.versionMinor = ZEROTIER_ONE_VERSION_MINOR;
	outp->data.fields.versionRev = CONST_TO_BE_UINT16(ZEROTIER_ONE_VERSION_REVISION);

	int outl = sizeof(Protocol::OK::HELLO);
	outp->wO(outl,p.path->address());
	if (!Buf<>::writeOverflow(outl)) {
		Protocol::armor(*outp,outl,peer->key(),ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012);
		p.path->send(RR,tPtr,outp->data.bytes,outl,RR->node->now());
	}

	peer->setRemoteVersion(pkt.data.fields.versionProtocol,pkt.data.fields.versionMajor,pkt.data.fields.versionMinor,Utils::ntoh(pkt.data.fields.versionRev));
	peer->received(tPtr,p.path,p.hops,p.idBE,p.size,Protocol::VERB_HELLO,0,Protocol::VERB_NOP,0);

	return true;
}
#endif

} // anonymous namespace

VL1::VL1(const RuntimeEnvironment *renv) :
	RR(renv),
	_vl2(nullptr)
{
}

VL1::~VL1()
{
}

void VL1::onRemotePacket(void *const tPtr,const int64_t localSocket,const InetAddress &fromAddr,SharedPtr<Buf> &data,const unsigned int len)
{
	static const uint64_t ZEROES32[4] = { 0,0,0,0 };
	const int64_t now = RR->node->now();
	const SharedPtr<Path> path(RR->topology->getPath(localSocket,fromAddr));
	path->received(now);

	if (len < ZT_PROTO_MIN_FRAGMENT_LENGTH)
		return;

	try {
		FCV<Buf::Slice,ZT_MAX_PACKET_FRAGMENTS> pktv;
		Address destination;

		if (data->b[ZT_PROTO_PACKET_FRAGMENT_INDICATOR_INDEX] == ZT_PROTO_PACKET_FRAGMENT_INDICATOR) {
			// Fragment -----------------------------------------------------------------------------------------------------

			const Protocol::FragmentHeader &fh = data->as<Protocol::FragmentHeader>();
			destination.setTo(fh.destination);

			if (destination != RR->identity.address()) {
				// Fragment is not address to this node -----------------------------------------------------------------------
				_relay(tPtr,path,destination,data,len);
				return;
			}

			switch (_inputPacketAssembler.assemble(
				fh.packetId,
				pktv,
				data,
				ZT_PROTO_PACKET_FRAGMENT_PAYLOAD_START_AT,
				(unsigned int)(len - ZT_PROTO_PACKET_FRAGMENT_PAYLOAD_START_AT),
				fh.counts & 0xfU, // fragment number
				fh.counts >> 4U,  // total number of fragments in message is specified in each fragment
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
			const Protocol::Header &ph = data->as<Protocol::Header>();
			destination.setTo(ph.destination);

			if (destination != RR->identity.address()) {
				// Packet or packet head is not address to this node ----------------------------------------------------------
				_relay(tPtr,path,destination,data,len);
				return;
			}

			if ((ph.flags & ZT_PROTO_FLAG_FRAGMENTED) != 0) {
				// Head of fragmented packet ----------------------------------------------------------------------------------
				switch (_inputPacketAssembler.assemble(
					ph.packetId,
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
			} else {
				// Unfragmented packet, skip defrag engine and just handle it -------------------------------------------------
				Buf::Slice &s = pktv.push();
				s.b = data;
				s.s = 0;
				s.e = len;
			}
		}

		// Packet defragmented and apparently addressed to this node ------------------------------------------------------

		// Subject pktv to a few sanity checks just to make sure Defragmenter worked correctly and
		// there is enough room in each slice to shift their contents to sizes that are multiples
		// of 64 if needed for crypto.
		if ((pktv.empty()) || (((int)pktv[0].e - (int)pktv[0].s) < sizeof(Protocol::Header)))
			return;
		for(FCV<Buf::Slice,ZT_MAX_PACKET_FRAGMENTS>::const_iterator s(pktv.begin());s!=pktv.end();++s) {
			if ((s->e > (ZT_BUF_MEM_SIZE - 64))||(s->s > s->e))
				return;
		}

		Protocol::Header *ph = &(pktv[0].b->as<Protocol::Header>(pktv[0].s));
		const Address source(ph->source);

		if (source == RR->identity.address())
			return;
		SharedPtr<Peer> peer(RR->topology->get(tPtr,source));

		Buf::Slice pkt;
		bool authenticated = false;

		const uint8_t hops = Protocol::packetHops(*ph);
		const uint8_t cipher = Protocol::packetCipher(*ph);

		unsigned int packetSize = pktv[0].e - pktv[0].s;
		for(FCV<Buf::Slice,ZT_MAX_PACKET_FRAGMENTS>::const_iterator s(pktv.begin()+1);s!=pktv.end();++s)
			packetSize += s->e - s->s;
		if (packetSize > ZT_PROTO_MAX_PACKET_LENGTH) {
			RR->t->incomingPacketDropped(tPtr,ph->packetId,0,Identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return;
		}

		// If we don't know this peer and this is not a HELLO, issue a WHOIS and enqueue this packet to try again.
		if ((!peer)&&(!(((cipher == ZT_PROTO_CIPHER_SUITE__POLY1305_NONE)||(cipher == ZT_PROTO_CIPHER_SUITE__NONE))&&((ph->verb & 0x1fU) == Protocol::VERB_HELLO)))) {
			pkt = Buf::assembleSliceVector(pktv);
			if (pkt.e < ZT_PROTO_MIN_PACKET_LENGTH) {
				RR->t->incomingPacketDropped(tPtr,ph->packetId,0,Identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
				return;
			}
			{
				Mutex::Lock wl(_whoisQueue_l);
				_WhoisQueueItem &wq = _whoisQueue[source];
				wq.inboundPackets.push_back(pkt);
				if (wq.retries == 0) {
					wq.retries = 1;
					_sendPendingWhois();
				}
			}
			return;
		}

		switch(cipher) {
			case ZT_PROTO_CIPHER_SUITE__POLY1305_NONE:
				if (peer) {
					pkt = Buf::assembleSliceVector(pktv);
					if (pkt.e < ZT_PROTO_MIN_PACKET_LENGTH)
						RR->t->incomingPacketDropped(tPtr,ph->packetId,0,Identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
					ph = &(pkt.b->as<Protocol::Header>());

					// Generate one-time-use MAC key using Salsa20.
					uint8_t perPacketKey[ZT_PEER_SECRET_KEY_LENGTH];
					uint8_t macKey[ZT_POLY1305_KEY_LEN];
					Protocol::salsa2012DeriveKey(peer->key(),perPacketKey,*pktv[0].b,packetSize);
					Salsa20(perPacketKey,&ph->packetId).crypt12(ZEROES32,macKey,ZT_POLY1305_KEY_LEN);

					// Verify packet MAC.
					uint64_t mac[2];
					poly1305(mac,pkt.b->b,pkt.e,macKey);
					if (ph->mac != mac[0]) {
						RR->t->incomingPacketDropped(tPtr,ph->packetId,0,peer->identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
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
					s20.crypt12(ZEROES32,macKey,ZT_POLY1305_KEY_LEN);

					// Get a buffer to store the decrypted and fully contiguous packet.
					pkt.b = Buf::get();
					if (!pkt.b) // only possible on out of memory condition
						return;

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
					poly1305(mac,pkt.b->b,pkt.e,macKey);
					if (ph->mac != mac[0]) {
						RR->t->incomingPacketDropped(tPtr,ph->packetId,0,peer->identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
						return;
					}
					authenticated = true;
				} else {
					RR->t->incomingPacketDropped(tPtr,ph->packetId,0,Identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
					return;
				}
				break;

			case ZT_PROTO_CIPHER_SUITE__NONE: {
				// CIPHER_SUITE__NONE is only used with trusted paths. Verification is performed by
				// checking the address and the presence of its corresponding trusted path ID in the
				// packet header's MAC field.

				pkt = Buf::assembleSliceVector(pktv);
				if (pkt.e < ZT_PROTO_MIN_PACKET_LENGTH)
					RR->t->incomingPacketDropped(tPtr,ph->packetId,0,Identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
				ph = &(pkt.b->as<Protocol::Header>());

				if (RR->topology->shouldInboundPathBeTrusted(path->address(),Utils::ntoh(ph->mac))) {
					authenticated = true;
				} else {
					if (peer)
						RR->t->incomingPacketDropped(tPtr,ph->packetId,0,peer->identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
					return;
				}
			} break;

			//case ZT_PROTO_CIPHER_SUITE__AES_GCM_NRH:
			//	if (peer) {
			//	}
			//	break;

			default:
				if (peer)
					RR->t->incomingPacketDropped(tPtr,ph->packetId,0,peer->identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
				return;
		}

		// Packet fully assembled and may be authenticated ----------------------------------------------------------------

		// Return any still held buffers in pktv to the buffer pool.
		pktv.clear();

		// Decompress packet payload if compressed.
		if (((ph->verb & ZT_PROTO_VERB_FLAG_COMPRESSED) != 0)&&(authenticated)) {
			SharedPtr<Buf> nb(Buf::get());
			if (!nb) // can only happen if we're out of memory
				return;

			const int uncompressedLen = LZ4_decompress_safe(
				reinterpret_cast<const char *>(pkt.b->b + ZT_PROTO_PACKET_PAYLOAD_START),
				reinterpret_cast<char *>(nb->b),
				(int)(packetSize - ZT_PROTO_PACKET_PAYLOAD_START),
				ZT_BUF_MEM_SIZE - ZT_PROTO_PACKET_PAYLOAD_START);

			if ((uncompressedLen > 0)&&(uncompressedLen <= (ZT_BUF_MEM_SIZE - ZT_PROTO_PACKET_PAYLOAD_START))) {
				pkt.b.swap(nb);
				pkt.e = packetSize = (unsigned int)uncompressedLen;
			} else {
				if (peer)
					RR->t->incomingPacketDropped(tPtr,ph->packetId,0,peer->identity(),path->address(),hops,(Protocol::Verb)(ph->verb & 0x1fU),ZT_TRACE_PACKET_DROP_REASON_INVALID_COMPRESSED_DATA);
				return;
			}
		}

		const Protocol::Verb verb = (Protocol::Verb)(ph->verb & 0x1fU);
		switch(verb) {
			case Protocol::VERB_NOP:
				peer->received(tPtr,path,hops,ph->packetId,packetSize - ZT_PROTO_PACKET_PAYLOAD_START,Protocol::VERB_NOP,0,Protocol::VERB_NOP,0);
				break;

			case Protocol::VERB_HELLO:                      _HELLO(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_ERROR:                      _ERROR(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_OK:                         _OK(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_WHOIS:                      _WHOIS(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_RENDEZVOUS:                 _RENDEZVOUS(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_FRAME:                      _vl2->_FRAME(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_EXT_FRAME:                  _vl2->_EXT_FRAME(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_ECHO:                       _ECHO(tPtr,path,peer,*pkt.b,packetSize,authenticated);
			case Protocol::VERB_MULTICAST_LIKE:             _vl2->_MULTICAST_LIKE(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_NETWORK_CREDENTIALS:        _vl2->_NETWORK_CREDENTIALS(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_NETWORK_CONFIG_REQUEST:     _vl2->_NETWORK_CONFIG_REQUEST(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_NETWORK_CONFIG:             _vl2->_NETWORK_CONFIG(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_MULTICAST_GATHER:           _vl2->_MULTICAST_LIKE(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_MULTICAST_FRAME_deprecated: _vl2->_MULTICAST_FRAME_deprecated(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_PUSH_DIRECT_PATHS:          _PUSH_DIRECT_PATHS(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_USER_MESSAGE:               _USER_MESSAGE(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_MULTICAST:                  _vl2->_MULTICAST(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;
			case Protocol::VERB_ENCAP:                      _ENCAP(tPtr,path,peer,*pkt.b,packetSize,authenticated); break;

			default:
				if (peer)
					RR->t->incomingPacketDropped(tPtr,ph->packetId,0,peer->identity(),path->address(),hops,verb,ZT_TRACE_PACKET_DROP_REASON_UNRECOGNIZED_VERB);
				break;
		}
	} catch ( ... ) {
		uint64_t packetId = 0;
		if (len >= 8) {
			for(int i=0;i<8;++i)
				reinterpret_cast<uint8_t *>(&packetId)[i] = data->b[i];
		}
		RR->t->incomingPacketDropped(tPtr,packetId,0,Identity(),path->address(),0,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
	}
}

void VL1::_relay(void *tPtr,const SharedPtr<Path> &path,const Address &destination,SharedPtr<Buf> &data,unsigned int len)
{
}

void VL1::_sendPendingWhois()
{
	// assume _whoisQueue_l locked
}

void VL1::_HELLO(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,unsigned int len,bool authenticated)
{
}

void VL1::_ERROR(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,unsigned int len,bool authenticated)
{
}

void VL1::_OK(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,unsigned int len,bool authenticated)
{
}

void VL1::_WHOIS(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,unsigned int len,bool authenticated)
{
}

void VL1::_PUSH_DIRECT_PATHS(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,unsigned int len,bool authenticated)
{
}

void VL1::_USER_MESSAGE(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,unsigned int len,bool authenticated)
{
}

void VL1::_ENCAP(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,unsigned int len,bool authenticated)
{
}

} // namespace ZeroTier
