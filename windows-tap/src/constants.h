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

//====================================================================
//                        Product and Version public settings
//====================================================================

#define PRODUCT_STRING PRODUCT_TAP_DEVICE_DESCRIPTION

#define TAP_NDIS_MAJOR_VERSION 5
#define TAP_NDIS_MINOR_VERSION 0

//===========================================================
// Driver constants
//===========================================================

#define ETHERNET_HEADER_SIZE     (sizeof (ETH_HEADER))
#define ETHERNET_MTU             1500
#define ETHERNET_PACKET_SIZE     (ETHERNET_MTU + ETHERNET_HEADER_SIZE)
#define DEFAULT_PACKET_LOOKAHEAD (ETHERNET_PACKET_SIZE)

#define NIC_MAX_MCAST_LIST 32  // Max length of multicast address list

#define MINIMUM_MTU 576        // USE TCP Minimum MTU
#define MAXIMUM_MTU 65536      // IP maximum MTU

#define PACKET_QUEUE_SIZE   64 // tap -> userspace queue size
#define IRP_QUEUE_SIZE      16 // max number of simultaneous i/o operations from userspace
#define INJECT_QUEUE_SIZE   16 // DHCP/ARP -> tap injection queue

#define TAP_LITTLE_ENDIAN      // affects ntohs, htonl, etc. functions
