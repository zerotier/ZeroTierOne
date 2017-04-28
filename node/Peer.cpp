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

#include "../version.h"

#include "Constants.hpp"
#include "Peer.hpp"
#include "Node.hpp"
#include "Switch.hpp"
#include "Network.hpp"
#include "SelfAwareness.hpp"
#include "Cluster.hpp"
#include "Packet.hpp"

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
	_vProto(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0),
	_id(peerIdentity),
	_latency(0),
	_directPathPushCutoffCount(0),
	_credentialsCutoffCount(0)
{
	if (!myIdentity.agree(peerIdentity,_key,ZT_PEER_SECRET_KEY_LENGTH))
		throw std::runtime_error("new peer identity key agreement failed");
}

void Peer::received(
	void *tPtr,
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
	bool isClusterSuboptimalPath = false;
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
				path->send(RR,tPtr,outp.data(),outp.size(),now);
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
				path->send(RR,tPtr,outp.data(),outp.size(),now);
			}
			isClusterSuboptimalPath = true;
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
		bool pathAlreadyKnown = false;
		{
			Mutex::Lock _l(_paths_m);
			if ((path->address().ss_family == AF_INET)&&(_v4Path.p)) {
				const struct sockaddr_in *const r = reinterpret_cast<const struct sockaddr_in *>(&(path->address()));
				const struct sockaddr_in *const l = reinterpret_cast<const struct sockaddr_in *>(&(_v4Path.p->address()));
				const struct sockaddr_in *const rl = reinterpret_cast<const struct sockaddr_in *>(&(path->localAddress()));
				const struct sockaddr_in *const ll = reinterpret_cast<const struct sockaddr_in *>(&(_v4Path.p->localAddress()));
				if ((r->sin_addr.s_addr == l->sin_addr.s_addr)&&(r->sin_port == l->sin_port)&&(rl->sin_addr.s_addr == ll->sin_addr.s_addr)&&(rl->sin_port == ll->sin_port)) {
					_v4Path.lr = now;
#ifdef ZT_ENABLE_CLUSTER
					_v4Path.localClusterSuboptimal = isClusterSuboptimalPath;
#endif
					pathAlreadyKnown = true;
				}
			} else if ((path->address().ss_family == AF_INET6)&&(_v6Path.p)) {
				const struct sockaddr_in6 *const r = reinterpret_cast<const struct sockaddr_in6 *>(&(path->address()));
				const struct sockaddr_in6 *const l = reinterpret_cast<const struct sockaddr_in6 *>(&(_v6Path.p->address()));
				const struct sockaddr_in6 *const rl = reinterpret_cast<const struct sockaddr_in6 *>(&(path->localAddress()));
				const struct sockaddr_in6 *const ll = reinterpret_cast<const struct sockaddr_in6 *>(&(_v6Path.p->localAddress()));
				if ((!memcmp(r->sin6_addr.s6_addr,l->sin6_addr.s6_addr,16))&&(r->sin6_port == l->sin6_port)&&(!memcmp(rl->sin6_addr.s6_addr,ll->sin6_addr.s6_addr,16))&&(rl->sin6_port == ll->sin6_port)) {
					_v6Path.lr = now;
#ifdef ZT_ENABLE_CLUSTER
					_v6Path.localClusterSuboptimal = isClusterSuboptimalPath;
#endif
					pathAlreadyKnown = true;
				}
			}
		}

		if ( (!pathAlreadyKnown) && (RR->node->shouldUsePathForZeroTierTraffic(tPtr,_id.address(),path->localAddress(),path->address())) ) {
			Mutex::Lock _l(_paths_m);
			_PeerPath *potentialNewPeerPath = (_PeerPath *)0;
			if (path->address().ss_family == AF_INET) {
				if ( (!_v4Path.p) || (!_v4Path.p->alive(now)) || ((_v4Path.p->address() != _v4ClusterPreferred)&&(path->preferenceRank() >= _v4Path.p->preferenceRank())) ) {
					potentialNewPeerPath = &_v4Path;
				}
			} else if (path->address().ss_family == AF_INET6) {
				if ( (!_v6Path.p) || (!_v6Path.p->alive(now)) || ((_v6Path.p->address() != _v6ClusterPreferred)&&(path->preferenceRank() >= _v6Path.p->preferenceRank())) ) {
					potentialNewPeerPath = &_v6Path;
				}
			}
			if (potentialNewPeerPath) {
				if (verb == Packet::VERB_OK) {
					potentialNewPeerPath->lr = now;
					potentialNewPeerPath->p = path;
#ifdef ZT_ENABLE_CLUSTER
					potentialNewPeerPath->localClusterSuboptimal = isClusterSuboptimalPath;
					if (RR->cluster)
						RR->cluster->broadcastHavePeer(_id);
#endif
				} else {
					TRACE("got %s via unknown path %s(%s), confirming...",Packet::verbString(verb),_id.address().toString().c_str(),path->address().toString().c_str());
					attemptToContactAt(tPtr,path->localAddress(),path->address(),now,true,path->nextOutgoingCounter());
					path->sent(now);
				}
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
						path->send(RR,tPtr,outp.data(),outp.size(),now);
					}
				}
			}
		}
	}
}

bool Peer::sendDirect(void *tPtr,const void *data,unsigned int len,uint64_t now,bool force)
{
	Mutex::Lock _l(_paths_m);

	uint64_t v6lr = 0;
	if ( ((now - _v6Path.lr) < ZT_PEER_PATH_EXPIRATION) && (_v6Path.p) )
		v6lr = _v6Path.p->lastIn();
	uint64_t v4lr = 0;
	if ( ((now - _v4Path.lr) < ZT_PEER_PATH_EXPIRATION) && (_v4Path.p) )
		v4lr = _v4Path.p->lastIn();

	if ( (v6lr > v4lr) && ((now - v6lr) < ZT_PATH_ALIVE_TIMEOUT) ) {
		return _v6Path.p->send(RR,tPtr,data,len,now);
	} else if ((now - v4lr) < ZT_PATH_ALIVE_TIMEOUT) {
		return _v4Path.p->send(RR,tPtr,data,len,now);
	} else if (force) {
		if (v6lr > v4lr) {
			return _v6Path.p->send(RR,tPtr,data,len,now);
		} else if (v4lr) {
			return _v4Path.p->send(RR,tPtr,data,len,now);
		}
	}

	return false;
}

SharedPtr<Path> Peer::getBestPath(uint64_t now,bool includeExpired)
{
	Mutex::Lock _l(_paths_m);

	uint64_t v6lr = 0;
	if ( ( includeExpired || ((now - _v6Path.lr) < ZT_PEER_PATH_EXPIRATION) ) && (_v6Path.p) )
		v6lr = _v6Path.p->lastIn();
	uint64_t v4lr = 0;
	if ( ( includeExpired || ((now - _v4Path.lr) < ZT_PEER_PATH_EXPIRATION) ) && (_v4Path.p) )
		v4lr = _v4Path.p->lastIn();

	if (v6lr > v4lr) {
		return _v6Path.p;
	} else if (v4lr) {
		return _v4Path.p;
	}

	return SharedPtr<Path>();
}

void Peer::sendHELLO(void *tPtr,const InetAddress &localAddr,const InetAddress &atAddress,uint64_t now,unsigned int counter)
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
		RR->node->putPacket(tPtr,localAddr,atAddress,outp.data(),outp.size());
	} else {
		RR->sw->send(tPtr,outp,false); // false == don't encrypt full payload, but add MAC
	}
}

void Peer::attemptToContactAt(void *tPtr,const InetAddress &localAddr,const InetAddress &atAddress,uint64_t now,bool sendFullHello,unsigned int counter)
{
	if ( (!sendFullHello) && (_vProto >= 5) && (!((_vMajor == 1)&&(_vMinor == 1)&&(_vRevision == 0))) ) {
		Packet outp(_id.address(),RR->identity.address(),Packet::VERB_ECHO);
		RR->node->expectReplyTo(outp.packetId());
		outp.armor(_key,true,counter);
		RR->node->putPacket(tPtr,localAddr,atAddress,outp.data(),outp.size());
	} else {
		sendHELLO(tPtr,localAddr,atAddress,now,counter);
	}
}

void Peer::tryMemorizedPath(void *tPtr,uint64_t now)
{
	if ((now - _lastTriedMemorizedPath) >= ZT_TRY_MEMORIZED_PATH_INTERVAL) {
		_lastTriedMemorizedPath = now;
		InetAddress mp;
		if (RR->node->externalPathLookup(tPtr,_id.address(),-1,mp))
			attemptToContactAt(tPtr,InetAddress(),mp,now,true,0);
	}
}

bool Peer::doPingAndKeepalive(void *tPtr,uint64_t now,int inetAddressFamily)
{
	Mutex::Lock _l(_paths_m);

	if (inetAddressFamily < 0) {
		uint64_t v6lr = 0;
		if ( ((now - _v6Path.lr) < ZT_PEER_PATH_EXPIRATION) && (_v6Path.p) )
			v6lr = _v6Path.p->lastIn();
		uint64_t v4lr = 0;
		if ( ((now - _v4Path.lr) < ZT_PEER_PATH_EXPIRATION) && (_v4Path.p) )
			v4lr = _v4Path.p->lastIn();

		if (v6lr > v4lr) {
			if ( ((now - _v6Path.lr) >= ZT_PEER_PING_PERIOD) || (_v6Path.p->needsHeartbeat(now)) ) {
				attemptToContactAt(tPtr,_v6Path.p->localAddress(),_v6Path.p->address(),now,false,_v6Path.p->nextOutgoingCounter());
				_v6Path.p->sent(now);
				return true;
			}
		} else if (v4lr) {
			if ( ((now - _v4Path.lr) >= ZT_PEER_PING_PERIOD) || (_v4Path.p->needsHeartbeat(now)) ) {
				attemptToContactAt(tPtr,_v4Path.p->localAddress(),_v4Path.p->address(),now,false,_v4Path.p->nextOutgoingCounter());
				_v4Path.p->sent(now);
				return true;
			}
		}
	} else {
		if ( (inetAddressFamily == AF_INET) && ((now - _v4Path.lr) < ZT_PEER_PATH_EXPIRATION) ) {
			if ( ((now - _v4Path.lr) >= ZT_PEER_PING_PERIOD) || (_v4Path.p->needsHeartbeat(now)) ) {
				attemptToContactAt(tPtr,_v4Path.p->localAddress(),_v4Path.p->address(),now,false,_v4Path.p->nextOutgoingCounter());
				_v4Path.p->sent(now);
				return true;
			}
		} else if ( (inetAddressFamily == AF_INET6) && ((now - _v6Path.lr) < ZT_PEER_PATH_EXPIRATION) ) {
			if ( ((now - _v6Path.lr) >= ZT_PEER_PING_PERIOD) || (_v6Path.p->needsHeartbeat(now)) ) {
				attemptToContactAt(tPtr,_v6Path.p->localAddress(),_v6Path.p->address(),now,false,_v6Path.p->nextOutgoingCounter());
				_v6Path.p->sent(now);
				return true;
			}
		}
	}

	return false;
}

} // namespace ZeroTier
