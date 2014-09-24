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

#include <algorithm>

#include "Constants.hpp"
#include "Defaults.hpp"
#include "Topology.hpp"
#include "NodeConfig.hpp"
#include "CMWC4096.hpp"
#include "Dictionary.hpp"

#define ZT_PEER_WRITE_BUF_SIZE 131072

namespace ZeroTier {

Topology::Topology(const RuntimeEnvironment *renv,bool enablePermanentIdCaching) :
	RR(renv),
	_amSupernode(false)
{
	if (enablePermanentIdCaching)
		_idCacheBase = (RR->homePath + ZT_PATH_SEPARATOR_S + "iddb.d");
	_loadPeers();
}

Topology::~Topology()
{
	clean();
	_dumpPeers();
}

void Topology::setSupernodes(const std::map< Identity,std::vector< std::pair<InetAddress,bool> > > &sn)
{
	Mutex::Lock _l(_supernodes_m);

	if (_supernodes == sn)
		return; // no change

	_supernodes = sn;
	_supernodeAddresses.clear();
	_supernodePeers.clear();
	uint64_t now = Utils::now();

	for(std::map< Identity,std::vector< std::pair<InetAddress,bool> > >::const_iterator i(sn.begin());i!=sn.end();++i) {
		if (i->first != RR->identity) {
			SharedPtr<Peer> p(getPeer(i->first.address()));
			if (!p)
				p = addPeer(SharedPtr<Peer>(new Peer(RR->identity,i->first)));
			for(std::vector< std::pair<InetAddress,bool> >::const_iterator j(i->second.begin());j!=i->second.end();++j)
				p->addPath(Path(j->first,(j->second) ? Path::PATH_TYPE_TCP_OUT : Path::PATH_TYPE_UDP,true));
			p->use(now);
			_supernodePeers.push_back(p);
		}
		_supernodeAddresses.insert(i->first.address());
	}

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
	Mutex::Lock _l(_activePeers_m);
	SharedPtr<Peer> p(_activePeers.insert(std::pair< Address,SharedPtr<Peer> >(peer->address(),peer)).first->second);
	p->use(now);
	saveIdentity(p->identity());
	return p;
}

SharedPtr<Peer> Topology::getPeer(const Address &zta) const
{
	if (zta == RR->identity.address()) {
		TRACE("BUG: ignored attempt to getPeer() for self, returned NULL");
		return SharedPtr<Peer>();
	}
	uint64_t now = Utils::now();
	Mutex::Lock _l(_activePeers_m);
	std::map< Address,SharedPtr<Peer> >::const_iterator ap(_activePeers.find(zta));
	if ((ap != _activePeers.end())&&(ap->second)) {
		ap->second->use(now);
		return ap->second;
	}
	return SharedPtr<Peer>();
}

Identity Topology::getIdentity(const Address &zta)
{
	SharedPtr<Peer> p(getPeer(zta));
	if (p)
		return p->identity();
	if (_idCacheBase.length()) {
		std::string idcPath(_idCacheBase + ZT_PATH_SEPARATOR_S + zta.toString());
		std::string ids;
		if (Utils::readFile(idcPath.c_str(),ids)) {
			try {
				return Identity(ids);
			} catch ( ... ) {} // ignore invalid IDs
		}
	}
	return Identity();
}

void Topology::saveIdentity(const Identity &id)
{
	if ((id)&&(_idCacheBase.length())) {
		std::string idcPath(_idCacheBase + ZT_PATH_SEPARATOR_S + id.address().toString());
		if (!Utils::fileExists(idcPath.c_str()))
			Utils::writeFile(idcPath.c_str(),id.toString(false));
	}
}

SharedPtr<Peer> Topology::getBestSupernode(const Address *avoid,unsigned int avoidCount,bool strictAvoid) const
{
	SharedPtr<Peer> bestSupernode;
	uint64_t now = Utils::now();
	Mutex::Lock _l(_supernodes_m);

	if (_amSupernode) {
		/* If I am a supernode, the "best" supernode is the one whose address
		 * is numerically greater than mine (with wrap at top of list). This
		 * causes packets searching for a route to pretty much literally
		 * circumnavigate the globe rather than bouncing between just two. */

		if (_supernodeAddresses.size() > 1) { // gotta be one other than me for this to work
			std::set<Address>::const_iterator sna(_supernodeAddresses.find(RR->identity.address()));
			if (sna != _supernodeAddresses.end()) { // sanity check -- _amSupernode should've been false in this case
				for(;;) {
					if (++sna == _supernodeAddresses.end())
						sna = _supernodeAddresses.begin(); // wrap around at end
					if (*sna != RR->identity.address()) { // pick one other than us -- starting from me+1 in sorted set order
						SharedPtr<Peer> p(getPeer(*sna));
						if ((p)&&(p->hasActiveDirectPath(now))) {
							bestSupernode = p;
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

void Topology::clean()
{
	uint64_t now = Utils::now();
	Mutex::Lock _l(_activePeers_m);
	Mutex::Lock _l2(_supernodes_m);
	for(std::map< Address,SharedPtr<Peer> >::iterator p(_activePeers.begin());p!=_activePeers.end();) {
		if (((now - p->second->lastUsed()) >= ZT_PEER_IN_MEMORY_EXPIRATION)&&(!_supernodeAddresses.count(p->second->address())))
			_activePeers.erase(p++);
		else {
			p->second->clean(now);
			++p;
		}
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

void Topology::_dumpPeers()
{
	Buffer<ZT_PEER_WRITE_BUF_SIZE> buf;
	std::string pdpath(RR->homePath + ZT_PATH_SEPARATOR_S + "peers.persist");
	Mutex::Lock _l(_activePeers_m);

	FILE *pd = fopen(pdpath.c_str(),"wb");
	if (!pd)
		return;
	if (fwrite("ZTPD0",5,1,pd) != 1) {
		fclose(pd);
		Utils::rm(pdpath);
		return;
	}

	for(std::map< Address,SharedPtr<Peer> >::iterator p(_activePeers.begin());p!=_activePeers.end();++p) {
		try {
			p->second->serialize(buf);
			if (buf.size() >= (ZT_PEER_WRITE_BUF_SIZE / 2)) {
				if (fwrite(buf.data(),buf.size(),1,pd) != 1) {
					fclose(pd);
					Utils::rm(pdpath);
					buf.burn();
					return;
				}
				buf.clear();
				buf.burn();
			}
		} catch ( ... ) {
			fclose(pd);
			Utils::rm(pdpath);
			buf.burn();
			return;
		}
	}

	if (buf.size()) {
		if (fwrite(buf.data(),buf.size(),1,pd) != 1) {
			fclose(pd);
			Utils::rm(pdpath);
			buf.burn();
			return;
		}
		buf.burn();
	}

	fclose(pd);
	Utils::lockDownFile(pdpath.c_str(),false);
	buf.burn();
}

void Topology::_loadPeers()
{
	Buffer<ZT_PEER_WRITE_BUF_SIZE> buf;
	std::string pdpath(RR->homePath + ZT_PATH_SEPARATOR_S + "peers.persist");
	Mutex::Lock _l(_activePeers_m);

	_activePeers.clear();

	FILE *pd = fopen(pdpath.c_str(),"rb");
	if (!pd)
		return;

	try {
		char magic[5];
		if ((fread(magic,5,1,pd) == 1)&&(!memcmp("ZTPD0",magic,5))) {
			long rlen = 0;
			do {
				long rlen = (long)fread(buf.data() + buf.size(),1,ZT_PEER_WRITE_BUF_SIZE - buf.size(),pd);
				if (rlen < 0) rlen = 0;
				buf.setSize(buf.size() + (unsigned int)rlen);
				unsigned int ptr = 0;
				while ((ptr < (ZT_PEER_WRITE_BUF_SIZE / 2))&&(ptr < buf.size())) {
					SharedPtr<Peer> p(new Peer());
					ptr += p->deserialize(buf,ptr);
					_activePeers[p->address()] = p;
					saveIdentity(p->identity());
				}
				if (ptr) {
					memmove(buf.data(),buf.data() + ptr,buf.size() - ptr);
					buf.setSize(buf.size() - ptr);
				}
			} while (rlen > 0);
		}
	} catch ( ... ) {
		_activePeers.clear();
	}

	fclose(pd);
	Utils::rm(pdpath);
	buf.burn();
}

} // namespace ZeroTier
