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

#include "../version.h"
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

namespace ZeroTier {

static unsigned char s_freeRandomByteCounter = 0;

Peer::Peer(const RuntimeEnvironment *renv,const Identity &myIdentity,const Identity &peerIdentity) :
	RR(renv),
	_lastReceive(0),
	_lastNontrivialReceive(0),
	_lastTriedMemorizedPath(0),
	_lastDirectPathPushSent(0),
	_lastDirectPathPushReceive(0),
	_lastCredentialRequestSent(0),
	_lastWhoisRequestReceived(0),
	_lastEchoRequestReceived(0),
	_lastCredentialsReceived(0),
	_lastTrustEstablishedPacketReceived(0),
	_lastSentFullHello(0),
	_lastACKWindowReset(0),
	_lastQoSWindowReset(0),
	_lastMultipathCompatibilityCheck(0),
	_freeRandomByte((unsigned char)((uintptr_t)this >> 4) ^ ++s_freeRandomByteCounter),
	_uniqueAlivePathCount(0),
	_localMultipathSupported(false),
	_remoteMultipathSupported(false),
	_canUseMultipath(false),
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
	if (!myIdentity.agree(peerIdentity,_key,ZT_PEER_SECRET_KEY_LENGTH))
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
	const bool trustEstablished,
	const uint64_t networkId)
{
	const int64_t now = RR->node->now();

	_lastReceive = now;
	switch (verb) {
		case Packet::VERB_FRAME:
		case Packet::VERB_EXT_FRAME:
		case Packet::VERB_NETWORK_CONFIG_REQUEST:
		case Packet::VERB_NETWORK_CONFIG:
		case Packet::VERB_MULTICAST_FRAME:
			_lastNontrivialReceive = now;
			break;
		default:
			break;
	}

	if (trustEstablished) {
		_lastTrustEstablishedPacketReceived = now;
		path->trustedPacketReceived(now);
	}

	{
		Mutex::Lock _l(_paths_m);

		recordIncomingPacket(tPtr, path, packetId, payloadLength, verb, now);

		if (_canUseMultipath) {
			if (path->needsToSendQoS(now)) {
				sendQOS_MEASUREMENT(tPtr, path, path->localSocket(), path->address(), now);
			}
			for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
				if (_paths[i].p) {
					_paths[i].p->processBackgroundPathMeasurements(now);
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
				if (_paths[i].p) {
					if (_paths[i].p == path) {
						_paths[i].lr = now;
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
				if (_paths[i].p) {
					if ( (_paths[i].p->alive(now)) && ( ((_paths[i].p->localSocket() == path->localSocket())&&(_paths[i].p->address().ss_family == path->address().ss_family)) || (_paths[i].p->address().ipsEqual2(path->address())) ) ) {
						redundant = true;
						break;
					}
					// If the path is the same address and port, simply assume this is a replacement
					if ( (_paths[i].p->address().ipsEqual2(path->address()))) {
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
					if (_paths[i].p) {
						const int q = _paths[i].p->quality(now);
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
					_paths[replacePath].lr = now;
					_paths[replacePath].p = path;
					_paths[replacePath].priority = 1;
				} else {
					attemptToContact = true;
				}
			}
		}

		if (attemptToContact) {
			attemptToContactAt(tPtr,path->localSocket(),path->address(),now,true);
			path->sent(now);
			RR->t->peerConfirmingUnknownPath(tPtr,networkId,*this,path,packetId,verb);
		}
	}

	// If we have a trust relationship periodically push a message enumerating
	// all known external addresses for ourselves. If we already have a path this
	// is done less frequently.
	if (this->trustEstablished(now)) {
		const int64_t sinceLastPush = now - _lastDirectPathPushSent;
		if (sinceLastPush >= ((hops == 0) ? ZT_DIRECT_PATH_PUSH_INTERVAL_HAVEPATH : ZT_DIRECT_PATH_PUSH_INTERVAL)) {
			_lastDirectPathPushSent = now;
			std::vector<InetAddress> pathsToPush(RR->node->directPaths());
			if (!pathsToPush.empty()) {
				std::vector<InetAddress>::const_iterator p(pathsToPush.begin());
				while (p != pathsToPush.end()) {
					Packet *const outp = new Packet(_id.address(),RR->identity.address(),Packet::VERB_PUSH_DIRECT_PATHS);
					outp->addSize(2); // leave room for count
					unsigned int count = 0;
					while ((p != pathsToPush.end())&&((outp->size() + 24) < 1200)) {
						uint8_t addressType = 4;
						switch(p->ss_family) {
							case AF_INET:
								break;
							case AF_INET6:
								addressType = 6;
								break;
							default: // we currently only push IP addresses
								++p;
								continue;
						}

						outp->append((uint8_t)0); // no flags
						outp->append((uint16_t)0); // no extensions
						outp->append(addressType);
						outp->append((uint8_t)((addressType == 4) ? 6 : 18));
						outp->append(p->rawIpData(),((addressType == 4) ? 4 : 16));
						outp->append((uint16_t)p->port());

						++count;
						++p;
					}
					if (count) {
						outp->setAt(ZT_PACKET_IDX_PAYLOAD,(uint16_t)count);
						outp->compress();
						outp->armor(_key,true);
						path->send(RR,tPtr,outp->data(),outp->size(),now);
					}
					delete outp;
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
		if (_paths[i].p) {
			relStability[i] = _paths[i].p->lastComputedStability();
			relThroughput[i] = (float)_paths[i].p->maxLifetimeThroughput();
			maxStability = relStability[i] > maxStability ? relStability[i] : maxStability;
			maxThroughput = relThroughput[i] > maxThroughput ? relThroughput[i] : maxThroughput;
			maxScope = _paths[i].p->ipScope() > maxScope ? _paths[i].p->ipScope() : maxScope;
		}
	}
	// Convert to relative values
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i].p) {
			relStability[i] /= maxStability ? maxStability : 1;
			relThroughput[i] /= maxThroughput ? maxThroughput : 1;
			float normalized_ma = Utils::normalize((float)_paths[i].p->ackAge(now), 0, ZT_PATH_MAX_AGE, 0, 10);
			float age_contrib = exp((-1)*normalized_ma);
			float relScope = ((float)(_paths[i].p->ipScope()+1) / (maxScope + 1));
			float relQuality =
				(relStability[i] * (float)ZT_PATH_CONTRIB_STABILITY)
				+ (fmaxf(1.0f, relThroughput[i]) * (float)ZT_PATH_CONTRIB_THROUGHPUT)
				+ relScope * (float)ZT_PATH_CONTRIB_SCOPE;
			relQuality *= age_contrib;
			// Arbitrary cutoffs
			relQuality = relQuality > (1.00f / 100.0f) ? relQuality : 0.0f;
			relQuality = relQuality < (99.0f / 100.0f) ? relQuality : 1.0f;
			totalRelativeQuality += relQuality;
			_paths[i].p->updateRelativeQuality(relQuality);
		}
	}
	// Convert set of relative performances into an allocation set
	for(uint16_t i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i].p) {
			_paths[i].p->updateComponentAllocationOfAggregateLink((unsigned char)((_paths[i].p->relativeQuality() / totalRelativeQuality) * 255));
		}
	}
}

int Peer::computeAggregateLinkPacketDelayVariance()
{
	float pdv = 0.0;
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i].p) {
			pdv += _paths[i].p->relativeQuality() * _paths[i].p->packetDelayVariance();
		}
	}
	return (int)pdv;
}

int Peer::computeAggregateLinkMeanLatency()
{
	int ml = 0;
	int pathCount = 0;
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i].p) {
			pathCount++;
			ml += (int)(_paths[i].p->relativeQuality() * _paths[i].p->meanLatency());
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
		if (_paths[i].p && _paths[i].p->alive(now)) {
			if (!ifnamemap[_paths[i].p->getName()]) {
				ifnamemap[_paths[i].p->getName()] = true;
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
		if (_paths[i].p && _paths[i].p->alive(now)) {
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
			if (_paths[i].p) {
				if ((includeExpired)||((now - _paths[i].lr) < ZT_PEER_PATH_EXPIRATION)) {
					const long q = _paths[i].p->quality(now) / _paths[i].priority;
					if (q <= bestPathQuality) {
						bestPathQuality = q;
						bestPath = i;
					}
				}
			} else break;
		}
		if (bestPath != ZT_MAX_PEER_NETWORK_PATHS) {
			return _paths[bestPath].p;
		}
		return SharedPtr<Path>();
	}

	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i].p) {
			_paths[i].p->processBackgroundPathMeasurements(now);
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
			if (_paths[i].p) {
				if (_paths[i].p->alive(now)) {
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
			return _paths[alivePaths[rf]].p;
		}
		else if(numStalePaths > 0) {
			// Resort to trying any non-expired path
			int rf = r % numStalePaths;
			return _paths[stalePaths[rf]].p;
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
			if (_paths[i].p) {
				if (rf < _paths[i].p->allocation()) {
					bestPath = i;
					_pathChoiceHist.push(bestPath); // Record which path we chose
					break;
				}
				rf -= _paths[i].p->allocation();
			}
		}
		if (bestPath < ZT_MAX_PEER_NETWORK_PATHS) {
			return _paths[bestPath].p;
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
		if (_paths[i].p && _paths[i].p->alive(now)) {
			int ipv = _paths[i].p->address().isV4();
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
			sprintf(tmp, "(%s, %s, %.3f)", _paths[i].p->getName(), ipvStr, currentAllocation);
			// Prevent duplicates
			if(ifnamemap[_paths[i].p->getName()] != ipv) {
				memcpy(ptr, tmp, strlen(tmp));
				ptr += strlen(tmp);
				*ptr = ' ';
				ptr++;
				ifnamemap[_paths[i].p->getName()] = ipv;
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
		if (_paths[i].p) {
			const long q = _paths[i].p->quality(now) / _paths[i].priority;
			const unsigned int s = (unsigned int)_paths[i].p->ipScope();
			switch(_paths[i].p->address().ss_family) {
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
		if (other->_paths[i].p) {
			const long q = other->_paths[i].p->quality(now) / other->_paths[i].priority;
			const unsigned int s = (unsigned int)other->_paths[i].p->ipScope();
			switch(other->_paths[i].p->address().ss_family) {
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
		unsigned int alt = (unsigned int)RR->node->prng() & 1; // randomize which hint we send first for black magickal NAT-t reasons
		const unsigned int completed = alt + 2;
		while (alt != completed) {
			if ((alt & 1) == 0) {
				Packet outp(_id.address(),RR->identity.address(),Packet::VERB_RENDEZVOUS);
				outp.append((uint8_t)0);
				other->_id.address().appendTo(outp);
				outp.append((uint16_t)other->_paths[theirs].p->address().port());
				if (other->_paths[theirs].p->address().ss_family == AF_INET6) {
					outp.append((uint8_t)16);
					outp.append(other->_paths[theirs].p->address().rawIpData(),16);
				} else {
					outp.append((uint8_t)4);
					outp.append(other->_paths[theirs].p->address().rawIpData(),4);
				}
				outp.armor(_key,true);
				_paths[mine].p->send(RR,tPtr,outp.data(),outp.size(),now);
			} else {
				Packet outp(other->_id.address(),RR->identity.address(),Packet::VERB_RENDEZVOUS);
				outp.append((uint8_t)0);
				_id.address().appendTo(outp);
				outp.append((uint16_t)_paths[mine].p->address().port());
				if (_paths[mine].p->address().ss_family == AF_INET6) {
					outp.append((uint8_t)16);
					outp.append(_paths[mine].p->address().rawIpData(),16);
				} else {
					outp.append((uint8_t)4);
					outp.append(_paths[mine].p->address().rawIpData(),4);
				}
				outp.armor(other->_key,true);
				other->_paths[theirs].p->send(RR,tPtr,outp.data(),outp.size(),now);
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
			if (_paths[i].p) {
				currAlivePathCount++;
				for (unsigned int j=0;j<ZT_MAX_PEER_NETWORK_PATHS;++j) {
					if (_paths[i].p && _paths[j].p && _paths[i].p->address().ipsEqual2(_paths[j].p->address()) && i != j) {
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

	outp.append((uint64_t)RR->topology->planetWorldId());
	outp.append((uint64_t)RR->topology->planetWorldTimestamp());

	const unsigned int startCryptedPortionAt = outp.size();

	std::vector<World> moons(RR->topology->moons());
	std::vector<uint64_t> moonsWanted(RR->topology->moonsWanted());
	outp.append((uint16_t)(moons.size() + moonsWanted.size()));
	for(std::vector<World>::const_iterator m(moons.begin());m!=moons.end();++m) {
		outp.append((uint8_t)m->type());
		outp.append((uint64_t)m->id());
		outp.append((uint64_t)m->timestamp());
	}
	for(std::vector<uint64_t>::const_iterator m(moonsWanted.begin());m!=moonsWanted.end();++m) {
		outp.append((uint8_t)World::TYPE_MOON);
		outp.append(*m);
		outp.append((uint64_t)0);
	}

	outp.cryptField(_key,startCryptedPortionAt,outp.size() - startCryptedPortionAt);

	RR->node->expectReplyTo(outp.packetId());

	if (atAddress) {
		outp.armor(_key,false); // false == don't encrypt full payload, but add MAC
		RR->node->putPacket(tPtr,localSocket,atAddress,outp.data(),outp.size());
	} else {
		RR->sw->send(tPtr,outp,false); // false == don't encrypt full payload, but add MAC
	}
}

void Peer::attemptToContactAt(void *tPtr,const int64_t localSocket,const InetAddress &atAddress,int64_t now,bool sendFullHello)
{
	if ( (!sendFullHello) && (_vProto >= 5) && (!((_vMajor == 1)&&(_vMinor == 1)&&(_vRevision == 0))) ) {
		Packet outp(_id.address(),RR->identity.address(),Packet::VERB_ECHO);
		RR->node->expectReplyTo(outp.packetId());
		outp.armor(_key,true);
		RR->node->putPacket(tPtr,localSocket,atAddress,outp.data(),outp.size());
	} else {
		sendHELLO(tPtr,localSocket,atAddress,now);
	}
}

void Peer::tryMemorizedPath(void *tPtr,int64_t now)
{
	if ((now - _lastTriedMemorizedPath) >= ZT_TRY_MEMORIZED_PATH_INTERVAL) {
		_lastTriedMemorizedPath = now;
		InetAddress mp;
		if (RR->node->externalPathLookup(tPtr,_id.address(),-1,mp))
			attemptToContactAt(tPtr,-1,mp,now,true);
	}
}

unsigned int Peer::doPingAndKeepalive(void *tPtr,int64_t now)
{
	unsigned int sent = 0;
	Mutex::Lock _l(_paths_m);

	const bool sendFullHello = ((now - _lastSentFullHello) >= ZT_PEER_PING_PERIOD);
	_lastSentFullHello = now;

	processBackgroundPeerTasks(now);

	// Emit traces regarding aggregate link status
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

	// Right now we only keep pinging links that have the maximum priority. The
	// priority is used to track cluster redirections, meaning that when a cluster
	// redirects us its redirect target links override all other links and we
	// let those old links expire.
	long maxPriority = 0;
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i].p)
			maxPriority = std::max(_paths[i].priority,maxPriority);
		else break;
	}

	unsigned int j = 0;
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i].p) {
			// Clean expired and reduced priority paths
			if ( ((now - _paths[i].lr) < ZT_PEER_PATH_EXPIRATION) && (_paths[i].priority == maxPriority) ) {
				if ((sendFullHello)||(_paths[i].p->needsHeartbeat(now))) {
					attemptToContactAt(tPtr,_paths[i].p->localSocket(),_paths[i].p->address(),now,sendFullHello);
					_paths[i].p->sent(now);
					sent |= (_paths[i].p->address().ss_family == AF_INET) ? 0x1 : 0x2;
				}
				if (i != j)
					_paths[j] = _paths[i];
				++j;
			}
		} else break;
	}
	if (canUseMultipath()) {
		while(j < ZT_MAX_PEER_NETWORK_PATHS) {
			_paths[j].lr = 0;
			_paths[j].p.zero();
			_paths[j].priority = 1;
			++j;
		}
	}
	return sent;
}

void Peer::clusterRedirect(void *tPtr,const SharedPtr<Path> &originatingPath,const InetAddress &remoteAddress,const int64_t now)
{
	SharedPtr<Path> np(RR->topology->getPath(originatingPath->localSocket(),remoteAddress));
	RR->t->peerRedirected(tPtr,0,*this,np);

	attemptToContactAt(tPtr,originatingPath->localSocket(),remoteAddress,now,true);

	{
		Mutex::Lock _l(_paths_m);

		// New priority is higher than the priority of the originating path (if known)
		long newPriority = 1;
		for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (_paths[i].p) {
				if (_paths[i].p == originatingPath) {
					newPriority = _paths[i].priority;
					break;
				}
			} else break;
		}
		newPriority += 2;

		// Erase any paths with lower priority than this one or that are duplicate
		// IPs and add this path.
		unsigned int j = 0;
		for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if (_paths[i].p) {
				if ((_paths[i].priority >= newPriority)&&(!_paths[i].p->address().ipsEqual2(remoteAddress))) {
					if (i != j)
						_paths[j] = _paths[i];
					++j;
				}
			}
		}
		if (j < ZT_MAX_PEER_NETWORK_PATHS) {
			_paths[j].lr = now;
			_paths[j].p = np;
			_paths[j].priority = newPriority;
			++j;
			while (j < ZT_MAX_PEER_NETWORK_PATHS) {
				_paths[j].lr = 0;
				_paths[j].p.zero();
				_paths[j].priority = 1;
				++j;
			}
		}
	}
}

void Peer::resetWithinScope(void *tPtr,InetAddress::IpScope scope,int inetAddressFamily,int64_t now)
{
	Mutex::Lock _l(_paths_m);
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i].p) {
			if ((_paths[i].p->address().ss_family == inetAddressFamily)&&(_paths[i].p->ipScope() == scope)) {
				attemptToContactAt(tPtr,_paths[i].p->localSocket(),_paths[i].p->address(),now,false);
				_paths[i].p->sent(now);
				_paths[i].lr = 0; // path will not be used unless it speaks again
			}
		} else break;
	}
}

} // namespace ZeroTier
