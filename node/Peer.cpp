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
	if (!hops) { // direct packet
		{
			Mutex::Lock _l(_lock);

			// Update receive time on known paths
			bool havePath = false;
			for(std::vector<Path>::iterator p(_paths.begin());p!=_paths.end();++p) {
				if ((p->address() == remoteAddr)&&(p->tcp() == (fromSock->type() == Socket::ZT_SOCKET_TYPE_TCP))) {
					p->received(now);
					havePath = true;
					break;
				}
			}

			// Learn new UDP paths (learning TCP would require an explicit mechanism)
			if ((!havePath)&&(fromSock->type() != Socket::ZT_SOCKET_TYPE_TCP)) {
				_paths.push_back(Path(remoteAddr,false,false));
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

	std::vector<Path>::iterator p(_paths.begin());
	if (p == _paths.end()) {
		TRACE("send to %s failed: no paths available",_id.address().toString().c_str());
		return false;
	}
	uint64_t bestPathLastReceived = p->lastReceived();
	std::vector<Path>::iterator bestPath = p;
	while (++p != _paths.end()) {
		uint64_t lr = p->lastReceived();
		if (lr > bestPathLastReceived) {
			bestPathLastReceived = lr;
			bestPath = p;
		}
	}

	TRACE("send to %s: using path: %s",_id.address().toString().c_str(),bestPath->toString().c_str());

	if (_r->sm->send(bestPath->address(),bestPath->tcp(),data,len)) {
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

bool Peer::sendPing(const RuntimeEnvironment *_r,uint64_t now,bool firstSinceReset)
{
	bool sent = false;
	SharedPtr<Peer> self(this);

	Mutex::Lock _l(_lock);

	// NOTE: this will never ping a peer that has *only* TCP paths. Right
	// now there's never such a thing as TCP is only for failover.

	bool pingTcp;
	if (!firstSinceReset) {
		// Do not use TCP if one of our UDP endpoints has answered recently.
		uint64_t lastPing = 0;
		uint64_t lastDirectReceive = 0;

		for(std::vector<Path>::iterator p(_paths.begin());p!=_paths.end();++p) {
			lastPing = std::max(lastPing,p->lastPing());
			lastDirectReceive = std::max(lastDirectReceive,p->lastReceived());
		}

		pingTcp = ( (lastDirectReceive < lastPing) && ((lastPing - lastDirectReceive) >= ZT_PING_UNANSWERED_AFTER) );
	} else pingTcp = false;

	TRACE("PING %s (pingTcp==%d)",_id.address().toString().c_str(),(int)pingTcp);

	for(std::vector<Path>::iterator p(_paths.begin());p!=_paths.end();++p) {
		if ((pingTcp)||(!p->tcp())) {
			if (_r->sw->sendHELLO(self,p->address(),p->tcp())) {
				p->sent(now);
				p->pinged(now);
				sent = true;
			}
		}
	}

	return sent;
}

} // namespace ZeroTier
