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

//======================================================
// This driver is designed to work on Windows Vista or higher
// versions of Windows.
//
// It is SMP-safe and handles power management.
//
// By default we operate as a "tap" virtual ethernet
// 802.3 interface, but we can emulate a "tun"
// interface (point-to-point IPv4) through the
// TAP_WIN_IOCTL_CONFIG_POINT_TO_POINT or
// TAP_WIN_IOCTL_CONFIG_TUN ioctl.
//======================================================

//
// Include files.
//

#include <string.h>

#include "tap.h"


// Global data
TAP_GLOBAL      GlobalData;


#ifdef ALLOC_PRAGMA
#pragma alloc_text( INIT, DriverEntry )
#pragma alloc_text( PAGE, TapDriverUnload)
#endif // ALLOC_PRAGMA

NTSTATUS
DriverEntry(
    __in PDRIVER_OBJECT   DriverObject,
    __in PUNICODE_STRING  RegistryPath
    )
/*++
Routine Description:

    In the context of its DriverEntry function, a miniport driver associates
    itself with NDIS, specifies the NDIS version that it is using, and
    registers its entry points.


Arguments:
    PVOID DriverObject - pointer to the driver object.
    PVOID RegistryPath - pointer to the driver registry path.

    Return Value:

    NTSTATUS code

--*/
{
    NTSTATUS                                status;

    UNREFERENCED_PARAMETER(RegistryPath);

    DEBUGP (("[TAP] --> DriverEntry; version [%d.%d] %s %s\n",
        TAP_DRIVER_MAJOR_VERSION,
        TAP_DRIVER_MINOR_VERSION,
        __DATE__,
        __TIME__));

    DEBUGP (("[TAP] Registry Path: '%wZ'\n", RegistryPath));

    //
    // Initialize any driver-global variables here.
    //
    NdisZeroMemory(&GlobalData, sizeof(GlobalData));

    //
    // The ApaterList in the GlobalData structure is used to track multiple
    // adapters controlled by this miniport.
    //
    NdisInitializeListHead(&GlobalData.AdapterList);

    //
    // This lock protects the AdapterList.
    //
    NdisInitializeReadWriteLock(&GlobalData.Lock);

    do
    {
        NDIS_MINIPORT_DRIVER_CHARACTERISTICS    miniportCharacteristics;

        NdisZeroMemory(&miniportCharacteristics, sizeof(miniportCharacteristics));

        {C_ASSERT(sizeof(miniportCharacteristics) >= NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2);}
        miniportCharacteristics.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
        miniportCharacteristics.Header.Size = NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;
        miniportCharacteristics.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_2;

        miniportCharacteristics.MajorNdisVersion = TAP_NDIS_MAJOR_VERSION;
        miniportCharacteristics.MinorNdisVersion = TAP_NDIS_MINOR_VERSION;

        miniportCharacteristics.MajorDriverVersion = TAP_DRIVER_MAJOR_VERSION;
        miniportCharacteristics.MinorDriverVersion = TAP_DRIVER_MINOR_VERSION;

        miniportCharacteristics.Flags = 0;

        //miniportCharacteristics.SetOptionsHandler = MPSetOptions; // Optional
        miniportCharacteristics.InitializeHandlerEx = AdapterCreate;
        miniportCharacteristics.HaltHandlerEx = AdapterHalt;
        miniportCharacteristics.UnloadHandler = TapDriverUnload;
        miniportCharacteristics.PauseHandler = AdapterPause;
        miniportCharacteristics.RestartHandler = AdapterRestart;
        miniportCharacteristics.OidRequestHandler = AdapterOidRequest;
        miniportCharacteristics.SendNetBufferListsHandler = AdapterSendNetBufferLists;
        miniportCharacteristics.ReturnNetBufferListsHandler = AdapterReturnNetBufferLists;
        miniportCharacteristics.CancelSendHandler = AdapterCancelSend;
        miniportCharacteristics.CheckForHangHandlerEx = AdapterCheckForHangEx;
        miniportCharacteristics.ResetHandlerEx = AdapterReset;
        miniportCharacteristics.DevicePnPEventNotifyHandler = AdapterDevicePnpEventNotify;
        miniportCharacteristics.ShutdownHandlerEx = AdapterShutdownEx;
        miniportCharacteristics.CancelOidRequestHandler = AdapterCancelOidRequest;

        //
        // Associate the miniport driver with NDIS by calling the
        // NdisMRegisterMiniportDriver. This function returns an NdisDriverHandle.
        // The miniport driver must retain this handle but it should never attempt
        // to access or interpret this handle.
        //
        // By calling NdisMRegisterMiniportDriver, the driver indicates that it
        // is ready for NDIS to call the driver's MiniportSetOptions and
        // MiniportInitializeEx handlers.
        //
        DEBUGP (("[TAP] Calling NdisMRegisterMiniportDriver...\n"));
        //NDIS_DECLARE_MINIPORT_DRIVER_CONTEXT(TAP_GLOBAL);
        status = NdisMRegisterMiniportDriver(
                    DriverObject,
                    RegistryPath,
                    &GlobalData,
                    &miniportCharacteristics,
                    &GlobalData.NdisDriverHandle
                    );

        if (NDIS_STATUS_SUCCESS == status)
        {
            DEBUGP (("[TAP] Registered miniport successfully\n"));
        }
        else
        {
            DEBUGP(("[TAP] NdisMRegisterMiniportDriver failed: %8.8X\n", status));
            TapDriverUnload(DriverObject);
            status = NDIS_STATUS_FAILURE;
            break;
        }
    } while(FALSE);

    DEBUGP (("[TAP] <-- DriverEntry; status = %8.8X\n",status));

    return status;
}

VOID
TapDriverUnload(
    __in PDRIVER_OBJECT DriverObject
    )
/*++

Routine Description:

    The unload handler is called during driver unload to free up resources
    acquired in DriverEntry. This handler is registered in DriverEntry through
    NdisMRegisterMiniportDriver. Note that an unload handler differs from
    a MiniportHalt function in that this unload handler releases resources that
    are global to the driver, while the halt handler releases resource for a
    particular adapter.

    Runs at IRQL = PASSIVE_LEVEL.

Arguments:

    DriverObject        Not used

Return Value:

    None.

--*/
{
    PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
    UNICODE_STRING uniWin32NameString;

    DEBUGP (("[TAP] --> TapDriverUnload; version [%d.%d] %s %s unloaded\n",
        TAP_DRIVER_MAJOR_VERSION,
        TAP_DRIVER_MINOR_VERSION,
        __DATE__,
        __TIME__
        ));

    PAGED_CODE();

    //
    // Clean up all globals that were allocated in DriverEntry
    //

    ASSERT(IsListEmpty(&GlobalData.AdapterList));

    if(GlobalData.NdisDriverHandle != NULL )
    {
        NdisMDeregisterMiniportDriver(GlobalData.NdisDriverHandle);
    }

    DEBUGP (("[TAP] <-- TapDriverUnload\n"));
}

