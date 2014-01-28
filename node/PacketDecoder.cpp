/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
#include "Topology.hpp"
#include "PacketDecoder.hpp"
#include "Switch.hpp"
#include "Peer.hpp"
#include "NodeConfig.hpp"
#include "Service.hpp"
#include "Demarc.hpp"
#include "SoftwareUpdater.hpp"

namespace ZeroTier {

bool PacketDecoder::tryDecode(const RuntimeEnvironment *_r)
{
	if ((!encrypted())&&(verb() == Packet::VERB_HELLO)) {
		// Unencrypted HELLOs are handled here since they are used to
		// populate our identity cache in the first place. _doHELLO() is special
		// in that it contains its own authentication logic.
		TRACE("HELLO from %s(%s)",source().toString().c_str(),_remoteAddress.toString().c_str());
		return _doHELLO(_r);
	}

	SharedPtr<Peer> peer = _r->topology->getPeer(source());
	if (peer) {
		// Resume saved intermediate decode state?
		if (_step == DECODE_WAITING_FOR_MULTICAST_FRAME_ORIGINAL_SENDER_LOOKUP) {
			// In this state we have already authenticated and decrypted the
			// packet and are waiting for the lookup of the original sender
			// for a multicast frame. So check to see if we've got it.
			return _doMULTICAST_FRAME(_r,peer);
		} else if (_step == DECODE_WAITING_FOR_NETWORK_MEMBERSHIP_CERTIFICATE_SIGNER_LOOKUP) {
			// In this state we have already authenticated and decoded the
			// packet and we're waiting for the identity of the cert's signer.
			return _doNETWORK_MEMBERSHIP_CERTIFICATE(_r,peer);
		}

		if (!dearmor(peer->key())) {
			TRACE("dropped packet from %s(%s), MAC authentication failed (size: %u)",source().toString().c_str(),_remoteAddress.toString().c_str(),size());
			return true;
		}
		if (!uncompress()) {
			TRACE("dropped packet from %s(%s), compressed data invalid",source().toString().c_str(),_remoteAddress.toString().c_str());
			return true;
		}

		switch(verb()) {
			case Packet::VERB_NOP:
				peer->onReceive(_r,_localPort,_remoteAddress,hops(),packetId(),Packet::VERB_NOP,0,Packet::VERB_NOP,Utils::now());
				return true;
			case Packet::VERB_HELLO:
				return _doHELLO(_r); // legal, but why? :)
			case Packet::VERB_ERROR:
				return _doERROR(_r,peer);
			case Packet::VERB_OK:
				return _doOK(_r,peer);
			case Packet::VERB_WHOIS:
				return _doWHOIS(_r,peer);
			case Packet::VERB_RENDEZVOUS:
				return _doRENDEZVOUS(_r,peer);
			case Packet::VERB_FRAME:
				return _doFRAME(_r,peer);
			case Packet::VERB_BRIDGED_FRAME:
				return _doBRIDGED_FRAME(_r,peer);
			case Packet::VERB_MULTICAST_FRAME:
				return _doMULTICAST_FRAME(_r,peer);
			case Packet::VERB_MULTICAST_LIKE:
				return _doMULTICAST_LIKE(_r,peer);
			case Packet::VERB_NETWORK_MEMBERSHIP_CERTIFICATE:
				return _doNETWORK_MEMBERSHIP_CERTIFICATE(_r,peer);
			case Packet::VERB_NETWORK_CONFIG_REQUEST:
				return _doNETWORK_CONFIG_REQUEST(_r,peer);
			case Packet::VERB_NETWORK_CONFIG_REFRESH:
				return _doNETWORK_CONFIG_REFRESH(_r,peer);
			default:
				// This might be something from a new or old version of the protocol.
				// Technically it passed MAC so the packet is still valid, but we
				// ignore it.
				TRACE("ignored unrecognized verb %.2x from %s(%s)",(unsigned int)verb(),source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
		}
	} else {
		_step = DECODE_WAITING_FOR_SENDER_LOOKUP; // should already be this...
		_r->sw->requestWhois(source());
		return false;
	}
}

bool PacketDecoder::_doERROR(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		Packet::Verb inReVerb = (Packet::Verb)(*this)[ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB];
		uint64_t inRePacketId = at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_IN_RE_PACKET_ID);
		Packet::ErrorCode errorCode = (Packet::ErrorCode)(*this)[ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE];

		TRACE("ERROR %s from %s(%s) in-re %s",Packet::errorString(errorCode),source().toString().c_str(),_remoteAddress.toString().c_str(),Packet::verbString(inReVerb));

		switch(errorCode) {
			case Packet::ERROR_OBJ_NOT_FOUND:
				if (inReVerb == Packet::VERB_WHOIS) {
					if (_r->topology->isSupernode(source()))
						_r->sw->cancelWhoisRequest(Address(field(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH));
				} else if (inReVerb == Packet::VERB_NETWORK_CONFIG_REQUEST) {
					SharedPtr<Network> network(_r->nc->network(at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)));
					if ((network)&&(network->controller() == source()))
						network->setNotFound();
				}
				break;
			case Packet::ERROR_IDENTITY_COLLISION:
				// TODO: if it comes from a supernode, regenerate a new identity
				// if (_r->topology->isSupernode(source())) {}
				break;
			case Packet::ERROR_NEED_MEMBERSHIP_CERTIFICATE: {
				SharedPtr<Network> network(_r->nc->network(at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)));
				if (network)
					network->pushMembershipCertificate(source(),true,Utils::now());
			}	break;
			case Packet::ERROR_NETWORK_ACCESS_DENIED_: {
				SharedPtr<Network> network(_r->nc->network(at<uint64_t>(ZT_PROTO_VERB_ERROR_IDX_PAYLOAD)));
				if ((network)&&(network->controller() == source()))
					network->setAccessDenied();
			}	break;
			default:
				break;
		}

		peer->onReceive(_r,_localPort,_remoteAddress,hops(),packetId(),Packet::VERB_ERROR,inRePacketId,inReVerb,Utils::now());
	} catch (std::exception &ex) {
		TRACE("dropped ERROR from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped ERROR from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool PacketDecoder::_doHELLO(const RuntimeEnvironment *_r)
{
	try {
		unsigned int protoVersion = (*this)[ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION];
		unsigned int vMajor = (*this)[ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION];
		unsigned int vMinor = (*this)[ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION];
		unsigned int vRevision = at<uint16_t>(ZT_PROTO_VERB_HELLO_IDX_REVISION);
		uint64_t timestamp = at<uint64_t>(ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP);
		Identity id(*this,ZT_PROTO_VERB_HELLO_IDX_IDENTITY);

		if (protoVersion != ZT_PROTO_VERSION) {
			TRACE("dropped HELLO from %s(%s): protocol version mismatch (%u, expected %u)",source().toString().c_str(),_remoteAddress.toString().c_str(),protoVersion,(unsigned int)ZT_PROTO_VERSION);
			return true;
		}
		if (!id.locallyValidate()) {
			TRACE("dropped HELLO from %s(%s): identity invalid",source().toString().c_str(),_remoteAddress.toString().c_str());
			return true;
		}

		// Do we already have this peer?
		SharedPtr<Peer> peer(_r->topology->getPeer(id.address()));
		if (peer) {
			// Check to make sure this isn't a colliding identity (different key,
			// but same address). The odds are spectacularly low but it could happen.
			// Could also be a sign of someone doing something nasty.
			if (peer->identity() != id) {
				unsigned char key[ZT_PEER_SECRET_KEY_LENGTH];
				if (_r->identity.agree(id,key,ZT_PEER_SECRET_KEY_LENGTH)) {
					if (dearmor(key)) { // ensure packet is authentic, otherwise drop
						TRACE("rejected HELLO from %s(%s): address already claimed",source().toString().c_str(),_remoteAddress.toString().c_str());
						Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
						outp.append((unsigned char)Packet::VERB_HELLO);
						outp.append(packetId());
						outp.append((unsigned char)Packet::ERROR_IDENTITY_COLLISION);
						outp.armor(key,true);
						_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
					} else {
						LOG("rejected HELLO from %s(%s): packet failed authentication",source().toString().c_str(),_remoteAddress.toString().c_str());
					}
				} else {
					TRACE("rejected HELLO from %s(%s): key agreement failed",source().toString().c_str(),_remoteAddress.toString().c_str());
				}
				return true;
			} else if (!dearmor(peer->key())) {
				TRACE("rejected HELLO from %s(%s): packet failed authentication",source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			} // else continue and respond
		} else {
			// If we don't have a peer record on file, check the identity cache (if
			// we have one) to see if we have a cached identity. Then check that for
			// collision before adding a new peer.
			Identity alreadyHaveCachedId(_r->topology->getIdentity(id.address()));
			if ((alreadyHaveCachedId)&&(id != alreadyHaveCachedId)) {
				unsigned char key[ZT_PEER_SECRET_KEY_LENGTH];
				if (_r->identity.agree(id,key,ZT_PEER_SECRET_KEY_LENGTH)) {
					if (dearmor(key)) { // ensure packet is authentic, otherwise drop
						TRACE("rejected HELLO from %s(%s): address already claimed",source().toString().c_str(),_remoteAddress.toString().c_str());
						Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
						outp.append((unsigned char)Packet::VERB_HELLO);
						outp.append(packetId());
						outp.append((unsigned char)Packet::ERROR_IDENTITY_COLLISION);
						outp.armor(key,true);
						_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
					} else {
						LOG("rejected HELLO from %s(%s): packet failed authentication",source().toString().c_str(),_remoteAddress.toString().c_str());
					}
				} else {
					TRACE("rejected HELLO from %s(%s): key agreement failed",source().toString().c_str(),_remoteAddress.toString().c_str());
				}
				return true;
			} // else continue since identity is already known and matches

			// If this is a new peer, learn it
			SharedPtr<Peer> newPeer(new Peer(_r->identity,id));
			if (!dearmor(newPeer->key())) {
				LOG("rejected HELLO from %s(%s): packet failed authentication",source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			}
			peer = _r->topology->addPeer(newPeer);
		}

		peer->onReceive(_r,_localPort,_remoteAddress,hops(),packetId(),Packet::VERB_HELLO,0,Packet::VERB_NOP,Utils::now());
		peer->setRemoteVersion(vMajor,vMinor,vRevision);

		// If a supernode has a version higher than ours, this causes a software
		// update check to run now.
		if ((_r->updater)&&(_r->topology->isSupernode(peer->address())))
			_r->updater->sawRemoteVersion(vMajor,vMinor,vRevision);

		Packet outp(source(),_r->identity.address(),Packet::VERB_OK);
		outp.append((unsigned char)Packet::VERB_HELLO);
		outp.append(packetId());
		outp.append(timestamp);
		outp.append((unsigned char)ZT_PROTO_VERSION);
		outp.append((unsigned char)ZEROTIER_ONE_VERSION_MAJOR);
		outp.append((unsigned char)ZEROTIER_ONE_VERSION_MINOR);
		outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
		outp.armor(peer->key(),true);
		_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
	} catch (std::exception &ex) {
		TRACE("dropped HELLO from %s(%s): %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped HELLO from %s(%s): unexpected exception",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool PacketDecoder::_doOK(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		Packet::Verb inReVerb = (Packet::Verb)(*this)[ZT_PROTO_VERB_OK_IDX_IN_RE_VERB];
		uint64_t inRePacketId = at<uint64_t>(ZT_PROTO_VERB_OK_IDX_IN_RE_PACKET_ID);

		TRACE("%s(%s): OK(%s)",source().toString().c_str(),_remoteAddress.toString().c_str(),Packet::verbString(inReVerb));

		switch(inReVerb) {
			case Packet::VERB_HELLO: {
				//unsigned int latency = std::min((unsigned int)(Utils::now() - at<uint64_t>(ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP)),(unsigned int)0xffff);
				unsigned int vMajor = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_MAJOR_VERSION];
				unsigned int vMinor = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_MINOR_VERSION];
				unsigned int vRevision = at<uint16_t>(ZT_PROTO_VERB_HELLO__OK__IDX_REVISION);
				TRACE("%s(%s): OK(HELLO), version %u.%u.%u",source().toString().c_str(),_remoteAddress.toString().c_str(),vMajor,vMinor,vRevision);
				peer->setRemoteVersion(vMajor,vMinor,vRevision);

				// If a supernode has a version higher than ours, this causes a software
				// update check to run now.
				if ((_r->updater)&&(_r->topology->isSupernode(peer->address())))
					_r->updater->sawRemoteVersion(vMajor,vMinor,vRevision);
			}	break;
			case Packet::VERB_WHOIS: {
				// Right now only supernodes are allowed to send OK(WHOIS) to prevent
				// poisoning attacks. Further decentralization will require some other
				// kind of trust mechanism.
				if (_r->topology->isSupernode(source())) {
					Identity id(*this,ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY);
					if (id.locallyValidate())
						_r->sw->doAnythingWaitingForPeer(_r->topology->addPeer(SharedPtr<Peer>(new Peer(_r->identity,id))));
				}
			} break;
			case Packet::VERB_NETWORK_CONFIG_REQUEST: {
				SharedPtr<Network> nw(_r->nc->network(at<uint64_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_NETWORK_ID)));
				if ((nw)&&(nw->controller() == source())) {
					// OK(NETWORK_CONFIG_REQUEST) is only accepted from a network's
					// controller.
					unsigned int dictlen = at<uint16_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_DICT_LEN);
					std::string dict((const char *)field(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST__OK__IDX_DICT,dictlen),dictlen);
					if (dict.length()) {
						nw->setConfiguration(Dictionary(dict));
						TRACE("got network configuration for network %.16llx from %s",(unsigned long long)nw->id(),source().toString().c_str());
					}
				}
			}	break;
			default:
				break;
		}

		peer->onReceive(_r,_localPort,_remoteAddress,hops(),packetId(),Packet::VERB_OK,inRePacketId,inReVerb,Utils::now());
	} catch (std::exception &ex) {
		TRACE("dropped OK from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped OK from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool PacketDecoder::_doWHOIS(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	if (payloadLength() == ZT_ADDRESS_LENGTH) {
		Identity id(_r->topology->getIdentity(Address(payload(),ZT_ADDRESS_LENGTH)));
		if (id) {
			Packet outp(source(),_r->identity.address(),Packet::VERB_OK);
			outp.append((unsigned char)Packet::VERB_WHOIS);
			outp.append(packetId());
			id.serialize(outp,false);
			outp.armor(peer->key(),true);
			_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
			//TRACE("sent WHOIS response to %s for %s",source().toString().c_str(),Address(payload(),ZT_ADDRESS_LENGTH).toString().c_str());
		} else {
			Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_WHOIS);
			outp.append(packetId());
			outp.append((unsigned char)Packet::ERROR_OBJ_NOT_FOUND);
			outp.append(payload(),ZT_ADDRESS_LENGTH);
			outp.armor(peer->key(),true);
			_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
			//TRACE("sent WHOIS ERROR to %s for %s (not found)",source().toString().c_str(),Address(payload(),ZT_ADDRESS_LENGTH).toString().c_str());
		}
	} else {
		TRACE("dropped WHOIS from %s(%s): missing or invalid address",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	peer->onReceive(_r,_localPort,_remoteAddress,hops(),packetId(),Packet::VERB_WHOIS,0,Packet::VERB_NOP,Utils::now());
	return true;
}

bool PacketDecoder::_doRENDEZVOUS(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		/*
		 * At the moment, we only obey RENDEZVOUS if it comes from a designated
		 * supernode. If relay offloading is implemented to scale the net, this
		 * will need reconsideration.
		 *
		 * The reason is that RENDEZVOUS could technically be used to cause a
		 * peer to send a weird encrypted UDP packet to an arbitrary IP:port.
		 * The sender of RENDEZVOUS has no control over the content of this
		 * packet, but it's still maybe something we want to not allow just
		 * anyone to order due to possible DDOS or network forensic implications.
		 * So if we diversify relays, we'll need some way of deciding whether the
		 * sender is someone we should trust with a RENDEZVOUS hint.
		 */
		if (_r->topology->isSupernode(source())) {
			Address with(field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ZTADDRESS,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
			SharedPtr<Peer> withPeer(_r->topology->getPeer(with));
			if (withPeer) {
				unsigned int port = at<uint16_t>(ZT_PROTO_VERB_RENDEZVOUS_IDX_PORT);
				unsigned int addrlen = (*this)[ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRLEN];
				if ((port > 0)&&((addrlen == 4)||(addrlen == 16))) {
					InetAddress atAddr(field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRESS,addrlen),addrlen,port);
					TRACE("RENDEZVOUS from %s says %s might be at %s, starting NAT-t",source().toString().c_str(),with.toString().c_str(),atAddr.toString().c_str());
					peer->onReceive(_r,_localPort,_remoteAddress,hops(),packetId(),Packet::VERB_RENDEZVOUS,0,Packet::VERB_NOP,Utils::now());
					_r->sw->contact(withPeer,atAddr);
				} else {
					TRACE("dropped corrupt RENDEZVOUS from %s(%s) (bad address or port)",source().toString().c_str(),_remoteAddress.toString().c_str());
				}
			} else {
				TRACE("ignored RENDEZVOUS from %s(%s) to meet unknown peer %s",source().toString().c_str(),_remoteAddress.toString().c_str(),with.toString().c_str());
			}
		} else {
			TRACE("ignored RENDEZVOUS from %s(%s): source not supernode",source().toString().c_str(),_remoteAddress.toString().c_str());
		}
	} catch (std::exception &ex) {
		TRACE("dropped RENDEZVOUS from %s(%s): %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped RENDEZVOUS from %s(%s): unexpected exception",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool PacketDecoder::_doFRAME(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		SharedPtr<Network> network(_r->nc->network(at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID)));
		if (network) {
			if (network->isAllowed(source())) {
				unsigned int etherType = at<uint16_t>(ZT_PROTO_VERB_FRAME_IDX_ETHERTYPE);
				if (size() > ZT_PROTO_VERB_FRAME_IDX_PAYLOAD) {
					if (network->config()->permitsEtherType(etherType)) {
						network->tapPut(source().toMAC(),etherType,data() + ZT_PROTO_VERB_FRAME_IDX_PAYLOAD,size() - ZT_PROTO_VERB_FRAME_IDX_PAYLOAD);
					} else {
						TRACE("dropped FRAME from %s: ethernet type %u not allowed on network %.16llx",source().toString().c_str(),etherType,(unsigned long long)network->id());
						return true;
					}
				} else return true; // ignore empty frames

				// Source moves "closer" to us in multicast propagation priority when
				// we receive unicast frames from it. This is called "implicit social
				// ordering" in other docs.
				_r->mc->bringCloser(network->id(),source());
				peer->onReceive(_r,_localPort,_remoteAddress,hops(),packetId(),Packet::VERB_FRAME,0,Packet::VERB_NOP,Utils::now());
			} else {
				TRACE("dropped FRAME from %s(%s): sender not a member of closed network %.16llx",source().toString().c_str(),_remoteAddress.toString().c_str(),network->id());

				Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
				outp.append((unsigned char)Packet::VERB_FRAME);
				outp.append(packetId());
				outp.append((unsigned char)Packet::ERROR_NEED_MEMBERSHIP_CERTIFICATE);
				outp.append(network->id());
				outp.armor(peer->key(),true);
				_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);

				return true;
			}
		} else {
			TRACE("dropped FRAME from %s(%s): we are not connected to network %.16llx",source().toString().c_str(),_remoteAddress.toString().c_str(),at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID));
			return true;
		}
	} catch (std::exception &ex) {
		TRACE("dropped FRAME from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped FRAME from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool PacketDecoder::_doBRIDGED_FRAME(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	// TODO: bridging is not implemented yet
	return true;
}

bool PacketDecoder::_doMULTICAST_FRAME(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		Address origin(Address(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ORIGIN,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_ORIGIN),ZT_ADDRESS_LENGTH));
		SharedPtr<Peer> originPeer(_r->topology->getPeer(origin));
		if (!originPeer) {
			// We must have the origin's identity in order to authenticate a multicast
			_r->sw->requestWhois(origin);
			_step = DECODE_WAITING_FOR_MULTICAST_FRAME_ORIGINAL_SENDER_LOOKUP; // causes processing to come back here
			return false;
		}

		// These fields change
		unsigned int depth = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_DEPTH);
		unsigned char *const fifo = field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_FIFO,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_FIFO);
		unsigned char *const bloom = field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_BLOOM,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_BLOOM);

		// These fields don't -- they're signed by the original sender
		const unsigned int flags = (*this)[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FLAGS];
		const uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID);
		const uint16_t bloomNonce = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_BLOOM_NONCE);
		const unsigned int prefixBits = (*this)[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_PREFIX_BITS];
		const unsigned int prefix = (*this)[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_PREFIX];
		const uint64_t guid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_GUID);
		const MAC sourceMac(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SOURCE_MAC,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_SOURCE_MAC));
		const MulticastGroup dest(MAC(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DEST_MAC,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_DEST_MAC)),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DEST_ADI));
		const unsigned int etherType = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ETHERTYPE);
		const unsigned int frameLen = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME_LEN);
		const unsigned char *const frame = field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME,frameLen);
		const unsigned int signatureLen = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME + frameLen);
		const unsigned char *const signature = field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME + frameLen + 2,signatureLen);

		SharedPtr<Network> network(_r->nc->network(nwid));

		/* Grab, verify, and learn certificate of network membership if any -- provided we are
		 * a member of this network. Note: we can do this before verification of the actual
		 * packet, since the certificate has its own separate signature. In other words a valid
		 * COM does not imply a valid multicast; they are two separate things. The ability to
		 * include the COM with the multicast is a performance optimization to allow peers to
		 * distribute their COM along with their packets instead of as a separate transaction.
		 * This causes network memberships to start working faster. */
		if (((flags & ZT_PROTO_VERB_MULTICAST_FRAME_FLAGS_HAS_MEMBERSHIP_CERTIFICATE))&&(network)) {
			CertificateOfMembership originCom(*this,ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME + frameLen + 2 + signatureLen);
			Address comSignedBy(originCom.signedBy());
			if ((originCom.networkId() == nwid)&&(comSignedBy == network->controller())) {
				SharedPtr<Peer> comSigningPeer(_r->topology->getPeer(comSignedBy));
				if (!comSigningPeer) {
					// Technically this should never happen because the COM should be signed by
					// the master for this network (in current usage) and we ought to already have
					// that cached. But handle it anyway.
					_r->sw->requestWhois(comSignedBy);
					_step = DECODE_WAITING_FOR_MULTICAST_FRAME_ORIGINAL_SENDER_LOOKUP; // causes processing to come back here
					return false;
				} else if (originCom.verify(comSigningPeer->identity())) {
					// The certificate is valid so learn it. As explained above this does not
					// imply validation of the multicast. That happens later. Look for a call
					// to network->isAllowed().
					network->addMembershipCertificate(originCom);
				} else {
					// Go ahead and drop the multicast though if the COM was invalid, since this
					// obviously signifies a problem.
					LOG("dropped MULTICAST_FRAME from %s(%s): included COM failed authentication check",source().toString().c_str(),_remoteAddress.toString().c_str());
					return true;
				}
			} else {
				// Go ahead and drop the multicast here too, since this also ought never to
				// happen and certainly indicates a problem.
				LOG("dropped MULTICAST_FRAME from %s(%s): included COM is not for this network",source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			}
		}

		// Check the multicast frame's signature to verify that its original sender is
		// who it claims to be.
		const unsigned int signedPartLen = (ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME - ZT_PROTO_VERB_MULTICAST_FRAME_IDX__START_OF_SIGNED_PORTION) + frameLen;
		if (!originPeer->identity().verify(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX__START_OF_SIGNED_PORTION,signedPartLen),signedPartLen,signature,signatureLen)) {
			LOG("dropped MULTICAST_FRAME from %s(%s): failed signature verification, claims to be from %s",source().toString().c_str(),_remoteAddress.toString().c_str(),origin.toString().c_str());
			return true;
		}

		// Security check to prohibit multicasts that are really Ethernet unicasts...
		// otherwise people could do weird things like multicast out a TCP SYN.
		if (!dest.mac().isMulticast()) {
			LOG("dropped MULTICAST_FRAME from %s(%s): %s is not a multicast/broadcast address",source().toString().c_str(),_remoteAddress.toString().c_str(),dest.mac().toString().c_str());
			return true;
		}

#ifdef ZT_TRACE_MULTICAST
		// This code, if enabled, sends a UDP pingback to a logger for each multicast.
		char mct[1024],mctdepth[1024];
		unsigned int startingFifoItems = 0;
		for(unsigned int i=0;i<ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_FIFO;i+=ZT_ADDRESS_LENGTH) {
			if (Utils::isZero(fifo + i,ZT_ADDRESS_LENGTH))
				break;
			else ++startingFifoItems;
		}
		for(unsigned int i=0;i<depth;++i)
			mctdepth[i] = ' ';
		mctdepth[depth] = 0;
		Utils::snprintf(mct,sizeof(mct),
			"%.16llx %.2u %.3u%s %c %s <- %s via %s len:%u fifosize:%u",
			guid,
			prefix,
			depth,
			mctdepth,
			(_r->topology->amSupernode() ? 'S' : '-'),
			_r->identity.address().toString().c_str(),
			origin.toString().c_str(),
			source().toString().c_str(),
			frameLen,
			startingFifoItems);
		_r->demarc->send(Demarc::ANY_PORT,ZT_DEFAULTS.multicastTraceWatcher,mct,strlen(mct),-1);
#endif

		// At this point the frame is basically valid, so we can call it a receive
		peer->onReceive(_r,_localPort,_remoteAddress,hops(),packetId(),Packet::VERB_MULTICAST_FRAME,0,Packet::VERB_NOP,Utils::now());

		// This gets updated later in most cases but start with the global limit.
		unsigned int maxDepth = ZT_MULTICAST_GLOBAL_MAX_DEPTH;

		if ((origin == _r->identity.address())||(_r->mc->deduplicate(nwid,guid))) {
			// This is a boomerang or a duplicate of a multicast we've already seen. Ordinary
			// nodes drop these, while supernodes will keep propagating them since they can
			// act as bridges between sparse multicast networks more than once.
			if (!_r->topology->amSupernode()) {
#ifdef ZT_TRACE_MULTICAST
				Utils::snprintf(mct,sizeof(mct),
					"%.16llx %.2u %.3u%s %c %s dropped: duplicate",
					guid,
					prefix,
					depth,
					mctdepth,
					(_r->topology->amSupernode() ? 'S' : '-'),
					_r->identity.address().toString().c_str());
				_r->demarc->send(Demarc::ANY_PORT,ZT_DEFAULTS.multicastTraceWatcher,mct,strlen(mct),-1);
#endif
				TRACE("dropped MULTICAST_FRAME from %s(%s): duplicate",source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			}
		} else {
			// If we are actually a member of this network (will just about always
			// be the case unless we're a supernode), check to see if we should
			// inject the packet. This also gives us an opportunity to check things
			// like multicast bandwidth constraints.
			if (network) {
				SharedPtr<NetworkConfig> nconf(network->config2());
				if (nconf) {
					maxDepth = std::min((unsigned int)ZT_MULTICAST_GLOBAL_MAX_DEPTH,nconf->multicastDepth());
					if (!maxDepth)
						maxDepth = ZT_MULTICAST_GLOBAL_MAX_DEPTH;

					if (!network->isAllowed(origin)) {
						TRACE("didn't inject MULTICAST_FRAME from %s(%s) into %.16llx: sender %s not allowed or we don't have a certificate",source().toString().c_str(),nwid,_remoteAddress.toString().c_str(),origin.toString().c_str());

						// Tell them we need a certificate
						Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
						outp.append((unsigned char)Packet::VERB_FRAME);
						outp.append(packetId());
						outp.append((unsigned char)Packet::ERROR_NEED_MEMBERSHIP_CERTIFICATE);
						outp.append(nwid);
						outp.armor(peer->key(),true);
						_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);

						// We do not terminate here, since if the member just has an out of
						// date cert or hasn't sent us a cert yet we still want to propagate
						// the message so multicast keeps working downstream.
					} else if ((!nconf->permitsBridging(origin))&&(!origin.wouldHaveMac(sourceMac))) {
						// This *does* terminate propagation, since it's technically a
						// security violation of the network's bridging policy. But if we
						// were to keep propagating it wouldn't hurt anything, just waste
						// bandwidth as everyone else would reject it too.
#ifdef ZT_TRACE_MULTICAST
						Utils::snprintf(mct,sizeof(mct),
							"%.16llx %.2u %.3u%s %c %s dropped: bridging not allowed",
							guid,
							prefix,
							depth,
							mctdepth,
							(_r->topology->amSupernode() ? 'S' : '-'),
							_r->identity.address().toString().c_str());
						_r->demarc->send(Demarc::ANY_PORT,ZT_DEFAULTS.multicastTraceWatcher,mct,strlen(mct),-1);
#endif
						TRACE("dropped MULTICAST_FRAME from %s(%s) into %.16llx: source mac %s doesn't belong to %s, and bridging is not supported on network",source().toString().c_str(),_remoteAddress.toString().c_str(),nwid,sourceMac.toString().c_str(),origin.toString().c_str());
						return true;
					} else if (!nconf->permitsEtherType(etherType)) {
						// Ditto for this-- halt propagation if this is for an ethertype
						// this network doesn't allow. Same principle as bridging test.
#ifdef ZT_TRACE_MULTICAST
						Utils::snprintf(mct,sizeof(mct),
							"%.16llx %.2u %.3u%s %c %s dropped: ethertype not allowed",
							guid,
							prefix,
							depth,
							mctdepth,
							(_r->topology->amSupernode() ? 'S' : '-'),
							_r->identity.address().toString().c_str());
						_r->demarc->send(Demarc::ANY_PORT,ZT_DEFAULTS.multicastTraceWatcher,mct,strlen(mct),-1);
#endif
						TRACE("dropped MULTICAST_FRAME from %s(%s) into %.16llx: ethertype %u is not allowed",source().toString().c_str(),nwid,_remoteAddress.toString().c_str(),etherType);
						return true;
					} else if (!network->updateAndCheckMulticastBalance(origin,dest,frameLen)) {
						// Rate limits can only be checked by members of this network, but
						// there should be enough of them that over-limit multicasts get
						// their propagation aborted.
#ifdef ZT_TRACE_MULTICAST
						Utils::snprintf(mct,sizeof(mct),
							"%.16llx %.2u %.3u%s %c %s dropped: rate limits exceeded",
							guid,
							prefix,
							depth,
							mctdepth,
							(_r->topology->amSupernode() ? 'S' : '-'),
							_r->identity.address().toString().c_str());
						_r->demarc->send(Demarc::ANY_PORT,ZT_DEFAULTS.multicastTraceWatcher,mct,strlen(mct),-1);
#endif
						TRACE("dropped MULTICAST_FRAME from %s(%s): rate limits exceeded for sender %s",source().toString().c_str(),_remoteAddress.toString().c_str(),origin.toString().c_str());
						return true;
					} else {
						network->tapPut(sourceMac,dest.mac(),etherType,frame,frameLen);
					}
				}
			}
		}

		if (depth == 0xffff) {
#ifdef ZT_TRACE_MULTICAST
			Utils::snprintf(mct,sizeof(mct),
				"%.16llx %.2u %.3u%s %c %s not forwarding: depth == 0xffff (do not forward)",
				guid,
				prefix,
				depth,
				mctdepth,
				(_r->topology->amSupernode() ? 'S' : '-'),
				_r->identity.address().toString().c_str());
			_r->demarc->send(Demarc::ANY_PORT,ZT_DEFAULTS.multicastTraceWatcher,mct,strlen(mct),-1);
#endif
			TRACE("not forwarding MULTICAST_FRAME from %s(%s): depth == 0xffff (do not forward)",source().toString().c_str(),_remoteAddress.toString().c_str());
			return true;
		}
		if (++depth > maxDepth) {
#ifdef ZT_TRACE_MULTICAST
			Utils::snprintf(mct,sizeof(mct),
				"%.16llx %.2u %.3u%s %c %s not forwarding: max propagation depth reached",
				guid,
				prefix,
				depth,
				mctdepth,
				(_r->topology->amSupernode() ? 'S' : '-'),
				_r->identity.address().toString().c_str());
			_r->demarc->send(Demarc::ANY_PORT,ZT_DEFAULTS.multicastTraceWatcher,mct,strlen(mct),-1);
#endif
			TRACE("not forwarding MULTICAST_FRAME from %s(%s): max propagation depth reached",source().toString().c_str(),_remoteAddress.toString().c_str());
			return true;
		}

		// Update depth in packet with new incremented value
		setAt(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_DEPTH,(uint16_t)depth);

		// New FIFO with room for one extra, since head will be next hop
		unsigned char newFifo[ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_FIFO + ZT_ADDRESS_LENGTH];
		unsigned char *newFifoPtr = newFifo;
		unsigned char *const newFifoEnd = newFifo + sizeof(newFifo);

		// Copy old FIFO into new buffer, terminating at first NULL address
		for(unsigned char *f=fifo,*const fifoEnd=(fifo + ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_FIFO);f!=fifoEnd;) {
			unsigned char *nf = newFifoPtr;
			unsigned char *e = nf + ZT_ADDRESS_LENGTH;
			unsigned char *ftmp = f;
			unsigned char zeroCheckMask = 0;
			while (nf != e)
				zeroCheckMask |= (*(nf++) = *(ftmp++));
			if (zeroCheckMask) {
				f = ftmp;
				newFifoPtr = nf;
			} else break;
		}

		// Add any next hops we know about to FIFO
#ifdef ZT_TRACE_MULTICAST
		unsigned char *beforeAdd = newFifoPtr;
#endif
		_r->mc->getNextHops(nwid,dest,Multicaster::AddToPropagationQueue(&newFifoPtr,newFifoEnd,bloom,bloomNonce,origin,prefixBits,prefix));
#ifdef ZT_TRACE_MULTICAST
		unsigned int numAdded = (unsigned int)(newFifoPtr - beforeAdd) / ZT_ADDRESS_LENGTH;
#endif

		// Zero-terminate new FIFO if not completely full. We pad the remainder with
		// zeroes because this improves data compression ratios.
		while (newFifoPtr != newFifoEnd)
			*(newFifoPtr++) = (unsigned char)0;

		// First element in newFifo[] is next hop
		Address nextHop(newFifo,ZT_ADDRESS_LENGTH);
		if ((!nextHop)&&(!_r->topology->amSupernode())) {
			SharedPtr<Peer> supernode(_r->topology->getBestSupernode(&origin,1,true));
			if (supernode)
				nextHop = supernode->address();
		}
		if ((!nextHop)||(nextHop == _r->identity.address())) { // check against our addr is a sanity check
#ifdef ZT_TRACE_MULTICAST
			Utils::snprintf(mct,sizeof(mct),
				"%.16llx %.2u %.3u%s %c %s not forwarding: no next hop",
				guid,
				prefix,
				depth,
				mctdepth,
				(_r->topology->amSupernode() ? 'S' : '-'),
				_r->identity.address().toString().c_str());
			_r->demarc->send(Demarc::ANY_PORT,ZT_DEFAULTS.multicastTraceWatcher,mct,strlen(mct),-1);
#endif
			//TRACE("not forwarding MULTICAST_FRAME from %s(%s): no next hop",source().toString().c_str(),_remoteAddress.toString().c_str());
			return true;
		}

		// The rest of newFifo[] goes back into the packet
		memcpy(fifo,newFifo + ZT_ADDRESS_LENGTH,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_FIFO);

#ifdef ZT_TRACE_MULTICAST
		Utils::snprintf(mct,sizeof(mct),
			"%.16llx %.2u %.3u%s %c %s -> origin %s, sending to next hop %s, +fifosize:%u",
			guid,
			prefix,
			depth,
			mctdepth,
			(_r->topology->amSupernode() ? 'S' : '-'),
			_r->identity.address().toString().c_str(),
			origin.toString().c_str(),
			nextHop.toString().c_str(),
			numAdded);
		_r->demarc->send(Demarc::ANY_PORT,ZT_DEFAULTS.multicastTraceWatcher,mct,strlen(mct),-1);
#endif

		// Send to next hop, reusing this packet as scratch space
		newInitializationVector();
		setDestination(nextHop);
		setSource(_r->identity.address());
		compress(); // note: bloom filters and empty FIFOs are highly compressable!
		_r->sw->send(*this,true);

		return true;
	} catch (std::exception &ex) {
		TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}

	return true;
}

bool PacketDecoder::_doMULTICAST_LIKE(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		Address src(source());
		uint64_t now = Utils::now();

		// Iterate through 18-byte network,MAC,ADI tuples
		for(unsigned int ptr=ZT_PACKET_IDX_PAYLOAD;ptr<size();ptr+=18) {
			uint64_t nwid = at<uint64_t>(ptr);
			SharedPtr<Network> network(_r->nc->network(nwid));
			if ((_r->topology->amSupernode())||((network)&&(network->isAllowed(peer->address())))) {
				_r->mc->likesGroup(nwid,src,MulticastGroup(MAC(field(ptr + 8,6)),at<uint32_t>(ptr + 14)),now);
				if (network)
					network->pushMembershipCertificate(peer->address(),false,now);
			}
		}

		peer->onReceive(_r,_localPort,_remoteAddress,hops(),packetId(),Packet::VERB_MULTICAST_LIKE,0,Packet::VERB_NOP,now);
	} catch (std::exception &ex) {
		TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool PacketDecoder::_doNETWORK_MEMBERSHIP_CERTIFICATE(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		CertificateOfMembership com;

		unsigned int ptr = ZT_PACKET_IDX_PAYLOAD;
		while (ptr < size()) {
			ptr += com.deserialize(*this,ptr);
			if ((com.hasRequiredFields())&&(com.signedBy())) {
				SharedPtr<Peer> signer(_r->topology->getPeer(com.signedBy()));
				if (signer) {
					if (com.verify(signer->identity())) {
						uint64_t nwid = com.networkId();
						SharedPtr<Network> network(_r->nc->network(nwid));
						if (network) {
							if (network->controller() == signer)
								network->addMembershipCertificate(com);
						}
					}
				} else {
					_r->sw->requestWhois(com.signedBy());
					_step = DECODE_WAITING_FOR_NETWORK_MEMBERSHIP_CERTIFICATE_SIGNER_LOOKUP;
					return false;
				}
			}
		}

		peer->onReceive(_r,_localPort,_remoteAddress,hops(),packetId(),Packet::VERB_NETWORK_MEMBERSHIP_CERTIFICATE,0,Packet::VERB_NOP,Utils::now());
	} catch (std::exception &ex) {
		TRACE("dropped NETWORK_MEMBERSHIP_CERTIFICATE from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped NETWORK_MEMBERSHIP_CERTIFICATE from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool PacketDecoder::_doNETWORK_CONFIG_REQUEST(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_NETWORK_ID);
#ifndef __WINDOWS__
		if (_r->netconfService) {
			char tmp[128];
			unsigned int dictLen = at<uint16_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT_LEN);

			Dictionary request;
			if (dictLen)
				request["meta"] = std::string((const char *)field(ZT_PROTO_VERB_NETWORK_CONFIG_REQUEST_IDX_DICT,dictLen),dictLen);
			request["type"] = "netconf-request";
			request["peerId"] = peer->identity().toString(false);
			Utils::snprintf(tmp,sizeof(tmp),"%llx",(unsigned long long)nwid);
			request["nwid"] = tmp;
			Utils::snprintf(tmp,sizeof(tmp),"%llx",(unsigned long long)packetId());
			request["requestId"] = tmp;
			if (!hops())
				request["from"] = _remoteAddress.toString();
			//TRACE("to netconf:\n%s",request.toString().c_str());
			_r->netconfService->send(request);
		} else {
#endif // !__WINDOWS__
			Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
			outp.append(packetId());
			outp.append((unsigned char)Packet::ERROR_UNSUPPORTED_OPERATION);
			outp.append(nwid);
			outp.armor(peer->key(),true);
			_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
#ifndef __WINDOWS__
		}
#endif // !__WINDOWS__
		peer->onReceive(_r,_localPort,_remoteAddress,hops(),packetId(),Packet::VERB_NETWORK_CONFIG_REQUEST,0,Packet::VERB_NOP,Utils::now());
	} catch (std::exception &exc) {
		TRACE("dropped NETWORK_CONFIG_REQUEST from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),exc.what());
	} catch ( ... ) {
		TRACE("dropped NETWORK_CONFIG_REQUEST from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool PacketDecoder::_doNETWORK_CONFIG_REFRESH(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		unsigned int ptr = ZT_PACKET_IDX_PAYLOAD;
		while ((ptr + sizeof(uint64_t)) <= size()) {
			uint64_t nwid = at<uint64_t>(ptr); ptr += sizeof(uint64_t);
			SharedPtr<Network> nw(_r->nc->network(nwid));
			if ((nw)&&(source() == nw->controller())) { // only respond to requests from controller
				TRACE("NETWORK_CONFIG_REFRESH from %s, refreshing network %.16llx",source().toString().c_str(),nwid);
				nw->requestConfiguration();
			}
		}
		peer->onReceive(_r,_localPort,_remoteAddress,hops(),packetId(),Packet::VERB_NETWORK_CONFIG_REFRESH,0,Packet::VERB_NOP,Utils::now());
	} catch (std::exception &exc) {
		TRACE("dropped NETWORK_CONFIG_REFRESH from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),exc.what());
	} catch ( ... ) {
		TRACE("dropped NETWORK_CONFIG_REFRESH from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

} // namespace ZeroTier
