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

#ifdef ZT_ENABLE_NETCON

#include <algorithm>
#include <utility>
#include <dlfcn.h>
#include <sys/poll.h>

#include "NetconEthernetTap.hpp"

#include "../node/Utils.hpp"
#include "../osdep/OSUtils.hpp"
#include "../osdep/Phy.hpp"

#include "Intercept.h"
#include "LWIPStack.hpp"

#include "lwip/tcp_impl.h"
#include "netif/etharp.h"
#include "lwip/api.h"
#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/ip_frag.h"
#include "lwip/tcp.h"

#include "Common.c"
#include "Sendfd.c"

#define APPLICATION_POLL_FREQ 			20
#define ZT_LWIP_TCP_TIMER_INTERVAL 		5
#define STATUS_TMR_INTERVAL				3000 // How often we check connection statuses

namespace ZeroTier {

// ---------------------------------------------------------------------------

/*
static void clearscreen(){
	fprintf(stderr, "\033[2J");
}
static void gotoxy(int x,int y) {
    fprintf(stderr, "%c[%d;%df",0x1B,y,x);
}
*/

// Gets the process/path name associated with a pid
static void get_path_from_pid(char* dest, int pid)
{
  char ppath[80];
  sprintf(ppath, "/proc/%d/exe", pid);
	if (readlink (ppath, dest, 80) != -1){
  }
}

// Gets the process/path name associated with a fd
/*
static void get_path_from_fd(char* dest, int pid, int fd)
{
	char ppfd[80];
	sprintf(ppfd, "/proc/%d/fd/%d", pid, fd);
	if (readlink (ppfd, dest, 80) != -1){
	}
}
*/

static err_t tapif_init(struct netif *netif)
{
  // Actual init functionality is in addIp() of tap
  return ERR_OK;
}

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  struct pbuf *q;
  char buf[ZT_MAX_MTU+32];
  char *bufptr;
  int tot_len = 0;

  ZeroTier::NetconEthernetTap *tap = (ZeroTier::NetconEthernetTap*)netif->state;

  /* initiate transfer(); */
  bufptr = buf;

  for(q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    /* send data from(q->payload, q->len); */
    memcpy(bufptr, q->payload, q->len);
    bufptr += q->len;
    tot_len += q->len;
  }

  // [Send packet to network]
  // Split ethernet header and feed into handler
  struct eth_hdr *ethhdr;
  ethhdr = (struct eth_hdr *)buf;

  ZeroTier::MAC src_mac;
  ZeroTier::MAC dest_mac;

  src_mac.setTo(ethhdr->src.addr, 6);
  dest_mac.setTo(ethhdr->dest.addr, 6);

  tap->_handler(tap->_arg,tap->_nwid,src_mac,dest_mac,
    Utils::ntoh((uint16_t)ethhdr->type),0,buf + sizeof(struct eth_hdr),tot_len - sizeof(struct eth_hdr));
  return ERR_OK;
}

/*
 * TCP connection administered by service
 */
class TcpConnection
{
public:
  int perceived_fd;
  int their_fd;
  bool pending;
  bool listening;
  int pid;

  unsigned long written;
  unsigned long acked;

  PhySocket *rpcSock;
  PhySocket *dataSock;
  struct tcp_pcb *pcb;

  unsigned char buf[DEFAULT_READ_BUFFER_SIZE];
  int idx;
};

/*
 * A helper class for passing a reference to _phy to LWIP callbacks as a "state"
 */
class Larg
{
public:
  NetconEthernetTap *tap;
  TcpConnection *conn;
  Larg(NetconEthernetTap *_tap, TcpConnection *conn) : tap(_tap), conn(conn) {}
};

// ---------------------------------------------------------------------------

NetconEthernetTap::NetconEthernetTap(
	const char *homePath,
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *friendlyName,
	void (*handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
	void *arg) :
  _nwid(nwid),
	_handler(handler),
	_arg(arg),
  _phy(this,false,true),
  _unixListenSocket((PhySocket *)0),
	_mac(mac),
	_homePath(homePath),
	_mtu(mtu),
	_enabled(true),
	_run(true)
{
	char sockPath[4096],lwipPath[4096];
	rpc_counter = -1;
	Utils::snprintf(sockPath,sizeof(sockPath),"%s%snetcon_service_%.16llx",homePath,ZT_PATH_SEPARATOR_S,(unsigned long long)nwid);
  _dev = sockPath; // in netcon mode, set device to be just the network ID

	Utils::snprintf(lwipPath,sizeof(lwipPath),"%s%sliblwip.so",homePath,ZT_PATH_SEPARATOR_S);
	lwipstack = new LWIPStack(lwipPath);
	if(!lwipstack)
		throw std::runtime_error("unable to dynamically load a new instance of liblwip.so (searched ZeroTier home path)");
	lwipstack->lwip_init();

	_unixListenSocket = _phy.unixListen(sockPath,(void *)this);
	dwr(MSG_INFO, " NetconEthernetTap initialized!\n", _phy.getDescriptor(_unixListenSocket));
	if (!_unixListenSocket)
		throw std::runtime_error(std::string("unable to bind to ")+sockPath);
	_thread = Thread::start(this);
}

NetconEthernetTap::~NetconEthernetTap()
{
	_run = false;
	_phy.whack();
	_phy.whack();
	Thread::join(_thread);
	_phy.close(_unixListenSocket,false);
	delete lwipstack;
}

void NetconEthernetTap::setEnabled(bool en)
{
	_enabled = en;
}

bool NetconEthernetTap::enabled() const
{
	return _enabled;
}

bool NetconEthernetTap::addIp(const InetAddress &ip)
{
	Mutex::Lock _l(_ips_m);
	if (std::find(_ips.begin(),_ips.end(),ip) == _ips.end()) {
		_ips.push_back(ip);
		std::sort(_ips.begin(),_ips.end());

		if (ip.isV4()) {
			// Set IP
			static ip_addr_t ipaddr, netmask, gw;
			IP4_ADDR(&gw,192,168,0,1);
			ipaddr.addr = *((u32_t *)ip.rawIpData());
			netmask.addr = *((u32_t *)ip.netmask().rawIpData());

			// Set up the lwip-netif for LWIP's sake
			lwipstack->netif_add(&interface,&ipaddr, &netmask, &gw, NULL, tapif_init, lwipstack->_ethernet_input);
			interface.state = this;
			interface.output = lwipstack->_etharp_output;
			_mac.copyTo(interface.hwaddr, 6);
			interface.mtu = _mtu;
			interface.name[0] = 't';
			interface.name[1] = 'p';
			interface.linkoutput = low_level_output;
			interface.hwaddr_len = 6;
			interface.flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;
			lwipstack->netif_set_default(&interface);
			lwipstack->netif_set_up(&interface);
		}
	}
	return true;
}

bool NetconEthernetTap::removeIp(const InetAddress &ip)
{
	Mutex::Lock _l(_ips_m);
	std::vector<InetAddress>::iterator i(std::find(_ips.begin(),_ips.end(),ip));
	if (i == _ips.end())
		return false;
	_ips.erase(i);
	if (ip.isV4()) {
		// TODO: dealloc from LWIP
	}
	return true;
}

std::vector<InetAddress> NetconEthernetTap::ips() const
{
	Mutex::Lock _l(_ips_m);
	return _ips;
}

void NetconEthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	struct pbuf *p,*q;
	if (!_enabled)
		return;

	struct eth_hdr ethhdr;
	from.copyTo(ethhdr.src.addr, 6);
	to.copyTo(ethhdr.dest.addr, 6);
	ethhdr.type = Utils::hton((uint16_t)etherType);

	// We allocate a pbuf chain of pbufs from the pool.
	p = lwipstack->pbuf_alloc(PBUF_RAW, len+sizeof(struct eth_hdr), PBUF_POOL);

	if (p != NULL) {
		const char *dataptr = reinterpret_cast<const char *>(data);

		// First pbuf gets ethernet header at start
		q = p;
		if (q->len < sizeof(ethhdr)) {
			dwr(MSG_ERROR, "_put(): Dropped packet: first pbuf smaller than ethernet header\n");
			return;
		}
		memcpy(q->payload,&ethhdr,sizeof(ethhdr));
		memcpy((char*)q->payload + sizeof(ethhdr),dataptr,q->len - sizeof(ethhdr));
		dataptr += q->len - sizeof(ethhdr);

		// Remaining pbufs (if any) get rest of data
		while ((q = q->next)) {
			memcpy(q->payload,dataptr,q->len);
			dataptr += q->len;
		}
	} else {
		dwr(MSG_ERROR, "put(): Dropped packet: no pbufs available\n");
		return;
	}

	{
		Mutex::Lock _l2(lwipstack->_lock);
		if(interface.input(p, &interface) != ERR_OK) {
			dwr(MSG_ERROR, "put(): Error while RXing packet (netif->input)\n");
		}
	}
}

std::string NetconEthernetTap::deviceName() const
{
	return _dev;
}

void NetconEthernetTap::setFriendlyName(const char *friendlyName) {
}

void NetconEthernetTap::scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed)
{
	std::vector<MulticastGroup> newGroups;
	Mutex::Lock _l(_multicastGroups_m);

	// TODO: get multicast subscriptions from LWIP

	std::vector<InetAddress> allIps(ips());
	for(std::vector<InetAddress>::iterator ip(allIps.begin());ip!=allIps.end();++ip)
		newGroups.push_back(MulticastGroup::deriveMulticastGroupForAddressResolution(*ip));

	std::sort(newGroups.begin(),newGroups.end());
	std::unique(newGroups.begin(),newGroups.end());

	for(std::vector<MulticastGroup>::iterator m(newGroups.begin());m!=newGroups.end();++m) {
		if (!std::binary_search(_multicastGroups.begin(),_multicastGroups.end(),*m))
			added.push_back(*m);
	}
	for(std::vector<MulticastGroup>::iterator m(_multicastGroups.begin());m!=_multicastGroups.end();++m) {
		if (!std::binary_search(newGroups.begin(),newGroups.end(),*m))
			removed.push_back(*m);
	}
	_multicastGroups.swap(newGroups);
}

TcpConnection *NetconEthernetTap::getConnectionByTheirFD(PhySocket *sock, int fd)
{
	for(size_t i=0; i<tcp_connections.size(); i++) {
		if(tcp_connections[i]->perceived_fd == fd && tcp_connections[i]->rpcSock == sock)
			return tcp_connections[i];
	}
	return NULL;
}

/*
 * Dumps service state in 80x25 when debug mode is off
 */
void NetconEthernetTap::compact_dump()
{
	/*
	clearscreen();
	gotoxy(0,0);

	fprintf(stderr, "ZeroTier - Network Containers Service [State Dump]\n\r");
	fprintf(stderr, "  RPC Sockets     = %d\n\r", rpc_sockets.size());
	fprintf(stderr, "  TCP Connections = %d\n\r", tcp_connections.size());

	for(size_t i=0; i<rpc_sockets.size(); i++) {
		int rpc_fd = _phy.getDescriptor(rpc_sockets[i]);
		char buf[80];
		int pid = pidmap[rpc_sockets[i]];
		memset(&buf, '\0', 80);
		get_path_from_pid(buf, pid);
		fprintf(stderr, "\n  Client(addr=0x%x, rpc=%d, pid=%d) %s\n", rpc_sockets[i], rpc_fd, pid, buf);
		for(size_t j=0; j<tcp_connections.size(); j++) {
			memset(&buf, '\0', 80);
			get_path_from_pid(buf, tcp_connections[j]->pid);
			if(tcp_connections[j]->rpcSock==rpc_sockets[i]) {
				fprintf(stderr, "\t\tpath\t\t= %s\n", buf);
			}
		}
	}
	*/
	for(size_t i=0; i<rpc_sockets.size(); i++) {
		fprintf(stderr, "\n\n\nrpc(%d)\n", _phy.getDescriptor(rpc_sockets[i]));
		for(size_t j=0; j<tcp_connections.size(); j++) {
			if(_phy.getDescriptor(tcp_connections[j]->rpcSock) == _phy.getDescriptor(rpc_sockets[i]))
				fprintf(stderr, "\t(%d) ----> (%d)\n\n", _phy.getDescriptor(tcp_connections[j]->dataSock), tcp_connections[j]->perceived_fd);
		}
	}
}

/*
 * Dumps service state
 */
void NetconEthernetTap::dump()
{
	fprintf(stderr, "\n\n---\n\ndie(): BEGIN SERVICE STATE DUMP\n");
	fprintf(stderr, "*** IF YOU SEE THIS, EMAIL THE DUMP TEXT TO joseph.henry@zerotier.com ***\n");
	fprintf(stderr, " tcp_conns = %lu, rpc_socks = %lu\n", tcp_connections.size(), rpc_sockets.size());

  	// TODO: Add logic to detect bad mapping conditions
	for(size_t i=0; i<rpc_sockets.size(); i++) {
		for(size_t j=0; j<rpc_sockets.size(); j++) {
			if(j != i && rpc_sockets[i] == rpc_sockets[j]) {
				fprintf(stderr, "Duplicate PhySockets found! (0x%p)\n", rpc_sockets[i]);
			}
		}
	}

	// Dump the state of the service mapping
	for(size_t i=0; i<rpc_sockets.size(); i++) {
		int rpc_fd = _phy.getDescriptor(rpc_sockets[i]);
		char buf[80];
		int pid = pidmap[rpc_sockets[i]];
		get_path_from_pid(buf, pid);

		fprintf(stderr, "\nClient(addr=0x%p, rpc=%d, pid=%d) %s\n", rpc_sockets[i], rpc_fd, pid, buf);
		for(size_t j=0; j<tcp_connections.size(); j++) {
			get_path_from_pid(buf, tcp_connections[j]->pid);
			if(tcp_connections[j]->rpcSock==rpc_sockets[i]){
				fprintf(stderr, "  |\n");
				fprintf(stderr, "  |-Connection(0x%p):\n", tcp_connections[j]);
				fprintf(stderr, "  |      path\t\t\t= %s\n", buf);
				fprintf(stderr, "  |      perceived_fd\t\t= %d\t(fd)\n", tcp_connections[j]->perceived_fd);
				fprintf(stderr, "  |      their_fd\t\t= %d\t(fd)\n", tcp_connections[j]->their_fd);
				fprintf(stderr, "  |      dataSock(0x%p)\t= %d\t(fd)\n", tcp_connections[j]->dataSock, _phy.getDescriptor(tcp_connections[j]->dataSock));
				fprintf(stderr, "  |      rpcSock(0x%p)\t= %d\t(fd)\n", tcp_connections[j]->rpcSock, _phy.getDescriptor(tcp_connections[j]->rpcSock));
				fprintf(stderr, "  |      pending\t\t= %d\n", tcp_connections[j]->pending);
				fprintf(stderr, "  |      listening\t\t= %d\n", tcp_connections[j]->listening);
				fprintf(stderr, "  \\------pcb(0x%p)->state\t= %d\n", tcp_connections[j]->pcb, tcp_connections[j]->pcb->state);
			}
		}
	}
	fprintf(stderr, "\n\ndie(): END SERVICE STATE DUMP\n\n---\n\n");
}

/*
 * Dumps service state and then exits
 */
void NetconEthernetTap::die(int exret) {
	dump();
	exit(exret);
}

/*
 * Closes a TcpConnection and associated LWIP PCB strcuture.
 */
void NetconEthernetTap::closeConnection(TcpConnection *conn)
{
	if(!conn)
		return;
	dwr(MSG_DEBUG, " closeConnection(%x, %d)\n", conn->pcb, _phy.getDescriptor(conn->dataSock));
  	//lwipstack->_tcp_sent(conn->pcb, NULL);
  	//lwipstack->_tcp_recv(conn->pcb, NULL);
  	//lwipstack->_tcp_err(conn->pcb, NULL);
  	//lwipstack->_tcp_poll(conn->pcb, NULL, 0);
	//lwipstack->_tcp_arg(conn->pcb, NULL);
	if(lwipstack->_tcp_close(conn->pcb) != ERR_OK) {
		dwr(MSG_ERROR, " closeConnection(): Error while calling tcp_close()\n");
		exit(0);
	}
	else {
		if(conn->dataSock) {
			close(_phy.getDescriptor(conn->dataSock));
			_phy.close(conn->dataSock,false);
		}
		/* Eventually we might want to use a map here instead */
		for(int i=0; i<tcp_connections.size(); i++) {
			if(tcp_connections[i] == conn) {
				tcp_connections.erase(tcp_connections.begin() + i);
				delete conn;
				break;
			}
		}
	}
}

/*
 * Close a single RPC connection and associated PhySocket
 */
void NetconEthernetTap::closeClient(PhySocket *sock)
{
	for(size_t i=0; i<rpc_sockets.size(); i++) {
		if(rpc_sockets[i] == sock){
			rpc_sockets.erase(rpc_sockets.begin() + i);
			break;
		}
	}
	close(_phy.getDescriptor(sock));
  _phy.close(sock);
}

/*
 * Close all RPC and TCP connections
 */
void NetconEthernetTap::closeAll()
{
	while(rpc_sockets.size())
		closeClient(rpc_sockets.front());
	while(tcp_connections.size())
		closeConnection(tcp_connections.front());
}

#include <sys/resource.h>

void NetconEthernetTap::threadMain()
	throw()
{
	uint64_t prev_tcp_time = 0;
	uint64_t prev_status_time = 0;
	uint64_t prev_etharp_time = 0;

/*
	fprintf(stderr, "- MEM_SIZE = %dM\n", MEM_SIZE / (1024*1024));
	fprintf(stderr, "- PBUF_POOL_SIZE = %d\n", PBUF_POOL_SIZE);
	fprintf(stderr, "- PBUF_POOL_BUFSIZE  = %d\n", PBUF_POOL_BUFSIZE);
	fprintf(stderr, "- MEMP_NUM_PBUF = %d\n", MEMP_NUM_PBUF);
	fprintf(stderr, "- MEMP_NUM_TCP_PCB = %d\n", MEMP_NUM_TCP_PCB);
	fprintf(stderr, "- MEMP_NUM_TCP_PCB_LISTEN = %d\n", MEMP_NUM_TCP_PCB_LISTEN);
	fprintf(stderr, "- MEMP_NUM_TCP_SEG = %d\n\n", MEMP_NUM_TCP_SEG);

	fprintf(stderr, "- TCP_SND_BUF = %dK\n", TCP_SND_BUF / 1024);
	fprintf(stderr, "- TCP_SND_QUEUELEN = %d\n\n", TCP_SND_QUEUELEN);

	fprintf(stderr, "- TCP_WND = %d\n", TCP_WND);
	fprintf(stderr, "- TCP_MSS = %d\n", TCP_MSS);
	fprintf(stderr, "- TCP_MAXRTX = %d\n", TCP_MAXRTX);
	fprintf(stderr, "- IP_REASSEMBLY = %d\n\n", IP_REASSEMBLY);
	fprintf(stderr, "- ARP_TMR_INTERVAL = %d\n", ARP_TMR_INTERVAL);
	fprintf(stderr, "- TCP_TMR_INTERVAL = %d\n", TCP_TMR_INTERVAL);
	fprintf(stderr, "- IP_TMR_INTERVAL  = %d\n", IP_TMR_INTERVAL);
*/

	// Main timer loop
	while (_run) {
		uint64_t now = OSUtils::now();
		uint64_t since_tcp = now - prev_tcp_time;
		uint64_t since_etharp = now - prev_etharp_time;
		uint64_t since_status = now - prev_status_time;
		uint64_t tcp_remaining = ZT_LWIP_TCP_TIMER_INTERVAL;
		uint64_t etharp_remaining = ARP_TMR_INTERVAL;
		uint64_t status_remaining = STATUS_TMR_INTERVAL;

		// Connection prunning
		if (since_status >= STATUS_TMR_INTERVAL) {
			//compact_dump();
			prev_status_time = now;
			status_remaining = STATUS_TMR_INTERVAL - since_status;
			if(rpc_sockets.size() || tcp_connections.size()) {

				// dump();
				// Here we will periodically check the list of rpc_sockets for those that
				// do not currently have any data connection associated with them. If they are
				// unused, then we will try to read from them, if they fail, we can safely assume
				// that the client has closed their end and we can close ours
				for(size_t i = 0; i<tcp_connections.size(); i++) {
					if(tcp_connections[i]->listening) {
						char c;
						if (read(_phy.getDescriptor(tcp_connections[i]->dataSock), &c, 1) < 0) {
							// Still in listening state
						}
						else {
							// Here we should handle the case there there is incoming data (?)
							dwr(MSG_DEBUG, " tap_thread(): Listening socketpair closed. Removing RPC connection (%d)\n",
								_phy.getDescriptor(tcp_connections[i]->dataSock));
							closeConnection(tcp_connections[i]);
						}
					}
				}
			}
			//dwr(4, " tap_thread(): tcp_conns = %d, rpc_socks = %d\n", tcp_connections.size(), rpc_sockets.size());
			for(size_t i=0, associated = 0; i<rpc_sockets.size(); i++, associated = 0) {
				for(size_t j=0; j<tcp_connections.size(); j++) {
					if (tcp_connections[j]->rpcSock == rpc_sockets[i])
						associated++;
				}
				if(!associated){
					// No TCP connections are associated, this is a candidate for removal
					int fd = _phy.getDescriptor(rpc_sockets[i]);
					fcntl(fd, F_SETFL, O_NONBLOCK);
					unsigned char tmpbuf[BUF_SZ];
					int n;
					if((n = read(fd,&tmpbuf,BUF_SZ)) < 0) {
						dwr(MSG_DEBUG, " tap_thread(): closing RPC (%d)\n", _phy.getDescriptor(rpc_sockets[i]));
						closeClient(rpc_sockets[i]);
					}
					// < 0 is failure
					//   0 nothing to read, RPC still active
					// > 0 RPC data read, handle it
					else if (n > 0) {
							// Handle RPC call, this is rare
							dwr(MSG_DEBUG, " tap_thread(): RPC read during connection check (%d bytes)\n", n);
							phyOnUnixData(rpc_sockets[i],_phy.getuptr(rpc_sockets[i]),&tmpbuf,BUF_SZ);
					}
				}
			}
		}
		// Main TCP/ETHARP timer section
		if (since_tcp >= ZT_LWIP_TCP_TIMER_INTERVAL) {
			prev_tcp_time = now;
			lwipstack->tcp_tmr();
		} else {
			tcp_remaining = ZT_LWIP_TCP_TIMER_INTERVAL - since_tcp;
		}
		if (since_etharp >= ARP_TMR_INTERVAL) {
			prev_etharp_time = now;
			lwipstack->etharp_tmr();
		} else {
			etharp_remaining = ARP_TMR_INTERVAL - since_etharp;
		}
		_phy.poll((unsigned long)std::min(tcp_remaining,etharp_remaining));
	}
	closeAll();
	dlclose(lwipstack->_libref);
}

// Unused -- no UDP or TCP from this thread/Phy<>
void NetconEthernetTap::phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len) {}
void NetconEthernetTap::phyOnTcpConnect(PhySocket *sock,void **uptr,bool success) {}
void NetconEthernetTap::phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from) {}
void NetconEthernetTap::phyOnTcpClose(PhySocket *sock,void **uptr) {}
void NetconEthernetTap::phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len) {}
void NetconEthernetTap::phyOnTcpWritable(PhySocket *sock,void **uptr) {}

void NetconEthernetTap::phyOnUnixClose(PhySocket *sock,void **uptr) {
	dwr(MSG_DEBUG, " phyOnUnixClose(sock=0x%x, uptr=0x%x): fd = %d\n", sock, uptr, _phy.getDescriptor(sock));
	TcpConnection *conn = (TcpConnection*)*uptr;
	closeConnection(conn);
}

/*
 * Handles data on a client's data buffer. Data is sent to LWIP to be enqueued.
 */
void NetconEthernetTap::phyOnFileDescriptorActivity(PhySocket *sock,void **uptr,bool readable,bool writable)
{
	if(readable) {
		TcpConnection *conn = (TcpConnection*)*uptr;
		if(conn->dataSock) { // Sometimes a connection may be closed via nc_recved, check first
			lwipstack->_lock.lock();
			handle_write(conn);
			lwipstack->_lock.unlock();
		}
	}
	else {
		dwr(MSG_ERROR, "phyOnFileDescriptorActivity(): PhySocket not readable\n");
	}
}

/*
 * Add a new PhySocket for the client connections
 */
void NetconEthernetTap::phyOnUnixAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN) {
	dwr(MSG_DEBUG, " phyOnUnixAccept(): accepting new connection\n");
	if(find(rpc_sockets.begin(), rpc_sockets.end(), sockN) != rpc_sockets.end()){
		dwr(MSG_ERROR, " phyOnUnixAccept(): SockN (0x%x) already exists!\n", sockN);
		return;
	}
	rpc_sockets.push_back(sockN);
}

/*
 * Processes incoming data on a client-specific RPC connection
 */
void NetconEthernetTap::phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len)
{
	pid_t pid, tid;
	int rpc_count;
	char cmd, timestamp[20];
	void *payload;
	unload_rpc(data, pid, tid, rpc_count, timestamp, cmd, payload);
	dwr(MSG_DEBUG, "\n\nRPC: (pid=%d, tid=%d, rpc_count=%d, timestamp=%s, cmd=%d\n", pid, tid, rpc_count, timestamp, cmd);
	unsigned char *buf = (unsigned char*)data;

	switch(cmd)
	{
		case RPC_SOCKET:
		dwr(MSG_DEBUG, "RPC_SOCKET\n");
		struct socket_st socket_rpc;
		memcpy(&socket_rpc, &buf[IDX_PAYLOAD+1], sizeof(struct socket_st));

	    if(rpc_count==rpc_counter) {
	    	dwr(MSG_ERROR, "Detected repeat RPC.\n");
	    	//return;
	    }
	    else {
	    	rpc_counter = rpc_count;
	    }

		TcpConnection * new_conn;
		if((new_conn = handle_socket(sock, uptr, &socket_rpc))) {
			pidmap[sock] = pid;
			new_conn->pid = pid;
		}
			break;
	  case RPC_LISTEN:
			dwr(MSG_DEBUG, "RPC_LISTEN\n");
	    struct listen_st listen_rpc;
	    memcpy(&listen_rpc,  &buf[IDX_PAYLOAD+1], sizeof(struct listen_st));
	    handle_listen(sock, uptr, &listen_rpc);
			break;
	  case RPC_BIND:
			dwr(MSG_DEBUG, "RPC_BIND\n");
	    struct bind_st bind_rpc;
	    memcpy(&bind_rpc,  &buf[IDX_PAYLOAD+1], sizeof(struct bind_st));
	    handle_bind(sock, uptr, &bind_rpc);
			break;
  	case RPC_CONNECT:
			dwr(MSG_DEBUG, "RPC_CONNECT\n");
	    struct connect_st connect_rpc;
	    memcpy(&connect_rpc,  &buf[IDX_PAYLOAD+1], sizeof(struct connect_st));
	    handle_connect(sock, uptr, &connect_rpc);
			break;
	  case RPC_MAP:
			dwr(MSG_DEBUG, "RPC_MAP (len = %d)\n", len);
			int newfd;
	    	memcpy(&newfd, &buf[IDX_PAYLOAD+1], sizeof(int));
	    	handle_retval(sock, uptr, rpc_count, newfd);
			break;
		case RPC_MAP_REQ:
			dwr(MSG_DEBUG, "RPC_MAP_REQ\n");
			handle_map_request(sock, uptr, buf);
			break;
		default:
			break;
	}
}

/*
 * Send a 'retval' and 'errno' to the client for an RPC over connection->rpcSock
 */
int NetconEthernetTap::send_return_value(TcpConnection *conn, int retval, int _errno = 0)
{
	if(conn) {
		int n = send_return_value(_phy.getDescriptor(conn->rpcSock), retval, _errno);
		if(n > 0)
			conn->pending = false;
		else {
			dwr(MSG_ERROR, " Unable to send return value to the intercept. Closing connection\n");
			closeConnection(conn);
		}
		return n;
	}
	return -1;
}

int NetconEthernetTap::send_return_value(int fd, int retval, int _errno = 0)
{
	dwr(MSG_DEBUG, " send_return_value(): fd = %d, retval = %d, errno = %d\n", fd, retval, _errno);
	int sz = sizeof(char) + sizeof(retval) + sizeof(errno);
	char retmsg[sz];
	memset(&retmsg, '\0', sizeof(retmsg));
	retmsg[0]=RPC_RETVAL;
	memcpy(&retmsg[1], &retval, sizeof(retval));
	memcpy(&retmsg[1]+sizeof(retval), &_errno, sizeof(_errno));
	return write(fd, &retmsg, sz);
}

/*------------------------------------------------------------------------------
--------------------------------- LWIP callbacks -------------------------------
------------------------------------------------------------------------------*/

// NOTE: these are called from within LWIP, meaning that lwipstack->_lock is ALREADY
// locked in this case!

/*
 * Callback from LWIP for when a connection has been accepted and the PCB has been
 * put into an ACCEPT state.
 *
 * A socketpair is created, one end is kept and wrapped into a PhySocket object
 * for use in the main ZT I/O loop, and one end is sent to the client. The client
 * is then required to tell the service what new file descriptor it has allocated
 * for this connection. After the mapping is complete, the accepted socket can be
 * used.
 *
 * @param associated service state object
 * @param newly allocated PCB
 * @param error code
 * @return ERR_OK if everything is ok, -1 otherwise

	 i := should be implemented in intercept lib
	 I := is implemented in intercept lib
	 X := is implemented in service
	 ? := required treatment Unknown
	 - := Not needed

	[ ] EAGAIN or EWOULDBLOCK - The socket is marked nonblocking and no connections are present
													to be accepted. POSIX.1-2001 allows either error to be returned for
													this case, and does not require these constants to have the same value,
													so a portable application should check for both possibilities.
	[I] EBADF - The descriptor is invalid.
	[I] ECONNABORTED - A connection has been aborted.
	[i] EFAULT - The addr argument is not in a writable part of the user address space.
	[-] EINTR - The system call was interrupted by a signal that was caught before a valid connection arrived; see signal(7).
	[I] EINVAL - Socket is not listening for connections, or addrlen is invalid (e.g., is negative).
	[I] EINVAL - (accept4()) invalid value in flags.
	[I] EMFILE - The per-process limit of open file descriptors has been reached.
	[ ] ENFILE - The system limit on the total number of open files has been reached.
	[ ] ENOBUFS, ENOMEM - Not enough free memory. This often means that the memory allocation is
												limited by the socket buffer limits, not by the system memory.
	[I] ENOTSOCK - The descriptor references a file, not a socket.
	[I] EOPNOTSUPP - The referenced socket is not of type SOCK_STREAM.
	[ ] EPROTO - Protocol error.

 *
 */
err_t NetconEthernetTap::nc_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	dwr(MSG_DEBUG, " nc_accept()\n");
	Larg *l = (Larg*)arg;
	TcpConnection *conn = l->conn;
	NetconEthernetTap *tap = l->tap;
	int listening_fd = tap->_phy.getDescriptor(conn->dataSock);

  if(conn) {
		ZT_PHY_SOCKFD_TYPE fds[2];
		if(socketpair(PF_LOCAL, SOCK_STREAM, 0, fds) < 0) {
			if(errno < 0) {
				l->tap->send_return_value(conn, -1, errno);
				dwr(MSG_ERROR, " nc_accept(): unable to create socketpair\n");
				return ERR_MEM;
			}
		}
		TcpConnection *new_tcp_conn = new TcpConnection();
		new_tcp_conn->dataSock = tap->_phy.wrapSocket(fds[0], new_tcp_conn);
		new_tcp_conn->rpcSock = conn->rpcSock;
		new_tcp_conn->pcb = newpcb;
		new_tcp_conn->their_fd = fds[1];
		tap->tcp_connections.push_back(new_tcp_conn);
		dwr(MSG_DEBUG, " nc_accept(): socketpair = {%d, %d}\n", fds[0], fds[1]);
		int n, send_fd = tap->_phy.getDescriptor(conn->rpcSock);
		if((n = send(listening_fd, "z", 1, MSG_NOSIGNAL)) < 0) {
			dwr(MSG_ERROR, " nc_accept(): Error: [send(listening_fd,...) = MSG_NOSIGNAL].\n");
			return -1;
		}
		else if(n > 0) {
			if(sock_fd_write(send_fd, fds[1]) > 0) {
				close(fds[1]); // close other end of socketpair
				new_tcp_conn->pending = true;
			}
			else {
				dwr(MSG_ERROR, " nc_accept(%d): unable to send fd to client\n", listening_fd);
			}
    }
    else {
      dwr(MSG_ERROR, " nc_accept(%d): error writing signal byte (send_fd = %d, perceived_fd = %d)\n", listening_fd, send_fd, fds[1]);
      return -1;
    }
    tap->lwipstack->_tcp_arg(newpcb, new Larg(tap, new_tcp_conn));
    tap->lwipstack->_tcp_recv(newpcb, nc_recved);
    tap->lwipstack->_tcp_err(newpcb, nc_err);
    tap->lwipstack->_tcp_sent(newpcb, nc_sent);
    tap->lwipstack->_tcp_poll(newpcb, nc_poll, 1);
    tcp_accepted(conn->pcb); // Let lwIP know that it can queue additional incoming connections
		return ERR_OK;
  }
  else {
    dwr(MSG_ERROR, " nc_accept(%d): can't locate Connection object for PCB.\n", listening_fd);
  }
  return -1;
}

/*
 * Callback from LWIP for when data is available to be read from the network.
 *
 * Data is in the form of a linked list of struct pbufs, it is then recombined and
 * send to the client over the associated unix socket.
 *
 * @param associated service state object
 * @param allocated PCB
 * @param chain of pbufs
 * @param error code
 * @return ERR_OK if everything is ok, -1 otherwise
 *
 */
err_t NetconEthernetTap::nc_recved(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	dwr(MSG_DEBUG, " nc_recved()\n");
	Larg *l = (Larg*)arg;
	int n;
  struct pbuf* q = p;

  if(!l->conn) {
		dwr(MSG_ERROR, " nc_recved(): no connection object\n");
    return ERR_OK; // ?
  }
  if(p == NULL) {
    if(l->conn) {
			dwr(MSG_INFO, " nc_recved(): closing connection\n");
			l->tap->closeConnection(l->conn);
			return ERR_ABRT;
    }
    else {
      dwr(MSG_ERROR, " nc_recved(): can't locate connection via (arg)\n");
    }
    return err;
  }
  q = p;
  while(p != NULL) { // Cycle through pbufs and write them to the socket
    if(p->len <= 0)
      break; // ?
    if((n = l->tap->_phy.streamSend(l->conn->dataSock,p->payload, p->len)) > 0) {
      if(n < p->len) {
        dwr(MSG_INFO, " nc_recved(): unable to write entire pbuf to buffer\n");
      }
      l->tap->lwipstack->_tcp_recved(tpcb, n); // TODO: would it be more efficient to call this once at the end?
      dwr(MSG_DEBUG, " nc_recved(): wrote %d bytes to (%d)\n", n, l->tap->_phy.getDescriptor(l->conn->dataSock));
    }
    else {
      dwr(MSG_INFO, " nc_recved(): No data written to intercept buffer (%d)\n", l->tap->_phy.getDescriptor(l->conn->dataSock));
    }
    p = p->next;
  }
  l->tap->lwipstack->_pbuf_free(q); // free pbufs
  return ERR_OK;
}

/*
 * Callback from LWIP when an internal error is associtated with the given (arg)
 *
 * Since the PCB related to this error might no longer exist, only its perviously
 * associated (arg) is provided to us.
 *
 * @param associated service state object
 * @param error code
 *
 */
void NetconEthernetTap::nc_err(void *arg, err_t err)
{
	dwr(MSG_DEBUG, "nc_err()\n");
	Larg *l = (Larg*)arg;
	if(!l->conn)
		dwr(MSG_ERROR, "nc_err(): Connection is NULL!\n");

	switch(err)
	{
		case ERR_MEM:
		  dwr(MSG_ERROR, "nc_err(): ERR_MEM->ENOMEM\n");
			l->tap->send_return_value(l->conn, -1, ENOMEM);
			break;
		case ERR_BUF:
			dwr(MSG_ERROR, "nc_err(): ERR_BUF->ENOBUFS\n");
			l->tap->send_return_value(l->conn, -1, ENOBUFS);
			break;
		case ERR_TIMEOUT:
			dwr(MSG_ERROR, "nc_err(): ERR_TIMEOUT->ETIMEDOUT\n");
			l->tap->send_return_value(l->conn, -1, ETIMEDOUT);
			break;
		case ERR_RTE:
			dwr(MSG_ERROR, "nc_err(): ERR_RTE->ENETUNREACH\n");
			l->tap->send_return_value(l->conn, -1, ENETUNREACH);
			break;
		case ERR_INPROGRESS:
			dwr(MSG_ERROR, "nc_err(): ERR_INPROGRESS->EINPROGRESS\n");
			l->tap->send_return_value(l->conn, -1, EINPROGRESS);
			break;
		case ERR_VAL:
			dwr(MSG_ERROR, "nc_err(): ERR_VAL->EINVAL\n");
			l->tap->send_return_value(l->conn, -1, EINVAL);
			break;
		case ERR_WOULDBLOCK:
			dwr(MSG_ERROR, "nc_err(): ERR_WOULDBLOCK->EWOULDBLOCK\n");
			l->tap->send_return_value(l->conn, -1, EWOULDBLOCK);
			break;
		case ERR_USE:
			dwr(MSG_ERROR, "nc_err(): ERR_USE->EADDRINUSE\n");
			l->tap->send_return_value(l->conn, -1, EADDRINUSE);
			break;
		case ERR_ISCONN:
			dwr(MSG_ERROR, "nc_err(): ERR_ISCONN->EISCONN\n");
			l->tap->send_return_value(l->conn, -1, EISCONN);
			break;
		case ERR_ABRT:
			dwr(MSG_ERROR, "nc_err(): ERR_ABRT->ECONNREFUSED\n");
			l->tap->send_return_value(l->conn, -1, ECONNREFUSED);
			break;

			// FIXME: Below are errors which don't have a standard errno correlate

		case ERR_RST:
			l->tap->send_return_value(l->conn, -1, -1);
			break;
		case ERR_CLSD:
			l->tap->send_return_value(l->conn, -1, -1);
			break;
		case ERR_CONN:
			l->tap->send_return_value(l->conn, -1, -1);
			break;
		case ERR_ARG:
			l->tap->send_return_value(l->conn, -1, -1);
			break;
		case ERR_IF:
			l->tap->send_return_value(l->conn, -1, -1);
			break;
		default:
			break;
	}
	dwr(MSG_ERROR, "nc_err(): closing connection\n");
  l->tap->closeConnection(l->conn);
}

/*
 * Callback from LWIP to do whatever work we might need to do.
 *
 * @param associated service state object
 * @param PCB we're polling on
 * @return ERR_OK if everything is ok, -1 otherwise
 *
 */
err_t NetconEthernetTap::nc_poll(void* arg, struct tcp_pcb *tpcb)
{
	//Larg *l = (Larg*)arg;
	/*
	Larg *l = (Larg*)arg;
	TcpConnection *conn = l->conn;
	NetconEthernetTap *tap = l->tap;
	if(conn && conn->idx) // if valid connection and non-zero index (indicating data present)
		tap->handle_write(conn);
	*/
	return ERR_OK;
}

/*
 * Callback from LWIP to signal that 'len' bytes have successfully been sent.
 * As a result, we should put our socket back into a notify-on-readability state
 * since there is now room on the PCB buffer to write to.
 *
 * NOTE: This could be used to track the amount of data sent by a connection.
 *
 * @param associated service state object
 * @param relevant PCB
 * @param length of data sent
 * @return ERR_OK if everything is ok, -1 otherwise
 *
 */
err_t NetconEthernetTap::nc_sent(void* arg, struct tcp_pcb *tpcb, u16_t len)
{
	//dwr(5, " nc_sent()\n");
	Larg *l = (Larg*)arg;
	if(len) {
		l->conn->acked+=len;
		//dwr("W = %d, A = %d\n", l->conn->written, l->conn->acked);
		//dwr("ACK = %d\n", len);
		l->tap->_phy.setNotifyReadable(l->conn->dataSock, true);
		l->tap->_phy.whack();
	}
	return ERR_OK;
}

/*
 * Callback from LWIP which sends a return value to the client to signal that
 * a connection was established for this PCB
 *
 * @param associated service state object
 * @param relevant PCB
 * @param error code
 * @return ERR_OK if everything is ok, -1 otherwise
 *
 */
err_t NetconEthernetTap::nc_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
	dwr(MSG_DEBUG, " nc_connected()\n");
	Larg *l = (Larg*)arg;
	l->tap->send_return_value(l->conn, ERR_OK);
	return ERR_OK;
}

/*------------------------------------------------------------------------------
----------------------------- RPC Handler functions ----------------------------
------------------------------------------------------------------------------*/

/* Unpacks the buffer from an RPC command */
void NetconEthernetTap::unload_rpc(void *data, pid_t &pid, pid_t &tid, int &rpc_count, char (timestamp[20]), char &cmd, void* &payload)
{
	unsigned char *buf = (unsigned char*)data;
	memcpy(&pid, 			&buf[IDX_PID], 		sizeof(pid_t));
	memcpy(&tid, 			&buf[IDX_TID], 		sizeof(pid_t));
	memcpy(&rpc_count, 	&buf[IDX_COUNT], 		sizeof(int));
	memcpy(timestamp, 	&buf[IDX_TIME], 		20);
	memcpy(&cmd, 			&buf[IDX_PAYLOAD], 	sizeof(char));
}

/*
	Responds to a request from the [intercept] to determine whether a local socket is
	mapped to this service. In other words, how do the intercept's overridden calls
	tell the difference between regular AF_LOCAL sockets and one of our socketpairs
	that is used to communicate over the network?
*/
void NetconEthernetTap::handle_map_request(PhySocket *sock, void **uptr, unsigned char* buf)
{
	dwr(4, " handle_map_request()\n");
	TcpConnection *conn = (TcpConnection*)*uptr;
	int req_fd;
	memcpy(&req_fd, &buf[IDX_PAYLOAD+1], sizeof(req_fd));
	for(size_t i=0; i<tcp_connections.size(); i++) {
		if(tcp_connections[i]->rpcSock == conn->rpcSock && tcp_connections[i]->perceived_fd == req_fd){
			send_return_value(conn, 1, ERR_OK); // True
			dwr(MSG_DEBUG, " handle_map_request(their=%d): MAPPED (to %d)\n", req_fd,
				_phy.getDescriptor(tcp_connections[i]->dataSock));
			return;
		}
	}
	send_return_value(conn, 0, ERR_OK); // False
	dwr(MSG_DEBUG, " handle_map_request(their=%d): NOT MAPPED\n", req_fd);
}

/**
 * Handles a return value (client's perceived fd) and completes a mapping
 * so that we know what connection an RPC call should be associated with.
 *
 * @param PhySocket associated with this RPC connection
 * @param structure containing the data and parameters for this client's RPC
 *
 */
void NetconEthernetTap::handle_retval(PhySocket *sock, void **uptr, int rpc_count, int newfd)
{
	dwr(MSG_DEBUG, " handle_retval()\n");
	TcpConnection *conn = (TcpConnection*)*uptr;
	if(!conn->pending)
		return;
	conn->pending = false;
    conn->perceived_fd = newfd;
    if(rpc_count==rpc_counter) {
    	dwr(MSG_ERROR, " handle_retval(): Detected repeat RPC.\n");
    	//return;
    }
    else
    	rpc_counter = rpc_count;

	dwr(MSG_DEBUG, " handle_retval(): CONN:%x - Mapping [our=%d -> their=%d]\n",conn,
	_phy.getDescriptor(conn->dataSock), conn->perceived_fd);

	/* Check for pre-existing connection for this socket ---
		This block is in response to interesting behaviour from redis-server. A
		socket is created, setsockopt is called and the socket is set to IPV6 but fails (for now),
		then it is closed and re-opened and consequently remapped. With two pipes mapped
		to the same socket, makes it possible that we write to the wrong pipe and fail. So
		this block merely searches for a possible duplicate mapping and erases it
	*/
	for(size_t i=0; i<tcp_connections.size(); i++) {
		if(tcp_connections[i] == conn)
			continue;
		if(tcp_connections[i]->rpcSock == conn->rpcSock) {
			if(tcp_connections[i]->perceived_fd == conn->perceived_fd) {
				int n;
				if((n = send(_phy.getDescriptor(tcp_connections[i]->dataSock), "z", 1, MSG_NOSIGNAL)) < 0) {
					dwr(MSG_DEBUG, " handle_retval(): CONN:%x - Socket (%d) already mapped (originally CONN:%x)\n", conn, tcp_connections[i]->perceived_fd, tcp_connections[i]);
					closeConnection(tcp_connections[i]);
				}
				else {
					dwr(MSG_ERROR, " handle_retval(): CONN:%x - This socket is mapped to two different pipes (?). Exiting.\n", conn);
					//die(0); // FIXME: Print service mapping state and exit
				}
			}
		}
	}
}

/*
 * Handles an RPC to bind an LWIP PCB to a given address and port
 *
 * @param PhySocket associated with this RPC connection
 * @param structure containing the data and parameters for this client's RPC
 *

 i := should be implemented in intercept lib
 I := is implemented in intercept lib
 X := is implemented in service
 ? := required treatment Unknown
 - := Not needed

	[ ]	EACCES - The address is protected, and the user is not the superuser.
	[X]	EADDRINUSE - The given address is already in use.
	[I]	EBADF - sockfd is not a valid descriptor.
	[X]	EINVAL - The socket is already bound to an address.
	[I]	ENOTSOCK - sockfd is a descriptor for a file, not a socket.

	[X]	ENOMEM - Insufficient kernel memory was available.

	  - The following errors are specific to UNIX domain (AF_UNIX) sockets:

	[-]	EACCES - Search permission is denied on a component of the path prefix. (See also path_resolution(7).)
	[-]	EADDRNOTAVAIL - A nonexistent interface was requested or the requested address was not local.
	[-]	EFAULT - addr points outside the user's accessible address space.
	[-]	EINVAL - The addrlen is wrong, or the socket was not in the AF_UNIX family.
	[-]	ELOOP - Too many symbolic links were encountered in resolving addr.
	[-]	ENAMETOOLONG - s addr is too long.
	[-]	ENOENT - The file does not exist.
	[-]	ENOTDIR - A component of the path prefix is not a directory.
	[-]	EROFS - The socket inode would reside on a read-only file system.

 */
void NetconEthernetTap::handle_bind(PhySocket *sock, void **uptr, struct bind_st *bind_rpc)
{

  struct sockaddr_in *connaddr;
  connaddr = (struct sockaddr_in *) &bind_rpc->addr;
  int conn_port = lwipstack->ntohs(connaddr->sin_port);
  ip_addr_t conn_addr;
	conn_addr.addr = *((u32_t *)_ips[0].rawIpData());
	TcpConnection *conn = getConnectionByTheirFD(sock, bind_rpc->sockfd);

  dwr(MSG_DEBUG, " handle_bind(%d)\n", bind_rpc->sockfd);

  if(conn) {
    if(conn->pcb->state == CLOSED){
      int err = lwipstack->tcp_bind(conn->pcb, &conn_addr, conn_port);
			if(err != ERR_OK) {
				int ip = connaddr->sin_addr.s_addr;
				unsigned char d[4];
				d[0] = ip & 0xFF;
				d[1] = (ip >>  8) & 0xFF;
				d[2] = (ip >> 16) & 0xFF;
				d[3] = (ip >> 24) & 0xFF;
				dwr(MSG_ERROR, " handle_bind(): error binding to %d.%d.%d.%d : %d\n", d[0],d[1],d[2],d[3], conn_port);
				dwr(MSG_ERROR, " handle_bind(): err = %d\n", err);

				if(err == ERR_USE)
					send_return_value(conn, -1, EADDRINUSE);
				if(err == ERR_MEM)
					send_return_value(conn, -1, ENOMEM);
				if(err == ERR_BUF)
					send_return_value(conn, -1, ENOMEM); // FIXME: Closest match
			}
			else
				send_return_value(conn, ERR_OK, ERR_OK); // Success
    }
    else {
			dwr(MSG_ERROR, " handle_bind(): PCB (%x) not in CLOSED state. Ignoring BIND request.\n", conn->pcb);
			send_return_value(conn, -1, EINVAL);
		}
  }
  else {
		dwr(MSG_ERROR, " handle_bind(): can't locate connection for PCB\n");
		send_return_value(conn, -1, EBADF);
	}
}

/*
 * Handles an RPC to put an LWIP PCB into LISTEN mode
 *
 * @param PhySocket associated with this RPC connection
 * @param structure containing the data and parameters for this client's RPC
 *

 i := should be implemented in intercept lib
 I := is implemented in intercept lib
 X := is implemented in service
 ? := required treatment Unknown
 - := Not needed

[?] EADDRINUSE - Another socket is already listening on the same port.
[IX] EBADF - The argument sockfd is not a valid descriptor.
[I] ENOTSOCK - The argument sockfd is not a socket.
[I] EOPNOTSUPP - The socket is not of a type that supports the listen() operation.

 */
void NetconEthernetTap::handle_listen(PhySocket *sock, void **uptr, struct listen_st *listen_rpc)
{
	dwr(3, " handle_listen(their=%d):\n", listen_rpc->sockfd);
	TcpConnection *conn = getConnectionByTheirFD(sock, listen_rpc->sockfd);
	if(!conn){
		dwr(MSG_ERROR, " handle_listen(): unable to locate connection object\n");
		// ? send_return_value(conn, -1, EBADF);
		return;
	}
	dwr(3, " handle_listen(our=%d -> their=%d)\n", _phy.getDescriptor(conn->dataSock), conn->perceived_fd);

  if(conn->pcb->state == LISTEN) {
    dwr(MSG_ERROR, " handle_listen(): PCB is already in listening state.\n");
    return;
  }
	struct tcp_pcb* listening_pcb;

#ifdef TCP_LISTEN_BACKLOG
		listening_pcb = lwipstack->tcp_listen_with_backlog(conn->pcb, listen_rpc->backlog);
#else
		listening_pcb = lwipstack->tcp_listen(conn->pcb);
#endif

  if(listening_pcb != NULL) {
    conn->pcb = listening_pcb;
    lwipstack->tcp_accept(listening_pcb, nc_accept);
		lwipstack->tcp_arg(listening_pcb, new Larg(this, conn));
		/* we need to wait for the client to send us the fd allocated on their end
		for this listening socket */
		fcntl(_phy.getDescriptor(conn->dataSock), F_SETFL, O_NONBLOCK);
		conn->listening = true;
		conn->pending = true;
		send_return_value(conn, ERR_OK, ERR_OK);
  }
  else {
		/*
		dwr"handle_listen(): unable to allocate memory for new listening PCB\n");
		 // FIXME: This does not have an equivalent errno value
		 // lwip will reclaim space with a tcp_listen call since a PCB in a LISTEN
		 // state takes up less space. If something goes wrong during the creation of a
		 // new listening socket we should return an error that implies we can't use this
		 // socket, even if the reason isn't describing what really happened internally.
		 // See: http://lwip.wikia.com/wiki/Raw/TCP
		send_return_value(conn, -1, EBADF);
  	*/
	}
}

/*
 * Handles an RPC to create a socket (LWIP PCB and associated socketpair)
 *
 * A socketpair is created, one end is kept and wrapped into a PhySocket object
 * for use in the main ZT I/O loop, and one end is sent to the client. The client
 * is then required to tell the service what new file descriptor it has allocated
 * for this connection. After the mapping is complete, the socket can be used.
 *
 * @param PhySocket associated with this RPC connection
 * @param structure containing the data and parameters for this client's RPC
 *

 i := should be implemented in intercept lib
 I := is implemented in intercept lib
 X := is implemented in service
 ? := required treatment Unknown
 - := Not needed

	[-] EACCES - Permission to create a socket of the specified type and/or protocol is denied.
  [I] EAFNOSUPPORT - The implementation does not support the specified address family.
  [I] EINVAL - Unknown protocol, or protocol family not available.
  [I] EINVAL - Invalid flags in type.
  [I] EMFILE - Process file table overflow.
  [?] ENFILE - The system limit on the total number of open files has been reached.
  [X] ENOBUFS or ENOMEM - Insufficient memory is available.  The socket cannot be created until sufficient resources are freed.
  [?] EPROTONOSUPPORT - The protocol type or the specified protocol is not supported within this domain.

 */
TcpConnection * NetconEthernetTap::handle_socket(PhySocket *sock, void **uptr, struct socket_st* socket_rpc)
{
	int rpc_fd = _phy.getDescriptor(sock);
	struct tcp_pcb *newpcb = lwipstack->tcp_new();
	dwr(MSG_DEBUG, " handle_socket(): pcb=%x\n", newpcb);
  	if(newpcb != NULL) {
		ZT_PHY_SOCKFD_TYPE fds[2];
		if(socketpair(PF_LOCAL, SOCK_STREAM, 0, fds) < 0) {
			if(errno < 0) {
				send_return_value(rpc_fd, -1, errno);
				return NULL;
			}
		}
		dwr(MSG_DEBUG, " handle_socket(): socketpair = {%d, %d}\n", fds[0], fds[1]);
		TcpConnection *new_conn = new TcpConnection();
		new_conn->dataSock = _phy.wrapSocket(fds[0], new_conn);
		*uptr = new_conn;
		new_conn->rpcSock = sock;
		new_conn->pcb = newpcb;
	    new_conn->their_fd = fds[1];
		tcp_connections.push_back(new_conn);
    	sock_fd_write(_phy.getDescriptor(sock), fds[1]);
		close(fds[1]); // close other end of socketpair
		// Once the client tells us what its fd is on the other end, we can then complete the mapping
    	new_conn->pending = true;
		return new_conn;
  }
  else {
		sock_fd_write(rpc_fd, -1); // Send a bad fd, to signal error
    dwr(MSG_ERROR, " handle_socket(): Memory not available for new PCB\n");
		send_return_value(rpc_fd, -1, ENOMEM);
		return NULL;
  }
}

/*
 * Handles an RPC to connect to a given address and port
 *
 * @param PhySocket associated with this RPC connection
 * @param structure containing the data and parameters for this client's RPC

	--- Error handling in this method will only catch problems which are immedately
	    apprent. Some errors will need to be caught in the nc_connected(0 callback

	 i := should be implemented in intercept lib
 	 I := is implemented in intercept lib
 	 X := is implemented in service
 	 ? := required treatment Unknown
 	 - := Not needed

	[-] EACCES - For UNIX domain sockets, which are identified by pathname: Write permission is denied ...
	[?] EACCES, EPERM - The user tried to connect to a broadcast address without having the socket broadcast flag enabled ...
	[X] EADDRINUSE - Local address is already in use.
	[I] EAFNOSUPPORT - The passed address didn't have the correct address family in its sa_family field.
	[X] EAGAIN - No more free local ports or insufficient entries in the routing cache.
	[ ] EALREADY - The socket is nonblocking and a previous connection attempt has not yet been completed.
	[IX] EBADF - The file descriptor is not a valid index in the descriptor table.
	[ ] ECONNREFUSED - No-one listening on the remote address.
	[i] EFAULT - The socket structure address is outside the user's address space.
	[ ] EINPROGRESS - The socket is nonblocking and the connection cannot be completed immediately.
	[-] EINTR - The system call was interrupted by a signal that was caught.
	[X] EISCONN - The socket is already connected.
	[X] ENETUNREACH - Network is unreachable.
	[I] ENOTSOCK - The file descriptor is not associated with a socket.
	[X] ETIMEDOUT - Timeout while attempting connection.

	[X] EINVAL - Invalid argument, SVr4, generally makes sense to set this

 *
 */
void NetconEthernetTap::handle_connect(PhySocket *sock, void **uptr, struct connect_st* connect_rpc)
{
	dwr(MSG_DEBUG, " handle_connect()\n");
	TcpConnection *conn = (TcpConnection*)*uptr;
	struct sockaddr_in *connaddr;
	connaddr = (struct sockaddr_in *) &connect_rpc->__addr;
	int conn_port = lwipstack->ntohs(connaddr->sin_port);
	ip_addr_t conn_addr = convert_ip((struct sockaddr_in *)&connect_rpc->__addr);

	if(conn != NULL) {
		lwipstack->tcp_sent(conn->pcb, nc_sent);
		lwipstack->tcp_recv(conn->pcb, nc_recved);
		lwipstack->tcp_err(conn->pcb, nc_err);
		lwipstack->tcp_poll(conn->pcb, nc_poll, APPLICATION_POLL_FREQ);
		lwipstack->tcp_arg(conn->pcb, new Larg(this, conn));

		int err = 0;
		if((err = lwipstack->tcp_connect(conn->pcb,&conn_addr,conn_port, nc_connected)) < 0)
		{
			if(err == ERR_ISCONN) {
				send_return_value(conn, -1, EISCONN); // Already in connected state
				return;
			}
			if(err == ERR_USE) {
				send_return_value(conn, -1, EADDRINUSE); // Already in use
				return;
			}
			if(err == ERR_VAL) {
				send_return_value(conn, -1, EINVAL); // Invalid ipaddress parameter
				return;
			}
			if(err == ERR_RTE) {
				send_return_value(conn, -1, ENETUNREACH); // No route to host
				return;
			}
			if(err == ERR_BUF) {
				send_return_value(conn, -1, EAGAIN); // No more ports available
				return;
			}
			if(err == ERR_MEM)
			{
				/* Can occur for the following reasons: tcp_enqueue_flags()

				1) tcp_enqueue_flags is always called with either SYN or FIN in flags.
				  We need one available snd_buf byte to do that.
				  This means we can't send FIN while snd_buf==0. A better fix would be to
				  not include SYN and FIN sequence numbers in the snd_buf count.

				2) Cannot allocate new pbuf
				3) Cannot allocate new TCP segment

				*/
				send_return_value(conn, -1, EAGAIN); // FIXME: Doesn't describe the problem well, but closest match
				return;
			}

			// We should only return a value if failure happens immediately
			// Otherwise, we still need to wait for a callback from lwIP.
			// - This is because an ERR_OK from tcp_connect() only verifies
			//   that the SYN packet was enqueued onto the stack properly,
			//   that's it!
			// - Most instances of a retval for a connect() should happen
			//   in the nc_connect() and nc_err() callbacks!
			dwr(MSG_ERROR, " handle_connect(): unable to connect\n");
			send_return_value(conn, -1, EAGAIN);
		}
		// Everything seems to be ok, but we don't have enough info to retval
		conn->pending=true;
	}
	else {
		dwr(MSG_ERROR, " handle_connect(): could not locate PCB based on their fd\n");
		send_return_value(conn, -1, EBADF);
	}
}


void NetconEthernetTap::handle_write(TcpConnection *conn)
{
	//dwr(MSG_DEBUG, " handle_write()\n");
	float max = (float)TCP_SND_BUF;
	int r;

	if(!conn) {
		dwr(MSG_ERROR, " handle_write(): could not locate connection for this fd\n");
		return;
	}
	if(conn->idx < max) {
		if(!conn->pcb) {
			dwr(MSG_ERROR, " handle_write(): conn->pcb == NULL. Failed to write.\n");
			return;
		}
		int sndbuf = conn->pcb->snd_buf; // How much we are currently allowed to write to the connection
		/* PCB send buffer is full,turn off readability notifications for the
		corresponding PhySocket until nc_sent() is called and confirms that there is
		now space on the buffer */
		if(sndbuf == 0) {
			_phy.setNotifyReadable(conn->dataSock, false);
			return;
		}
		if(!conn->listening)
			lwipstack->_tcp_output(conn->pcb);

		if(conn->dataSock) {
			int read_fd = _phy.getDescriptor(conn->dataSock);
			if((r = read(read_fd, (&conn->buf)+conn->idx, sndbuf)) > 0) {
				conn->idx += r;
				/* Writes data pulled from the client's socket buffer to LWIP. This merely sends the
				 * data to LWIP to be enqueued and eventually sent to the network. */
				if(r > 0) {
					int sz;
					// NOTE: this assumes that lwipstack->_lock is locked, either
					// because we are in a callback or have locked it manually.
					int err = lwipstack->_tcp_write(conn->pcb, &conn->buf, r, TCP_WRITE_FLAG_COPY);
					lwipstack->_tcp_output(conn->pcb);
					if(err != ERR_OK) {
						dwr(MSG_ERROR, " handle_write(): error while writing to PCB, (err = %d)\n", err);
						return;
					}
					else {
						sz = (conn->idx)-r;
						if(sz) {
							memmove(&conn->buf, (conn->buf+r), sz);
						}
						conn->idx -= r;
						conn->written+=r;
						return;
					}
				}
				else {
					dwr(MSG_INFO, " handle_write(): LWIP stack full\n");
					return;
				}
			}
		}
	}
}

} // namespace ZeroTier

#endif // ZT_ENABLE_NETCON
