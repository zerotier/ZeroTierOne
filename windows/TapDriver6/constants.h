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

//====================================================================
//                        Product and Version public settings
//====================================================================

#define PRODUCT_STRING PRODUCT_TAP_DEVICE_DESCRIPTION


//
// Update the driver version number every time you release a new driver
// The high word is the major version. The low word is the minor version.
// Also make sure that VER_FILEVERSION specified in the .RC file also
// matches with the driver version because NDISTESTER checks for that.
//
#ifndef TAP_DRIVER_MAJOR_VERSION

#define TAP_DRIVER_MAJOR_VERSION           0x04
#define TAP_DRIVER_MINOR_VERSION           0x02

#endif

#define TAP_DRIVER_VENDOR_VERSION          ((TAP_DRIVER_MAJOR_VERSION << 16) | TAP_DRIVER_MINOR_VERSION)

//
// Define the NDIS miniport interface version that this driver targets.
//
#if defined(NDIS60_MINIPORT)
#  define TAP_NDIS_MAJOR_VERSION    6
#  define TAP_NDIS_MINOR_VERSION    0
#elif defined(NDIS61_MINIPORT)
#  define TAP_NDIS_MAJOR_VERSION    6
#  define TAP_NDIS_MINOR_VERSION    1
#elif defined(NDIS620_MINIPORT)
#  define TAP_NDIS_MAJOR_VERSION    6
#  define TAP_NDIS_MINOR_VERSION    20
#elif defined(NDIS630_MINIPORT)
#  define TAP_NDIS_MAJOR_VERSION    6
#  define TAP_NDIS_MINOR_VERSION    30
#else
#define TAP_NDIS_MAJOR_VERSION      5
#define TAP_NDIS_MINOR_VERSION      0
#endif

//===========================================================
// Driver constants
//===========================================================

#define ETHERNET_HEADER_SIZE        (sizeof (ETH_HEADER))
#define ETHERNET_MTU                1500
#define ETHERNET_PACKET_SIZE        (ETHERNET_MTU + ETHERNET_HEADER_SIZE)
#define DEFAULT_PACKET_LOOKAHEAD    (ETHERNET_PACKET_SIZE)
#define VLAN_TAG_SIZE               4

//===========================================================
// Medium properties
//===========================================================

#define TAP_FRAME_HEADER_SIZE       ETHERNET_HEADER_SIZE
#define TAP_FRAME_MAX_DATA_SIZE     ETHERNET_MTU
#define TAP_MAX_FRAME_SIZE          (TAP_FRAME_HEADER_SIZE + TAP_FRAME_MAX_DATA_SIZE)
#define TAP_MIN_FRAME_SIZE          60

#define TAP_MEDIUM_TYPE             NdisMedium802_3

//===========================================================
// Physical adapter properties
//===========================================================

// The bus that connects the adapter to the PC.
// (Example: PCI adapters should use NdisInterfacePci).
#define TAP_INTERFACE_TYPE          NdisInterfaceInternal

#define TAP_VENDOR_DESC             PRODUCT_TAP_WIN_DEVICE_DESCRIPTION

// Highest byte is the NIC byte plus three vendor bytes. This is normally
// obtained from the NIC.
#define TAP_VENDOR_ID               0x00FFFFFF

// If you have physical hardware on 802.3, use NdisPhysicalMedium802_3.
#define TAP_PHYSICAL_MEDIUM         NdisPhysicalMediumUnspecified

// Claim to be 100mbps duplex
#define MEGABITS_PER_SECOND                1000000ULL
#define TAP_XMIT_SPEED                     (100ULL*MEGABITS_PER_SECOND)
#define TAP_RECV_SPEED                     (100ULL*MEGABITS_PER_SECOND)

// Max number of multicast addresses supported in hardware
#define TAP_MAX_MCAST_LIST                 32

#define TAP_MAX_LOOKAHEAD                  TAP_FRAME_MAX_DATA_SIZE
#define TAP_BUFFER_SIZE                    TAP_MAX_FRAME_SIZE

// Set this value to TRUE if there is a physical adapter.
#define TAP_HAS_PHYSICAL_CONNECTOR         FALSE
#define TAP_ACCESS_TYPE                    NET_IF_ACCESS_BROADCAST
#define TAP_DIRECTION_TYPE                 NET_IF_DIRECTION_SENDRECEIVE
#define TAP_CONNECTION_TYPE                NET_IF_CONNECTION_DEDICATED

// This value must match the *IfType in the driver .inf file
#define TAP_IFTYPE                         IF_TYPE_ETHERNET_CSMACD

//
// This is a virtual device, so it can tolerate surprise removal and
// suspend.  Ensure the correct flags are set for your hardware.
//
#define TAP_ADAPTER_ATTRIBUTES_FLAGS (\
                NDIS_MINIPORT_ATTRIBUTES_SURPRISE_REMOVE_OK | NDIS_MINIPORT_ATTRIBUTES_NDIS_WDM)

#define TAP_SUPPORTED_FILTERS ( \
                NDIS_PACKET_TYPE_DIRECTED   | \
                NDIS_PACKET_TYPE_MULTICAST  | \
                NDIS_PACKET_TYPE_BROADCAST  | \
                NDIS_PACKET_TYPE_ALL_LOCAL  | \
                NDIS_PACKET_TYPE_PROMISCUOUS | \
                NDIS_PACKET_TYPE_ALL_MULTICAST)

#define TAP_MAX_MCAST_LIST          32  // Max length of multicast address list

//
// Specify a bitmask that defines optional properties of the NIC.
// This miniport indicates receive with NdisMIndicateReceiveNetBufferLists
// function.  Such a driver should set this NDIS_MAC_OPTION_TRANSFERS_NOT_PEND
// flag.
//
// NDIS_MAC_OPTION_NO_LOOPBACK tells NDIS that NIC has no internal
// loopback support so NDIS will manage loopbacks on behalf of
// this driver.
//
// NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA tells the protocol that
// our receive buffer is not on a device-specific card. If
// NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA is not set, multi-buffer
// indications are copied to a single flat buffer.
//

#define TAP_MAC_OPTIONS (\
                NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA | \
                NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  | \
                NDIS_MAC_OPTION_NO_LOOPBACK)

#define TAP_ADAPTER_CHECK_FOR_HANG_TIME_IN_SECONDS 4


// NDIS 6.x miniports must support all counters in OID_GEN_STATISTICS.
#define TAP_SUPPORTED_STATISTICS (\
                NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_RCV    | \
                NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_RCV   | \
                NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_RCV   | \
                NDIS_STATISTICS_FLAGS_VALID_BYTES_RCV              | \
                NDIS_STATISTICS_FLAGS_VALID_RCV_DISCARDS           | \
                NDIS_STATISTICS_FLAGS_VALID_RCV_ERROR              | \
                NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_XMIT   | \
                NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_XMIT  | \
                NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_XMIT  | \
                NDIS_STATISTICS_FLAGS_VALID_BYTES_XMIT             | \
                NDIS_STATISTICS_FLAGS_VALID_XMIT_ERROR             | \
                NDIS_STATISTICS_FLAGS_VALID_XMIT_DISCARDS          | \
                NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_RCV     | \
                NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_RCV    | \
                NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_RCV    | \
                NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_XMIT    | \
                NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_XMIT   | \
                NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_XMIT)


#define MINIMUM_MTU                 576        // USE TCP Minimum MTU
#define MAXIMUM_MTU                 65536      // IP maximum MTU

#define PACKET_QUEUE_SIZE           64 // tap -> userspace queue size
#define IRP_QUEUE_SIZE              16 // max number of simultaneous i/o operations from userspace
#define INJECT_QUEUE_SIZE           16 // DHCP/ARP -> tap injection queue

#define TAP_LITTLE_ENDIAN      // affects ntohs, htonl, etc. functions
