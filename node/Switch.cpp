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
#include <stdlib.h>
#include <algorithm>
#include <utility>
#include <stdexcept>

#include "Switch.hpp"
#include "Node.hpp"
#include "EthernetTap.hpp"
#include "InetAddress.hpp"
#include "Topology.hpp"
#include "RuntimeEnvironment.hpp"
#include "Defaults.hpp"
#include "Peer.hpp"
#include "NodeConfig.hpp"
#include "Demarc.hpp"
#include "Filter.hpp"

#include "../version.h"

namespace ZeroTier {

Switch::Switch(const RuntimeEnvironment *renv) :
	_r(renv)
{
}

Switch::~Switch()
{
}

void Switch::onRemotePacket(Demarc::Port localPort,const InetAddress &fromAddr,const Buffer<4096> &data)
{
	Packet packet;
	try {
		if (data.size() > ZT_PROTO_MIN_FRAGMENT_LENGTH) {
			if (data[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR) {
				_handleRemotePacketFragment(localPort,fromAddr,data);
			} else if (data.size() > ZT_PROTO_MIN_PACKET_LENGTH) {
				_handleRemotePacketHead(localPort,fromAddr,data);
			}
		}
	} catch (std::exception &ex) {
		TRACE("dropped packet from %s: %s",fromAddr.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped packet from %s: unknown exception",fromAddr.toString().c_str());
	}
}

void Switch::onLocalEthernet(const SharedPtr<Network> &network,const MAC &from,const MAC &to,unsigned int etherType,const Buffer<4096> &data)
{
	if (from != network->tap().mac()) {
		LOG("ignored tap: %s -> %s %s (bridging is not (yet?) supported)",from.toString().c_str(),to.toString().c_str(),Filter::etherTypeName(etherType));
		return;
	}

	if (to == network->tap().mac()) {
		// Right thing to do? Will this ever happen?
		TRACE("weird OS behavior: ethernet frame received from self, reflecting");
		network->tap().put(from,to,etherType,data.data(),data.size());
		return;
	}

	if ((etherType != ZT_ETHERTYPE_ARP)&&(etherType != ZT_ETHERTYPE_IPV4)&&(etherType != ZT_ETHERTYPE_IPV6)) {
		LOG("ignored tap: %s -> %s %s (not a supported etherType)",from.toString().c_str(),to.toString().c_str(),Filter::etherTypeName(etherType));
		return;
	}

	if (to.isMulticast()) {
		MulticastGroup mg(to,0);

		if (to.isBroadcast()) {
			// Cram IPv4 IP into ADI field to make IPv4 ARP broadcast channel specific and scalable
			if ((etherType == ZT_ETHERTYPE_ARP)&&(data.size() == 28)&&(data[2] == 0x08)&&(data[3] == 0x00)&&(data[4] == 6)&&(data[5] == 4)&&(data[7] == 0x01))
				mg = MulticastGroup::deriveMulticastGroupForAddressResolution(InetAddress(data.field(24,4),4,0));
		}

		Multicaster::MulticastBloomFilter newbf;
		SharedPtr<Peer> propPeers[ZT_MULTICAST_PROPAGATION_BREADTH];
		unsigned int np = _multicaster.pickNextPropagationPeers(
			*(_r->topology),
			network->id(),
			mg,
			_r->identity.address(),
			Address(),
			newbf,
			ZT_MULTICAST_PROPAGATION_BREADTH,
			propPeers,
			Utils::now());

		if (!np)
			return;

		std::string signature(Multicaster::signMulticastPacket(_r->identity,network->id(),from,mg,etherType,data,len));
		if (!signature.length()) {
			TRACE("failure signing multicast message!");
			return;
		}

		Packet outpTmpl(propPeers[0]->address(),_r->identity.address(),Packet::VERB_MULTICAST_FRAME);
		outpTmpl.append((uint8_t)0);
		outpTmpl.append((uint64_t)network->id());
		outpTmpl.append(_r->identity.address().data(),ZT_ADDRESS_LENGTH);
		outpTmpl.append(from.data,6);
		outpTmpl.append(mg.mac().data,6);
		outpTmpl.append((uint32_t)mg.adi());
		outpTmpl.append(newBloom.data(),ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE);
		outpTmpl.append((uint8_t)0); // 0 hops
		outpTmpl.append((uint16_t)etherType);
		outpTmpl.append((uint16_t)len);
		outpTmpl.append((uint16_t)signature.length());
		outpTmpl.append(data,len);
		outpTmpl.append(signature.data(),signature.length());
		outpTmpl.compress();
		send(outpTmpl,true);
		for(unsigned int i=1;i<np;++i) {
			outpTmpl.newInitializationVector();
			outpTmpl.setDestination(propPeers[i]->address());
			send(outpTmpl,true);
		}
	} else if (to.isZeroTier()) {
		// Simple unicast frame from us to another node
		Address toZT(to.data + 1);
		if (network->isAllowed(toZT)) {
			Packet outp(toZT,_r->identity.address(),Packet::VERB_FRAME);
			outp.append(network->id());
			outp.append((uint16_t)etherType);
			outp.append(data);
			outp.compress();
			send(outp,true);
		} else {
			TRACE("UNICAST: %s -> %s %s (dropped, destination not a member of closed network %llu)",from.toString().c_str(),to.toString().c_str(),Filter::etherTypeName(etherType),network->id());
		}
	} else {
		TRACE("UNICAST: %s -> %s %s (dropped, destination MAC not ZeroTier)",from.toString().c_str(),to.toString().c_str(),Filter::etherTypeName(etherType));
	}
}

void Switch::send(const Packet &packet,bool encrypt)
{
	//TRACE("%.16llx %s -> %s (size: %u) (enc: %s)",packet.packetId(),Packet::verbString(packet.verb()),packet.destination().toString().c_str(),packet.size(),(encrypt ? "yes" : "no"));
	if (!_trySend(packet,encrypt)) {
		Mutex::Lock _l(_txQueue_m);
		_txQueue.insert(std::pair< uint64_t,TXQueueEntry >(packet.packetId(),TXQueueEntry(Utils::now(),packet,encrypt)));
	}
}

void Switch::sendHELLO(const Address &dest)
{
	Packet outp(dest,_r->identity.address(),Packet::VERB_HELLO);
	outp.append((unsigned char)ZT_PROTO_VERSION);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MAJOR);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MINOR);
	outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
	outp.append(Utils::now());
	_r->identity.serialize(outp,false);
	send(outp,false);
}

bool Switch::sendHELLO(const SharedPtr<Peer> &dest,Demarc::Port localPort,const InetAddress &addr)
{
	Packet outp(dest->address(),_r->identity.address(),Packet::VERB_HELLO);
	outp.append((unsigned char)ZT_PROTO_VERSION);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MAJOR);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MINOR);
	outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
	outp.append(Utils::now());
	_r->identity.serialize(outp,false);
	outp.hmacSet(dest->macKey());
	return _r->demarc->send(localPort,addr,outp.data(),outp.size(),-1);
}

bool Switch::unite(const Address &p1,const Address &p2,bool force)
{
	SharedPtr<Peer> p1p = _r->topology->getPeer(p1);
	if (!p1p)
		return false;
	SharedPtr<Peer> p2p = _r->topology->getPeer(p2);
	if (!p2p)
		return false;

	uint64_t now = Utils::now();

	std::pair<InetAddress,InetAddress> cg(Peer::findCommonGround(*p1p,*p2p,now));
	if (!(cg.first))
		return false;

	// Addresses are sorted in key for last unite attempt map for order
	// invariant lookup: (p1,p2) == (p2,p1)
	Array<Address,2> uniteKey;
	if (p1 >= p2) {
		uniteKey[0] = p2;
		uniteKey[1] = p1;
	} else {
		uniteKey[0] = p1;
		uniteKey[1] = p2;
	}
	{
		Mutex::Lock _l(_lastUniteAttempt_m);
		std::map< Array< Address,2 >,uint64_t >::const_iterator e(_lastUniteAttempt.find(uniteKey));
		if ((!force)&&(e != _lastUniteAttempt.end())&&((now - e->second) < ZT_MIN_UNITE_INTERVAL))
			return false;
		else _lastUniteAttempt[uniteKey] = now;
	}

	TRACE("unite: %s(%s) <> %s(%s)",p1.toString().c_str(),cg.second.toString().c_str(),p2.toString().c_str(),cg.first.toString().c_str());

	{	// tell p1 where to find p2
		Packet outp(p1,_r->identity.address(),Packet::VERB_RENDEZVOUS);
		outp.append(p2.data(),ZT_ADDRESS_LENGTH);
		outp.append((uint16_t)cg.first.port());
		if (cg.first.isV6()) {
			outp.append((unsigned char)16);
			outp.append(cg.first.rawIpData(),16);
		} else {
			outp.append((unsigned char)4);
			outp.append(cg.first.rawIpData(),4);
		}
		outp.encrypt(p1p->cryptKey());
		outp.hmacSet(p1p->macKey());
		p1p->send(_r,outp.data(),outp.size(),false,Packet::VERB_RENDEZVOUS,now);
	}
	{	// tell p2 where to find p1
		Packet outp(p2,_r->identity.address(),Packet::VERB_RENDEZVOUS);
		outp.append(p1.data(),ZT_ADDRESS_LENGTH);
		outp.append((uint16_t)cg.second.port());
		if (cg.second.isV6()) {
			outp.append((unsigned char)16);
			outp.append(cg.second.rawIpData(),16);
		} else {
			outp.append((unsigned char)4);
			outp.append(cg.second.rawIpData(),4);
		}
		outp.encrypt(p2p->cryptKey());
		outp.hmacSet(p2p->macKey());
		p2p->send(_r,outp.data(),outp.size(),false,Packet::VERB_RENDEZVOUS,now);
	}

	return true;
}

unsigned long Switch::doTimerTasks()
{
	unsigned long nextDelay = ~((unsigned long)0); // big number, caller will cap return value
	uint64_t now = Utils::now();

	{
		Mutex::Lock _l(_rendezvousQueue_m);
		for(std::map< Address,RendezvousQueueEntry >::iterator i(_rendezvousQueue.begin());i!=_rendezvousQueue.end();) {
			if (now >= i->second.fireAtTime) {
				SharedPtr<Peer> withPeer = _r->topology->getPeer(i->first);
				if (withPeer) {
					TRACE("sending NAT-T HELLO to %s(%s)",i->first.toString().c_str(),i->second.inaddr.toString().c_str());
					sendHELLO(withPeer,i->second.localPort,i->second.inaddr);
				}
				_rendezvousQueue.erase(i++);
			} else {
				nextDelay = std::min(nextDelay,(unsigned long)(i->second.fireAtTime - now));
				++i;
			}
		}
	}

	{
		Mutex::Lock _l(_outstandingWhoisRequests_m);
		for(std::map< Address,WhoisRequest >::iterator i(_outstandingWhoisRequests.begin());i!=_outstandingWhoisRequests.end();) {
			unsigned long since = (unsigned long)(now - i->second.lastSent);
			if (since >= ZT_WHOIS_RETRY_DELAY) {
				if (i->second.retries >= ZT_MAX_WHOIS_RETRIES) {
					TRACE("WHOIS %s timed out",i->first.toString().c_str());
					_outstandingWhoisRequests.erase(i++);
					continue;
				} else {
					i->second.lastSent = now;
					i->second.peersConsulted[i->second.retries] = _sendWhoisRequest(i->first,i->second.peersConsulted,i->second.retries);
					++i->second.retries;
					TRACE("WHOIS %s (retry %u)",i->first.toString().c_str(),i->second.retries);
					nextDelay = std::min(nextDelay,(unsigned long)ZT_WHOIS_RETRY_DELAY);
				}
			} else nextDelay = std::min(nextDelay,ZT_WHOIS_RETRY_DELAY - since);
			++i;
		}
	}

	{
		Mutex::Lock _l(_txQueue_m);
		for(std::map< uint64_t,TXQueueEntry >::iterator i(_txQueue.begin());i!=_txQueue.end();) {
			if (_trySend(i->second.packet,i->second.encrypt))
				_txQueue.erase(i++);
			else if ((now - i->second.creationTime) > ZT_TRANSMIT_QUEUE_TIMEOUT) {
				TRACE("TX %s -> %s timed out",i->second.packet.source().toString().c_str(),i->second.packet.destination().toString().c_str());
				_txQueue.erase(i++);
			} else ++i;
		}
	}
	{
		Mutex::Lock _l(_rxQueue_m);
		for(std::map< uint64_t,RXQueueEntry >::iterator i(_rxQueue.begin());i!=_rxQueue.end();) {
			if (_tryHandleRemotePacket(i->second.localPort,i->second.fromAddr,i->second.packet))
				_rxQueue.erase(i++);
			else if ((now - i->second.creationTime) > ZT_RECEIVE_QUEUE_TIMEOUT) {
				TRACE("RX from %s timed out waiting for WHOIS",i->second.packet.source().toString().c_str());
				_rxQueue.erase(i++);
			} else ++i;
		}
	}

	{
		Mutex::Lock _l(_defragQueue_m);
		for(std::map< uint64_t,DefragQueueEntry >::iterator i(_defragQueue.begin());i!=_defragQueue.end();) {
			if ((now - i->second.creationTime) > ZT_FRAGMENTED_PACKET_RECEIVE_TIMEOUT) {
				TRACE("incomplete fragmented packet %.16llx timed out, fragments discarded",i->first);
				_defragQueue.erase(i++);
			} else ++i;
		}
	}

	return std::max(nextDelay,(unsigned long)50); // minimum delay
}

void Switch::announceMulticastGroups(const std::map< SharedPtr<Network>,std::set<MulticastGroup> > &allMemberships)
{
	std::vector< SharedPtr<Peer> > directPeers;
	_r->topology->eachPeer(Topology::CollectPeersWithActiveDirectPath(directPeers));

#ifdef ZT_TRACE
	unsigned int totalMulticastGroups = 0;
	for(std::map< SharedPtr<Network>,std::set<MulticastGroup> >::const_iterator i(allMemberships.begin());i!=allMemberships.end();++i)
		totalMulticastGroups += (unsigned int)i->second.size();
	TRACE("announcing %u multicast groups for %u networks to %u peers",totalMulticastGroups,(unsigned int)allMemberships.size(),(unsigned int)directPeers.size());
#endif

	for(std::vector< SharedPtr<Peer> >::iterator p(directPeers.begin());p!=directPeers.end();++p) {
		Packet outp((*p)->address(),_r->identity.address(),Packet::VERB_MULTICAST_LIKE);

		for(std::map< SharedPtr<Network>,std::set<MulticastGroup> >::const_iterator nwmgs(allMemberships.begin());nwmgs!=allMemberships.end();++nwmgs) {
			if ((nwmgs->first->open())||(_r->topology->isSupernode((*p)->address()))||(nwmgs->first->isMember((*p)->address()))) {
				for(std::set<MulticastGroup>::iterator mg(nwmgs->second.begin());mg!=nwmgs->second.end();++mg) {
					if ((outp.size() + 18) > ZT_UDP_DEFAULT_PAYLOAD_MTU) {
						send(outp,true);
						outp.reset((*p)->address(),_r->identity.address(),Packet::VERB_MULTICAST_LIKE);
					}

					outp.append((uint64_t)nwmgs->first->id());
					outp.append(mg->mac().data,6);
					outp.append((uint32_t)mg->adi());
				}
			}
		}

		if (outp.size() > ZT_PROTO_MIN_PACKET_LENGTH)
			send(outp,true);
	}
}

void Switch::requestWhois(const Address &addr,const SharedPtr<PacketDecoder> &pd)
{
	TRACE("requesting WHOIS for %s",addr.toString().c_str());
	_sendWhoisRequest(addr,(const Address *)0,0);
	Mutex::Lock _l(_outstandingWhoisRequests_m);
	std::pair< std::map< Address,WhoisRequest >::iterator,bool > entry(_outstandingWhoisRequests.insert(std::pair<Address,WhoisRequest>(addr,WhoisRequest())));
	entry.first->second.lastSent = Utils::now();
	entry.first->second.retries = 0; // reset retry count if entry already existed
	entry.first->second.waitingPackets.insert(pd);
}

void Switch::_CBaddPeerFromHello(void *arg,const SharedPtr<Peer> &p,Topology::PeerVerifyResult result)
{
	_CBaddPeerFromHello_Data *req = (_CBaddPeerFromHello_Data *)arg;
	const RuntimeEnvironment *_r = req->parent->_r;

	switch(result) {
		case Topology::PEER_VERIFY_ACCEPTED_NEW:
		case Topology::PEER_VERIFY_ACCEPTED_ALREADY_HAVE:
		case Topology::PEER_VERIFY_ACCEPTED_DISPLACED_INVALID_ADDRESS: {
			req->parent->_finishWhoisRequest(p); // terminate any outstanding WHOIS too

			Packet outp(req->source,_r->identity.address(),Packet::VERB_OK);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(req->helloPacketId);
			outp.append(req->helloTimestamp);
			outp.encrypt(p->cryptKey());
			outp.hmacSet(p->macKey());
			req->parent->_r->demarc->send(req->localPort,req->fromAddr,outp.data(),outp.size(),-1);
		}	break;

		case Topology::PEER_VERIFY_REJECTED_INVALID_IDENTITY: {
			Packet outp(req->source,_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(req->helloPacketId);
			outp.append((unsigned char)Packet::ERROR_IDENTITY_INVALID);
			outp.encrypt(p->cryptKey());
			outp.hmacSet(p->macKey());
			req->parent->_r->demarc->send(req->localPort,req->fromAddr,outp.data(),outp.size(),-1);
		}	break;

		case Topology::PEER_VERIFY_REJECTED_DUPLICATE:
		case Topology::PEER_VERIFY_REJECTED_DUPLICATE_TRIAGED: {
			Packet outp(req->source,_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(req->helloPacketId);
			outp.append((unsigned char)Packet::ERROR_IDENTITY_COLLISION);
			outp.encrypt(p->cryptKey());
			outp.hmacSet(p->macKey());
			req->parent->_r->demarc->send(req->localPort,req->fromAddr,outp.data(),outp.size(),-1);
		}	break;
	}

	delete req;
}

void Switch::_CBaddPeerFromWhois(void *arg,const SharedPtr<Peer> &p,Topology::PeerVerifyResult result)
{
	switch(result) {
		case Topology::PEER_VERIFY_ACCEPTED_NEW:
		case Topology::PEER_VERIFY_ACCEPTED_ALREADY_HAVE:
		case Topology::PEER_VERIFY_ACCEPTED_DISPLACED_INVALID_ADDRESS:
			((Switch *)arg)->_finishWhoisRequest(p);
			break;
		default:
			break;
	}
}

void Switch::_finishWhoisRequest(
	const SharedPtr<Peer> &peer)
{
	Mutex::Lock _l(_outstandingWhoisRequests_m);
	std::map< Address,WhoisRequest >::iterator wr(_outstandingWhoisRequests.find(peer->address()));
	if (wr != _outstandingWhoisRequests.end()) {
		for(std::set<uint64_t>::iterator pid(wr->second.waitingPackets.begin());pid!=wr->second.waitingPackets.end();++pid) {
			{
				Mutex::Lock _l(_txQueue_m);
				std::map< uint64_t,TXQueueEntry >::iterator txitem(_txQueue.find(*pid));
				if (txitem != _txQueue.end()) {
					if (_trySend(txitem->second.packet,txitem->second.encrypt))
						_txQueue.erase(txitem);
				}
			}
			{
				Mutex::Lock _l(_rxQueue_m);
				std::map< uint64_t,RXQueueEntry >::iterator rxitem(_rxQueue.find(*pid));
				if (rxitem != _rxQueue.end()) {
					if (_tryHandleRemotePacket(rxitem->second.localPort,rxitem->second.fromAddr,rxitem->second.packet))
						_rxQueue.erase(rxitem);
				}
			}
		}
		_outstandingWhoisRequests.erase(wr);
	}
}

void Switch::_handleRemotePacketFragment(Demarc::Port localPort,const InetAddress &fromAddr,const Buffer<4096> &data)
{
	Packet::Fragment fragment(data);

	Address destination(fragment.destination());
	if (destination != _r->identity.address()) {
		// Fragment is not for us, so try to relay it

		if (fragment.hops() < ZT_RELAY_MAX_HOPS) {
			fragment.incrementHops();

			SharedPtr<Peer> relayTo = _r->topology->getPeer(destination);
			if ((!relayTo)||(!relayTo->send(_r,fragment.data(),fragment.size(),true,Packet::VERB_NOP,Utils::now()))) {
				relayTo = _r->topology->getBestSupernode();
				if (relayTo)
					relayTo->send(_r,fragment.data(),fragment.size(),true,Packet::VERB_NOP,Utils::now());
			}
		} else {
			TRACE("dropped relay [fragment](%s) -> %s, max hops exceeded",fromAddr.toString().c_str(),destination.toString().c_str());
		}
	} else {
		// Fragment looks like ours

		uint64_t pid = fragment.packetId();
		unsigned int fno = fragment.fragmentNumber();
		unsigned int tf = fragment.totalFragments();

		if ((tf <= ZT_MAX_PACKET_FRAGMENTS)&&(fno < ZT_MAX_PACKET_FRAGMENTS)&&(fno > 0)&&(tf > 1)) {
			// Fragment appears basically sane. Its fragment number must be
			// 1 or more, since a Packet with fragmented bit set is fragment 0.
			// Total fragments must be more than 1, otherwise why are we
			// seeing a Packet::Fragment?

			Mutex::Lock _l(_defragQueue_m);
			std::map< uint64_t,DefragQueueEntry >::iterator dqe(_defragQueue.find(pid));

			if (dqe == _defragQueue.end()) {
				// We received a Packet::Fragment without its head, so queue it and wait

				DefragQueueEntry &dq = _defragQueue[pid];
				dq.creationTime = Utils::now();
				dq.frags[fno - 1] = fragment;
				dq.totalFragments = tf; // total fragment count is known
				dq.haveFragments = 1 << fno; // we have only this fragment
				//TRACE("fragment (%u/%u) of %.16llx from %s",fno + 1,tf,pid,fromAddr.toString().c_str());
			} else if (!(dqe->second.haveFragments & (1 << fno))) {
				// We have other fragments and maybe the head, so add this one and check

				dqe->second.frags[fno - 1] = fragment;
				dqe->second.totalFragments = tf;
				//TRACE("fragment (%u/%u) of %.16llx from %s",fno + 1,tf,pid,fromAddr.toString().c_str());

				if (Utils::countBits(dqe->second.haveFragments |= (1 << fno)) == tf) {
					// We have all fragments -- assemble and process full Packet

					//TRACE("packet %.16llx is complete, assembling and processing...",pid);
					Packet packet(dqe->second.frag0);
					for(unsigned int f=1;f<tf;++f)
						packet.append(dqe->second.frags[f - 1].payload(),dqe->second.frags[f - 1].payloadLength());
					_defragQueue.erase(dqe);

					_handleRemotePacket(localPort,fromAddr,packet);
				}
			} // else this is a duplicate fragment, ignore
		}
	}
}

bool Switch::_handleRemotePacketHead(Demarc::Port localPort,const InetAddress &fromAddr,const Buffer<4096> &data)
{
	Packet packet(data);

	Address destination(packet.destination());
	if (destination != _r->identity.address()) {
		// Packet is not for us, so try to relay it

		if (packet.hops() < ZT_RELAY_MAX_HOPS) {
			packet.incrementHops();

			SharedPtr<Peer> relayTo = _r->topology->getPeer(destination);
			if ((relayTo)&&(relayTo->send(_r,packet.data(),packet.size(),true,Packet::VERB_NOP,Utils::now()))) {
				// TODO: don't unite immediately, wait until the peers have exchanged a packet or two
				unite(packet.source(),destination,false); // periodically try to get them to talk directly
			} else {
				relayTo = _r->topology->getBestSupernode();
				if (relayTo)
					relayTo->send(_r,packet.data(),packet.size(),true,Packet::VERB_NOP,Utils::now());
			}
		} else {
			TRACE("dropped relay %s(%s) -> %s, max hops exceeded",packet.source().toString().c_str(),fromAddr.toString().c_str(),destination.toString().c_str());
		}
	} else if (packet.fragmented()) {
		// Packet is the head of a fragmented packet series

		uint64_t pid = packet.packetId();
		Mutex::Lock _l(_defragQueue_m);
		std::map< uint64_t,DefragQueueEntry >::iterator dqe(_defragQueue.find(pid));

		if (dqe == _defragQueue.end()) {
			// If we have no other fragments yet, create an entry and save the head

			DefragQueueEntry &dq = _defragQueue[pid];
			dq.creationTime = Utils::now();
			dq.frag0 = packet;
			dq.totalFragments = 0; // 0 == unknown, waiting for Packet::Fragment
			dq.haveFragments = 1; // head is first bit (left to right)
			//TRACE("fragment (0/?) of %.16llx from %s",pid,fromAddr.toString().c_str());
		} else if (!(dqe->second.haveFragments & 1)) {
			// If we have other fragments but no head, see if we are complete with the head

			if ((dqe->second.totalFragments)&&(Utils::countBits(dqe->second.haveFragments |= 1) == dqe->second.totalFragments)) {
				// We have all fragments -- assemble and process full Packet

				//TRACE("packet %.16llx is complete, assembling and processing...",pid);
				// packet already contains head, so append fragments
				for(unsigned int f=1;f<dqe->second.totalFragments;++f)
					packet.append(dqe->second.frags[f - 1].payload(),dqe->second.frags[f - 1].payloadLength());
				_defragQueue.erase(dqe);

				_handleRemotePacket(localPort,fromAddr,packet);
			} else {
				// Still waiting on more fragments, so queue the head

				dqe->second.frag0 = packet;
			}
		} // else this is a duplicate head, ignore
	} else {
		// Packet is unfragmented, so just process it
		_handleRemotePacket(localPort,fromAddr,packet);
	}
}

//////////////////// OBSOLETE
bool Switch::_tryHandleRemotePacket(Demarc::Port localPort,const InetAddress &fromAddr,Packet &packet)
{

	Address source(packet.source());

	if ((!packet.encrypted())&&(packet.verb() == Packet::VERB_HELLO)) {
		// Unencrypted HELLOs are handled here since they are used to
		// populate our identity cache in the first place. Thus we might get
		// a HELLO for someone for whom we don't have a Peer record.
		TRACE("HELLO from %s(%s)",source.toString().c_str(),fromAddr.toString().c_str());
		_doHELLO(localPort,fromAddr,packet);
		return true;
	}

	SharedPtr<Peer> peer = _r->topology->getPeer(source);
	if (peer) {
		uint64_t now = Utils::now();
		unsigned int latency = 0;

		if (!packet.hmacVerify(peer->macKey())) {
			TRACE("dropped packet from %s(%s), HMAC authentication failed (size: %u)",source.toString().c_str(),fromAddr.toString().c_str(),packet.size());
			return true;
		}

		if (packet.encrypted()) {
			packet.decrypt(peer->cryptKey());
		} else {
			// Unencrypted is tolerated in case we want to run this on
			// devices where squeezing out cycles matters. HMAC is
			// what's really important.
			TRACE("ODD: %s from %s(%s) wasn't encrypted",Packet::verbString(packet.verb()),source.toString().c_str(),fromAddr.toString().c_str());
		}

		if (!packet.uncompress()) {
			TRACE("dropped packet from %s(%s), compressed data invalid",source.toString().c_str(),fromAddr.toString().c_str());
			return true;
		}

		switch(packet.verb()) {
			case Packet::VERB_NOP:
				TRACE("NOP from %s(%s)",source.toString().c_str(),fromAddr.toString().c_str());
				break;
			case Packet::VERB_HELLO:
				// HELLO is normally handled up top, but this is legal. Pointless, but legal.
				_doHELLO(localPort,fromAddr,packet);
				break;
			case Packet::VERB_ERROR:
				try {
#ifdef ZT_TRACE
					Packet::Verb inReVerb = (Packet::Verb)packet[ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB];
					Packet::ErrorCode errorCode = (Packet::ErrorCode)packet[ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE];
					TRACE("ERROR %s from %s(%s) in-re %s",Packet::errorString(errorCode),source.toString().c_str(),fromAddr.toString().c_str(),Packet::verbString(inReVerb));
#endif
					// TODO:
					// The fact is that the protocol works fine without error handling.
					// The only error that really needs to be handled here is duplicate
					// identity collision, which if it comes from a supernode should cause
					// us to restart and regenerate a new identity.
				} catch (std::exception &ex) {
					TRACE("dropped ERROR from %s(%s): unexpected exception: %s",source.toString().c_str(),fromAddr.toString().c_str(),ex.what());
				} catch ( ... ) {
					TRACE("dropped ERROR from %s(%s): unexpected exception: (unknown)",source.toString().c_str(),fromAddr.toString().c_str());
				}
				break;
			case Packet::VERB_OK:
				try {
					Packet::Verb inReVerb = (Packet::Verb)packet[ZT_PROTO_VERB_OK_IDX_IN_RE_VERB];
					switch(inReVerb) {
						case Packet::VERB_HELLO:
							// OK from HELLO permits computation of latency.
							latency = std::min((unsigned int)(now - packet.at<uint64_t>(ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP)),(unsigned int)0xffff);
							TRACE("%s(%s): OK(HELLO), latency: %u",source.toString().c_str(),fromAddr.toString().c_str(),latency);
							break;
						case Packet::VERB_WHOIS:
							// Right now we only query supernodes for WHOIS and only accept
							// OK back from them. If we query other nodes, we'll have to
							// do something to prevent WHOIS cache poisoning such as
							// using the packet ID field in the OK packet to match with the
							// original query. Technically we should be doing this anyway.
							TRACE("%s(%s): OK(%s)",source.toString().c_str(),fromAddr.toString().c_str(),Packet::verbString(inReVerb));
							if (_r->topology->isSupernode(source))
								_r->topology->addPeer(SharedPtr<Peer>(new Peer(_r->identity,Identity(packet,ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY))),&Switch::_CBaddPeerFromWhois,this);
							break;
						default:
							TRACE("%s(%s): OK(%s)",source.toString().c_str(),fromAddr.toString().c_str(),Packet::verbString(inReVerb));
							break;
					}
				} catch (std::exception &ex) {
					TRACE("dropped OK from %s(%s): unexpected exception: %s",source.toString().c_str(),fromAddr.toString().c_str(),ex.what());
				} catch ( ... ) {
					TRACE("dropped OK from %s(%s): unexpected exception: (unknown)",source.toString().c_str(),fromAddr.toString().c_str());
				}
				break;
			case Packet::VERB_WHOIS:
				if (packet.payloadLength() == ZT_ADDRESS_LENGTH) {
					SharedPtr<Peer> p(_r->topology->getPeer(Address(packet.payload())));
					if (p) {
						Packet outp(source,_r->identity.address(),Packet::VERB_OK);
						outp.append((unsigned char)Packet::VERB_WHOIS);
						outp.append(packet.packetId());
						p->identity().serialize(outp,false);
						outp.encrypt(peer->cryptKey());
						outp.hmacSet(peer->macKey());
						_r->demarc->send(localPort,fromAddr,outp.data(),outp.size(),-1);
						TRACE("sent WHOIS response to %s for %s",source.toString().c_str(),Address(packet.payload()).toString().c_str());
					} else {
						Packet outp(source,_r->identity.address(),Packet::VERB_ERROR);
						outp.append((unsigned char)Packet::VERB_WHOIS);
						outp.append(packet.packetId());
						outp.append((unsigned char)Packet::ERROR_NOT_FOUND);
						outp.append(packet.payload(),ZT_ADDRESS_LENGTH);
						outp.encrypt(peer->cryptKey());
						outp.hmacSet(peer->macKey());
						_r->demarc->send(localPort,fromAddr,outp.data(),outp.size(),-1);
						TRACE("sent WHOIS ERROR to %s for %s (not found)",source.toString().c_str(),Address(packet.payload()).toString().c_str());
					}
				} else {
					TRACE("dropped WHOIS from %s(%s): missing or invalid address",source.toString().c_str(),fromAddr.toString().c_str());
				}
				break;
			case Packet::VERB_RENDEZVOUS:
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
							TRACE("dropped corrupt RENDEZVOUS from %s(%s) (bad address or port)",source.toString().c_str(),fromAddr.toString().c_str());
						}
					} else {
						TRACE("ignored RENDEZVOUS from %s(%s) to meet unknown peer %s",source.toString().c_str(),fromAddr.toString().c_str(),with.toString().c_str());
					}
				} catch (std::exception &ex) {
					TRACE("dropped RENDEZVOUS from %s(%s): %s",source.toString().c_str(),fromAddr.toString().c_str(),ex.what());
				} catch ( ... ) {
					TRACE("dropped RENDEZVOUS from %s(%s): unexpected exception",source.toString().c_str(),fromAddr.toString().c_str());
				}
				break;
			case Packet::VERB_FRAME:
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
							TRACE("dropped FRAME from %s(%s): not a member of closed network %llu",source.toString().c_str(),fromAddr.toString().c_str(),network->id());
						}
					} else {
						TRACE("dropped FRAME from %s(%s): network %llu unknown",source.toString().c_str(),fromAddr.toString().c_str(),packet.at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID));
					}
				} catch (std::exception &ex) {
					TRACE("dropped FRAME from %s(%s): unexpected exception: %s",source.toString().c_str(),fromAddr.toString().c_str(),ex.what());
				} catch ( ... ) {
					TRACE("dropped FRAME from %s(%s): unexpected exception: (unknown)",source.toString().c_str(),fromAddr.toString().c_str());
				}
				break;
			case Packet::VERB_MULTICAST_LIKE:
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
								TRACE("ignored MULTICAST_LIKE from %s(%s): not a member of closed network %llu",source.toString().c_str(),fromAddr.toString().c_str(),nwid);
							}
						} else {
							TRACE("ignored MULTICAST_LIKE from %s(%s): network %llu unknown or we are not a member",source.toString().c_str(),fromAddr.toString().c_str(),nwid);
						}
					}

					Packet outp(source,_r->identity.address(),Packet::VERB_OK);
					outp.append((unsigned char)Packet::VERB_MULTICAST_LIKE);
					outp.append(packet.packetId());
					outp.append((uint16_t)numAccepted);
					outp.encrypt(peer->cryptKey());
					outp.hmacSet(peer->macKey());
					_r->demarc->send(localPort,fromAddr,outp.data(),outp.size(),-1);
				} catch (std::exception &ex) {
					TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception: %s",source.toString().c_str(),fromAddr.toString().c_str(),ex.what());
				} catch ( ... ) {
					TRACE("dropped MULTICAST_LIKE from %s(%s): unexpected exception: (unknown)",source.toString().c_str(),fromAddr.toString().c_str());
				}
				break;
			case Packet::VERB_MULTICAST_FRAME:
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
									TRACE("dropped boomerang MULTICAST_FRAME received from %s(%s)",source.toString().c_str(),fromAddr.toString().c_str());
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
										LOG("rejected MULTICAST_FRAME from %s(%s) due to failed signature check (claims original sender %s)",source.toString().c_str(),fromAddr.toString().c_str(),originalSubmitterAddress.toString().c_str());
									}
								} else {
									TRACE("dropped redundant MULTICAST_FRAME from %s(%s)",source.toString().c_str(),fromAddr.toString().c_str());
								}
							} else {
								TRACE("dropped MULTICAST_FRAME from %s(%s): invalid short packet",source.toString().c_str(),fromAddr.toString().c_str());
							}
						} else {
							TRACE("dropped MULTICAST_FRAME from %s(%s): not a member of closed network %llu",source.toString().c_str(),fromAddr.toString().c_str(),network->id());
						}
					} else {
						TRACE("dropped MULTICAST_FRAME from %s(%s): network %llu unknown or we are not a member",source.toString().c_str(),fromAddr.toString().c_str(),packet.at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID));
					}
				} catch (std::exception &ex) {
					TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception: %s",source.toString().c_str(),fromAddr.toString().c_str(),ex.what());
				} catch ( ... ) {
					TRACE("dropped MULTICAST_FRAME from %s(%s): unexpected exception: (unknown)",source.toString().c_str(),fromAddr.toString().c_str());
				}
				break;
			default:
				// This might be something from a new or old version of the protocol.
				// Technically it passed HMAC so the packet is still valid, but we
				// ignore it.
				TRACE("ignored unrecognized verb %.2x from %s(%s)",(unsigned int)packet.verb(),source.toString().c_str(),fromAddr.toString().c_str());
				break;
		}

		// Update peer timestamps and learn new links. This must only ever
		// be called on an authenticated and technically valid packet, since
		// we only learn paths to peers over the WAN by hearing directly
		// from them over those paths. (Or by having them authoritatively
		// and statically defined, like with supernodes, but that's done
		// elsewhere.)
		peer->onReceive(_r,localPort,fromAddr,latency,packet.hops(),packet.verb(),now);
	} else {
		_requestWhois(source,packet.packetId());
		return false;
	}

	return true;
}

void Switch::_doHELLO(Demarc::Port localPort,const InetAddress &fromAddr,Packet &packet)
{
	Address source(packet.source());
	try {
		unsigned int protoVersion = packet[ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION];
		unsigned int vMajor = packet[ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION];
		unsigned int vMinor = packet[ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION];
		unsigned int vRevision = packet.at<uint16_t>(ZT_PROTO_VERB_HELLO_IDX_REVISION);
		uint64_t timestamp = packet.at<uint64_t>(ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP);
		Identity id(packet,ZT_PROTO_VERB_HELLO_IDX_IDENTITY);

		SharedPtr<Peer> candidate(new Peer(_r->identity,id));
		candidate->setPathAddress(fromAddr,false);

		// Initial sniff test
		if (protoVersion != ZT_PROTO_VERSION) {
			TRACE("rejected HELLO from %s(%s): invalid protocol version",source.toString().c_str(),fromAddr.toString().c_str());
			Packet outp(source,_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(packet.packetId());
			outp.append((unsigned char)Packet::ERROR_BAD_PROTOCOL_VERSION);
			outp.encrypt(candidate->cryptKey());
			outp.hmacSet(candidate->macKey());
			_r->demarc->send(localPort,fromAddr,outp.data(),outp.size(),-1);
			return;
		}
		if (id.address().isReserved()) {
			TRACE("rejected HELLO from %s(%s): identity has reserved address",source.toString().c_str(),fromAddr.toString().c_str());
			Packet outp(source,_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(packet.packetId());
			outp.append((unsigned char)Packet::ERROR_IDENTITY_INVALID);
			outp.encrypt(candidate->cryptKey());
			outp.hmacSet(candidate->macKey());
			_r->demarc->send(localPort,fromAddr,outp.data(),outp.size(),-1);
			return;
		}
		if (id.address() != source) {
			TRACE("rejected HELLO from %s(%s): identity is not for sender of packet (HELLO is a self-announcement)",source.toString().c_str(),fromAddr.toString().c_str());
			Packet outp(source,_r->identity.address(),Packet::VERB_ERROR);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(packet.packetId());
			outp.append((unsigned char)Packet::ERROR_INVALID_REQUEST);
			outp.encrypt(candidate->cryptKey());
			outp.hmacSet(candidate->macKey());
			_r->demarc->send(localPort,fromAddr,outp.data(),outp.size(),-1);
			return;
		}

		// Is this a HELLO for a peer we already know? If so just update its
		// packet receive stats and send an OK.
		SharedPtr<Peer> existingPeer(_r->topology->getPeer(id.address()));
		if ((existingPeer)&&(existingPeer->identity() == id)) {
			existingPeer->onReceive(_r,localPort,fromAddr,0,packet.hops(),Packet::VERB_HELLO,Utils::now());

			Packet outp(source,_r->identity.address(),Packet::VERB_OK);
			outp.append((unsigned char)Packet::VERB_HELLO);
			outp.append(packet.packetId());
			outp.append(timestamp);
			outp.encrypt(existingPeer->cryptKey());
			outp.hmacSet(existingPeer->macKey());
			_r->demarc->send(localPort,fromAddr,outp.data(),outp.size(),-1);
			return;
		}

		// Otherwise we call addPeer() and set up a callback to handle the verdict
		_CBaddPeerFromHello_Data *arg = new _CBaddPeerFromHello_Data;
		arg->parent = this;
		arg->source = source;
		arg->fromAddr = fromAddr;
		arg->localPort = localPort;
		arg->vMajor = vMajor;
		arg->vMinor = vMinor;
		arg->vRevision = vRevision;
		arg->helloPacketId = packet.packetId();
		arg->helloTimestamp = timestamp;
		_r->topology->addPeer(candidate,&Switch::_CBaddPeerFromHello,arg);
	} catch (std::exception &ex) {
		TRACE("dropped HELLO from %s(%s): %s",source.toString().c_str(),fromAddr.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped HELLO from %s(%s): unexpected exception",source.toString().c_str(),fromAddr.toString().c_str());
	}
}

Address Switch::_sendWhoisRequest(const Address &addr,const Address *peersAlreadyConsulted,unsigned int numPeersAlreadyConsulted)
{
	SharedPtr<Peer> supernode(_r->topology->getBestSupernode(peersAlreadyConsulted,numPeersAlreadyConsulted));
	if (supernode) {
		Packet outp(supernode->address(),_r->identity.address(),Packet::VERB_WHOIS);
		outp.append(addr.data(),ZT_ADDRESS_LENGTH);
		outp.encrypt(supernode->cryptKey());
		outp.hmacSet(supernode->macKey());
		supernode->send(_r,outp.data(),outp.size(),false,Packet::VERB_WHOIS,Utils::now());
		return supernode->address();
	}
	return Address();
}

bool Switch::_trySend(const Packet &packet,bool encrypt)
{
	SharedPtr<Peer> peer(_r->topology->getPeer(packet.destination()));
	if (peer) {
		uint64_t now = Utils::now();

		bool isRelay;
		SharedPtr<Peer> via;
		if ((_r->topology->isSupernode(peer->address()))||(peer->hasActiveDirectPath(now))) {
			isRelay = false;
			via = peer;
		} else {
			isRelay = true;
			via = _r->topology->getBestSupernode();
			if (!via)
				return false;
		}

		Packet tmp(packet);

		unsigned int chunkSize = std::min(tmp.size(),(unsigned int)ZT_UDP_DEFAULT_PAYLOAD_MTU);
		tmp.setFragmented(chunkSize < tmp.size());

		if (encrypt)
			tmp.encrypt(peer->cryptKey());
		tmp.hmacSet(peer->macKey());

		Packet::Verb verb = packet.verb();
		if (via->send(_r,tmp.data(),chunkSize,isRelay,verb,now)) {
			if (chunkSize < tmp.size()) {
				// Too big for one bite, fragment the rest
				unsigned int fragStart = chunkSize;
				unsigned int remaining = tmp.size() - chunkSize;
				unsigned int fragsRemaining = (remaining / (ZT_UDP_DEFAULT_PAYLOAD_MTU - ZT_PROTO_MIN_FRAGMENT_LENGTH));
				if ((fragsRemaining * (ZT_UDP_DEFAULT_PAYLOAD_MTU - ZT_PROTO_MIN_FRAGMENT_LENGTH)) < remaining)
					++fragsRemaining;
				unsigned int totalFragments = fragsRemaining + 1;

				for(unsigned int f=0;f<fragsRemaining;++f) {
					chunkSize = std::min(remaining,(unsigned int)(ZT_UDP_DEFAULT_PAYLOAD_MTU - ZT_PROTO_MIN_FRAGMENT_LENGTH));
					Packet::Fragment frag(tmp,fragStart,chunkSize,f + 1,totalFragments);
					if (!via->send(_r,frag.data(),frag.size(),isRelay,verb,now)) {
						TRACE("WARNING: packet send to %s failed on later fragment #%u (check IP layer buffer sizes?)",via->address().toString().c_str(),f + 1);
						return false;
					}
					fragStart += chunkSize;
					remaining -= chunkSize;
				}
			}

			return true;
		}
		return false;
	}

	_requestWhois(packet.destination(),packet.packetId());
	return false;
}

} // namespace ZeroTier
