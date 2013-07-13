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

#include "RuntimeEnvironment.hpp"
#include "Topology.hpp"
#include "PacketDecoder.hpp"
#include "Switch.hpp"
#include "Peer.hpp"
#include "NodeConfig.hpp"
#include "Filter.hpp"

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
		if (_step == DECODE_STEP_WAITING_FOR_ORIGINAL_SUBMITTER_LOOKUP) {
			// This means we've already decoded, decrypted, decompressed, and
			// validated, and we're processing a MULTICAST_FRAME. We're waiting
			// for a lookup on the frame's original submitter. So try again and
			// see if we have it.
			return _doMULTICAST_FRAME(_r,peer);
		}

		if (!hmacVerify(peer->macKey())) {
			TRACE("dropped packet from %s(%s), HMAC authentication failed (size: %u)",source().toString().c_str(),_remoteAddress.toString().c_str(),size());
			return true;
		}

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

		// Validated packets that have passed HMAC can result in us learning a new
		// path to this peer.
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
			default:
				// This might be something from a new or old version of the protocol.
				// Technically it passed HMAC so the packet is still valid, but we
				// ignore it.
				TRACE("ignored unrecognized verb %.2x from %s(%s)",(unsigned int)v,source().toString().c_str(),_remoteAddress.toString().c_str());
				return true;
		}
	} else {
		_step = DECODE_STEP_WAITING_FOR_SENDER_LOOKUP;
		_r->sw->requestWhois(source());
		return false;
	}
}

void PacketDecoder::_CBaddPeerFromHello(void *arg,const SharedPtr<Peer> &p,Topology::PeerVerifyResult result)
{
	_CBaddPeerFromHello_Data *req = (_CBaddPeerFromHello_Data *)arg;
	const RuntimeEnvironment *_r = req->renv;

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

	delete req;
}

void PacketDecoder::_CBaddPeerFromWhois(void *arg,const SharedPtr<Peer> &p,Topology::PeerVerifyResult result)
{
	switch(result) {
		case Topology::PEER_VERIFY_ACCEPTED_NEW:
		case Topology::PEER_VERIFY_ACCEPTED_ALREADY_HAVE:
		case Topology::PEER_VERIFY_ACCEPTED_DISPLACED_INVALID_ADDRESS:
			((const RuntimeEnvironment *)arg)->sw->doAnythingWaitingForPeer(p);
			break;
		default:
			break;
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

		SharedPtr<Peer> candidate(new Peer(_r->identity,id));
		candidate->setPathAddress(_remoteAddress,false);

		// Initial sniff test
		if (id.address().isReserved()) {
			TRACE("rejected HELLO from %s(%s): identity has reserved address",source().toString().c_str(),_remoteAddress.toString().c_str());
			Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(packetId());
			outp.append((unsigned char)Packet::ERROR_IDENTITY_INVALID);
			outp.encrypt(candidate->cryptKey());
			outp.hmacSet(candidate->macKey());
			_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
			return true;
		}
		if (id.address() != source()) {
			TRACE("rejected HELLO from %s(%s): identity is not for sender of packet (HELLO is a self-announcement)",source().toString().c_str(),_remoteAddress.toString().c_str());
			Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(packetId());
			outp.append((unsigned char)Packet::ERROR_INVALID_REQUEST);
			outp.encrypt(candidate->cryptKey());
			outp.hmacSet(candidate->macKey());
			_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
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

		// Otherwise we call addPeer() and set up a callback to handle the verdict
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
			case Packet::VERB_WHOIS:
				// Right now we only query supernodes for WHOIS and only accept
				// OK back from them. If we query other nodes, we'll have to
				// do something to prevent WHOIS cache poisoning such as
				// using the packet ID field in the OK packet to match with the
				// original query. Technically we should be doing this anyway.
				TRACE("%s(%s): OK(%s)",source().toString().c_str(),_remoteAddress.toString().c_str(),Packet::verbString(inReVerb));
				if (_r->topology->isSupernode(source()))
					_r->topology->addPeer(SharedPtr<Peer>(new Peer(_r->identity,Identity(*this,ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY))),&PacketDecoder::_CBaddPeerFromWhois,const_cast<void *>((const void *)_r));
				break;
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
		SharedPtr<Peer> p(_r->topology->getPeer(Address(payload())));
		if (p) {
			Packet outp(source(),_r->identity.address(),Packet::VERB_OK);
			outp.append((unsigned char)Packet::VERB_WHOIS);
			outp.append(packetId());
			p->identity().serialize(outp,false);
			outp.encrypt(peer->cryptKey());
			outp.hmacSet(peer->macKey());
			_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
			TRACE("sent WHOIS response to %s for %s",source().toString().c_str(),Address(payload()).toString().c_str());
		} else {
			Packet outp(source(),_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_WHOIS);
			outp.append(packetId());
			outp.append((unsigned char)Packet::ERROR_NOT_FOUND);
			outp.append(payload(),ZT_ADDRESS_LENGTH);
			outp.encrypt(peer->cryptKey());
			outp.hmacSet(peer->macKey());
			_r->demarc->send(_localPort,_remoteAddress,outp.data(),outp.size(),-1);
			TRACE("sent WHOIS ERROR to %s for %s (not found)",source().toString().c_str(),Address(payload()).toString().c_str());
		}
	} else {
		TRACE("dropped WHOIS from %s(%s): missing or invalid address",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

bool PacketDecoder::_doRENDEZVOUS(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer)
{
	try {
		Address with(field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ZTADDRESS,ZT_ADDRESS_LENGTH));
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
				if ((etherType != ZT_ETHERTYPE_ARP)&&(etherType != ZT_ETHERTYPE_IPV4)&&(etherType != ZT_ETHERTYPE_IPV6)) {
					TRACE("dropped FRAME from %s: unsupported ethertype",source().toString().c_str());
				} else if (size() > ZT_PROTO_VERB_FRAME_IDX_PAYLOAD) {
					network->tap().put(source().toMAC(),network->tap().mac(),etherType,data() + ZT_PROTO_VERB_FRAME_IDX_PAYLOAD,size() - ZT_PROTO_VERB_FRAME_IDX_PAYLOAD);
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
			if (network) {
				if (network->isAllowed(source())) {
					MAC mac(field(ptr,6)); ptr += 6;
					uint32_t adi = at<uint32_t>(ptr); ptr += 4;
					//TRACE("peer %s likes multicast group %s:%.8lx on network %llu",source().toString().c_str(),mac.toString().c_str(),(unsigned long)adi,nwid);
					_r->multicaster->likesMulticastGroup(nwid,MulticastGroup(mac,adi),source(),now);
					++numAccepted;
				} else {
					TRACE("ignored MULTICAST_LIKE from %s(%s): not a member of closed network %llu",source().toString().c_str(),_remoteAddress.toString().c_str(),nwid);
				}
			} else {
				TRACE("ignored MULTICAST_LIKE from %s(%s): network %llu unknown or we are not a member",source().toString().c_str(),_remoteAddress.toString().c_str(),nwid);
			}
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
		if (network) {
			if (network->isAllowed(source())) {
				if (size() > ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD) {

					Address originalSubmitterAddress(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SUBMITTER_ADDRESS,ZT_ADDRESS_LENGTH));
					MAC fromMac(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SOURCE_MAC,6));
					MulticastGroup mg(MAC(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DESTINATION_MAC,6)),at<uint32_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ADI));
					unsigned int hops = (*this)[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_HOP_COUNT];
					unsigned int etherType = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ETHERTYPE);
					unsigned int datalen = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD_LENGTH);
					unsigned int signaturelen = at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SIGNATURE_LENGTH);
					unsigned char *dataAndSignature = field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD,datalen + signaturelen);

					uint64_t mccrc = Multicaster::computeMulticastDedupCrc(network->id(),fromMac,mg,etherType,dataAndSignature,datalen);
					uint64_t now = Utils::now();
					bool isDuplicate = _r->multicaster->checkDuplicate(mccrc,now);

					if (originalSubmitterAddress == _r->identity.address()) {
						// Technically should not happen, since the original submitter is
						// excluded from consideration as a propagation recipient.
						TRACE("dropped boomerang MULTICAST_FRAME received from %s(%s)",source().toString().c_str(),_remoteAddress.toString().c_str());
					} else if ((!isDuplicate)||(_r->topology->isSupernode(_r->identity.address()))) {
						// If I am a supernode, I will repeatedly propagate duplicates. That's
						// because supernodes are used to bridge sparse multicast groups. Non-
						// supernodes will ignore duplicates completely.
						SharedPtr<Peer> originalSubmitter(_r->topology->getPeer(originalSubmitterAddress));
						if (!originalSubmitter) {
							TRACE("requesting WHOIS on original multicast frame submitter %s",originalSubmitterAddress.toString().c_str());
							_r->sw->requestWhois(originalSubmitterAddress);
							_step = DECODE_STEP_WAITING_FOR_ORIGINAL_SUBMITTER_LOOKUP;
							return false;
						} else if (Multicaster::verifyMulticastPacket(originalSubmitter->identity(),network->id(),fromMac,mg,etherType,dataAndSignature,datalen,dataAndSignature + datalen,signaturelen)) {
							_r->multicaster->addToDedupHistory(mccrc,now);

							if (!isDuplicate)
								network->tap().put(fromMac,mg.mac(),etherType,dataAndSignature,datalen);

							if (++hops < ZT_MULTICAST_PROPAGATION_DEPTH) {
								Address upstream(source()); // save this since we mangle it

								Multicaster::MulticastBloomFilter bloom(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_BLOOM_FILTER,ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE_BYTES));
								SharedPtr<Peer> propPeers[ZT_MULTICAST_PROPAGATION_BREADTH];
								unsigned int np = _r->multicaster->pickNextPropagationPeers(
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

								// In a bit of a hack, we re-use this packet to repeat it
								// to our multicast propagation recipients. Afterwords we
								// return true just to be sure this is the end of this
								// packet's life cycle, since it is now mangled.

								setSource(_r->identity.address());
								(*this)[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_HOP_COUNT] = hops;
								memcpy(field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_BLOOM_FILTER,ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE_BYTES),bloom.data(),ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE_BYTES);
								compress();

								for(unsigned int i=0;i<np;++i) {
									TRACE("propagating multicast from original node %s: %s -> %s",originalSubmitterAddress.toString().c_str(),upstream.toString().c_str(),propPeers[i]->address().toString().c_str());
									// Re-use this packet to re-send multicast frame to everyone
									// downstream from us.
									newInitializationVector();
									setDestination(propPeers[i]->address());
									_r->sw->send(*this,true);
								}

								return true;
							} else {
								TRACE("terminating MULTICAST_FRAME propagation from %s(%s): max depth reached",source().toString().c_str(),_remoteAddress.toString().c_str());
							}
						} else {
							LOG("rejected MULTICAST_FRAME from %s(%s) due to failed signature check (claims original sender %s)",source().toString().c_str(),_remoteAddress.toString().c_str(),originalSubmitterAddress.toString().c_str());
						}
					} else {
						TRACE("dropped redundant MULTICAST_FRAME from %s(%s)",source().toString().c_str(),_remoteAddress.toString().c_str());
					}
				} else {
					TRACE("dropped MULTICAST_FRAME from %s(%s): invalid short packet",source().toString().c_str(),_remoteAddress.toString().c_str());
				}
			} else {
				TRACE("dropped MULTICAST_FRAME from %s(%s): not a member of closed network %llu",source().toString().c_str(),_remoteAddress.toString().c_str(),network->id());
			}
		} else {
			TRACE("dropped MULTICAST_FRAME from %s(%s): network %llu unknown or we are not a member",source().toString().c_str(),_remoteAddress.toString().c_str(),at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID));
		}
	} catch (std::exception &ex) {
		TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception: %s",source().toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception: (unknown)",source().toString().c_str(),_remoteAddress.toString().c_str());
	}
	return true;
}

} // namespace ZeroTier
