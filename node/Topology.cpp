/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#include "Constants.hpp"
#include "Topology.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"
#include "Network.hpp"
#include "NetworkConfig.hpp"
#include "Buffer.hpp"
#include "Switch.hpp"

namespace ZeroTier {

Topology::Topology(const RuntimeEnvironment *renv,void *tPtr) :
	RR(renv),
	_numConfiguredPhysicalPaths(0)
{
}

Topology::~Topology()
{
	Hashtable< Address,SharedPtr<Peer> >::Iterator i(_peers);
	Address *a = (Address *)0;
	SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
	while (i.next(a,p))
		_savePeer((void *)0,*p);
}

SharedPtr<Peer> Topology::addPeer(void *tPtr,const SharedPtr<Peer> &peer)
{
	SharedPtr<Peer> np;
	{
		Mutex::Lock _l(_peers_m);
		SharedPtr<Peer> &hp = _peers[peer->address()];
		if (!hp)
			hp = peer;
		np = hp;
	}
	return np;
}

SharedPtr<Peer> Topology::getPeer(void *tPtr,const Address &zta)
{
	if (zta == RR->identity.address())
		return SharedPtr<Peer>();

	{
		Mutex::Lock _l(_peers_m);
		const SharedPtr<Peer> *const ap = _peers.get(zta);
		if (ap)
			return *ap;
	}

	try {
		Buffer<ZT_PEER_MAX_SERIALIZED_STATE_SIZE> buf;
		uint64_t idbuf[2]; idbuf[0] = zta.toInt(); idbuf[1] = 0;
		int len = RR->node->stateObjectGet(tPtr,ZT_STATE_OBJECT_PEER,idbuf,buf.unsafeData(),ZT_PEER_MAX_SERIALIZED_STATE_SIZE);
		if (len > 0) {
			buf.setSize(len);
			Mutex::Lock _l(_peers_m);
			SharedPtr<Peer> &ap = _peers[zta];
			if (ap)
				return ap;
			ap = Peer::deserializeFromCache(RR->node->now(),tPtr,buf,RR);
			if (!ap) {
				_peers.erase(zta);
			}
			return SharedPtr<Peer>();
		}
	} catch ( ... ) {} // ignore invalid identities or other strange failures

	return SharedPtr<Peer>();
}

Identity Topology::getIdentity(void *tPtr,const Address &zta)
{
	if (zta == RR->identity.address()) {
		return RR->identity;
	} else {
		Mutex::Lock _l(_peers_m);
		const SharedPtr<Peer> *const ap = _peers.get(zta);
		if (ap)
			return (*ap)->identity();
	}
	return Identity();
}

SharedPtr<Peer> Topology::getUpstreamPeer()
{
	return SharedPtr<Peer>();
}

bool Topology::isUpstream(const Identity &id) const
{
	return false;
}

ZT_PeerRole Topology::role(const Address &ztaddr) const
{
	return ZT_PEER_ROLE_LEAF;
}

bool Topology::isProhibitedEndpoint(const Address &ztaddr,const InetAddress &ipaddr) const
{
	return false;
}

void Topology::doPeriodicTasks(void *tPtr,int64_t now)
{
	{
		Mutex::Lock _l1(_peers_m);
		Hashtable< Address,SharedPtr<Peer> >::Iterator i(_peers);
		Address *a = (Address *)0;
		SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
		while (i.next(a,p)) {
			if (!(*p)->isAlive(now)) {
				_savePeer(tPtr,*p);
				_peers.erase(*a);
			}
		}
	}

	{
		Mutex::Lock _l(_paths_m);
		Hashtable< Path::HashKey,SharedPtr<Path> >::Iterator i(_paths);
		Path::HashKey *k = (Path::HashKey *)0;
		SharedPtr<Path> *p = (SharedPtr<Path> *)0;
		while (i.next(k,p)) {
			if (p->references() <= 1)
				_paths.erase(*k);
		}
	}
}

void Topology::setPhysicalPathConfiguration(const struct sockaddr_storage *pathNetwork,const ZT_PhysicalPathConfiguration *pathConfig)
{
	if (!pathNetwork) {
		_numConfiguredPhysicalPaths = 0;
	} else {
		std::map<InetAddress,ZT_PhysicalPathConfiguration> cpaths;
		for(unsigned int i=0,j=_numConfiguredPhysicalPaths;i<j;++i)
			cpaths[_physicalPathConfig[i].first] = _physicalPathConfig[i].second;

		if (pathConfig) {
			ZT_PhysicalPathConfiguration pc(*pathConfig);

			if (pc.mtu <= 0)
				pc.mtu = ZT_DEFAULT_PHYSMTU;
			else if (pc.mtu < ZT_MIN_PHYSMTU)
				pc.mtu = ZT_MIN_PHYSMTU;
			else if (pc.mtu > ZT_MAX_PHYSMTU)
				pc.mtu = ZT_MAX_PHYSMTU;

			cpaths[*(reinterpret_cast<const InetAddress *>(pathNetwork))] = pc;
		} else {
			cpaths.erase(*(reinterpret_cast<const InetAddress *>(pathNetwork)));
		}

		unsigned int cnt = 0;
		for(std::map<InetAddress,ZT_PhysicalPathConfiguration>::const_iterator i(cpaths.begin());((i!=cpaths.end())&&(cnt<ZT_MAX_CONFIGURABLE_PATHS));++i) {
			_physicalPathConfig[cnt].first = i->first;
			_physicalPathConfig[cnt].second = i->second;
			++cnt;
		}
		_numConfiguredPhysicalPaths = cnt;
	}
}

void Topology::_savePeer(void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		Buffer<ZT_PEER_MAX_SERIALIZED_STATE_SIZE> buf;
		peer->serializeForCache(buf);
		uint64_t tmpid[2]; tmpid[0] = peer->address().toInt(); tmpid[1] = 0;
		RR->node->stateObjectPut(tPtr,ZT_STATE_OBJECT_PEER,tmpid,buf.data(),buf.size());
	} catch ( ... ) {} // sanity check, discard invalid entries
}

} // namespace ZeroTier
