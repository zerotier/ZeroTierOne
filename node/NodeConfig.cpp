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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <map>
#include <set>

#include "Constants.hpp"

#include "NodeConfig.hpp"
#include "RuntimeEnvironment.hpp"
#include "Defaults.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include "Topology.hpp"
#include "Packet.hpp"
#include "InetAddress.hpp"
#include "Peer.hpp"
#include "Node.hpp"
#include "SoftwareUpdater.hpp"

namespace ZeroTier {

NodeConfig::NodeConfig(const RuntimeEnvironment *renv) :
	RR(renv)
{
	{
		Mutex::Lock _l(_localConfig_m);
		_readLocalConfig();
	}

	std::string networksFolder(RR->homePath + ZT_PATH_SEPARATOR_S + "networks.d");
	std::map<std::string,bool> networksDotD(Utils::listDirectory(networksFolder.c_str()));
	std::vector<uint64_t> configuredNets;
	for(std::map<std::string,bool>::iterator d(networksDotD.begin());d!=networksDotD.end();++d) {
		if (!d->second) {
			std::string::size_type dot = d->first.rfind(".conf");
			if (dot != std::string::npos) {
				uint64_t nwid = Utils::hexStrToU64(d->first.substr(0,dot).c_str());
				if ((nwid > 0)&&(std::find(configuredNets.begin(),configuredNets.end(),nwid) == configuredNets.end()))
					configuredNets.push_back(nwid);
			}
		}
	}

	for(std::vector<uint64_t>::iterator n(configuredNets.begin());n!=configuredNets.end();++n) {
		try {
			_networks[*n] = Network::newInstance(RR,this,*n);
		} catch (std::exception &exc) {
			LOG("unable to create network %.16llx: %s",(unsigned long long)*n,exc.what());
		} catch ( ... ) {
			LOG("unable to create network %.16llx: (unknown exception)",(unsigned long long)*n);
		}
	}
}

NodeConfig::~NodeConfig()
{
	_writeLocalConfig();
}

void NodeConfig::putLocalConfig(const std::string &key,const char *value)
{
	Mutex::Lock _l(_localConfig_m);
	_localConfig[key] = value;
	_writeLocalConfig();
}

void NodeConfig::putLocalConfig(const std::string &key,const std::string &value)
{
	Mutex::Lock _l(_localConfig_m);
	_localConfig[key] = value;
	_writeLocalConfig();
}

std::string NodeConfig::getLocalConfig(const std::string &key) const
{
	Mutex::Lock _l(_localConfig_m);
	Dictionary::const_iterator i(_localConfig.find(key));
	if (i == _localConfig.end())
		return std::string();
	return i->second;
}

void NodeConfig::clean()
{
	Mutex::Lock _l(_networks_m);
	for(std::map< uint64_t,SharedPtr<Network> >::const_iterator n(_networks.begin());n!=_networks.end();++n)
		n->second->clean();
}

void NodeConfig::_readLocalConfig()
{
	// assumes _localConfig_m is locked
	std::string localDotConf(RR->homePath + ZT_PATH_SEPARATOR_S + "local.conf");
	std::string buf;
	if (Utils::readFile(localDotConf.c_str(),buf))
		_localConfig.fromString(buf.c_str());
}

void NodeConfig::_writeLocalConfig()
{
	// assumes _localConfig_m is locked
	Utils::writeFile(((RR->homePath + ZT_PATH_SEPARATOR_S + "local.conf")).c_str(),_localConfig.toString());
}

} // namespace ZeroTier
