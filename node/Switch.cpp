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
#include "CMWC4096.hpp"
#include "AntiRecursion.hpp"

#include "../version.h"

namespace ZeroTier {

Switch::Switch(const RuntimeEnvironment *renv) :
	RR(renv),
	_lastBeacon(0)
{
}

Switch::~Switch()
{
}

void Switch::onRemotePacket(const SharedPtr<Socket> &fromSock,const InetAddress &fromAddr,Buffer<ZT_SOCKET_MAX_MESSAGE_LEN> &data)
{
	try {
		if (data.size() == ZT_PROTO_BEACON_LENGTH) {
			_handleBeacon(fromSock,fromAddr,data);
		} else if (data.size() > ZT_PROTO_MIN_FRAGMENT_LENGTH) {
			if (data[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR)
				_handleRemotePacketFragment(fromSock,fromAddr,data);
			else if (data.size() >= ZT_PROTO_MIN_PACKET_LENGTH)
				_handleRemotePacketHead(fromSock,fromAddr,data);
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

	// Sanity check -- bridge loop? OS problem?
	if (to == network->mac())
		return;

	/* Check anti-recursion module to ensure that this is not ZeroTier talking over its own links.
	 * Note: even when we introduce a more purposeful binding of the main UDP port, this can
	 * still happen because Windows likes to send broadcasts over interfaces that have little
	 * to do with their intended target audience. :P */
	if (!RR->antiRec->checkEthernetFrame(data.data(),data.size())) {
		TRACE("%s: rejected recursively addressed ZeroTier packet by tail match (type %s, length: %u)",network->tapDeviceName().c_str(),etherTypeName(etherType),data.size());
		return;
	}

	// Check to make sure this protocol is allowed on this network
	if (!nconf->permitsEtherType(etherType)) {
		TRACE("%s: ignored tap: %s -> %s: ethertype %s not allowed on network %.16llx",network->tapDeviceName().c_str(),from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType),(unsigned long long)network->id());
		return;
	}

	// Check if this packet is from someone other than the tap -- i.e. bridged in
	bool fromBridged = false;
	if (from != network->mac()) {
		if (!network->permitsBridging(RR->identity.address())) {
			LOG("%s: %s -> %s %s not forwarded, bridging disabled on %.16llx or this peer not a bridge",network->tapDeviceName().c_str(),from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType),network->id());
			return;
		}
		fromBridged = true;
	}

	if (to.isMulticast()) {
		// Destination is a multicast address (including broadcast)

		uint64_t now = Utils::now();
		MulticastGroup mg(to,0);

		if (to.isBroadcast()) {
			if ((etherType == ZT_ETHERTYPE_ARP)&&(data.size() >= 28)&&(data[2] == 0x08)&&(data[3] == 0x00)&&(data[4] == 6)&&(data[5] == 4)&&(data[7] == 0x01)) {
				// Cram IPv4 IP into ADI field to make IPv4 ARP broadcast channel specific and scalable
				// Also: enableBroadcast() does not apply to ARP since it's required for IPv4
				mg = MulticastGroup::deriveMulticastGroupForAddressResolution(InetAddress(data.field(24,4),4,0));
			} else if (!nconf->enableBroadcast()) {
				// Don't transmit broadcasts if this network doesn't want them
				TRACE("%s: dropped broadcast since ff:ff:ff:ff:ff:ff is not enabled on network %.16llx",network->tapDeviceName().c_str(),network->id());
				return;
			}
		}

		/* Learn multicast groups for bridged-in hosts.
		 * Note that some OSes, most notably Linux, do this for you by learning
		 * multicast addresses on bridge interfaces and subscribing each slave.
		 * But in that case this does no harm, as the sets are just merged. */
		if (fromBridged)
			network->learnBridgedMulticastGroup(mg,now);

		// Check multicast/broadcast bandwidth quotas and reject if quota exceeded
		if (!network->updateAndCheckMulticastBalance(mg,data.size())) {
			TRACE("%s: didn't multicast %d bytes, quota exceeded for multicast group %s",network->tapDeviceName().c_str(),(int)data.size(),mg.toString().c_str());
			return;
		}

		TRACE("%s: MULTICAST %s -> %s %s %d",network->tapDeviceName().c_str(),from.toString().c_str(),mg.toString().c_str(),etherTypeName(etherType),(int)data.size());

		RR->mc->send(
			((!nconf->isPublic())&&(nconf->com())) ? &(nconf->com()) : (const CertificateOfMembership *)0,
			nconf->multicastLimit(),
			now,
			network->id(),
			nconf->activeBridges(),
			mg,
			(fromBridged) ? from : MAC(),
			etherType,
			data.data(),
			data.size());

		return;
	}

	if (to[0] == MAC::firstOctetForNetwork(network->id())) {
		// Destination is another ZeroTier peer

		Address toZT(to.toAddress(network->id()));
		if (network->isAllowed(toZT)) {
			if (network->peerNeedsOurMembershipCertificate(toZT,Utils::now())) {
				// TODO: once there are no more <1.0.0 nodes around, we can
				// bundle this with EXT_FRAME instead of sending two packets.
				Packet outp(toZT,RR->identity.address(),Packet::VERB_NETWORK_MEMBERSHIP_CERTIFICATE);
				nconf->com().serialize(outp);
				send(outp,true);
			}

			if (fromBridged) {
				// EXT_FRAME is used for bridging or if we want to include a COM
				Packet outp(toZT,RR->identity.address(),Packet::VERB_EXT_FRAME);
				outp.append(network->id());
				outp.append((unsigned char)0);
				to.appendTo(outp);
				from.appendTo(outp);
				outp.append((uint16_t)etherType);
				outp.append(data);
				outp.compress();
				send(outp,true);
			} else {
				// FRAME is a shorter version that can be used when there's no bridging and no COM
				Packet outp(toZT,RR->identity.address(),Packet::VERB_FRAME);
				outp.append(network->id());
				outp.append((uint16_t)etherType);
				outp.append(data);
				outp.compress();
				send(outp,true);
			}
		} else {
			TRACE("%s: UNICAST: %s -> %s %s dropped, destination not a member of closed network %.16llx",network->tapDeviceName().c_str(),from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType),network->id());
		}

		return;
	}

	{
		// Destination is bridged behind a remote peer

		Address bridges[ZT_MAX_BRIDGE_SPAM];
		unsigned int numBridges = 0;

		bridges[0] = network->findBridgeTo(to);
		if ((bridges[0])&&(bridges[0] != RR->identity.address())&&(network->isAllowed(bridges[0]))&&(network->permitsBridging(bridges[0]))) {
			// We have a known bridge route for this MAC.
			++numBridges;
		} else if (!nconf->activeBridges().empty()) {
			/* If there is no known route, spam to up to ZT_MAX_BRIDGE_SPAM active
			 * bridges. This is similar to what many switches do -- if they do not
			 * know which port corresponds to a MAC, they send it to all ports. If
			 * there aren't any active bridges, numBridges will stay 0 and packet
			 * is dropped. */
			std::vector<Address>::const_iterator ab(nconf->activeBridges().begin());
			if (nconf->activeBridges().size() <= ZT_MAX_BRIDGE_SPAM) {
				// If there are <= ZT_MAX_BRIDGE_SPAM active bridges, spam them all
				while (ab != nconf->activeBridges().end()) {
					if (network->isAllowed(*ab)) // config sanity check
						bridges[numBridges++] = *ab;
					++ab;
				}
			} else {
				// Otherwise pick a random set of them
				while (numBridges < ZT_MAX_BRIDGE_SPAM) {
					if (ab == nconf->activeBridges().end())
						ab = nconf->activeBridges().begin();
					if (((unsigned long)RR->prng->next32() % (unsigned long)nconf->activeBridges().size()) == 0) {
						if (network->isAllowed(*ab)) // config sanity check
							bridges[numBridges++] = *ab;
						++ab;
					} else ++ab;
				}
			}
		}

		for(unsigned int b=0;b<numBridges;++b) {
			Packet outp(bridges[b],RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(network->id());
			outp.append((unsigned char)0);
			to.appendTo(outp);
			from.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(data);
			outp.compress();
			send(outp,true);
		}
	}
}

void Switch::send(const Packet &packet,bool encrypt)
{
	if (packet.destination() == RR->identity.address()) {
		TRACE("BUG: caught attempt to send() to self, ignored");
		return;
	}

	if (!_trySend(packet,encrypt)) {
		Mutex::Lock _l(_txQueue_m);
		_txQueue.insert(std::pair< Address,TXQueueEntry >(packet.destination(),TXQueueEntry(Utils::now(),packet,encrypt)));
	}
}

#if 0
void Switch::sendHELLO(const Address &dest)
{
	Packet outp(dest,RR->identity.address(),Packet::VERB_HELLO);
	outp.append((unsigned char)ZT_PROTO_VERSION);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MAJOR);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MINOR);
	outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
	outp.append(Utils::now());
	RR->identity.serialize(outp,false);
	send(outp,false);
}
#endif

bool Switch::unite(const Address &p1,const Address &p2,bool force)
{
	if ((p1 == RR->identity.address())||(p2 == RR->identity.address()))
		return false;

	SharedPtr<Peer> p1p = RR->topology->getPeer(p1);
	if (!p1p)
		return false;
	SharedPtr<Peer> p2p = RR->topology->getPeer(p2);
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
	unsigned int alt = RR->prng->next32() & 1;
	unsigned int completed = alt + 2;
	while (alt != completed) {
		if ((alt & 1) == 0) {
			// Tell p1 where to find p2.
			Packet outp(p1,RR->identity.address(),Packet::VERB_RENDEZVOUS);
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
			p1p->send(RR,outp.data(),outp.size(),now);
		} else {
			// Tell p2 where to find p1.
			Packet outp(p2,RR->identity.address(),Packet::VERB_RENDEZVOUS);
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
			p2p->send(RR,outp.data(),outp.size(),now);
		}
		++alt; // counts up and also flips LSB
	}

	return true;
}

void Switch::contact(const SharedPtr<Peer> &peer,const InetAddress &atAddr)
{
	// Send simple packet directly to indicated address -- works for most NATs
	sendHELLO(peer,atAddr);
	TRACE("sending NAT-t HELLO to %s(%s)",peer->address().toString().c_str(),atAddr.toString().c_str());

	// If we have not punched through after this timeout, open refreshing can of whupass
	{
		Mutex::Lock _l(_contactQueue_m);
		_contactQueue.push_back(ContactQueueEntry(peer,Utils::now() + ZT_NAT_T_TACTICAL_ESCALATION_DELAY,atAddr));
	}

	// Kick main loop out of wait so that it can pick up this
	// change to our scheduled timer tasks.
	RR->sm->whack();
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
	{	// cancel pending WHOIS since we now know this peer
		Mutex::Lock _l(_outstandingWhoisRequests_m);
		_outstandingWhoisRequests.erase(peer->address());
	}

	{	// finish processing any packets waiting on peer's public key / identity
		Mutex::Lock _l(_rxQueue_m);
		for(std::list< SharedPtr<IncomingPacket> >::iterator rxi(_rxQueue.begin());rxi!=_rxQueue.end();) {
			if ((*rxi)->tryDecode(RR))
				_rxQueue.erase(rxi++);
			else ++rxi;
		}
	}

	{	// finish sending any packets waiting on peer's public key / identity
		Mutex::Lock _l(_txQueue_m);
		std::pair< std::multimap< Address,TXQueueEntry >::iterator,std::multimap< Address,TXQueueEntry >::iterator > waitingTxQueueItems(_txQueue.equal_range(peer->address()));
		for(std::multimap< Address,TXQueueEntry >::iterator txi(waitingTxQueueItems.first);txi!=waitingTxQueueItems.second;) {
			if (_trySend(txi->second.packet,txi->second.encrypt))
				_txQueue.erase(txi++);
			else ++txi;
		}
	}
}

unsigned long Switch::doTimerTasks()
{
	unsigned long nextDelay = ~((unsigned long)0); // big number, caller will cap return value
	uint64_t now = Utils::now();

	{
		Mutex::Lock _l(_contactQueue_m);
		for(std::list<ContactQueueEntry>::iterator qi(_contactQueue.begin());qi!=_contactQueue.end();) {
			if (now >= qi->fireAtTime) {
				if (!qi->peer->hasActiveDirectPath(now)) {
					TRACE("deploying aggressive NAT-t against %s(%s)",qi->peer->address().toString().c_str(),qi->inaddr.toString().c_str());

					/* Shotgun approach -- literally -- against symmetric NATs. Most of these
					 * either increment or decrement ports so this gets a good number. Also try
					 * the original port one more time for good measure, since sometimes it
					 * fails first time around. */
					int p = (int)qi->inaddr.port() - 2;
					for(int k=0;k<6;++k) {
						if ((p > 0)&&(p <= 0xffff)) {
							qi->inaddr.setPort((unsigned int)p);
							sendHELLO(qi->peer,qi->inaddr);
						}
						++p;
					}
				}

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
		for(std::list< SharedPtr<IncomingPacket> >::iterator i(_rxQueue.begin());i!=_rxQueue.end();) {
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

void Switch::_handleRemotePacketFragment(const SharedPtr<Socket> &fromSock,const InetAddress &fromAddr,const Buffer<4096> &data)
{
	Packet::Fragment fragment(data);
	Address destination(fragment.destination());

	if (destination != RR->identity.address()) {
		// Fragment is not for us, so try to relay it
		if (fragment.hops() < ZT_RELAY_MAX_HOPS) {
			fragment.incrementHops();

			// Note: we don't bother initiating NAT-t for fragments, since heads will set that off.
			// It wouldn't hurt anything, just redundant and unnecessary.
			SharedPtr<Peer> relayTo = RR->topology->getPeer(destination);
			if ((!relayTo)||(relayTo->send(RR,fragment.data(),fragment.size(),Utils::now()) == Path::PATH_TYPE_NULL)) {
				// Don't know peer or no direct path -- so relay via supernode
				relayTo = RR->topology->getBestSupernode();
				if (relayTo)
					relayTo->send(RR,fragment.data(),fragment.size(),Utils::now());
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

					SharedPtr<IncomingPacket> packet(dqe->second.frag0);
					for(unsigned int f=1;f<tf;++f)
						packet->append(dqe->second.frags[f - 1].payload(),dqe->second.frags[f - 1].payloadLength());
					_defragQueue.erase(dqe);

					if (!packet->tryDecode(RR)) {
						Mutex::Lock _l(_rxQueue_m);
						_rxQueue.push_back(packet);
					}
				}
			} // else this is a duplicate fragment, ignore
		}
	}
}

void Switch::_handleRemotePacketHead(const SharedPtr<Socket> &fromSock,const InetAddress &fromAddr,const Buffer<4096> &data)
{
	SharedPtr<IncomingPacket> packet(new IncomingPacket(data,fromSock,fromAddr));

	Address source(packet->source());
	Address destination(packet->destination());

	//TRACE("<< %.16llx %s -> %s (size: %u)",(unsigned long long)packet->packetId(),source.toString().c_str(),destination.toString().c_str(),packet->size());

	if (destination != RR->identity.address()) {
		// Packet is not for us, so try to relay it
		if (packet->hops() < ZT_RELAY_MAX_HOPS) {
			packet->incrementHops();

			SharedPtr<Peer> relayTo = RR->topology->getPeer(destination);
			Path::Type relayedVia;
			if ((relayTo)&&((relayedVia = relayTo->send(RR,packet->data(),packet->size(),Utils::now())) != Path::PATH_TYPE_NULL)) {
				/* If both paths are UDP, attempt to invoke UDP NAT-t between peers
				 * by sending VERB_RENDEZVOUS. Do not do this for TCP due to GitHub
				 * issue #63. */
				if ((fromSock->udp())&&(relayedVia == Path::PATH_TYPE_UDP))
					unite(source,destination,false);
			} else {
				// Don't know peer or no direct path -- so relay via supernode
				relayTo = RR->topology->getBestSupernode(&source,1,true);
				if (relayTo)
					relayTo->send(RR,packet->data(),packet->size(),Utils::now());
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

				if (!packet->tryDecode(RR)) {
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
		if (!packet->tryDecode(RR)) {
			Mutex::Lock _l(_rxQueue_m);
			_rxQueue.push_back(packet);
		}
	}
}

void Switch::_handleBeacon(const SharedPtr<Socket> &fromSock,const InetAddress &fromAddr,const Buffer<4096> &data)
{
	Address beaconAddr(data.field(ZT_PROTO_BEACON_IDX_ADDRESS,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
	if (beaconAddr == RR->identity.address())
		return;
	SharedPtr<Peer> peer(RR->topology->getPeer(beaconAddr));
	if (peer) {
		uint64_t now = Utils::now();
		if (peer->haveUdpPath(fromAddr)) {
			if ((now - peer->lastDirectReceive()) >= ZT_PEER_DIRECT_PING_DELAY)
				peer->sendPing(RR,now);
		} else {
			if ((now - _lastBeacon) < ZT_MIN_BEACON_RESPONSE_INTERVAL)
				return;
			_lastBeacon = now;
			sendHELLO(peer,fromAddr);
		}
	}
}

Address Switch::_sendWhoisRequest(const Address &addr,const Address *peersAlreadyConsulted,unsigned int numPeersAlreadyConsulted)
{
	SharedPtr<Peer> supernode(RR->topology->getBestSupernode(peersAlreadyConsulted,numPeersAlreadyConsulted,false));
	if (supernode) {
		Packet outp(supernode->address(),RR->identity.address(),Packet::VERB_WHOIS);
		addr.appendTo(outp);
		outp.armor(supernode->key(),true);
		uint64_t now = Utils::now();
		if (supernode->send(RR,outp.data(),outp.size(),now) != Path::PATH_TYPE_NULL)
			return supernode->address();
	}
	return Address();
}

bool Switch::_trySend(const Packet &packet,bool encrypt)
{
	SharedPtr<Peer> peer(RR->topology->getPeer(packet.destination()));

	if (peer) {
		uint64_t now = Utils::now();

		SharedPtr<Peer> via;
		if (peer->hasActiveDirectPath(now)) {
			via = peer;
		} else {
			via = RR->topology->getBestSupernode();
			if (!via)
				return false;
		}

		Packet tmp(packet);

		unsigned int chunkSize = std::min(tmp.size(),(unsigned int)ZT_UDP_DEFAULT_PAYLOAD_MTU);
		tmp.setFragmented(chunkSize < tmp.size());

		tmp.armor(peer->key(),encrypt);

		if (via->send(RR,tmp.data(),chunkSize,now) != Path::PATH_TYPE_NULL) {
			if (chunkSize < tmp.size()) {
				// Too big for one bite, fragment the rest
				unsigned int fragStart = chunkSize;
				unsigned int remaining = tmp.size() - chunkSize;
				unsigned int fragsRemaining = (remaining / (ZT_UDP_DEFAULT_PAYLOAD_MTU - ZT_PROTO_MIN_FRAGMENT_LENGTH));
				if ((fragsRemaining * (ZT_UDP_DEFAULT_PAYLOAD_MTU - ZT_PROTO_MIN_FRAGMENT_LENGTH)) < remaining)
					++fragsRemaining;
				unsigned int totalFragments = fragsRemaining + 1;

				for(unsigned int fno=1;fno<totalFragments;++fno) {
					chunkSize = std::min(remaining,(unsigned int)(ZT_UDP_DEFAULT_PAYLOAD_MTU - ZT_PROTO_MIN_FRAGMENT_LENGTH));
					Packet::Fragment frag(tmp,fragStart,chunkSize,fno,totalFragments);
					via->send(RR,frag.data(),frag.size(),now);
					fragStart += chunkSize;
					remaining -= chunkSize;
				}
			}
			return true;
		}
	} else {
		requestWhois(packet.destination());
	}
	return false;
}

} // namespace ZeroTier
