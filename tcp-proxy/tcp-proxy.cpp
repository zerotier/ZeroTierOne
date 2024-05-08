/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

// HACK! Will eventually use epoll() or something in Phy<> instead of select().
// Also be sure to change ulimit -n and fs.file-max in /etc/sysctl.conf on relays.
#if defined(__linux__) || defined(__LINUX__) || defined(__LINUX) || defined(LINUX)
#include <linux/posix_types.h>
#include <bits/types.h>
#undef __FD_SETSIZE
#define __FD_SETSIZE 1048576
#undef FD_SETSIZE
#define FD_SETSIZE 1048576
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <vector>

#include "../osdep/Phy.hpp"
#include "../osdep/OSUtils.hpp"

#include "Metrics.hpp"

#define ZT_TCP_PROXY_CONNECTION_TIMEOUT_SECONDS 300
#define ZT_TCP_PROXY_TCP_PORT 443

#define HOMEDIR "/var/lib/zt-tcp-proxy"

using namespace ZeroTier;

/*
 * ZeroTier TCP Proxy Server
 *
 * This implements a simple packet encapsulation that is designed to look like
 * a TLS connection. It's not a TLS connection, but it sends TLS format record
 * headers. It could be extended in the future to implement a fake TLS
 * handshake.
 *
 * At the moment, each packet is just made to look like TLS application data:
 *   <[1] TLS content type> - currently 0x17 for "application data"
 *   <[1] TLS major version> - currently 0x03 for TLS 1.2
 *   <[1] TLS minor version> - currently 0x03 for TLS 1.2
 *   <[2] payload length> - 16-bit length of payload in bytes
 *   <[...] payload> - Message payload
 *
 * TCP is inherently inefficient for encapsulating Ethernet, since TCP and TCP
 * like protocols over TCP lead to double-ACKs. So this transport is only used
 * to enable access when UDP or other datagram protocols are not available.
 *
 * Clients send a greeting, which is a four-byte message that contains:
 *   <[1] ZeroTier major version>
 *   <[1] minor version>
 *   <[2] revision>
 *
 * If a client has sent a greeting, it uses the new version of this protocol
 * in which every encapsulated ZT packet is prepended by an IP address where
 * it should be forwarded (or where it came from for replies). This causes
 * this proxy to act as a remote UDP socket similar to a socks proxy, which
 * will allow us to move this function off the rootservers and onto dedicated
 * proxy nodes.
 *
 * Older ZT clients that do not send this message get their packets relayed
 * to/from 127.0.0.1:9993, which will allow them to talk to and relay via
 * the ZT node on the same machine as the proxy. We'll only support this for
 * as long as such nodes appear to be in the wild.
 */

struct TcpProxyService;
struct TcpProxyService
{
	Phy<TcpProxyService *> *phy;
	int udpPortCounter;
	struct Client
	{
		char tcpReadBuf[131072];
		char tcpWriteBuf[131072];
		unsigned long tcpWritePtr;
		unsigned long tcpReadPtr;
		PhySocket *tcp;
		PhySocket *udp;
		time_t lastActivity;
		bool newVersion;
	};
	std::map< PhySocket *,Client > clients;

	PhySocket *getUnusedUdp(void *uptr)
	{
		for(int i=0;i<65535;++i) {
			++udpPortCounter;
			if (udpPortCounter > 0xfffe)
				udpPortCounter = 1024;
			struct sockaddr_in laddr;
			memset(&laddr,0,sizeof(struct sockaddr_in));
			laddr.sin_family = AF_INET;
			laddr.sin_port = htons((uint16_t)udpPortCounter);
			PhySocket *udp = phy->udpBind(reinterpret_cast<struct sockaddr *>(&laddr),uptr);
			if (udp)
				return udp;
		}
		return (PhySocket *)0;
	}

	void phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *localAddr,const struct sockaddr *from,void *data,unsigned long len)
	{
		if (!*uptr)
			return;
		if ((from->sa_family == AF_INET)&&(len >= 16)&&(len < 2048)) {
			Metrics::udp_bytes_in += len;

			Client &c = *((Client *)*uptr);
			c.lastActivity = time((time_t *)0);

			unsigned long mlen = len;
			if (c.newVersion)
				mlen += 7; // new clients get IP info

			if ((c.tcpWritePtr + 5 + mlen) <= sizeof(c.tcpWriteBuf)) {
				if (!c.tcpWritePtr)
					phy->setNotifyWritable(c.tcp,true);

				c.tcpWriteBuf[c.tcpWritePtr++] = 0x17; // look like TLS data
				c.tcpWriteBuf[c.tcpWritePtr++] = 0x03; // look like TLS 1.2
				c.tcpWriteBuf[c.tcpWritePtr++] = 0x03; // look like TLS 1.2

				c.tcpWriteBuf[c.tcpWritePtr++] = (char)((mlen >> 8) & 0xff);
				c.tcpWriteBuf[c.tcpWritePtr++] = (char)(mlen & 0xff);

				if (c.newVersion) {
					c.tcpWriteBuf[c.tcpWritePtr++] = (char)4; // IPv4
					*((uint32_t *)(c.tcpWriteBuf + c.tcpWritePtr)) = ((const struct sockaddr_in *)from)->sin_addr.s_addr;
					c.tcpWritePtr += 4;
					*((uint16_t *)(c.tcpWriteBuf + c.tcpWritePtr)) = ((const struct sockaddr_in *)from)->sin_port;
					c.tcpWritePtr += 2;
				}

				for(unsigned long i=0;i<len;++i)
					c.tcpWriteBuf[c.tcpWritePtr++] = ((const char *)data)[i];
			}

			printf("<< UDP %s:%d -> %.16llx\n",inet_ntoa(reinterpret_cast<const struct sockaddr_in *>(from)->sin_addr),(int)ntohs(reinterpret_cast<const struct sockaddr_in *>(from)->sin_port),(unsigned long long)&c);
		}
	}

	void phyOnTcpConnect(PhySocket *sock,void **uptr,bool success)
	{
		// unused, we don't initiate outbound connections
	}

	void phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
	{
		Client &c = clients[sockN];
		PhySocket *udp = getUnusedUdp((void *)&c);
		if (!udp) {
			Metrics::udp_open_failed++;
			phy->close(sockN);
			clients.erase(sockN);
			printf("** TCP rejected, no more UDP ports to assign\n");
			return;
		}
		c.tcpWritePtr = 0;
		c.tcpReadPtr = 0;
		c.tcp = sockN;
		c.udp = udp;
		c.lastActivity = time((time_t *)0);
		c.newVersion = false;
		*uptrN = (void *)&c;
		printf("<< TCP from %s -> %.16llx\n",inet_ntoa(reinterpret_cast<const struct sockaddr_in *>(from)->sin_addr),(unsigned long long)&c);
		Metrics::tcp_opened++;
		Metrics::tcp_connections++;
	}

	void phyOnTcpClose(PhySocket *sock,void **uptr)
	{
		if (!*uptr)
			return;
		Client &c = *((Client *)*uptr);
		phy->close(c.udp);
		clients.erase(sock);
		printf("** TCP %.16llx closed\n",(unsigned long long)*uptr);
		Metrics::tcp_closed++;
	}

	void phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len)
	{
		Client &c = *((Client *)*uptr);
		c.lastActivity = time((time_t *)0);

		Metrics::tcp_bytes_in += len;

		for(unsigned long i=0;i<len;++i) {
			if (c.tcpReadPtr >= sizeof(c.tcpReadBuf)) {
				phy->close(sock);
				return;
			}
			c.tcpReadBuf[c.tcpReadPtr++] = ((const char *)data)[i];

			if (c.tcpReadPtr >= 5) {
				unsigned long mlen = ( ((((unsigned long)c.tcpReadBuf[3]) & 0xff) << 8) | (((unsigned long)c.tcpReadBuf[4]) & 0xff) );
				if (c.tcpReadPtr >= (mlen + 5)) {
					if (mlen == 4) {
						// Right now just sending this means the client is 'new enough' for the IP header
						c.newVersion = true;
						printf("<< TCP %.16llx HELLO\n",(unsigned long long)*uptr);
					} else if (mlen >= 7) {
						char *payload = c.tcpReadBuf + 5;
						unsigned long payloadLen = mlen;

						struct sockaddr_in dest;
						memset(&dest,0,sizeof(dest));
						if (c.newVersion) {
							if (*payload == (char)4) {
								// New clients tell us where their packets go.
								++payload;
								dest.sin_family = AF_INET;
								dest.sin_addr.s_addr = *((uint32_t *)payload);
								payload += 4;
								dest.sin_port = *((uint16_t *)payload); // will be in network byte order already
								payload += 2;
								payloadLen -= 7;
							}
						} else {
							// For old clients we will just proxy everything to a local ZT instance. The
							// fact that this will come from 127.0.0.1 will in turn prevent that instance
							// from doing unite() with us. It'll just forward. There will not be many of
							// these.
							dest.sin_family = AF_INET;
							dest.sin_addr.s_addr = htonl(0x7f000001); // 127.0.0.1
							dest.sin_port = htons(9993);
						}

						// Note: we do not relay to privileged ports... just an abuse prevention rule.
						if ((ntohs(dest.sin_port) > 1024)&&(payloadLen >= 16)) {
							phy->udpSend(c.udp,(const struct sockaddr *)&dest,payload,payloadLen);
							printf(">> TCP %.16llx to %s:%d\n",(unsigned long long)*uptr,inet_ntoa(dest.sin_addr),(int)ntohs(dest.sin_port));
							Metrics::udp_bytes_out += payloadLen;
						}
					}

					memmove(c.tcpReadBuf,c.tcpReadBuf + (mlen + 5),c.tcpReadPtr -= (mlen + 5));
				}
			}
		}
	}

	void phyOnTcpWritable(PhySocket *sock,void **uptr)
	{
		Client &c = *((Client *)*uptr);
		if (c.tcpWritePtr) {
			long n = phy->streamSend(sock,c.tcpWriteBuf,c.tcpWritePtr);
			if (n > 0) {
				Metrics::tcp_bytes_out += n;
				memmove(c.tcpWriteBuf,c.tcpWriteBuf + n,c.tcpWritePtr -= (unsigned long)n);
				if (!c.tcpWritePtr)
					phy->setNotifyWritable(sock,false);
			}
		} else phy->setNotifyWritable(sock,false);
	}

	void doHousekeeping()
	{
		std::vector<PhySocket *> toClose;
		time_t now = time((time_t *)0);
		for(std::map< PhySocket *,Client >::iterator c(clients.begin());c!=clients.end();++c) {
			if ((now - c->second.lastActivity) >= ZT_TCP_PROXY_CONNECTION_TIMEOUT_SECONDS) {
				toClose.push_back(c->first);
				toClose.push_back(c->second.udp);
			}
		}
		for(std::vector<PhySocket *>::iterator s(toClose.begin());s!=toClose.end();++s) {
			phy->close(*s);
			Metrics::tcp_closed++;
			Metrics::tcp_connections--;
		}
	}
};

int main(int argc,char **argv)
{
	signal(SIGPIPE,SIG_IGN);
	signal(SIGHUP,SIG_IGN);
	srand(time((time_t *)0));

	if (!OSUtils::fileExists(HOMEDIR)) {
		if (!OSUtils::mkdir(HOMEDIR)) {
			fprintf(stderr,"%s: fatal error: unable to create %s\n",argv[0],HOMEDIR);
			return 1;
		}
	}

	prometheus::simpleapi::saver.set_registry(prometheus::simpleapi::registry_ptr);
	prometheus::simpleapi::saver.set_delay(std::chrono::seconds(5));
	prometheus::simpleapi::saver.set_out_file(HOMEDIR "/metrics.json");

	TcpProxyService svc;
	Phy<TcpProxyService *> phy(&svc,false,true);
	svc.phy = &phy;
	svc.udpPortCounter = 1023;

	{
		struct sockaddr_in laddr;
		memset(&laddr,0,sizeof(laddr));
		laddr.sin_family = AF_INET;
		laddr.sin_port = htons(ZT_TCP_PROXY_TCP_PORT);
		if (!phy.tcpListen((const struct sockaddr *)&laddr)) {
			fprintf(stderr,"%s: fatal error: unable to bind TCP port %d\n",argv[0],ZT_TCP_PROXY_TCP_PORT);
			return 1;
		}
	}

	time_t lastDidHousekeeping = time((time_t *)0);
	for(;;) {
		phy.poll(120000);
		time_t now = time((time_t *)0);
		if ((now - lastDidHousekeeping) > 120) {
			lastDidHousekeeping = now;
			svc.doHousekeeping();
		}
	}

	return 0;
}
