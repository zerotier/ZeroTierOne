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

//-----------------
// DEBUGGING OUTPUT
//-----------------

const char *g_LastErrorFilename;
int g_LastErrorLineNumber;

#if DBG

DebugOutput g_Debug;

BOOLEAN
NewlineExists (const char *str, int len)
{
    while (len-- > 0)
    {
        const char c = *str++;
        if (c == '\n')
            return TRUE;
        else if (c == '\0')
            break;
    }
    return FALSE;
}

VOID
MyDebugInit (unsigned int bufsiz)
{
    NdisZeroMemory (&g_Debug, sizeof (g_Debug));
    g_Debug.text = (char *) MemAlloc (bufsiz, FALSE);

    if (g_Debug.text)
    {
        g_Debug.capacity = bufsiz;
    }
}

VOID
MyDebugFree ()
{
    if (g_Debug.text)
    {
        MemFree (g_Debug.text, g_Debug.capacity);
    }

    NdisZeroMemory (&g_Debug, sizeof (g_Debug));
}

VOID
MyDebugPrint (const unsigned char* format, ...)
{
    if (g_Debug.text && g_Debug.capacity > 0 && CAN_WE_PRINT)
    {
        BOOLEAN owned;
        ACQUIRE_MUTEX_ADAPTIVE (&g_Debug.lock, owned);
        if (owned)
        {
            const int remaining = (int)g_Debug.capacity - (int)g_Debug.out;

            if (remaining > 0)
            {
                va_list args;
                NTSTATUS status;
                char *end;

#ifdef DBG_PRINT
                va_start (args, format);
                vDbgPrintEx (DPFLTR_IHVNETWORK_ID, DPFLTR_INFO_LEVEL, format, args);
                va_end (args);
#endif
                va_start (args, format);
                status = RtlStringCchVPrintfExA (g_Debug.text + g_Debug.out,
                    remaining,
                    &end,
                    NULL,
                    STRSAFE_NO_TRUNCATION | STRSAFE_IGNORE_NULLS,
                    format,
                    args);
                va_end (args);
                va_start (args, format);
                vDbgPrintEx(DPFLTR_IHVDRIVER_ID , 1, format, args);
                va_end (args);
                if (status == STATUS_SUCCESS)
                    g_Debug.out = (unsigned int) (end - g_Debug.text);
                else
                    g_Debug.error = TRUE;
            }
            else
                g_Debug.error = TRUE;

            RELEASE_MUTEX (&g_Debug.lock);
        }
        else
            g_Debug.error = TRUE;
    }
}

BOOLEAN
GetDebugLine (
    __in char *buf,
    __in const int len
    )
{
    static const char *truncated = "[OUTPUT TRUNCATED]\n";
    BOOLEAN ret = FALSE;

    NdisZeroMemory (buf, len);

    if (g_Debug.text && g_Debug.capacity > 0)
    {
        BOOLEAN owned;
        ACQUIRE_MUTEX_ADAPTIVE (&g_Debug.lock, owned);
        if (owned)
        {
            int i = 0;

            if (g_Debug.error || NewlineExists (g_Debug.text + g_Debug.in, (int)g_Debug.out - (int)g_Debug.in))
            {
                while (i < (len - 1) && g_Debug.in < g_Debug.out)
                {
                    const char c = g_Debug.text[g_Debug.in++];
                    if (c == '\n')
                        break;
                    buf[i++] = c;
                }
                if (i < len)
                    buf[i] = '\0';
            }

            if (!i)
            {
                if (g_Debug.in == g_Debug.out)
                {
                    g_Debug.in = g_Debug.out = 0;
                    if (g_Debug.error)
                    {
                        const unsigned int tlen = strlen (truncated);
                        if (tlen < g_Debug.capacity)
                        {
                            NdisMoveMemory (g_Debug.text, truncated, tlen+1);
                            g_Debug.out = tlen;
                        }
                        g_Debug.error = FALSE;
                    }
                }
            }
            else
                ret = TRUE;

            RELEASE_MUTEX (&g_Debug.lock);
        }      
    }
    return ret;
}

VOID
PrMac (const MACADDR mac)
{
  DEBUGP (("%x:%x:%x:%x:%x:%x",
	    mac[0], mac[1], mac[2],
	    mac[3], mac[4], mac[5]));
}

VOID
PrIP (IPADDR ip_addr)
{
  const unsigned char *ip = (const unsigned char *) &ip_addr;

  DEBUGP (("%d.%d.%d.%d",
	    ip[0], ip[1], ip[2], ip[3]));
}

const char *
PrIPProto (int proto)
{
    switch (proto)
    {
    case IPPROTO_UDP:
        return "UDP";

    case IPPROTO_TCP:
        return "TCP";

    case IPPROTO_ICMP:
        return "ICMP";

    case IPPROTO_IGMP:
        return "IGMP";

    default:
        return "???";
    }
}

VOID
DumpARP (const char *prefix, const ARP_PACKET *arp)
{
  DEBUGP (("%s ARP src=", prefix));
  PrMac (arp->m_MAC_Source);
  DEBUGP ((" dest="));
  PrMac (arp->m_MAC_Destination);
  DEBUGP ((" OP=0x%04x",
	    (int)ntohs(arp->m_ARP_Operation)));
  DEBUGP ((" M=0x%04x(%d)",
	    (int)ntohs(arp->m_MAC_AddressType),
	    (int)arp->m_MAC_AddressSize));
  DEBUGP ((" P=0x%04x(%d)",
	    (int)ntohs(arp->m_PROTO_AddressType),
	    (int)arp->m_PROTO_AddressSize));

  DEBUGP ((" MacSrc="));
  PrMac (arp->m_ARP_MAC_Source);
  DEBUGP ((" MacDest="));
  PrMac (arp->m_ARP_MAC_Destination);

  DEBUGP ((" IPSrc="));
  PrIP (arp->m_ARP_IP_Source);
  DEBUGP ((" IPDest="));
  PrIP (arp->m_ARP_IP_Destination);

  DEBUGP (("\n"));
}

struct ethpayload
{
  ETH_HEADER eth;
  UCHAR payload[DEFAULT_PACKET_LOOKAHEAD];
};

#ifdef ALLOW_PACKET_DUMP

VOID
DumpPacket2(
    __in const char *prefix,
    __in const ETH_HEADER *eth,
    __in const unsigned char *data,
    __in unsigned int len
    )
{
    struct ethpayload *ep = (struct ethpayload *) MemAlloc (sizeof (struct ethpayload), TRUE);
    if (ep)
    {
        if (len > DEFAULT_PACKET_LOOKAHEAD)
            len = DEFAULT_PACKET_LOOKAHEAD;
        ep->eth = *eth;
        NdisMoveMemory (ep->payload, data, len);
        DumpPacket (prefix, (unsigned char *) ep, sizeof (ETH_HEADER) + len);
        MemFree (ep, sizeof (struct ethpayload));
    }
}

VOID
DumpPacket(
    __in const char *prefix,
    __in const unsigned char *data,
    __in unsigned int len
    )
{
    const ETH_HEADER *eth = (const ETH_HEADER *) data;
    const IPHDR *ip = (const IPHDR *) (data + sizeof (ETH_HEADER));

    if (len < sizeof (ETH_HEADER))
    {
        DEBUGP (("%s TRUNCATED PACKET LEN=%d\n", prefix, len));
        return;
    }

    // ARP Packet?
    if (len >= sizeof (ARP_PACKET) && eth->proto == htons (ETH_P_ARP))
    {
        DumpARP (prefix, (const ARP_PACKET *) data);
        return;
    }

    // IPv4 packet?
    if (len >= (sizeof (IPHDR) + sizeof (ETH_HEADER))
        && eth->proto == htons (ETH_P_IP)
        && IPH_GET_VER (ip->version_len) == 4)
    {
        const int hlen = IPH_GET_LEN (ip->version_len);
        const int blen = len - sizeof (ETH_HEADER);
        BOOLEAN did = FALSE;

        DEBUGP (("%s IPv4 %s[%d]", prefix, PrIPProto (ip->protocol), len));

        if (!(ntohs (ip->tot_len) == blen && hlen <= blen))
        {
            DEBUGP ((" XXX"));
            return;
        }

        // TCP packet?
        if (ip->protocol == IPPROTO_TCP
            && blen - hlen >= (sizeof (TCPHDR)))
        {
            const TCPHDR *tcp = (TCPHDR *) (data + sizeof (ETH_HEADER) + hlen);
            DEBUGP ((" "));
            PrIP (ip->saddr);
            DEBUGP ((":%d", ntohs (tcp->source)));
            DEBUGP ((" -> "));
            PrIP (ip->daddr);
            DEBUGP ((":%d", ntohs (tcp->dest)));
            did = TRUE;
        }

        // UDP packet?
        else if ((ntohs (ip->frag_off) & IP_OFFMASK) == 0
            && ip->protocol == IPPROTO_UDP
            && blen - hlen >= (sizeof (UDPHDR)))
        {
            const UDPHDR *udp = (UDPHDR *) (data + sizeof (ETH_HEADER) + hlen);

            // DHCP packet?
            if ((udp->dest == htons (BOOTPC_PORT) || udp->dest == htons (BOOTPS_PORT))
                && blen - hlen >= (sizeof (UDPHDR) + sizeof (DHCP)))
            {
                const DHCP *dhcp = (DHCP *) (data
                    + hlen
                    + sizeof (ETH_HEADER)
                    + sizeof (UDPHDR));

                int optlen = len
                    - sizeof (ETH_HEADER)
                    - hlen
                    - sizeof (UDPHDR)
                    - sizeof (DHCP);

                if (optlen < 0)
                    optlen = 0;

                DumpDHCP (eth, ip, udp, dhcp, optlen);
                did = TRUE;
            }

            if (!did)
            {
                DEBUGP ((" "));
                PrIP (ip->saddr);
                DEBUGP ((":%d", ntohs (udp->source)));
                DEBUGP ((" -> "));
                PrIP (ip->daddr);
                DEBUGP ((":%d", ntohs (udp->dest)));
                did = TRUE;
            }
        }

        if (!did)
        {
            DEBUGP ((" ipproto=%d ", ip->protocol));
            PrIP (ip->saddr);
            DEBUGP ((" -> "));
            PrIP (ip->daddr);
        }

        DEBUGP (("\n"));
        return;
    }

    {
        DEBUGP (("%s ??? src=", prefix));
        PrMac (eth->src);
        DEBUGP ((" dest="));
        PrMac (eth->dest);
        DEBUGP ((" proto=0x%04x len=%d\n",
            (int) ntohs(eth->proto),
            len));
    }
}

#endif // ALLOW_PACKET_DUMP

#endif
