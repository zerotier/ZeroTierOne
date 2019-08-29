/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

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

#include "../ext/json/json.hpp"

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
#include <unordered_set>
#include <vector>
#include <atomic>
#include <mutex>

using namespace ZeroTier;
using json = nlohmann::json;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

struct IdentityHasher { ZT_ALWAYS_INLINE std::size_t operator()(const Identity &id) const { return (std::size_t)id.hashCode(); } };
struct AddressHasher { ZT_ALWAYS_INLINE std::size_t operator()(const Address &a) const { return (std::size_t)a.toInt(); } };
struct InetAddressHasher { ZT_ALWAYS_INLINE std::size_t operator()(const InetAddress &ip) const { return (std::size_t)ip.hashCode(); } };
struct MulticastGroupHasher { ZT_ALWAYS_INLINE std::size_t operator()(const MulticastGroup &mg) const { return (std::size_t)mg.hashCode(); } };

struct RendezvousKey
{
	RendezvousKey(const Address &aa,const Address &bb)
	{
		if (aa > bb) {
			a = aa;
			b = bb;
		} else {
			a = bb;
			b = aa;
		}
	}
	Address a,b;
	ZT_ALWAYS_INLINE bool operator==(const RendezvousKey &k) const { return ((a == k.a)&&(b == k.b)); }
	ZT_ALWAYS_INLINE bool operator!=(const RendezvousKey &k) const { return ((a != k.a)||(b != k.b)); }
	struct Hasher { ZT_ALWAYS_INLINE std::size_t operator()(const RendezvousKey &k) const { return (std::size_t)(k.a.toInt() ^ k.b.toInt()); } };
};

struct RootPeer
{
	Identity id;
	uint8_t key[32];
	InetAddress ip4,ip6;
	int64_t lastReceive;
	int64_t lastSync;

	AtomicCounter __refCount;

	ZT_ALWAYS_INLINE ~RootPeer() { Utils::burn(key,sizeof(key)); }
};

static Identity self;
static std::atomic_bool run;
static json config;

static std::unordered_map< uint64_t,std::unordered_map< MulticastGroup,std::unordered_map< Address,int64_t,AddressHasher >,MulticastGroupHasher > > multicastSubscriptions;
static std::unordered_map< Identity,SharedPtr<RootPeer>,IdentityHasher > peersByIdentity;
static std::unordered_map< Address,std::set< SharedPtr<RootPeer> >,AddressHasher > peersByVirtAddr;
static std::unordered_map< InetAddress,std::set< SharedPtr<RootPeer> >,InetAddressHasher > peersByPhysAddr;
static std::unordered_map< RendezvousKey,int64_t,RendezvousKey::Hasher > lastRendezvous;

static std::mutex multicastSubscriptions_l;
static std::mutex peersByIdentity_l;
static std::mutex peersByVirtAddr_l;
static std::mutex peersByPhysAddr_l;
static std::mutex lastRendezvous_l;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static void handlePacket(const int v4s,const int v6s,const InetAddress *const ip,Packet &pkt)
{
	char ipstr[128],ipstr2[128],astr[32],astr2[32],tmpstr[256];
	const bool fragment = pkt[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR;
	const Address source(pkt.source());
	const Address dest(pkt.destination());
	const int64_t now = OSUtils::now();

	// See if this is destined for us and isn't a fragment / fragmented. (No packets
	// understood by the root are fragments/fragmented.)
	if ((!fragment)&&(!pkt.fragmented())&&(dest == self.address())) {
		SharedPtr<RootPeer> peer;

		// If this is an un-encrypted HELLO, either learn a new peer or verify
		// that this is a peer we already know.
		if ((pkt.cipher() == ZT_PROTO_CIPHER_SUITE__POLY1305_NONE)&&(pkt.verb() == Packet::VERB_HELLO)) {
			std::lock_guard<std::mutex> pbi_l(peersByIdentity_l);
			std::lock_guard<std::mutex> pbv_l(peersByVirtAddr_l);

			Identity id;
			if (id.deserialize(pkt,ZT_PROTO_VERB_HELLO_IDX_IDENTITY)) {
				{
					auto pById = peersByIdentity.find(id);
					if (pById != peersByIdentity.end()) {
						peer = pById->second;
						//printf("%s has %s (known (1))" ZT_EOL_S,ip->toString(ipstr),source().toString(astr));
					}
				}
				if (peer) {
					if (!pkt.dearmor(peer->key)) {
						printf("%s HELLO rejected: packet authentication failed" ZT_EOL_S,ip->toString(ipstr));
						return;
					}
				} else {
					peer.set(new RootPeer);
					if (self.agree(id,peer->key)) {
						if (pkt.dearmor(peer->key)) {
							peer->id = id;
							peer->lastSync = 0;
							peersByIdentity.emplace(id,peer);
							peersByVirtAddr[id.address()].emplace(peer);
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
			auto peers = peersByVirtAddr.find(source);
			if (peers != peersByVirtAddr.end()) {
				for(auto p=peers->second.begin();p!=peers->second.end();++p) {
					if (pkt.dearmor((*p)->key)) {
						peer = (*p);
						//printf("%s has %s (known (2))" ZT_EOL_S,ip->toString(ipstr),source().toString(astr));
						break;
					}
				}
			}
		}

		// If we found the peer, update IP and/or time and handle certain key packet types that the
		// root must concern itself with.
		if (peer) {
			InetAddress *const peerIp = ip->isV4() ? &(peer->ip4) : &(peer->ip6);
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
				case Packet::VERB_HELLO:
					try {
						const uint64_t origId = pkt.packetId();
						const uint64_t ts = pkt.template at<uint64_t>(ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP);
						pkt.reset(source,self.address(),Packet::VERB_OK);
						pkt.append((uint8_t)Packet::VERB_HELLO);
						pkt.append(origId);
						pkt.append(ts);
						pkt.append((uint8_t)ZT_PROTO_VERSION);
						pkt.append((uint8_t)0);
						pkt.append((uint8_t)0);
						pkt.append((uint16_t)0);
						ip->serialize(pkt);
						pkt.armor(peer->key,true);
						sendto(ip->isV4() ? v4s : v6s,pkt.data(),pkt.size(),0,(const struct sockaddr *)ip,(socklen_t)((ip->ss_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)));
						//printf("%s <- OK(HELLO)" ZT_EOL_S,ip->toString(ipstr));
					} catch ( ... ) {
						printf("* unexpected exception handling HELLO from %s" ZT_EOL_S,ip->toString(ipstr));
					}
					break;

				case Packet::VERB_MULTICAST_LIKE:
					try {
						std::lock_guard<std::mutex> l(multicastSubscriptions_l);
						for(unsigned int ptr=ZT_PACKET_IDX_PAYLOAD;(ptr+18)<=pkt.size();ptr+=18) {
							const uint64_t nwid = pkt.template at<uint64_t>(ptr);
							const MulticastGroup mg(MAC(pkt.field(ptr + 8,6),6),pkt.template at<uint32_t>(ptr + 14));
							multicastSubscriptions[nwid][mg][peer->id.address()] = now;
							//printf("%s subscribes to %s/%.8lx on network %.16llx" ZT_EOL_S,ip->toString(ipstr),mg.mac().toString(tmpstr),(unsigned long)mg.adi(),(unsigned long long)nwid);
						}
					} catch ( ... ) {
						printf("* unexpected exception handling MULTICAST_LIKE from %s" ZT_EOL_S,ip->toString(ipstr));
					}
					break;

				case Packet::VERB_MULTICAST_GATHER:
					try {
						const uint64_t nwid = pkt.template at<uint64_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_NETWORK_ID);
						const unsigned int flags = pkt[ZT_PROTO_VERB_MULTICAST_GATHER_IDX_FLAGS];
						const MulticastGroup mg(MAC(pkt.field(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_MAC,6),6),pkt.template at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_ADI));
						unsigned int gatherLimit = pkt.template at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_GATHER_LIMIT);
						if (gatherLimit > 255)
							gatherLimit = 255;

						const uint64_t origId = pkt.packetId();
						pkt.reset(source,self.address(),Packet::VERB_OK);
						pkt.append((uint8_t)Packet::VERB_MULTICAST_GATHER);
						pkt.append(origId);
						pkt.append(nwid);
						mg.mac().appendTo(pkt);
						pkt.append((uint32_t)mg.adi());

						{
							std::lock_guard<std::mutex> l(multicastSubscriptions_l);
							auto forNet = multicastSubscriptions.find(nwid);
							if (forNet != multicastSubscriptions.end()) {
								auto forGroup = forNet->second.find(mg);
								if (forGroup != forNet->second.end()) {
									pkt.append((uint32_t)forGroup->second.size());
									pkt.append((uint16_t)std::min(std::min((unsigned int)forGroup->second.size(),(unsigned int)65535),gatherLimit));
									auto g = forGroup->second.begin();
									unsigned int l = 0;
									for(;((l<gatherLimit)&&(g!=forGroup->second.end()));++l,++g)
										g->first.appendTo(pkt);
									if (l > 0) {
										sendto(ip->isV4() ? v4s : v6s,pkt.data(),pkt.size(),0,(const struct sockaddr *)ip,(socklen_t)((ip->ss_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)));
										//printf("%s gathered %u subscribers to %s/%.8lx on network %.16llx" ZT_EOL_S,ip->toString(ipstr),l,mg.mac().toString(tmpstr),(unsigned long)mg.adi(),(unsigned long long)nwid);
									}
								}
							}
						}
					} catch ( ... ) {
						printf("* unexpected exception handling MULTICAST_GATHER from %s" ZT_EOL_S,ip->toString(ipstr));
					}
					break;

				default:
					break;
			}

			return;
		}
	}

	// If we made it here, we are forwarding this packet to someone else and also possibly
	// sending a RENDEZVOUS message.

	bool introduce = false;
	{
		RendezvousKey rk(source,dest);
		std::lock_guard<std::mutex> l(lastRendezvous_l);
		int64_t &lr = lastRendezvous[rk];
		if ((now - lr) >= 45000) {
			lr = now;
			introduce = true;
		}
	}

	std::vector< std::pair< InetAddress *,SharedPtr<RootPeer> > > toAddrs;
	{
		std::lock_guard<std::mutex> pbv_l(peersByVirtAddr_l);
		auto peers = peersByVirtAddr.find(dest);
		if (peers != peersByVirtAddr.end()) {
			for(auto p=peers->second.begin();p!=peers->second.end();++p) {
				if ((now - (*p)->lastReceive) < ZT_PEER_ACTIVITY_TIMEOUT) {
					if ((*p)->ip6) {
						toAddrs.push_back(std::pair< InetAddress *,SharedPtr<RootPeer> >(&((*p)->ip6),*p));
					} else if ((*p)->ip4) {
						toAddrs.push_back(std::pair< InetAddress *,SharedPtr<RootPeer> >(&((*p)->ip4),*p));
					}
				}
			}
		}
	}
	if (toAddrs.empty()) {
		//printf("%s not forwarding to %s: no destinations found" ZT_EOL_S,ip->toString(ipstr),dest().toString(astr));
		return;
	}

	if (introduce) {
		std::lock_guard<std::mutex> l(peersByVirtAddr_l);
		auto sources = peersByVirtAddr.find(source);
		if (sources != peersByVirtAddr.end()) {
			for(auto a=sources->second.begin();a!=sources->second.end();++a) {
				for(auto b=toAddrs.begin();b!=toAddrs.end();++b) {
					if (((*a)->ip6)&&(b->second->ip6)) {
						//printf("* introducing %s(%s) to %s(%s)" ZT_EOL_S,ip->toString(ipstr),source.toString(astr),b->second->ip6.toString(ipstr2),dest.toString(astr2));

						// Introduce source to destination (V6)
						Packet outp(source,self.address(),Packet::VERB_RENDEZVOUS);
						outp.append((uint8_t)0);
						dest.appendTo(outp);
						outp.append((uint16_t)b->second->ip6.port());
						outp.append((uint8_t)16);
						outp.append((const uint8_t *)b->second->ip6.rawIpData(),16);
						outp.armor((*a)->key,true);
						sendto(v6s,pkt.data(),pkt.size(),0,(const struct sockaddr *)&((*a)->ip6),(socklen_t)sizeof(struct sockaddr_in6));

						// Introduce destination to source (V6)
						outp.reset(dest,self.address(),Packet::VERB_RENDEZVOUS);
						outp.append((uint8_t)0);
						source.appendTo(outp);
						outp.append((uint16_t)ip->port());
						outp.append((uint8_t)16);
						outp.append((const uint8_t *)ip->rawIpData(),16);
						outp.armor(b->second->key,true);
						sendto(v6s,pkt.data(),pkt.size(),0,(const struct sockaddr *)&(b->second->ip6),(socklen_t)sizeof(struct sockaddr_in6));
					} else if (((*a)->ip4)&&(b->second->ip4)) {
						//printf("* introducing %s(%s) to %s(%s)" ZT_EOL_S,ip->toString(ipstr),source.toString(astr),b->second->ip4.toString(ipstr2),dest.toString(astr2));

						// Introduce source to destination (V4)
						Packet outp(source,self.address(),Packet::VERB_RENDEZVOUS);
						outp.append((uint8_t)0);
						dest.appendTo(outp);
						outp.append((uint16_t)b->second->ip4.port());
						outp.append((uint8_t)4);
						outp.append((const uint8_t *)b->second->ip4.rawIpData(),4);
						outp.armor((*a)->key,true);
						sendto(v4s,pkt.data(),pkt.size(),0,(const struct sockaddr *)&((*a)->ip4),(socklen_t)sizeof(struct sockaddr_in));

						// Introduce destination to source (V4)
						outp.reset(dest,self.address(),Packet::VERB_RENDEZVOUS);
						outp.append((uint8_t)0);
						source.appendTo(outp);
						outp.append((uint16_t)ip->port());
						outp.append((uint8_t)4);
						outp.append((const uint8_t *)ip->rawIpData(),4);
						outp.armor(b->second->key,true);
						sendto(v4s,pkt.data(),pkt.size(),0,(const struct sockaddr *)&(b->second->ip4),(socklen_t)sizeof(struct sockaddr_in));
					}
				}
			}
		}
	}

	if (fragment) {
		if (reinterpret_cast<Packet::Fragment *>(&pkt)->incrementHops() >= ZT_PROTO_MAX_HOPS) {
			printf("%s refused to forward to %s: max hop count exceeded" ZT_EOL_S,ip->toString(ipstr),dest.toString(astr));
			return;
		}
	} else {
		if (pkt.incrementHops() >= ZT_PROTO_MAX_HOPS) {
			printf("%s refused to forward to %s: max hop count exceeded" ZT_EOL_S,ip->toString(ipstr),dest.toString(astr));
			return;
		}
	}

	for(auto i=toAddrs.begin();i!=toAddrs.end();++i) {
		//printf("%s -> %s for %s" ZT_EOL_S,ip->toString(ipstr),i->toString(ipstr2),dest().toString(astr));
		sendto(i->first->isV4() ? v4s : v6s,pkt.data(),pkt.size(),0,(const struct sockaddr *)i->first,(socklen_t)((i->first->ss_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)));
	}
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

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

static void shutdownSigHandler(int sig) { run = false; }

int main(int argc,char **argv)
{
	signal(SIGTERM,shutdownSigHandler);
	signal(SIGINT,shutdownSigHandler);
	signal(SIGQUIT,shutdownSigHandler);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGUSR1,SIG_IGN);
	signal(SIGUSR2,SIG_IGN);
	signal(SIGCHLD,SIG_IGN);

	if (argc < 3) {
		printf("Usage: zerotier-root <identity.secret> <config path>" ZT_EOL_S);
		return 1;
	}

	{
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
	}
	{
		std::string configStr;
		if (!OSUtils::readFile(argv[2],configStr)) {
			printf("FATAL: cannot read config file at %s" ZT_EOL_S,argv[2]);
			return 1;
		}
		try {
			config = json::parse(configStr);
		} catch (std::exception &exc) {
			printf("FATAL: config file at %s invalid: %s" ZT_EOL_S,argv[2],exc.what());
			return 1;
		} catch ( ... ) {
			printf("FATAL: config file at %s invalid: unknown exception" ZT_EOL_S,argv[2]);
			return 1;
		}
		if (!config.is_object()) {
			printf("FATAL: config file at %s invalid: does not contain a JSON object" ZT_EOL_S,argv[2]);
			return 1;
		}
	}

	int port = ZT_DEFAULT_PORT;
	try {
		int port = config["port"];
		if ((port <= 0)||(port > 65535)) {
			printf("FATAL: invalid port in config file %d" ZT_EOL_S,port);
			return 1;
		}
	} catch ( ... ) {
		port = ZT_DEFAULT_PORT;
	}

	unsigned int ncores = std::thread::hardware_concurrency();
	if (ncores == 0) ncores = 1;

	run = true;

	std::vector<std::thread> threads;

	std::vector<int> sockets;
	for(unsigned int tn=0;tn<ncores;++tn) {
		struct sockaddr_in6 in6;
		memset(&in6,0,sizeof(in6));
		in6.sin6_family = AF_INET6;
		in6.sin6_port = htons((uint16_t)port);
		const int s6 = bindSocket((struct sockaddr *)&in6);
		if (s6 < 0) {
			std::cout << "ERROR: unable to bind to port " << ZT_DEFAULT_PORT << ZT_EOL_S;
			exit(1);
		}

		struct sockaddr_in in4;
		memset(&in4,0,sizeof(in4));
		in4.sin_family = AF_INET;
		in4.sin_port = htons((uint16_t)port);
		const int s4 = bindSocket((struct sockaddr *)&in4);
		if (s4 < 0) {
			std::cout << "ERROR: unable to bind to port " << ZT_DEFAULT_PORT << ZT_EOL_S;
			exit(1);
		}

		sockets.push_back(s6);
		sockets.push_back(s4);

		threads.push_back(std::thread([s4,s6]() {
			struct sockaddr_in6 in6;
			Packet pkt;
			memset(&in6,0,sizeof(in6));
			for(;;) {
				socklen_t sl = sizeof(in6);
				const int pl = (int)recvfrom(s6,pkt.unsafeData(),pkt.capacity(),0,(struct sockaddr *)&in6,&sl);
				if (pl > 0) {
					if (pl >= ZT_PROTO_MIN_FRAGMENT_LENGTH) {
						try {
							pkt.setSize((unsigned int)pl);
							handlePacket(s4,s6,reinterpret_cast<const InetAddress *>(&in6),pkt);
						} catch ( ... ) {
							char ipstr[128];
							printf("* unexpected exception handling packet from %s" ZT_EOL_S,reinterpret_cast<const InetAddress *>(&in6)->toString(ipstr));
						}
					}
				} else {
					break;
				}
			}
		}));

		threads.push_back(std::thread([s4,s6]() {
			struct sockaddr_in in4;
			Packet pkt;
			memset(&in4,0,sizeof(in4));
			for(;;) {
				socklen_t sl = sizeof(in4);
				const int pl = (int)recvfrom(s4,pkt.unsafeData(),pkt.capacity(),0,(struct sockaddr *)&in4,&sl);
				if (pl > 0) {
					if (pl >= ZT_PROTO_MIN_FRAGMENT_LENGTH) {
						try {
							pkt.setSize((unsigned int)pl);
							handlePacket(s4,s6,reinterpret_cast<const InetAddress *>(&in4),pkt);
						} catch ( ... ) {
							char ipstr[128];
							printf("* unexpected exception handling packet from %s" ZT_EOL_S,reinterpret_cast<const InetAddress *>(&in4)->toString(ipstr));
						}
					}
				} else {
					break;
				}
			}
		}));
	}

	int64_t lastCleanedMulticastSubscriptions = 0;
	int64_t lastCleanedPeers = 0;
	while (run) {
		peersByIdentity_l.lock();
		peersByPhysAddr_l.lock();
		printf("*** have %lu peers at %lu physical endpoints" ZT_EOL_S,(unsigned long)peersByIdentity.size(),(unsigned long)peersByPhysAddr.size());
		peersByPhysAddr_l.unlock();
		peersByIdentity_l.unlock();
		sleep(1);

		const int64_t now = OSUtils::now();

		if ((now - lastCleanedMulticastSubscriptions) > 120000) {
			lastCleanedMulticastSubscriptions = now;

			std::lock_guard<std::mutex> l(multicastSubscriptions_l);
			for(auto a=multicastSubscriptions.begin();a!=multicastSubscriptions.end();) {
				for(auto b=a->second.begin();b!=a->second.end();) {
					for(auto c=b->second.begin();c!=b->second.end();) {
						if ((now - c->second) > ZT_MULTICAST_LIKE_EXPIRE)
							b->second.erase(c++);
						else ++c;
					}
					if (b->second.empty())
						a->second.erase(b++);
					else ++b;
				}
				if (a->second.empty())
					multicastSubscriptions.erase(a++);
				else ++a;
			}
		}

		if ((now - lastCleanedPeers) > 120000) {
			lastCleanedPeers = now;

			std::lock_guard<std::mutex> pbi_l(peersByIdentity_l);
			for(auto p=peersByIdentity.begin();p!=peersByIdentity.end();) {
				if ((now - p->second->lastReceive) > ZT_PEER_ACTIVITY_TIMEOUT) {
					std::lock_guard<std::mutex> pbv_l(peersByVirtAddr_l);
					std::lock_guard<std::mutex> pbp_l(peersByPhysAddr_l);

					auto pbv = peersByVirtAddr.find(p->second->id.address());
					if (pbv != peersByVirtAddr.end()) {
						pbv->second.erase(p->second);
						if (pbv->second.empty())
							peersByVirtAddr.erase(pbv);
					}

					if (p->second->ip4) {
						auto pbp = peersByPhysAddr.find(p->second->ip4);
						if (pbp != peersByPhysAddr.end()) {
							pbp->second.erase(p->second);
							if (pbp->second.empty())
								peersByPhysAddr.erase(pbp);
						}
					}
					if (p->second->ip6) {
						auto pbp = peersByPhysAddr.find(p->second->ip6);
						if (pbp != peersByPhysAddr.end()) {
							pbp->second.erase(p->second);
							if (pbp->second.empty())
								peersByPhysAddr.erase(pbp);
						}
					}

					peersByIdentity.erase(p++);
				} else ++p;
			}
		}
	}

	for(auto s=sockets.begin();s!=sockets.end();++s) {
		shutdown(*s,SHUT_RDWR);
		close(*s);
	}
	for(auto t=threads.begin();t!=threads.end();++t)
		t->join();

	return 0;
}
