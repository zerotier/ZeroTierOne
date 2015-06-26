/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#include "../version.h"

#include "Constants.hpp"
#include "Peer.hpp"
#include "Node.hpp"
#include "Switch.hpp"
#include "Network.hpp"
#include "AntiRecursion.hpp"

#include <algorithm>

namespace ZeroTier {

Peer::Peer(const Identity &myIdentity,const Identity &peerIdentity)
	throw(std::runtime_error) :
	_lastUsed(0),
	_lastReceive(0),
	_lastUnicastFrame(0),
	_lastMulticastFrame(0),
	_lastAnnouncedTo(0),
	_lastPathConfirmationSent(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0),
	_id(peerIdentity),
	_numPaths(0),
	_latency(0)
{
	if (!myIdentity.agree(peerIdentity,_key,ZT_PEER_SECRET_KEY_LENGTH))
		throw std::runtime_error("new peer identity key agreement failed");
}

void Peer::received(
	const RuntimeEnvironment *RR,
	const InetAddress &remoteAddr,
	unsigned int hops,
	uint64_t packetId,
	Packet::Verb verb,
	uint64_t inRePacketId,
	Packet::Verb inReVerb)
{
	const uint64_t now = RR->node->now();
	_lastReceive = now;

	if (!hops) {
		bool pathIsConfirmed = false;

		/* Learn new paths from direct (hops == 0) packets */
		{
			unsigned int np = _numPaths;
			for(unsigned int p=0;p<np;++p) {
				if (_paths[p].address() == remoteAddr) {
					_paths[p].received(now);
					pathIsConfirmed = true;
					break;
				}
			}

			if (!pathIsConfirmed) {
				if ((verb == Packet::VERB_OK)&&(inReVerb == Packet::VERB_HELLO)) {
					// Learn paths if they've been confirmed via a HELLO
					Path *slot = (Path *)0;
					if (np < ZT1_MAX_PEER_NETWORK_PATHS) {
						// Add new path
						slot = &(_paths[np++]);
					} else {
						// Replace oldest non-fixed path
						uint64_t slotLRmin = 0xffffffffffffffffULL;
						for(unsigned int p=0;p<ZT1_MAX_PEER_NETWORK_PATHS;++p) {
							if ((!_paths[p].fixed())&&(_paths[p].lastReceived() <= slotLRmin)) {
								slotLRmin = _paths[p].lastReceived();
								slot = &(_paths[p]);
							}
						}
					}
					if (slot) {
						slot->init(remoteAddr,false);
						slot->received(now);
						_numPaths = np;
						pathIsConfirmed = true;
					}
				} else {
					/* If this path is not known, send a HELLO. We don't learn
					 * paths without confirming that a bidirectional link is in
					 * fact present, but any packet that decodes and authenticates
					 * correctly is considered valid. */
					if ((now - _lastPathConfirmationSent) >= ZT_MIN_PATH_CONFIRMATION_INTERVAL) {
						_lastPathConfirmationSent = now;
						TRACE("got %s via unknown path %s(%s), confirming...",Packet::verbString(verb),_id.address().toString().c_str(),remoteAddr.toString().c_str());
						attemptToContactAt(RR,remoteAddr,now);
					}
				}
			}
		}

		/* Announce multicast groups of interest to direct peers if they are
		 * considered authorized members of a given network. Also announce to
		 * root servers and network controllers. */
		if ((pathIsConfirmed)&&((now - _lastAnnouncedTo) >= ((ZT_MULTICAST_LIKE_EXPIRE / 2) - 1000))) {
			_lastAnnouncedTo = now;

			const bool isRoot = RR->topology->isRoot(_id);

			Packet outp(_id.address(),RR->identity.address(),Packet::VERB_MULTICAST_LIKE);
			const std::vector< SharedPtr<Network> > networks(RR->node->allNetworks());
			for(std::vector< SharedPtr<Network> >::const_iterator n(networks.begin());n!=networks.end();++n) {
				if ( (isRoot) || ((*n)->isAllowed(_id.address())) ) {
					const std::vector<MulticastGroup> mgs((*n)->allMulticastGroups());
					for(std::vector<MulticastGroup>::const_iterator mg(mgs.begin());mg!=mgs.end();++mg) {
						if ((outp.size() + 18) > ZT_UDP_DEFAULT_PAYLOAD_MTU) {
							outp.armor(_key,true);
							RR->node->putPacket(remoteAddr,outp.data(),outp.size());
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
				RR->node->putPacket(remoteAddr,outp.data(),outp.size());
			}
		}
	}

	if ((verb == Packet::VERB_FRAME)||(verb == Packet::VERB_EXT_FRAME))
		_lastUnicastFrame = now;
	else if (verb == Packet::VERB_MULTICAST_FRAME)
		_lastMulticastFrame = now;
}

void Peer::attemptToContactAt(const RuntimeEnvironment *RR,const InetAddress &atAddress,uint64_t now)
{
	Packet outp(_id.address(),RR->identity.address(),Packet::VERB_HELLO);
	outp.append((unsigned char)ZT_PROTO_VERSION);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MAJOR);
	outp.append((unsigned char)ZEROTIER_ONE_VERSION_MINOR);
	outp.append((uint16_t)ZEROTIER_ONE_VERSION_REVISION);
	outp.append(now);

	RR->identity.serialize(outp,false);

	switch(atAddress.ss_family) {
		case AF_INET:
			outp.append((unsigned char)ZT_PROTO_DEST_ADDRESS_TYPE_IPV4);
			outp.append(atAddress.rawIpData(),4);
			outp.append((uint16_t)atAddress.port());
			break;
		case AF_INET6:
			outp.append((unsigned char)ZT_PROTO_DEST_ADDRESS_TYPE_IPV6);
			outp.append(atAddress.rawIpData(),16);
			outp.append((uint16_t)atAddress.port());
			break;
		default:
			outp.append((unsigned char)ZT_PROTO_DEST_ADDRESS_TYPE_NONE);
			break;
	}

	outp.armor(_key,false); // HELLO is sent in the clear
	RR->node->putPacket(atAddress,outp.data(),outp.size());
}

void Peer::doPingAndKeepalive(const RuntimeEnvironment *RR,uint64_t now)
{
	Path *const bestPath = getBestPath(now);
	if ((bestPath)&&(bestPath->active(now))) {
		if ((now - bestPath->lastReceived()) >= ZT_PEER_DIRECT_PING_DELAY) {
			TRACE("PING %s(%s)",_id.address().toString().c_str(),bestPath->address().toString().c_str());
			attemptToContactAt(RR,bestPath->address(),now);
			bestPath->sent(now);
		} else if ((now - bestPath->lastSend()) >= ZT_NAT_KEEPALIVE_DELAY) {
			TRACE("NAT keepalive %s(%s)",_id.address().toString().c_str(),bestPath->address().toString().c_str());
			RR->node->putPacket(bestPath->address(),"",0);
			bestPath->sent(now);
		}
	}
}

void Peer::addPath(const Path &newp)
{
	unsigned int np = _numPaths;

	for(unsigned int p=0;p<np;++p) {
		if (_paths[p].address() == newp.address()) {
			_paths[p].setFixed(newp.fixed());
			return;
		}
	}

	Path *slot = (Path *)0;
	if (np < ZT1_MAX_PEER_NETWORK_PATHS) {
		// Add new path
		slot = &(_paths[np++]);
	} else {
		// Replace oldest non-fixed path
		uint64_t slotLRmin = 0xffffffffffffffffULL;
		for(unsigned int p=0;p<ZT1_MAX_PEER_NETWORK_PATHS;++p) {
			if ((!_paths[p].fixed())&&(_paths[p].lastReceived() <= slotLRmin)) {
				slotLRmin = _paths[p].lastReceived();
				slot = &(_paths[p]);
			}
		}
	}
	if (slot) {
		*slot = newp;
		_numPaths = np;
	}
}

void Peer::clearPaths(bool fixedToo)
{
	if (fixedToo) {
		_numPaths = 0;
	} else {
		unsigned int np = _numPaths;
		unsigned int x = 0;
		unsigned int y = 0;
		while (x < np) {
			if (_paths[x].fixed())
				_paths[y++] = _paths[x];
			++x;
		}
		_numPaths = y;
	}
}

bool Peer::resetWithinScope(const RuntimeEnvironment *RR,InetAddress::IpScope scope,uint64_t now)
{
	unsigned int np = _numPaths;
	unsigned int x = 0;
	unsigned int y = 0;
	while (x < np) {
		if (_paths[x].address().ipScope() == scope) {
			if (_paths[x].fixed()) {
				attemptToContactAt(RR,_paths[x].address(),now);
				_paths[y++] = _paths[x]; // keep fixed paths
			}
		} else {
			_paths[y++] = _paths[x]; // keep paths not in this scope
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

} // namespace ZeroTier
