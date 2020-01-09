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
#include "RingBuffer.hpp"
#include "Utils.hpp"
#include "ScopedPtr.hpp"

namespace ZeroTier {

Peer::Peer(const RuntimeEnvironment *renv,const Identity &myIdentity,const Identity &peerIdentity) :
	RR(renv),
	_lastReceive(0),
	_lastDirectPathPushSent(0),
	_lastDirectPathPushReceive(0),
	_lastCredentialRequestSent(0),
	_lastWhoisRequestReceived(0),
	_lastEchoRequestReceived(0),
	_lastCredentialsReceived(0),
	_lastACKWindowReset(0),
	_lastQoSWindowReset(0),
	_lastMultipathCompatibilityCheck(0),
	_lastTriedStaticPath(0),
	_uniqueAlivePathCount(0),
	_localMultipathSupported(false),
	_remoteMultipathSupported(false),
	_canUseMultipath(false),
	_freeRandomByte((uint8_t)Utils::random()),
	_vProto(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0),
	_id(peerIdentity),
	_directPathPushCutoffCount(0),
	_credentialsCutoffCount(0),
	_linkIsBalanced(false),
	_linkIsRedundant(false),
	_remotePeerMultipathEnabled(false),
	_lastAggregateStatsReport(0),
	_lastAggregateAllocation(0)
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

	{
		Mutex::Lock _l(_paths_m);

		recordIncomingPacket(tPtr, path, packetId, payloadLength, verb, now);

		if (_canUseMultipath) {
			if (path->needsToSendQoS(now)) {
				sendQOS_MEASUREMENT(tPtr, path, path->localSocket(), path->address(), now);
			}
			for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
				if (_paths[i]) {
					_paths[i]->processBackgroundPathMeasurements(now);
				}
			}
		}
	}

	if (hops == 0) {
		// If this is a direct packet (no hops), update existing paths or learn new ones
		bool havePath = false;
		{
			Mutex::Lock _l(_paths_m);
			for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
				if (_paths[i]) {
					if (_paths[i] == path) {
						havePath = true;
						break;
					}
				} else break;
			}
		}

		bool attemptToContact = false;
		if ((!havePath)&&(RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id.address(),path->localSocket(),path->address()))) {
			Mutex::Lock _l(_paths_m);

			// Paths are redundant if they duplicate an alive path to the same IP or
			// with the same local socket and address family.
			bool redundant = false;
			unsigned int replacePath = ZT_MAX_PEER_NETWORK_PATHS;
			for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
				if (_paths[i]) {
					if ( (_paths[i]->alive(now)) && ( ((_paths[i]->localSocket() == path->localSocket())&&(_paths[i]->address().ss_family == path->address().ss_family)) || (_paths[i]->address().ipsEqual2(path->address())) ) ) {
						redundant = true;
						break;
					}
					// If the path is the same address and port, simply assume this is a replacement
					if ( (_paths[i]->address().ipsEqual2(path->address()))) {
						replacePath = i;
						break;
					}
				} else break;
			}

			// If the path isn't a duplicate of the same localSocket AND we haven't already determined a replacePath,
			// then find the worst path and replace it.
			if (!redundant && replacePath == ZT_MAX_PEER_NETWORK_PATHS) {
				int replacePathQuality = 0;
				for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
					if (_paths[i]) {
						const int q = _paths[i]->quality(now);
						if (q > replacePathQuality) {
							replacePathQuality = q;
							replacePath = i;
						}
					} else {
						replacePath = i;
						break;
					}
				}
			}

			if (replacePath != ZT_MAX_PEER_NETWORK_PATHS) {
				if (verb == Packet::VERB_OK) {
					RR->t->peerLearnedNewPath(tPtr,networkId,*this,path,packetId);
					_paths[replacePath] = path;
				} else {
					attemptToContact = true;
				}
			}
		}

		if (attemptToContact) {
			sendHELLO(tPtr,path->localSocket(),path->address(),now);
			path->sent(now);
			RR->t->peerConfirmingUnknownPath(tPtr,networkId,*this,path,packetId,verb);
		}
	}

	// Periodically push direct paths to the peer, doing so more often if we do not
	// currently have a direct path.
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
}

void Peer::recordOutgoingPacket(const SharedPtr<Path> &path, const uint64_t packetId,
	uint16_t payloadLength, const Packet::Verb verb, int64_t now)
{
	_freeRandomByte += (unsigned char)(packetId >> 8); // grab entropy to use in path selection logic for multipath
	if (_canUseMultipath) {
		path->recordOutgoingPacket(now, packetId, payloadLength, verb);
	}
}

void Peer::recordIncomingPacket(void *tPtr, const SharedPtr<Path> &path, const uint64_t packetId,
	uint16_t payloadLength, const Packet::Verb verb, int64_t now)
{
	if (_canUseMultipath) {
		if (path->needsToSendAck(now)) {
			sendACK(tPtr, path, path->localSocket(), path->address(), now);
		}
		path->recordIncomingPacket(now, packetId, payloadLength, verb);
	}
}

void Peer::computeAggregateProportionalAllocation(int64_t now)
{
	float maxStability = 0;
	float totalRelativeQuality = 0;
	float maxThroughput = 1;
	float maxScope = 0;
	float relStability[ZT_MAX_PEER_NETWORK_PATHS];
	float relThroughput[ZT_MAX_PEER_NETWORK_PATHS];
	memset(&relStability, 0, sizeof(relStability));
	memset(&relThroughput, 0, sizeof(relThroughput));
	// Survey all paths
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i]) {
			relStability[i] = _paths[i]->lastComputedStability();
			relThroughput[i] = (float)_paths[i]->maxLifetimeThroughput();
			maxStability = relStability[i] > maxStability ? relStability[i] : maxStability;
			maxThroughput = relThroughput[i] > maxThroughput ? relThroughput[i] : maxThroughput;
			maxScope = _paths[i]->ipScope() > maxScope ? _paths[i]->ipScope() : maxScope;
		}
	}
	// Convert to relative values
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i]) {
			relStability[i] /= maxStability ? maxStability : 1;
			relThroughput[i] /= maxThroughput ? maxThroughput : 1;
			float normalized_ma = Utils::normalize((float)_paths[i]->ackAge(now), 0, ZT_PATH_MAX_AGE, 0, 10);
			float age_contrib = exp((-1)*normalized_ma);
			float relScope = ((float)(_paths[i]->ipScope()+1) / (maxScope + 1));
			float relQuality =
				(relStability[i] * (float)ZT_PATH_CONTRIB_STABILITY)
				+ (fmaxf(1.0f, relThroughput[i]) * (float)ZT_PATH_CONTRIB_THROUGHPUT)
				+ relScope * (float)ZT_PATH_CONTRIB_SCOPE;
			relQuality *= age_contrib;
			// Arbitrary cutoffs
			relQuality = relQuality > (1.00f / 100.0f) ? relQuality : 0.0f;
			relQuality = relQuality < (99.0f / 100.0f) ? relQuality : 1.0f;
			totalRelativeQuality += relQuality;
			_paths[i]->updateRelativeQuality(relQuality);
		}
	}
	// Convert set of relative performances into an allocation set
	for(uint16_t i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i]) {
			_paths[i]->updateComponentAllocationOfAggregateLink((unsigned char)((_paths[i]->relativeQuality() / totalRelativeQuality) * 255));
		}
	}
}

int Peer::computeAggregateLinkPacketDelayVariance()
{
	float pdv = 0.0;
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i]) {
			pdv += _paths[i]->relativeQuality() * _paths[i]->packetDelayVariance();
		}
	}
	return (int)pdv;
}

int Peer::computeAggregateLinkMeanLatency()
{
	int ml = 0;
	int pathCount = 0;
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i]) {
			pathCount++;
			ml += (int)(_paths[i]->relativeQuality() * _paths[i]->meanLatency());
		}
	}
	return ml / pathCount;
}

int Peer::aggregateLinkPhysicalPathCount()
{
	std::map<std::string, bool> ifnamemap;
	int pathCount = 0;
	int64_t now = RR->node->now();
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i] && _paths[i]->alive(now)) {
			if (!ifnamemap[_paths[i]->getName()]) {
				ifnamemap[_paths[i]->getName()] = true;
				pathCount++;
			}
		}
	}
	return pathCount;
}

int Peer::aggregateLinkLogicalPathCount()
{
	int pathCount = 0;
	int64_t now = RR->node->now();
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i] && _paths[i]->alive(now)) {
			pathCount++;
		}
	}
	return pathCount;
}

SharedPtr<Path> Peer::getAppropriatePath(int64_t now, bool includeExpired)
{
	Mutex::Lock _l(_paths_m);
	unsigned int bestPath = ZT_MAX_PEER_NETWORK_PATHS;

	/**
	 * Send traffic across the highest quality path only. This algorithm will still
	 * use the old path quality metric from protocol version 9.
	 */
	if (!_canUseMultipath) {
		long bestPathQuality = 2147483647;
		for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (_paths[i]) {
				if ((includeExpired)||(_paths[i]->alive(now))) {
					const long q = _paths[i]->quality(now);
					if (q <= bestPathQuality) {
						bestPathQuality = q;
						bestPath = i;
					}
				}
			} else break;
		}
		if (bestPath != ZT_MAX_PEER_NETWORK_PATHS) {
			return _paths[bestPath];
		}
		return SharedPtr<Path>();
	}

	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i]) {
			_paths[i]->processBackgroundPathMeasurements(now);
		}
	}

	/**
	 * Randomly distribute traffic across all paths
	 */
	int numAlivePaths = 0;
	int numStalePaths = 0;
	if (RR->node->getMultipathMode() == ZT_MULTIPATH_RANDOM) {
		int alivePaths[ZT_MAX_PEER_NETWORK_PATHS];
		int stalePaths[ZT_MAX_PEER_NETWORK_PATHS];
		memset(&alivePaths, -1, sizeof(alivePaths));
		memset(&stalePaths, -1, sizeof(stalePaths));
		for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (_paths[i]) {
				if (_paths[i]->alive(now)) {
					alivePaths[numAlivePaths] = i;
					numAlivePaths++;
				}
				else {
					stalePaths[numStalePaths] = i;
					numStalePaths++;
				}
			}
		}
		unsigned int r = _freeRandomByte;
		if (numAlivePaths > 0) {
			int rf = r % numAlivePaths;
			return _paths[alivePaths[rf]];
		}
		else if(numStalePaths > 0) {
			// Resort to trying any non-expired path
			int rf = r % numStalePaths;
			return _paths[stalePaths[rf]];
		}
	}

	/**
	 * Proportionally allocate traffic according to dynamic path quality measurements
	 */
	if (RR->node->getMultipathMode() == ZT_MULTIPATH_PROPORTIONALLY_BALANCED) {
		if ((now - _lastAggregateAllocation) >= ZT_PATH_QUALITY_COMPUTE_INTERVAL) {
			_lastAggregateAllocation = now;
			computeAggregateProportionalAllocation(now);
		}
		// Randomly choose path according to their allocations
		float rf = _freeRandomByte;
		for(int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (_paths[i]) {
				if (rf < _paths[i]->allocation()) {
					bestPath = i;
					_pathChoiceHist.push(bestPath); // Record which path we chose
					break;
				}
				rf -= _paths[i]->allocation();
			}
		}
		if (bestPath < ZT_MAX_PEER_NETWORK_PATHS) {
			return _paths[bestPath];
		}
	}
	return SharedPtr<Path>();
}

char *Peer::interfaceListStr()
{
	std::map<std::string, int> ifnamemap;
	char tmp[32];
	const int64_t now = RR->node->now();
	char *ptr = _interfaceListStr;
	bool imbalanced = false;
	memset(_interfaceListStr, 0, sizeof(_interfaceListStr));
	int alivePathCount = aggregateLinkLogicalPathCount();
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i] && _paths[i]->alive(now)) {
			int ipv = _paths[i]->address().isV4();
			// If this is acting as an aggregate link, check allocations
			float targetAllocation = 1.0f / (float)alivePathCount;
			float currentAllocation = 1.0f;
			if (alivePathCount > 1) {
				currentAllocation = (float)_pathChoiceHist.countValue(i) / (float)_pathChoiceHist.count();
				if (fabs(targetAllocation - currentAllocation) > ZT_PATH_IMBALANCE_THRESHOLD) {
					imbalanced = true;
				}
			}
			char *ipvStr = ipv ? (char*)"ipv4" : (char*)"ipv6";
			sprintf(tmp, "(%s, %s, %.3f)", _paths[i]->getName(), ipvStr, currentAllocation);
			// Prevent duplicates
			if(ifnamemap[_paths[i]->getName()] != ipv) {
				memcpy(ptr, tmp, strlen(tmp));
				ptr += strlen(tmp);
				*ptr = ' ';
				ptr++;
				ifnamemap[_paths[i]->getName()] = ipv;
			}
		}
	}
	ptr--; // Overwrite trailing space
	if (imbalanced) {
		sprintf(tmp, ", is asymmetrical");
		memcpy(ptr, tmp, sizeof(tmp));
	} else {
		*ptr = '\0';
	}
	return _interfaceListStr;
}

void Peer::introduce(void *const tPtr,const int64_t now,const SharedPtr<Peer> &other) const
{
	unsigned int myBestV4ByScope[ZT_INETADDRESS_MAX_SCOPE+1];
	unsigned int myBestV6ByScope[ZT_INETADDRESS_MAX_SCOPE+1];
	long myBestV4QualityByScope[ZT_INETADDRESS_MAX_SCOPE+1];
	long myBestV6QualityByScope[ZT_INETADDRESS_MAX_SCOPE+1];
	unsigned int theirBestV4ByScope[ZT_INETADDRESS_MAX_SCOPE+1];
	unsigned int theirBestV6ByScope[ZT_INETADDRESS_MAX_SCOPE+1];
	long theirBestV4QualityByScope[ZT_INETADDRESS_MAX_SCOPE+1];
	long theirBestV6QualityByScope[ZT_INETADDRESS_MAX_SCOPE+1];
	for(int i=0;i<=ZT_INETADDRESS_MAX_SCOPE;++i) {
		myBestV4ByScope[i] = ZT_MAX_PEER_NETWORK_PATHS;
		myBestV6ByScope[i] = ZT_MAX_PEER_NETWORK_PATHS;
		myBestV4QualityByScope[i] = 2147483647;
		myBestV6QualityByScope[i] = 2147483647;
		theirBestV4ByScope[i] = ZT_MAX_PEER_NETWORK_PATHS;
		theirBestV6ByScope[i] = ZT_MAX_PEER_NETWORK_PATHS;
		theirBestV4QualityByScope[i] = 2147483647;
		theirBestV6QualityByScope[i] = 2147483647;
	}

	Mutex::Lock _l1(_paths_m);

	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i]) {
			const long q = _paths[i]->quality(now);
			const unsigned int s = (unsigned int)_paths[i]->ipScope();
			switch(_paths[i]->address().ss_family) {
				case AF_INET:
					if (q <= myBestV4QualityByScope[s]) {
						myBestV4QualityByScope[s] = q;
						myBestV4ByScope[s] = i;
					}
					break;
				case AF_INET6:
					if (q <= myBestV6QualityByScope[s]) {
						myBestV6QualityByScope[s] = q;
						myBestV6ByScope[s] = i;
					}
					break;
			}
		} else break;
	}

	Mutex::Lock _l2(other->_paths_m);

	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (other->_paths[i]) {
			const long q = other->_paths[i]->quality(now);
			const unsigned int s = (unsigned int)other->_paths[i]->ipScope();
			switch(other->_paths[i]->address().ss_family) {
				case AF_INET:
					if (q <= theirBestV4QualityByScope[s]) {
						theirBestV4QualityByScope[s] = q;
						theirBestV4ByScope[s] = i;
					}
					break;
				case AF_INET6:
					if (q <= theirBestV6QualityByScope[s]) {
						theirBestV6QualityByScope[s] = q;
						theirBestV6ByScope[s] = i;
					}
					break;
			}
		} else break;
	}

	unsigned int mine = ZT_MAX_PEER_NETWORK_PATHS;
	unsigned int theirs = ZT_MAX_PEER_NETWORK_PATHS;

	for(int s=ZT_INETADDRESS_MAX_SCOPE;s>=0;--s) {
		if ((myBestV6ByScope[s] != ZT_MAX_PEER_NETWORK_PATHS)&&(theirBestV6ByScope[s] != ZT_MAX_PEER_NETWORK_PATHS)) {
			mine = myBestV6ByScope[s];
			theirs = theirBestV6ByScope[s];
			break;
		}
		if ((myBestV4ByScope[s] != ZT_MAX_PEER_NETWORK_PATHS)&&(theirBestV4ByScope[s] != ZT_MAX_PEER_NETWORK_PATHS)) {
			mine = myBestV4ByScope[s];
			theirs = theirBestV4ByScope[s];
			break;
		}
	}

	if (mine != ZT_MAX_PEER_NETWORK_PATHS) {
		unsigned int alt = (unsigned int)Utils::random() & 1; // randomize which hint we send first for black magickal NAT-t reasons
		const unsigned int completed = alt + 2;
		while (alt != completed) {
			if ((alt & 1) == 0) {
				Packet outp(_id.address(),RR->identity.address(),Packet::VERB_RENDEZVOUS);
				outp.append((uint8_t)0);
				other->_id.address().appendTo(outp);
				outp.append((uint16_t)other->_paths[theirs]->address().port());
				if (other->_paths[theirs]->address().ss_family == AF_INET6) {
					outp.append((uint8_t)16);
					outp.append(other->_paths[theirs]->address().rawIpData(),16);
				} else {
					outp.append((uint8_t)4);
					outp.append(other->_paths[theirs]->address().rawIpData(),4);
				}
				outp.armor(_key,true);
				_paths[mine]->send(RR,tPtr,outp.data(),outp.size(),now);
			} else {
				Packet outp(other->_id.address(),RR->identity.address(),Packet::VERB_RENDEZVOUS);
				outp.append((uint8_t)0);
				_id.address().appendTo(outp);
				outp.append((uint16_t)_paths[mine]->address().port());
				if (_paths[mine]->address().ss_family == AF_INET6) {
					outp.append((uint8_t)16);
					outp.append(_paths[mine]->address().rawIpData(),16);
				} else {
					outp.append((uint8_t)4);
					outp.append(_paths[mine]->address().rawIpData(),4);
				}
				outp.armor(other->_key,true);
				other->_paths[theirs]->send(RR,tPtr,outp.data(),outp.size(),now);
			}
			++alt;
		}
	}
}

inline void Peer::processBackgroundPeerTasks(const int64_t now)
{
	// Determine current multipath compatibility with other peer
	if ((now - _lastMultipathCompatibilityCheck) >= ZT_PATH_QUALITY_COMPUTE_INTERVAL) {
		//
		// Cache number of available paths so that we can short-circuit multipath logic elsewhere
		//
		// We also take notice of duplicate paths (same IP only) because we may have
		// recently received a direct path push from a peer and our list might contain
		// a dead path which hasn't been fully recognized as such. In this case we
		// don't want the duplicate to trigger execution of multipath code prematurely.
		//
		// This is done to support the behavior of auto multipath enable/disable
		// without user intervention.
		//
		int currAlivePathCount = 0;
		int duplicatePathsFound = 0;
		for (unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (_paths[i]) {
				currAlivePathCount++;
				for (unsigned int j=0;j<ZT_MAX_PEER_NETWORK_PATHS;++j) {
					if (_paths[i] && _paths[j] && _paths[i]->address().ipsEqual2(_paths[j]->address()) && i != j) {
						duplicatePathsFound+=1;
						break;
					}
				}
			}
		}
		_uniqueAlivePathCount = (currAlivePathCount - (duplicatePathsFound / 2));
		_lastMultipathCompatibilityCheck = now;
		_localMultipathSupported = ((RR->node->getMultipathMode() != ZT_MULTIPATH_NONE) && (ZT_PROTO_VERSION > 9));
		_remoteMultipathSupported = _vProto > 9;
		// If both peers support multipath and more than one path exist, we can use multipath logic
		_canUseMultipath = _localMultipathSupported && _remoteMultipathSupported && (_uniqueAlivePathCount > 1);
	}
}

void Peer::sendACK(void *tPtr,const SharedPtr<Path> &path,const int64_t localSocket,const InetAddress &atAddress,int64_t now)
{
	Packet outp(_id.address(),RR->identity.address(),Packet::VERB_ACK);
	uint32_t bytesToAck = path->bytesToAck();
	outp.append<uint32_t>(bytesToAck);
	if (atAddress) {
		outp.armor(_key,false);
		RR->node->putPacket(tPtr,localSocket,atAddress,outp.data(),outp.size());
	} else {
		RR->sw->send(tPtr,outp,false);
	}
	path->sentAck(now);
}

void Peer::sendQOS_MEASUREMENT(void *tPtr,const SharedPtr<Path> &path,const int64_t localSocket,const InetAddress &atAddress,int64_t now)
{
	const int64_t _now = RR->node->now();
	Packet outp(_id.address(),RR->identity.address(),Packet::VERB_QOS_MEASUREMENT);
	char qosData[ZT_PATH_MAX_QOS_PACKET_SZ];
	int16_t len = path->generateQoSPacket(_now,qosData);
	outp.append(qosData,len);
	if (atAddress) {
		outp.armor(_key,false);
		RR->node->putPacket(tPtr,localSocket,atAddress,outp.data(),outp.size());
	} else {
		RR->sw->send(tPtr,outp,false);
	}
	path->sentQoS(now);
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

void Peer::ping(void *tPtr,int64_t now,unsigned int &v4SendCount,unsigned int &v6SendCount)
{
	Mutex::Lock _l(_paths_m);

	if (_canUseMultipath) {
		int alivePathCount = aggregateLinkPhysicalPathCount();
		if ((now - _lastAggregateStatsReport) > ZT_PATH_AGGREGATE_STATS_REPORT_INTERVAL) {
			_lastAggregateStatsReport = now;
			if (alivePathCount) {
				RR->t->peerLinkAggregateStatistics(NULL,*this);
			}
		} if (alivePathCount < 2 && _linkIsRedundant) {
			_linkIsRedundant = !_linkIsRedundant;
			RR->t->peerLinkNoLongerRedundant(NULL,*this);
		} if (alivePathCount > 1 && !_linkIsRedundant) {
			_linkIsRedundant = !_linkIsRedundant;
			RR->t->peerLinkNowRedundant(NULL,*this);
		}
	}

	unsigned int j = 0;
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if ((_paths[i])&&(_paths[i]->alive(now))) {
			sendHELLO(tPtr,_paths[i]->localSocket(),_paths[i]->address(),now);

			_paths[i]->sent(now);
			if (_paths[i]->address().isV4())
				++v4SendCount;
			else if (_paths[i]->address().isV6())
				++v6SendCount;

			if (i != j)
				_paths[j] = _paths[i];
			++j;
		}
	}
	while(j < ZT_MAX_PEER_NETWORK_PATHS) {
		_paths[j].zero();
		++j;
	}
}

void Peer::resetWithinScope(void *tPtr,InetAddress::IpScope scope,int inetAddressFamily,int64_t now)
{
	Mutex::Lock _l(_paths_m);
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i]) {
			if ((_paths[i]->address().ss_family == inetAddressFamily)&&(_paths[i]->ipScope() == scope)) {
				sendHELLO(tPtr,_paths[i]->localSocket(),_paths[i]->address(),now);
				_paths[i]->sent(now);
			}
		} else break;
	}
}

} // namespace ZeroTier
