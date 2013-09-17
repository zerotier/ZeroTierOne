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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "NodeConfig.hpp"
#include "Network.hpp"
#include "Switch.hpp"
#include "Packet.hpp"
#include "Utils.hpp"

namespace ZeroTier {

void Network::CertificateOfMembership::addParameter(uint64_t id,uint64_t value,uint64_t maxDelta)
{
	_params.push_back(_Parameter(id,value,maxDelta));
	std::sort(_params.begin(),_params.end(),_SortByIdComparison());
}

std::string Network::CertificateOfMembership::toString() const
{
	uint64_t tmp[3000];
	unsigned long n = 0;
	for(std::vector<_Parameter>::const_iterator p(_params.begin());p!=_params.end();++p) {
		tmp[n++] = Utils::hton(p->id);
		tmp[n++] = Utils::hton(p->value);
		tmp[n++] = Utils::hton(p->maxDelta);
		if (n >= 3000)
			break; // sanity check -- certificates will never even approach this size
	}
	return Utils::hex(tmp,sizeof(uint64_t) * n);
}

void Network::CertificateOfMembership::fromString(const char *s)
{
	std::string tmp(Utils::unhex(s));
	_params.clear();
	const char *ptr = tmp.data();
	unsigned long remaining = tmp.length();
	while (remaining >= 24) {
		_Parameter p;
		p.id = Utils::ntoh(*((const uint64_t *)(ptr)));
		p.value = Utils::ntoh(*((const uint64_t *)(ptr + 8)));
		p.maxDelta = Utils::ntoh(*((const uint64_t *)(ptr + 16)));
		_params.push_back(p);
		ptr += 24;
		remaining -= 24;
	}
}

bool Network::CertificateOfMembership::compare(const CertificateOfMembership &other) const
	throw()
{
	unsigned long myidx = 0;
	unsigned long otheridx = 0;

	while (myidx < _params.size()) {
		// Fail if we're at the end of other, since this means the field is
		// missing.
		if (otheridx >= other._params.size())
			return false;

		// Seek to corresponding tuple in other, ignoring tuples that
		// we may not have. If we run off the end of other, the tuple is
		// missing. This works because tuples are sorted by ID.
		while (other._params[otheridx].id != _params[myidx].id) {
			++otheridx;
			if (otheridx >= other._params.size())
				return false;
		}

		// Compare to determine if the absolute value of the difference
		// between these two parameters is within our maxDelta.
		uint64_t a = _params[myidx].value;
		uint64_t b = other._params[myidx].value;
		if (a >= b) {
			if ((a - b) > _params[myidx].maxDelta)
				return false;
		} else {
			if ((b - a) > _params[myidx].maxDelta)
				return false;
		}

		++myidx;
	}

	return true;
}

const Network::MulticastRates::Rate Network::MulticastRates::GLOBAL_DEFAULT_RATE(65535,65535,64);

const char *Network::statusString(const Status s)
	throw()
{
	switch(s) {
		case NETWORK_WAITING_FOR_FIRST_AUTOCONF: return "WAITING_FOR_FIRST_AUTOCONF";
		case NETWORK_OK: return "OK";
		case NETWORK_ACCESS_DENIED: return "ACCESS_DENIED";
	}
	return "(invalid)";
}

Network::~Network()
{
	delete _tap;

	if (_destroyOnDelete) {
		std::string confPath(_r->homePath + ZT_PATH_SEPARATOR_S + "networks.d" + ZT_PATH_SEPARATOR_S + idString() + ".conf");
		std::string mcdbPath(_r->homePath + ZT_PATH_SEPARATOR_S + "networks.d" + ZT_PATH_SEPARATOR_S + idString() + ".mcerts");
		Utils::rm(confPath);
		Utils::rm(mcdbPath);
	} else {
		// Causes flush of membership certs to disk
		clean();
	}
}

SharedPtr<Network> Network::newInstance(const RuntimeEnvironment *renv,uint64_t id)
	throw(std::runtime_error)
{
	char tag[32];
	Utils::snprintf(tag,sizeof(tag),"%.16llx",(unsigned long long)id);

	// We construct Network via a static method to ensure that it is immediately
	// wrapped in a SharedPtr<>. Otherwise if there is traffic on the Ethernet
	// tap device, a SharedPtr<> wrap can occur in the Ethernet frame handler
	// that then causes the Network instance to be deleted before it is finished
	// being constructed. C++ edge cases, how I love thee.
	SharedPtr<Network> nw(new Network());
	nw->_ready = false; // disable handling of Ethernet frames during construct
	nw->_r = renv;
	nw->_tap = new EthernetTap(renv,tag,renv->identity.address().toMAC(),ZT_IF_MTU,&_CBhandleTapData,nw.ptr());
	nw->_multicastPropagationBreadth = 0;
	nw->_multicastPropagationDepth = 0;
	memset(nw->_etWhitelist,0,sizeof(nw->_etWhitelist));
	nw->_id = id;
	nw->_lastConfigUpdate = 0;
	nw->_destroyOnDelete = false;
	if (nw->controller() == renv->identity.address()) // sanity check, this isn't supported for now
		throw std::runtime_error("cannot add a network for which I am the netconf master");
	nw->_restoreState();
	nw->_ready = true; // enable handling of Ethernet frames
	nw->requestConfiguration();
	return nw;
}

void Network::setConfiguration(const Network::Config &conf)
{
	Mutex::Lock _l(_lock);
	try {
		if (conf.networkId() == _id) { // sanity check
			_configuration = conf;
			_myCertificate = conf.certificateOfMembership();
			_mcRates = conf.multicastRates();
			_multicastPropagationBreadth = conf.multicastPropagationBreadth();
			_multicastPropagationDepth = conf.multicastPropagationDepth();
			_lastConfigUpdate = Utils::now();

			_tap->setIps(conf.staticAddresses());
			_tap->setDisplayName((std::string("ZeroTier One [") + conf.name() + "]").c_str());

			memset(_etWhitelist,0,sizeof(_etWhitelist));
			std::set<unsigned int> wl(conf.etherTypes());
			for(std::set<unsigned int>::const_iterator t(wl.begin());t!=wl.end();++t)
				_etWhitelist[*t / 8] |= (unsigned char)(1 << (*t % 8));

			std::string confPath(_r->homePath + ZT_PATH_SEPARATOR_S + "networks.d" + ZT_PATH_SEPARATOR_S + idString() + ".conf");
			if (!Utils::writeFile(confPath.c_str(),conf.toString())) {
				LOG("error: unable to write network configuration file at: %s",confPath.c_str());
			}
		}
	} catch ( ... ) {
		_configuration = Config();
		_myCertificate = CertificateOfMembership();
		_lastConfigUpdate = 0;
		LOG("unexpected exception handling config for network %.16llx, retrying fetch...",(unsigned long long)_id);
	}
}

void Network::requestConfiguration()
{
	if (controller() == _r->identity.address()) {
		LOG("unable to request network configuration for network %.16llx: I am the network master, cannot query self",(unsigned long long)_id);
		return;
	}
	TRACE("requesting netconf for network %.16llx from netconf master %s",(unsigned long long)_id,controller().toString().c_str());
	Packet outp(controller(),_r->identity.address(),Packet::VERB_NETWORK_CONFIG_REQUEST);
	outp.append((uint64_t)_id);
	outp.append((uint16_t)0); // no meta-data
	_r->sw->send(outp,true);
}

void Network::addMembershipCertificate(const Address &peer,const CertificateOfMembership &cert)
{
	Mutex::Lock _l(_lock);
	if (!_configuration.isOpen())
		_membershipCertificates[peer] = cert;
}

bool Network::isAllowed(const Address &peer) const
{
	// Exceptions can occur if we do not yet have *our* configuration.
	try {
		Mutex::Lock _l(_lock);
		if (_configuration.isOpen())
			return true;
		std::map<Address,CertificateOfMembership>::const_iterator pc(_membershipCertificates.find(peer));
		if (pc == _membershipCertificates.end())
			return false;
		return _myCertificate.compare(pc->second);
	} catch (std::exception &exc) {
		TRACE("isAllowed() check failed for peer %s: unexpected exception: %s",peer.toString().c_str(),exc.what());
	} catch ( ... ) {
		TRACE("isAllowed() check failed for peer %s: unexpected exception: unknown exception",peer.toString().c_str());
	}
	return false;
}

void Network::clean()
{
	std::string mcdbPath(_r->homePath + ZT_PATH_SEPARATOR_S + "networks.d" + ZT_PATH_SEPARATOR_S + idString() + ".mcerts");

	Mutex::Lock _l(_lock);

	if ((!_id)||(_configuration.isOpen())) {
		_membershipCertificates.clear();
		Utils::rm(mcdbPath);
	} else {
		FILE *mcdb = fopen(mcdbPath.c_str(),"wb");
		bool writeError = false;
		if (!mcdb) {
			LOG("error: unable to open membership cert database at: %s",mcdbPath.c_str());
		} else {
			if ((writeError)||(fwrite("MCDB0",5,1,mcdb) != 1)) // version
				writeError = true;
		}

		for(std::map<Address,CertificateOfMembership>::iterator i=(_membershipCertificates.begin());i!=_membershipCertificates.end();) {
			if (_myCertificate.compare(i->second)) {
				if ((!writeError)&&(mcdb)) {
					char tmp[ZT_ADDRESS_LENGTH];
					i->first.copyTo(tmp,ZT_ADDRESS_LENGTH);
					if ((writeError)||(fwrite(tmp,ZT_ADDRESS_LENGTH,1,mcdb) != 1))
						writeError = true;
					std::string c(i->second.toString());
					uint32_t cl = Utils::hton((uint32_t)c.length());
					if ((writeError)||(fwrite(&cl,sizeof(cl),1,mcdb) != 1))
						writeError = true;
					if ((writeError)||(fwrite(c.data(),c.length(),1,mcdb) != 1))
						writeError = true;
				}
				++i;
			} else _membershipCertificates.erase(i++);
		}

		if (mcdb)
			fclose(mcdb);
		if (writeError) {
			Utils::rm(mcdbPath);
			LOG("error: unable to write to membership cert database at: %s",mcdbPath.c_str());
		}
	}
}

Network::Status Network::status() const
{
	Mutex::Lock _l(_lock);
	if (_configuration)
		return NETWORK_OK;
	return NETWORK_WAITING_FOR_FIRST_AUTOCONF;
}

void Network::_CBhandleTapData(void *arg,const MAC &from,const MAC &to,unsigned int etherType,const Buffer<4096> &data)
{
	if (!((Network *)arg)->_ready)
		return;
	const RuntimeEnvironment *_r = ((Network *)arg)->_r;
	if (_r->shutdownInProgress)
		return;
	try {
		_r->sw->onLocalEthernet(SharedPtr<Network>((Network *)arg),from,to,etherType,data);
	} catch (std::exception &exc) {
		TRACE("unexpected exception handling local packet: %s",exc.what());
	} catch ( ... ) {
		TRACE("unexpected exception handling local packet");
	}
}

void Network::_restoreState()
{
	std::string confPath(_r->homePath + ZT_PATH_SEPARATOR_S + "networks.d" + ZT_PATH_SEPARATOR_S + idString() + ".conf");
	std::string confs;
	if (Utils::readFile(confPath.c_str(),confs)) {
		try {
			if (confs.length())
				setConfiguration(Config(confs));
		} catch ( ... ) {} // ignore invalid config on disk, we will re-request
	} else {
		// If the conf file isn't present, "touch" it so we'll remember
		// the existence of this network.
		FILE *tmp = fopen(confPath.c_str(),"w");
		if (tmp)
			fclose(tmp);
	}
	// TODO: restore membership certs
}

} // namespace ZeroTier
