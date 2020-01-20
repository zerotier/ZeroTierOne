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
#include <wdmsec.h> // for SDDLs

//======================================================================
// TAP Win32 Device I/O Callbacks
//======================================================================

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, TapDeviceCreate)
#pragma alloc_text( PAGE, TapDeviceControl)
#pragma alloc_text( PAGE, TapDeviceCleanup)
#pragma alloc_text( PAGE, TapDeviceClose)
#endif // ALLOC_PRAGMA

//===================================================================
// Go back to default TAP mode from Point-To-Point mode.
// Also reset (i.e. disable) DHCP Masq mode.
//===================================================================
VOID tapResetAdapterState(
    __in PTAP_ADAPTER_CONTEXT Adapter
    )
{
  /*
  // Point-To-Point
  Adapter->m_tun = FALSE;
  Adapter->m_localIP = 0;
  Adapter->m_remoteNetwork = 0;
  Adapter->m_remoteNetmask = 0;
  NdisZeroMemory (&Adapter->m_TapToUser, sizeof (Adapter->m_TapToUser));
  NdisZeroMemory (&Adapter->m_UserToTap, sizeof (Adapter->m_UserToTap));
  NdisZeroMemory (&Adapter->m_UserToTap_IPv6, sizeof (Adapter->m_UserToTap_IPv6));
  */

  // DHCP Masq
  /*
  Adapter->m_dhcp_enabled = FALSE;
  Adapter->m_dhcp_server_arp = FALSE;
  Adapter->m_dhcp_user_supplied_options_buffer_len = 0;
  Adapter->m_dhcp_addr = 0;
  Adapter->m_dhcp_netmask = 0;
  Adapter->m_dhcp_server_ip = 0;
  Adapter->m_dhcp_lease_time = 0;
  Adapter->m_dhcp_received_discover = FALSE;
  Adapter->m_dhcp_bad_requests = 0;
  NdisZeroMemory (Adapter->m_dhcp_server_mac, MACADDR_SIZE);
  */
}

// IRP_MJ_CREATE
NTSTATUS
TapDeviceCreate(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
/*++

Routine Description:

    This routine is called by the I/O system when the device is opened.

    No action is performed other than completing the request successfully.

Arguments:

    DeviceObject - a pointer to the object that represents the device
    that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    NT status code

--*/
{
    NDIS_STATUS             status;
    PIO_STACK_LOCATION      irpSp;// Pointer to current stack location
    PTAP_ADAPTER_CONTEXT    adapter = NULL;
    PFILE_OBJECT            originalFileObject;

    PAGED_CODE();

    DEBUGP (("[TAP] --> TapDeviceCreate\n"));

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    //
    // Invalidate file context
    //
    irpSp->FileObject->FsContext = NULL;
    irpSp->FileObject->FsContext2 = NULL;

    //
    // Find adapter context for this device.
    // -------------------------------------
    // Returns with added reference on adapter context.
    //
    adapter = tapAdapterContextFromDeviceObject(DeviceObject);

    // Insure that adapter exists.
    ASSERT(adapter);

    if(adapter == NULL )
    {
        DEBUGP (("[TAP] release [%d.%d] open request; adapter not found\n",
            TAP_DRIVER_MAJOR_VERSION,
            TAP_DRIVER_MINOR_VERSION
            ));

        Irp->IoStatus.Status = STATUS_DEVICE_DOES_NOT_EXIST;
        Irp->IoStatus.Information = 0;

        IoCompleteRequest( Irp, IO_NO_INCREMENT );

        return STATUS_DEVICE_DOES_NOT_EXIST;
    }

    DEBUGP(("[%s] [TAP] release [%d.%d] open request (TapFileIsOpen=%d)\n",
        MINIPORT_INSTANCE_ID(adapter),
        TAP_DRIVER_MAJOR_VERSION,
        TAP_DRIVER_MINOR_VERSION,
        adapter->TapFileIsOpen
        ));

    // Enforce exclusive access
    originalFileObject = InterlockedCompareExchangePointer(
                    &adapter->TapFileObject,
                    irpSp->FileObject,
                    NULL
                    );

    if(originalFileObject == NULL)
    {
        irpSp->FileObject->FsContext = adapter; // Quick reference

        status = STATUS_SUCCESS;
    }
    else
    {
        status = STATUS_UNSUCCESSFUL;
    }

    // Release the lock.
    //tapAdapterReleaseLock(adapter,FALSE);

    if(status == STATUS_SUCCESS)
    {
        // Reset adapter state on successful open.
        tapResetAdapterState(adapter);

        adapter->TapFileIsOpen = 1;    // Legacy...

        // NOTE!!! Reference added by tapAdapterContextFromDeviceObject
        // will be removed when file is closed.
    }
    else
    {
        DEBUGP (("[%s] TAP is presently unavailable (TapFileIsOpen=%d)\n",
            MINIPORT_INSTANCE_ID(adapter), adapter->TapFileIsOpen
            ));

        NOTE_ERROR();

        // Remove reference added by tapAdapterContextFromDeviceObject.
        tapAdapterContextDereference(adapter);
    }

    // Complete the IRP.
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    DEBUGP (("[TAP] <-- TapDeviceCreate; status = %8.8X\n",status));

    return status;
}

//===================================================
// Tell Windows whether the TAP device should be
// considered "connected" or "disconnected".
//
// Allows application control of media connect state.
//===================================================
VOID
tapSetMediaConnectStatus(
    __in PTAP_ADAPTER_CONTEXT   Adapter,
    __in BOOLEAN                LogicalMediaState
    )
{
    NDIS_STATUS_INDICATION  statusIndication;
    NDIS_LINK_STATE         linkState;

    NdisZeroMemory(&statusIndication, sizeof(NDIS_STATUS_INDICATION));
    NdisZeroMemory(&linkState, sizeof(NDIS_LINK_STATE));

    //
    // Fill in object headers
    //
    statusIndication.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
    statusIndication.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
    statusIndication.Header.Size = sizeof(NDIS_STATUS_INDICATION);

    linkState.Header.Revision = NDIS_LINK_STATE_REVISION_1;
    linkState.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    linkState.Header.Size = sizeof(NDIS_LINK_STATE);

    //
    // Link state buffer
    //
    if(Adapter->LogicalMediaState == TRUE)
    {
        linkState.MediaConnectState = MediaConnectStateConnected;
    }

    linkState.MediaDuplexState = MediaDuplexStateFull;
    linkState.RcvLinkSpeed = TAP_RECV_SPEED;
    linkState.XmitLinkSpeed = TAP_XMIT_SPEED;

    //
    // Fill in the status buffer
    // 
    statusIndication.StatusCode = NDIS_STATUS_LINK_STATE;
    statusIndication.SourceHandle = Adapter->MiniportAdapterHandle;
    statusIndication.DestinationHandle = NULL;
    statusIndication.RequestId = 0;

    statusIndication.StatusBuffer = &linkState;
    statusIndication.StatusBufferSize = sizeof(NDIS_LINK_STATE);

    // Fill in new media connect state.
    if ( (Adapter->LogicalMediaState != LogicalMediaState) && !Adapter->MediaStateAlwaysConnected)
    {
        Adapter->LogicalMediaState = LogicalMediaState;

        if (LogicalMediaState == TRUE)
        {
            linkState.MediaConnectState = MediaConnectStateConnected;

            DEBUGP (("[TAP] Set MediaConnectState: Connected.\n"));
        }
        else
        {
            linkState.MediaConnectState = MediaConnectStateDisconnected;

            DEBUGP (("[TAP] Set MediaConnectState: Disconnected.\n"));
        }
    }

    // Make the status indication.
    if(Adapter->Locked.AdapterState != MiniportHaltedState)
    {
        NdisMIndicateStatusEx(Adapter->MiniportAdapterHandle, &statusIndication);
    }
}

/*
//======================================================
// If DHCP mode is used together with tun
// mode, consider the fact that the P2P remote subnet
// might enclose the DHCP masq server address.
//======================================================
VOID
CheckIfDhcpAndTunMode (
    __in PTAP_ADAPTER_CONTEXT   Adapter
    )
{
    if (Adapter->m_tun && Adapter->m_dhcp_enabled)
    {
        if ((Adapter->m_dhcp_server_ip & Adapter->m_remoteNetmask) == Adapter->m_remoteNetwork)
        {
            ETH_COPY_NETWORK_ADDRESS (Adapter->m_dhcp_server_mac, Adapter->m_TapToUser.dest);
            Adapter->m_dhcp_server_arp = FALSE;
        }
    }
}
*/

// IRP_MJ_DEVICE_CONTROL callback.
NTSTATUS
TapDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O system to perform a device I/O
    control function.

Arguments:

    DeviceObject - a pointer to the object that represents the device
        that I/O is to be done on.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    NT status code

--*/

{
    NTSTATUS                ntStatus = STATUS_SUCCESS; // Assume success
    PIO_STACK_LOCATION      irpSp; // Pointer to current stack location
    PTAP_ADAPTER_CONTEXT    adapter = NULL;
    ULONG                   inBufLength; // Input buffer length
    ULONG                   outBufLength; // Output buffer length
    PCHAR                   inBuf, outBuf; // pointer to Input and output buffer
    PMDL                    mdl = NULL;
    PCHAR                   buffer = NULL;

    PAGED_CODE();

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    // Fetch adapter context for this device.
    // --------------------------------------
    // Adapter pointer was stashed in FsContext when handle was opened.
    //
    adapter = (PTAP_ADAPTER_CONTEXT )(irpSp->FileObject)->FsContext;

    ASSERT(adapter);

    inBufLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    outBufLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

    if (!inBufLength || !outBufLength)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto End;
    }

    //
    // Determine which I/O control code was specified.
    //
    switch ( irpSp->Parameters.DeviceIoControl.IoControlCode )
    {
    case TAP_WIN_IOCTL_GET_MAC:
        {
            if (outBufLength >= MACADDR_SIZE )
            {
                ETH_COPY_NETWORK_ADDRESS(
                    Irp->AssociatedIrp.SystemBuffer,
                    adapter->CurrentAddress
                    );

                Irp->IoStatus.Information = MACADDR_SIZE;
            }
            else
            {
                NOTE_ERROR();
                Irp->IoStatus.Status = ntStatus = STATUS_BUFFER_TOO_SMALL;
            }
        }
        break;

    case TAP_WIN_IOCTL_GET_VERSION:
        {
            const ULONG size = sizeof (ULONG) * 3;

            if (outBufLength >= size)
            {
                ((PULONG) (Irp->AssociatedIrp.SystemBuffer))[0]
                    = TAP_DRIVER_MAJOR_VERSION;

                ((PULONG) (Irp->AssociatedIrp.SystemBuffer))[1]
                    = TAP_DRIVER_MINOR_VERSION;

                ((PULONG) (Irp->AssociatedIrp.SystemBuffer))[2]
#if DBG
                    = 1;
#else
                    = 0;
#endif
                Irp->IoStatus.Information = size;
            }
            else
            {
                NOTE_ERROR();
                Irp->IoStatus.Status = ntStatus = STATUS_BUFFER_TOO_SMALL;
            }
        }
        break;

    case TAP_WIN_IOCTL_GET_MTU:
        {
            const ULONG size = sizeof (ULONG) * 1;

            if (outBufLength >= size)
            {
                ((PULONG) (Irp->AssociatedIrp.SystemBuffer))[0]
                    = adapter->MtuSize;

                Irp->IoStatus.Information = size;
            }
            else
            {
                NOTE_ERROR();
                Irp->IoStatus.Status = ntStatus = STATUS_BUFFER_TOO_SMALL;
            }
        }
        break;

			// Allow ZeroTier One to get multicast memberships at the L2 level in a
			// protocol-neutral manner.
			case TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS:
				{
					if (outBufLength < TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS_OUTPUT_BUF_SIZE) {
						/* output buffer too small */
						NOTE_ERROR ();
		                Irp->IoStatus.Status = ntStatus = STATUS_BUFFER_TOO_SMALL;
					} else {
						char *out = (char *)Irp->AssociatedIrp.SystemBuffer;
						char *end = out + TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS_OUTPUT_BUF_SIZE;
						unsigned long i,j;
						for(i=0;i<adapter->ulMCListSize;++i) {
							if (i >= TAP_MAX_MCAST_LIST)
								break;
							for(j=0;j<6;++j)
								*(out++) = adapter->MCList[i][j];
							if (out >= end)
								break;
						}
						while (out < end)
							*(out++) = (char)0;
		                Irp->IoStatus.Information = TAP_WIN_IOCTL_GET_MULTICAST_MEMBERSHIPS_OUTPUT_BUF_SIZE;
						Irp->IoStatus.Status = ntStatus = STATUS_SUCCESS;
					}
					break;
				}


#if 0
    case TAP_WIN_IOCTL_CONFIG_TUN:
        {
            if(inBufLength >= sizeof(IPADDR)*3)
            {
                MACADDR dest;

                adapter->m_tun = FALSE;

                GenerateRelatedMAC (dest, adapter->CurrentAddress, 1);

                adapter->m_localIP =       ((IPADDR*) (Irp->AssociatedIrp.SystemBuffer))[0];
                adapter->m_remoteNetwork = ((IPADDR*) (Irp->AssociatedIrp.SystemBuffer))[1];
                adapter->m_remoteNetmask = ((IPADDR*) (Irp->AssociatedIrp.SystemBuffer))[2];

                // Sanity check on network/netmask
                if ((adapter->m_remoteNetwork & adapter->m_remoteNetmask) != adapter->m_remoteNetwork)
                {
                    NOTE_ERROR();
                    Irp->IoStatus.Status = ntStatus = STATUS_INVALID_PARAMETER;
                    break;
                }

                ETH_COPY_NETWORK_ADDRESS (adapter->m_TapToUser.src, adapter->CurrentAddress);
                ETH_COPY_NETWORK_ADDRESS (adapter->m_TapToUser.dest, dest);
                ETH_COPY_NETWORK_ADDRESS (adapter->m_UserToTap.src, dest);
                ETH_COPY_NETWORK_ADDRESS (adapter->m_UserToTap.dest, adapter->CurrentAddress);

                adapter->m_TapToUser.proto = adapter->m_UserToTap.proto = htons (NDIS_ETH_TYPE_IPV4);
                adapter->m_UserToTap_IPv6 = adapter->m_UserToTap;
                adapter->m_UserToTap_IPv6.proto = htons(NDIS_ETH_TYPE_IPV6);

                adapter->m_tun = TRUE;

                CheckIfDhcpAndTunMode (adapter);

                Irp->IoStatus.Information = 1; // Simple boolean value

                DEBUGP (("[TAP] Set TUN mode.\n"));
            }
            else
            {
                NOTE_ERROR();
                Irp->IoStatus.Status = ntStatus = STATUS_INVALID_PARAMETER;
            }
        }
        break;

    case TAP_WIN_IOCTL_CONFIG_POINT_TO_POINT:
        {
            if(inBufLength >= sizeof(IPADDR)*2)
            {
                MACADDR dest;

                adapter->m_tun = FALSE;

                GenerateRelatedMAC (dest, adapter->CurrentAddress, 1);

                adapter->m_localIP =       ((IPADDR*) (Irp->AssociatedIrp.SystemBuffer))[0];
                adapter->m_remoteNetwork = ((IPADDR*) (Irp->AssociatedIrp.SystemBuffer))[1];
                adapter->m_remoteNetmask = ~0;

                ETH_COPY_NETWORK_ADDRESS (adapter->m_TapToUser.src, adapter->CurrentAddress);
                ETH_COPY_NETWORK_ADDRESS (adapter->m_TapToUser.dest, dest);
                ETH_COPY_NETWORK_ADDRESS (adapter->m_UserToTap.src, dest);
                ETH_COPY_NETWORK_ADDRESS (adapter->m_UserToTap.dest, adapter->CurrentAddress);

                adapter->m_TapToUser.proto = adapter->m_UserToTap.proto = htons (NDIS_ETH_TYPE_IPV4);
                adapter->m_UserToTap_IPv6 = adapter->m_UserToTap;
                adapter->m_UserToTap_IPv6.proto = htons(NDIS_ETH_TYPE_IPV6);

                adapter->m_tun = TRUE;

                CheckIfDhcpAndTunMode (adapter);

                Irp->IoStatus.Information = 1; // Simple boolean value

                DEBUGP (("[TAP] Set P2P mode.\n"));
            }
            else
            {
                NOTE_ERROR();
                Irp->IoStatus.Status = ntStatus = STATUS_INVALID_PARAMETER;
            }
        }
        break;
#endif

#if 0
    case TAP_WIN_IOCTL_CONFIG_DHCP_MASQ:
        {
            if(inBufLength >= sizeof(IPADDR)*4)
            {
                adapter->m_dhcp_enabled = FALSE;
                adapter->m_dhcp_server_arp = FALSE;
                adapter->m_dhcp_user_supplied_options_buffer_len = 0;

                // Adapter IP addr / netmask
                adapter->m_dhcp_addr =
                    ((IPADDR*) (Irp->AssociatedIrp.SystemBuffer))[0];
                adapter->m_dhcp_netmask =
                    ((IPADDR*) (Irp->AssociatedIrp.SystemBuffer))[1];

                // IP addr of DHCP masq server
                adapter->m_dhcp_server_ip =
                    ((IPADDR*) (Irp->AssociatedIrp.SystemBuffer))[2];

                // Lease time in seconds
                adapter->m_dhcp_lease_time =
                    ((IPADDR*) (Irp->AssociatedIrp.SystemBuffer))[3];

                GenerateRelatedMAC(
                    adapter->m_dhcp_server_mac,
                    adapter->CurrentAddress,
                    2
                    );

                adapter->m_dhcp_enabled = TRUE;
                adapter->m_dhcp_server_arp = TRUE;

                CheckIfDhcpAndTunMode (adapter);

                Irp->IoStatus.Information = 1; // Simple boolean value

                DEBUGP (("[TAP] Configured DHCP MASQ.\n"));
            }
            else
            {
                NOTE_ERROR();
                Irp->IoStatus.Status = ntStatus = STATUS_INVALID_PARAMETER;
            }
        }
        break;

    case TAP_WIN_IOCTL_CONFIG_DHCP_SET_OPT:
        {
            if (inBufLength <=  DHCP_USER_SUPPLIED_OPTIONS_BUFFER_SIZE
                && adapter->m_dhcp_enabled)
            {
                adapter->m_dhcp_user_supplied_options_buffer_len = 0;

                NdisMoveMemory(
                    adapter->m_dhcp_user_supplied_options_buffer,
                    Irp->AssociatedIrp.SystemBuffer,
                    inBufLength
                    );

                adapter->m_dhcp_user_supplied_options_buffer_len = 
                    inBufLength;

                Irp->IoStatus.Information = 1; // Simple boolean value

                DEBUGP (("[TAP] Set DHCP OPT.\n"));
            }
            else
            {
                NOTE_ERROR();
                Irp->IoStatus.Status = ntStatus = STATUS_INVALID_PARAMETER;
            }
        }
        break;
#endif

#if 0
    case TAP_WIN_IOCTL_GET_INFO:
        {
            char state[16];

            // Fetch adapter (miniport) state.
            if (tapAdapterSendAndReceiveReady(adapter) == NDIS_STATUS_SUCCESS)
                state[0] = 'A';
            else
                state[0] = 'a';

            if (tapAdapterReadAndWriteReady(adapter))
                state[1] = 'T';
            else
                state[1] = 't';

            state[2] = '0' + adapter->CurrentPowerState;

            if (adapter->MediaStateAlwaysConnected)
                state[3] = 'C';
            else
                state[3] = 'c';

            state[4] = '\0';

            // BUGBUG!!! What follows, and is not yet implemented, is a real mess.
            // BUGBUG!!! Tied closely to the NDIS 5 implementation. Need to map
            //    as much as possible to the NDIS 6 implementation.
            Irp->IoStatus.Status = ntStatus = RtlStringCchPrintfExA (
                ((LPTSTR) (Irp->AssociatedIrp.SystemBuffer)),
                outBufLength,
                NULL,
                NULL,
                STRSAFE_FILL_BEHIND_NULL | STRSAFE_IGNORE_NULLS,
#if PACKET_TRUNCATION_CHECK
                "State=%s Err=[%s/%d] #O=%d Tx=[%d,%d,%d] Rx=[%d,%d,%d] IrpQ=[%d,%d,%d] PktQ=[%d,%d,%d] InjQ=[%d,%d,%d]",
#else
                "State=%s Err=[%s/%d] #O=%d Tx=[%d,%d] Rx=[%d,%d] IrpQ=[%d,%d,%d] PktQ=[%d,%d,%d] InjQ=[%d,%d,%d]",
#endif
                state,
                g_LastErrorFilename,
                g_LastErrorLineNumber,
                (int)adapter->TapFileOpenCount,
                (int)(adapter->FramesTxDirected + adapter->FramesTxMulticast + adapter->FramesTxBroadcast),
                (int)adapter->TransmitFailuresOther,
#if PACKET_TRUNCATION_CHECK
                (int)adapter->m_TxTrunc,
#endif
                (int)adapter->m_Rx,
                (int)adapter->m_RxErr,
#if PACKET_TRUNCATION_CHECK
                (int)adapter->m_RxTrunc,
#endif
                (int)adapter->PendingReadIrpQueue.Count,
                (int)adapter->PendingReadIrpQueue.MaxCount,
                (int)IRP_QUEUE_SIZE,        // Ignored in NDIS 6 driver...

                (int)adapter->SendPacketQueue.Count,
                (int)adapter->SendPacketQueue.MaxCount,
                (int)PACKET_QUEUE_SIZE,

                (int)0,         // adapter->InjectPacketQueue.Count - Unused
                (int)0,         // adapter->InjectPacketQueue.MaxCount - Unused
                (int)INJECT_QUEUE_SIZE
                );

            Irp->IoStatus.Information = outBufLength;

            // BUGBUG!!! Fail because this is not completely implemented.
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        }
#endif    

#if DBG
    case TAP_WIN_IOCTL_GET_LOG_LINE:
        {
            if (GetDebugLine( (LPTSTR)Irp->AssociatedIrp.SystemBuffer,outBufLength))
            {
                Irp->IoStatus.Status = ntStatus = STATUS_SUCCESS;
            }
            else
            {
                Irp->IoStatus.Status = ntStatus = STATUS_UNSUCCESSFUL;
            }

            Irp->IoStatus.Information = outBufLength;

            break;
        }
#endif

    case TAP_WIN_IOCTL_SET_MEDIA_STATUS:
        {
            if(inBufLength >= sizeof(ULONG))
            {
                ULONG parm = ((PULONG) (Irp->AssociatedIrp.SystemBuffer))[0];
                tapSetMediaConnectStatus (adapter, (BOOLEAN) parm);
                Irp->IoStatus.Information = 1;
            }
            else
            {
                NOTE_ERROR();
                Irp->IoStatus.Status = ntStatus = STATUS_INVALID_PARAMETER;
            }
        }
        break;

    default:

        //
        // The specified I/O control code is unrecognized by this driver.
        //
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

End:

    //
    // Finish the I/O operation by simply completing the packet and returning
    // the same status as in the packet itself.
    //
    Irp->IoStatus.Status = ntStatus;

    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    return ntStatus;
}

// Flush the pending read IRP queue.
VOID
tapFlushIrpQueues(
    __in PTAP_ADAPTER_CONTEXT   Adapter
    )
{

    DEBUGP (("[TAP] tapFlushIrpQueues: Flushing %d pending read IRPs\n",
        Adapter->PendingReadIrpQueue.Count));

    tapIrpCsqFlush(&Adapter->PendingReadIrpQueue);
}

// IRP_MJ_CLEANUP
NTSTATUS
TapDeviceCleanup(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
/*++

Routine Description:

    Receipt of this request indicates that the last handle for a file
    object that is associated with the target device object has been closed
    (but, due to outstanding I/O requests, might not have been released).

    A driver that holds pending IRPs internally must implement a routine for
    IRP_MJ_CLEANUP. When the routine is called, the driver should cancel all
    the pending IRPs that belong to the file object identified by the IRP_MJ_CLEANUP
    call.
    
    In other words, it should cancel all the IRPs that have the same file-object
    pointer as the one supplied in the current I/O stack location of the IRP for the
    IRP_MJ_CLEANUP call. Of course, IRPs belonging to other file objects should
    not be canceled. Also, if an outstanding IRP is completed immediately, the
    driver does not have to cancel it.

Arguments:

    DeviceObject - a pointer to the object that represents the device
    to be cleaned up.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    NT status code

--*/

{
    NDIS_STATUS             status = NDIS_STATUS_SUCCESS;   // Always succeed.
    PIO_STACK_LOCATION      irpSp;  // Pointer to current stack location
    PTAP_ADAPTER_CONTEXT    adapter = NULL;

    PAGED_CODE();

    DEBUGP (("[TAP] --> TapDeviceCleanup\n"));

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    //
    // Fetch adapter context for this device.
    // --------------------------------------
    // Adapter pointer was stashed in FsContext when handle was opened.
    //
    adapter = (PTAP_ADAPTER_CONTEXT )(irpSp->FileObject)->FsContext;

    // Insure that adapter exists.
    ASSERT(adapter);

    if(adapter == NULL )
    {
        DEBUGP (("[TAP] release [%d.%d] cleanup request; adapter not found\n",
            TAP_DRIVER_MAJOR_VERSION,
            TAP_DRIVER_MINOR_VERSION
            ));
    }

    if(adapter != NULL )
    {
        adapter->TapFileIsOpen = 0;    // Legacy...

        // Disconnect from media.
        tapSetMediaConnectStatus(adapter,FALSE);

        // Reset adapter state when cleaning up;
        tapResetAdapterState(adapter);

        // BUGBUG!!! Use RemoveLock???

        //
        // Flush pending send TAP packet queue.
        //
        tapFlushSendPacketQueue(adapter);

        ASSERT(adapter->SendPacketQueue.Count == 0);

        //
        // Flush the pending IRP queues
        //
        tapFlushIrpQueues(adapter);

        ASSERT(adapter->PendingReadIrpQueue.Count == 0);
    }

    // Complete the IRP.
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    DEBUGP (("[TAP] <-- TapDeviceCleanup; status = %8.8X\n",status));

    return status;
}

// IRP_MJ_CLOSE
NTSTATUS
TapDeviceClose(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
/*++

Routine Description:

    Receipt of this request indicates that the last handle of the file
    object that is associated with the target device object has been closed
    and released.
    
    All outstanding I/O requests have been completed or canceled.

Arguments:

    DeviceObject - a pointer to the object that represents the device
    to be closed.

    Irp - a pointer to the I/O Request Packet for this request.

Return Value:

    NT status code

--*/

{
    NDIS_STATUS             status = NDIS_STATUS_SUCCESS;   // Always succeed.
    PIO_STACK_LOCATION      irpSp;  // Pointer to current stack location
    PTAP_ADAPTER_CONTEXT    adapter = NULL;

    PAGED_CODE();

    DEBUGP (("[TAP] --> TapDeviceClose\n"));

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    //
    // Fetch adapter context for this device.
    // --------------------------------------
    // Adapter pointer was stashed in FsContext when handle was opened.
    //
    adapter = (PTAP_ADAPTER_CONTEXT )(irpSp->FileObject)->FsContext;

    // Insure that adapter exists.
    ASSERT(adapter);

    if(adapter == NULL )
    {
        DEBUGP (("[TAP] release [%d.%d] close request; adapter not found\n",
            TAP_DRIVER_MAJOR_VERSION,
            TAP_DRIVER_MINOR_VERSION
            ));
    }

    if(adapter != NULL )
    {
        if(adapter->TapFileObject == NULL)
        {
            // Should never happen!!!
            ASSERT(FALSE);
        }
        else
        {
            ASSERT(irpSp->FileObject->FsContext == adapter);

            ASSERT(adapter->TapFileObject == irpSp->FileObject);
        }

        adapter->TapFileObject = NULL;
        irpSp->FileObject = NULL;

        // Remove reference added by when handle was opened.
        tapAdapterContextDereference(adapter);
    }

    // Complete the IRP.
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    DEBUGP (("[TAP] <-- TapDeviceClose; status = %8.8X\n",status));

    return status;
}

NTSTATUS
tapConcatenateNdisStrings(
    __inout     PNDIS_STRING    DestinationString,
    __in_opt    PNDIS_STRING    SourceString1,
    __in_opt    PNDIS_STRING    SourceString2,
    __in_opt    PNDIS_STRING    SourceString3
    )
{
    NTSTATUS status;

    ASSERT(SourceString1 && SourceString2 && SourceString3);

    status = RtlAppendUnicodeStringToString(
                DestinationString,
                SourceString1
                );

    if(status == STATUS_SUCCESS)
    {
        status = RtlAppendUnicodeStringToString(
                    DestinationString,
                    SourceString2
                    );

        if(status == STATUS_SUCCESS)
        {
            status = RtlAppendUnicodeStringToString(
                        DestinationString,
                        SourceString3
                        );
        }
    }

    return status;
}

NTSTATUS
tapMakeDeviceNames(
    __in PTAP_ADAPTER_CONTEXT   Adapter
    )
{
    NDIS_STATUS     status;
    NDIS_STRING     deviceNamePrefix = NDIS_STRING_CONST("\\Device\\");
    NDIS_STRING     tapNameSuffix = NDIS_STRING_CONST(".tap");

    // Generate DeviceName from NetCfgInstanceId.
    Adapter->DeviceName.Buffer = Adapter->DeviceNameBuffer;
    Adapter->DeviceName.MaximumLength = sizeof(Adapter->DeviceNameBuffer);

    status = tapConcatenateNdisStrings(
                &Adapter->DeviceName,
                &deviceNamePrefix,
                &Adapter->NetCfgInstanceId,
                &tapNameSuffix
                );

    if(status == STATUS_SUCCESS)
    {
        NDIS_STRING     linkNamePrefix = NDIS_STRING_CONST("\\DosDevices\\Global\\");

        Adapter->LinkName.Buffer = Adapter->LinkNameBuffer;
        Adapter->LinkName.MaximumLength = sizeof(Adapter->LinkNameBuffer);

        status = tapConcatenateNdisStrings(
                    &Adapter->LinkName,
                    &linkNamePrefix,
                    &Adapter->NetCfgInstanceId,
                    &tapNameSuffix
                    );
    }

    return status;
}

NDIS_STATUS
CreateTapDevice(
    __in PTAP_ADAPTER_CONTEXT   Adapter
   )
{
    NDIS_STATUS                     status;
    NDIS_DEVICE_OBJECT_ATTRIBUTES   deviceAttribute;
    PDRIVER_DISPATCH                dispatchTable[IRP_MJ_MAXIMUM_FUNCTION+1];

    DEBUGP (("[TAP] version [%d.%d] creating tap device: %wZ\n",
        TAP_DRIVER_MAJOR_VERSION,
        TAP_DRIVER_MINOR_VERSION,
        &Adapter->NetCfgInstanceId));

    // Generate DeviceName and LinkName from NetCfgInstanceId.
    status = tapMakeDeviceNames(Adapter);

    if (NT_SUCCESS(status))
    {
        DEBUGP (("[TAP] DeviceName: %wZ\n",&Adapter->DeviceName));
        DEBUGP (("[TAP] LinkName: %wZ\n",&Adapter->LinkName));

        // Initialize dispatch table.
        NdisZeroMemory(dispatchTable, (IRP_MJ_MAXIMUM_FUNCTION+1) * sizeof(PDRIVER_DISPATCH));

        dispatchTable[IRP_MJ_CREATE] = TapDeviceCreate;
        dispatchTable[IRP_MJ_CLEANUP] = TapDeviceCleanup;
        dispatchTable[IRP_MJ_CLOSE] = TapDeviceClose;
        dispatchTable[IRP_MJ_READ] = TapDeviceRead;
        dispatchTable[IRP_MJ_WRITE] = TapDeviceWrite;
        dispatchTable[IRP_MJ_DEVICE_CONTROL] = TapDeviceControl;

        //
        // Create a device object and register dispatch handlers
        //
        NdisZeroMemory(&deviceAttribute, sizeof(NDIS_DEVICE_OBJECT_ATTRIBUTES));

        deviceAttribute.Header.Type = NDIS_OBJECT_TYPE_DEVICE_OBJECT_ATTRIBUTES;
        deviceAttribute.Header.Revision = NDIS_DEVICE_OBJECT_ATTRIBUTES_REVISION_1;
        deviceAttribute.Header.Size = sizeof(NDIS_DEVICE_OBJECT_ATTRIBUTES);

        deviceAttribute.DeviceName = &Adapter->DeviceName;
        deviceAttribute.SymbolicName = &Adapter->LinkName;
        deviceAttribute.MajorFunctions = &dispatchTable[0];
        //deviceAttribute.ExtensionSize = sizeof(FILTER_DEVICE_EXTENSION);

#if ENABLE_NONADMIN
        if(Adapter->AllowNonAdmin)
        {
            //
            // SDDL_DEVOBJ_SYS_ALL_WORLD_RWX_RES_RWX allows the kernel and system complete
            // control over the device. By default the admin can access the entire device,
            // but cannot change the ACL (the admin must take control of the device first)
            //
            // Everyone else, including "restricted" or "untrusted" code can read or write
            // to the device. Traversal beneath the device is also granted (removing it
            // would only effect storage devices, except if the "bypass-traversal"
            // privilege was revoked).
            //
            deviceAttribute.DefaultSDDLString = &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX;
        }
#endif

        status = NdisRegisterDeviceEx(
                    Adapter->MiniportAdapterHandle,
                    &deviceAttribute,
                    &Adapter->DeviceObject,
                    &Adapter->DeviceHandle
                    );
    }

    ASSERT(NT_SUCCESS(status));

    if (NT_SUCCESS(status))
    {
        // Set TAP device flags.
        (Adapter->DeviceObject)->Flags &= ~DO_BUFFERED_IO;
        (Adapter->DeviceObject)->Flags |= DO_DIRECT_IO;;

      //========================
      // Finalize initialization
      //========================

      Adapter->TapDeviceCreated = TRUE;

      DEBUGP (("[%wZ] successfully created TAP device [%wZ]\n",
	        &Adapter->NetCfgInstanceId,
            &Adapter->DeviceName
            ));
    }

    DEBUGP (("[TAP] <-- CreateTapDevice; status = %8.8X\n",status));

    return status;
}

//
// DestroyTapDevice is called from AdapterHalt and NDIS miniport
// is in Halted state. Prior to entering the Halted state the
// miniport would have passed through the Pausing and Paused
// states. These miniport states have responsibility for waiting
// until NDIS network operations have completed.
//
VOID
DestroyTapDevice(
    __in PTAP_ADAPTER_CONTEXT   Adapter
   )
{
    DEBUGP (("[TAP] --> DestroyTapDevice; Adapter: %wZ\n",
        &Adapter->NetCfgInstanceId));

    //
    // Let clients know we are shutting down
    //
    Adapter->TapDeviceCreated = FALSE;

    //
    // Flush pending send TAP packet queue.
    //
    tapFlushSendPacketQueue(Adapter);

    ASSERT(Adapter->SendPacketQueue.Count == 0);

    //
    // Flush IRP queues. Wait for pending I/O. Etc.
    // --------------------------------------------
    // Exhaust IRP and packet queues. Any pending IRPs will
    // be cancelled, causing user-space to get this error
    // on overlapped reads:
    //
    //   ERROR_OPERATION_ABORTED, code=995
    //
    //   "The I/O operation has been aborted because of either a
    //   thread exit or an application request."
    //
    // It's important that user-space close the device handle
    // when this code is returned, so that when we finally
    // do a NdisMDeregisterDeviceEx, the device reference count
    // is 0.  Otherwise the driver will not unload even if the
    // the last adapter has been halted.
    //
    // The act of flushing the queues at this point should result in the user-mode
    // application closing the adapter's device handle. Closing the handle will
    // result in the TapDeviceCleanup call being made, followed by the a call to
    // the TapDeviceClose callback.
    //
    tapFlushIrpQueues(Adapter);

    ASSERT(Adapter->PendingReadIrpQueue.Count == 0);

    //
    // Deregister the Win32 device.
    // ----------------------------
    // When a driver calls NdisDeregisterDeviceEx, the I/O manager deletes the
    // target device object if there are no outstanding references to it. However,
    // if any outstanding references remain, the I/O manager marks the device
    // object as "delete pending" and deletes the device object when the references
    // are finally released.
    //
    if(Adapter->DeviceHandle)
    {
        DEBUGP (("[TAP] Calling NdisDeregisterDeviceEx\n"));
        NdisDeregisterDeviceEx(Adapter->DeviceHandle);
    }

    Adapter->DeviceHandle = NULL;

    DEBUGP (("[TAP] <-- DestroyTapDevice\n"));
}

