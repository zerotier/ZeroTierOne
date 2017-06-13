/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifdef ZT_ENABLE_CLUSTER

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <map>
#include <algorithm>
#include <set>
#include <utility>
#include <list>
#include <stdexcept>

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
#include "Node.hpp"
#include "Network.hpp"
#include "Array.hpp"

namespace ZeroTier {

static inline double _dist3d(int x1,int y1,int z1,int x2,int y2,int z2)
	throw()
{
	double dx = ((double)x2 - (double)x1);
	double dy = ((double)y2 - (double)y1);
	double dz = ((double)z2 - (double)z1);
	return sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

// An entry in _ClusterSendQueue
struct _ClusterSendQueueEntry
{
	uint64_t timestamp;
	Address fromPeerAddress;
	Address toPeerAddress;
	// if we ever support larger transport MTUs this must be increased
	unsigned char data[ZT_CLUSTER_SEND_QUEUE_DATA_MAX];
	unsigned int len;
	bool unite;
};

// A multi-index map with entry memory pooling -- this allows our queue to
// be O(log(N)) and is complex enough that it makes the code a lot cleaner
// to break it out from Cluster.
class _ClusterSendQueue
{
public:
	_ClusterSendQueue() :
		_poolCount(0) {}
	~_ClusterSendQueue() {} // memory is automatically freed when _chunks is destroyed

	inline void enqueue(uint64_t now,const Address &from,const Address &to,const void *data,unsigned int len,bool unite)
	{
		if (len > ZT_CLUSTER_SEND_QUEUE_DATA_MAX)
			return;

		Mutex::Lock _l(_lock);

		// Delete oldest queue entry for this sender if this enqueue() would take them over the per-sender limit
		{
			std::set< std::pair<Address,_ClusterSendQueueEntry *> >::iterator qi(_bySrc.lower_bound(std::pair<Address,_ClusterSendQueueEntry *>(from,(_ClusterSendQueueEntry *)0)));
			std::set< std::pair<Address,_ClusterSendQueueEntry *> >::iterator oldest(qi);
			unsigned long countForSender = 0;
			while ((qi != _bySrc.end())&&(qi->first == from)) {
				if (qi->second->timestamp < oldest->second->timestamp)
					oldest = qi;
				++countForSender;
				++qi;
			}
			if (countForSender >= ZT_CLUSTER_MAX_QUEUE_PER_SENDER) {
				_byDest.erase(std::pair<Address,_ClusterSendQueueEntry *>(oldest->second->toPeerAddress,oldest->second));
				_pool[_poolCount++] = oldest->second;
				_bySrc.erase(oldest);
			}
		}

		_ClusterSendQueueEntry *e;
		if (_poolCount > 0) {
			e = _pool[--_poolCount];
		} else {
			if (_chunks.size() >= ZT_CLUSTER_MAX_QUEUE_CHUNKS)
				return; // queue is totally full!
			_chunks.push_back(Array<_ClusterSendQueueEntry,ZT_CLUSTER_QUEUE_CHUNK_SIZE>());
			e = &(_chunks.back().data[0]);
			for(unsigned int i=1;i<ZT_CLUSTER_QUEUE_CHUNK_SIZE;++i)
				_pool[_poolCount++] = &(_chunks.back().data[i]);
		}

		e->timestamp = now;
		e->fromPeerAddress = from;
		e->toPeerAddress = to;
		memcpy(e->data,data,len);
		e->len = len;
		e->unite = unite;

		_bySrc.insert(std::pair<Address,_ClusterSendQueueEntry *>(from,e));
		_byDest.insert(std::pair<Address,_ClusterSendQueueEntry *>(to,e));
	}

	inline void expire(uint64_t now)
	{
		Mutex::Lock _l(_lock);
		for(std::set< std::pair<Address,_ClusterSendQueueEntry *> >::iterator qi(_bySrc.begin());qi!=_bySrc.end();) {
			if ((now - qi->second->timestamp) > ZT_CLUSTER_QUEUE_EXPIRATION) {
				_byDest.erase(std::pair<Address,_ClusterSendQueueEntry *>(qi->second->toPeerAddress,qi->second));
				_pool[_poolCount++] = qi->second;
				_bySrc.erase(qi++);
			} else ++qi;
		}
	}

	/**
	 * Get and dequeue entries for a given destination address
	 *
	 * After use these entries must be returned with returnToPool()!
	 *
	 * @param dest Destination address
	 * @param results Array to fill with results
	 * @param maxResults Size of results[] in pointers
	 * @return Number of actual results returned
	 */
	inline unsigned int getByDest(const Address &dest,_ClusterSendQueueEntry **results,unsigned int maxResults)
	{
		unsigned int count = 0;
		Mutex::Lock _l(_lock);
		std::set< std::pair<Address,_ClusterSendQueueEntry *> >::iterator qi(_byDest.lower_bound(std::pair<Address,_ClusterSendQueueEntry *>(dest,(_ClusterSendQueueEntry *)0)));
		while ((qi != _byDest.end())&&(qi->first == dest)) {
			_bySrc.erase(std::pair<Address,_ClusterSendQueueEntry *>(qi->second->fromPeerAddress,qi->second));
			results[count++] = qi->second;
			if (count == maxResults)
				break;
			_byDest.erase(qi++);
		}
		return count;
	}

	/**
	 * Return entries to pool after use
	 *
	 * @param entries Array of entries
	 * @param count Number of entries
	 */
	inline void returnToPool(_ClusterSendQueueEntry **entries,unsigned int count)
	{
		Mutex::Lock _l(_lock);
		for(unsigned int i=0;i<count;++i)
			_pool[_poolCount++] = entries[i];
	}

private:
	std::list< Array<_ClusterSendQueueEntry,ZT_CLUSTER_QUEUE_CHUNK_SIZE> > _chunks;
	_ClusterSendQueueEntry *_pool[ZT_CLUSTER_QUEUE_CHUNK_SIZE * ZT_CLUSTER_MAX_QUEUE_CHUNKS];
	unsigned long _poolCount;
	std::set< std::pair<Address,_ClusterSendQueueEntry *> > _bySrc;
	std::set< std::pair<Address,_ClusterSendQueueEntry *> > _byDest;
	Mutex _lock;
};

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
	_sendQueue(new _ClusterSendQueue()),
	_sendFunction(sendFunction),
	_sendFunctionArg(sendFunctionArg),
	_addressToLocationFunction(addressToLocationFunction),
	_addressToLocationFunctionArg(addressToLocationFunctionArg),
	_x(x),
	_y(y),
	_z(z),
	_id(id),
	_zeroTierPhysicalEndpoints(zeroTierPhysicalEndpoints),
	_members(new _Member[ZT_CLUSTER_MAX_MEMBERS]),
	_lastFlushed(0),
	_lastCleanedRemotePeers(0),
	_lastCleanedQueue(0)
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
	delete _sendQueue;
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
		Salsa20 s20(keytmp,reinterpret_cast<const char *>(msg) + 8);
		Utils::burn(keytmp,sizeof(keytmp));

		// One-time-use Poly1305 key from first 32 bytes of Salsa20 keystream (as per DJB/NaCl "standard")
		char polykey[ZT_POLY1305_KEY_LEN];
		memset(polykey,0,sizeof(polykey));
		s20.crypt12(polykey,polykey,sizeof(polykey));

		// Compute 16-byte MAC
		char mac[ZT_POLY1305_MAC_LEN];
		Poly1305::compute(mac,reinterpret_cast<const char *>(msg) + 24,len - 24,polykey);

		// Check first 8 bytes of MAC against 64-bit MAC in stream
		if (!Utils::secureEq(mac,reinterpret_cast<const char *>(msg) + 16,8))
			return;

		// Decrypt!
		dmsg.setSize(len - 24);
		s20.crypt12(reinterpret_cast<const char *>(msg) + 24,const_cast<void *>(dmsg.data()),dmsg.size());
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

					case CLUSTER_MESSAGE_ALIVE: {
						_Member &m = _members[fromMemberId];
						Mutex::Lock mlck(m.lock);
						ptr += 7; // skip version stuff, not used yet
						m.x = dmsg.at<int32_t>(ptr); ptr += 4;
						m.y = dmsg.at<int32_t>(ptr); ptr += 4;
						m.z = dmsg.at<int32_t>(ptr); ptr += 4;
						ptr += 8; // skip local clock, not used
						m.load = dmsg.at<uint64_t>(ptr); ptr += 8;
						m.peers = dmsg.at<uint64_t>(ptr); ptr += 8;
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

					case CLUSTER_MESSAGE_HAVE_PEER: {
						Identity id;
						ptr += id.deserialize(dmsg,ptr);
						if (id) {
							{
								Mutex::Lock _l(_remotePeers_m);
								_RemotePeer &rp = _remotePeers[std::pair<Address,unsigned int>(id.address(),(unsigned int)fromMemberId)];
								if (!rp.lastHavePeerReceived) {
									RR->topology->saveIdentity((void *)0,id);
									RR->identity.agree(id,rp.key,ZT_PEER_SECRET_KEY_LENGTH);
								}
								rp.lastHavePeerReceived = RR->node->now();
							}

							_ClusterSendQueueEntry *q[16384]; // 16384 is "tons"
							unsigned int qc = _sendQueue->getByDest(id.address(),q,16384);
							for(unsigned int i=0;i<qc;++i)
								this->relayViaCluster(q[i]->fromPeerAddress,q[i]->toPeerAddress,q[i]->data,q[i]->len,q[i]->unite);
							_sendQueue->returnToPool(q,qc);

							TRACE("[%u] has %s (retried %u queued sends)",(unsigned int)fromMemberId,id.address().toString().c_str(),qc);
						}
					}	break;

					case CLUSTER_MESSAGE_WANT_PEER: {
						const Address zeroTierAddress(dmsg.field(ptr,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); ptr += ZT_ADDRESS_LENGTH;
						SharedPtr<Peer> peer(RR->topology->getPeerNoCache(zeroTierAddress));
						if ( (peer) && (peer->hasLocalClusterOptimalPath(RR->node->now())) ) {
							Buffer<1024> buf;
							peer->identity().serialize(buf);
							Mutex::Lock _l2(_members[fromMemberId].lock);
							_send(fromMemberId,CLUSTER_MESSAGE_HAVE_PEER,buf.data(),buf.size());
						}
					}	break;

					case CLUSTER_MESSAGE_REMOTE_PACKET: {
						const unsigned int plen = dmsg.at<uint16_t>(ptr); ptr += 2;
						if (plen) {
							Packet remotep(dmsg.field(ptr,plen),plen); ptr += plen;
							//TRACE("remote %s from %s via %u (%u bytes)",Packet::verbString(remotep.verb()),remotep.source().toString().c_str(),fromMemberId,plen);
							switch(remotep.verb()) {
								case Packet::VERB_WHOIS:            _doREMOTE_WHOIS(fromMemberId,remotep); break;
								case Packet::VERB_MULTICAST_GATHER: _doREMOTE_MULTICAST_GATHER(fromMemberId,remotep); break;
								default: break; // ignore things we don't care about across cluster
							}
						}
					}	break;

					case CLUSTER_MESSAGE_PROXY_UNITE: {
						const Address localPeerAddress(dmsg.field(ptr,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); ptr += ZT_ADDRESS_LENGTH;
						const Address remotePeerAddress(dmsg.field(ptr,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); ptr += ZT_ADDRESS_LENGTH;
						const unsigned int numRemotePeerPaths = dmsg[ptr++];
						InetAddress remotePeerPaths[256]; // size is 8-bit, so 256 is max
						for(unsigned int i=0;i<numRemotePeerPaths;++i)
							ptr += remotePeerPaths[i].deserialize(dmsg,ptr);

						TRACE("[%u] requested that we unite local %s with remote %s",(unsigned int)fromMemberId,localPeerAddress.toString().c_str(),remotePeerAddress.toString().c_str());

						const uint64_t now = RR->node->now();
						SharedPtr<Peer> localPeer(RR->topology->getPeerNoCache(localPeerAddress));
						if ((localPeer)&&(numRemotePeerPaths > 0)) {
							InetAddress bestLocalV4,bestLocalV6;
							localPeer->getRendezvousAddresses(now,bestLocalV4,bestLocalV6);

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

							Packet rendezvousForLocal(localPeerAddress,RR->identity.address(),Packet::VERB_RENDEZVOUS);
							rendezvousForLocal.append((uint8_t)0);
							remotePeerAddress.appendTo(rendezvousForLocal);

							Buffer<2048> rendezvousForRemote;
							remotePeerAddress.appendTo(rendezvousForRemote);
							rendezvousForRemote.append((uint8_t)Packet::VERB_RENDEZVOUS);
							rendezvousForRemote.addSize(2); // space for actual packet payload length
							rendezvousForRemote.append((uint8_t)0); // flags == 0
							localPeerAddress.appendTo(rendezvousForRemote);

							bool haveMatch = false;
							if ((bestLocalV6)&&(bestRemoteV6)) {
								haveMatch = true;

								rendezvousForLocal.append((uint16_t)bestRemoteV6.port());
								rendezvousForLocal.append((uint8_t)16);
								rendezvousForLocal.append(bestRemoteV6.rawIpData(),16);

								rendezvousForRemote.append((uint16_t)bestLocalV6.port());
								rendezvousForRemote.append((uint8_t)16);
								rendezvousForRemote.append(bestLocalV6.rawIpData(),16);
								rendezvousForRemote.setAt<uint16_t>(ZT_ADDRESS_LENGTH + 1,(uint16_t)(9 + 16));
							} else if ((bestLocalV4)&&(bestRemoteV4)) {
								haveMatch = true;

								rendezvousForLocal.append((uint16_t)bestRemoteV4.port());
								rendezvousForLocal.append((uint8_t)4);
								rendezvousForLocal.append(bestRemoteV4.rawIpData(),4);

								rendezvousForRemote.append((uint16_t)bestLocalV4.port());
								rendezvousForRemote.append((uint8_t)4);
								rendezvousForRemote.append(bestLocalV4.rawIpData(),4);
								rendezvousForRemote.setAt<uint16_t>(ZT_ADDRESS_LENGTH + 1,(uint16_t)(9 + 4));
							}

							if (haveMatch) {
								{
									Mutex::Lock _l2(_members[fromMemberId].lock);
									_send(fromMemberId,CLUSTER_MESSAGE_PROXY_SEND,rendezvousForRemote.data(),rendezvousForRemote.size());
								}
								RR->sw->send((void *)0,rendezvousForLocal,true);
							}
						}
					}	break;

					case CLUSTER_MESSAGE_PROXY_SEND: {
						const Address rcpt(dmsg.field(ptr,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); ptr += ZT_ADDRESS_LENGTH;
						const Packet::Verb verb = (Packet::Verb)dmsg[ptr++];
						const unsigned int len = dmsg.at<uint16_t>(ptr); ptr += 2;
						Packet outp(rcpt,RR->identity.address(),verb);
						outp.append(dmsg.field(ptr,len),len); ptr += len;
						RR->sw->send((void *)0,outp,true);
						//TRACE("[%u] proxy send %s to %s length %u",(unsigned int)fromMemberId,Packet::verbString(verb),rcpt.toString().c_str(),len);
					}	break;

					case CLUSTER_MESSAGE_NETWORK_CONFIG: {
						const SharedPtr<Network> network(RR->node->network(dmsg.at<uint64_t>(ptr)));
						if (network) {
							// Copy into a Packet just to conform to Network API. Eventually
							// will want to refactor.
							network->handleConfigChunk((void *)0,0,Address(),Buffer<ZT_PROTO_MAX_PACKET_LENGTH>(dmsg),ptr);
						}
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

void Cluster::broadcastHavePeer(const Identity &id)
{
	Buffer<1024> buf;
	id.serialize(buf);
	Mutex::Lock _l(_memberIds_m);
	for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
		Mutex::Lock _l2(_members[*mid].lock);
		_send(*mid,CLUSTER_MESSAGE_HAVE_PEER,buf.data(),buf.size());
	}
}

void Cluster::broadcastNetworkConfigChunk(const void *chunk,unsigned int len)
{
	Mutex::Lock _l(_memberIds_m);
	for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
		Mutex::Lock _l2(_members[*mid].lock);
		_send(*mid,CLUSTER_MESSAGE_NETWORK_CONFIG,chunk,len);
	}
}

int Cluster::checkSendViaCluster(const Address &toPeerAddress,uint64_t &mostRecentTs,void *peerSecret)
{
	const uint64_t now = RR->node->now();
	mostRecentTs = 0;
	int mostRecentMemberId = -1;
	{
		Mutex::Lock _l2(_remotePeers_m);
		std::map< std::pair<Address,unsigned int>,_RemotePeer >::const_iterator rpe(_remotePeers.lower_bound(std::pair<Address,unsigned int>(toPeerAddress,0)));
		for(;;) {
			if ((rpe == _remotePeers.end())||(rpe->first.first != toPeerAddress))
				break;
			else if (rpe->second.lastHavePeerReceived > mostRecentTs) {
				mostRecentTs = rpe->second.lastHavePeerReceived;
				memcpy(peerSecret,rpe->second.key,ZT_PEER_SECRET_KEY_LENGTH);
				mostRecentMemberId = (int)rpe->first.second;
			}
			++rpe;
		}
	}

	const uint64_t ageOfMostRecentHavePeerAnnouncement = now - mostRecentTs;
	if (ageOfMostRecentHavePeerAnnouncement >= (ZT_PEER_ACTIVITY_TIMEOUT / 3)) {
		if (ageOfMostRecentHavePeerAnnouncement >= ZT_PEER_ACTIVITY_TIMEOUT)
			mostRecentMemberId = -1;

		bool sendWantPeer = true;
		{
			Mutex::Lock _l(_remotePeers_m);
			_RemotePeer &rp = _remotePeers[std::pair<Address,unsigned int>(toPeerAddress,(unsigned int)_id)];
			if ((now - rp.lastSentWantPeer) >= ZT_CLUSTER_WANT_PEER_EVERY) {
				rp.lastSentWantPeer = now;
			} else {
				sendWantPeer = false; // don't flood WANT_PEER
			}
		}
		if (sendWantPeer) {
			char tmp[ZT_ADDRESS_LENGTH];
			toPeerAddress.copyTo(tmp,ZT_ADDRESS_LENGTH);
			{
				Mutex::Lock _l(_memberIds_m);
				for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
					Mutex::Lock _l2(_members[*mid].lock);
					_send(*mid,CLUSTER_MESSAGE_WANT_PEER,tmp,ZT_ADDRESS_LENGTH);
				}
			}
		}
	}

	return mostRecentMemberId;
}

bool Cluster::sendViaCluster(int mostRecentMemberId,const Address &toPeerAddress,const void *data,unsigned int len)
{
	if ((mostRecentMemberId < 0)||(mostRecentMemberId >= ZT_CLUSTER_MAX_MEMBERS)) // sanity check
		return false;
	Mutex::Lock _l2(_members[mostRecentMemberId].lock);
	for(std::vector<InetAddress>::const_iterator i1(_zeroTierPhysicalEndpoints.begin());i1!=_zeroTierPhysicalEndpoints.end();++i1) {
		for(std::vector<InetAddress>::const_iterator i2(_members[mostRecentMemberId].zeroTierPhysicalEndpoints.begin());i2!=_members[mostRecentMemberId].zeroTierPhysicalEndpoints.end();++i2) {
			if (i1->ss_family == i2->ss_family) {
				TRACE("sendViaCluster sending %u bytes to %s by way of %u (%s->%s)",len,toPeerAddress.toString().c_str(),(unsigned int)mostRecentMemberId,i1->toString().c_str(),i2->toString().c_str());
				RR->node->putPacket((void *)0,*i1,*i2,data,len);
				return true;
			}
		}
	}
	return false;
}

void Cluster::relayViaCluster(const Address &fromPeerAddress,const Address &toPeerAddress,const void *data,unsigned int len,bool unite)
{
	if (len > ZT_PROTO_MAX_PACKET_LENGTH) // sanity check
		return;

	const uint64_t now = RR->node->now();

	uint64_t mostRecentTs = 0;
	int mostRecentMemberId = -1;
	{
		Mutex::Lock _l2(_remotePeers_m);
		std::map< std::pair<Address,unsigned int>,_RemotePeer >::const_iterator rpe(_remotePeers.lower_bound(std::pair<Address,unsigned int>(toPeerAddress,0)));
		for(;;) {
			if ((rpe == _remotePeers.end())||(rpe->first.first != toPeerAddress))
				break;
			else if (rpe->second.lastHavePeerReceived > mostRecentTs) {
				mostRecentTs = rpe->second.lastHavePeerReceived;
				mostRecentMemberId = (int)rpe->first.second;
			}
			++rpe;
		}
	}

	const uint64_t ageOfMostRecentHavePeerAnnouncement = now - mostRecentTs;
	if (ageOfMostRecentHavePeerAnnouncement >= (ZT_PEER_ACTIVITY_TIMEOUT / 3)) {
		// Enqueue and wait if peer seems alive, but do WANT_PEER to refresh homing
		const bool enqueueAndWait = ((ageOfMostRecentHavePeerAnnouncement >= ZT_PEER_ACTIVITY_TIMEOUT)||(mostRecentMemberId < 0));

		// Poll everyone with WANT_PEER if the age of our most recent entry is
		// approaching expiration (or has expired, or does not exist).
		bool sendWantPeer = true;
		{
			Mutex::Lock _l(_remotePeers_m);
			_RemotePeer &rp = _remotePeers[std::pair<Address,unsigned int>(toPeerAddress,(unsigned int)_id)];
			if ((now - rp.lastSentWantPeer) >= ZT_CLUSTER_WANT_PEER_EVERY) {
				rp.lastSentWantPeer = now;
			} else {
				sendWantPeer = false; // don't flood WANT_PEER
			}
		}
		if (sendWantPeer) {
			char tmp[ZT_ADDRESS_LENGTH];
			toPeerAddress.copyTo(tmp,ZT_ADDRESS_LENGTH);
			{
				Mutex::Lock _l(_memberIds_m);
				for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
					Mutex::Lock _l2(_members[*mid].lock);
					_send(*mid,CLUSTER_MESSAGE_WANT_PEER,tmp,ZT_ADDRESS_LENGTH);
				}
			}
		}

		// If there isn't a good place to send via, then enqueue this for retrying
		// later and return after having broadcasted a WANT_PEER.
		if (enqueueAndWait) {
			TRACE("relayViaCluster %s -> %s enqueueing to wait for HAVE_PEER",fromPeerAddress.toString().c_str(),toPeerAddress.toString().c_str());
			_sendQueue->enqueue(now,fromPeerAddress,toPeerAddress,data,len,unite);
			return;
		}
	}

	if (mostRecentMemberId >= 0) {
		Buffer<1024> buf;
		if (unite) {
			InetAddress v4,v6;
			if (fromPeerAddress) {
				SharedPtr<Peer> fromPeer(RR->topology->getPeerNoCache(fromPeerAddress));
				if (fromPeer)
					fromPeer->getRendezvousAddresses(now,v4,v6);
			}
			uint8_t addrCount = 0;
			if (v4)
				++addrCount;
			if (v6)
				++addrCount;
			if (addrCount) {
				toPeerAddress.appendTo(buf);
				fromPeerAddress.appendTo(buf);
				buf.append(addrCount);
				if (v4)
					v4.serialize(buf);
				if (v6)
					v6.serialize(buf);
			}
		}

		{
			Mutex::Lock _l2(_members[mostRecentMemberId].lock);
			if (buf.size() > 0)
				_send(mostRecentMemberId,CLUSTER_MESSAGE_PROXY_UNITE,buf.data(),buf.size());

			for(std::vector<InetAddress>::const_iterator i1(_zeroTierPhysicalEndpoints.begin());i1!=_zeroTierPhysicalEndpoints.end();++i1) {
				for(std::vector<InetAddress>::const_iterator i2(_members[mostRecentMemberId].zeroTierPhysicalEndpoints.begin());i2!=_members[mostRecentMemberId].zeroTierPhysicalEndpoints.end();++i2) {
					if (i1->ss_family == i2->ss_family) {
						TRACE("relayViaCluster relaying %u bytes from %s to %s by way of %u (%s->%s)",len,fromPeerAddress.toString().c_str(),toPeerAddress.toString().c_str(),(unsigned int)mostRecentMemberId,i1->toString().c_str(),i2->toString().c_str());
						RR->node->putPacket((void *)0,*i1,*i2,data,len);
						return;
					}
				}
			}

			TRACE("relayViaCluster relaying %u bytes from %s to %s by way of %u failed: no common endpoints with the same address family!",len,fromPeerAddress.toString().c_str(),toPeerAddress.toString().c_str(),(unsigned int)mostRecentMemberId);
		}
	}
}

void Cluster::sendDistributedQuery(const Packet &pkt)
{
	Buffer<4096> buf;
	buf.append((uint16_t)pkt.size());
	buf.append(pkt.data(),pkt.size());
	Mutex::Lock _l(_memberIds_m);
	for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
		Mutex::Lock _l2(_members[*mid].lock);
		_send(*mid,CLUSTER_MESSAGE_REMOTE_PACKET,buf.data(),buf.size());
	}
}

void Cluster::doPeriodicTasks()
{
	const uint64_t now = RR->node->now();

	if ((now - _lastFlushed) >= ZT_CLUSTER_FLUSH_PERIOD) {
		_lastFlushed = now;

		Mutex::Lock _l(_memberIds_m);
		for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
			Mutex::Lock _l2(_members[*mid].lock);

			if ((now - _members[*mid].lastAnnouncedAliveTo) >= ((ZT_CLUSTER_TIMEOUT / 2) - 1000)) {
				_members[*mid].lastAnnouncedAliveTo = now;

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
				alive.append((uint64_t)RR->topology->countActive(now));
				alive.append((uint64_t)0); // unused/reserved flags
				alive.append((uint8_t)_zeroTierPhysicalEndpoints.size());
				for(std::vector<InetAddress>::const_iterator pe(_zeroTierPhysicalEndpoints.begin());pe!=_zeroTierPhysicalEndpoints.end();++pe)
					pe->serialize(alive);
				_send(*mid,CLUSTER_MESSAGE_ALIVE,alive.data(),alive.size());
			}

			_flush(*mid);
		}
	}

	if ((now - _lastCleanedRemotePeers) >= (ZT_PEER_ACTIVITY_TIMEOUT * 2)) {
		_lastCleanedRemotePeers = now;

		Mutex::Lock _l(_remotePeers_m);
		for(std::map< std::pair<Address,unsigned int>,_RemotePeer >::iterator rp(_remotePeers.begin());rp!=_remotePeers.end();) {
			if ((now - rp->second.lastHavePeerReceived) >= ZT_PEER_ACTIVITY_TIMEOUT)
				_remotePeers.erase(rp++);
			else ++rp;
		}
	}

	if ((now - _lastCleanedQueue) >= ZT_CLUSTER_QUEUE_EXPIRATION) {
		_lastCleanedQueue = now;
		_sendQueue->expire(now);
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

bool Cluster::findBetterEndpoint(InetAddress &redirectTo,const Address &peerAddress,const InetAddress &peerPhysicalAddress,bool offload)
{
	if (_addressToLocationFunction) {
		// Pick based on location if it can be determined
		int px = 0,py = 0,pz = 0;
		if (_addressToLocationFunction(_addressToLocationFunctionArg,reinterpret_cast<const struct sockaddr_storage *>(&peerPhysicalAddress),&px,&py,&pz) == 0) {
			TRACE("no geolocation data for %s",peerPhysicalAddress.toIpString().c_str());
			return false;
		}

		// Find member closest to this peer
		const uint64_t now = RR->node->now();
		std::vector<InetAddress> best;
		const double currentDistance = _dist3d(_x,_y,_z,px,py,pz);
		double bestDistance = (offload ? 2147483648.0 : currentDistance);
#ifdef ZT_TRACE
		unsigned int bestMember = _id;
#endif
		{
			Mutex::Lock _l(_memberIds_m);
			for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
				_Member &m = _members[*mid];
				Mutex::Lock _ml(m.lock);

				// Consider member if it's alive and has sent us a location and one or more physical endpoints to send peers to
				if ( ((now - m.lastReceivedAliveAnnouncement) < ZT_CLUSTER_TIMEOUT) && ((m.x != 0)||(m.y != 0)||(m.z != 0)) && (m.zeroTierPhysicalEndpoints.size() > 0) ) {
					const double mdist = _dist3d(m.x,m.y,m.z,px,py,pz);
					if (mdist < bestDistance) {
						bestDistance = mdist;
#ifdef ZT_TRACE
						bestMember = *mid;
#endif
						best = m.zeroTierPhysicalEndpoints;
					}
				}
			}
		}

		// Redirect to a closer member if it has a ZeroTier endpoint address in the same ss_family
		for(std::vector<InetAddress>::const_iterator a(best.begin());a!=best.end();++a) {
			if (a->ss_family == peerPhysicalAddress.ss_family) {
				TRACE("%s at [%d,%d,%d] is %f from us but %f from %u, can redirect to %s",peerAddress.toString().c_str(),px,py,pz,currentDistance,bestDistance,bestMember,a->toString().c_str());
				redirectTo = *a;
				return true;
			}
		}
		TRACE("%s at [%d,%d,%d] is %f from us, no better endpoints found",peerAddress.toString().c_str(),px,py,pz,currentDistance);
		return false;
	} else {
		// TODO: pick based on load if no location info?
		return false;
	}
}

bool Cluster::isClusterPeerFrontplane(const InetAddress &ip) const
{
	Mutex::Lock _l(_memberIds_m);
	for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
		Mutex::Lock _l2(_members[*mid].lock);
		for(std::vector<InetAddress>::const_iterator i2(_members[*mid].zeroTierPhysicalEndpoints.begin());i2!=_members[*mid].zeroTierPhysicalEndpoints.end();++i2) {
			if (ip == *i2)
				return true;
		}
	}
	return false;
}

void Cluster::status(ZT_ClusterStatus &status) const
{
	const uint64_t now = RR->node->now();
	memset(&status,0,sizeof(ZT_ClusterStatus));

	status.myId = _id;

	{
		ZT_ClusterMemberStatus *const s = &(status.members[status.clusterSize++]);
		s->id = _id;
		s->alive = 1;
		s->x = _x;
		s->y = _y;
		s->z = _z;
		s->load = 0; // TODO
		s->peers = RR->topology->countActive(now);
		for(std::vector<InetAddress>::const_iterator ep(_zeroTierPhysicalEndpoints.begin());ep!=_zeroTierPhysicalEndpoints.end();++ep) {
			if (s->numZeroTierPhysicalEndpoints >= ZT_CLUSTER_MAX_ZT_PHYSICAL_ADDRESSES) // sanity check
				break;
			memcpy(&(s->zeroTierPhysicalEndpoints[s->numZeroTierPhysicalEndpoints++]),&(*ep),sizeof(struct sockaddr_storage));
		}
	}

	{
		Mutex::Lock _l1(_memberIds_m);
		for(std::vector<uint16_t>::const_iterator mid(_memberIds.begin());mid!=_memberIds.end();++mid) {
			if (status.clusterSize >= ZT_CLUSTER_MAX_MEMBERS) // sanity check
				break;

			_Member &m = _members[*mid];
			Mutex::Lock ml(m.lock);

			ZT_ClusterMemberStatus *const s = &(status.members[status.clusterSize++]);
			s->id = *mid;
			s->msSinceLastHeartbeat = (unsigned int)std::min((uint64_t)(~((unsigned int)0)),(now - m.lastReceivedAliveAnnouncement));
			s->alive = (s->msSinceLastHeartbeat < ZT_CLUSTER_TIMEOUT) ? 1 : 0;
			s->x = m.x;
			s->y = m.y;
			s->z = m.z;
			s->load = m.load;
			s->peers = m.peers;
			for(std::vector<InetAddress>::const_iterator ep(m.zeroTierPhysicalEndpoints.begin());ep!=m.zeroTierPhysicalEndpoints.end();++ep) {
				if (s->numZeroTierPhysicalEndpoints >= ZT_CLUSTER_MAX_ZT_PHYSICAL_ADDRESSES) // sanity check
					break;
				memcpy(&(s->zeroTierPhysicalEndpoints[s->numZeroTierPhysicalEndpoints++]),&(*ep),sizeof(struct sockaddr_storage));
			}
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
		Salsa20 s20(keytmp,m.q.field(8,8));
		Utils::burn(keytmp,sizeof(keytmp));

		// One-time-use Poly1305 key from first 32 bytes of Salsa20 keystream (as per DJB/NaCl "standard")
		char polykey[ZT_POLY1305_KEY_LEN];
		memset(polykey,0,sizeof(polykey));
		s20.crypt12(polykey,polykey,sizeof(polykey));

		// Encrypt m.q in place
		s20.crypt12(reinterpret_cast<const char *>(m.q.data()) + 24,const_cast<char *>(reinterpret_cast<const char *>(m.q.data())) + 24,m.q.size() - 24);

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

void Cluster::_doREMOTE_WHOIS(uint64_t fromMemberId,const Packet &remotep)
{
	if (remotep.payloadLength() >= ZT_ADDRESS_LENGTH) {
		Identity queried(RR->topology->getIdentity((void *)0,Address(remotep.payload(),ZT_ADDRESS_LENGTH)));
		if (queried) {
			Buffer<1024> routp;
			remotep.source().appendTo(routp);
			routp.append((uint8_t)Packet::VERB_OK);
			routp.addSize(2); // space for length
			routp.append((uint8_t)Packet::VERB_WHOIS);
			routp.append(remotep.packetId());
			queried.serialize(routp);
			routp.setAt<uint16_t>(ZT_ADDRESS_LENGTH + 1,(uint16_t)(routp.size() - ZT_ADDRESS_LENGTH - 3));

			TRACE("responding to remote WHOIS from %s @ %u with identity of %s",remotep.source().toString().c_str(),(unsigned int)fromMemberId,queried.address().toString().c_str());
			Mutex::Lock _l2(_members[fromMemberId].lock);
			_send(fromMemberId,CLUSTER_MESSAGE_PROXY_SEND,routp.data(),routp.size());
		}
	}
}

void Cluster::_doREMOTE_MULTICAST_GATHER(uint64_t fromMemberId,const Packet &remotep)
{
	const uint64_t nwid = remotep.at<uint64_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_NETWORK_ID);
	const MulticastGroup mg(MAC(remotep.field(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_MAC,6),6),remotep.at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_ADI));
	unsigned int gatherLimit = remotep.at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_GATHER_LIMIT);
	const Address remotePeerAddress(remotep.source());

	if (gatherLimit) {
		Buffer<ZT_PROTO_MAX_PACKET_LENGTH> routp;
		remotePeerAddress.appendTo(routp);
		routp.append((uint8_t)Packet::VERB_OK);
		routp.addSize(2); // space for length
		routp.append((uint8_t)Packet::VERB_MULTICAST_GATHER);
		routp.append(remotep.packetId());
		routp.append(nwid);
		mg.mac().appendTo(routp);
		routp.append((uint32_t)mg.adi());

		if (gatherLimit > ((ZT_CLUSTER_MAX_MESSAGE_LENGTH - 80) / 5))
			gatherLimit = ((ZT_CLUSTER_MAX_MESSAGE_LENGTH - 80) / 5);
		if (RR->mc->gather(remotePeerAddress,nwid,mg,routp,gatherLimit)) {
			routp.setAt<uint16_t>(ZT_ADDRESS_LENGTH + 1,(uint16_t)(routp.size() - ZT_ADDRESS_LENGTH - 3));

			TRACE("responding to remote MULTICAST_GATHER from %s @ %u with %u bytes",remotePeerAddress.toString().c_str(),(unsigned int)fromMemberId,routp.size());
			Mutex::Lock _l2(_members[fromMemberId].lock);
			_send(fromMemberId,CLUSTER_MESSAGE_PROXY_SEND,routp.data(),routp.size());
		}
	}
}

} // namespace ZeroTier

#endif // ZT_ENABLE_CLUSTER
