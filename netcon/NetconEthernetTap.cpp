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

#include "NetconEthernetTap.hpp"

#include "../node/Utils.hpp"
#include "../osdep/OSUtils.hpp"
#include "../osdep/Phy.hpp"

#include "lwip/tcp_impl.h"
#include "netif/etharp.h"
#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/ip_frag.h"

#include "LWIPStack.hpp"
#include "NetconService.h"
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
	nc_service = new NetconService(lwipstack, sockPath); // Netcon Service

	_unixListenSocket = _phy.unixListen(sockPath,(void *)this);
	if (!_unixListenSocket)
		throw std::runtime_error(std::string("unable to bind to ")+sockPath);
	else
		_unixListenSocket.uptr = (void*) new NetconSocket(_unixListenSocket.sock, NetconSocketType.RPC);
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

		// TODO: alloc IP in LWIP
		//netif_set_addr(netif, ipaddr, netmask, gw);
	}
}

bool NetconEthernetTap::removeIp(const InetAddress &ip)
{
	Mutex::Lock _l(_ips_m);
	std::vector<InetAddress>::iterator i(std::find(_ips.begin(),_ips.end(),ip));
	if (i == _ips.end())
		return false;

	_ips.erase(i);
	// TODO: dealloc IP from LWIP

	return true;
}

std::vector<InetAddress> NetconEthernetTap::ips() const
{
	Mutex::Lock _l(_ips_m);
	return _ips;
}

void NetconEthernetTap::put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
{
	if (!_enabled)
		return;
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
	// TODO: get multicast subscriptions from LWIP
}

void NetconEthernetTap::threadMain()
	throw()
{
	static ip_addr_t ipaddr, netmask, gw;
	char ip_str[16] = {0}, nm_str[16] = {0}, gw_str[16] = {0};
  IP4_ADDR(&gw, 192,168,0,1);
  IP4_ADDR(&netmask, 255,255,255,0);
  IP4_ADDR(&ipaddr, 192,168,0,2);
	strncpy(ip_str, lwipstack->ipaddr_ntoa(&ipaddr), sizeof(ip_str));
  strncpy(nm_str, lwipstack->ipaddr_ntoa(&netmask), sizeof(nm_str));
  strncpy(gw_str, lwipstack->ipaddr_ntoa(&gw), sizeof(gw_str));

	unsigned long tcp_time = ARP_TMR_INTERVAL / 5000;
  unsigned long ipreass_time = TCP_TMR_INTERVAL / 1000;
  unsigned long etharp_time = IP_TMR_INTERVAL / 1000;
  unsigned long prev_tcp_time = 0;
  unsigned long prev_etharp_time = 0;
  unsigned long curr_time;
  unsigned long since_tcp;
  unsigned long since_etharp;

	struct timeval tv;
  struct timeval tv_sel;

	while (_run) {
		gettimeofday(&tv, NULL);
	  curr_time = (unsigned long)(tv.tv_sec) * 1000 + (unsigned long)(tv.tv_usec) / 1000;

	  since_tcp = curr_time - prev_tcp_time;
	  since_etharp = curr_time - prev_etharp_time;
	  int min_time = min(since_tcp, since_etharp) * 1000; // usec

	  if(since_tcp > tcp_time)
	  {
	    prev_tcp_time = curr_time+1;
	    lwipstack->tcp_tmr();
	  }

		if(since_etharp > etharp_time)
		{
			prev_etharp_time = curr_time;
			lwipstack->etharp_tmr();
		}
		// should be set every time since tv_sel is modified after each select() call
		tv_sel.tv_sec = 0;
		tv_sel.tv_usec = min_time;

		// Assemble/copy our fd_sets to poll on
		if(nc_service->possible_state_change) {
		  nc_service->assemble_fd_sets();
		}
		memcpy(&(nc_service->fdset), &(nc_service->cached_fdset), sizeof(nc_service->cached_fdset));
		memcpy(&(nc_service->exfdset), &(nc_service->cached_exfdset), sizeof(nc_service->cached_exfdset));
		memcpy(&(nc_service->alltypes), &(nc_service->cached_alltypes), sizeof(nc_service->cached_alltypes));
		nc_service->maxfd = nc_service->cached_maxfd;
		nc_service->sz = nc_service->cached_sz;

		_phy.poll(min_time * 1000); // conversion from usec to millisec, TODO: double check
	}

	// TODO: cleanup -- destroy LWIP state, kill any clients, unload .so, etc.
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
}

void NetconEthernetTap::phyOnUnixClose(PhySocket *sock,void **uptr)
{
}

void NetconEthernetTap::phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len)
{
	Phy<NetconEthernetTap*>::PhySocketImpl &sws = *(reinterpret_cast<Phy<NetconEthernetTap*>::PhySocketImpl *>(sock));

	int r;
	nc_service->possible_state_change = true;
	if(sws->uptr->type == NetconSocketType.BUFFER) {
		NetconConnection* c = nc_service->get_connection_by_buf_sock(sws->sock);
		if(c) {
			if(c->idx < DEFAULT_READ_BUFFER_SIZE) {
				//tcp_output(c->pcb);
				if((r = read(sws->sock, (&c->buf)+c->idx, DEFAULT_READ_BUFFER_SIZE-(c->idx))) > 0) {
					c->idx += r;
					handle_write(c);
				}
			}
		}
		else {
			//dwr(-1, "can't find connection for this fd: %d\n", ns->allfds[i].fd);
		}
	}
	if(sws->uptr->type == NetconSocketType.RPC)
	{
		NetconIntercept *h = nc_service->get_intercept_by_rpc(sws->sock);
		switch(data[0])
		{
			case RPC_SOCKET:
		    struct socket_st socket_rpc;
		    memcpy(&socket_rpc, &data[1], sizeof(struct socket_st));
		    h->tid = socket_rpc.__tid;
		    //dwr(h->tid,"__RPC_SOCKET\n");
		    handle_socket(h, &socket_rpc);
				break;
		  case RPC_LISTEN:
		    struct listen_st listen_rpc;
		    memcpy(&listen_rpc, &data[1], sizeof(struct listen_st));
		    h->tid = listen_rpc.__tid;
		    //dwr(h->tid,"__RPC_LISTEN\n");
		    handle_listen(h, &listen_rpc);
				break;
		  case RPC_BIND:
		    struct bind_st bind_rpc;
		    memcpy(&bind_rpc, &data[1], sizeof(struct bind_st));
		    h->tid = bind_rpc.__tid;
		    //dwr(h->tid,"__RPC_BIND\n");
		    handle_bind(h, &bind_rpc);
				break;
		  case RPC_KILL_INTERCEPT:
		    //dwr(h->tid,"__RPC_KILL_INTERCEPT\n");
		    handle_kill_intercept(h);
				break;
	  	case RPC_CONNECT:
		    struct connect_st connect_rpc;
		    memcpy(&connect_rpc, &data[1], sizeof(struct connect_st));
		    h->tid = connect_rpc.__tid;
		    //dwr("__RPC_CONNECT\n");
		    handle_connect(h, &connect_rpc);
				break;
		  case RPC_FD_MAP_COMPLETION:
		    //dwr("__RPC_FD_MAP_COMPLETION\n");
		    handle_retval(h, data);
				break;
			default:
				break;
		}
	}
}

void NetconEthernetTap::phyOnUnixWritable(PhySocket *sock,void **uptr)
{
}

void NetconEthernetTap::handle_kill_intercept(NetconIntercept* h) {
  nc_service->possible_state_change = true;
  // Close all owned connections
  for(size_t i=0; i<h->owned_connections.size(); i++) {
    nc_close(h->owned_connections[i]->pcb);
    close(h->owned_connections[i]->our_fd);
  }
  // Close RPC socketpair for this intercept
  close(h->rpc);
  nc_service->remove_intercept(h);
}

int NetconEthernetTap::send_return_value(NetconIntercept *h, int retval)
{
  if(!h->waiting_for_retval){
    //dwr(h->tid, "ERROR: intercept isn't waiting for return value. Why are we here?\n");
    return 0;
  }
  char retmsg[4];
  memset(&retmsg, '\0', sizeof(retmsg));
  retmsg[0]=RPC_RETVAL;
  memcpy(&retmsg[1], &retval, sizeof(retval));
  int n = write(h->rpc, &retmsg, sizeof(retmsg));

  if(n > 0) {
		/* signal that we've satisfied this requirement */
    h->waiting_for_retval = false;
  }
  else {
    /* in the event that we can't write to the intercept's RPC, we
    should assume that it has failed to connect */
    //dwr(h->tid, "ERROR: unable to send return value to the intercept\n");
    //dwr(h->tid, "removing intercept.\n");
    nc_service->remove_intercept(h);
  }
  return n;
}

/*------------------------------------------------------------------------------
--------------------------------- LWIP callbacks -------------------------------
------------------------------------------------------------------------------*/

err_t NetconEthernetTap::nc_poll(void* arg, struct tcp_pcb *tpcb)
{
	NetconConnection* c = nc_service->get_connection_by_buf_sock((intptr_t)arg);
	if(c)
		handle_write(c);
	return ERR_OK;
}

err_t NetconEthernetTap::nc_accept(void* arg, struct tcp_pcb *newpcb, err_t err)
{
	nc_service->possible_state_change = true;
  NetconConnection *c = nc_service->get_connection_by_buf_sock((intptr_t)arg);
  if(c && c->owner) {
    // Generate new socketpair and Connection. Use newly-allocated PCB
    int fd[2];
    socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
    NetconConnection *new_connection = nc_service->add_connection(c->owner, c->owner->tid, fd[0], -1, newpcb);
    //dwr(c->owner->tid, "socketpair { fd[0]=%d, fd[1]=%d }\n", fd[0], fd[1]);
    if(new_connection == NULL) {
      //printf("error adding new connection\n");
      return -1;
    }
    new_connection->owner->unmapped_conn = new_connection;
		// write byte to let accept call know we have a new connection
    int n = write(c->our_fd, "z", 1);
    if(n > 0) {
      //dwr(c->owner->tid, "sending socketpair fd... %d\n", fd[1]);
      sock_fd_write(c->owner->rpc, fd[1]);
    }
    else {
      //dwr(c->owner->tid, "nc_accept() - unknown error writing signal byte to listening socket\n");
      return -1;
    }
		// Set PCB-specific callbacks
    //dwr(c->owner->tid, "tcp_arg(pcb, %d)\n", new_connection->our_fd);
    lwipstack->tcp_arg(newpcb, (void*)(intptr_t)(new_connection->our_fd));
    lwipstack->tcp_recv(newpcb, nc_recved);
    lwipstack->tcp_err(newpcb, nc_err);
    lwipstack->tcp_sent(newpcb, nc_sent);
    lwipstack->tcp_poll(newpcb, nc_poll, APPLICATION_POLL_FREQ);
    tcp_accepted(c->pcb);
    return ERR_OK;
  }
  else {
    //dwr("can't locate Connection object for PCB\n");
  }
  return -1;
}

err_t NetconEthernetTap::nc_recved(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	int n;
  struct pbuf* q = p;
  NetconConnection *c = nc_service->get_connection_by_buf_sock((intptr_t)arg);
  if(c) {
    //dwr(c->owner->tid, "nc_recved(%d)\n", (intptr_t)arg);
  }
  else {
    //dwr(-1, "nc_recved(%d): unable to locate connection\n", (intptr_t)arg);
    return ERR_OK; // ?
  }
  if(p == NULL) {
    //dwr(c->owner->tid, "nc_recved() = %s\n", lwiperror(err));
    if(c)
      //dwr(c->owner->tid, "nc_recved()\n");
    if(c) {
      //dwr(c->owner->tid, "closing connection\n");
      nc_close(tpcb);
      close(c->our_fd); /* TODO: Check logic */
      nc_service->remove_connection(c);
      nc_service->possible_state_change = true;
    }
    else {
      //dwr(-1, "can't locate connection via (arg)\n");
    }
    return err;
  }
  q = p;
  while(p != NULL) { // Cycle through pbufs and write them to the socket
    //dwr(c->owner->tid, "writing data to mapped sock (%d)\n", c->our_fd);
    if(p->len <= 0)
      break; // ?
    if((n = write(c->our_fd, p->payload, p->len)) > 0) {
      if(n < p->len) {
        //dwr(c->owner->tid, "ERROR: unable to write entire pbuf to buffer\n");
      }
      lwipstack->tcp_recved(tpcb, n);
    }
    else {
      //dwr(c->owner->tid, "ERROR: No data written to intercept buffer.\n");
    }
    p = p->next;
  }
  lwipstack->pbuf_free(q); /* free pbufs */
  return ERR_OK;
}

void NetconEthernetTap::nc_err(void *arg, err_t err)
{
  nc_service->possible_state_change = true;
  NetconConnection *c = nc_service->get_connection_by_this_fd((intptr_t)arg);
  if(c) {
  	//dwr(c->owner->tid, "nc_err: %s\n", lwiperror(err));
    nc_service->remove_connection(c);
    //tcp_close(c->pcb);
    //dwr(-1, "connection removed.\n");
  }
  else {
    //dwr("ERROR: can't locate connection object for PCB.\n");
  }
  //nc_service->print_fd_set();
}

void NetconEthernetTap::nc_close(struct tcp_pcb* tpcb)
{
  nc_service->possible_state_change = true;

  NetconConnection *c = nc_service->get_connection_by_pcb(tpcb);
  if(c) {
    //dwr(c->owner->tid, "nc_close(): closing connection (their=%d, our=%d)\n", c->their_fd, c->our_fd);
  }
  else {
    //dwr(-1, "nc_close(): closing connection\n");
  }
  lwipstack->tcp_arg(tpcb, NULL);
  lwipstack->tcp_sent(tpcb, NULL);
  lwipstack->tcp_recv(tpcb, NULL);
  lwipstack->tcp_err(tpcb, NULL);
  lwipstack->tcp_poll(tpcb, NULL, 0);
  int err = lwipstack->tcp_close(tpcb);
  //dwr(-1, "tcp_close: %s\n", lwiperror(err));
}

err_t NetconEthernetTap::nc_send(struct tcp_pcb *tpcb)
{
	return ERR_OK;
}

err_t NetconEthernetTap::nc_sent(void* arg, struct tcp_pcb *tpcb, u16_t len)
{
	NetconConnection *c = nc_service->get_connection_by_pcb(tpcb);
	if(c)
		c->data_sent += len;
	return len;
}

err_t NetconEthernetTap::nc_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
	nc_service->possible_state_change = true;
	NetconIntercept *h = nc_service->get_intercept_by_pcb(tpcb);
	if(h) {
		//dwr(h->tid, "nc_connected()\n");
		send_return_value(h,err);
	}
	return err;
}

/*------------------------------------------------------------------------------
----------------------------- RPC Handler functions ----------------------------
------------------------------------------------------------------------------*/

void NetconEthernetTap::handle_bind(NetconIntercept *h, struct bind_st *bind_rpc)
{
	// FIXME: Is this hack still needed?
  struct sockaddr_in *connaddr;
  connaddr = (struct sockaddr_in *) &bind_rpc->addr;
  int conn_port = lwipstack->ntohs(connaddr->sin_port);
  ip_addr_t conn_addr;
  IP4_ADDR(&conn_addr, 192,168,0,2);

  int ip = connaddr->sin_addr.s_addr;
  unsigned char bytes[4];
  bytes[0] = ip & 0xFF;
  bytes[1] = (ip >> 8) & 0xFF;
  bytes[2] = (ip >> 16) & 0xFF;
  bytes[3] = (ip >> 24) & 0xFF;
  //dwr(h->tid, "binding to: %d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]);

  NetconConnection *c = nc_service->get_connection_by_that_fd(h, bind_rpc->sockfd);
  if(c)
  {
    if(c->pcb->state == CLOSED){
      int err = lwipstack->tcp_bind(c->pcb, &conn_addr, conn_port);
      if(err != ERR_OK) {
        //dwr(h->tid, "ERROR: while binding to addr/port\n");
      }
      else {
        //dwr(h->tid, "bind successful (fd=%d)\n", bind_rpc->sockfd);
      }
    }
    else {
      //dwr(h->tid, "PCB not in CLOSED state. Ignoring BIND request.\n");
    }
  }
  else {
    //dwr(h->tid, "can't locate connection for PCB (%d)\n", bind_rpc->sockfd);
  }
}

void NetconEthernetTap::handle_listen(NetconIntercept *h, struct listen_st *listen_rpc)
{
	NetconConnection *c = nc_service->get_connection_by_that_fd(h, listen_rpc->sockfd);
  if(c) {
    //dwr(c->owner->tid, "listen(%d, backlog=%d) from (tid=%d)\n", listen_rpc->sockfd, listen_rpc->backlog, listen_rpc->__tid);
    if(c->pcb->state == LISTEN) {
      //dwr(c->owner->tid, "PCB is already in listening state.\n");
      return;
    }
    struct tcp_pcb* listening_pcb = lwipstack->tcp_listen(c->pcb);
    if(listening_pcb != NULL) {
      //dwr(h->tid, "created new listening PCB\n");
      c->pcb = listening_pcb;
      lwipstack->tcp_accept(listening_pcb, nc_accept);
      //dwr(h->tid, "tcp_arg(pcb, %d)\n", (void*)(intptr_t)c->our_fd);
      lwipstack->tcp_arg(listening_pcb, (void*)(intptr_t)c->our_fd);
      h->waiting_for_retval=true;
    }
    else {
      //dwr(h->tid, "unable to allocate memory for new listening PCB\n");
    }
  }
  else {
    //dwr(h->tid, "can't locate connection for PCB (%d)\n", listen_rpc->sockfd);
  }
}

void NetconEthernetTap::handle_retval(NetconIntercept *h, unsigned char* buf)
{
	if(h->unmapped_conn != NULL) {
		memcpy(&(h->unmapped_conn->their_fd), &buf[1], sizeof(int));
		//dwr(h->tid, "intercept_fd(%d) -> service_fd(%d)\n",
		//	h->unmapped_conn->their_fd, h->unmapped_conn->our_fd);
		h->unmapped_conn = NULL;
	}
}

void NetconEthernetTap::handle_socket(NetconIntercept *h, struct socket_st* socket_rpc)
{
	struct tcp_pcb *pcb = lwipstack->tcp_new();
  if(pcb != NULL) {
    int fd[2];
    socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
    NetconConnection* new_connection = nc_service->add_connection(h, h->tid, fd[0], -1, pcb);
    //dwr(h->tid, "socketpair { fd[0]=%d, fd[1]=%d }\n", fd[0], fd[1]);
    //dwr(h->tid, "connections = %d\n", nc_service->connections.size());
    //dwr(h->tid, "added new socket entry\n");
    //dwr(h->tid, "sending socketpair buffer fd... %d\n", fd[1]);
    sock_fd_write(h->rpc, fd[1]);
    h->unmapped_conn = new_connection;
  }
  else {
    //dwr(h->tid, "ERROR: Memory not available for new PCB\n");
  }
}

void NetconEthernetTap::handle_connect(NetconIntercept *h, struct connect_st* connect_rpc)
{
	// FIXME: Parse out address information -- Probably a more elegant way to do this
	struct sockaddr_in *connaddr;
	connaddr = (struct sockaddr_in *) &connect_rpc->__addr;
	int conn_port = lwipstack->ntohs(connaddr->sin_port);
	ip_addr_t conn_addr = convert_ip((struct sockaddr_in *)&connect_rpc->__addr);
	NetconConnection *c = nc_service->get_connection_by_that_fd(h, connect_rpc->__fd);
	//print_ip(connaddr->sin_addr.s_addr);

	if(c!= NULL) {
		//dwr(-1, "connect(): TCP_SNDBUF = %d\n", tcp_sndbuf(nc->pcb));
		lwipstack->tcp_sent(c->pcb, nc_sent); // FIXME: Move?
		lwipstack->tcp_recv(c->pcb, nc_recved);
		lwipstack->tcp_err(c->pcb, ZeroTier::NetconEthernetTap::nc_err);
		lwipstack->tcp_poll(c->pcb, nc_poll, APPLICATION_POLL_FREQ);
		lwipstack->tcp_arg(c->pcb,(void*)(intptr_t)c->our_fd);

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
			send_return_value(h, err);
		}
		// Everything seems to be ok, but we don't have enough info to retval
		h->waiting_for_retval=true;
	}
	else {
		//dwr(h->tid, "ERROR: could not locate PCB based on their_fd (%d)", connect_rpc->__fd);
	}
}

void NetconEthernetTap::handle_write(NetconConnection *c)
{
	if(c) {
		int sndbuf = c->pcb->snd_buf;

		float avail = (float)sndbuf;
		float max = (float)TCP_SND_BUF;
		float load = 1.0 - (avail / max);

		if(load >= 0.9) {
			return;
		}
		int write_allowance =  sndbuf < c->idx ? sndbuf : c->idx;
		int sz;

		if(write_allowance > 0)
		{
			// FIXME: Copying data is expensive, we actually want TCP_WRITE_FLAG_MORE!
			int err = lwipstack->tcp_write(c->pcb, &c->buf, write_allowance, TCP_WRITE_FLAG_COPY);
			if(err != ERR_OK) {
				//dwr(c->owner->tid, "ERROR(%d): while writing to PCB, %s\n", err, lwiperror(err));
				return;
			}
			else {
				sz = (c->idx)-write_allowance;
				if(sz) {
					//printf(" w = %d\n\n", c->written);
					//printf("sz = %d\n", (c->idx)-write_allowance);
					memmove(&c->buf, (c->buf+write_allowance), sz);
				}
				c->idx -= write_allowance;
				c->data_sent += write_allowance;
				return;
			}
		}
		else {
			//dwr(c->owner->tid, "ERROR: lwIP stack full.\n");
			return;
		}
	}
	else {
		//dwr("ERROR: could not locate connection for this fd\n");
	}
}

} // namespace ZeroTier

#endif // ZT_ENABLE_NETCON
