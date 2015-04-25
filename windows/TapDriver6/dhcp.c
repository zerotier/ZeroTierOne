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

#include "tap.h"

//=========================
// Code to set DHCP options
//=========================

VOID
SetDHCPOpt(
    __in DHCPMsg *m,
    __in void *data,
    __in unsigned int len
    )
{
    if (!m->overflow)
    {
        if (m->optlen + len <= DHCP_OPTIONS_BUFFER_SIZE)
        {
            if (len)
            {
                NdisMoveMemory (m->msg.options + m->optlen, data, len);
                m->optlen += len;
            }
        }
        else
        {
            m->overflow = TRUE;
        }
    }
}

VOID
SetDHCPOpt0(
    __in DHCPMsg *msg,
    __in int type
    )
{
    DHCPOPT0 opt;
    opt.type = (UCHAR) type;
    SetDHCPOpt (msg, &opt, sizeof (opt));
}

VOID
SetDHCPOpt8(
    __in DHCPMsg *msg,
    __in int type,
    __in ULONG data
    )
{
    DHCPOPT8 opt;
    opt.type = (UCHAR) type;
    opt.len = sizeof (opt.data);
    opt.data = (UCHAR) data;
    SetDHCPOpt (msg, &opt, sizeof (opt));
}

VOID
SetDHCPOpt32(
    __in DHCPMsg *msg,
    __in int type,
    __in ULONG data
    )
{
    DHCPOPT32 opt;
    opt.type = (UCHAR) type;
    opt.len = sizeof (opt.data);
    opt.data = data;
    SetDHCPOpt (msg, &opt, sizeof (opt));
}

//==============
// Checksum code
//==============

USHORT
ip_checksum(
    __in const UCHAR *buf,
    __in const int len_ip_header
    )
{
    USHORT word16;
    ULONG sum = 0;
    int i;

    // make 16 bit words out of every two adjacent 8 bit words in the packet
    // and add them up
    for (i = 0; i < len_ip_header - 1; i += 2)
    {
        word16 = ((buf[i] << 8) & 0xFF00) + (buf[i+1] & 0xFF);
        sum += (ULONG) word16;
    }

    // take only 16 bits out of the 32 bit sum and add up the carries
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // one's complement the result
    return ((USHORT) ~sum);
}

USHORT
udp_checksum (
    __in const UCHAR *buf,
    __in const int len_udp,
    __in const UCHAR *src_addr,
    __in const UCHAR *dest_addr
    )
{
    USHORT word16;
    ULONG sum = 0;
    int i;

    // make 16 bit words out of every two adjacent 8 bit words and 
    // calculate the sum of all 16 bit words
    for (i = 0; i < len_udp; i += 2)
    {
        word16 = ((buf[i] << 8) & 0xFF00) + ((i + 1 < len_udp) ? (buf[i+1] & 0xFF) : 0);
        sum += word16;
    }

    // add the UDP pseudo header which contains the IP source and destination addresses
    for (i = 0; i < 4; i += 2)
    {
        word16 =((src_addr[i] << 8) & 0xFF00) + (src_addr[i+1] & 0xFF);
        sum += word16;
    }

    for (i = 0; i < 4; i += 2)
    {
        word16 =((dest_addr[i] << 8) & 0xFF00) + (dest_addr[i+1] & 0xFF);
        sum += word16; 	
    }

    // the protocol number and the length of the UDP packet
    sum += (USHORT) IPPROTO_UDP + (USHORT) len_udp;

    // keep only the last 16 bits of the 32 bit calculated sum and add the carries
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // Take the one's complement of sum
    return ((USHORT) ~sum);
}

//================================
// Set IP and UDP packet checksums
//================================

VOID
SetChecksumDHCPMsg(
    __in DHCPMsg *m
    )
{
    // Set IP checksum
    m->msg.pre.ip.check = htons (ip_checksum ((UCHAR *) &m->msg.pre.ip, sizeof (IPHDR)));

    // Set UDP Checksum
    m->msg.pre.udp.check = htons (udp_checksum ((UCHAR *) &m->msg.pre.udp, 
        sizeof (UDPHDR) + sizeof (DHCP) + m->optlen,
        (UCHAR *)&m->msg.pre.ip.saddr,
        (UCHAR *)&m->msg.pre.ip.daddr));
}

//===================
// DHCP message tests
//===================

int
GetDHCPMessageType(
    __in const DHCP *dhcp,
    __in const int optlen
    )
{
    const UCHAR *p = (UCHAR *) (dhcp + 1);
    int i;

    for (i = 0; i < optlen; ++i)
    {
        const UCHAR type = p[i];
        const int room = optlen - i - 1;

        if (type == DHCP_END)           // didn't find what we were looking for
            return -1;
        else if (type == DHCP_PAD)      // no-operation
            ;
        else if (type == DHCP_MSG_TYPE) // what we are looking for
        {
            if (room >= 2)
            {
                if (p[i+1] == 1)        // message length should be 1
                    return p[i+2];        // return message type
            }
            return -1;
        }
        else                            // some other message
        {
            if (room >= 1)
            {
                const int len = p[i+1]; // get message length
                i += (len + 1);         // advance to next message
            }
        }
    }
    return -1;
}

BOOLEAN
DHCPMessageOurs (
    __in const PTAP_ADAPTER_CONTEXT Adapter,
    __in const ETH_HEADER *eth,
    __in const IPHDR *ip,
    __in const UDPHDR *udp,
    __in const DHCP *dhcp
    )
{
    // Must be UDPv4 protocol
    if (!(eth->proto == htons (NDIS_ETH_TYPE_IPV4) && ip->protocol == IPPROTO_UDP))
    {
        return FALSE;
    }

    // Source MAC must be our adapter
    if (!MAC_EQUAL (eth->src, Adapter->CurrentAddress))
    {
        return FALSE;
    }

    // Dest MAC must be either broadcast or our virtual DHCP server
    if (!(ETH_IS_BROADCAST(eth->dest)
        || MAC_EQUAL (eth->dest, Adapter->m_dhcp_server_mac)))
    {
        return FALSE;
    }

    // Port numbers must be correct
    if (!(udp->dest == htons (BOOTPS_PORT)
        && udp->source == htons (BOOTPC_PORT)))
    {
        return FALSE;
    }

    // Hardware address must be MAC addr sized
    if (!(dhcp->hlen == sizeof (MACADDR)))
    {
        return FALSE;
    }

    // Hardware address must match our adapter
    if (!MAC_EQUAL (eth->src, dhcp->chaddr))
    {
        return FALSE;
    }

    return TRUE;
}


//=====================================================
// Build all of DHCP packet except for DHCP options.
// Assume that *p has been zeroed before we are called.
//=====================================================

VOID
BuildDHCPPre (
    __in const PTAP_ADAPTER_CONTEXT Adapter,
    __inout DHCPPre *p,
    __in const ETH_HEADER *eth,
    __in const IPHDR *ip,
    __in const UDPHDR *udp,
    __in const DHCP *dhcp,
    __in const int optlen,
    __in const int type)
{
    // Should we broadcast or direct to a specific MAC / IP address?
    const BOOLEAN broadcast = (type == DHCPNAK
        || ETH_IS_BROADCAST(eth->dest));

    //
    // Build ethernet header
    //
    ETH_COPY_NETWORK_ADDRESS (p->eth.src, Adapter->m_dhcp_server_mac);

    if (broadcast)
    {
        memset(p->eth.dest,0xFF,ETH_LENGTH_OF_ADDRESS);
    }
    else
    {
        ETH_COPY_NETWORK_ADDRESS (p->eth.dest, eth->src);
    }

    p->eth.proto = htons (NDIS_ETH_TYPE_IPV4);

    //
    // Build IP header
    //
    p->ip.version_len = (4 << 4) | (sizeof (IPHDR) >> 2);
    p->ip.tos = 0;
    p->ip.tot_len = htons (sizeof (IPHDR) + sizeof (UDPHDR) + sizeof (DHCP) + optlen);
    p->ip.id = 0;
    p->ip.frag_off = 0;
    p->ip.ttl = 16;
    p->ip.protocol = IPPROTO_UDP;
    p->ip.check = 0;
    p->ip.saddr = Adapter->m_dhcp_server_ip;

    if (broadcast)
    {
        p->ip.daddr = ~0;
    }
    else
    {
        p->ip.daddr = Adapter->m_dhcp_addr;
    }

    //
    // Build UDP header
    //
    p->udp.source = htons (BOOTPS_PORT);
    p->udp.dest = htons (BOOTPC_PORT);
    p->udp.len = htons (sizeof (UDPHDR) + sizeof (DHCP) + optlen);
    p->udp.check = 0;

    // Build DHCP response

    p->dhcp.op = BOOTREPLY;
    p->dhcp.htype = 1;
    p->dhcp.hlen = sizeof (MACADDR);
    p->dhcp.hops = 0;
    p->dhcp.xid = dhcp->xid;
    p->dhcp.secs = 0;
    p->dhcp.flags = 0;
    p->dhcp.ciaddr = 0;

    if (type == DHCPNAK)
    {
        p->dhcp.yiaddr = 0;
    }
    else
    {
        p->dhcp.yiaddr = Adapter->m_dhcp_addr;
    }

    p->dhcp.siaddr = Adapter->m_dhcp_server_ip;
    p->dhcp.giaddr = 0;
    ETH_COPY_NETWORK_ADDRESS (p->dhcp.chaddr, eth->src);
    p->dhcp.magic = htonl (0x63825363);
}

//=============================
// Build specific DHCP messages
//=============================

VOID
SendDHCPMsg(
    __in PTAP_ADAPTER_CONTEXT   Adapter,
    __in const int type,
    __in const ETH_HEADER *eth,
    __in const IPHDR *ip,
    __in const UDPHDR *udp,
    __in const DHCP *dhcp
    )
{
    DHCPMsg *pkt;

    if (!(type == DHCPOFFER || type == DHCPACK || type == DHCPNAK))
    {
        DEBUGP (("[TAP] SendDHCPMsg: Bad DHCP type: %d\n", type));
        return;
    }

    pkt = (DHCPMsg *) MemAlloc (sizeof (DHCPMsg), TRUE);

    if(pkt)
    {
        //-----------------------
        // Build DHCP options
        //-----------------------

        // Message Type
        SetDHCPOpt8 (pkt, DHCP_MSG_TYPE, type);

        // Server ID
        SetDHCPOpt32 (pkt, DHCP_SERVER_ID, Adapter->m_dhcp_server_ip);

        if (type == DHCPOFFER || type == DHCPACK)
        {
            // Lease Time
            SetDHCPOpt32 (pkt, DHCP_LEASE_TIME, htonl (Adapter->m_dhcp_lease_time));

            // Netmask
            SetDHCPOpt32 (pkt, DHCP_NETMASK, Adapter->m_dhcp_netmask);

            // Other user-defined options
            SetDHCPOpt (
                pkt,
                Adapter->m_dhcp_user_supplied_options_buffer,
                Adapter->m_dhcp_user_supplied_options_buffer_len);
        }

        // End
        SetDHCPOpt0 (pkt, DHCP_END);

        if (!DHCPMSG_OVERFLOW (pkt))
        {
            // The initial part of the DHCP message (not including options) gets built here
            BuildDHCPPre (
                Adapter,
                &pkt->msg.pre,
                eth,
                ip,
                udp,
                dhcp,
                DHCPMSG_LEN_OPT (pkt),
                type);

            SetChecksumDHCPMsg (pkt);

            DUMP_PACKET ("DHCPMsg",
                DHCPMSG_BUF (pkt),
                DHCPMSG_LEN_FULL (pkt));

            // Return DHCP response to kernel
            IndicateReceivePacket(
                Adapter,
                DHCPMSG_BUF (pkt),
                DHCPMSG_LEN_FULL (pkt)
                );
        }
        else
        {
            DEBUGP (("[TAP] SendDHCPMsg: DHCP buffer overflow\n"));
        }

        MemFree (pkt, sizeof (DHCPMsg));
    }
}

//===================================================================
// Handle a BOOTPS packet produced by the local system to
// resolve the address/netmask of this adapter.
// If we are in TAP_WIN_IOCTL_CONFIG_DHCP_MASQ mode, reply
// to the message.  Return TRUE if we processed the passed
// message, so that downstream stages can ignore it.
//===================================================================

BOOLEAN
ProcessDHCP(
    __in PTAP_ADAPTER_CONTEXT   Adapter,
    __in const ETH_HEADER *eth,
    __in const IPHDR *ip,
    __in const UDPHDR *udp,
    __in const DHCP *dhcp,
    __in int optlen
    )
{
    int msg_type;

    // Sanity check IP header
    if (!(ntohs (ip->tot_len) == sizeof (IPHDR) + sizeof (UDPHDR) + sizeof (DHCP) + optlen
        && (ntohs (ip->frag_off) & IP_OFFMASK) == 0))
    {
        return TRUE;
    }

    // Does this message belong to us?
    if (!DHCPMessageOurs (Adapter, eth, ip, udp, dhcp))
    {
        return FALSE;
    }

    msg_type = GetDHCPMessageType (dhcp, optlen);

    // Drop non-BOOTREQUEST messages
    if (dhcp->op != BOOTREQUEST)
    {
        return TRUE;
    }

    // Drop any messages except DHCPDISCOVER or DHCPREQUEST
    if (!(msg_type == DHCPDISCOVER || msg_type == DHCPREQUEST))
    {
        return TRUE;
    }

    // Should we reply with DHCPOFFER, DHCPACK, or DHCPNAK?
    if (msg_type == DHCPREQUEST
        && ((dhcp->ciaddr && dhcp->ciaddr != Adapter->m_dhcp_addr)
        || !Adapter->m_dhcp_received_discover
        || Adapter->m_dhcp_bad_requests >= BAD_DHCPREQUEST_NAK_THRESHOLD))
    {
        SendDHCPMsg(
            Adapter,
            DHCPNAK,
            eth, ip, udp, dhcp
            );
    }
    else
    {
        SendDHCPMsg(
            Adapter,
            (msg_type == DHCPDISCOVER ? DHCPOFFER : DHCPACK),
            eth, ip, udp, dhcp
            );
    }

    // Remember if we received a DHCPDISCOVER
    if (msg_type == DHCPDISCOVER)
    {
        Adapter->m_dhcp_received_discover = TRUE;
    }

    // Is this a bad DHCPREQUEST?
    if (msg_type == DHCPREQUEST && dhcp->ciaddr && dhcp->ciaddr != Adapter->m_dhcp_addr)
    {
        ++Adapter->m_dhcp_bad_requests;
    }

    return TRUE;
}

#if DBG

const char *
    message_op_text (int op)
{
    switch (op)
    {
    case BOOTREQUEST:
        return "BOOTREQUEST";

    case BOOTREPLY:
        return "BOOTREPLY";

    default:
        return "???";
    }
}

const char *
    message_type_text (int type)
{
    switch (type)
    {
    case DHCPDISCOVER:
        return "DHCPDISCOVER";

    case DHCPOFFER:
        return "DHCPOFFER";

    case DHCPREQUEST:
        return "DHCPREQUEST";

    case DHCPDECLINE:
        return "DHCPDECLINE";

    case DHCPACK:
        return "DHCPACK";

    case DHCPNAK:
        return "DHCPNAK";

    case DHCPRELEASE:
        return "DHCPRELEASE";

    case DHCPINFORM:
        return "DHCPINFORM";

    default:
        return "???";
    }
}

const char *
port_name (int port)
{
    switch (port)
    {
    case BOOTPS_PORT:
        return "BOOTPS";

    case BOOTPC_PORT:
        return "BOOTPC";

    default:
        return "unknown";
    }
}

VOID
DumpDHCP (
    const ETH_HEADER *eth,
    const IPHDR *ip,
    const UDPHDR *udp,
    const DHCP *dhcp,
    const int optlen
    )
{
    DEBUGP ((" %s", message_op_text (dhcp->op)));
    DEBUGP ((" %s ", message_type_text (GetDHCPMessageType (dhcp, optlen))));
    PrIP (ip->saddr);
    DEBUGP ((":%s[", port_name (ntohs (udp->source))));
    PrMac (eth->src);
    DEBUGP (("] -> "));
    PrIP (ip->daddr);
    DEBUGP ((":%s[", port_name (ntohs (udp->dest))));
    PrMac (eth->dest);
    DEBUGP (("]"));
    if (dhcp->ciaddr)
    {
        DEBUGP ((" ci="));
        PrIP (dhcp->ciaddr);
    }
    if (dhcp->yiaddr)
    {
        DEBUGP ((" yi="));
        PrIP (dhcp->yiaddr);
    }
    if (dhcp->siaddr)
    {
        DEBUGP ((" si="));
        PrIP (dhcp->siaddr);
    }
    if (dhcp->hlen == sizeof (MACADDR))
    {
        DEBUGP ((" ch="));
        PrMac (dhcp->chaddr);
    }

    DEBUGP ((" xid=0x%08x", ntohl (dhcp->xid)));

    if (ntohl (dhcp->magic) != 0x63825363)
        DEBUGP ((" ma=0x%08x", ntohl (dhcp->magic)));
    if (dhcp->htype != 1)
        DEBUGP ((" htype=%d", dhcp->htype));
    if (dhcp->hops)
        DEBUGP ((" hops=%d", dhcp->hops));
    if (ntohs (dhcp->secs))
        DEBUGP ((" secs=%d", ntohs (dhcp->secs)));
    if (ntohs (dhcp->flags))
        DEBUGP ((" flags=0x%04x", ntohs (dhcp->flags)));

    // extra stuff

    if (ip->version_len != 0x45)
        DEBUGP ((" vl=0x%02x", ip->version_len));
    if (ntohs (ip->tot_len) != sizeof (IPHDR) + sizeof (UDPHDR) + sizeof (DHCP) + optlen)
        DEBUGP ((" tl=%d", ntohs (ip->tot_len)));
    if (ntohs (udp->len) != sizeof (UDPHDR) + sizeof (DHCP) + optlen)
        DEBUGP ((" ul=%d", ntohs (udp->len)));

    if (ip->tos)
        DEBUGP ((" tos=0x%02x", ip->tos));
    if (ntohs (ip->id))
        DEBUGP ((" id=0x%04x", ntohs (ip->id)));
    if (ntohs (ip->frag_off))
        DEBUGP ((" frag_off=0x%04x", ntohs (ip->frag_off)));

    DEBUGP ((" ttl=%d", ip->ttl));
    DEBUGP ((" ic=0x%04x [0x%04x]", ntohs (ip->check),
        ip_checksum ((UCHAR*)ip, sizeof (IPHDR))));
    DEBUGP ((" uc=0x%04x [0x%04x/%d]", ntohs (udp->check),
        udp_checksum ((UCHAR *) udp,
        sizeof (UDPHDR) + sizeof (DHCP) + optlen,
        (UCHAR *) &ip->saddr,
        (UCHAR *) &ip->daddr),
        optlen));

    // Options
    {
        const UCHAR *opt = (UCHAR *) (dhcp + 1);
        int i;

        DEBUGP ((" OPT"));
        for (i = 0; i < optlen; ++i)
        {
            const UCHAR data = opt[i];
            DEBUGP ((".%d", data));
        }
    }
}

#endif /* DBG */
