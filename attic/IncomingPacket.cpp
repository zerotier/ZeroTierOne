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

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "IncomingPacket.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Peer.hpp"
#include "NetworkController.hpp"
#include "SelfAwareness.hpp"
#include "Salsa20.hpp"
#include "Node.hpp"
#include "CertificateOfMembership.hpp"
#include "Capability.hpp"
#include "Tag.hpp"
#include "Revocation.hpp"
#include "Trace.hpp"
#include "Buf.hpp"

#include <cstring>
#include <cstdlib>

// Macro to avoid calling hton() on values known at compile time.
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define CONST_TO_BE_UINT16(x) ((uint16_t)((uint16_t)((uint16_t)(x) << 8U) | (uint16_t)((uint16_t)(x) >> 8U)))
#else
#define CONST_TO_BE_UINT16(x) ((uint16_t)(x))
#endif

namespace ZeroTier {

namespace {

volatile uint16_t junk = 0;

void _sendErrorNeedCredentials(IncomingPacket &p,const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer,const uint64_t nwid)
{
	ZT_GET_NEW_BUF(outp,Protocol::ERROR::NEED_MEMBERSHIP_CERTIFICATE);

	outp->data.fields.h.packetId = Protocol::getPacketId();
	peer->address().copyTo(outp->data.fields.h.destination);
	RR->identity.address().copyTo(outp->data.fields.h.source);
	outp->data.fields.h.flags = 0;
	outp->data.fields.h.verb = Protocol::VERB_ERROR;

	outp->data.fields.eh.inRePacketId = p.idBE;
	outp->data.fields.eh.inReVerb = p.pkt->data.fields.verb;
	outp->data.fields.eh.error = Protocol::ERROR_NEED_MEMBERSHIP_CERTIFICATE;
	outp->data.fields.networkId = nwid;

	Protocol::armor(*outp,sizeof(Protocol::ERROR::NEED_MEMBERSHIP_CERTIFICATE),peer->key(),ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012);
	p.path->send(RR,tPtr,outp->data.bytes,sizeof(Protocol::ERROR::NEED_MEMBERSHIP_CERTIFICATE),RR->node->now());
}

ZT_ALWAYS_INLINE bool _doHELLO(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const bool alreadyAuthenticated)
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

ZT_ALWAYS_INLINE bool _doERROR(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	if (p.size < sizeof(Protocol::ERROR::Header)) {
		RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_ERROR,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return true;
	}
	Buf< Protocol::ERROR::Header > &pkt = reinterpret_cast<Buf< Protocol::ERROR::Header > &>(*p.pkt);

	uint64_t networkId = 0;
	int ptr = sizeof(Protocol::ERROR::Header);

	/* Security note: we do not gate doERROR() with expectingReplyTo() to
	 * avoid having to log every outgoing packet ID. Instead we put the
	 * logic to determine whether we should consider an ERROR in each
	 * error handler. In most cases these are only trusted in specific
	 * circumstances. */

	switch(pkt.data.fields.error) {

		case Protocol::ERROR_OBJ_NOT_FOUND:
			// Object not found, currently only meaningful from network controllers.
			if (pkt.data.fields.inReVerb == Protocol::VERB_NETWORK_CONFIG_REQUEST) {
				networkId = pkt.rI64(ptr);
				const SharedPtr<Network> network(RR->node->network(networkId));
				if ((network)&&(network->controller() == peer->address()))
					network->setNotFound();
			}
			break;

		case Protocol::ERROR_UNSUPPORTED_OPERATION:
			// This can be sent in response to any operation, though right now we only
			// consider it meaningful from network controllers. This would indicate
			// that the queried node does not support acting as a controller.
			if (pkt.data.fields.inReVerb == Protocol::VERB_NETWORK_CONFIG_REQUEST) {
				networkId = pkt.rI64(ptr);
				const SharedPtr<Network> network(RR->node->network(networkId));
				if ((network)&&(network->controller() == peer->address()))
					network->setNotFound();
			}
			break;

		case Protocol::ERROR_NEED_MEMBERSHIP_CERTIFICATE: {
			// Peers can send this to ask for a cert for a network.
			networkId = pkt.rI64(ptr);
			const SharedPtr<Network> network(RR->node->network(networkId));
			const int64_t now = RR->node->now();
			if ((network)&&(network->config().com))
				network->pushCredentialsNow(tPtr,peer->address(),now);
		}	break;

		case Protocol::ERROR_NETWORK_ACCESS_DENIED_: {
			// Network controller: network access denied.
			networkId = pkt.rI64(ptr);
			const SharedPtr<Network> network(RR->node->network(networkId));
			if ((network)&&(network->controller() == peer->address()))
				network->setAccessDenied();
		}	break;

		default: break;
	}

	peer->received(tPtr,p.path,p.hops,p.idBE,p.size,Protocol::VERB_ERROR,pkt.data.fields.inRePacketId,(Protocol::Verb)pkt.data.fields.inReVerb,networkId);

	return true;
}

ZT_ALWAYS_INLINE bool _doOK(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	if (p.size < sizeof(Protocol::OK::Header)) {
		RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_OK,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return true;
	}
	Buf< Protocol::OK::Header > &pkt = reinterpret_cast<Buf< Protocol::OK::Header > &>(*p.pkt);

	uint64_t networkId = 0;
	int ptr = sizeof(Protocol::OK::Header);

	if (!RR->node->expectingReplyTo(p.idBE))
		return true;

	switch(pkt.data.fields.inReVerb) {

		case Protocol::VERB_HELLO: {
			if (p.size < sizeof(Protocol::OK::HELLO)) {
				RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
				return true;
			}
			Buf< Protocol::OK::HELLO > &pkt2 = reinterpret_cast<Buf< Protocol::OK::HELLO > &>(pkt);

			if (pkt2.data.fields.versionProtocol < ZT_PROTO_VERSION_MIN)
				return true;
			peer->updateLatency((unsigned int)(p.receiveTime - Utils::ntoh(pkt2.data.fields.timestampEcho)));
			peer->setRemoteVersion(pkt2.data.fields.versionProtocol,pkt2.data.fields.versionMajor,pkt2.data.fields.versionMinor,Utils::ntoh(pkt2.data.fields.versionRev));

			ptr = sizeof(Protocol::OK::HELLO);
			if (ptr < p.size) {
				InetAddress externalSurfaceAddress;
				if (pkt2.rO(ptr,externalSurfaceAddress) < 0)
					return true;
				if ((externalSurfaceAddress)&&(p.hops == 0))
					RR->sa->iam(tPtr,peer->identity(),p.path->localSocket(),p.path->address(),externalSurfaceAddress,RR->topology->isRoot(peer->identity()),RR->node->now());
			}
		}	break;

		case Protocol::VERB_WHOIS:
			if (RR->topology->isRoot(peer->identity())) {
				while (ptr < p.size) {
					Identity id;
					if (pkt.rO(ptr,id) < 0)
						break;
					Locator loc;
					if (ptr < p.size) { // older nodes did not send the locator
						if (pkt.rO(ptr,loc) < 0)
							break;
					}
					if (id) {
						SharedPtr<Peer> ptmp(RR->topology->add(tPtr,SharedPtr<Peer>(new Peer(RR))));
						ptmp->init(RR->identity,id);
						RR->sw->doAnythingWaitingForPeer(tPtr,ptmp);
					}
				}
			}
			break;

		case Protocol::VERB_NETWORK_CONFIG_REQUEST: {
			networkId = pkt.rI64(ptr);
			const SharedPtr<Network> network(RR->node->network(networkId));
			if (network)
				network->handleConfigChunk(tPtr,p.idBE,peer,pkt,sizeof(Protocol::OK::Header),(int)p.size);
		}	break;

		case Protocol::VERB_MULTICAST_GATHER: {
			// TODO
		}	break;

		default: break;
	}

	peer->received(tPtr,p.path,p.hops,p.idBE,p.size,Protocol::VERB_OK,pkt.data.fields.inRePacketId,(Protocol::Verb)pkt.data.fields.inReVerb,networkId);

	return true;
}

ZT_ALWAYS_INLINE bool _doWHOIS(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	if (!peer->rateGateInboundWhoisRequest(RR->node->now())) {
		RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_WHOIS,ZT_TRACE_PACKET_DROP_REASON_RATE_LIMIT_EXCEEDED);
		return true;
	}

	ZT_GET_NEW_BUF(outp,Protocol::OK::WHOIS);

	outp->data.fields.h.packetId = Protocol::getPacketId();
	peer->address().copyTo(outp->data.fields.h.destination);
	RR->identity.address().copyTo(outp->data.fields.h.source);
	outp->data.fields.h.flags = 0;
	outp->data.fields.h.verb = Protocol::VERB_OK;

	outp->data.fields.oh.inReVerb = Protocol::VERB_WHOIS;
	outp->data.fields.oh.inRePacketId = p.idBE;

	int ptr = sizeof(Protocol::Header);
	int outl = sizeof(Protocol::OK::WHOIS);
	while ((ptr + ZT_ADDRESS_LENGTH) <= p.size) {
		const SharedPtr<Peer> ptmp(RR->topology->get(tPtr,Address(p.pkt->data.bytes + ptr)));
		if (ptmp) {
			outp->wO(outl,ptmp->identity());
			Locator loc(ptmp->locator());
			outp->wO(outl,loc);
		}
		ptr += ZT_ADDRESS_LENGTH;
	}

	if ((outl > sizeof(Protocol::OK::WHOIS))&&(!Buf<>::writeOverflow(outl))) {
		Protocol::armor(*outp,outl,peer->key(),ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012);
		p.path->send(RR,tPtr,outp->data.bytes,outl,RR->node->now());
	}

	peer->received(tPtr,p.path,p.hops,p.idBE,p.size,Protocol::VERB_WHOIS,0,Protocol::VERB_NOP,0);

	return true;
}

ZT_ALWAYS_INLINE bool _doRENDEZVOUS(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	if (RR->topology->isRoot(peer->identity())) {
		if (p.size < sizeof(Protocol::RENDEZVOUS)) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_RENDEZVOUS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return true;
		}
		Buf< Protocol::RENDEZVOUS > &pkt = reinterpret_cast<Buf< Protocol::RENDEZVOUS > &>(*p.pkt);

		const SharedPtr<Peer> with(RR->topology->get(tPtr,Address(pkt.data.fields.peerAddress)));
		if (with) {
			const unsigned int port = Utils::ntoh(pkt.data.fields.port);
			if (port != 0) {
				switch(pkt.data.fields.addressLength) {
					case 4:
						if ((sizeof(Protocol::RENDEZVOUS) + 4) <= p.size) {
							InetAddress atAddr(pkt.data.fields.address,4,port);
							++junk;
							RR->node->putPacket(tPtr,p.path->localSocket(),atAddr,(const void *)&junk,2,2); // IPv4 "firewall opener" hack
							with->sendHELLO(tPtr,p.path->localSocket(),atAddr,RR->node->now());
							RR->t->tryingNewPath(tPtr,with->identity(),atAddr,p.path->address(),p.idBE,Protocol::VERB_RENDEZVOUS,peer->address(),peer->identity().hash(),ZT_TRACE_TRYING_NEW_PATH_REASON_RENDEZVOUS);
						}
						break;
					case 16:
						if ((sizeof(Protocol::RENDEZVOUS) + 16) <= p.size) {
							InetAddress atAddr(pkt.data.fields.address,16,port);
							with->sendHELLO(tPtr,p.path->localSocket(),atAddr,RR->node->now());
							RR->t->tryingNewPath(tPtr,with->identity(),atAddr,p.path->address(),p.idBE,Protocol::VERB_RENDEZVOUS,peer->address(),peer->identity().hash(),ZT_TRACE_TRYING_NEW_PATH_REASON_RENDEZVOUS);
						}
						break;
				}
			}
		}
	}
	peer->received(tPtr,p.path,p.hops,p.idBE,p.size,Protocol::VERB_RENDEZVOUS,0,Protocol::VERB_NOP,0);
	return true;
}

ZT_ALWAYS_INLINE bool _doFRAME(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	if (p.size < sizeof(Protocol::FRAME)) {
		RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_EXT_FRAME,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return true;
	}
	Buf< Protocol::FRAME > &pkt = reinterpret_cast<Buf< Protocol::FRAME > &>(*p.pkt);

	const SharedPtr<Network> network(RR->node->network(Utils::ntoh(pkt.data.fields.networkId)));
	if (network) {
		if (network->gate(tPtr,peer)) {
			const unsigned int etherType = Utils::ntoh(pkt.data.fields.etherType);
			const MAC sourceMac(peer->address(),network->id());
			const unsigned int frameLen = (unsigned int)(p.size - sizeof(Protocol::FRAME));
			if (network->filterIncomingPacket(tPtr,peer,RR->identity.address(),sourceMac,network->mac(),pkt.data.fields.data,frameLen,etherType,0) > 0)
				RR->node->putFrame(tPtr,network->id(),network->userPtr(),sourceMac,network->mac(),etherType,0,pkt.data.fields.data,frameLen);
		} else {
			RR->t->incomingNetworkFrameDropped(tPtr,network->id(),MAC(),MAC(),peer->identity(),p.path->address(),p.hops,0,nullptr,Protocol::VERB_FRAME,true,ZT_TRACE_FRAME_DROP_REASON_PERMISSION_DENIED);
			_sendErrorNeedCredentials(p,RR,tPtr,peer,network->id());
			return false; // try to decode again after we get credentials?
		}
	}
	peer->received(tPtr,p.path,p.hops,p.idBE,p.size,Protocol::VERB_FRAME,0,Protocol::VERB_NOP,0);
	return true;
}

ZT_ALWAYS_INLINE bool _doEXT_FRAME(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	if (p.size < sizeof(Protocol::EXT_FRAME)) {
		RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_EXT_FRAME,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return true;
	}
	Buf< Protocol::EXT_FRAME > &pkt = reinterpret_cast<Buf< Protocol::EXT_FRAME > &>(*p.pkt);

	const SharedPtr<Network> network(RR->node->network(Utils::ntoh(pkt.data.fields.networkId)));
	if (network) {
		int ptr = sizeof(Protocol::EXT_FRAME);
		const uint8_t flags = pkt.data.fields.flags;

		if ((flags & Protocol::EXT_FRAME_FLAG_COM_ATTACHED_deprecated) != 0) {
			CertificateOfMembership com;
			if (pkt.rO(ptr,com) < 0) {
				RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_EXT_FRAME,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
				return true;
			}
			if (com)
				network->addCredential(tPtr,peer->identity(),com);
		}

		if (!network->gate(tPtr,peer)) {
			RR->t->incomingNetworkFrameDropped(tPtr,network->id(),MAC(),MAC(),peer->identity(),p.path->address(),p.hops,0,nullptr,Protocol::VERB_EXT_FRAME,true,ZT_TRACE_FRAME_DROP_REASON_PERMISSION_DENIED);
			_sendErrorNeedCredentials(p,RR,tPtr,peer,network->id());
			return false; // try to parse again if we get credentials
		}

		const MAC to(pkt.rBnc(ptr,6));
		const MAC from(pkt.rBnc(ptr,6));
		const unsigned int etherType = pkt.rI16(ptr);

		if ((from)&&(from != network->mac())&&(!Buf<>::readOverflow(ptr,p.size))) {
			const int frameSize = (int)(p.size - ptr);
			if (frameSize >= 0) {
				const uint64_t nwid = network->id();
				const uint8_t *const frameData = pkt.data.bytes + ptr;
				switch (network->filterIncomingPacket(tPtr,peer,RR->identity.address(),from,to,frameData,frameSize,etherType,0)) {

					case 1:
						if (from != MAC(peer->address(),nwid)) {
							if (network->config().permitsBridging(peer->address())) {
								network->learnBridgeRoute(from,peer->address());
							} else {
								RR->t->incomingNetworkFrameDropped(tPtr,nwid,from,to,peer->identity(),p.path->address(),p.hops,(uint16_t)frameSize,frameData,Protocol::VERB_EXT_FRAME,true,ZT_TRACE_FRAME_DROP_REASON_BRIDGING_NOT_ALLOWED_REMOTE);
								goto packet_dropped;
							}
						} else if (to != network->mac()) {
							if (to.isMulticast()) {
								if (network->config().multicastLimit == 0) {
									RR->t->incomingNetworkFrameDropped(tPtr,nwid,from,to,peer->identity(),p.path->address(),p.hops,(uint16_t)frameSize,frameData,Protocol::VERB_EXT_FRAME,true,ZT_TRACE_FRAME_DROP_REASON_MULTICAST_DISABLED);
									goto packet_dropped;
								}
							} else if (!network->config().permitsBridging(RR->identity.address())) {
								RR->t->incomingNetworkFrameDropped(tPtr,nwid,from,to,peer->identity(),p.path->address(),p.hops,(uint16_t)frameSize,frameData,Protocol::VERB_EXT_FRAME,true,ZT_TRACE_FRAME_DROP_REASON_BRIDGING_NOT_ALLOWED_LOCAL);
								goto packet_dropped;
							}
						}
						// fall through -- 2 means accept regardless of bridging checks or other restrictions

					case 2:
						RR->node->putFrame(tPtr,nwid,network->userPtr(),from,to,etherType,0,frameData,frameSize);
						break;

				}
			}
		}

		if ((flags & Protocol::EXT_FRAME_FLAG_ACK_REQUESTED) != 0) {
			ZT_GET_NEW_BUF(outp,Protocol::OK::EXT_FRAME);

			outp->data.fields.h.packetId = Protocol::getPacketId();
			peer->address().copyTo(outp->data.fields.h.destination);
			RR->identity.address().copyTo(outp->data.fields.h.source);
			outp->data.fields.h.flags = 0;
			outp->data.fields.h.verb = Protocol::VERB_OK;

			outp->data.fields.oh.inReVerb = Protocol::VERB_EXT_FRAME;
			outp->data.fields.oh.inRePacketId = p.idBE;

			outp->data.fields.networkId = pkt.data.fields.networkId;
			outp->data.fields.flags = 0;
			to.copyTo(outp->data.fields.destMac);
			from.copyTo(outp->data.fields.sourceMac);
			outp->data.fields.etherType = Utils::hton((uint16_t)etherType);

			Protocol::armor(*outp,sizeof(Protocol::OK::EXT_FRAME),peer->key(),ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012);
			p.path->send(RR,tPtr,outp->data.bytes,sizeof(Protocol::OK::EXT_FRAME),RR->node->now());
		}
	}

packet_dropped:
	peer->received(tPtr,p.path,p.hops,p.idBE,p.size,Protocol::VERB_EXT_FRAME,0,Protocol::VERB_NOP,0);
	return true;
}

ZT_ALWAYS_INLINE bool _doECHO(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	if (!peer->rateGateEchoRequest(RR->node->now())) {
		RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_ECHO,ZT_TRACE_PACKET_DROP_REASON_RATE_LIMIT_EXCEEDED);
		return true;
	}

	ZT_GET_NEW_BUF(outp,Protocol::OK::ECHO);

	outp->data.fields.h.packetId = Protocol::getPacketId();
	peer->address().copyTo(outp->data.fields.h.destination);
	RR->identity.address().copyTo(outp->data.fields.h.source);
	outp->data.fields.h.flags = 0;
	outp->data.fields.h.verb = Protocol::VERB_OK;

	outp->data.fields.oh.inReVerb = Protocol::VERB_ECHO;
	outp->data.fields.oh.inRePacketId = p.idBE;

	int outl = sizeof(Protocol::OK::ECHO);
	if (p.size > sizeof(Protocol::Header)) {
		outp->wB(outl,p.pkt->data.bytes + sizeof(Protocol::Header),p.size - sizeof(Protocol::Header));
	}

	if (!Buf<>::writeOverflow(outl)) {
		Protocol::armor(*outp,outl,peer->key(),ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012);
		p.path->send(RR,tPtr,outp->data.bytes,outl,RR->node->now());
	}

	peer->received(tPtr,p.path,p.hops,p.idBE,p.size,Protocol::VERB_ECHO,0,Protocol::VERB_NOP,0);

	return true;
}

ZT_ALWAYS_INLINE bool _doNETWORK_CREDENTIALS(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	int ptr = sizeof(Protocol::Header);
	const uint8_t *payload = p.pkt->data.bytes;
	SharedPtr<Network> network;

	// Early versions of ZeroTier sent only the certificate of membership. The COM always
	// starts with a non-zero byte. To extend this message we then parse COMs until we find
	// a zero byte, then parse the other types (which are prefaced by a count for better
	// extensibility) if they are present.

	// Also note that technically these can be for different networks but in practice they
	// are always for the same network (when talking with current nodes). This code therefore
	// accepts different networks for each credential and ignores any credentials for
	// networks that we've not currently joined.

	while ((ptr < p.size)&&(payload[ptr] != 0)) {
		CertificateOfMembership com;
		int l = com.unmarshal(payload + ptr,(int)(p.size - ptr));
		if (l < 0) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_NETWORK_CREDENTIALS,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
			return true;
		}
		ptr += l;

		const uint64_t nwid = com.networkId();
		if ((!network)||(network->id() != nwid))
			network = RR->node->network(nwid);
		if (network) {
			if (network->addCredential(tPtr,peer->identity(),com) == Membership::ADD_DEFERRED_FOR_WHOIS)
				return false;
		}
	}
	++ptr; // skip trailing 0 after COMs if present

	// The following code is copypasta for each credential type: capability, tag, revocation,
	// and certificate of ownership. Each type is prefaced by a count, but it's legal for the
	// packet to terminate prematurely if all remaining counts are zero.

	if (ptr >= p.size)
		return true;

	unsigned int count = p.pkt->rI16(ptr);
	for(unsigned int i=0;i<count;++i) {
		if (ptr >= p.size) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_NETWORK_CREDENTIALS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return true;
		}
		Capability cap;
		int l = cap.unmarshal(payload + ptr,(int)(p.size - ptr));
		if (l < 0) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_NETWORK_CREDENTIALS,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
			return true;
		}
		ptr += l;

		const uint64_t nwid = cap.networkId();
		if ((!network)||(network->id() != nwid))
			network = RR->node->network(nwid);
		if (network) {
			if (network->addCredential(tPtr,peer->identity(),cap) == Membership::ADD_DEFERRED_FOR_WHOIS)
				return false;
		}
	}

	if (ptr >= p.size)
		return true;

	count = p.pkt->rI16(ptr);
	for(unsigned int i=0;i<count;++i) {
		if (ptr >= p.size) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_NETWORK_CREDENTIALS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return true;
		}
		Tag tag;
		int l = tag.unmarshal(payload + ptr,(int)(p.size - ptr));
		if (l < 0) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_NETWORK_CREDENTIALS,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
			return true;
		}
		ptr += l;

		const uint64_t nwid = tag.networkId();
		if ((!network)||(network->id() != nwid))
			network = RR->node->network(nwid);
		if (network) {
			if (network->addCredential(tPtr,peer->identity(),tag) == Membership::ADD_DEFERRED_FOR_WHOIS)
				return false;
		}
	}

	if (ptr >= p.size)
		return true;

	count = p.pkt->rI16(ptr);
	for(unsigned int i=0;i<count;++i) {
		if (ptr >= p.size) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_NETWORK_CREDENTIALS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return true;
		}
		Revocation rev;
		int l = rev.unmarshal(payload + ptr,(int)(p.size - ptr));
		if (l < 0) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_NETWORK_CREDENTIALS,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
			return true;
		}
		ptr += l;

		const uint64_t nwid = rev.networkId();
		if ((!network)||(network->id() != nwid))
			network = RR->node->network(nwid);
		if (network) {
			if (network->addCredential(tPtr,peer->identity(),rev) == Membership::ADD_DEFERRED_FOR_WHOIS)
				return false;
		}
	}

	if (ptr >= p.size)
		return true;

	count = p.pkt->rI16(ptr);
	for(unsigned int i=0;i<count;++i) {
		if (ptr >= p.size) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_NETWORK_CREDENTIALS,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
			return true;
		}
		CertificateOfOwnership coo;
		int l = coo.unmarshal(payload + ptr,(int)(p.size - ptr));
		if (l < 0) {
			RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_NETWORK_CREDENTIALS,ZT_TRACE_PACKET_DROP_REASON_INVALID_OBJECT);
			return true;
		}
		ptr += l;

		const uint64_t nwid = coo.networkId();
		if ((!network)||(network->id() != nwid))
			network = RR->node->network(nwid);
		if (network) {
			if (network->addCredential(tPtr,peer->identity(),coo) == Membership::ADD_DEFERRED_FOR_WHOIS)
				return false;
		}
	}

	peer->received(tPtr,p.path,p.hops,p.idBE,p.size,Protocol::VERB_NETWORK_CREDENTIALS,0,Protocol::VERB_NOP,(network) ? network->id() : 0);

	return true;
}

ZT_ALWAYS_INLINE bool _doNETWORK_CONFIG_REQUEST(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	int ptr = sizeof(Protocol::Header);

	const uint64_t nwid = p.pkt->rI64(ptr);
	const unsigned int dictSize = p.pkt->rI16(ptr);
	const uint8_t *dictData = p.pkt->rBnc(ptr,dictSize);
	if (Buf<>::readOverflow(ptr,p.size)) {
		RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_NETWORK_CONFIG_REQUEST,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return true;
	}

	if (RR->localNetworkController) {
		Dictionary requestMetaData;
		if ((dictSize > 0)&&(dictData)) {
			if (!requestMetaData.decode(dictData,dictSize)) {
				RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_NETWORK_CONFIG_REQUEST,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
				return true;
			}
		}
		RR->localNetworkController->request(nwid,(p.hops > 0) ? InetAddress::NIL : p.path->address(),Utils::ntoh(p.idBE),peer->identity(),requestMetaData);
	} else {
		ZT_GET_NEW_BUF(outp,Protocol::ERROR::UNSUPPORTED_OPERATION__NETWORK_CONFIG_REQUEST);

		outp->data.fields.h.packetId = Protocol::getPacketId();
		peer->address().copyTo(outp->data.fields.h.destination);
		RR->identity.address().copyTo(outp->data.fields.h.source);
		outp->data.fields.h.flags = 0;
		outp->data.fields.h.verb = Protocol::VERB_OK;

		outp->data.fields.eh.inReVerb = Protocol::VERB_NETWORK_CONFIG_REQUEST;
		outp->data.fields.eh.inRePacketId = p.idBE;
		outp->data.fields.eh.error = Protocol::ERROR_UNSUPPORTED_OPERATION;

		outp->data.fields.networkId = Utils::hton(nwid);

		Protocol::armor(*outp,sizeof(Protocol::ERROR::UNSUPPORTED_OPERATION__NETWORK_CONFIG_REQUEST),peer->key(),ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012);
		p.path->send(RR,tPtr,outp->data.bytes,sizeof(Protocol::ERROR::UNSUPPORTED_OPERATION__NETWORK_CONFIG_REQUEST),RR->node->now());
	}

	// Note that NETWORK_CONFIG_REQUEST does not pertain to a network we have *joined*, but one
	// we may control. The network ID parameter to peer->received() is therefore zero.
	peer->received(tPtr,p.path,p.hops,p.idBE,p.size,Protocol::VERB_NETWORK_CONFIG_REQUEST,0,Protocol::VERB_NOP,0);
	return true;
}

ZT_ALWAYS_INLINE bool _doNETWORK_CONFIG(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	int ptr = sizeof(Protocol::Header);
	const uint64_t nwid = p.pkt->rI64(ptr);
	if (ptr >= (int)p.size) {
		RR->t->incomingPacketDropped(tPtr,p.idBE,0,peer->identity(),p.path->address(),p.hops,Protocol::VERB_NETWORK_CONFIG,ZT_TRACE_PACKET_DROP_REASON_MALFORMED_PACKET);
		return true;
	}

	const SharedPtr<Network> network(RR->node->network(nwid));
	if (network) {
		const uint64_t configUpdateId = network->handleConfigChunk(tPtr,p.idBE,peer,*p.pkt,ptr,(int)p.size - ptr);
		if (configUpdateId != 0) {
			ZT_GET_NEW_BUF(outp,Protocol::OK::NETWORK_CONFIG);

			outp->data.fields.h.packetId = Protocol::getPacketId();
			peer->address().copyTo(outp->data.fields.h.destination);
			RR->identity.address().copyTo(outp->data.fields.h.source);
			outp->data.fields.h.flags = 0;
			outp->data.fields.h.verb = Protocol::VERB_OK;

			outp->data.fields.oh.inReVerb = Protocol::VERB_NETWORK_CONFIG;
			outp->data.fields.oh.inRePacketId = p.idBE;

			outp->data.fields.networkId = Utils::hton(nwid);
			outp->data.fields.configUpdateId = Utils::hton(configUpdateId);

			Protocol::armor(*outp,sizeof(Protocol::OK::NETWORK_CONFIG),peer->key(),ZT_PROTO_CIPHER_SUITE__POLY1305_SALSA2012);
			p.path->send(RR,tPtr,outp->data.bytes,sizeof(Protocol::OK::NETWORK_CONFIG),RR->node->now());
		}
	}

	peer->received(tPtr,p.path,p.hops,p.idBE,p.size,Protocol::VERB_NETWORK_CONFIG,0,Protocol::VERB_NOP,nwid);
	return true;
}

ZT_ALWAYS_INLINE bool _doMULTICAST_GATHER(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	return true;
}

ZT_ALWAYS_INLINE bool _doPUSH_DIRECT_PATHS(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	/*
	const int64_t now = RR->node->now();

	if (peer->rateGateInboundPushDirectPaths(now)) {
		uint8_t countPerScope[ZT_INETADDRESS_MAX_SCOPE+1][2]; // [][0] is v4, [][1] is v6
		memset(countPerScope,0,sizeof(countPerScope));

		unsigned int count = pkt.at<uint16_t>(ZT_PACKET_IDX_PAYLOAD);
		unsigned int ptr = ZT_PACKET_IDX_PAYLOAD + 2;

		while (count--) {
			// unsigned int flags = (*this)[ptr++];
			++ptr;
			unsigned int extLen = pkt.at<uint16_t>(ptr); ptr += 2;
			ptr += extLen; // unused right now
			unsigned int addrType = pkt[ptr++];
			unsigned int addrLen = pkt[ptr++];

			switch(addrType) {
				case 4: {
					const InetAddress a(pkt.field(ptr,4),4,pkt.at<uint16_t>(ptr + 4));
					if (peer->shouldTryPath(tPtr,now,peer,a)) {
						if (++countPerScope[(int)a.ipScope()][0] <= ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY) {
							RR->node->putPacket(tPtr,path->localSocket(),a,(const void *)&junk,sizeof(junk),2); // IPv4 "firewall opener"
							++junk;
							peer->sendHELLO(tPtr,-1,a,now);
							RR->t->tryingNewPath(tPtr,peer->identity(),a,path->address(),pkt.packetId(),Packet::VERB_PUSH_DIRECT_PATHS,peer->address(),peer->identity().hash(),ZT_TRACE_TRYING_NEW_PATH_REASON_RECEIVED_PUSH_DIRECT_PATHS);
						}
					}
				}	break;
				case 6: {
					const InetAddress a(pkt.field(ptr,16),16,pkt.at<uint16_t>(ptr + 16));
					if (peer->shouldTryPath(tPtr,now,peer,a)) {
						if (++countPerScope[(int)a.ipScope()][1] <= ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY) {
							peer->sendHELLO(tPtr,-1,a,now);
							RR->t->tryingNewPath(tPtr,peer->identity(),a,path->address(),pkt.packetId(),Packet::VERB_PUSH_DIRECT_PATHS,peer->address(),peer->identity().hash(),ZT_TRACE_TRYING_NEW_PATH_REASON_RECEIVED_PUSH_DIRECT_PATHS);
						}
					}
				}	break;
			}

			ptr += addrLen;
		}
	}

	peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_PUSH_DIRECT_PATHS,0,Packet::VERB_NOP,0);
*/
	return true;
}

ZT_ALWAYS_INLINE bool _doUSER_MESSAGE(IncomingPacket &p,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer)
{
	ZT_UserMessage um;
	int ptr = sizeof(Protocol::Header);
	um.id = reinterpret_cast<const ZT_Identity *>(&(peer->identity()));
	um.typeId = p.pkt->rI64(ptr);
	int ds = (int)p.size - ptr;
	if (ds > 0) {
		um.data = p.pkt->data.bytes + ptr;
		um.length = (unsigned int)ds;
		RR->node->postEvent(tPtr,ZT_EVENT_USER_MESSAGE,reinterpret_cast<const void *>(&um));
	}
	peer->received(tPtr,p.path,p.hops,p.idBE,p.size,Protocol::VERB_USER_MESSAGE,0,Protocol::VERB_NOP,0);
	return true;
}

//////////////////////////////////////////////////////////////////////////////
} // anonymous namespace

bool IncomingPacket::tryDecode(const RuntimeEnvironment *RR,void *tPtr)
{
	const Address source(pkt->data.fields.source);
	const SharedPtr<Peer> peer(RR->topology->get(tPtr,source));

	try {
		// Check for trusted paths or unencrypted HELLOs (HELLO is the only packet sent in the clear)
		const uint8_t c = Protocol::packetCipher(pkt->data.fields);
		bool trusted = false;
		if (c == ZT_PROTO_CIPHER_SUITE__NONE) {
			// If this is marked as a packet via a trusted path, check source address and path ID.
			// Obviously if no trusted paths are configured this always returns false and such
			// packets are dropped on the floor.
			const uint64_t tpid = Utils::ntoh(pkt->data.fields.mac); // the MAC is the trusted path ID on these packets
			if (RR->topology->shouldInboundPathBeTrusted(path->address(),tpid)) {
				trusted = true;
			} else {
				if (peer)
					RR->t->incomingPacketDropped(tPtr,idBE,0,peer->identity(),path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
				return true;
			}
		} else if ((c == ZT_PROTO_CIPHER_SUITE__POLY1305_NONE)&&(pkt->data.fields.verb == Protocol::VERB_HELLO)) {
			// Only HELLO is allowed in the clear, but the MAC is still checked in _doHELLO().
			return _doHELLO(*this,RR,tPtr,false);
		}

		if (!peer) {
			RR->sw->requestWhois(tPtr,RR->node->now(),source);
			return false;
		}

		if (!trusted) {
			if (!dearmor(peer->key())) {
				RR->t->incomingPacketDropped(tPtr,idBE,0,peer->identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_MAC_FAILED);
				return true;
			}
		}

		if (!uncompress()) {
			RR->t->incomingPacketDropped(tPtr,idBE,0,peer->identity(),path->address(),hops,Protocol::VERB_NOP,ZT_TRACE_PACKET_DROP_REASON_INVALID_COMPRESSED_DATA);
			return true;
		}

		const Protocol::Verb verb = (Protocol::Verb)pkt->data.fields.verb;
		bool r = true;
		switch(verb) {
			default: // ignore unknown verbs, but if they pass auth check they are "received" and considered NOPs by peer->receive()
				RR->t->incomingPacketDropped(tPtr,idBE,0,peer->identity(),path->address(),hops,verb,ZT_TRACE_PACKET_DROP_REASON_UNRECOGNIZED_VERB);
				// fall through
			case Protocol::VERB_NOP:
				peer->received(tPtr,path,hops,idBE,size,Protocol::VERB_NOP,0,Protocol::VERB_NOP,0);
				break;
			case Protocol::VERB_HELLO:                      r = _doHELLO(*this,RR,tPtr,true);                  break;
			case Protocol::VERB_ERROR:                      r = _doERROR(*this,RR,tPtr,peer);                  break;
			case Protocol::VERB_OK:                         r = _doOK(*this,RR,tPtr,peer);                     break;
			case Protocol::VERB_WHOIS:                      r = _doWHOIS(*this,RR,tPtr,peer);                  break;
			case Protocol::VERB_RENDEZVOUS:                 r = _doRENDEZVOUS(*this,RR,tPtr,peer);             break;
			case Protocol::VERB_FRAME:                      r = _doFRAME(*this,RR,tPtr,peer);                  break;
			case Protocol::VERB_EXT_FRAME:                  r = _doEXT_FRAME(*this,RR,tPtr,peer);              break;
			case Protocol::VERB_ECHO:                       r = _doECHO(*this,RR,tPtr,peer);                   break;
			case Protocol::VERB_NETWORK_CREDENTIALS:        r = _doNETWORK_CREDENTIALS(*this,RR,tPtr,peer);    break;
			case Protocol::VERB_NETWORK_CONFIG_REQUEST:     r = _doNETWORK_CONFIG_REQUEST(*this,RR,tPtr,peer); break;
			case Protocol::VERB_NETWORK_CONFIG:             r = _doNETWORK_CONFIG(*this,RR,tPtr,peer);         break;
			case Protocol::VERB_MULTICAST_GATHER:           r = _doMULTICAST_GATHER(*this,RR,tPtr,peer);       break;
			case Protocol::VERB_PUSH_DIRECT_PATHS:          r = _doPUSH_DIRECT_PATHS(*this,RR,tPtr,peer);      break;
			case Protocol::VERB_USER_MESSAGE:               r = _doUSER_MESSAGE(*this,RR,tPtr,peer);           break;
		}
		return r;
	} catch (int ztExcCode) {
	} catch ( ... ) {}

	if (peer)
		RR->t->incomingPacketDropped(tPtr,idBE,0,peer->identity(),path->address(),hops,Protocol::VERB_HELLO,ZT_TRACE_PACKET_DROP_REASON_UNSPECIFIED);
	return true;
}

} // namespace ZeroTier
