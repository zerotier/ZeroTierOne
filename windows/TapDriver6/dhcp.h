/*
 *  TAP-Windows -- A kernel driver to provide virtual tap
 *                 device functionality on Windows.
 *
 *  This code was inspired by the CIPE-Win32 driver by Damion K. Wilson.
 *
 *  This source code is Copyright (C) 2002-2014 OpenVPN Technologies, Inc.,
 *  and is released under the GPL version 2 (see below).
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#pragma once

#pragma pack(1)

//===================================================
// How many bad DHCPREQUESTs do we receive before we
// return a NAK?
//
// A bad DHCPREQUEST is defined to be one where the
// requestor doesn't know its IP address.
//===================================================

#define BAD_DHCPREQUEST_NAK_THRESHOLD 3

//==============================================
// Maximum number of DHCP options bytes supplied
//==============================================

#define DHCP_USER_SUPPLIED_OPTIONS_BUFFER_SIZE 256
#define DHCP_OPTIONS_BUFFER_SIZE               256

//===================================
// UDP port numbers of DHCP messages.
//===================================

#define BOOTPS_PORT 67
#define BOOTPC_PORT 68

//===========================
// The DHCP message structure
//===========================

typedef struct {
# define BOOTREQUEST 1
# define BOOTREPLY   2
  UCHAR op;          /* message op */

  UCHAR  htype;      /* hardware address type (e.g. '1' = 10Mb Ethernet) */
  UCHAR  hlen;       /* hardware address length (e.g. '6' for 10Mb Ethernet) */
  UCHAR  hops;       /* client sets to 0, may be used by relay agents */
  ULONG  xid;        /* transaction ID, chosen by client */
  USHORT secs;       /* seconds since request process began, set by client */
  USHORT flags;
  ULONG  ciaddr;     /* client IP address, client sets if known */
  ULONG  yiaddr;     /* 'your' IP address -- server's response to client */
  ULONG  siaddr;     /* server IP address */
  ULONG  giaddr;     /* relay agent IP address */
  UCHAR  chaddr[16]; /* client hardware address */
  UCHAR  sname[64];  /* optional server host name */
  UCHAR  file[128];  /* boot file name */
  ULONG  magic;      /* must be 0x63825363 (network order) */
} DHCP;

typedef struct {
  ETH_HEADER eth;
  IPHDR ip;
  UDPHDR udp;
  DHCP dhcp;
} DHCPPre;

typedef struct {
  DHCPPre pre;
  UCHAR options[DHCP_OPTIONS_BUFFER_SIZE];
} DHCPFull;

typedef struct {
  unsigned int optlen;
  BOOLEAN overflow;
  DHCPFull msg;
} DHCPMsg;

//===================
// Macros for DHCPMSG
//===================

#define DHCPMSG_LEN_BASE(p) (sizeof (DHCPPre))
#define DHCPMSG_LEN_OPT(p)  ((p)->optlen)
#define DHCPMSG_LEN_FULL(p) (DHCPMSG_LEN_BASE(p) + DHCPMSG_LEN_OPT(p))
#define DHCPMSG_BUF(p)      ((UCHAR*) &(p)->msg)
#define DHCPMSG_OVERFLOW(p) ((p)->overflow)

//========================================
// structs to hold individual DHCP options
//========================================

typedef struct {
  UCHAR type;
} DHCPOPT0;

typedef struct {
  UCHAR type;
  UCHAR len;
  UCHAR data;
} DHCPOPT8;

typedef struct {
  UCHAR type;
  UCHAR len;
  ULONG data;
} DHCPOPT32;

#pragma pack()

//==================
// DHCP Option types
//==================

#define DHCP_MSG_TYPE    53  /* message type (u8) */
#define DHCP_PARM_REQ    55  /* parameter request list: c1 (u8), ... */
#define DHCP_CLIENT_ID   61  /* client ID: type (u8), i1 (u8), ... */
#define DHCP_IP          50  /* requested IP addr (u32) */
#define DHCP_NETMASK      1  /* subnet mask (u32) */
#define DHCP_LEASE_TIME  51  /* lease time sec (u32) */
#define DHCP_RENEW_TIME  58  /* renewal time sec (u32) */
#define DHCP_REBIND_TIME 59  /* rebind time sec (u32) */
#define DHCP_SERVER_ID   54  /* server ID: IP addr (u32) */
#define DHCP_PAD          0
#define DHCP_END        255

//====================
// DHCP Messages types
//====================

#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPDECLINE  4
#define DHCPACK      5
#define DHCPNAK      6
#define DHCPRELEASE  7
#define DHCPINFORM   8

#if DBG

VOID
DumpDHCP (const ETH_HEADER *eth,
	  const IPHDR *ip,
	  const UDPHDR *udp,
	  const DHCP *dhcp,
	  const int optlen);

#endif
