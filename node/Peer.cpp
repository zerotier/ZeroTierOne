/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include "Constants.hpp"
#include "Peer.hpp"
#include "Switch.hpp"
#include "Packet.hpp"
#include "Network.hpp"
#include "NodeConfig.hpp"
#include "AntiRecursion.hpp"

#include <algorithm>

namespace ZeroTier {

Peer::Peer() :
	_lastUsed(0),
	_lastReceive(0),
	_lastUnicastFrame(0),
	_lastMulticastFrame(0),
	_lastAnnouncedTo(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0),
	_latency(0) {}

Peer::Peer(const Identity &myIdentity,const Identity &peerIdentity)
	throw(std::runtime_error) :
	_id(peerIdentity),
	_lastUsed(0),
	_lastReceive(0),
	_lastUnicastFrame(0),
	_lastMulticastFrame(0),
	_lastAnnouncedTo(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0),
	_latency(0)
{
	if (!myIdentity.agree(peerIdentity,_key,ZT_PEER_SECRET_KEY_LENGTH))
		throw std::runtime_error("new peer identity key agreement failed");
}

void Peer::receive(
	const RuntimeEnvironment *RR,
	const SharedPtr<Socket> &fromSock,
	const InetAddress &remoteAddr,
	unsigned int hops,
	uint64_t packetId,
	Packet::Verb verb,
	uint64_t inRePacketId,
	Packet::Verb inReVerb,
	uint64_t now)
{
	// Update system-wide last packet receive time
	*((const_cast<uint64_t *>(&(RR->timeOfLastPacketReceived)))) = now;

	Mutex::Lock _l(_lock);

	// Global last receive time regardless of path
	_lastReceive = now;

	if (!hops) {
		// Learn paths from direct packets (hops == 0)
		{
			bool havePath = false;
			for(std::vector<Path>::iterator p(_paths.begin());p!=_paths.end();++p) {
				if ((p->address() == remoteAddr)&&(p->tcp() == fromSock->tcp())) {
					p->received(now);
					havePath = true;
					break;
				}
			}

			if (!havePath) {
				Path::Type pt = Path::PATH_TYPE_UDP;
				switch(fromSock->type()) {
					case Socket::ZT_SOCKET_TYPE_TCP_IN:
						pt = Path::PATH_TYPE_TCP_IN;
						break;
					case Socket::ZT_SOCKET_TYPE_TCP_OUT:
						pt = Path::PATH_TYPE_TCP_OUT;
						break;
					default:
						break;
				}
				_paths.push_back(Path(remoteAddr,pt,false));
				_paths.back().received(now);
			}
		}

		/* Announce multicast groups of interest to direct peers if they are
		 * considered authorized members of a given network. Also announce to
		 * supernodes and network controllers. The other place this is done
		 * is in rescanMulticastGroups() in Network, but that only sends something
		 * if a network's multicast groups change. */
		if ((now - _lastAnnouncedTo) >= ((ZT_MULTICAST_LIKE_EXPIRE / 2) - 1000)) {
			_lastAnnouncedTo = now;

			Packet outp(_id.address(),RR->identity.address(),Packet::VERB_MULTICAST_LIKE);
			std::vector< SharedPtr<Network> > networks(RR->nc->networks());
			for(std::vector< SharedPtr<Network> >::iterator n(networks.begin());n!=networks.end();++n) {
				if ( ((*n)->isAllowed(_id.address())) || ((*n)->controller() == _id.address()) || (RR->topology->isSupernode(_id.address())) ) {
					std::set<MulticastGroup> mgs((*n)->multicastGroups());
					for(std::set<MulticastGroup>::iterator mg(mgs.begin());mg!=mgs.end();++mg) {
						if ((outp.size() + 18) > ZT_UDP_DEFAULT_PAYLOAD_MTU) {
							outp.armor(_key,true);
							fromSock->send(remoteAddr,outp.data(),outp.size());
							outp.reset(_id.address(),RR->identity.address(),Packet::VERB_MULTICAST_LIKE);
						}

						// network ID, MAC, ADI
						outp.append((uint64_t)(*n)->id());
						mg->mac().appendTo(outp);
						outp.append((uint32_t)mg->adi());
					}
				}
			}
			if (outp.size() > ZT_PROTO_MIN_PACKET_LENGTH) {
				outp.armor(_key,true);
				fromSock->send(remoteAddr,outp.data(),outp.size());
			}
		}
	}

	if ((verb == Packet::VERB_FRAME)||(verb == Packet::VERB_EXT_FRAME))
		_lastUnicastFrame = now;
	else if ((verb == Packet::VERB_P5_MULTICAST_FRAME)||(verb == Packet::VERB_MULTICAST_FRAME))
		_lastMulticastFrame = now;
}

Path::Type Peer::send(const RuntimeEnvironment *RR,const void *data,unsigned int len,uint64_t now)
{
	Mutex::Lock _l(_lock);

	/* For sending ordinary packets, paths are divided into two categories:
	 * "normal" and "TCP out." Normal includes UDP and incoming TCP. We want
	 * to treat outbound TCP differently since if we use it it may end up
	 * overriding UDP and UDP performs much better. We only want to initiate
	 * TCP if it looks like UDP isn't available. */
	Path *bestNormalPath = (Path *)0;
	Path *bestTcpOutPath = (Path *)0;
	uint64_t bestNormalPathLastReceived = 0;
	uint64_t bestTcpOutPathLastReceived = 0;
	for(std::vector<Path>::iterator p(_paths.begin());p!=_paths.end();++p) {
		uint64_t lr = p->lastReceived();
		if (p->type() == Path::PATH_TYPE_TCP_OUT) {
			if (lr >= bestTcpOutPathLastReceived) {
				bestTcpOutPathLastReceived = lr;
				bestTcpOutPath = &(*p);
			}
		} else {
			if (lr >= bestNormalPathLastReceived) {
				bestNormalPathLastReceived = lr;
				bestNormalPath = &(*p);
			}
		}
	}

	Path *bestPath = (Path *)0;
	if (bestTcpOutPath) { // we have a TCP out path
		if (bestNormalPath) { // we have both paths, decide which to use
			if (RR->tcpTunnelingEnabled) { // TCP tunneling is enabled, so use normal path only if it looks alive
				if ((bestNormalPathLastReceived > RR->timeOfLastResynchronize)&&((now - bestNormalPathLastReceived) < ZT_PEER_PATH_ACTIVITY_TIMEOUT))
					bestPath = bestNormalPath;
				else bestPath = bestTcpOutPath;
			} else { // TCP tunneling is disabled, use normal path
				bestPath = bestNormalPath;
			}
		} else { // we only have a TCP_OUT path, so use it regardless
			bestPath = bestTcpOutPath;
		}
	} else { // we only have a normal path (or none at all, that case is caught below)
		bestPath = bestNormalPath;
	}
	if (!bestPath)
		return Path::PATH_TYPE_NULL;

	RR->antiRec->logOutgoingZT(data,len);

	if (RR->sm->send(bestPath->address(),bestPath->tcp(),bestPath->type() == Path::PATH_TYPE_TCP_OUT,data,len)) {
		bestPath->sent(now);
		return bestPath->type();
	}

	return Path::PATH_TYPE_NULL;
}

bool Peer::sendPing(const RuntimeEnvironment *RR,uint64_t now)
{
	bool sent = false;
	SharedPtr<Peer> self(this);
	Mutex::Lock _l(_lock);

	/* Ping (and thus open) outbound TCP connections if we have no other options
	 * or if the TCP tunneling master switch is enabled and pings have been
	 * unanswered for ZT_TCP_TUNNEL_FAILOVER_TIMEOUT ms over normal channels. */
	uint64_t lastNormalPingSent = 0;
	uint64_t lastNormalReceive = 0;
	bool haveNormal = false;
	for(std::vector<Path>::const_iterator p(_paths.begin());p!=_paths.end();++p) {
		if (p->type() != Path::PATH_TYPE_TCP_OUT) {
			lastNormalPingSent = std::max(lastNormalPingSent,p->lastPing());
			lastNormalReceive = std::max(lastNormalReceive,p->lastReceived());
			haveNormal = true;
		}
	}
	const bool useTcpOut = ( (!haveNormal) || ( (RR->tcpTunnelingEnabled) && (lastNormalPingSent > RR->timeOfLastResynchronize) && (lastNormalPingSent > lastNormalReceive) && ((lastNormalPingSent - lastNormalReceive) >= ZT_TCP_TUNNEL_FAILOVER_TIMEOUT) ) );

	TRACE("PING %s (useTcpOut==%d)",_id.address().toString().c_str(),(int)useTcpOut);

	for(std::vector<Path>::iterator p(_paths.begin());p!=_paths.end();++p) {
		if ((useTcpOut)||(p->type() != Path::PATH_TYPE_TCP_OUT)) {
			p->pinged(now); // attempts to ping are logged whether they look successful or not
			if (RR->sw->sendHELLO(self,*p)) {
				p->sent(now);
				sent = true;
			}
		}
	}

	return sent;
}

void Peer::clean(uint64_t now)
{
	Mutex::Lock _l(_lock);
	unsigned long i = 0,o = 0,l = (unsigned long)_paths.size();
	while (i != l) {
		if (_paths[i].active(now)) // active includes fixed
			_paths[o++] = _paths[i];
		++i;
	}
	_paths.resize(o);
}

void Peer::getBestActiveUdpPathAddresses(uint64_t now,InetAddress &v4,InetAddress &v6) const
{
	uint64_t bestV4 = 0,bestV6 = 0;
	Mutex::Lock _l(_lock);
	for(std::vector<Path>::const_iterator p(_paths.begin());p!=_paths.end();++p) {
		if ((p->type() == Path::PATH_TYPE_UDP)&&(p->active(now))) {
			uint64_t lr = p->lastReceived();
			if (lr) {
				if (p->address().isV4()) {
					if (lr >= bestV4) {
						bestV4 = lr;
						v4 = p->address();
					}
				} else if (p->address().isV6()) {
					if (lr >= bestV6) {
						bestV6 = lr;
						v6 = p->address();
					}
				}
			}
		}
	}
}

} // namespace ZeroTier
