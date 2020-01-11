/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <utility>
#include <stdexcept>

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
#include "Trace.hpp"

namespace ZeroTier {

Switch::Switch(const RuntimeEnvironment *renv) :
	RR(renv),
	_lastCheckedQueues(0)
{
}

void Switch::onRemotePacket(void *tPtr,const int64_t localSocket,const InetAddress &fromAddr,const void *data,unsigned int len)
{
	try {
		const int64_t now = RR->node->now();

		const SharedPtr<Path> path(RR->topology->getPath(localSocket,fromAddr));
		path->received(now);

		if (len > ZT_PROTO_MIN_FRAGMENT_LENGTH) { // SECURITY: min length check is important since we do some C-style stuff below!
			if (reinterpret_cast<const uint8_t *>(data)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR) {
				// Handle fragment ----------------------------------------------------

				Packet::Fragment fragment(data,len);
				const Address destination(fragment.destination());

				if (destination != RR->identity.address()) {
					if (fragment.hops() < ZT_RELAY_MAX_HOPS) {
						fragment.incrementHops();
						SharedPtr<Peer> relayTo = RR->topology->get(destination);
						if ((!relayTo)||(!relayTo->sendDirect(tPtr,fragment.data(),fragment.size(),now))) {
							relayTo = RR->topology->findRelayTo(now,destination);
							if (relayTo)
								relayTo->sendDirect(tPtr,fragment.data(),fragment.size(),now);
						}
					}
				} else {
					// Fragment looks like ours
					const uint64_t fragmentPacketId = fragment.packetId();
					const unsigned int fragmentNumber = fragment.fragmentNumber();
					const unsigned int totalFragments = fragment.totalFragments();

					if ((totalFragments <= ZT_MAX_PACKET_FRAGMENTS)&&(fragmentNumber < ZT_MAX_PACKET_FRAGMENTS)&&(fragmentNumber > 0)&&(totalFragments > 1)) {
						// Fragment appears basically sane. Its fragment number must be
						// 1 or more, since a Packet with fragmented bit set is fragment 0.
						// Total fragments must be more than 1, otherwise why are we
						// seeing a Packet::Fragment?

						RXQueueEntry *const rq = _findRXQueueEntry(fragmentPacketId);
						Mutex::Lock rql(rq->lock);
						if (rq->packetId != fragmentPacketId) {
							// No packet found, so we received a fragment without its head.

							rq->timestamp = now;
							rq->packetId = fragmentPacketId;
							rq->frags[fragmentNumber - 1] = fragment;
							rq->totalFragments = totalFragments; // total fragment count is known
							rq->haveFragments = 1 << fragmentNumber; // we have only this fragment
							rq->complete = false;
						} else if (!(rq->haveFragments & (1 << fragmentNumber))) {
							// We have other fragments and maybe the head, so add this one and check

							rq->frags[fragmentNumber - 1] = fragment;
							rq->totalFragments = totalFragments;

							if (Utils::countBits(rq->haveFragments |= (1 << fragmentNumber)) == totalFragments) {
								// We have all fragments -- assemble and process full Packet

								for(unsigned int f=1;f<totalFragments;++f)
									rq->frag0.append(rq->frags[f - 1].payload(),rq->frags[f - 1].payloadLength());

								if (rq->frag0.tryDecode(RR,tPtr)) {
									rq->timestamp = 0; // packet decoded, free entry
								} else {
									rq->complete = true; // set complete flag but leave entry since it probably needs WHOIS or something
								}
							}
						} // else this is a duplicate fragment, ignore
					}
				}

				// --------------------------------------------------------------------
			} else if (len >= ZT_PROTO_MIN_PACKET_LENGTH) { // min length check is important!
				// Handle packet head -------------------------------------------------

				const Address destination(reinterpret_cast<const uint8_t *>(data) + 8,ZT_ADDRESS_LENGTH);
				const Address source(reinterpret_cast<const uint8_t *>(data) + 13,ZT_ADDRESS_LENGTH);

				if (source == RR->identity.address())
					return;

				if (destination != RR->identity.address()) {
					// This packet is not for this node, so possibly relay it ----------

					Packet packet(data,len);
					if (packet.hops() < ZT_RELAY_MAX_HOPS) {
						packet.incrementHops();
						SharedPtr<Peer> relayTo = RR->topology->get(destination);
						if ((!relayTo)||(!relayTo->sendDirect(tPtr,packet.data(),packet.size(),now))) {
							relayTo = RR->topology->findRelayTo(now,destination);
							if ((relayTo)&&(relayTo->address() != source))
								relayTo->sendDirect(tPtr,packet.data(),packet.size(),now);
						}
					}

				} else if ((reinterpret_cast<const uint8_t *>(data)[ZT_PACKET_IDX_FLAGS] & ZT_PROTO_FLAG_FRAGMENTED) != 0) {
					// Packet is the head of a fragmented packet series ----------------

					const uint64_t packetId = (
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[0]) << 56U) |
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[1]) << 48U) |
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[2]) << 40U) |
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[3]) << 32U) |
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[4]) << 24U) |
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[5]) << 16U) |
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[6]) << 8U) |
						((uint64_t)reinterpret_cast<const uint8_t *>(data)[7])
					);

					RXQueueEntry *const rq = _findRXQueueEntry(packetId);
					Mutex::Lock rql(rq->lock);
					if (rq->packetId != packetId) {
						// If we have no other fragments yet, create an entry and save the head

						rq->timestamp = now;
						rq->packetId = packetId;
						rq->frag0.init(data,len,path,now);
						rq->totalFragments = 0;
						rq->haveFragments = 1;
						rq->complete = false;
					} else if (!(rq->haveFragments & 1)) {
						// Check if packet is complete -----------------------------------

						if ((rq->totalFragments > 1)&&(Utils::countBits(rq->haveFragments |= 1) == rq->totalFragments)) {
							// We have all fragments -- assemble and process full Packet ---

							rq->frag0.init(data,len,path,now);
							for(unsigned int f=1;f<rq->totalFragments;++f)
								rq->frag0.append(rq->frags[f - 1].payload(),rq->frags[f - 1].payloadLength());

							if (rq->frag0.tryDecode(RR,tPtr)) {
								rq->timestamp = 0; // packet decoded, free entry
							} else {
								rq->complete = true; // set complete flag but leave entry since it probably needs WHOIS or something
							}

						} else {
							// Still waiting on more fragments, but keep the head ----------

							rq->frag0.init(data,len,path,now);

						}
					} // else this is a duplicate head, ignore
				} else {

					// Packet is unfragmented, so just process it ----------------------
					IncomingPacket packet(data,len,path,now);
					if (!packet.tryDecode(RR,tPtr)) {
						RXQueueEntry *const rq = _nextRXQueueEntry();
						Mutex::Lock rql(rq->lock);
						rq->timestamp = now;
						rq->packetId = packet.packetId();
						rq->frag0 = packet;
						rq->totalFragments = 1;
						rq->haveFragments = 1;
						rq->complete = true;
					}

				}

				// --------------------------------------------------------------------
			}
		}
	} catch ( ... ) {} // sanity check, should be caught elsewhere
}

void Switch::onLocalEthernet(void *tPtr,const SharedPtr<Network> &network,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
{
	if (!network->hasConfig())
		return;

	// Check if this packet is from someone other than the tap -- i.e. bridged in
	bool fromBridged;
	if ((fromBridged = (from != network->mac()))) {
		if (!network->config().permitsBridging(RR->identity.address())) {
			RR->t->outgoingNetworkFrameDropped(tPtr,network,from,to,etherType,vlanId,len,"not a bridge");
			return;
		}
	}

	uint8_t qosBucket = 0;

	if (to.isMulticast()) {
		MulticastGroup multicastGroup(to,0);

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
				multicastGroup = MulticastGroup::deriveMulticastGroupForAddressResolution(InetAddress(((const unsigned char *)data) + 24,4,0));
			} else if (!network->config().enableBroadcast()) {
				// Don't transmit broadcasts if this network doesn't want them
				RR->t->outgoingNetworkFrameDropped(tPtr,network,from,to,etherType,vlanId,len,"broadcast disabled");
				return;
			}
		} else if ((etherType == ZT_ETHERTYPE_IPV6)&&(len >= (40 + 8 + 16))) {
			// IPv6 NDP emulation for certain very special patterns of private IPv6 addresses -- if enabled
			if ((network->config().ndpEmulation())&&(reinterpret_cast<const uint8_t *>(data)[6] == 0x3a)&&(reinterpret_cast<const uint8_t *>(data)[40] == 0x87)) { // ICMPv6 neighbor solicitation
				Address v6EmbeddedAddress;
				const uint8_t *const pkt6 = reinterpret_cast<const uint8_t *>(data) + 40 + 8;
				const uint8_t *my6 = (const uint8_t *)0;

				// ZT-RFC4193 address: fdNN:NNNN:NNNN:NNNN:NN99:93DD:DDDD:DDDD / 88 (one /128 per actual host)

				// ZT-6PLANE address:  fcXX:XXXX:XXDD:DDDD:DDDD:####:####:#### / 40 (one /80 per actual host)
				// (XX - lower 32 bits of network ID XORed with higher 32 bits)

				// For these to work, we must have a ZT-managed address assigned in one of the
				// above formats, and the query must match its prefix.
				for(unsigned int sipk=0;sipk<network->config().staticIpCount;++sipk) {
					const InetAddress *const sip = &(network->config().staticIps[sipk]);
					if (sip->ss_family == AF_INET6) {
						my6 = reinterpret_cast<const uint8_t *>(reinterpret_cast<const struct sockaddr_in6 *>(&(*sip))->sin6_addr.s6_addr);
						const unsigned int sipNetmaskBits = Utils::ntoh((uint16_t)reinterpret_cast<const struct sockaddr_in6 *>(&(*sip))->sin6_port);
						if ((sipNetmaskBits == 88)&&(my6[0] == 0xfd)&&(my6[9] == 0x99)&&(my6[10] == 0x93)) { // ZT-RFC4193 /88 ???
							unsigned int ptr = 0;
							while (ptr != 11) {
								if (pkt6[ptr] != my6[ptr])
									break;
								++ptr;
							}
							if (ptr == 11) { // prefix match!
								v6EmbeddedAddress.setTo(pkt6 + ptr,5);
								break;
							}
						} else if (sipNetmaskBits == 40) { // ZT-6PLANE /40 ???
							const uint32_t nwid32 = (uint32_t)((network->id() ^ (network->id() >> 32U)) & 0xffffffffU);
							if ( (my6[0] == 0xfc) && (my6[1] == (uint8_t)((nwid32 >> 24U) & 0xffU)) && (my6[2] == (uint8_t)((nwid32 >> 16U) & 0xffU)) && (my6[3] == (uint8_t)((nwid32 >> 8U) & 0xffU)) && (my6[4] == (uint8_t)(nwid32 & 0xffU))) {
								unsigned int ptr = 0;
								while (ptr != 5) {
									if (pkt6[ptr] != my6[ptr])
										break;
									++ptr;
								}
								if (ptr == 5) { // prefix match!
									v6EmbeddedAddress.setTo(pkt6 + ptr,5);
									break;
								}
							}
						}
					}
				}

				if ((v6EmbeddedAddress)&&(v6EmbeddedAddress != RR->identity.address())) {
					const MAC peerMac(v6EmbeddedAddress,network->id());

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
					adv[66] = peerMac[0]; adv[67] = peerMac[1]; adv[68] = peerMac[2]; adv[69] = peerMac[3]; adv[70] = peerMac[4]; adv[71] = peerMac[5];

					uint16_t pseudo_[36];
					uint8_t *const pseudo = reinterpret_cast<uint8_t *>(pseudo_);
					for(int i=0;i<32;++i) pseudo[i] = adv[8 + i];
					pseudo[32] = 0x00; pseudo[33] = 0x00; pseudo[34] = 0x00; pseudo[35] = 0x20;
					pseudo[36] = 0x00; pseudo[37] = 0x00; pseudo[38] = 0x00; pseudo[39] = 0x3a;
					for(int i=0;i<32;++i) pseudo[40 + i] = adv[40 + i];
					uint32_t checksum = 0;
					for(int i=0;i<36;++i) checksum += Utils::hton(pseudo_[i]);
					while ((checksum >> 16U)) checksum = (checksum & 0xffffU) + (checksum >> 16U);
					checksum = ~checksum;
					adv[42] = (checksum >> 8U) & 0xffU;
					adv[43] = checksum & 0xffU;

					RR->node->putFrame(tPtr,network->id(),network->userPtr(),peerMac,from,ZT_ETHERTYPE_IPV6,0,adv,72);
					return; // NDP emulation done. We have forged a "fake" reply, so no need to send actual NDP query.
				} // else no NDP emulation
			} // else no NDP emulation
		}

		// Check this after NDP emulation, since that has to be allowed in exactly this case
		if (network->config().multicastLimit == 0) {
			RR->t->outgoingNetworkFrameDropped(tPtr,network,from,to,etherType,vlanId,len,"multicast disabled");
			return;
		}

		/* Learn multicast groups for bridged-in hosts.
		 * Note that some OSes, most notably Linux, do this for you by learning
		 * multicast addresses on bridge interfaces and subscribing each slave.
		 * But in that case this does no harm, as the sets are just merged. */
		if (fromBridged)
			network->learnBridgedMulticastGroup(tPtr,multicastGroup,RR->node->now());

		// First pass sets noTee to false, but noTee is set to true in OutboundMulticast to prevent duplicates.
		if (!network->filterOutgoingPacket(tPtr,false,RR->identity.address(),Address(),from,to,(const uint8_t *)data,len,etherType,vlanId,qosBucket)) {
			RR->t->outgoingNetworkFrameDropped(tPtr,network,from,to,etherType,vlanId,len,"filter blocked");
			return;
		}

		// TODO
		/*
		RR->mc->send(
			tPtr,
			RR->node->now(),
			network,
			Address(),
			multicastGroup,
			(fromBridged) ? from : MAC(),
			etherType,
			data,
			len);
		*/
	} else if (to == network->mac()) {
		// Destination is this node, so just reinject it -------------------------

		RR->node->putFrame(tPtr,network->id(),network->userPtr(),from,to,etherType,vlanId,data,len);

	} else if (to[0] == MAC::firstOctetForNetwork(network->id())) {
		// Destination is another ZeroTier peer on the same network --------------

		Address toZT(to.toAddress(network->id())); // since in-network MACs are derived from addresses and network IDs, we can reverse this
		SharedPtr<Peer> toPeer(RR->topology->get(toZT));

		if (!network->filterOutgoingPacket(tPtr,false,RR->identity.address(),toZT,from,to,(const uint8_t *)data,len,etherType,vlanId,qosBucket)) {
			RR->t->outgoingNetworkFrameDropped(tPtr,network,from,to,etherType,vlanId,len,"filter blocked");
			return;
		}

		network->pushCredentialsIfNeeded(tPtr,toZT,RR->node->now());

		if (fromBridged) {
			Packet outp(toZT,RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(network->id());
			outp.append((unsigned char)0x00);
			to.appendTo(outp);
			from.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(data,len);
		} else {
			Packet outp(toZT,RR->identity.address(),Packet::VERB_FRAME);
			outp.append(network->id());
			outp.append((uint16_t)etherType);
			outp.append(data,len);
		}
	} else {
		// Destination is bridged behind a remote peer ---------------------------

		// We filter with a NULL destination ZeroTier address first. Filtrations
		// for each ZT destination are also done below. This is the same rationale
		// and design as for multicast.
		if (!network->filterOutgoingPacket(tPtr,false,RR->identity.address(),Address(),from,to,(const uint8_t *)data,len,etherType,vlanId,qosBucket)) {
			RR->t->outgoingNetworkFrameDropped(tPtr,network,from,to,etherType,vlanId,len,"filter blocked");
			return;
		}

		Address bridges[ZT_MAX_BRIDGE_SPAM];
		unsigned int numBridges = 0;

		/* Create an array of up to ZT_MAX_BRIDGE_SPAM recipients for this bridged frame. */
		bridges[0] = network->findBridgeTo(to);
		std::vector<Address> activeBridges;
		for(unsigned int i=0;i<network->config().specialistCount;++i) {
			if ((network->config().specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE) != 0)
				activeBridges.push_back(Address(network->config().specialists[i]));
		}
		if ((bridges[0])&&(bridges[0] != RR->identity.address())&&(network->config().permitsBridging(bridges[0]))) {
			/* We have a known bridge route for this MAC, send it there. */
			++numBridges;
		} else if (!activeBridges.empty()) {
			/* If there is no known route, spam to up to ZT_MAX_BRIDGE_SPAM active
			 * bridges. If someone responds, we'll learn the route. */
			std::vector<Address>::const_iterator ab(activeBridges.begin());
			if (activeBridges.size() <= ZT_MAX_BRIDGE_SPAM) {
				// If there are <= ZT_MAX_BRIDGE_SPAM active bridges, spam them all
				while (ab != activeBridges.end()) {
					bridges[numBridges++] = *ab;
					++ab;
				}
			} else {
				// Otherwise pick a random set of them
				while (numBridges < ZT_MAX_BRIDGE_SPAM) {
					if (ab == activeBridges.end())
						ab = activeBridges.begin();
					if (((unsigned long)Utils::random() % (unsigned long)activeBridges.size()) == 0) {
						bridges[numBridges++] = *ab;
						++ab;
					} else ++ab;
				}
			}
		}

		for(unsigned int b=0;b<numBridges;++b) {
			if (network->filterOutgoingPacket(tPtr,true,RR->identity.address(),bridges[b],from,to,(const uint8_t *)data,len,etherType,vlanId,qosBucket)) {
				Packet outp(bridges[b],RR->identity.address(),Packet::VERB_EXT_FRAME);
				outp.append(network->id());
				outp.append((uint8_t)0x00);
				to.appendTo(outp);
				from.appendTo(outp);
				outp.append((uint16_t)etherType);
				outp.append(data,len);
			} else {
				RR->t->outgoingNetworkFrameDropped(tPtr,network,from,to,etherType,vlanId,len,"filter blocked (bridge replication)");
			}
		}
	}
}

void Switch::send(void *tPtr,Packet &packet,bool encrypt)
{
	const Address dest(packet.destination());
	if (dest == RR->identity.address())
		return;
	if (!_trySend(tPtr,packet,encrypt)) {
		{
			Mutex::Lock _l(_txQueue_m);
			if (_txQueue.size() >= ZT_TX_QUEUE_SIZE) {
				_txQueue.pop_front();
			}
			_txQueue.push_back(TXQueueEntry(dest,RR->node->now(),packet,encrypt));
		}
		if (!RR->topology->get(dest))
			requestWhois(tPtr,RR->node->now(),dest);
	}
}

void Switch::requestWhois(void *tPtr,const int64_t now,const Address &addr)
{
	if (addr == RR->identity.address())
		return;

	{
		Mutex::Lock _l(_lastSentWhoisRequest_m);
		int64_t &last = _lastSentWhoisRequest[addr];
		if ((now - last) < ZT_WHOIS_RETRY_DELAY)
			return;
		else last = now;
	}

	const SharedPtr<Peer> root(RR->topology->root());
	if (root) {
		Packet outp(root->address(),RR->identity.address(),Packet::VERB_WHOIS);
		addr.appendTo(outp);
		RR->node->expectReplyTo(outp.packetId());
		root->sendDirect(tPtr,outp.data(),outp.size(),now);
	}
}

void Switch::doAnythingWaitingForPeer(void *tPtr,const SharedPtr<Peer> &peer)
{
	{
		Mutex::Lock _l(_lastSentWhoisRequest_m);
		_lastSentWhoisRequest.erase(peer->address());
	}

	const int64_t now = RR->node->now();
	for(unsigned int ptr=0;ptr<ZT_RX_QUEUE_SIZE;++ptr) {
		RXQueueEntry *const rq = &(_rxQueue[ptr]);
		Mutex::Lock rql(rq->lock);
		if ((rq->timestamp)&&(rq->complete)) {
			if ((rq->frag0.tryDecode(RR,tPtr))||((now - rq->timestamp) > ZT_RECEIVE_QUEUE_TIMEOUT))
				rq->timestamp = 0;
		}
	}

	{
		Mutex::Lock _l(_txQueue_m);
		for(std::list< TXQueueEntry >::iterator txi(_txQueue.begin());txi!=_txQueue.end();) {
			if (txi->dest == peer->address()) {
				if (_trySend(tPtr,txi->packet,txi->encrypt)) {
					_txQueue.erase(txi++);
				} else {
					++txi;
				}
			} else {
				++txi;
			}
		}
	}
}

unsigned long Switch::doTimerTasks(void *tPtr,int64_t now)
{
	const uint64_t timeSinceLastCheck = now - _lastCheckedQueues;
	if (timeSinceLastCheck < ZT_WHOIS_RETRY_DELAY)
		return (unsigned long)(ZT_WHOIS_RETRY_DELAY - timeSinceLastCheck);
	_lastCheckedQueues = now;

	std::vector<Address> needWhois;
	{
		Mutex::Lock _l(_txQueue_m);

		for(std::list< TXQueueEntry >::iterator txi(_txQueue.begin());txi!=_txQueue.end();) {
			if (_trySend(tPtr,txi->packet,txi->encrypt)) {
				_txQueue.erase(txi++);
			} else if ((now - txi->creationTime) > ZT_TRANSMIT_QUEUE_TIMEOUT) {
				_txQueue.erase(txi++);
			} else {
				if (!RR->topology->get(txi->dest))
					needWhois.push_back(txi->dest);
				++txi;
			}
		}
	}
	for(std::vector<Address>::const_iterator i(needWhois.begin());i!=needWhois.end();++i)
		requestWhois(tPtr,now,*i);

	for(unsigned int ptr=0;ptr<ZT_RX_QUEUE_SIZE;++ptr) {
		RXQueueEntry *const rq = &(_rxQueue[ptr]);
		Mutex::Lock rql(rq->lock);
		if ((rq->timestamp)&&(rq->complete)) {
			if ((rq->frag0.tryDecode(RR,tPtr))||((now - rq->timestamp) > ZT_RECEIVE_QUEUE_TIMEOUT)) {
				rq->timestamp = 0;
			} else {
				const Address src(rq->frag0.source());
				if (!RR->topology->get(src))
					requestWhois(tPtr,now,src);
			}
		}
	}

	{
		Mutex::Lock _l(_lastSentWhoisRequest_m);
		Hashtable< Address,int64_t >::Iterator i(_lastSentWhoisRequest);
		Address *a = (Address *)0;
		int64_t *ts = (int64_t *)0;
		while (i.next(a,ts)) {
			if ((now - *ts) > (ZT_WHOIS_RETRY_DELAY * 2))
				_lastSentWhoisRequest.erase(*a);
		}
	}

	return ZT_WHOIS_RETRY_DELAY;
}

bool Switch::_trySend(void *tPtr,Packet &packet,bool encrypt)
{
	SharedPtr<Path> viaPath;
	const int64_t now = RR->node->now();
	const Address destination(packet.destination());

	const SharedPtr<Peer> peer(RR->topology->get(destination));
	if (peer) {
		viaPath = peer->path();
		if (!viaPath) {
			if (peer->rateGateTryStaticPath(now)) {
				InetAddress tryAddr;
				bool gotPath = RR->node->externalPathLookup(tPtr,peer->identity(),AF_INET6,tryAddr);
				if ((gotPath)&&(tryAddr)) {
					peer->sendHELLO(tPtr,-1,tryAddr,now);
				} else {
					gotPath = RR->node->externalPathLookup(tPtr,peer->identity(),AF_INET,tryAddr);
					if ((gotPath)&&(tryAddr))
						peer->sendHELLO(tPtr,-1,tryAddr,now);
				}
			}

			const SharedPtr<Peer> relay(RR->topology->findRelayTo(now,destination));
			if (relay) {
				viaPath = relay->path();
				if (!viaPath)
					return false;
			}

			return false;
		}
	} else {
		return false;
	}

	unsigned int mtu = ZT_DEFAULT_PHYSMTU;
	uint64_t trustedPathId = 0;
	RR->topology->getOutboundPathInfo(viaPath->address(),mtu,trustedPathId);

	unsigned int chunkSize = std::min(packet.size(),mtu);
	packet.setFragmented(chunkSize < packet.size());

	if (trustedPathId) {
		packet.setTrusted(trustedPathId);
	} else {
		packet.armor(peer->key(),encrypt);
	}

	if (viaPath->send(RR,tPtr,packet.data(),chunkSize,now)) {
		if (chunkSize < packet.size()) {
			// Too big for one packet, fragment the rest
			unsigned int fragStart = chunkSize;
			unsigned int remaining = packet.size() - chunkSize;
			unsigned int fragsRemaining = (remaining / (mtu - ZT_PROTO_MIN_FRAGMENT_LENGTH));
			if ((fragsRemaining * (mtu - ZT_PROTO_MIN_FRAGMENT_LENGTH)) < remaining)
				++fragsRemaining;
			const unsigned int totalFragments = fragsRemaining + 1;

			for(unsigned int fno=1;fno<totalFragments;++fno) {
				chunkSize = std::min(remaining,(unsigned int)(mtu - ZT_PROTO_MIN_FRAGMENT_LENGTH));
				Packet::Fragment frag(packet,fragStart,chunkSize,fno,totalFragments);
				viaPath->send(RR,tPtr,frag.data(),frag.size(),now);
				fragStart += chunkSize;
				remaining -= chunkSize;
			}
		}
	}

	return true;
}

} // namespace ZeroTier
