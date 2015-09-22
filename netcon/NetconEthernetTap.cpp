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

#include "NetconEthernetTap.hpp"

#include "../node/Utils.hpp"
#include "../osdep/OSUtils.hpp"
#include "../osdep/Phy.hpp"

#include "lwip/tcp_impl.h"
#include "netif/etharp.h"
#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/ip_frag.h"
#include "lwip/tcp.h"

#include "LWIPStack.hpp"
#include "NetconService.hpp"
#include "Intercept.h"
#include "NetconUtilities.hpp"

#define APPLICATION_POLL_FREQ 1

namespace ZeroTier {


NetconEthernetTap::NetconEthernetTap(
	const char *homePath,
	const MAC &mac,
	unsigned int mtu,
	unsigned int metric,
	uint64_t nwid,
	const char *friendlyName,
	void (*handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
	void *arg) :
	_phy(this,false,true),
	_unixListenSocket((PhySocket *)0),
	_handler(handler),
	_arg(arg),
	_nwid(nwid),
	_mac(mac),
	_homePath(homePath),
	_mtu(mtu),
	_enabled(true),
	_run(true)
{
	char sockPath[4096];
	Utils::snprintf(sockPath,sizeof(sockPath),"/tmp/.ztnc_%.16llx",(unsigned long long)nwid);
	_dev = sockPath;

	lwipstack = new LWIPStack("/root/dev/netcon/liblwip.so");
	if(!lwipstack) // TODO double check this check
		throw std::runtime_error("unable to load lwip lib.");
	lwipstack->lwip_init();

	_unixListenSocket = _phy.unixListen(sockPath,(void *)this);
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
			fprintf(stderr, "initializing interface ip==%.8lx netmask==%.8lx\n",(unsigned long)ipaddr.addr,(unsigned long)netmask.addr);
			lwipstack->netif_add(&interface,&ipaddr, &netmask, &gw, NULL, tapif_init, lwipstack->ethernet_input);
			interface.state = this;
			interface.output = lwipstack->etharp_output;
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

	fprintf(stderr, "_put(%s,%s,%.4x,[data],%u)\n",from.toString().c_str(),to.toString().c_str(),etherType,len);
	if (!_enabled)
		return;

	//printf(">> %.4x %s\n",etherType,Utils::hex(data,len).c_str());
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
			fprintf(stderr,"_put(): Dropped packet: first pbuf smaller than ethernet header\n");
			return;
		}
		memcpy(q->payload,&ethhdr,sizeof(ethhdr));
		memcpy(q->payload + sizeof(ethhdr),dataptr,q->len - sizeof(ethhdr));
		dataptr += q->len - sizeof(ethhdr);

		// Remaining pbufs (if any) get rest of data
		while ((q = q->next)) {
			memcpy(q->payload,dataptr,q->len);
			dataptr += q->len;
		}
	} else {
		fprintf(stderr, "_put(): Dropped packet: no pbufs available\n");
		return;
	}

	//printf("p->len == %u, p->payload == %s\n",p->len,Utils::hex(p->payload,p->len).c_str());
	if(interface.input(p, &interface) != ERR_OK) {
		fprintf(stderr, "_put(): Error while RXing packet (netif->input)\n");
	}
	printf("_put(): length = %d\n", len);
}

std::string NetconEthernetTap::deviceName() const
{
	return _dev;
}

void NetconEthernetTap::setFriendlyName(const char *friendlyName)
{
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

NetconConnection *NetconEthernetTap::getConnectionByPCB(struct tcp_pcb *pcb)
{
	//fprintf(stderr, "  getConnectionByPCB\n");
	NetconConnection *c;
	//fprintf(stderr, "checking %d clients\n", clients.size());
	for(size_t i=0; i<clients.size(); i++) {
		c = clients[i]->containsPCB(pcb);
		if(c) {
			return c;
		}
	}
	return NULL;
}

NetconConnection *NetconEthernetTap::getConnectionByThisFD(int fd)
{
	//fprintf(stderr, "  getConnectionByThisFD\n");
	for(size_t i=0; i<clients.size(); i++) {
		for(size_t j=0; j<clients[i]->connections.size(); j++) {
			if(_phy.getDescriptor(clients[i]->connections[j]->sock) == fd) {
				return clients[i]->connections[j];
			}
		}
	}
	return NULL;
}

NetconClient *NetconEthernetTap::getClientByPCB(struct tcp_pcb *pcb)
{
	//fprintf(stderr, "  getClientByPCB\n");
	for(size_t i=0; i<clients.size(); i++) {
		if(clients[i]->containsPCB(pcb)) {
			return clients[i];
		}
	}
	return NULL;
}

void NetconEthernetTap::closeClient(NetconClient *client)
{
//fprintf(stderr, "closeClient\n");
	//fprintf(stderr, "closeClient\n");
	NetconConnection *temp_conn;
	closeConnection(client->rpc);
	for(size_t i=0; i<client->connections.size(); i++) {
		temp_conn = client->connections[i];
		closeConnection(client->connections[i]);
		delete temp_conn;
	}
	delete client;
}

void NetconEthernetTap::closeAllClients()
{
	//fprintf(stderr, "closeAllClients\n");
	for(int i=0; i<clients.size(); i++){
		closeClient(clients[i]);
	}
}

void NetconEthernetTap::closeConnection(NetconConnection *conn)
{
	NetconClient *client = conn->owner;
	_phy.close(conn->sock);
	lwipstack->tcp_close(conn->pcb);
	client->removeConnection(conn->sock);
}


/*------------------------------------------------------------------------------
------------------------ low-level Interface functions -------------------------
------------------------------------------------------------------------------*/

#define ZT_LWIP_TCP_TIMER_INTERVAL 10

void NetconEthernetTap::threadMain()
	throw()
{
	//unsigned long tcp_time = ARP_TMR_INTERVAL / 5000;
  //unsigned long etharp_time = IP_TMR_INTERVAL / 1000;
  //unsigned long prev_tcp_time = 0;
  //unsigned long prev_etharp_time = 0;
  //unsigned long curr_time;
  //unsigned long since_tcp;
  //unsigned long since_etharp;
	//struct timeval tv;
	uint64_t prev_tcp_time = 0;
	uint64_t prev_etharp_time = 0;

	fprintf(stderr, "- MEM_SIZE = %dM\n", MEM_SIZE / (1024*1024));
	fprintf(stderr, "- TCP_SND_BUF = %dK\n", TCP_SND_BUF / 1024);
	fprintf(stderr, "- MEMP_NUM_PBUF = %d\n", MEMP_NUM_PBUF);
	fprintf(stderr, "- MEMP_NUM_TCP_PCB = %d\n", MEMP_NUM_TCP_PCB);
	fprintf(stderr, "- MEMP_NUM_TCP_PCB_LISTEN = %d\n", MEMP_NUM_TCP_PCB_LISTEN);
	fprintf(stderr, "- MEMP_NUM_TCP_SEG = %d\n", MEMP_NUM_TCP_SEG);
	fprintf(stderr, "- PBUF_POOL_SIZE = %d\n", PBUF_POOL_SIZE);
	fprintf(stderr, "- TCP_SND_QUEUELEN = %d\n", TCP_SND_QUEUELEN);
	fprintf(stderr, "- IP_REASSEMBLY = %d\n", IP_REASSEMBLY);
	fprintf(stderr, "- TCP_WND = %d\n", TCP_WND);
	fprintf(stderr, "- TCP_MSS = %d\n", TCP_MSS);
	fprintf(stderr, "- NO_SYS           = %d\n", NO_SYS);
	fprintf(stderr, "- LWIP_SOCKET      = %d\n", LWIP_SOCKET);
	fprintf(stderr, "- LWIP_NETCONN     = %d\n", LWIP_NETCONN);
	fprintf(stderr, "- ARP_TMR_INTERVAL = %d\n", ARP_TMR_INTERVAL);
	fprintf(stderr, "- TCP_TMR_INTERVAL = %d\n", TCP_TMR_INTERVAL);
	fprintf(stderr, "- IP_TMR_INTERVAL  = %d\n", IP_TMR_INTERVAL);
	fprintf(stderr, "- DEFAULT_READ_BUFFER_SIZE  = %d\n", DEFAULT_READ_BUFFER_SIZE);

	// Main timer loop
	while (_run) {
		uint64_t now = OSUtils::now();

		uint64_t since_tcp = now - prev_tcp_time;
		uint64_t since_etharp = now - prev_etharp_time;

		uint64_t tcp_remaining = ZT_LWIP_TCP_TIMER_INTERVAL;
		uint64_t etharp_remaining = ARP_TMR_INTERVAL;

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

		/*
		gettimeofday(&tv, NULL);
	  curr_time = (unsigned long)(tv.tv_sec) * 1000 + (unsigned long)(tv.tv_usec) / 1000;
	  since_tcp = curr_time - prev_tcp_time;
	  since_etharp = curr_time - prev_etharp_time;
	  int min_time = min(since_tcp, since_etharp) * 1000; // usec

		//fprintf(stderr, "_run\n");

	  if(since_tcp > tcp_time)
	  {
	    prev_tcp_time = curr_time+1;
			//fprintf(stderr, "tcp_tmr\n");
	    lwipstack->tcp_tmr();
	  }
		if(since_etharp > etharp_time)
		{
			prev_etharp_time = curr_time;
			//fprintf(stderr, "etharp_tmr\n");
			lwipstack->etharp_tmr();
		}
		_phy.poll(50); // conversion from usec to millisec, TODO: double check
		*/
	}
	closeAllClients();
	// TODO: cleanup -- destroy LWIP state, kill any clients, unload .so, etc.
}

void NetconEthernetTap::phyOnSocketPairEndpointClose(PhySocket *sock, void **uptr)
{
	//fprintf(stderr, "phyOnSocketPairEndpointClose\n");
	_phy.setNotifyWritable(sock, false);
	NetconClient *client = (NetconClient*)*uptr;
	//closeConnection(client->getConnection(sock));
	// TODO check logic
}

void NetconEthernetTap::phyOnSocketPairEndpointData(PhySocket *sock, void **uptr, void *buf, unsigned long n)
{
	//fprintf(stderr, "phyOnSocketPairEndpointData\n");
	/*
	int r;
	NetconConnection *c = ((NetconClient*)*uptr)->getConnection(sock);
	if(c) {
		if(c->idx < DEFAULT_READ_BUFFER_SIZE) {
			if((r = read(_phy.getDescriptor(c->sock), (&c->buf)+c->idx, DEFAULT_READ_BUFFER_SIZE-(c->idx))) > 0) {
				c->idx += r;
				handle_write(c);
			}
		}
	}
	*/
	/*
	fprintf(stderr, "OnData(): len = %u\n", n);
	NetconConnection *c = ((NetconClient*)*uptr)->getConnection(sock);
	handle_write(c, buf, n);
	*/
}


void NetconEthernetTap::phyOnFileDescriptorActivity(PhySocket *sock,void **uptr,bool readable,bool writable)
{
	fprintf(stderr, "phyOnFileDescriptorActivity()\n");
	if(readable)
	{
		int r;
		fprintf(stderr, "  is readable\n");
		NetconConnection *c = ((NetconClient*)*uptr)->getConnection(sock);
		if(c->idx < DEFAULT_READ_BUFFER_SIZE) {
			//tcp_output(c->pcb);
			if((r = read(_phy.getDescriptor(sock), (&c->buf)+c->idx, DEFAULT_READ_BUFFER_SIZE-(c->idx))) > 0) {
				c->idx += r;
				handle_write(c);
			}
		}
	}
}

void NetconEthernetTap::phyOnSocketPairEndpointWritable(PhySocket *sock, void **uptr)
{
	//fprintf(stderr, "phyOnSocketPairEndpointWritable\n");
	_phy.setNotifyWritable(sock, false);
}

// Unused -- no UDP or TCP from this thread/Phy<>
void NetconEthernetTap::phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len) {}

void NetconEthernetTap::phyOnTcpConnect(PhySocket *sock,void **uptr,bool success) {}
void NetconEthernetTap::phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from) {}
void NetconEthernetTap::phyOnTcpClose(PhySocket *sock,void **uptr) {}
void NetconEthernetTap::phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len) {}
void NetconEthernetTap::phyOnTcpWritable(PhySocket *sock,void **uptr) {}

void NetconEthernetTap::phyOnUnixAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN)
{
	//fprintf(stderr, "phyOnUnixAccept\n");
	NetconClient *newClient = new NetconClient();
	newClient->rpc = newClient->addConnection(RPC, sockN);
	*uptrN = newClient;
	clients.push_back(newClient);
}

void NetconEthernetTap::phyOnUnixClose(PhySocket *sock,void **uptr)
{
	//fprintf(stderr, "phyOnUnixClose\n");
	_phy.setNotifyWritable(sock, false);
	//fprintf(stderr, "phyOnUnixClose\n");
	closeClient(((NetconClient*)*uptr));
}

void NetconEthernetTap::phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len)
{
	fprintf(stderr, "phyOnUnixData(): rpc = %d\n", _phy.getDescriptor(sock));
	unsigned char *buf = (unsigned char*)data;
	NetconClient *client = (NetconClient*)*uptr;
	if(!client)
		fprintf(stderr, "!client\n");

	switch(buf[0])
	{
		case RPC_SOCKET:
			fprintf(stderr, "RPC_SOCKET\n");
	    struct socket_st socket_rpc;
	    memcpy(&socket_rpc, &buf[1], sizeof(struct socket_st));
	    client->tid = socket_rpc.__tid;
	    handle_socket(client, &socket_rpc);
			break;
	  case RPC_LISTEN:
			fprintf(stderr, "RPC_LISTEN\n");
	    struct listen_st listen_rpc;
	    memcpy(&listen_rpc, &buf[1], sizeof(struct listen_st));
	    client->tid = listen_rpc.__tid;
	    handle_listen(client, &listen_rpc);
			break;
	  case RPC_BIND:
			fprintf(stderr, "RPC_BIND\n");
	    struct bind_st bind_rpc;
	    memcpy(&bind_rpc, &buf[1], sizeof(struct bind_st));
	    client->tid = bind_rpc.__tid;
	    handle_bind(client, &bind_rpc);
			break;
	  case RPC_KILL_INTERCEPT:
			fprintf(stderr, "RPC_KILL_INTERCEPT\n");
	    closeClient(client);
			break;
  	case RPC_CONNECT:
			fprintf(stderr, "RPC_CONNECT\n");
	    struct connect_st connect_rpc;
	    memcpy(&connect_rpc, &buf[1], sizeof(struct connect_st));
	    client->tid = connect_rpc.__tid;
	    handle_connect(client, &connect_rpc);
			break;
	  case RPC_FD_MAP_COMPLETION:
			fprintf(stderr, "RPC_FD_MAP_COMPLETION\n");
	    handle_retval(client, buf);
			break;
		default:
			break;
	}
}

void NetconEthernetTap::phyOnUnixWritable(PhySocket *sock,void **uptr)
{
}

int NetconEthernetTap::send_return_value(NetconClient *client, int retval)
{
	fprintf(stderr, "send_return_value\n");
  if(!client->waiting_for_retval){
    fprintf(stderr, "intercept isn't waiting for return value. Why are we here?\n");
    return 0;
  }
  char retmsg[4];
  memset(&retmsg, '\0', sizeof(retmsg));
  retmsg[0]=RPC_RETVAL;
  memcpy(&retmsg[1], &retval, sizeof(retval));
  int n = write(_phy.getDescriptor(client->rpc->sock), &retmsg, sizeof(retmsg));

  if(n > 0) {
		// signal that we've satisfied this requirement
    client->waiting_for_retval = false;
  }
  else {
    fprintf(stderr, "unable to send return value to the intercept\n");
		closeClient(client);
  }
  return n;
}

/*------------------------------------------------------------------------------
--------------------------------- LWIP callbacks -------------------------------
------------------------------------------------------------------------------*/

err_t NetconEthernetTap::nc_poll(void* arg, struct tcp_pcb *tpcb)
{
	/*
	Larg *l = (Larg*)arg;
	fprintf(stderr, "nc_poll(): [pcb = %x], [larg = %x]\n", tpcb, l);
	NetconConnection *c = l->tap->getConnectionByPCB(tpcb);
	NetconEthernetTap *tap = l->tap;
	if(c && c->idx > 0){
		fprintf(stderr, "nc_poll(): calling handle_Write()\n");
		tap->handle_write(c);
	}
	*/
	return ERR_OK;
}

err_t NetconEthernetTap::nc_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	Larg *l = (Larg*)arg;
	//fprintf(stderr, "nc_accept(): [pcb = %x], [larg = %x]\n", newpcb, l);
	int larg_fd = l->tap->_phy.getDescriptor(l->sock);
	fprintf(stderr, "nc_accept(): accepting on %d\n", larg_fd);
	NetconEthernetTap *tap = l->tap;
	NetconConnection *c = tap->getConnectionByThisFD(larg_fd);

  if(c) {
		NetconClient *client = c->owner;
		if(!client){
			fprintf(stderr, "nc_accpet(): unable to locate client for this PCB\n");
			return -1;
		}

		ZT_PHY_SOCKFD_TYPE fds[2];
		socketpair(PF_LOCAL, SOCK_STREAM, 0, fds);

		PhySocket *new_sock = tap->_phy.wrapSocket(fds[0], client);

		NetconConnection *new_conn = client->addConnection(BUFFER, new_sock);
		int our_fd = tap->_phy.getDescriptor(new_sock);
		client->connections.push_back(new_conn);
		new_conn->their_fd = fds[1];
		new_conn->pcb = newpcb;
		int send_fd = tap->_phy.getDescriptor(client->rpc->sock);
		int n = write(larg_fd, "z", 1);
    if(n > 0) {
			sock_fd_write(send_fd, fds[1]);
			client->unmapped_conn = new_conn;
			fprintf(stderr, "creating new conn (%d). Sending (%d) for their fd over (%d)\n", our_fd, fds[1], send_fd);
    }
    else {
      fprintf(stderr, "nc_accept(): error writing signal byte (our_fd = %d, send_fd = %d, their_fd = %d)\n", our_fd, send_fd, fds[1]);
      return -1;
    }
    tap->lwipstack->tcp_arg(newpcb, new Larg(tap, new_conn->sock));
    tap->lwipstack->tcp_recv(newpcb, nc_recved);
    tap->lwipstack->tcp_err(newpcb, nc_err);
    tap->lwipstack->tcp_sent(newpcb, nc_sent);
    tap->lwipstack->tcp_poll(newpcb, nc_poll, 1);
    tcp_accepted(c->pcb);

		return ERR_OK;
  }
  else {
    fprintf(stderr, "nc_accept(): can't locate Connection object for PCB. \n");
  }
  return -1;


	return ERR_OK;
}

err_t NetconEthernetTap::nc_recved(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	//fprintf(stderr, "nc_recved\n");
	Larg *l = (Larg*)arg;
	int larg_fd = l->tap->_phy.getDescriptor(l->sock);
	fprintf(stderr, "nc_recved(): RXing on %d\n", larg_fd);
	//fprintf(stderr, "nc_recved(): [pcb = %x], [larg = %x]\n", tpcb, l);
	if(!l)
		fprintf(stderr, "nc_recved(): could not find Larg for this [pcb = %x]\n", tpcb);
	fprintf(stderr, "nc_recved(): tap = %x\n", l->tap);
	NetconConnection *c = l->tap->getConnectionByPCB(tpcb);
	if(!c)
		fprintf(stderr, "nc_recved(): unable to locate connection\n");
	NetconEthernetTap *tap = l->tap;

	int n;
  struct pbuf* q = p;
	int our_fd = tap->_phy.getDescriptor(c->sock);

	fprintf(stderr, "nc_recved(): our_fd = %d\n", our_fd);

  if(!c) {
		fprintf(stderr, "nc_recved(): no connection object\n");
    return ERR_OK; // ?
  }
  if(p == NULL) {
    if(c) {
			fprintf(stderr, "nc_recved(): closing connection (p==NULL)\n");
      nc_close(tpcb);
			tap->_phy.close(c->sock);
			tap->closeConnection(c);
    }
    else {
      fprintf(stderr, "can't locate connection via (arg)\n");
    }
    return err;
  }
  q = p;
  while(p != NULL) { // Cycle through pbufs and write them to the socket
		fprintf(stderr, "nc_recved(): writing pbufs to socket\n");
    if(p->len <= 0)
      break; // ?
    if(

			/*(n = write(our_fd, p->payload, p->len))*/

			(n = tap->_phy.streamSend(c->sock,p->payload, p->len)) > 0) {
      if(n < p->len) {
        fprintf(stderr, "ERROR: unable to write entire pbuf to buffer\n");
				//tap->_phy.setNotifyWritable(l->sock, true);
      }
			fprintf(stderr, "nc_recved(): wrote (%d) bytes to (%d)\n", n, our_fd);
      tap->lwipstack->tcp_recved(tpcb, n);
    }
    else {
      fprintf(stderr, "Error: No data written to intercept buffer\n");
    }
    p = p->next;
  }
  tap->lwipstack->pbuf_free(q); // free pbufs
  return ERR_OK;
}

void NetconEthernetTap::nc_err(void *arg, err_t err)
{
	fprintf(stderr, "nc_err\n");
	Larg *l = (Larg*)arg;
	NetconEthernetTap *tap = l->tap;
	NetconConnection *c = tap->getConnectionByThisFD(tap->_phy.getDescriptor(l->sock));
  if(c) {
    tap->closeConnection(c);
  }
  else {
    fprintf(stderr, "can't locate connection object for PCB\n");
  }
}

void NetconEthernetTap::nc_close(struct tcp_pcb* tpcb)
{
	fprintf(stderr, "nc_close\n");
	//closeConnection(getConnectionByPCB(tpcb));
	/*
  lwipstack->tcp_arg(tpcb, NULL);
  lwipstack->tcp_sent(tpcb, NULL);
  lwipstack->tcp_recv(tpcb, NULL);
  lwipstack->tcp_err(tpcb, NULL);
  lwipstack->tcp_poll(tpcb, NULL, 0);
  lwipstack->tcp_close(tpcb);
	*/
}

err_t NetconEthernetTap::nc_send(struct tcp_pcb *tpcb)
{
	fprintf(stderr, "nc_send\n");
	return ERR_OK;
}

err_t NetconEthernetTap::nc_sent(void* arg, struct tcp_pcb *tpcb, u16_t len)
{
	fprintf(stderr, "nc_sent\n");
	return len;
}

err_t NetconEthernetTap::nc_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
	fprintf(stderr, "nc_connected\n");
	Larg *l = (Larg*)arg;
	NetconEthernetTap *tap = l->tap;
	for(size_t i=0; i<tap->clients.size(); i++) {
		if(tap->clients[i]->containsPCB(tpcb)) {
			tap->send_return_value(tap->clients[i],err);
		}
	}
	return err;
}



/*------------------------------------------------------------------------------
----------------------------- RPC Handler functions ----------------------------
------------------------------------------------------------------------------*/

void NetconEthernetTap::handle_bind(NetconClient *client, struct bind_st *bind_rpc)
{
	// FIXME: Is this hack still needed?
  struct sockaddr_in *connaddr;
  connaddr = (struct sockaddr_in *) &bind_rpc->addr;
  int conn_port = lwipstack->ntohs(connaddr->sin_port);
  ip_addr_t conn_addr;
	conn_addr.addr = *((u32_t *)_ips[0].rawIpData());

  int ip = connaddr->sin_addr.s_addr;
  unsigned char bytes[4];
  bytes[0] = ip & 0xFF;
  bytes[1] = (ip >> 8) & 0xFF;
  bytes[2] = (ip >> 16) & 0xFF;
  bytes[3] = (ip >> 24) & 0xFF;
  fprintf(stderr, "binding to: %d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]);
	fprintf(stderr, "PORT = %d\n", conn_port);

	NetconConnection *c = client->getConnectionByTheirFD(bind_rpc->sockfd);
  if(c) {
    if(c->pcb->state == CLOSED){
      int err = lwipstack->tcp_bind(c->pcb, &conn_addr, conn_port);
      if(err != ERR_OK) {
        fprintf(stderr, "error while binding to addr/port\n");
      }
      else {
        fprintf(stderr, "bind successful\n");
      }
    }
    else {
      fprintf(stderr, "PCB not in CLOSED state. Ignoring BIND request.\n");
    }
  }
  else {
    fprintf(stderr, "can't locate connection for PCB\n");
  }
}

void NetconEthernetTap::handle_listen(NetconClient *client, struct listen_st *listen_rpc)
{
	fprintf(stderr, "client->rpc->sock->fd = %d\n", _phy.getDescriptor(client->rpc->sock));
	NetconConnection *c = client->getConnectionByTheirFD(listen_rpc->sockfd);
  if(c) {
    if(c->pcb->state == LISTEN) {
      fprintf(stderr, "PCB is already in listening state.\n");
      return;
    }
    struct tcp_pcb* listening_pcb = lwipstack->tcp_listen(c->pcb);
    if(listening_pcb != NULL) {
			fprintf(stderr, "handle_listen(): c->pcb(%x) = listening_pcb(%x)\n", c->pcb, listening_pcb);
      c->pcb = listening_pcb;
      lwipstack->tcp_accept(listening_pcb, nc_accept);
			lwipstack->tcp_arg(listening_pcb, new Larg(this, c->sock));
      client->waiting_for_retval=true;
    }
    else {
			fprintf(stderr, "unable to allocate memory for new listening PCB\n");
    }
  }
  else {
    fprintf(stderr, "can't locate connection for PCB\n");
  }
}

void NetconEthernetTap::handle_retval(NetconClient *client, unsigned char* buf)
{
	if(client->unmapped_conn != NULL) {
		memcpy(&(client->unmapped_conn->their_fd), &buf[1], sizeof(int));
		fprintf(stderr, "handle_retval(): RXed (our_fd = %d, their_fd = %d)\n", _phy.getDescriptor(client->unmapped_conn->sock), client->unmapped_conn->their_fd);
		client->connections.push_back(client->unmapped_conn);
		client->unmapped_conn = NULL;
	}
}

void NetconEthernetTap::handle_socket(NetconClient *client, struct socket_st* socket_rpc)
{
	struct tcp_pcb *pcb = lwipstack->tcp_new();
  if(pcb != NULL) {
		ZT_PHY_SOCKFD_TYPE fds[2];
		socketpair(PF_LOCAL, SOCK_STREAM, 0, fds);
		PhySocket *our_sock = _phy.wrapSocket(fds[0], client);
		int our_fd = _phy.getDescriptor(our_sock);
		NetconConnection *new_conn = client->addConnection(BUFFER, our_sock);
		new_conn->their_fd = fds[1];
		new_conn->pcb = pcb;
		PhySocket *sock = client->rpc->sock;
		int send_fd = _phy.getDescriptor(sock);
    sock_fd_write(send_fd, fds[1]);
    client->unmapped_conn = new_conn;
		fprintf(stderr, "handle_socket(): [pcb = %x], their_fd = %d, send_fd = %d, our_fd = %d\n", pcb, fds[1], send_fd, our_fd);
  }
  else {
    fprintf(stderr, "Memory not available for new PCB\n");
  }
}

void NetconEthernetTap::handle_connect(NetconClient *client, struct connect_st* connect_rpc)
{
	// FIXME: Parse out address information -- Probably a more elegant way to do this
	struct sockaddr_in *connaddr;
	connaddr = (struct sockaddr_in *) &connect_rpc->__addr;
	int conn_port = lwipstack->ntohs(connaddr->sin_port);
	ip_addr_t conn_addr = convert_ip((struct sockaddr_in *)&connect_rpc->__addr);

	fprintf(stderr, "getConnectionByTheirFD(%d)\n", connect_rpc->__fd);
	NetconConnection *c = client->getConnectionByTheirFD(connect_rpc->__fd);

	if(c!= NULL) {
		lwipstack->tcp_sent(c->pcb, nc_sent); // FIXME: Move?
		lwipstack->tcp_recv(c->pcb, nc_recved);
		lwipstack->tcp_err(c->pcb, nc_err);
		lwipstack->tcp_poll(c->pcb, nc_poll, APPLICATION_POLL_FREQ);
		lwipstack->tcp_arg(c->pcb, new Larg(this, c->sock));

		int err = 0;
		if((err = lwipstack->tcp_connect(c->pcb,&conn_addr,conn_port, nc_connected)) < 0)
		{
			// dwr(h->tid, "tcp_connect() = %s\n", lwiperror(err));
			// We should only return a value if failure happens immediately
			// Otherwise, we still need to wait for a callback from lwIP.
			// - This is because an ERR_OK from tcp_connect() only verifies
			//   that the SYN packet was enqueued onto the stack properly,
			//   that's it!
			// - Most instances of a retval for a connect() should happen
			//   in the nc_connect() and nc_err() callbacks!
			//fprintf(stderr, "failed to connect: %s\n", lwiperror(err));
			send_return_value(client, err);
		}
		// Everything seems to be ok, but we don't have enough info to retval
		client->waiting_for_retval=true;
	}
	else {
		fprintf(stderr, "could not locate PCB based on their fd\n");
	}
}

void NetconEthernetTap::handle_write(NetconConnection *c)
{
	fprintf(stderr, "handle_write()\n");
	if(c) {
		int sndbuf = c->pcb->snd_buf;
		float avail = (float)sndbuf;
		float max = (float)TCP_SND_BUF;
		float load = 1.0 - (avail / max);

		if(load >= 0.9) {
			return;
		}

		//c->idx += len;
		// TODO: write logic here!

		int write_allowance =  sndbuf < c->idx ? sndbuf : c->idx;
		int sz;

		fprintf(stderr, "handle_write(): write_allowance = %d, pcb->sndbuf = %d\n", write_allowance, sndbuf);
		if(write_allowance > 0) {
			int err = lwipstack->tcp_write(c->pcb, &c->buf, write_allowance, TCP_WRITE_FLAG_COPY);
			if(err != ERR_OK) {
				fprintf(stderr, "handle_write(): error while writing to PCB\n");
				return;
			}
			else {
				sz = (c->idx)-write_allowance;
				if(sz) {
					memmove(&c->buf, (c->buf+write_allowance), sz);
				}
				c->idx -= write_allowance;
				//c->data_sent += write_allowance;
				return;
			}
		}
		else {
			fprintf(stderr, "handle_write(): lwIP stack full\n");
			return;
		}
	}
	else {
		fprintf(stderr, "handle_write(): could not locate connection for this fd\n");
	}
}

} // namespace ZeroTier

#endif // ZT_ENABLE_NETCON
