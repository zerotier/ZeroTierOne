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

#include "NodeControlService.hpp"
#include "../node/Node.hpp"
#include "../node/Utils.hpp"

namespace ZeroTier {

NodeControlService::NodeControlService(Node *node,const char *authToken) :
	_node(node),
	_listener((IpcListener *)0),
	_authToken(authToken),
	_running(true),
	_thread(Thread::start(this))
{
}

NodeControlService::~NodeControlService()
{
	_running = false;
	Thread::join(_thread);
	{
		Mutex::Lock _l(_connections_m);
		for(std::map< IpcConnection *,bool >::iterator c(_connections.begin());c!=_connections.end();++c)
			delete c->first;
		_connections.clear();
	}
	delete _listener;
}

void NodeControlService::threadMain()
	throw()
{
	char tmp[1024];
	try {
		while (_running) {
			if (!_node->running()) {
				break;
			} else if ((_node->initialized())&&(_node->address())) {
				Utils::snprintf(tmp,sizeof(tmp),"%s%.10llx",ZT_IPC_ENDPOINT_BASE,(unsigned long long)_node->address());
				_listener = new IpcListener(tmp,&_CBcommandHandler,this);
			}
		}
	} catch ( ... ) {
		delete _listener;
		_listener = (IpcListener *)0;
	}
}

void NodeControlService::_CBcommandHandler(void *arg,IpcConnection *ipcc,IpcConnection::EventType event,const char *commandLine)
{
	if (!((NodeControlService *)arg)->_running)
		return;
	if ((!commandLine)||(!commandLine[0]))
		return;
	switch(event) {
		case IpcConnection::IPC_EVENT_COMMAND: {
			((NodeControlService *)arg)->_doCommand(ipcc,commandLine);
		}	break;
		case IpcConnection::IPC_EVENT_NEW_CONNECTION: {
			Mutex::Lock _l(((NodeControlService *)arg)->_connections_m);
			((NodeControlService *)arg)->_connections[ipcc] = false; // not yet authenticated
		}	break;
		case IpcConnection::IPC_EVENT_CONNECTION_CLOSED: {
			Mutex::Lock _l(((NodeControlService *)arg)->_connections_m);
			((NodeControlService *)arg)->_connections.erase(ipcc);
			delete ipcc;
		}	break;
	}
}

void NodeControlService::_doCommand(IpcConnection *ipcc,const char *commandLine)
{
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
			ipcc->printf("200 info %.10llx %s %s"ZT_EOL_S,_node->address(),(_node->online() ? "ONLINE" : "OFFLINE"),Node::versionString());
		} else if (cmd[0] == "listpeers") {
			ipcc->printf("200 listpeers <ztaddr> <paths> <latency> <version>"ZT_EOL_S);
			ZT1_Node_PeerList *pl = _node->listPeers();
			if (pl) {
				for(unsigned int i=0;i<pl->numPeers;++i) {
					ipcc->printf("200 listpeers %.10llx ",(unsigned long long)pl->peers[i].rawAddress);
					for(unsigned int j=0;j<pl->peers[i].numPaths;++j) {
						if (j > 0)
							ipcc->printf(",");
						switch(pl->peers[i].paths[j].type) {
							default:
								ipcc->printf("unknown;");
								break;
							case ZT1_Node_PhysicalPath::ZT1_Node_PhysicalPath_TYPE_UDP:
								ipcc->printf("udp;");
								break;
							case ZT1_Node_PhysicalPath::ZT1_Node_PhysicalPath_TYPE_TCP_OUT:
								ipcc->printf("tcp_out;");
								break;
							case ZT1_Node_PhysicalPath::ZT1_Node_PhysicalPath_TYPE_TCP_IN:
								ipcc->printf("tcp_in;");
								break;
							case ZT1_Node_PhysicalPath::ZT1_Node_PhysicalPath_TYPE_ETHERNET:
								ipcc->printf("eth;");
								break;
						}
						ipcc->printf("%s/%d;%ld;%ld;%ld;%s",
							pl->peers[i].paths[j].address.ascii,
							(int)pl->peers[i].paths[j].address.port,
							pl->peers[i].paths[j].lastSend,
							pl->peers[i].paths[j].lastReceive,
							pl->peers[i].paths[j].lastPing,
							(pl->peers[i].paths[j].fixed ? "fixed" : (pl->peers[i].paths[j].active ? "active" : "inactive")));
					}
					ipcc->printf(ZT_EOL_S);
				}
				_node->freeQueryResult(pl);
			}
		} else if (cmd[0] == "listnetworks") {
			ipcc->printf("200 listnetworks <nwid> <name> <mac> <status> <config age> <type> <dev> <ips>"ZT_EOL_S);
			ZT1_Node_NetworkList *nl = _node->listNetworks();
			if (nl) {
				for(unsigned int i=0;i<nl->numNetworks;++i) {
					ipcc->printf("200 listnetworks %s %s %s %s %ld %s %s ",
						nl->networks[i].nwidHex,
						nl->networks[i].name,
						nl->networks[i].macStr,
						nl->networks[i].statusStr,
						nl->networks[i].configAge,
						(nl->networks[i].isPrivate ? "private" : "public"),
						nl->networks[i].device);
					if (nl->networks[i].numIps > 0) {
						for(unsigned int j=0;j<nl->networks[i].numIps;++j) {
							if (j > 0)
								ipcc->printf(",");
							ipcc->printf("%s/%d",nl->networks[i].ips[j].ascii,(int)nl->networks[i].ips[j].port);
						}
					} else ipcc->printf("-");
					ipcc->printf(ZT_EOL_S);
				}
				_node->freeQueryResult(nl);
			}
		} else if (cmd[0] == "join") {
			if (cmd.size() > 1) {
				uint64_t nwid = Utils::hexStrToU64(cmd[1].c_str());
				_node->join(nwid);
				ipcc->printf("200 join %.16llx OK"ZT_EOL_S,(unsigned long long)nwid);
			} else {
				ipcc->printf("400 join requires a network ID (>0) in hexadecimal format"ZT_EOL_S);
			}
		} else if (cmd[0] == "leave") {
			if (cmd.size() > 1) {
				uint64_t nwid = Utils::hexStrToU64(cmd[1].c_str());
				_node->leave(nwid);
				ipcc->printf("200 leave %.16llx OK"ZT_EOL_S,(unsigned long long)nwid);
			} else {
				ipcc->printf("400 leave requires a network ID (>0) in hexadecimal format"ZT_EOL_S);
			}
		} else if (cmd[0] == "terminate") {
			if (cmd.size() > 1)
				_node->terminate(Node::NODE_NORMAL_TERMINATION,cmd[1].c_str());
			else _node->terminate(Node::NODE_NORMAL_TERMINATION,"terminate via IPC command");
		} else if (cmd[0] == "updatecheck") {
			if (_node->updateCheck()) {
				ipcc->printf("500 software updates are not enabled"ZT_EOL_S);
			} else {
				ipcc->printf("200 OK"ZT_EOL_S);
			}
		} else {
			ipcc->printf("404 %s No such command. Use 'help' for help."ZT_EOL_S,cmd[0].c_str());
		}
	}

	ipcc->printf("."ZT_EOL_S); // blank line ends response
}

std::string NodeControlService::readOrCreateAuthtoken(const char *path,bool generateIfNotFound)
{
	unsigned char randbuf[24];
	std::string token;

	if (Utils::readFile(path,token))
		return token;
	else token = "";

	if (generateIfNotFound) {
		Utils::getSecureRandom(randbuf,sizeof(randbuf));
		for(unsigned int i=0;i<sizeof(randbuf);++i)
			token.push_back(("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")[(unsigned int)randbuf[i] % 62]);
		if (!Utils::writeFile(path,token))
			return std::string();
		Utils::lockDownFile(path,false);
	}

	return token;
}

} // namespace ZeroTier
