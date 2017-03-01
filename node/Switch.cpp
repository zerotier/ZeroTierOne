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
	_lastUniteAttempt(8) // only really used on root servers and upstreams, and it'll grow there just fine
{
}

void Switch::onRemotePacket(const InetAddress &localAddr,const InetAddress &fromAddr,const void *data,unsigned int len)
{
	try {
		const uint64_t now = RR->node->now();

		SharedPtr<Path> path(RR->topology->getPath(localAddr,fromAddr));
		path->received(now);

		if (len == 13) {
			/* LEGACY: before VERB_PUSH_DIRECT_PATHS, peers used broadcast
			 * announcements on the LAN to solve the 'same network problem.' We
			 * no longer send these, but we'll listen for them for a while to
			 * locate peers with versions <1.0.4. */

			const Address beaconAddr(reinterpret_cast<const char *>(data) + 8,5);
			if (beaconAddr == RR->identity.address())
				return;
			if (!RR->node->shouldUsePathForZeroTierTraffic(beaconAddr,localAddr,fromAddr))
				return;
			const SharedPtr<Peer> peer(RR->topology->getPeer(beaconAddr));
			if (peer) { // we'll only respond to beacons from known peers
				if ((now - _lastBeaconResponse) >= 2500) { // limit rate of responses
					_lastBeaconResponse = now;
					Packet outp(peer->address(),RR->identity.address(),Packet::VERB_NOP);
					outp.armor(peer->key(),true,path->nextOutgoingCounter());
					path->send(RR,outp.data(),outp.size(),now);
				}
			}

		} else if (len > ZT_PROTO_MIN_FRAGMENT_LENGTH) { // SECURITY: min length check is important since we do some C-style stuff below!
			if (reinterpret_cast<const uint8_t *>(data)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR) {
				// Handle fragment ----------------------------------------------------

				Packet::Fragment fragment(data,len);
				const Address destination(fragment.destination());

				if (destination != RR->identity.address()) {
#ifdef ZT_ENABLE_CLUSTER
					const bool isClusterFrontplane = ((RR->cluster)&&(RR->cluster->isClusterPeerFrontplane(fromAddr)));
#else
					const bool isClusterFrontplane = false;
#endif

					if ( (!RR->topology->amRoot()) && (!path->trustEstablished(now)) && (!isClusterFrontplane) )
						return;

					if (fragment.hops() < ZT_RELAY_MAX_HOPS) {
						fragment.incrementHops();

						// Note: we don't bother initiating NAT-t for fragments, since heads will set that off.
						// It wouldn't hurt anything, just redundant and unnecessary.
						SharedPtr<Peer> relayTo = RR->topology->getPeer(destination);
						if ((!relayTo)||(!relayTo->sendDirect(fragment.data(),fragment.size(),now,false))) {
#ifdef ZT_ENABLE_CLUSTER
							if ((RR->cluster)&&(!isClusterFrontplane)) {
								RR->cluster->relayViaCluster(Address(),destination,fragment.data(),fragment.size(),false);
								return;
							}
#endif

							// Don't know peer or no direct path -- so relay via someone upstream
							relayTo = RR->topology->getUpstreamPeer();
							if (relayTo)
								relayTo->sendDirect(fragment.data(),fragment.size(),now,true);
						}
					} else {
						TRACE("dropped relay [fragment](%s) -> %s, max hops exceeded",fromAddr.toString().c_str(),destination.toString().c_str());
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

						Mutex::Lock _l(_rxQueue_m);
						RXQueueEntry *const rq = _findRXQueueEntry(now,fragmentPacketId);

						if ((!rq->timestamp)||(rq->packetId != fragmentPacketId)) {
							// No packet found, so we received a fragment without its head.
							//TRACE("fragment (%u/%u) of %.16llx from %s",fragmentNumber + 1,totalFragments,fragmentPacketId,fromAddr.toString().c_str());

							rq->timestamp = now;
							rq->packetId = fragmentPacketId;
							rq->frags[fragmentNumber - 1] = fragment;
							rq->totalFragments = totalFragments; // total fragment count is known
							rq->haveFragments = 1 << fragmentNumber; // we have only this fragment
							rq->complete = false;
						} else if (!(rq->haveFragments & (1 << fragmentNumber))) {
							// We have other fragments and maybe the head, so add this one and check
							//TRACE("fragment (%u/%u) of %.16llx from %s",fragmentNumber + 1,totalFragments,fragmentPacketId,fromAddr.toString().c_str());

							rq->frags[fragmentNumber - 1] = fragment;
							rq->totalFragments = totalFragments;

							if (Utils::countBits(rq->haveFragments |= (1 << fragmentNumber)) == totalFragments) {
								// We have all fragments -- assemble and process full Packet
								//TRACE("packet %.16llx is complete, assembling and processing...",fragmentPacketId);

								for(unsigned int f=1;f<totalFragments;++f)
									rq->frag0.append(rq->frags[f - 1].payload(),rq->frags[f - 1].payloadLength());

								if (rq->frag0.tryDecode(RR)) {
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

				//TRACE("<< %.16llx %s -> %s (size: %u)",(unsigned long long)packet->packetId(),source.toString().c_str(),destination.toString().c_str(),packet->size());

#ifdef ZT_ENABLE_CLUSTER
				if ( (source == RR->identity.address()) && ((!RR->cluster)||(!RR->cluster->isClusterPeerFrontplane(fromAddr))) )
					return;
#else
				if (source == RR->identity.address())
					return;
#endif

				if (destination != RR->identity.address()) {
					if ( (!RR->topology->amRoot()) && (!path->trustEstablished(now)) && (source != RR->identity.address()) )
						return;

					Packet packet(data,len);

					if (packet.hops() < ZT_RELAY_MAX_HOPS) {
#ifdef ZT_ENABLE_CLUSTER
						if (source != RR->identity.address()) // don't increment hops for cluster frontplane relays
							packet.incrementHops();
#else
						packet.incrementHops();
#endif

						SharedPtr<Peer> relayTo = RR->topology->getPeer(destination);
						if ((relayTo)&&(relayTo->sendDirect(packet.data(),packet.size(),now,false))) {
							if ((source != RR->identity.address())&&(_shouldUnite(now,source,destination))) { // don't send RENDEZVOUS for cluster frontplane relays
								const InetAddress *hintToSource = (InetAddress *)0;
								const InetAddress *hintToDest = (InetAddress *)0;

								InetAddress destV4,destV6;
								InetAddress sourceV4,sourceV6;
								relayTo->getRendezvousAddresses(now,destV4,destV6);

								const SharedPtr<Peer> sourcePeer(RR->topology->getPeer(source));
								if (sourcePeer) {
									sourcePeer->getRendezvousAddresses(now,sourceV4,sourceV6);
									if ((destV6)&&(sourceV6)) {
										hintToSource = &destV6;
										hintToDest = &sourceV6;
									} else if ((destV4)&&(sourceV4)) {
										hintToSource = &destV4;
										hintToDest = &sourceV4;
									}

									if ((hintToSource)&&(hintToDest)) {
										unsigned int alt = (unsigned int)RR->node->prng() & 1; // randomize which hint we send first for obscure NAT-t reasons
										const unsigned int completed = alt + 2;
										while (alt != completed) {
											if ((alt & 1) == 0) {
												Packet outp(source,RR->identity.address(),Packet::VERB_RENDEZVOUS);
												outp.append((uint8_t)0);
												destination.appendTo(outp);
												outp.append((uint16_t)hintToSource->port());
												if (hintToSource->ss_family == AF_INET6) {
													outp.append((uint8_t)16);
													outp.append(hintToSource->rawIpData(),16);
												} else {
													outp.append((uint8_t)4);
													outp.append(hintToSource->rawIpData(),4);
												}
												send(outp,true);
											} else {
												Packet outp(destination,RR->identity.address(),Packet::VERB_RENDEZVOUS);
												outp.append((uint8_t)0);
												source.appendTo(outp);
												outp.append((uint16_t)hintToDest->port());
												if (hintToDest->ss_family == AF_INET6) {
													outp.append((uint8_t)16);
													outp.append(hintToDest->rawIpData(),16);
												} else {
													outp.append((uint8_t)4);
													outp.append(hintToDest->rawIpData(),4);
												}
												send(outp,true);
											}
											++alt;
										}
									}
								}
							}
						} else {
#ifdef ZT_ENABLE_CLUSTER
							if ((RR->cluster)&&(source != RR->identity.address())) {
								RR->cluster->relayViaCluster(source,destination,packet.data(),packet.size(),_shouldUnite(now,source,destination));
								return;
							}
#endif
							relayTo = RR->topology->getUpstreamPeer(&source,1,true);
							if (relayTo)
								relayTo->sendDirect(packet.data(),packet.size(),now,true);
						}
					} else {
						TRACE("dropped relay %s(%s) -> %s, max hops exceeded",packet.source().toString().c_str(),fromAddr.toString().c_str(),destination.toString().c_str());
					}
				} else if ((reinterpret_cast<const uint8_t *>(data)[ZT_PACKET_IDX_FLAGS] & ZT_PROTO_FLAG_FRAGMENTED) != 0) {
					// Packet is the head of a fragmented packet series

					const uint64_t packetId = (
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[0]) << 56) |
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[1]) << 48) |
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[2]) << 40) |
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[3]) << 32) |
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[4]) << 24) |
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[5]) << 16) |
						(((uint64_t)reinterpret_cast<const uint8_t *>(data)[6]) << 8) |
						((uint64_t)reinterpret_cast<const uint8_t *>(data)[7])
					);

					Mutex::Lock _l(_rxQueue_m);
					RXQueueEntry *const rq = _findRXQueueEntry(now,packetId);

					if ((!rq->timestamp)||(rq->packetId != packetId)) {
						// If we have no other fragments yet, create an entry and save the head
						//TRACE("fragment (0/?) of %.16llx from %s",pid,fromAddr.toString().c_str());

						rq->timestamp = now;
						rq->packetId = packetId;
						rq->frag0.init(data,len,path,now);
						rq->totalFragments = 0;
						rq->haveFragments = 1;
						rq->complete = false;
					} else if (!(rq->haveFragments & 1)) {
						// If we have other fragments but no head, see if we are complete with the head

						if ((rq->totalFragments > 1)&&(Utils::countBits(rq->haveFragments |= 1) == rq->totalFragments)) {
							// We have all fragments -- assemble and process full Packet
							//TRACE("packet %.16llx is complete, assembling and processing...",pid);

							rq->frag0.init(data,len,path,now);
							for(unsigned int f=1;f<rq->totalFragments;++f)
								rq->frag0.append(rq->frags[f - 1].payload(),rq->frags[f - 1].payloadLength());

							if (rq->frag0.tryDecode(RR)) {
								rq->timestamp = 0; // packet decoded, free entry
							} else {
								rq->complete = true; // set complete flag but leave entry since it probably needs WHOIS or something
							}
						} else {
							// Still waiting on more fragments, but keep the head
							rq->frag0.init(data,len,path,now);
						}
					} // else this is a duplicate head, ignore
				} else {
					// Packet is unfragmented, so just process it
					IncomingPacket packet(data,len,path,now);
					if (!packet.tryDecode(RR)) {
						Mutex::Lock _l(_rxQueue_m);
						RXQueueEntry *rq = &(_rxQueue[ZT_RX_QUEUE_SIZE - 1]);
						unsigned long i = ZT_RX_QUEUE_SIZE - 1;
						while ((i)&&(rq->timestamp)) {
							RXQueueEntry *tmp = &(_rxQueue[--i]);
							if (tmp->timestamp < rq->timestamp)
								rq = tmp;
						}
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
	} catch (std::exception &ex) {
		TRACE("dropped packet from %s: unexpected exception: %s",fromAddr.toString().c_str(),ex.what());
	} catch ( ... ) {
		TRACE("dropped packet from %s: unexpected exception: (unknown)",fromAddr.toString().c_str());
	}
}

void Switch::onLocalEthernet(const SharedPtr<Network> &network,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
{
	if (!network->hasConfig())
		return;

	// Check if this packet is from someone other than the tap -- i.e. bridged in
	bool fromBridged;
	if ((fromBridged = (from != network->mac()))) {
		if (!network->config().permitsBridging(RR->identity.address())) {
			TRACE("%.16llx: %s -> %s %s not forwarded, bridging disabled or this peer not a bridge",network->id(),from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType));
			return;
		}
	}

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
				TRACE("%.16llx: dropped broadcast since ff:ff:ff:ff:ff:ff is not enabled",network->id());
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
							const uint32_t nwid32 = (uint32_t)((network->id() ^ (network->id() >> 32)) & 0xffffffff);
							if ( (my6[0] == 0xfc) && (my6[1] == (uint8_t)((nwid32 >> 24) & 0xff)) && (my6[2] == (uint8_t)((nwid32 >> 16) & 0xff)) && (my6[3] == (uint8_t)((nwid32 >> 8) & 0xff)) && (my6[4] == (uint8_t)(nwid32 & 0xff))) {
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
					TRACE("IPv6 NDP emulation: %.16llx: forging response for %s/%s",network->id(),v6EmbeddedAddress.toString().c_str(),peerMac.toString().c_str());

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
					while ((checksum >> 16)) checksum = (checksum & 0xffff) + (checksum >> 16);
					checksum = ~checksum;
					adv[42] = (checksum >> 8) & 0xff;
					adv[43] = checksum & 0xff;

					RR->node->putFrame(network->id(),network->userPtr(),peerMac,from,ZT_ETHERTYPE_IPV6,0,adv,72);
					return; // NDP emulation done. We have forged a "fake" reply, so no need to send actual NDP query.
				} // else no NDP emulation
			} // else no NDP emulation
		}

		// Check this after NDP emulation, since that has to be allowed in exactly this case
		if (network->config().multicastLimit == 0) {
			TRACE("%.16llx: dropped multicast: not allowed on network",network->id());
			return;
		}

		/* Learn multicast groups for bridged-in hosts.
		 * Note that some OSes, most notably Linux, do this for you by learning
		 * multicast addresses on bridge interfaces and subscribing each slave.
		 * But in that case this does no harm, as the sets are just merged. */
		if (fromBridged)
			network->learnBridgedMulticastGroup(multicastGroup,RR->node->now());

		//TRACE("%.16llx: MULTICAST %s -> %s %s %u",network->id(),from.toString().c_str(),multicastGroup.toString().c_str(),etherTypeName(etherType),len);

		// First pass sets noTee to false, but noTee is set to true in OutboundMulticast to prevent duplicates.
		if (!network->filterOutgoingPacket(false,RR->identity.address(),Address(),from,to,(const uint8_t *)data,len,etherType,vlanId)) {
			TRACE("%.16llx: %s -> %s %s packet not sent: filterOutgoingPacket() returned false",network->id(),from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType));
			return;
		}

		RR->mc->send(
			network->config().multicastLimit,
			RR->node->now(),
			network->id(),
			network->config().disableCompression(),
			network->config().activeBridges(),
			multicastGroup,
			(fromBridged) ? from : MAC(),
			etherType,
			data,
			len);
	} else if (to == network->mac()) {
		// Destination is this node, so just reinject it
		RR->node->putFrame(network->id(),network->userPtr(),from,to,etherType,vlanId,data,len);
	} else if (to[0] == MAC::firstOctetForNetwork(network->id())) {
		// Destination is another ZeroTier peer on the same network

		Address toZT(to.toAddress(network->id())); // since in-network MACs are derived from addresses and network IDs, we can reverse this
		SharedPtr<Peer> toPeer(RR->topology->getPeer(toZT));

		if (!network->filterOutgoingPacket(false,RR->identity.address(),toZT,from,to,(const uint8_t *)data,len,etherType,vlanId)) {
			TRACE("%.16llx: %s -> %s %s packet not sent: filterOutgoingPacket() returned false",network->id(),from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType));
			return;
		}

		if (fromBridged) {
			Packet outp(toZT,RR->identity.address(),Packet::VERB_EXT_FRAME);
			outp.append(network->id());
			outp.append((unsigned char)0x00);
			to.appendTo(outp);
			from.appendTo(outp);
			outp.append((uint16_t)etherType);
			outp.append(data,len);
			if (!network->config().disableCompression())
				outp.compress();
			send(outp,true);
		} else {
			Packet outp(toZT,RR->identity.address(),Packet::VERB_FRAME);
			outp.append(network->id());
			outp.append((uint16_t)etherType);
			outp.append(data,len);
			if (!network->config().disableCompression())
				outp.compress();
			send(outp,true);
		}

		//TRACE("%.16llx: UNICAST: %s -> %s etherType==%s(%.4x) vlanId==%u len==%u fromBridged==%d includeCom==%d",network->id(),from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType),etherType,vlanId,len,(int)fromBridged,(int)includeCom);
	} else {
		// Destination is bridged behind a remote peer

		// We filter with a NULL destination ZeroTier address first. Filtrations
		// for each ZT destination are also done below. This is the same rationale
		// and design as for multicast.
		if (!network->filterOutgoingPacket(false,RR->identity.address(),Address(),from,to,(const uint8_t *)data,len,etherType,vlanId)) {
			TRACE("%.16llx: %s -> %s %s packet not sent: filterOutgoingPacket() returned false",network->id(),from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType));
			return;
		}

		Address bridges[ZT_MAX_BRIDGE_SPAM];
		unsigned int numBridges = 0;

		/* Create an array of up to ZT_MAX_BRIDGE_SPAM recipients for this bridged frame. */
		bridges[0] = network->findBridgeTo(to);
		std::vector<Address> activeBridges(network->config().activeBridges());
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
					if (((unsigned long)RR->node->prng() % (unsigned long)activeBridges.size()) == 0) {
						bridges[numBridges++] = *ab;
						++ab;
					} else ++ab;
				}
			}
		}

		for(unsigned int b=0;b<numBridges;++b) {
			if (network->filterOutgoingPacket(true,RR->identity.address(),bridges[b],from,to,(const uint8_t *)data,len,etherType,vlanId)) {
				Packet outp(bridges[b],RR->identity.address(),Packet::VERB_EXT_FRAME);
				outp.append(network->id());
				outp.append((uint8_t)0x00);
				to.appendTo(outp);
				from.appendTo(outp);
				outp.append((uint16_t)etherType);
				outp.append(data,len);
				if (!network->config().disableCompression())
					outp.compress();
				send(outp,true);
			} else {
				TRACE("%.16llx: %s -> %s %s packet not sent: filterOutgoingPacket() returned false",network->id(),from.toString().c_str(),to.toString().c_str(),etherTypeName(etherType));
			}
		}
	}
}

void Switch::send(Packet &packet,bool encrypt)
{
	if (packet.destination() == RR->identity.address()) {
		TRACE("BUG: caught attempt to send() to self, ignored");
		return;
	}

	if (!_trySend(packet,encrypt)) {
		Mutex::Lock _l(_txQueue_m);
		_txQueue.push_back(TXQueueEntry(packet.destination(),RR->node->now(),packet,encrypt));
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
		unsigned long i = ZT_RX_QUEUE_SIZE;
		while (i) {
			RXQueueEntry *rq = &(_rxQueue[--i]);
			if ((rq->timestamp)&&(rq->complete)) {
				if (rq->frag0.tryDecode(RR))
					rq->timestamp = 0;
			}
		}
	}

	{	// finish sending any packets waiting on peer's public key / identity
		Mutex::Lock _l(_txQueue_m);
		for(std::list< TXQueueEntry >::iterator txi(_txQueue.begin());txi!=_txQueue.end();) {
			if (txi->dest == peer->address()) {
				if (_trySend(txi->packet,txi->encrypt))
					_txQueue.erase(txi++);
				else ++txi;
			} else ++txi;
		}
	}
}

unsigned long Switch::doTimerTasks(uint64_t now)
{
	unsigned long nextDelay = 0xffffffff; // ceiling delay, caller will cap to minimum

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
			if (_trySend(txi->packet,txi->encrypt))
				_txQueue.erase(txi++);
			else if ((now - txi->creationTime) > ZT_TRANSMIT_QUEUE_TIMEOUT) {
				TRACE("TX %s -> %s timed out",txi->packet.source().toString().c_str(),txi->packet.destination().toString().c_str());
				_txQueue.erase(txi++);
			} else ++txi;
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

bool Switch::_shouldUnite(const uint64_t now,const Address &source,const Address &destination)
{
	Mutex::Lock _l(_lastUniteAttempt_m);
	uint64_t &ts = _lastUniteAttempt[_LastUniteKey(source,destination)];
	if ((now - ts) >= ZT_MIN_UNITE_INTERVAL) {
		ts = now;
		return true;
	}
	return false;
}

Address Switch::_sendWhoisRequest(const Address &addr,const Address *peersAlreadyConsulted,unsigned int numPeersAlreadyConsulted)
{
	SharedPtr<Peer> upstream(RR->topology->getUpstreamPeer(peersAlreadyConsulted,numPeersAlreadyConsulted,false));
	if (upstream) {
		Packet outp(upstream->address(),RR->identity.address(),Packet::VERB_WHOIS);
		addr.appendTo(outp);
		RR->node->expectReplyTo(outp.packetId());
		send(outp,true);
	}
	return Address();
}

bool Switch::_trySend(Packet &packet,bool encrypt)
{
	SharedPtr<Path> viaPath;
	const uint64_t now = RR->node->now();
	const Address destination(packet.destination());

#ifdef ZT_ENABLE_CLUSTER
	uint64_t clusterMostRecentTs = 0;
	int clusterMostRecentMemberId = -1;
	uint8_t clusterPeerSecret[ZT_PEER_SECRET_KEY_LENGTH];
	if (RR->cluster)
		clusterMostRecentMemberId = RR->cluster->checkSendViaCluster(destination,clusterMostRecentTs,clusterPeerSecret);
#endif

	const SharedPtr<Peer> peer(RR->topology->getPeer(destination));
	if (peer) {
		/* First get the best path, and if it's dead (and this is not a root)
		 * we attempt to re-activate that path but this packet will flow
		 * upstream. If the path comes back alive, it will be used in the future.
		 * For roots we don't do the alive check since roots are not required
		 * to send heartbeats "down" and because we have to at least try to
		 * go somewhere. */

		viaPath = peer->getBestPath(now,false);
		if ( (viaPath) && (!viaPath->alive(now)) && (!RR->topology->isUpstream(peer->identity())) ) {
#ifdef ZT_ENABLE_CLUSTER
			if ((clusterMostRecentMemberId < 0)||(viaPath->lastIn() > clusterMostRecentTs)) {
#endif
				if ((now - viaPath->lastOut()) > std::max((now - viaPath->lastIn()) * 4,(uint64_t)ZT_PATH_MIN_REACTIVATE_INTERVAL)) {
					peer->attemptToContactAt(viaPath->localAddress(),viaPath->address(),now,false,viaPath->nextOutgoingCounter());
					viaPath->sent(now);
				}
#ifdef ZT_ENABLE_CLUSTER
			}
#endif
			viaPath.zero();
		}

#ifdef ZT_ENABLE_CLUSTER
		if (clusterMostRecentMemberId >= 0) {
			if ((viaPath)&&(viaPath->lastIn() < clusterMostRecentTs))
				viaPath.zero();
		} else if (!viaPath) {
#else
		if (!viaPath) {
#endif
			peer->tryMemorizedPath(now); // periodically attempt memorized or statically defined paths, if any are known
			const SharedPtr<Peer> relay(RR->topology->getUpstreamPeer());
			if ( (!relay) || (!(viaPath = relay->getBestPath(now,false))) ) {
				if (!(viaPath = peer->getBestPath(now,true)))
					return false;
			}
#ifdef ZT_ENABLE_CLUSTER
		}
#else
		}
#endif
	} else {
#ifdef ZT_ENABLE_CLUSTER
		if (clusterMostRecentMemberId < 0) {
#else
			requestWhois(destination);
			return false; // if we are not in cluster mode, there is no way we can send without knowing the peer directly
#endif
#ifdef ZT_ENABLE_CLUSTER
		}
#endif
	}

	unsigned int chunkSize = std::min(packet.size(),(unsigned int)ZT_UDP_DEFAULT_PAYLOAD_MTU);
	packet.setFragmented(chunkSize < packet.size());

#ifdef ZT_ENABLE_CLUSTER
	const uint64_t trustedPathId = (viaPath) ? RR->topology->getOutboundPathTrust(viaPath->address()) : 0;
	if (trustedPathId) {
		packet.setTrusted(trustedPathId);
	} else {
		packet.armor((clusterMostRecentMemberId >= 0) ? clusterPeerSecret : peer->key(),encrypt,(viaPath) ? viaPath->nextOutgoingCounter() : 0);
	}
#else
	const uint64_t trustedPathId = RR->topology->getOutboundPathTrust(viaPath->address());
	if (trustedPathId) {
		packet.setTrusted(trustedPathId);
	} else {
		packet.armor(peer->key(),encrypt,viaPath->nextOutgoingCounter());
	}
#endif

#ifdef ZT_ENABLE_CLUSTER
	if ( ((viaPath)&&(viaPath->send(RR,packet.data(),chunkSize,now))) || ((clusterMostRecentMemberId >= 0)&&(RR->cluster->sendViaCluster(clusterMostRecentMemberId,destination,packet.data(),chunkSize))) ) {
#else
	if (viaPath->send(RR,packet.data(),chunkSize,now)) {
#endif
		if (chunkSize < packet.size()) {
			// Too big for one packet, fragment the rest
			unsigned int fragStart = chunkSize;
			unsigned int remaining = packet.size() - chunkSize;
			unsigned int fragsRemaining = (remaining / (ZT_UDP_DEFAULT_PAYLOAD_MTU - ZT_PROTO_MIN_FRAGMENT_LENGTH));
			if ((fragsRemaining * (ZT_UDP_DEFAULT_PAYLOAD_MTU - ZT_PROTO_MIN_FRAGMENT_LENGTH)) < remaining)
				++fragsRemaining;
			const unsigned int totalFragments = fragsRemaining + 1;

			for(unsigned int fno=1;fno<totalFragments;++fno) {
				chunkSize = std::min(remaining,(unsigned int)(ZT_UDP_DEFAULT_PAYLOAD_MTU - ZT_PROTO_MIN_FRAGMENT_LENGTH));
				Packet::Fragment frag(packet,fragStart,chunkSize,fno,totalFragments);
#ifdef ZT_ENABLE_CLUSTER
				if (viaPath)
					viaPath->send(RR,frag.data(),frag.size(),now);
				else if (clusterMostRecentMemberId >= 0)
					RR->cluster->sendViaCluster(clusterMostRecentMemberId,destination,frag.data(),frag.size());
#else
				viaPath->send(RR,frag.data(),frag.size(),now);
#endif
				fragStart += chunkSize;
				remaining -= chunkSize;
			}
		}
	}

	return true;
}

} // namespace ZeroTier
