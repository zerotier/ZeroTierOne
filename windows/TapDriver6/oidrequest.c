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

//
// Include files.
//

#include "tap.h"

#ifndef DBG

#define DBG_PRINT_OID_NAME

#else

VOID
DBG_PRINT_OID_NAME(
    __in  NDIS_OID  Oid
    )
{
    PCHAR oidName = NULL;

    switch (Oid){

        #undef MAKECASE
        #define MAKECASE(oidx) case oidx: oidName = #oidx "\n"; break;

        /* Operational OIDs */
        MAKECASE(OID_GEN_SUPPORTED_LIST)
        MAKECASE(OID_GEN_HARDWARE_STATUS)
        MAKECASE(OID_GEN_MEDIA_SUPPORTED)
        MAKECASE(OID_GEN_MEDIA_IN_USE)
        MAKECASE(OID_GEN_MAXIMUM_LOOKAHEAD)
        MAKECASE(OID_GEN_MAXIMUM_FRAME_SIZE)
        MAKECASE(OID_GEN_LINK_SPEED)
        MAKECASE(OID_GEN_TRANSMIT_BUFFER_SPACE)
        MAKECASE(OID_GEN_RECEIVE_BUFFER_SPACE)
        MAKECASE(OID_GEN_TRANSMIT_BLOCK_SIZE)
        MAKECASE(OID_GEN_RECEIVE_BLOCK_SIZE)
        MAKECASE(OID_GEN_VENDOR_ID)
        MAKECASE(OID_GEN_VENDOR_DESCRIPTION)
        MAKECASE(OID_GEN_VENDOR_DRIVER_VERSION)
        MAKECASE(OID_GEN_CURRENT_PACKET_FILTER)
        MAKECASE(OID_GEN_CURRENT_LOOKAHEAD)
        MAKECASE(OID_GEN_DRIVER_VERSION)
        MAKECASE(OID_GEN_MAXIMUM_TOTAL_SIZE)
        MAKECASE(OID_GEN_PROTOCOL_OPTIONS)
        MAKECASE(OID_GEN_MAC_OPTIONS)
        MAKECASE(OID_GEN_MEDIA_CONNECT_STATUS)
        MAKECASE(OID_GEN_MAXIMUM_SEND_PACKETS)
        MAKECASE(OID_GEN_SUPPORTED_GUIDS)
        MAKECASE(OID_GEN_NETWORK_LAYER_ADDRESSES)
        MAKECASE(OID_GEN_TRANSPORT_HEADER_OFFSET)
        MAKECASE(OID_GEN_MEDIA_CAPABILITIES)
        MAKECASE(OID_GEN_PHYSICAL_MEDIUM)
        MAKECASE(OID_GEN_MACHINE_NAME)
        MAKECASE(OID_GEN_VLAN_ID)
        MAKECASE(OID_GEN_RNDIS_CONFIG_PARAMETER)

        /* Operational OIDs for NDIS 6.0 */
        MAKECASE(OID_GEN_MAX_LINK_SPEED)
        MAKECASE(OID_GEN_LINK_STATE)
        MAKECASE(OID_GEN_LINK_PARAMETERS)
        MAKECASE(OID_GEN_MINIPORT_RESTART_ATTRIBUTES)
        MAKECASE(OID_GEN_ENUMERATE_PORTS)
        MAKECASE(OID_GEN_PORT_STATE)
        MAKECASE(OID_GEN_PORT_AUTHENTICATION_PARAMETERS)
        MAKECASE(OID_GEN_INTERRUPT_MODERATION)
        MAKECASE(OID_GEN_PHYSICAL_MEDIUM_EX)

        /* Statistical OIDs */
        MAKECASE(OID_GEN_XMIT_OK)
        MAKECASE(OID_GEN_RCV_OK)
        MAKECASE(OID_GEN_XMIT_ERROR)
        MAKECASE(OID_GEN_RCV_ERROR)
        MAKECASE(OID_GEN_RCV_NO_BUFFER)
        MAKECASE(OID_GEN_DIRECTED_BYTES_XMIT)
        MAKECASE(OID_GEN_DIRECTED_FRAMES_XMIT)
        MAKECASE(OID_GEN_MULTICAST_BYTES_XMIT)
        MAKECASE(OID_GEN_MULTICAST_FRAMES_XMIT)
        MAKECASE(OID_GEN_BROADCAST_BYTES_XMIT)
        MAKECASE(OID_GEN_BROADCAST_FRAMES_XMIT)
        MAKECASE(OID_GEN_DIRECTED_BYTES_RCV)
        MAKECASE(OID_GEN_DIRECTED_FRAMES_RCV)
        MAKECASE(OID_GEN_MULTICAST_BYTES_RCV)
        MAKECASE(OID_GEN_MULTICAST_FRAMES_RCV)
        MAKECASE(OID_GEN_BROADCAST_BYTES_RCV)
        MAKECASE(OID_GEN_BROADCAST_FRAMES_RCV)
        MAKECASE(OID_GEN_RCV_CRC_ERROR)
        MAKECASE(OID_GEN_TRANSMIT_QUEUE_LENGTH)

        /* Statistical OIDs for NDIS 6.0 */
        MAKECASE(OID_GEN_STATISTICS)
        MAKECASE(OID_GEN_BYTES_RCV)
        MAKECASE(OID_GEN_BYTES_XMIT)
        MAKECASE(OID_GEN_RCV_DISCARDS)
        MAKECASE(OID_GEN_XMIT_DISCARDS)

        /* Misc OIDs */
        MAKECASE(OID_GEN_GET_TIME_CAPS)
        MAKECASE(OID_GEN_GET_NETCARD_TIME)
        MAKECASE(OID_GEN_NETCARD_LOAD)
        MAKECASE(OID_GEN_DEVICE_PROFILE)
        MAKECASE(OID_GEN_INIT_TIME_MS)
        MAKECASE(OID_GEN_RESET_COUNTS)
        MAKECASE(OID_GEN_MEDIA_SENSE_COUNTS)

        /* PnP power management operational OIDs */
        MAKECASE(OID_PNP_CAPABILITIES)
        MAKECASE(OID_PNP_SET_POWER)
        MAKECASE(OID_PNP_QUERY_POWER)
        MAKECASE(OID_PNP_ADD_WAKE_UP_PATTERN)
        MAKECASE(OID_PNP_REMOVE_WAKE_UP_PATTERN)
        MAKECASE(OID_PNP_ENABLE_WAKE_UP)
        MAKECASE(OID_PNP_WAKE_UP_PATTERN_LIST)

        /* PnP power management statistical OIDs */
        MAKECASE(OID_PNP_WAKE_UP_ERROR)
        MAKECASE(OID_PNP_WAKE_UP_OK)

        /* Ethernet operational OIDs */
        MAKECASE(OID_802_3_PERMANENT_ADDRESS)
        MAKECASE(OID_802_3_CURRENT_ADDRESS)
        MAKECASE(OID_802_3_MULTICAST_LIST)
        MAKECASE(OID_802_3_MAXIMUM_LIST_SIZE)
        MAKECASE(OID_802_3_MAC_OPTIONS)

        /* Ethernet operational OIDs for NDIS 6.0 */
        MAKECASE(OID_802_3_ADD_MULTICAST_ADDRESS)
        MAKECASE(OID_802_3_DELETE_MULTICAST_ADDRESS)

        /* Ethernet statistical OIDs */
        MAKECASE(OID_802_3_RCV_ERROR_ALIGNMENT)
        MAKECASE(OID_802_3_XMIT_ONE_COLLISION)
        MAKECASE(OID_802_3_XMIT_MORE_COLLISIONS)
        MAKECASE(OID_802_3_XMIT_DEFERRED)
        MAKECASE(OID_802_3_XMIT_MAX_COLLISIONS)
        MAKECASE(OID_802_3_RCV_OVERRUN)
        MAKECASE(OID_802_3_XMIT_UNDERRUN)
        MAKECASE(OID_802_3_XMIT_HEARTBEAT_FAILURE)
        MAKECASE(OID_802_3_XMIT_TIMES_CRS_LOST)
        MAKECASE(OID_802_3_XMIT_LATE_COLLISIONS)

        /*  TCP/IP OIDs */
        MAKECASE(OID_TCP_TASK_OFFLOAD)
        MAKECASE(OID_TCP_TASK_IPSEC_ADD_SA)
        MAKECASE(OID_TCP_TASK_IPSEC_DELETE_SA)
        MAKECASE(OID_TCP_SAN_SUPPORT)
        MAKECASE(OID_TCP_TASK_IPSEC_ADD_UDPESP_SA)
        MAKECASE(OID_TCP_TASK_IPSEC_DELETE_UDPESP_SA)
        MAKECASE(OID_TCP4_OFFLOAD_STATS)
        MAKECASE(OID_TCP6_OFFLOAD_STATS)
        MAKECASE(OID_IP4_OFFLOAD_STATS)
        MAKECASE(OID_IP6_OFFLOAD_STATS)

        /* TCP offload OIDs for NDIS 6 */
        MAKECASE(OID_TCP_OFFLOAD_CURRENT_CONFIG)
        MAKECASE(OID_TCP_OFFLOAD_PARAMETERS)
        MAKECASE(OID_TCP_OFFLOAD_HARDWARE_CAPABILITIES)
        MAKECASE(OID_TCP_CONNECTION_OFFLOAD_CURRENT_CONFIG)
        MAKECASE(OID_TCP_CONNECTION_OFFLOAD_HARDWARE_CAPABILITIES)
        MAKECASE(OID_OFFLOAD_ENCAPSULATION)

#if (NDIS_SUPPORT_NDIS620)
        /* VMQ OIDs for NDIS 6.20 */
        MAKECASE(OID_RECEIVE_FILTER_FREE_QUEUE)
        MAKECASE(OID_RECEIVE_FILTER_CLEAR_FILTER)
        MAKECASE(OID_RECEIVE_FILTER_ALLOCATE_QUEUE)
        MAKECASE(OID_RECEIVE_FILTER_QUEUE_ALLOCATION_COMPLETE)
        MAKECASE(OID_RECEIVE_FILTER_SET_FILTER)
#endif

#if (NDIS_SUPPORT_NDIS630)
        /* NDIS QoS OIDs for NDIS 6.30 */
        MAKECASE(OID_QOS_PARAMETERS)
#endif
    }

    if (oidName)
    {
        DEBUGP(("OID: %s", oidName));
    }
    else
    {
        DEBUGP(("<** Unknown OID 0x%08x **>\n", Oid));
    }
}

#endif // DBG

//======================================================================
// TAP NDIS 6 OID Request Callbacks
//======================================================================

NDIS_STATUS
tapSetMulticastList(
    __in PTAP_ADAPTER_CONTEXT   Adapter,
    __in PNDIS_OID_REQUEST      OidRequest
    )
{
    NDIS_STATUS   status = NDIS_STATUS_SUCCESS;

    //
    // Initialize.
    //
    OidRequest->DATA.SET_INFORMATION.BytesNeeded = MACADDR_SIZE;
    OidRequest->DATA.SET_INFORMATION.BytesRead
        = OidRequest->DATA.SET_INFORMATION.InformationBufferLength;


    do
    {
        if (OidRequest->DATA.SET_INFORMATION.InformationBufferLength % MACADDR_SIZE)
        {
            status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        if (OidRequest->DATA.SET_INFORMATION.InformationBufferLength > (TAP_MAX_MCAST_LIST * MACADDR_SIZE))
        {
            status = NDIS_STATUS_MULTICAST_FULL;
            OidRequest->DATA.SET_INFORMATION.BytesNeeded = TAP_MAX_MCAST_LIST * MACADDR_SIZE;
            break;
        }

        // BUGBUG!!! Is lock needed??? If so, use NDIS_RW_LOCK. Also apply to packet filter.

        NdisZeroMemory(Adapter->MCList,
                       TAP_MAX_MCAST_LIST * MACADDR_SIZE);

        NdisMoveMemory(Adapter->MCList,
                       OidRequest->DATA.SET_INFORMATION.InformationBuffer,
                       OidRequest->DATA.SET_INFORMATION.InformationBufferLength);

        Adapter->ulMCListSize = OidRequest->DATA.SET_INFORMATION.InformationBufferLength / MACADDR_SIZE;

    } while(FALSE);
    return status;
}

NDIS_STATUS
tapSetPacketFilter(
    __in PTAP_ADAPTER_CONTEXT   Adapter,
    __in ULONG                  PacketFilter
    )
{
    NDIS_STATUS   status = NDIS_STATUS_SUCCESS;

    // any bits not supported?
    if (PacketFilter & ~(TAP_SUPPORTED_FILTERS))
    {
        DEBUGP (("[TAP] Unsupported packet filter: 0x%08x\n", PacketFilter));
        status = NDIS_STATUS_NOT_SUPPORTED;
    }
    else
    {
        // Any actual filtering changes?
        if (PacketFilter != Adapter->PacketFilter)
        {
            //
            // Change the filtering modes on hardware
            //

            // Save the new packet filter value
            Adapter->PacketFilter = PacketFilter;
        }
    }

    return status;
}

NDIS_STATUS
AdapterSetPowerD0(
    __in PTAP_ADAPTER_CONTEXT   Adapter
    )
/*++
Routine Description:

    NIC power has been restored to the working power state (D0).
    Prepare the NIC for normal operation:
        - Restore hardware context (packet filters, multicast addresses, MAC address, etc.)
        - Enable interrupts and the NIC's DMA engine.

Arguments:

    Adapter     - Pointer to adapter block

Return Value:

    NDIS_STATUS   

--*/      
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;

    DEBUGP (("[TAP] PowerState: Fully powered\n"));

    // Start data path...

    return status;
}

NDIS_STATUS
AdapterSetPowerLow(
    __in PTAP_ADAPTER_CONTEXT       Adapter,
    __in NDIS_DEVICE_POWER_STATE    PowerState
    )
/*++
Routine Description:

    The NIC is about to be transitioned to a low power state. 
    Prepare the NIC for the sleeping state:
        - Disable interrupts and the NIC's DMA engine, cancel timers.  
        - Save any hardware context that the NIC cannot preserve in 
          a sleeping state (packet filters, multicast addresses, 
          the current MAC address, etc.)
    A miniport driver cannot access the NIC hardware after 
    the NIC has been set to the D3 state by the bus driver.

    Miniport drivers NDIS v6.30 and above 
        Do NOT wait for NDIS to return the ownership of all 
        NBLs from outstanding receive indications
        Retain ownership of all the receive descriptors and 
        packet buffers previously owned by the hardware.

Arguments:

    Adapter         - Pointer to adapter block
    PowerState      - New power state

Return Value:

    NDIS_STATUS   

--*/      
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;

    DEBUGP (("[TAP] PowerState: Low-power\n"));

    //
    // Miniport drivers NDIS v6.20 and below are 
    // paused prior the low power transition 
    //

    // Check for paused state...
    // Verify data path stopped...

    return status;
}

NDIS_STATUS
tapSetInformation(
    __in PTAP_ADAPTER_CONTEXT   Adapter,
    __in PNDIS_OID_REQUEST      OidRequest
    )
/*++

Routine Description:

    Helper function to perform a set OID request

Arguments:

    Adapter         -
    NdisSetRequest  - The OID to set

Return Value:

    NDIS_STATUS

--*/
{
    NDIS_STATUS    status = NDIS_STATUS_SUCCESS;

    DBG_PRINT_OID_NAME(OidRequest->DATA.SET_INFORMATION.Oid);

    switch(OidRequest->DATA.SET_INFORMATION.Oid)
    {
    case OID_802_3_MULTICAST_LIST:
        //
        // Set the multicast address list on the NIC for packet reception.
        // The NIC driver can set a limit on the number of multicast
        // addresses bound protocol drivers can enable simultaneously.
        // NDIS returns NDIS_STATUS_MULTICAST_FULL if a protocol driver
        // exceeds this limit or if it specifies an invalid multicast
        // address.
        //
        status = tapSetMulticastList(Adapter,OidRequest);
        break;

    case OID_GEN_CURRENT_LOOKAHEAD:
        //
        // A protocol driver can set a suggested value for the number
        // of bytes to be used in its binding; however, the underlying
        // NIC driver is never required to limit its indications to
        // the value set.
        //
        if (OidRequest->DATA.SET_INFORMATION.InformationBufferLength != sizeof(ULONG))
        {
            OidRequest->DATA.SET_INFORMATION.BytesNeeded = sizeof(ULONG);
            status = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        Adapter->ulLookahead = *(PULONG)OidRequest->DATA.SET_INFORMATION.InformationBuffer;

        OidRequest->DATA.SET_INFORMATION.BytesRead = sizeof(ULONG);
        status = NDIS_STATUS_SUCCESS;
        break;

    case OID_GEN_CURRENT_PACKET_FILTER:
            //
            // Program the hardware to indicate the packets
            // of certain filter types.
            //
            if(OidRequest->DATA.SET_INFORMATION.InformationBufferLength != sizeof(ULONG))
            {
                OidRequest->DATA.SET_INFORMATION.BytesNeeded = sizeof(ULONG);
                status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }

            OidRequest->DATA.SET_INFORMATION.BytesRead
                = OidRequest->DATA.SET_INFORMATION.InformationBufferLength;

            status = tapSetPacketFilter(
                            Adapter,
                            *((PULONG)OidRequest->DATA.SET_INFORMATION.InformationBuffer)
                            );

            break;

    case OID_PNP_SET_POWER:
        {
            // Sanity check.
            if (OidRequest->DATA.SET_INFORMATION.InformationBufferLength
                < sizeof(NDIS_DEVICE_POWER_STATE)
                )
            {
                status = NDIS_STATUS_INVALID_LENGTH;
            }
            else
            {
                NDIS_DEVICE_POWER_STATE     PowerState;

                PowerState = *(PNDIS_DEVICE_POWER_STATE UNALIGNED)OidRequest->DATA.SET_INFORMATION.InformationBuffer;
                OidRequest->DATA.SET_INFORMATION.BytesRead = sizeof(NDIS_DEVICE_POWER_STATE);

                if(PowerState < NdisDeviceStateD0  ||
                    PowerState > NdisDeviceStateD3)
                {
                    status = NDIS_STATUS_INVALID_DATA;
                }
                else
                {
                    Adapter->CurrentPowerState = PowerState;

                    if (PowerState == NdisDeviceStateD0)
                    {
                        status = AdapterSetPowerD0(Adapter);
                    }
                    else
                    {
                        status = AdapterSetPowerLow(Adapter, PowerState);
                    }
                }
            }
        }
        break;

#if (NDIS_SUPPORT_NDIS61)
    case OID_PNP_ADD_WAKE_UP_PATTERN:
    case OID_PNP_REMOVE_WAKE_UP_PATTERN:
    case OID_PNP_ENABLE_WAKE_UP:
#endif
        ASSERT(!"NIC does not support wake on LAN OIDs"); 
    default:
        //
        // The entry point may by used by other requests
        //
        status = NDIS_STATUS_NOT_SUPPORTED;
        break;
    }

    return status;
}

NDIS_STATUS
tapQueryInformation(
    __in PTAP_ADAPTER_CONTEXT   Adapter,
    __in PNDIS_OID_REQUEST      OidRequest
    )
/*++

Routine Description:

    Helper function to perform a query OID request

Arguments:

    Adapter         -
    OidRequest  - The OID request that is being queried

Return Value:

    NDIS_STATUS

--*/
{
    NDIS_STATUS             status = NDIS_STATUS_SUCCESS;
    NDIS_MEDIUM             Medium = TAP_MEDIUM_TYPE;
    NDIS_HARDWARE_STATUS    HardwareStatus = NdisHardwareStatusReady;
    UCHAR                   VendorDesc[] = TAP_VENDOR_DESC;
    ULONG                   ulInfo;
    USHORT                  usInfo;
    ULONG64                 ulInfo64;

    // Default to returning the ULONG value
    PVOID                   pInfo=NULL;
    ULONG                   ulInfoLen = sizeof(ulInfo);

    // ATTENTION!!! Ignore OIDs to noisy to print...
    if((OidRequest->DATA.QUERY_INFORMATION.Oid != OID_GEN_STATISTICS)
        && (OidRequest->DATA.QUERY_INFORMATION.Oid != OID_IP4_OFFLOAD_STATS)
        && (OidRequest->DATA.QUERY_INFORMATION.Oid != OID_IP6_OFFLOAD_STATS)
        )
    {
        DBG_PRINT_OID_NAME(OidRequest->DATA.QUERY_INFORMATION.Oid);
    }

    // Dispatch based on object identifier (OID).
    switch(OidRequest->DATA.QUERY_INFORMATION.Oid)
    {
    case OID_GEN_HARDWARE_STATUS:
        //
        // Specify the current hardware status of the underlying NIC as
        // one of the following NDIS_HARDWARE_STATUS-type values.
        //
        pInfo = (PVOID) &HardwareStatus;
        ulInfoLen = sizeof(NDIS_HARDWARE_STATUS);
        break;

    case OID_802_3_PERMANENT_ADDRESS:
        //
        // Return the MAC address of the NIC burnt in the hardware.
        //
        pInfo = Adapter->PermanentAddress;
        ulInfoLen = MACADDR_SIZE;
        break;

    case OID_802_3_CURRENT_ADDRESS:
        //
        // Return the MAC address the NIC is currently programmed to
        // use. Note that this address could be different from the
        // permananent address as the user can override using
        // registry. Read NdisReadNetworkAddress doc for more info.
        //
        pInfo = Adapter->CurrentAddress;
        ulInfoLen = MACADDR_SIZE;
        break;

    case OID_GEN_MEDIA_SUPPORTED:
        //
        // Return an array of media that are supported by the miniport.
        // This miniport only supports one medium (Ethernet), so the OID
        // returns identical results to OID_GEN_MEDIA_IN_USE.
        //

        __fallthrough;

    case OID_GEN_MEDIA_IN_USE:
        //
        // Return an array of media that are currently in use by the
        // miniport.  This array should be a subset of the array returned
        // by OID_GEN_MEDIA_SUPPORTED.
        //
        pInfo = &Medium;
        ulInfoLen = sizeof(Medium);
        break;

    case OID_GEN_MAXIMUM_TOTAL_SIZE:
        //
        // Specify the maximum total packet length, in bytes, the NIC
        // supports including the header. A protocol driver might use
        // this returned length as a gauge to determine the maximum
        // size packet that a NIC driver could forward to the
        // protocol driver. The miniport driver must never indicate
        // up to the bound protocol driver packets received over the
        // network that are longer than the packet size specified by
        // OID_GEN_MAXIMUM_TOTAL_SIZE.
        //

        __fallthrough;

    case OID_GEN_TRANSMIT_BLOCK_SIZE:
        //
        // The OID_GEN_TRANSMIT_BLOCK_SIZE OID specifies the minimum
        // number of bytes that a single net packet occupies in the
        // transmit buffer space of the NIC. In our case, the transmit
        // block size is identical to its maximum packet size.
        __fallthrough;

    case OID_GEN_RECEIVE_BLOCK_SIZE:
        //
        // The OID_GEN_RECEIVE_BLOCK_SIZE OID specifies the amount of
        // storage, in bytes, that a single packet occupies in the receive
        // buffer space of the NIC.
        //
        ulInfo = (ULONG) TAP_MAX_FRAME_SIZE;
        pInfo = &ulInfo;
        break;

    case OID_GEN_INTERRUPT_MODERATION:
        {
            PNDIS_INTERRUPT_MODERATION_PARAMETERS moderationParams
                = (PNDIS_INTERRUPT_MODERATION_PARAMETERS)OidRequest->DATA.QUERY_INFORMATION.InformationBuffer;

            moderationParams->Header.Type = NDIS_OBJECT_TYPE_DEFAULT; 
            moderationParams->Header.Revision = NDIS_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
            moderationParams->Header.Size = NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
            moderationParams->Flags = 0;
            moderationParams->InterruptModeration = NdisInterruptModerationNotSupported;
            ulInfoLen = NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
        }
        break;

    case OID_PNP_QUERY_POWER:
        // Simply succeed this.
        break;

    case OID_GEN_VENDOR_ID:
        //
        // Specify a three-byte IEEE-registered vendor code, followed
        // by a single byte that the vendor assigns to identify a
        // particular NIC. The IEEE code uniquely identifies the vendor
        // and is the same as the three bytes appearing at the beginning
        // of the NIC hardware address. Vendors without an IEEE-registered
        // code should use the value 0xFFFFFF.
        //

        ulInfo = TAP_VENDOR_ID;
        pInfo = &ulInfo;
        break;

    case OID_GEN_VENDOR_DESCRIPTION:
        //
        // Specify a zero-terminated string describing the NIC vendor.
        //
        pInfo = VendorDesc;
        ulInfoLen = sizeof(VendorDesc);
        break;

    case OID_GEN_VENDOR_DRIVER_VERSION:
        //
        // Specify the vendor-assigned version number of the NIC driver.
        // The low-order half of the return value specifies the minor
        // version; the high-order half specifies the major version.
        //

        ulInfo = TAP_DRIVER_VENDOR_VERSION;
        pInfo = &ulInfo;
        break;

    case OID_GEN_DRIVER_VERSION:
        //
        // Specify the NDIS version in use by the NIC driver. The high
        // byte is the major version number; the low byte is the minor
        // version number.
        //
        usInfo = (USHORT) (TAP_NDIS_MAJOR_VERSION<<8) + TAP_NDIS_MINOR_VERSION;
        pInfo = (PVOID) &usInfo;
        ulInfoLen = sizeof(USHORT);
        break;

    case OID_802_3_MAXIMUM_LIST_SIZE:
        //
        // The maximum number of multicast addresses the NIC driver
        // can manage. This list is global for all protocols bound
        // to (or above) the NIC. Consequently, a protocol can receive
        // NDIS_STATUS_MULTICAST_FULL from the NIC driver when
        // attempting to set the multicast address list, even if
        // the number of elements in the given list is less than
        // the number originally returned for this query.
        //

        ulInfo = TAP_MAX_MCAST_LIST;
        pInfo = &ulInfo;
        break;

    case OID_GEN_XMIT_ERROR:
        ulInfo = (ULONG)
            (Adapter->TxAbortExcessCollisions +
            Adapter->TxDmaUnderrun +
            Adapter->TxLostCRS +
            Adapter->TxLateCollisions+
            Adapter->TransmitFailuresOther);
        pInfo = &ulInfo;
        break;

    case OID_GEN_RCV_ERROR:
        ulInfo = (ULONG)
            (Adapter->RxCrcErrors +
            Adapter->RxAlignmentErrors +
            Adapter->RxDmaOverrunErrors +
            Adapter->RxRuntErrors);
        pInfo = &ulInfo;
        break;

    case OID_GEN_RCV_DISCARDS:
        ulInfo = (ULONG)Adapter->RxResourceErrors;
        pInfo = &ulInfo;
        break;

    case OID_GEN_RCV_NO_BUFFER:
        ulInfo = (ULONG)Adapter->RxResourceErrors;
        pInfo = &ulInfo;
        break;

    case OID_GEN_XMIT_OK:
        ulInfo64 = Adapter->FramesTxBroadcast
            + Adapter->FramesTxMulticast
            + Adapter->FramesTxDirected;
        pInfo = &ulInfo64;
        if (OidRequest->DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(ULONG64) ||
            OidRequest->DATA.QUERY_INFORMATION.InformationBufferLength == 0)
        {
            ulInfoLen = sizeof(ULONG64);
        }
        else
        {
            ulInfoLen = sizeof(ULONG);
        }

        // We should always report that only 8 bytes are required to keep ndistest happy
        OidRequest->DATA.QUERY_INFORMATION.BytesNeeded =  sizeof(ULONG64);
        break;

    case OID_GEN_RCV_OK:
        ulInfo64 = Adapter->FramesRxBroadcast
            + Adapter->FramesRxMulticast
            + Adapter->FramesRxDirected;

        pInfo = &ulInfo64;

        if (OidRequest->DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(ULONG64) ||
            OidRequest->DATA.QUERY_INFORMATION.InformationBufferLength == 0)
        {
            ulInfoLen = sizeof(ULONG64);
        }
        else
        {
            ulInfoLen = sizeof(ULONG);
        }

        // We should always report that only 8 bytes are required to keep ndistest happy
        OidRequest->DATA.QUERY_INFORMATION.BytesNeeded =  sizeof(ULONG64);
        break;

    case OID_802_3_RCV_ERROR_ALIGNMENT:

        ulInfo = Adapter->RxAlignmentErrors;
        pInfo = &ulInfo;
        break;

    case OID_802_3_XMIT_ONE_COLLISION:

        ulInfo = Adapter->OneRetry;
        pInfo = &ulInfo;
        break;

    case OID_802_3_XMIT_MORE_COLLISIONS:

        ulInfo = Adapter->MoreThanOneRetry;
        pInfo = &ulInfo;
        break;

    case OID_802_3_XMIT_DEFERRED:

        ulInfo = Adapter->TxOKButDeferred;
        pInfo = &ulInfo;
        break;

    case OID_802_3_XMIT_MAX_COLLISIONS:

        ulInfo = Adapter->TxAbortExcessCollisions;
        pInfo = &ulInfo;
        break;

    case OID_802_3_RCV_OVERRUN:

        ulInfo = Adapter->RxDmaOverrunErrors;
        pInfo = &ulInfo;
        break;

    case OID_802_3_XMIT_UNDERRUN:

        ulInfo = Adapter->TxDmaUnderrun;
        pInfo = &ulInfo;
        break;

    case OID_GEN_STATISTICS:

        if (OidRequest->DATA.QUERY_INFORMATION.InformationBufferLength < sizeof(NDIS_STATISTICS_INFO))
        {
            status = NDIS_STATUS_INVALID_LENGTH;
            OidRequest->DATA.QUERY_INFORMATION.BytesNeeded = sizeof(NDIS_STATISTICS_INFO);
            break;
        }
        else
        {
            PNDIS_STATISTICS_INFO Statistics
                = (PNDIS_STATISTICS_INFO)OidRequest->DATA.QUERY_INFORMATION.InformationBuffer;

            {C_ASSERT(sizeof(NDIS_STATISTICS_INFO) >= NDIS_SIZEOF_STATISTICS_INFO_REVISION_1);}
            Statistics->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
            Statistics->Header.Size = NDIS_SIZEOF_STATISTICS_INFO_REVISION_1;
            Statistics->Header.Revision = NDIS_STATISTICS_INFO_REVISION_1;

            Statistics->SupportedStatistics = TAP_SUPPORTED_STATISTICS;

            /* Bytes in */
            Statistics->ifHCInOctets =
                Adapter->BytesRxDirected +
                Adapter->BytesRxMulticast +
                Adapter->BytesRxBroadcast;

            Statistics->ifHCInUcastOctets =
                Adapter->BytesRxDirected;

            Statistics->ifHCInMulticastOctets =
                Adapter->BytesRxMulticast;

            Statistics->ifHCInBroadcastOctets =
                Adapter->BytesRxBroadcast;

            /* Packets in */
            Statistics->ifHCInUcastPkts =
                Adapter->FramesRxDirected;

            Statistics->ifHCInMulticastPkts =
                Adapter->FramesRxMulticast;

            Statistics->ifHCInBroadcastPkts =
                Adapter->FramesRxBroadcast;

            /* Errors in */
            Statistics->ifInErrors =
                Adapter->RxCrcErrors +
                Adapter->RxAlignmentErrors +
                Adapter->RxDmaOverrunErrors +
                Adapter->RxRuntErrors;

            Statistics->ifInDiscards =
                Adapter->RxResourceErrors;


            /* Bytes out */
            Statistics->ifHCOutOctets =
                Adapter->BytesTxDirected +
                Adapter->BytesTxMulticast +
                Adapter->BytesTxBroadcast;

            Statistics->ifHCOutUcastOctets =
                Adapter->BytesTxDirected;

            Statistics->ifHCOutMulticastOctets =
                Adapter->BytesTxMulticast;

            Statistics->ifHCOutBroadcastOctets =
                Adapter->BytesTxBroadcast;

            /* Packets out */
            Statistics->ifHCOutUcastPkts =
                Adapter->FramesTxDirected;

            Statistics->ifHCOutMulticastPkts =
                Adapter->FramesTxMulticast;

            Statistics->ifHCOutBroadcastPkts =
                Adapter->FramesTxBroadcast;

            /* Errors out */
            Statistics->ifOutErrors =
                Adapter->TxAbortExcessCollisions +
                Adapter->TxDmaUnderrun +
                Adapter->TxLostCRS +
                Adapter->TxLateCollisions+
                Adapter->TransmitFailuresOther;

            Statistics->ifOutDiscards = 0ULL;

            ulInfoLen = NDIS_SIZEOF_STATISTICS_INFO_REVISION_1;
        }

        break;

        // TODO: Inplement these query information requests.
    case OID_GEN_RECEIVE_BUFFER_SPACE:
    case OID_GEN_MAXIMUM_SEND_PACKETS:
    case OID_GEN_TRANSMIT_QUEUE_LENGTH:
    case OID_802_3_XMIT_HEARTBEAT_FAILURE:
    case OID_802_3_XMIT_TIMES_CRS_LOST:
    case OID_802_3_XMIT_LATE_COLLISIONS:

    default:
        //
        // The entry point may by used by other requests
        //
        status = NDIS_STATUS_NOT_SUPPORTED;
        break;
    }

    if (status == NDIS_STATUS_SUCCESS)
    {
        ASSERT(ulInfoLen > 0);

        if (ulInfoLen <= OidRequest->DATA.QUERY_INFORMATION.InformationBufferLength)
        {
            if(pInfo)
            {
                // Copy result into InformationBuffer
                NdisMoveMemory(
                    OidRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                    pInfo,
                    ulInfoLen
                    );
            }

            OidRequest->DATA.QUERY_INFORMATION.BytesWritten = ulInfoLen;
        }
        else
        {
            // too short
            OidRequest->DATA.QUERY_INFORMATION.BytesNeeded = ulInfoLen;
            status = NDIS_STATUS_BUFFER_TOO_SHORT;
        }
    }

    return status;
}

NDIS_STATUS
AdapterOidRequest(
    __in  NDIS_HANDLE             MiniportAdapterContext,
    __in  PNDIS_OID_REQUEST       OidRequest
    )
/*++

Routine Description:

    Entry point called by NDIS to get or set the value of a specified OID.

Arguments:

    MiniportAdapterContext  - Our adapter handle
    NdisRequest             - The OID request to handle

Return Value:

    Return code from the NdisRequest below.

--*/
{
    PTAP_ADAPTER_CONTEXT   adapter = (PTAP_ADAPTER_CONTEXT )MiniportAdapterContext;
    NDIS_STATUS    status;

    // Dispatch based on request type.
    switch (OidRequest->RequestType)
    {
    case NdisRequestSetInformation:
        status = tapSetInformation(adapter,OidRequest);
        break;

    case NdisRequestQueryInformation:
    case NdisRequestQueryStatistics:
        status = tapQueryInformation(adapter,OidRequest);
        break;

    case NdisRequestMethod: // TAP doesn't need to respond to this request type.
    default:
        //
        // The entry point may by used by other requests
        //
        status = NDIS_STATUS_NOT_SUPPORTED;
        break;
    }

    return status;
}

VOID
AdapterCancelOidRequest(
    __in NDIS_HANDLE              MiniportAdapterContext,
    __in PVOID                    RequestId
    )
{
    PTAP_ADAPTER_CONTEXT   adapter = (PTAP_ADAPTER_CONTEXT )MiniportAdapterContext;

    UNREFERENCED_PARAMETER(RequestId);

    //
    // This miniport sample does not pend any OID requests, so we don't have
    // to worry about cancelling them.
    //
}

