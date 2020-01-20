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

	SharedPtr<Peer> peer(RR->topology->get(tPtr,id.address()));
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
		SharedPtr<Peer> newPeer(new Peer(RR));
		if (!newPeer->init(RR->identity,id)) {
			RR->t->incomingPacketDroppedHELLO(tPtr,path,pid,fromAddress,"error initializing peer");
			return true;
		}
		if (!pkt.dearmor(newPeer->key())) {
			RR->t->incomingPacketMessageAuthenticationFailure(tPtr,path,pid,fromAddress,pkt.hops(),"invalid MAC");
			return true;
		}

		// Check that identity's address is valid as per the derivation function
		if (!id.locallyValidate()) {
			RR->t->incomingPacketDroppedHELLO(tPtr,path,pid,fromAddress,"invalid identity");
			return true;
		}

		peer = RR->topology->add(tPtr,newPeer);

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
				if ((ZT_PROTO_VERB_HELLO__OK__IDX_REVISION + 2) < pkt.size()) {
					InetAddress externalSurfaceAddress;
					externalSurfaceAddress.deserialize(pkt,ZT_PROTO_VERB_HELLO__OK__IDX_REVISION + 2);
					if (externalSurfaceAddress)
						RR->sa->iam(tPtr,peer->address(),path->localSocket(),path->address(),externalSurfaceAddress,RR->topology->isRoot(peer->identity()),RR->node->now());
				}
			}

			peer->updateLatency((unsigned int)latency);
			peer->setRemoteVersion(vProto,vMajor,vMinor,vRevision);
		}	break;

		case Packet::VERB_WHOIS:
			if (RR->topology->isRoot(peer->identity())) {
				unsigned int p = ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY;
				while (p < pkt.size()) {
					try {
						Identity id;
						p += id.deserialize(pkt,p);
						if (id) {
							SharedPtr<Peer> ptmp(RR->topology->add(tPtr,SharedPtr<Peer>(new Peer(RR))));
							ptmp->init(RR->identity,id);
							RR->sw->doAnythingWaitingForPeer(tPtr,ptmp);
						}
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

		const SharedPtr<Peer> ptmp(RR->topology->get(tPtr,addr));
		if (ptmp) {
			ptmp->identity().serialize(outp,false);
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
		uint16_t junk = (uint16_t)Utils::random();
		const Address with(pkt.field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ZTADDRESS,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
		const SharedPtr<Peer> rendezvousWith(RR->topology->get(tPtr,with));
		if (rendezvousWith) {
			const unsigned int port = pkt.at<uint16_t>(ZT_PROTO_VERB_RENDEZVOUS_IDX_PORT);
			const unsigned int addrlen = pkt[ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRLEN];
			if ((port > 0)&&((addrlen == 4)||(addrlen == 16))) {
				InetAddress atAddr(pkt.field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRESS,addrlen),addrlen,port);
				if (rendezvousWith->shouldTryPath(tPtr,RR->node->now(),peer,atAddr)) {
					if (atAddr.isV4())
						RR->node->putPacket(tPtr,path->localSocket(),atAddr,&junk,2,2); // IPv4 "firewall opener"
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

		if ((flags & 0x10U) != 0) { // ACK requested
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

	if (peer->rateGateInboundPushDirectPaths(now)) {
		uint8_t countPerScope[ZT_INETADDRESS_MAX_SCOPE+1][2]; // [][0] is v4, [][1] is v6
		memset(countPerScope,0,sizeof(countPerScope));

		unsigned int count = pkt.at<uint16_t>(ZT_PACKET_IDX_PAYLOAD);
		unsigned int ptr = ZT_PACKET_IDX_PAYLOAD + 2;
		uint16_t junk = (uint16_t)Utils::random();

		while (count--) {
			/* unsigned int flags = (*this)[ptr++]; */ ++ptr;
			unsigned int extLen = pkt.at<uint16_t>(ptr); ptr += 2;
			ptr += extLen; // unused right now
			unsigned int addrType = pkt[ptr++];
			unsigned int addrLen = pkt[ptr++];

			switch(addrType) {
				case 4: {
					const InetAddress a(pkt.field(ptr,4),4,pkt.at<uint16_t>(ptr + 4));
					if (peer->shouldTryPath(tPtr,now,peer,a)) {
						if (++countPerScope[(int)a.ipScope()][0] <= ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY) {
							RR->node->putPacket(tPtr,path->localSocket(),a,&junk,2,2); // IPv4 "firewall opener"
							++junk;
							peer->sendHELLO(tPtr,-1,a,now);
						}
					}
				}	break;
				case 6: {
					const InetAddress a(pkt.field(ptr,16),16,pkt.at<uint16_t>(ptr + 16));
					if (peer->shouldTryPath(tPtr,now,peer,a)) {
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
		um.id = (const ZT_Identity *)(&(peer->identity()));
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

		const SharedPtr<Peer> peer(RR->topology->get(tPtr,sourceAddress));
		if (peer) {
			if (!trusted) {
				if (!dearmor(peer->key())) {
					RR->t->incomingPacketMessageAuthenticationFailure(tPtr,_path,packetId(),sourceAddress,hops(),"invalid MAC");
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

} // namespace ZeroTier
