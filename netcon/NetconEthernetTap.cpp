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


NetconConnection *NetconEthernetTap::getConnectionByPCB(struct tcp_pcb *pcb)
{
	NetconConnection *c;
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
	for(size_t i=0; i<clients.size(); i++) {
		if(clients[i]->containsPCB(pcb)) {
			return clients[i];
		}
	}
	return NULL;
}


void NetconEthernetTap::closeClient(NetconClient *client)
{
	// erase from clients vector
	client->close();
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
		//tv_sel.tv_sec = 0;
		//tv_sel.tv_usec = min_time;

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
	NetconClient *newClient = new NetconClient();
	newClient->addConnection(RPC, *uptrN);
}

void NetconEthernetTap::phyOnUnixClose(PhySocket *sock,void **uptr)
{
	((NetconClient*)*uptr)->close();
}

void NetconEthernetTap::phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len)
{
	unsigned char *buf = (unsigned char*)data;

	NetconConnection *c = ((NetconClient*)*uptr)->getConnection(sock);
	int r;
	if(c->type == BUFFER) {
		if(c) {
			if(c->idx < DEFAULT_READ_BUFFER_SIZE) {
				if((r = read(_phy.getDescriptor(c->sock), (&c->buf)+c->idx, DEFAULT_READ_BUFFER_SIZE-(c->idx))) > 0) {
					c->idx += r;
					handle_write(c);
				}
			}
		}
		else {
			// can't find connection for this fd
		}
	}
	if(c->type == RPC)
	{
		NetconClient *client = (NetconClient*)*uptr;
		switch(buf[0])
		{
			case RPC_SOCKET:
		    struct socket_st socket_rpc;
		    memcpy(&socket_rpc, &buf[1], sizeof(struct socket_st));
		    client->tid = socket_rpc.__tid;
		    handle_socket(client, &socket_rpc);
				break;
		  case RPC_LISTEN:
		    struct listen_st listen_rpc;
		    memcpy(&listen_rpc, &buf[1], sizeof(struct listen_st));
		    client->tid = listen_rpc.__tid;
		    handle_listen(client, &listen_rpc);
				break;
		  case RPC_BIND:
		    struct bind_st bind_rpc;
		    memcpy(&bind_rpc, &buf[1], sizeof(struct bind_st));
		    client->tid = bind_rpc.__tid;
		    handle_bind(client, &bind_rpc);
				break;
		  case RPC_KILL_INTERCEPT:
		    client->close();
				break;
	  	case RPC_CONNECT:
		    struct connect_st connect_rpc;
		    memcpy(&connect_rpc, &buf[1], sizeof(struct connect_st));
		    client->tid = connect_rpc.__tid;
		    handle_connect(client, &connect_rpc);
				break;
		  case RPC_FD_MAP_COMPLETION:
		    handle_retval(client, buf);
				break;
			default:
				break;
		}
	}
}

void NetconEthernetTap::phyOnUnixWritable(PhySocket *sock,void **uptr)
{
}

int NetconEthernetTap::send_return_value(NetconClient *client, int retval)
{
  if(!client->waiting_for_retval){
    // intercept isn't waiting for return value. Why are we here?
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
    // unable to send return value to the intercept
		closeClient(client);
  }
  return n;
}

/*------------------------------------------------------------------------------
--------------------------------- LWIP callbacks -------------------------------
------------------------------------------------------------------------------*/

err_t NetconEthernetTap::nc_poll(void* arg, struct tcp_pcb *tpcb)
{
	NetconConnection *c = getConnectionByPCB(tpcb); // TODO: make sure this works, if not, use arg to look up the connection
	if(c)
		handle_write(c);
	return ERR_OK;
}



err_t NetconEthernetTap::nc_recved(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	int n;
  struct pbuf* q = p;
	NetconConnection *c = getConnectionByPCB(tpcb); // TODO: make sure this works, if not, use arg as "buf sock"
	int our_fd = _phy.getDescriptor(c->sock);

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
      close(our_fd); /* TODO: Check logic */
      //nc_service->remove_connection(c);
			c->owner->close(c);
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
    if((n = write(our_fd, p->payload, p->len)) > 0) {
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
	NetconConnection *c = getConnectionByThisFD((intptr)arg);
  if(c) {
    //nc_service->remove_connection(c);
    c->owner->close(c);
		//tcp_close(c->pcb);
  }
  else {
    // can't locate connection object for PCB
  }
}

void NetconEthernetTap::nc_close(struct tcp_pcb* tpcb)
{
  //NetconConnection *c = getConnectionByPCB(tpcb);
  lwipstack->tcp_arg(tpcb, NULL);
  lwipstack->tcp_sent(tpcb, NULL);
  lwipstack->tcp_recv(tpcb, NULL);
  lwipstack->tcp_err(tpcb, NULL);
  lwipstack->tcp_poll(tpcb, NULL, 0);
  lwipstack->tcp_close(tpcb);
}

err_t NetconEthernetTap::nc_send(struct tcp_pcb *tpcb)
{
	return ERR_OK;
}

err_t NetconEthernetTap::nc_sent(void* arg, struct tcp_pcb *tpcb, u16_t len)
{
	//NetconConnection *c = _phy->getConnectionByPCB(tpcb);
	//if(c)
		//c->data_sent += len;
	return len;
}

err_t NetconEthernetTap::nc_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
	for(int i=0; i<clients.size(); i++) {
		if(clients[i].containsPCB(tpcb)) {
			send_return_value(clients[i],err);
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
  IP4_ADDR(&conn_addr, 192,168,0,2);

  int ip = connaddr->sin_addr.s_addr;
  unsigned char bytes[4];
  bytes[0] = ip & 0xFF;
  bytes[1] = (ip >> 8) & 0xFF;
  bytes[2] = (ip >> 16) & 0xFF;
  bytes[3] = (ip >> 24) & 0xFF;
  // "binding to: %d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]
  NetconConnection *c = client->getConnectionByTheirFD(bind_rpc->sockfd);
  if(c) {
    if(c->pcb->state == CLOSED){
      int err = lwipstack->tcp_bind(c->pcb, &conn_addr, conn_port);
      if(err != ERR_OK) {
        // error while binding to addr/port
      }
      else {
        // bind successful
      }
    }
    else {
      // PCB not in CLOSED state. Ignoring BIND request.
    }
  }
  else {
    // can't locate connection for PCB
  }
}

void NetconEthernetTap::handle_listen(NetconClient *client, struct listen_st *listen_rpc)
{
	NetconConnection *c = client->getConnectionByTheirFD(listen_rpc->sockfd);
  if(c) {
    if(c->pcb->state == LISTEN) {
      // PCB is already in listening state.
      return;
    }
    struct tcp_pcb* listening_pcb = lwipstack->tcp_listen(c->pcb);
    if(listening_pcb != NULL) {
      c->pcb = listening_pcb;
      lwipstack->tcp_accept(listening_pcb, nc_accept);
			int our_fd = _phy.getDescriptor(c->sock);
      lwipstack->tcp_arg(listening_pcb, (void*)(intptr_t)our_fd);
      client->waiting_for_retval=true;
    }
    else {
      // unable to allocate memory for new listening PCB
    }
  }
  else {
    // can't locate connection for PCB
  }
}

void NetconEthernetTap::handle_retval(NetconClient *client, unsigned char* buf)
{
	if(client->unmapped_conn != NULL) {
		memcpy(&(client->unmapped_conn->their_fd), &buf[1], sizeof(int));
		client->unmapped_conn = NULL;
	}
}

void NetconEthernetTap::handle_socket(NetconClient *client, struct socket_st* socket_rpc)
{
	struct tcp_pcb *pcb = lwipstack->tcp_new();
  if(pcb != NULL) {
		int *their_fd;
		NetconConnection *new_conn = client->addConnection(BUFFER, _phy.createSocketPair(*their_fd, client));
		new_conn->their_fd = *their_fd;
		new_conn->pcb = pcb;
    sock_fd_write(_phy.getDescriptor(client->rpc->sock), *their_fd);
    client->unmapped_conn = new_conn;
  }
  else {
    // Memory not available for new PCB
  }
}

void NetconEthernetTap::handle_connect(NetconClient *client, struct connect_st* connect_rpc)
{
	// FIXME: Parse out address information -- Probably a more elegant way to do this
	struct sockaddr_in *connaddr;
	connaddr = (struct sockaddr_in *) &connect_rpc->__addr;
	int conn_port = lwipstack->ntohs(connaddr->sin_port);
	ip_addr_t conn_addr = convert_ip((struct sockaddr_in *)&connect_rpc->__addr);
	NetconConnection *c = client->getConnectionByTheirFD(connect_rpc->__fd);
	int our_fd = _phy.getDescriptor(c->sock);

	if(c!= NULL) {
		lwipstack->tcp_sent(c->pcb, nc_sent); // FIXME: Move?
		lwipstack->tcp_recv(c->pcb, nc_recved);
		lwipstack->tcp_err(c->pcb, nc_err);
		lwipstack->tcp_poll(c->pcb, nc_poll, APPLICATION_POLL_FREQ);
		lwipstack->tcp_arg(c->pcb,(void*)(intptr_t)our_fd);

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
			send_return_value(client, err);
		}
		// Everything seems to be ok, but we don't have enough info to retval
		client->waiting_for_retval=true;
	}
	else {
		// could not locate PCB based on their fd
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

		if(write_allowance > 0) {
			int err = lwipstack->tcp_write(c->pcb, &c->buf, write_allowance, TCP_WRITE_FLAG_COPY);
			if(err != ERR_OK) {
				// error while writing to PCB
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
			// lwIP stack full
			return;
		}
	}
	else {
		// could not locate connection for this fd
	}
}

} // namespace ZeroTier

#endif // ZT_ENABLE_NETCON
