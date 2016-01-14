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

#include <algorithm>
#include <utility>
#include <dlfcn.h>
#include <sys/poll.h>
#include <stdint.h>
#include <utility>
#include <string>
#include <sys/resource.h>

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

#include "common.inc.c"
#include "RPC.h"

#define APPLICATION_POLL_FREQ 			2
#define ZT_LWIP_TCP_TIMER_INTERVAL 		5
#define STATUS_TMR_INTERVAL				1000 // How often we check connection statuses (in ms)

#define DEFAULT_READ_BUFFER_SIZE   1024 * 1024 * 2


namespace ZeroTier {

// ---------------------------------------------------------------------------

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

  bool pending, listening;
  int pid, idx;
  unsigned long written, acked;

  PhySocket *rpcsock;
  PhySocket *sock;
  struct tcp_pcb *pcb;
  struct sockaddr_storage *addr;
  unsigned char buf[DEFAULT_READ_BUFFER_SIZE];
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
	rcqidx = 0;
	Utils::snprintf(sockPath,sizeof(sockPath),"%s%snc_%.16llx",homePath,ZT_PATH_SEPARATOR_S,_nwid,ZT_PATH_SEPARATOR_S,(unsigned long long)nwid);
  _dev = sockPath; // in netcon mode, set device to be just the network ID

	Utils::snprintf(lwipPath,sizeof(lwipPath),"%s%sliblwip.so",homePath,ZT_PATH_SEPARATOR_S);
	lwipstack = new LWIPStack(lwipPath);
	if(!lwipstack)
		throw std::runtime_error("unable to dynamically load a new instance of liblwip.so (searched ZeroTier home path)");
	lwipstack->lwip_init();

	_unixListenSocket = _phy.unixListen(sockPath,(void *)this);
	dwr(MSG_INFO," NetconEthernetTap initialized!\n", _phy.getDescriptor(_unixListenSocket));
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
			dwr(MSG_ERROR,"_put(): Dropped packet: first pbuf smaller than ethernet header\n");
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
		dwr(MSG_ERROR,"put(): Dropped packet: no pbufs available\n");
		return;
	}

	{
		Mutex::Lock _l2(lwipstack->_lock);
		if(interface.input(p, &interface) != ERR_OK) {
			dwr(MSG_ERROR,"put(): Error while RXing packet (netif->input)\n");
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

void NetconEthernetTap::threadMain()
	throw()
{
	uint64_t prev_tcp_time = 0;
	uint64_t prev_status_time = 0;
	uint64_t prev_etharp_time = 0;

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

			prev_status_time = now;
			status_remaining = STATUS_TMR_INTERVAL - since_status;


			//dwr(MSG_DEBUG," tap_thread(): tcp\\jobs = {%d, %d}\n", tcp_connections.size(), jobmap.size());
			for(size_t i=0; i<tcp_connections.size(); i++) {

				// No TCP connections are associated, this is a candidate for removal
				if(!tcp_connections[i]->sock)
					continue; // Skip, this is a pending connection
				int fd = _phy.getDescriptor(tcp_connections[i]->sock);

				fcntl(fd, F_SETFL, O_NONBLOCK);
				unsigned char tmpbuf[BUF_SZ];
				int n = read(fd,&tmpbuf,BUF_SZ);
				//dwr(MSG_DEBUG,"  tap_thread(): <%x> conn->idx = %d\n", tcp_connections[i]->sock, tcp_connections[i]->idx);
				if(tcp_connections[i]->pcb->state == SYN_SENT) {
					dwr(MSG_DEBUG,"  tap_thread(): <%x> state = SYN_SENT, candidate for removal\n", tcp_connections[i]->sock);
				}
				if((n < 0 && errno != EAGAIN) || (n == 0 && errno == EAGAIN)) {
					dwr(MSG_DEBUG," tap_thread(): closing sock (%x)\n", tcp_connections[i]->sock);
					closeConnection(tcp_connections[i]->sock);
				}
				else if (n > 0) {
					dwr(MSG_DEBUG," tap_thread(): data read during connection check (%d bytes)\n", n);
					phyOnUnixData(tcp_connections[i]->sock,_phy.getuptr(tcp_connections[i]->sock),&tmpbuf,BUF_SZ);
				}				
			}

		}
		// Main TCP/ETHARP timer section
		if (since_tcp >= ZT_LWIP_TCP_TIMER_INTERVAL) {
			prev_tcp_time = now;
			lwipstack->tcp_tmr();

			// Makeshift poll
			for(size_t i=0; i<tcp_connections.size(); i++) {
				if(tcp_connections[i]->idx > 0){
					lwipstack->_lock.lock();
					handle_write(tcp_connections[i]);
					lwipstack->_lock.unlock();
				}
			}

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
	dlclose(lwipstack->_libref);
}

// Unused -- no UDP or TCP from this thread/Phy<>
void NetconEthernetTap::phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len) {}
void NetconEthernetTap::phyOnTcpConnect(PhySocket *sock,void **uptr,bool success) {}
void NetconEthernetTap::phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from) {}
void NetconEthernetTap::phyOnTcpClose(PhySocket *sock,void **uptr) {}
void NetconEthernetTap::phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len) {}
void NetconEthernetTap::phyOnTcpWritable(PhySocket *sock,void **uptr) {}

void NetconEthernetTap::addConnection(TcpConnection *conn)
{
	Mutex::Lock _l(_tcpconns_m);
	tcp_connections.push_back(conn);
}

void NetconEthernetTap::removeConnection(TcpConnection *conn)
{
	Mutex::Lock _l(_tcpconns_m);
	for(size_t i=0; i<tcp_connections.size(); i++) {
		if(tcp_connections[i] == conn){
			tcp_connections.erase(tcp_connections.begin() + i);
			return;
		}
	}
}

TcpConnection *NetconEthernetTap::getConnection(PhySocket *sock)
{
	Mutex::Lock _l(_tcpconns_m);
	for(size_t i=0; i<tcp_connections.size(); i++) {
		if(tcp_connections[i]->sock == sock){
			return tcp_connections[i];
		}
	}
	return NULL;
}

/*
 * Closes a TcpConnection and associated LWIP PCB strcuture.
 */
void NetconEthernetTap::closeConnection(PhySocket *sock)
{
	dwr(MSG_DEBUG,"closeConnection(%x)\n",sock);
	if(!sock) {
		dwr(MSG_DEBUG," closeConnection(): invalid PhySocket\n");
		return;
	}
	TcpConnection *conn = getConnection(sock);
	if(!conn)
		return;
	else
		removeConnection(conn);
	if(!conn->pcb)
		return;
	if(conn->pcb->state == SYN_SENT) {
		dwr(MSG_DEBUG," closeConnection(): invalid PCB state (SYN_SENT) -- cannot close right now\n");
		return;
	}	
	dwr(MSG_DEBUG," closeConnection(): PCB->state = %d\n", conn->pcb->state);
	if(lwipstack->_tcp_close(conn->pcb) != ERR_OK) {
		dwr(MSG_ERROR," closeConnection(): Error while calling tcp_close()\n");
	}
	if(!sock)
		return;
	close(_phy.getDescriptor(sock)); // close underlying fd
	_phy.close(sock, false); // close PhySocket
}

void NetconEthernetTap::phyOnUnixClose(PhySocket *sock,void **uptr) {
	dwr(MSG_DEBUG,"\nphyOnUnixClose(): close connection = %x\n", sock);
	closeConnection(sock);
}

/*
 * Handles data on a client's data buffer. Data is sent to LWIP to be enqueued.
 */
void NetconEthernetTap::phyOnFileDescriptorActivity(PhySocket *sock,void **uptr,bool readable,bool writable) {
	dwr(MSG_DEBUG,"\nphyOnFileDescriptorActivity(): new connection = %x\n", sock);
}

/*
 * Add a new PhySocket for the client connections
 */
void NetconEthernetTap::phyOnUnixAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN) {
	dwr(MSG_DEBUG,"\nphyOnUnixAccept(): new connection = %x\n", sockN);
}

/* Unpacks the buffer from an RPC command */
void NetconEthernetTap::unload_rpc(void *data, pid_t &pid, pid_t &tid, 
	int &rpc_count, char (timestamp[20]), char (CANARY[sizeof(uint64_t)]), char &cmd, void* &payload)
{
	unsigned char *buf = (unsigned char*)data;
	memcpy(&pid, &buf[IDX_PID], sizeof(pid_t));
	memcpy(&tid, &buf[IDX_TID], sizeof(pid_t));
	memcpy(&rpc_count, &buf[IDX_COUNT], sizeof(int));
	memcpy(timestamp, &buf[IDX_TIME], 20);
	memcpy(&cmd, &buf[IDX_PAYLOAD], sizeof(char));
	memcpy(CANARY, &buf[IDX_PAYLOAD+1], CANARY_SIZE);
}


void NetconEthernetTap::phyOnUnixWritable(PhySocket *sock,void **uptr)
{
	TcpConnection *conn = getConnection(sock);
	int len = rcqidx;
	int n = _phy.streamSend(conn->sock, rcq, len);
	if(n > 0) {
		if(n < len) {
		    dwr(MSG_INFO,"\n phyOnUnixWritable(): unable to write entire \"block\" to stream\n");
		}
		memcpy(rcq, rcq+n, rcqidx-n);
	  	rcqidx -= n;
	  	lwipstack->_tcp_recved(conn->pcb, n);
	  	if(rcqidx == 0)
	  		_phy.setNotifyWritable(conn->sock, false); // Nothing more to be notified about
		dwr(MSG_DEBUG," phyOnUnixWritable(): wrote %d bytes from RX buffer to <%x> (idx = %d)\n", n, conn->sock, rcqidx);
	}
	else {
		perror("\n");
		fprintf(stderr, "errno = %d\n", errno);
		dwr(MSG_INFO," phyOnUnixWritable(): No data written to stream <%x>\n", conn->sock);
	}
}


/*
 * Processes incoming data on a client-specific RPC connection
 */
void NetconEthernetTap::phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len)
{		
	uint64_t CANARY_num;
	pid_t pid, tid;
	int rpc_count;
	char cmd, timestamp[20], CANARY[CANARY_SIZE];
	void *payload;
	unsigned char *buf = (unsigned char*)data;
	std::pair<PhySocket*, void*> sockdata;
	PhySocket *rpcsock;
	bool found_job = false, detected_rpc = false;
	TcpConnection *conn;
	int wlen = len;

	// RPC
	char phrase[RPC_PHRASE_SIZE];
	memset(phrase, 0, RPC_PHRASE_SIZE);
	if(len == BUF_SZ) {
		memcpy(phrase, buf, RPC_PHRASE_SIZE);
		if(strcmp(phrase, RPC_PHRASE) == 0)
			detected_rpc = true;
	}
	if(detected_rpc) {
		unload_rpc(data, pid, tid, rpc_count, timestamp, CANARY, cmd, payload);
		memcpy(&CANARY_num, CANARY, CANARY_SIZE);
		dwr(MSG_DEBUG," <%x> RPC: (pid=%d, tid=%d, rpc_count=%d, timestamp=%s, cmd=%d)\n", sock, pid, tid, rpc_count, timestamp, cmd);
		if(cmd == RPC_SOCKET) {				
			dwr(MSG_DEBUG,"  <%x> RPC_SOCKET\n", sock);
			// Create new lwip socket and associate it with this sock
			struct socket_st socket_rpc;
			memcpy(&socket_rpc, &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct socket_st));
			TcpConnection * new_conn;
			if((new_conn = handle_socket(sock, uptr, &socket_rpc))) {
				pidmap[sock] = pid;
				new_conn->pid = pid;
			}
		}
		else { // All RPCs other than RPC_SOCKET
			jobmap[CANARY_num] = std::make_pair<PhySocket*, void*>(sock, data);
		}
		write(_phy.getDescriptor(sock), "z", 1); // RPC ACK byte to maintain RPC->Stream order
	}
	
	// STREAM
	else {
		int data_start = -1, data_end = -1, token_pos = -1, padding_pos = -1;
		char padding[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89};
		dwr(MSG_DEBUG," <%x> stream data, len = %d\n", sock, len);
		// Look for padding
		std::string padding_pattern(padding, padding+CANARY_PADDING_SIZE);
		std::string buffer(buf, buf + len);
		padding_pos = buffer.find(padding_pattern);
		token_pos = padding_pos-CANARY_SIZE;
		dwr(MSG_DEBUG, " <%x> padding_pos = %d\n", sock, padding_pos);
		// Grab token, next we'll use it to look up an RPC job
		if(token_pos > -1) {
			memcpy(&CANARY_num, buf+token_pos, CANARY_SIZE);
			if(CANARY_num != 0) { // TODO: Added to address CANARY_num==0 bug, last seeen 20160108
				// Find job
				sockdata = jobmap[CANARY_num];
				if(!sockdata.first) { // Stream before RPC
					dwr(MSG_DEBUG,"       <%x> unable to locate job entry for %llu\n", sock, CANARY_num);
					return;
				}
				else
					found_job = true;
			}
		}

		conn = getConnection(sock);
		if(!conn)
			return;

		if(padding_pos == -1) { // [DATA]
			memcpy(&conn->buf[conn->idx], buf, wlen);
		}
		else { // Padding found, implies a token is present
			// [TOKEN]
			if(len == TOKEN_SIZE && token_pos == 0) {
				wlen = 0; // Nothing to write
			}
			else {
				// [TOKEN] + [DATA]
				if(len > TOKEN_SIZE && token_pos == 0) {
					wlen = len - TOKEN_SIZE;
					data_start = padding_pos+CANARY_PADDING_SIZE;
					memcpy((&conn->buf)+conn->idx, buf+data_start, wlen);
				}
				// [DATA] + [TOKEN]
				if(len > TOKEN_SIZE && token_pos > 0 && token_pos == len - TOKEN_SIZE) {
					wlen = len - TOKEN_SIZE;
					data_start = 0;
					memcpy((&conn->buf)+conn->idx, buf+data_start, wlen);												
				}
				// [DATA] + [TOKEN] + [DATA]
				if(len > TOKEN_SIZE && token_pos > 0 && len > (token_pos + TOKEN_SIZE)) {
					wlen = len - TOKEN_SIZE;
					data_start = 0;
					data_end = padding_pos-CANARY_SIZE;
					memcpy((&conn->buf)+conn->idx, buf+data_start, (data_end-data_start)+1);
					memcpy((&conn->buf)+conn->idx, buf+(padding_pos+CANARY_PADDING_SIZE), len-(token_pos+TOKEN_SIZE));
				}
			}
		}
		// Write data from stream
		if(conn->idx > (DEFAULT_READ_BUFFER_SIZE / 2)) {
			_phy.setNotifyReadable(sock, false);
		}
		lwipstack->_lock.lock();
		conn->idx += wlen;
		handle_write(conn);
		lwipstack->_lock.unlock();
	}

	if(found_job) {
		rpcsock = sockdata.first;
		buf = (unsigned char*)sockdata.second;
	}

	// Process RPC if we have a corresponding jobmap entry
	if(found_job) {
		unload_rpc(buf, pid, tid, rpc_count, timestamp, CANARY, cmd, payload);
		dwr(MSG_DEBUG," <%x> RPC: (pid=%d, tid=%d, rpc_count=%d, timestamp=%s, cmd=%d)\n", sock, pid, tid, rpc_count, timestamp, cmd);
		switch(cmd) {
			case RPC_BIND:
				dwr(MSG_DEBUG,"  <%x> RPC_BIND\n", sock);
			    struct bind_st bind_rpc;
			    memcpy(&bind_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct bind_st));
			    handle_bind(sock, rpcsock, uptr, &bind_rpc);
				break;
		  	case RPC_LISTEN:
				dwr(MSG_DEBUG,"  <%x> RPC_LISTEN\n", sock);
			    struct listen_st listen_rpc;
			    memcpy(&listen_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct listen_st));
			    handle_listen(sock, rpcsock, uptr, &listen_rpc);
				break;
		  	case RPC_GETSOCKNAME:
		  		dwr(MSG_DEBUG,"  <%x> RPC_GETSOCKNAME\n", sock);
		  		struct getsockname_st getsockname_rpc;
		    	memcpy(&getsockname_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct getsockname_st));
		  		handle_getsockname(sock, rpcsock, uptr, &getsockname_rpc);
		  		break;
			case RPC_CONNECT:
				dwr(MSG_DEBUG,"  <%x> RPC_CONNECT\n", sock);
			    struct connect_st connect_rpc;
			    memcpy(&connect_rpc,  &buf[IDX_PAYLOAD+STRUCT_IDX], sizeof(struct connect_st));
			    handle_connect(sock, rpcsock, conn, &connect_rpc);
			    jobmap.erase(CANARY_num);
				return; // Keep open RPC, we'll use it once in nc_connected to send retval
		  	default:
				break;
		}
		closeConnection(sockdata.first); // close RPC after sending retval, no longer needed
		jobmap.erase(CANARY_num);
		return;
	}
}

int NetconEthernetTap::send_return_value(PhySocket *sock, int retval, int _errno = 0){
	return send_return_value(_phy.getDescriptor(sock), retval, _errno);
}

int NetconEthernetTap::send_return_value(int fd, int retval, int _errno = 0)
{
	dwr(MSG_DEBUG," send_return_value(): fd = %d, retval = %d, errno = %d\n", fd, retval, _errno);
	int sz = sizeof(char) + sizeof(retval) + sizeof(errno);
	char retmsg[sz];
	memset(&retmsg, 0, sizeof(retmsg));
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
	dwr(MSG_DEBUG," nc_accept()\n");
	Larg *l = (Larg*)arg;
	TcpConnection *conn = l->conn;
	NetconEthernetTap *tap = l->tap;

	if(!conn->sock)
		return -1;
	int listening_fd = tap->_phy.getDescriptor(conn->sock);

  	if(conn) {
	  	// create new socketpair
	  	ZT_PHY_SOCKFD_TYPE fds[2];
		if(socketpair(PF_LOCAL, SOCK_STREAM, 0, fds) < 0) {
			if(errno < 0) {
				l->tap->send_return_value(conn, -1, errno);
				dwr(MSG_ERROR," nc_accept(): unable to create socketpair\n");
				return ERR_MEM;
			}
		}
		// create and populate new TcpConnection
		TcpConnection *new_tcp_conn = new TcpConnection();
		tap->addConnection(new_tcp_conn);
		new_tcp_conn->pcb = newpcb;
		new_tcp_conn->sock = tap->_phy.wrapSocket(fds[0], new_tcp_conn);

		if(sock_fd_write(listening_fd, fds[1]) < 0)
	  		return -1;
		else {
			//close(fds[1]); // close other end of socketpair
			new_tcp_conn->pending = true;
		}
	    tap->lwipstack->_tcp_arg(newpcb, new Larg(tap, new_tcp_conn));
	    tap->lwipstack->_tcp_recv(newpcb, nc_recved);
	    tap->lwipstack->_tcp_err(newpcb, nc_err);
	    tap->lwipstack->_tcp_sent(newpcb, nc_sent);
	    tap->lwipstack->_tcp_poll(newpcb, nc_poll, 1);
	    if(conn->pcb->state == LISTEN) {
	    	dwr(MSG_DEBUG," nc_accept(): Can't call tcp_accept() on LISTEN socket (pcb = %x)\n", conn->pcb);
	    	return ERR_OK; // TODO: Verify this is correct
	    }
	    tcp_accepted(conn->pcb); // Let lwIP know that it can queue additional incoming connections
		return ERR_OK;
  	}
  	else
  		dwr(MSG_ERROR," nc_accept(%d): can't locate Connection object for PCB.\n", listening_fd);
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
	Larg *l = (Larg*)arg;
	NetconEthernetTap *tap = l->tap;
	int tot = 0;
  	struct pbuf* q = p;

	if(!l->conn) {
		dwr(MSG_ERROR," nc_recved(): no connection\n");
		return ERR_OK; 
	}
	if(p == NULL) {
		if(l->conn && !l->conn->listening) {
			dwr(MSG_INFO," nc_recved(): closing connection\n");
			if(tap->lwipstack->_tcp_close(l->conn->pcb) != ERR_OK) {
				dwr(MSG_ERROR," nc_recved(): Error while calling tcp_close()\n");
			}
			tap->closeConnection(l->conn->sock);
			return ERR_ABRT;
		}
		else {
			//dwr(MSG_ERROR," nc_recved(): invalid connection/state\n");
		}
		return err;
	}
	// Cycle through pbufs and write them to the RX buffer
	// The RX buffer will be emptied via phyOnUnixWritable()
	while(p != NULL) {
		if(p->len <= 0)
			break;
		int avail = DEFAULT_READ_BUFFER_SIZE - tap->rcqidx;
		int len = p->len;
		if(avail < len) {
			dwr(MSG_DEBUG," nc_recv(): not enough room (%d bytes) on RX buffer\n", avail);
			exit(1);
		}
		memcpy(tap->rcq + (tap->rcqidx), p->payload, len);
		tap->rcqidx += len;
		tap->_phy.setNotifyWritable(l->conn->sock, true); // Signal that we're interested in knowing when we can write
		p = p->next;
		tot += len;
	}
	dwr(MSG_DEBUG," nc_recv(): wrote %d bytes to RX buffer (idx = %d)\n", tot, tap->rcqidx);
	tap->lwipstack->_pbuf_free(q);
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
	dwr(MSG_DEBUG,"nc_err() = %d\n", err);
	Larg *l = (Larg*)arg;
	if(!l->conn)
		dwr(MSG_ERROR,"nc_err(): Connection is NULL!\n");

	switch(err)
	{
		case ERR_MEM:
		  dwr(MSG_ERROR,"nc_err(): ERR_MEM->ENOMEM\n");
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, ENOMEM);
			break;
		case ERR_BUF:
			dwr(MSG_ERROR,"nc_err(): ERR_BUF->ENOBUFS\n");
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, ENOBUFS);
			break;
		case ERR_TIMEOUT:
			dwr(MSG_ERROR,"nc_err(): ERR_TIMEOUT->ETIMEDOUT\n");
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, ETIMEDOUT);
			break;
		case ERR_RTE:
			dwr(MSG_ERROR,"nc_err(): ERR_RTE->ENETUNREACH\n");
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, ENETUNREACH);
			break;
		case ERR_INPROGRESS:
			dwr(MSG_ERROR,"nc_err(): ERR_INPROGRESS->EINPROGRESS\n");
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, EINPROGRESS);
			break;
		case ERR_VAL:
			dwr(MSG_ERROR,"nc_err(): ERR_VAL->EINVAL\n");
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, EINVAL);
			break;
		case ERR_WOULDBLOCK:
			dwr(MSG_ERROR,"nc_err(): ERR_WOULDBLOCK->EWOULDBLOCK\n");
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, EWOULDBLOCK);
			break;
		case ERR_USE:
			dwr(MSG_ERROR,"nc_err(): ERR_USE->EADDRINUSE\n");
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, EADDRINUSE);
			break;
		case ERR_ISCONN:
			dwr(MSG_ERROR,"nc_err(): ERR_ISCONN->EISCONN\n");
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, EISCONN);
			break;
		case ERR_ABRT:
			dwr(MSG_ERROR,"nc_err(): ERR_ABRT->ECONNREFUSED\n");
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, ECONNREFUSED);
			break;

			// FIXME: Below are errors which don't have a standard errno correlate

		case ERR_RST:
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, -1);
			break;
		case ERR_CLSD:
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, -1);
			break;
		case ERR_CONN:
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, -1);
			break;
		case ERR_ARG:
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, -1);
			break;
		case ERR_IF:
			l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->sock), -1, -1);
			break;
		default:
			break;
	}
	dwr(MSG_ERROR,"nc_err(): closing connection\n");
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
	Larg *l = (Larg*)arg;
	if(len) {
		if(l->conn->idx < DEFAULT_READ_BUFFER_SIZE / 2) {
			l->tap->_phy.setNotifyReadable(l->conn->sock, true);
			l->tap->_phy.whack();
		}
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
	dwr(MSG_DEBUG," nc_connected()\n");
	Larg *l = (Larg*)arg;
	l->tap->send_return_value(l->tap->_phy.getDescriptor(l->conn->rpcsock), ERR_OK);
	return ERR_OK;
}

/*------------------------------------------------------------------------------
----------------------------- RPC Handler functions ----------------------------
------------------------------------------------------------------------------*/

/* Return the address that the socket is bound to */
void NetconEthernetTap::handle_getsockname(PhySocket *sock, PhySocket *rpcsock, void **uptr, struct getsockname_st *getsockname_rpc)
{
	TcpConnection *conn = getConnection(sock);
	// Assemble address "command" to send to intercept
	char retmsg[sizeof(struct sockaddr_storage)];
	memset(&retmsg, 0, sizeof(retmsg));
	if ((conn)&&(conn->addr))
    	memcpy(&retmsg, conn->addr, sizeof(struct sockaddr_storage));
	write(_phy.getDescriptor(rpcsock), &retmsg, sizeof(struct sockaddr_storage));
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
void NetconEthernetTap::handle_bind(PhySocket *sock, PhySocket *rpcsock, void **uptr, struct bind_st *bind_rpc)
{
	struct sockaddr_in *connaddr;
	connaddr = (struct sockaddr_in *) &bind_rpc->addr;
	int conn_port = lwipstack->ntohs(connaddr->sin_port);
	ip_addr_t conn_addr;
	conn_addr.addr = *((u32_t *)_ips[0].rawIpData());
	TcpConnection *conn = getConnection(sock);
	dwr(MSG_DEBUG," handle_bind(%d)\n", bind_rpc->sockfd);

	if(conn) {
		if(conn->pcb->state == CLOSED){
	  		int err = lwipstack->tcp_bind(conn->pcb, &conn_addr, conn_port);
			int ip = connaddr->sin_addr.s_addr;
			unsigned char d[4];
			d[0] = ip & 0xFF;
			d[1] = (ip >>  8) & 0xFF;
			d[2] = (ip >> 16) & 0xFF;
			d[3] = (ip >> 24) & 0xFF;
			dwr(MSG_DEBUG," handle_bind(): %d.%d.%d.%d : %d\n", d[0],d[1],d[2],d[3], conn_port);

			if(err != ERR_OK) {
				dwr(MSG_ERROR," handle_bind(): err = %d\n", err);
				if(err == ERR_USE)
					send_return_value(rpcsock, -1, EADDRINUSE);
				if(err == ERR_MEM)
					send_return_value(rpcsock, -1, ENOMEM);
				if(err == ERR_BUF)
					send_return_value(rpcsock, -1, ENOMEM);
			}
			else {
				conn->addr = (struct sockaddr_storage *) &bind_rpc->addr;
				send_return_value(rpcsock, ERR_OK, ERR_OK); // Success
			}
		}
		else {
			dwr(MSG_ERROR," handle_bind(): PCB (%x) not in CLOSED state. Ignoring BIND request.\n", conn->pcb);
			send_return_value(rpcsock, -1, EINVAL);
		}
	}
	else {
		dwr(MSG_ERROR," handle_bind(): can't locate connection for PCB\n");
		send_return_value(rpcsock, -1, EBADF);
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
void NetconEthernetTap::handle_listen(PhySocket *sock, PhySocket *rpcsock, void **uptr, struct listen_st *listen_rpc)
{
	dwr(MSG_DEBUG," handle_listen(their=%d):\n", listen_rpc->sockfd);
	TcpConnection *conn = getConnection(sock);
	if(!conn){
		dwr(MSG_ERROR," handle_listen(): unable to locate connection object\n");
		send_return_value(rpcsock, -1, EBADF);
		return;
	}
	if(conn->pcb->state == LISTEN) {
		dwr(MSG_ERROR," handle_listen(): PCB is already in listening state.\n");
		send_return_value(rpcsock, ERR_OK, ERR_OK);
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
		fcntl(_phy.getDescriptor(conn->sock), F_SETFL, O_NONBLOCK);
		conn->listening = true;
		conn->pending = true;
		send_return_value(rpcsock, ERR_OK, ERR_OK);
		return;
  	}
  send_return_value(rpcsock, -1, -1);
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
	struct tcp_pcb *newpcb = lwipstack->tcp_new();
	dwr(MSG_DEBUG," handle_socket(): pcb=%x\n", newpcb);
  	if(newpcb != NULL) {
  		TcpConnection *new_conn = new TcpConnection();
  		*uptr = new_conn;
  		new_conn->sock = sock;
  		new_conn->pcb = newpcb;
  		addConnection(new_conn);
  		new_conn->pending = true;
		return new_conn;
	}
	dwr(MSG_ERROR," handle_socket(): Memory not available for new PCB\n");
	send_return_value(_phy.getDescriptor(sock), -1, ENOMEM);
	return NULL;
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
void NetconEthernetTap::handle_connect(PhySocket *sock, PhySocket *rpcsock, TcpConnection *conn, struct connect_st* connect_rpc)
{
	dwr(MSG_DEBUG," handle_connect()\n");
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

		  int ip = connaddr->sin_addr.s_addr;
		  unsigned char d[4];
		  d[0] = ip & 0xFF;
		  d[1] = (ip >>  8) & 0xFF;
		  d[2] = (ip >> 16) & 0xFF;
		  d[3] = (ip >> 24) & 0xFF;
		  dwr(MSG_DEBUG,"handle_write(): %d.%d.%d.%d:\n", d[0],d[1],d[2],d[3]);	
		  	
		dwr(MSG_DEBUG,"handle_connect(): conn_port = %d\n", conn_port);
		int err = 0;

		dwr(MSG_DEBUG,"handle_connect(): pcb->state = %x\n", conn->pcb->state);
		if(conn->pcb->state != CLOSED) {
			dwr(MSG_DEBUG,"handle_connect(): PCB != CLOSED, cannot connect using this PCB\n");
			send_return_value(rpcsock, -1, EAGAIN);
			return;
		}

		if((err = lwipstack->tcp_connect(conn->pcb,&conn_addr,conn_port, nc_connected)) < 0)
		{
			if(err == ERR_ISCONN) {
				send_return_value(rpcsock, -1, EISCONN); // Already in connected state
				return;
			}
			if(err == ERR_USE) {
				send_return_value(rpcsock, -1, EADDRINUSE); // Already in use
				return;
			}
			if(err == ERR_VAL) {
				send_return_value(rpcsock, -1, EINVAL); // Invalid ipaddress parameter
				return;
			}
			if(err == ERR_RTE) {
				send_return_value(rpcsock, -1, ENETUNREACH); // No route to host
				return;
			}
			if(err == ERR_BUF) {
				send_return_value(rpcsock, -1, EAGAIN); // No more ports available
				return;
			}
			if(err == ERR_MEM) {
				/* Can occur for the following reasons: tcp_enqueue_flags()

				1) tcp_enqueue_flags is always called with either SYN or FIN in flags.
				  We need one available snd_buf byte to do that.
				  This means we can't send FIN while snd_buf==0. A better fix would be to
				  not include SYN and FIN sequence numbers in the snd_buf count.

				2) Cannot allocate new pbuf
				3) Cannot allocate new TCP segment

				*/
				send_return_value(rpcsock, -1, EAGAIN); // FIXME: Doesn't describe the problem well, but closest match
				return;
			}

			// We should only return a value if failure happens immediately
			// Otherwise, we still need to wait for a callback from lwIP.
			// - This is because an ERR_OK from tcp_connect() only verifies
			//   that the SYN packet was enqueued onto the stack properly,
			//   that's it!
			// - Most instances of a retval for a connect() should happen
			//   in the nc_connect() and nc_err() callbacks!
			dwr(MSG_ERROR," handle_connect(): unable to connect\n");
			send_return_value(rpcsock, -1, EAGAIN);
		}
		// Everything seems to be ok, but we don't have enough info to retval
		conn->pending=true;
		conn->listening=true;
		conn->rpcsock=rpcsock; // used for return value from lwip CB
	}
	else {
		dwr(MSG_ERROR," handle_connect(): could not locate PCB based on their fd\n");
		send_return_value(rpcsock, -1, EBADF);
	}
}



void NetconEthernetTap::handle_write(TcpConnection *conn)
{
	dwr(MSG_DEBUG_EXTRA,"handle_write(): conn->idx = %d, conn->sock = %x\n", conn->idx, conn->sock);
	if(!conn) {
		dwr(MSG_ERROR," handle_write(): invalid connection\n");
		return;
	}
	if(!conn->pcb) {
		dwr(MSG_ERROR," handle_write(): conn->pcb == NULL. Failed to write.\n");
		return;
	}
	int err, sz, r, sndbuf = conn->pcb->snd_buf; // How much we are currently allowed to write to the connection
	if(sndbuf == 0) {
		/* PCB send buffer is full,turn off readability notifications for the
		corresponding PhySocket until nc_sent() is called and confirms that there is
		now space on the buffer */
		dwr(MSG_DEBUG," handle_write(): sndbuf == 0, LWIP stack is full\n");
		_phy.setNotifyReadable(conn->sock, false);
		return;
	}
	if(conn->idx <= 0) {
		dwr(MSG_DEBUG,"handle_write(): conn->idx <= 0, nothing in buffer to write\n");
		return;
	}
	if(!conn->listening)
		lwipstack->_tcp_output(conn->pcb);

	if(conn->sock) {
		r = conn->idx < sndbuf ? conn->idx : sndbuf;
		dwr(MSG_DEBUG,"handle_write(): r = %d, sndbuf = %d\n", r, sndbuf);
		/* Writes data pulled from the client's socket buffer to LWIP. This merely sends the
		 * data to LWIP to be enqueued and eventually sent to the network. */
		if(r > 0) {
			// NOTE: this assumes that lwipstack->_lock is locked, either
			// because we are in a callback or have locked it manually.
			err = lwipstack->_tcp_write(conn->pcb, &conn->buf, r, TCP_WRITE_FLAG_COPY);
			lwipstack->_tcp_output(conn->pcb);
			if(err != ERR_OK) {
				dwr(MSG_ERROR," handle_write(): error while writing to PCB, (err = %d)\n", err);
				if(err == -1) 
					dwr(MSG_DEBUG," handle_write(): possibly out of memory\n");
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
	}
}



} // namespace ZeroTier
