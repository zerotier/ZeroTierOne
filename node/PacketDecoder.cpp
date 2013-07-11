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

namespace ZeroTier {

bool PacketDecoder::tryDecode(const RuntimeEnvironment *_r)
	throw(std::out_of_range,std::runtime_error)
{
	Address source(source());

	if ((!encrypted())&&(verb() == Packet::VERB_HELLO)) {
		// Unencrypted HELLOs are handled here since they are used to
		// populate our identity cache in the first place. Thus we might get
		// a HELLO for someone for whom we don't have a Peer record.
		TRACE("HELLO from %s(%s)",source.toString().c_str(),_remoteAddress.toString().c_str());
		return _doHELLO(_r);
	}

	if (_step == DECODE_STEP_WAITING_FOR_ORIGINAL_SUBMITTER_LOOKUP) {
		// This means we've already decoded, decrypted, decompressed, and
		// validated, and we're processing a MULTICAST_FRAME. We're waiting
		// for a lookup on the frame's original submitter.
		return _doMULTICAST_FRAME(_r);
	}

	SharedPtr<Peer> peer = _r->topology->getPeer(source);
	if (peer) {
		if (!hmacVerify(peer->macKey())) {
			TRACE("dropped packet from %s(%s), HMAC authentication failed (size: %u)",source.toString().c_str(),_remoteAddress.toString().c_str(),size());
			return true;
		}

		if (encrypted()) {
			decrypt(peer->cryptKey());
		} else {
			// Unencrypted is tolerated in case we want to run this on
			// devices where squeezing out cycles matters. HMAC is
			// what's really important.
			TRACE("ODD: %s from %s(%s) wasn't encrypted",Packet::verbString(verb()),source.toString().c_str(),_remoteAddress.toString().c_str());
		}

		if (!uncompress()) {
			TRACE("dropped packet from %s(%s), compressed data invalid",source.toString().c_str(),_remoteAddress.toString().c_str());
			return true;
		}

		// Validated packets that have passed HMAC can result in us learning a new
		// path to this peer.
		peer->onReceive(_r,localPort,_remoteAddress,hops(),verb(),Utils::now());

		switch(verb()) {
			case Packet::VERB_NOP:
				TRACE("NOP from %s(%s)",source.toString().c_str(),_remoteAddress.toString().c_str());
				return true;
			case Packet::VERB_HELLO:
				return _doHELLO(_r);
			case Packet::VERB_ERROR:
				return _doERROR(_r);
			case Packet::VERB_OK:
				return _doOK(_r);
			case Packet::VERB_WHOIS:
				return _doWHOIS(_r);
			case Packet::VERB_RENDEZVOUS:
				return _doRENDEZVOUS(_r);
			case Packet::VERB_FRAME:
				return _doFRAME(_r);
			case Packet::VERB_MULTICAST_LIKE:
				return _doMULTICAST_LIKE(_r);
			case Packet::VERB_MULTICAST_FRAME:
				return _doMULTICAST_FRAME(_r);
			default:
				// This might be something from a new or old version of the protocol.
				// Technically it passed HMAC so the packet is still valid, but we
				// ignore it.
				TRACE("ignored unrecognized verb %.2x from %s(%s)",(unsigned int)packet.verb(),source.toString().c_str(),_remoteAddress.toString().c_str());
				return true;
		}
	} else {
		_r->sw->requestWhois(source);
		return false;
	}
}

bool PacketDecoder::_doERROR(const RuntimeEnvironment *_r)
{
	try {
#ifdef ZT_TRACE
		Packet::Verb inReVerb = (Packet::Verb)packet[ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB];
		Packet::ErrorCode errorCode = (Packet::ErrorCode)packet[ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE];
		TRACE("ERROR %s from %s(%s) in-re %s",Packet::errorString(errorCode),source.toString().c_str(),_remoteAddress.toString().c_str(),Packet::verbString(inReVerb));
#endif
		// TODO (sorta):
		// The fact is that the protocol works fine without error handling.
		// The only error that really needs to be handled here is duplicate
		// identity collision, which if it comes from a supernode should cause
		// us to restart and regenerate a new identity.
	} catch (std::exception &ex) {
		TRACE("dropped ERROR from %s(%s): unexpected exception: %s",source.toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped ERROR from %s(%s): unexpected exception: (unknown)",source.toString().c_str(),_remoteAddress.toString().c_str());
	}
}

bool PacketDecoder::_doHELLO(const RuntimeEnvironment *_r)
{
	Address source(source());

	try {
		unsigned int protoVersion = packet[ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION];
		unsigned int vMajor = packet[ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION];
		unsigned int vMinor = packet[ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION];
		unsigned int vRevision = packet.at<uint16_t>(ZT_PROTO_VERB_HELLO_IDX_REVISION);
		uint64_t timestamp = packet.at<uint64_t>(ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP);
		Identity id(packet,ZT_PROTO_VERB_HELLO_IDX_IDENTITY);

		SharedPtr<Peer> candidate(new Peer(_r->identity,id));
		candidate->setPathAddress(_remoteAddress,false);

		// Initial sniff test
		if (protoVersion != ZT_PROTO_VERSION) {
			TRACE("rejected HELLO from %s(%s): invalid protocol version",source.toString().c_str(),_remoteAddress.toString().c_str());
			Packet outp(source,_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(packet.packetId());
			outp.append((unsigned char)Packet::ERROR_BAD_PROTOCOL_VERSION);
			outp.encrypt(candidate->cryptKey());
			outp.hmacSet(candidate->macKey());
			_r->demarc->send(localPort,_remoteAddress,outp.data(),outp.size(),-1);
			return;
		}
		if (id.address().isReserved()) {
			TRACE("rejected HELLO from %s(%s): identity has reserved address",source.toString().c_str(),_remoteAddress.toString().c_str());
			Packet outp(source,_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(packet.packetId());
			outp.append((unsigned char)Packet::ERROR_IDENTITY_INVALID);
			outp.encrypt(candidate->cryptKey());
			outp.hmacSet(candidate->macKey());
			_r->demarc->send(localPort,_remoteAddress,outp.data(),outp.size(),-1);
			return;
		}
		if (id.address() != source) {
			TRACE("rejected HELLO from %s(%s): identity is not for sender of packet (HELLO is a self-announcement)",source.toString().c_str(),_remoteAddress.toString().c_str());
			Packet outp(source,_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(packet.packetId());
			outp.append((unsigned char)Packet::ERROR_INVALID_REQUEST);
			outp.encrypt(candidate->cryptKey());
			outp.hmacSet(candidate->macKey());
			_r->demarc->send(localPort,_remoteAddress,outp.data(),outp.size(),-1);
			return;
		}

		// Is this a HELLO for a peer we already know? If so just update its
		// packet receive stats and send an OK.
		SharedPtr<Peer> existingPeer(_r->topology->getPeer(id.address()));
		if ((existingPeer)&&(existingPeer->identity() == id)) {
			existingPeer->onReceive(_r,localPort,_remoteAddress,0,packet.hops(),Packet::VERB_HELLO,Utils::now());

			Packet outp(source,_r->identity.address(),Packet::VERB_OK);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(packet.packetId());
			outp.append(timestamp);
			outp.encrypt(existingPeer->cryptKey());
			outp.hmacSet(existingPeer->macKey());
			_r->demarc->send(localPort,_remoteAddress,outp.data(),outp.size(),-1);
			return;
		}

		// Otherwise we call addPeer() and set up a callback to handle the verdict
		_CBaddPeerFromHello_Data *arg = new _CBaddPeerFromHello_Data;
		arg->parent = this;
		arg->source = source;
		arg->_remoteAddress = _remoteAddress;
		arg->localPort = localPort;
		arg->vMajor = vMajor;
		arg->vMinor = vMinor;
		arg->vRevision = vRevision;
		arg->helloPacketId = packet.packetId();
		arg->helloTimestamp = timestamp;
		_r->topology->addPeer(candidate,&Switch::_CBaddPeerFromHello,arg);
	} catch (std::exception &ex) {
		TRACE("dropped HELLO from %s(%s): %s",source.toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped HELLO from %s(%s): unexpected exception",source.toString().c_str(),_remoteAddress.toString().c_str());
	}
}

bool PacketDecoder::_doOK(const RuntimeEnvironment *_r)
{
	try {
		Packet::Verb inReVerb = (Packet::Verb)packet[ZT_PROTO_VERB_OK_IDX_IN_RE_VERB];
		switch(inReVerb) {
			case Packet::VERB_HELLO:
				// OK from HELLO permits computation of latency.
				latency = std::min((unsigned int)(now - packet.at<uint64_t>(ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP)),(unsigned int)0xffff);
				TRACE("%s(%s): OK(HELLO), latency: %u",source.toString().c_str(),_remoteAddress.toString().c_str(),latency);
				break;
			case Packet::VERB_WHOIS:
				// Right now we only query supernodes for WHOIS and only accept
				// OK back from them. If we query other nodes, we'll have to
				// do something to prevent WHOIS cache poisoning such as
				// using the packet ID field in the OK packet to match with the
				// original query. Technically we should be doing this anyway.
				TRACE("%s(%s): OK(%s)",source.toString().c_str(),_remoteAddress.toString().c_str(),Packet::verbString(inReVerb));
				if (_r->topology->isSupernode(source))
					_r->topology->addPeer(SharedPtr<Peer>(new Peer(_r->identity,Identity(packet,ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY))),&Switch::_CBaddPeerFromWhois,this);
				break;
			default:
				TRACE("%s(%s): OK(%s)",source.toString().c_str(),_remoteAddress.toString().c_str(),Packet::verbString(inReVerb));
				break;
		}
	} catch (std::exception &ex) {
		TRACE("dropped OK from %s(%s): unexpected exception: %s",source.toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped OK from %s(%s): unexpected exception: (unknown)",source.toString().c_str(),_remoteAddress.toString().c_str());
	}
}

bool PacketDecoder::_doWHOIS(const RuntimeEnvironment *_r)
{
	if (packet.payloadLength() == ZT_ADDRESS_LENGTH) {
		SharedPtr<Peer> p(_r->topology->getPeer(Address(packet.payload())));
		if (p) {
			Packet outp(source,_r->identity.address(),Packet::VERB_OK);
			outp.append((unsigned char)Packet::VERB_WHOIS);
			outp.append(packet.packetId());
			p->identity().serialize(outp,false);
			outp.encrypt(peer->cryptKey());
			outp.hmacSet(peer->macKey());
			_r->demarc->send(localPort,_remoteAddress,outp.data(),outp.size(),-1);
			TRACE("sent WHOIS response to %s for %s",source.toString().c_str(),Address(packet.payload()).toString().c_str());
		} else {
			Packet outp(source,_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_WHOIS);
			outp.append(packet.packetId());
			outp.append((unsigned char)Packet::ERROR_NOT_FOUND);
			outp.append(packet.payload(),ZT_ADDRESS_LENGTH);
			outp.encrypt(peer->cryptKey());
			outp.hmacSet(peer->macKey());
			_r->demarc->send(localPort,_remoteAddress,outp.data(),outp.size(),-1);
			TRACE("sent WHOIS ERROR to %s for %s (not found)",source.toString().c_str(),Address(packet.payload()).toString().c_str());
		}
	} else {
		TRACE("dropped WHOIS from %s(%s): missing or invalid address",source.toString().c_str(),_remoteAddress.toString().c_str());
	}
}

bool PacketDecoder::_doRENDEZVOUS(const RuntimeEnvironment *_r)
{
	try {
		Address with(packet.field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ZTADDRESS,ZT_ADDRESS_LENGTH));
		RendezvousQueueEntry qe;
		if (_r->topology->getPeer(with)) {
			unsigned int port = packet.at<uint16_t>(ZT_PROTO_VERB_RENDEZVOUS_IDX_PORT);
			unsigned int addrlen = packet[ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRLEN];
			if ((port > 0)&&((addrlen == 4)||(addrlen == 16))) {
				qe.inaddr.set(packet.field(ZT_PROTO_VERB_RENDEZVOUS_IDX_ADDRESS,addrlen),addrlen,port);
				qe.fireAtTime = now + ZT_RENDEZVOUS_NAT_T_DELAY; // then send real packet in a few ms
				qe.localPort = _r->demarc->pick(qe.inaddr);
				TRACE("RENDEZVOUS from %s says %s might be at %s, starting NAT-t",source.toString().c_str(),with.toString().c_str(),qe.inaddr.toString().c_str());
				_r->demarc->send(qe.localPort,qe.inaddr,"\0",1,ZT_FIREWALL_OPENER_HOPS); // start with firewall opener
				{
					Mutex::Lock _l(_rendezvousQueue_m);
					_rendezvousQueue[with] = qe;
				}
			} else {
				TRACE("dropped corrupt RENDEZVOUS from %s(%s) (bad address or port)",source.toString().c_str(),_remoteAddress.toString().c_str());
			}
		} else {
			TRACE("ignored RENDEZVOUS from %s(%s) to meet unknown peer %s",source.toString().c_str(),_remoteAddress.toString().c_str(),with.toString().c_str());
		}
	} catch (std::exception &ex) {
		TRACE("dropped RENDEZVOUS from %s(%s): %s",source.toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped RENDEZVOUS from %s(%s): unexpected exception",source.toString().c_str(),_remoteAddress.toString().c_str());
	}
}

bool PacketDecoder::_doFRAME(const RuntimeEnvironment *_r)
{
	try {
		SharedPtr<Network> network(_r->nc->network(packet.at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID)));
		if (network) {
			if (network->isAllowed(source)) {
				unsigned int etherType = packet.at<uint16_t>(ZT_PROTO_VERB_FRAME_IDX_ETHERTYPE);
				if ((etherType != ZT_ETHERTYPE_ARP)&&(etherType != ZT_ETHERTYPE_IPV4)&&(etherType != ZT_ETHERTYPE_IPV6)) {
					TRACE("dropped FRAME from %s: unsupported ethertype",source.toString().c_str());
				} else if (packet.size() > ZT_PROTO_VERB_FRAME_IDX_PAYLOAD) {
					network->tap().put(source.toMAC(),network->tap().mac(),etherType,packet.data() + ZT_PROTO_VERB_FRAME_IDX_PAYLOAD,packet.size() - ZT_PROTO_VERB_FRAME_IDX_PAYLOAD);
				}
			} else {
				TRACE("dropped FRAME from %s(%s): not a member of closed network %llu",source.toString().c_str(),_remoteAddress.toString().c_str(),network->id());
			}
		} else {
			TRACE("dropped FRAME from %s(%s): network %llu unknown",source.toString().c_str(),_remoteAddress.toString().c_str(),packet.at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID));
		}
	} catch (std::exception &ex) {
		TRACE("dropped FRAME from %s(%s): unexpected exception: %s",source.toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped FRAME from %s(%s): unexpected exception: (unknown)",source.toString().c_str(),_remoteAddress.toString().c_str());
	}
}

bool PacketDecoder::_doMULTICAST_LIKE(const RuntimeEnvironment *_r)
{
	try {
		unsigned int ptr = ZT_PACKET_IDX_PAYLOAD;
		unsigned int numAccepted = 0;

		// Iterate through 18-byte network,MAC,ADI tuples:
		while ((ptr + 18) <= packet.size()) {
			uint64_t nwid = packet.at<uint64_t>(ptr); ptr += 8;
			SharedPtr<Network> network(_r->nc->network(nwid));
			if (network) {
				if (network->isAllowed(source)) {
					MAC mac(packet.field(ptr,6)); ptr += 6;
					uint32_t adi = packet.at<uint32_t>(ptr); ptr += 4;
					TRACE("peer %s likes multicast group %s:%.8lx on network %llu",source.toString().c_str(),mac.toString().c_str(),(unsigned long)adi,nwid);
					_multicaster.likesMulticastGroup(nwid,MulticastGroup(mac,adi),source,now);
					++numAccepted;
				} else {
					TRACE("ignored MULTICAST_LIKE from %s(%s): not a member of closed network %llu",source.toString().c_str(),_remoteAddress.toString().c_str(),nwid);
				}
			} else {
				TRACE("ignored MULTICAST_LIKE from %s(%s): network %llu unknown or we are not a member",source.toString().c_str(),_remoteAddress.toString().c_str(),nwid);
			}
		}

		Packet outp(source,_r->identity.address(),Packet::VERB_OK);
		outp.append((unsigned char)Packet::VERB_MULTICAST_LIKE);
		outp.append(packet.packetId());
		outp.append((uint16_t)numAccepted);
		outp.encrypt(peer->cryptKey());
		outp.hmacSet(peer->macKey());
		_r->demarc->send(localPort,_remoteAddress,outp.data(),outp.size(),-1);
	} catch (std::exception &ex) {
		TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception: %s",source.toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception: (unknown)",source.toString().c_str(),_remoteAddress.toString().c_str());
	}
}

bool PacketDecoder::_doMULTICAST_FRAME(const RuntimeEnvironment *_r)
{
	try {
		SharedPtr<Network> network(_r->nc->network(packet.at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID)));
		if (network) {
			if (network->isAllowed(source)) {
				if (packet.size() > ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD) {
					Address originalSubmitterAddress(packet.field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SUBMITTER_ADDRESS,ZT_ADDRESS_LENGTH));
					MAC fromMac(packet.field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SOURCE_MAC,6));
					MulticastGroup mg(MAC(packet.field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_DESTINATION_MAC,6)),packet.at<uint32_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ADI));
					unsigned int hops = packet[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_HOP_COUNT];
					unsigned int etherType = packet.at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ETHERTYPE);
					unsigned int datalen = packet.at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD_LENGTH);
					unsigned int signaturelen = packet.at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_SIGNATURE_LENGTH);
					unsigned char *dataAndSignature = packet.field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD,datalen + signaturelen);

					bool isDuplicate = _multicaster.checkAndUpdateMulticastHistory(fromMac,mg,payload,payloadLen,network->id(),now);

					if (originalSubmitterAddress == _r->identity.address()) {
						// Technically should not happen, since the original submitter is
						// excluded from consideration as a propagation recipient.
						TRACE("dropped boomerang MULTICAST_FRAME received from %s(%s)",source.toString().c_str(),_remoteAddress.toString().c_str());
					} else if ((!isDuplicate)||(_r->topology.isSupernode(_r->identity.address()))) {
						// If I am a supernode, I will repeatedly propagate duplicates. That's
						// because supernodes are used to bridge sparse multicast groups. Non-
						// supernodes will ignore duplicates completely.
						SharedPtr<Peer> originalSubmitter(_r->topology->getPeer(originalSubmitterAddress));
						if (!originalSubmitter) {
							TRACE("requesting WHOIS on original multicast frame submitter %s",originalSubmitterAddress.toString().c_str());
							_requestWhois(originalSubmitterAddress,packet.packetId());
							return false;
						} else if (Multicaster::verifyMulticastPacket(originalSubmitter->identity(),fromMac,mg,etherType,data,datalen,dataAndSignature + datalen,signaturelen)) {
							if (!isDuplicate)
								network->tap().put(fromMac,mg.mac(),etherType,payload,payloadLen);
							_propagateMulticast(network,originalSubmitterAddress,source,packet.field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_BLOOM,ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE),mg,hops+1,fromMac,etherType,payload,payloadLen);
						} else {
							LOG("rejected MULTICAST_FRAME from %s(%s) due to failed signature check (claims original sender %s)",source.toString().c_str(),_remoteAddress.toString().c_str(),originalSubmitterAddress.toString().c_str());
						}
					} else {
						TRACE("dropped redundant MULTICAST_FRAME from %s(%s)",source.toString().c_str(),_remoteAddress.toString().c_str());
					}
				} else {
					TRACE("dropped MULTICAST_FRAME from %s(%s): invalid short packet",source.toString().c_str(),_remoteAddress.toString().c_str());
				}
			} else {
				TRACE("dropped MULTICAST_FRAME from %s(%s): not a member of closed network %llu",source.toString().c_str(),_remoteAddress.toString().c_str(),network->id());
			}
		} else {
			TRACE("dropped MULTICAST_FRAME from %s(%s): network %llu unknown or we are not a member",source.toString().c_str(),_remoteAddress.toString().c_str(),packet.at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID));
		}
	} catch (std::exception &ex) {
		TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception: %s",source.toString().c_str(),_remoteAddress.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception: (unknown)",source.toString().c_str(),_remoteAddress.toString().c_str());
	}
}

} // namespace ZeroTier
