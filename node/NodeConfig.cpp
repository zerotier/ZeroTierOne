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
		_P("200 listpeers %s %s %s %u",
			p->address().toString().c_str(),
			((v4) ? v4.toString().c_str() : "(none)"),
			((v6) ? v6.toString().c_str() : "(none)"),
			(((v4)||(v6)) ? p->latency() : 0));
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
		_P("200 help join <network ID> [<network invitation code>]");
		_P("200 help leave <network ID>");
	} else if (cmd[0] == "listpeers") {
		_r->topology->eachPeer(_DumpPeerStatistics(r));
	} else if (cmd[0] == "listnetworks") {
		Mutex::Lock _l(_networks_m);
		for(std::map< uint64_t,SharedPtr<Network> >::const_iterator nw(_networks.begin());nw!=_networks.end();++nw) {
			_P("200 listnetworks %llu %s %s",
				nw->first,
				nw->second->tap().deviceName().c_str(),
				(nw->second->open() ? "public" : "private"));
		}
	} else if (cmd[0] == "join") {
		_P("404 join Not implemented yet.");
	} else if (cmd[0] == "leave") {
		_P("404 leave Not implemented yet.");
	} else {
		_P("404 %s No such command. Use 'help' for help.",cmd[0].c_str());
	}

	return r;
}

std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> > NodeConfig::encodeControlMessage(const void *key,unsigned long conversationId,const std::vector<std::string> &payload)
	throw(std::out_of_range)
{
	char hmac[32];
	char keytmp[32];
	std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> > packets;
	Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> packet;

	packet.setSize(16); // HMAC and IV
	packet.append((uint32_t)(conversationId & 0xffffffff));
	for(unsigned int i=0;i<payload.size();++i) {
		packet.append(payload[i]); // will throw if too big
		packet.append((unsigned char)0);

		if (((i + 1) >= payload.size())||((packet.size() + payload[i + 1].length() + 1) >= packet.capacity())) {
			Utils::getSecureRandom(packet.field(8,8),8);

			memcpy(keytmp,key,32);
			for(unsigned int i=0;i<32;++i)
				keytmp[i] ^= 0x77; // use a different permutation of key for HMAC than for Salsa20
			HMAC::sha256(keytmp,32,packet.field(16,packet.size() - 16),packet.size() - 16,hmac);
			memcpy(packet.field(0,8),hmac,8);

			Salsa20 s20(key,256,packet.field(8,8));
			s20.encrypt(packet.field(16,packet.size() - 16),packet.field(16,packet.size() - 16),packet.size() - 16);

			packets.push_back(packet);

			packet.setSize(16); // HMAC and IV
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
		payload.clear();
		for(unsigned int i=0;i<pll;) {
			unsigned int eos = i;
			while ((eos < pll)&&(pl[eos]))
				++eos;
			if (eos > i) {
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
}

} // namespace ZeroTier
