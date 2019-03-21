/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../version.h"
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
#include "SHA512.hpp"
#include "World.hpp"
#include "Node.hpp"
#include "CertificateOfMembership.hpp"
#include "Capability.hpp"
#include "Tag.hpp"
#include "Revocation.hpp"
#include "Trace.hpp"

namespace ZeroTier {

bool IncomingPacket::tryDecode(const RuntimeEnvironment *RR,void *tPtr)
{
	const Address sourceAddress(source());

	try {
		// Check for trusted paths or unencrypted HELLOs (HELLO is the only packet sent in the clear)
		const unsigned int c = cipher();
		bool trusted = false;
		if (c == ZT_PROTO_CIPHER_SUITE__NO_CRYPTO_TRUSTED_PATH) {
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
		} else if ((c == ZT_PROTO_CIPHER_SUITE__C25519_POLY1305_NONE)&&(verb() == Packet::VERB_HELLO)) {
			// Only HELLO is allowed in the clear, but will still have a MAC
			return _doHELLO(RR,tPtr,false);
		}

		const SharedPtr<Peer> peer(RR->topology->getPeer(tPtr,sourceAddress));
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
					peer->received(tPtr,_path,hops(),packetId(),payloadLength(),v,0,Packet::VERB_NOP,false,0);
					break;
				case Packet::VERB_HELLO:                      r = _doHELLO(RR,tPtr,true); break;
				case Packet::VERB_ACK:                        r = _doACK(RR,tPtr,peer); break;
				case Packet::VERB_QOS_MEASUREMENT:            r = _doQOS_MEASUREMENT(RR,tPtr,peer); break;
				case Packet::VERB_ERROR:                      r = _doERROR(RR,tPtr,peer); break;
				case Packet::VERB_OK:                         r = _doOK(RR,tPtr,peer); break;
				case Packet::VERB_WHOIS:                      r = _doWHOIS(RR,tPtr,peer); break;
				case Packet::VERB_RENDEZVOUS:                 r = _doRENDEZVOUS(RR,tPtr,peer); break;
				case Packet::VERB_FRAME:                      r = _doFRAME(RR,tPtr,peer); break;
				case Packet::VERB_EXT_FRAME:                  r = _doEXT_FRAME(RR,tPtr,peer); break;
				case Packet::VERB_ECHO:                       r = _doECHO(RR,tPtr,peer); break;
				case Packet::VERB_MULTICAST_LIKE:             r = _doMULTICAST_LIKE(RR,tPtr,peer); break;
				case Packet::VERB_NETWORK_CREDENTIALS:        r = _doNETWORK_CREDENTIALS(RR,tPtr,peer); break;
				case Packet::VERB_NETWORK_CONFIG_REQUEST:     r = _doNETWORK_CONFIG_REQUEST(RR,tPtr,peer); break;
				case Packet::VERB_NETWORK_CONFIG:             r = _doNETWORK_CONFIG(RR,tPtr,peer); break;
				case Packet::VERB_MULTICAST_GATHER:           r = _doMULTICAST_GATHER(RR,tPtr,peer); break;
				case Packet::VERB_MULTICAST_FRAME:            r = _doMULTICAST_FRAME(RR,tPtr,peer); break;
				case Packet::VERB_PUSH_DIRECT_PATHS:          r = _doPUSH_DIRECT_PATHS(RR,tPtr,peer); break;
				case Packet::VERB_USER_MESSAGE:               r = _doUSER_MESSAGE(RR,tPtr,peer); break;
				case Packet::VERB_REMOTE_TRACE:               r = _doREMOTE_TRACE(RR,tPtr,peer); break;
			}
			if (r) {
				RR->node->statsLogVerb((unsigned int)v,(unsigned int)size());
				return true;
			}
			return false;
		} else {
			RR->sw->requestWhois(tPtr,RR->node->now(),sourceAddress);
			return false;
		}
	} catch ( ... ) {
		RR->t->incomingPacketInvalid(tPtr,_path,packetId(),sourceAddress,hops(),verb(),"unexpected exception in tryDecode()");
		return true;
	}
}

bool IncomingPacket::_doERROR(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	const Packet::Verb inReVerb = (Packet::Verb)(*this)[ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB];
	const uint64_t inRePacketId = at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_IN_RE_PACKET_ID);
	const Packet::ErrorCode errorCode = (Packet::ErrorCode)(*this)[ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE];
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
				const SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)));
				if ((network)&&(network->controller() == peer->address()))
					network->setNotFound();
			}
			break;

		case Packet::ERROR_UNSUPPORTED_OPERATION:
			// This can be sent in response to any operation, though right now we only
			// consider it meaningful from network controllers. This would indicate
			// that the queried node does not support acting as a controller.
			if (inReVerb == Packet::VERB_NETWORK_CONFIG_REQUEST) {
				const SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)));
				if ((network)&&(network->controller() == peer->address()))
					network->setNotFound();
			}
			break;

		case Packet::ERROR_IDENTITY_COLLISION:
			// FIXME: for federation this will need a payload with a signature or something.
			if (RR->topology->isUpstream(peer->identity()))
				RR->node->postEvent(tPtr,ZT_EVENT_FATAL_ERROR_IDENTITY_COLLISION);
			break;

		case Packet::ERROR_NEED_MEMBERSHIP_CERTIFICATE: {
			// Peers can send this in response to frames if they do not have a recent enough COM from us
			networkId = at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD);
			const SharedPtr<Network> network(RR->node->network(networkId));
			const int64_t now = RR->node->now();
			if ((network)&&(network->config().com))
				network->pushCredentialsNow(tPtr,peer->address(),now);
		}	break;

		case Packet::ERROR_NETWORK_ACCESS_DENIED_: {
			// Network controller: network access denied.
			const SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)));
			if ((network)&&(network->controller() == peer->address()))
				network->setAccessDenied();
		}	break;

		case Packet::ERROR_UNWANTED_MULTICAST: {
			// Members of networks can use this error to indicate that they no longer
			// want to receive multicasts on a given channel.
			networkId = at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD);
			const SharedPtr<Network> network(RR->node->network(networkId));
			if ((network)&&(network->gate(tPtr,peer))) {
				const MulticastGroup mg(MAC(field(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD + 8,6),6),at<uint32_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD + 14));
				RR->mc->remove(network->id(),mg,peer->address());
			}
		}	break;

		default: break;
	}

	peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_ERROR,inRePacketId,inReVerb,false,networkId);

	return true;
}

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

bool IncomingPacket::_doHELLO(const RuntimeEnvironment *RR,void *tPtr,const bool alreadyAuthenticated)
{
	const int64_t now = RR->node->now();

	const uint64_t pid = packetId();
	const Address fromAddress(source());
	const unsigned int protoVersion = (*this)[ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION];
	const unsigned int vMajor = (*this)[ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION];
	const unsigned int vMinor = (*this)[ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION];
	const unsigned int vRevision = at<uint16_t>(ZT_PROTO_VERB_HELLO_IDX_REVISION);
	const int64_t timestamp = at<int64_t>(ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP);
	Identity id;
	unsigned int ptr = ZT_PROTO_VERB_HELLO_IDX_IDENTITY + id.deserialize(*this,ZT_PROTO_VERB_HELLO_IDX_IDENTITY);

	if (protoVersion < ZT_PROTO_VERSION_MIN) {
		RR->t->incomingPacketDroppedHELLO(tPtr,_path,pid,fromAddress,"protocol version too old");
		return true;
	}
	if (fromAddress != id.address()) {
		RR->t->incomingPacketDroppedHELLO(tPtr,_path,pid,fromAddress,"identity/address mismatch");
		return true;
	}

	SharedPtr<Peer> peer(RR->topology->getPeer(tPtr,id.address()));
	if (peer) {
		// We already have an identity with this address -- check for collisions
		if (!alreadyAuthenticated) {
			if (peer->identity() != id) {
				// Identity is different from the one we already have -- address collision

				// Check rate limits
				if (!RR->node->rateGateIdentityVerification(now,_path->address()))
					return true;

				uint8_t key[ZT_PEER_SECRET_KEY_LENGTH];
				if (RR->identity.agree(id,key,ZT_PEER_SECRET_KEY_LENGTH)) {
					if (dearmor(key)) { // ensure packet is authentic, otherwise drop
						RR->t->incomingPacketDroppedHELLO(tPtr,_path,pid,fromAddress,"address collision");
						Packet outp(id.address(),RR->identity.address(),Packet::VERB_ERROR);
						outp.append((uint8_t)Packet::VERB_HELLO);
						outp.append((uint64_t)pid);
						outp.append((uint8_t)Packet::ERROR_IDENTITY_COLLISION);
						outp.armor(key,true);
						_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
					} else {
						RR->t->incomingPacketMessageAuthenticationFailure(tPtr,_path,pid,fromAddress,hops(),"invalid MAC");
					}
				} else {
					RR->t->incomingPacketMessageAuthenticationFailure(tPtr,_path,pid,fromAddress,hops(),"invalid identity");
				}

				return true;
			} else {
				// Identity is the same as the one we already have -- check packet integrity

				if (!dearmor(peer->key())) {
					RR->t->incomingPacketMessageAuthenticationFailure(tPtr,_path,pid,fromAddress,hops(),"invalid MAC");
					return true;
				}

				// Continue at // VALID
			}
		} // else if alreadyAuthenticated then continue at // VALID
	} else {
		// We don't already have an identity with this address -- validate and learn it

		// Sanity check: this basically can't happen
		if (alreadyAuthenticated) {
			RR->t->incomingPacketDroppedHELLO(tPtr,_path,pid,fromAddress,"illegal alreadyAuthenticated state");
			return true;
		}

		// Check rate limits
		if (!RR->node->rateGateIdentityVerification(now,_path->address())) {
			RR->t->incomingPacketDroppedHELLO(tPtr,_path,pid,fromAddress,"rate limit exceeded");
			return true;
		}

		// Check packet integrity and MAC (this is faster than locallyValidate() so do it first to filter out total crap)
		SharedPtr<Peer> newPeer(new Peer(RR,RR->identity,id));
		if (!dearmor(newPeer->key())) {
			RR->t->incomingPacketMessageAuthenticationFailure(tPtr,_path,pid,fromAddress,hops(),"invalid MAC");
			return true;
		}

		// Check that identity's address is valid as per the derivation function
		if (!id.locallyValidate()) {
			RR->t->incomingPacketDroppedHELLO(tPtr,_path,pid,fromAddress,"invalid identity");
			return true;
		}

		peer = RR->topology->addPeer(tPtr,newPeer);

		// Continue at // VALID
	}

	// VALID -- if we made it here, packet passed identity and authenticity checks!

	// Get external surface address if present (was not in old versions)
	InetAddress externalSurfaceAddress;
	if (ptr < size()) {
		ptr += externalSurfaceAddress.deserialize(*this,ptr);
		if ((externalSurfaceAddress)&&(hops() == 0))
			RR->sa->iam(tPtr,id.address(),_path->localSocket(),_path->address(),externalSurfaceAddress,RR->topology->isUpstream(id),now);
	}

	// Get primary planet world ID and world timestamp if present
	uint64_t planetWorldId = 0;
	uint64_t planetWorldTimestamp = 0;
	if ((ptr + 16) <= size()) {
		planetWorldId = at<uint64_t>(ptr); ptr += 8;
		planetWorldTimestamp = at<uint64_t>(ptr); ptr += 8;
	}

	std::vector< std::pair<uint64_t,uint64_t> > moonIdsAndTimestamps;
	if (ptr < size()) {
		// Remainder of packet, if present, is encrypted
		cryptField(peer->key(),ptr,size() - ptr);

		// Get moon IDs and timestamps if present
		if ((ptr + 2) <= size()) {
			const unsigned int numMoons = at<uint16_t>(ptr); ptr += 2;
			for(unsigned int i=0;i<numMoons;++i) {
				if ((World::Type)(*this)[ptr++] == World::TYPE_MOON)
					moonIdsAndTimestamps.push_back(std::pair<uint64_t,uint64_t>(at<uint64_t>(ptr),at<uint64_t>(ptr + 8)));
				ptr += 16;
			}
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

	if (protoVersion >= 5) {
		_path->address().serialize(outp);
	} else {
		/* LEGACY COMPATIBILITY HACK:
		 *
		 * For a while now (since 1.0.3), ZeroTier has recognized changes in
		 * its network environment empirically by examining its external network
		 * address as reported by trusted peers. In versions prior to 1.1.0
		 * (protocol version < 5), they did this by saving a snapshot of this
		 * information (in SelfAwareness.hpp) keyed by reporting device ID and
		 * address type.
		 *
		 * This causes problems when clustering is combined with symmetric NAT.
		 * Symmetric NAT remaps ports, so different endpoints in a cluster will
		 * report back different exterior addresses. Since the old code keys
		 * this by device ID and not sending physical address and compares the
		 * entire address including port, it constantly thinks its external
		 * surface is changing and resets connections when talking to a cluster.
		 *
		 * In new code we key by sending physical address and device and we also
		 * take the more conservative position of only interpreting changes in
		 * IP address (neglecting port) as a change in network topology that
		 * necessitates a reset. But we can make older clients work here by
		 * nulling out the port field. Since this info is only used for empirical
		 * detection of link changes, it doesn't break anything else.
		 */
		InetAddress tmpa(_path->address());
		tmpa.setPort(0);
		tmpa.serialize(outp);
	}

	const unsigned int worldUpdateSizeAt = outp.size();
	outp.addSize(2); // make room for 16-bit size field
	if ((planetWorldId)&&(RR->topology->planetWorldTimestamp() > planetWorldTimestamp)&&(planetWorldId == RR->topology->planetWorldId())) {
		RR->topology->planet().serialize(outp,false);
	}
	if (moonIdsAndTimestamps.size() > 0) {
		std::vector<World> moons(RR->topology->moons());
		for(std::vector<World>::const_iterator m(moons.begin());m!=moons.end();++m) {
			for(std::vector< std::pair<uint64_t,uint64_t> >::const_iterator i(moonIdsAndTimestamps.begin());i!=moonIdsAndTimestamps.end();++i) {
				if (i->first == m->id()) {
					if (m->timestamp() > i->second)
						m->serialize(outp,false);
					break;
				}
			}
		}
	}
	outp.setAt<uint16_t>(worldUpdateSizeAt,(uint16_t)(outp.size() - (worldUpdateSizeAt + 2)));

	outp.armor(peer->key(),true);
	_path->send(RR,tPtr,outp.data(),outp.size(),now);

	peer->setRemoteVersion(protoVersion,vMajor,vMinor,vRevision); // important for this to go first so received() knows the version
	peer->received(tPtr,_path,hops(),pid,payloadLength(),Packet::VERB_HELLO,0,Packet::VERB_NOP,false,0);

	return true;
}

bool IncomingPacket::_doOK(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	const Packet::Verb inReVerb = (Packet::Verb)(*this)[ZT_PROTO_VERB_OK_IDX_IN_RE_VERB];
	const uint64_t inRePacketId = at<uint64_t>(ZT_PROTO_VERB_OK_IDX_IN_RE_PACKET_ID);
	uint64_t networkId = 0;

	if (!RR->node->expectingReplyTo(inRePacketId))
		return true;

	switch(inReVerb) {

		case Packet::VERB_HELLO: {
			const uint64_t latency = RR->node->now() - at<uint64_t>(ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP);
			if (latency > ZT_HELLO_MAX_ALLOWABLE_LATENCY)
				return true;

			const unsigned int vProto = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_PROTOCOL_VERSION];
			const unsigned int vMajor = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_MAJOR_VERSION];
			const unsigned int vMinor = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_MINOR_VERSION];
			const unsigned int vRevision = at<uint16_t>(ZT_PROTO_VERB_HELLO__OK__IDX_REVISION);
			if (vProto < ZT_PROTO_VERSION_MIN)
				return true;

			InetAddress externalSurfaceAddress;
			unsigned int ptr = ZT_PROTO_VERB_HELLO__OK__IDX_REVISION + 2;

			// Get reported external surface address if present
			if (ptr < size())
				ptr += externalSurfaceAddress.deserialize(*this,ptr);

			// Handle planet or moon updates if present
			if ((ptr + 2) <= size()) {
				const unsigned int worldsLen = at<uint16_t>(ptr); ptr += 2;
				if (RR->topology->shouldAcceptWorldUpdateFrom(peer->address())) {
					const unsigned int endOfWorlds = ptr + worldsLen;
					while (ptr < endOfWorlds) {
						World w;
						ptr += w.deserialize(*this,ptr);
						RR->topology->addWorld(tPtr,w,false);
					}
				} else {
					ptr += worldsLen;
				}
			}

			if (!hops() && (RR->node->getMultipathMode() != ZT_MULTIPATH_NONE)) {
				_path->updateLatency((unsigned int)latency, RR->node->now());
			}

			peer->setRemoteVersion(vProto,vMajor,vMinor,vRevision);

			if ((externalSurfaceAddress)&&(hops() == 0))
				RR->sa->iam(tPtr,peer->address(),_path->localSocket(),_path->address(),externalSurfaceAddress,RR->topology->isUpstream(peer->identity()),RR->node->now());
		}	break;

		case Packet::VERB_WHOIS:
			if (RR->topology->isUpstream(peer->identity())) {
				const Identity id(*this,ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY);
				RR->sw->doAnythingWaitingForPeer(tPtr,RR->topology->addPeer(tPtr,SharedPtr<Peer>(new Peer(RR,RR->identity,id))));
			}
			break;

		case Packet::VERB_NETWORK_CONFIG_REQUEST: {
			networkId = at<uint64_t>(ZT_PROTO_VERB_OK_IDX_PAYLOAD);
			const SharedPtr<Network> network(RR->node->network(networkId));
			if (network)
				network->handleConfigChunk(tPtr,packetId(),source(),*this,ZT_PROTO_VERB_OK_IDX_PAYLOAD);
		}	break;

		case Packet::VERB_MULTICAST_GATHER: {
			networkId = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_NETWORK_ID);
			const SharedPtr<Network> network(RR->node->network(networkId));
			if (network) {
				const MulticastGroup mg(MAC(field(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_MAC,6),6),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_ADI));
				const unsigned int count = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_GATHER_RESULTS + 4);
				RR->mc->addMultiple(tPtr,RR->node->now(),networkId,mg,field(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_GATHER_RESULTS + 6,count * 5),count,at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_GATHER_RESULTS));
			}
		}	break;

		case Packet::VERB_MULTICAST_FRAME: {
			const unsigned int flags = (*this)[ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_FLAGS];
			networkId = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_NETWORK_ID);
			const MulticastGroup mg(MAC(field(ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_MAC,6),6),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_ADI));

			const SharedPtr<Network> network(RR->node->network(networkId));
			if (network) {
				unsigned int offset = 0;

				if ((flags & 0x01) != 0) { // deprecated but still used by older peers
					CertificateOfMembership com;
					offset += com.deserialize(*this,ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_COM_AND_GATHER_RESULTS);
					if (com)
						network->addCredential(tPtr,com);
				}

				if ((flags & 0x02) != 0) {
					// OK(MULTICAST_FRAME) includes implicit gather results
					offset += ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_COM_AND_GATHER_RESULTS;
					unsigned int totalKnown = at<uint32_t>(offset); offset += 4;
					unsigned int count = at<uint16_t>(offset); offset += 2;
					RR->mc->addMultiple(tPtr,RR->node->now(),networkId,mg,field(offset,count * 5),count,totalKnown);
				}
			}
		}	break;

		default: break;
	}

	peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_OK,inRePacketId,inReVerb,false,networkId);

	return true;
}

bool IncomingPacket::_doWHOIS(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	if ((!RR->topology->amUpstream())&&(!peer->rateGateInboundWhoisRequest(RR->node->now())))
		return true;

	Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
	outp.append((unsigned char)Packet::VERB_WHOIS);
	outp.append(packetId());

	unsigned int count = 0;
	unsigned int ptr = ZT_PACKET_IDX_PAYLOAD;
	while ((ptr + ZT_ADDRESS_LENGTH) <= size()) {
		const Address addr(field(ptr,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
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
		_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
	}

	peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_WHOIS,0,Packet::VERB_NOP,false,0);

	return true;
}

bool IncomingPacket::_doRENDEZVOUS(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	if (RR->topology->isUpstream(peer->identity())) {
		const Address with(field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ZTADDRESS,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
		const SharedPtr<Peer> rendezvousWith(RR->topology->getPeer(tPtr,with));
		if (rendezvousWith) {
			const unsigned int port = at<uint16_t>(ZT_PROTO_VERB_RENDEZVOUS_IDX_PORT);
			const unsigned int addrlen = (*this)[ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRLEN];
			if ((port > 0)&&((addrlen == 4)||(addrlen == 16))) {
				const InetAddress atAddr(field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRESS,addrlen),addrlen,port);
				if (RR->node->shouldUsePathForZeroTierTraffic(tPtr,with,_path->localSocket(),atAddr)) {
					const uint64_t junk = RR->node->prng();
					RR->node->putPacket(tPtr,_path->localSocket(),atAddr,&junk,4,2); // send low-TTL junk packet to 'open' local NAT(s) and stateful firewalls
					rendezvousWith->attemptToContactAt(tPtr,_path->localSocket(),atAddr,RR->node->now(),false);
				}
			}
		}
	}

	peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_RENDEZVOUS,0,Packet::VERB_NOP,false,0);

	return true;
}

bool IncomingPacket::_doFRAME(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID);
	const SharedPtr<Network> network(RR->node->network(nwid));
	bool trustEstablished = false;
	if (network) {
		if (network->gate(tPtr,peer)) {
			trustEstablished = true;
			if (size() > ZT_PROTO_VERB_FRAME_IDX_PAYLOAD) {
				const unsigned int etherType = at<uint16_t>(ZT_PROTO_VERB_FRAME_IDX_ETHERTYPE);
				const MAC sourceMac(peer->address(),nwid);
				const unsigned int frameLen = size() - ZT_PROTO_VERB_FRAME_IDX_PAYLOAD;
				const uint8_t *const frameData = reinterpret_cast<const uint8_t *>(data()) + ZT_PROTO_VERB_FRAME_IDX_PAYLOAD;
				if (network->filterIncomingPacket(tPtr,peer,RR->identity.address(),sourceMac,network->mac(),frameData,frameLen,etherType,0) > 0)
					RR->node->putFrame(tPtr,nwid,network->userPtr(),sourceMac,network->mac(),etherType,0,(const void *)frameData,frameLen);
			}
		} else {
			_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
			return false;
		}
	}

	peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_FRAME,0,Packet::VERB_NOP,trustEstablished,nwid);

	return true;
}

bool IncomingPacket::_doEXT_FRAME(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_EXT_FRAME_IDX_NETWORK_ID);
	const SharedPtr<Network> network(RR->node->network(nwid));
	if (network) {
		const unsigned int flags = (*this)[ZT_PROTO_VERB_EXT_FRAME_IDX_FLAGS];

		unsigned int comLen = 0;
		if ((flags & 0x01) != 0) { // inline COM with EXT_FRAME is deprecated but still used with old peers
			CertificateOfMembership com;
			comLen = com.deserialize(*this,ZT_PROTO_VERB_EXT_FRAME_IDX_COM);
			if (com)
				network->addCredential(tPtr,com);
		}

		if (!network->gate(tPtr,peer)) {
			RR->t->incomingNetworkAccessDenied(tPtr,network,_path,packetId(),size(),peer->address(),Packet::VERB_EXT_FRAME,true);
			_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
			return false;
		}

		if (size() > ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD) {
			const unsigned int etherType = at<uint16_t>(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_ETHERTYPE);
			const MAC to(field(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_TO,ZT_PROTO_VERB_EXT_FRAME_LEN_TO),ZT_PROTO_VERB_EXT_FRAME_LEN_TO);
			const MAC from(field(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_FROM,ZT_PROTO_VERB_EXT_FRAME_LEN_FROM),ZT_PROTO_VERB_EXT_FRAME_LEN_FROM);
			const unsigned int frameLen = size() - (comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD);
			const uint8_t *const frameData = (const uint8_t *)field(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD,frameLen);

			if ((!from)||(from == network->mac())) {
				peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,true,nwid); // trustEstablished because COM is okay
				return true;
			}

			switch (network->filterIncomingPacket(tPtr,peer,RR->identity.address(),from,to,frameData,frameLen,etherType,0)) {
				case 1:
					if (from != MAC(peer->address(),nwid)) {
						if (network->config().permitsBridging(peer->address())) {
							network->learnBridgeRoute(from,peer->address());
						} else {
							RR->t->incomingNetworkFrameDropped(tPtr,network,_path,packetId(),size(),peer->address(),Packet::VERB_EXT_FRAME,from,to,"bridging not allowed (remote)");
							peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,true,nwid); // trustEstablished because COM is okay
							return true;
						}
					} else if (to != network->mac()) {
						if (to.isMulticast()) {
							if (network->config().multicastLimit == 0) {
								RR->t->incomingNetworkFrameDropped(tPtr,network,_path,packetId(),size(),peer->address(),Packet::VERB_EXT_FRAME,from,to,"multicast disabled");
								peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,true,nwid); // trustEstablished because COM is okay
								return true;
							}
						} else if (!network->config().permitsBridging(RR->identity.address())) {
							RR->t->incomingNetworkFrameDropped(tPtr,network,_path,packetId(),size(),peer->address(),Packet::VERB_EXT_FRAME,from,to,"bridging not allowed (local)");
							peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,true,nwid); // trustEstablished because COM is okay
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
			outp.append((uint64_t)packetId());
			outp.append((uint64_t)nwid);
			outp.armor(peer->key(),true);
			_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
		}

		peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,true,nwid);
	} else {
		peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,false,nwid);
	}

	return true;
}

bool IncomingPacket::_doECHO(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	if (!peer->rateGateEchoRequest(RR->node->now()))
		return true;

	const uint64_t pid = packetId();
	Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
	outp.append((unsigned char)Packet::VERB_ECHO);
	outp.append((uint64_t)pid);
	if (size() > ZT_PACKET_IDX_PAYLOAD)
		outp.append(reinterpret_cast<const unsigned char *>(data()) + ZT_PACKET_IDX_PAYLOAD,size() - ZT_PACKET_IDX_PAYLOAD);
	outp.armor(peer->key(),true);
	_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());

	peer->received(tPtr,_path,hops(),pid,payloadLength(),Packet::VERB_ECHO,0,Packet::VERB_NOP,false,0);

	return true;
}

bool IncomingPacket::_doMULTICAST_LIKE(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	const int64_t now = RR->node->now();
	bool authorized = false;
	uint64_t lastNwid = 0;

	// Packet contains a series of 18-byte network,MAC,ADI tuples
	for(unsigned int ptr=ZT_PACKET_IDX_PAYLOAD;ptr<size();ptr+=18) {
		const uint64_t nwid = at<uint64_t>(ptr);
		if (nwid != lastNwid) {
			lastNwid = nwid;
			SharedPtr<Network> network(RR->node->network(nwid));
			if (network)
				authorized = network->gate(tPtr,peer);
			if (!authorized)
				authorized = ((RR->topology->amUpstream())||(RR->node->localControllerHasAuthorized(now,nwid,peer->address())));
		}
		if (authorized)
			RR->mc->add(tPtr,now,nwid,MulticastGroup(MAC(field(ptr + 8,6),6),at<uint32_t>(ptr + 14)),peer->address());
	}

	peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_MULTICAST_LIKE,0,Packet::VERB_NOP,false,0);
	return true;
}

bool IncomingPacket::_doNETWORK_CREDENTIALS(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	if (!peer->rateGateCredentialsReceived(RR->node->now()))
		return true;

	CertificateOfMembership com;
	Capability cap;
	Tag tag;
	Revocation revocation;
	CertificateOfOwnership coo;
	bool trustEstablished = false;
	SharedPtr<Network> network;

	unsigned int p = ZT_PACKET_IDX_PAYLOAD;
	while ((p < size())&&((*this)[p] != 0)) {
		p += com.deserialize(*this,p);
		if (com) {
			network = RR->node->network(com.networkId());
			if (network) {
				switch (network->addCredential(tPtr,com)) {
					case Membership::ADD_REJECTED:
						break;
					case Membership::ADD_ACCEPTED_NEW:
					case Membership::ADD_ACCEPTED_REDUNDANT:
						trustEstablished = true;
						break;
					case Membership::ADD_DEFERRED_FOR_WHOIS:
						return false;
				}
			}
		}
	}
	++p; // skip trailing 0 after COMs if present

	if (p < size()) { // older ZeroTier versions do not send capabilities, tags, or revocations
		const unsigned int numCapabilities = at<uint16_t>(p); p += 2;
		for(unsigned int i=0;i<numCapabilities;++i) {
			p += cap.deserialize(*this,p);
			if ((!network)||(network->id() != cap.networkId()))
				network = RR->node->network(cap.networkId());
			if (network) {
				switch (network->addCredential(tPtr,cap)) {
					case Membership::ADD_REJECTED:
						break;
					case Membership::ADD_ACCEPTED_NEW:
					case Membership::ADD_ACCEPTED_REDUNDANT:
						trustEstablished = true;
						break;
					case Membership::ADD_DEFERRED_FOR_WHOIS:
						return false;
				}
			}
		}

		if (p >= size()) return true;

		const unsigned int numTags = at<uint16_t>(p); p += 2;
		for(unsigned int i=0;i<numTags;++i) {
			p += tag.deserialize(*this,p);
			if ((!network)||(network->id() != tag.networkId()))
				network = RR->node->network(tag.networkId());
			if (network) {
				switch (network->addCredential(tPtr,tag)) {
					case Membership::ADD_REJECTED:
						break;
					case Membership::ADD_ACCEPTED_NEW:
					case Membership::ADD_ACCEPTED_REDUNDANT:
						trustEstablished = true;
						break;
					case Membership::ADD_DEFERRED_FOR_WHOIS:
						return false;
				}
			}
		}

		if (p >= size()) return true;

		const unsigned int numRevocations = at<uint16_t>(p); p += 2;
		for(unsigned int i=0;i<numRevocations;++i) {
			p += revocation.deserialize(*this,p);
			if ((!network)||(network->id() != revocation.networkId()))
				network = RR->node->network(revocation.networkId());
			if (network) {
				switch(network->addCredential(tPtr,peer->address(),revocation)) {
					case Membership::ADD_REJECTED:
						break;
					case Membership::ADD_ACCEPTED_NEW:
					case Membership::ADD_ACCEPTED_REDUNDANT:
						trustEstablished = true;
						break;
					case Membership::ADD_DEFERRED_FOR_WHOIS:
						return false;
				}
			}
		}

		if (p >= size()) return true;

		const unsigned int numCoos = at<uint16_t>(p); p += 2;
		for(unsigned int i=0;i<numCoos;++i) {
			p += coo.deserialize(*this,p);
			if ((!network)||(network->id() != coo.networkId()))
				network = RR->node->network(coo.networkId());
			if (network) {
				switch(network->addCredential(tPtr,coo)) {
					case Membership::ADD_REJECTED:
						break;
					case Membership::ADD_ACCEPTED_NEW:
					case Membership::ADD_ACCEPTED_REDUNDANT:
						trustEstablished = true;
						break;
					case Membership::ADD_DEFERRED_FOR_WHOIS:
						return false;
				}
			}
		}
	}

	peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_NETWORK_CREDENTIALS,0,Packet::VERB_NOP,trustEstablished,(network) ? network->id() : 0);

	return true;
}

bool IncomingPacket::_doNETWORK_CONFIG_REQUEST(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_NETWORK_ID);
	const unsigned int hopCount = hops();
	const uint64_t requestPacketId = packetId();

	if (RR->localNetworkController) {
		const unsigned int metaDataLength = (ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT_LEN <= size()) ? at<uint16_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT_LEN) : 0;
		const char *metaDataBytes = (metaDataLength != 0) ? (const char *)field(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT,metaDataLength) : (const char *)0;
		const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> metaData(metaDataBytes,metaDataLength);
		RR->localNetworkController->request(nwid,(hopCount > 0) ? InetAddress() : _path->address(),requestPacketId,peer->identity(),metaData);
	} else {
		Packet outp(peer->address(),RR->identity.address(),Packet::VERB_ERROR);
		outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
		outp.append(requestPacketId);
		outp.append((unsigned char)Packet::ERROR_UNSUPPORTED_OPERATION);
		outp.append(nwid);
		outp.armor(peer->key(),true);
		_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
	}

	peer->received(tPtr,_path,hopCount,requestPacketId,payloadLength(),Packet::VERB_NETWORK_CONFIG_REQUEST,0,Packet::VERB_NOP,false,nwid);

	return true;
}

bool IncomingPacket::_doNETWORK_CONFIG(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	const SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PACKET_IDX_PAYLOAD)));
	if (network) {
		const uint64_t configUpdateId = network->handleConfigChunk(tPtr,packetId(),source(),*this,ZT_PACKET_IDX_PAYLOAD);
		if (configUpdateId) {
			Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
			outp.append((uint8_t)Packet::VERB_ECHO);
			outp.append((uint64_t)packetId());
			outp.append((uint64_t)network->id());
			outp.append((uint64_t)configUpdateId);
			outp.armor(peer->key(),true);
			_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
		}
	}

	peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_NETWORK_CONFIG,0,Packet::VERB_NOP,false,(network) ? network->id() : 0);

	return true;
}

bool IncomingPacket::_doMULTICAST_GATHER(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_NETWORK_ID);
	const unsigned int flags = (*this)[ZT_PROTO_VERB_MULTICAST_GATHER_IDX_FLAGS];
	const MulticastGroup mg(MAC(field(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_MAC,6),6),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_ADI));
	const unsigned int gatherLimit = at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_GATHER_LIMIT);

	const SharedPtr<Network> network(RR->node->network(nwid));

	if ((flags & 0x01) != 0) {
		try {
			CertificateOfMembership com;
			com.deserialize(*this,ZT_PROTO_VERB_MULTICAST_GATHER_IDX_COM);
			if ((com)&&(network))
				network->addCredential(tPtr,com);
		} catch ( ... ) {} // discard invalid COMs
	}

	bool trustEstablished = false;
	if (network) {
		if (network->gate(tPtr,peer)) {
			trustEstablished = true;
		} else {
			_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
			return false;
		}
	}

	const int64_t now = RR->node->now();
	if ((gatherLimit > 0)&&((trustEstablished)||(RR->topology->amUpstream())||(RR->node->localControllerHasAuthorized(now,nwid,peer->address())))) {
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
	}

	peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_MULTICAST_GATHER,0,Packet::VERB_NOP,trustEstablished,nwid);

	return true;
}

bool IncomingPacket::_doMULTICAST_FRAME(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID);
	const unsigned int flags = (*this)[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FLAGS];

	const SharedPtr<Network> network(RR->node->network(nwid));
	if (network) {
		// Offset -- size of optional fields added to position of later fields
		unsigned int offset = 0;

		if ((flags & 0x01) != 0) {
			// This is deprecated but may still be sent by old peers
			CertificateOfMembership com;
			offset += com.deserialize(*this,ZT_PROTO_VERB_MULTICAST_FRAME_IDX_COM);
			if (com)
				network->addCredential(tPtr,com);
		}

		if (!network->gate(tPtr,peer)) {
			_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
			return false;
		}

		unsigned int gatherLimit = 0;
		if ((flags & 0x02) != 0) {
			gatherLimit = at<uint32_t>(offset + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_GATHER_LIMIT);
			offset += 4;
		}

		MAC from;
		if ((flags & 0x04) != 0) {
			from.setTo(field(offset + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SOURCE_MAC,6),6);
			offset += 6;
		} else {
			from.fromAddress(peer->address(),nwid);
		}

		const MulticastGroup to(MAC(field(offset + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DEST_MAC,6),6),at<uint32_t>(offset + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DEST_ADI));
		const unsigned int etherType = at<uint16_t>(offset + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ETHERTYPE);
		const unsigned int frameLen = size() - (offset + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME);

		if (network->config().multicastLimit == 0) {
			RR->t->incomingNetworkFrameDropped(tPtr,network,_path,packetId(),size(),peer->address(),Packet::VERB_MULTICAST_FRAME,from,to.mac(),"multicast disabled");
			peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,false,nwid);
			return true;
		}

		if ((frameLen > 0)&&(frameLen <= ZT_MAX_MTU)) {
			if (!to.mac().isMulticast()) {
				RR->t->incomingPacketInvalid(tPtr,_path,packetId(),source(),hops(),Packet::VERB_MULTICAST_FRAME,"destination not multicast");
				peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,true,nwid); // trustEstablished because COM is okay
				return true;
			}
			if ((!from)||(from.isMulticast())||(from == network->mac())) {
				RR->t->incomingPacketInvalid(tPtr,_path,packetId(),source(),hops(),Packet::VERB_MULTICAST_FRAME,"invalid source MAC");
				peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,true,nwid); // trustEstablished because COM is okay
				return true;
			}

			const uint8_t *const frameData = (const uint8_t *)field(offset + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME,frameLen);

			if ((flags & 0x08)&&(network->config().isMulticastReplicator(RR->identity.address())))
				RR->mc->send(tPtr,RR->node->now(),network,peer->address(),to,from,etherType,frameData,frameLen);

			if (from != MAC(peer->address(),nwid)) {
				if (network->config().permitsBridging(peer->address())) {
					network->learnBridgeRoute(from,peer->address());
				} else {
					RR->t->incomingNetworkFrameDropped(tPtr,network,_path,packetId(),size(),peer->address(),Packet::VERB_MULTICAST_FRAME,from,to.mac(),"bridging not allowed (remote)");
					peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,true,nwid); // trustEstablished because COM is okay
					return true;
				}
			}

			if (network->filterIncomingPacket(tPtr,peer,RR->identity.address(),from,to.mac(),frameData,frameLen,etherType,0) > 0)
				RR->node->putFrame(tPtr,nwid,network->userPtr(),from,to.mac(),etherType,0,(const void *)frameData,frameLen);
		}

		if (gatherLimit) {
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
		}

		peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,true,nwid);
	} else {
		_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
		return false;
	}

	return true;
}

bool IncomingPacket::_doPUSH_DIRECT_PATHS(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	const int64_t now = RR->node->now();

	// First, subject this to a rate limit
	if (!peer->rateGatePushDirectPaths(now)) {
		peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_PUSH_DIRECT_PATHS,0,Packet::VERB_NOP,false,0);
		return true;
	}

	// Second, limit addresses by scope and type
	uint8_t countPerScope[ZT_INETADDRESS_MAX_SCOPE+1][2]; // [][0] is v4, [][1] is v6
	memset(countPerScope,0,sizeof(countPerScope));

	unsigned int count = at<uint16_t>(ZT_PACKET_IDX_PAYLOAD);
	unsigned int ptr = ZT_PACKET_IDX_PAYLOAD + 2;

	while (count--) { // if ptr overflows Buffer will throw
		// TODO: some flags are not yet implemented

		unsigned int flags = (*this)[ptr++];
		unsigned int extLen = at<uint16_t>(ptr); ptr += 2;
		ptr += extLen; // unused right now
		unsigned int addrType = (*this)[ptr++];
		unsigned int addrLen = (*this)[ptr++];

		switch(addrType) {
			case 4: {
				const InetAddress a(field(ptr,4),4,at<uint16_t>(ptr + 4));
				if (
					((flags & ZT_PUSH_DIRECT_PATHS_FLAG_FORGET_PATH) == 0) && // not being told to forget
						(!( ((flags & ZT_PUSH_DIRECT_PATHS_FLAG_CLUSTER_REDIRECT) == 0) && (peer->hasActivePathTo(now,a)) )) && // not already known
						(RR->node->shouldUsePathForZeroTierTraffic(tPtr,peer->address(),_path->localSocket(),a)) ) // should use path
				{
					if ((flags & ZT_PUSH_DIRECT_PATHS_FLAG_CLUSTER_REDIRECT) != 0) {
						peer->clusterRedirect(tPtr,_path,a,now);
					} else if (++countPerScope[(int)a.ipScope()][0] <= ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY) {
						peer->attemptToContactAt(tPtr,InetAddress(),a,now,false);
					}
				}
			}	break;
			case 6: {
				const InetAddress a(field(ptr,16),16,at<uint16_t>(ptr + 16));
				if (
					((flags & ZT_PUSH_DIRECT_PATHS_FLAG_FORGET_PATH) == 0) && // not being told to forget
						(!( ((flags & ZT_PUSH_DIRECT_PATHS_FLAG_CLUSTER_REDIRECT) == 0) && (peer->hasActivePathTo(now,a)) )) && // not already known
						(RR->node->shouldUsePathForZeroTierTraffic(tPtr,peer->address(),_path->localSocket(),a)) ) // should use path
				{
					if ((flags & ZT_PUSH_DIRECT_PATHS_FLAG_CLUSTER_REDIRECT) != 0) {
						peer->clusterRedirect(tPtr,_path,a,now);
					} else if (++countPerScope[(int)a.ipScope()][1] <= ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY) {
						peer->attemptToContactAt(tPtr,InetAddress(),a,now,false);
					}
				}
			}	break;
		}
		ptr += addrLen;
	}

	peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_PUSH_DIRECT_PATHS,0,Packet::VERB_NOP,false,0);

	return true;
}

bool IncomingPacket::_doUSER_MESSAGE(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	if (likely(size() >= (ZT_PACKET_IDX_PAYLOAD + 8))) {
		ZT_UserMessage um;
		um.origin = peer->address().toInt();
		um.typeId = at<uint64_t>(ZT_PACKET_IDX_PAYLOAD);
		um.data = reinterpret_cast<const void *>(reinterpret_cast<const uint8_t *>(data()) + ZT_PACKET_IDX_PAYLOAD + 8);
		um.length = size() - (ZT_PACKET_IDX_PAYLOAD + 8);
		RR->node->postEvent(tPtr,ZT_EVENT_USER_MESSAGE,reinterpret_cast<const void *>(&um));
	}

	peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_USER_MESSAGE,0,Packet::VERB_NOP,false,0);

	return true;
}

bool IncomingPacket::_doREMOTE_TRACE(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	ZT_RemoteTrace rt;
	const char *ptr = reinterpret_cast<const char *>(data()) + ZT_PACKET_IDX_PAYLOAD;
	const char *const eof = reinterpret_cast<const char *>(data()) + size();
	rt.origin = peer->address().toInt();
	rt.data = const_cast<char *>(ptr); // start of first string
	while (ptr < eof) {
		if (!*ptr) { // end of string
			rt.len = (unsigned int)(ptr - rt.data);
			if ((rt.len > 0)&&(rt.len <= ZT_MAX_REMOTE_TRACE_SIZE)) {
				RR->node->postEvent(tPtr,ZT_EVENT_REMOTE_TRACE,&rt);
			}
			rt.data = const_cast<char *>(++ptr); // start of next string, if any
		} else {
			++ptr;
		}
	}

	peer->received(tPtr,_path,hops(),packetId(),payloadLength(),Packet::VERB_REMOTE_TRACE,0,Packet::VERB_NOP,false,0);

	return true;
}

void IncomingPacket::_sendErrorNeedCredentials(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer,const uint64_t nwid)
{
	Packet outp(source(),RR->identity.address(),Packet::VERB_ERROR);
	outp.append((uint8_t)verb());
	outp.append(packetId());
	outp.append((uint8_t)Packet::ERROR_NEED_MEMBERSHIP_CERTIFICATE);
	outp.append(nwid);
	outp.armor(peer->key(),true);
	_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
}

} // namespace ZeroTier
