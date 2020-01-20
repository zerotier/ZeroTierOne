/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
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

#include <set>

namespace ZeroTier {

struct _PathPriorityComparisonOperator
{
	ZT_ALWAYS_INLINE bool operator()(const SharedPtr<Path> &a,const SharedPtr<Path> &b) const
	{
		return ( ((a)&&(a->lastIn() > 0)) && ((!b)||(b->lastIn() <= 0)||(a->lastIn() < b->lastIn())) );
	}
};

Peer::Peer(const RuntimeEnvironment *renv) :
	RR(renv),
	_lastReceive(0),
	_lastWhoisRequestReceived(0),
	_lastEchoRequestReceived(0),
	_lastPushDirectPathsReceived(0),
	_lastAttemptedP2PInit(0),
	_lastTriedStaticPath(0),
	_lastPrioritizedPaths(0),
	_latency(0xffff),
	_alivePathCount(0)
{
}

bool Peer::init(const Identity &myIdentity,const Identity &peerIdentity)
{
	_id = peerIdentity;
	_vProto = 0;
	_vMajor = 0;
	_vMinor = 0;
	_vRevision = 0;
	return myIdentity.agree(peerIdentity,_key);
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
		_lock.rlock();
		for(int i=0;i<(int)_alivePathCount;++i) {
			if (_paths[i] == path) {
				_lock.runlock();
				goto path_check_done;
			}
		}
		_lock.runlock();

		if (verb == Packet::VERB_OK) {
			RWMutex::Lock l(_lock);

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
			_bootstrap = path->address();
			_prioritizePaths(now);
			RR->t->peerLearnedNewPath(tPtr,networkId,*this,path,packetId);
		} else {
			if (RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id,path->localSocket(),path->address())) {
				sendHELLO(tPtr,path->localSocket(),path->address(),now);
				path->sent(now);
				RR->t->peerConfirmingUnknownPath(tPtr,networkId,*this,path,packetId,verb);
			}
		}
	}

path_check_done:
	const int64_t sinceLastP2PInit = now - _lastAttemptedP2PInit;
	if (sinceLastP2PInit >= ((hops == 0) ? ZT_DIRECT_PATH_PUSH_INTERVAL_HAVEPATH : ZT_DIRECT_PATH_PUSH_INTERVAL)) {
		_lastAttemptedP2PInit = now;

		InetAddress addr;
		if (_bootstrap.type() == Endpoint::INETADDR)
			sendHELLO(tPtr,-1,_bootstrap.inetAddr(),now);
		if (RR->node->externalPathLookup(tPtr,_id,-1,addr)) {
			if (RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id,-1,addr))
				sendHELLO(tPtr,-1,addr,now);
		}

		std::vector<ZT_InterfaceAddress> localInterfaceAddresses(RR->node->localInterfaceAddresses());
		std::multimap<unsigned long,InetAddress> detectedAddresses(RR->sa->externalAddresses(now));
		std::set<InetAddress> addrs;
		for(std::vector<ZT_InterfaceAddress>::const_iterator i(localInterfaceAddresses.begin());i!=localInterfaceAddresses.end();++i)
			addrs.insert(asInetAddress(i->address));
		for(std::multimap<unsigned long,InetAddress>::const_reverse_iterator i(detectedAddresses.rbegin());i!=detectedAddresses.rend();++i) {
			if (i->first <= 1)
				break;
			if (addrs.count(i->second) == 0) {
				addrs.insert(i->second);
				break;
			}
		}

		if (!addrs.empty()) {
			ScopedPtr<Packet> outp(new Packet(_id.address(),RR->identity.address(),Packet::VERB_PUSH_DIRECT_PATHS));
			outp->addSize(2); // leave room for count
			unsigned int count = 0;
			for(std::set<InetAddress>::iterator a(addrs.begin());a!=addrs.end();++a) {
				uint8_t addressType = 4;
				uint8_t addressLength = 6;
				unsigned int ipLength = 4;
				const void *rawIpData = (const void *)0;
				uint16_t port = 0;
				switch(a->ss_family) {
					case AF_INET:
						rawIpData = &(reinterpret_cast<const sockaddr_in *>(&(*a))->sin_addr.s_addr);
						port = Utils::ntoh((uint16_t)reinterpret_cast<const sockaddr_in *>(&(*a))->sin_port);
						break;
					case AF_INET6:
						rawIpData = reinterpret_cast<const sockaddr_in6 *>(&(*a))->sin6_addr.s6_addr;
						port = Utils::ntoh((uint16_t)reinterpret_cast<const sockaddr_in6 *>(&(*a))->sin6_port);
						addressType = 6;
						addressLength = 18;
						ipLength = 16;
						break;
					default:
						continue;
				}

				outp->append((uint8_t)0); // no flags
				outp->append((uint16_t)0); // no extensions
				outp->append(addressType);
				outp->append(addressLength);
				outp->append(rawIpData,ipLength);
				outp->append(port);

				++count;
				if (outp->size() >= (ZT_PROTO_MAX_PACKET_LENGTH - 32))
					break;
			}
			if (count > 0) {
				outp->setAt(ZT_PACKET_IDX_PAYLOAD,(uint16_t)count);
				outp->compress();
				outp->armor(_key,true);
				path->send(RR,tPtr,outp->data(),outp->size(),now);
			}
		}
	}
}

bool Peer::shouldTryPath(void *tPtr,int64_t now,const SharedPtr<Peer> &suggestedBy,const InetAddress &addr) const
{
	int maxHaveScope = -1;
	{
		RWMutex::RLock l(_lock);
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
	return ( ((int)addr.ipScope() > maxHaveScope) && RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id,-1,addr) );
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

void Peer::ping(void *tPtr,int64_t now,const bool pingAllAddressTypes)
{
	RWMutex::RLock l(_lock);

	_lastPrioritizedPaths = now;
	_prioritizePaths(now);

	if (_alivePathCount > 0) {
		for (unsigned int i = 0; i < _alivePathCount; ++i) {
			sendHELLO(tPtr,_paths[i]->localSocket(),_paths[i]->address(),now);
			_paths[i]->sent(now);
			if (!pingAllAddressTypes)
				return;
		}
		return;
	}

	if (_bootstrap.type() == Endpoint::INETADDR)
		sendHELLO(tPtr,-1,_bootstrap.inetAddr(),now);

	SharedPtr<Peer> r(RR->topology->root());
	if ((r)&&(r.ptr() != this)) {
		SharedPtr<Path> rp(r->path(now));
		if (rp) {
			sendHELLO(tPtr,rp->localSocket(),rp->address(),now);
			rp->sent(now);
			return;
		}
	}
}

void Peer::resetWithinScope(void *tPtr,InetAddress::IpScope scope,int inetAddressFamily,int64_t now)
{
	RWMutex::RLock l(_lock);
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
		_lock.lock();
		_prioritizePaths(now);
		if (_alivePathCount == 0) {
			_lock.unlock();
			return false;
		}
		const bool r = _paths[0]->send(RR,tPtr,data,len,now);
		_lock.unlock();
		return r;
	} else {
		_lock.rlock();
		if (_alivePathCount == 0) {
			_lock.runlock();
			return false;
		}
		const bool r = _paths[0]->send(RR,tPtr,data,len,now);
		_lock.runlock();
		return r;
	}
}

SharedPtr<Path> Peer::path(const int64_t now)
{
	if ((now - _lastPrioritizedPaths) > ZT_PEER_PRIORITIZE_PATHS_INTERVAL) {
		_lastPrioritizedPaths = now;
		RWMutex::Lock l(_lock);
		_prioritizePaths(now);
		if (_alivePathCount == 0)
			return SharedPtr<Path>();
		return _paths[0];
	} else {
		RWMutex::RLock l(_lock);
		if (_alivePathCount == 0)
			return SharedPtr<Path>();
		return _paths[0];
	}
}

void Peer::getAllPaths(std::vector< SharedPtr<Path> > &paths)
{
	RWMutex::RLock l(_lock);
	paths.clear();
	paths.assign(_paths,_paths + _alivePathCount);
}

void Peer::save(void *tPtr) const
{
	uint8_t *const buf = (uint8_t *)malloc(ZT_PEER_MARSHAL_SIZE_MAX);
	if (!buf) return;

	_lock.rlock();
	const int len = marshal(buf);
	_lock.runlock();

	if (len > 0) {
		uint64_t id[2];
		id[0] = _id.address().toInt();
		id[1] = 0;
		RR->node->stateObjectPut(tPtr,ZT_STATE_OBJECT_PEER,id,buf,(unsigned int)len);
	}

	free(buf);
}

int Peer::marshal(uint8_t data[ZT_PEER_MARSHAL_SIZE_MAX]) const
{
	RWMutex::RLock l(_lock);

	data[0] = 0; // serialized peer version

	int s = _id.marshal(data + 1,false);
	if (s <= 0)
		return s;
	int p = 1 + s;
	s = _locator.marshal(data + p);
	if (s <= 0)
		return s;
	p += s;
	s = _bootstrap.marshal(data + p);
	if (s <= 0)
		return s;
	p += s;

	Utils::storeBigEndian(data + p,(uint16_t)_vProto);
	p += 2;
	Utils::storeBigEndian(data + p,(uint16_t)_vMajor);
	p += 2;
	Utils::storeBigEndian(data + p,(uint16_t)_vMinor);
	p += 2;
	Utils::storeBigEndian(data + p,(uint16_t)_vRevision);
	p += 2;

	data[p++] = 0;
	data[p++] = 0;

	return p;
}

int Peer::unmarshal(const uint8_t *restrict data,const int len)
{
	RWMutex::Lock l(_lock);

	if ((len <= 1)||(data[0] != 0))
		return -1;

	int s = _id.unmarshal(data + 1,len - 1);
	if (s <= 0)
		return s;
	int p = 1 + s;
	s = _locator.unmarshal(data + p,len - p);
	if (s <= 0)
		return s;
	p += s;
	s = _bootstrap.unmarshal(data + p,len - p);
	if (s <= 0)
		return s;
	p += s;

	if ((p + 10) > len)
		return -1;
	_vProto = Utils::loadBigEndian<uint16_t>(data + p);
	p += 2;
	_vMajor = Utils::loadBigEndian<uint16_t>(data + p);
	p += 2;
	_vMinor = Utils::loadBigEndian<uint16_t>(data + p);
	p += 2;
	_vRevision = Utils::loadBigEndian<uint16_t>(data + p);
	p += 2;
	p += 2 + (int)Utils::loadBigEndian<uint16_t>(data + p);
	if (p > len)
		return -1;

	return p;
}

void Peer::_prioritizePaths(const int64_t now)
{
	// assumes _lock is locked for writing
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
