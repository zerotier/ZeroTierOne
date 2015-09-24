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

#include "Constants.hpp"
#include "Topology.hpp"
#include "RuntimeEnvironment.hpp"
#include "Defaults.hpp"
#include "Dictionary.hpp"
#include "Node.hpp"

namespace ZeroTier {

Topology::Topology(const RuntimeEnvironment *renv) :
	RR(renv),
	_amRoot(false)
{
}

Topology::~Topology()
{
}

void Topology::setRootServers(const std::map< Identity,std::vector<InetAddress> > &sn)
{
	Mutex::Lock _l(_lock);

	if (_roots == sn)
		return; // no change

	_roots = sn;
	_rootAddresses.clear();
	_rootPeers.clear();
	const uint64_t now = RR->node->now();

	for(std::map< Identity,std::vector<InetAddress> >::const_iterator i(sn.begin());i!=sn.end();++i) {
		if (i->first != RR->identity) { // do not add self as a peer
			SharedPtr<Peer> &p = _activePeers[i->first.address()];
			if (!p)
				p = SharedPtr<Peer>(new Peer(RR->identity,i->first));
			for(std::vector<InetAddress>::const_iterator j(i->second.begin());j!=i->second.end();++j)
				p->addPath(RemotePath(0,*j,true));
			p->use(now);
			_rootPeers.push_back(p);
		}
		_rootAddresses.push_back(i->first.address());
	}

	std::sort(_rootAddresses.begin(),_rootAddresses.end());

	_amRoot = (_roots.find(RR->identity) != _roots.end());
}

void Topology::setRootServers(const Dictionary &sn)
{
	std::map< Identity,std::vector<InetAddress> > m;
	for(Dictionary::const_iterator d(sn.begin());d!=sn.end();++d) {
		if ((d->first.length() == ZT_ADDRESS_LENGTH_HEX)&&(d->second.length() > 0)) {
			try {
				Dictionary snspec(d->second);
				std::vector<InetAddress> &a = m[Identity(snspec.get("id"))];
				std::string udp(snspec.get("udp",std::string()));
				if (udp.length() > 0)
					a.push_back(InetAddress(udp));
			} catch ( ... ) {
				TRACE("root server list contained invalid entry for: %s",d->first.c_str());
			}
		}
	}
	this->setRootServers(m);
}

SharedPtr<Peer> Topology::addPeer(const SharedPtr<Peer> &peer)
{
	if (peer->address() == RR->identity.address()) {
		TRACE("BUG: addNewPeer() caught and ignored attempt to add peer for self");
		throw std::logic_error("cannot add peer for self");
	}

	const uint64_t now = RR->node->now();
	Mutex::Lock _l(_lock);

	SharedPtr<Peer> &p = _activePeers.set(peer->address(),peer);
	p->use(now);
	_saveIdentity(p->identity());

	return p;
}

SharedPtr<Peer> Topology::getPeer(const Address &zta)
{
	if (zta == RR->identity.address()) {
		TRACE("BUG: ignored attempt to getPeer() for self, returned NULL");
		return SharedPtr<Peer>();
	}

	const uint64_t now = RR->node->now();
	Mutex::Lock _l(_lock);

	SharedPtr<Peer> &ap = _activePeers[zta];

	if (ap) {
		ap->use(now);
		return ap;
	}

	Identity id(_getIdentity(zta));
	if (id) {
		try {
			ap = SharedPtr<Peer>(new Peer(RR->identity,id));
			ap->use(now);
			return ap;
		} catch ( ... ) {} // invalid identity?
	}

	_activePeers.erase(zta);

	return SharedPtr<Peer>();
}

SharedPtr<Peer> Topology::getBestRoot(const Address *avoid,unsigned int avoidCount,bool strictAvoid)
{
	SharedPtr<Peer> bestRoot;
	const uint64_t now = RR->node->now();
	Mutex::Lock _l(_lock);

	if (_amRoot) {
		/* If I am a root server, the "best" root server is the one whose address
		 * is numerically greater than mine (with wrap at top of list). This
		 * causes packets searching for a route to pretty much literally
		 * circumnavigate the globe rather than bouncing between just two. */

		if (_rootAddresses.size() > 1) { // gotta be one other than me for this to work
			std::vector<Address>::const_iterator sna(std::find(_rootAddresses.begin(),_rootAddresses.end(),RR->identity.address()));
			if (sna != _rootAddresses.end()) { // sanity check -- _amRoot should've been false in this case
				for(;;) {
					if (++sna == _rootAddresses.end())
						sna = _rootAddresses.begin(); // wrap around at end
					if (*sna != RR->identity.address()) { // pick one other than us -- starting from me+1 in sorted set order
						SharedPtr<Peer> *p = _activePeers.get(*sna);
						if ((p)&&((*p)->hasActiveDirectPath(now))) {
							bestRoot = *p;
							break;
						}
					}
				}
			}
		}
	} else {
		/* If I am not a root server, the best root server is the active one with
		 * the lowest latency. */

		unsigned int l,bestLatency = 65536;
		uint64_t lds,ldr;

		// First look for a best root by comparing latencies, but exclude
		// root servers that have not responded to direct messages in order to
		// try to exclude any that are dead or unreachable.
		for(std::vector< SharedPtr<Peer> >::const_iterator sn(_rootPeers.begin());sn!=_rootPeers.end();) {
			// Skip explicitly avoided relays
			for(unsigned int i=0;i<avoidCount;++i) {
				if (avoid[i] == (*sn)->address())
					goto keep_searching_for_roots;
			}

			// Skip possibly comatose or unreachable relays
			lds = (*sn)->lastDirectSend();
			ldr = (*sn)->lastDirectReceive();
			if ((lds)&&(lds > ldr)&&((lds - ldr) > ZT_PEER_RELAY_CONVERSATION_LATENCY_THRESHOLD))
				goto keep_searching_for_roots;

			if ((*sn)->hasActiveDirectPath(now)) {
				l = (*sn)->latency();
				if (bestRoot) {
					if ((l)&&(l < bestLatency)) {
						bestLatency = l;
						bestRoot = *sn;
					}
				} else {
					if (l)
						bestLatency = l;
					bestRoot = *sn;
				}
			}

keep_searching_for_roots:
			++sn;
		}

		if (bestRoot) {
			bestRoot->use(now);
			return bestRoot;
		} else if (strictAvoid)
			return SharedPtr<Peer>();

		// If we have nothing from above, just pick one without avoidance criteria.
		for(std::vector< SharedPtr<Peer> >::const_iterator sn=_rootPeers.begin();sn!=_rootPeers.end();++sn) {
			if ((*sn)->hasActiveDirectPath(now)) {
				unsigned int l = (*sn)->latency();
				if (bestRoot) {
					if ((l)&&(l < bestLatency)) {
						bestLatency = l;
						bestRoot = *sn;
					}
				} else {
					if (l)
						bestLatency = l;
					bestRoot = *sn;
				}
			}
		}
	}

	if (bestRoot)
		bestRoot->use(now);
	return bestRoot;
}

bool Topology::isRoot(const Identity &id) const
	throw()
{
	Mutex::Lock _l(_lock);
	return (_roots.count(id) != 0);
}

void Topology::clean(uint64_t now)
{
	Mutex::Lock _l(_lock);
	Hashtable< Address,SharedPtr<Peer> >::Iterator i(_activePeers);
	Address *a = (Address *)0;
	SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
	while (i.next(a,p))
		if (((now - (*p)->lastUsed()) >= ZT_PEER_IN_MEMORY_EXPIRATION)&&(std::find(_rootAddresses.begin(),_rootAddresses.end(),*a) == _rootAddresses.end())) {
			_activePeers.erase(*a);
	}
}

bool Topology::authenticateRootTopology(const Dictionary &rt)
{
	try {
		std::string signer(rt.signingIdentity());
		if (!signer.length())
			return false;
		Identity signerId(signer);
		std::map< Address,Identity >::const_iterator authority(ZT_DEFAULTS.rootTopologyAuthorities.find(signerId.address()));
		if (authority == ZT_DEFAULTS.rootTopologyAuthorities.end())
			return false;
		if (signerId != authority->second)
			return false;
		return rt.verify(authority->second);
	} catch ( ... ) {
		return false;
	}
}

Identity Topology::_getIdentity(const Address &zta)
{
	char p[128];
	Utils::snprintf(p,sizeof(p),"iddb.d/%.10llx",(unsigned long long)zta.toInt());
	std::string ids(RR->node->dataStoreGet(p));
	if (ids.length() > 0) {
		try {
			return Identity(ids);
		} catch ( ... ) {} // ignore invalid IDs
	}
	return Identity();
}

void Topology::_saveIdentity(const Identity &id)
{
	if (id) {
		char p[128];
		Utils::snprintf(p,sizeof(p),"iddb.d/%.10llx",(unsigned long long)id.address().toInt());
		RR->node->dataStorePut(p,id.toString(false),false);
	}
}

} // namespace ZeroTier
