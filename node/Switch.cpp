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

#include "Constants.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#endif

#include "Switch.hpp"
#include "Node.hpp"
#include "EthernetTap.hpp"
#include "InetAddress.hpp"
#include "Topology.hpp"
#include "RuntimeEnvironment.hpp"
#include "Peer.hpp"
#include "NodeConfig.hpp"
#include "Demarc.hpp"
#include "CMWC4096.hpp"

#include "../version.h"

namespace ZeroTier {

Switch::Switch(const RuntimeEnvironment *renv) :
	_r(renv),
	_multicastIdCounter((unsigned int)renv->prng->next32()) // start a random spot to minimize possible collisions on startup
{
}

Switch::~Switch()
{
}

void Switch::onRemotePacket(Demarc::Port localPort,const InetAddress &fromAddr,const Buffer<4096> &data)
{
	try {
		if (data.size() >= ZT_PROTO_MIN_FRAGMENT_LENGTH) {
			if (data[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR)
				_handleRemotePacketFragment(localPort,fromAddr,data);
			else if (data.size() >= ZT_PROTO_MIN_PACKET_LENGTH)
				_handleRemotePacketHead(localPort,fromAddr,data);
		}
	} catch (std::exception &ex) {
		TRACE("dropped packet from %s: unexpected exception: %s",fromAddr.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped packet from %s: unexpected exception: (unknown)",fromAddr.toString().c_str());
	}
}

void Switch::onLocalEthernet(const SharedPtr<Network> &network,const MAC &from,const MAC &to,unsigned int etherType,const Buffer<4096> &data)
{
	SharedPtr<NetworkConfig> nconf(network->config2());
	if (!nconf)
		return;

	if (to == network->mac()) {
		LOG("%s: frame received from self, ignoring (bridge loop? OS bug?)",network->tapDeviceName().c_str());
		return;
	}
	if (from != network->mac()) {
		LOG("ignored tap: %s -> %s %s (bridging not supported)",from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType));
		return;
	}

	if (!nconf->permitsEtherType(etherType)) {
		LOG("ignored tap: %s -> %s: ethertype %s not allowed on network %.16llx",from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType),(unsigned long long)network->id());
		return;
	}

	if (to.isMulticast()) {
		MulticastGroup mg(to,0);

		if (to.isBroadcast()) {
			// Cram IPv4 IP into ADI field to make IPv4 ARP broadcast channel specific and scalable
			if ((etherType == ZT_ETHERTYPE_ARP)&&(data.size() == 28)&&(data[2] == 0x08)&&(data[3] == 0x00)&&(data[4] == 6)&&(data[5] == 4)&&(data[7] == 0x01))
				mg = MulticastGroup::deriveMulticastGroupForAddressResolution(InetAddress(data.field(24,4),4,0));
		}

		const unsigned int mcid = ++_multicastIdCounter & 0xffffff;
		const uint16_t bloomNonce = (uint16_t)(_r->prng->next32() & 0xffff); // doesn't need to be cryptographically strong
		unsigned char bloom[ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_BLOOM];
		unsigned char fifo[ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_FIFO + ZT_ADDRESS_LENGTH];
		unsigned char *const fifoEnd = fifo + sizeof(fifo);
		const unsigned int signedPartLen = (ZT_PROTO_VERB_MULTICAST_FRAME_IDX_FRAME - ZT_PROTO_VERB_MULTICAST_FRAME_IDX__START_OF_SIGNED_PORTION) + data.size();
		const SharedPtr<Peer> supernode(_r->topology->getBestSupernode());

		for(unsigned int prefix=0,np=((unsigned int)2 << (nconf->multicastPrefixBits() - 1));prefix<np;++prefix) {
			memset(bloom,0,sizeof(bloom));

			unsigned char *fifoPtr = fifo;
			_r->mc->getNextHops(network->id(),mg,Multicaster::AddToPropagationQueue(&fifoPtr,fifoEnd,bloom,bloomNonce,_r->identity.address(),nconf->multicastPrefixBits(),prefix));
			while (fifoPtr != fifoEnd)
				*(fifoPtr++) = (unsigned char)0;

			Address firstHop(fifo,ZT_ADDRESS_LENGTH); // fifo is +1 in size, with first element being used here
			if (!firstHop) {
				if (supernode)
					firstHop = supernode->address();
				else continue;
			}

			Packet outp(firstHop,_r->identity.address(),Packet::VERB_MULTICAST_FRAME);
			outp.append((uint16_t)0);
			outp.append(fifo + ZT_ADDRESS_LENGTH,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_FIFO); // remainder of fifo is loaded into packet
			outp.append(bloom,ZT_PROTO_VERB_MULTICAST_FRAME_LEN_PROPAGATION_BLOOM);
			outp.append((nconf->com()) ? (unsigned char)ZT_PROTO_VERB_MULTICAST_FRAME_FLAGS_HAS_MEMBERSHIP_CERTIFICATE : (unsigned char)0);
			outp.append(network->id());
			outp.append(bloomNonce);
			outp.append((unsigned char)nconf->multicastPrefixBits());
			outp.append((unsigned char)prefix);
			_r->identity.address().appendTo(outp);
			outp.append((unsigned char)((mcid >> 16) & 0xff));
			outp.append((unsigned char)((mcid >> 8) & 0xff));
			outp.append((unsigned char)(mcid & 0xff));
			outp.append(from.data,6);
			outp.append(mg.mac().data,6);
			outp.append(mg.adi());
			outp.append((uint16_t)etherType);
			outp.append((uint16_t)data.size());
			outp.append(data);

			C25519::Signature sig(_r->identity.sign(outp.field(ZT_PROTO_VERB_MULTICAST_FRAME_IDX__START_OF_SIGNED_PORTION,signedPartLen),signedPartLen));
			outp.append((uint16_t)sig.size());
			outp.append(sig.data,(unsigned int)sig.size());

			if (nconf->com())
				nconf->com().serialize(outp);

			outp.compress();
			send(outp,true);
		}
	} else if (to.isZeroTier()) {
		// Simple unicast frame from us to another node
		Address toZT(to.data + 1,ZT_ADDRESS_LENGTH);
		if (network->isAllowed(toZT)) {
			network->pushMembershipCertificate(toZT,false,Utils::now());

			Packet outp(toZT,_r->identity.address(),Packet::VERB_FRAME);
			outp.append(network->id());
			outp.append((uint16_t)etherType);
			outp.append(data);
			outp.compress();
			send(outp,true);
		} else {
			TRACE("UNICAST: %s -> %s %s (dropped, destination not a member of closed network %llu)",from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType),network->id());
		}
	} else {
		TRACE("UNICAST: %s -> %s %s (dropped, destination MAC not ZeroTier)",from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType));
	}
}

void Switch::send(const Packet &packet,bool encrypt)
{
	if (packet.destination() == _r->identity.address()) {
		TRACE("BUG: caught attempt to send() to self, ignored");
		return;
	}

	if (!_trySend(packet,encrypt)) {
		Mutex::Lock _l(_txQueue_m);
		_txQueue.insert(std::pair< Address,TXQueueEntry >(packet.destination(),TXQueueEntry(Utils::now(),packet,encrypt)));
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

bool Switch::sendHELLO(const SharedPtr<Peer> &dest,Demarc::Port localPort,const InetAddress &remoteAddr)
{
	uint64_t now = Utils::now();
	Packet outp(dest->address(),_r->identity.address(),Packet::VERB_HELLO);
	outp.append((unsigned char)ZT_PROTO_VERSION);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MAJOR);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MINOR);
	outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
	outp.append(now);
	_r->identity.serialize(outp,false);
	outp.armor(dest->key(),false);

	if (_r->demarc->send(localPort,remoteAddr,outp.data(),outp.size(),-1)) {
		dest->expectResponseTo(outp.packetId(),Packet::VERB_HELLO,localPort,now);
		return true;
	} else return false;
}

bool Switch::unite(const Address &p1,const Address &p2,bool force)
{
	if ((p1 == _r->identity.address())||(p2 == _r->identity.address()))
		return false;

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

	/* Tell P1 where to find P2 and vice versa, sending the packets to P1 and
	 * P2 in randomized order in terms of which gets sent first. This is done
	 * since in a few cases NAT-t can be sensitive to slight timing differences
	 * in terms of when the two peers initiate. Normally this is accounted for
	 * by the nearly-simultaneous RENDEZVOUS kickoff from the supernode, but
	 * given that supernodes are hosted on cloud providers this can in some
	 * cases have a few ms of latency between packet departures. By randomizing
	 * the order we make each attempted NAT-t favor one or the other going
	 * first, meaning if it doesn't succeed the first time it might the second
	 * and so forth. */
	unsigned int alt = _r->prng->next32() & 1;
	unsigned int completed = alt + 2;
	while (alt != completed) {
		if ((alt & 1) == 0) {
			// Tell p1 where to find p2.
			Packet outp(p1,_r->identity.address(),Packet::VERB_RENDEZVOUS);
			outp.append((unsigned char)0);
			p2.appendTo(outp);
			outp.append((uint16_t)cg.first.port());
			if (cg.first.isV6()) {
				outp.append((unsigned char)16);
				outp.append(cg.first.rawIpData(),16);
			} else {
				outp.append((unsigned char)4);
				outp.append(cg.first.rawIpData(),4);
			}
			outp.armor(p1p->key(),true);
			p1p->send(_r,outp.data(),outp.size(),now);
		} else {
			// Tell p2 where to find p1.
			Packet outp(p2,_r->identity.address(),Packet::VERB_RENDEZVOUS);
			outp.append((unsigned char)0);
			p1.appendTo(outp);
			outp.append((uint16_t)cg.second.port());
			if (cg.second.isV6()) {
				outp.append((unsigned char)16);
				outp.append(cg.second.rawIpData(),16);
			} else {
				outp.append((unsigned char)4);
				outp.append(cg.second.rawIpData(),4);
			}
			outp.armor(p2p->key(),true);
			p2p->send(_r,outp.data(),outp.size(),now);
		}
		++alt; // counts up and also flips LSB
	}

	return true;
}

void Switch::contact(const SharedPtr<Peer> &peer,const InetAddress &atAddr)
{
	Demarc::Port fromPort = _r->demarc->pick(atAddr);
	_r->demarc->send(fromPort,atAddr,"\0",1,ZT_FIREWALL_OPENER_HOPS);

	{
		Mutex::Lock _l(_contactQueue_m);
		_contactQueue.push_back(ContactQueueEntry(peer,Utils::now() + ZT_RENDEZVOUS_NAT_T_DELAY,fromPort,atAddr));
	}

	// Kick main loop out of wait so that it can pick up this
	// change to our scheduled timer tasks.
	_r->mainLoopWaitCondition.signal();
}

unsigned long Switch::doTimerTasks()
{
	unsigned long nextDelay = ~((unsigned long)0); // big number, caller will cap return value
	uint64_t now = Utils::now();

	{
		Mutex::Lock _l(_contactQueue_m);
		for(std::list<ContactQueueEntry>::iterator qi(_contactQueue.begin());qi!=_contactQueue.end();) {
			if (now >= qi->fireAtTime) {
				TRACE("sending NAT-T HELLO to %s(%s)",qi->peer->address().toString().c_str(),qi->inaddr.toString().c_str());
				sendHELLO(qi->peer,qi->localPort,qi->inaddr);
				_contactQueue.erase(qi++);
			} else {
				nextDelay = std::min(nextDelay,(unsigned long)(qi->fireAtTime - now));
				++qi;
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
		for(std::list< SharedPtr<PacketDecoder> >::iterator i(_rxQueue.begin());i!=_rxQueue.end();) {
			if ((now - (*i)->receiveTime()) > ZT_RECEIVE_QUEUE_TIMEOUT) {
				TRACE("RX %s -> %s timed out",(*i)->source().toString().c_str(),(*i)->destination().toString().c_str());
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

	return std::max(nextDelay,(unsigned long)10); // minimum delay
}

void Switch::announceMulticastGroups(const std::map< SharedPtr<Network>,std::set<MulticastGroup> > &allMemberships)
{
	std::vector< SharedPtr<Peer> > directPeers;
	_r->topology->eachPeer(Topology::CollectPeersWithActiveDirectPath(directPeers,Utils::now()));

#ifdef ZT_TRACE
	unsigned int totalMulticastGroups = 0;
	for(std::map< SharedPtr<Network>,std::set<MulticastGroup> >::const_iterator i(allMemberships.begin());i!=allMemberships.end();++i)
		totalMulticastGroups += (unsigned int)i->second.size();
	TRACE("announcing %u multicast groups for %u networks to %u peers",totalMulticastGroups,(unsigned int)allMemberships.size(),(unsigned int)directPeers.size());
#endif

	uint64_t now = Utils::now();
	for(std::vector< SharedPtr<Peer> >::iterator p(directPeers.begin());p!=directPeers.end();++p) {
		Packet outp((*p)->address(),_r->identity.address(),Packet::VERB_MULTICAST_LIKE);

		for(std::map< SharedPtr<Network>,std::set<MulticastGroup> >::const_iterator nwmgs(allMemberships.begin());nwmgs!=allMemberships.end();++nwmgs) {
			nwmgs->first->pushMembershipCertificate((*p)->address(),false,now);

			if ((_r->topology->isSupernode((*p)->address()))||(nwmgs->first->isAllowed((*p)->address()))) {
				for(std::set<MulticastGroup>::iterator mg(nwmgs->second.begin());mg!=nwmgs->second.end();++mg) {
					if ((outp.size() + 18) > ZT_UDP_DEFAULT_PAYLOAD_MTU) {
						send(outp,true);
						outp.reset((*p)->address(),_r->identity.address(),Packet::VERB_MULTICAST_LIKE);
					}

					// network ID, MAC, ADI
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

void Switch::announceMulticastGroups(const SharedPtr<Peer> &peer)
{
	Packet outp(peer->address(),_r->identity.address(),Packet::VERB_MULTICAST_LIKE);
	std::vector< SharedPtr<Network> > networks(_r->nc->networks());
	uint64_t now = Utils::now();
	for(std::vector< SharedPtr<Network> >::iterator n(networks.begin());n!=networks.end();++n) {
		if (((*n)->isAllowed(peer->address()))||(_r->topology->isSupernode(peer->address()))) {
			(*n)->pushMembershipCertificate(peer->address(),false,now);

			std::set<MulticastGroup> mgs((*n)->multicastGroups());
			for(std::set<MulticastGroup>::iterator mg(mgs.begin());mg!=mgs.end();++mg) {
				if ((outp.size() + 18) > ZT_UDP_DEFAULT_PAYLOAD_MTU) {
					send(outp,true);
					outp.reset(peer->address(),_r->identity.address(),Packet::VERB_MULTICAST_LIKE);
				}

				// network ID, MAC, ADI
				outp.append((uint64_t)(*n)->id());
				outp.append(mg->mac().data,6);
				outp.append((uint32_t)mg->adi());
			}
		}
	}
	if (outp.size() > ZT_PROTO_MIN_PACKET_LENGTH)
		send(outp,true);
}

void Switch::requestWhois(const Address &addr)
{
	//TRACE("requesting WHOIS for %s",addr.toString().c_str());
	bool inserted = false;
	{
		Mutex::Lock _l(_outstandingWhoisRequests_m);
		std::pair< std::map< Address,WhoisRequest >::iterator,bool > entry(_outstandingWhoisRequests.insert(std::pair<Address,WhoisRequest>(addr,WhoisRequest())));
		if ((inserted = entry.second))
			entry.first->second.lastSent = Utils::now();
		entry.first->second.retries = 0; // reset retry count if entry already existed
	}
	if (inserted)
		_sendWhoisRequest(addr,(const Address *)0,0);
}

void Switch::cancelWhoisRequest(const Address &addr)
{
	Mutex::Lock _l(_outstandingWhoisRequests_m);
	_outstandingWhoisRequests.erase(addr);
}

void Switch::doAnythingWaitingForPeer(const SharedPtr<Peer> &peer)
{
	{
		Mutex::Lock _l(_outstandingWhoisRequests_m);
		_outstandingWhoisRequests.erase(peer->address());
	}

	{
		Mutex::Lock _l(_rxQueue_m);
		for(std::list< SharedPtr<PacketDecoder> >::iterator rxi(_rxQueue.begin());rxi!=_rxQueue.end();) {
			if ((*rxi)->tryDecode(_r))
				_rxQueue.erase(rxi++);
			else ++rxi;
		}
	}

	{
		Mutex::Lock _l(_txQueue_m);
		std::pair< std::multimap< Address,TXQueueEntry >::iterator,std::multimap< Address,TXQueueEntry >::iterator > waitingTxQueueItems(_txQueue.equal_range(peer->address()));
		for(std::multimap< Address,TXQueueEntry >::iterator txi(waitingTxQueueItems.first);txi!=waitingTxQueueItems.second;) {
			if (_trySend(txi->second.packet,txi->second.encrypt))
				_txQueue.erase(txi++);
			else ++txi;
		}
	}
}

const char *Switch::etherTypeName(const unsigned int etherType)
	throw()
{
	switch(etherType) {
		case ZT_ETHERTYPE_IPV4:  return "IPV4";
		case ZT_ETHERTYPE_ARP:   return "ARP";
		case ZT_ETHERTYPE_RARP:  return "RARP";
		case ZT_ETHERTYPE_ATALK: return "ATALK";
		case ZT_ETHERTYPE_AARP:  return "AARP";
		case ZT_ETHERTYPE_IPX_A: return "IPX_A";
		case ZT_ETHERTYPE_IPX_B: return "IPX_B";
		case ZT_ETHERTYPE_IPV6:  return "IPV6";
	}
	return "UNKNOWN";
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
			if ((!relayTo)||(!relayTo->send(_r,fragment.data(),fragment.size(),Utils::now()))) {
				relayTo = _r->topology->getBestSupernode();
				if (relayTo)
					relayTo->send(_r,fragment.data(),fragment.size(),Utils::now());
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

					SharedPtr<PacketDecoder> packet(dqe->second.frag0);
					for(unsigned int f=1;f<tf;++f)
						packet->append(dqe->second.frags[f - 1].payload(),dqe->second.frags[f - 1].payloadLength());
					_defragQueue.erase(dqe);

					if (!packet->tryDecode(_r)) {
						Mutex::Lock _l(_rxQueue_m);
						_rxQueue.push_back(packet);
					}
				}
			} // else this is a duplicate fragment, ignore
		}
	}
}

void Switch::_handleRemotePacketHead(Demarc::Port localPort,const InetAddress &fromAddr,const Buffer<4096> &data)
{
	SharedPtr<PacketDecoder> packet(new PacketDecoder(data,localPort,fromAddr));

	Address source(packet->source());
	Address destination(packet->destination());

	//TRACE("<< %.16llx %s -> %s (size: %u)",(unsigned long long)packet->packetId(),source.toString().c_str(),destination.toString().c_str(),packet->size());

	if (destination != _r->identity.address()) {
		// Packet is not for us, so try to relay it
		if (packet->hops() < ZT_RELAY_MAX_HOPS) {
			packet->incrementHops();

			SharedPtr<Peer> relayTo = _r->topology->getPeer(destination);
			if ((relayTo)&&(relayTo->send(_r,packet->data(),packet->size(),Utils::now()))) {
				// If we've relayed, this periodically tries to get them to
				// talk directly to save our bandwidth.
				unite(source,destination,false);
			} else {
				// If we've received a packet not for us and we don't have
				// a direct path to its recipient, pass it to (another)
				// supernode. This can happen due to Internet weather -- the
				// most direct supernode may not be reachable, yet another
				// further away may be.
				relayTo = _r->topology->getBestSupernode(&source,1,true);
				if (relayTo)
					relayTo->send(_r,packet->data(),packet->size(),Utils::now());
			}
		} else {
			TRACE("dropped relay %s(%s) -> %s, max hops exceeded",packet->source().toString().c_str(),fromAddr.toString().c_str(),destination.toString().c_str());
		}
	} else if (packet->fragmented()) {
		// Packet is the head of a fragmented packet series

		uint64_t pid = packet->packetId();
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
					packet->append(dqe->second.frags[f - 1].payload(),dqe->second.frags[f - 1].payloadLength());
				_defragQueue.erase(dqe);

				if (!packet->tryDecode(_r)) {
					Mutex::Lock _l(_rxQueue_m);
					_rxQueue.push_back(packet);
				}
			} else {
				// Still waiting on more fragments, so queue the head
				dqe->second.frag0 = packet;
			}
		} // else this is a duplicate head, ignore
	} else {
		// Packet is unfragmented, so just process it
		if (!packet->tryDecode(_r)) {
			Mutex::Lock _l(_rxQueue_m);
			_rxQueue.push_back(packet);
		}
	}
}

Address Switch::_sendWhoisRequest(const Address &addr,const Address *peersAlreadyConsulted,unsigned int numPeersAlreadyConsulted)
{
	SharedPtr<Peer> supernode(_r->topology->getBestSupernode(peersAlreadyConsulted,numPeersAlreadyConsulted,false));
	if (supernode) {
		Packet outp(supernode->address(),_r->identity.address(),Packet::VERB_WHOIS);
		addr.appendTo(outp);
		outp.armor(supernode->key(),true);
		uint64_t now = Utils::now();
		if (supernode->send(_r,outp.data(),outp.size(),now))
			return supernode->address();
	}
	return Address();
}

bool Switch::_trySend(const Packet &packet,bool encrypt)
{
	SharedPtr<Peer> peer(_r->topology->getPeer(packet.destination()));

	if (peer) {
		uint64_t now = Utils::now();

		SharedPtr<Peer> via;
		if ((_r->topology->isSupernode(peer->address()))||(peer->hasActiveDirectPath(now))) {
			via = peer;
		} else {
			via = _r->topology->getBestSupernode();
			if (!via)
				return false;
		}

		Packet tmp(packet);

		unsigned int chunkSize = std::min(tmp.size(),(unsigned int)ZT_UDP_DEFAULT_PAYLOAD_MTU);
		tmp.setFragmented(chunkSize < tmp.size());

		tmp.armor(peer->key(),encrypt);

		Demarc::Port localPort;
		if ((localPort = via->send(_r,tmp.data(),chunkSize,now))) {
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
					if (!via->send(_r,frag.data(),frag.size(),now)) {
						TRACE("WARNING: packet send to %s failed on later fragment #%u (check IP layer buffer sizes?)",via->address().toString().c_str(),f + 1);
					}
					fragStart += chunkSize;
					remaining -= chunkSize;
				}
			}

			switch(packet.verb()) {
				case Packet::VERB_HELLO:
					peer->expectResponseTo(packet.packetId(),Packet::VERB_HELLO,localPort,now);
					break;
				default:
					break;
			}

			return true;
		}
		return false;
	}

	requestWhois(packet.destination());
	return false;
}

} // namespace ZeroTier
