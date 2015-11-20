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


#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"

#include "../node/Mutex.hpp"

#include <stdio.h>
#include <dlfcn.h>

#ifndef LWIPSTACK_H
#define LWIPSTACK_H

#ifdef D_GNU_SOURCE
  #define _GNU_SOURCE
#endif

typedef ip_addr ip_addr_t;
struct tcp_pcb;

#define TCP_WRITE_SIG struct tcp_pcb *pcb, const void *arg, u16_t len, u8_t apiflags
#define TCP_SENT_SIG struct tcp_pcb * pcb, err_t (* sent)(void * arg, struct tcp_pcb * tpcb, u16_t len)
#define TCP_NEW_SIG void
#define TCP_SNDBUF_SIG struct tcp_pcb * pcb
#define TCP_CONNECT_SIG struct tcp_pcb * pcb, struct ip_addr * ipaddr, u16_t port, err_t (* connected)(void * arg, struct tcp_pcb * tpcb, err_t err)
#define TCP_RECV_SIG struct tcp_pcb * pcb, err_t (* recv)(void * arg, struct tcp_pcb * tpcb, struct pbuf * p, err_t err)
#define TCP_RECVED_SIG struct tcp_pcb * pcb, u16_t len
#define TCP_ERR_SIG struct tcp_pcb * pcb, void (* err)(void * arg, err_t err)
#define TCP_POLL_SIG struct tcp_pcb * pcb, err_t (* poll)(void * arg, struct tcp_pcb * tpcb), u8_t interval
#define TCP_ARG_SIG struct tcp_pcb * pcb, void * arg
#define TCP_CLOSE_SIG struct tcp_pcb * pcb
#define TCP_ABORT_SIG struct tcp_pcb * pcb
#define TCP_OUTPUT_SIG struct tcp_pcb * pcb
#define TCP_ACCEPT_SIG struct tcp_pcb * pcb, err_t (* accept)(void * arg, struct tcp_pcb * newpcb, err_t err)
#define TCP_LISTEN_SIG struct tcp_pcb * pcb
#define TCP_LISTEN_WITH_BACKLOG_SIG struct tcp_pcb * pcb, u8_t backlog
#define TCP_BIND_SIG struct tcp_pcb * pcb, struct ip_addr * ipaddr, u16_t port
#define PBUF_FREE_SIG struct pbuf *p
#define PBUF_ALLOC_SIG pbuf_layer layer, u16_t length, pbuf_type type
#define LWIP_HTONS_SIG u16_t x
#define LWIP_NTOHS_SIG u16_t x
#define IPADDR_NTOA_SIG const ip_addr_t *addr
#define ETHARP_OUTPUT_SIG struct netif *netif, struct pbuf *q, ip_addr_t *ipaddr
#define ETHERNET_INPUT_SIG struct pbuf *p, struct netif *netif
#define TCP_INPUT_SIG struct pbuf *p, struct netif *inp
#define IP_INPUT_SIG struct pbuf *p, struct netif *inp
#define NETIF_SET_DEFAULT_SIG struct netif *netif
#define NETIF_ADD_SIG struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask, ip_addr_t *gw, void *state, netif_init_fn init, netif_input_fn input
#define NETIF_SET_UP_SIG struct netif *netif
#define NETIF_POLL_SIG struct netif *netif

namespace ZeroTier {

/**
 * Loads an instance of liblwip.so in a private memory arena
 *
 * This uses dlmopen() to load an instance of the LWIP stack into its
 * own private memory space. This is done to get around the stack's
 * lack of thread-safety or multi-instance support. The alternative
 * would be to massively refactor the stack so everything lives in a
 * state object instead of static memory space.
 */
class LWIPStack
{
public:
  void *_libref;

  void (*_lwip_init)();
  err_t (*_tcp_write)(TCP_WRITE_SIG);
  void (*_tcp_sent)(TCP_SENT_SIG);
  struct tcp_pcb * (*_tcp_new)(TCP_NEW_SIG);
  u16_t (*_tcp_sndbuf)(TCP_SNDBUF_SIG);
  err_t (*_tcp_connect)(TCP_CONNECT_SIG);
  void (*_tcp_recv)(TCP_RECV_SIG);
  void (*_tcp_recved)(TCP_RECVED_SIG);
  void (*_tcp_err)(TCP_ERR_SIG);
  void (*_tcp_poll)(TCP_POLL_SIG);
  void (*_tcp_arg)(TCP_ARG_SIG);
  err_t (*_tcp_close)(TCP_CLOSE_SIG);
  void (*_tcp_abort)(TCP_ABORT_SIG);
  err_t (*_tcp_output)(TCP_OUTPUT_SIG);
  void (*_tcp_accept)(TCP_ACCEPT_SIG);
  struct tcp_pcb * (*_tcp_listen)(TCP_LISTEN_SIG);
  struct tcp_pcb * (*_tcp_listen_with_backlog)(TCP_LISTEN_WITH_BACKLOG_SIG);
  err_t (*_tcp_bind)(TCP_BIND_SIG);
  void (*_etharp_tmr)(void);
  void (*_tcp_tmr)(void);
  u8_t (*_pbuf_free)(PBUF_FREE_SIG);
  struct pbuf * (*_pbuf_alloc)(PBUF_ALLOC_SIG);
  u16_t (*_lwip_htons)(LWIP_HTONS_SIG);
  u16_t (*_lwip_ntohs)(LWIP_NTOHS_SIG);
  char* (*_ipaddr_ntoa)(IPADDR_NTOA_SIG);
  err_t (*_etharp_output)(ETHARP_OUTPUT_SIG);
  err_t (*_ethernet_input)(ETHERNET_INPUT_SIG);
  void (*_tcp_input)(TCP_INPUT_SIG);
  err_t (*_ip_input)(IP_INPUT_SIG);
  void (*_netif_set_default)(NETIF_SET_DEFAULT_SIG);
  struct netif * (*_netif_add)(NETIF_ADD_SIG);
  void (*_netif_set_up)(NETIF_SET_UP_SIG);
  void (*_netif_poll)(NETIF_POLL_SIG);


  Mutex _lock;

  LWIPStack(const char* path) :
    _libref(NULL)
  {
    _libref = dlmopen(LM_ID_NEWLM, path, RTLD_NOW);
    if(_libref == NULL)
      printf("dlerror(): %s\n", dlerror());

    _lwip_init = (void(*)(void))dlsym(_libref, "lwip_init");
    _tcp_write = (err_t(*)(TCP_WRITE_SIG))dlsym(_libref, "tcp_write");
    _tcp_sent = (void(*)(TCP_SENT_SIG))dlsym(_libref, "tcp_sent");
    _tcp_new = (struct tcp_pcb*(*)(TCP_NEW_SIG))dlsym(_libref, "tcp_new");
    _tcp_sndbuf = (u16_t(*)(TCP_SNDBUF_SIG))dlsym(_libref, "tcp_sndbuf");
    _tcp_connect = (err_t(*)(TCP_CONNECT_SIG))dlsym(_libref, "tcp_connect");
    _tcp_recv = (void(*)(TCP_RECV_SIG))dlsym(_libref, "tcp_recv");
    _tcp_recved = (void(*)(TCP_RECVED_SIG))dlsym(_libref, "tcp_recved");
    _tcp_err = (void(*)(TCP_ERR_SIG))dlsym(_libref, "tcp_err");
    _tcp_poll = (void(*)(TCP_POLL_SIG))dlsym(_libref, "tcp_poll");
    _tcp_arg = (void(*)(TCP_ARG_SIG))dlsym(_libref, "tcp_arg");
    _tcp_close = (err_t(*)(TCP_CLOSE_SIG))dlsym(_libref, "tcp_close");
    _tcp_abort = (void(*)(TCP_ABORT_SIG))dlsym(_libref, "tcp_abort");
    _tcp_output = (err_t(*)(TCP_OUTPUT_SIG))dlsym(_libref, "tcp_output");
    _tcp_accept = (void(*)(TCP_ACCEPT_SIG))dlsym(_libref, "tcp_accept");
    _tcp_listen = (struct tcp_pcb*(*)(TCP_LISTEN_SIG))dlsym(_libref, "tcp_listen");
    _tcp_listen_with_backlog = (struct tcp_pcb*(*)(TCP_LISTEN_WITH_BACKLOG_SIG))dlsym(_libref, "tcp_listen_with_backlog");
    _tcp_bind = (err_t(*)(TCP_BIND_SIG))dlsym(_libref, "tcp_bind");
    _etharp_tmr = (void(*)(void))dlsym(_libref, "etharp_tmr");
    _tcp_tmr = (void(*)(void))dlsym(_libref, "tcp_tmr");
    _pbuf_free = (u8_t(*)(PBUF_FREE_SIG))dlsym(_libref, "pbuf_free");
    _pbuf_alloc = (struct pbuf*(*)(PBUF_ALLOC_SIG))dlsym(_libref, "pbuf_alloc");
    _lwip_htons = (u16_t(*)(LWIP_HTONS_SIG))dlsym(_libref, "lwip_htons");
    _lwip_ntohs = (u16_t(*)(LWIP_NTOHS_SIG))dlsym(_libref, "lwip_ntohs");
    _ipaddr_ntoa = (char*(*)(IPADDR_NTOA_SIG))dlsym(_libref, "ipaddr_ntoa");
    _etharp_output = (err_t(*)(ETHARP_OUTPUT_SIG))dlsym(_libref, "etharp_output");
    _ethernet_input = (err_t(*)(ETHERNET_INPUT_SIG))dlsym(_libref, "ethernet_input");
    _tcp_input = (void(*)(TCP_INPUT_SIG))dlsym(_libref, "tcp_input");
    _ip_input = (err_t(*)(IP_INPUT_SIG))dlsym(_libref, "ip_input");
    _netif_set_default = (void(*)(NETIF_SET_DEFAULT_SIG))dlsym(_libref, "netif_set_default");
    _netif_add = (struct netif*(*)(NETIF_ADD_SIG))dlsym(_libref, "netif_add");
    _netif_set_up = (void(*)(NETIF_SET_UP_SIG))dlsym(_libref, "netif_set_up");
    _netif_poll = (void(*)(NETIF_POLL_SIG))dlsym(_libref, "netif_poll");
  }

  ~LWIPStack()
  {
    if (_libref)
      dlclose(_libref);
  }

  inline void lwip_init() throw() { Mutex::Lock _l(_lock); return _lwip_init(); }
  inline err_t tcp_write(TCP_WRITE_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_write(pcb,arg,len,apiflags); }
  inline void tcp_sent(TCP_SENT_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_sent(pcb,sent); }
  inline struct tcp_pcb * tcp_new(TCP_NEW_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_new(); }
  inline u16_t tcp_sndbuf(TCP_SNDBUF_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_sndbuf(pcb); }
  inline err_t tcp_connect(TCP_CONNECT_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_connect(pcb,ipaddr,port,connected); }
  inline void tcp_recv(TCP_RECV_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_recv(pcb,recv); }
  inline void tcp_recved(TCP_RECVED_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_recved(pcb,len); }
  inline void tcp_err(TCP_ERR_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_err(pcb,err); }
  inline void tcp_poll(TCP_POLL_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_poll(pcb,poll,interval); }
  inline void tcp_arg(TCP_ARG_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_arg(pcb,arg); }
  inline err_t tcp_close(TCP_CLOSE_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_close(pcb); }
  inline void tcp_abort(TCP_ABORT_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_abort(pcb); }
  inline err_t tcp_output(TCP_OUTPUT_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_output(pcb); }
  inline void tcp_accept(TCP_ACCEPT_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_accept(pcb,accept); }
  inline struct tcp_pcb * tcp_listen(TCP_LISTEN_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_listen(pcb); }
  inline struct tcp_pcb * tcp_listen_with_backlog(TCP_LISTEN_WITH_BACKLOG_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_listen_with_backlog(pcb,backlog); }
  inline err_t tcp_bind(TCP_BIND_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_bind(pcb,ipaddr,port); }
  inline void etharp_tmr(void) throw() { Mutex::Lock _l(_lock); return _etharp_tmr(); }
  inline void tcp_tmr(void) throw() { Mutex::Lock _l(_lock); return _tcp_tmr(); }
  inline u8_t pbuf_free(PBUF_FREE_SIG) throw() { Mutex::Lock _l(_lock); return _pbuf_free(p); }
  inline struct pbuf * pbuf_alloc(PBUF_ALLOC_SIG) throw() { Mutex::Lock _l(_lock); return _pbuf_alloc(layer,length,type); }
  inline u16_t lwip_htons(LWIP_HTONS_SIG) throw() { Mutex::Lock _l(_lock); return _lwip_htons(x); }
  inline u16_t lwip_ntohs(LWIP_NTOHS_SIG) throw() { Mutex::Lock _l(_lock); return _lwip_ntohs(x); }
  inline char* ipaddr_ntoa(IPADDR_NTOA_SIG) throw() { Mutex::Lock _l(_lock); return _ipaddr_ntoa(addr); }
  inline err_t etharp_output(ETHARP_OUTPUT_SIG) throw() { Mutex::Lock _l(_lock); return _etharp_output(netif,q,ipaddr); }
  inline err_t ethernet_input(ETHERNET_INPUT_SIG) throw() { Mutex::Lock _l(_lock); return _ethernet_input(p,netif); }
  inline void tcp_input(TCP_INPUT_SIG) throw() { Mutex::Lock _l(_lock); return _tcp_input(p,inp); }
  inline err_t ip_input(IP_INPUT_SIG) throw() { Mutex::Lock _l(_lock); return _ip_input(p,inp); }
  inline void netif_set_default(NETIF_SET_DEFAULT_SIG) throw() { Mutex::Lock _l(_lock); return _netif_set_default(netif); }
  inline struct netif * netif_add(NETIF_ADD_SIG) throw() { Mutex::Lock _l(_lock); return _netif_add(netif,ipaddr,netmask,gw,state,init,input); }
  inline void netif_set_up(NETIF_SET_UP_SIG) throw() { Mutex::Lock _l(_lock); return _netif_set_up(netif); }
  inline void netif_poll(NETIF_POLL_SIG) throw() { Mutex::Lock _l(_lock); return _netif_poll(netif); }
};

} // namespace ZeroTier

#endif
