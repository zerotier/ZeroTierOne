/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include "Cluster.hpp"
#include "Node.hpp"
#include "CertificateOfMembership.hpp"
#include "CertificateOfRepresentation.hpp"
#include "Capability.hpp"
#include "Tag.hpp"
#include "Revocation.hpp"

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
			if (RR->topology->shouldInboundPathBeTrusted(_path->address(),trustedPathId())) {
				trusted = true;
				TRACE("TRUSTED PATH packet approved from %s(%s), trusted path ID %llx",sourceAddress.toString().c_str(),_path->address().toString().c_str(),trustedPathId());
			} else {
				TRACE("dropped packet from %s(%s), cipher set to trusted path mode but path %llx@%s is not trusted!",sourceAddress.toString().c_str(),_path->address().toString().c_str(),trustedPathId(),_path->address().toString().c_str());
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
					//fprintf(stderr,"dropped packet from %s(%s), MAC authentication failed (size: %u)" ZT_EOL_S,sourceAddress.toString().c_str(),_path->address().toString().c_str(),size());
					TRACE("dropped packet from %s(%s), MAC authentication failed (size: %u)",sourceAddress.toString().c_str(),_path->address().toString().c_str(),size());
					return true;
				}
			}

			if (!uncompress()) {
				//fprintf(stderr,"dropped packet from %s(%s), compressed data invalid (size %u, verb may be %u)" ZT_EOL_S,sourceAddress.toString().c_str(),_path->address().toString().c_str(),size(),(unsigned int)verb());
				TRACE("dropped packet from %s(%s), compressed data invalid (size %u, verb may be %u)",sourceAddress.toString().c_str(),_path->address().toString().c_str(),size(),(unsigned int)verb());
				return true;
			}

			const Packet::Verb v = verb();
			//TRACE("<< %s from %s(%s)",Packet::verbString(v),sourceAddress.toString().c_str(),_path->address().toString().c_str());
			switch(v) {
				//case Packet::VERB_NOP:
				default: // ignore unknown verbs, but if they pass auth check they are "received"
					peer->received(tPtr,_path,hops(),packetId(),v,0,Packet::VERB_NOP,false);
					return true;

				case Packet::VERB_HELLO:                      return _doHELLO(RR,tPtr,true);
				case Packet::VERB_ERROR:                      return _doERROR(RR,tPtr,peer);
				case Packet::VERB_OK:                         return _doOK(RR,tPtr,peer);
				case Packet::VERB_WHOIS:                      return _doWHOIS(RR,tPtr,peer);
				case Packet::VERB_RENDEZVOUS:                 return _doRENDEZVOUS(RR,tPtr,peer);
				case Packet::VERB_FRAME:                      return _doFRAME(RR,tPtr,peer);
				case Packet::VERB_EXT_FRAME:                  return _doEXT_FRAME(RR,tPtr,peer);
				case Packet::VERB_ECHO:                       return _doECHO(RR,tPtr,peer);
				case Packet::VERB_MULTICAST_LIKE:             return _doMULTICAST_LIKE(RR,tPtr,peer);
				case Packet::VERB_NETWORK_CREDENTIALS:        return _doNETWORK_CREDENTIALS(RR,tPtr,peer);
				case Packet::VERB_NETWORK_CONFIG_REQUEST:     return _doNETWORK_CONFIG_REQUEST(RR,tPtr,peer);
				case Packet::VERB_NETWORK_CONFIG:             return _doNETWORK_CONFIG(RR,tPtr,peer);
				case Packet::VERB_MULTICAST_GATHER:           return _doMULTICAST_GATHER(RR,tPtr,peer);
				case Packet::VERB_MULTICAST_FRAME:            return _doMULTICAST_FRAME(RR,tPtr,peer);
				case Packet::VERB_PUSH_DIRECT_PATHS:          return _doPUSH_DIRECT_PATHS(RR,tPtr,peer);
				case Packet::VERB_CIRCUIT_TEST:               return _doCIRCUIT_TEST(RR,tPtr,peer);
				case Packet::VERB_CIRCUIT_TEST_REPORT:        return _doCIRCUIT_TEST_REPORT(RR,tPtr,peer);
				case Packet::VERB_USER_MESSAGE:               return _doUSER_MESSAGE(RR,tPtr,peer);
			}
		} else {
			RR->sw->requestWhois(tPtr,sourceAddress);
			return false;
		}
	} catch ( ... ) {
		// Exceptions are more informatively caught in _do...() handlers but
		// this outer try/catch will catch anything else odd.
		TRACE("dropped ??? from %s(%s): unexpected exception in tryDecode()",sourceAddress.toString().c_str(),_path->address().toString().c_str());
		return true;
	}
}

bool IncomingPacket::_doERROR(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		const Packet::Verb inReVerb = (Packet::Verb)(*this)[ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB];
		const uint64_t inRePacketId = at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_IN_RE_PACKET_ID);
		const Packet::ErrorCode errorCode = (Packet::ErrorCode)(*this)[ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE];

		//TRACE("ERROR %s from %s(%s) in-re %s",Packet::errorString(errorCode),peer->address().toString().c_str(),_path->address().toString().c_str(),Packet::verbString(inReVerb));

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
				const SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)));
				const uint64_t now = RR->node->now();
				if ( (network) && (network->config().com) && (peer->rateGateIncomingComRequest(now)) )
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
				const SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)));
				if ((network)&&(network->gate(tPtr,peer))) {
					const MulticastGroup mg(MAC(field(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD + 8,6),6),at<uint32_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD + 14));
					TRACE("%.16llx: peer %s unsubscrubed from multicast group %s",network->id(),peer->address().toString().c_str(),mg.toString().c_str());
					RR->mc->remove(network->id(),mg,peer->address());
				}
			}	break;

			default: break;
		}

		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_ERROR,inRePacketId,inReVerb,false);
	} catch ( ... ) {
		TRACE("dropped ERROR from %s(%s): unexpected exception",peer->address().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doHELLO(const RuntimeEnvironment *RR,void *tPtr,const bool alreadyAuthenticated)
{
	try {
		const uint64_t now = RR->node->now();

		const uint64_t pid = packetId();
		const Address fromAddress(source());
		const unsigned int protoVersion = (*this)[ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION];
		const unsigned int vMajor = (*this)[ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION];
		const unsigned int vMinor = (*this)[ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION];
		const unsigned int vRevision = at<uint16_t>(ZT_PROTO_VERB_HELLO_IDX_REVISION);
		const uint64_t timestamp = at<uint64_t>(ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP);
		Identity id;
		unsigned int ptr = ZT_PROTO_VERB_HELLO_IDX_IDENTITY + id.deserialize(*this,ZT_PROTO_VERB_HELLO_IDX_IDENTITY);

		if (protoVersion < ZT_PROTO_VERSION_MIN) {
			TRACE("dropped HELLO from %s(%s): protocol version too old",id.address().toString().c_str(),_path->address().toString().c_str());
			return true;
		}
		if (fromAddress != id.address()) {
			TRACE("dropped HELLO from %s(%s): identity does not match packet source address",fromAddress.toString().c_str(),_path->address().toString().c_str());
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
							TRACE("rejected HELLO from %s(%s): address already claimed",id.address().toString().c_str(),_path->address().toString().c_str());
							Packet outp(id.address(),RR->identity.address(),Packet::VERB_ERROR);
							outp.append((uint8_t)Packet::VERB_HELLO);
							outp.append((uint64_t)pid);
							outp.append((uint8_t)Packet::ERROR_IDENTITY_COLLISION);
							outp.armor(key,true,_path->nextOutgoingCounter());
							_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
						} else {
							TRACE("rejected HELLO from %s(%s): packet failed authentication",id.address().toString().c_str(),_path->address().toString().c_str());
						}
					} else {
						TRACE("rejected HELLO from %s(%s): key agreement failed",id.address().toString().c_str(),_path->address().toString().c_str());
					}

					return true;
				} else {
					// Identity is the same as the one we already have -- check packet integrity

					if (!dearmor(peer->key())) {
						TRACE("rejected HELLO from %s(%s): packet failed authentication",id.address().toString().c_str(),_path->address().toString().c_str());
						return true;
					}

					// Continue at // VALID
				}
			} // else if alreadyAuthenticated then continue at // VALID
		} else {
			// We don't already have an identity with this address -- validate and learn it

			// Sanity check: this basically can't happen
			if (alreadyAuthenticated) {
				TRACE("dropped HELLO from %s(%s): somehow already authenticated with unknown peer?",id.address().toString().c_str(),_path->address().toString().c_str());
				return true;
			}

			// Check rate limits
			if (!RR->node->rateGateIdentityVerification(now,_path->address()))
				return true;

			// Check packet integrity and MAC (this is faster than locallyValidate() so do it first to filter out total crap)
			SharedPtr<Peer> newPeer(new Peer(RR,RR->identity,id));
			if (!dearmor(newPeer->key())) {
				TRACE("rejected HELLO from %s(%s): packet failed authentication",id.address().toString().c_str(),_path->address().toString().c_str());
				return true;
			}

			// Check that identity's address is valid as per the derivation function
			if (!id.locallyValidate()) {
				TRACE("dropped HELLO from %s(%s): identity invalid",id.address().toString().c_str(),_path->address().toString().c_str());
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
				RR->sa->iam(tPtr,id.address(),_path->localAddress(),_path->address(),externalSurfaceAddress,RR->topology->isUpstream(id),now);
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

			// Handle COR if present (older versions don't send this)
			if ((ptr + 2) <= size()) {
				if (at<uint16_t>(ptr) > 0) {
					CertificateOfRepresentation cor;
					ptr += 2;
					ptr += cor.deserialize(*this,ptr);
				} else ptr += 2;
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

		const unsigned int corSizeAt = outp.size();
		outp.addSize(2);
		RR->topology->appendCertificateOfRepresentation(outp);
		outp.setAt(corSizeAt,(uint16_t)(outp.size() - (corSizeAt + 2)));

		outp.armor(peer->key(),true,_path->nextOutgoingCounter());
		_path->send(RR,tPtr,outp.data(),outp.size(),now);

		peer->setRemoteVersion(protoVersion,vMajor,vMinor,vRevision); // important for this to go first so received() knows the version
		peer->received(tPtr,_path,hops(),pid,Packet::VERB_HELLO,0,Packet::VERB_NOP,false);
	} catch ( ... ) {
		TRACE("dropped HELLO from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doOK(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		const Packet::Verb inReVerb = (Packet::Verb)(*this)[ZT_PROTO_VERB_OK_IDX_IN_RE_VERB];
		const uint64_t inRePacketId = at<uint64_t>(ZT_PROTO_VERB_OK_IDX_IN_RE_PACKET_ID);

		if (!RR->node->expectingReplyTo(inRePacketId)) {
			TRACE("%s(%s): OK(%s) DROPPED: not expecting reply to %.16llx",peer->address().toString().c_str(),_path->address().toString().c_str(),Packet::verbString(inReVerb),packetId());
			return true;
		}

		//TRACE("%s(%s): OK(%s)",peer->address().toString().c_str(),_path->address().toString().c_str(),Packet::verbString(inReVerb));

		switch(inReVerb) {

			case Packet::VERB_HELLO: {
				const uint64_t latency = RR->node->now() - at<uint64_t>(ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP);
				if (latency > ZT_HELLO_MAX_ALLOWABLE_LATENCY)
					return true;

				const unsigned int vProto = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_PROTOCOL_VERSION];
				const unsigned int vMajor = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_MAJOR_VERSION];
				const unsigned int vMinor = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_MINOR_VERSION];
				const unsigned int vRevision = at<uint16_t>(ZT_PROTO_VERB_HELLO__OK__IDX_REVISION);

				if (vProto < ZT_PROTO_VERSION_MIN) {
					TRACE("%s(%s): OK(HELLO) dropped, protocol version too old",source().toString().c_str(),_path->address().toString().c_str());
					return true;
				}

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

				// Handle certificate of representation if present
				if ((ptr + 2) <= size()) {
					if (at<uint16_t>(ptr) > 0) {
						CertificateOfRepresentation cor;
						ptr += 2;
						ptr += cor.deserialize(*this,ptr);
					} else ptr += 2;
				}

#ifdef ZT_TRACE
				const std::string tmp1(source().toString());
				const std::string tmp2(_path->address().toString());
				TRACE("%s(%s): OK(HELLO), version %u.%u.%u, latency %u",tmp1.c_str(),tmp2.c_str(),vMajor,vMinor,vRevision,latency);
#endif

				if (!hops())
					peer->addDirectLatencyMeasurment((unsigned int)latency);
				peer->setRemoteVersion(vProto,vMajor,vMinor,vRevision);

				if ((externalSurfaceAddress)&&(hops() == 0))
					RR->sa->iam(tPtr,peer->address(),_path->localAddress(),_path->address(),externalSurfaceAddress,RR->topology->isUpstream(peer->identity()),RR->node->now());
			}	break;

			case Packet::VERB_WHOIS:
				if (RR->topology->isUpstream(peer->identity())) {
					const Identity id(*this,ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY);
					RR->sw->doAnythingWaitingForPeer(tPtr,RR->topology->addPeer(tPtr,SharedPtr<Peer>(new Peer(RR,RR->identity,id))));
				}
				break;

			case Packet::VERB_NETWORK_CONFIG_REQUEST: {
				const SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_OK_IDX_PAYLOAD)));
				if (network)
					network->handleConfigChunk(tPtr,packetId(),source(),*this,ZT_PROTO_VERB_OK_IDX_PAYLOAD);
			}	break;

			case Packet::VERB_MULTICAST_GATHER: {
				const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_NETWORK_ID);
				const SharedPtr<Network> network(RR->node->network(nwid));
				if (network) {
					const MulticastGroup mg(MAC(field(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_MAC,6),6),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_ADI));
					//TRACE("%s(%s): OK(MULTICAST_GATHER) %.16llx/%s length %u",source().toString().c_str(),_path->address().toString().c_str(),nwid,mg.toString().c_str(),size());
					const unsigned int count = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_GATHER_RESULTS + 4);
					RR->mc->addMultiple(tPtr,RR->node->now(),nwid,mg,field(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_GATHER_RESULTS + 6,count * 5),count,at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_GATHER_RESULTS));
				}
			}	break;

			case Packet::VERB_MULTICAST_FRAME: {
				const unsigned int flags = (*this)[ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_FLAGS];
				const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_NETWORK_ID);
				const MulticastGroup mg(MAC(field(ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_MAC,6),6),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_ADI));

				//TRACE("%s(%s): OK(MULTICAST_FRAME) %.16llx/%s flags %.2x",peer->address().toString().c_str(),_path->address().toString().c_str(),nwid,mg.toString().c_str(),flags);

				const SharedPtr<Network> network(RR->node->network(nwid));
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
						RR->mc->addMultiple(tPtr,RR->node->now(),nwid,mg,field(offset,count * 5),count,totalKnown);
					}
				}
			}	break;

			default: break;
		}

		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_OK,inRePacketId,inReVerb,false);
	} catch ( ... ) {
		TRACE("dropped OK from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doWHOIS(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		if ((!RR->topology->amRoot())&&(!peer->rateGateInboundWhoisRequest(RR->node->now()))) {
			TRACE("dropped WHOIS from %s(%s): rate limit circuit breaker tripped",source().toString().c_str(),_path->address().toString().c_str());
			return true;
		}

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
				RR->sw->requestWhois(tPtr,addr);
#ifdef ZT_ENABLE_CLUSTER
				// Distribute WHOIS queries across a cluster if we do not know the ID.
				// This may result in duplicate OKs to the querying peer, which is fine.
				if (RR->cluster)
					RR->cluster->sendDistributedQuery(*this);
#endif
			}
		}

		if (count > 0) {
			outp.armor(peer->key(),true,_path->nextOutgoingCounter());
			_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
		}

		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_WHOIS,0,Packet::VERB_NOP,false);
	} catch ( ... ) {
		TRACE("dropped WHOIS from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doRENDEZVOUS(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		if (!RR->topology->isUpstream(peer->identity())) {
			TRACE("RENDEZVOUS from %s ignored since source is not upstream",peer->address().toString().c_str());
		} else {
			const Address with(field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ZTADDRESS,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
			const SharedPtr<Peer> rendezvousWith(RR->topology->getPeer(tPtr,with));
			if (rendezvousWith) {
				const unsigned int port = at<uint16_t>(ZT_PROTO_VERB_RENDEZVOUS_IDX_PORT);
				const unsigned int addrlen = (*this)[ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRLEN];
				if ((port > 0)&&((addrlen == 4)||(addrlen == 16))) {
					const InetAddress atAddr(field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRESS,addrlen),addrlen,port);
					if (RR->node->shouldUsePathForZeroTierTraffic(tPtr,with,_path->localAddress(),atAddr)) {
						RR->node->putPacket(tPtr,_path->localAddress(),atAddr,"ABRE",4,2); // send low-TTL junk packet to 'open' local NAT(s) and stateful firewalls
						rendezvousWith->attemptToContactAt(tPtr,_path->localAddress(),atAddr,RR->node->now(),false,0);
						TRACE("RENDEZVOUS from %s says %s might be at %s, sent verification attempt",peer->address().toString().c_str(),with.toString().c_str(),atAddr.toString().c_str());
					} else {
						TRACE("RENDEZVOUS from %s says %s might be at %s, ignoring since path is not suitable",peer->address().toString().c_str(),with.toString().c_str(),atAddr.toString().c_str());
					}
				} else {
					TRACE("dropped corrupt RENDEZVOUS from %s(%s) (bad address or port)",peer->address().toString().c_str(),_path->address().toString().c_str());
				}
			} else {
				TRACE("ignored RENDEZVOUS from %s(%s) to meet unknown peer %s",peer->address().toString().c_str(),_path->address().toString().c_str(),with.toString().c_str());
			}
		}
		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_RENDEZVOUS,0,Packet::VERB_NOP,false);
	} catch ( ... ) {
		TRACE("dropped RENDEZVOUS from %s(%s): unexpected exception",peer->address().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doFRAME(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
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
				TRACE("dropped FRAME from %s(%s): not a member of private network %.16llx",peer->address().toString().c_str(),_path->address().toString().c_str(),(unsigned long long)network->id());
				_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
			}
		} else {
			TRACE("dropped FRAME from %s(%s): we are not a member of network %.16llx",source().toString().c_str(),_path->address().toString().c_str(),at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID));
			_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
		}
		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_FRAME,0,Packet::VERB_NOP,trustEstablished);
	} catch ( ... ) {
		TRACE("dropped FRAME from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doEXT_FRAME(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
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
				TRACE("dropped EXT_FRAME from %s(%s): not a member of private network %.16llx",peer->address().toString().c_str(),_path->address().toString().c_str(),network->id());
				_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
				peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,false);
				return true;
			}

			if (size() > ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD) {
				const unsigned int etherType = at<uint16_t>(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_ETHERTYPE);
				const MAC to(field(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_TO,ZT_PROTO_VERB_EXT_FRAME_LEN_TO),ZT_PROTO_VERB_EXT_FRAME_LEN_TO);
				const MAC from(field(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_FROM,ZT_PROTO_VERB_EXT_FRAME_LEN_FROM),ZT_PROTO_VERB_EXT_FRAME_LEN_FROM);
				const unsigned int frameLen = size() - (comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD);
				const uint8_t *const frameData = (const uint8_t *)field(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD,frameLen);

				if ((!from)||(from.isMulticast())||(from == network->mac())) {
					TRACE("dropped EXT_FRAME from %s@%s(%s) to %s: invalid source MAC %s",from.toString().c_str(),peer->address().toString().c_str(),_path->address().toString().c_str(),to.toString().c_str(),from.toString().c_str());
					peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,true); // trustEstablished because COM is okay
					return true;
				}

				switch (network->filterIncomingPacket(tPtr,peer,RR->identity.address(),from,to,frameData,frameLen,etherType,0)) {
					case 1:
						if (from != MAC(peer->address(),nwid)) {
							if (network->config().permitsBridging(peer->address())) {
								network->learnBridgeRoute(from,peer->address());
							} else {
								TRACE("dropped EXT_FRAME from %s@%s(%s) to %s: sender not allowed to bridge into %.16llx",from.toString().c_str(),peer->address().toString().c_str(),_path->address().toString().c_str(),to.toString().c_str(),network->id());
								peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,true); // trustEstablished because COM is okay
								return true;
							}
						} else if (to != network->mac()) {
							if (to.isMulticast()) {
								if (network->config().multicastLimit == 0) {
									TRACE("dropped EXT_FRAME from %s@%s(%s) to %s: network %.16llx does not allow multicast",from.toString().c_str(),peer->address().toString().c_str(),_path->address().toString().c_str(),to.toString().c_str(),network->id());
									peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,true); // trustEstablished because COM is okay
									return true;
								}
							} else if (!network->config().permitsBridging(RR->identity.address())) {
								TRACE("dropped EXT_FRAME from %s@%s(%s) to %s: I cannot bridge to %.16llx or bridging disabled on network",from.toString().c_str(),peer->address().toString().c_str(),_path->address().toString().c_str(),to.toString().c_str(),network->id());
								peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,true); // trustEstablished because COM is okay
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
				outp.armor(peer->key(),true,_path->nextOutgoingCounter());
				_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
			}

			peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,true);
		} else {
			TRACE("dropped EXT_FRAME from %s(%s): we are not connected to network %.16llx",source().toString().c_str(),_path->address().toString().c_str(),at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID));
			_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
			peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP,false);
		}
	} catch ( ... ) {
		TRACE("dropped EXT_FRAME from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doECHO(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		if (!peer->rateGateEchoRequest(RR->node->now())) {
			TRACE("dropped ECHO from %s(%s): rate limit circuit breaker tripped",source().toString().c_str(),_path->address().toString().c_str());
			return true;
		}

		const uint64_t pid = packetId();
		Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
		outp.append((unsigned char)Packet::VERB_ECHO);
		outp.append((uint64_t)pid);
		if (size() > ZT_PACKET_IDX_PAYLOAD)
			outp.append(reinterpret_cast<const unsigned char *>(data()) + ZT_PACKET_IDX_PAYLOAD,size() - ZT_PACKET_IDX_PAYLOAD);
		outp.armor(peer->key(),true,_path->nextOutgoingCounter());
		_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());

		peer->received(tPtr,_path,hops(),pid,Packet::VERB_ECHO,0,Packet::VERB_NOP,false);
	} catch ( ... ) {
		TRACE("dropped ECHO from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doMULTICAST_LIKE(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		const uint64_t now = RR->node->now();

		uint64_t authOnNetwork[256]; // cache for approved network IDs
		unsigned int authOnNetworkCount = 0;
		SharedPtr<Network> network;
		bool trustEstablished = false;

		// Iterate through 18-byte network,MAC,ADI tuples
		for(unsigned int ptr=ZT_PACKET_IDX_PAYLOAD;ptr<size();ptr+=18) {
			const uint64_t nwid = at<uint64_t>(ptr);

			bool auth = false;
			for(unsigned int i=0;i<authOnNetworkCount;++i) {
				if (nwid == authOnNetwork[i]) {
					auth = true;
					break;
				}
			}
			if (!auth) {
				if ((!network)||(network->id() != nwid))
					network = RR->node->network(nwid);
				const bool authOnNet = ((network)&&(network->gate(tPtr,peer)));
				if (!authOnNet)
					_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
				trustEstablished |= authOnNet;
				if (authOnNet||RR->mc->cacheAuthorized(peer->address(),nwid,now)) {
					auth = true;
					if (authOnNetworkCount < 256) // sanity check, packets can't really be this big
						authOnNetwork[authOnNetworkCount++] = nwid;
				}
			}

			if (auth) {
				const MulticastGroup group(MAC(field(ptr + 8,6),6),at<uint32_t>(ptr + 14));
				RR->mc->add(tPtr,now,nwid,group,peer->address());
			}
		}

		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_MULTICAST_LIKE,0,Packet::VERB_NOP,trustEstablished);
	} catch ( ... ) {
		TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doNETWORK_CREDENTIALS(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		if (!peer->rateGateCredentialsReceived(RR->node->now())) {
			TRACE("dropped NETWORK_CREDENTIALS from %s(%s): rate limit circuit breaker tripped",source().toString().c_str(),_path->address().toString().c_str());
			return true;
		}

		CertificateOfMembership com;
		Capability cap;
		Tag tag;
		Revocation revocation;
		CertificateOfOwnership coo;
		bool trustEstablished = false;

		unsigned int p = ZT_PACKET_IDX_PAYLOAD;
		while ((p < size())&&((*this)[p] != 0)) {
			p += com.deserialize(*this,p);
			if (com) {
				const SharedPtr<Network> network(RR->node->network(com.networkId()));
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
				} else RR->mc->addCredential(tPtr,com,false);
			}
		}
		++p; // skip trailing 0 after COMs if present

		if (p < size()) { // older ZeroTier versions do not send capabilities, tags, or revocations
			const unsigned int numCapabilities = at<uint16_t>(p); p += 2;
			for(unsigned int i=0;i<numCapabilities;++i) {
				p += cap.deserialize(*this,p);
				const SharedPtr<Network> network(RR->node->network(cap.networkId()));
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
				const SharedPtr<Network> network(RR->node->network(tag.networkId()));
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
				const SharedPtr<Network> network(RR->node->network(revocation.networkId()));
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
				const SharedPtr<Network> network(RR->node->network(coo.networkId()));
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

		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_NETWORK_CREDENTIALS,0,Packet::VERB_NOP,trustEstablished);
	} catch (std::exception &exc) {
		//fprintf(stderr,"dropped NETWORK_CREDENTIALS from %s(%s): %s" ZT_EOL_S,source().toString().c_str(),_path->address().toString().c_str(),exc.what());
		TRACE("dropped NETWORK_CREDENTIALS from %s(%s): %s",source().toString().c_str(),_path->address().toString().c_str(),exc.what());
	} catch ( ... ) {
		//fprintf(stderr,"dropped NETWORK_CREDENTIALS from %s(%s): unknown exception" ZT_EOL_S,source().toString().c_str(),_path->address().toString().c_str());
		TRACE("dropped NETWORK_CREDENTIALS from %s(%s): unknown exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doNETWORK_CONFIG_REQUEST(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
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
			outp.armor(peer->key(),true,_path->nextOutgoingCounter());
			_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
		}

		peer->received(tPtr,_path,hopCount,requestPacketId,Packet::VERB_NETWORK_CONFIG_REQUEST,0,Packet::VERB_NOP,false);
	} catch (std::exception &exc) {
		//fprintf(stderr,"dropped NETWORK_CONFIG_REQUEST from %s(%s): %s" ZT_EOL_S,source().toString().c_str(),_path->address().toString().c_str(),exc.what());
		TRACE("dropped NETWORK_CONFIG_REQUEST from %s(%s): %s",source().toString().c_str(),_path->address().toString().c_str(),exc.what());
	} catch ( ... ) {
		//fprintf(stderr,"dropped NETWORK_CONFIG_REQUEST from %s(%s): unknown exception" ZT_EOL_S,source().toString().c_str(),_path->address().toString().c_str());
		TRACE("dropped NETWORK_CONFIG_REQUEST from %s(%s): unknown exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doNETWORK_CONFIG(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		const SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PACKET_IDX_PAYLOAD)));
		if (network) {
			const uint64_t configUpdateId = network->handleConfigChunk(tPtr,packetId(),source(),*this,ZT_PACKET_IDX_PAYLOAD);
			if (configUpdateId) {
				Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
				outp.append((uint8_t)Packet::VERB_ECHO);
				outp.append((uint64_t)packetId());
				outp.append((uint64_t)network->id());
				outp.append((uint64_t)configUpdateId);
				outp.armor(peer->key(),true,_path->nextOutgoingCounter());
				_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
			}
		}
		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_NETWORK_CONFIG,0,Packet::VERB_NOP,false);
	} catch ( ... ) {
		TRACE("dropped NETWORK_CONFIG_REFRESH from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doMULTICAST_GATHER(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_NETWORK_ID);
		const unsigned int flags = (*this)[ZT_PROTO_VERB_MULTICAST_GATHER_IDX_FLAGS];
		const MulticastGroup mg(MAC(field(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_MAC,6),6),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_ADI));
		const unsigned int gatherLimit = at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_GATHER_LIMIT);

		//TRACE("<<MC %s(%s) GATHER up to %u in %.16llx/%s",source().toString().c_str(),_path->address().toString().c_str(),gatherLimit,nwid,mg.toString().c_str());

		const SharedPtr<Network> network(RR->node->network(nwid));

		if ((flags & 0x01) != 0) {
			try {
				CertificateOfMembership com;
				com.deserialize(*this,ZT_PROTO_VERB_MULTICAST_GATHER_IDX_COM);
				if (com) {
					if (network)
						network->addCredential(tPtr,com);
					else RR->mc->addCredential(tPtr,com,false);
				}
			} catch ( ... ) {
				TRACE("MULTICAST_GATHER from %s(%s): discarded invalid COM",peer->address().toString().c_str(),_path->address().toString().c_str());
			}
		}

		const bool trustEstablished = ((network)&&(network->gate(tPtr,peer)));
		if (!trustEstablished)
			_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
		if ( ( trustEstablished || RR->mc->cacheAuthorized(peer->address(),nwid,RR->node->now()) ) && (gatherLimit > 0) ) {
			Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
			outp.append((unsigned char)Packet::VERB_MULTICAST_GATHER);
			outp.append(packetId());
			outp.append(nwid);
			mg.mac().appendTo(outp);
			outp.append((uint32_t)mg.adi());
			const unsigned int gatheredLocally = RR->mc->gather(peer->address(),nwid,mg,outp,gatherLimit);
			if (gatheredLocally > 0) {
				outp.armor(peer->key(),true,_path->nextOutgoingCounter());
				_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
			}

			// If we are a member of a cluster, distribute this GATHER across it
#ifdef ZT_ENABLE_CLUSTER
			if ((RR->cluster)&&(gatheredLocally < gatherLimit))
				RR->cluster->sendDistributedQuery(*this);
#endif
		}

		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_MULTICAST_GATHER,0,Packet::VERB_NOP,trustEstablished);
	} catch ( ... ) {
		TRACE("dropped MULTICAST_GATHER from %s(%s): unexpected exception",peer->address().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doMULTICAST_FRAME(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
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
				TRACE("dropped MULTICAST_FRAME from %s(%s): not a member of private network %.16llx",peer->address().toString().c_str(),_path->address().toString().c_str(),(unsigned long long)network->id());
				_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
				peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,false);
				return true;
			}

			if (network->config().multicastLimit == 0) {
				TRACE("dropped MULTICAST_FRAME from %s(%s): network %.16llx does not allow multicast",peer->address().toString().c_str(),_path->address().toString().c_str(),(unsigned long long)network->id());
				peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,false);
				return true;
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

			//TRACE("<<MC FRAME %.16llx/%s from %s@%s flags %.2x length %u",nwid,to.toString().c_str(),from.toString().c_str(),peer->address().toString().c_str(),flags,frameLen);

			if ((frameLen > 0)&&(frameLen <= ZT_IF_MTU)) {
				if (!to.mac().isMulticast()) {
					TRACE("dropped MULTICAST_FRAME from %s@%s(%s) to %s: destination is unicast, must use FRAME or EXT_FRAME",from.toString().c_str(),peer->address().toString().c_str(),_path->address().toString().c_str(),to.toString().c_str());
					peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,true); // trustEstablished because COM is okay
					return true;
				}
				if ((!from)||(from.isMulticast())||(from == network->mac())) {
					TRACE("dropped MULTICAST_FRAME from %s@%s(%s) to %s: invalid source MAC",from.toString().c_str(),peer->address().toString().c_str(),_path->address().toString().c_str(),to.toString().c_str());
					peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,true); // trustEstablished because COM is okay
					return true;
				}

				if (from != MAC(peer->address(),nwid)) {
					if (network->config().permitsBridging(peer->address())) {
						network->learnBridgeRoute(from,peer->address());
					} else {
						TRACE("dropped MULTICAST_FRAME from %s@%s(%s) to %s: sender not allowed to bridge into %.16llx",from.toString().c_str(),peer->address().toString().c_str(),_path->address().toString().c_str(),to.toString().c_str(),network->id());
						peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,true); // trustEstablished because COM is okay
						return true;
					}
				}

				const uint8_t *const frameData = (const uint8_t *)field(offset + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME,frameLen);
				if (network->filterIncomingPacket(tPtr,peer,RR->identity.address(),from,to.mac(),frameData,frameLen,etherType,0) > 0) {
					RR->node->putFrame(tPtr,nwid,network->userPtr(),from,to.mac(),etherType,0,(const void *)frameData,frameLen);
				}
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
					outp.armor(peer->key(),true,_path->nextOutgoingCounter());
					_path->send(RR,tPtr,outp.data(),outp.size(),RR->node->now());
				}
			}

			peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,true);
		} else {
			_sendErrorNeedCredentials(RR,tPtr,peer,nwid);
			peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,false);
		}
	} catch ( ... ) {
		TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doPUSH_DIRECT_PATHS(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		const uint64_t now = RR->node->now();

		// First, subject this to a rate limit
		if (!peer->rateGatePushDirectPaths(now)) {
			TRACE("dropped PUSH_DIRECT_PATHS from %s(%s): circuit breaker tripped",source().toString().c_str(),_path->address().toString().c_str());
			peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_PUSH_DIRECT_PATHS,0,Packet::VERB_NOP,false);
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
							(RR->node->shouldUsePathForZeroTierTraffic(tPtr,peer->address(),_path->localAddress(),a)) ) // should use path
					{
						if ((flags & ZT_PUSH_DIRECT_PATHS_FLAG_CLUSTER_REDIRECT) != 0)
							peer->setClusterPreferred(a);
						if (++countPerScope[(int)a.ipScope()][0] <= ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY) {
							TRACE("attempting to contact %s at pushed direct path %s",peer->address().toString().c_str(),a.toString().c_str());
							peer->attemptToContactAt(tPtr,InetAddress(),a,now,false,0);
						} else {
							TRACE("ignoring contact for %s at %s -- too many per scope",peer->address().toString().c_str(),a.toString().c_str());
						}
					}
				}	break;
				case 6: {
					const InetAddress a(field(ptr,16),16,at<uint16_t>(ptr + 16));
					if (
					    ((flags & ZT_PUSH_DIRECT_PATHS_FLAG_FORGET_PATH) == 0) && // not being told to forget
							(!( ((flags & ZT_PUSH_DIRECT_PATHS_FLAG_CLUSTER_REDIRECT) == 0) && (peer->hasActivePathTo(now,a)) )) && // not already known
							(RR->node->shouldUsePathForZeroTierTraffic(tPtr,peer->address(),_path->localAddress(),a)) ) // should use path
					{
						if ((flags & ZT_PUSH_DIRECT_PATHS_FLAG_CLUSTER_REDIRECT) != 0)
							peer->setClusterPreferred(a);
						if (++countPerScope[(int)a.ipScope()][1] <= ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY) {
							TRACE("attempting to contact %s at pushed direct path %s",peer->address().toString().c_str(),a.toString().c_str());
							peer->attemptToContactAt(tPtr,InetAddress(),a,now,false,0);
						} else {
							TRACE("ignoring contact for %s at %s -- too many per scope",peer->address().toString().c_str(),a.toString().c_str());
						}
					}
				}	break;
			}
			ptr += addrLen;
		}

		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_PUSH_DIRECT_PATHS,0,Packet::VERB_NOP,false);
	} catch ( ... ) {
		TRACE("dropped PUSH_DIRECT_PATHS from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doCIRCUIT_TEST(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		const Address originatorAddress(field(ZT_PACKET_IDX_PAYLOAD,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
		SharedPtr<Peer> originator(RR->topology->getPeer(tPtr,originatorAddress));
		if (!originator) {
			RR->sw->requestWhois(tPtr,originatorAddress);
			return false;
		}

		const unsigned int flags = at<uint16_t>(ZT_PACKET_IDX_PAYLOAD + 5);
		const uint64_t timestamp = at<uint64_t>(ZT_PACKET_IDX_PAYLOAD + 7);
		const uint64_t testId = at<uint64_t>(ZT_PACKET_IDX_PAYLOAD + 15);

		// Tracks total length of variable length fields, initialized to originator credential length below
		unsigned int vlf;

		// Originator credentials -- right now only a network ID for which the originator is controller or is authorized by controller is allowed
		const unsigned int originatorCredentialLength = vlf = at<uint16_t>(ZT_PACKET_IDX_PAYLOAD + 23);
		uint64_t originatorCredentialNetworkId = 0;
		if (originatorCredentialLength >= 1) {
			switch((*this)[ZT_PACKET_IDX_PAYLOAD + 25]) {
				case 0x01: { // 64-bit network ID, originator must be controller
					if (originatorCredentialLength >= 9)
						originatorCredentialNetworkId = at<uint64_t>(ZT_PACKET_IDX_PAYLOAD + 26);
				}	break;
				default: break;
			}
		}

		// Add length of "additional fields," which are currently unused
		vlf += at<uint16_t>(ZT_PACKET_IDX_PAYLOAD + 25 + vlf);

		// Verify signature -- only tests signed by their originators are allowed
		const unsigned int signatureLength = at<uint16_t>(ZT_PACKET_IDX_PAYLOAD + 27 + vlf);
		if (!originator->identity().verify(field(ZT_PACKET_IDX_PAYLOAD,27 + vlf),27 + vlf,field(ZT_PACKET_IDX_PAYLOAD + 29 + vlf,signatureLength),signatureLength)) {
			TRACE("dropped CIRCUIT_TEST from %s(%s): signature by originator %s invalid",source().toString().c_str(),_path->address().toString().c_str(),originatorAddress.toString().c_str());
			peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_CIRCUIT_TEST,0,Packet::VERB_NOP,false);
			return true;
		}
		vlf += signatureLength;

		// Save this length so we can copy the immutable parts of this test
		// into the one we send along to next hops.
		const unsigned int lengthOfSignedPortionAndSignature = 29 + vlf;

		// Add length of second "additional fields" section.
		vlf += at<uint16_t>(ZT_PACKET_IDX_PAYLOAD + 29 + vlf);

		uint64_t reportFlags = 0;

		// Check credentials (signature already verified)
		if (originatorCredentialNetworkId) {
			SharedPtr<Network> network(RR->node->network(originatorCredentialNetworkId));
			if ((!network)||(!network->config().circuitTestingAllowed(originatorAddress))) {
				TRACE("dropped CIRCUIT_TEST from %s(%s): originator %s specified network ID %.16llx as credential, and we don't belong to that network or originator is not allowed'",source().toString().c_str(),_path->address().toString().c_str(),originatorAddress.toString().c_str(),originatorCredentialNetworkId);
				peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_CIRCUIT_TEST,0,Packet::VERB_NOP,false);
				return true;
			}
			if (network->gate(tPtr,peer))
				reportFlags |= ZT_CIRCUIT_TEST_REPORT_FLAGS_UPSTREAM_AUTHORIZED_IN_PATH;
		} else {
			TRACE("dropped CIRCUIT_TEST from %s(%s): originator %s did not specify a credential or credential type",source().toString().c_str(),_path->address().toString().c_str(),originatorAddress.toString().c_str());
			peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_CIRCUIT_TEST,0,Packet::VERB_NOP,false);
			return true;
		}

		const uint64_t now = RR->node->now();

		unsigned int breadth = 0;
		Address nextHop[256]; // breadth is a uin8_t, so this is the max
		InetAddress nextHopBestPathAddress[256];
		unsigned int remainingHopsPtr = ZT_PACKET_IDX_PAYLOAD + 33 + vlf;
		if ((ZT_PACKET_IDX_PAYLOAD + 31 + vlf) < size()) {
			// unsigned int nextHopFlags = (*this)[ZT_PACKET_IDX_PAYLOAD + 31 + vlf]
			breadth = (*this)[ZT_PACKET_IDX_PAYLOAD + 32 + vlf];
			for(unsigned int h=0;h<breadth;++h) {
				nextHop[h].setTo(field(remainingHopsPtr,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
				remainingHopsPtr += ZT_ADDRESS_LENGTH;
				SharedPtr<Peer> nhp(RR->topology->getPeer(tPtr,nextHop[h]));
				if (nhp) {
					SharedPtr<Path> nhbp(nhp->getBestPath(now,false));
					if ((nhbp)&&(nhbp->alive(now)))
						nextHopBestPathAddress[h] = nhbp->address();
				}
			}
		}

		// Report back to originator, depending on flags and whether we are last hop
		if ( ((flags & 0x01) != 0) || ((breadth == 0)&&((flags & 0x02) != 0)) ) {
			Packet outp(originatorAddress,RR->identity.address(),Packet::VERB_CIRCUIT_TEST_REPORT);
			outp.append((uint64_t)timestamp);
			outp.append((uint64_t)testId);
			outp.append((uint64_t)0); // field reserved for future use
			outp.append((uint8_t)ZT_VENDOR_ZEROTIER);
			outp.append((uint8_t)ZT_PROTO_VERSION);
			outp.append((uint8_t)ZEROTIER_ONE_VERSION_MAJOR);
			outp.append((uint8_t)ZEROTIER_ONE_VERSION_MINOR);
			outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
			outp.append((uint16_t)ZT_PLATFORM_UNSPECIFIED);
			outp.append((uint16_t)ZT_ARCHITECTURE_UNSPECIFIED);
			outp.append((uint16_t)0); // error code, currently unused
			outp.append((uint64_t)reportFlags);
			outp.append((uint64_t)packetId());
			peer->address().appendTo(outp);
			outp.append((uint8_t)hops());
			_path->localAddress().serialize(outp);
			_path->address().serialize(outp);
			outp.append((uint16_t)_path->linkQuality());
			outp.append((uint8_t)breadth);
			for(unsigned int h=0;h<breadth;++h) {
				nextHop[h].appendTo(outp);
				nextHopBestPathAddress[h].serialize(outp); // appends 0 if null InetAddress
			}
			RR->sw->send(tPtr,outp,true);
		}

		// If there are next hops, forward the test along through the graph
		if (breadth > 0) {
			Packet outp(Address(),RR->identity.address(),Packet::VERB_CIRCUIT_TEST);
			outp.append(field(ZT_PACKET_IDX_PAYLOAD,lengthOfSignedPortionAndSignature),lengthOfSignedPortionAndSignature);
			outp.append((uint16_t)0); // no additional fields
			if (remainingHopsPtr < size())
				outp.append(field(remainingHopsPtr,size() - remainingHopsPtr),size() - remainingHopsPtr);

			for(unsigned int h=0;h<breadth;++h) {
				if (RR->identity.address() != nextHop[h]) { // next hops that loop back to the current hop are not valid
					outp.newInitializationVector();
					outp.setDestination(nextHop[h]);
					RR->sw->send(tPtr,outp,true);
				}
			}
		}

		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_CIRCUIT_TEST,0,Packet::VERB_NOP,false);
	} catch ( ... ) {
		TRACE("dropped CIRCUIT_TEST from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doCIRCUIT_TEST_REPORT(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		ZT_CircuitTestReport report;
		memset(&report,0,sizeof(report));

		report.current = peer->address().toInt();
		report.upstream = Address(field(ZT_PACKET_IDX_PAYLOAD + 52,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH).toInt();
		report.testId = at<uint64_t>(ZT_PACKET_IDX_PAYLOAD + 8);
		report.timestamp = at<uint64_t>(ZT_PACKET_IDX_PAYLOAD);
		report.sourcePacketId = at<uint64_t>(ZT_PACKET_IDX_PAYLOAD + 44);
		report.flags = at<uint64_t>(ZT_PACKET_IDX_PAYLOAD + 36);
		report.sourcePacketHopCount = (*this)[ZT_PACKET_IDX_PAYLOAD + 57]; // end of fixed length headers: 58
		report.errorCode = at<uint16_t>(ZT_PACKET_IDX_PAYLOAD + 34);
		report.vendor = (enum ZT_Vendor)((*this)[ZT_PACKET_IDX_PAYLOAD + 24]);
		report.protocolVersion = (*this)[ZT_PACKET_IDX_PAYLOAD + 25];
		report.majorVersion = (*this)[ZT_PACKET_IDX_PAYLOAD + 26];
		report.minorVersion = (*this)[ZT_PACKET_IDX_PAYLOAD + 27];
		report.revision = at<uint16_t>(ZT_PACKET_IDX_PAYLOAD + 28);
		report.platform = (enum ZT_Platform)at<uint16_t>(ZT_PACKET_IDX_PAYLOAD + 30);
		report.architecture = (enum ZT_Architecture)at<uint16_t>(ZT_PACKET_IDX_PAYLOAD + 32);

		const unsigned int receivedOnLocalAddressLen = reinterpret_cast<InetAddress *>(&(report.receivedOnLocalAddress))->deserialize(*this,ZT_PACKET_IDX_PAYLOAD + 58);
		const unsigned int receivedFromRemoteAddressLen = reinterpret_cast<InetAddress *>(&(report.receivedFromRemoteAddress))->deserialize(*this,ZT_PACKET_IDX_PAYLOAD + 58 + receivedOnLocalAddressLen);
		unsigned int ptr = ZT_PACKET_IDX_PAYLOAD + 58 + receivedOnLocalAddressLen + receivedFromRemoteAddressLen;
		if (report.protocolVersion >= 9) {
			report.receivedFromLinkQuality = at<uint16_t>(ptr); ptr += 2;
		} else {
			report.receivedFromLinkQuality = ZT_PATH_LINK_QUALITY_MAX;
			ptr += at<uint16_t>(ptr) + 2; // this field was once an 'extended field length' reserved field, which was always set to 0
		}

		report.nextHopCount = (*this)[ptr++];
		if (report.nextHopCount > ZT_CIRCUIT_TEST_MAX_HOP_BREADTH) // sanity check, shouldn't be possible
			report.nextHopCount = ZT_CIRCUIT_TEST_MAX_HOP_BREADTH;
		for(unsigned int h=0;h<report.nextHopCount;++h) {
			report.nextHops[h].address = Address(field(ptr,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH).toInt(); ptr += ZT_ADDRESS_LENGTH;
			ptr += reinterpret_cast<InetAddress *>(&(report.nextHops[h].physicalAddress))->deserialize(*this,ptr);
		}

		RR->node->postCircuitTestReport(&report);

		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_CIRCUIT_TEST_REPORT,0,Packet::VERB_NOP,false);
	} catch ( ... ) {
		TRACE("dropped CIRCUIT_TEST_REPORT from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doUSER_MESSAGE(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		if (size() >= (ZT_PACKET_IDX_PAYLOAD + 8)) {
			ZT_UserMessage um;
			um.origin = peer->address().toInt();
			um.typeId = at<uint64_t>(ZT_PACKET_IDX_PAYLOAD);
			um.data = reinterpret_cast<const void *>(reinterpret_cast<const uint8_t *>(data()) + ZT_PACKET_IDX_PAYLOAD + 8);
			um.length = size() - (ZT_PACKET_IDX_PAYLOAD + 8);
			RR->node->postEvent(tPtr,ZT_EVENT_USER_MESSAGE,reinterpret_cast<const void *>(&um));
		}
		peer->received(tPtr,_path,hops(),packetId(),Packet::VERB_CIRCUIT_TEST_REPORT,0,Packet::VERB_NOP,false);
	} catch ( ... ) {
		TRACE("dropped CIRCUIT_TEST_REPORT from %s(%s): unexpected exception",source().toString().c_str(),_path->address().toString().c_str());
	}
	return true;
}

void IncomingPacket::_sendErrorNeedCredentials(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer,const uint64_t nwid)
{
	const uint64_t now = RR->node->now();
	if (peer->rateGateOutgoingComRequest(now)) {
		Packet outp(source(),RR->identity.address(),Packet::VERB_ERROR);
		outp.append((uint8_t)verb());
		outp.append(packetId());
		outp.append((uint8_t)Packet::ERROR_NEED_MEMBERSHIP_CERTIFICATE);
		outp.append(nwid);
		outp.armor(peer->key(),true,_path->nextOutgoingCounter());
		_path->send(RR,tPtr,outp.data(),outp.size(),now);
	}
}

} // namespace ZeroTier
