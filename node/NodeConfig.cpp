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
#include <stdint.h>

#include <memory>
#include <string>
#include <map>
#include <set>

#include "Constants.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#endif

#include "NodeConfig.hpp"
#include "RuntimeEnvironment.hpp"
#include "Defaults.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include "Topology.hpp"
#include "Demarc.hpp"
#include "Packet.hpp"
#include "InetAddress.hpp"
#include "Peer.hpp"
#include "Salsa20.hpp"
#include "Poly1305.hpp"
#include "SHA512.hpp"
#include "Node.hpp"
#include "SoftwareUpdater.hpp"

namespace ZeroTier {

NodeConfig::NodeConfig(const RuntimeEnvironment *renv,const char *authToken,unsigned int controlPort) :
	_r(renv),
	_controlSocket(true,controlPort,false,&_CBcontrolPacketHandler,this)
{
	{
		unsigned int csk[64];
		SHA512::hash(csk,authToken,(unsigned int)strlen(authToken));
		memcpy(_controlSocketKey,csk,32);
	}

	std::string networksFolder(_r->homePath + ZT_PATH_SEPARATOR_S + "networks.d");
	std::map<std::string,bool> networksDotD(Utils::listDirectory(networksFolder.c_str()));
	std::set<uint64_t> nwids;
	for(std::map<std::string,bool>::iterator d(networksDotD.begin());d!=networksDotD.end();++d) {
		if (!d->second) {
			std::string::size_type dot = d->first.rfind(".conf");
			if (dot != std::string::npos) {
				uint64_t nwid = Utils::hexStrToU64(d->first.substr(0,dot).c_str());

				// TODO: remove legacy code once out of beta
				if (nwid == 0x6c92786fee000001ULL) {
					nwid = 0xbc8f9a8ee3000001ULL;
					Utils::rm((networksFolder + ZT_PATH_SEPARATOR_S + d->first).c_str());
				}
				if (nwid == 0xbc8f9a8ee3000001ULL) {
					nwid = 0x8D93FBE886000001ULL;
					Utils::rm((networksFolder + ZT_PATH_SEPARATOR_S + d->first).c_str());
				}
				if (nwid == 0x8D93FBE886000001ULL) {
					nwid = 0x8056c2e21c000001ULL;
					Utils::rm((networksFolder + ZT_PATH_SEPARATOR_S + d->first).c_str());
				}

				if (nwid > 0)
					nwids.insert(nwid);
			}
		}
	}

	for(std::set<uint64_t>::iterator nwid(nwids.begin());nwid!=nwids.end();++nwid) {
		try {
			SharedPtr<Network> nw(Network::newInstance(_r,*nwid));
			_networks[*nwid] = nw;
		} catch (std::exception &exc) {
			LOG("unable to create network %.16llx: %s",(unsigned long long)*nwid,exc.what());
		} catch ( ... ) {
			LOG("unable to create network %.16llx: (unknown exception)",(unsigned long long)*nwid);
		}
	}
}

NodeConfig::~NodeConfig()
{
}

void NodeConfig::whackAllTaps()
{
	std::vector< SharedPtr<Network> > nwlist;
	Mutex::Lock _l(_networks_m);
	for(std::map< uint64_t,SharedPtr<Network> >::const_iterator n(_networks.begin());n!=_networks.end();++n)
		n->second->tap().whack();
}

void NodeConfig::clean()
{
	Mutex::Lock _l(_networks_m);
	for(std::map< uint64_t,SharedPtr<Network> >::const_iterator n(_networks.begin());n!=_networks.end();++n)
		n->second->clean();
}

// Macro used in execute() to push lines onto the return packet
#undef _P
#define _P(f,...) { r.push_back(std::string()); Utils::stdsprintf(r.back(),(f),##__VA_ARGS__); }

// Used with Topology::eachPeer to dump peer stats
class _DumpPeerStatistics
{
public:
	_DumpPeerStatistics(std::vector<std::string> &out) :
		r(out),
		_now(Utils::now())
	{
	}

	inline void operator()(Topology &t,const SharedPtr<Peer> &p)
	{
		InetAddress v4(p->ipv4ActivePath(_now));
		InetAddress v6(p->ipv6ActivePath(_now));
		if ((v4)||(v6)) {
			_P("200 listpeers %s %s %s %u %s",
				p->address().toString().c_str(),
				((v4) ? v4.toString().c_str() : "-"),
				((v6) ? v6.toString().c_str() : "-"),
				p->latency(),
				p->remoteVersion().c_str());
		} else {
			_P("200 listpeers %s - - - %s",
				p->address().toString().c_str(),
				p->remoteVersion().c_str());
		}
	}

private:
	std::vector<std::string> &r;
	uint64_t _now;
};

std::vector<std::string> NodeConfig::execute(const char *command)
{
	std::vector<std::string> r;
	std::vector<std::string> cmd(Utils::split(command,"\r\n \t","\\","'"));

	/* Not coincidentally, response type codes correspond with HTTP
	 * status codes. Technically a little arbitrary, but would maybe
	 * make things easier if we wanted to slap some kind of web API
	 * in front of this thing. */

	if ((cmd.empty())||(cmd[0] == "help")) {
		_P("200 help help");
		_P("200 help info");
		_P("200 help listpeers");
		_P("200 help listnetworks");
		_P("200 help join <network ID>");
		_P("200 help leave <network ID>");
		_P("200 help terminate [<reason>]");
		_P("200 help updatecheck");
	} else if (cmd[0] == "info") {
		bool isOnline = false;
		uint64_t now = Utils::now();
		std::vector< SharedPtr<Peer> > snp(_r->topology->supernodePeers());
		for(std::vector< SharedPtr<Peer> >::const_iterator sn(snp.begin());sn!=snp.end();++sn) {
			if ((*sn)->hasActiveDirectPath(now)) {
				isOnline = true;
				break;
			}
		}
		_P("200 info %s %s %s",_r->identity.address().toString().c_str(),(isOnline ? "ONLINE" : "OFFLINE"),Node::versionString());
	} else if (cmd[0] == "listpeers") {
		_P("200 listpeers <ztaddr> <ipv4> <ipv6> <latency> <version>");
		_r->topology->eachPeer(_DumpPeerStatistics(r));
	} else if (cmd[0] == "listnetworks") {
		Mutex::Lock _l(_networks_m);
		_P("200 listnetworks <nwid> <name> <status> <config age> <type> <dev> <ips>");
		for(std::map< uint64_t,SharedPtr<Network> >::const_iterator nw(_networks.begin());nw!=_networks.end();++nw) {
			std::string tmp;
			std::set<InetAddress> ips(nw->second->tap().ips());
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

			_P("200 listnetworks %.16llx %s %s %lld %s %s %s",
				(unsigned long long)nw->first,
				((nconf) ? nconf->name().c_str() : "?"),
				Network::statusString(nw->second->status()),
				age,
				((nconf) ? (nconf->isOpen() ? "public" : "private") : "?"),
				nw->second->tap().deviceName().c_str(),
				((tmp.length() > 0) ? tmp.c_str() : "-"));
		}
	} else if (cmd[0] == "join") {
		if (cmd.size() > 1) {
			uint64_t nwid = Utils::hexStrToU64(cmd[1].c_str());
			if (nwid > 0) {
				Mutex::Lock _l(_networks_m);
				if (_networks.count(nwid)) {
					_P("409 already a member of %.16llx",(unsigned long long)nwid);
				} else {
					try {
						SharedPtr<Network> nw(Network::newInstance(_r,nwid));
						_networks[nwid] = nw;
						_P("200 join %.16llx OK",(unsigned long long)nwid);
					} catch (std::exception &exc) {
						_P("500 join %.16llx ERROR: %s",(unsigned long long)nwid,exc.what());
					} catch ( ... ) {
						_P("500 join %.16llx ERROR: (unknown exception)",(unsigned long long)nwid);
					}
				}
			} else {
				_P("400 join requires a network ID (>0) in hexadecimal format");
			}
		} else {
			_P("400 join requires a network ID (>0) in hexadecimal format");
		}
	} else if (cmd[0] == "leave") {
		if (cmd.size() > 1) {
			Mutex::Lock _l(_networks_m);
			uint64_t nwid = Utils::hexStrToU64(cmd[1].c_str());
			std::map< uint64_t,SharedPtr<Network> >::iterator nw(_networks.find(nwid));
			if (nw == _networks.end()) {
				_P("404 leave %.16llx ERROR: not a member of that network",(unsigned long long)nwid);
			} else {
				nw->second->destroyOnDelete();
				_networks.erase(nw);
			}
		} else {
			_P("400 leave requires a network ID (>0) in hexadecimal format");
		}
	} else if (cmd[0] == "terminate") {
		if (cmd.size() > 1)
			_r->node->terminate(Node::NODE_NORMAL_TERMINATION,cmd[1].c_str());
		else _r->node->terminate(Node::NODE_NORMAL_TERMINATION,(const char *)0);
	} else if (cmd[0] == "updatecheck") {
		if (_r->updater) {
			_P("200 checking for software updates now at: %s",ZT_DEFAULTS.updateLatestNfoURL.c_str());
			_r->updater->checkNow();
		} else {
			_P("500 software updates are not enabled");
		}
	} else {
		_P("404 %s No such command. Use 'help' for help.",cmd[0].c_str());
	}

	r.push_back(std::string()); // terminate with empty line

	return r;
}

std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> > NodeConfig::encodeControlMessage(const void *key,unsigned long conversationId,const std::vector<std::string> &payload)
{
	char poly1305tag[ZT_POLY1305_MAC_LEN];
	char iv[8];
	char keytmp[32];
	std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> > packets;
	Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> packet;

	packet.setSize(16); // room for poly1305 auth tag and IV
	packet.append((uint32_t)(conversationId & 0xffffffff));

	for(unsigned int i=0;i<payload.size();++i) {
		packet.append(payload[i]); // will throw if too big
		packet.append((unsigned char)0);

		if (((i + 1) >= payload.size())||((packet.size() + payload[i + 1].length() + 1) >= packet.capacity())) {
			Utils::getSecureRandom(iv,8);
			memcpy(packet.field(8,8),iv,8);

			Salsa20 s20(key,256,iv,ZT_PROTO_SALSA20_ROUNDS);
			s20.encrypt(packet.field(16,packet.size() - 16),packet.field(16,packet.size() - 16),packet.size() - 16);

			memcpy(keytmp,key,32);
			for(unsigned int i=0;i<8;++i)
				keytmp[i] ^= iv[i]; // can't reuse poly1305 keys, so mangle key with IV each time
			Poly1305::compute(poly1305tag,packet.field(16,packet.size() - 16),packet.size() - 16,keytmp);
			memcpy(packet.field(0,8),poly1305tag,8);

			packets.push_back(packet);

			packet.setSize(16); // room for poly1305 auth tag and IV
			packet.append((uint32_t)(conversationId & 0xffffffff));
		}
	}

	return packets;
}

bool NodeConfig::decodeControlMessagePacket(const void *key,const void *data,unsigned int len,unsigned long &conversationId,std::vector<std::string> &payload)
{
	char poly1305tag[ZT_POLY1305_MAC_LEN];
	char keytmp[32];
	char iv[8];

	try {
		if (len < 20)
			return false;

		Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> packet(data,len);

		memcpy(keytmp,key,32);
		memcpy(iv,packet.field(8,8),8);
		for(unsigned int i=0;i<8;++i)
			keytmp[i] ^= iv[i];
		Poly1305::compute(poly1305tag,packet.field(16,packet.size() - 16),packet.size() - 16,keytmp);
		if (!Utils::secureEq(packet.field(0,8),poly1305tag,8))
			return false;

		Salsa20 s20(key,256,packet.field(8,8),ZT_PROTO_SALSA20_ROUNDS);
		s20.decrypt(packet.field(16,packet.size() - 16),packet.field(16,packet.size() - 16),packet.size() - 16);

		conversationId = packet.at<uint32_t>(16);

		const char *pl = ((const char *)packet.data()) + 20;
		unsigned int pll = packet.size() - 20;
		for(unsigned int i=0;i<pll;) {
			unsigned int eos = i;
			while ((eos < pll)&&(pl[eos]))
				++eos;
			if (eos >= i) {
				payload.push_back(std::string(pl + i,eos - i));
				i = eos + 1;
			} else break;
		}

		return true;
	} catch ( ... ) {
		return false;
	}
}

void NodeConfig::_CBcontrolPacketHandler(UdpSocket *sock,void *arg,const InetAddress &remoteAddr,const void *data,unsigned int len)
{
	NodeConfig *nc = (NodeConfig *)arg;
#ifdef ZT_TRACE
	const RuntimeEnvironment *_r = nc->_r;
#endif

	try {
		unsigned long convId = 0;
		std::vector<std::string> commands;

		if (!decodeControlMessagePacket(nc->_controlSocketKey,data,len,convId,commands)) {
			TRACE("control bus packet from %s failed decode, discarded",remoteAddr.toString().c_str());
			return;
		}
		TRACE("control bus packet from %s, contains %d commands",remoteAddr.toString().c_str(),(int)commands.size());

		for(std::vector<std::string>::iterator c(commands.begin());c!=commands.end();++c) {
			std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> > resultPackets(encodeControlMessage(nc->_controlSocketKey,convId,nc->execute(c->c_str())));
			for(std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> >::iterator p(resultPackets.begin());p!=resultPackets.end();++p)
				sock->send(remoteAddr,p->data(),p->size(),-1);
		}
	} catch (std::exception &exc) {
		TRACE("exception handling control bus packet from %s: %s",remoteAddr.toString().c_str(),exc.what());
	} catch ( ... ) {
		TRACE("exception handling control bus packet from %s: (unknown)",remoteAddr.toString().c_str());
	}
}

} // namespace ZeroTier
