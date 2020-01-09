/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "../include/ZeroTierOne.h"

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

#include <cstring>
#include <cstdlib>

#include <list>

namespace ZeroTier {

namespace {
//////////////////////////////////////////////////////////////////////////////
// Implementation of each protocol verb                                     //
//////////////////////////////////////////////////////////////////////////////

void _sendErrorNeedCredentials(IncomingPacket &pkt,const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer,const uint64_t nwid,const SharedPtr<Path> &path)
{
	Packet outp(pkt.source(),RR->identity.address(),Packet::VERB_ERROR);
	outp.append((uint8_t)pkt.verb());
	outp.append(pkt.packetId());
	outp.append((uint8_t)Packet::ERROR_NEED_MEMBERSHIP_CERTIFICATE);
	outp.append(nwid);
	outp.armor(peer->key(),true);
	path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
}

ZT_ALWAYS_INLINE bool _doHELLO(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const bool alreadyAuthenticated,const SharedPtr<Path> &path)
{
	const int64_t now = RR->node->now();

	const uint64_t pid = pkt.packetId();
	const Address fromAddress(pkt.source());
	const unsigned int protoVersion = pkt[ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION];
	const unsigned int vMajor = pkt[ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION];
	const unsigned int vMinor = pkt[ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION];
	const unsigned int vRevision = pkt.at<uint16_t>(ZT_PROTO_VERB_HELLO_IDX_REVISION);
	const int64_t timestamp = pkt.at<int64_t>(ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP);
	Identity id;
	unsigned int ptr = ZT_PROTO_VERB_HELLO_IDX_IDENTITY + id.deserialize(pkt,ZT_PROTO_VERB_HELLO_IDX_IDENTITY);

	if (protoVersion < ZT_PROTO_VERSION_MIN) {
		RR->t->incomingPacketDroppedHELLO(tPtr,path,pid,fromAddress,"protocol version too old");
		return true;
	}
	if (fromAddress != id.address()) {
		RR->t->incomingPacketDroppedHELLO(tPtr,path,pid,fromAddress,"identity/address mismatch");
		return true;
	}

	SharedPtr<Peer> peer(RR->topology->get(id.address()));
	if (peer) {
		// We already have an identity with this address -- check for collisions
		if (!alreadyAuthenticated) {
			if (peer->identity() != id) {
				// Identity is different from the one we already have -- address collision

				// Check rate limits
				if (!RR->node->rateGateIdentityVerification(now,path->address()))
					return true;

				uint8_t key[ZT_PEER_SECRET_KEY_LENGTH];
				if (RR->identity.agree(id,key)) {
					if (pkt.dearmor(key)) { // ensure packet is authentic, otherwise drop
						RR->t->incomingPacketDroppedHELLO(tPtr,path,pid,fromAddress,"address collision");
						Packet outp(id.address(),RR->identity.address(),Packet::VERB_ERROR);
						outp.append((uint8_t)Packet::VERB_HELLO);
						outp.append((uint64_t)pid);
						outp.append((uint8_t)Packet::ERROR_IDENTITY_COLLISION);
						outp.armor(key,true);
						path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
					} else {
						RR->t->incomingPacketMessageAuthenticationFailure(tPtr,path,pid,fromAddress,pkt.hops(),"invalid MAC");
					}
				} else {
					RR->t->incomingPacketMessageAuthenticationFailure(tPtr,path,pid,fromAddress,pkt.hops(),"invalid identity");
				}

				return true;
			} else {
				// Identity is the same as the one we already have -- check packet integrity

				if (!pkt.dearmor(peer->key())) {
					RR->t->incomingPacketMessageAuthenticationFailure(tPtr,path,pid,fromAddress,pkt.hops(),"invalid MAC");
					return true;
				}

				// Continue at // VALID
			}
		} // else if alreadyAuthenticated then continue at // VALID
	} else {
		// We don't already have an identity with this address -- validate and learn it

		// Sanity check: this basically can't happen
		if (alreadyAuthenticated) {
			RR->t->incomingPacketDroppedHELLO(tPtr,path,pid,fromAddress,"illegal alreadyAuthenticated state");
			return true;
		}

		// Check rate limits
		if (!RR->node->rateGateIdentityVerification(now,path->address())) {
			RR->t->incomingPacketDroppedHELLO(tPtr,path,pid,fromAddress,"rate limit exceeded");
			return true;
		}

		// Check packet integrity and MAC (this is faster than locallyValidate() so do it first to filter out total crap)
		SharedPtr<Peer> newPeer(new Peer(RR,RR->identity,id));
		if (!pkt.dearmor(newPeer->key())) {
			RR->t->incomingPacketMessageAuthenticationFailure(tPtr,path,pid,fromAddress,pkt.hops(),"invalid MAC");
			return true;
		}

		// Check that identity's address is valid as per the derivation function
		if (!id.locallyValidate()) {
			RR->t->incomingPacketDroppedHELLO(tPtr,path,pid,fromAddress,"invalid identity");
			return true;
		}

		peer = RR->topology->add(newPeer);

		// Continue at // VALID
	}

	// VALID -- if we made it here, packet passed identity and authenticity checks!

	// Get address to which this packet was sent to learn our external surface address if packet was direct.
	if (pkt.hops() == 0) {
		InetAddress externalSurfaceAddress;
		if (ptr < pkt.size()) {
			ptr += externalSurfaceAddress.deserialize(pkt,ptr);
			if ((externalSurfaceAddress)&&(pkt.hops() == 0))
				RR->sa->iam(tPtr,id.address(),path->localSocket(),path->address(),externalSurfaceAddress,RR->topology->isRoot(id),now);
		}
	}

	// Send OK(HELLO) with an echo of the packet's timestamp and some of the same
	// information about us: version, sent-to address, etc.

	Packet outp(id.address(),RR->identity.address(),Packet::VERB_OK);
	outp.append((unsigned char)Packet::VERB_HELLO);
	outp.append((uint64_t)pid);
	outp.append((uint64_t)timestamp);
	outp.append((unsigned char)ZT_PROTO_VERSION);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MAJOR);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MINOR);
	outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
	path->address().serialize(outp);
	outp.armor(peer->key(),true);
	path->send(RR,tPtr,outp.data(),outp.size(),now);

	peer->setRemoteVersion(protoVersion,vMajor,vMinor,vRevision); // important for this to go first so received() knows the version
	peer->received(tPtr,path,pkt.hops(),pid,pkt.payloadLength(),Packet::VERB_HELLO,0,Packet::VERB_NOP,0);

	return true;
}

ZT_ALWAYS_INLINE bool _doACK(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
}

ZT_ALWAYS_INLINE bool _doQOS_MEASUREMENT(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
}

ZT_ALWAYS_INLINE bool _doERROR(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	const Packet::Verb inReVerb = (Packet::Verb)pkt[ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB];
	const uint64_t inRePacketId = pkt.at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_IN_RE_PACKET_ID);
	const Packet::ErrorCode errorCode = (Packet::ErrorCode)pkt[ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE];
	uint64_t networkId = 0;

	/* Security note: we do not gate doERROR() with expectingReplyTo() to
	 * avoid having to log every outgoing packet ID. Instead we put the
	 * logic to determine whether we should consider an ERROR in each
	 * error handler. In most cases these are only trusted in specific
	 * circumstances. */

	switch(errorCode) {

		case Packet::ERROR_OBJ_NOT_FOUND:
			// Object not found, currently only meaningful from network controllers.
			if (inReVerb == Packet::VERB_NETWORK_CONFIG_REQUEST) {
				networkId = pkt.at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD);
				const SharedPtr<Network> network(RR->node->network(networkId));
				if ((network)&&(network->controller() == peer->address()))
					network->setNotFound();
			}
			break;

		case Packet::ERROR_UNSUPPORTED_OPERATION:
			// This can be sent in response to any operation, though right now we only
			// consider it meaningful from network controllers. This would indicate
			// that the queried node does not support acting as a controller.
			if (inReVerb == Packet::VERB_NETWORK_CONFIG_REQUEST) {
				networkId = pkt.at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD);
				const SharedPtr<Network> network(RR->node->network(networkId));
				if ((network)&&(network->controller() == peer->address()))
					network->setNotFound();
			}
			break;

		case Packet::ERROR_NEED_MEMBERSHIP_CERTIFICATE: {
			// Peers can send this to ask for a cert for a network.
			networkId = pkt.at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD);
			const SharedPtr<Network> network(RR->node->network(networkId));
			const int64_t now = RR->node->now();
			if ((network)&&(network->config().com))
				network->pushCredentialsNow(tPtr,peer->address(),now);
		}	break;

		case Packet::ERROR_NETWORK_ACCESS_DENIED_: {
			// Network controller: network access denied.
			networkId = pkt.at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD);
			const SharedPtr<Network> network(RR->node->network(networkId));
			if ((network)&&(network->controller() == peer->address()))
				network->setAccessDenied();
		}	break;

		default: break;
	}

	peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_ERROR,inRePacketId,inReVerb,networkId);

	return true;
}

ZT_ALWAYS_INLINE bool _doOK(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	const Packet::Verb inReVerb = (Packet::Verb)pkt[ZT_PROTO_VERB_OK_IDX_IN_RE_VERB];
	const uint64_t inRePacketId = pkt.at<uint64_t>(ZT_PROTO_VERB_OK_IDX_IN_RE_PACKET_ID);
	uint64_t networkId = 0;

	if (!RR->node->expectingReplyTo(inRePacketId))
		return true;

	switch(inReVerb) {

		case Packet::VERB_HELLO: {
			const uint64_t latency = RR->node->now() - pkt.at<uint64_t>(ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP);
			const unsigned int vProto = pkt[ZT_PROTO_VERB_HELLO__OK__IDX_PROTOCOL_VERSION];
			const unsigned int vMajor = pkt[ZT_PROTO_VERB_HELLO__OK__IDX_MAJOR_VERSION];
			const unsigned int vMinor = pkt[ZT_PROTO_VERB_HELLO__OK__IDX_MINOR_VERSION];
			const unsigned int vRevision = pkt.at<uint16_t>(ZT_PROTO_VERB_HELLO__OK__IDX_REVISION);
			if (vProto < ZT_PROTO_VERSION_MIN)
				return true;

			if (pkt.hops() == 0) {
				path->updateLatency((unsigned int)latency,RR->node->now());
				if ((ZT_PROTO_VERB_HELLO__OK__IDX_REVISION + 2) < pkt.size()) {
					InetAddress externalSurfaceAddress;
					externalSurfaceAddress.deserialize(pkt,ZT_PROTO_VERB_HELLO__OK__IDX_REVISION + 2);
					if (externalSurfaceAddress)
						RR->sa->iam(tPtr,peer->address(),path->localSocket(),path->address(),externalSurfaceAddress,RR->topology->isRoot(peer->identity()),RR->node->now());
				}
			}

			peer->setRemoteVersion(vProto,vMajor,vMinor,vRevision);
		}	break;

		case Packet::VERB_WHOIS:
			if (RR->topology->isRoot(peer->identity())) {
				unsigned int p = ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY;
				while (p < pkt.size()) {
					try {
						Identity id;
						p += id.deserialize(pkt,p);
						if (id)
							RR->sw->doAnythingWaitingForPeer(tPtr,RR->topology->add(SharedPtr<Peer>(new Peer(RR,RR->identity,id))));
					} catch ( ... ) {
						break;
					}
				}
			}
			break;

		case Packet::VERB_NETWORK_CONFIG_REQUEST: {
			networkId = pkt.at<uint64_t>(ZT_PROTO_VERB_OK_IDX_PAYLOAD);
			const SharedPtr<Network> network(RR->node->network(networkId));
			if (network)
				network->handleConfigChunk(tPtr,pkt.packetId(),pkt.source(),pkt,ZT_PROTO_VERB_OK_IDX_PAYLOAD);
		}	break;

		case Packet::VERB_MULTICAST_GATHER: {
		}	break;

		default: break;
	}

	peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_OK,inRePacketId,inReVerb,networkId);

	return true;
}

ZT_ALWAYS_INLINE bool _doWHOIS(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	if (!peer->rateGateInboundWhoisRequest(RR->node->now()))
		return true;

	Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
	outp.append((unsigned char)Packet::VERB_WHOIS);
	outp.append(pkt.packetId());

	unsigned int count = 0;
	unsigned int ptr = ZT_PACKET_IDX_PAYLOAD;
	while ((ptr + ZT_ADDRESS_LENGTH) <= pkt.size()) {
		const Address addr(pkt.field(ptr,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
		ptr += ZT_ADDRESS_LENGTH;

		const Identity id(RR->topology->getIdentity(tPtr,addr));
		if (id) {
			id.serialize(outp,false);
			++count;
		} else {
			// Request unknown WHOIS from upstream from us (if we have one)
			RR->sw->requestWhois(tPtr,RR->node->now(),addr);
		}
	}

	if (count > 0) {
		outp.armor(peer->key(),true);
		path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
	}

	peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_WHOIS,0,Packet::VERB_NOP,0);

	return true;
}

ZT_ALWAYS_INLINE bool _doRENDEZVOUS(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	if (RR->topology->isRoot(peer->identity())) {
		const Address with(pkt.field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ZTADDRESS,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
		const SharedPtr<Peer> rendezvousWith(RR->topology->get(with));
		if (rendezvousWith) {
			const unsigned int port = pkt.at<uint16_t>(ZT_PROTO_VERB_RENDEZVOUS_IDX_PORT);
			const unsigned int addrlen = pkt[ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRLEN];
			if ((port > 0)&&((addrlen == 4)||(addrlen == 16))) {
				InetAddress atAddr(pkt.field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRESS,addrlen),addrlen,port);
				if (RR->node->shouldUsePathForZeroTierTraffic(tPtr,with,path->localSocket(),atAddr)) {
					const uint64_t junk = Utils::random();
					RR->node->putPacket(tPtr,path->localSocket(),atAddr,&junk,4,2); // send low-TTL junk packet to 'open' local NAT(s) and stateful firewalls
					rendezvousWith->sendHELLO(tPtr,path->localSocket(),atAddr,RR->node->now());
				}
			}
		}
	}
	peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_RENDEZVOUS,0,Packet::VERB_NOP,0);
	return true;
}

ZT_ALWAYS_INLINE bool _doFRAME(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	const uint64_t nwid = pkt.at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID);
	const SharedPtr<Network> network(RR->node->network(nwid));
	if (network) {
		if (network->gate(tPtr,peer)) {
			if (pkt.size() > ZT_PROTO_VERB_FRAME_IDX_PAYLOAD) {
				const unsigned int etherType = pkt.at<uint16_t>(ZT_PROTO_VERB_FRAME_IDX_ETHERTYPE);
				const MAC sourceMac(peer->address(),nwid);
				const unsigned int frameLen = pkt.size() - ZT_PROTO_VERB_FRAME_IDX_PAYLOAD;
				const uint8_t *const frameData = reinterpret_cast<const uint8_t *>(pkt.data()) + ZT_PROTO_VERB_FRAME_IDX_PAYLOAD;
				if (network->filterIncomingPacket(tPtr,peer,RR->identity.address(),sourceMac,network->mac(),frameData,frameLen,etherType,0) > 0)
					RR->node->putFrame(tPtr,nwid,network->userPtr(),sourceMac,network->mac(),etherType,0,(const void *)frameData,frameLen);
			}
		} else {
			_sendErrorNeedCredentials(pkt,RR,tPtr,peer,nwid,path);
			return false;
		}
	}
	peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_FRAME,0,Packet::VERB_NOP,nwid);
	return true;
}

ZT_ALWAYS_INLINE bool _doEXT_FRAME(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	const uint64_t nwid = pkt.at<uint64_t>(ZT_PROTO_VERB_EXT_FRAME_IDX_NETWORK_ID);
	const SharedPtr<Network> network(RR->node->network(nwid));
	if (network) {
		const unsigned int flags = pkt[ZT_PROTO_VERB_EXT_FRAME_IDX_FLAGS];

		unsigned int comLen = 0;
		if ((flags & 0x01) != 0) { // inline COM with EXT_FRAME is deprecated but still used with old peers
			CertificateOfMembership com;
			comLen = com.deserialize(pkt,ZT_PROTO_VERB_EXT_FRAME_IDX_COM);
			if (com)
				network->addCredential(tPtr,com);
		}

		if (!network->gate(tPtr,peer)) {
			RR->t->incomingNetworkAccessDenied(tPtr,network,path,pkt.packetId(),pkt.size(),peer->address(),Packet::VERB_EXT_FRAME,true);
			_sendErrorNeedCredentials(pkt,RR,tPtr,peer,nwid,path);
			return false;
		}

		if (pkt.size() > ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD) {
			const unsigned int etherType = pkt.at<uint16_t>(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_ETHERTYPE);
			const MAC to(pkt.field(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_TO,ZT_PROTO_VERB_EXT_FRAME_LEN_TO),ZT_PROTO_VERB_EXT_FRAME_LEN_TO);
			const MAC from(pkt.field(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_FROM,ZT_PROTO_VERB_EXT_FRAME_LEN_FROM),ZT_PROTO_VERB_EXT_FRAME_LEN_FROM);
			const unsigned int frameLen = pkt.size() - (comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD);
			const uint8_t *const frameData = (const uint8_t *)pkt.field(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD,frameLen);

			if ((!from)||(from == network->mac())) {
				peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,nwid);
				return true;
			}

			switch (network->filterIncomingPacket(tPtr,peer,RR->identity.address(),from,to,frameData,frameLen,etherType,0)) {
				case 1:
					if (from != MAC(peer->address(),nwid)) {
						if (network->config().permitsBridging(peer->address())) {
							network->learnBridgeRoute(from,peer->address());
						} else {
							RR->t->incomingNetworkFrameDropped(tPtr,network,path,pkt.packetId(),pkt.size(),peer->address(),Packet::VERB_EXT_FRAME,from,to,"bridging not allowed (remote)");
							peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,nwid);
							return true;
						}
					} else if (to != network->mac()) {
						if (to.isMulticast()) {
							if (network->config().multicastLimit == 0) {
								RR->t->incomingNetworkFrameDropped(tPtr,network,path,pkt.packetId(),pkt.size(),peer->address(),Packet::VERB_EXT_FRAME,from,to,"multicast disabled");
								peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,nwid);
								return true;
							}
						} else if (!network->config().permitsBridging(RR->identity.address())) {
							RR->t->incomingNetworkFrameDropped(tPtr,network,path,pkt.packetId(),pkt.size(),peer->address(),Packet::VERB_EXT_FRAME,from,to,"bridging not allowed (local)");
							peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,nwid);
							return true;
						}
					}
					// fall through -- 2 means accept regardless of bridging checks or other restrictions
				case 2:
					RR->node->putFrame(tPtr,nwid,network->userPtr(),from,to,etherType,0,(const void *)frameData,frameLen);
					break;
			}
		}

		if ((flags & 0x10) != 0) { // ACK requested
			Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
			outp.append((uint8_t)Packet::VERB_EXT_FRAME);
			outp.append((uint64_t)pkt.packetId());
			outp.append((uint64_t)nwid);
			outp.armor(peer->key(),true);
			path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
		}

	}

	peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,nwid);
	return true;
}

ZT_ALWAYS_INLINE bool _doECHO(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	if (!peer->rateGateEchoRequest(RR->node->now()))
		return true;

	const uint64_t pid = pkt.packetId();
	Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
	outp.append((unsigned char)Packet::VERB_ECHO);
	outp.append((uint64_t)pid);
	if (pkt.size() > ZT_PACKET_IDX_PAYLOAD)
		outp.append(reinterpret_cast<const unsigned char *>(pkt.data()) + ZT_PACKET_IDX_PAYLOAD,pkt.size() - ZT_PACKET_IDX_PAYLOAD);
	outp.armor(peer->key(),true);
	path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());

	peer->received(tPtr,path,pkt.hops(),pid,pkt.payloadLength(),Packet::VERB_ECHO,0,Packet::VERB_NOP,0);

	return true;
}

ZT_ALWAYS_INLINE bool _doNETWORK_CREDENTIALS(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	if (!peer->rateGateCredentialsReceived(RR->node->now()))
		return true;

	CertificateOfMembership com;
	Capability cap;
	Tag tag;
	Revocation revocation;
	CertificateOfOwnership coo;
	SharedPtr<Network> network;

	unsigned int p = ZT_PACKET_IDX_PAYLOAD;
	while ((p < pkt.size())&&(pkt[p] != 0)) {
		p += com.deserialize(pkt,p);
		if (com) {
			network = RR->node->network(com.networkId());
			if (network) {
				if (network->addCredential(tPtr,com) == Membership::ADD_DEFERRED_FOR_WHOIS)
					return false;
			}
		}
	}
	++p; // skip trailing 0 after COMs if present

	if (p < pkt.size()) { // older ZeroTier versions do not send capabilities, tags, or revocations
		const unsigned int numCapabilities = pkt.at<uint16_t>(p); p += 2;
		for(unsigned int i=0;i<numCapabilities;++i) {
			p += cap.deserialize(pkt,p);
			if ((!network)||(network->id() != cap.networkId()))
				network = RR->node->network(cap.networkId());
			if (network) {
				if (network->addCredential(tPtr,cap) == Membership::ADD_DEFERRED_FOR_WHOIS)
					return false;
			}
		}

		if (p >= pkt.size()) return true;

		const unsigned int numTags = pkt.at<uint16_t>(p); p += 2;
		for(unsigned int i=0;i<numTags;++i) {
			p += tag.deserialize(pkt,p);
			if ((!network)||(network->id() != tag.networkId()))
				network = RR->node->network(tag.networkId());
			if (network) {
				if (network->addCredential(tPtr,tag) == Membership::ADD_DEFERRED_FOR_WHOIS)
					return false;
			}
		}

		if (p >= pkt.size()) return true;

		const unsigned int numRevocations = pkt.at<uint16_t>(p); p += 2;
		for(unsigned int i=0;i<numRevocations;++i) {
			p += revocation.deserialize(pkt,p);
			if ((!network)||(network->id() != revocation.networkId()))
				network = RR->node->network(revocation.networkId());
			if (network) {
				if (network->addCredential(tPtr,peer->address(),revocation) == Membership::ADD_DEFERRED_FOR_WHOIS)
					return false;
			}
		}

		if (p >= pkt.size()) return true;

		const unsigned int numCoos = pkt.at<uint16_t>(p); p += 2;
		for(unsigned int i=0;i<numCoos;++i) {
			p += coo.deserialize(pkt,p);
			if ((!network)||(network->id() != coo.networkId()))
				network = RR->node->network(coo.networkId());
			if (network) {
				if (network->addCredential(tPtr,coo) == Membership::ADD_DEFERRED_FOR_WHOIS)
					return false;
			}
		}
	}

	peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_NETWORK_CREDENTIALS,0,Packet::VERB_NOP,(network) ? network->id() : 0);

	return true;
}

ZT_ALWAYS_INLINE bool _doNETWORK_CONFIG_REQUEST(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	const uint64_t nwid = pkt.at<uint64_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_NETWORK_ID);
	const unsigned int hopCount = pkt.hops();
	const uint64_t requestPacketId = pkt.packetId();

	if (RR->localNetworkController) {
		const unsigned int metaDataLength = (ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT_LEN <= pkt.size()) ? pkt.at<uint16_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT_LEN) : 0;
		const char *metaDataBytes = (metaDataLength != 0) ? (const char *)pkt.field(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT,metaDataLength) : (const char *)0;
		const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> metaData(metaDataBytes,metaDataLength);
		RR->localNetworkController->request(nwid,(hopCount > 0) ? InetAddress() : path->address(),requestPacketId,peer->identity(),metaData);
	} else {
		Packet outp(peer->address(),RR->identity.address(),Packet::VERB_ERROR);
		outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
		outp.append(requestPacketId);
		outp.append((unsigned char)Packet::ERROR_UNSUPPORTED_OPERATION);
		outp.append(nwid);
		outp.armor(peer->key(),true);
		path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
	}

	peer->received(tPtr,path,hopCount,requestPacketId,pkt.payloadLength(),Packet::VERB_NETWORK_CONFIG_REQUEST,0,Packet::VERB_NOP,nwid);

	return true;
}

ZT_ALWAYS_INLINE bool _doNETWORK_CONFIG(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	const SharedPtr<Network> network(RR->node->network(pkt.at<uint64_t>(ZT_PACKET_IDX_PAYLOAD)));
	if (network) {
		const uint64_t configUpdateId = network->handleConfigChunk(tPtr,pkt.packetId(),pkt.source(),pkt,ZT_PACKET_IDX_PAYLOAD);
		if (configUpdateId) {
			Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
			outp.append((uint8_t)Packet::VERB_ECHO);
			outp.append((uint64_t)pkt.packetId());
			outp.append((uint64_t)network->id());
			outp.append((uint64_t)configUpdateId);
			outp.armor(peer->key(),true);
			path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
		}
	}
	peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_NETWORK_CONFIG,0,Packet::VERB_NOP,(network) ? network->id() : 0);
	return true;
}

ZT_ALWAYS_INLINE bool _doMULTICAST_GATHER(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	const uint64_t nwid = pkt.at<uint64_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_NETWORK_ID);
	const unsigned int flags = pkt[ZT_PROTO_VERB_MULTICAST_GATHER_IDX_FLAGS];
	const MulticastGroup mg(MAC(pkt.field(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_MAC,6),6),pkt.at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_ADI));
	const unsigned int gatherLimit = pkt.at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_GATHER_LIMIT);

	const SharedPtr<Network> network(RR->node->network(nwid));

	if ((flags & 0x01) != 0) {
		try {
			CertificateOfMembership com;
			com.deserialize(pkt,ZT_PROTO_VERB_MULTICAST_GATHER_IDX_COM);
			if ((com)&&(network))
				network->addCredential(tPtr,com);
		} catch ( ... ) {} // discard invalid COMs
	}

	if (network) {
		if (!network->gate(tPtr,peer)) {
			_sendErrorNeedCredentials(pkt,RR,tPtr,peer,nwid,path);
			return false;
		}
	}

	const int64_t now = RR->node->now();
	if (gatherLimit) {
		// TODO
		/*
		Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
		outp.append((unsigned char)Packet::VERB_MULTICAST_GATHER);
		outp.append(packetId());
		outp.append(nwid);
		mg.mac().appendTo(outp);
		outp.append((uint32_t)mg.adi());
		const unsigned int gatheredLocally = RR->mc->gather(peer->address(),nwid,mg,outp,gatherLimit);
		if (gatheredLocally > 0) {
			outp.armor(peer->key(),true);
			_path->send(RR,tPtr,outp.data(),outp.size(),now);
		}
		*/
	}

	peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_MULTICAST_GATHER,0,Packet::VERB_NOP,nwid);

	return true;
}

ZT_ALWAYS_INLINE bool _doPUSH_DIRECT_PATHS(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	const int64_t now = RR->node->now();

	if (peer->rateGatePushDirectPaths(now)) {
		uint8_t countPerScope[ZT_INETADDRESS_MAX_SCOPE+1][2]; // [][0] is v4, [][1] is v6
		memset(countPerScope,0,sizeof(countPerScope));

		unsigned int count = pkt.at<uint16_t>(ZT_PACKET_IDX_PAYLOAD);
		unsigned int ptr = ZT_PACKET_IDX_PAYLOAD + 2;

		while (count--) {
			/* unsigned int flags = (*this)[ptr++]; */ ++ptr;
			unsigned int extLen = pkt.at<uint16_t>(ptr); ptr += 2;
			ptr += extLen; // unused right now
			unsigned int addrType = pkt[ptr++];
			unsigned int addrLen = pkt[ptr++];

			switch(addrType) {

				case 4: {
					const InetAddress a(pkt.field(ptr,4),4,pkt.at<uint16_t>(ptr + 4));
					if ((!peer->hasActivePathTo(now,a)) && // not already known
							(RR->node->shouldUsePathForZeroTierTraffic(tPtr,peer->address(),-1,a)) ) // should use path
					{
						if (++countPerScope[(int)a.ipScope()][0] <= ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY)
							peer->sendHELLO(tPtr,-1,a,now);
					}
				}	break;

				case 6: {
					const InetAddress a(pkt.field(ptr,16),16,pkt.at<uint16_t>(ptr + 16));
					if ((!peer->hasActivePathTo(now,a)) && // not already known
							(RR->node->shouldUsePathForZeroTierTraffic(tPtr,peer->address(),-1,a)) ) // should use path
					{
						if (++countPerScope[(int)a.ipScope()][1] <= ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY)
							peer->sendHELLO(tPtr,-1,a,now);
					}
				}	break;

			}
			ptr += addrLen;
		}
	}

	peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_PUSH_DIRECT_PATHS,0,Packet::VERB_NOP,0);

	return true;
}

ZT_ALWAYS_INLINE bool _doUSER_MESSAGE(IncomingPacket &pkt,const RuntimeEnvironment *const RR,void *const tPtr,const SharedPtr<Peer> &peer,const SharedPtr<Path> &path)
{
	if (likely(pkt.size() >= (ZT_PACKET_IDX_PAYLOAD + 8))) {
		ZT_UserMessage um;
		um.origin = peer->address().toInt();
		um.typeId = pkt.at<uint64_t>(ZT_PACKET_IDX_PAYLOAD);
		um.data = reinterpret_cast<const void *>(reinterpret_cast<const uint8_t *>(pkt.data()) + ZT_PACKET_IDX_PAYLOAD + 8);
		um.length = pkt.size() - (ZT_PACKET_IDX_PAYLOAD + 8);
		RR->node->postEvent(tPtr,ZT_EVENT_USER_MESSAGE,reinterpret_cast<const void *>(&um));
	}
	peer->received(tPtr,path,pkt.hops(),pkt.packetId(),pkt.payloadLength(),Packet::VERB_USER_MESSAGE,0,Packet::VERB_NOP,0);
	return true;
}

//////////////////////////////////////////////////////////////////////////////
} // anonymous namespace

bool IncomingPacket::tryDecode(const RuntimeEnvironment *RR,void *tPtr)
{
	const Address sourceAddress(source());

	try {
		// Check for trusted paths or unencrypted HELLOs (HELLO is the only packet sent in the clear)
		const unsigned int c = cipher();
		bool trusted = false;
		if (c == ZT_PROTO_CIPHER_SUITE__NONE) {
			// If this is marked as a packet via a trusted path, check source address and path ID.
			// Obviously if no trusted paths are configured this always returns false and such
			// packets are dropped on the floor.
			const uint64_t tpid = trustedPathId();
			if (RR->topology->shouldInboundPathBeTrusted(_path->address(),tpid)) {
				trusted = true;
			} else {
				RR->t->incomingPacketMessageAuthenticationFailure(tPtr,_path,packetId(),sourceAddress,hops(),"path not trusted");
				return true;
			}
		} else if ((c == ZT_PROTO_CIPHER_SUITE__POLY1305_NONE)&&(verb() == Packet::VERB_HELLO)) {
			// Only HELLO is allowed in the clear, but will still have a MAC
			return _doHELLO(*this,RR,tPtr,false,_path);
		}

		const SharedPtr<Peer> peer(RR->topology->get(sourceAddress));
		if (peer) {
			if (!trusted) {
				if (!dearmor(peer->key())) {
					RR->t->incomingPacketMessageAuthenticationFailure(tPtr,_path,packetId(),sourceAddress,hops(),"invalid MAC");
					_path->recordInvalidPacket();
					return true;
				}
			}

			if (!uncompress()) {
				RR->t->incomingPacketInvalid(tPtr,_path,packetId(),sourceAddress,hops(),Packet::VERB_NOP,"LZ4 decompression failed");
				return true;
			}

			const Packet::Verb v = verb();
			bool r = true;
			switch(v) {
				//case Packet::VERB_NOP:
				default: // ignore unknown verbs, but if they pass auth check they are "received"
					peer->received(tPtr,_path,hops(),packetId(),payloadLength(),v,0,Packet::VERB_NOP,0);
					break;
				case Packet::VERB_HELLO:                      r = _doHELLO(*this,RR,tPtr,true,_path);                  break;
				case Packet::VERB_ACK:                        r = _doACK(*this,RR,tPtr,peer,_path);                    break;
				case Packet::VERB_QOS_MEASUREMENT:            r = _doQOS_MEASUREMENT(*this,RR,tPtr,peer,_path);        break;
				case Packet::VERB_ERROR:                      r = _doERROR(*this,RR,tPtr,peer,_path);                  break;
				case Packet::VERB_OK:                         r = _doOK(*this,RR,tPtr,peer,_path);                     break;
				case Packet::VERB_WHOIS:                      r = _doWHOIS(*this,RR,tPtr,peer,_path);                  break;
				case Packet::VERB_RENDEZVOUS:                 r = _doRENDEZVOUS(*this,RR,tPtr,peer,_path);             break;
				case Packet::VERB_FRAME:                      r = _doFRAME(*this,RR,tPtr,peer,_path);                  break;
				case Packet::VERB_EXT_FRAME:                  r = _doEXT_FRAME(*this,RR,tPtr,peer,_path);              break;
				case Packet::VERB_ECHO:                       r = _doECHO(*this,RR,tPtr,peer,_path);                   break;
				case Packet::VERB_NETWORK_CREDENTIALS:        r = _doNETWORK_CREDENTIALS(*this,RR,tPtr,peer,_path);    break;
				case Packet::VERB_NETWORK_CONFIG_REQUEST:     r = _doNETWORK_CONFIG_REQUEST(*this,RR,tPtr,peer,_path); break;
				case Packet::VERB_NETWORK_CONFIG:             r = _doNETWORK_CONFIG(*this,RR,tPtr,peer,_path);         break;
				case Packet::VERB_MULTICAST_GATHER:           r = _doMULTICAST_GATHER(*this,RR,tPtr,peer,_path);       break;
				case Packet::VERB_PUSH_DIRECT_PATHS:          r = _doPUSH_DIRECT_PATHS(*this,RR,tPtr,peer,_path);      break;
				case Packet::VERB_USER_MESSAGE:               r = _doUSER_MESSAGE(*this,RR,tPtr,peer,_path);           break;
			}
			return r;
		} else {
			RR->sw->requestWhois(tPtr,RR->node->now(),sourceAddress);
			return false;
		}
	} catch (int ztExcCode) {
		RR->t->incomingPacketInvalid(tPtr,_path,packetId(),sourceAddress,hops(),verb(),"unexpected exception in tryDecode()");
		return true;
	} catch ( ... ) {
		RR->t->incomingPacketInvalid(tPtr,_path,packetId(),sourceAddress,hops(),verb(),"unexpected exception in tryDecode()");
		return true;
	}
}

#if 0

bool IncomingPacket::_doACK(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	if (!peer->rateGateACK(RR->node->now()))
		return true;
	/* Dissect incoming ACK packet. From this we can estimate current throughput of the path, establish known
	 * maximums and detect packet loss. */
	if (peer->localMultipathSupport()) {
		int32_t ackedBytes;
		if (payloadLength() != sizeof(ackedBytes)) {
			return true; // ignore
		}
		memcpy(&ackedBytes, payload(), sizeof(ackedBytes));
		_path->receivedAck(RR->node->now(), Utils::ntoh(ackedBytes));
		peer->inferRemoteMultipathEnabled();
	}

	return true;
}

bool IncomingPacket::_doQOS_MEASUREMENT(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	if (!peer->rateGateQoS(RR->node->now()))
		return true;

	/* Dissect incoming QoS packet. From this we can compute latency values and their variance.
	 * The latency variance is used as a measure of "jitter". */
	if (peer->localMultipathSupport()) {
		if (payloadLength() > ZT_PATH_MAX_QOS_PACKET_SZ || payloadLength() < ZT_PATH_MIN_QOS_PACKET_SZ) {
			return true; // ignore
		}
		const int64_t now = RR->node->now();
		uint64_t rx_id[ZT_PATH_QOS_TABLE_SIZE];
		uint16_t rx_ts[ZT_PATH_QOS_TABLE_SIZE];
		char *begin = (char *)payload();
		char *ptr = begin;
		int count = 0;
		int len = payloadLength();
		// Read packet IDs and latency compensation intervals for each packet tracked by this QoS packet
		while (ptr < (begin + len) && (count < ZT_PATH_QOS_TABLE_SIZE)) {
			memcpy((void*)&rx_id[count], ptr, sizeof(uint64_t));
			ptr+=sizeof(uint64_t);
			memcpy((void*)&rx_ts[count], ptr, sizeof(uint16_t));
			ptr+=sizeof(uint16_t);
			count++;
		}
		_path->receivedQoS(now, count, rx_id, rx_ts);
		peer->inferRemoteMultipathEnabled();
	}

	return true;
}

bool IncomingPacket::_doMULTICAST_FRAME(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	unsigned int offset = ZT_PACKET_IDX_PAYLOAD;
	const uint64_t nwid = at<uint64_t>(offset); offset += 8;
	const unsigned int flags = (*this)[offset]; ++offset;

	const SharedPtr<Network> network(RR->node->network(nwid));
	if (network) {
		if ((flags & 0x01) != 0) {
			// This is deprecated but may still be sent by old peers
			CertificateOfMembership com;
			offset += com.deserialize(*this,offset);
			if (com)
				network->addCredential(tPtr,com);
		}

		if (!network->gate(tPtr,peer)) {
			_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
			return false;
		}

		unsigned int gatherLimit = 0;
		if ((flags & 0x02) != 0) {
			gatherLimit = at<uint32_t>(offset); offset += 4;
		}

		MAC from;
		if ((flags & 0x04) != 0) {
			from.setTo(field(offset,6),6); offset += 6;
		} else {
			from.fromAddress(peer->address(),nwid);
		}

		const unsigned int recipientsOffset = offset;
		std::list<Address> recipients;
		if ((flags & 0x08) != 0) {
			const unsigned int rc = at<uint16_t>(offset); offset += 2;
			for(unsigned int i=0;i<rc;++i) {
				const Address a(field(offset,5),5);
				if ((a != peer->address())&&(a != RR->identity.address())) {
					recipients.push_back(a);
				}
				offset += 5;
			}
		}
		const unsigned int afterRecipientsOffset = offset;

		const MulticastGroup to(MAC(field(offset,6),6),at<uint32_t>(offset + 6)); offset += 10;
		const unsigned int etherType = at<uint16_t>(offset); offset += 2;
		const unsigned int frameLen = size() - offset;

		if (network->config().multicastLimit == 0) {
			RR->t->incomingNetworkFrameDropped(tPtr,network,_path,packetId(),size(),peer->address(),Packet::VERB_MULTICAST_FRAME,from,to.mac(),"multicast disabled");
			peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,nwid);
			return true;
		}
		if (!to.mac().isMulticast()) {
			RR->t->incomingPacketInvalid(tPtr,_path,packetId(),source(),hops(),Packet::VERB_MULTICAST_FRAME,"destination not multicast");
			peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,nwid);
			return true;
		}
		if ((!from)||(from.isMulticast())||(from == network->mac())) {
			RR->t->incomingPacketInvalid(tPtr,_path,packetId(),source(),hops(),Packet::VERB_MULTICAST_FRAME,"invalid source MAC");
			peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,nwid);
			return true;
		}

		if ((frameLen > 0)&&(frameLen <= ZT_MAX_MTU)) {
			const uint8_t *const frameData = ((const uint8_t *)unsafeData()) + offset;
			if (network->filterIncomingPacket(tPtr,peer,RR->identity.address(),from,to.mac(),frameData,frameLen,etherType,0) > 0) {
				RR->node->putFrame(tPtr,nwid,network->userPtr(),from,to.mac(),etherType,0,(const void *)frameData,frameLen);
			}
		}

		if (!recipients.empty()) {
			// TODO
			/*
			const std::vector<Address> anchors = network->config().anchors();
			const bool amAnchor = (std::find(anchors.begin(),anchors.end(),RR->identity.address()) != anchors.end());

			for(std::list<Address>::iterator ra(recipients.begin());ra!=recipients.end();) {
				SharedPtr<Peer> recipient(RR->topology->get(*ra));
				if ((recipient)&&((recipient->remoteVersionProtocol() < 10)||(amAnchor))) {
					Packet outp(*ra,RR->identity.address(),Packet::VERB_MULTICAST_FRAME);
					outp.append(field(ZT_PACKET_IDX_PAYLOAD,recipientsOffset - ZT_PACKET_IDX_PAYLOAD),recipientsOffset - ZT_PACKET_IDX_PAYLOAD);
					outp.append(field(afterRecipientsOffset,size() - afterRecipientsOffset),size() - afterRecipientsOffset);
					RR->sw->send(tPtr,outp,true);
					recipients.erase(ra++);
				} else ++ra;
			}

			if (!recipients.empty()) {
				Packet outp(recipients.front(),RR->identity.address(),Packet::VERB_MULTICAST_FRAME);
				recipients.pop_front();
				outp.append(field(ZT_PACKET_IDX_PAYLOAD,recipientsOffset - ZT_PACKET_IDX_PAYLOAD),recipientsOffset - ZT_PACKET_IDX_PAYLOAD);
				if (!recipients.empty()) {
					outp.append((uint16_t)recipients.size());
					for(std::list<Address>::iterator ra(recipients.begin());ra!=recipients.end();++ra)
						ra->appendTo(outp);
				}
				outp.append(field(afterRecipientsOffset,size() - afterRecipientsOffset),size() - afterRecipientsOffset);
				RR->sw->send(tPtr,outp,true);
			}
			*/
		}

		if (gatherLimit) { // DEPRECATED but still supported
			/*
			Packet outp(source(),RR->identity.address(),Packet::VERB_OK);
			outp.append((unsigned char)Packet::VERB_MULTICAST_FRAME);
			outp.append(packetId());
			outp.append(nwid);
			to.mac().appendTo(outp);
			outp.append((uint32_t)to.adi());
			outp.append((unsigned char)0x02); // flag 0x02 = contains gather results
			if (RR->mc->gather(peer->address(),nwid,to,outp,gatherLimit)) {
				outp.armor(peer->key(),true);
				_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
			}
			*/
		}

		peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,nwid);
		return true;
	} else {
		_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
		return false;
	}
}

#endif

} // namespace ZeroTier
