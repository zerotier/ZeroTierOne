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
	_authToken(authToken),
	_controlSocket(true,ZT_CONTROL_UDP_PORT,false,&_CBcontrolPacketHandler,this)
{
	SHA256_CTX sha;

	SHA256_Init(&sha);
	SHA256_Update(&sha,_authToken.data(),_authToken.length());
	SHA256_Final(_keys,&sha); // first 32 bytes of keys[]: Salsa20 key

	SHA256_Init(&sha);
	SHA256_Update(&sha,_keys,32);
	SHA256_Update(&sha,_authToken.data(),_authToken.length());
	SHA256_Final(_keys + 32,&sha); // second 32 bytes of keys[]: HMAC key
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

void NodeConfig::_CBcontrolPacketHandler(UdpSocket *sock,void *arg,const InetAddress &remoteAddr,const void *data,unsigned int len)
{
	char hmacKey[32];
	char hmac[32];
	char buf[131072];
	NodeConfig *nc = (NodeConfig *)arg;
	const RuntimeEnvironment *_r = nc->_r;

	try {
		// Minimum length
		if (len < 28)
			return;
		if (len >= sizeof(buf)) // only up to len - 28 bytes are used on receive/decrypt
			return;

		// Compare first 16 bytes of HMAC, which is after IV in packet
		memcpy(hmacKey,nc->_keys + 32,32);
		*((uint64_t *)hmacKey) ^= *((const uint64_t *)data); // include IV in HMAC
		HMAC::sha256(hmacKey,32,((const unsigned char *)data) + 28,len - 28,hmac);
		if (memcmp(hmac,((const unsigned char *)data) + 8,16))
			return;

		// Decrypt payload if we passed HMAC
		Salsa20 s20(nc->_keys,256,data); // first 64 bits of data are IV
		s20.decrypt(((const unsigned char *)data) + 28,buf,len - 28);

		// Null-terminate string for execute()
		buf[len - 28] = (char)0;

		// Execute command
		std::vector<std::string> r(nc->execute(buf));

		// Result packet contains a series of null-terminated results
		unsigned int resultLen = 28;
		for(std::vector<std::string>::iterator i(r.begin());i!=r.end();++i) {
			if ((resultLen + i->length() + 1) >= sizeof(buf))
				return; // result too long
			memcpy(buf + resultLen,i->c_str(),i->length() + 1);
			resultLen += i->length() + 1;
		}

		// Generate result packet IV
		Utils::getSecureRandom(buf,8);

		// Generate result packet HMAC
		memcpy(hmacKey,nc->_keys + 32,32);
		*((uint64_t *)hmacKey) ^= *((const uint64_t *)buf); // include IV in HMAC
		HMAC::sha256(hmacKey,32,((const unsigned char *)buf) + 28,resultLen - 28,hmac);
		memcpy(buf + 8,hmac,16);

		// Copy arbitrary tag from original packet
		memcpy(buf + 24,((const unsigned char *)data) + 24,4);

		// Send encrypted result back to requester
		sock->send(remoteAddr,buf,resultLen,-1);
	} catch ( ... ) {
		TRACE("unexpected exception parsing control packet or generating response");
	}
}

} // namespace ZeroTier
