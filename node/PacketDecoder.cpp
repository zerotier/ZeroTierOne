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

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "Topology.hpp"
#include "PacketDecoder.hpp"
#include "Switch.hpp"
#include "Peer.hpp"
#include "NodeConfig.hpp"
#include "Filter.hpp"
#include "Service.hpp"

/*
 * The big picture:
 *
 * tryDecode() gets called for a given fully-assembled packet until it returns
 * true or the packet's time to live has been exceeded. The state machine must
 * therefore be re-entrant if it ever returns false. Take care here!
 *
 * Stylistic note:
 *
 * There's a lot of unnecessary if nesting. It's mostly to allow TRACE to
 * print informative messages on every possible reason something gets
 * rejected or fails.
 */

namespace ZeroTier {

bool PacketDecoder::tryDecode(const RuntimeEnvironment *_r)
	throw(std::out_of_range,std::runtime_error)
{
	if ((!encrypted())&&(verb() == Packet::VERB_HELLO)) {
		// Unencrypted HELLOs are handled here since they are used to
		// populate our identity cache in the first place. Thus we might get
		// a HELLO for someone for whom we don't have a Peer record.
		TRACE("HELLO from %s(%s)",source().toString().c_str(),_remoteAddress.toString().c_str());
		return _doHELLO(_r);
	}

	SharedPtr<Peer> peer = _r->topology->getPeer(source());
	if (peer) {
		// Resume saved state?
		if (_step == DECODE_WAITING_FOR_MULTICAST_FRAME_ORIGINAL_SENDER_LOOKUP) {
			// In this state we have already authenticated and decrypted the
			// packet and are waiting for the lookup of the original sender
			// for a multicast frame. So check to see if we've got it.
			return _doMULTICAST_FRAME(_r,peer);
		}

		// No saved state? Verify MAC before we proceed.
		if (!hmacVerify(peer->macKey())) {
			TRACE("dropped packet from %s(%s), HMAC authentication failed (size: %u)",source().toString().c_str(),_remoteAddress.toString().c_str(),size());
			return true;
		}

		// If MAC authentication passed, decrypt and uncompress
		if (encrypted()) {
			decrypt(peer->cryptKey());
		} else {
			// Unencrypted is tolerated in case we want to run this on
			// devices where squeezing out cycles matters. HMAC is
			// what's really important. But log it in debug to catch any
			// packets being mistakenly sent in the clear.
			TRACE("ODD: %s from %s(%s) wasn't encrypted",Packet::verbString(verb()),source().toString().c_str(),_remoteAddress.toString().c_str());
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
				return _doHELLO(_r);
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
			case Packet::VERB_MULTICAST_LIKE:
				return _doMULTICAST_LIKE(_r,peer);
			case Packet::VERB_MULTICAST_FRAME:
				return _doMULTICAST_FRAME(_r,peer);
			case Packet::VERB_NETWORK_MEMBERSHIP_CERTIFICATE:
				return _doNETWORK_MEMBERSHIP_CERTIFICATE(_r,peer);
			case Packet::VERB_NETWORK_CONFIG_REQUEST:
				return _doNETWORK_CONFIG_REQUEST(_r,peer);
			case Packet::VERB_NETWORK_CONFIG_REFRESH:
				return _doNETWORK_CONFIG_REFRESH(_r,peer);
			default:
				// This might be something from a new or old version of the protocol.
				// Technically it passed HMAC so the packet is still valid, but we
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
				outp.encrypt(p->cryptKey());
				outp.hmacSet(p->macKey());
				_r->demarc->send(req->localPort,req->remoteAddress,outp.data(),outp.size(),-1);
			}	break;

			case Topology::PEER_VERIFY_REJECTED_INVALID_IDENTITY: {
				Packet outp(req->source,_r->identity.address(),Packet::VERB_ERROR);
				outp.append((unsigned char)Packet::VERB_HELLO);
				outp.append(req->helloPacketId);
				outp.append((unsigned char)Packet::ERROR_IDENTITY_INVALID);
				outp.encrypt(p->cryptKey());
				outp.hmacSet(p->macKey());
				_r->demarc->send(req->localPort,req->remoteAddress,outp.data(),outp.size(),-1);
			}	break;

			case Topology::PEER_VERIFY_REJECTED_DUPLICATE:
			case Topology::PEER_VERIFY_REJECTED_DUPLICATE_TRIAGED: {
				Packet outp(req->source,_r->identity.address(),Packet::VERB_ERROR);
				outp.append((unsigned char)Packet::VERB_HELLO);
				outp.append(req->helloPacketId);
				outp.append((unsigned char)Packet::ERROR_IDENTITY_COLLISION);
				outp.encrypt(p->cryptKey());
				outp.hmacSet(p->macKey());
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
#ifdef ZT_TRACE
		Packet::Verb inReVerb = (Packet::Verb)(*this)[ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB];
		Packet::ErrorCode errorCode = (Packet::ErrorCode)(*this)[ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE];
		TRACE("ERROR %s from %s(%s) in-re %s",Packet::errorString(errorCode),source().toString().c_str(),_remoteAddress.toString().c_str(),Packet::verbString(inReVerb));
#endif
		// TODO (sorta):
		// The fact is that the protocol works fine without error handling.
		// The only error that really needs to be handled here is duplicate
		// identity collision, which if it comes from a supernode should cause
		// us to restart and regenerate a new identity.
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
			outp.encrypt(existingPeer->cryptKey());
			outp.hmacSet(existingPeer->macKey());
			_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
			return true;
		}

		SharedPtr<Peer> candidate(new Peer(_r->identity,id));
		candidate->setPathAddress(_remoteAddress,false);

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
				TRACE("%s(%s): OK(HELLO), latency: %u",source().toString().c_str(),_remoteAddress.toString().c_str(),latency);
				peer->setLatency(_remoteAddress,latency);
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
						if ((netconf.networkId() == nw->id())&&(netconf.peerAddress() == _r->identity.address())) { // sanity check
							LOG("got network configuration for network %.16llx from %s",(unsigned long long)nw->id(),source().toString().c_str());
							nw->setConfiguration(netconf);
						}
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
			outp.encrypt(peer->cryptKey());
			outp.hmacSet(peer->macKey());
			_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
			TRACE("sent WHOIS response to %s for %s",source().toString().c_str(),Address(payload(),ZT_ADDRESS_LENGTH).toString().c_str());
		} else {
			Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_WHOIS);
			outp.append(packetId());
			outp.append((unsigned char)Packet::ERROR_OBJ_NOT_FOUND);
			outp.append(payload(),ZT_ADDRESS_LENGTH);
			outp.encrypt(peer->cryptKey());
			outp.hmacSet(peer->macKey());
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
			} else {
				TRACE("dropped FRAME from %s(%s): not a member of closed network %llu",source().toString().c_str(),_remoteAddress.toString().c_str(),network->id());
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

bool PacketDecoder::_doMULTICAST_LIKE(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		unsigned int ptr = ZT_PACKET_IDX_PAYLOAD;
		unsigned int numAccepted = 0;
		uint64_t now = Utils::now();

		// Iterate through 18-byte network,MAC,ADI tuples:
		while ((ptr + 18) <= size()) {
			uint64_t nwid = at<uint64_t>(ptr); ptr += 8;
			SharedPtr<Network> network(_r->nc->network(nwid));
			if ((network)&&(network->isAllowed(source()))) {
				MAC mac(field(ptr,6)); ptr += 6;
				uint32_t adi = at<uint32_t>(ptr); ptr += 4;
				//TRACE("peer %s likes multicast group %s:%.8lx on network %llu",source().toString().c_str(),mac.toString().c_str(),(unsigned long)adi,nwid);
				_r->multicaster->likesMulticastGroup(nwid,MulticastGroup(mac,adi),source(),now);
				++numAccepted;
			} else ptr += 10;
		}

		Packet outp(source(),_r->identity.address(),Packet::VERB_OK);
		outp.append((unsigned char)Packet::VERB_MULTICAST_LIKE);
		outp.append(packetId());
		outp.append((uint16_t)numAccepted);
		outp.encrypt(peer->cryptKey());
		outp.hmacSet(peer->macKey());
		_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
	} catch (std::exception &ex) {
		TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool PacketDecoder::_doMULTICAST_FRAME(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		SharedPtr<Network> network(_r->nc->network(at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID)));
		if ((network)&&(network->isAllowed(source()))) {
			Address originalSubmitterAddress(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SUBMITTER_ADDRESS,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);

			if (originalSubmitterAddress.isReserved()) {
				TRACE("dropped MULTICAST_FRAME from original submitter %s, received from %s(%s): invalid original submitter address",originalSubmitterAddress.toString().c_str(),source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			}
			if (originalSubmitterAddress == _r->identity.address()) {
				TRACE("dropped MULTICAST_FRAME from original submitter %s, received from %s(%s): boomerang!",originalSubmitterAddress.toString().c_str(),source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			}

			SharedPtr<Peer> originalSubmitter(_r->topology->getPeer(originalSubmitterAddress));
			if (!originalSubmitter) {
				TRACE("requesting WHOIS on original multicast frame submitter %s",originalSubmitterAddress.toString().c_str());
				_r->sw->requestWhois(originalSubmitterAddress);
				_step = DECODE_WAITING_FOR_MULTICAST_FRAME_ORIGINAL_SENDER_LOOKUP;
				return false; // try again if/when we get OK(WHOIS)
			}

			MAC fromMac(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SOURCE_MAC,6));
			MulticastGroup mg(MAC(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DESTINATION_MAC,6)),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ADI));
			unsigned int hops = (*this)[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_HOP_COUNT];
			unsigned int etherType = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ETHERTYPE);
			unsigned int datalen = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD_LENGTH);
			unsigned int signaturelen = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SIGNATURE_LENGTH);
			unsigned char *dataAndSignature = field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD,datalen + signaturelen);

			if (!Multicaster::verifyMulticastPacket(originalSubmitter->identity(),network->id(),fromMac,mg,etherType,dataAndSignature,datalen,dataAndSignature + datalen,signaturelen)) {
				LOG("dropped MULTICAST_FRAME from original submitter %s, received from %s(%s): FAILED SIGNATURE CHECK (spoofed original submitter?)",originalSubmitterAddress.toString().c_str(),source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			}

			if (!network->permitsEtherType(etherType)) {
				LOG("dropped MULTICAST_FRAME from original submitter %s, received from %s(%s): ethernet type %s not allowed on network %.16llx",originalSubmitterAddress.toString().c_str(),source().toString().c_str(),_remoteAddress.toString().c_str(),Filter::etherTypeName(etherType),(unsigned long long)network->id());
				return true;
			}

			uint64_t mccrc = Multicaster::computeMulticastDedupCrc(network->id(),fromMac,mg,etherType,dataAndSignature,datalen);
			uint64_t now = Utils::now();
			bool isDuplicate = _r->multicaster->checkDuplicate(mccrc,now);

			if (!isDuplicate) {
				//if (network->multicastRateGate(originalSubmitterAddress,datalen)) {
					network->tap().put(fromMac,mg.mac(),etherType,dataAndSignature,datalen);
				//} else {
				//	TRACE("dropped MULTICAST_FRAME from original submitter %s, received from %s(%s): sender rate limit exceeded",originalSubmitterAddress.toString().c_str(),source().toString().c_str(),_remoteAddress.toString().c_str());
				//	return true;
				//}

				/* It's important that we do this *after* rate limit checking,
				 * otherwise supernodes could be used to execute a flood by
				 * first bouncing a multicast off a supernode and then flooding
				 * it with retransmits. */
				_r->multicaster->addToDedupHistory(mccrc,now);
			}

			if (++hops >= ZT_MULTICAST_PROPAGATION_DEPTH) {
				TRACE("not propagating MULTICAST_FRAME from original submitter %s, received from %s(%s): max depth reached",originalSubmitterAddress.toString().c_str(),source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			}

			Address upstream(source()); // save this since we might mangle it below
			Multicaster::MulticastBloomFilter bloom(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_BLOOM_FILTER,ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE_BYTES));
			SharedPtr<Peer> propPeers[ZT_MULTICAST_PROPAGATION_BREADTH];
			unsigned int np = 0;

			if (_r->topology->amSupernode()) {
				/* Supernodes behave differently here from ordinary nodes, as their
				 * role in the network is to bridge gaps between unconnected islands
				 * in a multicast propagation graph. Instead of using the ordinary
				 * multicast peer picker, supernodes propagate to random unvisited
				 * peers. They will also repeatedly propagate duplicate multicasts to
				 * new peers, while regular nodes simply discard them. This allows
				 * such gaps to be bridged more than once by ping-ponging off the
				 * same supernode -- a simple way to implement this without requiring
				 * that supernodes maintain a lot of state at the cost of a small
				 * amount of bandwidth. */
				np = _r->multicaster->pickRandomPropagationPeers(
					*(_r->prng),
					*(_r->topology),
					network->id(),
					mg,
					originalSubmitterAddress,
					upstream,
					bloom,
					ZT_MULTICAST_PROPAGATION_BREADTH,
					propPeers,
					now);
			} else if (isDuplicate) {
				TRACE("dropped MULTICAST_FRAME from original submitter %s, received from %s(%s): duplicate",originalSubmitterAddress.toString().c_str(),source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			} else {
				/* Regular peers only propagate non-duplicate packets, and do so
				 * according to ordinary propagation priority rules. */
				np = _r->multicaster->pickSocialPropagationPeers(
					*(_r->prng),
					*(_r->topology),
					network->id(),
					mg,
					originalSubmitterAddress,
					upstream,
					bloom,
					ZT_MULTICAST_PROPAGATION_BREADTH,
					propPeers,
					now);
			}

			/* Re-use *this* packet to repeat it to our propagation
			 * recipients, which invalidates its current contents and
			 * state. */

			if (np) {
				setSource(_r->identity.address());
				(*this)[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_HOP_COUNT] = hops;
				memcpy(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_BLOOM_FILTER,ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE_BYTES),bloom.data(),ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE_BYTES);
				compress();
				for(unsigned int i=0;i<np;++i) {
					newInitializationVector();
					setDestination(propPeers[i]->address());
					_r->sw->send(*this,true);
				}
			}

			/* Just to be safe, return true here to terminate processing as we
			 * have thoroughly destroyed our state by doing the above. */
			return true;
		} else {
			TRACE("dropped MULTICAST_FRAME from %s(%s): network %.16llx unknown or sender not allowed",source().toString().c_str(),_remoteAddress.toString().c_str(),(unsigned long long)network->id());
		}
	} catch (std::exception &ex) {
		TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
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
			outp.encrypt(peer->cryptKey());
			outp.hmacSet(peer->macKey());
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
