/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../version.h"

#include "Constants.hpp"
#include "Defaults.hpp"
#include "RuntimeEnvironment.hpp"
#include "IncomingPacket.hpp"
#include "Topology.hpp"
#include "Switch.hpp"
#include "Peer.hpp"
#include "NetworkController.hpp"
#include "SelfAwareness.hpp"

namespace ZeroTier {

bool IncomingPacket::tryDecode(const RuntimeEnvironment *RR)
{
	try {
		if ((cipher() == ZT_PROTO_CIPHER_SUITE__C25519_POLY1305_NONE)&&(verb() == Packet::VERB_HELLO)) {
			// Unencrypted HELLOs are handled here since they are used to
			// populate our identity cache in the first place. _doHELLO() is special
			// in that it contains its own authentication logic.
			return _doHELLO(RR);
		}

		SharedPtr<Peer> peer = RR->topology->getPeer(source());
		if (peer) {
			if (!dearmor(peer->key())) {
				TRACE("dropped packet from %s(%s), MAC authentication failed (size: %u)",source().toString().c_str(),_remoteAddress.toString().c_str(),size());
				return true;
			}
			if (!uncompress()) {
				TRACE("dropped packet from %s(%s), compressed data invalid",source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			}

			//TRACE("<< %s from %s(%s)",Packet::verbString(verb()),source().toString().c_str(),_remoteAddress.toString().c_str());

			switch(verb()) {
				//case Packet::VERB_NOP:
				default: // ignore unknown verbs, but if they pass auth check they are "received"
					peer->received(RR,_remoteAddress,hops(),packetId(),verb(),0,Packet::VERB_NOP);
					return true;
				case Packet::VERB_HELLO:                          return _doHELLO(RR);
				case Packet::VERB_ERROR:                          return _doERROR(RR,peer);
				case Packet::VERB_OK:                             return _doOK(RR,peer);
				case Packet::VERB_WHOIS:                          return _doWHOIS(RR,peer);
				case Packet::VERB_RENDEZVOUS:                     return _doRENDEZVOUS(RR,peer);
				case Packet::VERB_FRAME:                          return _doFRAME(RR,peer);
				case Packet::VERB_EXT_FRAME:                      return _doEXT_FRAME(RR,peer);
				case Packet::VERB_MULTICAST_LIKE:                 return _doMULTICAST_LIKE(RR,peer);
				case Packet::VERB_NETWORK_MEMBERSHIP_CERTIFICATE: return _doNETWORK_MEMBERSHIP_CERTIFICATE(RR,peer);
				case Packet::VERB_NETWORK_CONFIG_REQUEST:         return _doNETWORK_CONFIG_REQUEST(RR,peer);
				case Packet::VERB_NETWORK_CONFIG_REFRESH:         return _doNETWORK_CONFIG_REFRESH(RR,peer);
				case Packet::VERB_MULTICAST_GATHER:               return _doMULTICAST_GATHER(RR,peer);
				case Packet::VERB_MULTICAST_FRAME:                return _doMULTICAST_FRAME(RR,peer);
				case Packet::VERB_PUSH_DIRECT_PATHS:              return _doPUSH_DIRECT_PATHS(RR,peer);
			}
		} else {
			RR->sw->requestWhois(source());
			return false;
		}
	} catch ( ... ) {
		// Exceptions are more informatively caught in _do...() handlers but
		// this outer try/catch will catch anything else odd.
		TRACE("dropped ??? from %s(%s): unexpected exception in tryDecode()",source().toString().c_str(),_remoteAddress.toString().c_str());
		return true;
	}
}

bool IncomingPacket::_doERROR(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	try {
		const Packet::Verb inReVerb = (Packet::Verb)(*this)[ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB];
		const uint64_t inRePacketId = at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_IN_RE_PACKET_ID);
		const Packet::ErrorCode errorCode = (Packet::ErrorCode)(*this)[ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE];

		//TRACE("ERROR %s from %s(%s) in-re %s",Packet::errorString(errorCode),source().toString().c_str(),_remoteAddress.toString().c_str(),Packet::verbString(inReVerb));

		switch(errorCode) {

			case Packet::ERROR_OBJ_NOT_FOUND:
				if (inReVerb == Packet::VERB_WHOIS) {
					if (RR->topology->isRoot(peer->identity()))
						RR->sw->cancelWhoisRequest(Address(field(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH));
				} else if (inReVerb == Packet::VERB_NETWORK_CONFIG_REQUEST) {
					SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)));
					if ((network)&&(network->controller() == source()))
						network->setNotFound();
				}
				break;

			case Packet::ERROR_UNSUPPORTED_OPERATION:
				if (inReVerb == Packet::VERB_NETWORK_CONFIG_REQUEST) {
					SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)));
					if ((network)&&(network->controller() == source()))
						network->setNotFound();
				}
				break;

			case Packet::ERROR_IDENTITY_COLLISION:
				if (RR->topology->isRoot(peer->identity()))
					RR->node->postEvent(ZT1_EVENT_FATAL_ERROR_IDENTITY_COLLISION);
				break;

			case Packet::ERROR_NEED_MEMBERSHIP_CERTIFICATE: {
				SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)));
				if (network) {
					SharedPtr<NetworkConfig> nconf(network->config2());
					if (nconf) {
						Packet outp(peer->address(),RR->identity.address(),Packet::VERB_NETWORK_MEMBERSHIP_CERTIFICATE);
						nconf->com().serialize(outp);
						outp.armor(peer->key(),true);
						RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
					}
				}
			}	break;

			case Packet::ERROR_NETWORK_ACCESS_DENIED_: {
				SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)));
				if ((network)&&(network->controller() == source()))
					network->setAccessDenied();
			}	break;

			case Packet::ERROR_UNWANTED_MULTICAST: {
				// TODO: unsubscribe
			}	break;

			default: break;
		}

		peer->received(RR,_remoteAddress,hops(),packetId(),Packet::VERB_ERROR,inRePacketId,inReVerb);
	} catch (std::exception &ex) {
		TRACE("dropped ERROR from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped ERROR from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doHELLO(const RuntimeEnvironment *RR)
{
	try {
		const unsigned int protoVersion = (*this)[ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION];
		const unsigned int vMajor = (*this)[ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION];
		const unsigned int vMinor = (*this)[ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION];
		const unsigned int vRevision = at<uint16_t>(ZT_PROTO_VERB_HELLO_IDX_REVISION);
		const uint64_t timestamp = at<uint64_t>(ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP);

		Identity id;
		unsigned int destAddrPtr = id.deserialize(*this,ZT_PROTO_VERB_HELLO_IDX_IDENTITY) + ZT_PROTO_VERB_HELLO_IDX_IDENTITY;

		InetAddress destAddr;
		if (destAddrPtr < size()) { // ZeroTier One < 1.0.3 did not include this field
			const unsigned int destAddrType = (*this)[destAddrPtr++];
			switch(destAddrType) {
				case ZT_PROTO_DEST_ADDRESS_TYPE_IPV4:
					destAddr.set(field(destAddrPtr,4),4,at<uint16_t>(destAddrPtr + 4));
					break;
				case ZT_PROTO_DEST_ADDRESS_TYPE_IPV6:
					destAddr.set(field(destAddrPtr,16),16,at<uint16_t>(destAddrPtr + 16));
					break;
			}
		}

		if (source() != id.address()) {
			TRACE("dropped HELLO from %s(%s): identity not for sending address",source().toString().c_str(),_remoteAddress.toString().c_str());
			return true;
		}

		if (protoVersion < ZT_PROTO_VERSION_MIN) {
			TRACE("dropped HELLO from %s(%s): protocol version too old",id.address().toString().c_str(),_remoteAddress.toString().c_str());
			return true;
		}

		SharedPtr<Peer> peer(RR->topology->getPeer(id.address()));
		if (peer) {
			// We already have an identity with this address -- check for collisions

			if (peer->identity() != id) {
				// Identity is different from the one we already have -- address collision

				unsigned char key[ZT_PEER_SECRET_KEY_LENGTH];
				if (RR->identity.agree(id,key,ZT_PEER_SECRET_KEY_LENGTH)) {
					if (dearmor(key)) { // ensure packet is authentic, otherwise drop
						RR->node->postEvent(ZT1_EVENT_AUTHENTICATION_FAILURE,(const void *)&_remoteAddress);
						TRACE("rejected HELLO from %s(%s): address already claimed",id.address().toString().c_str(),_remoteAddress.toString().c_str());
						Packet outp(id.address(),RR->identity.address(),Packet::VERB_ERROR);
						outp.append((unsigned char)Packet::VERB_HELLO);
						outp.append(packetId());
						outp.append((unsigned char)Packet::ERROR_IDENTITY_COLLISION);
						outp.armor(key,true);
						RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
					} else {
						RR->node->postEvent(ZT1_EVENT_AUTHENTICATION_FAILURE,(const void *)&_remoteAddress);
						TRACE("rejected HELLO from %s(%s): packet failed authentication",id.address().toString().c_str(),_remoteAddress.toString().c_str());
					}
				} else {
					RR->node->postEvent(ZT1_EVENT_AUTHENTICATION_FAILURE,(const void *)&_remoteAddress);
					TRACE("rejected HELLO from %s(%s): key agreement failed",id.address().toString().c_str(),_remoteAddress.toString().c_str());
				}

				return true;
			} else {
				// Identity is the same as the one we already have -- check packet integrity

				if (!dearmor(peer->key())) {
					RR->node->postEvent(ZT1_EVENT_AUTHENTICATION_FAILURE,(const void *)&_remoteAddress);
					TRACE("rejected HELLO from %s(%s): packet failed authentication",id.address().toString().c_str(),_remoteAddress.toString().c_str());
					return true;
				}

				// Continue at // VALID
			}
		} else {
			// We don't already have an identity with this address -- validate and learn it

			if (!id.locallyValidate()) {
				RR->node->postEvent(ZT1_EVENT_AUTHENTICATION_FAILURE,(const void *)&_remoteAddress);
				TRACE("dropped HELLO from %s(%s): identity invalid",id.address().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			}

			SharedPtr<Peer> newPeer(new Peer(RR->identity,id));
			if (!dearmor(newPeer->key())) {
				RR->node->postEvent(ZT1_EVENT_AUTHENTICATION_FAILURE,(const void *)&_remoteAddress);
				TRACE("rejected HELLO from %s(%s): packet failed authentication",id.address().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			}

			peer = RR->topology->addPeer(newPeer);

			// Continue at // VALID
		}

		// VALID -- continues here

		peer->received(RR,_remoteAddress,hops(),packetId(),Packet::VERB_HELLO,0,Packet::VERB_NOP);
		peer->setRemoteVersion(protoVersion,vMajor,vMinor,vRevision);

		bool trusted = false;
		if (RR->topology->isRoot(id)) {
			RR->node->postNewerVersionIfNewer(vMajor,vMinor,vRevision);
			trusted = true;
		}
		if (destAddr)
			RR->sa->iam(id.address(),_remoteAddress,destAddr,trusted,RR->node->now());

		Packet outp(id.address(),RR->identity.address(),Packet::VERB_OK);

		outp.append((unsigned char)Packet::VERB_HELLO);
		outp.append(packetId());
		outp.append(timestamp);
		outp.append((unsigned char)ZT_PROTO_VERSION);
		outp.append((unsigned char)ZEROTIER_ONE_VERSION_MAJOR);
		outp.append((unsigned char)ZEROTIER_ONE_VERSION_MINOR);
		outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);

		switch(_remoteAddress.ss_family) {
			case AF_INET:
				outp.append((unsigned char)ZT_PROTO_DEST_ADDRESS_TYPE_IPV4);
				outp.append(_remoteAddress.rawIpData(),4);
				outp.append((uint16_t)_remoteAddress.port());
				break;
			case AF_INET6:
				outp.append((unsigned char)ZT_PROTO_DEST_ADDRESS_TYPE_IPV6);
				outp.append(_remoteAddress.rawIpData(),16);
				outp.append((uint16_t)_remoteAddress.port());
				break;
			default:
				outp.append((unsigned char)ZT_PROTO_DEST_ADDRESS_TYPE_NONE);
				break;
		}

		outp.armor(peer->key(),true);
		RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
	} catch (std::exception &ex) {
		TRACE("dropped HELLO from %s(%s): %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped HELLO from %s(%s): unexpected exception",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doOK(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	try {
		const Packet::Verb inReVerb = (Packet::Verb)(*this)[ZT_PROTO_VERB_OK_IDX_IN_RE_VERB];
		const uint64_t inRePacketId = at<uint64_t>(ZT_PROTO_VERB_OK_IDX_IN_RE_PACKET_ID);

		//TRACE("%s(%s): OK(%s)",source().toString().c_str(),_remoteAddress.toString().c_str(),Packet::verbString(inReVerb));

		switch(inReVerb) {

			case Packet::VERB_HELLO: {
				const unsigned int latency = std::min((unsigned int)(RR->node->now() - at<uint64_t>(ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP)),(unsigned int)0xffff);
				const unsigned int vProto = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_PROTOCOL_VERSION];
				const unsigned int vMajor = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_MAJOR_VERSION];
				const unsigned int vMinor = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_MINOR_VERSION];
				const unsigned int vRevision = at<uint16_t>(ZT_PROTO_VERB_HELLO__OK__IDX_REVISION);

				InetAddress destAddr;
				unsigned int destAddrPtr = ZT_PROTO_VERB_HELLO__OK__IDX_REVISION + 2; // dest address, if present, will start after 16-bit revision
				if (destAddrPtr < size()) { // ZeroTier One < 1.0.3 did not include this field
					const unsigned int destAddrType = (*this)[destAddrPtr++];
					switch(destAddrType) {
						case ZT_PROTO_DEST_ADDRESS_TYPE_IPV4:
							destAddr.set(field(destAddrPtr,4),4,at<uint16_t>(destAddrPtr + 4));
							break;
						case ZT_PROTO_DEST_ADDRESS_TYPE_IPV6:
							destAddr.set(field(destAddrPtr,16),16,at<uint16_t>(destAddrPtr + 16));
							break;
					}
				}

				if (vProto < ZT_PROTO_VERSION_MIN) {
					TRACE("%s(%s): OK(HELLO) dropped, protocol version too old",source().toString().c_str(),_remoteAddress.toString().c_str());
					return true;
				}

				TRACE("%s(%s): OK(HELLO), version %u.%u.%u, latency %u, reported external address %s",source().toString().c_str(),_remoteAddress.toString().c_str(),vMajor,vMinor,vRevision,latency,((destAddr) ? destAddr.toString().c_str() : "(none)"));

				peer->addDirectLatencyMeasurment(latency);
				peer->setRemoteVersion(vProto,vMajor,vMinor,vRevision);

				bool trusted = false;
				if (RR->topology->isRoot(peer->identity())) {
					RR->node->postNewerVersionIfNewer(vMajor,vMinor,vRevision);
					trusted = true;
				}
				if (destAddr)
					RR->sa->iam(peer->address(),_remoteAddress,destAddr,trusted,RR->node->now());
			}	break;

			case Packet::VERB_WHOIS: {
				/* Right now only root servers are allowed to send OK(WHOIS) to prevent
				 * poisoning attacks. Further decentralization will require some other
				 * kind of trust mechanism. */
				if (RR->topology->isRoot(peer->identity())) {
					const Identity id(*this,ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY);
					if (id.locallyValidate())
						RR->sw->doAnythingWaitingForPeer(RR->topology->addPeer(SharedPtr<Peer>(new Peer(RR->identity,id))));
				}
			} break;

			case Packet::VERB_NETWORK_CONFIG_REQUEST: {
				const SharedPtr<Network> nw(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_NETWORK_ID)));
				if ((nw)&&(nw->controller() == peer->address())) {
					const unsigned int dictlen = at<uint16_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_DICT_LEN);
					const std::string dict((const char *)field(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_DICT,dictlen),dictlen);
					if (dict.length()) {
						if (nw->setConfiguration(Dictionary(dict)) == 2) { // 2 == accepted and actually new
							/* If this configuration was indeed new, we do another
							 * controller request with its revision. We do this in
							 * order to (a) tell the network controller we got it (it
							 * won't send a duplicate if ts == current), and (b)
							 * get another one if the controller is changing rapidly
							 * until we finally have the final version.
							 *
							 * Note that we don't do this for network controllers with
							 * versions <= 1.0.3, since those regenerate a new controller
							 * with a new revision every time. In that case this double
							 * confirmation would create a race condition. */
							const SharedPtr<NetworkConfig> nc(nw->config2());
							if ((peer->atLeastVersion(1,0,3))&&(nc)&&(nc->revision() > 0)) {
								Packet outp(peer->address(),RR->identity.address(),Packet::VERB_NETWORK_CONFIG_REQUEST);
								outp.append((uint64_t)nw->id());
								outp.append((uint16_t)0); // no meta-data
								outp.append((uint64_t)nc->revision());
								outp.armor(peer->key(),true);
								RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
							}
						}
						TRACE("got network configuration for network %.16llx from %s",(unsigned long long)nw->id(),source().toString().c_str());
					}
				}
			}	break;

			case Packet::VERB_MULTICAST_GATHER: {
				const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_NETWORK_ID);
				const MulticastGroup mg(MAC(field(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_MAC,6),6),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_ADI));
				TRACE("%s(%s): OK(MULTICAST_GATHER) %.16llx/%s length %u",source().toString().c_str(),_remoteAddress.toString().c_str(),nwid,mg.toString().c_str(),size());
				const unsigned int count = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_GATHER_RESULTS + 4);
				RR->mc->addMultiple(RR->node->now(),nwid,mg,field(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_GATHER_RESULTS + 6,count * 5),count,at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER__OK__IDX_GATHER_RESULTS));
			}	break;

			case Packet::VERB_MULTICAST_FRAME: {
				const unsigned int flags = (*this)[ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_FLAGS];
				const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_NETWORK_ID);
				const MulticastGroup mg(MAC(field(ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_MAC,6),6),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_ADI));

				//TRACE("%s(%s): OK(MULTICAST_FRAME) %.16llx/%s flags %.2x",peer->address().toString().c_str(),_remoteAddress.toString().c_str(),nwid,mg.toString().c_str(),flags);

				unsigned int offset = 0;

				if ((flags & 0x01) != 0) {
					// OK(MULTICAST_FRAME) includes certificate of membership update
					CertificateOfMembership com;
					offset += com.deserialize(*this,ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_COM_AND_GATHER_RESULTS);
					SharedPtr<Network> network(RR->node->network(nwid));
					if ((network)&&(com.hasRequiredFields()))
						network->addMembershipCertificate(com,false);
				}

				if ((flags & 0x02) != 0) {
					// OK(MULTICAST_FRAME) includes implicit gather results
					offset += ZT_PROTO_VERB_MULTICAST_FRAME__OK__IDX_COM_AND_GATHER_RESULTS;
					unsigned int totalKnown = at<uint32_t>(offset); offset += 4;
					unsigned int count = at<uint16_t>(offset); offset += 2;
					RR->mc->addMultiple(RR->node->now(),nwid,mg,field(offset,count * 5),count,totalKnown);
				}
			}	break;

			default: break;
		}

		peer->received(RR,_remoteAddress,hops(),packetId(),Packet::VERB_OK,inRePacketId,inReVerb);
	} catch (std::exception &ex) {
		TRACE("dropped OK from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped OK from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doWHOIS(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	try {
		if (payloadLength() == ZT_ADDRESS_LENGTH) {
			const SharedPtr<Peer> queried(RR->topology->getPeer(Address(payload(),ZT_ADDRESS_LENGTH)));
			if (queried) {
				Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
				outp.append((unsigned char)Packet::VERB_WHOIS);
				outp.append(packetId());
				queried->identity().serialize(outp,false);
				outp.armor(peer->key(),true);
				RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
			} else {
				Packet outp(peer->address(),RR->identity.address(),Packet::VERB_ERROR);
				outp.append((unsigned char)Packet::VERB_WHOIS);
				outp.append(packetId());
				outp.append((unsigned char)Packet::ERROR_OBJ_NOT_FOUND);
				outp.append(payload(),ZT_ADDRESS_LENGTH);
				outp.armor(peer->key(),true);
				RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
			}
		} else {
			TRACE("dropped WHOIS from %s(%s): missing or invalid address",source().toString().c_str(),_remoteAddress.toString().c_str());
		}
		peer->received(RR,_remoteAddress,hops(),packetId(),Packet::VERB_WHOIS,0,Packet::VERB_NOP);
	} catch ( ... ) {
		TRACE("dropped WHOIS from %s(%s): unexpected exception",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doRENDEZVOUS(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	try {
		const Address with(field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ZTADDRESS,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
		const SharedPtr<Peer> withPeer(RR->topology->getPeer(with));
		if (withPeer) {
			const unsigned int port = at<uint16_t>(ZT_PROTO_VERB_RENDEZVOUS_IDX_PORT);
			const unsigned int addrlen = (*this)[ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRLEN];
			if ((port > 0)&&((addrlen == 4)||(addrlen == 16))) {
				InetAddress atAddr(field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRESS,addrlen),addrlen,port);
				TRACE("RENDEZVOUS from %s says %s might be at %s, starting NAT-t",peer->address().toString().c_str(),with.toString().c_str(),atAddr.toString().c_str());
				peer->received(RR,_remoteAddress,hops(),packetId(),Packet::VERB_RENDEZVOUS,0,Packet::VERB_NOP);
				RR->sw->contact(withPeer,atAddr);
			} else {
				TRACE("dropped corrupt RENDEZVOUS from %s(%s) (bad address or port)",peer->address().toString().c_str(),_remoteAddress.toString().c_str());
			}
		} else {
			TRACE("ignored RENDEZVOUS from %s(%s) to meet unknown peer %s",peer->address().toString().c_str(),_remoteAddress.toString().c_str(),with.toString().c_str());
		}
	} catch (std::exception &ex) {
		TRACE("dropped RENDEZVOUS from %s(%s): %s",peer->address().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped RENDEZVOUS from %s(%s): unexpected exception",peer->address().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doFRAME(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	try {
		const SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID)));
		if (network) {
			if (size() > ZT_PROTO_VERB_FRAME_IDX_PAYLOAD) {
				if (!network->isAllowed(peer->address())) {
					TRACE("dropped FRAME from %s(%s): not a member of private network %.16llx",peer->address().toString().c_str(),_remoteAddress.toString().c_str(),(unsigned long long)network->id());
					_sendErrorNeedCertificate(RR,peer,network->id());
					return true;
				}

				const unsigned int etherType = at<uint16_t>(ZT_PROTO_VERB_FRAME_IDX_ETHERTYPE);
				if (!network->config()->permitsEtherType(etherType)) {
					TRACE("dropped FRAME from %s(%s): ethertype %.4x not allowed on %.16llx",peer->address().toString().c_str(),_remoteAddress.toString().c_str(),(unsigned int)etherType,(unsigned long long)network->id());
					return true;
				}

				const unsigned int payloadLen = size() - ZT_PROTO_VERB_FRAME_IDX_PAYLOAD;
				RR->node->putFrame(network->id(),MAC(peer->address(),network->id()),network->mac(),etherType,0,field(ZT_PROTO_VERB_FRAME_IDX_PAYLOAD,payloadLen),payloadLen);
			}

			peer->received(RR,_remoteAddress,hops(),packetId(),Packet::VERB_FRAME,0,Packet::VERB_NOP);
		} else {
			TRACE("dropped FRAME from %s(%s): we are not connected to network %.16llx",source().toString().c_str(),_remoteAddress.toString().c_str(),at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID));
		}
	} catch (std::exception &ex) {
		TRACE("dropped FRAME from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped FRAME from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doEXT_FRAME(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	try {
		SharedPtr<Network> network(RR->node->network(at<uint64_t>(ZT_PROTO_VERB_EXT_FRAME_IDX_NETWORK_ID)));
		if (network) {
			if (size() > ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD) {
				const unsigned int flags = (*this)[ZT_PROTO_VERB_EXT_FRAME_IDX_FLAGS];

				unsigned int comLen = 0;
				if ((flags & 0x01) != 0) {
					CertificateOfMembership com;
					comLen = com.deserialize(*this,ZT_PROTO_VERB_EXT_FRAME_IDX_COM);
					if (com.hasRequiredFields())
						network->addMembershipCertificate(com,false);
				}

				if (!network->isAllowed(peer->address())) {
					TRACE("dropped EXT_FRAME from %s(%s): not a member of private network %.16llx",peer->address().toString().c_str(),_remoteAddress.toString().c_str(),network->id());
					_sendErrorNeedCertificate(RR,peer,network->id());
					return true;
				}

				// Everything after flags must be adjusted based on the length
				// of the certificate, if there was one...

				const unsigned int etherType = at<uint16_t>(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_ETHERTYPE);
				if (!network->config()->permitsEtherType(etherType)) {
					TRACE("dropped EXT_FRAME from %s(%s): ethertype %.4x not allowed on network %.16llx",peer->address().toString().c_str(),_remoteAddress.toString().c_str(),(unsigned int)etherType,(unsigned long long)network->id());
					return true;
				}

				const MAC to(field(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_TO,ZT_PROTO_VERB_EXT_FRAME_LEN_TO),ZT_PROTO_VERB_EXT_FRAME_LEN_TO);
				const MAC from(field(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_FROM,ZT_PROTO_VERB_EXT_FRAME_LEN_FROM),ZT_PROTO_VERB_EXT_FRAME_LEN_FROM);

				if (to.isMulticast()) {
					TRACE("dropped EXT_FRAME from %s@%s(%s) to %s: destination is multicast, must use MULTICAST_FRAME",from.toString().c_str(),peer->address().toString().c_str(),_remoteAddress.toString().c_str(),to.toString().c_str());
					return true;
				}

				if ((!from)||(from.isMulticast())||(from == network->mac())) {
					TRACE("dropped EXT_FRAME from %s@%s(%s) to %s: invalid source MAC",from.toString().c_str(),peer->address().toString().c_str(),_remoteAddress.toString().c_str(),to.toString().c_str());
					return true;
				}

				if (from != MAC(peer->address(),network->id())) {
					if (network->permitsBridging(peer->address())) {
						network->learnBridgeRoute(from,peer->address());
					} else {
						TRACE("dropped EXT_FRAME from %s@%s(%s) to %s: sender not allowed to bridge into %.16llx",from.toString().c_str(),peer->address().toString().c_str(),_remoteAddress.toString().c_str(),to.toString().c_str(),network->id());
						return true;
					}
				} else if (to != network->mac()) {
					if (!network->permitsBridging(RR->identity.address())) {
						TRACE("dropped EXT_FRAME from %s@%s(%s) to %s: I cannot bridge to %.16llx or bridging disabled on network",from.toString().c_str(),peer->address().toString().c_str(),_remoteAddress.toString().c_str(),to.toString().c_str(),network->id());
						return true;
					}
				}

				const unsigned int payloadLen = size() - (comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD);
				RR->node->putFrame(network->id(),from,to,etherType,0,field(comLen + ZT_PROTO_VERB_EXT_FRAME_IDX_PAYLOAD,payloadLen),payloadLen);
			}

			peer->received(RR,_remoteAddress,hops(),packetId(),Packet::VERB_EXT_FRAME,0,Packet::VERB_NOP);
		} else {
			TRACE("dropped EXT_FRAME from %s(%s): we are not connected to network %.16llx",source().toString().c_str(),_remoteAddress.toString().c_str(),at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID));
		}
	} catch (std::exception &ex) {
		TRACE("dropped EXT_FRAME from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped EXT_FRAME from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doMULTICAST_LIKE(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	try {
		const uint64_t now = RR->node->now();

		// Iterate through 18-byte network,MAC,ADI tuples
		for(unsigned int ptr=ZT_PACKET_IDX_PAYLOAD;ptr<size();ptr+=18)
			RR->mc->add(now,at<uint64_t>(ptr),MulticastGroup(MAC(field(ptr + 8,6),6),at<uint32_t>(ptr + 14)),peer->address());

		peer->received(RR,_remoteAddress,hops(),packetId(),Packet::VERB_MULTICAST_LIKE,0,Packet::VERB_NOP);
	} catch (std::exception &ex) {
		TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doNETWORK_MEMBERSHIP_CERTIFICATE(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	try {
		CertificateOfMembership com;

		unsigned int ptr = ZT_PACKET_IDX_PAYLOAD;
		while (ptr < size()) {
			ptr += com.deserialize(*this,ptr);
			if (com.hasRequiredFields()) {
				SharedPtr<Network> network(RR->node->network(com.networkId()));
				if (network)
					network->addMembershipCertificate(com,false);
			}
		}

		peer->received(RR,_remoteAddress,hops(),packetId(),Packet::VERB_NETWORK_MEMBERSHIP_CERTIFICATE,0,Packet::VERB_NOP);
	} catch (std::exception &ex) {
		TRACE("dropped NETWORK_MEMBERSHIP_CERTIFICATE from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped NETWORK_MEMBERSHIP_CERTIFICATE from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doNETWORK_CONFIG_REQUEST(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	try {
		const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_NETWORK_ID);
		const unsigned int metaDataLength = at<uint16_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT_LEN);
		const Dictionary metaData((const char *)field(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT,metaDataLength),metaDataLength);
		const uint64_t haveRevision = ((ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT + metaDataLength + 8) <= size()) ? at<uint64_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT + metaDataLength) : 0ULL;

		const unsigned int h = hops();
		const uint64_t pid = packetId();
		peer->received(RR,_remoteAddress,h,pid,Packet::VERB_NETWORK_CONFIG_REQUEST,0,Packet::VERB_NOP);

		if (RR->localNetworkController) {
			Dictionary netconf;
			switch(RR->localNetworkController->doNetworkConfigRequest((h > 0) ? InetAddress() : _remoteAddress,RR->identity,peer->identity(),nwid,metaData,haveRevision,netconf)) {
				case NetworkController::NETCONF_QUERY_OK: {
					const std::string netconfStr(netconf.toString());
					if (netconfStr.length() > 0xffff) { // sanity check since field ix 16-bit
						TRACE("NETWORK_CONFIG_REQUEST failed: internal error: netconf size %u is too large",(unsigned int)netconfStr.length());
					} else {
						Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
						outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
						outp.append(pid);
						outp.append(nwid);
						outp.append((uint16_t)netconfStr.length());
						outp.append(netconfStr.data(),(unsigned int)netconfStr.length());
						outp.compress();
						outp.armor(peer->key(),true);
						if (outp.size() > ZT_PROTO_MAX_PACKET_LENGTH) {
							TRACE("NETWORK_CONFIG_REQUEST failed: internal error: netconf size %u is too large",(unsigned int)netconfStr.length());
						} else {
							RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
						}
					}
				}	break;
				case NetworkController::NETCONF_QUERY_OK_BUT_NOT_NEWER: // nothing to do -- netconf has not changed
					break;
				case NetworkController::NETCONF_QUERY_OBJECT_NOT_FOUND: {
					Packet outp(peer->address(),RR->identity.address(),Packet::VERB_ERROR);
					outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
					outp.append(pid);
					outp.append((unsigned char)Packet::ERROR_OBJ_NOT_FOUND);
					outp.append(nwid);
					outp.armor(peer->key(),true);
					RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
				}	break;
				case NetworkController::NETCONF_QUERY_ACCESS_DENIED: {
					Packet outp(peer->address(),RR->identity.address(),Packet::VERB_ERROR);
					outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
					outp.append(pid);
					outp.append((unsigned char)Packet::ERROR_NETWORK_ACCESS_DENIED_);
					outp.append(nwid);
					outp.armor(peer->key(),true);
					RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
				} break;
				case NetworkController::NETCONF_QUERY_INTERNAL_SERVER_ERROR:
					TRACE("NETWORK_CONFIG_REQUEST failed: internal error: %s",netconf.get("error","(unknown)").c_str());
					break;
				default:
					TRACE("NETWORK_CONFIG_REQUEST failed: invalid return value from NetworkController::doNetworkConfigRequest()");
					break;
			}
		} else {
			Packet outp(peer->address(),RR->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
			outp.append(pid);
			outp.append((unsigned char)Packet::ERROR_UNSUPPORTED_OPERATION);
			outp.append(nwid);
			outp.armor(peer->key(),true);
			RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
		}
	} catch (std::exception &exc) {
		TRACE("dropped NETWORK_CONFIG_REQUEST from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),exc.what());
	} catch ( ... ) {
		TRACE("dropped NETWORK_CONFIG_REQUEST from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doNETWORK_CONFIG_REFRESH(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	try {
		unsigned int ptr = ZT_PACKET_IDX_PAYLOAD;
		while ((ptr + 8) <= size()) {
			uint64_t nwid = at<uint64_t>(ptr);
			SharedPtr<Network> nw(RR->node->network(nwid));
			if ((nw)&&(peer->address() == nw->controller()))
				nw->requestConfiguration();
			ptr += 8;
		}
		peer->received(RR,_remoteAddress,hops(),packetId(),Packet::VERB_NETWORK_CONFIG_REFRESH,0,Packet::VERB_NOP);
	} catch (std::exception &exc) {
		TRACE("dropped NETWORK_CONFIG_REFRESH from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),exc.what());
	} catch ( ... ) {
		TRACE("dropped NETWORK_CONFIG_REFRESH from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doMULTICAST_GATHER(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	try {
		const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_NETWORK_ID);
		const MulticastGroup mg(MAC(field(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_MAC,6),6),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_ADI));
		const unsigned int gatherLimit = at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_GATHER_LIMIT);

		//TRACE("<<MC %s(%s) GATHER up to %u in %.16llx/%s",source().toString().c_str(),_remoteAddress.toString().c_str(),gatherLimit,nwid,mg.toString().c_str());

		if (gatherLimit) {
			Packet outp(peer->address(),RR->identity.address(),Packet::VERB_OK);
			outp.append((unsigned char)Packet::VERB_MULTICAST_GATHER);
			outp.append(packetId());
			outp.append(nwid);
			mg.mac().appendTo(outp);
			outp.append((uint32_t)mg.adi());
			if (RR->mc->gather(peer->address(),nwid,mg,outp,gatherLimit)) {
				outp.armor(peer->key(),true);
				RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
			}
		}

		peer->received(RR,_remoteAddress,hops(),packetId(),Packet::VERB_MULTICAST_GATHER,0,Packet::VERB_NOP);
	} catch (std::exception &exc) {
		TRACE("dropped MULTICAST_GATHER from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),exc.what());
	} catch ( ... ) {
		TRACE("dropped MULTICAST_GATHER from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doMULTICAST_FRAME(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	try {
		const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID);
		const unsigned int flags = (*this)[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FLAGS];

		const SharedPtr<Network> network(RR->node->network(nwid));
		if (network) {
			// Offset -- size of optional fields added to position of later fields
			unsigned int offset = 0;

			if ((flags & 0x01) != 0) {
				CertificateOfMembership com;
				offset += com.deserialize(*this,ZT_PROTO_VERB_MULTICAST_FRAME_IDX_COM);
				if (com.hasRequiredFields())
					network->addMembershipCertificate(com,false);
			}

			// Check membership after we've read any included COM, since
			// that cert might be what we needed.
			if (!network->isAllowed(peer->address())) {
				TRACE("dropped MULTICAST_FRAME from %s(%s): not a member of private network %.16llx",peer->address().toString().c_str(),_remoteAddress.toString().c_str(),(unsigned long long)network->id());
				_sendErrorNeedCertificate(RR,peer,network->id());
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
			const unsigned int payloadLen = size() - (offset + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME);

			//TRACE("<<MC FRAME %.16llx/%s from %s@%s flags %.2x length %u",nwid,to.toString().c_str(),from.toString().c_str(),peer->address().toString().c_str(),flags,payloadLen);

			if ((payloadLen > 0)&&(payloadLen <= ZT_IF_MTU)) {
				if (!to.mac().isMulticast()) {
					TRACE("dropped MULTICAST_FRAME from %s@%s(%s) to %s: destination is unicast, must use FRAME or EXT_FRAME",from.toString().c_str(),peer->address().toString().c_str(),_remoteAddress.toString().c_str(),to.toString().c_str());
					return true;
				}
				if ((!from)||(from.isMulticast())||(from == network->mac())) {
					TRACE("dropped MULTICAST_FRAME from %s@%s(%s) to %s: invalid source MAC",from.toString().c_str(),peer->address().toString().c_str(),_remoteAddress.toString().c_str(),to.toString().c_str());
					return true;
				}

				if (from != MAC(peer->address(),network->id())) {
					if (network->permitsBridging(peer->address())) {
						network->learnBridgeRoute(from,peer->address());
					} else {
						TRACE("dropped MULTICAST_FRAME from %s@%s(%s) to %s: sender not allowed to bridge into %.16llx",from.toString().c_str(),peer->address().toString().c_str(),_remoteAddress.toString().c_str(),to.toString().c_str(),network->id());
						return true;
					}
				}

				RR->node->putFrame(network->id(),from,to.mac(),etherType,0,field(offset + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME,payloadLen),payloadLen);
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
					RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
				}
			}
		} // else ignore -- not a member of this network

		peer->received(RR,_remoteAddress,hops(),packetId(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP);
	} catch (std::exception &exc) {
		TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),exc.what());
	} catch ( ... ) {
		TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool IncomingPacket::_doPUSH_DIRECT_PATHS(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer)
{
	return true;
}

void IncomingPacket::_sendErrorNeedCertificate(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer,uint64_t nwid)
{
	Packet outp(source(),RR->identity.address(),Packet::VERB_ERROR);
	outp.append((unsigned char)verb());
	outp.append(packetId());
	outp.append((unsigned char)Packet::ERROR_NEED_MEMBERSHIP_CERTIFICATE);
	outp.append(nwid);
	outp.armor(peer->key(),true);
	RR->node->putPacket(_remoteAddress,outp.data(),outp.size());
}

} // namespace ZeroTier
