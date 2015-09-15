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

/*
#include "lwip/timers.h"
#include "lwip/opt.h"
#include "lwip/init.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/sys.h"
#include "lwip/stats.h"
//#include "lwip/tcp_impl.h"
//#include "lwip/inet_chksum.h"
#include "lwip/tcpip.h"
//#include "lwip/ip_addr.h"
#include "lwip/debug.h"
//#include "lwip/ip.h"
//#include "lwip/ip_frag.h"
*/

#include <stdio.h>
#include <dlfcn.h>

#ifndef LWIPSTACK_H
#define LWIPSTACK_H

#ifdef D_GNU_SOURCE
  #define _GNU_SOURCE
#endif


typedef ip_addr ip_addr_t;


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
//#define TAPIF_INIT_SIG struct netif *netif
//#define TAPIF_INPUT_SIG LWIPStack* ls, struct netif *netif
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



class LWIPStack{
  void* libref;

public:

  void (*lwip_init)();
  err_t (*tcp_write)(TCP_WRITE_SIG);
  void (*tcp_sent)(TCP_SENT_SIG);
  struct tcp_pcb * (*tcp_new)(TCP_NEW_SIG);
  u16_t (*tcp_sndbuf)(TCP_SNDBUF_SIG);
  err_t (*tcp_connect)(TCP_CONNECT_SIG);
  void (*tcp_recv)(TCP_RECV_SIG);
  void (*tcp_recved)(TCP_RECVED_SIG);
  void (*tcp_err)(TCP_ERR_SIG);
  void (*tcp_poll)(TCP_POLL_SIG);
  void (*tcp_arg)(TCP_ARG_SIG);
  err_t (*tcp_close)(TCP_CLOSE_SIG);
  void (*tcp_abort)(TCP_ABORT_SIG);
  err_t (*tcp_output)(TCP_OUTPUT_SIG);
  void (*tcp_accept)(TCP_ACCEPT_SIG);
  struct tcp_pcb * (*tcp_listen)(TCP_LISTEN_SIG);
  struct tcp_pcb * (*tcp_listen_with_backlog)(TCP_LISTEN_WITH_BACKLOG_SIG);
  err_t (*tcp_bind)(TCP_BIND_SIG);
  void (*etharp_tmr)(void);
  void (*tcp_tmr)(void);
  //err_t (*tapif_init)(TAPIF_INIT_SIG);
  //void (*tapif_input)(TAPIF_INPUT_SIG);
  u8_t (*pbuf_free)(PBUF_FREE_SIG);
  struct pbuf * (*pbuf_alloc)(PBUF_ALLOC_SIG);
  u16_t (*lwip_htons)(LWIP_HTONS_SIG);
  u16_t (*lwip_ntohs)(LWIP_NTOHS_SIG);
  char* (*ipaddr_ntoa)(IPADDR_NTOA_SIG);
  err_t (*etharp_output)(ETHARP_OUTPUT_SIG);
  err_t (*ethernet_input)(ETHERNET_INPUT_SIG);
  void (*tcp_input)(TCP_INPUT_SIG);
  err_t (*ip_input)(IP_INPUT_SIG);

  void (*netif_set_default)(NETIF_SET_DEFAULT_SIG);
  struct netif * (*netif_add)(NETIF_ADD_SIG);
  void (*netif_set_up)(NETIF_SET_UP_SIG);
  void (*netif_poll)(NETIF_POLL_SIG);



  LWIPStack(const char* path)
  {
    libref = dlmopen(LM_ID_NEWLM, path, RTLD_NOW);
    if(libref == NULL)
      printf("dlerror(): %s\n", dlerror());

    /* assign function pointers to symbols in dynamically-loaded lib */
    lwip_init = (void(*)(void))dlsym(libref, "lwip_init");
    tcp_write = (err_t(*)(TCP_WRITE_SIG))dlsym(libref, "tcp_write");
    tcp_sent = (void(*)(TCP_SENT_SIG))dlsym(libref, "tcp_sent");
    tcp_new = (struct tcp_pcb*(*)(TCP_NEW_SIG))dlsym(libref, "tcp_new");
    tcp_sndbuf = (u16_t(*)(TCP_SNDBUF_SIG))dlsym(libref, "tcp_sndbuf");
    tcp_connect = (err_t(*)(TCP_CONNECT_SIG))dlsym(libref, "tcp_connect");
    tcp_recv = (void(*)(TCP_RECV_SIG))dlsym(libref, "tcp_recv");
    tcp_recved = (void(*)(TCP_RECVED_SIG))dlsym(libref, "tcp_recved");
    tcp_err = (void(*)(TCP_ERR_SIG))dlsym(libref, "tcp_err");
    tcp_poll = (void(*)(TCP_POLL_SIG))dlsym(libref, "tcp_poll");
    tcp_arg = (void(*)(TCP_ARG_SIG))dlsym(libref, "tcp_arg");
    tcp_close = (err_t(*)(TCP_CLOSE_SIG))dlsym(libref, "tcp_close");
    tcp_abort = (void(*)(TCP_ABORT_SIG))dlsym(libref, "tcp_abort");
    tcp_output = (err_t(*)(TCP_OUTPUT_SIG))dlsym(libref, "tcp_output");
    tcp_accept = (void(*)(TCP_ACCEPT_SIG))dlsym(libref, "tcp_accept");
    tcp_listen = (struct tcp_pcb*(*)(TCP_LISTEN_SIG))dlsym(libref, "tcp_listen");
    tcp_listen_with_backlog = (struct tcp_pcb*(*)(TCP_LISTEN_WITH_BACKLOG_SIG))dlsym(libref, "tcp_listen_with_backlog");
    tcp_bind = (err_t(*)(TCP_BIND_SIG))dlsym(libref, "tcp_bind");
    etharp_tmr = (void(*)(void))dlsym(libref, "etharp_tmr");
    tcp_tmr = (void(*)(void))dlsym(libref, "tcp_tmr");
    //tapif_init = (err_t(*)(TAPIF_INIT_SIG))dlsym(libref, "tapif_init");
    //tapif_input = (void(*)(TAPIF_INPUT_SIG))dlsym(libref, "tapif_input");
    pbuf_free = (u8_t(*)(PBUF_FREE_SIG))dlsym(libref, "pbuf_free");
    pbuf_alloc = (struct pbuf*(*)(PBUF_ALLOC_SIG))dlsym(libref, "pbuf_alloc");
    lwip_htons = (u16_t(*)(LWIP_HTONS_SIG))dlsym(libref, "lwip_htons");
    lwip_ntohs = (u16_t(*)(LWIP_NTOHS_SIG))dlsym(libref, "lwip_ntohs");
    ipaddr_ntoa = (char*(*)(IPADDR_NTOA_SIG))dlsym(libref, "ipaddr_ntoa");
    etharp_output = (err_t(*)(ETHARP_OUTPUT_SIG))dlsym(libref, "etharp_output");
    ethernet_input = (err_t(*)(ETHERNET_INPUT_SIG))dlsym(libref, "ethernet_input");

    tcp_input = (void(*)(TCP_INPUT_SIG))dlsym(libref, "tcp_input");
    ip_input = (err_t(*)(IP_INPUT_SIG))dlsym(libref, "ip_input");

    netif_set_default = (void(*)(NETIF_SET_DEFAULT_SIG))dlsym(libref, "netif_set_default");
    netif_add = (struct netif*(*)(NETIF_ADD_SIG))dlsym(libref, "netif_add");
    netif_set_up = (void(*)(NETIF_SET_UP_SIG))dlsym(libref, "netif_set_up");
    netif_poll = (void(*)(NETIF_POLL_SIG))dlsym(libref, "netif_poll");
  }
};

#endif
