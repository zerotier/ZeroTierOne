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

#include "../version.h"

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "Topology.hpp"
#include "PacketDecoder.hpp"
#include "Switch.hpp"
#include "Peer.hpp"
#include "NodeConfig.hpp"
#include "Filter.hpp"
#include "Service.hpp"

namespace ZeroTier {

bool PacketDecoder::tryDecode(const RuntimeEnvironment *_r)
	throw(std::out_of_range,std::runtime_error)
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
		}

		if (!dearmor(peer->key())) {
			TRACE("dropped packet from %s(%s), MAC authentication failed (size: %u)",source().toString().c_str(),_remoteAddress.toString().c_str(),size());
			return true;
		}
		if (!uncompress()) {
			TRACE("dropped packet from %s(%s), compressed data invalid",source().toString().c_str(),_remoteAddress.toString().c_str());
			return true;
		}

		Packet::Verb v = verb();

		// Once a packet is determined to be basically valid, it can be used
		// to passively learn a new network path to the sending peer. It
		// also results in statistics updates.
		peer->onReceive(_r,_localPort,_remoteAddress,hops(),v,Utils::now());

		switch(v) {
			case Packet::VERB_NOP:
				TRACE("NOP from %s(%s)",source().toString().c_str(),_remoteAddress.toString().c_str());
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
			case Packet::VERB_PROXY_FRAME:
				return _doPROXY_FRAME(_r,peer);
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
				TRACE("ignored unrecognized verb %.2x from %s(%s)",(unsigned int)v,source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
		}
	} else {
		_step = DECODE_WAITING_FOR_SENDER_LOOKUP; // should already be this...
		_r->sw->requestWhois(source());
		return false;
	}
}

void PacketDecoder::_CBaddPeerFromHello(void *arg,const SharedPtr<Peer> &p,Topology::PeerVerifyResult result)
{
	_CBaddPeerFromHello_Data *req = (_CBaddPeerFromHello_Data *)arg;
	const RuntimeEnvironment *_r = req->renv;

	try {
		switch(result) {
			case Topology::PEER_VERIFY_ACCEPTED_NEW:
			case Topology::PEER_VERIFY_ACCEPTED_ALREADY_HAVE:
			case Topology::PEER_VERIFY_ACCEPTED_DISPLACED_INVALID_ADDRESS: {
				_r->sw->doAnythingWaitingForPeer(p);
				Packet outp(req->source,_r->identity.address(),Packet::VERB_OK);
				outp.append((unsigned char)Packet::VERB_HELLO);
				outp.append(req->helloPacketId);
				outp.append(req->helloTimestamp);
				outp.append((unsigned char)ZT_PROTO_VERSION);
				outp.append((unsigned char)ZEROTIER_ONE_VERSION_MAJOR);
				outp.append((unsigned char)ZEROTIER_ONE_VERSION_MINOR);
				outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
				outp.armor(p->key(),true);
				_r->demarc->send(req->localPort,req->remoteAddress,outp.data(),outp.size(),-1);
			}	break;

			case Topology::PEER_VERIFY_REJECTED_INVALID_IDENTITY: {
				Packet outp(req->source,_r->identity.address(),Packet::VERB_ERROR);
				outp.append((unsigned char)Packet::VERB_HELLO);
				outp.append(req->helloPacketId);
				outp.append((unsigned char)Packet::ERROR_IDENTITY_INVALID);
				outp.armor(p->key(),true);
				_r->demarc->send(req->localPort,req->remoteAddress,outp.data(),outp.size(),-1);
			}	break;

			case Topology::PEER_VERIFY_REJECTED_DUPLICATE:
			case Topology::PEER_VERIFY_REJECTED_DUPLICATE_TRIAGED: {
				Packet outp(req->source,_r->identity.address(),Packet::VERB_ERROR);
				outp.append((unsigned char)Packet::VERB_HELLO);
				outp.append(req->helloPacketId);
				outp.append((unsigned char)Packet::ERROR_IDENTITY_COLLISION);
				outp.armor(p->key(),true);
				_r->demarc->send(req->localPort,req->remoteAddress,outp.data(),outp.size(),-1);
			}	break;
		}
	} catch ( ... ) {
		TRACE("unexpected exception in addPeer() result callback for peer received via HELLO");
	}

	delete req;
}

void PacketDecoder::_CBaddPeerFromWhois(void *arg,const SharedPtr<Peer> &p,Topology::PeerVerifyResult result)
{
	const RuntimeEnvironment *_r = (const RuntimeEnvironment *)arg;
	try {
		switch(result) {
			case Topology::PEER_VERIFY_ACCEPTED_NEW:
			case Topology::PEER_VERIFY_ACCEPTED_ALREADY_HAVE:
			case Topology::PEER_VERIFY_ACCEPTED_DISPLACED_INVALID_ADDRESS:
				_r->sw->doAnythingWaitingForPeer(p);
				break;
			default:
				break;
		}
	} catch ( ... ) {
		TRACE("unexpected exception in addPeer() result callback for peer received via OK(WHOIS)");
	}
}

bool PacketDecoder::_doERROR(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		Packet::Verb inReVerb = (Packet::Verb)(*this)[ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB];
		Packet::ErrorCode errorCode = (Packet::ErrorCode)(*this)[ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE];
		TRACE("ERROR %s from %s(%s) in-re %s",Packet::errorString(errorCode),source().toString().c_str(),_remoteAddress.toString().c_str(),Packet::verbString(inReVerb));

		switch(errorCode) {
			case Packet::ERROR_OBJ_NOT_FOUND:
				if (inReVerb == Packet::VERB_WHOIS) {
					// TODO: abort WHOIS if sender is a supernode
				}
				break;
			case Packet::ERROR_IDENTITY_COLLISION:
			case Packet::ERROR_IDENTITY_INVALID:
				// TODO: if it comes from a supernode, regenerate a new identity
				break;
			case Packet::ERROR_NO_MEMBER_CERTIFICATE:
				// TODO: send member certificate
				break;
			default:
				break;
		}
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
		//unsigned int protoVersion = (*this)[ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION];
		unsigned int vMajor = (*this)[ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION];
		unsigned int vMinor = (*this)[ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION];
		unsigned int vRevision = at<uint16_t>(ZT_PROTO_VERB_HELLO_IDX_REVISION);
		uint64_t timestamp = at<uint64_t>(ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP);
		Identity id(*this,ZT_PROTO_VERB_HELLO_IDX_IDENTITY);

		// Initial sniff test for valid addressing and that this is indeed the
		// submitter's identity.
		if ((id.address().isReserved())||(id.address() != source())) {
#ifdef ZT_TRACE
			if (id.address().isReserved()) {
				TRACE("dropped HELLO from %s(%s): identity has reserved address",source().toString().c_str(),_remoteAddress.toString().c_str());
			} else {
				TRACE("dropped HELLO from %s(%s): identity is not for sender of packet (HELLO is a self-announcement)",source().toString().c_str(),_remoteAddress.toString().c_str());
			}
#endif
			return true;
		}

		// Is this a HELLO for a peer we already know? If so just update its
		// packet receive stats and send an OK.
		SharedPtr<Peer> existingPeer(_r->topology->getPeer(id.address()));
		if ((existingPeer)&&(existingPeer->identity() == id)) {
			existingPeer->onReceive(_r,_localPort,_remoteAddress,hops(),Packet::VERB_HELLO,Utils::now());
			existingPeer->setRemoteVersion(vMajor,vMinor,vRevision);

			Packet outp(source(),_r->identity.address(),Packet::VERB_OK);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(packetId());
			outp.append(timestamp);
			outp.append((unsigned char)ZT_PROTO_VERSION);
			outp.append((unsigned char)ZEROTIER_ONE_VERSION_MAJOR);
			outp.append((unsigned char)ZEROTIER_ONE_VERSION_MINOR);
			outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
			outp.armor(existingPeer->key(),true);
			_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
			return true;
		}

		SharedPtr<Peer> candidate(new Peer(_r->identity,id));
		candidate->setPathAddress(_remoteAddress,false);
		candidate->setRemoteVersion(vMajor,vMinor,vRevision);

		_CBaddPeerFromHello_Data *arg = new _CBaddPeerFromHello_Data;
		arg->renv = _r;
		arg->source = source();
		arg->remoteAddress = _remoteAddress;
		arg->localPort = _localPort;
		arg->vMajor = vMajor;
		arg->vMinor = vMinor;
		arg->vRevision = vRevision;
		arg->helloPacketId = packetId();
		arg->helloTimestamp = timestamp;
		_r->topology->addPeer(candidate,&PacketDecoder::_CBaddPeerFromHello,arg);
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
		switch(inReVerb) {
			case Packet::VERB_HELLO: {
				// OK from HELLO permits computation of latency.
				unsigned int latency = std::min((unsigned int)(Utils::now() - at<uint64_t>(ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP)),(unsigned int)0xffff);
				unsigned int vMajor = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_MAJOR_VERSION];
				unsigned int vMinor = (*this)[ZT_PROTO_VERB_HELLO__OK__IDX_MINOR_VERSION];
				unsigned int vRevision = at<uint16_t>(ZT_PROTO_VERB_HELLO__OK__IDX_REVISION);
				TRACE("%s(%s): OK(HELLO), latency: %u, version %u.%u.%u",source().toString().c_str(),_remoteAddress.toString().c_str(),latency,vMajor,vMinor,vRevision);
				peer->setLatency(_remoteAddress,latency);
				peer->setRemoteVersion(vMajor,vMinor,vRevision);
			}	break;
			case Packet::VERB_WHOIS: {
				TRACE("%s(%s): OK(%s)",source().toString().c_str(),_remoteAddress.toString().c_str(),Packet::verbString(inReVerb));
				if (_r->topology->isSupernode(source())) {
					// Right now, only supernodes are queried for WHOIS so we only
					// accept OK(WHOIS) from supernodes. Otherwise peers could
					// potentially cache-poison.
					_r->topology->addPeer(SharedPtr<Peer>(new Peer(_r->identity,Identity(*this,ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY))),&PacketDecoder::_CBaddPeerFromWhois,const_cast<void *>((const void *)_r));
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
						Network::Config netconf(dict);
						TRACE("got network configuration for network %.16llx from %s",(unsigned long long)nw->id(),source().toString().c_str());
						nw->setConfiguration(netconf);
					}
				}
			}	break;
			default:
				//TRACE("%s(%s): OK(%s)",source().toString().c_str(),_remoteAddress.toString().c_str(),Packet::verbString(inReVerb));
				break;
		}
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
		SharedPtr<Peer> p(_r->topology->getPeer(Address(payload(),ZT_ADDRESS_LENGTH)));
		if (p) {
			Packet outp(source(),_r->identity.address(),Packet::VERB_OK);
			outp.append((unsigned char)Packet::VERB_WHOIS);
			outp.append(packetId());
			p->identity().serialize(outp,false);
			outp.armor(peer->key(),true);
			_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
			TRACE("sent WHOIS response to %s for %s",source().toString().c_str(),Address(payload(),ZT_ADDRESS_LENGTH).toString().c_str());
		} else {
			Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_WHOIS);
			outp.append(packetId());
			outp.append((unsigned char)Packet::ERROR_OBJ_NOT_FOUND);
			outp.append(payload(),ZT_ADDRESS_LENGTH);
			outp.armor(peer->key(),true);
			_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
			TRACE("sent WHOIS ERROR to %s for %s (not found)",source().toString().c_str(),Address(payload(),ZT_ADDRESS_LENGTH).toString().c_str());
		}
	} else {
		TRACE("dropped WHOIS from %s(%s): missing or invalid address",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
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
				if (network->permitsEtherType(etherType)) {
					network->tap().put(source().toMAC(),network->tap().mac(),etherType,data() + ZT_PROTO_VERB_FRAME_IDX_PAYLOAD,size() - ZT_PROTO_VERB_FRAME_IDX_PAYLOAD);
				} else if (size() > ZT_PROTO_VERB_FRAME_IDX_PAYLOAD) {
					TRACE("dropped FRAME from %s: ethernet type %u not allowed on network %.16llx",source().toString().c_str(),etherType,(unsigned long long)network->id());
				}

				// Source moves "closer" to us in multicast propagation priority when
				// we receive unicast frames from it. This is called "implicit social
				// ordering" in other docs.
				_r->mc->bringCloser(network->id(),source());
			} else {
				TRACE("dropped FRAME from %s(%s): not a member of closed network %llu",source().toString().c_str(),_remoteAddress.toString().c_str(),network->id());

				Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
				outp.append((unsigned char)Packet::VERB_FRAME);
				outp.append(packetId());
				outp.append((unsigned char)Packet::ERROR_NO_MEMBER_CERTIFICATE);
				outp.append(network->id());
				outp.armor(peer->key(),true);
				_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
			}
		} else {
			TRACE("dropped FRAME from %s(%s): network %llu unknown",source().toString().c_str(),_remoteAddress.toString().c_str(),at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID));
		}
	} catch (std::exception &ex) {
		TRACE("dropped FRAME from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped FRAME from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool PacketDecoder::_doPROXY_FRAME(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
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

		uint16_t depth = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_DEPTH);
		unsigned char *fifo = field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_FIFO,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_FIFO);
		unsigned char *bloom = field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_BLOOM,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_BLOOM);
		uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID);
		uint16_t bloomNonce = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_BLOOM_NONCE);
		unsigned int prefixBits = (*this)[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_PREFIX_BITS];
		unsigned int prefix = (*this)[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_PREFIX];
		uint64_t guid = at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_GUID);
		MAC sourceMac(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SOURCE_MAC,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_SOURCE_MAC));
		MulticastGroup dest(MAC(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DEST_MAC,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_DEST_MAC)),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DEST_ADI));
		unsigned int etherType = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ETHERTYPE);
		unsigned int frameLen = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME_LEN);
		unsigned char *frame = field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME,frameLen);
		unsigned int signatureLen = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME + frameLen);
		unsigned char *signature = field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME + frameLen + 2,signatureLen);

		unsigned int signedPartLen = (ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME - ZT_PROTO_VERB_MULTICAST_FRAME_IDX__START_OF_SIGNED_PORTION) + frameLen;
		if (!originPeer->identity().verify(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX__START_OF_SIGNED_PORTION,signedPartLen),signedPartLen,signature,signatureLen)) {
			TRACE("dropped MULTICAST_FRAME from %s(%s): failed signature verification, claims to be from %s",source().toString().c_str(),_remoteAddress.toString().c_str(),origin.toString().c_str());
			return true;
		}

		if (!dest.mac().isMulticast()) {
			TRACE("dropped MULTICAST_FRAME from %s(%s): %s is not a multicast/broadcast address",source().toString().c_str(),_remoteAddress.toString().c_str(),dest.mac().toString().c_str());
			return true;
		}

		bool rateLimitsExceeded = false;
		unsigned int maxDepth = ZT_MULTICAST_GLOBAL_MAX_DEPTH;

		if ((origin == _r->identity.address())||(_r->mc->deduplicate(nwid,guid))) {
			// Ordinary frames will drop duplicates. Supernodes keep propagating
			// them since they're used as hubs to link disparate clusters of
			// members of the same multicast group.
			if (!_r->topology->amSupernode()) {
				TRACE("dropped MULTICAST_FRAME from %s(%s): duplicate",source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			}
		} else {
			// Supernodes however won't do this more than once. If the supernode
			// does happen to be a member of the network -- which is usually not
			// true -- we don't want to see a ton of copies of the same frame on
			// its tap device. Also double or triple counting bandwidth metrics
			// for the same frame would not be fair.
			SharedPtr<Network> network(_r->nc->network(nwid));
			if (network) {
				maxDepth = network->multicastDepth(); // pull from network config if available
				if (!network->isAllowed(origin)) {
					TRACE("didn't inject MULTICAST_FRAME from %s(%s) into %.16llx: sender %s not allowed or we don't have a certificate",source().toString().c_str(),nwid,_remoteAddress.toString().c_str(),origin.toString().c_str());

					Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
					outp.append((unsigned char)Packet::VERB_FRAME);
					outp.append(packetId());
					outp.append((unsigned char)Packet::ERROR_NO_MEMBER_CERTIFICATE);
					outp.append(nwid);
					outp.armor(peer->key(),true);
					_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);

					// We do not terminate here, since if the member just has an out of
					// date cert or hasn't sent us a cert yet we still want to propagate
					// the message so multicast works.
				} else if ((!network->permitsBridging())&&(!origin.wouldHaveMac(sourceMac))) {
					TRACE("didn't inject MULTICAST_FRAME from %s(%s) into %.16llx: source mac %s doesn't belong to %s, and bridging is not supported on network",source().toString().c_str(),nwid,_remoteAddress.toString().c_str(),sourceMac.toString().c_str(),origin.toString().c_str());
				} else if (!network->permitsEtherType(etherType)) {
					TRACE("didn't inject MULTICAST_FRAME from %s(%s) into %.16llx: ethertype %u is not allowed",source().toString().c_str(),nwid,_remoteAddress.toString().c_str(),etherType);
				} else if (!network->updateAndCheckMulticastBalance(origin,dest,frameLen)) {
					rateLimitsExceeded = true;
				} else {
					network->tap().put(sourceMac,dest.mac(),etherType,frame,frameLen);
				}
			}
		}

		// We can only really know if rate limit was exceeded if we're a member of
		// this network. This will nearly always be true for anyone getting a
		// multicast except supernodes, so the net effect will be to truncate
		// multicast propagation if the rate limit is exceeded.
		if (rateLimitsExceeded) {
			TRACE("dropped MULTICAST_FRAME from %s(%s): rate limits exceeded for sender %s",source().toString().c_str(),_remoteAddress.toString().c_str(),origin.toString().c_str());
			return true;
		}

		if (depth == 0xffff) {
			TRACE("not forwarding MULTICAST_FRAME from %s(%s): depth == 0xffff (do not forward)",source().toString().c_str(),_remoteAddress.toString().c_str());
			return true;
		}
		if (++depth > maxDepth) {
			TRACE("not forwarding MULTICAST_FRAME from %s(%s): max propagation depth reached",source().toString().c_str(),_remoteAddress.toString().c_str());
			return true;
		}
		setAt(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PROPAGATION_DEPTH,(uint16_t)depth);

		// New FIFO with room for one extra, since head will be next hop
		unsigned char newFifo[ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_FIFO + ZT_ADDRESS_LENGTH];
		unsigned char *newFifoPtr = newFifo;
		unsigned char *newFifoEnd = newFifoPtr + sizeof(newFifo);
		for(unsigned int i=0;i<ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_FIFO;) {
			unsigned int j = i;
			i += ZT_ADDRESS_LENGTH;
			unsigned char zm = 0;
			while (j != i)
				zm |= (*(newFifoPtr++) = fifo[j++]);
			if (!zm) // stop at zero address
				break;
		}

		// Add any next hops we know about to FIFO
		_r->mc->getNextHops(nwid,dest,Multicaster::AddToPropagationQueue(&newFifoPtr,newFifoEnd,bloom,bloomNonce,origin,prefixBits,prefix));

		// Zero-terminate new FIFO if not completely full
		while (newFifoPtr != newFifoEnd)
			*(newFifoPtr++) = (unsigned char)0;

		// First element in newFifo[] is next hop
		Address nextHop(newFifo,ZT_ADDRESS_LENGTH);
		if (!nextHop)
			nextHop = _r->topology->getBestSupernode(&origin,1,true); // exclude origin in case it's itself a supernode
		if ((!nextHop)||(nextHop == _r->identity.address())) { // check against our addr is a sanity check
			TRACE("not forwarding MULTICAST_FRAME from %s(%s): no next hop",source().toString().c_str(),_remoteAddress.toString().c_str());
			return true;
		}

		// The rest of newFifo[] goes back into the packet
		memcpy(fifo,newFifo + ZT_ADDRESS_LENGTH,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_FIFO);

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
		unsigned int ptr = ZT_PACKET_IDX_PAYLOAD;
		if (ptr >= size())
			return true;
		uint64_t now = Utils::now();
		Address src(source());

		// Iterate through 18-byte network,MAC,ADI tuples
		for(;;) {
			_r->mc->likesGroup(at<uint64_t>(ptr),src,MulticastGroup(MAC(field(ptr + 8,6)),at<uint32_t>(ptr + 14)),now);
			if ((ptr += 18) >= size())
				break;
		}
	} catch (std::exception &ex) {
		TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}

	return true;
}

bool PacketDecoder::_doNETWORK_MEMBERSHIP_CERTIFICATE(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	// TODO: not implemented yet, will be needed for private networks.
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
		uint64_t nwid = at<uint64_t>(ZT_PROTO_VERB_NETWORK_CONFIG_REFRESH_IDX_NETWORK_ID);
		SharedPtr<Network> nw(_r->nc->network(nwid));
		if ((nw)&&(source() == nw->controller())) // only respond to requests from controller
			nw->requestConfiguration();
	} catch (std::exception &exc) {
		TRACE("dropped NETWORK_CONFIG_REFRESH from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),exc.what());
	} catch ( ... ) {
		TRACE("dropped NETWORK_CONFIG_REFRESH from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

} // namespace ZeroTier
