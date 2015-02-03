/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2015  ZeroTier Networks
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
#include "Defaults.hpp"
#include "Topology.hpp"
#include "NodeConfig.hpp"
#include "CMWC4096.hpp"
#include "Dictionary.hpp"

#define ZT_PEER_WRITE_BUF_SIZE 131072

namespace ZeroTier {

Topology::Topology(const RuntimeEnvironment *renv) :
	RR(renv),
	_idCacheBase(renv->homePath + ZT_PATH_SEPARATOR_S + "iddb.d"),
	_amSupernode(false)
{
}

Topology::~Topology()
{
}

void Topology::setSupernodes(const std::map< Identity,std::vector< std::pair<InetAddress,bool> > > &sn)
{
	Mutex::Lock _l(_lock);

	if (_supernodes == sn)
		return; // no change

	_supernodes = sn;
	_supernodeAddresses.clear();
	_supernodePeers.clear();
	uint64_t now = Utils::now();

	for(std::map< Identity,std::vector< std::pair<InetAddress,bool> > >::const_iterator i(sn.begin());i!=sn.end();++i) {
		if (i->first != RR->identity) { // do not add self as a peer
			SharedPtr<Peer> &p = _activePeers[i->first.address()];
			if (!p)
				p = SharedPtr<Peer>(new Peer(RR->identity,i->first));
			for(std::vector< std::pair<InetAddress,bool> >::const_iterator j(i->second.begin());j!=i->second.end();++j)
				p->addPath(Path(j->first,(j->second) ? Path::PATH_TYPE_TCP_OUT : Path::PATH_TYPE_UDP,true));
			p->use(now);
			_supernodePeers.push_back(p);
		}
		_supernodeAddresses.push_back(i->first.address());
	}

	std::sort(_supernodeAddresses.begin(),_supernodeAddresses.end());

	_amSupernode = (_supernodes.find(RR->identity) != _supernodes.end());
}

void Topology::setSupernodes(const Dictionary &sn)
{
	std::map< Identity,std::vector< std::pair<InetAddress,bool> > > m;
	for(Dictionary::const_iterator d(sn.begin());d!=sn.end();++d) {
		if ((d->first.length() == ZT_ADDRESS_LENGTH_HEX)&&(d->second.length() > 0)) {
			try {
				Dictionary snspec(d->second);
				std::vector< std::pair<InetAddress,bool> > &a = m[Identity(snspec.get("id"))];
				std::string udp(snspec.get("udp",std::string()));
				if (udp.length() > 0)
					a.push_back(std::pair<InetAddress,bool>(InetAddress(udp),false));
				std::string tcp(snspec.get("tcp",std::string()));
				if (tcp.length() > 0)
					a.push_back(std::pair<InetAddress,bool>(InetAddress(tcp),true));
			} catch ( ... ) {
				LOG("supernode list contained invalid entry for: %s",d->first.c_str());
			}
		}
	}
	this->setSupernodes(m);
}

SharedPtr<Peer> Topology::addPeer(const SharedPtr<Peer> &peer)
{
	if (peer->address() == RR->identity.address()) {
		TRACE("BUG: addNewPeer() caught and ignored attempt to add peer for self");
		throw std::logic_error("cannot add peer for self");
	}

	uint64_t now = Utils::now();
	Mutex::Lock _l(_lock);

	SharedPtr<Peer> p(_activePeers.insert(std::pair< Address,SharedPtr<Peer> >(peer->address(),peer)).first->second);
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

	uint64_t now = Utils::now();
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

SharedPtr<Peer> Topology::getBestSupernode(const Address *avoid,unsigned int avoidCount,bool strictAvoid)
{
	SharedPtr<Peer> bestSupernode;
	uint64_t now = Utils::now();
	Mutex::Lock _l(_lock);

	if (_amSupernode) {
		/* If I am a supernode, the "best" supernode is the one whose address
		 * is numerically greater than mine (with wrap at top of list). This
		 * causes packets searching for a route to pretty much literally
		 * circumnavigate the globe rather than bouncing between just two. */

		if (_supernodeAddresses.size() > 1) { // gotta be one other than me for this to work
			std::vector<Address>::const_iterator sna(std::find(_supernodeAddresses.begin(),_supernodeAddresses.end(),RR->identity.address()));
			if (sna != _supernodeAddresses.end()) { // sanity check -- _amSupernode should've been false in this case
				for(;;) {
					if (++sna == _supernodeAddresses.end())
						sna = _supernodeAddresses.begin(); // wrap around at end
					if (*sna != RR->identity.address()) { // pick one other than us -- starting from me+1 in sorted set order
						std::map< Address,SharedPtr<Peer> >::const_iterator p(_activePeers.find(*sna));
						if ((p != _activePeers.end())&&(p->second->hasActiveDirectPath(now))) {
							bestSupernode = p->second;
							break;
						}
					}
				}
			}
		}
	} else {
		/* If I am not a supernode, the best supernode is the active one with
		 * the lowest latency. */

		unsigned int l,bestSupernodeLatency = 65536;
		uint64_t lds,ldr;

		// First look for a best supernode by comparing latencies, but exclude
		// supernodes that have not responded to direct messages in order to
		// try to exclude any that are dead or unreachable.
		for(std::vector< SharedPtr<Peer> >::const_iterator sn(_supernodePeers.begin());sn!=_supernodePeers.end();) {
			// Skip explicitly avoided relays
			for(unsigned int i=0;i<avoidCount;++i) {
				if (avoid[i] == (*sn)->address())
					goto keep_searching_for_supernodes;
			}

			// Skip possibly comatose or unreachable relays
			lds = (*sn)->lastDirectSend();
			ldr = (*sn)->lastDirectReceive();
			if ((lds)&&(lds > ldr)&&((lds - ldr) > ZT_PEER_RELAY_CONVERSATION_LATENCY_THRESHOLD))
				goto keep_searching_for_supernodes;

			if ((*sn)->hasActiveDirectPath(now)) {
				l = (*sn)->latency();
				if (bestSupernode) {
					if ((l)&&(l < bestSupernodeLatency)) {
						bestSupernodeLatency = l;
						bestSupernode = *sn;
					}
				} else {
					if (l)
						bestSupernodeLatency = l;
					bestSupernode = *sn;
				}
			}

keep_searching_for_supernodes:
			++sn;
		}

		if (bestSupernode) {
			bestSupernode->use(now);
			return bestSupernode;
		} else if (strictAvoid)
			return SharedPtr<Peer>();

		// If we have nothing from above, just pick one without avoidance criteria.
		for(std::vector< SharedPtr<Peer> >::const_iterator sn=_supernodePeers.begin();sn!=_supernodePeers.end();++sn) {
			if ((*sn)->hasActiveDirectPath(now)) {
				unsigned int l = (*sn)->latency();
				if (bestSupernode) {
					if ((l)&&(l < bestSupernodeLatency)) {
						bestSupernodeLatency = l;
						bestSupernode = *sn;
					}
				} else {
					if (l)
						bestSupernodeLatency = l;
					bestSupernode = *sn;
				}
			}
		}
	}

	if (bestSupernode)
		bestSupernode->use(now);
	return bestSupernode;
}

void Topology::clean(uint64_t now)
{
	Mutex::Lock _l(_lock);
	for(std::map< Address,SharedPtr<Peer> >::iterator p(_activePeers.begin());p!=_activePeers.end();) {
		if (((now - p->second->lastUsed()) >= ZT_PEER_IN_MEMORY_EXPIRATION)&&(std::find(_supernodeAddresses.begin(),_supernodeAddresses.end(),p->first) == _supernodeAddresses.end())) {
			_activePeers.erase(p++);
		} else {
			p->second->clean(now);
			++p;
		}
	}
}

bool Topology::updateSurface(const SharedPtr<Peer> &remotePeer,const InetAddress &mirroredAddress)
{
	Mutex::Lock _l(_lock);

	if (std::find(_supernodeAddresses.begin(),_supernodeAddresses.end(),remotePeer->address()) == _supernodeAddresses.end())
		return false;

	if (_surface.update(mirroredAddress)) {
		// Clear non-fixed paths for all peers
		for(std::map< Address,SharedPtr<Peer> >::const_iterator ap(_activePeers.begin());ap!=_activePeers.end();++ap)
			ap->second->clearPaths(false);

		return true;
	}

	return false;
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
	std::string idcPath(_idCacheBase + ZT_PATH_SEPARATOR_S + zta.toString());
	std::string ids;
	if (Utils::readFile(idcPath.c_str(),ids)) {
		try {
			return Identity(ids);
		} catch ( ... ) {} // ignore invalid IDs
	}
	return Identity();
}

void Topology::_saveIdentity(const Identity &id)
{
	if (id) {
		std::string idcPath(_idCacheBase + ZT_PATH_SEPARATOR_S + id.address().toString());
		Utils::writeFile(idcPath.c_str(),id.toString(false));
	}
}

} // namespace ZeroTier
