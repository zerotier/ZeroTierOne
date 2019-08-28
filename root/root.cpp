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
#include "../node/SharedPtr.hpp"
#include "../node/MulticastGroup.hpp"
#include "../osdep/OSUtils.hpp"

#include <string>
#include <thread>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <mutex>

using namespace ZeroTier;

struct IdentityHasher { ZT_ALWAYS_INLINE std::size_t operator()(const Identity &id) const { return (std::size_t)id.hashCode(); } };
struct AddressHasher { ZT_ALWAYS_INLINE std::size_t operator()(const Address &a) const { return (std::size_t)a.toInt(); } };
struct InetAddressHasher { ZT_ALWAYS_INLINE std::size_t operator()(const InetAddress &ip) const { return (std::size_t)ip.hashCode(); } };
struct MulticastGroupHasher { ZT_ALWAYS_INLINE std::size_t operator()(const MulticastGroup &mg) const { return (std::size_t)mg.hashCode(); } };

struct PeerInfo
{
	Identity id;
	uint8_t key[32];
	InetAddress ip4,ip6;
	int64_t lastReceive;

	std::unordered_map< uint64_t,std::unordered_map< MulticastGroup,int64_t,MulticastGroupHasher > > multicastGroups;
	Mutex multicastGroups_l;

	AtomicCounter __refCount;

	ZT_ALWAYS_INLINE ~PeerInfo() { Utils::burn(key,sizeof(key)); }
};

static Identity self;
static std::atomic_bool run;

static std::vector< SharedPtr<PeerInfo> > newPeers;

static std::unordered_map< Identity,SharedPtr<PeerInfo>,IdentityHasher > peersByIdentity;
static std::unordered_map< Address,std::set< SharedPtr<PeerInfo> >,AddressHasher > peersByVirtAddr;
static std::unordered_map< InetAddress,std::set< SharedPtr<PeerInfo> >,InetAddressHasher > peersByPhysAddr;

static std::mutex newPeers_l;
static std::mutex peersByIdentity_l;
static std::mutex peersByVirtAddr_l;
static std::mutex peersByPhysAddr_l;

static void handlePacket(const int sock,const InetAddress *const ip,Packet &pkt)
{
	char ipstr[128],ipstr2[128],astr[32],tmpstr[256];
	const bool fragment = pkt[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR;

	// See if this is destined for us and isn't a fragment / fragmented. (No packets
	// understood by the root are fragments/fragmented.)
	if ((!fragment)&&(!pkt.fragmented())&&(pkt.destination() == self.address())) {
		SharedPtr<PeerInfo> peer;

		// If this is an un-encrypted HELLO, either learn a new peer or verify
		// that this is a peer we already know.
		if ((pkt.cipher() == ZT_PROTO_CIPHER_SUITE__POLY1305_NONE)&&(pkt.verb() == Packet::VERB_HELLO)) {
			Identity id;
			if (id.deserialize(pkt,ZT_PROTO_VERB_HELLO_IDX_IDENTITY)) {
				{
					std::lock_guard<std::mutex> pbi_l(peersByIdentity_l);
					auto pById = peersByIdentity.find(id);
					if (pById != peersByIdentity.end()) {
						peer = pById->second;
						//printf("%s has %s (known (1))" ZT_EOL_S,ip->toString(ipstr),pkt.source().toString(astr));
					}
				}
				if (peer) {
					if (!pkt.dearmor(peer->key)) {
						printf("%s HELLO rejected: packet authentication failed" ZT_EOL_S,ip->toString(ipstr));
						return;
					}
				} else {
					peer.set(new PeerInfo);
					if (self.agree(id,peer->key)) {
						if (pkt.dearmor(peer->key)) {
							peer->id = id;
							{
								std::lock_guard<std::mutex> np_l(newPeers_l);
								newPeers.push_back(peer);
							}
							{
								std::lock_guard<std::mutex> pbi_l(peersByIdentity_l);
								peersByIdentity.emplace(id,peer);
							}
							{
								std::lock_guard<std::mutex> pbv_l(peersByVirtAddr_l);
								peersByVirtAddr[id.address()].emplace(peer);
							}
						} else {
							printf("%s HELLO rejected: packet authentication failed" ZT_EOL_S,ip->toString(ipstr));
							return;
						}
					} else {
						printf("%s HELLO rejected: key agreement failed" ZT_EOL_S,ip->toString(ipstr));
						return;
					}
				}
			}
		}

		// If it wasn't a HELLO, check to see if any known identities for the sender's
		// short ZT address successfully decrypt the packet.
		if (!peer) {
			std::lock_guard<std::mutex> pbv_l(peersByVirtAddr_l);
			auto peers = peersByVirtAddr.find(pkt.source());
			if (peers != peersByVirtAddr.end()) {
				for(auto p=peers->second.begin();p!=peers->second.end();++p) {
					if (pkt.dearmor((*p)->key)) {
						peer = (*p);
						//printf("%s has %s (known (2))" ZT_EOL_S,ip->toString(ipstr),pkt.source().toString(astr));
						break;
					}
				}
			}
		}

		// If we found the peer, update IP and/or time.
		if (peer) {
			InetAddress *const peerIp = (ip->ss_family == AF_INET) ? &(peer->ip4) : &(peer->ip6);
			if (*peerIp != ip) {
				std::lock_guard<std::mutex> pbp_l(peersByPhysAddr_l);
				if (*peerIp) {
					auto prev = peersByPhysAddr.find(*peerIp);
					if (prev != peersByPhysAddr.end()) {
						prev->second.erase(peer);
						if (prev->second.empty())
							peersByPhysAddr.erase(prev);
					}
				}
				*peerIp = ip;
				peersByPhysAddr[ip].emplace(peer);
			}

			const int64_t now = OSUtils::now();
			peer->lastReceive = now;

			switch(pkt.verb()) {
				case Packet::VERB_HELLO: {
					if (pkt.source() == 0x89e92ceee5) {
						printf("ME!\n");
						const uint64_t origId = pkt.packetId();
						const uint64_t ts = pkt.template at<uint64_t>(ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP);
						pkt.reset(pkt.source(),self.address(),Packet::VERB_OK);
						pkt.append((uint8_t)Packet::VERB_HELLO);
						pkt.append(origId);
						pkt.append(ts);
						pkt.append((uint8_t)ZT_PROTO_VERSION);
						pkt.append((uint16_t)1);
						pkt.append((uint16_t)9);
						pkt.append((uint16_t)0);
						ip->serialize(pkt);
						pkt.armor(peer->key,true);
						sendto(sock,pkt.data(),pkt.size(),0,(const struct sockaddr *)ip,(socklen_t)((ip->ss_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)));
						printf("%s <- OK(HELLO)" ZT_EOL_S,ip->toString(ipstr));
					}
				}	break;

				case Packet::VERB_MULTICAST_LIKE: {
					printf("LIKE\n");
					Mutex::Lock l(peer->multicastGroups_l);
					for(unsigned int ptr=ZT_PACKET_IDX_PAYLOAD;(ptr+18)<=pkt.size();ptr+=18) {
						const uint64_t nwid = pkt.template at<uint64_t>(ptr);
						const MulticastGroup mg(MAC(pkt.field(ptr + 8,6),6),pkt.template at<uint32_t>(ptr + 14));
						peer->multicastGroups[nwid][mg] = now;
						printf("%s subscribes to %s/%.8lx on network %.16llx" ZT_EOL_S,ip->toString(ipstr),mg.mac().toString(tmpstr),(unsigned long)mg.adi(),(unsigned long long)nwid);
					}
				}	break;

				case Packet::VERB_MULTICAST_GATHER: {
				}	break;

				default:
					break;
			}

			return;
		}
	}

	std::vector<InetAddress> toAddrs;
	{
		std::lock_guard<std::mutex> pbv_l(peersByVirtAddr_l);
		auto peers = peersByVirtAddr.find(pkt.destination());
		if (peers != peersByVirtAddr.end()) {
			for(auto p=peers->second.begin();p!=peers->second.end();++p) {
				if ((*p)->ip6)
					toAddrs.push_back((*p)->ip6);
				else if ((*p)->ip4)
					toAddrs.push_back((*p)->ip4);
			}
		}
	}
	if (toAddrs.empty()) {
		printf("%s not forwarding to %s: no destinations found" ZT_EOL_S,ip->toString(ipstr),pkt.destination().toString(astr));
		return;
	}

	if (fragment) {
		if (reinterpret_cast<Packet::Fragment *>(&pkt)->incrementHops() >= ZT_PROTO_MAX_HOPS) {
			printf("%s refused to forward to %s: max hop count exceeded" ZT_EOL_S,ip->toString(ipstr),pkt.destination().toString(astr));
			return;
		}
	} else {
		if (pkt.incrementHops() >= ZT_PROTO_MAX_HOPS) {
			printf("%s refused to forward to %s: max hop count exceeded" ZT_EOL_S,ip->toString(ipstr),pkt.destination().toString(astr));
			return;
		}
	}

	for(auto i=toAddrs.begin();i!=toAddrs.end();++i) {
		printf("%s -> %s for %s" ZT_EOL_S,ip->toString(ipstr),i->toString(ipstr2),pkt.destination().toString(astr));
		//sendto(sock,pkt.data(),pkt.size(),0,(const struct sockaddr *)&(*i),(socklen_t)((i->ss_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)));
	}
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

void shutdownSigHandler(int sig)
{
	run = false;
}

int main(int argc,char **argv)
{
	signal(SIGTERM,shutdownSigHandler);
	signal(SIGINT,shutdownSigHandler);
	signal(SIGQUIT,shutdownSigHandler);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGUSR1,SIG_IGN);
	signal(SIGUSR2,SIG_IGN);

	if (argc < 2) {
		printf("Usage: zerotier-root <identity.secret> [<port>]" ZT_EOL_S);
		return 1;
	}

	std::string myIdStr;
	if (!OSUtils::readFile(argv[1],myIdStr)) {
		printf("FATAL: cannot read identity.secret at %s" ZT_EOL_S,argv[1]);
		return 1;
	}
	if (!self.fromString(myIdStr.c_str())) {
		printf("FATAL: cannot read identity.secret at %s (invalid identity)" ZT_EOL_S,argv[1]);
		return 1;
	}
	if (!self.hasPrivate()) {
		printf("FATAL: cannot read identity.secret at %s (missing secret key)" ZT_EOL_S,argv[1]);
		return 1;
	}

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
						handlePacket(s6,reinterpret_cast<const InetAddress *>(&in6),pkt);
					} catch ( ... ) {
						printf("* unexpected exception" ZT_EOL_S);
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
						handlePacket(s4,reinterpret_cast<const InetAddress *>(&in4),pkt);
					} catch ( ... ) {
						printf("* unexpected exception" ZT_EOL_S);
					}
				} else {
					break;
				}
			}
		}));
	}

	while (run) {
		peersByIdentity_l.lock();
		printf("* have %lu peers" ZT_EOL_S,(unsigned long)peersByIdentity.size());
		peersByIdentity_l.unlock();
		sleep(1);
	}

	for(auto s=sockets.begin();s!=sockets.end();++s) {
		shutdown(*s,SHUT_RDWR);
		close(*s);
	}
	for(auto t=threads.begin();t!=threads.end();++t)
		t->join();

	return 0;
}
