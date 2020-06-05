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

#ifndef TAP_PROTOTYPES_DEFINED
#define TAP_PROTOTYPES_DEFINED

DRIVER_INITIALIZE   DriverEntry;

//VOID AdapterFreeResources
//   (
//    TapAdapterPointer p_Adapter
//   );
//

//
//NTSTATUS TapDeviceHook
//   (
//    IN PDEVICE_OBJECT p_DeviceObject,
//    IN PIRP p_IRP
//   );
//

NDIS_STATUS
CreateTapDevice(
    __in PTAP_ADAPTER_CONTEXT   Adapter
   );

VOID
DestroyTapDevice(
    __in PTAP_ADAPTER_CONTEXT   Adapter
   );

// Flush the pending send TAP packet queue.
VOID
tapFlushSendPacketQueue(
    __in PTAP_ADAPTER_CONTEXT   Adapter
    );

VOID
IndicateReceivePacket(
    __in PTAP_ADAPTER_CONTEXT  Adapter,
    __in PUCHAR packetData,
    __in const unsigned int packetLength
    );

/*
BOOLEAN
ProcessDHCP(
    __in PTAP_ADAPTER_CONTEXT   Adapter,
    __in const ETH_HEADER *eth,
    __in const IPHDR *ip,
    __in const UDPHDR *udp,
    __in const DHCP *dhcp,
    __in int optlen
    );
*/

/*
BOOLEAN
ProcessARP(
    __in PTAP_ADAPTER_CONTEXT   Adapter,
    __in const PARP_PACKET src,
    __in const IPADDR adapter_ip,
    __in const IPADDR ip_network,
    __in const IPADDR ip_netmask,
    __in const MACADDR mac
   );
*/

#endif
