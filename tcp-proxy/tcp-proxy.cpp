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

#define ZT_TCP_PROXY_UDP_POOL_SIZE 1024
#define ZT_TCP_PROXY_UDP_POOL_START_PORT 10000
#define ZT_TCP_PROXY_CONNECTION_TIMEOUT_SECONDS 300

using namespace ZeroTier;

/*
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
 * The primary purpose of TCP sockets is to work over ports like HTTPS(443),
 * allowing users behind particularly fascist firewalls to at least reach
 * ZeroTier's supernodes. UDP is the preferred method of communication as
 * encapsulating L2 and L3 protocols over TCP is inherently inefficient
 * due to double-ACKs. So TCP is only used as a fallback.
 *
 * New clients send a HELLO message consisting of a 4-byte message (too small
 * for a ZT packet) containing:
 *   <[1] ZeroTier major version>
 *   <[1] minor version>
 *   <[2] revision>
 *
 * Clients that have send a HELLO and that have a new enough version prepend
 * each payload with the remote IP the message is destined for. This is in
 * the same format as the IP portion of ZeroTier HELLO packets.
 */

struct TcpProxyService;
struct TcpProxyService
{
	Phy<TcpProxyService *> *phy;
	PhySocket *udpPool[ZT_TCP_PROXY_UDP_POOL_SIZE];

	struct Client
	{
		char tcpReadBuf[131072];
		char tcpWriteBuf[131072];
		unsigned long tcpWritePtr;
		unsigned long tcpReadPtr;
		PhySocket *tcp;
		PhySocket *assignedUdp;
		time_t lastActivity;
		bool newVersion;
	};

	std::map< PhySocket *,Client > clients;

	struct ReverseMappingKey
	{
		uint64_t sourceZTAddress;
		PhySocket *sendingUdpSocket;
		uint32_t destIp;
		unsigned int destPort;

		ReverseMappingKey() {}
		ReverseMappingKey(uint64_t zt,PhySocket *s,uint32_t ip,unsigned int port) : sourceZTAddress(zt),sendingUdpSocket(s),destIp(ip),destPort(port) {}
		inline bool operator<(const ReverseMappingKey &k) const throw() { return (memcmp((const void *)this,(const void *)&k,sizeof(ReverseMappingKey)) < 0); }
		inline bool operator==(const ReverseMappingKey &k) const throw() { return (memcmp((const void *)this,(const void *)&k,sizeof(ReverseMappingKey)) == 0); }
	};

	std::map< ReverseMappingKey,Client * > reverseMappings;

	void phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len)
	{
		if ((from->sa_family == AF_INET)&&(len > 16)&&(len < 2048)) {
			const uint64_t destZt = (
				(((uint64_t)(((const unsigned char *)data)[8])) << 32) |
				(((uint64_t)(((const unsigned char *)data)[9])) << 24) |
				(((uint64_t)(((const unsigned char *)data)[10])) << 16) |
				(((uint64_t)(((const unsigned char *)data)[11])) << 8) |
				((uint64_t)(((const unsigned char *)data)[12])) );
			const uint32_t fromIp = ((const struct sockaddr_in *)from)->sin_addr.s_addr;
			const unsigned int fromPort = ntohs(((const struct sockaddr_in *)from)->sin_port);

			std::map< ReverseMappingKey,Client * >::iterator rm(reverseMappings.find(ReverseMappingKey(destZt,sock,fromIp,fromPort)));
			if (rm != reverseMappings.end()) {
				Client &c = *(rm->second);

				unsigned long mlen = len;
				if (c.newVersion)
					mlen += 7; // new clients get IP info

				if ((c.tcpWritePtr + 5 + mlen) <= sizeof(c.tcpWriteBuf)) {
					if (!c.tcpWritePtr)
						phy->tcpSetNotifyWritable(c.tcp,true);

					c.tcpWriteBuf[c.tcpWritePtr++] = 0x17; // look like TLS data
					c.tcpWriteBuf[c.tcpWritePtr++] = 0x03; // look like TLS 1.2
					c.tcpWriteBuf[c.tcpWritePtr++] = 0x03; // look like TLS 1.2

					c.tcpWriteBuf[c.tcpWritePtr++] = (char)((mlen >> 8) & 0xff);
					c.tcpWriteBuf[c.tcpWritePtr++] = (char)(mlen & 0xff);

					if (c.newVersion) {
						c.tcpWriteBuf[c.tcpWritePtr++] = (char)4; // IPv4
						*((uint32_t *)(c.tcpWriteBuf + c.tcpWritePtr)) = fromIp;
						c.tcpWritePtr += 4;
						c.tcpWriteBuf[c.tcpWritePtr++] = (char)((fromPort >> 8) & 0xff);
						c.tcpWriteBuf[c.tcpWritePtr++] = (char)(fromPort & 0xff);
					}

					for(unsigned long i=0;i<len;++i)
						c.tcpWriteBuf[c.tcpWritePtr++] = ((const char *)data)[i];
				}
			}
		}
	}

	void phyOnTcpConnect(PhySocket *sock,void **uptr,bool success)
	{
		// unused, we don't initiate
	}

	void phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
	{
		Client &c = clients[sockN];
		c.tcpWritePtr = 0;
		c.tcpReadPtr = 0;
		c.tcp = sockN;
		c.assignedUdp = udpPool[rand() % ZT_TCP_PROXY_UDP_POOL_SIZE];
		c.lastActivity = time((time_t *)0);
		c.newVersion = false;
		*uptrN = (void *)&c;
	}

	void phyOnTcpClose(PhySocket *sock,void **uptr)
	{
		for(std::map< ReverseMappingKey,Client * >::iterator rm(reverseMappings.begin());rm!=reverseMappings.end();) {
			if (rm->second == (Client *)*uptr)
				reverseMappings.erase(rm++);
			else ++rm;
		}
		clients.erase(sock);
	}

	void phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len)
	{
		Client &c = *((Client *)*uptr);
		c.lastActivity = time((time_t *)0);

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
							if ((payloadLen >= 28)&&(payload[13] != (char)0xff)) {
								// Learn reverse mappings -- we will route replies to these packets
								// back to their sending TCP socket. They're on a first come first
								// served basis.
								const uint64_t sourceZt = (
									(((uint64_t)(((const unsigned char *)payload)[13])) << 32) |
									(((uint64_t)(((const unsigned char *)payload)[14])) << 24) |
									(((uint64_t)(((const unsigned char *)payload)[15])) << 16) |
									(((uint64_t)(((const unsigned char *)payload)[16])) << 8) |
									((uint64_t)(((const unsigned char *)payload)[17])) );
								ReverseMappingKey k(sourceZt,c.assignedUdp,dest.sin_addr.s_addr,ntohl(dest.sin_port));
								if (reverseMappings.count(k) == 0)
									reverseMappings[k] = &c;
							}

							phy->udpSend(c.assignedUdp,(const struct sockaddr *)&dest,payload,payloadLen);
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
			long n = phy->tcpSend(sock,c.tcpWriteBuf,c.tcpWritePtr);
			if (n > 0) {
				memmove(c.tcpWriteBuf,c.tcpWriteBuf + n,c.tcpWritePtr -= (unsigned long)n);
				if (!c.tcpWritePtr)
					phy->tcpSetNotifyWritable(sock,false);
			}
		} else phy->tcpSetNotifyWritable(sock,false);
	}

	void doHousekeeping()
	{
		std::vector<PhySocket *> toClose;
		time_t now = time((time_t *)0);
		for(std::map< PhySocket *,Client >::iterator c(clients.begin());c!=clients.end();++c) {
			if ((now - c->second.lastActivity) >= ZT_TCP_PROXY_CONNECTION_TIMEOUT_SECONDS)
				toClose.push_back(c->first);
		}
		for(std::vector<PhySocket *>::iterator s(toClose.begin());s!=toClose.end();++s)
			phy->close(*s); // will call phyOnTcpClose() which does cleanup
	}
};

int main(int argc,char **argv)
{
	signal(SIGPIPE,SIG_IGN);
	signal(SIGHUP,SIG_IGN);
	srand(time((time_t *)0));

	TcpProxyService svc;
	Phy<TcpProxyService *> phy(&svc,true);
	svc.phy = &phy;

	{
		int poolSize = 0;
		for(unsigned int p=ZT_TCP_PROXY_UDP_POOL_START_PORT;((poolSize<ZT_TCP_PROXY_UDP_POOL_SIZE)&&(p<=65535));++p) {
			struct sockaddr_in laddr;
			memset(&laddr,0,sizeof(laddr));
			laddr.sin_family = AF_INET;
			laddr.sin_port = htons((uint16_t)p);
			PhySocket *s = phy.udpBind((const struct sockaddr *)&laddr);
			if (s)
				svc.udpPool[poolSize++] = s;
		}
		if (poolSize < ZT_TCP_PROXY_UDP_POOL_SIZE) {
			fprintf(stderr,"%s: fatal error: cannot bind %d UDP ports\n",argv[0],ZT_TCP_PROXY_UDP_POOL_SIZE);
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
}
