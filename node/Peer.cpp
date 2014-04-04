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

#include "Peer.hpp"
#include "Switch.hpp"

#include <algorithm>

namespace ZeroTier {

Peer::Peer() :
	_lastUsed(0),
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
	const RuntimeEnvironment *_r,
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
	*((const_cast<uint64_t *>(&(_r->timeOfLastPacketReceived)))) = now;

	// Learn paths from direct packets (hops == 0)
	if (!hops) {
		{
			Mutex::Lock _l(_lock);

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

		// Announce multicast LIKEs to peers to whom we have a direct link
		// Lock can't be locked here or it'll recurse and deadlock.
		if ((now - _lastAnnouncedTo) >= ((ZT_MULTICAST_LIKE_EXPIRE / 2) - 1000)) {
			_lastAnnouncedTo = now;
			_r->sw->announceMulticastGroups(SharedPtr<Peer>(this));
		}
	}

	if (verb == Packet::VERB_FRAME)
		_lastUnicastFrame = now;
	else if (verb == Packet::VERB_MULTICAST_FRAME)
		_lastMulticastFrame = now;
}


bool Peer::send(const RuntimeEnvironment *_r,const void *data,unsigned int len,uint64_t now)
{
	Mutex::Lock _l(_lock);
	bool useTcp = _isTcpFailoverTime(_r,now);

	std::vector<Path>::iterator p(_paths.begin());
	if (useTcp) {
		while ((p->tcp())&&(p != _paths.end()))
			++p;
	}
	if (p == _paths.end())
		return false;

	uint64_t bestPathLastReceived = p->lastReceived();
	std::vector<Path>::iterator bestPath = p;
	while (++p != _paths.end()) {
		uint64_t lr = p->lastReceived();
		if ( (lr > bestPathLastReceived) && ((useTcp)||(!p->tcp())) ) {
			bestPathLastReceived = lr;
			bestPath = p;
		}
	}

	if (_r->sm->send(bestPath->address(),bestPath->tcp(),bestPath->type() == Path::PATH_TYPE_TCP_OUT,data,len)) {
		bestPath->sent(now);
		return true;
	}

	return false;
}

bool Peer::sendFirewallOpener(const RuntimeEnvironment *_r,uint64_t now)
{
	bool sent = false;
	Mutex::Lock _l(_lock);

	for(std::vector<Path>::iterator p(_paths.begin());p!=_paths.end();++p) {
		if (!p->tcp())
			sent |= _r->sm->sendFirewallOpener(p->address(),ZT_FIREWALL_OPENER_HOPS);
	}

	return sent;
}

bool Peer::sendPing(const RuntimeEnvironment *_r,uint64_t now)
{
	bool sent = false;
	SharedPtr<Peer> self(this);
	Mutex::Lock _l(_lock);
	bool useTcp = _isTcpFailoverTime(_r,now);

	TRACE("PING %s (useTcp==%d)",_id.address().toString().c_str(),(int)useTcp);

	for(std::vector<Path>::iterator p(_paths.begin());p!=_paths.end();++p) {
		if ((useTcp)||(!p->tcp())) {
			p->pinged(now); // we log pings sent even if the send "fails", since what we want to track is when we last tried to ping
			if (_r->sw->sendHELLO(self,*p)) {
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
		if (_paths[i].active(now))
			_paths[o++] = _paths[i];
		++i;
	}
	_paths.resize(o);
}

bool Peer::_isTcpFailoverTime(const RuntimeEnvironment *_r,uint64_t now) const
	throw()
{
	// assumes _lock is locked
	uint64_t lastResync = _r->timeOfLastResynchronize;
	if ((now - lastResync) >= ZT_TCP_TUNNEL_FAILOVER_TIMEOUT) {
		if ((now - _r->timeOfLastPacketReceived) >= ZT_TCP_TUNNEL_FAILOVER_TIMEOUT)
			return true;

		uint64_t lastUdpPingSent = 0;
		uint64_t lastUdpReceive = 0;

		for(std::vector<Path>::const_iterator p(_paths.begin());p!=_paths.end();++p) {
			if (p->type() == Path::PATH_TYPE_UDP) {
				lastUdpPingSent = std::max(lastUdpPingSent,p->lastPing());
				lastUdpReceive = std::max(lastUdpReceive,p->lastReceived());
			}
		}

		return ( (lastUdpPingSent > lastResync) && (lastUdpPingSent > lastUdpReceive) && ((now - lastUdpPingSent) >= ZT_TCP_TUNNEL_FAILOVER_TIMEOUT) );
	}
	return false;
}

} // namespace ZeroTier
