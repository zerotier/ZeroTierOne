/*
*  TAP-Windows -- A kernel driver to provide virtual tap
*                 device functionality on Windows.
*
*  This code was inspired by the CIPE-Win32 driver by Damion K. Wilson.
*
*  This source code is Copyright (C) 2002-2010 OpenVPN Technologies, Inc.,
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

typedef unsigned char MACADDR [6];

//-----------------
// Ethernet address
//-----------------

typedef struct {
	MACADDR addr;
} ETH_ADDR;

typedef struct {
	ETH_ADDR list[NIC_MAX_MCAST_LIST];
} MC_LIST;

//----------------
// Ethernet header
//----------------

typedef struct
{
	MACADDR dest;               /* destination eth addr	*/
	MACADDR src;                /* source ether addr	*/

# define ETH_P_IP   0x0800    /* IPv4 protocol */
# define ETH_P_IPV6 0x86DD    /* IPv6 protocol */
# define ETH_P_ARP  0x0806    /* ARP protocol */
	USHORT proto;               /* packet type ID field	*/
} ETH_HEADER, *PETH_HEADER;

#pragma pack()
