/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

namespace ZeroTier {

Peer::Peer() :
	_dirty(false)
{
}

Peer::Peer(const Identity &myIdentity,const Identity &peerIdentity)
	throw(std::runtime_error) :
	_id(peerIdentity),
	_dirty(true)
{
	if (!myIdentity.agree(peerIdentity,_keys,sizeof(_keys)))
		throw std::runtime_error("new peer identity key agreement failed");
}

void Peer::onReceive(const RuntimeEnvironment *_r,Demarc::Port localPort,const InetAddress &fromAddr,unsigned int latency,unsigned int hops,Packet::Verb verb,uint64_t now)
{
	if (!hops) { // direct packet
		WanPath *wp = (fromAddr.isV4() ? &_ipv4p : &_ipv6p);

		wp->lastReceive = now;
		if (verb == Packet::VERB_FRAME)
			wp->lastUnicastFrame = now;
		if (latency)
			wp->latency = latency;
		wp->localPort = localPort;
		if (!wp->fixed)
			wp->addr = fromAddr;

		_dirty = true;
	}
}

bool Peer::send(const RuntimeEnvironment *_r,const void *data,unsigned int len,bool relay,Packet::Verb verb,uint64_t now)
{
	if ((_ipv6p.isActive(now))||((!(_ipv4p.addr))&&(_ipv6p.addr))) {
		if (_r->demarc->send(_ipv6p.localPort,_ipv6p.addr,data,len,-1)) {
			_ipv6p.lastSend = now;
			if (verb == Packet::VERB_FRAME)
				_ipv6p.lastUnicastFrame = now;
			_dirty = true;
			return true;
		}
	}

	if (_ipv4p.addr) {
		if (_r->demarc->send(_ipv4p.localPort,_ipv4p.addr,data,len,-1)) {
			_ipv4p.lastSend = now;
			if (verb == Packet::VERB_FRAME)
				_ipv4p.lastUnicastFrame = now;
			_dirty = true;
			return true;
		}
	}

	return false;
}

bool Peer::sendFirewallOpener(const RuntimeEnvironment *_r,uint64_t now)
{
	bool sent = false;
	if (_ipv4p.addr) {
		if (_r->demarc->send(_ipv4p.localPort,_ipv4p.addr,"\0",1,ZT_FIREWALL_OPENER_HOPS)) {
			_ipv4p.lastFirewallOpener = now;
			_dirty = true;
			sent = true;
		}
	}
	if (_ipv6p.addr) {
		if (_r->demarc->send(_ipv6p.localPort,_ipv6p.addr,"\0",1,ZT_FIREWALL_OPENER_HOPS)) {
			_ipv6p.lastFirewallOpener = now;
			_dirty = true;
			sent = true;
		}
	}
	return sent;
}

void Peer::setPathAddress(const InetAddress &addr,bool fixed)
{
	if (addr.isV4()) {
		_ipv4p.addr = addr;
		_ipv4p.fixed = fixed;
		_dirty = true;
	} else if (addr.isV6()) {
		_ipv6p.addr = addr;
		_ipv6p.fixed = fixed;
		_dirty = true;
	}
}

void Peer::clearFixedFlag(InetAddress::AddressType t)
{
	switch(t) {
		case InetAddress::TYPE_NULL:
			_ipv4p.fixed = false;
			_ipv6p.fixed = false;
			_dirty = true;
			break;
		case InetAddress::TYPE_IPV4:
			_ipv4p.fixed = false;
			_dirty = true;
			break;
		case InetAddress::TYPE_IPV6:
			_ipv6p.fixed = false;
			_dirty = true;
			break;
	}
}

} // namespace ZeroTier
