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

#include <algorithm>
#include <utility>

#include "Cluster.hpp"
#include "RuntimeEnvironment.hpp"
#include "MulticastGroup.hpp"
#include "CertificateOfMembership.hpp"
#include "Salsa20.hpp"
#include "Poly1305.hpp"
#include "Packet.hpp"
#include "Peer.hpp"
#include "Switch.hpp"
#include "Node.hpp"

namespace ZeroTier {

Cluster::Cluster(const RuntimeEnvironment *renv,uint16_t id,DistanceAlgorithm da,int32_t x,int32_t y,int32_t z,void (*sendFunction)(void *,uint16_t,const void *,unsigned int),void *arg) :
	RR(renv),
	_sendFunction(sendFunction),
	_arg(arg),
	_x(x),
	_y(y),
	_z(z),
	_da(da),
	_id(id)
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
		dmsg.setSize(len - 16);
		s20.decrypt12(reinterpret_cast<const char *>(msg) + 16,const_cast<void *>(dmsg.data()),dmsg.size());
	}

	if (dmsg.size() < 2)
		return;
	const uint16_t fromMemberId = dmsg.at<uint16_t>(0);
	unsigned int ptr = 2;

	_Member &m = _members[fromMemberId];
	Mutex::Lock mlck(m.lock);

	m.lastReceivedFrom = RR->node->now();

	try {
		while (ptr < dmsg.size()) {
			const unsigned int mlen = dmsg.at<uint16_t>(ptr); ptr += 2;
			const unsigned int nextPtr = ptr + mlen;

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
						m.physicalAddressCount = dmsg[ptr++];
						if (m.physicalAddressCount > ZT_CLUSTER_MEMBER_MAX_PHYSICAL_ADDRS)
							m.physicalAddressCount = ZT_CLUSTER_MEMBER_MAX_PHYSICAL_ADDRS;
						for(unsigned int i=0;i<m.physicalAddressCount;++i)
							ptr += m.physicalAddresses[i].deserialize(dmsg,ptr);
						m.lastReceivedAliveAnnouncement = RR->node->now();
					}	break;

					case STATE_MESSAGE_HAVE_PEER: {
						try {
							Identity id;
							ptr += id.deserialize(dmsg,ptr);
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
					}	break;

					case STATE_MESSAGE_COM: {
						// TODO: not used yet
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
							SharedPtr<Peer> destinationPeer(RR->topology->getPeer(destinationAddress));
							if (destinationPeer) {
								RemotePath *destinationPath = destinationPeer->send(RR,packet,packetLen,RR->node->now());
								if ((destinationPath)&&(numRemotePeerPaths > 0)&&(packetLen >= 18)&&(reinterpret_cast<const unsigned char *>(packet)[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR)) {
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
									rendezvousForOtherEnd.addSize(2); // leave room for payload size
									rendezvousForOtherEnd.append((uint8_t)STATE_MESSAGE_PROXY_SEND);
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
										RR->sw->send(rendezvousForDest,true,0);
										rendezvousForOtherEnd.setAt<uint16_t>(0,(uint16_t)(rendezvousForOtherEnd.size() - 2));
										_send(fromMemberId,rendezvousForOtherEnd.data(),rendezvousForOtherEnd.size());
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

void Cluster::replicateHavePeer(const Address &peerAddress)
{
}

void Cluster::replicateMulticastLike(uint64_t nwid,const Address &peerAddress,const MulticastGroup &group)
{
}

void Cluster::replicateCertificateOfNetworkMembership(const CertificateOfMembership &com)
{
}

void Cluster::doPeriodicTasks()
{
	// Go ahead and flush whenever possible right now
	{
		Mutex::Lock _l(_memberIds_m);
		for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
			Mutex::Lock _l2(_members[*mid].lock);
			_flush(*mid);
		}
	}
}

void Cluster::addMember(uint16_t memberId)
{
	Mutex::Lock _l2(_members[memberId].lock);

	Mutex::Lock _l(_memberIds_m);
	_memberIds.push_back(memberId);
	std::sort(_memberIds.begin(),_memberIds.end());

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
}

void Cluster::_send(uint16_t memberId,const void *msg,unsigned int len)
{
	_Member &m = _members[memberId];
	// assumes m.lock is locked!
	for(;;) {
		if ((m.q.size() + len) > ZT_CLUSTER_MAX_MESSAGE_LENGTH)
			_flush(memberId);
		else {
			m.q.append(msg,len);
			break;
		}
	}
}

void Cluster::_flush(uint16_t memberId)
{
	_Member &m = _members[memberId];
	// assumes m.lock is locked!
	if (m.q.size() > 24) {
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
		_sendFunction(_arg,memberId,m.q.data(),m.q.size());

		// Prepare for more
		m.q.clear();
		char iv[16];
		Utils::getSecureRandom(iv,16);
		m.q.append(iv,16);
		m.q.addSize(8); // room for MAC
	}
}

} // namespace ZeroTier

#endif // ZT_ENABLE_CLUSTER
