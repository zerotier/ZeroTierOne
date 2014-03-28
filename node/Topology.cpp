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

#include "Topology.hpp"
#include "NodeConfig.hpp"
#include "CMWC4096.hpp"

#define ZT_PEER_WRITE_BUF_SIZE 131072

namespace ZeroTier {

Topology::Topology(const RuntimeEnvironment *renv,bool enablePermanentIdCaching) :
	_r(renv),
	_amSupernode(false)
{
	if (enablePermanentIdCaching)
		_idCacheBase = (_r->homePath + ZT_PATH_SEPARATOR_S + "iddb.d");
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

	_supernodes = sn;
	_supernodeAddresses.clear();
	_supernodePeers.clear();
	uint64_t now = Utils::now();

	for(std::map< Identity,std::vector< std::pair<InetAddress,bool> > >::const_iterator i(sn.begin());i!=sn.end();++i) {
		if (i->first != _r->identity) {
			SharedPtr<Peer> p(getPeer(i->first.address()));
			if (!p)
				p = addPeer(SharedPtr<Peer>(new Peer(_r->identity,i->first)));
			for(std::vector< std::pair<InetAddress,bool> >::const_iterator j(i->second.begin());j!=i->second.end();++j)
				p->addPath(Path(j->first,j->second,true));
			p->use(now);
			_supernodePeers.push_back(p);
		}
		_supernodeAddresses.insert(i->first.address());
	}

	_amSupernode = (_supernodes.find(_r->identity) != _supernodes.end());
}

SharedPtr<Peer> Topology::addPeer(const SharedPtr<Peer> &peer)
{
	if (peer->address() == _r->identity.address()) {
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

SharedPtr<Peer> Topology::getPeer(const Address &zta)
{
	if (zta == _r->identity.address()) {
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
	unsigned int l,bestSupernodeLatency = 65536;
	uint64_t now = Utils::now();
	uint64_t lds,ldr;

	Mutex::Lock _l(_supernodes_m);

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
		else ++p;
	}
}

void Topology::_dumpPeers()
{
	Buffer<ZT_PEER_WRITE_BUF_SIZE> buf;
	std::string pdpath(_r->homePath + ZT_PATH_SEPARATOR_S + "peers.persist");
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
					return;
				}
				buf.clear();
			}
		} catch ( ... ) {
			fclose(pd);
			Utils::rm(pdpath);
			return;
		}
	}

	if (buf.size()) {
		if (fwrite(buf.data(),buf.size(),1,pd) != 1) {
			fclose(pd);
			Utils::rm(pdpath);
			return;
		}
	}

	fclose(pd);
	Utils::lockDownFile(pdpath.c_str(),false);
}

void Topology::_loadPeers()
{
	Buffer<ZT_PEER_WRITE_BUF_SIZE> buf;
	std::string pdpath(_r->homePath + ZT_PATH_SEPARATOR_S + "peers.persist");
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
}

} // namespace ZeroTier
