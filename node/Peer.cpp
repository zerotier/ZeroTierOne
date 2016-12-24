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

#include <algorithm>

#define ZT_PEER_PATH_SORT_INTERVAL 5000

namespace ZeroTier {

// Used to send varying values for NAT keepalive
static uint32_t _natKeepaliveBuf = 0;

Peer::Peer(const RuntimeEnvironment *renv,const Identity &myIdentity,const Identity &peerIdentity) :
	RR(renv),
	_lastUsed(0),
	_lastReceive(0),
	_lastUnicastFrame(0),
	_lastMulticastFrame(0),
	_lastAnnouncedTo(0),
	_lastDirectPathPushSent(0),
	_lastDirectPathPushReceive(0),
	_lastPathSort(0),
	_vProto(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0),
	_id(peerIdentity),
	_numPaths(0),
	_latency(0),
	_directPathPushCutoffCount(0),
	_networkComs(4),
	_lastPushedComs(4)
{
	if (!myIdentity.agree(peerIdentity,_key,ZT_PEER_SECRET_KEY_LENGTH))
		throw std::runtime_error("new peer identity key agreement failed");
}

void Peer::received(
	const InetAddress &localAddr,
	const InetAddress &remoteAddr,
	unsigned int hops,
	uint64_t packetId,
	Packet::Verb verb,
	uint64_t inRePacketId,
	Packet::Verb inReVerb)
{
#ifdef ZT_ENABLE_CLUSTER
	bool suboptimalPath = false;
	if ((RR->cluster)&&(hops == 0)) {
		// Note: findBetterEndpoint() is first since we still want to check
		// for a better endpoint even if we don't actually send a redirect.
		InetAddress redirectTo;
		if ( (verb != Packet::VERB_OK) && (verb != Packet::VERB_ERROR) && (verb != Packet::VERB_RENDEZVOUS) && (verb != Packet::VERB_PUSH_DIRECT_PATHS) && (RR->cluster->findBetterEndpoint(redirectTo,_id.address(),remoteAddr,false)) ) {
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
				outp.armor(_key,true);
				RR->node->putPacket(localAddr,remoteAddr,outp.data(),outp.size());
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
				outp.armor(_key,true);
				RR->node->putPacket(localAddr,remoteAddr,outp.data(),outp.size());
			}
			suboptimalPath = true;
		}
	}
#endif

	const uint64_t now = RR->node->now();
	_lastReceive = now;
	if ((verb == Packet::VERB_FRAME)||(verb == Packet::VERB_EXT_FRAME))
		_lastUnicastFrame = now;
	else if (verb == Packet::VERB_MULTICAST_FRAME)
		_lastMulticastFrame = now;

	if (hops == 0) {
		bool pathIsConfirmed = false;
		unsigned int np = _numPaths;
		for(unsigned int p=0;p<np;++p) {
			if ((_paths[p].address() == remoteAddr)&&(_paths[p].localAddress() == localAddr)) {
				_paths[p].received(now);
#ifdef ZT_ENABLE_CLUSTER
				_paths[p].setClusterSuboptimal(suboptimalPath);
#endif
				pathIsConfirmed = true;
				break;
			}
		}

		if ((!pathIsConfirmed)&&(RR->node->shouldUsePathForZeroTierTraffic(localAddr,remoteAddr))) {
			if (verb == Packet::VERB_OK) {

				Path *slot = (Path *)0;
				if (np < ZT_MAX_PEER_NETWORK_PATHS) {
					slot = &(_paths[np++]);
				} else {
					uint64_t slotWorstScore = 0xffffffffffffffffULL;
					for(unsigned int p=0;p<ZT_MAX_PEER_NETWORK_PATHS;++p) {
						if (!_paths[p].active(now)) {
							slot = &(_paths[p]);
							break;
						} else {
							const uint64_t score = _paths[p].score();
							if (score <= slotWorstScore) {
								slotWorstScore = score;
								slot = &(_paths[p]);
							}
						}
					}
				}
				if (slot) {
					*slot = Path(localAddr,remoteAddr);
					slot->received(now);
#ifdef ZT_ENABLE_CLUSTER
					slot->setClusterSuboptimal(suboptimalPath);
#endif
					_numPaths = np;
				}

#ifdef ZT_ENABLE_CLUSTER
				if (RR->cluster)
					RR->cluster->broadcastHavePeer(_id);
#endif

			} else {

				TRACE("got %s via unknown path %s(%s), confirming...",Packet::verbString(verb),_id.address().toString().c_str(),remoteAddr.toString().c_str());

				if ( (_vProto >= 5) && ( !((_vMajor == 1)&&(_vMinor == 1)&&(_vRevision == 0)) ) ) {
					Packet outp(_id.address(),RR->identity.address(),Packet::VERB_ECHO);
					outp.armor(_key,true);
					RR->node->putPacket(localAddr,remoteAddr,outp.data(),outp.size());
				} else {
					sendHELLO(localAddr,remoteAddr,now);
				}

			}
		}
	}

	if ((now - _lastAnnouncedTo) >= ((ZT_MULTICAST_LIKE_EXPIRE / 2) - 1000)) {
		_lastAnnouncedTo = now;
		const std::vector< SharedPtr<Network> > networks(RR->node->allNetworks());
		for(std::vector< SharedPtr<Network> >::const_iterator n(networks.begin());n!=networks.end();++n)
			(*n)->tryAnnounceMulticastGroupsTo(SharedPtr<Peer>(this));
	}
}

void Peer::sendHELLO(const InetAddress &localAddr,const InetAddress &atAddress,uint64_t now,unsigned int ttl)
{
	Packet outp(_id.address(),RR->identity.address(),Packet::VERB_HELLO);
	outp.append((unsigned char)ZT_PROTO_VERSION);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MAJOR);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MINOR);
	outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
	outp.append(now);
	RR->identity.serialize(outp,false);
	atAddress.serialize(outp);
	outp.append((uint64_t)RR->topology->worldId());
	outp.append((uint64_t)RR->topology->worldTimestamp());

	outp.armor(_key,false); // HELLO is sent in the clear
	RR->node->putPacket(localAddr,atAddress,outp.data(),outp.size(),ttl);
}

bool Peer::doPingAndKeepalive(uint64_t now,int inetAddressFamily)
{
	Path *p = (Path *)0;

	if (inetAddressFamily != 0) {
		p = _getBestPath(now,inetAddressFamily);
	} else {
		p = _getBestPath(now);
	}

	if (p) {
		if ((now - p->lastReceived()) >= ZT_PEER_DIRECT_PING_DELAY) {
			//TRACE("PING %s(%s) after %llums/%llums send/receive inactivity",_id.address().toString().c_str(),p->address().toString().c_str(),now - p->lastSend(),now - p->lastReceived());
			sendHELLO(p->localAddress(),p->address(),now);
			p->sent(now);
			p->pinged(now);
		} else if ( ((now - std::max(p->lastSend(),p->lastKeepalive())) >= ZT_NAT_KEEPALIVE_DELAY) && (!p->reliable()) ) {
			//TRACE("NAT keepalive %s(%s) after %llums/%llums send/receive inactivity",_id.address().toString().c_str(),p->address().toString().c_str(),now - p->lastSend(),now - p->lastReceived());
			_natKeepaliveBuf += (uint32_t)((now * 0x9e3779b1) >> 1); // tumble this around to send constantly varying (meaningless) payloads
			RR->node->putPacket(p->localAddress(),p->address(),&_natKeepaliveBuf,sizeof(_natKeepaliveBuf));
			p->sentKeepalive(now);
		} else {
			//TRACE("no PING or NAT keepalive: addr==%s reliable==%d %llums/%llums send/receive inactivity",p->address().toString().c_str(),(int)p->reliable(),now - p->lastSend(),now - p->lastReceived());
		}
		return true;
	}

	return false;
}

bool Peer::pushDirectPaths(const InetAddress &localAddr,const InetAddress &toAddress,uint64_t now,bool force,bool includePrivatePaths)
{
#ifdef ZT_ENABLE_CLUSTER
	// Cluster mode disables normal PUSH_DIRECT_PATHS in favor of cluster-based peer redirection
	if (RR->cluster)
		return false;
#endif

	if (!force) {
		if ((now - _lastDirectPathPushSent) < ZT_DIRECT_PATH_PUSH_INTERVAL)
			return false;
		else _lastDirectPathPushSent = now;
	}

	std::vector<InetAddress> pathsToPush;

	std::vector<InetAddress> dps(RR->node->directPaths());
	for(std::vector<InetAddress>::const_iterator i(dps.begin());i!=dps.end();++i) {
		if ((includePrivatePaths)||(i->ipScope() == InetAddress::IP_SCOPE_GLOBAL))
			pathsToPush.push_back(*i);
	}

	std::vector<InetAddress> sym(RR->sa->getSymmetricNatPredictions());
	for(unsigned long i=0,added=0;i<sym.size();++i) {
		InetAddress tmp(sym[(unsigned long)RR->node->prng() % sym.size()]);
		if (std::find(pathsToPush.begin(),pathsToPush.end(),tmp) == pathsToPush.end()) {
			pathsToPush.push_back(tmp);
			if (++added >= ZT_PUSH_DIRECT_PATHS_MAX_PER_SCOPE_AND_FAMILY)
				break;
		}
	}
	if (pathsToPush.empty())
		return false;

#ifdef ZT_TRACE
	{
		std::string ps;
		for(std::vector<InetAddress>::const_iterator p(pathsToPush.begin());p!=pathsToPush.end();++p) {
			if (ps.length() > 0)
				ps.push_back(',');
			ps.append(p->toString());
		}
		TRACE("pushing %u direct paths to %s: %s",(unsigned int)pathsToPush.size(),_id.address().toString().c_str(),ps.c_str());
	}
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
			outp.armor(_key,true);
			RR->node->putPacket(localAddr,toAddress,outp.data(),outp.size(),0);
		}
	}

	return true;
}

bool Peer::resetWithinScope(InetAddress::IpScope scope,uint64_t now)
{
	unsigned int np = _numPaths;
	unsigned int x = 0;
	unsigned int y = 0;
	while (x < np) {
		if (_paths[x].address().ipScope() == scope) {
			// Resetting a path means sending a HELLO and then forgetting it. If we
			// get OK(HELLO) then it will be re-learned.
			sendHELLO(_paths[x].localAddress(),_paths[x].address(),now);
		} else {
			_paths[y++] = _paths[x];
		}
		++x;
	}
	_numPaths = y;
	return (y < np);
}

void Peer::getBestActiveAddresses(uint64_t now,InetAddress &v4,InetAddress &v6) const
{
	uint64_t bestV4 = 0,bestV6 = 0;
	for(unsigned int p=0,np=_numPaths;p<np;++p) {
		if (_paths[p].active(now)) {
			uint64_t lr = _paths[p].lastReceived();
			if (lr) {
				if (_paths[p].address().isV4()) {
					if (lr >= bestV4) {
						bestV4 = lr;
						v4 = _paths[p].address();
					}
				} else if (_paths[p].address().isV6()) {
					if (lr >= bestV6) {
						bestV6 = lr;
						v6 = _paths[p].address();
					}
				}
			}
		}
	}
}

bool Peer::networkMembershipCertificatesAgree(uint64_t nwid,const CertificateOfMembership &com) const
{
	Mutex::Lock _l(_networkComs_m);
	const _NetworkCom *ourCom = _networkComs.get(nwid);
	if (ourCom)
		return ourCom->com.agreesWith(com);
	return false;
}

bool Peer::validateAndSetNetworkMembershipCertificate(uint64_t nwid,const CertificateOfMembership &com)
{
	// Sanity checks
	if ((!com)||(com.issuedTo() != _id.address()))
		return false;

	// Return true if we already have this *exact* COM
	{
		Mutex::Lock _l(_networkComs_m);
		_NetworkCom *ourCom = _networkComs.get(nwid);
		if ((ourCom)&&(ourCom->com == com))
			return true;
	}

	// Check signature, log and return if cert is invalid
	if (com.signedBy() != Network::controllerFor(nwid)) {
		TRACE("rejected network membership certificate for %.16llx signed by %s: signer not a controller of this network",(unsigned long long)nwid,com.signedBy().toString().c_str());
		return false; // invalid signer
	}

	if (com.signedBy() == RR->identity.address()) {

		// We are the controller: RR->identity.address() == controller() == cert.signedBy()
		// So, verify that we signed th cert ourself
		if (!com.verify(RR->identity)) {
			TRACE("rejected network membership certificate for %.16llx self signed by %s: signature check failed",(unsigned long long)nwid,com.signedBy().toString().c_str());
			return false; // invalid signature
		}

	} else {

		SharedPtr<Peer> signer(RR->topology->getPeer(com.signedBy()));

		if (!signer) {
			// This would be rather odd, since this is our controller... could happen
			// if we get packets before we've gotten config.
			RR->sw->requestWhois(com.signedBy());
			return false; // signer unknown
		}

		if (!com.verify(signer->identity())) {
			TRACE("rejected network membership certificate for %.16llx signed by %s: signature check failed",(unsigned long long)nwid,com.signedBy().toString().c_str());
			return false; // invalid signature
		}
	}

	// If we made it past all those checks, add or update cert in our cert info store
	{
		Mutex::Lock _l(_networkComs_m);
		_networkComs.set(nwid,_NetworkCom(RR->node->now(),com));
	}

	return true;
}

bool Peer::needsOurNetworkMembershipCertificate(uint64_t nwid,uint64_t now,bool updateLastPushedTime)
{
	Mutex::Lock _l(_networkComs_m);
	uint64_t &lastPushed = _lastPushedComs[nwid];
	const uint64_t tmp = lastPushed;
	if (updateLastPushedTime)
		lastPushed = now;
	return ((now - tmp) >= (ZT_NETWORK_AUTOCONF_DELAY / 3));
}

void Peer::clean(uint64_t now)
{
	{
		unsigned int np = _numPaths;
		unsigned int x = 0;
		unsigned int y = 0;
		while (x < np) {
			if (_paths[x].active(now))
				_paths[y++] = _paths[x];
			++x;
		}
		_numPaths = y;
	}

	{
		Mutex::Lock _l(_networkComs_m);
		{
			uint64_t *k = (uint64_t *)0;
			_NetworkCom *v = (_NetworkCom *)0;
			Hashtable< uint64_t,_NetworkCom >::Iterator i(_networkComs);
			while (i.next(k,v)) {
				if ( (!RR->node->belongsToNetwork(*k)) && ((now - v->ts) >= ZT_PEER_NETWORK_COM_EXPIRATION) )
					_networkComs.erase(*k);
			}
		}
		{
			uint64_t *k = (uint64_t *)0;
			uint64_t *v = (uint64_t *)0;
			Hashtable< uint64_t,uint64_t >::Iterator i(_lastPushedComs);
			while (i.next(k,v)) {
				if ((now - *v) > (ZT_NETWORK_AUTOCONF_DELAY * 2))
					_lastPushedComs.erase(*k);
			}
		}
	}
}

void Peer::_doDeadPathDetection(Path &p,const uint64_t now)
{
	/* Dead path detection: if we have sent something to this peer and have not
	 * yet received a reply, double check this path. The majority of outbound
	 * packets including Ethernet frames do generate some kind of reply either
	 * immediately or at some point in the near future. This will occasionally
	 * (every NO_ANSWER_TIMEOUT ms) check paths unnecessarily if traffic that
	 * does not generate a response is being sent such as multicast announcements
	 * or frames belonging to unidirectional UDP protocols, but the cost is very
	 * tiny and the benefit in reliability is very large. This takes care of many
	 * failure modes including crap NATs that forget links and spurious changes
	 * to physical network topology that cannot be otherwise detected.
	 *
	 * Each time we do this we increment a probation counter in the path. This
	 * counter is reset on any packet receive over this path. If it reaches the
	 * MAX_PROBATION threshold the path is considred dead. */

	if (
	     (p.lastSend() > p.lastReceived()) &&
			 ((p.lastSend() - p.lastReceived()) >= ZT_PEER_DEAD_PATH_DETECTION_NO_ANSWER_TIMEOUT) &&
			 ((now - p.lastPing()) >= ZT_PEER_DEAD_PATH_DETECTION_NO_ANSWER_TIMEOUT) &&
			 (!p.isClusterSuboptimal()) &&
			 (!RR->topology->amRoot())
		 ) {
		TRACE("%s(%s) does not seem to be answering in a timely manner, checking if dead (probation == %u)",_id.address().toString().c_str(),p.address().toString().c_str(),p.probation());

		if ( (_vProto >= 5) && ( !((_vMajor == 1)&&(_vMinor == 1)&&(_vRevision == 0)) ) ) {
			Packet outp(_id.address(),RR->identity.address(),Packet::VERB_ECHO);
			outp.armor(_key,true);
			p.send(RR,outp.data(),outp.size(),now);
			p.pinged(now);
		} else {
			sendHELLO(p.localAddress(),p.address(),now);
			p.sent(now);
			p.pinged(now);
		}

		p.increaseProbation();
	}
}

Path *Peer::_getBestPath(const uint64_t now)
{
	Path *bestPath = (Path *)0;
	uint64_t bestPathScore = 0;
	for(unsigned int i=0;i<_numPaths;++i) {
		const uint64_t score = _paths[i].score();
		if ((score >= bestPathScore)&&(_paths[i].active(now))) {
			bestPathScore = score;
			bestPath = &(_paths[i]);
		}
	}
	if (bestPath)
		_doDeadPathDetection(*bestPath,now);
	return bestPath;
}

Path *Peer::_getBestPath(const uint64_t now,int inetAddressFamily)
{
	Path *bestPath = (Path *)0;
	uint64_t bestPathScore = 0;
	for(unsigned int i=0;i<_numPaths;++i) {
		const uint64_t score = _paths[i].score();
		if (((int)_paths[i].address().ss_family == inetAddressFamily)&&(score >= bestPathScore)&&(_paths[i].active(now))) {
			bestPathScore = score;
			bestPath = &(_paths[i]);
		}
	}
	if (bestPath)
		_doDeadPathDetection(*bestPath,now);
	return bestPath;
}

} // namespace ZeroTier
