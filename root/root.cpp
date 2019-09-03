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

/*
 * This is a high-throughput minimal root server. It implements only
 * those functions of a ZT node that a root must perform and does so
 * using highly efficient multithreaded I/O code. It's only been
 * thoroughly tested on Linux but should also run on BSDs.
 *
 * Root configuration file format (JSON):
 *
 * {
 *   "name": Name of this root for documentation/UI purposes (string)
 *   "port": UDP port (int)
 *   "httpPort": Local HTTP port for basic stats (int)
 *   "relayMaxHops": Max hops (up to 7)
 *   "planetFile": Location of planet file for pre-2.x peers (string)
 *   "statsRoot": If present, path to periodically save stats files (string)
 *   "s_siblings": [
 *     {
 *       "name": Sibling name for UI/documentation purposes (string)
 *       "id": Full public identity of subling (string)
 *       "ip": IP address of sibling (string)
 *       "port": port of subling (for ZeroTier UDP) (int)
 *     }, ...
 *   ]
 * }
 *
 * The only required field is port. If statsRoot is present then files
 * are periodically written there containing the root's current state.
 * It should be a memory filesystem like /dev/shm on Linux as these
 * files are large and rewritten frequently and do not need to be
 * persisted.
 *
 * s_siblings are other root servers that should receive packets to peers
 * that we can't find. This can occur due to e.g. network topology
 * hiccups, IP blockages, etc. s_siblings are used in the order in which
 * they appear with the first alive sibling being used.
 */

#include <Constants.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
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
#include <netinet/udp.h>

#include <json.hpp>
#include <httplib.h>

#include <Packet.hpp>
#include <Utils.hpp>
#include <Address.hpp>
#include <Identity.hpp>
#include <InetAddress.hpp>
#include <Mutex.hpp>
#include <SharedPtr.hpp>
#include <MulticastGroup.hpp>
#include <CertificateOfMembership.hpp>
#include <OSUtils.hpp>
#include <Meter.hpp>

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
#include <sstream>

using namespace ZeroTier;
using json = nlohmann::json;

#ifdef MSG_DONTWAIT
#define SENDTO_FLAGS MSG_DONTWAIT
#else
#define SENDTO_FLAGS 0
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

// Hashers for std::unordered_map
struct IdentityHasher { ZT_ALWAYS_INLINE std::size_t operator()(const Identity &id) const { return (std::size_t)id.hashCode(); } };
struct AddressHasher { ZT_ALWAYS_INLINE std::size_t operator()(const Address &a) const { return (std::size_t)a.toInt(); } };
struct InetAddressHasher { ZT_ALWAYS_INLINE std::size_t operator()(const InetAddress &ip) const { return (std::size_t)ip.hashCode(); } };
struct MulticastGroupHasher { ZT_ALWAYS_INLINE std::size_t operator()(const MulticastGroup &mg) const { return (std::size_t)mg.hashCode(); } };

// An ordered tuple key representing an introduction of one peer to another
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

/**
 * RootPeer is a normal peer known to this root
 *
 * This can also be a sibling root, which is itself a peer. Sibling roots
 * are sent HELLO while for other peers we only listen for HELLO.
 */
struct RootPeer
{
	ZT_ALWAYS_INLINE RootPeer() : lastSend(0),lastReceive(0),lastSync(0),lastEcho(0),lastHello(0),vProto(-1),vMajor(-1),vMinor(-1),vRev(-1),sibling(false) {}
	ZT_ALWAYS_INLINE ~RootPeer() { Utils::burn(key,sizeof(key)); }

	Identity id;            // Identity
	uint8_t key[32];        // Shared secret key
	InetAddress ip4,ip6;    // IPv4 and IPv6 addresses
	int64_t lastSend;       // Time of last send (any packet)
	int64_t lastReceive;    // Time of last receive (any packet)
	int64_t lastSync;       // Time of last data synchronization with LF or other root state backend (currently unused)
	int64_t lastEcho;       // Time of last received ECHO
	int64_t lastHello;      // Time of last received HELLO
	int vProto;             // Protocol version
	int vMajor,vMinor,vRev; // Peer version or -1,-1,-1 if unknown
	bool sibling;           // If true, this is a sibling root that will get forwards we don't know where to send
	std::mutex lock;

	AtomicCounter __refCount;
};

static int64_t s_startTime;       // Time service was started
static std::vector<int> s_ports;  // Ports to bind for UDP traffic
static int s_relayMaxHops = 0;    // Max relay hops
static Identity s_self;           // My identity (including secret)
static std::atomic_bool s_run;    // Remains true until shutdown is ordered
static json s_config;             // JSON config file contents
static std::string s_statsRoot;   // Root to write stats, peers, etc.

static Meter s_inputRate;
static Meter s_outputRate;
static Meter s_forwardRate;
static Meter s_siblingForwardRate;
static Meter s_discardedForwardRate;

static std::string s_planet;
static std::vector< SharedPtr<RootPeer> > s_siblings;
static std::unordered_map< uint64_t,std::unordered_map< MulticastGroup,std::unordered_map< Address,int64_t,AddressHasher >,MulticastGroupHasher > > s_multicastSubscriptions;
static std::unordered_map< Identity,SharedPtr<RootPeer>,IdentityHasher > s_peersByIdentity;
static std::unordered_map< Address,std::set< SharedPtr<RootPeer> >,AddressHasher > s_peersByVirtAddr;
static std::unordered_map< InetAddress,std::set< SharedPtr<RootPeer> >,InetAddressHasher > s_peersByPhysAddr;
static std::unordered_map< RendezvousKey,int64_t,RendezvousKey::Hasher > s_lastSentRendezvous;

static std::mutex s_planet_l;
static std::mutex s_siblings_l;
static std::mutex s_multicastSubscriptions_l;
static std::mutex s_peersByIdentity_l;
static std::mutex s_peersByVirtAddr_l;
static std::mutex s_peersByPhysAddr_l;
static std::mutex s_lastSentRendezvous_l;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static void handlePacket(const int v4s,const int v6s,const InetAddress *const ip,Packet &pkt)
{
	char ipstr[128],ipstr2[128],astr[32],astr2[32],tmpstr[256];
	const bool fragment = pkt[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR;
	const Address source(pkt.source());
	const Address dest(pkt.destination());
	const int64_t now = OSUtils::now();

	s_inputRate.log(now,pkt.size());

	if ((!fragment)&&(!pkt.fragmented())&&(dest == s_self.address())) {
		SharedPtr<RootPeer> peer;

		// If this is an un-encrypted HELLO, either learn a new peer or verify
		// that this is a peer we already know.
		if ((pkt.cipher() == ZT_PROTO_CIPHER_SUITE__POLY1305_NONE)&&(pkt.verb() == Packet::VERB_HELLO)) {
			std::lock_guard<std::mutex> pbi_l(s_peersByIdentity_l);
			std::lock_guard<std::mutex> pbv_l(s_peersByVirtAddr_l);

			Identity id;
			if (id.deserialize(pkt,ZT_PROTO_VERB_HELLO_IDX_IDENTITY)) {
				{
					auto pById = s_peersByIdentity.find(id);
					if (pById != s_peersByIdentity.end()) {
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
					if (s_self.agree(id,peer->key)) {
						if (pkt.dearmor(peer->key)) {
							if (!pkt.uncompress()) {
								printf("%s HELLO rejected: decompression failed" ZT_EOL_S,ip->toString(ipstr));
								return;
							}
							peer->id = id;
							peer->lastReceive = now;
							s_peersByIdentity.emplace(id,peer);
							s_peersByVirtAddr[id.address()].emplace(peer);
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
			std::lock_guard<std::mutex> pbv_l(s_peersByVirtAddr_l);
			auto peers = s_peersByVirtAddr.find(source);
			if (peers != s_peersByVirtAddr.end()) {
				for(auto p=peers->second.begin();p!=peers->second.end();++p) {
					if (pkt.dearmor((*p)->key)) {
						if (!pkt.uncompress()) {
							printf("%s packet rejected: decompression failed" ZT_EOL_S,ip->toString(ipstr));
							return;
						}
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
			std::lock_guard<std::mutex> pl(peer->lock);

			InetAddress *const peerIp = ip->isV4() ? &(peer->ip4) : &(peer->ip6);
			if (*peerIp != ip) {
				std::lock_guard<std::mutex> pbp_l(s_peersByPhysAddr_l);
				if (*peerIp) {
					auto prev = s_peersByPhysAddr.find(*peerIp);
					if (prev != s_peersByPhysAddr.end()) {
						prev->second.erase(peer);
						if (prev->second.empty())
							s_peersByPhysAddr.erase(prev);
					}
				}
				*peerIp = ip;
				s_peersByPhysAddr[ip].emplace(peer);
			}

			const int64_t now = OSUtils::now();
			peer->lastReceive = now;

			switch(pkt.verb()) {
				case Packet::VERB_HELLO:
					try {
						if ((now - peer->lastHello) > 1000) {
							peer->lastHello = now;
							peer->vProto = (int)pkt[ZT_PROTO_VERB_HELLO_IDX_PROTOCOL_VERSION];
							peer->vMajor = (int)pkt[ZT_PROTO_VERB_HELLO_IDX_MAJOR_VERSION];
							peer->vMinor = (int)pkt[ZT_PROTO_VERB_HELLO_IDX_MINOR_VERSION];
							peer->vRev = (int)pkt.template at<uint16_t>(ZT_PROTO_VERB_HELLO_IDX_REVISION);
							const uint64_t origId = pkt.packetId();
							const uint64_t ts = pkt.template at<uint64_t>(ZT_PROTO_VERB_HELLO_IDX_TIMESTAMP);

							pkt.reset(source,s_self.address(),Packet::VERB_OK);
							pkt.append((uint8_t)Packet::VERB_HELLO);
							pkt.append(origId);
							pkt.append(ts);
							pkt.append((uint8_t)ZT_PROTO_VERSION);
							pkt.append((uint8_t)0);
							pkt.append((uint8_t)0);
							pkt.append((uint16_t)0);
							ip->serialize(pkt);
							if (peer->vProto < 11) { // send planet file for pre-2.x peers
								std::lock_guard<std::mutex> pl(s_planet_l);
								if (s_planet.length() > 0) {
									pkt.append((uint16_t)s_planet.size());
									pkt.append((const uint8_t *)s_planet.data(),s_planet.size());
								}
							}
							pkt.armor(peer->key,true);
							sendto(ip->isV4() ? v4s : v6s,pkt.data(),pkt.size(),SENDTO_FLAGS,(const struct sockaddr *)ip,(socklen_t)((ip->ss_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)));

							s_outputRate.log(now,pkt.size());
							peer->lastSend = now;
						}
					} catch ( ... ) {
						printf("* unexpected exception handling HELLO from %s" ZT_EOL_S,ip->toString(ipstr));
					}
					break;

				case Packet::VERB_ECHO:
					try {
						if ((now - peer->lastEcho) > 1000) {
							peer->lastEcho = now;

							Packet outp(source,s_self.address(),Packet::VERB_OK);
							outp.append((uint8_t)Packet::VERB_ECHO);
							outp.append(pkt.packetId());
							outp.append(((const uint8_t *)pkt.data()) + ZT_PACKET_IDX_PAYLOAD,pkt.size() - ZT_PACKET_IDX_PAYLOAD);
							outp.compress();
							outp.armor(peer->key,true);
							sendto(ip->isV4() ? v4s : v6s,outp.data(),outp.size(),SENDTO_FLAGS,(const struct sockaddr *)ip,(socklen_t)((ip->ss_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)));

							s_outputRate.log(now,outp.size());
							peer->lastSend = now;
						}
					} catch ( ... ) {
						printf("* unexpected exception handling ECHO from %s" ZT_EOL_S,ip->toString(ipstr));
					}

				case Packet::VERB_WHOIS:
					try {
						std::vector< SharedPtr<RootPeer> > results;
						{
							std::lock_guard<std::mutex> l(s_peersByVirtAddr_l);
							for(unsigned int ptr=ZT_PACKET_IDX_PAYLOAD;(ptr+ZT_ADDRESS_LENGTH)<=pkt.size();ptr+=ZT_ADDRESS_LENGTH) {
								auto peers = s_peersByVirtAddr.find(Address(pkt.field(ptr,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH));
								if (peers != s_peersByVirtAddr.end()) {
									for(auto p=peers->second.begin();p!=peers->second.end();++p)
										results.push_back(*p);
								}
							}
						}

						if (!results.empty()) {
							const uint64_t origId = pkt.packetId();
							pkt.reset(source,s_self.address(),Packet::VERB_OK);
							pkt.append((uint8_t)Packet::VERB_WHOIS);
							pkt.append(origId);
							for(auto p=results.begin();p!=results.end();++p)
								(*p)->id.serialize(pkt,false);
							pkt.armor(peer->key,true);
							sendto(ip->isV4() ? v4s : v6s,pkt.data(),pkt.size(),SENDTO_FLAGS,(const struct sockaddr *)ip,(socklen_t)((ip->ss_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)));

							s_outputRate.log(now,pkt.size());
							peer->lastSend = now;
						}
					} catch ( ... ) {
						printf("* unexpected exception handling ECHO from %s" ZT_EOL_S,ip->toString(ipstr));
					}

				case Packet::VERB_MULTICAST_LIKE:
					try {
						std::lock_guard<std::mutex> l(s_multicastSubscriptions_l);
						for(unsigned int ptr=ZT_PACKET_IDX_PAYLOAD;(ptr+18)<=pkt.size();ptr+=18) {
							const uint64_t nwid = pkt.template at<uint64_t>(ptr);
							const MulticastGroup mg(MAC(pkt.field(ptr + 8,6),6),pkt.template at<uint32_t>(ptr + 14));
							s_multicastSubscriptions[nwid][mg][source] = now;
							//printf("%s %s subscribes to %s/%.8lx on network %.16llx" ZT_EOL_S,ip->toString(ipstr),source.toString(astr),mg.mac().toString(tmpstr),(unsigned long)mg.adi(),(unsigned long long)nwid);
						}
					} catch ( ... ) {
						printf("* unexpected exception handling MULTICAST_LIKE from %s" ZT_EOL_S,ip->toString(ipstr));
					}
					break;

				case Packet::VERB_MULTICAST_GATHER:
					try {
						const uint64_t nwid = pkt.template at<uint64_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_NETWORK_ID);
						//const unsigned int flags = pkt[ZT_PROTO_VERB_MULTICAST_GATHER_IDX_FLAGS];
						const MulticastGroup mg(MAC(pkt.field(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_MAC,6),6),pkt.template at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_ADI));
						unsigned int gatherLimit = pkt.template at<uint32_t>(ZT_PROTO_VERB_MULTICAST_GATHER_IDX_GATHER_LIMIT);
						if (gatherLimit > 255)
							gatherLimit = 255;

						const uint64_t origId = pkt.packetId();
						pkt.reset(source,s_self.address(),Packet::VERB_OK);
						pkt.append((uint8_t)Packet::VERB_MULTICAST_GATHER);
						pkt.append(origId);
						pkt.append(nwid);
						mg.mac().appendTo(pkt);
						pkt.append((uint32_t)mg.adi());

						{
							std::lock_guard<std::mutex> l(s_multicastSubscriptions_l);
							auto forNet = s_multicastSubscriptions.find(nwid);
							if (forNet != s_multicastSubscriptions.end()) {
								auto forGroup = forNet->second.find(mg);
								if (forGroup != forNet->second.end()) {
									pkt.append((uint32_t)forGroup->second.size());
									const unsigned int countAt = pkt.size();
									pkt.addSize(2);

									unsigned int l = 0;
									for(auto g=forGroup->second.begin();((l<gatherLimit)&&(g!=forGroup->second.end()));++g) {
										if (g->first != source) {
											++l;
											g->first.appendTo(pkt);
										}
									}

									if (l > 0) {
										pkt.setAt<uint16_t>(countAt,(uint16_t)l);
										pkt.armor(peer->key,true);
										sendto(ip->isV4() ? v4s : v6s,pkt.data(),pkt.size(),SENDTO_FLAGS,(const struct sockaddr *)ip,(socklen_t)(ip->isV4() ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)));

										s_outputRate.log(now,pkt.size());
										peer->lastSend = now;
										//printf("%s %s gathered %u subscribers to %s/%.8lx on network %.16llx" ZT_EOL_S,ip->toString(ipstr),source.toString(astr),l,mg.mac().toString(tmpstr),(unsigned long)mg.adi(),(unsigned long long)nwid);
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

	int hops = 0;
	bool introduce = false;
	if (fragment) {
		if ((hops = (int)reinterpret_cast<Packet::Fragment *>(&pkt)->incrementHops()) > s_relayMaxHops) {
			//printf("%s refused to forward to %s: max hop count exceeded" ZT_EOL_S,ip->toString(ipstr),dest.toString(astr));
			s_discardedForwardRate.log(now,pkt.size());
			return;
		}
	} else {
		if ((hops = (int)pkt.incrementHops()) > s_relayMaxHops) {
			//printf("%s refused to forward to %s: max hop count exceeded" ZT_EOL_S,ip->toString(ipstr),dest.toString(astr));
			s_discardedForwardRate.log(now,pkt.size());
			return;
		}

		if (hops == 1) {
			RendezvousKey rk(source,dest);
			std::lock_guard<std::mutex> l(s_lastSentRendezvous_l);
			int64_t &lr = s_lastSentRendezvous[rk];
			if ((now - lr) >= 45000) {
				lr = now;
				introduce = true;
			}
		}
	}

	std::vector< std::pair< InetAddress *,SharedPtr<RootPeer> > > toAddrs;
	{
		std::lock_guard<std::mutex> pbv_l(s_peersByVirtAddr_l);
		auto peers = s_peersByVirtAddr.find(dest);
		if (peers != s_peersByVirtAddr.end()) {
			for(auto p=peers->second.begin();p!=peers->second.end();++p) {
				if ((*p)->ip4) {
					toAddrs.push_back(std::pair< InetAddress *,SharedPtr<RootPeer> >(&((*p)->ip4),*p));
				} else if ((*p)->ip6) {
					toAddrs.push_back(std::pair< InetAddress *,SharedPtr<RootPeer> >(&((*p)->ip6),*p));
				}
			}
		}
	}
	if (toAddrs.empty()) {
		std::lock_guard<std::mutex> sib_l(s_siblings_l);
		for(auto s=s_siblings.begin();s!=s_siblings.end();++s) {
			if (((now - (*s)->lastReceive) < (ZT_PEER_PING_PERIOD * 2))&&((*s)->sibling)) {
				if ((*s)->ip4) {
					toAddrs.push_back(std::pair< InetAddress *,SharedPtr<RootPeer> >(&((*s)->ip4),*s));
				} else if ((*s)->ip6) {
					toAddrs.push_back(std::pair< InetAddress *,SharedPtr<RootPeer> >(&((*s)->ip6),*s));
				}
			}
		}
	}
	if (toAddrs.empty()) {
		s_discardedForwardRate.log(now,pkt.size());
		return;
	}

	if (introduce) {
		std::lock_guard<std::mutex> l(s_peersByVirtAddr_l);
		auto sources = s_peersByVirtAddr.find(source);
		if (sources != s_peersByVirtAddr.end()) {
			for(auto a=sources->second.begin();a!=sources->second.end();++a) {
				for(auto b=toAddrs.begin();b!=toAddrs.end();++b) {
					if (((*a)->ip6)&&(b->second->ip6)) {
						//printf("* introducing %s(%s) to %s(%s)" ZT_EOL_S,ip->toString(ipstr),source.toString(astr),b->second->ip6.toString(ipstr2),dest.toString(astr2));

						// Introduce source to destination (V6)
						Packet outp(source,s_self.address(),Packet::VERB_RENDEZVOUS);
						outp.append((uint8_t)0);
						dest.appendTo(outp);
						outp.append((uint16_t)b->second->ip6.port());
						outp.append((uint8_t)16);
						outp.append((const uint8_t *)b->second->ip6.rawIpData(),16);
						outp.armor((*a)->key,true);
						sendto(v6s,outp.data(),outp.size(),SENDTO_FLAGS,(const struct sockaddr *)&((*a)->ip6),(socklen_t)sizeof(struct sockaddr_in6));

						s_outputRate.log(now,outp.size());
						(*a)->lastSend = now;

						// Introduce destination to source (V6)
						outp.reset(dest,s_self.address(),Packet::VERB_RENDEZVOUS);
						outp.append((uint8_t)0);
						source.appendTo(outp);
						outp.append((uint16_t)ip->port());
						outp.append((uint8_t)16);
						outp.append((const uint8_t *)ip->rawIpData(),16);
						outp.armor(b->second->key,true);
						sendto(v6s,outp.data(),outp.size(),SENDTO_FLAGS,(const struct sockaddr *)&(b->second->ip6),(socklen_t)sizeof(struct sockaddr_in6));

						s_outputRate.log(now,outp.size());
						b->second->lastSend = now;
					}
					if (((*a)->ip4)&&(b->second->ip4)) {
						//printf("* introducing %s(%s) to %s(%s)" ZT_EOL_S,ip->toString(ipstr),source.toString(astr),b->second->ip4.toString(ipstr2),dest.toString(astr2));

						// Introduce source to destination (V4)
						Packet outp(source,s_self.address(),Packet::VERB_RENDEZVOUS);
						outp.append((uint8_t)0);
						dest.appendTo(outp);
						outp.append((uint16_t)b->second->ip4.port());
						outp.append((uint8_t)4);
						outp.append((const uint8_t *)b->second->ip4.rawIpData(),4);
						outp.armor((*a)->key,true);
						sendto(v4s,outp.data(),outp.size(),SENDTO_FLAGS,(const struct sockaddr *)&((*a)->ip4),(socklen_t)sizeof(struct sockaddr_in));

						s_outputRate.log(now,outp.size());
						(*a)->lastSend = now;

						// Introduce destination to source (V4)
						outp.reset(dest,s_self.address(),Packet::VERB_RENDEZVOUS);
						outp.append((uint8_t)0);
						source.appendTo(outp);
						outp.append((uint16_t)ip->port());
						outp.append((uint8_t)4);
						outp.append((const uint8_t *)ip->rawIpData(),4);
						outp.armor(b->second->key,true);
						sendto(v4s,outp.data(),outp.size(),SENDTO_FLAGS,(const struct sockaddr *)&(b->second->ip4),(socklen_t)sizeof(struct sockaddr_in));

						s_outputRate.log(now,outp.size());
						b->second->lastSend = now;
					}
				}
			}
		}
	}

	for(auto i=toAddrs.begin();i!=toAddrs.end();++i) {
		//printf("%s -> %s for %s -> %s (%u bytes)" ZT_EOL_S,ip->toString(ipstr),i->first->toString(ipstr2),source.toString(astr),dest.toString(astr2),pkt.size());
		if (sendto(i->first->isV4() ? v4s : v6s,pkt.data(),pkt.size(),SENDTO_FLAGS,(const struct sockaddr *)i->first,(socklen_t)(i->first->isV4() ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))) > 0) {
			s_outputRate.log(now,pkt.size());
			s_forwardRate.log(now,pkt.size());
			if (i->second->sibling)
				s_siblingForwardRate.log(now,pkt.size());
			i->second->lastSend = now;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static int bindSocket(struct sockaddr *const bindAddr)
{
	const int s = socket(bindAddr->sa_family,SOCK_DGRAM,0);
	if (s < 0) {
		close(s);
		return -1;
	}

	int f = 1048576;
	while (f > 16384) {
		if (setsockopt(s,SOL_SOCKET,SO_RCVBUF,(const char *)&f,sizeof(f)) == 0)
			break;
		f -= 16384;
	}
	f = 1048576;
	while (f > 16384) {
		if (setsockopt(s,SOL_SOCKET,SO_SNDBUF,(const char *)&f,sizeof(f)) == 0)
			break;
		f -= 16384;
	}

	if (bindAddr->sa_family == AF_INET6) {
		f = 1; setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&f,sizeof(f));
#ifdef IPV6_MTU_DISCOVER
		f = 0; setsockopt(s,IPPROTO_IPV6,IPV6_MTU_DISCOVER,&f,sizeof(f));
#endif
#ifdef IPV6_DONTFRAG
		f = 0; setsockopt(s,IPPROTO_IPV6,IPV6_DONTFRAG,&f,sizeof(f));
#endif
	}
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

#if defined(SO_REUSEPORT)
	f = 1; setsockopt(s,SOL_SOCKET,SO_REUSEPORT,(void *)&f,sizeof(f));
#endif
#ifndef __LINUX__ // linux wants just SO_REUSEPORT
	f = 1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
#endif

	if (bind(s,bindAddr,(bindAddr->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))) {
		close(s);
		//printf("%s\n",strerror(errno));
		return -1;
	}

	return s;
}

static void shutdownSigHandler(int sig) { s_run = false; }

int main(int argc,char **argv)
{
	signal(SIGTERM,shutdownSigHandler);
	signal(SIGINT,shutdownSigHandler);
	signal(SIGQUIT,shutdownSigHandler);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGUSR1,SIG_IGN);
	signal(SIGUSR2,SIG_IGN);
	signal(SIGCHLD,SIG_IGN);

	s_startTime = OSUtils::now();

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
		if (!s_self.fromString(myIdStr.c_str())) {
			printf("FATAL: cannot read identity.secret at %s (invalid identity)" ZT_EOL_S,argv[1]);
			return 1;
		}
		if (!s_self.hasPrivate()) {
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
			s_config = json::parse(configStr);
		} catch (std::exception &exc) {
			printf("FATAL: config file at %s invalid: %s" ZT_EOL_S,argv[2],exc.what());
			return 1;
		} catch ( ... ) {
			printf("FATAL: config file at %s invalid: unknown exception" ZT_EOL_S,argv[2]);
			return 1;
		}
		if (!s_config.is_object()) {
			printf("FATAL: config file at %s invalid: does not contain a JSON object" ZT_EOL_S,argv[2]);
			return 1;
		}
	}

	try {
		auto jport = s_config["port"];
		if (jport.is_array()) {
			for(long i=0;i<(long)jport.size();++i) {
				int port = jport[i];
				if ((port <= 0)||(port > 65535)) {
					printf("FATAL: invalid port in config file %d" ZT_EOL_S,port);
					return 1;
				}
				s_ports.push_back(port);
			}
		} else {
			int port = jport;
			if ((port <= 0)||(port > 65535)) {
				printf("FATAL: invalid port in config file %d" ZT_EOL_S,port);
				return 1;
			}
			s_ports.push_back(port);
		}
	} catch ( ... ) {}
	if (s_ports.empty())
		s_ports.push_back(ZT_DEFAULT_PORT);
	std::sort(s_ports.begin(),s_ports.end());

	int httpPort = ZT_DEFAULT_PORT;
	try {
		httpPort = s_config["httpPort"];
		if ((httpPort <= 0)||(httpPort > 65535)) {
			printf("FATAL: invalid HTTP port in config file %d" ZT_EOL_S,httpPort);
			return 1;
		}
	} catch ( ... ) {
		httpPort = ZT_DEFAULT_PORT;
	}

	std::string planetFilePath;
	try {
		planetFilePath = s_config["planetFile"];
	} catch ( ... ) {
		planetFilePath = "";
	}

	try {
		s_statsRoot = s_config["statsRoot"];
		while ((s_statsRoot.length() > 0)&&(s_statsRoot[s_statsRoot.length()-1] == ZT_PATH_SEPARATOR))
			s_statsRoot = s_statsRoot.substr(0,s_statsRoot.length()-1);
		if (s_statsRoot.length() > 0)
			OSUtils::mkdir(s_statsRoot);
	} catch ( ... ) {
		s_statsRoot = "";
	}

	s_relayMaxHops = ZT_RELAY_MAX_HOPS;
	try {
		s_relayMaxHops = s_config["s_relayMaxHops"];
		if (s_relayMaxHops > ZT_PROTO_MAX_HOPS)
			s_relayMaxHops = ZT_PROTO_MAX_HOPS;
		else if (s_relayMaxHops < 0)
			s_relayMaxHops = 0;
	} catch ( ... ) {
		s_relayMaxHops = ZT_RELAY_MAX_HOPS;
	}

	try {
		auto sibs = s_config["s_siblings"];
		if (sibs.is_array()) {
			for(long i=0;i<(long)sibs.size();++i) {
				auto sib = sibs[i];
				if (sib.is_object()) {
					std::string idStr = sib["id"];
					std::string ipStr = sib["ip"];
					Identity id;
					if (!id.fromString(idStr.c_str())) {
						printf("FATAL: invalid JSON while parsing s_siblings section in config file: invalid identity in sibling entry" ZT_EOL_S);
						return 1;
					}
					InetAddress ip;
					if (!ip.fromString(ipStr.c_str())) {
						printf("FATAL: invalid JSON while parsing s_siblings section in config file: invalid IP address in sibling entry" ZT_EOL_S);
						return 1;
					}
					ip.setPort((unsigned int)sib["port"]);
					SharedPtr<RootPeer> rp(new RootPeer);
					rp->id = id;
					if (!s_self.agree(id,rp->key)) {
						printf("FATAL: invalid JSON while parsing s_siblings section in config file: invalid identity in sibling entry (unable to execute key agreement)" ZT_EOL_S);
						return 1;
					}
					if (ip.isV4()) {
						rp->ip4 = ip;
					} else if (ip.isV6()) {
						rp->ip6 = ip;
					} else {
						printf("FATAL: invalid JSON while parsing s_siblings section in config file: invalid IP address in sibling entry" ZT_EOL_S);
						return 1;
					}
					rp->sibling = true;
					s_siblings.push_back(rp);
					s_peersByIdentity[id] = rp;
					s_peersByVirtAddr[id.address()].insert(rp);
					s_peersByPhysAddr[ip].insert(rp);
				} else {
					printf("FATAL: invalid JSON while parsing s_siblings section in config file: sibling entry is not a JSON object" ZT_EOL_S);
					return 1;
				}
			}
		} else {
			printf("FATAL: invalid JSON while parsing s_siblings section in config file: s_siblings is not a JSON array" ZT_EOL_S);
			return 1;
		}
	} catch ( ... ) {
		printf("FATAL: invalid JSON while parsing s_siblings section in config file: parse error" ZT_EOL_S);
		return 1;
	}

	unsigned int ncores = std::thread::hardware_concurrency();
	if (ncores == 0) ncores = 1;

	s_run = true;

	std::vector<std::thread> threads;
	std::vector<int> sockets;
	int v4Sock = -1,v6Sock = -1;

	for(auto port=s_ports.begin();port!=s_ports.end();++port) {
		for(unsigned int tn=0;tn<ncores;++tn) {
			struct sockaddr_in6 in6;
			memset(&in6,0,sizeof(in6));
			in6.sin6_family = AF_INET6;
			in6.sin6_port = htons((uint16_t)*port);
			const int s6 = bindSocket((struct sockaddr *)&in6);
			if (s6 < 0) {
				std::cout << "ERROR: unable to bind to port " << *port << ZT_EOL_S;
				exit(1);
			}

			struct sockaddr_in in4;
			memset(&in4,0,sizeof(in4));
			in4.sin_family = AF_INET;
			in4.sin_port = htons((uint16_t)*port);
			const int s4 = bindSocket((struct sockaddr *)&in4);
			if (s4 < 0) {
				std::cout << "ERROR: unable to bind to port " << *port << ZT_EOL_S;
				exit(1);
			}

			sockets.push_back(s6);
			sockets.push_back(s4);
			if (v4Sock < 0) v4Sock = s4;
			if (v6Sock < 0) v6Sock = s6;

			threads.push_back(std::thread([s6,s4]() {
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

			threads.push_back(std::thread([s6,s4]() {
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
	}

	// Minimal local API for use with monitoring clients, etc.
	httplib::Server apiServ;
	threads.push_back(std::thread([&apiServ,httpPort]() {
		apiServ.Get("/",[](const httplib::Request &req,httplib::Response &res) {
			std::ostringstream o;
			std::lock_guard<std::mutex> l0(s_peersByIdentity_l);
			std::lock_guard<std::mutex> l1(s_peersByPhysAddr_l);
			o << "ZeroTier Root Server " << ZEROTIER_ONE_VERSION_MAJOR << '.' << ZEROTIER_ONE_VERSION_MINOR << '.' << ZEROTIER_ONE_VERSION_REVISION << ZT_EOL_S;
			o << "(c)2019 ZeroTier, Inc." ZT_EOL_S "Licensed under the ZeroTier BSL 1.1" ZT_EOL_S ZT_EOL_S;
			o << "Peers Online:       " << s_peersByIdentity.size() << ZT_EOL_S;
			o << "Physical Addresses: " << s_peersByPhysAddr.size() << ZT_EOL_S;
			res.set_content(o.str(),"text/plain");
		});
		apiServ.Get("/peer",[](const httplib::Request &req,httplib::Response &res) {
			char tmp[256];
			std::ostringstream o;
			o << '[';
			{
				bool first = true;
				std::lock_guard<std::mutex> l(s_peersByIdentity_l);
				for(auto p=s_peersByIdentity.begin();p!=s_peersByIdentity.end();++p) {
					if (first)
						first = false;
					else o << ',';
					o <<
					"{\"address\":\"" << p->first.address().toString(tmp) << "\""
					",\"latency\":-1"
					",\"paths\":[";
					if (p->second->ip4) {
						o <<
						"{\"active\":true"
						",\"address\":\"" << p->second->ip4.toIpString(tmp) << "\\/" << p->second->ip4.port() << "\""
						",\"expired\":false"
						",\"lastReceive\":" << p->second->lastReceive <<
						",\"lastSend\":" << p->second->lastSend <<
						",\"preferred\":true"
						",\"trustedPathId\":0}";
					}
					if (p->second->ip6) {
						if (p->second->ip4)
							o << ',';
						o <<
						"{\"active\":true"
						",\"address\":\"" << p->second->ip6.toIpString(tmp) << "\\/" << p->second->ip6.port() << "\""
						",\"expired\":false"
						",\"lastReceive\":" << p->second->lastReceive <<
						",\"lastSend\":" << p->second->lastSend <<
						",\"preferred\":" << ((p->second->ip4) ? "false" : "true") <<
						",\"trustedPathId\":0}";
					}
					o << "]"
					",\"role\":\"LEAF\""
					",\"version\":\"" << p->second->vMajor << '.' << p->second->vMinor << '.' << p->second->vRev << "\""
					",\"versionMajor\":" << p->second->vMajor <<
					",\"versionMinor\":" << p->second->vMinor <<
					",\"versionRev\":" << p->second->vRev << "}";
				}
			}
			o << ']';
			res.set_content(o.str(),"application/json");
		});
		apiServ.listen("127.0.0.1",httpPort,0);
	}));

	// In the main thread periodically clean stuff up
	int64_t lastCleaned = 0;
	int64_t lastWroteStats = 0;
	int64_t lastPingeds_siblings = 0;
	while (s_run) {
		//s_peersByIdentity_l.lock();
		//s_peersByPhysAddr_l.lock();
		//printf("*** have %lu peers at %lu physical endpoints" ZT_EOL_S,(unsigned long)s_peersByIdentity.size(),(unsigned long)s_peersByPhysAddr.size());
		//s_peersByPhysAddr_l.unlock();
		//s_peersByIdentity_l.unlock();
		sleep(1);

		const int64_t now = OSUtils::now();

		// Send HELLO to sibling roots
		if ((now - lastPingeds_siblings) >= ZT_PEER_PING_PERIOD) {
			lastPingeds_siblings = now;
			std::lock_guard<std::mutex> l(s_siblings_l);
			for(auto s=s_siblings.begin();s!=s_siblings.end();++s) {
				const InetAddress *ip = nullptr;
				socklen_t sl = 0;
				Packet outp((*s)->id.address(),s_self.address(),Packet::VERB_HELLO);
				outp.append((uint8_t)ZT_PROTO_VERSION);
				outp.append((uint8_t)0);
				outp.append((uint8_t)0);
				outp.append((uint16_t)0);
				outp.append((uint64_t)now);
				s_self.serialize(outp,false);
				if ((*s)->ip4) {
					(*s)->ip4.serialize(outp);
					ip = &((*s)->ip4);
					sl = sizeof(struct sockaddr_in);
				} else if ((*s)->ip6) {
					(*s)->ip6.serialize(outp);
					ip = &((*s)->ip6);
					sl = sizeof(struct sockaddr_in6);
				}
				if (ip) {
					outp.armor((*s)->key,false);
					sendto(ip->isV4() ? v4Sock : v6Sock,outp.data(),outp.size(),SENDTO_FLAGS,(const struct sockaddr *)ip,sl);
				}
			}
		}

		if ((now - lastCleaned) > 120000) {
			lastCleaned = now;

			// Old multicast subscription cleanup
			{
				std::lock_guard<std::mutex> l(s_multicastSubscriptions_l);
				for(auto a=s_multicastSubscriptions.begin();a!=s_multicastSubscriptions.end();) {
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
						s_multicastSubscriptions.erase(a++);
					else ++a;
				}
			}

			// Remove expired peers
			{
				std::lock_guard<std::mutex> pbi_l(s_peersByIdentity_l);
				for(auto p=s_peersByIdentity.begin();p!=s_peersByIdentity.end();) {
					if (((now - p->second->lastReceive) > ZT_PEER_ACTIVITY_TIMEOUT)&&(!p->second->sibling)) {
						std::lock_guard<std::mutex> pbv_l(s_peersByVirtAddr_l);
						std::lock_guard<std::mutex> pbp_l(s_peersByPhysAddr_l);

						auto pbv = s_peersByVirtAddr.find(p->second->id.address());
						if (pbv != s_peersByVirtAddr.end()) {
							pbv->second.erase(p->second);
							if (pbv->second.empty())
								s_peersByVirtAddr.erase(pbv);
						}

						if (p->second->ip4) {
							auto pbp = s_peersByPhysAddr.find(p->second->ip4);
							if (pbp != s_peersByPhysAddr.end()) {
								pbp->second.erase(p->second);
								if (pbp->second.empty())
									s_peersByPhysAddr.erase(pbp);
							}
						}
						if (p->second->ip6) {
							auto pbp = s_peersByPhysAddr.find(p->second->ip6);
							if (pbp != s_peersByPhysAddr.end()) {
								pbp->second.erase(p->second);
								if (pbp->second.empty())
									s_peersByPhysAddr.erase(pbp);
							}
						}

						s_peersByIdentity.erase(p++);
					} else ++p;
				}
			}

			// Remove old rendezvous tracking entries
			{
				std::lock_guard<std::mutex> l(s_lastSentRendezvous_l);
				for(auto lr=s_lastSentRendezvous.begin();lr!=s_lastSentRendezvous.end();) {
					if ((now - lr->second) > ZT_PEER_ACTIVITY_TIMEOUT)
						s_lastSentRendezvous.erase(lr++);
					else ++lr;
				}
			}
		}

		// Write stats if configured to do so, and periodically refresh planet file (if any)
		if (((now - lastWroteStats) > 15000)&&(s_statsRoot.length() > 0)) {
			lastWroteStats = now;

			try {
				if (planetFilePath.length() > 0) {
					std::string planetData;
					if ((OSUtils::readFile(planetFilePath.c_str(),planetData))&&(planetData.length() > 0)) {
						std::lock_guard<std::mutex> pl(s_planet_l);
						s_planet = planetData;
					}
				}
			} catch ( ... ) {
				std::lock_guard<std::mutex> pl(s_planet_l);
				s_planet.clear();
			}

			std::string peersFilePath(s_statsRoot);
			peersFilePath.append("/.peers.tmp");
			FILE *pf = fopen(peersFilePath.c_str(),"wb");
			if (pf) {
				std::vector< SharedPtr<RootPeer> > sp;
				{
					std::lock_guard<std::mutex> pbi_l(s_peersByIdentity_l);
					sp.reserve(s_peersByIdentity.size());
					for(auto p=s_peersByIdentity.begin();p!=s_peersByIdentity.end();++p) {
						sp.push_back(p->second);
					}
				}
				std::sort(sp.begin(),sp.end(),[](const SharedPtr<RootPeer> &a,const SharedPtr<RootPeer> &b) { return (a->id < b->id); });

				char ip4[128],ip6[128];
				for(auto p=sp.begin();p!=sp.end();++p) {
					if ((*p)->ip4) {
						(*p)->ip4.toString(ip4);
					} else {
						ip4[0] = '-';
						ip4[1] = 0;
					}
					if ((*p)->ip6) {
						(*p)->ip6.toString(ip6);
					} else {
						ip6[0] = '-';
						ip6[1] = 0;
					}
					fprintf(pf,"%.10llx %21s %45s %5.4f %d.%d.%d" ZT_EOL_S,(unsigned long long)(*p)->id.address().toInt(),ip4,ip6,fabs((double)(now - (*p)->lastReceive) / 1000.0),(*p)->vMajor,(*p)->vMinor,(*p)->vRev);
				}

				fclose(pf);
				std::string peersFilePath2(s_statsRoot);
				peersFilePath2.append("/peers");
				OSUtils::rm(peersFilePath2);
				OSUtils::rename(peersFilePath.c_str(),peersFilePath2.c_str());
			}

			std::string statsFilePath(s_statsRoot);
			statsFilePath.append("/.stats.tmp");
			FILE *sf = fopen(statsFilePath.c_str(),"wb");
			if (sf) {
				fprintf(sf,"Uptime (seconds)           : %ld" ZT_EOL_S,(long)((now - s_startTime) / 1000));
				s_peersByIdentity_l.lock();
				fprintf(sf,"Peers                      : %llu" ZT_EOL_S,(unsigned long long)s_peersByIdentity.size());
				s_peersByVirtAddr_l.lock();
				fprintf(sf,"Virtual Address Collisions : %llu" ZT_EOL_S,(unsigned long long)(s_peersByIdentity.size() - s_peersByVirtAddr.size()));
				s_peersByVirtAddr_l.unlock();
				s_peersByIdentity_l.unlock();
				s_peersByPhysAddr_l.lock();
				fprintf(sf,"Physical Endpoints         : %llu" ZT_EOL_S,(unsigned long long)s_peersByPhysAddr.size());
				s_peersByPhysAddr_l.unlock();
				s_lastSentRendezvous_l.lock();
				fprintf(sf,"Recent P2P Graph Edges     : %llu" ZT_EOL_S,(unsigned long long)s_lastSentRendezvous.size());
				s_lastSentRendezvous_l.unlock();
				fprintf(sf,"Input BPS                  : %.4f" ZT_EOL_S,s_inputRate.perSecond(now));
				fprintf(sf,"Output BPS                 : %.4f" ZT_EOL_S,s_outputRate.perSecond(now));
				fprintf(sf,"Forwarded BPS              : %.4f" ZT_EOL_S,s_forwardRate.perSecond(now));
				fprintf(sf,"Sibling Forwarded BPS      : %.4f" ZT_EOL_S,s_siblingForwardRate.perSecond(now));
				fprintf(sf,"Discarded Forward BPS      : %.4f" ZT_EOL_S,s_discardedForwardRate.perSecond(now));

				fclose(sf);
				std::string statsFilePath2(s_statsRoot);
				statsFilePath2.append("/stats");
				OSUtils::rm(statsFilePath2);
				OSUtils::rename(statsFilePath.c_str(),statsFilePath2.c_str());
			}
		}
	}

	// If we received a kill signal, close everything and wait
	// for threads to die before exiting.
	apiServ.stop();
	for(auto s=sockets.begin();s!=sockets.end();++s) {
		shutdown(*s,SHUT_RDWR);
		close(*s);
	}
	for(auto t=threads.begin();t!=threads.end();++t)
		t->join();

	return 0;
}
