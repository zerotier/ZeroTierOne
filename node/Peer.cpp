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

#include "Constants.hpp"
#include "Peer.hpp"
#include "Node.hpp"
#include "Switch.hpp"
#include "Network.hpp"
#include "SelfAwareness.hpp"
#include "Packet.hpp"
#include "Trace.hpp"
#include "InetAddress.hpp"

namespace ZeroTier {

struct _PathPriorityComparisonOperator
{
	ZT_ALWAYS_INLINE bool operator()(const SharedPtr<Path> &a,const SharedPtr<Path> &b) const
	{
		return ( ((a)&&(a->lastIn() > 0)) && ((!b)||(b->lastIn() <= 0)||(a->lastIn() < b->lastIn())) );
	}
};

Peer::Peer(const RuntimeEnvironment *renv,const Identity &myIdentity,const Identity &peerIdentity) :
	RR(renv),
	_lastReceive(0),
	_lastWhoisRequestReceived(0),
	_lastEchoRequestReceived(0),
	_lastPushDirectPathsReceived(0),
	_lastPushDirectPathsSent(0),
	_lastTriedStaticPath(0),
	_lastPrioritizedPaths(0),
	_latency(0xffff),
	_alivePathCount(0),
	_id(peerIdentity),
	_vProto(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0)
{
	if (!myIdentity.agree(peerIdentity,_key))
		throw ZT_EXCEPTION_INVALID_ARGUMENT;
}

void Peer::received(
	void *tPtr,
	const SharedPtr<Path> &path,
	const unsigned int hops,
	const uint64_t packetId,
	const unsigned int payloadLength,
	const Packet::Verb verb,
	const uint64_t inRePacketId,
	const Packet::Verb inReVerb,
	const uint64_t networkId)
{
	const int64_t now = RR->node->now();
	_lastReceive = now;

	if (hops == 0) {
		_paths_l.rlock();
		for(int i=0;i<(int)_alivePathCount; ++i) {
			if (_paths[i] == path) {
				_paths_l.runlock();
				goto path_check_done;
			}
		}
		_paths_l.runlock();

		if (verb == Packet::VERB_OK) {
			RWMutex::Lock l(_paths_l);

			int64_t lastReceiveTimeMax = 0;
			int lastReceiveTimeMaxAt = 0;
			for(int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
				if ((_paths[i]->address().ss_family == path->address().ss_family) &&
				    (_paths[i]->localSocket() == path->localSocket()) && // TODO: should be localInterface when multipath is integrated
				    (_paths[i]->address().ipsEqual2(path->address()))) {
					// If this is another path to the same place, swap it out as the
					// one we just received from may replace an old one but don't
					// learn it as a new path.
					_paths[i] = path;
					goto path_check_done;
				} else {
					if (_paths[i]) {
						if (_paths[i]->lastIn() > lastReceiveTimeMax) {
							lastReceiveTimeMax = _paths[i]->lastIn();
							lastReceiveTimeMaxAt = i;
						}
					} else {
						lastReceiveTimeMax = 0x7fffffffffffffffLL;
						lastReceiveTimeMaxAt = i;
					}
				}
			}

			_lastPrioritizedPaths = now;
			_paths[lastReceiveTimeMaxAt] = path;
			_prioritizePaths(now);
			RR->t->peerLearnedNewPath(tPtr,networkId,*this,path,packetId);
		} else {
			if (RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id.address(),path->localSocket(),path->address())) {
				sendHELLO(tPtr,path->localSocket(),path->address(),now);
				path->sent(now);
				RR->t->peerConfirmingUnknownPath(tPtr,networkId,*this,path,packetId,verb);
			}
		}
	}

path_check_done:
	const int64_t sinceLastPush = now - _lastPushDirectPathsSent;
	if (sinceLastPush >= ((hops == 0) ? ZT_DIRECT_PATH_PUSH_INTERVAL_HAVEPATH : ZT_DIRECT_PATH_PUSH_INTERVAL)) {
		_lastPushDirectPathsReceived = now;
	}

	/*
	const int64_t sinceLastPush = now - _lastDirectPathPushSent;
	if (sinceLastPush >= ((hops == 0) ? ZT_DIRECT_PATH_PUSH_INTERVAL_HAVEPATH : ZT_DIRECT_PATH_PUSH_INTERVAL)) {
		_lastDirectPathPushSent = now;
		std::vector<ZT_InterfaceAddress> pathsToPush(RR->node->directPaths());
		if (pathsToPush.size() > 0) {
			std::vector<ZT_InterfaceAddress>::const_iterator p(pathsToPush.begin());
			while (p != pathsToPush.end()) {
				ScopedPtr<Packet> outp(new Packet(_id.address(),RR->identity.address(),Packet::VERB_PUSH_DIRECT_PATHS));
				outp->addSize(2); // leave room for count
				unsigned int count = 0;
				while ((p != pathsToPush.end())&&((outp->size() + 24) < 1200)) {
					uint8_t addressType = 4;
					uint8_t addressLength = 6;
					unsigned int ipLength = 4;
					const void *rawIpData;
					const void *rawIpPort;
					switch(p->address.ss_family) {
						case AF_INET:
							rawIpData = &(reinterpret_cast<const struct sockaddr_in *>(&(p->address))->sin_addr.s_addr);
							rawIpPort = &(reinterpret_cast<const struct sockaddr_in *>(&(p->address))->sin_port);
							break;
						case AF_INET6:
							rawIpData = reinterpret_cast<const struct sockaddr_in6 *>(&(p->address))->sin6_addr.s6_addr;
							rawIpPort = &(reinterpret_cast<const struct sockaddr_in6 *>(&(p->address))->sin6_port);
							addressType = 6;
							addressLength = 18;
							ipLength = 16;
							break;
						default: // we currently only push IP addresses
							++p;
							continue;
					}

					outp->append((uint8_t)0); // no flags
					outp->append((uint16_t)0); // no extensions
					outp->append(addressType);
					outp->append(addressLength);
					outp->append(rawIpData,ipLength);
					outp->append(rawIpPort,2);

					++count;
					++p;
				}
				if (count) {
					outp->setAt(ZT_PACKET_IDX_PAYLOAD,(uint16_t)count);
					outp->compress();
					outp->armor(_key,true);
					path->send(RR,tPtr,outp->data(),outp->size(),now);
				}
			}
		}
	}
	*/
}

bool Peer::shouldTryPath(void *tPtr,int64_t now,const SharedPtr<Peer> &suggestedBy,const InetAddress &addr) const
{
	int maxHaveScope = -1;
	{
		RWMutex::RLock l(_paths_l);
		for (unsigned int i = 0; i < _alivePathCount; ++i) {
			if (_paths[i]) {
				if (_paths[i]->address().ipsEqual2(addr))
					return false;

				int s = (int)_paths[i]->address().ipScope();
				if (s > maxHaveScope)
					maxHaveScope = s;
			}
		}
	}
	return ( ((int)addr.ipScope() > maxHaveScope) && RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id.address(),-1,addr) );
}

void Peer::sendHELLO(void *tPtr,const int64_t localSocket,const InetAddress &atAddress,int64_t now)
{
	Packet outp(_id.address(),RR->identity.address(),Packet::VERB_HELLO);

	outp.append((unsigned char)ZT_PROTO_VERSION);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MAJOR);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MINOR);
	outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
	outp.append(now);
	RR->identity.serialize(outp,false);
	atAddress.serialize(outp);

	RR->node->expectReplyTo(outp.packetId());

	if (atAddress) {
		outp.armor(_key,false); // false == don't encrypt full payload, but add MAC
		RR->node->putPacket(tPtr,localSocket,atAddress,outp.data(),outp.size());
	} else {
		RR->sw->send(tPtr,outp,false); // false == don't encrypt full payload, but add MAC
	}
}

void Peer::ping(void *tPtr,int64_t now,unsigned int &v4SendCount,unsigned int &v6SendCount,const bool pingAllAddressTypes)
{
	RWMutex::RLock l(_paths_l);

	_lastPrioritizedPaths = now;
	_prioritizePaths(now);

	if (_alivePathCount > 0) {
		for (unsigned int i = 0; i < _alivePathCount; ++i) {
			sendHELLO(tPtr,_paths[i]->localSocket(),_paths[i]->address(),now);
			_paths[i]->sent(now);

			if (_paths[i]->address().isV4())
				++v4SendCount;
			else if (_paths[i]->address().isV6())
				++v6SendCount;

			if (!pingAllAddressTypes)
				break;
		}
	} else {
		SharedPtr<Peer> r(RR->topology->root());
		if (r) {
			SharedPtr<Path> rp(r->path(now));
			if (rp) {
				sendHELLO(tPtr,rp->localSocket(),rp->address(),now);
				rp->sent(now);
			}
		}
	}
}

void Peer::resetWithinScope(void *tPtr,InetAddress::IpScope scope,int inetAddressFamily,int64_t now)
{
	RWMutex::RLock l(_paths_l);
	for(unsigned int i=0; i < _alivePathCount; ++i) {
		if ((_paths[i])&&((_paths[i]->address().ss_family == inetAddressFamily)&&(_paths[i]->address().ipScope() == scope))) {
			sendHELLO(tPtr,_paths[i]->localSocket(),_paths[i]->address(),now);
			_paths[i]->sent(now);
		}
	}
}

void Peer::updateLatency(const unsigned int l)
{
	if ((l > 0)&&(l < 0xffff)) {
		unsigned int lat = _latency;
		if (lat < 0xffff) {
			_latency = (l + l + lat) / 3;
		} else {
			_latency = l;
		}
	}
}

bool Peer::sendDirect(void *tPtr,const void *data,const unsigned int len,const int64_t now)
{
	if ((now - _lastPrioritizedPaths) > ZT_PEER_PRIORITIZE_PATHS_INTERVAL) {
		_lastPrioritizedPaths = now;
		_paths_l.lock();
		_prioritizePaths(now);
		if (_alivePathCount == 0) {
			_paths_l.unlock();
			return false;
		}
		const bool r = _paths[0]->send(RR,tPtr,data,len,now);
		_paths_l.unlock();
		return r;
	} else {
		_paths_l.rlock();
		if (_alivePathCount == 0) {
			_paths_l.runlock();
			return false;
		}
		const bool r = _paths[0]->send(RR,tPtr,data,len,now);
		_paths_l.runlock();
		return r;
	}
}

SharedPtr<Path> Peer::path(const int64_t now)
{
	if ((now - _lastPrioritizedPaths) > ZT_PEER_PRIORITIZE_PATHS_INTERVAL) {
		_lastPrioritizedPaths = now;
		RWMutex::Lock l(_paths_l);
		_prioritizePaths(now);
		if (_alivePathCount == 0)
			return SharedPtr<Path>();
		return _paths[0];
	} else {
		RWMutex::RLock l(_paths_l);
		if (_alivePathCount == 0)
			return SharedPtr<Path>();
		return _paths[0];
	}
}

void Peer::getAllPaths(std::vector< SharedPtr<Path> > &paths)
{
	RWMutex::RLock l(_paths_l);
	paths.clear();
	paths.assign(_paths,_paths + _alivePathCount);
}

void Peer::_prioritizePaths(const int64_t now)
{
	// assumes _paths_l is locked for writing
	std::sort(_paths,_paths + ZT_MAX_PEER_NETWORK_PATHS,_PathPriorityComparisonOperator());

	for(int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if ((!_paths[i]) || (!_paths[i]->alive(now))) {
			_alivePathCount = i;

			for(;i<ZT_MAX_PEER_NETWORK_PATHS;++i)
				_paths[i].zero();

			return;
		}
	}
}

} // namespace ZeroTier
