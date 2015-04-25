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

//============================================================
// MAC address, Ethernet header, and ARP
//============================================================

#pragma pack(1)

#define IP_HEADER_SIZE 20
#define IPV6_HEADER_SIZE 40

#define MACADDR_SIZE    6
typedef unsigned char MACADDR[MACADDR_SIZE];

typedef unsigned long IPADDR;
typedef unsigned char IPV6ADDR[16];

//-----------------
// Ethernet address
//-----------------

typedef struct {
  MACADDR addr;
} ETH_ADDR;

typedef struct {
  ETH_ADDR list[TAP_MAX_MCAST_LIST];
} MC_LIST;


// BUGBUG!!! Consider using ststem defines in netiodef.h!!!

//----------------
// Ethernet header
//----------------
typedef struct
{
    MACADDR dest;               /* destination eth addr	*/
    MACADDR src;                /* source ether addr	*/
    USHORT proto;               /* packet type ID field	*/
} ETH_HEADER, *PETH_HEADER;

//----------------
// ARP packet
//----------------

typedef struct
   {
    MACADDR        m_MAC_Destination;        // Reverse these two
    MACADDR        m_MAC_Source;             // to answer ARP requests
    USHORT         m_Proto;                  // 0x0806

#   define MAC_ADDR_TYPE 0x0001
    USHORT         m_MAC_AddressType;        // 0x0001

    USHORT         m_PROTO_AddressType;      // 0x0800
    UCHAR          m_MAC_AddressSize;        // 0x06
    UCHAR          m_PROTO_AddressSize;      // 0x04

#   define ARP_REQUEST 0x0001
#   define ARP_REPLY   0x0002
    USHORT         m_ARP_Operation;          // 0x0001 for ARP request, 0x0002 for ARP reply

    MACADDR        m_ARP_MAC_Source;
    IPADDR         m_ARP_IP_Source;
    MACADDR        m_ARP_MAC_Destination;
    IPADDR         m_ARP_IP_Destination;
   }
ARP_PACKET, *PARP_PACKET;

//----------
// IP Header
//----------

typedef struct {
# define IPH_GET_VER(v) (((v) >> 4) & 0x0F)
# define IPH_GET_LEN(v) (((v) & 0x0F) << 2)
  UCHAR    version_len;

  UCHAR    tos;
  USHORT   tot_len;
  USHORT   id;

# define IP_OFFMASK 0x1fff
  USHORT   frag_off;

  UCHAR    ttl;

# define IPPROTO_UDP  17  /* UDP protocol */
# define IPPROTO_TCP   6  /* TCP protocol */
# define IPPROTO_ICMP  1  /* ICMP protocol */
# define IPPROTO_IGMP  2  /* IGMP protocol */
  UCHAR    protocol;

  USHORT   check;
  ULONG    saddr;
  ULONG    daddr;
  /* The options start here. */
} IPHDR;

//-----------
// UDP header
//-----------

typedef struct {
  USHORT   source;
  USHORT   dest;
  USHORT   len;
  USHORT   check;
} UDPHDR;

//--------------------------
// TCP header, per RFC 793.
//--------------------------

typedef struct {
  USHORT      source;    /* source port */
  USHORT      dest;      /* destination port */
  ULONG       seq;       /* sequence number */
  ULONG       ack_seq;   /* acknowledgement number */

# define TCPH_GET_DOFF(d) (((d) & 0xF0) >> 2)
  UCHAR       doff_res;

# define TCPH_FIN_MASK (1<<0)
# define TCPH_SYN_MASK (1<<1)
# define TCPH_RST_MASK (1<<2)
# define TCPH_PSH_MASK (1<<3)
# define TCPH_ACK_MASK (1<<4)
# define TCPH_URG_MASK (1<<5)
# define TCPH_ECE_MASK (1<<6)
# define TCPH_CWR_MASK (1<<7)
  UCHAR       flags;

  USHORT      window;
  USHORT      check;
  USHORT      urg_ptr;
} TCPHDR;

#define	TCPOPT_EOL     0
#define	TCPOPT_NOP     1
#define	TCPOPT_MAXSEG  2
#define TCPOLEN_MAXSEG 4

//------------
// IPv6 Header
//------------

typedef struct {
  UCHAR    version_prio;
  UCHAR    flow_lbl[3];
  USHORT   payload_len;
# define IPPROTO_ICMPV6  0x3a  /* ICMP protocol v6 */
  UCHAR    nexthdr;
  UCHAR    hop_limit;
  IPV6ADDR saddr;
  IPV6ADDR daddr;
} IPV6HDR;

//--------------------------------------------
// IPCMPv6 NS/NA Packets (RFC4443 and RFC4861)
//--------------------------------------------

// Neighbor Solictiation - RFC 4861, 4.3
// (this is just the ICMPv6 part of the packet)
typedef struct {
  UCHAR    type;
# define ICMPV6_TYPE_NS	135		// neighbour solicitation
  UCHAR    code;
# define ICMPV6_CODE_0	0		// no specific sub-code for NS/NA
  USHORT   checksum;
  ULONG    reserved;
  IPV6ADDR target_addr;
} ICMPV6_NS;

// Neighbor Advertisement - RFC 4861, 4.4 + 4.6/4.6.1
// (this is just the ICMPv6 payload)
typedef struct {
  UCHAR    type;
# define ICMPV6_TYPE_NA	136		// neighbour advertisement
  UCHAR    code;
# define ICMPV6_CODE_0	0		// no specific sub-code for NS/NA
  USHORT   checksum;
  UCHAR    rso_bits;			// Router(0), Solicited(2), Ovrrd(4)
  UCHAR	   reserved[3];
  IPV6ADDR target_addr;
// always include "Target Link-layer Address" option (RFC 4861 4.6.1)
  UCHAR    opt_type;
#define ICMPV6_OPTION_TLLA 2
  UCHAR    opt_length;
#define ICMPV6_LENGTH_TLLA 1		// multiplied by 8 -> 1 = 8 bytes
  MACADDR  target_macaddr;
} ICMPV6_NA;

// this is the complete packet with Ethernet and IPv6 headers
typedef struct {
  ETH_HEADER eth;
  IPV6HDR    ipv6;
  ICMPV6_NA  icmpv6;
} ICMPV6_NA_PKT;

#pragma pack()
