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
#ifndef __TAP_ADAPTER_CONTEXT_H_
#define __TAP_ADAPTER_CONTEXT_H_

#include "tap.h"

// Memory allocation tags.
#define TAP_ADAPTER_TAG             ((ULONG)'ApaT')     // "TapA
#define TAP_RX_NBL_TAG              ((ULONG)'RpaT')     // "TapR
#define TAP_RX_INJECT_BUFFER_TAG    ((ULONG)'IpaT')     // "TapI

#define TAP_MAX_NDIS_NAME_LENGTH     64     // 38 character GUID string plus extra..

// TAP receive indication NBL flag definitions.
#define TAP_RX_NBL_FLAGS                    NBL_FLAGS_MINIPORT_RESERVED
#define TAP_RX_NBL_FLAGS_CLEAR_ALL(_NBL)    ((_NBL)->Flags &= ~TAP_RX_NBL_FLAGS)
#define TAP_RX_NBL_FLAG_SET(_NBL, _F)       ((_NBL)->Flags |= ((_F) & TAP_RX_NBL_FLAGS))
#define TAP_RX_NBL_FLAG_CLEAR(_NBL, _F)     ((_NBL)->Flags &= ~((_F) & TAP_RX_NBL_FLAGS))
#define TAP_RX_NBL_FLAG_TEST(_NBL, _F)      (((_NBL)->Flags & ((_F) & TAP_RX_NBL_FLAGS)) != 0)

#define TAP_RX_NBL_FLAGS_IS_P2P             0x00001000
#define TAP_RX_NBL_FLAGS_IS_INJECTED        0x00002000

// MSDN Ref: http://msdn.microsoft.com/en-us/library/windows/hardware/ff560490(v=vs.85).aspx
typedef
enum _TAP_MINIPORT_ADAPTER_STATE
{
    // The Halted state is the initial state of all adapters. When an
    // adapter is in the Halted state, NDIS can call the driver's
    // MiniportInitializeEx function to initialize the adapter.
    MiniportHaltedState,

    // In the Shutdown state, a system shutdown and restart must occur
    // before the system can use the adapter again.
    MiniportShutdownState,

    // In the Initializing state, a miniport driver completes any
    //operations that are required to initialize an adapter.
    MiniportInitializingState,

    // Entering the Paused state...
    MiniportPausingState,

    // In the Paused state, the adapter does not indicate received
    // network data or accept send requests.
    MiniportPausedState,

    // In the Running state, a miniport driver performs send and
    // receive processing for an adapter.
    MiniportRunning,

    // In the Restarting state, a miniport driver completes any
    // operations that are required to restart send and receive
    // operations for an adapter.
    MiniportRestartingState
} TAP_MINIPORT_ADAPTER_STATE, *PTAP_MINIPORT_ADAPTER_STATE;

//
// Each adapter managed by this driver has a TapAdapter struct.
// ------------------------------------------------------------
// Since there is a one-to-one relationship between adapter instances
// and device instances this structure is the device extension as well.
//
typedef struct _TAP_ADAPTER_CONTEXT
{
    LIST_ENTRY                  AdapterListLink;

    volatile LONG               RefCount;

    NDIS_HANDLE                 MiniportAdapterHandle;

    NDIS_SPIN_LOCK              AdapterLock;    // Lock for protection of state and outstanding sends and recvs

    //
    // All fields that are protected by the AdapterLock are included
    // in the Locked structure to remind us to take the Lock
    // before accessing them :)
    //
    struct
    {
        TAP_MINIPORT_ADAPTER_STATE  AdapterState;
    } Locked;

    BOOLEAN                     ResetInProgress;

    //
    // NetCfgInstanceId as UNICODE_STRING
    // ----------------------------------
    // This a GUID string provided by NDIS that identifies the adapter instance.
    // An example is:
    // 
    //    NetCfgInstanceId={410EB49D-2381-4FE7-9B36-498E22619DF0}
    //
    // Other names are derived from NetCfgInstanceId. For example, MiniportName:
    //
    //    MiniportName=\DEVICE\{410EB49D-2381-4FE7-9B36-498E22619DF0}
    //
    NDIS_STRING                 NetCfgInstanceId;
    WCHAR                       NetCfgInstanceIdBuffer[TAP_MAX_NDIS_NAME_LENGTH];

# define MINIPORT_INSTANCE_ID(a) ((a)->NetCfgInstanceIdAnsi.Buffer)
    ANSI_STRING                 NetCfgInstanceIdAnsi;   // Used occasionally

    ULONG                       MtuSize;        // 1500 byte (typical)

    // TRUE if adapter should always be "connected" even when device node
    // is not open by a userspace process.
    //
    // FALSE if connection state is application controlled.
    BOOLEAN                     MediaStateAlwaysConnected;

    // TRUE if device is "connected".
    BOOLEAN                     LogicalMediaState;

    NDIS_DEVICE_POWER_STATE     CurrentPowerState;

    BOOLEAN                     AllowNonAdmin;

    MACADDR                     PermanentAddress;   // From registry, if available
    MACADDR                     CurrentAddress;

    // Device registration parameters from NdisRegisterDeviceEx.
    NDIS_STRING                 DeviceName;
    WCHAR                       DeviceNameBuffer[TAP_MAX_NDIS_NAME_LENGTH];

    NDIS_STRING                 LinkName;
    WCHAR                       LinkNameBuffer[TAP_MAX_NDIS_NAME_LENGTH];

    NDIS_HANDLE                 DeviceHandle;
    PDEVICE_OBJECT              DeviceObject;
    BOOLEAN                     TapDeviceCreated;   // WAS: m_TapIsRunning

    PFILE_OBJECT                TapFileObject;      // Exclusive access
    BOOLEAN                     TapFileIsOpen;      // WAS: m_TapOpens
    LONG                        TapFileOpenCount;   // WAS: m_NumTapOpens

    // Cancel-Safe read IRP queue.
    TAP_IRP_CSQ                 PendingReadIrpQueue;

    // Queue containing TAP packets representing host send NBs. These are
    // waiting to be read by user-mode application.
    TAP_PACKET_QUEUE            SendPacketQueue;

    // NBL pool for making TAP receive indications.
    NDIS_HANDLE                 ReceiveNblPool;

    volatile LONG               ReceiveNblInFlightCount;
#define TAP_WAIT_POLL_LOOP_TIMEOUT  3000    // 3 seconds
    NDIS_EVENT                  ReceiveNblInFlightCountZeroEvent;

	/*
    // Info for point-to-point mode
    BOOLEAN                     m_tun;
    IPADDR                      m_localIP;
    IPADDR                      m_remoteNetwork;
    IPADDR                      m_remoteNetmask;
    ETH_HEADER                  m_TapToUser;
    ETH_HEADER                  m_UserToTap;
    ETH_HEADER                  m_UserToTap_IPv6; // same as UserToTap but proto=ipv6
	*/

	// Info for DHCP server masquerade
	/*
    BOOLEAN                     m_dhcp_enabled;
    IPADDR                      m_dhcp_addr;
    ULONG                       m_dhcp_netmask;
    IPADDR                      m_dhcp_server_ip;
    BOOLEAN                     m_dhcp_server_arp;
    MACADDR                     m_dhcp_server_mac;
    ULONG                       m_dhcp_lease_time;
    UCHAR                       m_dhcp_user_supplied_options_buffer[DHCP_USER_SUPPLIED_OPTIONS_BUFFER_SIZE];
    ULONG                       m_dhcp_user_supplied_options_buffer_len;
    BOOLEAN                     m_dhcp_received_discover;
    ULONG                       m_dhcp_bad_requests;
	*/

    // Multicast list. Fixed size.
    ULONG                       ulMCListSize;
    UCHAR                       MCList[TAP_MAX_MCAST_LIST][MACADDR_SIZE];

    ULONG                       PacketFilter;
    ULONG                       ulLookahead;

    //
    // Statistics
    // -------------------------------------------------------------------------
    //

    // Packet counts
    ULONG64                     FramesRxDirected;
    ULONG64                     FramesRxMulticast;
    ULONG64                     FramesRxBroadcast;
    ULONG64                     FramesTxDirected;
    ULONG64                     FramesTxMulticast;
    ULONG64                     FramesTxBroadcast;

    // Byte counts
    ULONG64                     BytesRxDirected;
    ULONG64                     BytesRxMulticast;
    ULONG64                     BytesRxBroadcast;
    ULONG64                     BytesTxDirected;
    ULONG64                     BytesTxMulticast;
    ULONG64                     BytesTxBroadcast;

    // Count of transmit errors
    ULONG                       TxAbortExcessCollisions;
    ULONG                       TxLateCollisions;
    ULONG                       TxDmaUnderrun;
    ULONG                       TxLostCRS;
    ULONG                       TxOKButDeferred;
    ULONG                       OneRetry;
    ULONG                       MoreThanOneRetry;
    ULONG                       TotalRetries;
    ULONG                       TransmitFailuresOther;

    // Count of receive errors
    ULONG                       RxCrcErrors;
    ULONG                       RxAlignmentErrors;
    ULONG                       RxResourceErrors;
    ULONG                       RxDmaOverrunErrors;
    ULONG                       RxCdtFrames;
    ULONG                       RxRuntErrors;

#if PACKET_TRUNCATION_CHECK
    LONG                        m_RxTrunc, m_TxTrunc;
#endif

  BOOLEAN m_InterfaceIsRunning;
  LONG m_Rx, m_RxErr;
  NDIS_MEDIUM m_Medium;

  // Help to tear down the adapter by keeping
  // some state information on allocated
  // resources.
  BOOLEAN m_CalledAdapterFreeResources;
  BOOLEAN m_RegisteredAdapterShutdownHandler;

} TAP_ADAPTER_CONTEXT, *PTAP_ADAPTER_CONTEXT;

FORCEINLINE
LONG
tapAdapterContextReference(
    __in PTAP_ADAPTER_CONTEXT   Adapter
    )
{
    LONG    refCount = NdisInterlockedIncrement(&Adapter->RefCount);

    ASSERT(refCount>1);     // Cannot dereference a zombie.

    return refCount;
}

VOID
tapAdapterContextFree(
    __in PTAP_ADAPTER_CONTEXT     Adapter
    );

FORCEINLINE
LONG
tapAdapterContextDereference(
    IN PTAP_ADAPTER_CONTEXT     Adapter
    )
{
    LONG    refCount = NdisInterlockedDecrement(&Adapter->RefCount);
    ASSERT(refCount >= 0);
    if (!refCount)
    {
        tapAdapterContextFree(Adapter);
    }

    return refCount;
}

VOID
tapAdapterAcquireLock(
    __in    PTAP_ADAPTER_CONTEXT    Adapter,
    __in    BOOLEAN                 DispatchLevel
    );

VOID
tapAdapterReleaseLock(
    __in    PTAP_ADAPTER_CONTEXT    Adapter,
    __in    BOOLEAN                 DispatchLevel
    );

// Returns with added reference on adapter context.
PTAP_ADAPTER_CONTEXT
tapAdapterContextFromDeviceObject(
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
tapAdapterReadAndWriteReady(
    __in PTAP_ADAPTER_CONTEXT     Adapter
    );

NDIS_STATUS
tapAdapterSendAndReceiveReady(
    __in PTAP_ADAPTER_CONTEXT     Adapter
    );

ULONG
tapGetNetBufferFrameType(
    __in PNET_BUFFER       NetBuffer
    );

ULONG
tapGetNetBufferCountsFromNetBufferList(
    __in PNET_BUFFER_LIST   NetBufferList,
    __inout_opt PULONG      TotalByteCount      // Of all linked NBs
    );

// Prototypes for standard NDIS miniport entry points
MINIPORT_SET_OPTIONS                AdapterSetOptions;
MINIPORT_INITIALIZE                 AdapterCreate;
MINIPORT_HALT                       AdapterHalt;
MINIPORT_UNLOAD                     TapDriverUnload;
MINIPORT_PAUSE                      AdapterPause;
MINIPORT_RESTART                    AdapterRestart;
MINIPORT_OID_REQUEST                AdapterOidRequest;
MINIPORT_SEND_NET_BUFFER_LISTS      AdapterSendNetBufferLists;
MINIPORT_RETURN_NET_BUFFER_LISTS    AdapterReturnNetBufferLists;
MINIPORT_CANCEL_SEND                AdapterCancelSend;
MINIPORT_CHECK_FOR_HANG             AdapterCheckForHangEx;
MINIPORT_RESET                      AdapterReset;
MINIPORT_DEVICE_PNP_EVENT_NOTIFY    AdapterDevicePnpEventNotify;
MINIPORT_SHUTDOWN                   AdapterShutdownEx;
MINIPORT_CANCEL_OID_REQUEST         AdapterCancelOidRequest;

#endif // __TAP_ADAPTER_CONTEXT_H_