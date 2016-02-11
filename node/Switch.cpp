/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <utility>
#include <stdexcept>

#include "../version.h"
#include "../include/ZeroTierOne.h"

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "Switch.hpp"
#include "Node.hpp"
#include "InetAddress.hpp"
#include "Topology.hpp"
#include "Peer.hpp"
#include "SelfAwareness.hpp"
#include "Packet.hpp"
#include "Cluster.hpp"

namespace ZeroTier {

#ifdef ZT_TRACE
static const char *etherTypeName(const unsigned int etherType)
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
#endif // ZT_TRACE

Switch::Switch(const RuntimeEnvironment *renv) :
	RR(renv),
	_lastBeaconResponse(0),
	_outstandingWhoisRequests(32),
	_defragQueue(32),
	_lastUniteAttempt(8) // only really used on root servers and upstreams, and it'll grow there just fine
{
}

Switch::~Switch()
{
}

void Switch::onRemotePacket(const InetAddress &localAddr,const InetAddress &fromAddr,const void *data,unsigned int len)
{
	try {
		if (len == 13) {
			/* LEGACY: before VERB_PUSH_DIRECT_PATHS, peers used broadcast
			 * announcements on the LAN to solve the 'same network problem.' We
			 * no longer send these, but we'll listen for them for a while to
			 * locate peers with versions <1.0.4. */
			Address beaconAddr(reinterpret_cast<const char *>(data) + 8,5);
			if (beaconAddr == RR->identity.address())
				return;
			if (!RR->node->shouldUsePathForZeroTierTraffic(localAddr,fromAddr))
				return;
			SharedPtr<Peer> peer(RR->topology->getPeer(beaconAddr));
			if (peer) { // we'll only respond to beacons from known peers
				const uint64_t now = RR->node->now();
				if ((now - _lastBeaconResponse) >= 2500) { // limit rate of responses
					_lastBeaconResponse = now;
					Packet outp(peer->address(),RR->identity.address(),Packet::VERB_NOP);
					outp.armor(peer->key(),true);
					RR->node->putPacket(localAddr,fromAddr,outp.data(),outp.size());
				}
			}
		} else if (len > ZT_PROTO_MIN_FRAGMENT_LENGTH) {
			if (((const unsigned char *)data)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR) {
				_handleRemotePacketFragment(localAddr,fromAddr,data,len);
			} else if (len >= ZT_PROTO_MIN_PACKET_LENGTH) {
				_handleRemotePacketHead(localAddr,fromAddr,data,len);
			}
		}
	} catch (std::exception &ex) {
		TRACE("dropped packet from %s: unexpected exception: %s",fromAddr.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped packet from %s: unexpected exception: (unknown)",fromAddr.toString().c_str());
	}
}

void Switch::onLocalEthernet(const SharedPtr<Network> &network,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
{
	SharedPtr<NetworkConfig> nconf(network->config2());
	if (!nconf)
		return;

	// Sanity check -- bridge loop? OS problem?
	if (to == network->mac())
		return;

	// Check to make sure this protocol is allowed on this network
	if (!nconf->permitsEtherType(etherType)) {
		TRACE("%.16llx: ignored tap: %s -> %s: ethertype %s not allowed on network %.16llx",network->id(),from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType),(unsigned long long)network->id());
		return;
	}

	// Check if this packet is from someone other than the tap -- i.e. bridged in
	bool fromBridged = false;
	if (from != network->mac()) {
		if (!network->permitsBridging(RR->identity.address())) {
			TRACE("%.16llx: %s -> %s %s not forwarded, bridging disabled or this peer not a bridge",network->id(),from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType));
			return;
		}
		fromBridged = true;
	}

	if (to.isMulticast()) {
		// Destination is a multicast address (including broadcast)
		MulticastGroup mg(to,0);

		if (to.isBroadcast()) {
			if ( (etherType == ZT_ETHERTYPE_ARP) && (len >= 28) && ((((const uint8_t *)data)[2] == 0x08)&&(((const uint8_t *)data)[3] == 0x00)&&(((const uint8_t *)data)[4] == 6)&&(((const uint8_t *)data)[5] == 4)&&(((const uint8_t *)data)[7] == 0x01)) ) {
				/* IPv4 ARP is one of the few special cases that we impose upon what is
				 * otherwise a straightforward Ethernet switch emulation. Vanilla ARP
				 * is dumb old broadcast and simply doesn't scale. ZeroTier multicast
				 * groups have an additional field called ADI (additional distinguishing
			   * information) which was added specifically for ARP though it could
				 * be used for other things too. We then take ARP broadcasts and turn
				 * them into multicasts by stuffing the IP address being queried into
				 * the 32-bit ADI field. In practice this uses our multicast pub/sub
				 * system to implement a kind of extended/distributed ARP table. */
				mg = MulticastGroup::deriveMulticastGroupForAddressResolution(InetAddress(((const unsigned char *)data) + 24,4,0));
			} else if (!nconf->enableBroadcast()) {
				// Don't transmit broadcasts if this network doesn't want them
				TRACE("%.16llx: dropped broadcast since ff:ff:ff:ff:ff:ff is not enabled",network->id());
				return;
			}
		} else if ((etherType == ZT_ETHERTYPE_IPV6)&&(len >= (40 + 8 + 16))) {
			/* IPv6 NDP emulation on ZeroTier-RFC4193 addressed networks! This allows
			 * for multicast-free operation in IPv6 networks, which both improves
			 * performance and is friendlier to mobile and (especially) IoT devices.
			 * In the future there may be a no-multicast build option for embedded
			 * and IoT use and this will be the preferred addressing mode. Note that
			 * it plays nice with our L2 emulation philosophy and even with bridging.
			 * While "real" devices behind the bridge can't have ZT-RFC4193 addresses
			 * themselves, they can look these addresses up with NDP and it will
			 * work just fine. */
			if ((reinterpret_cast<const uint8_t *>(data)[6] == 0x3a)&&(reinterpret_cast<const uint8_t *>(data)[40] == 0x87)) { // ICMPv6 neighbor solicitation
				for(std::vector<InetAddress>::const_iterator sip(nconf->staticIps().begin()),sipend(nconf->staticIps().end());sip!=sipend;++sip) {
					if ((sip->ss_family == AF_INET6)&&(Utils::ntoh((uint16_t)reinterpret_cast<const struct sockaddr_in6 *>(&(*sip))->sin6_port) == 88)) {
						const uint8_t *my6 = reinterpret_cast<const uint8_t *>(reinterpret_cast<const struct sockaddr_in6 *>(&(*sip))->sin6_addr.s6_addr);
						if ((my6[0] == 0xfd)&&(my6[9] == 0x99)&&(my6[10] == 0x93)) { // ZT-RFC4193 == fd__:____:____:____:__99:93__:____:____ / 88
							const uint8_t *pkt6 = reinterpret_cast<const uint8_t *>(data) + 40 + 8;
							unsigned int ptr = 0;
							while (ptr != 11) {
								if (pkt6[ptr] != my6[ptr])
									break;
								++ptr;
							}
							if (ptr == 11) { // /88 matches an assigned address on this network
								const Address atPeer(pkt6 + ptr,5);
								if (atPeer != RR->identity.address()) {
									const MAC atPeerMac(atPeer,network->id());
									TRACE("ZT-RFC4193 NDP emulation: %.16llx: forging response for %s/%s",network->id(),atPeer.toString().c_str(),atPeerMac.toString().c_str());

									uint8_t adv[72];
									adv[0] = 0x60; adv[1] = 0x00; adv[2] = 0x00; adv[3] = 0x00;
									adv[4] = 0x00; adv[5] = 0x20;
									adv[6] = 0x3a; adv[7] = 0xff;
									for(int i=0;i<16;++i) adv[8 + i] = pkt6[i];
									for(int i=0;i<16;++i) adv[24 + i] = my6[i];
									adv[40] = 0x88; adv[41] = 0x00;
									adv[42] = 0x00; adv[43] = 0x00; // future home of checksum
									adv[44] = 0x60; adv[45] = 0x00; adv[46] = 0x00; adv[47] = 0x00;
									for(int i=0;i<16;++i) adv[48 + i] = pkt6[i];
									adv[64] = 0x02; adv[65] = 0x01;
									adv[66] = atPeerMac[0]; adv[67] = atPeerMac[1]; adv[68] = atPeerMac[2]; adv[69] = atPeerMac[3]; adv[70] = atPeerMac[4]; adv[71] = atPeerMac[5];

									uint16_t pseudo_[36];
									uint8_t *const pseudo = reinterpret_cast<uint8_t *>(pseudo_);
									for(int i=0;i<32;++i) pseudo[i] = adv[8 + i];
									pseudo[32] = 0x00; pseudo[33] = 0x00; pseudo[34] = 0x00; pseudo[35] = 0x20;
									pseudo[36] = 0x00; pseudo[37] = 0x00; pseudo[38] = 0x00; pseudo[39] = 0x3a;
									for(int i=0;i<32;++i) pseudo[40 + i] = adv[40 + i];
									uint32_t checksum = 0;
									for(int i=0;i<36;++i) checksum += Utils::hton(pseudo_[i]);
									while ((checksum >> 16)) checksum = (checksum & 0xffff) + (checksum >> 16);
									checksum = ~checksum;
									adv[42] = (checksum >> 8) & 0xff;
									adv[43] = checksum & 0xff;

									RR->node->putFrame(network->id(),network->userPtr(),atPeerMac,from,ZT_ETHERTYPE_IPV6,0,adv,72);
									return; // stop processing: we have handled this frame with a spoofed local reply so no need to send it anywhere
								}
							}
						}
					}
				}
			}
		}

		/* Learn multicast groups for bridged-in hosts.
		 * Note that some OSes, most notably Linux, do this for you by learning
		 * multicast addresses on bridge interfaces and subscribing each slave.
		 * But in that case this does no harm, as the sets are just merged. */
		if (fromBridged)
			network->learnBridgedMulticastGroup(mg,RR->node->now());

		//TRACE("%.16llx: MULTICAST %s -> %s %s %u",network->id(),from.toString().c_str(),mg.toString().c_str(),etherTypeName(etherType),len);

		RR->mc->send(
			((!nconf->isPublic())&&(nconf->com())) ? &(nconf->com()) : (const CertificateOfMembership *)0,
			nconf->multicastLimit(),
			RR->node->now(),
			network->id(),
			nconf->activeBridges(),
			mg,
			(fromBridged) ? from : MAC(),
			etherType,
			data,
			len);

		return;
	}

	if (to[0] == MAC::firstOctetForNetwork(network->id())) {
		// Destination is another ZeroTier peer on the same network

		Address toZT(to.toAddress(network->id())); // since in-network MACs are derived from addresses and network IDs, we can reverse this
		SharedPtr<Peer> toPeer(RR->topology->getPeer(toZT));
		const bool includeCom = ( (nconf->isPrivate()) && (nconf->com()) && ((!toPeer)||(toPeer->needsOurNetworkMembershipCertificate(network->id(),RR->node->now(),true))) );
		if ((fromBridged)||(includeCom)) {
			Packet outp(toZT,RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(network->id());
			if (includeCom) {
				outp.append((unsigned char)0x01); // 0x01 -- COM included
				nconf->com().serialize(outp);
			} else {
				outp.append((unsigned char)0x00);
			}
			to.appendTo(outp);
			from.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(data,len);
			outp.compress();
			send(outp,true,network->id());
		} else {
			Packet outp(toZT,RR->identity.address(),Packet::VERB_FRAME);
			outp.append(network->id());
			outp.append((uint16_t)etherType);
			outp.append(data,len);
			outp.compress();
			send(outp,true,network->id());
		}

		//TRACE("%.16llx: UNICAST: %s -> %s etherType==%s(%.4x) vlanId==%u len==%u fromBridged==%d includeCom==%d",network->id(),from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType),etherType,vlanId,len,(int)fromBridged,(int)includeCom);

		return;
	}

	{
		// Destination is bridged behind a remote peer

		Address bridges[ZT_MAX_BRIDGE_SPAM];
		unsigned int numBridges = 0;

		/* Create an array of up to ZT_MAX_BRIDGE_SPAM recipients for this bridged frame. */
		bridges[0] = network->findBridgeTo(to);
		if ((bridges[0])&&(bridges[0] != RR->identity.address())&&(network->permitsBridging(bridges[0]))) {
			/* We have a known bridge route for this MAC, send it there. */
			++numBridges;
		} else if (!nconf->activeBridges().empty()) {
			/* If there is no known route, spam to up to ZT_MAX_BRIDGE_SPAM active
			 * bridges. If someone responds, we'll learn the route. */
			std::vector<Address>::const_iterator ab(nconf->activeBridges().begin());
			if (nconf->activeBridges().size() <= ZT_MAX_BRIDGE_SPAM) {
				// If there are <= ZT_MAX_BRIDGE_SPAM active bridges, spam them all
				while (ab != nconf->activeBridges().end()) {
					bridges[numBridges++] = *ab;
					++ab;
				}
			} else {
				// Otherwise pick a random set of them
				while (numBridges < ZT_MAX_BRIDGE_SPAM) {
					if (ab == nconf->activeBridges().end())
						ab = nconf->activeBridges().begin();
					if (((unsigned long)RR->node->prng() % (unsigned long)nconf->activeBridges().size()) == 0) {
						bridges[numBridges++] = *ab;
						++ab;
					} else ++ab;
				}
			}
		}

		for(unsigned int b=0;b<numBridges;++b) {
			SharedPtr<Peer> bridgePeer(RR->topology->getPeer(bridges[b]));
			Packet outp(bridges[b],RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(network->id());
			if ( (nconf->isPrivate()) && (nconf->com()) && ((!bridgePeer)||(bridgePeer->needsOurNetworkMembershipCertificate(network->id(),RR->node->now(),true))) ) {
				outp.append((unsigned char)0x01); // 0x01 -- COM included
				nconf->com().serialize(outp);
			} else {
				outp.append((unsigned char)0);
			}
			to.appendTo(outp);
			from.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(data,len);
			outp.compress();
			send(outp,true,network->id());
		}
	}
}

void Switch::send(const Packet &packet,bool encrypt,uint64_t nwid)
{
	if (packet.destination() == RR->identity.address()) {
		TRACE("BUG: caught attempt to send() to self, ignored");
		return;
	}

	//TRACE(">> %s to %s (%u bytes, encrypt==%d, nwid==%.16llx)",Packet::verbString(packet.verb()),packet.destination().toString().c_str(),packet.size(),(int)encrypt,nwid);

	if (!_trySend(packet,encrypt,nwid)) {
		Mutex::Lock _l(_txQueue_m);
		_txQueue.push_back(TXQueueEntry(packet.destination(),RR->node->now(),packet,encrypt,nwid));
	}
}

bool Switch::unite(const Address &p1,const Address &p2)
{
	if ((p1 == RR->identity.address())||(p2 == RR->identity.address()))
		return false;
	SharedPtr<Peer> p1p = RR->topology->getPeer(p1);
	if (!p1p)
		return false;
	SharedPtr<Peer> p2p = RR->topology->getPeer(p2);
	if (!p2p)
		return false;

	const uint64_t now = RR->node->now();

	std::pair<InetAddress,InetAddress> cg(Peer::findCommonGround(*p1p,*p2p,now));
	if ((!(cg.first))||(cg.first.ipScope() != cg.second.ipScope()))
		return false;

	TRACE("unite: %s(%s) <> %s(%s)",p1.toString().c_str(),cg.second.toString().c_str(),p2.toString().c_str(),cg.first.toString().c_str());

	/* Tell P1 where to find P2 and vice versa, sending the packets to P1 and
	 * P2 in randomized order in terms of which gets sent first. This is done
	 * since in a few cases NAT-t can be sensitive to slight timing differences
	 * in terms of when the two peers initiate. Normally this is accounted for
	 * by the nearly-simultaneous RENDEZVOUS kickoff from the relay, but
	 * given that relay are hosted on cloud providers this can in some
	 * cases have a few ms of latency between packet departures. By randomizing
	 * the order we make each attempted NAT-t favor one or the other going
	 * first, meaning if it doesn't succeed the first time it might the second
	 * and so forth. */
	unsigned int alt = (unsigned int)RR->node->prng() & 1;
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
			p1p->send(outp.data(),outp.size(),now);
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
			p2p->send(outp.data(),outp.size(),now);
		}
		++alt; // counts up and also flips LSB
	}

	return true;
}

void Switch::rendezvous(const SharedPtr<Peer> &peer,const InetAddress &localAddr,const InetAddress &atAddr)
{
	TRACE("sending NAT-t message to %s(%s)",peer->address().toString().c_str(),atAddr.toString().c_str());
	const uint64_t now = RR->node->now();
	peer->sendHELLO(localAddr,atAddr,now,2); // first attempt: send low-TTL packet to 'open' local NAT
	{
		Mutex::Lock _l(_contactQueue_m);
		_contactQueue.push_back(ContactQueueEntry(peer,now + ZT_NAT_T_TACTICAL_ESCALATION_DELAY,localAddr,atAddr));
	}
}

void Switch::requestWhois(const Address &addr)
{
	bool inserted = false;
	{
		Mutex::Lock _l(_outstandingWhoisRequests_m);
		WhoisRequest &r = _outstandingWhoisRequests[addr];
		if (r.lastSent) {
			r.retries = 0; // reset retry count if entry already existed, but keep waiting and retry again after normal timeout
		} else {
			r.lastSent = RR->node->now();
			inserted = true;
		}
	}
	if (inserted)
		_sendWhoisRequest(addr,(const Address *)0,0);
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
			if ((*rxi)->tryDecode(RR,false))
				_rxQueue.erase(rxi++);
			else ++rxi;
		}
	}

	{	// finish sending any packets waiting on peer's public key / identity
		Mutex::Lock _l(_txQueue_m);
		for(std::list< TXQueueEntry >::iterator txi(_txQueue.begin());txi!=_txQueue.end();) {
			if (txi->dest == peer->address()) {
				if (_trySend(txi->packet,txi->encrypt,txi->nwid))
					_txQueue.erase(txi++);
				else ++txi;
			} else ++txi;
		}
	}
}

unsigned long Switch::doTimerTasks(uint64_t now)
{
	unsigned long nextDelay = 0xffffffff; // ceiling delay, caller will cap to minimum

	{	// Iterate through NAT traversal strategies for entries in contact queue
		Mutex::Lock _l(_contactQueue_m);
		for(std::list<ContactQueueEntry>::iterator qi(_contactQueue.begin());qi!=_contactQueue.end();) {
			if (now >= qi->fireAtTime) {
				if (!qi->peer->pushDirectPaths(qi->localAddr,qi->inaddr,now,true))
					qi->peer->sendHELLO(qi->localAddr,qi->inaddr,now);
				_contactQueue.erase(qi++);
				continue;
				/* Old symmetric NAT buster code, obsoleted by port prediction alg in SelfAwareness but left around for now in case we revert
				if (qi->strategyIteration == 0) {
					// First strategy: send packet directly to destination
					qi->peer->sendHELLO(qi->localAddr,qi->inaddr,now);
				} else if (qi->strategyIteration <= 3) {
					// Strategies 1-3: try escalating ports for symmetric NATs that remap sequentially
					InetAddress tmpaddr(qi->inaddr);
					int p = (int)qi->inaddr.port() + qi->strategyIteration;
					if (p > 65535)
						p -= 64511;
					tmpaddr.setPort((unsigned int)p);
					qi->peer->sendHELLO(qi->localAddr,tmpaddr,now);
				} else {
					// All strategies tried, expire entry
					_contactQueue.erase(qi++);
					continue;
				}
				++qi->strategyIteration;
				qi->fireAtTime = now + ZT_NAT_T_TACTICAL_ESCALATION_DELAY;
				nextDelay = std::min(nextDelay,(unsigned long)ZT_NAT_T_TACTICAL_ESCALATION_DELAY);
				*/
			} else {
				nextDelay = std::min(nextDelay,(unsigned long)(qi->fireAtTime - now));
			}
			++qi; // if qi was erased, loop will have continued before here
		}
	}

	{	// Retry outstanding WHOIS requests
		Mutex::Lock _l(_outstandingWhoisRequests_m);
		Hashtable< Address,WhoisRequest >::Iterator i(_outstandingWhoisRequests);
		Address *a = (Address *)0;
		WhoisRequest *r = (WhoisRequest *)0;
		while (i.next(a,r)) {
			const unsigned long since = (unsigned long)(now - r->lastSent);
			if (since >= ZT_WHOIS_RETRY_DELAY) {
				if (r->retries >= ZT_MAX_WHOIS_RETRIES) {
					TRACE("WHOIS %s timed out",a->toString().c_str());
					_outstandingWhoisRequests.erase(*a);
				} else {
					r->lastSent = now;
					r->peersConsulted[r->retries] = _sendWhoisRequest(*a,r->peersConsulted,r->retries);
					++r->retries;
					TRACE("WHOIS %s (retry %u)",a->toString().c_str(),r->retries);
					nextDelay = std::min(nextDelay,(unsigned long)ZT_WHOIS_RETRY_DELAY);
				}
			} else {
				nextDelay = std::min(nextDelay,ZT_WHOIS_RETRY_DELAY - since);
			}
		}
	}

	{	// Time out TX queue packets that never got WHOIS lookups or other info.
		Mutex::Lock _l(_txQueue_m);
		for(std::list< TXQueueEntry >::iterator txi(_txQueue.begin());txi!=_txQueue.end();) {
			if (_trySend(txi->packet,txi->encrypt,txi->nwid))
				_txQueue.erase(txi++);
			else if ((now - txi->creationTime) > ZT_TRANSMIT_QUEUE_TIMEOUT) {
				TRACE("TX %s -> %s timed out",txi->packet.source().toString().c_str(),txi->packet.destination().toString().c_str());
				_txQueue.erase(txi++);
			} else ++txi;
		}
	}

	{	// Time out RX queue packets that never got WHOIS lookups or other info.
		Mutex::Lock _l(_rxQueue_m);
		for(std::list< SharedPtr<IncomingPacket> >::iterator i(_rxQueue.begin());i!=_rxQueue.end();) {
			if ((now - (*i)->receiveTime()) > ZT_RECEIVE_QUEUE_TIMEOUT) {
				TRACE("RX %s -> %s timed out",(*i)->source().toString().c_str(),(*i)->destination().toString().c_str());
				_rxQueue.erase(i++);
			} else ++i;
		}
	}

	{	// Time out packets that didn't get all their fragments.
		Mutex::Lock _l(_defragQueue_m);
		Hashtable< uint64_t,DefragQueueEntry >::Iterator i(_defragQueue);
		uint64_t *packetId = (uint64_t *)0;
		DefragQueueEntry *qe = (DefragQueueEntry *)0;
		while (i.next(packetId,qe)) {
			if ((now - qe->creationTime) > ZT_FRAGMENTED_PACKET_RECEIVE_TIMEOUT) {
				TRACE("incomplete fragmented packet %.16llx timed out, fragments discarded",*packetId);
				_defragQueue.erase(*packetId);
			}
		}
	}

	{	// Remove really old last unite attempt entries to keep table size controlled
		Mutex::Lock _l(_lastUniteAttempt_m);
		Hashtable< _LastUniteKey,uint64_t >::Iterator i(_lastUniteAttempt);
		_LastUniteKey *k = (_LastUniteKey *)0;
		uint64_t *v = (uint64_t *)0;
		while (i.next(k,v)) {
			if ((now - *v) >= (ZT_MIN_UNITE_INTERVAL * 8))
				_lastUniteAttempt.erase(*k);
		}
	}

	return nextDelay;
}

void Switch::_handleRemotePacketFragment(const InetAddress &localAddr,const InetAddress &fromAddr,const void *data,unsigned int len)
{
	Packet::Fragment fragment(data,len);
	Address destination(fragment.destination());

	if (destination != RR->identity.address()) {
		// Fragment is not for us, so try to relay it
		if (fragment.hops() < ZT_RELAY_MAX_HOPS) {
			fragment.incrementHops();

			// Note: we don't bother initiating NAT-t for fragments, since heads will set that off.
			// It wouldn't hurt anything, just redundant and unnecessary.
			SharedPtr<Peer> relayTo = RR->topology->getPeer(destination);
			if ((!relayTo)||(!relayTo->send(fragment.data(),fragment.size(),RR->node->now()))) {
#ifdef ZT_ENABLE_CLUSTER
				if (RR->cluster) {
					RR->cluster->sendViaCluster(Address(),destination,fragment.data(),fragment.size(),false);
					return;
				}
#endif

				// Don't know peer or no direct path -- so relay via root server
				relayTo = RR->topology->getBestRoot();
				if (relayTo)
					relayTo->send(fragment.data(),fragment.size(),RR->node->now());
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
			DefragQueueEntry &dq = _defragQueue[pid];

			if (!dq.creationTime) {
				// We received a Packet::Fragment without its head, so queue it and wait

				dq.creationTime = RR->node->now();
				dq.frags[fno - 1] = fragment;
				dq.totalFragments = tf; // total fragment count is known
				dq.haveFragments = 1 << fno; // we have only this fragment
				//TRACE("fragment (%u/%u) of %.16llx from %s",fno + 1,tf,pid,fromAddr.toString().c_str());
			} else if (!(dq.haveFragments & (1 << fno))) {
				// We have other fragments and maybe the head, so add this one and check

				dq.frags[fno - 1] = fragment;
				dq.totalFragments = tf;
				//TRACE("fragment (%u/%u) of %.16llx from %s",fno + 1,tf,pid,fromAddr.toString().c_str());

				if (Utils::countBits(dq.haveFragments |= (1 << fno)) == tf) {
					// We have all fragments -- assemble and process full Packet
					//TRACE("packet %.16llx is complete, assembling and processing...",pid);

					SharedPtr<IncomingPacket> packet(dq.frag0);
					for(unsigned int f=1;f<tf;++f)
						packet->append(dq.frags[f - 1].payload(),dq.frags[f - 1].payloadLength());
					_defragQueue.erase(pid); // dq no longer valid after this

					if (!packet->tryDecode(RR,false)) {
						Mutex::Lock _l(_rxQueue_m);
						_rxQueue.push_back(packet);
					}
				}
			} // else this is a duplicate fragment, ignore
		}
	}
}

void Switch::_handleRemotePacketHead(const InetAddress &localAddr,const InetAddress &fromAddr,const void *data,unsigned int len)
{
	const uint64_t now = RR->node->now();
	SharedPtr<IncomingPacket> packet(new IncomingPacket(data,len,localAddr,fromAddr,now));

	Address source(packet->source());
	Address destination(packet->destination());

	// Catch this and toss it -- it would never work, but it could happen if we somehow
	// mistakenly guessed an address we're bound to as a destination for another peer.
	if (source == RR->identity.address())
		return;

	//TRACE("<< %.16llx %s -> %s (size: %u)",(unsigned long long)packet->packetId(),source.toString().c_str(),destination.toString().c_str(),packet->size());

	if (destination != RR->identity.address()) {
		// Packet is not for us, so try to relay it
		if (packet->hops() < ZT_RELAY_MAX_HOPS) {
			packet->incrementHops();

			SharedPtr<Peer> relayTo = RR->topology->getPeer(destination);
			if ((relayTo)&&((relayTo->send(packet->data(),packet->size(),now)))) {
				Mutex::Lock _l(_lastUniteAttempt_m);
				uint64_t &luts = _lastUniteAttempt[_LastUniteKey(source,destination)];
				if ((now - luts) >= ZT_MIN_UNITE_INTERVAL) {
					luts = now;
					unite(source,destination);
				}
			} else {
#ifdef ZT_ENABLE_CLUSTER
				if (RR->cluster) {
					bool shouldUnite;
					{
						Mutex::Lock _l(_lastUniteAttempt_m);
						uint64_t &luts = _lastUniteAttempt[_LastUniteKey(source,destination)];
						shouldUnite = ((now - luts) >= ZT_MIN_UNITE_INTERVAL);
						if (shouldUnite)
							luts = now;
					}
					RR->cluster->sendViaCluster(source,destination,packet->data(),packet->size(),shouldUnite);
					return;
				}
#endif

				relayTo = RR->topology->getBestRoot(&source,1,true);
				if (relayTo)
					relayTo->send(packet->data(),packet->size(),now);
			}
		} else {
			TRACE("dropped relay %s(%s) -> %s, max hops exceeded",packet->source().toString().c_str(),fromAddr.toString().c_str(),destination.toString().c_str());
		}
	} else if (packet->fragmented()) {
		// Packet is the head of a fragmented packet series

		uint64_t pid = packet->packetId();
		Mutex::Lock _l(_defragQueue_m);
		DefragQueueEntry &dq = _defragQueue[pid];

		if (!dq.creationTime) {
			// If we have no other fragments yet, create an entry and save the head

			dq.creationTime = now;
			dq.frag0 = packet;
			dq.totalFragments = 0; // 0 == unknown, waiting for Packet::Fragment
			dq.haveFragments = 1; // head is first bit (left to right)
			//TRACE("fragment (0/?) of %.16llx from %s",pid,fromAddr.toString().c_str());
		} else if (!(dq.haveFragments & 1)) {
			// If we have other fragments but no head, see if we are complete with the head

			if ((dq.totalFragments)&&(Utils::countBits(dq.haveFragments |= 1) == dq.totalFragments)) {
				// We have all fragments -- assemble and process full Packet

				//TRACE("packet %.16llx is complete, assembling and processing...",pid);
				// packet already contains head, so append fragments
				for(unsigned int f=1;f<dq.totalFragments;++f)
					packet->append(dq.frags[f - 1].payload(),dq.frags[f - 1].payloadLength());
				_defragQueue.erase(pid); // dq no longer valid after this

				if (!packet->tryDecode(RR,false)) {
					Mutex::Lock _l(_rxQueue_m);
					_rxQueue.push_back(packet);
				}
			} else {
				// Still waiting on more fragments, so queue the head
				dq.frag0 = packet;
			}
		} // else this is a duplicate head, ignore
	} else {
		// Packet is unfragmented, so just process it
		if (!packet->tryDecode(RR,false)) {
			Mutex::Lock _l(_rxQueue_m);
			_rxQueue.push_back(packet);
		}
	}
}

Address Switch::_sendWhoisRequest(const Address &addr,const Address *peersAlreadyConsulted,unsigned int numPeersAlreadyConsulted)
{
	SharedPtr<Peer> root(RR->topology->getBestRoot(peersAlreadyConsulted,numPeersAlreadyConsulted,false));
	if (root) {
		Packet outp(root->address(),RR->identity.address(),Packet::VERB_WHOIS);
		addr.appendTo(outp);
		outp.armor(root->key(),true);
		if (root->send(outp.data(),outp.size(),RR->node->now()))
			return root->address();
	}
	return Address();
}

bool Switch::_trySend(const Packet &packet,bool encrypt,uint64_t nwid)
{
	SharedPtr<Peer> peer(RR->topology->getPeer(packet.destination()));

	if (peer) {
		const uint64_t now = RR->node->now();

		SharedPtr<Network> network;
		SharedPtr<NetworkConfig> nconf;
		if (nwid) {
			network = RR->node->network(nwid);
			if (!network)
				return false; // we probably just left this network, let its packets die
			nconf = network->config2();
			if (!nconf)
				return false; // sanity check: unconfigured network? why are we trying to talk to it?
		}

		Path *viaPath = peer->getBestPath(now);
		SharedPtr<Peer> relay;
		if (!viaPath) {
			// See if this network has a preferred relay (if packet has an associated network)
			if (nconf) {
				unsigned int bestq = ~((unsigned int)0);
				for(std::vector< std::pair<Address,InetAddress> >::const_iterator r(nconf->relays().begin());r!=nconf->relays().end();++r) {
					if (r->first != peer->address()) {
						SharedPtr<Peer> rp(RR->topology->getPeer(r->first));
						if (rp) {
							const unsigned int q = rp->relayQuality(now);
							if (q < bestq) { // SUBTILE: < == don't use these if they are nil quality (unsigned int max), instead use a root
								bestq = q;
								rp.swap(relay);
							}
						}
					}
				}
			}

			// Otherwise relay off a root server
			if (!relay)
				relay = RR->topology->getBestRoot();

			if (!(relay)||(!(viaPath = relay->getBestPath(now))))
				return false; // no paths, no root servers?, no relays? :P~~~
		}

		if ((network)&&(relay)&&(network->isAllowed(peer))) {
			// Push hints for direct connectivity to this peer if we are relaying
			peer->pushDirectPaths(viaPath->localAddress(),viaPath->address(),now,false);
			viaPath->sent(now);
		}

		Packet tmp(packet);

		unsigned int chunkSize = std::min(tmp.size(),(unsigned int)ZT_UDP_DEFAULT_PAYLOAD_MTU);
		tmp.setFragmented(chunkSize < tmp.size());

		tmp.armor(peer->key(),encrypt);

		if (viaPath->send(RR,tmp.data(),chunkSize,now)) {
			if (chunkSize < tmp.size()) {
				// Too big for one packet, fragment the rest
				unsigned int fragStart = chunkSize;
				unsigned int remaining = tmp.size() - chunkSize;
				unsigned int fragsRemaining = (remaining / (ZT_UDP_DEFAULT_PAYLOAD_MTU - ZT_PROTO_MIN_FRAGMENT_LENGTH));
				if ((fragsRemaining * (ZT_UDP_DEFAULT_PAYLOAD_MTU - ZT_PROTO_MIN_FRAGMENT_LENGTH)) < remaining)
					++fragsRemaining;
				unsigned int totalFragments = fragsRemaining + 1;

				for(unsigned int fno=1;fno<totalFragments;++fno) {
					chunkSize = std::min(remaining,(unsigned int)(ZT_UDP_DEFAULT_PAYLOAD_MTU - ZT_PROTO_MIN_FRAGMENT_LENGTH));
					Packet::Fragment frag(tmp,fragStart,chunkSize,fno,totalFragments);
					viaPath->send(RR,frag.data(),frag.size(),now);
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
