/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
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
#include <iomanip>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <atomic>
#include <mutex>
#include <list>
#include <sstream>
#include <iomanip>

#include "geoip-html.h"

using namespace ZeroTier;
using json = nlohmann::json;

#ifdef MSG_DONTWAIT
#define SENDTO_FLAGS MSG_DONTWAIT
#define RECVFROM_FLAGS 0
#else
#define SENDTO_FLAGS 0
#define RECVFROM_FLAGS 0
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

/**
 * RootPeer is a normal peer known to this root
 *
 * This struct must remain memcpy-able. Identity, InetAddress, and
 * Atomic all satisfy this. Take care when adding fields that
 * this remains true.
 */
struct RootPeer
{
	ZT_ALWAYS_INLINE RootPeer() : lastSend(0),lastReceive(0),lastEcho(0),lastHello(0),vProto(-1),vMajor(-1),vMinor(-1),vRev(-1) {}
	ZT_ALWAYS_INLINE ~RootPeer() { Utils::burn(key,sizeof(key)); }

	Identity id;            // Identity
	uint8_t key[32];        // Shared secret key
	InetAddress ip4,ip6;    // IPv4 and IPv6 addresses
	int64_t lastSend;       // Time of last send (any packet)
	int64_t lastReceive;    // Time of last receive (any packet)
	int64_t lastEcho;       // Time of last received ECHO
	int64_t lastHello;      // Time of last received HELLO
	int vProto;             // Protocol version or -1 if unknown
	int vMajor,vMinor,vRev; // Peer version or -1,-1,-1 if unknown

	Atomic __refCount;
};

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

struct RendezvousStats
{
	RendezvousStats() : count(0),ts(0) {}
	int64_t count;
	int64_t ts;
};

// These fields are not locked as they're only initialized on startup or are atomic
static int64_t s_startTime;            // Time service was started
static std::vector<int> s_ports;       // Ports to bind for UDP traffic
static int s_relayMaxHops = 0;         // Max relay hops
static Identity s_self;                // My identity (including secret)
static std::atomic_bool s_run;         // Remains true until shutdown is ordered
static json s_config;                  // JSON config file contents
static std::string s_statsRoot;        // Root to write stats, peers, etc.
static std::atomic_bool s_geoInit;     // True if geoIP data is initialized
static std::string s_googleMapsAPIKey; // Google maps API key for GeoIP /map feature

// These are only modified during GeoIP database load (if enabled) and become static after s_geoInit is set to true.
static std::map< std::pair< uint32_t,uint32_t >,std::pair< float,float > > s_geoIp4;
static std::map< std::pair< std::array< uint64_t,2 >,std::array< uint64_t,2 > >,std::pair< float,float > > s_geoIp6;

// Rate meters for statistical purposes (locks are internal to Meter)
static Meter s_inputRate;
static Meter s_outputRate;
static Meter s_forwardRate;
static Meter s_discardedForwardRate;

// These fields are locked using mutexes below as they're modified during runtime
static std::string s_planet;
static std::list< SharedPtr<RootPeer> > s_peers;
static std::unordered_map< uint64_t,std::unordered_map< MulticastGroup,std::unordered_map< Address,int64_t,AddressHasher >,MulticastGroupHasher > > s_multicastSubscriptions;
static std::unordered_map< Identity,SharedPtr<RootPeer>,IdentityHasher > s_peersByIdentity;
static std::unordered_map< Address,std::set< SharedPtr<RootPeer> >,AddressHasher > s_peersByVirtAddr;
static std::unordered_map< RendezvousKey,RendezvousStats,RendezvousKey::Hasher > s_rendezvousTracking;

static std::mutex s_planet_l;
static std::mutex s_peers_l;
static std::mutex s_multicastSubscriptions_l;
static std::mutex s_peersByIdentity_l;
static std::mutex s_peersByVirtAddr_l;
static std::mutex s_rendezvousTracking_l;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

// Construct GeoIP key for IPv4 IPs
static ZT_ALWAYS_INLINE uint32_t ip4ToH32(const InetAddress &ip)
{
	return Utils::ntoh((uint32_t)(((const struct sockaddr_in *)&ip)->sin_addr.s_addr));
}

// Construct GeoIP key for IPv6 IPs
static ZT_ALWAYS_INLINE std::array< uint64_t,2 > ip6ToH128(const InetAddress &ip)
{
	std::array<uint64_t,2> i128;
	memcpy(i128.data(),ip.rawIpData(),16);
	i128[0] = Utils::ntoh(i128[0]);
	i128[1] = Utils::ntoh(i128[1]);
	return i128;
}

static void handlePacket(const int v4s,const int v6s,const InetAddress *const ip,Packet &pkt)
{
	char ipstr[128];
	const bool fragment = pkt[ZT_PACKET_FRAGMENT_IDX_FRAGMENT_INDICATOR] == ZT_PACKET_FRAGMENT_INDICATOR;
	const Address source(pkt.source());
	const Address dest(pkt.destination());
	const int64_t now = OSUtils::now();

	s_inputRate.log(now,pkt.size());

	if ((!fragment)&&(pkt.size() < ZT_PROTO_MIN_PACKET_LENGTH))
		return;

	if ((!fragment)&&(!pkt.fragmented())&&(dest == s_self.address())) {
		SharedPtr<RootPeer> peer;

		// If this is an un-encrypted HELLO, either learn a new peer or verify
		// that this is a peer we already know.
		if ((pkt.cipher() == ZT_PROTO_CIPHER_SUITE__POLY1305_NONE)&&(pkt.verb() == Packet::VERB_HELLO)) {
			Identity id;
			if (id.deserialize(pkt,ZT_PROTO_VERB_HELLO_IDX_IDENTITY)) {
				{
					std::lock_guard<std::mutex> pbi_l(s_peersByIdentity_l);
					auto pById = s_peersByIdentity.find(id);
					if (pById != s_peersByIdentity.end())
						peer = pById->second;
				}
				if (peer) {
					if (!pkt.dearmor(peer->key)) {
						printf("%s HELLO rejected: packet authentication failed" ZT_EOL_S,ip->toString(ipstr));
						return;
					}
				} else {
					peer.set(new RootPeer);

					if (!s_self.agree(id,peer->key)) {
						printf("%s HELLO rejected: key agreement failed" ZT_EOL_S,ip->toString(ipstr));
						return;
					}
					if (!pkt.dearmor(peer->key)) {
						printf("%s HELLO rejected: packet authentication failed" ZT_EOL_S,ip->toString(ipstr));
						return;
					}
					if (!pkt.uncompress()) {
						printf("%s HELLO rejected: decompression failed" ZT_EOL_S,ip->toString(ipstr));
						return;
					}

					peer->id = id;
					peer->lastReceive = now;

					bool added = false;
					{
						std::lock_guard<std::mutex> pbi_l(s_peersByIdentity_l);
						auto existing = s_peersByIdentity.find(id); // make sure another thread didn't do this while we were
						if (existing == s_peersByIdentity.end()) {
							s_peersByIdentity.emplace(id,peer);
							added = true;
						} else {
							peer = existing->second;
						}
					}
					if (added) {
						{
							std::lock_guard<std::mutex> pl(s_peers_l);
							s_peers.emplace_back(peer);
						}
						{
							std::lock_guard<std::mutex> pbv_l(s_peersByVirtAddr_l);
							s_peersByVirtAddr[id.address()].emplace(peer);
						}
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
						break;
					}
				}
			}
		}

		// If we found the peer, update IP and/or time and handle certain key packet types that the
		// root must concern itself with.
		if (peer) {
			if (ip->isV4())
				peer->ip4 = ip;
			else if (ip->isV6())
				peer->ip6 = ip;

			const int64_t now = OSUtils::now();
			peer->lastReceive = now;

			switch(pkt.verb()) {
				case Packet::VERB_HELLO:
					try {
						if ((now - peer->lastHello) > 500) {
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
						if ((now - peer->lastEcho) > 500) {
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
			s_discardedForwardRate.log(now,pkt.size());
			return;
		}
	} else {
		if ((hops = (int)pkt.incrementHops()) > s_relayMaxHops) {
			s_discardedForwardRate.log(now,pkt.size());
			return;
		}

		if (hops == 1) {
			RendezvousKey rk(source,dest);
			std::lock_guard<std::mutex> l(s_rendezvousTracking_l);
			RendezvousStats &lr = s_rendezvousTracking[rk];
			if ((now - lr.ts) >= 30000) {
				++lr.count;
				lr.ts = now;
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
					toAddrs.emplace_back(std::pair< InetAddress *,SharedPtr<RootPeer> >(&((*p)->ip4),*p));
				} else if ((*p)->ip6) {
					toAddrs.emplace_back(std::pair< InetAddress *,SharedPtr<RootPeer> >(&((*p)->ip6),*p));
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

						// Introduce source to destination (V6)
						Packet outp(source,s_self.address(),Packet::VERB_RENDEZVOUS);
						outp.append((uint8_t)0);
						dest.appendTo(outp);
						outp.append((uint16_t)b->second->ip6.port());
						outp.append((uint8_t)16);
						outp.append((const uint8_t *)(b->second->ip6.rawIpData()),16);
						outp.armor((*a)->key,true);
						sendto(v6s,outp.data(),outp.size(),SENDTO_FLAGS,(const struct sockaddr *)&((*a)->ip6),(socklen_t)sizeof(struct sockaddr_in6));

						s_outputRate.log(now,outp.size());
						(*a)->lastSend = now;

						// Introduce destination to source (V6)
						outp.reset(dest,s_self.address(),Packet::VERB_RENDEZVOUS);
						outp.append((uint8_t)0);
						source.appendTo(outp);
						outp.append((uint16_t)(*a)->ip6.port());
						outp.append((uint8_t)16);
						outp.append((const uint8_t *)((*a)->ip6.rawIpData()),16);
						outp.armor(b->second->key,true);
						sendto(v6s,outp.data(),outp.size(),SENDTO_FLAGS,(const struct sockaddr *)&(b->second->ip6),(socklen_t)sizeof(struct sockaddr_in6));

						s_outputRate.log(now,outp.size());
						b->second->lastSend = now;
					}
					if (((*a)->ip4)&&(b->second->ip4)) {

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
						outp.append((uint16_t)(*a)->ip4.port());
						outp.append((uint8_t)4);
						outp.append((const uint8_t *)((*a)->ip4.rawIpData()),4);
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
		if (sendto(i->first->isV4() ? v4s : v6s,pkt.data(),pkt.size(),SENDTO_FLAGS,(const struct sockaddr *)i->first,(socklen_t)(i->first->isV4() ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))) > 0) {
			s_outputRate.log(now,pkt.size());
			s_forwardRate.log(now,pkt.size());
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

	int f = 16777216;
	while (f > 131072) {
		if (setsockopt(s,SOL_SOCKET,SO_RCVBUF,(const char *)&f,sizeof(f)) == 0)
			break;
		f -= 131072;
	}
	f = 16777216;
	while (f > 131072) {
		if (setsockopt(s,SOL_SOCKET,SO_SNDBUF,(const char *)&f,sizeof(f)) == 0)
			break;
		f -= 131072;
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

/*
#ifdef SO_NO_CHECK
	if (bindAddr->sa_family == AF_INET) {
		f = 1; setsockopt(s,SOL_SOCKET,SO_NO_CHECK,(void *)&f,sizeof(f));
	}
#endif
*/

#ifdef SO_REUSEPORT
	f = 1; setsockopt(s,SOL_SOCKET,SO_REUSEPORT,(void *)&f,sizeof(f));
#endif
#ifndef __LINUX__ // linux wants just SO_REUSEPORT
	f = 1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(void *)&f,sizeof(f));
#endif

#ifdef __LINUX__
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(const void *)&tv,sizeof(tv));
#endif

	if (bind(s,bindAddr,(bindAddr->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))) {
		close(s);
		//printf("%s\n",strerror(errno));
		return -1;
	}

	return s;
}

static void shutdownSigHandler(int sig)
{
	s_run = false;
}

int main(int argc,char **argv)
{
	std::vector<std::thread> threads;
	std::vector<int> sockets;
	int v4Sock = -1,v6Sock = -1;

	signal(SIGTERM,shutdownSigHandler);
	signal(SIGINT,shutdownSigHandler);
	signal(SIGQUIT,shutdownSigHandler);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGUSR1,SIG_IGN);
	signal(SIGUSR2,SIG_IGN);
	signal(SIGCHLD,SIG_IGN);

	s_startTime = OSUtils::now();
	s_geoInit = false;

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
		s_relayMaxHops = s_config["relayMaxHops"];
		if (s_relayMaxHops > ZT_PROTO_MAX_HOPS)
			s_relayMaxHops = ZT_PROTO_MAX_HOPS;
		else if (s_relayMaxHops < 0)
			s_relayMaxHops = 0;
	} catch ( ... ) {
		s_relayMaxHops = ZT_RELAY_MAX_HOPS;
	}

	try {
		s_googleMapsAPIKey = s_config["googleMapsAPIKey"];
		std::string geoIpPath = s_config["geoIp"];
		if (geoIpPath.length() > 0) {
			FILE *gf = fopen(geoIpPath.c_str(),"rb");
			if (gf) {
				threads.emplace_back(std::thread([gf]() {
					try {
						char line[1024];
						line[1023] = 0;
						while (fgets(line,sizeof(line)-1,gf)) {
							InetAddress start,end;
							float lat = 0.0F,lon = 0.0F;
							int field = 0;
							for(char *saveptr=nullptr,*f=Utils::stok(line,",\r\n",&saveptr);(f);f=Utils::stok(nullptr,",\r\n",&saveptr)) {
								switch(field++) {
									case 0:
										start.fromString(f);
										break;
									case 1:
										end.fromString(f);
										break;
									case 2:
										lat = strtof(f,nullptr);
										break;
									case 3:
										lon = strtof(f,nullptr);
										break;
								}
							}
							if ((start)&&(end)&&(start.ss_family == end.ss_family)&&(lat >= -90.0F)&&(lat <= 90.0F)&&(lon >= -180.0F)&&(lon <= 180.0F)) {
								if (start.ss_family == AF_INET) {
									s_geoIp4[std::pair< uint32_t,uint32_t >(ip4ToH32(start),ip4ToH32(end))] = std::pair< float,float >(lat,lon);
								} else if (start.ss_family == AF_INET6) {
									s_geoIp6[std::pair< std::array< uint64_t,2 >,std::array< uint64_t,2 > >(ip6ToH128(start),ip6ToH128(end))] = std::pair< float,float >(lat,lon);
								}
							}
						}
						s_geoInit = true;
					} catch ( ... ) {}
					fclose(gf);
				}));
			}
		}
	} catch ( ... ) {}

	unsigned int ncores = std::thread::hardware_concurrency();
	if (ncores == 0) ncores = 1;

	s_run = true;

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
				Packet *pkt = new Packet();
				for(;;) {
					memset(&in6,0,sizeof(in6));
					socklen_t sl = sizeof(in6);
					const int pl = (int)recvfrom(s6,pkt->unsafeData(),pkt->capacity(),RECVFROM_FLAGS,(struct sockaddr *)&in6,&sl);
					if (pl > 0) {
						if ((pl >= ZT_PROTO_MIN_FRAGMENT_LENGTH)&&(pl <= ZT_PROTO_MAX_PACKET_LENGTH)) {
							try {
								pkt->setSize((unsigned int)pl);
								handlePacket(s4,s6,reinterpret_cast<const InetAddress *>(&in6),*pkt);
							} catch (std::exception &exc) {
								char ipstr[128];
								printf("WARNING: unexpected exception handling packet from %s: %s" ZT_EOL_S,reinterpret_cast<const InetAddress *>(&in6)->toString(ipstr),exc.what());
							} catch (int exc) {
								char ipstr[128];
								printf("WARNING: unexpected exception handling packet from %s: ZT exception code %d" ZT_EOL_S,reinterpret_cast<const InetAddress *>(&in6)->toString(ipstr),exc);
							} catch ( ... ) {
								char ipstr[128];
								printf("WARNING: unexpected exception handling packet from %s: unknown exception" ZT_EOL_S,reinterpret_cast<const InetAddress *>(&in6)->toString(ipstr));
							}
						}
					} else if (!s_run) {
						break;
					}
				}
				delete pkt;
			}));

			threads.push_back(std::thread([s6,s4]() {
				struct sockaddr_in in4;
				Packet *pkt = new Packet();
				for(;;) {
					memset(&in4,0,sizeof(in4));
					socklen_t sl = sizeof(in4);
					const int pl = (int)recvfrom(s4,pkt->unsafeData(),pkt->capacity(),RECVFROM_FLAGS,(struct sockaddr *)&in4,&sl);
					if (pl > 0) {
						if ((pl >= ZT_PROTO_MIN_FRAGMENT_LENGTH)&&(pl <= ZT_PROTO_MAX_PACKET_LENGTH)) {
							try {
								pkt->setSize((unsigned int)pl);
								handlePacket(s4,s6,reinterpret_cast<const InetAddress *>(&in4),*pkt);
							} catch (std::exception &exc) {
								char ipstr[128];
								printf("WARNING: unexpected exception handling packet from %s: %s" ZT_EOL_S,reinterpret_cast<const InetAddress *>(&in4)->toString(ipstr),exc.what());
							} catch (int exc) {
								char ipstr[128];
								printf("WARNING: unexpected exception handling packet from %s: ZT exception code %d" ZT_EOL_S,reinterpret_cast<const InetAddress *>(&in4)->toString(ipstr),exc);
							} catch ( ... ) {
								char ipstr[128];
								printf("WARNING: unexpected exception handling packet from %s: unknown exception" ZT_EOL_S,reinterpret_cast<const InetAddress *>(&in4)->toString(ipstr));
							}
						}
					} else if (!s_run) {
						break;
					}
				}
				delete pkt;
			}));
		}
	}

	// A minimal read-only local API for monitoring and status queries
	httplib::Server apiServ;
	threads.push_back(std::thread([&apiServ,httpPort]() {
		// Human readable status page
		apiServ.Get("/",[](const httplib::Request &req,httplib::Response &res) {
			std::ostringstream o;
			o << "ZeroTier Root Server " << ZEROTIER_ONE_VERSION_MAJOR << '.' << ZEROTIER_ONE_VERSION_MINOR << '.' << ZEROTIER_ONE_VERSION_REVISION << ZT_EOL_S;
			o << "(c)2019 ZeroTier, Inc." ZT_EOL_S "Licensed under the ZeroTier BSL 1.1" ZT_EOL_S ZT_EOL_S;
			s_peersByIdentity_l.lock();
			o << "Peers Online:       " << s_peersByIdentity.size() << ZT_EOL_S;
			s_peersByIdentity_l.unlock();
			res.set_content(o.str(),"text/plain");
		});

		apiServ.Get("/metrics",[](const httplib::Request &req, httplib::Response &res) {
			std::ostringstream o;
			int64_t now = OSUtils::now();

			char buf[11];
			const char *root_id = s_self.address().toString(buf);
			o << "# HELP root_peers_online Number of active peers online" << ZT_EOL_S;
			o << "# TYPE root_peers_online gauge" << ZT_EOL_S;
			s_peersByIdentity_l.lock();
			o << "root_peers_online{root_id=\"" << root_id << "\"} " << s_peersByIdentity.size() << ZT_EOL_S;
			s_peersByIdentity_l.unlock();
			o << "# HELP root_input_rate Input rate MiB/s" << ZT_EOL_S;
			o << "# TYPE root_input_rate gauge" << ZT_EOL_S;
			o << "root_input_rate{root_id=\"" <<  root_id << "\"} " << std::setprecision(5) << (s_inputRate.perSecond(now)/1048576.0) << ZT_EOL_S;
			o << "# HELP root_output_rate Output rate MiB/s" << ZT_EOL_S;
			o << "# TYPE root_output_rate gauge" << ZT_EOL_S;
			o << "root_output_rate{root_id=\"" << root_id << "\"} " << std::setprecision(5) << (s_outputRate.perSecond(now)/1048576.0) << ZT_EOL_S;
			o << "# HELP root_forwarded_rate Forwarded packet rate MiB/s" << ZT_EOL_S;
			o << "# TYPE root_forwarded_rate gauge" << ZT_EOL_S;
			o << "root_forwarded_rate{root_id=\"" << root_id << "\"} " << std::setprecision(5) << (s_forwardRate.perSecond(now)/1048576.0) << ZT_EOL_S;
			o << "# HELP root_discarded_rate Discarded forwards MiB/s" << ZT_EOL_S;
			o << "# TYPE root_discarded_rate gauge" << ZT_EOL_S;
			o << "root_discarded_rate{root_id=\"" << root_id << "\"} " << std::setprecision(5) << (s_discardedForwardRate.perSecond(now)/1048576.0) << ZT_EOL_S;

			res.set_content(o.str(), "text/plain");
		});

		// Peer list for compatibility with software that monitors regular nodes
		apiServ.Get("/peer",[](const httplib::Request &req,httplib::Response &res) {
			char tmp[256];
			std::ostringstream o;
			o << '[';
			try {
				bool first = true;
				std::lock_guard<std::mutex> l(s_peers_l);
				for(auto p=s_peers.begin();p!=s_peers.end();++p) {
					if (first)
						first = false;
					else o << ',';
					o <<
					"{\"address\":\"" << (*p)->id.address().toString(tmp) << "\""
					",\"latency\":-1"
					",\"paths\":[";
					if ((*p)->ip4) {
						o <<
						"{\"active\":true"
						",\"address\":\"" << (*p)->ip4.toIpString(tmp) << "\\/" << (*p)->ip4.port() << "\""
						",\"expired\":false"
						",\"lastReceive\":" << (*p)->lastReceive <<
						",\"lastSend\":" << (*p)->lastSend <<
						",\"preferred\":true"
						",\"trustedPathId\":0}";
					}
					if ((*p)->ip6) {
						if ((*p)->ip4)
							o << ',';
						o <<
						"{\"active\":true"
						",\"address\":\"" << (*p)->ip6.toIpString(tmp) << "\\/" << (*p)->ip6.port() << "\""
						",\"expired\":false"
						",\"lastReceive\":" << (*p)->lastReceive <<
						",\"lastSend\":" << (*p)->lastSend <<
						",\"preferred\":" << (((*p)->ip4) ? "false" : "true") <<
						",\"trustedPathId\":0}";
					}
					o << "]"
					",\"role\":\"LEAF\""
					",\"version\":\"" << (*p)->vMajor << '.' << (*p)->vMinor << '.' << (*p)->vRev << "\""
					",\"versionMajor\":" << (*p)->vMajor <<
					",\"versionMinor\":" << (*p)->vMinor <<
					",\"versionRev\":" << (*p)->vRev << "}";
				}
			} catch ( ... ) {}
			o << ']';
			res.set_content(o.str(),"application/json");
		});

		// GeoIP map if enabled
		apiServ.Get("/map",[](const httplib::Request &req,httplib::Response &res) {
			char tmp[4096];
			if (!s_geoInit) {
				res.set_content("Not enabled or GeoIP CSV file not finished reading.","text/plain");
				return;
			}
			std::ostringstream o;
			o << ZT_GEOIP_HTML_HEAD;
			try {
				bool firstCoord = true;
				std::pair< uint32_t,uint32_t > k4(0,0xffffffff);
				std::pair< std::array< uint64_t,2 >,std::array< uint64_t,2 > > k6;
				k6.second[0] = 0xffffffffffffffffULL; k6.second[1] = 0xffffffffffffffffULL;

				std::unordered_map< InetAddress,std::set<Address>,InetAddressHasher > ips;
				{
					std::lock_guard<std::mutex> l(s_peers_l);
					for(auto p=s_peers.begin();p!=s_peers.end();++p) {
						if ((*p)->ip4)
							ips[(*p)->ip4].insert((*p)->id.address());
						if ((*p)->ip6)
							ips[(*p)->ip6].insert((*p)->id.address());
					}
				}

				for(auto p=ips.begin();p!=ips.end();++p) {
					if (p->first.isV4()) {
						k4.first = ip4ToH32(p->first);
						auto geo = std::map< std::pair< uint32_t,uint32_t >,std::pair< float,float > >::reverse_iterator(s_geoIp4.upper_bound(k4));
						uint32_t bestRangeSize = 0xffffffff;
						std::pair< float,float > bestRangeLatLon;
						while (geo != s_geoIp4.rend()) {
							if ((geo->first.first <= k4.first)&&(geo->first.second >= k4.first)) {
								uint32_t range = geo->first.second - geo->first.first;
								if (range <= bestRangeSize) {
									bestRangeSize = range;
									bestRangeLatLon = geo->second;
								}
							} else if ((geo->first.first < k4.first)&&(geo->first.second < k4.first)) {
								break;
							}
							++geo;
						}
						if (bestRangeSize != 0xffffffff) {
							if (!firstCoord)
								o << ',';
							firstCoord = false;
							o << "{lat:" << bestRangeLatLon.first << ",lng:" << bestRangeLatLon.second << ",_l:\"";
							bool firstAddr = true;
							for(auto a=p->second.begin();a!=p->second.end();++a) {
								if (!firstAddr)
									o << ',';
								o << a->toString(tmp);
								firstAddr = false;
							}
							o << "\"}";
						}
					} else if (p->first.isV6()) {
						k6.first = ip6ToH128(p->first);
						auto geo = std::map< std::pair< std::array< uint64_t,2 >,std::array< uint64_t,2 > >,std::pair< float,float > >::reverse_iterator(s_geoIp6.upper_bound(k6));
						while (geo != s_geoIp6.rend()) {
							if ((geo->first.first <= k6.first)&&(geo->first.second >= k6.first)) {
								if (!firstCoord)
									o << ',';
								firstCoord = false;
								o << "{lat:" << geo->second.first << ",lng:" << geo->second.second << ",_l:\"";
								bool firstAddr = true;
								for(auto a=p->second.begin();a!=p->second.end();++a) {
									if (!firstAddr)
										o << ',';
									o << a->toString(tmp);
									firstAddr = false;
								}
								o << "\"}";
								break;
							} else if ((geo->first.first < k6.first)&&(geo->first.second < k6.first)) {
								break;
							}
							++geo;
						}
					}
				}
			} catch ( ... ) {
				res.set_content("Internal error: unexpected exception resolving GeoIP locations","text/plain");
				return;
			}
			OSUtils::ztsnprintf(tmp,sizeof(tmp),ZT_GEOIP_HTML_TAIL,s_googleMapsAPIKey.c_str());
			o << tmp;
			res.set_content(o.str(),"text/html");
		});

		apiServ.listen("127.0.0.1",httpPort,0);
	}));

	// In the main thread periodically clean stuff up
	int64_t lastCleaned = 0;
	int64_t lastWroteStats = 0;
	while (s_run) {
		sleep(1);

		const int64_t now = OSUtils::now();

		if ((now - lastCleaned) > 300000) {
			lastCleaned = now;

			// Old multicast subscription cleanup
			{
				std::lock_guard<std::mutex> l(s_multicastSubscriptions_l);
				for(auto a=s_multicastSubscriptions.begin();a!=s_multicastSubscriptions.end();) {
					for(auto b=a->second.begin();b!=a->second.end();) {
						for(auto c=b->second.begin();c!=b->second.end();) {
							if ((now - c->second) > ZT_PEER_ACTIVITY_TIMEOUT)
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
			try {
				std::vector< SharedPtr<RootPeer> > toRemove;
				toRemove.reserve(1024);
				{
					std::lock_guard<std::mutex> pbi_l(s_peers_l);
					for(auto p=s_peers.begin();p!=s_peers.end();) {
						if ((now - (*p)->lastReceive) > ZT_PEER_ACTIVITY_TIMEOUT) {
							toRemove.emplace_back(*p);
							s_peers.erase(p++);
						} else ++p;
					}
				}
				for(auto p=toRemove.begin();p!=toRemove.end();++p) {
					{
						std::lock_guard<std::mutex> pbi_l(s_peersByIdentity_l);
						s_peersByIdentity.erase((*p)->id);
					}
					{
						std::lock_guard<std::mutex> pbv_l(s_peersByVirtAddr_l);
						auto pbv = s_peersByVirtAddr.find((*p)->id.address());
						if (pbv != s_peersByVirtAddr.end()) {
							pbv->second.erase(*p);
							if (pbv->second.empty())
								s_peersByVirtAddr.erase(pbv);
						}
					}
				}
			} catch ( ... ) {}

			// Remove old rendezvous entries
			{
				std::lock_guard<std::mutex> l(s_rendezvousTracking_l);
				for(auto lr=s_rendezvousTracking.begin();lr!=s_rendezvousTracking.end();) {
					if ((now - lr->second.ts) > ZT_PEER_ACTIVITY_TIMEOUT)
						s_rendezvousTracking.erase(lr++);
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
					std::lock_guard<std::mutex> pbi_l(s_peers_l);
					sp.reserve(s_peers.size());
					for(auto p=s_peers.begin();p!=s_peers.end();++p) {
						sp.emplace_back(*p);
					}
				}
				std::sort(sp.begin(),sp.end(),[](const SharedPtr<RootPeer> &a,const SharedPtr<RootPeer> &b) { return (a->id < b->id); });

				fprintf(pf,"Address    %21s %45s %10s %6s %10s" ZT_EOL_S,"IPv4","IPv6","Age(sec)","Vers","Fwd(KiB/s)");
				{
					char ip4[128],ip6[128],ver[128];
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
						OSUtils::ztsnprintf(ver,sizeof(ver),"%d.%d.%d",(*p)->vMajor,(*p)->vMinor,(*p)->vRev);
						fprintf(pf,"%.10llx %21s %45s %10.4f %6s" ZT_EOL_S,
							(unsigned long long)(*p)->id.address().toInt(),
							ip4,
							ip6,
							fabs((double)(now - (*p)->lastReceive) / 1000.0),
							ver);
					}
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
				auto peersByIdentitySize = s_peersByIdentity.size();
				s_peersByIdentity_l.unlock();
				fprintf(sf,"Peers                      : %llu" ZT_EOL_S,(unsigned long long)peersByIdentitySize);
				s_peersByVirtAddr_l.lock();
				fprintf(sf,"Virtual Address Collisions : %llu" ZT_EOL_S,(unsigned long long)(peersByIdentitySize - s_peersByVirtAddr.size()));
				s_peersByVirtAddr_l.unlock();
				s_rendezvousTracking_l.lock();
				uint64_t unsuccessfulp2p = 0;
				for(auto lr=s_rendezvousTracking.begin();lr!=s_rendezvousTracking.end();++lr) {
					if (lr->second.count > 6) // 6 == two attempts per edge, one for each direction
						++unsuccessfulp2p;
				}
				fprintf(sf,"Recent P2P Graph Edges     : %llu" ZT_EOL_S,(unsigned long long)s_rendezvousTracking.size());
				if (s_rendezvousTracking.empty()) {
					fprintf(sf,"Recent P2P Success Rate    : 100.0000%%" ZT_EOL_S);
				} else {
					fprintf(sf,"Recent P2P Success Rate    : %.4f%%" ZT_EOL_S,(1.0 - ((double)unsuccessfulp2p / (double)s_rendezvousTracking.size())) * 100.0);
				}
				s_rendezvousTracking_l.unlock();
				fprintf(sf,"Input (MiB/s)              : %.4f" ZT_EOL_S,s_inputRate.perSecond(now) / 1048576.0);
				fprintf(sf,"Output (MiB/s)             : %.4f" ZT_EOL_S,s_outputRate.perSecond(now) / 1048576.0);
				fprintf(sf,"Forwarded (MiB/s)          : %.4f" ZT_EOL_S,s_forwardRate.perSecond(now) / 1048576.0);
				fprintf(sf,"Discarded Forward (MiB/s)  : %.4f" ZT_EOL_S,s_discardedForwardRate.perSecond(now) / 1048576.0);

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
	s_run = false; // sanity check
	apiServ.stop();
	for(auto s=sockets.begin();s!=sockets.end();++s) {
		shutdown(*s,SHUT_RDWR);
		close(*s);
	}
	for(auto t=threads.begin();t!=threads.end();++t)
		t->join();

	return 0;
}
