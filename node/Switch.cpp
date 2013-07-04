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

#include "../version.h"

namespace ZeroTier {

Switch::Switch(const RuntimeEnvironment *renv) :
	_r(renv)
{
	memset(_multicastHistory,0,sizeof(_multicastHistory));
}

Switch::~Switch()
{
}

void Switch::onRemotePacket(Demarc::Port localPort,const InetAddress &fromAddr,const Buffer<4096> &data)
{
	Packet packet;

	try {
		if (data.size() > ZT_PROTO_MIN_FRAGMENT_LENGTH) {
			// Message is long enough to be a Packet or Packet::Fragment

			if (data[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR) {
				// Looks like a Packet::Fragment
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
								packet = dqe->second.frag0;
								for(unsigned int f=1;f<tf;++f)
									packet.append(dqe->second.frags[f - 1].payload(),dqe->second.frags[f - 1].payloadLength());
								_defragQueue.erase(dqe);

								goto Switch_onRemotePacket_complete_packet_handler;
							}
						} // else this is a duplicate fragment, ignore
					}
				}

			} else if (data.size() > ZT_PROTO_MIN_PACKET_LENGTH) {
				// Looks like a Packet -- either unfragmented or a fragmented packet head
				packet = data;

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

							goto Switch_onRemotePacket_complete_packet_handler;
						} else {
							// Still waiting on more fragments, so queue the head

							dqe->second.frag0 = packet;
						}
					} // else this is a duplicate head, ignore
				} else {
					// Packet is unfragmented, so just process it
					goto Switch_onRemotePacket_complete_packet_handler;
				}

			}
		}

		// If we made it here and didn't jump over, we either queued a fragment
		// or dropped an invalid or duplicate one. (The goto looks easier to
		// understand than having a million returns up there.)
		return;

Switch_onRemotePacket_complete_packet_handler:
		// Packets that get here are ours and are fully assembled. Don't worry -- if
		// they are corrupt HMAC authentication will reject them later.

		{
			//TRACE("%s : %s -> %s",fromAddr.toString().c_str(),packet.source().toString().c_str(),packet.destination().toString().c_str());
			PacketServiceAttemptResult r = _tryHandleRemotePacket(localPort,fromAddr,packet);
			if (r != PACKET_SERVICE_ATTEMPT_OK) {
				Address source(packet.source());
				{
					Mutex::Lock _l(_rxQueue_m);
					std::multimap< Address,RXQueueEntry >::iterator qe(_rxQueue.insert(std::pair< Address,RXQueueEntry >(source,RXQueueEntry())));
					qe->second.creationTime = Utils::now();
					qe->second.packet = packet;
					qe->second.localPort = localPort;
					qe->second.fromAddr = fromAddr;
				}
				if (r == PACKET_SERVICE_ATTEMPT_PEER_UNKNOWN)
					_requestWhois(source);
			}
		}
	} catch (std::exception &ex) {
		TRACE("dropped packet from %s: %s",fromAddr.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped packet from %s: unexpected exception",fromAddr.toString().c_str());
	}
}

void Switch::onLocalEthernet(const SharedPtr<Network> &network,const MAC &from,const MAC &to,unsigned int etherType,const Buffer<4096> &data)
{
	if (from != network->tap().mac()) {
		LOG("ignored tap: %s -> %s %s (bridging is not supported)",from.toString().c_str(),to.toString().c_str(),Utils::etherTypeName(etherType));
		return;
	}

	if (to == network->tap().mac()) {
		// Right thing to do? Will this ever happen?
		TRACE("weird OS behavior: ethernet frame received from self, reflecting");
		network->tap().put(from,to,etherType,data.data(),data.size());
		return;
	}

	if ((etherType != ZT_ETHERTYPE_ARP)&&(etherType != ZT_ETHERTYPE_IPV4)&&(etherType != ZT_ETHERTYPE_IPV6)) {
		LOG("ignored tap: %s -> %s %s (not a supported etherType)",from.toString().c_str(),to.toString().c_str(),Utils::etherTypeName(etherType));
		return;
	}

	if (to.isMulticast()) {
		MulticastGroup mg(to,0);

		// Handle special cases: IPv4 ARP
		if ((etherType == ZT_ETHERTYPE_ARP)&&(data.size() == 28)&&(data[2] == 0x08)&&(data[3] == 0x00)&&(data[4] == 6)&&(data[5] == 4)&&(data[7] == 0x01))
			mg = MulticastGroup::deriveMulticastGroupForAddressResolution(InetAddress(data.field(24,4),4,0));

		// Remember this message's CRC, but don't drop if we've already seen it
		// since it's our own.
		_checkAndUpdateMulticastHistory(from,mg.mac(),data.data(),data.size(),network->id(),Utils::now());

		// Start multicast propagation with empty bloom filter
		unsigned char bloom[ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE];
		memset(bloom,0,sizeof(bloom));
		_propagateMulticast(network,bloom,mg,0,0,from,etherType,data.data(),data.size());
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
			TRACE("UNICAST: %s -> %s %s (dropped, destination not a member of closed network %llu)",from.toString().c_str(),to.toString().c_str(),Utils::etherTypeName(etherType),network->id());
		}
	} else {
		TRACE("UNICAST: %s -> %s %s (dropped, destination MAC not ZeroTier)",from.toString().c_str(),to.toString().c_str(),Utils::etherTypeName(etherType));
	}
}

void Switch::send(const Packet &packet,bool encrypt)
{
	//TRACE("%.16llx %s -> %s (size: %u) (enc: %s)",packet.packetId(),Packet::verbString(packet.verb()),packet.destination().toString().c_str(),packet.size(),(encrypt ? "yes" : "no"));

	PacketServiceAttemptResult r = _trySend(packet,encrypt);
	if (r != PACKET_SERVICE_ATTEMPT_OK) {
		{
			Mutex::Lock _l(_txQueue_m);
			std::multimap< Address,TXQueueEntry >::iterator qe(_txQueue.insert(std::pair< Address,TXQueueEntry >(packet.destination(),TXQueueEntry())));
			qe->second.creationTime = Utils::now();
			qe->second.packet = packet;
			qe->second.encrypt = encrypt;
		}
		if (r == PACKET_SERVICE_ATTEMPT_PEER_UNKNOWN)
			_requestWhois(packet.destination());
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
					TRACE("sending NAT-T NOP to %s(%s)",i->first.toString().c_str(),i->second.inaddr.toString().c_str());
					Packet outp(i->first,_r->identity.address(),Packet::VERB_NOP);
					outp.append("ZT",2); // arbitrary payload
					outp.hmacSet(withPeer->macKey());
					_r->demarc->send(i->second.localPort,i->second.inaddr,outp.data(),outp.size(),-1);
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
		for(std::multimap< Address,TXQueueEntry >::iterator i(_txQueue.begin());i!=_txQueue.end();) {
			if (_trySend(i->second.packet,i->second.encrypt) == PACKET_SERVICE_ATTEMPT_OK)
				_txQueue.erase(i++);
			else if ((now - i->second.creationTime) > ZT_TRANSMIT_QUEUE_TIMEOUT) {
				TRACE("TX %s -> %s timed out",i->second.packet.source().toString().c_str(),i->second.packet.destination().toString().c_str());
				_txQueue.erase(i++);
			} else ++i;
		}
	}
	{
		Mutex::Lock _l(_rxQueue_m);
		for(std::multimap< Address,RXQueueEntry >::iterator i(_rxQueue.begin());i!=_rxQueue.end();) {
			if ((now - i->second.creationTime) > ZT_RECEIVE_QUEUE_TIMEOUT) {
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

void Switch::_CBaddPeerFromHello(void *arg,const SharedPtr<Peer> &p,Topology::PeerVerifyResult result)
{
	_CBaddPeerFromHello_Data *req = (_CBaddPeerFromHello_Data *)arg;
	const RuntimeEnvironment *_r = req->parent->_r;

	switch(result) {
		case Topology::PEER_VERIFY_ACCEPTED_NEW:
		case Topology::PEER_VERIFY_ACCEPTED_ALREADY_HAVE:
		case Topology::PEER_VERIFY_ACCEPTED_DISPLACED_INVALID_ADDRESS: {
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
	Switch *d = (Switch *)arg;

	switch(result) {
		case Topology::PEER_VERIFY_ACCEPTED_NEW:
		case Topology::PEER_VERIFY_ACCEPTED_ALREADY_HAVE:
		case Topology::PEER_VERIFY_ACCEPTED_DISPLACED_INVALID_ADDRESS:
			d->_outstandingWhoisRequests_m.lock();
			d->_outstandingWhoisRequests.erase(p->identity().address());
			d->_outstandingWhoisRequests_m.unlock();
			d->_retryPendingFor(p->identity().address());
			break;
		default:
			break;
	}
}

void Switch::_propagateMulticast(const SharedPtr<Network> &network,unsigned char *bloom,const MulticastGroup &mg,unsigned int mcHops,unsigned int mcLoadFactor,const MAC &from,unsigned int etherType,const void *data,unsigned int len)
{
	SharedPtr<Peer> propPeers[ZT_MULTICAST_PROPAGATION_BREADTH];
	unsigned int np = _r->topology->pickMulticastPropagationPeers(network->id(),Address(),bloom,ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE * 8,ZT_MULTICAST_PROPAGATION_BREADTH,mg,propPeers);

	for(unsigned int i=0;i<np;++i)
		Utils::bloomAdd(bloom,ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE,propPeers[i]->address().sum());

	for(unsigned int i=0;i<np;++i) {
		Packet outp(propPeers[i]->address(),_r->identity.address(),Packet::VERB_MULTICAST_FRAME);
		outp.append(network->id());
		outp.append(mg.mac().data,6);
		outp.append((uint32_t)mg.adi());
		outp.append(bloom,ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE);
		outp.append((uint8_t)mcHops);
		outp.append((uint16_t)mcLoadFactor);
		outp.append(from.data,6);
		outp.append((uint16_t)etherType);
		outp.append(data,len);
		outp.compress();
		send(outp,true);
	}
}

Switch::PacketServiceAttemptResult Switch::_tryHandleRemotePacket(Demarc::Port localPort,const InetAddress &fromAddr,Packet &packet)
{
	// NOTE: We assume any packet that's made it here is for us. If it's not it
	// will fail HMAC validation and be discarded anyway, amounting to a second
	// layer of sanity checking.

	Address source(packet.source());

	if ((!packet.encrypted())&&(packet.verb() == Packet::VERB_HELLO)) {
		// Unencrypted HELLOs are handled here since they are used to
		// populate our identity cache in the first place. Thus we might get
		// a HELLO for someone for whom we don't have a Peer record.
		TRACE("HELLO from %s(%s)",source.toString().c_str(),fromAddr.toString().c_str());
		_doHELLO(localPort,fromAddr,packet);
		return PACKET_SERVICE_ATTEMPT_OK;
	}

	SharedPtr<Peer> peer = _r->topology->getPeer(source);
	if (peer) {
		uint64_t now = Utils::now();
		unsigned int latency = 0;

		if (!packet.hmacVerify(peer->macKey())) {
			TRACE("dropped packet from %s(%s), HMAC authentication failed (size: %u)",source.toString().c_str(),fromAddr.toString().c_str(),packet.size());
			return PACKET_SERVICE_ATTEMPT_OK;
		}
		if (packet.encrypted()) {
			packet.decrypt(peer->cryptKey());
		} else if (packet.verb() != Packet::VERB_NOP) {
			TRACE("ODD: %s from %s wasn't encrypted",Packet::verbString(packet.verb()),source.toString().c_str());
		}
		if (!packet.uncompress()) {
			TRACE("dropped packet from %s(%s), compressed data invalid",source.toString().c_str(),fromAddr.toString().c_str());
			return PACKET_SERVICE_ATTEMPT_OK;
		}

		switch(packet.verb()) {
			case Packet::VERB_NOP: // these are sent for NAT-t
				TRACE("NOP from %s(%s) (probably NAT-t)",source.toString().c_str(),fromAddr.toString().c_str());
				break;
			case Packet::VERB_HELLO: // usually they're handled up top, but technically an encrypted HELLO is legal
				_doHELLO(localPort,fromAddr,packet);
				break;
			case Packet::VERB_ERROR:
				try {
#ifdef ZT_TRACE
					Packet::Verb inReVerb = (Packet::Verb)packet[ZT_PROTO_VERB_ERROR_IDX_IN_RE_VERB];
					Packet::ErrorCode errorCode = (Packet::ErrorCode)packet[ZT_PROTO_VERB_ERROR_IDX_ERROR_CODE];
					TRACE("ERROR %s from %s in-re %s",Packet::errorString(errorCode),source.toString().c_str(),Packet::verbString(inReVerb));
#endif
					// TODO: handle key errors, such as duplicate identity
				} catch (std::exception &ex) {
					TRACE("dropped ERROR from %s: unexpected exception: %s",source.toString().c_str(),ex.what());
				} catch ( ... ) {
					TRACE("dropped ERROR from %s: unexpected exception: (unknown)",source.toString().c_str());
				}
				break;
			case Packet::VERB_OK:
				try {
					Packet::Verb inReVerb = (Packet::Verb)packet[ZT_PROTO_VERB_OK_IDX_IN_RE_VERB];
					switch(inReVerb) {
						case Packet::VERB_HELLO:
							latency = std::min((unsigned int)(now - packet.at<uint64_t>(ZT_PROTO_VERB_HELLO__OK__IDX_TIMESTAMP)),(unsigned int)0xffff);
							TRACE("OK(HELLO), latency to %s: %u",source.toString().c_str(),latency);
							break;
						case Packet::VERB_WHOIS:
							// Right now we only query supernodes for WHOIS and only accept
							// OK back from them. If we query other nodes, we'll have to
							// do something to prevent WHOIS cache poisoning such as
							// using the packet ID field in the OK packet to match with the
							// original query. Technically we should be doing this anyway.
							if (_r->topology->isSupernode(source))
								_r->topology->addPeer(SharedPtr<Peer>(new Peer(_r->identity,Identity(packet,ZT_PROTO_VERB_WHOIS__OK__IDX_IDENTITY))),&Switch::_CBaddPeerFromWhois,this);
							break;
						default:
							break;
					}
				} catch (std::exception &ex) {
					TRACE("dropped OK from %s: unexpected exception: %s",source.toString().c_str(),ex.what());
				} catch ( ... ) {
					TRACE("dropped OK from %s: unexpected exception: (unknown)",source.toString().c_str());
				}
				break;
			case Packet::VERB_WHOIS: {
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
					TRACE("dropped WHOIS from %s: missing or invalid address",source.toString().c_str());
				}
			}	break;
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
							TRACE("dropped corrupt RENDEZVOUS from %s (bad address or port)",source.toString().c_str());
						}
					} else {
						TRACE("ignored RENDEZVOUS from %s for unknown peer %s",source.toString().c_str(),with.toString().c_str());
					}
				} catch (std::exception &ex) {
					TRACE("dropped RENDEZVOUS from %s: %s",source.toString().c_str(),ex.what());
				} catch ( ... ) {
					TRACE("dropped RENDEZVOUS from %s: unexpected exception",source.toString().c_str());
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
							TRACE("dropped FRAME from %s: not a member of closed network %llu",source.toString().c_str(),network->id());
						}
					} else {
						TRACE("dropped FRAME from %s: network %llu unknown",source.toString().c_str(),packet.at<uint64_t>(ZT_PROTO_VERB_FRAME_IDX_NETWORK_ID));
					}
				} catch (std::exception &ex) {
					TRACE("dropped FRAME from %s: unexpected exception: %s",source.toString().c_str(),ex.what());
				} catch ( ... ) {
					TRACE("dropped FRAME from %s: unexpected exception: (unknown)",source.toString().c_str());
				}
				break;
			case Packet::VERB_MULTICAST_FRAME:
				try {
					SharedPtr<Network> network(_r->nc->network(packet.at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID)));
					if (network) {
						if (network->isAllowed(source)) {
							if (packet.size() > ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD) {
								MulticastGroup mg(MAC(packet.field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_MULTICAST_MAC,6)),packet.at<uint32_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ADI));
								unsigned char bloom[ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE];
								memcpy(bloom,packet.field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_BLOOM,ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE),ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE);
								unsigned int hops = packet[ZT_PROTO_VERB_MULTICAST_FRAME_IDX_HOPS];
								unsigned int loadFactor = packet.at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_LOAD_FACTOR);
								MAC fromMac(packet.field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FROM_MAC,6));
								unsigned int etherType = packet.at<uint16_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_ETHERTYPE);

								if ((fromMac.isZeroTier())&&(network->isAllowed(Address(fromMac)))) {
									if (_checkAndUpdateMulticastHistory(fromMac,mg.mac(),packet.data() + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD,packet.size() - ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD,network->id(),now)) {
										TRACE("dropped MULTICAST_FRAME from %s: duplicate multicast",source.toString().c_str());
									} else {
										//TRACE("MULTICAST_FRAME: %s -> %s (adi: %.8lx), %u bytes, net: %llu",fromMac.toString().c_str(),mg.mac().toString().c_str(),(unsigned long)mg.adi(),packet.size() - ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD,network->id());
										network->tap().put(fromMac,mg.mac(),etherType,packet.data() + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD,packet.size() - ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD);

										// TODO: implement load factor based propagation rate limitation
										// How it will work: each node will adjust loadFactor based on
										// its current load of multicast traffic. Then it will probabilistically
										// fail to propagate, with the probability being based on load factor.
										// This will need some in-the-field testing and tuning to get right.
										_propagateMulticast(network,bloom,mg,hops+1,loadFactor,fromMac,etherType,packet.data() + ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD,packet.size() - ZT_PROTO_VERB_MULTICAST_FRAME_IDX_PAYLOAD);
									}
								} else {
									TRACE("dropped MULTICAST_FRAME from %s: ultimate sender %s not a member of closed network %llu",source.toString().c_str(),fromMac.toString().c_str(),network->id());
								}
							}
						} else {
							TRACE("dropped MULTICAST_FRAME from %s: not a member of closed network %llu",source.toString().c_str(),network->id());
						}
					} else {
						TRACE("dropped MULTICAST_FRAME from %s: network %llu unknown",source.toString().c_str(),packet.at<uint64_t>(ZT_PROTO_VERB_MULTICAST_FRAME_IDX_NETWORK_ID));
					}
				} catch (std::exception &ex) {
					TRACE("dropped MULTICAST_FRAME from %s: unexpected exception: %s",source.toString().c_str(),ex.what());
				} catch ( ... ) {
					TRACE("dropped MULTICAST_FRAME from %s: unexpected exception: (unknown)",source.toString().c_str());
				}
				break;
			case Packet::VERB_MULTICAST_LIKE:
				try {
					unsigned int ptr = ZT_PACKET_IDX_PAYLOAD;
					unsigned int numAccepted = 0;
					while ((ptr + 18) <= packet.size()) {
						uint64_t nwid = packet.at<uint64_t>(ptr); ptr += 8;
						SharedPtr<Network> network(_r->nc->network(nwid));
						if (network) {
							if (network->isAllowed(source)) {
								MAC mac(packet.field(ptr,6)); ptr += 6;
								uint32_t adi = packet.at<uint32_t>(ptr); ptr += 4;
								TRACE("peer %s likes multicast group %s:%.8lx on network %llu",source.toString().c_str(),mac.toString().c_str(),(unsigned long)adi,nwid);
								_r->topology->likesMulticastGroup(nwid,MulticastGroup(mac,adi),source,now);
								++numAccepted;
							} else {
								TRACE("ignored MULTICAST_LIKE from %s: not a member of closed network %llu",source.toString().c_str(),nwid);
							}
						} else {
							TRACE("ignored MULTICAST_LIKE from %s: network %llu unknown",source.toString().c_str(),nwid);
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
					TRACE("dropped MULTICAST_LIKE from %s: unexpected exception: %s",source.toString().c_str(),ex.what());
				} catch ( ... ) {
					TRACE("dropped MULTICAST_LIKE from %s: unexpected exception: (unknown)",source.toString().c_str());
				}
				break;
			default:
				TRACE("ignored unrecognized verb %.2x from %s",(unsigned int)packet.verb(),source.toString().c_str());
				break;
		}

		// Update peer timestamps and learn new links
		peer->onReceive(_r,localPort,fromAddr,latency,packet.hops(),packet.verb(),now);
	} else return PACKET_SERVICE_ATTEMPT_PEER_UNKNOWN;

	return PACKET_SERVICE_ATTEMPT_OK;
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

void Switch::_requestWhois(const Address &addr)
{
	TRACE("requesting WHOIS for %s",addr.toString().c_str());
	_sendWhoisRequest(addr,(const Address *)0,0);
	Mutex::Lock _l(_outstandingWhoisRequests_m);
	std::pair< std::map< Address,WhoisRequest >::iterator,bool > entry(_outstandingWhoisRequests.insert(std::pair<Address,WhoisRequest>(addr,WhoisRequest())));
	entry.first->second.lastSent = Utils::now();
	entry.first->second.retries = 0; // reset retry count if entry already existed
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

Switch::PacketServiceAttemptResult Switch::_trySend(const Packet &packet,bool encrypt)
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
				return PACKET_SERVICE_ATTEMPT_SEND_FAILED;
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
						return PACKET_SERVICE_ATTEMPT_SEND_FAILED;
					}
					fragStart += chunkSize;
					remaining -= chunkSize;
				}
			}

			return PACKET_SERVICE_ATTEMPT_OK;
		}
		return PACKET_SERVICE_ATTEMPT_SEND_FAILED;
	}
	return PACKET_SERVICE_ATTEMPT_PEER_UNKNOWN;
}

void Switch::_retryPendingFor(const Address &addr)
{
	{
		Mutex::Lock _l(_txQueue_m);
		std::pair< std::multimap< Address,TXQueueEntry >::iterator,std::multimap< Address,TXQueueEntry >::iterator > eqrange = _txQueue.equal_range(addr);
		for(std::multimap< Address,TXQueueEntry >::iterator i(eqrange.first);i!=eqrange.second;) {
			if (_trySend(i->second.packet,i->second.encrypt) == PACKET_SERVICE_ATTEMPT_OK)
				_txQueue.erase(i++);
			else ++i;
		}
	}
	{
		Mutex::Lock _l(_rxQueue_m);
		std::pair< std::multimap< Address,RXQueueEntry >::iterator,std::multimap< Address,RXQueueEntry >::iterator > eqrange = _rxQueue.equal_range(addr);
		for(std::multimap< Address,RXQueueEntry >::iterator i(eqrange.first);i!=eqrange.second;) {
			if (_tryHandleRemotePacket(i->second.localPort,i->second.fromAddr,i->second.packet) == PACKET_SERVICE_ATTEMPT_OK)
				_rxQueue.erase(i++);
			else ++i;
		}
	}
}

} // namespace ZeroTier
