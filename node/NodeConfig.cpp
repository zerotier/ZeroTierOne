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

#include <openssl/sha.h>

#include "NodeConfig.hpp"
#include "RuntimeEnvironment.hpp"
#include "Defaults.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include "Topology.hpp"
#include "Demarc.hpp"
#include "InetAddress.hpp"
#include "Peer.hpp"
#include "Salsa20.hpp"
#include "HMAC.hpp"

#ifdef __WINDOWS__
#define strtoull _strtoui64
#endif

namespace ZeroTier {

NodeConfig::NodeConfig(const RuntimeEnvironment *renv,const char *authToken)
	throw(std::runtime_error) :
	_r(renv),
	_controlSocket(true,ZT_CONTROL_UDP_PORT,false,&_CBcontrolPacketHandler,this)
{
	SHA256_CTX sha;
	SHA256_Init(&sha);
	SHA256_Update(&sha,authToken,strlen(authToken));
	SHA256_Final(_controlSocketKey,&sha);

	std::map<std::string,bool> networksDotD(Utils::listDirectory((_r->homePath + ZT_PATH_SEPARATOR_S + "networks.d").c_str()));
	std::set<uint64_t> nwids;
	for(std::map<std::string,bool>::iterator d(networksDotD.begin());d!=networksDotD.end();++d) {
		if (!d->second) {
			std::string::size_type dot = d->first.rfind(".conf");
			if (dot != std::string::npos) {
				uint64_t nwid = strtoull(d->first.substr(0,dot).c_str(),(char **)0,16);
				if (nwid > 0)
					nwids.insert(nwid);
			}
		}
	}

	// TODO:
	// This might go away eventually. This causes the LAN called Earth to be
	// automatically joined if there are no other networks. This is for backward
	// compatibility with the expectations of previous alpha users.
	if (nwids.empty())
		nwids.insert(0x6c92786fee000001ULL);

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

// Macro used in execute()
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
		_P("200 listpeers %s %s %s %u %s",
			p->address().toString().c_str(),
			((v4) ? v4.toString().c_str() : "-"),
			((v6) ? v6.toString().c_str() : "-"),
			(((v4)||(v6)) ? p->latency() : 0),
			p->remoteVersion().c_str());
	}

private:
	std::vector<std::string> &r;
	uint64_t _now;
};

std::vector<std::string> NodeConfig::execute(const char *command)
{
	std::vector<std::string> r;
	std::vector<std::string> cmd(Utils::split(command,"\r\n \t","\\","'"));

	//
	// Not coincidentally, response type codes correspond with HTTP
	// status codes.
	//

	if ((cmd.empty())||(cmd[0] == "help")) {
		_P("200 help help");
		_P("200 help listpeers");
		_P("200 help listnetworks");
		_P("200 help join <network ID>");
		_P("200 help leave <network ID>");
	} else if (cmd[0] == "listpeers") {
		_P("200 listpeers <ztaddr> <ipv4> <ipv6> <latency> <version>");
		_r->topology->eachPeer(_DumpPeerStatistics(r));
	} else if (cmd[0] == "listnetworks") {
		Mutex::Lock _l(_networks_m);
		_P("200 listnetworks <nwid> <status> <type> <dev> <ips>");
		for(std::map< uint64_t,SharedPtr<Network> >::const_iterator nw(_networks.begin());nw!=_networks.end();++nw) {
			std::string tmp;
			std::set<InetAddress> ips(nw->second->tap().ips());
			for(std::set<InetAddress>::iterator i(ips.begin());i!=ips.end();++i) {
				if (tmp.length())
					tmp.push_back(',');
				tmp.append(i->toString());
			}
			// TODO: display network status, such as "permission denied to closed
			// network" or "waiting".
			_P("200 listnetworks %.16llx %s %s %s %s",
				(unsigned long long)nw->first,
				Network::statusString(nw->second->status()),
				(nw->second->isOpen() ? "open" : "private"),
				nw->second->tap().deviceName().c_str(),
				((tmp.length() > 0) ? tmp.c_str() : "-"));
		}
	} else if (cmd[0] == "join") {
		if (cmd.size() > 1) {
			uint64_t nwid = strtoull(cmd[1].c_str(),(char **)0,16);
			if (nwid > 0) {
				Mutex::Lock _l(_networks_m);
				if (_networks.count(nwid)) {
					_P("400 already a member of %.16llx",(unsigned long long)nwid);
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
			uint64_t nwid = strtoull(cmd[1].c_str(),(char **)0,16);
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
	} else {
		_P("404 %s No such command. Use 'help' for help.",cmd[0].c_str());
	}

	r.push_back(std::string()); // terminate with empty line

	return r;
}

std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> > NodeConfig::encodeControlMessage(const void *key,unsigned long conversationId,const std::vector<std::string> &payload)
	throw(std::out_of_range)
{
	char hmac[32];
	char keytmp[32];
	std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> > packets;
	Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> packet;

	packet.setSize(16); // room for HMAC and IV
	packet.append((uint32_t)(conversationId & 0xffffffff));

	for(unsigned int i=0;i<payload.size();++i) {
		packet.append(payload[i]); // will throw if too big
		packet.append((unsigned char)0);

		if (((i + 1) >= payload.size())||((packet.size() + payload[i + 1].length() + 1) >= packet.capacity())) {
			Utils::getSecureRandom(packet.field(8,8),8);

			Salsa20 s20(key,256,packet.field(8,8));
			s20.encrypt(packet.field(16,packet.size() - 16),packet.field(16,packet.size() - 16),packet.size() - 16);

			memcpy(keytmp,key,32);
			for(unsigned int i=0;i<32;++i)
				keytmp[i] ^= 0x77; // use a different permutation of key for HMAC than for Salsa20
			HMAC::sha256(keytmp,32,packet.field(16,packet.size() - 16),packet.size() - 16,hmac);
			memcpy(packet.field(0,8),hmac,8);

			packets.push_back(packet);

			packet.setSize(16); // room for HMAC and IV
			packet.append((uint32_t)(conversationId & 0xffffffff));
		}
	}

	return packets;
}

bool NodeConfig::decodeControlMessagePacket(const void *key,const void *data,unsigned int len,unsigned long &conversationId,std::vector<std::string> &payload)
{
	char hmac[32];
	char keytmp[32];

	try {
		if (len < 20)
			return false;

		Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> packet(data,len);

		memcpy(keytmp,key,32);
		for(unsigned int i=0;i<32;++i)
			keytmp[i] ^= 0x77; // use a different permutation of key for HMAC than for Salsa20
		HMAC::sha256(keytmp,32,packet.field(16,packet.size() - 16),packet.size() - 16,hmac);
		if (memcmp(packet.field(0,8),hmac,8))
			return false;

		Salsa20 s20(key,256,packet.field(8,8));
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
	const RuntimeEnvironment *_r = nc->_r;

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
