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

namespace ZeroTier {

Peer::Peer(const RuntimeEnvironment *renv,const Identity &myIdentity,const Identity &peerIdentity) :
	RR(renv),
	_lastReceive(0),
	_lastWhoisRequestReceived(0),
	_lastEchoRequestReceived(0),
	_lastPushDirectPathsReceived(0),
	_lastTriedStaticPath(0),
	_latency(0xffff),
	_pathCount(0),
	_id(peerIdentity),
	_vProto(0),
	_vMajor(0),
	_vMinor(0),
	_vRevision(0)
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

	/*
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
	*/

	// Periodically push direct paths to the peer, doing so more often if we do not
	// currently have a direct path.
	/*
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
	*/
}

bool Peer::hasActivePathTo(int64_t now,const InetAddress &addr) const
{
	// TODO
	return false;
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
	/*
	Mutex::Lock _l(_paths_m);

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
	*/
}

void Peer::resetWithinScope(void *tPtr,InetAddress::IpScope scope,int inetAddressFamily,int64_t now)
{
	/*
	Mutex::Lock _l(_paths_m);
	for(unsigned int i=0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
		if (_paths[i]) {
			if ((_paths[i]->address().ss_family == inetAddressFamily)&&(_paths[i]->ipScope() == scope)) {
				sendHELLO(tPtr,_paths[i]->localSocket(),_paths[i]->address(),now);
				_paths[i]->sent(now);
			}
		} else break;
	}
	*/
}

void Peer::getAllPaths(std::vector< SharedPtr<Path> > &paths)
{
	RWMutex::RLock l(_paths_l);
	paths.clear();
	paths.assign(_paths,_paths + _pathCount);
}

} // namespace ZeroTier
