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
#include <bits/types.h>
#include <linux/posix_types.h>
#undef __FD_SETSIZE
#define __FD_SETSIZE 1048576
#undef FD_SETSIZE
#define FD_SETSIZE 1048576
#include <fstream>
#include <iostream>
#include <bits/stdc++.h>
#include <stdarg.h>
#include "../ext/nlohmann/json.hpp"  // This path is correct as tcp-proxy.cpp is in the root directory
using json = nlohmann::json;
#endif

#include "../node/Metrics.hpp"
#include "../osdep/Phy.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <memory>

// Only include cloud provider functionality when ENABLE_CLOUD_PROVIDER is defined
#ifdef ENABLE_CLOUD_PROVIDER
#include <curl/curl.h>
#include "cloud/provider/FirewallManagerFactory.hpp"
#endif

#define ZT_TCP_PROXY_CONNECTION_TIMEOUT_SECONDS 300

int ZT_TCP_PROXY_TCP_PORT;
int ZT_TCP_PROXY_EXTERNAL_TCP_PORT;

// Cloud provider variables only used when ENABLE_CLOUD_PROVIDER is defined
#ifdef ENABLE_CLOUD_PROVIDER
std::string ZT_CLOUD_PROVIDER = "linode"; // Default to linode for backward compatibility
std::string ZT_CLOUD_API_TOKEN;
std::string ZT_CLOUD_FIREWALL_ID;
bool ZT_CLOUD_FIREWALL_ENABLED = false;
std::unique_ptr<CloudFirewallManager> firewallManager;
#endif

using namespace ZeroTier;

// declaring datetime function
void datetime_fprintf(FILE *stream, const char *fmt, ...) __attribute__((format(printf,2,3)));

void datetime_fprintf(FILE *stream, const char *fmt, ...)
{
	va_list ap;
	time_t timer;
	struct tm * timeinfo;
	char buffer [100];
	time(&timer);
	timeinfo = localtime (&timer);
	strftime (buffer,100,"[%c] ",timeinfo);
	va_start(ap, fmt);
	fprintf(stream, "%s", buffer);
	vfprintf(stream, fmt, ap);
	va_end(ap);
}
#define datetime_printf(...)	datetime_fprintf(stderr, __VA_ARGS__)

// TCP_PORT namespace
namespace TCP_PORT

{
  void func()

  {
	// Path to the directory and config file
	std::string dir = "/var/lib/zerotier-one/";
	std::string filePath = dir + "local.conf";
    std::ifstream config(filePath);
	
	if (!config)
	{
		datetime_printf("- No config file present.\n");
		datetime_printf("- Switching to default port, Server listening on TCP port : 443\n");
		ZT_TCP_PROXY_TCP_PORT = 443;
	} else {
		nlohmann::json j;
		config >> j;
		
		// Get TCP port
		if (j["settings"].contains("tcpPort")) {
			std::uint64_t tcpPort = j["settings"]["tcpPort"];
			ZT_TCP_PROXY_TCP_PORT = tcpPort;
			datetime_printf("- Config file present.\n");
			datetime_printf("- Server listening on TCP port : %d\n",ZT_TCP_PROXY_TCP_PORT);
		} else {
			ZT_TCP_PROXY_TCP_PORT = 443;
			datetime_printf("- No TCP port specified in config, using default port 443\n");
		}
		
		// Get external TCP port if specified, otherwise use internal port
		if (j["settings"].contains("externalTcpPort")) {
			std::uint64_t externalTcpPort = j["settings"]["externalTcpPort"];
			ZT_TCP_PROXY_EXTERNAL_TCP_PORT = externalTcpPort;
			datetime_printf("- Using external TCP port %d for firewall rules\n", ZT_TCP_PROXY_EXTERNAL_TCP_PORT);
		} else {
			ZT_TCP_PROXY_EXTERNAL_TCP_PORT = ZT_TCP_PROXY_TCP_PORT;
			datetime_printf("- No external TCP port specified, using internal port %d for firewall rules\n", ZT_TCP_PROXY_TCP_PORT);
		}
		
		// Get Cloud Firewall API settings if present
#ifdef ENABLE_CLOUD_PROVIDER
		// For backward compatibility, check for Linode settings first
		if (j["settings"].contains("linodeApiToken") && j["settings"].contains("linodeFirewallId")) {
			ZT_CLOUD_PROVIDER = "linode";
			ZT_CLOUD_API_TOKEN = j["settings"]["linodeApiToken"];
			ZT_CLOUD_FIREWALL_ID = j["settings"]["linodeFirewallId"];
			ZT_CLOUD_FIREWALL_ENABLED = true;
			datetime_printf("- Linode Firewall integration enabled for firewall ID: %s\n", ZT_CLOUD_FIREWALL_ID.c_str());
		} 
		// Check for generic cloud provider settings
		else if (j["settings"].contains("cloudProvider") && 
			   j["settings"].contains("cloudApiToken") && 
			   j["settings"].contains("cloudFirewallId")) {
			ZT_CLOUD_PROVIDER = j["settings"]["cloudProvider"];
			ZT_CLOUD_API_TOKEN = j["settings"]["cloudApiToken"];
			ZT_CLOUD_FIREWALL_ID = j["settings"]["cloudFirewallId"];
			ZT_CLOUD_FIREWALL_ENABLED = true;
			datetime_printf("- %s Firewall integration enabled for firewall ID: %s\n", 
				ZT_CLOUD_PROVIDER.c_str(), ZT_CLOUD_FIREWALL_ID.c_str());
		} else {
			datetime_printf("- Cloud Firewall integration disabled (API token or Firewall ID not provided)\n");
		}
#else
		if (j["settings"].contains("linodeApiToken") || j["settings"].contains("cloudApiToken")) {
			datetime_printf("- Cloud Firewall integration not compiled in this build\n");
		}
#endif
	}
  }
}


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
		uint16_t udpPort; // Store the UDP port number
		time_t lastActivity;
		bool newVersion;
		bool isLocalhost; // Flag to indicate if client is localhost/127.0.0.1
	};
	std::map< PhySocket *,Client > clients;

	PhySocket *getUnusedUdp(Client *client)
	{
		for(int i=0;i<65535;++i) {
			++udpPortCounter;
			if (udpPortCounter > 0xfffe)
				udpPortCounter = 1024;
			struct sockaddr_in laddr;
			memset(&laddr,0,sizeof(struct sockaddr_in));
			laddr.sin_family = AF_INET;
			laddr.sin_port = htons((uint16_t)udpPortCounter);
			PhySocket *udp = phy->udpBind(reinterpret_cast<struct sockaddr *>(&laddr), (void*)client);
			if (udp) {
				// Store the UDP port in the client structure
				client->udpPort = (uint16_t)udpPortCounter;
				datetime_printf(">> Assigned UDP port %d to %.16llx\n", udpPortCounter, (unsigned long long)client);
				
				// Add port to Cloud Firewall if enabled
#ifdef ENABLE_CLOUD_PROVIDER
				if (ZT_CLOUD_FIREWALL_ENABLED && firewallManager) {
					if (!client->isLocalhost) {
						// Only add port to firewall if request is not from localhost
						if (firewallManager->addPort((uint16_t)udpPortCounter)) {
							datetime_printf(">> Added UDP port %d to %s Firewall\n", udpPortCounter, ZT_CLOUD_PROVIDER.empty() ? "Cloud" : ZT_CLOUD_PROVIDER.c_str());
						} else {
							datetime_printf("!! Failed to add UDP port %d to %s Firewall\n", udpPortCounter, ZT_CLOUD_PROVIDER.empty() ? "Cloud" : ZT_CLOUD_PROVIDER.c_str());
						}
					} else {
						datetime_printf(">> Skipped adding UDP port %d to %s Firewall (localhost request)\n", udpPortCounter, ZT_CLOUD_PROVIDER.empty() ? "Cloud" : ZT_CLOUD_PROVIDER.c_str());
					}
				}
#endif
				
				return udp;
			}
		}
		return (PhySocket *)0;
	}

	void phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *localAddr,const struct sockaddr *from,void *data,unsigned long len)
	{
		if (!*uptr)
			return;
		if ((from->sa_family == AF_INET)&&(len >= 16)&&(len < 2048)) {
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

			datetime_printf("<< UDP %s:%d -> %.16llx\n",inet_ntoa(reinterpret_cast<const struct sockaddr_in *>(from)->sin_addr),(int)ntohs(reinterpret_cast<const struct sockaddr_in *>(from)->sin_port),(unsigned long long)&c);
		}
	}

	void phyOnTcpConnect(PhySocket *sock,void **uptr,bool success)
	{
		// unused, we don't initiate outbound connections
	}

	void phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from)
	{
		Client &c = clients[sockN];
		c.tcpWritePtr = 0;
		c.tcpReadPtr = 0;
		c.tcp = sockN;
		c.udpPort = 0; // Initialize UDP port to 0
		c.lastActivity = time((time_t *)0);
		c.newVersion = false;
		
		// Check if client is localhost/127.0.0.1
		c.isLocalhost = false;
		if (from && from->sa_family == AF_INET) {
			const struct sockaddr_in* clientAddr = reinterpret_cast<const struct sockaddr_in*>(from);
			c.isLocalhost = (clientAddr->sin_addr.s_addr == htonl(INADDR_LOOPBACK));
			if (c.isLocalhost) {
				datetime_printf("** TCP connection from localhost/127.0.0.1 detected\n");
			}
		}
		
		*uptrN = (void *)&c;
		
		PhySocket *udp = getUnusedUdp(&c);
		if (!udp) {
			phy->close(sockN);
			clients.erase(sockN);
			datetime_printf("** TCP rejected, no more UDP ports to assign\n");
			return;
		}
		c.udp = udp;
		datetime_printf("<< TCP from %s -> %.16llx\n",inet_ntoa(reinterpret_cast<const struct sockaddr_in *>(from)->sin_addr),(unsigned long long)&c);
	}

	void phyOnTcpClose(PhySocket *sock,void **uptr)
	{
		if (!*uptr)
			return;
		
		// Make a copy of the necessary values before erasing the client
		uint16_t udpPort = 0;
		bool isLocalhost = false;
		PhySocket *udpSock = nullptr;
		
		{
			Client &c = *((Client *)*uptr);
			udpPort = c.udpPort;
			isLocalhost = c.isLocalhost;
			udpSock = c.udp;
		}
		
		// Close the UDP socket
		if (udpSock) {
			phy->close(udpSock);
		}
		
		// Remove client from map
		clients.erase(sock);
		datetime_printf("** TCP %.16llx closed\n",(unsigned long long)*uptr);
		
		// Remove port from Cloud Firewall if enabled
#ifdef ENABLE_CLOUD_PROVIDER
		if (ZT_CLOUD_FIREWALL_ENABLED && firewallManager && udpPort > 0) {
			if (!isLocalhost) {
				// Only remove port from firewall if request is not from localhost
				if (firewallManager->removePort(udpPort)) {
					datetime_printf(">> Removed UDP port %d from %s Firewall\n", udpPort, ZT_CLOUD_PROVIDER.empty() ? "Cloud" : ZT_CLOUD_PROVIDER.c_str());
				} else {
					datetime_printf("!! Failed to remove UDP port %d from %s Firewall\n", udpPort, ZT_CLOUD_PROVIDER.empty() ? "Cloud" : ZT_CLOUD_PROVIDER.c_str());
				}
			} else {
				datetime_printf(">> Skipped removing UDP port %d from %s Firewall (localhost request)\n", udpPort, ZT_CLOUD_PROVIDER.empty() ? "Cloud" : ZT_CLOUD_PROVIDER.c_str());
			}
		}
#endif
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
						datetime_printf("<< TCP %.16llx HELLO\n",(unsigned long long)*uptr);
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
							datetime_printf(">> TCP %.16llx to %s:%d\n",(unsigned long long)*uptr,inet_ntoa(dest.sin_addr),(int)ntohs(dest.sin_port));
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
		for(std::vector<PhySocket *>::iterator s(toClose.begin());s!=toClose.end();++s)
			phy->close(*s);
	}
};

int main(int argc,char **argv)
{
	TCP_PORT :: func();
	signal(SIGPIPE,SIG_IGN);
	signal(SIGHUP,SIG_IGN);
	srand(time((time_t *)0));
	
	// Initialize Cloud Firewall Manager if enabled
#ifdef ENABLE_CLOUD_PROVIDER
	if (ZT_CLOUD_FIREWALL_ENABLED) {
		try {
			const char* providerName = ZT_CLOUD_PROVIDER.empty() ? "Cloud" : ZT_CLOUD_PROVIDER.c_str();
			datetime_printf("Initializing %s Firewall Manager...\n", providerName);
			datetime_printf("- Using %s API Token (length: %zu) and Firewall ID: %s\n", 
				providerName, ZT_CLOUD_API_TOKEN.length(), ZT_CLOUD_FIREWALL_ID.c_str());
			
			firewallManager = FirewallManagerFactory::createFirewallManager(
				ZT_CLOUD_PROVIDER, ZT_CLOUD_API_TOKEN, ZT_CLOUD_FIREWALL_ID, 
				ZT_TCP_PROXY_TCP_PORT, ZT_TCP_PROXY_EXTERNAL_TCP_PORT);
			
			if (!firewallManager || !firewallManager->initialize()) {
				datetime_printf("!! Failed to initialize %s Firewall Manager\n", providerName);
				firewallManager.reset();
				ZT_CLOUD_FIREWALL_ENABLED = false;
			} else {
				datetime_printf("- %s Firewall Manager initialized successfully\n", providerName);
				datetime_printf("- Will dynamically manage UDP ports for ZeroTier connections\n");
				datetime_printf("- Will preserve existing firewall rules (TCP, ICMP, etc.)\n");
				
				// Immediately sync firewall rules to ensure TCP rule is created
				datetime_printf("- Performing initial firewall sync to ensure TCP rule is created...\n");
				if (!firewallManager->syncFirewallRules()) {
					datetime_printf("!! Failed to perform initial sync of %s Firewall rules\n", providerName);
				} else {
					datetime_printf("- Initial %s Firewall rules synchronization successful\n", providerName);
				}
			}
		} catch (const std::exception& e) {
			datetime_printf("!! Exception initializing %s Firewall Manager: %s\n", 
				ZT_CLOUD_PROVIDER.empty() ? "Cloud" : ZT_CLOUD_PROVIDER.c_str(), e.what());
			firewallManager.reset();
			ZT_CLOUD_FIREWALL_ENABLED = false;
		}
	}
#endif

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
			datetime_printf("%s: fatal error: unable to bind TCP port %d\n",argv[0],ZT_TCP_PROXY_TCP_PORT);
			return 1;
		}
	}

	time_t lastDidHousekeeping = time((time_t *)0);
	time_t lastFirewallSync = time((time_t *)0);
	for(;;) {
		phy.poll(120000);
		time_t now = time((time_t *)0);
		if ((now - lastDidHousekeeping) > 120) {
			lastDidHousekeeping = now;
			svc.doHousekeeping();
		}
		
		// Periodically sync firewall rules (every 15 minutes)
#ifdef ENABLE_CLOUD_PROVIDER
		if (ZT_CLOUD_FIREWALL_ENABLED && firewallManager && ((now - lastFirewallSync) > 900)) {
			lastFirewallSync = now;
			const char* providerName = ZT_CLOUD_PROVIDER.empty() ? "Cloud" : ZT_CLOUD_PROVIDER.c_str();
			datetime_printf("Performing periodic %s Firewall sync (every 15 minutes)...\n", providerName);
			if (!firewallManager->syncFirewallRules()) {
				datetime_printf("!! Failed to sync %s Firewall rules\n", providerName);
			} else {
				datetime_printf("- %s Firewall rules synchronized successfully\n", providerName);
			}
		}
#endif
	}

	// Clean up (this will never be reached in normal operation)
#ifdef ENABLE_CLOUD_PROVIDER
	firewallManager.reset();
#endif

	return 0;
}
