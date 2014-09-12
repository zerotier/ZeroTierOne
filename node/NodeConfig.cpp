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

NodeConfig::NodeConfig(const RuntimeEnvironment *renv,const char *authToken) :
	_r(renv)
//	_ipcListener((std::string(ZT_IPC_ENDPOINT_BASE) + renv->identity.address().toString()).c_str(),&_CBcommandHandler,this),
//	_authToken(authToken)
{
	{
		Mutex::Lock _l(_localConfig_m);
		_readLocalConfig();
	}

	std::string networksFolder(_r->homePath + ZT_PATH_SEPARATOR_S + "networks.d");
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
			_networks[*n] = Network::newInstance(_r,this,*n);
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

	// Close any open IPC connections
	/*
	Mutex::Lock _l(_connections_m);
	for(std::map< IpcConnection *,bool >::iterator c(_connections.begin());c!=_connections.end();++c)
		delete c->first;
	_connections.clear();
	*/
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

/*
void NodeConfig::_CBcommandHandler(void *arg,IpcConnection *ipcc,IpcConnection::EventType event,const char *commandLine)
{
	switch(event) {
		case IpcConnection::IPC_EVENT_COMMAND:
			((NodeConfig *)arg)->_doCommand(ipcc,commandLine);
			break;
		case IpcConnection::IPC_EVENT_NEW_CONNECTION: {
			Mutex::Lock _l(((NodeConfig *)arg)->_connections_m);
			((NodeConfig *)arg)->_connections[ipcc] = false; // not yet authenticated
		}	break;
		case IpcConnection::IPC_EVENT_CONNECTION_CLOSED: {
			Mutex::Lock _l(((NodeConfig *)arg)->_connections_m);
			((NodeConfig *)arg)->_connections.erase(ipcc);
			delete ipcc;
		}	break;
	}
}

// Used with Topology::eachPeer to dump peer stats
class _DumpPeerStatistics
{
public:
	_DumpPeerStatistics(IpcConnection *i) :
		ipcc(i),
		now(Utils::now())
	{
	}
	inline void operator()(Topology &t,const SharedPtr<Peer> &p)
	{
		std::vector<Path> pp(p->paths());
		std::string pathsStr;
		for(std::vector<Path>::const_iterator ppp(pp.begin());ppp!=pp.end();++ppp) {
			if (pathsStr.length())
				pathsStr.push_back(',');
			pathsStr.append(ppp->toString());
		}
		ipcc->printf("200 listpeers %s %s %u %s"ZT_EOL_S,
			p->address().toString().c_str(),
			((pathsStr.length() > 0) ? pathsStr.c_str() : "-"),
			p->latency(),
			p->remoteVersion().c_str());
	}
	IpcConnection *ipcc;
	uint64_t now;
};

void NodeConfig::_doCommand(IpcConnection *ipcc,const char *commandLine)
{
	if ((!commandLine)||(!commandLine[0]))
		return;
	std::vector<std::string> r;
	std::vector<std::string> cmd(Utils::split(commandLine,"\r\n \t","\\","'"));

	if ((cmd.empty())||(cmd[0] == "help")) {
		ipcc->printf("200 help help"ZT_EOL_S);
		ipcc->printf("200 help auth <token>"ZT_EOL_S);
		ipcc->printf("200 help info"ZT_EOL_S);
		ipcc->printf("200 help listpeers"ZT_EOL_S);
		ipcc->printf("200 help listnetworks"ZT_EOL_S);
		ipcc->printf("200 help join <network ID>"ZT_EOL_S);
		ipcc->printf("200 help leave <network ID>"ZT_EOL_S);
		ipcc->printf("200 help terminate [<reason>]"ZT_EOL_S);
		ipcc->printf("200 help updatecheck"ZT_EOL_S);
	} else if (cmd[0] == "auth") {
		if ((cmd.size() > 1)&&(_authToken == cmd[1])) {
			Mutex::Lock _l(_connections_m);
			_connections[ipcc] = true;
			ipcc->printf("200 auth OK"ZT_EOL_S);
		} else ipcc->printf("403 auth failed"ZT_EOL_S);
	} else {
		{
			Mutex::Lock _l(_connections_m);
			if (!_connections[ipcc]) {
				ipcc->printf("403 %s unauthorized"ZT_EOL_S"."ZT_EOL_S,cmd[0].c_str());
				return;
			}
		}

		if (cmd[0] == "info") {
			// We are online if at least one supernode has spoken to us since the last time our
			// network environment changed and also less than ZT_PEER_LINK_ACTIVITY_TIMEOUT ago.
			bool isOnline = false;
			uint64_t now = Utils::now();
			uint64_t since = _r->timeOfLastResynchronize;
			std::vector< SharedPtr<Peer> > snp(_r->topology->supernodePeers());
			for(std::vector< SharedPtr<Peer> >::const_iterator sn(snp.begin());sn!=snp.end();++sn) {
				uint64_t lastRec = (*sn)->lastDirectReceive();
				if ((lastRec)&&(lastRec > since)&&((now - lastRec) < ZT_PEER_PATH_ACTIVITY_TIMEOUT)) {
					isOnline = true;
					break;
				}
			}

			ipcc->printf("200 info %s %s %s"ZT_EOL_S,_r->identity.address().toString().c_str(),(isOnline ? "ONLINE" : "OFFLINE"),Node::versionString());
		} else if (cmd[0] == "listpeers") {
			ipcc->printf("200 listpeers <ztaddr> <paths> <latency> <version>"ZT_EOL_S);
			_r->topology->eachPeer(_DumpPeerStatistics(ipcc));
		} else if (cmd[0] == "listnetworks") {
			Mutex::Lock _l(_networks_m);
			ipcc->printf("200 listnetworks <nwid> <name> <mac> <status> <config age> <type> <dev> <ips>"ZT_EOL_S);
			for(std::map< uint64_t,SharedPtr<Network> >::const_iterator nw(_networks.begin());nw!=_networks.end();++nw) {
				std::string tmp;
				std::set<InetAddress> ips(nw->second->ips());
				for(std::set<InetAddress>::iterator i(ips.begin());i!=ips.end();++i) {
					if (tmp.length())
						tmp.push_back(',');
					tmp.append(i->toString());
				}

				SharedPtr<NetworkConfig> nconf(nw->second->config2());

				long long age = (nconf) ? ((long long)Utils::now() - (long long)nconf->timestamp()) : (long long)0;
				if (age < 0)
					age = 0;
				age /= 1000;

				std::string dn(nw->second->tapDeviceName());
				ipcc->printf("200 listnetworks %.16llx %s %s %s %lld %s %s %s"ZT_EOL_S,
					(unsigned long long)nw->first,
					((nconf) ? nconf->name().c_str() : "?"),
					nw->second->mac().toString().c_str(),
					Network::statusString(nw->second->status()),
					age,
					((nconf) ? (nconf->isPublic() ? "public" : "private") : "?"),
					(dn.length() > 0) ? dn.c_str() : "?",
					((tmp.length() > 0) ? tmp.c_str() : "-"));
			}
		} else if (cmd[0] == "join") {
			if (cmd.size() > 1) {
				uint64_t nwid = Utils::hexStrToU64(cmd[1].c_str());
				if (nwid > 0) {
					Mutex::Lock _l(_networks_m);
					if (_networks.count(nwid)) {
						ipcc->printf("409 already a member of %.16llx"ZT_EOL_S,(unsigned long long)nwid);
					} else {
						try {
							SharedPtr<Network> nw(Network::newInstance(_r,this,nwid));
							_networks[nwid] = nw;
							ipcc->printf("200 join %.16llx OK"ZT_EOL_S,(unsigned long long)nwid);
						} catch (std::exception &exc) {
							ipcc->printf("500 join %.16llx ERROR: %s"ZT_EOL_S,(unsigned long long)nwid,exc.what());
						} catch ( ... ) {
							ipcc->printf("500 join %.16llx ERROR: (unknown exception)"ZT_EOL_S,(unsigned long long)nwid);
						}
					}
				} else {
					ipcc->printf("400 join requires a network ID (>0) in hexadecimal format"ZT_EOL_S);
				}
			} else {
				ipcc->printf("400 join requires a network ID (>0) in hexadecimal format"ZT_EOL_S);
			}
		} else if (cmd[0] == "leave") {
			if (cmd.size() > 1) {
				Mutex::Lock _l(_networks_m);
				uint64_t nwid = Utils::hexStrToU64(cmd[1].c_str());
				std::map< uint64_t,SharedPtr<Network> >::iterator nw(_networks.find(nwid));
				if (nw == _networks.end()) {
					ipcc->printf("404 leave %.16llx ERROR: not a member of that network"ZT_EOL_S,(unsigned long long)nwid);
				} else {
					nw->second->destroy();
					_networks.erase(nw);
				}
			} else {
				ipcc->printf("400 leave requires a network ID (>0) in hexadecimal format"ZT_EOL_S);
			}
		} else if (cmd[0] == "terminate") {
			if (cmd.size() > 1)
				_r->node->terminate(Node::NODE_NORMAL_TERMINATION,cmd[1].c_str());
			else _r->node->terminate(Node::NODE_NORMAL_TERMINATION,"terminate via IPC command");
		} else if (cmd[0] == "updatecheck") {
			if (_r->updater) {
				ipcc->printf("200 checking for software updates now at: %s"ZT_EOL_S,ZT_DEFAULTS.updateLatestNfoURL.c_str());
				_r->updater->checkNow();
			} else {
				ipcc->printf("500 software updates are not enabled"ZT_EOL_S);
			}
		} else {
			ipcc->printf("404 %s No such command. Use 'help' for help."ZT_EOL_S,cmd[0].c_str());
		}
	}

	ipcc->printf("."ZT_EOL_S); // blank line ends response
}
*/

void NodeConfig::_readLocalConfig()
{
	// assumes _localConfig_m is locked
	std::string localDotConf(_r->homePath + ZT_PATH_SEPARATOR_S + "local.conf");
	std::string buf;
	if (Utils::readFile(localDotConf.c_str(),buf))
		_localConfig.fromString(buf.c_str());
}

void NodeConfig::_writeLocalConfig()
{
	// assumes _localConfig_m is locked
	Utils::writeFile(((_r->homePath + ZT_PATH_SEPARATOR_S + "local.conf")).c_str(),_localConfig.toString());
}

} // namespace ZeroTier
