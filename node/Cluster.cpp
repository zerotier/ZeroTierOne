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

#ifdef ZT_ENABLE_CLUSTER

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <algorithm>
#include <utility>

#include "../version.h"

#include "Cluster.hpp"
#include "RuntimeEnvironment.hpp"
#include "MulticastGroup.hpp"
#include "CertificateOfMembership.hpp"
#include "Salsa20.hpp"
#include "Poly1305.hpp"
#include "Identity.hpp"
#include "Topology.hpp"
#include "Packet.hpp"
#include "Switch.hpp"

namespace ZeroTier {

static inline double _dist3d(int x1,int y1,int z1,int x2,int y2,int z2)
	throw()
{
	double dx = ((double)x2 - (double)x1);
	double dy = ((double)y2 - (double)y1);
	double dz = ((double)z2 - (double)z1);
	return sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

Cluster::Cluster(
	const RuntimeEnvironment *renv,
	uint16_t id,
	const std::vector<InetAddress> &zeroTierPhysicalEndpoints,
	int32_t x,
	int32_t y,
	int32_t z,
	void (*sendFunction)(void *,unsigned int,const void *,unsigned int),
	void *sendFunctionArg,
	int (*addressToLocationFunction)(void *,const struct sockaddr_storage *,int *,int *,int *),
	void *addressToLocationFunctionArg) :
	RR(renv),
	_sendFunction(sendFunction),
	_sendFunctionArg(sendFunctionArg),
	_addressToLocationFunction(addressToLocationFunction),
	_addressToLocationFunctionArg(addressToLocationFunctionArg),
	_x(x),
	_y(y),
	_z(z),
	_id(id),
	_zeroTierPhysicalEndpoints(zeroTierPhysicalEndpoints),
	_members(new _Member[ZT_CLUSTER_MAX_MEMBERS])
{
	uint16_t stmp[ZT_SHA512_DIGEST_LEN / sizeof(uint16_t)];

	// Generate master secret by hashing the secret from our Identity key pair
	RR->identity.sha512PrivateKey(_masterSecret);

	// Generate our inbound message key, which is the master secret XORed with our ID and hashed twice
	memcpy(stmp,_masterSecret,sizeof(stmp));
	stmp[0] ^= Utils::hton(id);
	SHA512::hash(stmp,stmp,sizeof(stmp));
	SHA512::hash(stmp,stmp,sizeof(stmp));
	memcpy(_key,stmp,sizeof(_key));
	Utils::burn(stmp,sizeof(stmp));
}

Cluster::~Cluster()
{
	Utils::burn(_masterSecret,sizeof(_masterSecret));
	Utils::burn(_key,sizeof(_key));
	delete [] _members;
}

void Cluster::handleIncomingStateMessage(const void *msg,unsigned int len)
{
	Buffer<ZT_CLUSTER_MAX_MESSAGE_LENGTH> dmsg;
	{
		// FORMAT: <[16] iv><[8] MAC><... data>
		if ((len < 24)||(len > ZT_CLUSTER_MAX_MESSAGE_LENGTH))
			return;

		// 16-byte IV: first 8 bytes XORed with key, last 8 bytes used as Salsa20 64-bit IV
		char keytmp[32];
		memcpy(keytmp,_key,32);
		for(int i=0;i<8;++i)
			keytmp[i] ^= reinterpret_cast<const char *>(msg)[i];
		Salsa20 s20(keytmp,256,reinterpret_cast<const char *>(msg) + 8);
		Utils::burn(keytmp,sizeof(keytmp));

		// One-time-use Poly1305 key from first 32 bytes of Salsa20 keystream (as per DJB/NaCl "standard")
		char polykey[ZT_POLY1305_KEY_LEN];
		memset(polykey,0,sizeof(polykey));
		s20.encrypt12(polykey,polykey,sizeof(polykey));

		// Compute 16-byte MAC
		char mac[ZT_POLY1305_MAC_LEN];
		Poly1305::compute(mac,reinterpret_cast<const char *>(msg) + 24,len - 24,polykey);

		// Check first 8 bytes of MAC against 64-bit MAC in stream
		if (!Utils::secureEq(mac,reinterpret_cast<const char *>(msg) + 16,8))
			return;

		// Decrypt!
		dmsg.setSize(len - 24);
		s20.decrypt12(reinterpret_cast<const char *>(msg) + 24,const_cast<void *>(dmsg.data()),dmsg.size());
	}

	if (dmsg.size() < 4)
		return;
	const uint16_t fromMemberId = dmsg.at<uint16_t>(0);
	unsigned int ptr = 2;
	if (fromMemberId == _id) // sanity check: we don't talk to ourselves
		return;
	const uint16_t toMemberId = dmsg.at<uint16_t>(ptr);
	ptr += 2;
	if (toMemberId != _id) // sanity check: message not for us?
		return;

	{	// make sure sender is actually considered a member
		Mutex::Lock _l3(_memberIds_m);
		if (std::find(_memberIds.begin(),_memberIds.end(),fromMemberId) == _memberIds.end())
			return;
	}

	{
		_Member &m = _members[fromMemberId];
		Mutex::Lock mlck(m.lock);

		try {
			while (ptr < dmsg.size()) {
				const unsigned int mlen = dmsg.at<uint16_t>(ptr); ptr += 2;
				const unsigned int nextPtr = ptr + mlen;
				if (nextPtr > dmsg.size())
					break;

				int mtype = -1;
				try {
					switch((StateMessageType)(mtype = (int)dmsg[ptr++])) {
						default:
							break;

						case STATE_MESSAGE_ALIVE: {
							ptr += 7; // skip version stuff, not used yet
							m.x = dmsg.at<int32_t>(ptr); ptr += 4;
							m.y = dmsg.at<int32_t>(ptr); ptr += 4;
							m.z = dmsg.at<int32_t>(ptr); ptr += 4;
							ptr += 8; // skip local clock, not used
							m.load = dmsg.at<uint64_t>(ptr); ptr += 8;
							ptr += 8; // skip flags, unused
#ifdef ZT_TRACE
							std::string addrs;
#endif
							unsigned int physicalAddressCount = dmsg[ptr++];
							m.zeroTierPhysicalEndpoints.clear();
							for(unsigned int i=0;i<physicalAddressCount;++i) {
								m.zeroTierPhysicalEndpoints.push_back(InetAddress());
								ptr += m.zeroTierPhysicalEndpoints.back().deserialize(dmsg,ptr);
								if (!(m.zeroTierPhysicalEndpoints.back())) {
									m.zeroTierPhysicalEndpoints.pop_back();
								}
#ifdef ZT_TRACE
								else {
									if (addrs.length() > 0)
										addrs.push_back(',');
									addrs.append(m.zeroTierPhysicalEndpoints.back().toString());
								}
#endif
							}
#ifdef ZT_TRACE
							if ((RR->node->now() - m.lastReceivedAliveAnnouncement) >= ZT_CLUSTER_TIMEOUT) {
								TRACE("[%u] I'm alive! peers close to %d,%d,%d can be redirected to: %s",(unsigned int)fromMemberId,m.x,m.y,m.z,addrs.c_str());
							}
#endif
							m.lastReceivedAliveAnnouncement = RR->node->now();
						}	break;

						case STATE_MESSAGE_HAVE_PEER: {
							try {
								Identity id;
								ptr += id.deserialize(dmsg,ptr);
								if (id) {
									RR->topology->saveIdentity(id);

									{	// Add or update peer affinity entry
										_PeerAffinity pa(id.address(),fromMemberId,RR->node->now());
										Mutex::Lock _l2(_peerAffinities_m);
										std::vector<_PeerAffinity>::iterator i(std::lower_bound(_peerAffinities.begin(),_peerAffinities.end(),pa)); // O(log(n))
										if ((i != _peerAffinities.end())&&(i->key == pa.key)) {
											i->timestamp = pa.timestamp;
										} else {
											_peerAffinities.push_back(pa);
											std::sort(_peerAffinities.begin(),_peerAffinities.end()); // probably a more efficient way to insert but okay for now
										}
			 						}

			 						TRACE("[%u] has %s",(unsigned int)fromMemberId,id.address().toString().c_str());
			 					}
							} catch ( ... ) {
								// ignore invalid identities
							}
						}	break;

						case STATE_MESSAGE_MULTICAST_LIKE: {
							const uint64_t nwid = dmsg.at<uint64_t>(ptr); ptr += 8;
							const Address address(dmsg.field(ptr,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); ptr += ZT_ADDRESS_LENGTH;
							const MAC mac(dmsg.field(ptr,6),6); ptr += 6;
							const uint32_t adi = dmsg.at<uint32_t>(ptr); ptr += 4;
							RR->mc->add(RR->node->now(),nwid,MulticastGroup(mac,adi),address);
							TRACE("[%u] %s likes %s/%.8x on %.16llu",(unsigned int)fromMemberId,address.toString().c_str(),mac.toString().c_str(),(unsigned int)adi,nwid);
						}	break;

						case STATE_MESSAGE_COM: {
							CertificateOfMembership com;
							ptr += com.deserialize(dmsg,ptr);
							if (com) {
								TRACE("[%u] COM for %s on %.16llu rev %llu",(unsigned int)fromMemberId,com.issuedTo().toString().c_str(),com.networkId(),com.revision());
							}
						}	break;

						case STATE_MESSAGE_RELAY: {
							const unsigned int numRemotePeerPaths = dmsg[ptr++];
							InetAddress remotePeerPaths[256]; // size is 8-bit, so 256 is max
							for(unsigned int i=0;i<numRemotePeerPaths;++i)
								ptr += remotePeerPaths[i].deserialize(dmsg,ptr);
							const unsigned int packetLen = dmsg.at<uint16_t>(ptr); ptr += 2;
							const void *packet = (const void *)dmsg.field(ptr,packetLen); ptr += packetLen;

							if (packetLen >= ZT_PROTO_MIN_FRAGMENT_LENGTH) { // ignore anything too short to contain a dest address
								const Address destinationAddress(reinterpret_cast<const char *>(packet) + 8,ZT_ADDRESS_LENGTH);
								TRACE("[%u] relay %u bytes to %s (%u remote paths included)",(unsigned int)fromMemberId,packetLen,destinationAddress.toString().c_str(),numRemotePeerPaths);

								SharedPtr<Peer> destinationPeer(RR->topology->getPeer(destinationAddress));
								if (destinationPeer) {
									if (
									    (destinationPeer->send(RR,packet,packetLen,RR->node->now()))&&
									    (numRemotePeerPaths > 0)&&
									    (packetLen >= 18)&&
									    (reinterpret_cast<const unsigned char *>(packet)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR)
									   ) {
										// If remote peer paths were sent with this relayed packet, we do
										// RENDEZVOUS. It's handled here for cluster-relayed packets since
										// we don't have both Peer records so this is a different path.

										const Address remotePeerAddress(reinterpret_cast<const char *>(packet) + 13,ZT_ADDRESS_LENGTH);

										InetAddress bestDestV4,bestDestV6;
										destinationPeer->getBestActiveAddresses(RR->node->now(),bestDestV4,bestDestV6);
										InetAddress bestRemoteV4,bestRemoteV6;
										for(unsigned int i=0;i<numRemotePeerPaths;++i) {
											if ((bestRemoteV4)&&(bestRemoteV6))
												break;
											switch(remotePeerPaths[i].ss_family) {
												case AF_INET:
													if (!bestRemoteV4)
														bestRemoteV4 = remotePeerPaths[i];
													break;
												case AF_INET6:
													if (!bestRemoteV6)
														bestRemoteV6 = remotePeerPaths[i];
													break;
											}
										}

										Packet rendezvousForDest(destinationAddress,RR->identity.address(),Packet::VERB_RENDEZVOUS);
										rendezvousForDest.append((uint8_t)0);
										remotePeerAddress.appendTo(rendezvousForDest);

										Buffer<2048> rendezvousForOtherEnd;
										remotePeerAddress.appendTo(rendezvousForOtherEnd);
										rendezvousForOtherEnd.append((uint8_t)Packet::VERB_RENDEZVOUS);
										const unsigned int rendezvousForOtherEndPayloadSizePtr = rendezvousForOtherEnd.size();
										rendezvousForOtherEnd.addSize(2); // space for actual packet payload length
										rendezvousForOtherEnd.append((uint8_t)0); // flags == 0
										destinationAddress.appendTo(rendezvousForOtherEnd);

										bool haveMatch = false;
										if ((bestDestV6)&&(bestRemoteV6)) {
											haveMatch = true;

											rendezvousForDest.append((uint16_t)bestRemoteV6.port());
											rendezvousForDest.append((uint8_t)16);
											rendezvousForDest.append(bestRemoteV6.rawIpData(),16);

											rendezvousForOtherEnd.append((uint16_t)bestDestV6.port());
											rendezvousForOtherEnd.append((uint8_t)16);
											rendezvousForOtherEnd.append(bestDestV6.rawIpData(),16);
											rendezvousForOtherEnd.setAt<uint16_t>(rendezvousForOtherEndPayloadSizePtr,(uint16_t)(9 + 16));
										} else if ((bestDestV4)&&(bestRemoteV4)) {
											haveMatch = true;

											rendezvousForDest.append((uint16_t)bestRemoteV4.port());
											rendezvousForDest.append((uint8_t)4);
											rendezvousForDest.append(bestRemoteV4.rawIpData(),4);

											rendezvousForOtherEnd.append((uint16_t)bestDestV4.port());
											rendezvousForOtherEnd.append((uint8_t)4);
											rendezvousForOtherEnd.append(bestDestV4.rawIpData(),4);
											rendezvousForOtherEnd.setAt<uint16_t>(rendezvousForOtherEndPayloadSizePtr,(uint16_t)(9 + 4));
										}

										if (haveMatch) {
											_send(fromMemberId,STATE_MESSAGE_PROXY_SEND,rendezvousForOtherEnd.data(),rendezvousForOtherEnd.size());
											RR->sw->send(rendezvousForDest,true,0);
										}
									}
								}
							}
						}	break;

						case STATE_MESSAGE_PROXY_SEND: {
							const Address rcpt(dmsg.field(ptr,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH);
							const Packet::Verb verb = (Packet::Verb)dmsg[ptr++];
							const unsigned int len = dmsg.at<uint16_t>(ptr); ptr += 2;
							Packet outp(rcpt,RR->identity.address(),verb);
							outp.append(dmsg.field(ptr,len),len);
							RR->sw->send(outp,true,0);
							TRACE("[%u] proxy send %s to %s length %u",(unsigned int)fromMemberId,Packet::verbString(verb),rcpt.toString().c_str(),len);
						}	break;
					}
				} catch ( ... ) {
					TRACE("invalid message of size %u type %d (inner decode), discarding",mlen,mtype);
					// drop invalids
				}

				ptr = nextPtr;
			}
		} catch ( ... ) {
			TRACE("invalid message (outer loop), discarding");
			// drop invalids
		}
	}
}

bool Cluster::sendViaCluster(const Address &fromPeerAddress,const Address &toPeerAddress,const void *data,unsigned int len)
{
	if (len > 16384) // sanity check
		return false;

	uint64_t mostRecentTimestamp = 0;
	uint16_t canHasPeer = 0;

	{	// Anyone got this peer?
		Mutex::Lock _l2(_peerAffinities_m);
		std::vector<_PeerAffinity>::iterator i(std::lower_bound(_peerAffinities.begin(),_peerAffinities.end(),_PeerAffinity(toPeerAddress,0,0))); // O(log(n))
		while ((i != _peerAffinities.end())&&(i->address() == toPeerAddress)) {
			uint16_t mid = i->clusterMemberId();
			if ((mid != _id)&&(i->timestamp > mostRecentTimestamp)) {
				mostRecentTimestamp = i->timestamp;
				canHasPeer = mid;
			}
		}
	}

	const uint64_t now = RR->node->now();
	if ((now - mostRecentTimestamp) < ZT_PEER_ACTIVITY_TIMEOUT) {
		Buffer<16384> buf;

		InetAddress v4,v6;
		if (fromPeerAddress) {
			SharedPtr<Peer> fromPeer(RR->topology->getPeer(fromPeerAddress));
			if (fromPeer)
				fromPeer->getBestActiveAddresses(now,v4,v6);
		}
		buf.append((uint8_t)( (v4) ? ((v6) ? 2 : 1) : ((v6) ? 1 : 0) ));
		if (v4)
			v4.serialize(buf);
		if (v6)
			v6.serialize(buf);
		buf.append((uint16_t)len);
		buf.append(data,len);

		{
			Mutex::Lock _l2(_members[canHasPeer].lock);
			_send(canHasPeer,STATE_MESSAGE_RELAY,buf.data(),buf.size());
		}

		TRACE("sendViaCluster(): relaying %u bytes from %s to %s by way of %u",len,fromPeerAddress.toString().c_str(),toPeerAddress.toString().c_str(),(unsigned int)canHasPeer);
		return true;
	} else {
		TRACE("sendViaCluster(): unable to relay %u bytes from %s to %s since no cluster members seem to have it!",len,fromPeerAddress.toString().c_str(),toPeerAddress.toString().c_str());
		return false;
	}
}

void Cluster::replicateHavePeer(const Identity &peerId)
{
	{	// Use peer affinity table to track our own last announce time for peers
		_PeerAffinity pa(peerId.address(),_id,RR->node->now());
		Mutex::Lock _l2(_peerAffinities_m);
		std::vector<_PeerAffinity>::iterator i(std::lower_bound(_peerAffinities.begin(),_peerAffinities.end(),pa)); // O(log(n))
		if ((i != _peerAffinities.end())&&(i->key == pa.key)) {
			if ((pa.timestamp - i->timestamp) >= ZT_CLUSTER_HAVE_PEER_ANNOUNCE_PERIOD) {
				i->timestamp = pa.timestamp;
				// continue to announcement
			} else {
				// we've already announced this peer recently, so skip
				return;
			}
		} else {
			_peerAffinities.push_back(pa);
			std::sort(_peerAffinities.begin(),_peerAffinities.end()); // probably a more efficient way to insert but okay for now
			// continue to announcement
		}
	}

	// announcement
	Buffer<4096> buf;
	peerId.serialize(buf,false);
	{
		Mutex::Lock _l(_memberIds_m);
		for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
			Mutex::Lock _l2(_members[*mid].lock);
			_send(*mid,STATE_MESSAGE_HAVE_PEER,buf.data(),buf.size());
		}
	}
}

void Cluster::replicateMulticastLike(uint64_t nwid,const Address &peerAddress,const MulticastGroup &group)
{
	Buffer<2048> buf;
	buf.append((uint64_t)nwid);
	peerAddress.appendTo(buf);
	group.mac().appendTo(buf);
	buf.append((uint32_t)group.adi());
	TRACE("replicating %s MULTICAST_LIKE %.16llx/%s/%u to all members",peerAddress.toString().c_str(),nwid,group.mac().toString().c_str(),(unsigned int)group.adi());
	{
		Mutex::Lock _l(_memberIds_m);
		for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
			Mutex::Lock _l2(_members[*mid].lock);
			_send(*mid,STATE_MESSAGE_MULTICAST_LIKE,buf.data(),buf.size());
		}
	}
}

void Cluster::replicateCertificateOfNetworkMembership(const CertificateOfMembership &com)
{
	Buffer<2048> buf;
	com.serialize(buf);
	TRACE("replicating %s COM for %.16llx to all members",com.issuedTo().toString().c_str(),com.networkId());
	{
		Mutex::Lock _l(_memberIds_m);
		for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
			Mutex::Lock _l2(_members[*mid].lock);
			_send(*mid,STATE_MESSAGE_COM,buf.data(),buf.size());
		}
	}
}

void Cluster::doPeriodicTasks()
{
	const uint64_t now = RR->node->now();

	{
		Mutex::Lock _l(_memberIds_m);
		for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
			Mutex::Lock _l2(_members[*mid].lock);

			if ((now - _members[*mid].lastAnnouncedAliveTo) >= ((ZT_CLUSTER_TIMEOUT / 2) - 1000)) {
				Buffer<2048> alive;
				alive.append((uint16_t)ZEROTIER_ONE_VERSION_MAJOR);
				alive.append((uint16_t)ZEROTIER_ONE_VERSION_MINOR);
				alive.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
				alive.append((uint8_t)ZT_PROTO_VERSION);
				if (_addressToLocationFunction) {
					alive.append((int32_t)_x);
					alive.append((int32_t)_y);
					alive.append((int32_t)_z);
				} else {
					alive.append((int32_t)0);
					alive.append((int32_t)0);
					alive.append((int32_t)0);
				}
				alive.append((uint64_t)now);
				alive.append((uint64_t)0); // TODO: compute and send load average
				alive.append((uint64_t)0); // unused/reserved flags
				alive.append((uint8_t)_zeroTierPhysicalEndpoints.size());
				for(std::vector<InetAddress>::const_iterator pe(_zeroTierPhysicalEndpoints.begin());pe!=_zeroTierPhysicalEndpoints.end();++pe)
					pe->serialize(alive);
				_send(*mid,STATE_MESSAGE_ALIVE,alive.data(),alive.size());
				_members[*mid].lastAnnouncedAliveTo = now;
			}

			_flush(*mid); // does nothing if nothing to flush
		}
	}
}

void Cluster::addMember(uint16_t memberId)
{
	if ((memberId >= ZT_CLUSTER_MAX_MEMBERS)||(memberId == _id))
		return;

	Mutex::Lock _l2(_members[memberId].lock);

	{
		Mutex::Lock _l(_memberIds_m);
		if (std::find(_memberIds.begin(),_memberIds.end(),memberId) != _memberIds.end())
			return;
		_memberIds.push_back(memberId);
		std::sort(_memberIds.begin(),_memberIds.end());
	}

	_members[memberId].clear();

	// Generate this member's message key from the master and its ID
	uint16_t stmp[ZT_SHA512_DIGEST_LEN / sizeof(uint16_t)];
	memcpy(stmp,_masterSecret,sizeof(stmp));
	stmp[0] ^= Utils::hton(memberId);
	SHA512::hash(stmp,stmp,sizeof(stmp));
	SHA512::hash(stmp,stmp,sizeof(stmp));
	memcpy(_members[memberId].key,stmp,sizeof(_members[memberId].key));
	Utils::burn(stmp,sizeof(stmp));

	// Prepare q
	_members[memberId].q.clear();
	char iv[16];
	Utils::getSecureRandom(iv,16);
	_members[memberId].q.append(iv,16);
	_members[memberId].q.addSize(8); // room for MAC
	_members[memberId].q.append((uint16_t)_id);
	_members[memberId].q.append((uint16_t)memberId);
}

void Cluster::removeMember(uint16_t memberId)
{
	Mutex::Lock _l(_memberIds_m);
	std::vector<uint16_t> newMemberIds;
	for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
		if (*mid != memberId)
			newMemberIds.push_back(*mid);
	}
	_memberIds = newMemberIds;
}

InetAddress Cluster::findBetterEndpoint(const Address &peerAddress,const InetAddress &peerPhysicalAddress,bool offload)
{
	if (!peerPhysicalAddress) // sanity check
		return InetAddress();

	if (_addressToLocationFunction) {
		// Pick based on location if it can be determined
		int px = 0,py = 0,pz = 0;
		if (_addressToLocationFunction(_addressToLocationFunctionArg,reinterpret_cast<const struct sockaddr_storage *>(&peerPhysicalAddress),&px,&py,&pz) == 0) {
			TRACE("no geolocation data for %s (geo-lookup is lazy/async so it may work next time)",peerPhysicalAddress.toIpString().c_str());
			return InetAddress();
		}

		// Find member closest to this peer
		const uint64_t now = RR->node->now();
		std::vector<InetAddress> best; // initial "best" is for peer to stay put
		const double currentDistance = _dist3d(_x,_y,_z,px,py,pz);
		double bestDistance = (offload ? 2147483648.0 : currentDistance);
		unsigned int bestMember = _id;
		{
			Mutex::Lock _l(_memberIds_m);
			for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
				_Member &m = _members[*mid];
				Mutex::Lock _ml(m.lock);

				// Consider member if it's alive and has sent us a location and one or more physical endpoints to send peers to
				if ( ((now - m.lastReceivedAliveAnnouncement) < ZT_CLUSTER_TIMEOUT) && ((m.x != 0)||(m.y != 0)||(m.z != 0)) && (m.zeroTierPhysicalEndpoints.size() > 0) ) {
					double mdist = _dist3d(m.x,m.y,m.z,px,py,pz);
					if (mdist < bestDistance) {
						bestDistance = mdist;
						bestMember = *mid;
						best = m.zeroTierPhysicalEndpoints;
					}
				}
			}
		}

		for(std::vector<InetAddress>::const_iterator a(best.begin());a!=best.end();++a) {
			if (a->ss_family == peerPhysicalAddress.ss_family) {
				TRACE("%s at [%d,%d,%d] is %f from us but %f from %u, can redirect to %s",peerAddress.toString().c_str(),px,py,pz,currentDistance,bestDistance,bestMember,a->toString().c_str());
				return *a;
			}
		}
		TRACE("%s at [%d,%d,%d] is %f from us, no better endpoints found",peerAddress.toString().c_str(),px,py,pz,currentDistance);
		return InetAddress();
	} else {
		// TODO: pick based on load if no location info?
		return InetAddress();
	}
}

void Cluster::status(ZT_ClusterStatus &status) const
{
	const uint64_t now = RR->node->now();
	memset(&status,0,sizeof(ZT_ClusterStatus));
	ZT_ClusterMemberStatus *ms[ZT_CLUSTER_MAX_MEMBERS];
	memset(ms,0,sizeof(ms));

	status.myId = _id;

	ms[_id] = &(status.members[status.clusterSize++]);
	ms[_id]->id = _id;
	ms[_id]->alive = 1;
	ms[_id]->x = _x;
	ms[_id]->y = _y;
	ms[_id]->z = _z;
	ms[_id]->peers = RR->topology->countAlive();
	for(std::vector<InetAddress>::const_iterator ep(_zeroTierPhysicalEndpoints.begin());ep!=_zeroTierPhysicalEndpoints.end();++ep) {
		if (ms[_id]->numZeroTierPhysicalEndpoints >= ZT_CLUSTER_MAX_ZT_PHYSICAL_ADDRESSES) // sanity check
			break;
		memcpy(&(ms[_id]->zeroTierPhysicalEndpoints[ms[_id]->numZeroTierPhysicalEndpoints++]),&(*ep),sizeof(struct sockaddr_storage));
	}

	{
		Mutex::Lock _l1(_memberIds_m);
		for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
			if (status.clusterSize >= ZT_CLUSTER_MAX_MEMBERS) // sanity check
				break;
			ZT_ClusterMemberStatus *s = ms[*mid] = &(status.members[status.clusterSize++]);
			_Member &m = _members[*mid];
			Mutex::Lock ml(m.lock);

			s->id = *mid;
			s->msSinceLastHeartbeat = (unsigned int)std::min((uint64_t)(~((unsigned int)0)),(now - m.lastReceivedAliveAnnouncement));
			s->alive = (s->msSinceLastHeartbeat < ZT_CLUSTER_TIMEOUT) ? 1 : 0;
			s->x = m.x;
			s->y = m.y;
			s->z = m.z;
			s->load = m.load;
			for(std::vector<InetAddress>::const_iterator ep(m.zeroTierPhysicalEndpoints.begin());ep!=m.zeroTierPhysicalEndpoints.end();++ep) {
				if (s->numZeroTierPhysicalEndpoints >= ZT_CLUSTER_MAX_ZT_PHYSICAL_ADDRESSES) // sanity check
					break;
				memcpy(&(s->zeroTierPhysicalEndpoints[s->numZeroTierPhysicalEndpoints++]),&(*ep),sizeof(struct sockaddr_storage));
			}
		}
	}

	{
		Mutex::Lock _l2(_peerAffinities_m);
		for(std::vector<_PeerAffinity>::const_iterator pi(_peerAffinities.begin());pi!=_peerAffinities.end();++pi) {
			unsigned int mid = pi->clusterMemberId();
			if ((ms[mid])&&(mid != _id)&&((now - pi->timestamp) < ZT_PEER_ACTIVITY_TIMEOUT))
				++ms[mid]->peers;
		}
	}
}

void Cluster::_send(uint16_t memberId,StateMessageType type,const void *msg,unsigned int len)
{
	if ((len + 3) > (ZT_CLUSTER_MAX_MESSAGE_LENGTH - (24 + 2 + 2))) // sanity check
		return;
	_Member &m = _members[memberId];
	// assumes m.lock is locked!
	if ((m.q.size() + len + 3) > ZT_CLUSTER_MAX_MESSAGE_LENGTH)
		_flush(memberId);
	m.q.append((uint16_t)(len + 1));
	m.q.append((uint8_t)type);
	m.q.append(msg,len);
}

void Cluster::_flush(uint16_t memberId)
{
	_Member &m = _members[memberId];
	// assumes m.lock is locked!
	if (m.q.size() > (24 + 2 + 2)) { // 16-byte IV + 8-byte MAC + 2 byte from-member-ID + 2 byte to-member-ID
		// Create key from member's key and IV
		char keytmp[32];
		memcpy(keytmp,m.key,32);
		for(int i=0;i<8;++i)
			keytmp[i] ^= m.q[i];
		Salsa20 s20(keytmp,256,m.q.field(8,8));
		Utils::burn(keytmp,sizeof(keytmp));

		// One-time-use Poly1305 key from first 32 bytes of Salsa20 keystream (as per DJB/NaCl "standard")
		char polykey[ZT_POLY1305_KEY_LEN];
		memset(polykey,0,sizeof(polykey));
		s20.encrypt12(polykey,polykey,sizeof(polykey));

		// Encrypt m.q in place
		s20.encrypt12(reinterpret_cast<const char *>(m.q.data()) + 24,const_cast<char *>(reinterpret_cast<const char *>(m.q.data())) + 24,m.q.size() - 24);

		// Add MAC for authentication (encrypt-then-MAC)
		char mac[ZT_POLY1305_MAC_LEN];
		Poly1305::compute(mac,reinterpret_cast<const char *>(m.q.data()) + 24,m.q.size() - 24,polykey);
		memcpy(m.q.field(16,8),mac,8);

		// Send!
		_sendFunction(_sendFunctionArg,memberId,m.q.data(),m.q.size());

		// Prepare for more
		m.q.clear();
		char iv[16];
		Utils::getSecureRandom(iv,16);
		m.q.append(iv,16);
		m.q.addSize(8); // room for MAC
		m.q.append((uint16_t)_id); // from member ID
		m.q.append((uint16_t)memberId); // to member ID
	}
}

} // namespace ZeroTier

#endif // ZT_ENABLE_CLUSTER
