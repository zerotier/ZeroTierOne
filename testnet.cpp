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

/* SEE: testnet/README.md */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <map>
#include <vector>
#include <set>

#include "node/Constants.hpp"
#include "node/Node.hpp"
#include "node/Utils.hpp"
#include "node/Address.hpp"
#include "node/Identity.hpp"
#include "node/Thread.hpp"
#include "node/CMWC4096.hpp"
#include "node/Dictionary.hpp"

#include "testnet/SimNet.hpp"
#include "testnet/SimNetSocketManager.hpp"
#include "testnet/TestEthernetTap.hpp"
#include "testnet/TestEthernetTapFactory.hpp"
#include "testnet/TestRoutingTable.hpp"

#ifdef __WINDOWS__
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

using namespace ZeroTier;

class SimNode
{
public:
	SimNode(SimNet &net,const std::string &hp,const char *rootTopology,bool issn,const InetAddress &addr) :
		home(hp),
		tapFactory(),
		routingTable(),
		socketManager(net.newEndpoint(addr)),
		node(home.c_str(),&tapFactory,&routingTable,socketManager,false,rootTopology),
		reasonForTermination(Node::NODE_RUNNING),
		supernode(issn)
	{
		thread = Thread::start(this);
	}

	~SimNode()
	{
		node.terminate(Node::NODE_NORMAL_TERMINATION,"SimNode shutdown");
		Thread::join(thread);
	}

	void threadMain()
		throw()
	{
		reasonForTermination = node.run();
	}

	std::string home;
	TestEthernetTapFactory tapFactory;
	TestRoutingTable routingTable;
	SimNetSocketManager *socketManager;
	Node node;
	Node::ReasonForTermination reasonForTermination;
	bool supernode;
	Thread thread;
};

static std::string basePath;
static SimNet net;
static std::map< Address,SimNode * > nodes;
static std::map< InetAddress,Address > usedIps;
static CMWC4096 prng;
static std::string rootTopology;

// Converts an address into a fake IP not already claimed.
// Be sure to call only once, as this claims the IP before returning it.
static InetAddress inetAddressFromZeroTierAddress(const Address &addr)
{
	uint32_t ip = (uint32_t)(addr.toInt() & 0xffffffff);
	for(;;) {
		if (((ip >> 24) & 0xff) >= 240) {
			ip &= 0x00ffffff;
			ip |= (((ip >> 24) & 0xff) % 240) << 24;
		}
		if (((ip >> 24) & 0xff) == 0)
			ip |= 0x01000000;
		if (((ip & 0xff) == 0)||((ip & 0xff) == 255))
			ip ^= 0x00000001;
		InetAddress inaddr(Utils::hton(ip),9993);
		if (usedIps.find(inaddr) == usedIps.end()) {
			usedIps[inaddr] = addr;
			return inaddr;
		}
		++ip; // keep looking sequentially for an unclaimed IP
	}
}

static Identity makeNodeHome(bool super)
{
	Identity id;
	id.generate();

	std::string path(basePath + ZT_PATH_SEPARATOR_S + (super ? "S" : "N") + id.address().toString());

#ifdef __WINDOWS__
	CreateDirectoryA(path.c_str(),NULL);
#else
	mkdir(path.c_str(),0700);
#endif

	if (!Utils::writeFile((path + ZT_PATH_SEPARATOR_S + "identity.secret").c_str(),id.toString(true)))
		return Identity();
	if (!Utils::writeFile((path + ZT_PATH_SEPARATOR_S + "identity.public").c_str(),id.toString(false)))
		return Identity();

	return id;
}

// Instantiates supernodes by scanning for S########## subdirectories
static std::vector<Address> initSupernodes()
{
	Dictionary supernodes;
	std::vector< std::pair<Identity,InetAddress> > snids;
	std::map<std::string,bool> dir(Utils::listDirectory(basePath.c_str()));

	for(std::map<std::string,bool>::iterator d(dir.begin());d!=dir.end();++d) {
		if ((d->first.length() == 11)&&(d->second)&&(d->first[0] == 'S')) {
			std::string idbuf;
			if (Utils::readFile((basePath + ZT_PATH_SEPARATOR_S + d->first + ZT_PATH_SEPARATOR_S + "identity.public").c_str(),idbuf)) {
				Identity id(idbuf);
				if (id) {
					InetAddress inaddr(inetAddressFromZeroTierAddress(id.address()));
					snids.push_back(std::pair<Identity,InetAddress>(id,inaddr));

					Dictionary snd;
					snd["id"] = id.toString(false);
					snd["udp"] = inaddr.toString();
					snd["desc"] = id.address().toString();
					snd["dns"] = inaddr.toIpString();
					supernodes[id.address().toString()] = snd.toString();
				}
			}
		}
	}

	Dictionary rtd;
	rtd["supernodes"] = supernodes.toString();
	rtd["noupdate"] = "1";
	rootTopology = rtd.toString();

	std::vector<Address> newNodes;

	for(std::vector< std::pair<Identity,InetAddress> >::iterator i(snids.begin());i!=snids.end();++i) {
		SimNode *n = new SimNode(net,(basePath + ZT_PATH_SEPARATOR_S + "S" + i->first.address().toString()),rootTopology.c_str(),true,i->second);
		nodes[i->first.address()] = n;
		newNodes.push_back(i->first.address());
	}

	return newNodes;
}

// Instantiates any not-already-instantiated regular nodes
static std::vector<Address> scanForNewNodes()
{
	std::vector<Address> newNodes;
	std::map<std::string,bool> dir(Utils::listDirectory(basePath.c_str()));

	for(std::map<std::string,bool>::iterator d(dir.begin());d!=dir.end();++d) {
		if ((d->first.length() == 11)&&(d->second)&&(d->first[0] == 'N')) {
			Address na(d->first.c_str() + 1);
			if (nodes.find(na) == nodes.end()) {
				InetAddress inaddr(inetAddressFromZeroTierAddress(na));

				SimNode *n = new SimNode(net,(basePath + ZT_PATH_SEPARATOR_S + d->first),rootTopology.c_str(),false,inaddr);
				nodes[na] = n;

				newNodes.push_back(na);
			}
		}
	}

	return newNodes;
}

static void doHelp(const std::vector<std::string> &cmd)
{
	printf("---------- help"ZT_EOL_S);
	printf("---------- mksn <number of supernodes>"ZT_EOL_S);
	printf("---------- mkn <number of normal nodes>"ZT_EOL_S);
	printf("---------- list"ZT_EOL_S);
	printf("---------- join <address/*/**> <network ID>"ZT_EOL_S);
	printf("---------- leave <address/*/**> <network ID>"ZT_EOL_S);
	printf("---------- listnetworks <address/*/**>"ZT_EOL_S);
	printf("---------- listpeers <address/*/**>"ZT_EOL_S);
	printf("---------- unicast <address/*/**> <address/*/**> <network ID> <frame length, min: 16> [<timeout (sec)>]"ZT_EOL_S);
	printf("---------- multicast <address/*/**> <MAC/* for bcast> <network ID> <frame length, min: 16> [<timeout (sec)>]"ZT_EOL_S);
	printf("---------- quit"ZT_EOL_S);
	printf("---------- ( * means all regular nodes, ** means including supernodes )"ZT_EOL_S);
	printf("---------- ( . runs previous command again )"ZT_EOL_S);
}

static void doMKSN(const std::vector<std::string> &cmd)
{
	if (cmd.size() < 2) {
		doHelp(cmd);
		return;
	}
	if (nodes.size() > 0) {
		printf("---------- mksn error: mksn can only be called once (network already exists)"ZT_EOL_S);
		return;
	}

	int count = Utils::strToInt(cmd[1].c_str());
	for(int i=0;i<count;++i) {
		Identity id(makeNodeHome(true));
		printf("%s identity created"ZT_EOL_S,id.address().toString().c_str());
	}

	std::vector<Address> nodes(initSupernodes());
	for(std::vector<Address>::iterator a(nodes.begin());a!=nodes.end();++a)
		printf("%s started (supernode)"ZT_EOL_S,a->toString().c_str());

	//printf("---------- root topology is: %s"ZT_EOL_S,rootTopology.c_str());
}

static void doMKN(const std::vector<std::string> &cmd)
{
	if (cmd.size() < 2) {
		doHelp(cmd);
		return;
	}
	if (nodes.size() == 0) {
		printf("---------- mkn error: use mksn to create supernodes first."ZT_EOL_S);
		return;
	}

	int count = Utils::strToInt(cmd[1].c_str());
	for(int i=0;i<count;++i) {
		Identity id(makeNodeHome(false));
		printf("%s identity created"ZT_EOL_S,id.address().toString().c_str());
	}

	std::vector<Address> nodes(scanForNewNodes());
	for(std::vector<Address>::iterator a(nodes.begin());a!=nodes.end();++a)
		printf("%s started (regular node)"ZT_EOL_S,a->toString().c_str());
}

static void doList(const std::vector<std::string> &cmd)
{
	unsigned int peers = 0,supernodes = 0;
	ZT1_Node_Status status;
	for(std::map< Address,SimNode * >::iterator n(nodes.begin());n!=nodes.end();++n) {
		n->second->node.status(&status);
		if (status.initialized) {
			printf("%s %c %s (%u peers, %u direct links)"ZT_EOL_S,
				n->first.toString().c_str(),
				n->second->supernode ? 'S' : 'N',
				(status.online ? "ONLINE" : "OFFLINE"),
				status.knownPeers,
				status.directlyConnectedPeers);
			if (n->second->supernode)
				++supernodes;
			else ++peers;
		} else printf("%s ? INITIALIZING (0 peers, 0 direct links)"ZT_EOL_S,n->first.toString().c_str());
	}
	printf("---------- %u regular peers, %u supernodes"ZT_EOL_S,peers,supernodes);
}

static void doJoin(const std::vector<std::string> &cmd)
{
	if (cmd.size() < 3) {
		doHelp(cmd);
		return;
	}

	std::vector<Address> addrs;
	if ((cmd[1] == "*")||(cmd[1] == "**")) {
		bool includeSuper = (cmd[1] == "**");
		for(std::map< Address,SimNode * >::iterator n(nodes.begin());n!=nodes.end();++n) {
			if ((includeSuper)||(!n->second->supernode))
				addrs.push_back(n->first);
		}
	} else addrs.push_back(Address(cmd[1]));

	uint64_t nwid = Utils::hexStrToU64(cmd[2].c_str());

	for(std::vector<Address>::iterator a(addrs.begin());a!=addrs.end();++a) {
		std::map< Address,SimNode * >::iterator n(nodes.find(*a));
		if (n != nodes.end()) {
			n->second->node.join(nwid);
			printf("%s join %.16llx"ZT_EOL_S,n->first.toString().c_str(),(unsigned long long)nwid);
		}
	}
}

static void doLeave(const std::vector<std::string> &cmd)
{
	if (cmd.size() < 3) {
		doHelp(cmd);
		return;
	}

	std::vector<Address> addrs;
	if ((cmd[1] == "*")||(cmd[1] == "**")) {
		bool includeSuper = (cmd[1] == "**");
		for(std::map< Address,SimNode * >::iterator n(nodes.begin());n!=nodes.end();++n) {
			if ((includeSuper)||(!n->second->supernode))
				addrs.push_back(n->first);
		}
	} else addrs.push_back(Address(cmd[1]));

	uint64_t nwid = Utils::hexStrToU64(cmd[2].c_str());

	for(std::vector<Address>::iterator a(addrs.begin());a!=addrs.end();++a) {
		std::map< Address,SimNode * >::iterator n(nodes.find(*a));
		if (n != nodes.end()) {
			n->second->node.leave(nwid);
			printf("%s leave %.16llx"ZT_EOL_S,n->first.toString().c_str(),(unsigned long long)nwid);
		}
	}
}

static void doListNetworks(const std::vector<std::string> &cmd)
{
	if (cmd.size() < 2) {
		doHelp(cmd);
		return;
	}

	std::vector<Address> addrs;
	if ((cmd[1] == "*")||(cmd[1] == "**")) {
		bool includeSuper = (cmd[1] == "**");
		for(std::map< Address,SimNode * >::iterator n(nodes.begin());n!=nodes.end();++n) {
			if ((includeSuper)||(!n->second->supernode))
				addrs.push_back(n->first);
		}
	} else addrs.push_back(Address(cmd[1]));

	printf("---------- <nwid> <name> <mac> <status> <config age> <type> <dev> <ips>"ZT_EOL_S);

	for(std::vector<Address>::iterator a(addrs.begin());a!=addrs.end();++a) {
		std::string astr(a->toString());
		std::map< Address,SimNode * >::iterator n(nodes.find(*a));
		if (n != nodes.end()) {
			ZT1_Node_NetworkList *nl = n->second->node.listNetworks();
			if (nl) {
				for(unsigned int i=0;i<nl->numNetworks;++i) {
					printf("%s %s %s %s %s %ld %s %s ",
						astr.c_str(),
						nl->networks[i].nwidHex,
						nl->networks[i].name,
						nl->networks[i].macStr,
						nl->networks[i].statusStr,
						nl->networks[i].configAge,
						(nl->networks[i].isPrivate ? "private" : "public"),
						nl->networks[i].device);
					if (nl->networks[i].numIps > 0) {
						for(unsigned int j=0;j<nl->networks[i].numIps;++j) {
							if (j > 0)
								printf(",");
							printf("%s/%d",nl->networks[i].ips[j].ascii,(int)nl->networks[i].ips[j].port);
						}
					} else printf("-");
					printf(ZT_EOL_S);
				}
				n->second->node.freeQueryResult(nl);
			}
		}
	}
}

static void doListPeers(const std::vector<std::string> &cmd)
{
	if (cmd.size() < 2) {
		doHelp(cmd);
		return;
	}

	std::vector<Address> addrs;
	if ((cmd[1] == "*")||(cmd[1] == "**")) {
		bool includeSuper = (cmd[1] == "**");
		for(std::map< Address,SimNode * >::iterator n(nodes.begin());n!=nodes.end();++n) {
			if ((includeSuper)||(!n->second->supernode))
				addrs.push_back(n->first);
		}
	} else addrs.push_back(Address(cmd[1]));

	printf("---------- <ztaddr> <paths> <latency> <version> <role>"ZT_EOL_S);

	for(std::vector<Address>::iterator a(addrs.begin());a!=addrs.end();++a) {
		std::string astr(a->toString());
		std::map< Address,SimNode * >::iterator n(nodes.find(*a));
		if (n != nodes.end()) {
			ZT1_Node_PeerList *pl = n->second->node.listPeers();
			if (pl) {
				for(unsigned int i=0;i<pl->numPeers;++i) {
					printf("%s %.10llx ",astr.c_str(),(unsigned long long)pl->peers[i].rawAddress);
					if (pl->peers[i].numPaths == 0)
						printf("-");
					else {
						for(unsigned int j=0;j<pl->peers[i].numPaths;++j) {
							if (j > 0)
								printf(",");
							switch(pl->peers[i].paths[j].type) {
								default:
									printf("unknown;");
									break;
								case ZT1_Node_PhysicalPath_TYPE_UDP:
									printf("udp;");
									break;
								case ZT1_Node_PhysicalPath_TYPE_TCP_OUT:
									printf("tcp_out;");
									break;
								case ZT1_Node_PhysicalPath_TYPE_TCP_IN:
									printf("tcp_in;");
									break;
								case ZT1_Node_PhysicalPath_TYPE_ETHERNET:
									printf("eth;");
									break;
							}
							printf("%s/%d;%ld;%ld;%ld;%s",
								pl->peers[i].paths[j].address.ascii,
								(int)pl->peers[i].paths[j].address.port,
								pl->peers[i].paths[j].lastSend,
								pl->peers[i].paths[j].lastReceive,
								pl->peers[i].paths[j].lastPing,
								(pl->peers[i].paths[j].fixed ? "fixed" : (pl->peers[i].paths[j].active ? "active" : "inactive")));
						}
					}
					const char *rolestr;
					switch(pl->peers[i].role) {
						case ZT1_Node_Peer_SUPERNODE: rolestr = "SUPERNODE"; break;
						case ZT1_Node_Peer_HUB: rolestr = "HUB"; break;
						case ZT1_Node_Peer_NODE: rolestr = "NODE"; break;
						default: rolestr = "?"; break;
					}
					printf(" %u %s %s"ZT_EOL_S,
						pl->peers[i].latency,
						((pl->peers[i].remoteVersion[0]) ? pl->peers[i].remoteVersion : "-"),
						rolestr);
				}
				n->second->node.freeQueryResult(pl);
			}
		}
	}
}

static void doUnicast(const std::vector<std::string> &cmd)
{
	union {
		uint64_t i[2];
		unsigned char data[2800];
	} pkt;

	if (cmd.size() < 5) {
		doHelp(cmd);
		return;
	}

	uint64_t nwid = Utils::hexStrToU64(cmd[3].c_str());
	unsigned int frameLen = Utils::strToUInt(cmd[4].c_str());
	uint64_t tout = 2000;
	if (cmd.size() >= 6)
		tout = Utils::strToU64(cmd[5].c_str()) * 1000ULL;

	if (frameLen < 16)
		frameLen = 16;
	if (frameLen > 2800)
		frameLen = 2800;

	std::vector<Address> senders;
	if ((cmd[1] == "*")||(cmd[1] == "**")) {
		bool includeSuper = (cmd[1] == "**");
		for(std::map< Address,SimNode * >::iterator n(nodes.begin());n!=nodes.end();++n) {
			if ((includeSuper)||(!n->second->supernode))
				senders.push_back(n->first);
		}
	} else senders.push_back(Address(cmd[1]));

	std::vector<Address> receivers;
	if ((cmd[2] == "*")||(cmd[2] == "**")) {
		bool includeSuper = (cmd[2] == "**");
		for(std::map< Address,SimNode * >::iterator n(nodes.begin());n!=nodes.end();++n) {
			if ((includeSuper)||(!n->second->supernode))
				receivers.push_back(n->first);
		}
	} else receivers.push_back(Address(cmd[2]));

	for(unsigned int i=0;i<frameLen;++i)
		pkt.data[i] = (unsigned char)prng.next32();

	std::set< std::pair<Address,Address> > sentPairs;
	for(std::vector<Address>::iterator s(senders.begin());s!=senders.end();++s) {
		for(std::vector<Address>::iterator r(receivers.begin());r!=receivers.end();++r) {
			if (*s == *r)
				continue;

			SimNode *sender = nodes[*s];
			SimNode *receiver = nodes[*r];
			TestEthernetTap *stap = sender->tapFactory.getByNwid(nwid);
			TestEthernetTap *rtap = receiver->tapFactory.getByNwid(nwid);

			if ((stap)&&(rtap)) {
				pkt.i[0] = s->toInt();
				pkt.i[1] = Utils::now();
				stap->injectPacketFromHost(stap->mac(),rtap->mac(),0xdead,pkt.data,frameLen);
				printf("%s -> %s etherType 0xdead network %.16llx length %u"ZT_EOL_S,s->toString().c_str(),r->toString().c_str(),(unsigned long long)nwid,frameLen);
				sentPairs.insert(std::pair<Address,Address>(*s,*r));
			} else if (stap) {
				printf("%s -> !%s (receiver not a member of %.16llx)"ZT_EOL_S,s->toString().c_str(),r->toString().c_str(),(unsigned long long)nwid);
			} else if (rtap) {
				printf("%s -> !%s (sender not a member of %.16llx)"ZT_EOL_S,s->toString().c_str(),r->toString().c_str(),(unsigned long long)nwid);
			} else {
				printf("%s -> !%s (neither party is a member of %.16llx)"ZT_EOL_S,s->toString().c_str(),r->toString().c_str(),(unsigned long long)nwid);
			}
		}
	}

	printf("---------- waiting up to %llu seconds..."ZT_EOL_S,tout / 1000ULL);

	std::set< std::pair<Address,Address> > receivedPairs;
	TestEthernetTap::TestFrame frame;
	uint64_t toutend = Utils::now() + tout;
	do {
		for(std::vector<Address>::iterator r(receivers.begin());r!=receivers.end();++r) {
			SimNode *receiver = nodes[*r];
			TestEthernetTap *rtap = receiver->tapFactory.getByNwid(nwid);

			if ((rtap)&&(rtap->getNextReceivedFrame(frame,5))) {
				if ((frame.len == frameLen)&&(!memcmp(frame.data + 16,pkt.data + 16,frameLen - 16))) {
					uint64_t ints[2];
					memcpy(ints,frame.data,16);
					printf("%s <- %.10llx received test packet, length == %u, latency == %llums"ZT_EOL_S,r->toString().c_str(),(unsigned long long)ints[0],frame.len,(unsigned long long)(frame.timestamp - ints[1]));
					receivedPairs.insert(std::pair<Address,Address>(Address(ints[0]),*r));
				} else {
					printf("%s !! got spurious packet, length == %u, etherType == 0x%.4x"ZT_EOL_S,r->toString().c_str(),frame.len,frame.etherType);
				}
			}
		}

		Thread::sleep(100);
	} while ((receivedPairs.size() < sentPairs.size())&&(Utils::now() < toutend));

	for(std::vector<Address>::iterator s(senders.begin());s!=senders.end();++s) {
		for(std::vector<Address>::iterator r(receivers.begin());r!=receivers.end();++r) {
			if (*s == *r)
				continue;
			if ((sentPairs.count(std::pair<Address,Address>(*s,*r)))&&(!receivedPairs.count(std::pair<Address,Address>(*s,*r)))) {
				printf("%s <- %s was never received (timed out)"ZT_EOL_S,r->toString().c_str(),s->toString().c_str());
			}
		}
	}

	printf("---------- sent %u, received %u"ZT_EOL_S,(unsigned int)sentPairs.size(),(unsigned int)receivedPairs.size());
}

static void doMulticast(const std::vector<std::string> &cmd)
{
	union {
		uint64_t i[2];
		unsigned char data[2800];
	} pkt;

	if (cmd.size() < 5) {
		doHelp(cmd);
		return;
	}

	uint64_t nwid = Utils::hexStrToU64(cmd[3].c_str());
	unsigned int frameLen = Utils::strToUInt(cmd[4].c_str());
	uint64_t tout = 2000;
	if (cmd.size() >= 6)
		tout = Utils::strToU64(cmd[5].c_str()) * 1000ULL;

	if (frameLen < 16)
		frameLen = 16;
	if (frameLen > 2800)
		frameLen = 2800;

	std::vector<Address> senders;
	if ((cmd[1] == "*")||(cmd[1] == "**")) {
		bool includeSuper = (cmd[1] == "**");
		for(std::map< Address,SimNode * >::iterator n(nodes.begin());n!=nodes.end();++n) {
			if ((includeSuper)||(!n->second->supernode))
				senders.push_back(n->first);
		}
	} else senders.push_back(Address(cmd[1]));

	MAC mcaddr;
	if (cmd[2] == "*")
		mcaddr = MAC(0xff,0xff,0xff,0xff,0xff,0xff);
	else mcaddr.fromString(cmd[2].c_str());

	if (!mcaddr.isMulticast()) {
		printf("---------- %s is not a multicast MAC address"ZT_EOL_S,mcaddr.toString().c_str());
		return;
	}

	for(unsigned int i=0;i<frameLen;++i)
		pkt.data[i] = (unsigned char)prng.next32();

	for(std::vector<Address>::iterator s(senders.begin());s!=senders.end();++s) {
		SimNode *sender = nodes[*s];
		TestEthernetTap *stap = sender->tapFactory.getByNwid(nwid);

		if (stap) {
			pkt.i[0] = s->toInt();
			pkt.i[1] = Utils::now();
			stap->injectPacketFromHost(stap->mac(),mcaddr,0xdead,pkt.data,frameLen);
			printf("%s -> %s etherType 0xdead network %.16llx length %u"ZT_EOL_S,s->toString().c_str(),mcaddr.toString().c_str(),(unsigned long long)nwid,frameLen);
		} else {
			printf("%s -> !%s (sender is not a member of %.16llx)"ZT_EOL_S,s->toString().c_str(),mcaddr.toString().c_str(),(unsigned long long)nwid);
		}
	}

	printf("---------- waiting %llu seconds..."ZT_EOL_S,tout / 1000ULL);

	unsigned int receiveCount = 0;
	TestEthernetTap::TestFrame frame;
	uint64_t toutend = Utils::now() + tout;
	do {
		for(std::map< Address,SimNode * >::iterator nn(nodes.begin());nn!=nodes.end();++nn) {
			SimNode *receiver = nn->second;
			TestEthernetTap *rtap = receiver->tapFactory.getByNwid(nwid);

			if ((rtap)&&(rtap->getNextReceivedFrame(frame,5))) {
				if ((frame.len == frameLen)&&(!memcmp(frame.data + 16,pkt.data + 16,frameLen - 16))) {
					uint64_t ints[2];
					memcpy(ints,frame.data,16);
					printf("%s <- %.10llx received test packet, length == %u, latency == %llums"ZT_EOL_S,nn->first.toString().c_str(),(unsigned long long)ints[0],frame.len,(unsigned long long)(frame.timestamp - ints[1]));
					++receiveCount;
				} else {
					printf("%s !! got spurious packet, length == %u, etherType == 0x%.4x"ZT_EOL_S,nn->first.toString().c_str(),frame.len,frame.etherType);
				}
			}
		}

		Thread::sleep(100);
	} while (Utils::now() < toutend);

	printf("---------- test multicast received by %u peers"ZT_EOL_S,receiveCount);
}

int main(int argc,char **argv)
{
	char linebuf[1024];

	if (argc <= 1) {
		fprintf(stderr,"Usage: %s <base path for temporary node home directories>"ZT_EOL_S,argv[0]);
		return 1;
	}

	basePath = argv[1];
#ifdef __WINDOWS__
	CreateDirectoryA(basePath.c_str(),NULL);
#else
	mkdir(basePath.c_str(),0700);
#endif

	printf("*** ZeroTier One Version %s -- Headless Network Simulator ***"ZT_EOL_S,Node::versionString());
	printf(ZT_EOL_S);

	{
		printf("---------- scanning '%s' for existing network..."ZT_EOL_S,basePath.c_str());
		std::vector<Address> snodes(initSupernodes());
		if (snodes.empty()) {
			printf("---------- no existing network found; use 'mksn' to create one."ZT_EOL_S);
		} else {
			for(std::vector<Address>::iterator a(snodes.begin());a!=snodes.end();++a)
				printf("%s started (supernode)"ZT_EOL_S,a->toString().c_str());
			//printf("---------- root topology is: %s"ZT_EOL_S,rootTopology.c_str());
			std::vector<Address> nodes(scanForNewNodes());
			for(std::vector<Address>::iterator a(nodes.begin());a!=nodes.end();++a)
				printf("%s started (normal peer)"ZT_EOL_S,a->toString().c_str());
			printf("---------- %u peers and %u supernodes loaded!"ZT_EOL_S,(unsigned int)nodes.size(),(unsigned int)snodes.size());
		}
	}
	printf(ZT_EOL_S);

	printf("Type 'help' for help."ZT_EOL_S);
	printf(ZT_EOL_S);

	std::vector<std::string> cmd,prevCmd;
	bool run = true;
	while (run) {
		printf(">> ");
		fflush(stdout);
		if (!fgets(linebuf,sizeof(linebuf),stdin))
			break;

		cmd = Utils::split(linebuf," \r\n\t","\\","\"");

		for(;;) {
			if (cmd.size() == 0)
				break;
			else if (cmd[0] == "quit")
				run = false;
			else if (cmd[0] == "help")
				doHelp(cmd);
			else if (cmd[0] == "mksn")
				doMKSN(cmd);
			else if (cmd[0] == "mkn")
				doMKN(cmd);
			else if (cmd[0] == "list")
				doList(cmd);
			else if (cmd[0] == "join")
				doJoin(cmd);
			else if (cmd[0] == "leave")
				doLeave(cmd);
			else if (cmd[0] == "listnetworks")
				doListNetworks(cmd);
			else if (cmd[0] == "listpeers")
				doListPeers(cmd);
			else if (cmd[0] == "unicast")
				doUnicast(cmd);
			else if (cmd[0] == "multicast")
				doMulticast(cmd);
			else if ((cmd[0] == ".")&&(prevCmd.size() > 0)) {
				cmd = prevCmd;
				continue;
			} else doHelp(cmd);
			break;
		}

		if ((cmd.size() > 0)&&(cmd[0] != "."))
			prevCmd = cmd;
	}

	for(std::map< Address,SimNode * >::iterator n(nodes.begin());n!=nodes.end();++n) {
		printf("%s shutting down..."ZT_EOL_S,n->first.toString().c_str());
		delete n->second;
	}

	return 0;
}
