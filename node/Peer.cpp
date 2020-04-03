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

namespace ZeroTier {

Peer::Peer(const RuntimeEnvironment *renv) : // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
	RR(renv),
	_lastReceive(0),
	_lastSend(0),
	_lastSentHello(),
	_lastWhoisRequestReceived(0),
	_lastEchoRequestReceived(0),
	_lastPushDirectPathsReceived(0),
	_lastProbeReceived(0),
	_lastAttemptedP2PInit(0),
	_lastPrioritizedPaths(0),
	_lastAttemptedAggressiveNATTraversal(0),
	_latency(-1),
	_alivePathCount(0),
	_vProto(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0)
{
	Utils::memoryLock(_identityKey,sizeof(_identityKey));
}

Peer::~Peer()
{
	Utils::memoryUnlock(_identityKey,sizeof(_identityKey));
	Utils::burn(_identityKey,sizeof(_identityKey));
}

bool Peer::init(const Identity &peerIdentity)
{
	RWMutex::Lock l(_lock);
	if (_id == peerIdentity)
		return true;
	_id = peerIdentity;
	if (!RR->identity.agree(peerIdentity,_identityKey))
		return false;
	_incomingProbe = Protocol::createProbe(_id,RR->identity,_identityKey);
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
	_inMeter.log(now,payloadLength);

	if (hops == 0) {
		RWMutex::RMaybeWLock l(_lock);

		// If this matches an existing path, skip path learning stuff.
		for (unsigned int i=0;i<_alivePathCount;++i) {
			if (_paths[i] == path) {
				_lock.runlock();
				return;
			}
		}

		// If we made it here, we don't already know this path.
		if (RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id,path->localSocket(),path->address())) {
			if (verb == Protocol::VERB_OK) {
				l.writing();

				// If the path list is full, replace the least recently active path.
				unsigned int newPathIdx = 0;
				if (_alivePathCount >= ZT_MAX_PEER_NETWORK_PATHS) {
					int64_t lastReceiveTimeMax = 0;
					for (unsigned int i=0;i<_alivePathCount;++i) {
						if ((_paths[i]->address().family() == path->address().family()) &&
						    (_paths[i]->localSocket() == path->localSocket()) && // TODO: should be localInterface when multipath is integrated
						    (_paths[i]->address().ipsEqual2(path->address()))) {
							// Replace older path if everything is the same except the port number, since NAT/firewall reboots
							// and other wacky stuff can change port number assignments.
							_paths[i] = path;
							return;
						} else if (_paths[i]->lastIn() > lastReceiveTimeMax) {
							lastReceiveTimeMax = _paths[i]->lastIn();
							newPathIdx = i;
						}
					}
				} else {
					newPathIdx = _alivePathCount++;
				}

				InetAddress old;
				if (_paths[newPathIdx])
					old = _paths[newPathIdx]->address();
				_paths[newPathIdx] = path;
				_prioritizePaths(now);

				Endpoint pathEndpoint(path->address());
				_bootstrap[pathEndpoint.type()] = pathEndpoint;

				RR->t->learnedNewPath(tPtr,0x582fabdd,packetId,_id,path->address(),old);
			} else {
				path->sent(now,hello(tPtr,path->localSocket(),path->address(),now));
				RR->t->tryingNewPath(tPtr,0xb7747ddd,_id,path->address(),path->address(),packetId,(uint8_t)verb,_id,ZT_TRACE_TRYING_NEW_PATH_REASON_PACKET_RECEIVED_FROM_UNKNOWN_PATH);
			}
		}
	} else if ((now - _lastAttemptedP2PInit) >= ZT_DIRECT_CONNECT_ATTEMPT_INTERVAL) {
		_lastAttemptedP2PInit = now;
		std::set<InetAddress> addrs;

		// Addresses assigned to local system interfaces (as configured via the API).
		std::vector<ZT_InterfaceAddress> localInterfaceAddresses(RR->node->localInterfaceAddresses());
		for(std::vector<ZT_InterfaceAddress>::const_iterator i(localInterfaceAddresses.begin());i!=localInterfaceAddresses.end();++i)
			addrs.insert(asInetAddress(i->address));

		// We also advertise IPs reported to us by our peers in OK(HELLO) replies.
		std::multimap<unsigned long,InetAddress> detectedAddresses(RR->sa->externalAddresses(now));
		for(std::multimap<unsigned long,InetAddress>::const_reverse_iterator i(detectedAddresses.rbegin());i!=detectedAddresses.rend();++i) {
			if (addrs.count(i->second) == 0) {
				addrs.insert(i->second);
				break;
			}
			if (i->first <= 1)
				break;
		}

		if (!addrs.empty()) {
			// TODO
		}
	}
}

void Peer::send(void *const tPtr,const int64_t now,const void *const data,const unsigned int len,const SharedPtr<Path> &via) noexcept
{
	via->send(RR,tPtr,data,len,now);
	sent(now,len);
}

void Peer::send(void *const tPtr,const int64_t now,const void *const data,const unsigned int len) noexcept
{
	SharedPtr<Path> via(this->path(now));
	if (via) {
		via->send(RR,tPtr,data,len,now);
	} else {
		const SharedPtr<Peer> root(RR->topology->root());
		if ((root)&&(root.ptr() != this)) {
			via = root->path(now);
			if (via) {
				via->send(RR,tPtr,data,len,now);
				root->relayed(now,len);
			} else {
				return;
			}
		} else {
			return;
		}
	}
	sent(now,len);
}

unsigned int Peer::hello(void *tPtr,int64_t localSocket,const InetAddress &atAddress,int64_t now)
{
#if 0
	Packet outp(_id.address(),RR->identity.address(),Packet::VERB_HELLO);

	outp.append((unsigned char)ZT_PROTO_VERSION);
	outp.append((unsigned char)ZEROTIER_VERSION_MAJOR);
	outp.append((unsigned char)ZEROTIER_VERSION_MINOR);
	outp.append((uint16_t)ZEROTIER_VERSION_REVISION);
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

unsigned int Peer::sendNOP(void *const tPtr,const int64_t localSocket,const InetAddress &atAddress,const int64_t now)
{
	Buf outp;
	Protocol::Header &ph = outp.as<Protocol::Header>(); // NOLINT(hicpp-use-auto,modernize-use-auto)
	ph.packetId = Protocol::getPacketId();
	_id.address().copyTo(ph.destination);
	RR->identity.address().copyTo(ph.source);
	ph.flags = 0;
	ph.verb = Protocol::VERB_NOP;
	Protocol::armor(outp,sizeof(Protocol::Header),_identityKey,this->cipher());
	RR->node->putPacket(tPtr,localSocket,atAddress,outp.unsafeData,sizeof(Protocol::Header));
	return sizeof(Protocol::Header);
}

void Peer::pulse(void *const tPtr,const int64_t now,const bool isRoot)
{
	RWMutex::Lock l(_lock);

	bool needHello = false;
	if ((now - _lastSentHello) >= ZT_PEER_HELLO_INTERVAL) {
		_lastSentHello = now;
		needHello = true;
	}

	_prioritizePaths(now);

	for(unsigned int i=0;i<_alivePathCount;++i) {
		if (needHello) {
			needHello = false;
			const unsigned int bytes = hello(tPtr,_paths[i]->localSocket(),_paths[i]->address(),now);
			_paths[i]->sent(now,bytes);
			sent(now,bytes);
		} else if ((now - _paths[i]->lastOut()) >= ZT_PATH_KEEPALIVE_PERIOD) {
			_paths[i]->send(RR,tPtr,&now,1,now);
			sent(now,1);
		}

		// TODO: when we merge multipath we'll keep one open per interface to non-roots.
		// For roots we try to keep every path open.
		if (!isRoot)
			return;
	}

	if (needHello) {
		// Try any statically configured addresses.
		InetAddress addr;
		if (RR->node->externalPathLookup(tPtr,_id,-1,addr)) {
			if (RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id,-1,addr)) {
				RR->t->tryingNewPath(tPtr,0x84a10000,_id,addr,InetAddress::NIL,0,0,Identity::NIL,ZT_TRACE_TRYING_NEW_PATH_REASON_EXPLICITLY_SUGGESTED_ADDRESS);
				hello(tPtr,-1,addr,now);
			}
		}

		if (!_bootstrap.empty()) {
			if (isRoot) {
				// Try all bootstrap addresses if this is a root.
				for(std::map< Endpoint::Type,Endpoint >::const_iterator i(_bootstrap.begin());i!=_bootstrap.end();++i) {
					if ( ((i->first == Endpoint::TYPE_INETADDR_V4)||(i->first == Endpoint::TYPE_INETADDR_V6)) && (!i->second.inetAddr().ipsEqual(addr)) ) {
						RR->t->tryingNewPath(tPtr,0x0a009444,_id,i->second.inetAddr(),InetAddress::NIL,0,0,Identity::NIL,ZT_TRACE_TRYING_NEW_PATH_REASON_BOOTSTRAP_ADDRESS);
						hello(tPtr,-1,i->second.inetAddr(),now);
					}
				}
			} else {
				// Otherwise try a random bootstrap address.
				unsigned int tryAtIndex = (unsigned int)Utils::random() % (unsigned int)_bootstrap.size();
				for(std::map< Endpoint::Type,Endpoint >::const_iterator i(_bootstrap.begin());i!=_bootstrap.end();++i) {
					if (tryAtIndex > 0) {
						--tryAtIndex;
					} else {
						if ( ((i->first == Endpoint::TYPE_INETADDR_V4)||(i->first == Endpoint::TYPE_INETADDR_V6)) && (!i->second.inetAddr().ipsEqual(addr)) ) {
							RR->t->tryingNewPath(tPtr,0x0a009444,_id,i->second.inetAddr(),InetAddress::NIL,0,0,Identity::NIL,ZT_TRACE_TRYING_NEW_PATH_REASON_BOOTSTRAP_ADDRESS);
							hello(tPtr,-1,i->second.inetAddr(),now);
						}
					}
				}
			}
		}
	}
}

void Peer::resetWithinScope(void *tPtr,InetAddress::IpScope scope,int inetAddressFamily,int64_t now)
{
	RWMutex::RLock l(_lock);
	for(unsigned int i=0;i<_alivePathCount;++i) {
		if ((_paths[i])&&((_paths[i]->address().family() == inetAddressFamily)&&(_paths[i]->address().ipScope() == scope)))
			_paths[i]->sent(now,sendNOP(tPtr,_paths[i]->localSocket(),_paths[i]->address(),now));
	}
}

bool Peer::directlyConnected(int64_t now)
{
	if ((now - _lastPrioritizedPaths) > ZT_PEER_PRIORITIZE_PATHS_INTERVAL) {
		RWMutex::Lock l(_lock);
		_prioritizePaths(now);
		return _alivePathCount > 0;
	} else {
		RWMutex::RLock l(_lock);
		return _alivePathCount > 0;
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
	uint8_t buf[8 + ZT_PEER_MARSHAL_SIZE_MAX];

	// Prefix each saved peer with the current timestamp.
	Utils::storeBigEndian<uint64_t>(buf,(uint64_t)RR->node->now());

	const int len = marshal(buf + 8);
	if (len > 0) {
		uint64_t id[2];
		id[0] = _id.address().toInt();
		id[1] = 0;
		RR->node->stateObjectPut(tPtr,ZT_STATE_OBJECT_PEER,id,buf,(unsigned int)len + 8);
	}
}

void Peer::tryToContactAt(void *const tPtr,const Endpoint &ep,const int64_t now,const bool bfg1024)
{
	static uint8_t junk = 0;

	if (ep.inetAddr()) { // only this endpoint type is currently implemented
		if (!RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id,-1,ep.inetAddr()))
			return;

		// Sending a packet with a low TTL before the real message assists traversal with some
		// stateful firewalls and is harmless otherwise AFAIK.
		++junk;
		RR->node->putPacket(tPtr,-1,ep.inetAddr(),&junk,1,2);

		// In a few hundred milliseconds we'll send the real packet.
		{
			RWMutex::Lock l(_lock);
			_contactQueue.push_back(_ContactQueueItem(ep.inetAddr(),ZT_MAX_PEER_NETWORK_PATHS)); // NOLINT(hicpp-use-emplace,modernize-use-emplace)
		}

		// If the peer indicates that they may be behind a symmetric NAT and there are no
		// living direct paths, try a few more aggressive things.
		if ((ep.inetAddr().family() == AF_INET) && (!directlyConnected(now))) {
			unsigned int port = ep.inetAddr().port();
			if ((bfg1024)&&(port < 1024)&&(RR->node->natMustDie())) {
				// If the other side is using a low-numbered port and has elected to
				// have this done, we can try scanning every port below 1024. The search
				// space here is small enough that we have a very good chance of punching.

				// Generate a random order list of all <1024 ports except 0 and the original sending port.
				uint16_t ports[1022];
				uint16_t ctr = 1;
				for (int i=0;i<1022;++i) { // NOLINT(modernize-loop-convert)
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
						_contactQueue.push_back(_ContactQueueItem(ep.inetAddr(),ports + i,ports + i + 128,1)); // NOLINT(hicpp-use-emplace,modernize-use-emplace)
					_contactQueue.push_back(_ContactQueueItem(ep.inetAddr(),ports + 896,ports + 1022,1)); // NOLINT(hicpp-use-emplace,modernize-use-emplace)
				}
			} else {
				// Otherwise use the simpler sequential port attempt method in intervals.
				RWMutex::Lock l(_lock);
				for (int k=0;k<3;++k) {
					if (++port > 65535) break;
					InetAddress tryNext(ep.inetAddr());
					tryNext.setPort(port);
					_contactQueue.push_back(_ContactQueueItem(tryNext,1)); // NOLINT(hicpp-use-emplace,modernize-use-emplace)
				}
			}
		}

		// Start alarms going off to actually send these...
		RR->node->setPeerAlarm(_id.fingerprint(),now + ZT_NAT_TRAVERSAL_INTERVAL);
	}
}

void Peer::alarm(void *tPtr,const int64_t now)
{
	// Right now alarms are only used for multi-phase or multi-step NAT traversal operations.

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

		for(std::list<_ContactQueueItem>::iterator q(_contactQueue.begin());q!=_contactQueue.end();) { // NOLINT(hicpp-use-auto,modernize-use-auto)
			if (_alivePathCount >= q->alivePathThreshold)
				_contactQueue.erase(q++);
			else ++q;
		}

		stillHaveContactQueueItems = !_contactQueue.empty();
	}

	if (_vProto >= 11) {
		uint64_t outgoingProbe = Protocol::createProbe(RR->identity,_id,_identityKey);
		if (qi.ports.empty()) {
			RR->node->putPacket(tPtr,-1,qi.address,&outgoingProbe,ZT_PROTO_PROBE_LENGTH);
		} else {
			for (std::vector<uint16_t>::iterator p(qi.ports.begin()); p != qi.ports.end(); ++p) { // NOLINT(hicpp-use-auto,modernize-use-auto)
				qi.address.setPort(*p);
				RR->node->putPacket(tPtr,-1,qi.address,&outgoingProbe,ZT_PROTO_PROBE_LENGTH);
			}
		}
	} else {
		if (qi.ports.empty()) {
			this->sendNOP(tPtr,-1,qi.address,now);
		} else {
			for (std::vector<uint16_t>::iterator p(qi.ports.begin()); p != qi.ports.end(); ++p) { // NOLINT(hicpp-use-auto,modernize-use-auto)
				qi.address.setPort(*p);
				this->sendNOP(tPtr,-1,qi.address,now);
			}
		}
	}

	if (stillHaveContactQueueItems)
		RR->node->setPeerAlarm(_id.fingerprint(),now + ZT_NAT_TRAVERSAL_INTERVAL);
}

int Peer::marshal(uint8_t data[ZT_PEER_MARSHAL_SIZE_MAX]) const noexcept
{
	data[0] = 0; // serialized peer version

	// For faster unmarshaling on large nodes the long-term secret key is cached. It's
	// encrypted with a symmetric key derived from a hash of the local node's identity
	// secrets, so the local node's address is also included. That way the unmarshal
	// code can check this address and not use this cached key if the local identity has
	// changed. In that case agreement must be executed again.
	RR->identity.address().copyTo(data + 1);
	RR->localCacheSymmetric.encrypt(_identityKey,data + 6);
	RR->localCacheSymmetric.encrypt(_identityKey + 16,data + 22);

	RWMutex::RLock l(_lock);

	int s = _id.marshal(data + 38,false);
	if (s <= 0)
		return s;
	int p = s + 38;

	s = _locator.marshal(data + p);
	if (s <= 0)
		return s;
	p += s;

	data[p++] = (uint8_t)_bootstrap.size();
	for(std::map< Endpoint::Type,Endpoint >::const_iterator i(_bootstrap.begin());i!=_bootstrap.end();++i) { // NOLINT(modernize-loop-convert,hicpp-use-auto,modernize-use-auto)
		s = i->second.marshal(data + p);
		if (s <= 0)
			return s;
		p += s;
	}

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
	bool mustRecomputeSecret;

	{
		RWMutex::Lock l(_lock);

		if ((len <= 38) || (data[0] != 0))
			return -1;

		if (Address(data + 1) == RR->identity.address()) {
			RR->localCacheSymmetric.decrypt(data + 6,_identityKey);
			RR->localCacheSymmetric.decrypt(data + 22,_identityKey + 16);
			mustRecomputeSecret = false;
		} else {
			mustRecomputeSecret = true; // can't use cached key if local identity has changed
		}

		int s = _id.unmarshal(data + 38,len - 38);
		if (s <= 0)
			return s;
		p = s + 38;
		s = _locator.unmarshal(data + p,len - p);
		if (s <= 0)
			return s;
		p += s;

		if (p >= len)
			return -1;
		const unsigned int bootstrapCount = data[p++];
		if (bootstrapCount > ZT_MAX_PEER_NETWORK_PATHS)
			return -1;
		_bootstrap.clear();
		for(unsigned int i=0;i<bootstrapCount;++i) {
			Endpoint tmp;
			s = tmp.unmarshal(data + p,len - p);
			if (s <= 0)
				return s;
			p += s;
			_bootstrap[tmp.type()] = tmp;
		}

		if ((p + 10) > len)
			return -1;

		_vProto = Utils::loadBigEndian<uint16_t>(data + p); p += 2;
		_vMajor = Utils::loadBigEndian<uint16_t>(data + p); p += 2;
		_vMinor = Utils::loadBigEndian<uint16_t>(data + p); p += 2;
		_vRevision = Utils::loadBigEndian<uint16_t>(data + p); p += 2;
		p += 2 + (int)Utils::loadBigEndian<uint16_t>(data + p);

		if (p > len)
			return -1;
	}

	if (mustRecomputeSecret) {
		if (!RR->identity.agree(_id,_identityKey))
			return -1;
	}

	_incomingProbe = Protocol::createProbe(_id,RR->identity,_identityKey);

	return p;
}

struct _PathPriorityComparisonOperator
{
	ZT_INLINE bool operator()(const SharedPtr<Path> &a,const SharedPtr<Path> &b) const noexcept
	{
		// Sort in order of last received time for receipt of anything over path, which prioritizes
		// paths by aliveness. This will go away when we merge in multipath in favor of something
		// much smarter.
		return ( ((a)&&(a->lastIn() > 0)) && ((!b)||(b->lastIn() <= 0)||(a->lastIn() < b->lastIn())) );
	}
};

void Peer::_prioritizePaths(const int64_t now)
{
	// assumes _lock is locked for writing
	_lastPrioritizedPaths = now;

	std::sort(_paths,_paths + ZT_MAX_PEER_NETWORK_PATHS,_PathPriorityComparisonOperator());

	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if ((!_paths[i]) || (!_paths[i]->alive(now))) {
			_alivePathCount = i;

			for(;i<ZT_MAX_PEER_NETWORK_PATHS;++i)
				_paths[i].zero();

			break;
		}
	}
}

} // namespace ZeroTier
