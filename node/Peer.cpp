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
#include "RuntimeEnvironment.hpp"
#include "Trace.hpp"
#include "Peer.hpp"
#include "Topology.hpp"
#include "Node.hpp"
#include "SelfAwareness.hpp"
#include "InetAddress.hpp"
#include "Protocol.hpp"
#include "Endpoint.hpp"

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
	_lastProbeReceived(0),
	_lastAttemptedP2PInit(0),
	_lastTriedStaticPath(0),
	_lastPrioritizedPaths(0),
	_lastAttemptedAggressiveNATTraversal(0),
	_latency(0xffff),
	_alivePathCount(0),
	_vProto(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0)
{
}

bool Peer::init(const Identity &peerIdentity)
{
	RWMutex::Lock l(_lock);
	if (_id == peerIdentity)
		return true;
	_id = peerIdentity;
	if (!RR->identity.agree(peerIdentity,_key))
		return false;
	_incomingProbe = Protocol::createProbe(_id,RR->identity,_key);
	return true;
}

void Peer::received(
	void *tPtr,
	const SharedPtr<Path> &path,
	const unsigned int hops,
	const uint64_t packetId,
	const unsigned int payloadLength,
	const Protocol::Verb verb,
	const Protocol::Verb inReVerb)
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

		if (verb == Protocol::VERB_OK) {
			RWMutex::Lock l(_lock);

			int64_t lastReceiveTimeMax = 0;
			int lastReceiveTimeMaxAt = 0;
			for(int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
				if ((_paths[i]->address().family() == path->address().family()) &&
				    (_paths[i]->localSocket() == path->localSocket()) && // TODO: should be localInterface when multipath is integrated
				    (_paths[i]->address().ipsEqual2(path->address()))) {
					// Replace older path if everything is the same except the port number.
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
			InetAddress old;
			if (_paths[lastReceiveTimeMaxAt])
				old = _paths[lastReceiveTimeMaxAt]->address();
			_paths[lastReceiveTimeMaxAt] = path;
			_bootstrap = Endpoint(path->address());
			_prioritizePaths(now);
			RR->t->learnedNewPath(tPtr,0x582fabdd,packetId,_id,path->address(),old);
		} else {
			if (RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id,path->localSocket(),path->address())) {
				RR->t->tryingNewPath(tPtr,0xb7747ddd,_id,path->address(),path->address(),packetId,(uint8_t)verb,_id.address(),_id.hash().data(),ZT_TRACE_TRYING_NEW_PATH_REASON_PACKET_RECEIVED_FROM_UNKNOWN_PATH);
				path->sent(now,sendHELLO(tPtr,path->localSocket(),path->address(),now));
			}
		}
	}

path_check_done:
	if ((now - _lastAttemptedP2PInit) >= ((hops == 0) ? ZT_DIRECT_PATH_PUSH_INTERVAL_HAVEPATH : ZT_DIRECT_PATH_PUSH_INTERVAL)) {
		_lastAttemptedP2PInit = now;

		InetAddress addr;
		if ((_bootstrap.type() == Endpoint::TYPE_INETADDR_V4)||(_bootstrap.type() == Endpoint::TYPE_INETADDR_V6)) {
			RR->t->tryingNewPath(tPtr,0x0a009444,_id,_bootstrap.inetAddr(),InetAddress::NIL,0,0,0,nullptr,ZT_TRACE_TRYING_NEW_PATH_REASON_BOOTSTRAP_ADDRESS);
			sendHELLO(tPtr,-1,_bootstrap.inetAddr(),now);
		} if (RR->node->externalPathLookup(tPtr,_id,-1,addr)) {
			if (RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id,-1,addr)) {
				RR->t->tryingNewPath(tPtr,0x84a10000,_id,_bootstrap.inetAddr(),InetAddress::NIL,0,0,0,nullptr,ZT_TRACE_TRYING_NEW_PATH_REASON_EXPLICITLY_SUGGESTED_ADDRESS);
				sendHELLO(tPtr,-1,addr,now);
			}
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
#if 0
			ScopedPtr<Packet> outp(new Packet(_id.address(),RR->identity.address(),Packet::VERB_PUSH_DIRECT_PATHS));
			outp->addSize(2); // leave room for count
			unsigned int count = 0;
			for(std::set<InetAddress>::iterator a(addrs.begin());a!=addrs.end();++a) {
				uint8_t addressType = 4;
				uint8_t addressLength = 6;
				unsigned int ipLength = 4;
				const void *rawIpData = nullptr;
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
#endif
		}
	}
}

unsigned int Peer::sendHELLO(void *tPtr,const int64_t localSocket,const InetAddress &atAddress,int64_t now)
{
#if 0
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
#endif
}

unsigned int Peer::sendNOP(void *tPtr,const int64_t localSocket,const InetAddress &atAddress,int64_t now)
{
	Buf outp;
	Protocol::Header &ph = outp.as<Protocol::Header>();
	ph.packetId = Protocol::getPacketId();
	_id.address().copyTo(ph.destination);
	RR->identity.address().copyTo(ph.source);
	ph.flags = 0;
	ph.verb = Protocol::VERB_NOP;
	Protocol::armor(outp,sizeof(Protocol::Header),_key,this->cipher());
	RR->node->putPacket(tPtr,localSocket,atAddress,outp.unsafeData,sizeof(Protocol::Header));
	return sizeof(Protocol::Header);
}

void Peer::ping(void *tPtr,int64_t now,const bool pingAllAddressTypes)
{
	RWMutex::RLock l(_lock);

	_lastPrioritizedPaths = now;
	_prioritizePaths(now);

	if (_alivePathCount > 0) {
		for (unsigned int i = 0; i < _alivePathCount; ++i) {
			_paths[i]->sent(now,sendHELLO(tPtr,_paths[i]->localSocket(),_paths[i]->address(),now));
			if (!pingAllAddressTypes)
				return;
		}
		return;
	}

	if ((_bootstrap.type() == Endpoint::TYPE_INETADDR_V4)||(_bootstrap.type() == Endpoint::TYPE_INETADDR_V6))
		sendHELLO(tPtr,-1,_bootstrap.inetAddr(),now);

	SharedPtr<Peer> r(RR->topology->root());
	if ((r)&&(r.ptr() != this)) {
		SharedPtr<Path> rp(r->path(now));
		if (rp) {
			rp->sent(now,sendHELLO(tPtr,rp->localSocket(),rp->address(),now));
			return;
		}
	}
}

void Peer::resetWithinScope(void *tPtr,InetAddress::IpScope scope,int inetAddressFamily,int64_t now)
{
	RWMutex::RLock l(_lock);
	for(unsigned int i=0; i < _alivePathCount; ++i) {
		if ((_paths[i])&&((_paths[i]->address().family() == inetAddressFamily)&&(_paths[i]->address().ipScope() == scope))) {
			_paths[i]->sent(now,sendHELLO(tPtr,_paths[i]->localSocket(),_paths[i]->address(),now));
		}
	}
}

void Peer::updateLatency(const unsigned int l) noexcept
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

bool Peer::direct(const int64_t now)
{
	if ((now - _lastPrioritizedPaths) > ZT_PEER_PRIORITIZE_PATHS_INTERVAL) {
		_lastPrioritizedPaths = now;
		RWMutex::Lock l(_lock);
		_prioritizePaths(now);
		return (_alivePathCount > 0);
	} else {
		RWMutex::RLock l(_lock);
		return (_alivePathCount > 0);
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
	uint8_t *const buf = (uint8_t *)malloc(8 + ZT_PEER_MARSHAL_SIZE_MAX);
	if (!buf) return;

	Utils::storeBigEndian<uint64_t>(buf,(uint64_t)RR->node->now());

	_lock.rlock();
	const int len = marshal(buf + 8);
	_lock.runlock();

	if (len > 0) {
		uint64_t id[2];
		id[0] = _id.address().toInt();
		id[1] = 0;
		RR->node->stateObjectPut(tPtr,ZT_STATE_OBJECT_PEER,id,buf,(unsigned int)len + 8);
	}

	free(buf);
}

void Peer::contact(void *tPtr,const Endpoint &ep,const int64_t now,const bool bfg1024)
{
	static uint8_t junk = 0;

	InetAddress phyAddr(ep.inetAddr());
	if (phyAddr) { // only this endpoint type is currently implemented
		if (!RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id,-1,phyAddr))
			return;

		// Sending a packet with a low TTL before the real message assists traversal with some
		// stateful firewalls and is harmless otherwise AFAIK.
		++junk;
		RR->node->putPacket(tPtr,-1,phyAddr,&junk,1,2);

		// In a few hundred milliseconds we'll send the real packet.
		{
			RWMutex::Lock l(_lock);
			_contactQueue.push_back(_ContactQueueItem(phyAddr,ZT_MAX_PEER_NETWORK_PATHS));
		}

		// If the peer indicates that they may be behind a symmetric NAT and there are no
		// living direct paths, try a few more aggressive things.
		if ((phyAddr.family() == AF_INET) && (!direct(now))) {
			unsigned int port = phyAddr.port();
			if ((bfg1024)&&(port < 1024)&&(RR->node->natMustDie())) {
				// If the other side is using a low-numbered port and has elected to
				// have this done, we can try scanning every port below 1024. The search
				// space here is small enough that we have a very good chance of punching.

				// Generate a random order list of all <1024 ports except 0 and the original sending port.
				uint16_t ports[1022];
				uint16_t ctr = 1;
				for (int i=0;i<1022;++i) {
					if (ctr == port) ++ctr;
					ports[i] = ctr++;
				}
				for (int i=0;i<512;++i) {
					uint64_t rn = Utils::random();
					unsigned int a = ((unsigned int)rn) % 1022;
					unsigned int b = ((unsigned int)(rn >> 24U)) % 1022;
					if (a != b) {
						uint16_t tmp = ports[a];
						ports[a] = ports[b];
						ports[b] = tmp;
					}
				}

				// Chunk ports into chunks of 128 to try in few hundred millisecond intervals,
				// abandoning attempts once there is at least one direct path.
				{
					RWMutex::Lock l(_lock);
					for (int i=0;i<896;i+=128)
						_contactQueue.push_back(_ContactQueueItem(phyAddr,ports + i,ports + i + 128,1));
					_contactQueue.push_back(_ContactQueueItem(phyAddr,ports + 896,ports + 1022,1));
				}
			} else {
				// Otherwise use the simpler sequential port attempt method in intervals.
				RWMutex::Lock l(_lock);
				for (int k=0;k<3;++k) {
					if (++port > 65535) break;
					InetAddress tryNext(phyAddr);
					tryNext.setPort(port);
					_contactQueue.push_back(_ContactQueueItem(tryNext,1));
				}
			}
		}

		// Start alarms going off to actually send these...
		RR->node->setPeerAlarm(_id.address(),now + ZT_NAT_TRAVERSAL_INTERVAL);
	}
}

void Peer::alarm(void *tPtr,const int64_t now)
{
	// Pop one contact queue item and also clean the queue of any that are no
	// longer applicable because the alive path count has exceeded their threshold.
	bool stillHaveContactQueueItems;
	_ContactQueueItem qi;
	{
		RWMutex::Lock l(_lock);

		if (_contactQueue.empty())
			return;
		while (_alivePathCount >= _contactQueue.front().alivePathThreshold) {
			_contactQueue.pop_front();
			if (_contactQueue.empty())
				return;
		}

		_ContactQueueItem &qi2 = _contactQueue.front();
		qi.address = qi2.address;
		qi.ports.swap(qi2.ports);
		qi.alivePathThreshold = qi2.alivePathThreshold;
		_contactQueue.pop_front();

		for(std::list<_ContactQueueItem>::iterator q(_contactQueue.begin());q!=_contactQueue.end();) {
			if (_alivePathCount >= q->alivePathThreshold)
				_contactQueue.erase(q++);
			else ++q;
		}

		stillHaveContactQueueItems = !_contactQueue.empty();
	}

	if (_vProto >= 11) {
		uint64_t outgoingProbe = Protocol::createProbe(RR->identity,_id,_key);
		if (qi.ports.empty()) {
			RR->node->putPacket(tPtr,-1,qi.address,&outgoingProbe,ZT_PROTO_PROBE_LENGTH);
		} else {
			for (std::vector<uint16_t>::iterator p(qi.ports.begin()); p != qi.ports.end(); ++p) {
				qi.address.setPort(*p);
				RR->node->putPacket(tPtr,-1,qi.address,&outgoingProbe,ZT_PROTO_PROBE_LENGTH);
			}
		}
	} else {
		if (qi.ports.empty()) {
			this->sendNOP(tPtr,-1,qi.address,now);
		} else {
			for (std::vector<uint16_t>::iterator p(qi.ports.begin()); p != qi.ports.end(); ++p) {
				qi.address.setPort(*p);
				this->sendNOP(tPtr,-1,qi.address,now);
			}
		}
	}

	if (stillHaveContactQueueItems)
		RR->node->setPeerAlarm(_id.address(),now + ZT_NAT_TRAVERSAL_INTERVAL);
}

int Peer::marshal(uint8_t data[ZT_PEER_MARSHAL_SIZE_MAX]) const noexcept
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

int Peer::unmarshal(const uint8_t *restrict data,const int len) noexcept
{
	int p;

	{
		RWMutex::Lock l(_lock);

		if ((len <= 1) || (data[0] != 0))
			return -1;

		int s = _id.unmarshal(data + 1,len - 1);
		if (s <= 0)
			return s;
		p = 1 + s;
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
	}

	if (!RR->identity.agree(_id,_key))
		return -1;
	_incomingProbe = Protocol::createProbe(_id,RR->identity,_key);

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
