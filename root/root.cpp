/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#include "../node/Constants.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>

#include "../node/Packet.hpp"
#include "../node/Utils.hpp"
#include "../node/Address.hpp"
#include "../node/Identity.hpp"
#include "../node/InetAddress.hpp"
#include "../node/Mutex.hpp"
#include "../osdep/OSUtils.hpp"

#include <string>
#include <thread>
#include <map>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <atomic>

using namespace ZeroTier;

struct PeerInfo
{
	InetAddress address;
	int64_t lastReceive;
};

struct AddressHasher { ZT_ALWAYS_INLINE std::size_t operator()(const Address &a) const { return (std::size_t)a.toInt(); } };
struct InetAddressHasher { ZT_ALWAYS_INLINE std::size_t operator()(const InetAddress &ip) const { return (std::size_t)ip.hashCode(); } };

static std::atomic_bool run;
static std::unordered_map< Address,std::map< Identity,PeerInfo >,AddressHasher > peersByVirtAddr;
static std::unordered_map< InetAddress,std::map< Identity,PeerInfo >,InetAddressHasher > peersByPhysAddr;
static Mutex peersByVirtAddr_l;
static Mutex peersByAddress_l;

static void handlePacket(const InetAddress *const ip,const Packet *const pkt)
{
	char stmp[128];
	printf("%s\n",ip->toString(stmp));
}

static int bindSocket(struct sockaddr *bindAddr)
{
	int s = socket(bindAddr->sa_family,SOCK_DGRAM,0);
	if (s < 0) {
		close(s);
		return -1;
	}

	int f = 131072;
	setsockopt(s,SOL_SOCKET,SO_RCVBUF,(const char *)&f,sizeof(f));
	f = 131072;
	setsockopt(s,SOL_SOCKET,SO_SNDBUF,(const char *)&f,sizeof(f));

	if (bindAddr->sa_family == AF_INET6) {
		f = 1; setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&f,sizeof(f));
#ifdef IPV6_MTU_DISCOVER
		f = 0; setsockopt(s,IPPROTO_IPV6,IPV6_MTU_DISCOVER,&f,sizeof(f));
#endif
#ifdef IPV6_DONTFRAG
		f = 0; setsockopt(s,IPPROTO_IPV6,IPV6_DONTFRAG,&f,sizeof(f));
#endif
	}
	f = 1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
	f = 1; setsockopt(s,SOL_SOCKET,SO_REUSEPORT,(void *)&f,sizeof(f));
	f = 1; setsockopt(s,SOL_SOCKET,SO_BROADCAST,(void *)&f,sizeof(f));
#ifdef IP_DONTFRAG
	f = 0; setsockopt(s,IPPROTO_IP,IP_DONTFRAG,&f,sizeof(f));
#endif
#ifdef IP_MTU_DISCOVER
	f = IP_PMTUDISC_DONT; setsockopt(s,IPPROTO_IP,IP_MTU_DISCOVER,&f,sizeof(f));
#endif
#ifdef SO_NO_CHECK
	if (bindAddr->sa_family == AF_INET) {
		f = 1; setsockopt(s,SOL_SOCKET,SO_NO_CHECK,(void *)&f,sizeof(f));
	}
#endif

	if (bind(s,bindAddr,(bindAddr->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))) {
		close(s);
		return -1;
	}

	return s;
}

int main(int argc,char **argv)
{
	unsigned int ncores = std::thread::hardware_concurrency();
	if (ncores == 0) ncores = 1;

	run = true;

	std::vector<int> sockets;
	std::vector<std::thread> threads;
	for(unsigned int tn=0;tn<ncores;++tn) {
		struct sockaddr_in6 in6;
		memset(&in6,0,sizeof(in6));
		in6.sin6_family = AF_INET6;
		in6.sin6_port = htons(ZT_DEFAULT_PORT);
		const int s6 = bindSocket((struct sockaddr *)&in6);
		if (s6 < 0) {
			std::cout << "ERROR: unable to bind to port " << ZT_DEFAULT_PORT << ZT_EOL_S;
			exit(1);
		}

		struct sockaddr_in in4;
		memset(&in4,0,sizeof(in4));
		in4.sin_family = AF_INET;
		in4.sin_port = htons(ZT_DEFAULT_PORT);
		const int s4 = bindSocket((struct sockaddr *)&in4);
		if (s4 < 0) {
			std::cout << "ERROR: unable to bind to port " << ZT_DEFAULT_PORT << ZT_EOL_S;
			exit(1);
		}

		sockets.push_back(s6);
		sockets.push_back(s4);

		threads.push_back(std::thread([s6]() {
			struct sockaddr_in6 in6;
			Packet pkt;
			memset(&in6,0,sizeof(in6));
			for(;;) {
				socklen_t sl = sizeof(in6);
				const int pl = (int)recvfrom(s6,pkt.unsafeData(),pkt.capacity(),0,(struct sockaddr *)&in6,&sl);
				if (pl > 0) {
					try {
						pkt.setSize((unsigned int)pl);
						handlePacket(reinterpret_cast<const InetAddress *>(&in6),&pkt);
					} catch ( ... ) {
					}
				} else {
					break;
				}
			}
		}));

		threads.push_back(std::thread([s4]() {
			struct sockaddr_in in4;
			Packet pkt;
			memset(&in4,0,sizeof(in4));
			for(;;) {
				socklen_t sl = sizeof(in4);
				const int pl = (int)recvfrom(s4,pkt.unsafeData(),pkt.capacity(),0,(struct sockaddr *)&in4,&sl);
				if (pl > 0) {
					try {
						pkt.setSize((unsigned int)pl);
						handlePacket(reinterpret_cast<const InetAddress *>(&in4),&pkt);
					} catch ( ... ) {
					}
				} else {
					break;
				}
			}
		}));
	}

	while (run) {
		sleep(1);
	}

	return 0;
}
