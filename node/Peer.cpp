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

#include "../version.h"

#include "Constants.hpp"
#include "Peer.hpp"
#include "Node.hpp"
#include "Switch.hpp"
#include "Network.hpp"
#include "SelfAwareness.hpp"
#include "Cluster.hpp"
#include "Packet.hpp"

#ifndef AF_MAX
#if AF_INET > AF_INET6
#define AF_MAX AF_INET
#else
#define AF_MAX AF_INET6
#endif
#endif

namespace ZeroTier {

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
	_lastComRequestReceived(0),
	_lastComRequestSent(0),
	_lastCredentialsReceived(0),
	_lastTrustEstablishedPacketReceived(0),
	_remoteClusterOptimal4(0),
	_vProto(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0),
	_id(peerIdentity),
	_numPaths(0),
	_latency(0),
	_directPathPushCutoffCount(0),
	_credentialsCutoffCount(0)
{
	memset(_remoteClusterOptimal6,0,sizeof(_remoteClusterOptimal6));
	if (!myIdentity.agree(peerIdentity,_key,ZT_PEER_SECRET_KEY_LENGTH))
		throw std::runtime_error("new peer identity key agreement failed");
}

void Peer::received(
	const SharedPtr<Path> &path,
	const unsigned int hops,
	const uint64_t packetId,
	const Packet::Verb verb,
	const uint64_t inRePacketId,
	const Packet::Verb inReVerb,
	const bool trustEstablished)
{
	const uint64_t now = RR->node->now();

#ifdef ZT_ENABLE_CLUSTER
	bool suboptimalPath = false;
	if ((RR->cluster)&&(hops == 0)) {
		// Note: findBetterEndpoint() is first since we still want to check
		// for a better endpoint even if we don't actually send a redirect.
		InetAddress redirectTo;
		if ( (verb != Packet::VERB_OK) && (verb != Packet::VERB_ERROR) && (verb != Packet::VERB_RENDEZVOUS) && (verb != Packet::VERB_PUSH_DIRECT_PATHS) && (RR->cluster->findBetterEndpoint(redirectTo,_id.address(),path->address(),false)) ) {
			if (_vProto >= 5) {
				// For newer peers we can send a more idiomatic verb: PUSH_DIRECT_PATHS.
				Packet outp(_id.address(),RR->identity.address(),Packet::VERB_PUSH_DIRECT_PATHS);
				outp.append((uint16_t)1); // count == 1
				outp.append((uint8_t)ZT_PUSH_DIRECT_PATHS_FLAG_CLUSTER_REDIRECT); // flags: cluster redirect
				outp.append((uint16_t)0); // no extensions
				if (redirectTo.ss_family == AF_INET) {
					outp.append((uint8_t)4);
					outp.append((uint8_t)6);
					outp.append(redirectTo.rawIpData(),4);
				} else {
					outp.append((uint8_t)6);
					outp.append((uint8_t)18);
					outp.append(redirectTo.rawIpData(),16);
				}
				outp.append((uint16_t)redirectTo.port());
				outp.armor(_key,true,path->nextOutgoingCounter());
				path->send(RR,outp.data(),outp.size(),now);
			} else {
				// For older peers we use RENDEZVOUS to coax them into contacting us elsewhere.
				Packet outp(_id.address(),RR->identity.address(),Packet::VERB_RENDEZVOUS);
				outp.append((uint8_t)0); // no flags
				RR->identity.address().appendTo(outp);
				outp.append((uint16_t)redirectTo.port());
				if (redirectTo.ss_family == AF_INET) {
					outp.append((uint8_t)4);
					outp.append(redirectTo.rawIpData(),4);
				} else {
					outp.append((uint8_t)16);
					outp.append(redirectTo.rawIpData(),16);
				}
				outp.armor(_key,true,path->nextOutgoingCounter());
				path->send(RR,outp.data(),outp.size(),now);
			}
			suboptimalPath = true;
		}
	}
#endif

	_lastReceive = now;
	switch (verb) {
		case Packet::VERB_FRAME:
		case Packet::VERB_EXT_FRAME:
		case Packet::VERB_NETWORK_CONFIG_REQUEST:
		case Packet::VERB_NETWORK_CONFIG:
		case Packet::VERB_MULTICAST_FRAME:
			_lastNontrivialReceive = now;
			break;
		default: break;
	}

	if (trustEstablished) {
		_lastTrustEstablishedPacketReceived = now;
		path->trustedPacketReceived(now);
	}

	if (_vProto >= 9)
		path->updateLinkQuality((unsigned int)(packetId & 7));

	if (hops == 0) {
		bool pathIsConfirmed = false;
		{
			Mutex::Lock _l(_paths_m);
			for(unsigned int p=0;p<_numPaths;++p) {
				if (_paths[p].path->address() == path->address()) {
					_paths[p].lastReceive = now;
					_paths[p].path = path; // local address may have changed!
#ifdef ZT_ENABLE_CLUSTER
					_paths[p].localClusterSuboptimal = suboptimalPath;
#endif
					pathIsConfirmed = true;
					break;
				}
			}
		}

		if ( (!pathIsConfirmed) && (RR->node->shouldUsePathForZeroTierTraffic(_id.address(),path->localAddress(),path->address())) ) {
			if (verb == Packet::VERB_OK) {
				Mutex::Lock _l(_paths_m);

				// Since this is a new path, figure out where to put it (possibly replacing an old/dead one)
				unsigned int slot;
				if (_numPaths < ZT_MAX_PEER_NETWORK_PATHS) {
					slot = _numPaths++;
				} else {
					// First try to replace the worst within the same address family, if possible
					int worstSlot = -1;
					uint64_t worstScore = 0xffffffffffffffffULL;
					for(unsigned int p=0;p<_numPaths;++p) {
						if (_paths[p].path->address().ss_family == path->address().ss_family) {
							const uint64_t s = _pathScore(p,now);
							if (s < worstScore) {
								worstScore = s;
								worstSlot = (int)p;
							}
						}
					}
					if (worstSlot >= 0) {
						slot = (unsigned int)worstSlot;
					} else {
						// If we can't find one with the same family, replace the worst of any family
						slot = ZT_MAX_PEER_NETWORK_PATHS - 1;
						for(unsigned int p=0;p<_numPaths;++p) {
							const uint64_t s = _pathScore(p,now);
							if (s < worstScore) {
								worstScore = s;
								slot = p;
							}
						}
					}
				}

				_paths[slot].lastReceive = now;
				_paths[slot].path = path;
#ifdef ZT_ENABLE_CLUSTER
				_paths[slot].localClusterSuboptimal = suboptimalPath;
				if (RR->cluster)
					RR->cluster->broadcastHavePeer(_id);
#endif
			} else {
				TRACE("got %s via unknown path %s(%s), confirming...",Packet::verbString(verb),_id.address().toString().c_str(),path->address().toString().c_str());
				attemptToContactAt(path->localAddress(),path->address(),now,true,path->nextOutgoingCounter());
				path->sent(now);
			}
		}
	} else if (this->trustEstablished(now)) {
		// Send PUSH_DIRECT_PATHS if hops>0 (relayed) and we have a trust relationship (common network membership)
#ifdef ZT_ENABLE_CLUSTER
			// Cluster mode disables normal PUSH_DIRECT_PATHS in favor of cluster-based peer redirection
			const bool haveCluster = (RR->cluster);
#else
			const bool haveCluster = false;
#endif
		if ( ((now - _lastDirectPathPushSent) >= ZT_DIRECT_PATH_PUSH_INTERVAL) && (!haveCluster) ) {
			_lastDirectPathPushSent = now;

			std::vector<InetAddress> pathsToPush;

			std::vector<InetAddress> dps(RR->node->directPaths());
			for(std::vector<InetAddress>::const_iterator i(dps.begin());i!=dps.end();++i)
				pathsToPush.push_back(*i);

			std::vector<InetAddress> sym(RR->sa->getSymmetricNatPredictions());
			for(unsigned long i=0,added=0;i<sym.size();++i) {
				InetAddress tmp(sym[(unsigned long)RR->node->prng() % sym.size()]);
				if (std::find(pathsToPush.begin(),pathsToPush.end(),tmp) == pathsToPush.end()) {
					pathsToPush.push_back(tmp);
					if (++added >= ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY)
						break;
				}
			}

			if (pathsToPush.size() > 0) {
#ifdef ZT_TRACE
				std::string ps;
				for(std::vector<InetAddress>::const_iterator p(pathsToPush.begin());p!=pathsToPush.end();++p) {
					if (ps.length() > 0)
						ps.push_back(',');
					ps.append(p->toString());
				}
				TRACE("pushing %u direct paths to %s: %s",(unsigned int)pathsToPush.size(),_id.address().toString().c_str(),ps.c_str());
#endif

				std::vector<InetAddress>::const_iterator p(pathsToPush.begin());
				while (p != pathsToPush.end()) {
					Packet outp(_id.address(),RR->identity.address(),Packet::VERB_PUSH_DIRECT_PATHS);
					outp.addSize(2); // leave room for count

					unsigned int count = 0;
					while ((p != pathsToPush.end())&&((outp.size() + 24) < 1200)) {
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

						outp.append((uint8_t)0); // no flags
						outp.append((uint16_t)0); // no extensions
						outp.append(addressType);
						outp.append((uint8_t)((addressType == 4) ? 6 : 18));
						outp.append(p->rawIpData(),((addressType == 4) ? 4 : 16));
						outp.append((uint16_t)p->port());

						++count;
						++p;
					}

					if (count) {
						outp.setAt(ZT_PACKET_IDX_PAYLOAD,(uint16_t)count);
						outp.armor(_key,true,path->nextOutgoingCounter());
						path->send(RR,outp.data(),outp.size(),now);
					}
				}
			}
		}
	}
}

bool Peer::hasActivePathTo(uint64_t now,const InetAddress &addr) const
{
	Mutex::Lock _l(_paths_m);
	for(unsigned int p=0;p<_numPaths;++p) {
		if ( (_paths[p].path->address() == addr) && ((now - _paths[p].lastReceive) <= ZT_PEER_PATH_EXPIRATION) && (_paths[p].path->alive(now)) )
			return true; 
	}
	return false;
}

bool Peer::sendDirect(const void *data,unsigned int len,uint64_t now,bool forceEvenIfDead)
{
	Mutex::Lock _l(_paths_m);

	int bestp = -1;
	uint64_t best = 0ULL;
	for(unsigned int p=0;p<_numPaths;++p) {
		if ( ((now - _paths[p].lastReceive) <= ZT_PEER_PATH_EXPIRATION) && (_paths[p].path->alive(now)||(forceEvenIfDead)) ) {
			const uint64_t s = _pathScore(p,now);
			if (s >= best) {
				best = s;
				bestp = (int)p;
			}
		}
	}

	if (bestp >= 0) {
		return _paths[bestp].path->send(RR,data,len,now);
	} else {
		return false;
	}
}

SharedPtr<Path> Peer::getBestPath(uint64_t now,bool includeExpired)
{
	Mutex::Lock _l(_paths_m);

	int bestp = -1;
	uint64_t best = 0ULL;
	for(unsigned int p=0;p<_numPaths;++p) {
		if ( ((now - _paths[p].lastReceive) <= ZT_PEER_PATH_EXPIRATION) || (includeExpired) ) {
			const uint64_t s = _pathScore(p,now);
			if (s >= best) {
				best = s;
				bestp = (int)p;
			}
		}
	}

	if (bestp >= 0) {
		return _paths[bestp].path;
	} else {
		return SharedPtr<Path>();
	}
}

void Peer::sendHELLO(const InetAddress &localAddr,const InetAddress &atAddress,uint64_t now,unsigned int counter)
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

	const unsigned int corSizeAt = outp.size();
	outp.addSize(2);
	RR->topology->appendCertificateOfRepresentation(outp);
	outp.setAt(corSizeAt,(uint16_t)(outp.size() - (corSizeAt + 2)));

	outp.cryptField(_key,startCryptedPortionAt,outp.size() - startCryptedPortionAt);

	RR->node->expectReplyTo(outp.packetId());

	if (atAddress) {
		outp.armor(_key,false,counter); // false == don't encrypt full payload, but add MAC
		RR->node->putPacket(localAddr,atAddress,outp.data(),outp.size());
	} else {
		RR->sw->send(outp,false); // false == don't encrypt full payload, but add MAC
	}
}

void Peer::attemptToContactAt(const InetAddress &localAddr,const InetAddress &atAddress,uint64_t now,bool sendFullHello,unsigned int counter)
{
	if ( (!sendFullHello) && (_vProto >= 5) && (!((_vMajor == 1)&&(_vMinor == 1)&&(_vRevision == 0))) ) {
		Packet outp(_id.address(),RR->identity.address(),Packet::VERB_ECHO);
		RR->node->expectReplyTo(outp.packetId());
		outp.armor(_key,true,counter);
		RR->node->putPacket(localAddr,atAddress,outp.data(),outp.size());
	} else {
		sendHELLO(localAddr,atAddress,now,counter);
	}
}

void Peer::tryMemorizedPath(uint64_t now)
{
	if ((now - _lastTriedMemorizedPath) >= ZT_TRY_MEMORIZED_PATH_INTERVAL) {
		_lastTriedMemorizedPath = now;
		InetAddress mp;
		if (RR->node->externalPathLookup(_id.address(),-1,mp))
			attemptToContactAt(InetAddress(),mp,now,true,0);
	}
}

bool Peer::doPingAndKeepalive(uint64_t now,int inetAddressFamily)
{
	Mutex::Lock _l(_paths_m);

	int bestp = -1;
	uint64_t best = 0ULL;
	for(unsigned int p=0;p<_numPaths;++p) {
		if ( ((now - _paths[p].lastReceive) <= ZT_PEER_PATH_EXPIRATION) && ((inetAddressFamily < 0)||((int)_paths[p].path->address().ss_family == inetAddressFamily)) ) {
			const uint64_t s = _pathScore(p,now);
			if (s >= best) {
				best = s;
				bestp = (int)p;
			}
		}
	}

	if (bestp >= 0) {
		if ( ((now - _paths[bestp].lastReceive) >= ZT_PEER_PING_PERIOD) || (_paths[bestp].path->needsHeartbeat(now)) ) {
			attemptToContactAt(_paths[bestp].path->localAddress(),_paths[bestp].path->address(),now,false,_paths[bestp].path->nextOutgoingCounter());
			_paths[bestp].path->sent(now);
		}
		return true;
	} else {
		return false;
	}
}

bool Peer::hasActiveDirectPath(uint64_t now) const
{
	Mutex::Lock _l(_paths_m);
	for(unsigned int p=0;p<_numPaths;++p) {
		if (((now - _paths[p].lastReceive) <= ZT_PEER_PATH_EXPIRATION)&&(_paths[p].path->alive(now)))
			return true;
	}
	return false;
}

void Peer::resetWithinScope(InetAddress::IpScope scope,int inetAddressFamily,uint64_t now)
{
	Mutex::Lock _l(_paths_m);
	for(unsigned int p=0;p<_numPaths;++p) {
		if ( (_paths[p].path->address().ss_family == inetAddressFamily) && (_paths[p].path->address().ipScope() == scope) ) {
			attemptToContactAt(_paths[p].path->localAddress(),_paths[p].path->address(),now,false,_paths[p].path->nextOutgoingCounter());
			_paths[p].path->sent(now);
			_paths[p].lastReceive = 0; // path will not be used unless it speaks again
		}
	}
}

void Peer::getRendezvousAddresses(uint64_t now,InetAddress &v4,InetAddress &v6) const
{
	Mutex::Lock _l(_paths_m);

	int bestp4 = -1,bestp6 = -1;
	uint64_t best4 = 0ULL,best6 = 0ULL;
	for(unsigned int p=0;p<_numPaths;++p) {
		if ( ((now - _paths[p].lastReceive) <= ZT_PEER_PATH_EXPIRATION) && (_paths[p].path->alive(now)) ) {
			if (_paths[p].path->address().ss_family == AF_INET) {
				const uint64_t s = _pathScore(p,now);
				if (s >= best4) {
					best4 = s;
					bestp4 = (int)p;
				}
			} else if (_paths[p].path->address().ss_family == AF_INET6) {
				const uint64_t s = _pathScore(p,now);
				if (s >= best6) {
					best6 = s;
					bestp6 = (int)p;
				}
			}
		}
	}

	if (bestp4 >= 0)
		v4 = _paths[bestp4].path->address();
	if (bestp6 >= 0)
		v6 = _paths[bestp6].path->address();
}

} // namespace ZeroTier
