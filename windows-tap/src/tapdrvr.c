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

//======================================================
// This driver is designed to work on Win 2000 or higher
// versions of Windows.
//
// It is SMP-safe and handles NDIS 5 power management.
//
// By default we operate as a "tap" virtual ethernet
// 802.3 interface, but we can emulate a "tun"
// interface (point-to-point IPv4) through the
// TAP_WIN_IOCTL_CONFIG_POINT_TO_POINT or
// TAP_WIN_IOCTL_CONFIG_TUN ioctl.
//======================================================

#include "tap-windows.h"
#include "config.h"

#define NDIS_MINIPORT_DRIVER
#define BINARY_COMPATIBLE 0
#define NDIS50_MINIPORT 1
#define NDIS_WDM 0
#define NDIS50 1
#define NTSTRSAFE_LIB

// Debug info output
#define ALSO_DBGPRINT           1
#define DEBUGP_AT_DISPATCH      0

//========================================================
// Check for truncated IPv4 packets, log errors if found.
//========================================================
#define PACKET_TRUNCATION_CHECK 0

//========================================================
// EXPERIMENTAL -- Configure TAP device object to be
// accessible from non-administrative accounts, based
// on an advanced properties setting.
//
// Duplicates the functionality of OpenVPN's
// --allow-nonadmin directive.
//========================================================
#define ENABLE_NONADMIN 1

#if defined(DDKVER_MAJOR) && DDKVER_MAJOR < 5600
#include <ndis.h>
#include <ntstrsafe.h>
#include <ntddk.h>
#else
#include <ntifs.h>
#include <ndis.h>
#include <ntstrsafe.h>
#endif

#include "lock.h"
#include "constants.h"
#include "proto.h"
#include "error.h"
#include "endian.h"
#include "dhcp.h"
#include "types.h"
#include "prototypes.h"

#include "mem.c"
#include "macinfo.c"
#include "error.c"
#include "dhcp.c"
#include "instance.c"

#define IS_UP(ta) \
  ((ta)->m_InterfaceIsRunning && (ta)->m_Extension.m_TapIsRunning)

#define INCREMENT_STAT(s) ++(s)

#define NAME_BUFFER_SIZE 80

//========================================================
//                            Globals
//========================================================

NDIS_HANDLE g_NdisWrapperHandle;

const UINT g_SupportedOIDList[] = {
  OID_GEN_HARDWARE_STATUS,
  OID_GEN_MEDIA_SUPPORTED,
  OID_GEN_MEDIA_IN_USE,
  OID_GEN_MAXIMUM_LOOKAHEAD,
  OID_GEN_MAC_OPTIONS,
  OID_GEN_LINK_SPEED,
  OID_GEN_TRANSMIT_BLOCK_SIZE,
  OID_GEN_RECEIVE_BLOCK_SIZE,
  OID_GEN_VENDOR_DESCRIPTION,
  OID_GEN_DRIVER_VERSION,
  OID_GEN_XMIT_OK,
  OID_GEN_RCV_OK,
  OID_GEN_XMIT_ERROR,
  OID_GEN_RCV_ERROR,
  OID_802_3_PERMANENT_ADDRESS,
  OID_802_3_CURRENT_ADDRESS,
  OID_GEN_RCV_NO_BUFFER,
  OID_802_3_RCV_ERROR_ALIGNMENT,
  OID_802_3_XMIT_ONE_COLLISION,
  OID_802_3_XMIT_MORE_COLLISIONS,
  OID_802_3_MULTICAST_LIST,
  OID_802_3_MAXIMUM_LIST_SIZE,
  OID_GEN_VENDOR_ID,
  OID_GEN_CURRENT_LOOKAHEAD,
  OID_GEN_CURRENT_PACKET_FILTER,
  OID_GEN_PROTOCOL_OPTIONS,
  OID_GEN_MAXIMUM_TOTAL_SIZE,
  OID_GEN_TRANSMIT_BUFFER_SPACE,
  OID_GEN_RECEIVE_BUFFER_SPACE,
  OID_GEN_MAXIMUM_FRAME_SIZE,
  OID_GEN_VENDOR_DRIVER_VERSION,
  OID_GEN_MAXIMUM_SEND_PACKETS,
  OID_GEN_MEDIA_CONNECT_STATUS,
  OID_GEN_SUPPORTED_LIST
};

//============================================================
//                         Driver Entry
//============================================================
#pragma NDIS_INIT_FUNCTION (DriverEntry)

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (IN PDRIVER_OBJECT p_DriverObject,
	     IN PUNICODE_STRING p_RegistryPath)
{
  NDIS_STATUS l_Status = NDIS_STATUS_FAILURE;
  NDIS_MINIPORT_CHARACTERISTICS *l_Properties = NULL;

  //========================================================
  // Notify NDIS that a new miniport driver is initializing.
  //========================================================

  NdisMInitializeWrapper (&g_NdisWrapperHandle,
			  p_DriverObject,
			  p_RegistryPath, NULL);

  //======================
  // Global initialization
  //======================

#if DBG
  MyDebugInit (10000); // Allocate debugging text space
#endif

  if (!InitInstanceList ())
    {
      DEBUGP (("[TAP] Allocation failed for adapter instance list\n"));
      goto cleanup;
    }

  //=======================================
  // Set and register miniport entry points
  //=======================================

  l_Properties = MemAlloc (sizeof (NDIS_MINIPORT_CHARACTERISTICS), TRUE);

  if (l_Properties == NULL)
    {
      DEBUGP (("[TAP] Allocation failed for miniport entry points\n"));
      goto cleanup;
    }

  l_Properties->MajorNdisVersion = TAP_NDIS_MAJOR_VERSION;
  l_Properties->MinorNdisVersion = TAP_NDIS_MINOR_VERSION;
  l_Properties->InitializeHandler = AdapterCreate;
  l_Properties->HaltHandler = AdapterHalt;
  l_Properties->ResetHandler = AdapterReset;               /* DISPATCH_LEVEL */
  l_Properties->TransferDataHandler = AdapterReceive;      /* DISPATCH_LEVEL */
  l_Properties->SendHandler = AdapterTransmit;             /* DISPATCH_LEVEL */
  l_Properties->QueryInformationHandler = AdapterQuery;    /* DISPATCH_LEVEL */
  l_Properties->SetInformationHandler = AdapterModify;     /* DISPATCH_LEVEL */

  switch (l_Status =
	  NdisMRegisterMiniport (g_NdisWrapperHandle, l_Properties,
				 sizeof (NDIS_MINIPORT_CHARACTERISTICS)))
    {
    case NDIS_STATUS_SUCCESS:
      {
	DEBUGP (("[TAP] version [%d.%d] %s %s registered miniport successfully\n",
		 TAP_DRIVER_MAJOR_VERSION,
		 TAP_DRIVER_MINOR_VERSION,
		 __DATE__,
		 __TIME__));
	DEBUGP (("Registry Path: '%.*S'\n", p_RegistryPath->Length/2, p_RegistryPath->Buffer));
	break;
      }

    case NDIS_STATUS_BAD_CHARACTERISTICS:
      {
	DEBUGP (("[TAP] Miniport characteristics were badly defined\n"));
	NdisTerminateWrapper (g_NdisWrapperHandle, NULL);
	break;
      }

    case NDIS_STATUS_BAD_VERSION:
      {
	DEBUGP
	  (("[TAP] NDIS Version is wrong for the given characteristics\n"));
	NdisTerminateWrapper (g_NdisWrapperHandle, NULL);
	break;
      }

    case NDIS_STATUS_RESOURCES:
      {
	DEBUGP (("[TAP] Insufficient resources\n"));
	NdisTerminateWrapper (g_NdisWrapperHandle, NULL);
	break;
      }

    default:
    case NDIS_STATUS_FAILURE:
      {
	DEBUGP (("[TAP] Unknown fatal registration error\n"));
	NdisTerminateWrapper (g_NdisWrapperHandle, NULL);
	break;
      }
    }

 cleanup:
  if (l_Properties)
    MemFree (l_Properties, sizeof (NDIS_MINIPORT_CHARACTERISTICS));

  if (l_Status == NDIS_STATUS_SUCCESS)
    NdisMRegisterUnloadHandler (g_NdisWrapperHandle, TapDriverUnload);
  else
    TapDriverUnload (p_DriverObject);

  return l_Status;
}

//============================================================
//                         Driver Unload
//============================================================
DRIVER_UNLOAD TapDriverUnload;
VOID 
TapDriverUnload (IN PDRIVER_OBJECT p_DriverObject)
{
  DEBUGP (("[TAP] version [%d.%d] %s %s unloaded, instances=%d, imbs=%d\n",
	   TAP_DRIVER_MAJOR_VERSION,
	   TAP_DRIVER_MINOR_VERSION,
	   __DATE__,
	   __TIME__,
	   NInstances(),
	   InstanceMaxBucketSize()));

  FreeInstanceList ();

  //==============================
  // Free debugging text space
  //==============================
#if DBG
  MyDebugFree ();
#endif
}

//==========================================================
//                            Adapter Initialization
//==========================================================
NDIS_STATUS AdapterCreate
(OUT PNDIS_STATUS p_ErrorStatus,
 OUT PUINT p_MediaIndex,
 IN PNDIS_MEDIUM p_Media,
 IN UINT p_MediaCount,
 IN NDIS_HANDLE p_AdapterHandle,
 IN NDIS_HANDLE p_ConfigurationHandle)
{
  TapAdapterPointer l_Adapter = NULL;

  NDIS_MEDIUM l_PreferredMedium = NdisMedium802_3; // Ethernet
  BOOLEAN l_MacFromRegistry = FALSE;
  UINT l_Index;
  NDIS_STATUS status;

#if ENABLE_NONADMIN
  BOOLEAN enable_non_admin = FALSE;
#endif

  DEBUGP (("[TAP] AdapterCreate called\n"));

  //====================================
  // Make sure adapter type is supported
  //====================================

  for (l_Index = 0;
       l_Index < p_MediaCount && p_Media[l_Index] != l_PreferredMedium;
       ++l_Index);

  if (l_Index == p_MediaCount)
    {
      DEBUGP (("[TAP] Unsupported adapter type [wanted: %d]\n",
	       l_PreferredMedium));
      return NDIS_STATUS_UNSUPPORTED_MEDIA;
    }

  *p_MediaIndex = l_Index;

  //=========================================
  // Allocate memory for TapAdapter structure
  //=========================================

  l_Adapter = MemAlloc (sizeof (TapAdapter), TRUE);

  if (l_Adapter == NULL)
    {
      DEBUGP (("[TAP] Couldn't allocate adapter memory\n"));
      return NDIS_STATUS_RESOURCES;
    }

  //==========================================
  // Inform the NDIS library about significant
  // features of our virtual NIC.
  //==========================================

  NdisMSetAttributesEx
    (p_AdapterHandle,
     (NDIS_HANDLE) l_Adapter,
     16,
     NDIS_ATTRIBUTE_DESERIALIZE
     | NDIS_ATTRIBUTE_IGNORE_PACKET_TIMEOUT
     | NDIS_ATTRIBUTE_IGNORE_REQUEST_TIMEOUT
     | NDIS_ATTRIBUTE_NO_HALT_ON_SUSPEND,
     NdisInterfaceInternal);

  //=====================================
  // Initialize simple Adapter parameters
  //=====================================

  l_Adapter->m_Lookahead = DEFAULT_PACKET_LOOKAHEAD;
  l_Adapter->m_Medium = l_PreferredMedium;
  l_Adapter->m_DeviceState = '?';
  l_Adapter->m_MiniportAdapterHandle = p_AdapterHandle;

  //==================================
  // Allocate spinlock for controlling
  // access to multicast address list.
  //==================================
  NdisAllocateSpinLock (&l_Adapter->m_MCLock);
  l_Adapter->m_MCLockAllocated = TRUE;

  //====================================================
  // Register a shutdown handler which will be called
  // on system restart/shutdown to halt our virtual NIC.
  //====================================================

  NdisMRegisterAdapterShutdownHandler (p_AdapterHandle, l_Adapter,
				       AdapterHalt);
  l_Adapter->m_RegisteredAdapterShutdownHandler = TRUE;

  //============================================
  // Get parameters from registry which were set
  // in the adapter advanced properties dialog.
  //============================================
  {
    NDIS_STATUS status;
    NDIS_HANDLE configHandle;
    NDIS_CONFIGURATION_PARAMETER *parm;

    // set defaults in case our registry query fails
    l_Adapter->m_MTU = ETHERNET_MTU;
    l_Adapter->m_MediaStateAlwaysConnected = FALSE;
    l_Adapter->m_MediaState = FALSE;

    NdisOpenConfiguration (&status, &configHandle, p_ConfigurationHandle);
    if (status != NDIS_STATUS_SUCCESS)
      {
	DEBUGP (("[TAP] Couldn't open adapter registry\n"));
	AdapterFreeResources (l_Adapter);
	return status;
      }

    //====================================
    // Allocate and construct adapter name
    //====================================
    {
      
      NDIS_STRING mkey = NDIS_STRING_CONST("MiniportName");
      NDIS_STRING vkey = NDIS_STRING_CONST("NdisVersion");
      NDIS_STATUS vstatus;
      NDIS_CONFIGURATION_PARAMETER *vparm;

      NdisReadConfiguration (&vstatus, &vparm, configHandle, &vkey, NdisParameterInteger);
      if (vstatus == NDIS_STATUS_SUCCESS)
	DEBUGP (("[TAP] NdisReadConfiguration NdisVersion=%X\n", vparm->ParameterData.IntegerData));

      NdisReadConfiguration (&status, &parm, configHandle, &mkey, NdisParameterString);
      if (status == NDIS_STATUS_SUCCESS)
	{
	  if (parm->ParameterType == NdisParameterString)
	    {
	      DEBUGP (("[TAP] NdisReadConfiguration (MiniportName=%.*S)\n",
		       parm->ParameterData.StringData.Length/2,
		       parm->ParameterData.StringData.Buffer));

	      if (RtlUnicodeStringToAnsiString (
						&l_Adapter->m_NameAnsi,
						&parm->ParameterData.StringData,
						TRUE) != STATUS_SUCCESS)
		{
		  DEBUGP (("[TAP] MiniportName failed\n"));
		  status = NDIS_STATUS_RESOURCES;
		}
	    }
	}
      else
	{
	  /* "MiniportName" is available only XP and above.  Not on Windows 2000. */
	  if (vstatus == NDIS_STATUS_SUCCESS && vparm->ParameterData.IntegerData == 0x50000)
	    {
	      /* Fallback for Windows 2000 with NDIS version 5.00.00
		 Don't use this on Vista, 'NDIS_MINIPORT_BLOCK' was changed! */
	      if (RtlUnicodeStringToAnsiString (&l_Adapter->m_NameAnsi,
						&((struct WIN2K_NDIS_MINIPORT_BLOCK *) p_AdapterHandle)->MiniportName,
						TRUE) != STATUS_SUCCESS)
		{
		  DEBUGP (("[TAP] MiniportName (W2K) failed\n"));
		  status = NDIS_STATUS_RESOURCES;
		}
	      else
		{
		  DEBUGP (("[TAP] MiniportName (W2K) succeeded: %s\n", l_Adapter->m_NameAnsi.Buffer));
		  status = NDIS_STATUS_SUCCESS;
		}
	    }
	}
    }

    /* Can't continue without name (see macro 'NAME') */
    if (status != NDIS_STATUS_SUCCESS || !l_Adapter->m_NameAnsi.Buffer)
      {
	NdisCloseConfiguration (configHandle);
	AdapterFreeResources (l_Adapter);
	DEBUGP (("[TAP] failed to get miniport name\n"));
	return NDIS_STATUS_RESOURCES;
      }

    /* Read MTU setting from registry */
    {
      NDIS_STRING key = NDIS_STRING_CONST("MTU");
      NdisReadConfiguration (&status, &parm, configHandle,
			     &key, NdisParameterInteger);
      if (status == NDIS_STATUS_SUCCESS)
	{
	  if (parm->ParameterType == NdisParameterInteger)
	    {
	      int mtu = parm->ParameterData.IntegerData;
	      if (mtu < MINIMUM_MTU)
		mtu = MINIMUM_MTU;
	      if (mtu > MAXIMUM_MTU)
		mtu = MAXIMUM_MTU;
	      l_Adapter->m_MTU = mtu;
	    }
	}
    }

    /* Read Media Status setting from registry */
    {
      NDIS_STRING key = NDIS_STRING_CONST("MediaStatus");
      NdisReadConfiguration (&status, &parm, configHandle,
			     &key, NdisParameterInteger);
      if (status == NDIS_STATUS_SUCCESS)
	{
	  if (parm->ParameterType == NdisParameterInteger)
	    {
	      if (parm->ParameterData.IntegerData)
		{
		  l_Adapter->m_MediaStateAlwaysConnected = TRUE;
		  l_Adapter->m_MediaState = TRUE;
		}
	    }
	}
    }

#if ENABLE_NONADMIN
    /* Read AllowNonAdmin setting from registry */
    {
      NDIS_STRING key = NDIS_STRING_CONST("AllowNonAdmin");
      NdisReadConfiguration (&status, &parm, configHandle,
			     &key, NdisParameterInteger);
      if (status == NDIS_STATUS_SUCCESS)
	{
	  if (parm->ParameterType == NdisParameterInteger)
	    {
	      if (parm->ParameterData.IntegerData)
		{
		  enable_non_admin = TRUE;
		}
	    }
	}
    }
#endif

    /* Read optional MAC setting from registry */
    {
      NDIS_STRING key = NDIS_STRING_CONST("MAC");
      ANSI_STRING mac_string;
      NdisReadConfiguration (&status, &parm, configHandle,
			     &key, NdisParameterString);
      if (status == NDIS_STATUS_SUCCESS)
	{
	  if (parm->ParameterType == NdisParameterString)
	    {
	      if (RtlUnicodeStringToAnsiString (&mac_string, &parm->ParameterData.StringData, TRUE) == STATUS_SUCCESS)
		{
		  l_MacFromRegistry = ParseMAC (l_Adapter->m_MAC, mac_string.Buffer);
		  RtlFreeAnsiString (&mac_string);
		}
	    }
	}
    }

    NdisCloseConfiguration (configHandle);

    DEBUGP (("[%s] MTU=%d\n", NAME (l_Adapter), l_Adapter->m_MTU));
  }

  //==================================
  // Store and update MAC address info
  //==================================

  if (!l_MacFromRegistry)
    GenerateRandomMac (l_Adapter->m_MAC, NAME (l_Adapter));

  DEBUGP (("[%s] Using MAC %x:%x:%x:%x:%x:%x\n",
	   NAME (l_Adapter),
	   l_Adapter->m_MAC[0], l_Adapter->m_MAC[1], l_Adapter->m_MAC[2],
	   l_Adapter->m_MAC[3], l_Adapter->m_MAC[4], l_Adapter->m_MAC[5]));

  //==================
  // Set broadcast MAC
  //==================
  {
    int i;
    for (i = 0; i < sizeof (MACADDR); ++i)
      l_Adapter->m_MAC_Broadcast[i] = 0xFF;
  }

  //====================================
  // Initialize TAP device
  //====================================
  {
    NDIS_STATUS tap_status;
    tap_status = CreateTapDevice (&l_Adapter->m_Extension, NAME (l_Adapter));
    if (tap_status != NDIS_STATUS_SUCCESS)
      {
	AdapterFreeResources (l_Adapter);
	DEBUGP (("[TAP] CreateTapDevice failed\n"));
	return tap_status;
      }
  }

  if (!AddAdapterToInstanceList (l_Adapter))
    {
      NOTE_ERROR ();
      TapDeviceFreeResources (&l_Adapter->m_Extension);
      AdapterFreeResources (l_Adapter);
      DEBUGP (("[TAP] AddAdapterToInstanceList failed\n"));
      return NDIS_STATUS_RESOURCES;
    }

  l_Adapter->m_InterfaceIsRunning = TRUE;

#if ENABLE_NONADMIN
  if (enable_non_admin)
    AllowNonAdmin (&l_Adapter->m_Extension);
#endif

  return NDIS_STATUS_SUCCESS;
}

VOID
AdapterHalt (IN NDIS_HANDLE p_AdapterContext)
{
  BOOLEAN status;

  TapAdapterPointer l_Adapter = (TapAdapterPointer) p_AdapterContext;

  NOTE_ERROR ();

  l_Adapter->m_InterfaceIsRunning = FALSE;

  DEBUGP (("[%s] is being halted\n", NAME (l_Adapter)));
  
  DestroyTapDevice (&l_Adapter->m_Extension);

  // Free resources
  DEBUGP (("[%s] Freeing Resources\n", NAME (l_Adapter)));
  AdapterFreeResources (l_Adapter);

  status = RemoveAdapterFromInstanceList (l_Adapter);
  DEBUGP (("[TAP] RemoveAdapterFromInstanceList returned %d\n", (int) status));

  DEBUGP (("[TAP] version [%d.%d] %s %s AdapterHalt returning\n",
	   TAP_DRIVER_MAJOR_VERSION,
	   TAP_DRIVER_MINOR_VERSION,
	   __DATE__,
	   __TIME__));
}

VOID
AdapterFreeResources (TapAdapterPointer p_Adapter)
{
  MYASSERT (!p_Adapter->m_CalledAdapterFreeResources);
  p_Adapter->m_CalledAdapterFreeResources = TRUE;

  if (p_Adapter->m_NameAnsi.Buffer)
    RtlFreeAnsiString (&p_Adapter->m_NameAnsi);
  
  if (p_Adapter->m_RegisteredAdapterShutdownHandler)
    NdisMDeregisterAdapterShutdownHandler (p_Adapter->m_MiniportAdapterHandle);

  if (p_Adapter->m_MCLockAllocated)
    NdisFreeSpinLock (&p_Adapter->m_MCLock);
}

VOID
DestroyTapDevice (TapExtensionPointer p_Extension)
{
  DEBUGP (("[%s] Destroying tap device\n", p_Extension->m_TapName));

  //======================================
  // Let clients know we are shutting down
  //======================================
  p_Extension->m_TapIsRunning = FALSE;
  p_Extension->m_TapOpens = 0;
  p_Extension->m_Halt = TRUE;

  //=====================================
  // If we are concurrently executing in
  // TapDeviceHook or AdapterTransmit,
  // give those calls time to finish.
  // Note that we must be running at IRQL
  // < DISPATCH_LEVEL in order to call
  // NdisMSleep.
  //=====================================
  NdisMSleep (500000);

  //===========================================================
  // Exhaust IRP and packet queues.  Any pending IRPs will
  // be cancelled, causing user-space to get this error
  // on overlapped reads:
  //   The I/O operation has been aborted because of either a
  //   thread exit or an application request.   (code=995)
  // It's important that user-space close the device handle
  // when this code is returned, so that when we finally
  // do a NdisMDeregisterDevice, the device reference count
  // is 0.  Otherwise the driver will not unload even if the
  // the last adapter has been halted.
  //===========================================================
  FlushQueues (p_Extension);
  NdisMSleep (500000); // give user space time to respond to IRP cancel

  TapDeviceFreeResources (p_Extension);
}

VOID
TapDeviceFreeResources (TapExtensionPointer p_Extension)
{
  MYASSERT (p_Extension);
  MYASSERT (!p_Extension->m_CalledTapDeviceFreeResources);
  p_Extension->m_CalledTapDeviceFreeResources = TRUE;

  if (p_Extension->m_PacketQueue)
    QueueFree (p_Extension->m_PacketQueue);
  if (p_Extension->m_IrpQueue)
    QueueFree (p_Extension->m_IrpQueue);
  if (p_Extension->m_InjectQueue)
    QueueFree (p_Extension->m_InjectQueue);

  if (p_Extension->m_CreatedUnicodeLinkName)
    RtlFreeUnicodeString (&p_Extension->m_UnicodeLinkName);

  //==========================================================
  // According to DDK docs, the device is not actually deleted
  // until its reference count falls to zero.  That means we
  // still need to gracefully fail TapDeviceHook requests
  // after this point, otherwise ugly things would happen if
  // the device was disabled (e.g. in the network connections
  // control panel) while a userspace app still held an open
  // file handle to it.
  //==========================================================
  
  if (p_Extension->m_TapDevice)
    {
      BOOLEAN status;
      status = (NdisMDeregisterDevice (p_Extension->m_TapDeviceHandle)
		== NDIS_STATUS_SUCCESS);
      DEBUGP (("[TAP] Deregistering TAP device, status=%d\n", (int)status));
    }

  if (p_Extension->m_TapName)
    MemFree (p_Extension->m_TapName, NAME_BUFFER_SIZE);
  
  if (p_Extension->m_InjectDpcInitialized)
    KeRemoveQueueDpc (&p_Extension->m_InjectDpc);

  if (p_Extension->m_AllocatedSpinlocks)
    {
      NdisFreeSpinLock (&p_Extension->m_QueueLock);
      NdisFreeSpinLock (&p_Extension->m_InjectLock);
    }
}

//========================================================================
//                             Tap Device Initialization
//========================================================================

NDIS_STATUS
CreateTapDevice (TapExtensionPointer p_Extension, const char *p_Name)
{
# define SIZEOF_DISPATCH (sizeof(PDRIVER_DISPATCH) * (IRP_MJ_MAXIMUM_FUNCTION + 1))
  PDRIVER_DISPATCH *l_Dispatch = NULL;
  ANSI_STRING l_TapString, l_LinkString;
  UNICODE_STRING l_TapUnicode;
  BOOLEAN l_FreeTapUnicode = FALSE;
  NTSTATUS l_Status, l_Return = NDIS_STATUS_SUCCESS;
  const char *l_UsableName;

  DEBUGP (("[TAP] version [%d.%d] creating tap device: %s\n",
	   TAP_DRIVER_MAJOR_VERSION,
	   TAP_DRIVER_MINOR_VERSION,
	   p_Name));

  NdisZeroMemory (p_Extension, sizeof (TapExtension));

  INIT_MUTEX (&p_Extension->m_OpenCloseMutex);

  l_LinkString.Buffer = NULL;
  l_TapString.Buffer = NULL;

  l_TapString.MaximumLength = l_LinkString.MaximumLength = NAME_BUFFER_SIZE;

  //=======================================
  // Set TAP device entry points
  //=======================================

  if ((l_Dispatch = MemAlloc (SIZEOF_DISPATCH, TRUE)) == NULL)
    {
      DEBUGP (("[%s] couldn't alloc TAP dispatch table\n", p_Name));
      l_Return = NDIS_STATUS_RESOURCES;
      goto cleanup;
    }

  l_Dispatch[IRP_MJ_DEVICE_CONTROL] = TapDeviceHook;
  l_Dispatch[IRP_MJ_READ] = TapDeviceHook;
  l_Dispatch[IRP_MJ_WRITE] = TapDeviceHook;
  l_Dispatch[IRP_MJ_CREATE] = TapDeviceHook;
  l_Dispatch[IRP_MJ_CLOSE] = TapDeviceHook;

  //==================================
  // Find the beginning of the GUID
  //==================================
  l_UsableName = p_Name;
  while (*l_UsableName != '{')
    {
      if (*l_UsableName == '\0')
	{
	  DEBUGP (("[%s] couldn't find leading '{' in name\n", p_Name));
	  l_Return = NDIS_STATUS_RESOURCES;
	  goto cleanup;
	}
      ++l_UsableName;
    }

  //==================================
  // Allocate pool for TAP device name
  //==================================

  if ((p_Extension->m_TapName = l_TapString.Buffer =
       MemAlloc (NAME_BUFFER_SIZE, TRUE)) == NULL)
    {
      DEBUGP (("[%s] couldn't alloc TAP name buffer\n", p_Name));
      l_Return = NDIS_STATUS_RESOURCES;
      goto cleanup;
    }

  //================================================
  // Allocate pool for TAP symbolic link name buffer
  //================================================

  if ((l_LinkString.Buffer =
       MemAlloc (NAME_BUFFER_SIZE, TRUE)) == NULL)
    {
      DEBUGP (("[%s] couldn't alloc TAP symbolic link name buffer\n",
	       p_Name));
      l_Return = NDIS_STATUS_RESOURCES;
      goto cleanup;
    }

  //=======================================================
  // Set TAP device name
  //=======================================================

  l_Status = RtlStringCchPrintfExA
    (l_TapString.Buffer,
     l_TapString.MaximumLength,
     NULL,
     NULL,
     STRSAFE_FILL_BEHIND_NULL | STRSAFE_IGNORE_NULLS,
     "%s%s%s",
     SYSDEVICEDIR,
     l_UsableName,
     TAP_WIN_SUFFIX);

  if (l_Status != STATUS_SUCCESS)
    {
      DEBUGP (("[%s] couldn't format TAP device name\n",
	       p_Name));
      l_Return = NDIS_STATUS_RESOURCES;
      goto cleanup;
    }
  l_TapString.Length = (USHORT) strlen (l_TapString.Buffer);

  DEBUGP (("TAP DEV NAME: '%s'\n", l_TapString.Buffer));

  //=======================================================
  // Set TAP link name
  //=======================================================

  l_Status = RtlStringCchPrintfExA
    (l_LinkString.Buffer,
     l_LinkString.MaximumLength,
     NULL,
     NULL,
     STRSAFE_FILL_BEHIND_NULL | STRSAFE_IGNORE_NULLS,
     "%s%s%s",
     USERDEVICEDIR,
     l_UsableName,
     TAP_WIN_SUFFIX);

  if (l_Status != STATUS_SUCCESS)
    {
      DEBUGP (("[%s] couldn't format TAP device symbolic link\n",
	       p_Name));
      l_Return = NDIS_STATUS_RESOURCES;
      goto cleanup;
    }
  l_LinkString.Length = (USHORT) strlen (l_LinkString.Buffer);

  DEBUGP (("TAP LINK NAME: '%s'\n", l_LinkString.Buffer));

  //==================================================
  // Convert strings to unicode
  //==================================================
  if (RtlAnsiStringToUnicodeString (&l_TapUnicode, &l_TapString, TRUE) !=
      STATUS_SUCCESS)
    {
      DEBUGP (("[%s] couldn't alloc TAP unicode name buffer\n",
		p_Name));
      l_Return = NDIS_STATUS_RESOURCES;
      goto cleanup;
    }
  l_FreeTapUnicode = TRUE;

  if (RtlAnsiStringToUnicodeString
      (&p_Extension->m_UnicodeLinkName, &l_LinkString, TRUE)
      != STATUS_SUCCESS)
    {
      DEBUGP
	(("[%s] Couldn't allocate unicode string for symbolic link name\n",
	 p_Name));
      l_Return = NDIS_STATUS_RESOURCES;
      goto cleanup;
    }
  p_Extension->m_CreatedUnicodeLinkName = TRUE;

  //==================================================
  // Create new TAP device with symbolic
  // link and associate with adapter.
  //==================================================

  l_Status = NdisMRegisterDevice
    (g_NdisWrapperHandle,
     &l_TapUnicode,
     &p_Extension->m_UnicodeLinkName,
     l_Dispatch,
     &p_Extension->m_TapDevice,
     &p_Extension->m_TapDeviceHandle
     );

  if (l_Status != STATUS_SUCCESS)
    {
      DEBUGP (("[%s] couldn't be created\n", p_Name));
      l_Return = NDIS_STATUS_RESOURCES;
      goto cleanup;
    }

  /* Set TAP device flags */
  p_Extension->m_TapDevice->Flags |= DO_DIRECT_IO;

  //========================================================
  // Initialize Packet and IRP queues.
  //
  // The packet queue is used to buffer data which has been
  // "transmitted" by the virtual NIC, before user space
  // has had a chance to read it.
  //
  // The IRP queue is used to buffer pending I/O requests
  // from userspace, i.e. read requests on the TAP device
  // waiting for the system to "transmit" something through
  // the virtual NIC.
  //
  // Basically, packets in the packet queue are used
  // to satisfy IRP requests in the IRP queue.
  //
  // QueueLock is used to lock the packet queue used
  // for the TAP-Windows NIC -> User Space packet flow direction.
  //
  // All accesses to packet or IRP queues should be
  // bracketed by the QueueLock spinlock,
  // in order to be SMP-safe.
  //========================================================

  NdisAllocateSpinLock (&p_Extension->m_QueueLock);
  NdisAllocateSpinLock (&p_Extension->m_InjectLock);
  p_Extension->m_AllocatedSpinlocks = TRUE;

  p_Extension->m_PacketQueue = QueueInit (PACKET_QUEUE_SIZE);
  p_Extension->m_IrpQueue = QueueInit (IRP_QUEUE_SIZE);
  p_Extension->m_InjectQueue = QueueInit (INJECT_QUEUE_SIZE);
  if (!p_Extension->m_PacketQueue
      || !p_Extension->m_IrpQueue
      || !p_Extension->m_InjectQueue)
    {
      DEBUGP (("[%s] couldn't alloc TAP queues\n", p_Name));
      l_Return = NDIS_STATUS_RESOURCES;
      goto cleanup;
    }

  //=================================================================
  // Initialize deferred procedure call for DHCP/ARP packet injection
  //=================================================================

  KeInitializeDpc (&p_Extension->m_InjectDpc, InjectPacketDpc, NULL);
  p_Extension->m_InjectDpcInitialized = TRUE;

  //========================
  // Finalize initialization
  //========================

  p_Extension->m_TapIsRunning = TRUE;

  DEBUGP (("[%s] successfully created TAP device [%s]\n", p_Name,
	    p_Extension->m_TapName));

 cleanup:
  if (l_FreeTapUnicode)
    RtlFreeUnicodeString (&l_TapUnicode);
  if (l_LinkString.Buffer)
    MemFree (l_LinkString.Buffer, NAME_BUFFER_SIZE);
  if (l_Dispatch)
    MemFree (l_Dispatch, SIZEOF_DISPATCH);

  if (l_Return != NDIS_STATUS_SUCCESS)
    TapDeviceFreeResources (p_Extension);

  return l_Return;
}
#undef SIZEOF_DISPATCH

//========================================================
//                      Adapter Control
//========================================================
NDIS_STATUS
AdapterReset (OUT PBOOLEAN p_AddressingReset, IN NDIS_HANDLE p_AdapterContext)
{
  TapAdapterPointer l_Adapter = (TapAdapterPointer) p_AdapterContext;
  DEBUGP (("[%s] is resetting\n", NAME (l_Adapter)));
  return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS AdapterReceive
  (OUT PNDIS_PACKET p_Packet,
   OUT PUINT p_Transferred,
   IN NDIS_HANDLE p_AdapterContext,
   IN NDIS_HANDLE p_ReceiveContext,
   IN UINT p_Offset,
   IN UINT p_ToTransfer)
{
  return NDIS_STATUS_SUCCESS;
}

//==============================================================
//                  Adapter Option Query/Modification
//==============================================================
NDIS_STATUS AdapterQuery
(IN NDIS_HANDLE p_AdapterContext,
 IN NDIS_OID p_OID,
 IN PVOID p_Buffer,
 IN ULONG p_BufferLength,
 OUT PULONG p_BytesWritten, OUT PULONG p_BytesNeeded)
{
  TapAdapterPointer l_Adapter = (TapAdapterPointer) p_AdapterContext;
  TapAdapterQuery l_Query, *l_QueryPtr = &l_Query;
  NDIS_STATUS l_Status = NDIS_STATUS_SUCCESS;
  UINT l_QueryLength = 4;
  BOOLEAN lock_succeeded;

  NdisZeroMemory (&l_Query, sizeof (l_Query));

  switch (p_OID)
    {
      //===================================================================
      //                       Vendor & Driver version Info
      //===================================================================
    case OID_GEN_VENDOR_DESCRIPTION:
      l_QueryPtr = (TapAdapterQueryPointer) PRODUCT_TAP_WIN_DEVICE_DESCRIPTION;
      l_QueryLength = strlen (PRODUCT_TAP_WIN_DEVICE_DESCRIPTION) + 1;
      break;

    case OID_GEN_VENDOR_ID:
      l_Query.m_Long = 0xffffff;
      break;

    case OID_GEN_DRIVER_VERSION:
      l_Query.m_Short =
	(((USHORT) TAP_NDIS_MAJOR_VERSION) << 8 | (USHORT)
	 TAP_NDIS_MINOR_VERSION);
      l_QueryLength = sizeof (unsigned short);
      break;

    case OID_GEN_VENDOR_DRIVER_VERSION:
      l_Query.m_Long =
	(((USHORT) PRODUCT_TAP_WIN_MAJOR) << 8 | (USHORT)
	 PRODUCT_TAP_WIN_MINOR);
      break;

      //=================================================================
      //                             Statistics
      //=================================================================
    case OID_GEN_RCV_NO_BUFFER:
      l_Query.m_Long = 0;
      break;

    case OID_802_3_RCV_ERROR_ALIGNMENT:
      l_Query.m_Long = 0;
      break;

    case OID_802_3_XMIT_ONE_COLLISION:
      l_Query.m_Long = 0;
      break;

    case OID_802_3_XMIT_MORE_COLLISIONS:
      l_Query.m_Long = 0;
      break;

    case OID_GEN_XMIT_OK:
      l_Query.m_Long = l_Adapter->m_Tx;
      break;

    case OID_GEN_RCV_OK:
      l_Query.m_Long = l_Adapter->m_Rx;
      break;

    case OID_GEN_XMIT_ERROR:
      l_Query.m_Long = l_Adapter->m_TxErr;
      break;

    case OID_GEN_RCV_ERROR:
      l_Query.m_Long = l_Adapter->m_RxErr;
      break;

      //===================================================================
      //                       Device & Protocol Options
      //===================================================================
    case OID_GEN_SUPPORTED_LIST:
      l_QueryPtr = (TapAdapterQueryPointer) g_SupportedOIDList;
      l_QueryLength = sizeof (g_SupportedOIDList);
      break;

    case OID_GEN_MAC_OPTIONS:
      // This MUST be here !!!
      l_Query.m_Long = (NDIS_MAC_OPTION_RECEIVE_SERIALIZED
			| NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA
			| NDIS_MAC_OPTION_NO_LOOPBACK
			| NDIS_MAC_OPTION_TRANSFERS_NOT_PEND);

      break;

    case OID_GEN_CURRENT_PACKET_FILTER:
      l_Query.m_Long =
	(NDIS_PACKET_TYPE_ALL_LOCAL |
	 NDIS_PACKET_TYPE_BROADCAST |
	 NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_ALL_FUNCTIONAL);

      break;

    case OID_GEN_PROTOCOL_OPTIONS:
      l_Query.m_Long = 0;
      break;

      //==================================================================
      //                            Device Info
      //==================================================================
    case OID_GEN_MEDIA_CONNECT_STATUS:
      l_Query.m_Long = l_Adapter->m_MediaState
	? NdisMediaStateConnected : NdisMediaStateDisconnected;
      break;

    case OID_GEN_HARDWARE_STATUS:
      l_Query.m_HardwareStatus = NdisHardwareStatusReady;
      l_QueryLength = sizeof (NDIS_HARDWARE_STATUS);
      break;

    case OID_GEN_MEDIA_SUPPORTED:
    case OID_GEN_MEDIA_IN_USE:
      l_Query.m_Medium = l_Adapter->m_Medium;
      l_QueryLength = sizeof (NDIS_MEDIUM);
      break;

    case OID_GEN_PHYSICAL_MEDIUM:
      l_Query.m_PhysicalMedium = NdisPhysicalMediumUnspecified;
      l_QueryLength = sizeof (NDIS_PHYSICAL_MEDIUM);
      break;
      
    case OID_GEN_LINK_SPEED:
      l_Query.m_Long = 100000; // rate / 100 bps
      break;

    case OID_802_3_PERMANENT_ADDRESS:
    case OID_802_3_CURRENT_ADDRESS:
      COPY_MAC (l_Query.m_MacAddress, l_Adapter->m_MAC);
      l_QueryLength = sizeof (MACADDR);
      break;

      //==================================================================
      //                             Limits
      //==================================================================

    case OID_GEN_MAXIMUM_SEND_PACKETS:
      l_Query.m_Long = 1;
      break;

    case OID_802_3_MAXIMUM_LIST_SIZE:
      l_Query.m_Long = NIC_MAX_MCAST_LIST;
      break;

    case OID_GEN_CURRENT_LOOKAHEAD:
      l_Query.m_Long = l_Adapter->m_Lookahead;
      break;

    case OID_GEN_MAXIMUM_LOOKAHEAD:
    case OID_GEN_MAXIMUM_TOTAL_SIZE:
    case OID_GEN_RECEIVE_BUFFER_SPACE:
    case OID_GEN_RECEIVE_BLOCK_SIZE:
      l_Query.m_Long = DEFAULT_PACKET_LOOKAHEAD;
      break;

    case OID_GEN_MAXIMUM_FRAME_SIZE:
    case OID_GEN_TRANSMIT_BLOCK_SIZE:
    case OID_GEN_TRANSMIT_BUFFER_SPACE:
      l_Query.m_Long = l_Adapter->m_MTU;
      break;

    case OID_PNP_CAPABILITIES:
      do
	{
	  PNDIS_PNP_CAPABILITIES pPNPCapabilities;
	  PNDIS_PM_WAKE_UP_CAPABILITIES pPMstruct;

	  if (p_BufferLength >= sizeof (NDIS_PNP_CAPABILITIES))
	    {
	      pPNPCapabilities = (PNDIS_PNP_CAPABILITIES) (p_Buffer);

	      //
	      // Setting up the buffer to be returned
	      // to the Protocol above the Passthru miniport
	      //
	      pPMstruct = &pPNPCapabilities->WakeUpCapabilities;
	      pPMstruct->MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
	      pPMstruct->MinPatternWakeUp = NdisDeviceStateUnspecified;
	      pPMstruct->MinLinkChangeWakeUp = NdisDeviceStateUnspecified;
	    }
	  l_QueryLength = sizeof (NDIS_PNP_CAPABILITIES);
	}
      while (FALSE);
      break;
    case OID_PNP_QUERY_POWER:
      break;

      // Required OIDs that we don't support

    case OID_GEN_SUPPORTED_GUIDS:
    case OID_GEN_MEDIA_CAPABILITIES:
    case OID_TCP_TASK_OFFLOAD:
    case OID_FFP_SUPPORT:
      l_Status = NDIS_STATUS_INVALID_OID;
      break;

      // Optional stats OIDs

    case OID_GEN_DIRECTED_BYTES_XMIT:
    case OID_GEN_DIRECTED_FRAMES_XMIT:
    case OID_GEN_MULTICAST_BYTES_XMIT:
    case OID_GEN_MULTICAST_FRAMES_XMIT:
    case OID_GEN_BROADCAST_BYTES_XMIT:
    case OID_GEN_BROADCAST_FRAMES_XMIT:
    case OID_GEN_DIRECTED_BYTES_RCV:
    case OID_GEN_DIRECTED_FRAMES_RCV:
    case OID_GEN_MULTICAST_BYTES_RCV:
    case OID_GEN_MULTICAST_FRAMES_RCV:
    case OID_GEN_BROADCAST_BYTES_RCV:
    case OID_GEN_BROADCAST_FRAMES_RCV:
      l_Status = NDIS_STATUS_INVALID_OID;
      break;

      //===================================================================
      //                          Not Handled
      //===================================================================
    default:
      DEBUGP (("[%s] Unhandled OID %lx\n", NAME (l_Adapter), p_OID));
      l_Status = NDIS_STATUS_INVALID_OID;
      break;
    }

  if (l_Status != NDIS_STATUS_SUCCESS)
    ;
  else if (l_QueryLength > p_BufferLength)
    {
      l_Status = NDIS_STATUS_INVALID_LENGTH;
      *p_BytesNeeded = l_QueryLength;
    }
  else
    NdisMoveMemory (p_Buffer, (PVOID) l_QueryPtr,
		    (*p_BytesWritten = l_QueryLength));

  return l_Status;
}

NDIS_STATUS AdapterModify
(IN NDIS_HANDLE p_AdapterContext,
 IN NDIS_OID p_OID,
 IN PVOID p_Buffer,
 IN ULONG p_BufferLength,
 OUT PULONG p_BytesRead,
 OUT PULONG p_BytesNeeded)
{
  TapAdapterQueryPointer l_Query = (TapAdapterQueryPointer) p_Buffer;
  TapAdapterPointer l_Adapter = (TapAdapterPointer) p_AdapterContext;
  NDIS_STATUS l_Status = NDIS_STATUS_INVALID_OID;
  ULONG l_Long;

  switch (p_OID)
    {
      //==================================================================
      //                            Device Info
      //==================================================================
    case OID_802_3_MULTICAST_LIST:
      DEBUGP (("[%s] Setting [OID_802_3_MULTICAST_LIST]\n",
	       NAME (l_Adapter)));

      *p_BytesNeeded = sizeof (ETH_ADDR);
      *p_BytesRead = p_BufferLength;

      if (p_BufferLength % sizeof (ETH_ADDR))
	l_Status = NDIS_STATUS_INVALID_LENGTH;
      else if (p_BufferLength > sizeof (MC_LIST))
	{
	  l_Status = NDIS_STATUS_MULTICAST_FULL;
	  *p_BytesNeeded = sizeof (MC_LIST);
	}
      else
	{
	  NdisAcquireSpinLock (&l_Adapter->m_MCLock);

	  NdisZeroMemory(&l_Adapter->m_MCList, sizeof (MC_LIST));
        
	  NdisMoveMemory(&l_Adapter->m_MCList,
			 p_Buffer,
			 p_BufferLength);

	  l_Adapter->m_MCListSize = p_BufferLength / sizeof (ETH_ADDR);
        
	  NdisReleaseSpinLock (&l_Adapter->m_MCLock);

	  l_Status = NDIS_STATUS_SUCCESS;
	}
      break;

    case OID_GEN_CURRENT_PACKET_FILTER:
      l_Status = NDIS_STATUS_INVALID_LENGTH;
      *p_BytesNeeded = 4;

      if (p_BufferLength >= sizeof (ULONG))
	{
	  DEBUGP
	    (("[%s] Setting [OID_GEN_CURRENT_PACKET_FILTER] to [0x%02lx]\n",
	      NAME (l_Adapter), l_Query->m_Long));
	  l_Status = NDIS_STATUS_SUCCESS;
	  *p_BytesRead = sizeof (ULONG);
	}
      break;

    case OID_GEN_CURRENT_LOOKAHEAD:
      if (p_BufferLength < sizeof (ULONG))
	{
	  l_Status = NDIS_STATUS_INVALID_LENGTH;
	  *p_BytesNeeded = 4;
	}
      else if (l_Query->m_Long > DEFAULT_PACKET_LOOKAHEAD
	       || l_Query->m_Long <= 0)
	{
	  l_Status = NDIS_STATUS_INVALID_DATA;
	}
      else
	{
	  DEBUGP (("[%s] Setting [OID_GEN_CURRENT_LOOKAHEAD] to [%d]\n",
		   NAME (l_Adapter), l_Query->m_Long));
	  l_Adapter->m_Lookahead = l_Query->m_Long;
	  l_Status = NDIS_STATUS_SUCCESS;
	  *p_BytesRead = sizeof (ULONG);
	}
      break;

    case OID_GEN_NETWORK_LAYER_ADDRESSES:
      l_Status = NDIS_STATUS_SUCCESS;
      *p_BytesRead = *p_BytesNeeded = 0;
      break;

    case OID_GEN_TRANSPORT_HEADER_OFFSET:
      l_Status = NDIS_STATUS_SUCCESS;
      *p_BytesRead = *p_BytesNeeded = 0;
      break;

    case OID_PNP_SET_POWER:
      do
	{
	  NDIS_DEVICE_POWER_STATE NewDeviceState;

	  NewDeviceState = (*(PNDIS_DEVICE_POWER_STATE) p_Buffer);

	  switch (NewDeviceState)
	    {
	    case NdisDeviceStateD0:
	      l_Adapter->m_DeviceState = '0';
	      break;
	    case NdisDeviceStateD1:
	      l_Adapter->m_DeviceState = '1';
	      break;
	    case NdisDeviceStateD2:
	      l_Adapter->m_DeviceState = '2';
	      break;
	    case NdisDeviceStateD3:
	      l_Adapter->m_DeviceState = '3';
	      break;
	    default:
	      l_Adapter->m_DeviceState = '?';
	      break;
	    }

	  l_Status = NDIS_STATUS_FAILURE;

	  //
	  // Check for invalid length
	  //
	  if (p_BufferLength < sizeof (NDIS_DEVICE_POWER_STATE))
	    {
	      l_Status = NDIS_STATUS_INVALID_LENGTH;
	      break;
	    }

	  if (NewDeviceState > NdisDeviceStateD0)
	    {
	      l_Adapter->m_InterfaceIsRunning = FALSE;
	      DEBUGP (("[%s] Power management device state OFF\n",
		       NAME (l_Adapter)));
	    }
	  else
	    {
	      l_Adapter->m_InterfaceIsRunning = TRUE;
	      DEBUGP (("[%s] Power management device state ON\n",
		       NAME (l_Adapter)));
	    }

	  l_Status = NDIS_STATUS_SUCCESS;
	}
      while (FALSE);

      if (l_Status == NDIS_STATUS_SUCCESS)
	{
	  *p_BytesRead = sizeof (NDIS_DEVICE_POWER_STATE);
	  *p_BytesNeeded = 0;
	}
      else
	{
	  *p_BytesRead = 0;
	  *p_BytesNeeded = sizeof (NDIS_DEVICE_POWER_STATE);
	}
      break;

    case OID_PNP_REMOVE_WAKE_UP_PATTERN:
    case OID_PNP_ADD_WAKE_UP_PATTERN:
      l_Status = NDIS_STATUS_SUCCESS;
      *p_BytesRead = *p_BytesNeeded = 0;
      break;

    default:
      DEBUGP (("[%s] Can't set value for OID %lx\n", NAME (l_Adapter),
	       p_OID));
      l_Status = NDIS_STATUS_INVALID_OID;
      *p_BytesRead = *p_BytesNeeded = 0;
      break;
    }

  return l_Status;
}

// checksum code for ICMPv6 packet, taken from dhcp.c / udp_checksum
// see RFC 4443, 2.3, and RFC 2460, 8.1
USHORT
icmpv6_checksum (const UCHAR *buf,
	         const int len_icmpv6,
	         const UCHAR *saddr6,
	         const UCHAR *daddr6)
{
  USHORT word16;
  ULONG sum = 0;
  int i;

  // make 16 bit words out of every two adjacent 8 bit words and
  // calculate the sum of all 16 bit words
  for (i = 0; i < len_icmpv6; i += 2){
    word16 = ((buf[i] << 8) & 0xFF00) + ((i + 1 < len_icmpv6) ? (buf[i+1] & 0xFF) : 0);
    sum += word16;
  }

  // add the IPv6 pseudo header which contains the IP source and destination addresses
  for (i = 0; i < 16; i += 2){
    word16 =((saddr6[i] << 8) & 0xFF00) + (saddr6[i+1] & 0xFF);
    sum += word16;
  }
  for (i = 0; i < 16; i += 2){
    word16 =((daddr6[i] << 8) & 0xFF00) + (daddr6[i+1] & 0xFF);
    sum += word16;
  }

  // the next-header number and the length of the ICMPv6 packet
  sum += (USHORT) IPPROTO_ICMPV6 + (USHORT) len_icmpv6;

  // keep only the last 16 bits of the 32 bit calculated sum and add the carries
  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);

  // Take the one's complement of sum
  return ((USHORT) ~sum);
}

// check IPv6 packet for "is this an IPv6 Neighbor Solicitation that
// the tap driver needs to answer?"
// see RFC 4861 4.3 for the different cases
static IPV6ADDR IPV6_NS_TARGET_MCAST =
	{ 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x08 };
static IPV6ADDR IPV6_NS_TARGET_UNICAST =
	{ 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08 };

BOOLEAN
HandleIPv6NeighborDiscovery( TapAdapterPointer p_Adapter, UCHAR * m_Data )
{
    const ETH_HEADER * e = (ETH_HEADER *) m_Data;
    const IPV6HDR *ipv6 = (IPV6HDR *) (m_Data + sizeof (ETH_HEADER));
    const ICMPV6_NS * icmpv6_ns = (ICMPV6_NS *) (m_Data + sizeof (ETH_HEADER) + sizeof (IPV6HDR));
    ICMPV6_NA_PKT *na;
    USHORT icmpv6_len, icmpv6_csum;

    // we don't really care about the destination MAC address here
    // - it's either a multicast MAC, or the userland destination MAC
    // but since the TAP driver is point-to-point, all packets are "for us"

    // IPv6 target address must be ff02::1::ff00:8 (multicast for
    // initial NS) or fe80::1 (unicast for recurrent NUD)
    if ( memcmp( ipv6->daddr, IPV6_NS_TARGET_MCAST,
		 sizeof(IPV6ADDR) ) != 0 &&
         memcmp( ipv6->daddr, IPV6_NS_TARGET_UNICAST,
		 sizeof(IPV6ADDR) ) != 0 )
    {
	return FALSE;				// wrong target address
    }

    // IPv6 Next-Header must be ICMPv6
    if ( ipv6->nexthdr != IPPROTO_ICMPV6 )
    {
	return FALSE;				// wrong next-header
    }

    // ICMPv6 type+code must be 135/0 for NS
    if ( icmpv6_ns->type != ICMPV6_TYPE_NS ||
	 icmpv6_ns->code != ICMPV6_CODE_0 )
    {
	return FALSE;				// wrong ICMPv6 type
    }

    // ICMPv6 target address must be fe80::8 (magic)
    if ( memcmp( icmpv6_ns->target_addr, IPV6_NS_TARGET_UNICAST,
	         sizeof(IPV6ADDR) ) != 0 )
    {
	return FALSE;				// not for us
    }

    // packet identified, build magic response packet

    na = (ICMPV6_NA_PKT *) MemAlloc (sizeof (ICMPV6_NA_PKT), TRUE);
    if ( !na ) return FALSE;

    //------------------------------------------------
    // Initialize Neighbour Advertisement reply packet
    //------------------------------------------------

    // ethernet header
    na->eth.proto = htons(ETH_P_IPV6);
    COPY_MAC(na->eth.dest, p_Adapter->m_MAC);
    COPY_MAC(na->eth.src, p_Adapter->m_TapToUser.dest);

    // IPv6 header
    na->ipv6.version_prio = ipv6->version_prio;
    NdisMoveMemory( na->ipv6.flow_lbl, ipv6->flow_lbl,
		    sizeof(na->ipv6.flow_lbl) );
    icmpv6_len = sizeof(ICMPV6_NA_PKT) - sizeof(ETH_HEADER) - sizeof(IPV6HDR);
    na->ipv6.payload_len = htons(icmpv6_len);
    na->ipv6.nexthdr = IPPROTO_ICMPV6;
    na->ipv6.hop_limit = 255;
    NdisMoveMemory( na->ipv6.saddr, IPV6_NS_TARGET_UNICAST,
		    sizeof(IPV6ADDR) );
    NdisMoveMemory( na->ipv6.daddr, ipv6->saddr,
		    sizeof(IPV6ADDR) );

    // ICMPv6
    na->icmpv6.type = ICMPV6_TYPE_NA;
    na->icmpv6.code = ICMPV6_CODE_0;
    na->icmpv6.checksum = 0;
    na->icmpv6.rso_bits = 0x60;		// Solicited + Override
    NdisZeroMemory( na->icmpv6.reserved, sizeof(na->icmpv6.reserved) );
    NdisMoveMemory( na->icmpv6.target_addr, IPV6_NS_TARGET_UNICAST,
		    sizeof(IPV6ADDR) );

    // ICMPv6 option "Target Link Layer Address"
    na->icmpv6.opt_type = ICMPV6_OPTION_TLLA;
    na->icmpv6.opt_length = ICMPV6_LENGTH_TLLA;
    COPY_MAC( na->icmpv6.target_macaddr, p_Adapter->m_TapToUser.dest );

    // calculate and set checksum
    icmpv6_csum = icmpv6_checksum ( (UCHAR*) &(na->icmpv6),
				    icmpv6_len,
				    na->ipv6.saddr,
				    na->ipv6.daddr );
    na->icmpv6.checksum = htons( icmpv6_csum );

    DUMP_PACKET ("HandleIPv6NeighborDiscovery",
		 (unsigned char *) na,
		 sizeof (ICMPV6_NA_PKT));

    InjectPacketDeferred (p_Adapter, (UCHAR *) na, sizeof (ICMPV6_NA_PKT));

    MemFree (na, sizeof (ICMPV6_NA_PKT));

    return TRUE;				// all fine
}

//====================================================================
//                               Adapter Transmission
//====================================================================
NDIS_STATUS
AdapterTransmit (IN NDIS_HANDLE p_AdapterContext,
		 IN PNDIS_PACKET p_Packet,
		 IN UINT p_Flags)
{
  TapAdapterPointer l_Adapter = (TapAdapterPointer) p_AdapterContext;
  ULONG l_Index = 0, l_PacketLength = 0;
  UINT l_BufferLength = 0;
  PIRP l_IRP;
  TapPacketPointer l_PacketBuffer;
  PNDIS_BUFFER l_NDIS_Buffer;
  PUCHAR l_Buffer;
  PVOID result;

  NdisQueryPacket (p_Packet, NULL, NULL, &l_NDIS_Buffer, &l_PacketLength);

  //====================================================
  // Here we abandon the transmission attempt if any of
  // the parameters is wrong or memory allocation fails
  // but we do not indicate failure. The packet is
  // silently dropped.
  //====================================================

  if (l_PacketLength < ETHERNET_HEADER_SIZE || l_PacketLength > 65535)
    goto exit_fail;
  else if (!l_Adapter->m_Extension.m_TapOpens || !l_Adapter->m_MediaState)
    goto exit_success;              // Nothing is bound to the TAP device

  if (NdisAllocateMemoryWithTag (&l_PacketBuffer,
				 TAP_PACKET_SIZE (l_PacketLength),
				 '5PAT') != NDIS_STATUS_SUCCESS)
    goto exit_no_resources;

  if (l_PacketBuffer == NULL)
    goto exit_no_resources;

  l_PacketBuffer->m_SizeFlags = (l_PacketLength & TP_SIZE_MASK);

  //===========================
  // Reassemble packet contents
  //===========================

  __try
  {
    l_Index = 0;
    while (l_NDIS_Buffer && l_Index < l_PacketLength)
      {
	ULONG newlen;
	NdisQueryBuffer (l_NDIS_Buffer, (PVOID *) & l_Buffer,
			 &l_BufferLength);
	newlen = l_Index + l_BufferLength;
	if (newlen > l_PacketLength)
	  {
	    NOTE_ERROR ();
	    goto no_queue; /* overflow */
	  }
	NdisMoveMemory (l_PacketBuffer->m_Data + l_Index, l_Buffer,
			l_BufferLength);
	l_Index = newlen;
	NdisGetNextBuffer (l_NDIS_Buffer, &l_NDIS_Buffer);
      }
    if (l_Index != l_PacketLength)
      {
	NOTE_ERROR ();
	goto no_queue; /* underflow */
      }

    DUMP_PACKET ("AdapterTransmit", l_PacketBuffer->m_Data, l_PacketLength);

    //=====================================================
    // If IPv4 packet, check whether or not packet
    // was truncated.
    //=====================================================
#if PACKET_TRUNCATION_CHECK
    IPv4PacketSizeVerify (l_PacketBuffer->m_Data, l_PacketLength, FALSE, "TX", &l_Adapter->m_TxTrunc);
#endif

    //=====================================================
    // Are we running in DHCP server masquerade mode?
    //
    // If so, catch both DHCP requests and ARP queries
    // to resolve the address of our virtual DHCP server.
    //=====================================================
    if (l_Adapter->m_dhcp_enabled)
      {
	const ETH_HEADER *eth = (ETH_HEADER *) l_PacketBuffer->m_Data;
	const IPHDR *ip = (IPHDR *) (l_PacketBuffer->m_Data + sizeof (ETH_HEADER));
	const UDPHDR *udp = (UDPHDR *) (l_PacketBuffer->m_Data + sizeof (ETH_HEADER) + sizeof (IPHDR));

	// ARP packet?
	if (l_PacketLength == sizeof (ARP_PACKET)
	    && eth->proto == htons (ETH_P_ARP)
	    && l_Adapter->m_dhcp_server_arp)
	  {
	    if (ProcessARP (l_Adapter,
			    (PARP_PACKET) l_PacketBuffer->m_Data,
			    l_Adapter->m_dhcp_addr,
			    l_Adapter->m_dhcp_server_ip,
			    ~0,
			    l_Adapter->m_dhcp_server_mac))
	      goto no_queue;
	  }

	// DHCP packet?
	else if (l_PacketLength >= sizeof (ETH_HEADER) + sizeof (IPHDR) + sizeof (UDPHDR) + sizeof (DHCP)
		 && eth->proto == htons (ETH_P_IP)
		 && ip->version_len == 0x45 // IPv4, 20 byte header
		 && ip->protocol == IPPROTO_UDP
		 && udp->dest == htons (BOOTPS_PORT))
	  {
	    const DHCP *dhcp = (DHCP *) (l_PacketBuffer->m_Data
					 + sizeof (ETH_HEADER)
					 + sizeof (IPHDR)
					 + sizeof (UDPHDR));

	    const int optlen = l_PacketLength
	      - sizeof (ETH_HEADER)
	      - sizeof (IPHDR)
	      - sizeof (UDPHDR)
	      - sizeof (DHCP);

	    if (optlen > 0) // we must have at least one DHCP option
	      {
		if (ProcessDHCP (l_Adapter, eth, ip, udp, dhcp, optlen))
		  goto no_queue;
	      }
	    else
	      goto no_queue;
	  }
      }

    //===============================================
    // In Point-To-Point mode, check to see whether
    // packet is ARP (handled) or IPv4 (sent to app).
    // IPv6 packets are inspected for neighbour discovery
    // (to be handled locally), and the rest is forwarded
    // all other protocols are dropped
    //===============================================
    if (l_Adapter->m_tun)
      {
	ETH_HEADER *e;

	if (l_PacketLength < ETHERNET_HEADER_SIZE)
	  goto no_queue;

	e = (ETH_HEADER *) l_PacketBuffer->m_Data;

	switch (ntohs (e->proto))
	  {
	  case ETH_P_ARP:

	    // Make sure that packet is the
	    // right size for ARP.
	    if (l_PacketLength != sizeof (ARP_PACKET))
	      goto no_queue;

	    ProcessARP (l_Adapter,
			(PARP_PACKET) l_PacketBuffer->m_Data,
			l_Adapter->m_localIP,
			l_Adapter->m_remoteNetwork,
			l_Adapter->m_remoteNetmask,
			l_Adapter->m_TapToUser.dest);

	  default:
	    goto no_queue;

	  case ETH_P_IP:

	    // Make sure that packet is large
	    // enough to be IPv4.
	    if (l_PacketLength
		< ETHERNET_HEADER_SIZE + IP_HEADER_SIZE)
	      goto no_queue;

	    // Only accept directed packets,
	    // not broadcasts.
	    if (memcmp (e, &l_Adapter->m_TapToUser, ETHERNET_HEADER_SIZE))
	      goto no_queue;

	    // Packet looks like IPv4, queue it.
	    l_PacketBuffer->m_SizeFlags |= TP_TUN;
	    break;

	  case ETH_P_IPV6:
	    // make sure that packet is large
	    // enough to be IPv6
	    if (l_PacketLength
		< ETHERNET_HEADER_SIZE + IPV6_HEADER_SIZE)
	      goto no_queue;

	    // broadcasts and multicasts are handled specially
	    // (to be implemented)

	    // neighbor discovery packets to fe80::8 are special
	    // OpenVPN sets this next-hop to signal "handled by tapdrv"
	    if ( HandleIPv6NeighborDiscovery( l_Adapter,
					      l_PacketBuffer->m_Data ))
	      {
		goto no_queue;
	      }

	    // Packet looks like IPv6, queue it :-)
	    l_PacketBuffer->m_SizeFlags |= TP_TUN;
	  }
      }

    //===============================================
    // Push packet onto queue to wait for read from
    // userspace.
    //===============================================

    NdisAcquireSpinLock (&l_Adapter->m_Extension.m_QueueLock);

    result = NULL;
    if (IS_UP (l_Adapter))
      result = QueuePush (l_Adapter->m_Extension.m_PacketQueue, l_PacketBuffer);

    NdisReleaseSpinLock (&l_Adapter->m_Extension.m_QueueLock);

    if ((TapPacketPointer) result != l_PacketBuffer)
      {
	// adapter receive overrun
	INCREMENT_STAT (l_Adapter->m_TxErr);
	goto no_queue;
      }
    else
      {
	INCREMENT_STAT (l_Adapter->m_Tx);
      }

    //============================================================
    // Cycle through IRPs and packets, try to satisfy each pending
    // IRP with a queued packet.
    //============================================================
    while (TRUE)
      {
	l_IRP = NULL;
	l_PacketBuffer = NULL;

	NdisAcquireSpinLock (&l_Adapter->m_Extension.m_QueueLock);

	if (IS_UP (l_Adapter)
	    && QueueCount (l_Adapter->m_Extension.m_PacketQueue)
	    && QueueCount (l_Adapter->m_Extension.m_IrpQueue))
	  {
	    l_IRP = (PIRP) QueuePop (l_Adapter->m_Extension.m_IrpQueue);
	    l_PacketBuffer = (TapPacketPointer)
	      QueuePop (l_Adapter->m_Extension.m_PacketQueue);
	  }

	NdisReleaseSpinLock (&l_Adapter->m_Extension.m_QueueLock);

	MYASSERT ((l_IRP != NULL) + (l_PacketBuffer != NULL) != 1);

	if (l_IRP && l_PacketBuffer)
	  {
	    CompleteIRP (l_IRP,
			 l_PacketBuffer, 
			 IO_NETWORK_INCREMENT);
	  }
	else
	  break;
      }
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

  return NDIS_STATUS_SUCCESS;

 no_queue:
  NdisFreeMemory (l_PacketBuffer,
		  TAP_PACKET_SIZE (l_PacketLength),
		  0);
  
 exit_success:
  return NDIS_STATUS_SUCCESS;
    
 exit_fail:
  return NDIS_STATUS_FAILURE;

 exit_no_resources:
  return NDIS_STATUS_RESOURCES;
}

//======================================================================
// Hooks for catching TAP device IRP's.
//======================================================================

DRIVER_DISPATCH TapDeviceHook;
NTSTATUS
TapDeviceHook (IN PDEVICE_OBJECT p_DeviceObject, IN PIRP p_IRP)
{
  TapAdapterPointer l_Adapter = LookupAdapterInInstanceList (p_DeviceObject);
  PIO_STACK_LOCATION l_IrpSp;
  NTSTATUS l_Status = STATUS_SUCCESS;
  BOOLEAN accessible;

  l_IrpSp = IoGetCurrentIrpStackLocation (p_IRP);

  p_IRP->IoStatus.Status = STATUS_SUCCESS;
  p_IRP->IoStatus.Information = 0;

  if (!l_Adapter || l_Adapter->m_Extension.m_Halt)
    {
      DEBUGP (("TapDeviceHook called when TAP device is halted, MajorFunction=%d\n",
	       (int)l_IrpSp->MajorFunction));

      if (l_IrpSp->MajorFunction == IRP_MJ_CLOSE)
	{
	  IoCompleteRequest (p_IRP, IO_NO_INCREMENT);
	  return STATUS_SUCCESS;
	}
      else
	{
	  p_IRP->IoStatus.Status = STATUS_NO_SUCH_DEVICE;
	  IoCompleteRequest (p_IRP, IO_NO_INCREMENT);
	  return STATUS_NO_SUCH_DEVICE;
	}
    }

  switch (l_IrpSp->MajorFunction)
    {
      //===========================================================
      //                 Ioctl call handlers
      //===========================================================
    case IRP_MJ_DEVICE_CONTROL:
      {
	switch (l_IrpSp->Parameters.DeviceIoControl.IoControlCode)
	  {
	  case TAP_WIN_IOCTL_GET_MAC:
	    {
	      if (l_IrpSp->Parameters.DeviceIoControl.OutputBufferLength
		  >= sizeof (MACADDR))
		{
		  COPY_MAC (p_IRP->AssociatedIrp.SystemBuffer,
			    l_Adapter->m_MAC);
		  p_IRP->IoStatus.Information = sizeof (MACADDR);
		}
	      else
		{
		  NOTE_ERROR ();
		  p_IRP->IoStatus.Status = l_Status = STATUS_BUFFER_TOO_SMALL;
		}
	      break;
	    }
	  case TAP_WIN_IOCTL_GET_VERSION:
	    {
	      const ULONG size = sizeof (ULONG) * 3;
	      if (l_IrpSp->Parameters.DeviceIoControl.OutputBufferLength
		  >= size)
		{
		  ((PULONG) (p_IRP->AssociatedIrp.SystemBuffer))[0]
		    = TAP_DRIVER_MAJOR_VERSION;
		  ((PULONG) (p_IRP->AssociatedIrp.SystemBuffer))[1]
		    = TAP_DRIVER_MINOR_VERSION;
		  ((PULONG) (p_IRP->AssociatedIrp.SystemBuffer))[2]
#if DBG
		    = 1;
#else
		  = 0;
#endif
		  p_IRP->IoStatus.Information = size;
		}
	      else
		{
		  NOTE_ERROR ();
		  p_IRP->IoStatus.Status = l_Status = STATUS_BUFFER_TOO_SMALL;
		}

	      break;
	    }
	  case TAP_WIN_IOCTL_GET_MTU:
	    {
	      const ULONG size = sizeof (ULONG) * 1;
	      if (l_IrpSp->Parameters.DeviceIoControl.OutputBufferLength
		  >= size)
		{
		  ((PULONG) (p_IRP->AssociatedIrp.SystemBuffer))[0]
		    = l_Adapter->m_MTU;
		  p_IRP->IoStatus.Information = size;
		}
	      else
		{
		  NOTE_ERROR ();
		  p_IRP->IoStatus.Status = l_Status = STATUS_BUFFER_TOO_SMALL;
		}

	      break;
	    }
	  case TAP_WIN_IOCTL_GET_INFO:
	    {
	      char state[16];
	      if (l_Adapter->m_InterfaceIsRunning)
		state[0] = 'A';
	      else
		state[0] = 'a';
	      if (l_Adapter->m_Extension.m_TapIsRunning)
		state[1] = 'T';
	      else
		state[1] = 't';
	      state[2] = l_Adapter->m_DeviceState;
	      if (l_Adapter->m_MediaStateAlwaysConnected)
		state[3] = 'C';
	      else
		state[3] = 'c';
	      state[4] = '\0';

	      p_IRP->IoStatus.Status = l_Status = RtlStringCchPrintfExA (
	        ((LPTSTR) (p_IRP->AssociatedIrp.SystemBuffer)),
		l_IrpSp->Parameters.DeviceIoControl.OutputBufferLength,
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
		(int)l_Adapter->m_Extension.m_NumTapOpens,
		(int)l_Adapter->m_Tx,
		(int)l_Adapter->m_TxErr,
#if PACKET_TRUNCATION_CHECK
		(int)l_Adapter->m_TxTrunc,
#endif
		(int)l_Adapter->m_Rx,
		(int)l_Adapter->m_RxErr,
#if PACKET_TRUNCATION_CHECK
		(int)l_Adapter->m_RxTrunc,
#endif
		(int)l_Adapter->m_Extension.m_IrpQueue->size,
		(int)l_Adapter->m_Extension.m_IrpQueue->max_size,
		(int)IRP_QUEUE_SIZE,
		(int)l_Adapter->m_Extension.m_PacketQueue->size,
		(int)l_Adapter->m_Extension.m_PacketQueue->max_size,
		(int)PACKET_QUEUE_SIZE,
		(int)l_Adapter->m_Extension.m_InjectQueue->size,
		(int)l_Adapter->m_Extension.m_InjectQueue->max_size,
		(int)INJECT_QUEUE_SIZE
		);

	      p_IRP->IoStatus.Information
		= l_IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

	      break;
	    }

#if DBG
	  case TAP_WIN_IOCTL_GET_LOG_LINE:
	    {
	      if (GetDebugLine ((LPTSTR)p_IRP->AssociatedIrp.SystemBuffer,
				l_IrpSp->Parameters.DeviceIoControl.OutputBufferLength))
		p_IRP->IoStatus.Status = l_Status = STATUS_SUCCESS;
	      else
		p_IRP->IoStatus.Status = l_Status = STATUS_UNSUCCESSFUL;

	      p_IRP->IoStatus.Information
		= l_IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

	      break;
	    }
#endif

	  case TAP_WIN_IOCTL_CONFIG_TUN:
	    {
	      if (l_IrpSp->Parameters.DeviceIoControl.InputBufferLength >=
		  (sizeof (IPADDR) * 3))
		{
		  MACADDR dest;

		  l_Adapter->m_tun = FALSE;

		  GenerateRelatedMAC (dest, l_Adapter->m_MAC, 1);

		  l_Adapter->m_localIP =       ((IPADDR*) (p_IRP->AssociatedIrp.SystemBuffer))[0];
		  l_Adapter->m_remoteNetwork = ((IPADDR*) (p_IRP->AssociatedIrp.SystemBuffer))[1];
		  l_Adapter->m_remoteNetmask = ((IPADDR*) (p_IRP->AssociatedIrp.SystemBuffer))[2];

		  // sanity check on network/netmask
		  if ((l_Adapter->m_remoteNetwork & l_Adapter->m_remoteNetmask) != l_Adapter->m_remoteNetwork)
		    {
		      NOTE_ERROR ();
		      p_IRP->IoStatus.Status = l_Status = STATUS_INVALID_PARAMETER;
		      break;
		    }

		  COPY_MAC (l_Adapter->m_TapToUser.src, l_Adapter->m_MAC);
		  COPY_MAC (l_Adapter->m_TapToUser.dest, dest);
		  COPY_MAC (l_Adapter->m_UserToTap.src, dest);
		  COPY_MAC (l_Adapter->m_UserToTap.dest, l_Adapter->m_MAC);

		  l_Adapter->m_TapToUser.proto = l_Adapter->m_UserToTap.proto = htons (ETH_P_IP);
		  l_Adapter->m_UserToTap_IPv6 = l_Adapter->m_UserToTap;
		  l_Adapter->m_UserToTap_IPv6.proto = htons(ETH_P_IPV6);

		  l_Adapter->m_tun = TRUE;

		  CheckIfDhcpAndTunMode (l_Adapter);

		  p_IRP->IoStatus.Information = 1; // Simple boolean value
		}
	      else
		{
		  NOTE_ERROR ();
		  p_IRP->IoStatus.Status = l_Status = STATUS_INVALID_PARAMETER;
		}
	      
	      break;
	    }

	  case TAP_WIN_IOCTL_CONFIG_POINT_TO_POINT: // Obsoleted by TAP_WIN_IOCTL_CONFIG_TUN
	    {
	      if (l_IrpSp->Parameters.DeviceIoControl.InputBufferLength >=
		  (sizeof (IPADDR) * 2))
		{
		  MACADDR dest;

		  l_Adapter->m_tun = FALSE;

		  GenerateRelatedMAC (dest, l_Adapter->m_MAC, 1);

		  l_Adapter->m_localIP =       ((IPADDR*) (p_IRP->AssociatedIrp.SystemBuffer))[0];
		  l_Adapter->m_remoteNetwork = ((IPADDR*) (p_IRP->AssociatedIrp.SystemBuffer))[1];
		  l_Adapter->m_remoteNetmask = ~0;

		  COPY_MAC (l_Adapter->m_TapToUser.src, l_Adapter->m_MAC);
		  COPY_MAC (l_Adapter->m_TapToUser.dest, dest);
		  COPY_MAC (l_Adapter->m_UserToTap.src, dest);
		  COPY_MAC (l_Adapter->m_UserToTap.dest, l_Adapter->m_MAC);

		  l_Adapter->m_TapToUser.proto = l_Adapter->m_UserToTap.proto = htons (ETH_P_IP);
		  l_Adapter->m_UserToTap_IPv6 = l_Adapter->m_UserToTap;
		  l_Adapter->m_UserToTap_IPv6.proto = htons(ETH_P_IPV6);

		  l_Adapter->m_tun = TRUE;

		  CheckIfDhcpAndTunMode (l_Adapter);

		  p_IRP->IoStatus.Information = 1; // Simple boolean value
		}
	      else
		{
		  NOTE_ERROR ();
		  p_IRP->IoStatus.Status = l_Status = STATUS_INVALID_PARAMETER;
		}
	      
	      break;
	    }

	  case TAP_WIN_IOCTL_SET_MEDIA_STATUS:
	    {
	      if (l_IrpSp->Parameters.DeviceIoControl.InputBufferLength >=
		  (sizeof (ULONG) * 1))
		{
		  ULONG parm = ((PULONG) (p_IRP->AssociatedIrp.SystemBuffer))[0];
		  SetMediaStatus (l_Adapter, (BOOLEAN) parm);
		  p_IRP->IoStatus.Information = 1;
		}
	      else
		{
		  NOTE_ERROR ();
		  p_IRP->IoStatus.Status = l_Status = STATUS_INVALID_PARAMETER;
		}
	      break;
	    }

	  case TAP_WIN_IOCTL_CONFIG_DHCP_MASQ:
	    {
	      if (l_IrpSp->Parameters.DeviceIoControl.InputBufferLength >=
		  (sizeof (IPADDR) * 4))
		{
		  l_Adapter->m_dhcp_enabled = FALSE;
		  l_Adapter->m_dhcp_server_arp = FALSE;
		  l_Adapter->m_dhcp_user_supplied_options_buffer_len = 0;

		  // Adapter IP addr / netmask
		  l_Adapter->m_dhcp_addr =
		    ((IPADDR*) (p_IRP->AssociatedIrp.SystemBuffer))[0];
		  l_Adapter->m_dhcp_netmask =
		    ((IPADDR*) (p_IRP->AssociatedIrp.SystemBuffer))[1];

		  // IP addr of DHCP masq server
		  l_Adapter->m_dhcp_server_ip =
		    ((IPADDR*) (p_IRP->AssociatedIrp.SystemBuffer))[2];

		  // Lease time in seconds
		  l_Adapter->m_dhcp_lease_time =
		    ((IPADDR*) (p_IRP->AssociatedIrp.SystemBuffer))[3];

		  GenerateRelatedMAC (l_Adapter->m_dhcp_server_mac, l_Adapter->m_MAC, 2);

		  l_Adapter->m_dhcp_enabled = TRUE;
		  l_Adapter->m_dhcp_server_arp = TRUE;

		  CheckIfDhcpAndTunMode (l_Adapter);

		  p_IRP->IoStatus.Information = 1; // Simple boolean value
		}
	      else
		{
		  NOTE_ERROR ();
		  p_IRP->IoStatus.Status = l_Status = STATUS_INVALID_PARAMETER;
		}
	      
	      break;
	    }

	  case TAP_WIN_IOCTL_CONFIG_DHCP_SET_OPT:
	    {
	      if (l_IrpSp->Parameters.DeviceIoControl.InputBufferLength <=
		  DHCP_USER_SUPPLIED_OPTIONS_BUFFER_SIZE
		  && l_Adapter->m_dhcp_enabled)
		{
		  l_Adapter->m_dhcp_user_supplied_options_buffer_len = 0;

		  NdisMoveMemory (l_Adapter->m_dhcp_user_supplied_options_buffer,
				  p_IRP->AssociatedIrp.SystemBuffer,
				  l_IrpSp->Parameters.DeviceIoControl.InputBufferLength);
		  
		  l_Adapter->m_dhcp_user_supplied_options_buffer_len = 
		    l_IrpSp->Parameters.DeviceIoControl.InputBufferLength;

		  p_IRP->IoStatus.Information = 1; // Simple boolean value
		}
	      else
		{
		  NOTE_ERROR ();
		  p_IRP->IoStatus.Status = l_Status = STATUS_INVALID_PARAMETER;
		}
	      
	      break;
	    }

	  default:
	    {
	      NOTE_ERROR ();
	      p_IRP->IoStatus.Status = l_Status = STATUS_INVALID_PARAMETER;
	      break;
	    }
	  }

	IoCompleteRequest (p_IRP, IO_NO_INCREMENT);
	break;
      }

      //===========================================================
      // User mode thread issued a read request on the tap device
      // If there are packets waiting to be read, then the request
      // will be satisfied here. If not, then the request will be
      // queued and satisfied by any packet that is not used to
      // satisfy requests ahead of it.
      //===========================================================
    case IRP_MJ_READ:
      {
	TapPacketPointer l_PacketBuffer;
	BOOLEAN pending = FALSE;

	// Save IRP-accessible copy of buffer length
	p_IRP->IoStatus.Information = l_IrpSp->Parameters.Read.Length;

	if (p_IRP->MdlAddress == NULL)
	  {
	    DEBUGP (("[%s] MdlAddress is NULL for IRP_MJ_READ\n",
		     NAME (l_Adapter)));
	    NOTE_ERROR ();
	    p_IRP->IoStatus.Status = l_Status = STATUS_INVALID_PARAMETER;
	    p_IRP->IoStatus.Information = 0;
	    IoCompleteRequest (p_IRP, IO_NO_INCREMENT);
	    break;
	  }
	else if ((p_IRP->AssociatedIrp.SystemBuffer =
		  MmGetSystemAddressForMdlSafe
		  (p_IRP->MdlAddress, NormalPagePriority)) == NULL)
	  {
	    DEBUGP (("[%s] Could not map address in IRP_MJ_READ\n",
		     NAME (l_Adapter)));
	    NOTE_ERROR ();
	    p_IRP->IoStatus.Status = l_Status = STATUS_INSUFFICIENT_RESOURCES;
	    p_IRP->IoStatus.Information = 0;
	    IoCompleteRequest (p_IRP, IO_NO_INCREMENT);
	    break;
	  }
	else if (!l_Adapter->m_InterfaceIsRunning)
	  {
	    DEBUGP (("[%s] Interface is down in IRP_MJ_READ\n",
		     NAME (l_Adapter)));
	    NOTE_ERROR ();
	    p_IRP->IoStatus.Status = l_Status = STATUS_UNSUCCESSFUL;
	    p_IRP->IoStatus.Information = 0;
	    IoCompleteRequest (p_IRP, IO_NO_INCREMENT);
	    break;
	  }

	//==================================
	// Can we provide immediate service?
	//==================================

	l_PacketBuffer = NULL;

	NdisAcquireSpinLock (&l_Adapter->m_Extension.m_QueueLock);

	if (IS_UP (l_Adapter)
	    && QueueCount (l_Adapter->m_Extension.m_PacketQueue)
	    && QueueCount (l_Adapter->m_Extension.m_IrpQueue) == 0)
	  {
	    l_PacketBuffer = (TapPacketPointer)
	      QueuePop (l_Adapter->m_Extension.m_PacketQueue);
	  }

	NdisReleaseSpinLock (&l_Adapter->m_Extension.m_QueueLock);

	if (l_PacketBuffer)
	  {
	    l_Status = CompleteIRP (p_IRP,
				    l_PacketBuffer,
				    IO_NO_INCREMENT);
	    break;
	  }

	//=============================
	// Attempt to pend read request
	//=============================

	NdisAcquireSpinLock (&l_Adapter->m_Extension.m_QueueLock);

	if (IS_UP (l_Adapter)
	    && QueuePush (l_Adapter->m_Extension.m_IrpQueue, p_IRP) == (PIRP) p_IRP)
	  {
	    IoSetCancelRoutine (p_IRP, CancelIRPCallback);
	    l_Status = STATUS_PENDING;
	    IoMarkIrpPending (p_IRP);
	    pending = TRUE;
	  }

	NdisReleaseSpinLock (&l_Adapter->m_Extension.m_QueueLock);

	if (pending)
	  break;

	// Can't queue anymore IRP's
	DEBUGP (("[%s] TAP [%s] read IRP overrun\n",
		 NAME (l_Adapter), l_Adapter->m_Extension.m_TapName));
	NOTE_ERROR ();
	p_IRP->IoStatus.Status = l_Status = STATUS_UNSUCCESSFUL;
	p_IRP->IoStatus.Information = 0;
	IoCompleteRequest (p_IRP, IO_NO_INCREMENT);
	break;
      }

      //==============================================================
      // User mode issued a WriteFile request on the TAP file handle.
      // The request will always get satisfied here.  The call may
      // fail if there are too many pending packets (queue full).
      //==============================================================
    case IRP_MJ_WRITE:
      {
	if (p_IRP->MdlAddress == NULL)
	  {
	    DEBUGP (("[%s] MdlAddress is NULL for IRP_MJ_WRITE\n",
		     NAME (l_Adapter)));
	    NOTE_ERROR ();
	    p_IRP->IoStatus.Status = l_Status = STATUS_INVALID_PARAMETER;
	    p_IRP->IoStatus.Information = 0;
	  }
	else if ((p_IRP->AssociatedIrp.SystemBuffer =
		  MmGetSystemAddressForMdlSafe
		  (p_IRP->MdlAddress, NormalPagePriority)) == NULL)
	  {
	    DEBUGP (("[%s] Could not map address in IRP_MJ_WRITE\n",
		     NAME (l_Adapter)));
	    NOTE_ERROR ();
	    p_IRP->IoStatus.Status = l_Status = STATUS_INSUFFICIENT_RESOURCES;
	    p_IRP->IoStatus.Information = 0;
	  }
	else if (!l_Adapter->m_InterfaceIsRunning)
	  {
	    DEBUGP (("[%s] Interface is down in IRP_MJ_WRITE\n",
		     NAME (l_Adapter)));
	    NOTE_ERROR ();
	    p_IRP->IoStatus.Status = l_Status = STATUS_UNSUCCESSFUL;
	    p_IRP->IoStatus.Information = 0;
	  }
	else if (!l_Adapter->m_tun && ((l_IrpSp->Parameters.Write.Length) >= ETHERNET_HEADER_SIZE))
	  {
	    __try
	      {
		p_IRP->IoStatus.Information = l_IrpSp->Parameters.Write.Length;

		DUMP_PACKET ("IRP_MJ_WRITE ETH",
			     (unsigned char *) p_IRP->AssociatedIrp.SystemBuffer,
			     l_IrpSp->Parameters.Write.Length);

    //=====================================================
    // If IPv4 packet, check whether or not packet
    // was truncated.
    //=====================================================
#if PACKET_TRUNCATION_CHECK
		IPv4PacketSizeVerify ((unsigned char *) p_IRP->AssociatedIrp.SystemBuffer,
				      l_IrpSp->Parameters.Write.Length,
				      FALSE,
				      "RX",
				      &l_Adapter->m_RxTrunc);
#endif

		NdisMEthIndicateReceive
		  (l_Adapter->m_MiniportAdapterHandle,
		   (NDIS_HANDLE) l_Adapter,
		   (unsigned char *) p_IRP->AssociatedIrp.SystemBuffer,
		   ETHERNET_HEADER_SIZE,
		   (unsigned char *) p_IRP->AssociatedIrp.SystemBuffer + ETHERNET_HEADER_SIZE,
		   l_IrpSp->Parameters.Write.Length - ETHERNET_HEADER_SIZE,
		   l_IrpSp->Parameters.Write.Length - ETHERNET_HEADER_SIZE);
		
		NdisMEthIndicateReceiveComplete (l_Adapter->m_MiniportAdapterHandle);

		p_IRP->IoStatus.Status = l_Status = STATUS_SUCCESS;
	      }
	    __except (EXCEPTION_EXECUTE_HANDLER)
	      {
		DEBUGP (("[%s] NdisMEthIndicateReceive failed in IRP_MJ_WRITE\n",
			 NAME (l_Adapter)));
		NOTE_ERROR ();
		p_IRP->IoStatus.Status = l_Status = STATUS_UNSUCCESSFUL;
		p_IRP->IoStatus.Information = 0;
	      }
	  }
	else if (l_Adapter->m_tun && ((l_IrpSp->Parameters.Write.Length) >= IP_HEADER_SIZE))
	  {
	    __try
	      {
		ETH_HEADER * p_UserToTap = &l_Adapter->m_UserToTap;

		// for IPv6, need to use ethernet header with IPv6 proto
		if ( IPH_GET_VER( ((IPHDR*) p_IRP->AssociatedIrp.SystemBuffer)->version_len) == 6 )
		  {
		    p_UserToTap = &l_Adapter->m_UserToTap_IPv6;
		  }

		p_IRP->IoStatus.Information = l_IrpSp->Parameters.Write.Length;

		DUMP_PACKET2 ("IRP_MJ_WRITE P2P",
			      p_UserToTap,
			      (unsigned char *) p_IRP->AssociatedIrp.SystemBuffer,
			      l_IrpSp->Parameters.Write.Length);

    //=====================================================
    // If IPv4 packet, check whether or not packet
    // was truncated.
    //=====================================================
#if PACKET_TRUNCATION_CHECK
		IPv4PacketSizeVerify ((unsigned char *) p_IRP->AssociatedIrp.SystemBuffer,
				      l_IrpSp->Parameters.Write.Length,
				      TRUE,
				      "RX",
				      &l_Adapter->m_RxTrunc);
#endif

		NdisMEthIndicateReceive
		  (l_Adapter->m_MiniportAdapterHandle,
		   (NDIS_HANDLE) l_Adapter,
		   (unsigned char *) p_UserToTap,
		   sizeof (ETH_HEADER),
		   (unsigned char *) p_IRP->AssociatedIrp.SystemBuffer,
		   l_IrpSp->Parameters.Write.Length,
		   l_IrpSp->Parameters.Write.Length);

		NdisMEthIndicateReceiveComplete (l_Adapter->m_MiniportAdapterHandle);

		p_IRP->IoStatus.Status = l_Status = STATUS_SUCCESS;
	      }
	    __except (EXCEPTION_EXECUTE_HANDLER)
	      {
		DEBUGP (("[%s] NdisMEthIndicateReceive failed in IRP_MJ_WRITE (P2P)\n",
			 NAME (l_Adapter)));
		NOTE_ERROR ();
		p_IRP->IoStatus.Status = l_Status = STATUS_UNSUCCESSFUL;
		p_IRP->IoStatus.Information = 0;
	      }
	  }
	else
	  {
	    DEBUGP (("[%s] Bad buffer size in IRP_MJ_WRITE, len=%d\n",
		     NAME (l_Adapter),
		     l_IrpSp->Parameters.Write.Length));
	    NOTE_ERROR ();
	    p_IRP->IoStatus.Information = 0;	// ETHERNET_HEADER_SIZE;
	    p_IRP->IoStatus.Status = l_Status = STATUS_BUFFER_TOO_SMALL;
	  }

	if (l_Status == STATUS_SUCCESS)
	  INCREMENT_STAT (l_Adapter->m_Rx);
	else
	  INCREMENT_STAT (l_Adapter->m_RxErr);

	IoCompleteRequest (p_IRP, IO_NO_INCREMENT);
	break;
      }

      //--------------------------------------------------------------
      //   User mode thread has called CreateFile() on the tap device
      //--------------------------------------------------------------
    case IRP_MJ_CREATE:
      {
	BOOLEAN succeeded = FALSE;
	BOOLEAN mutex_succeeded;

	DEBUGP
	  (("[%s] [TAP] release [%d.%d] open request (m_TapOpens=%d)\n",
	    NAME (l_Adapter), TAP_DRIVER_MAJOR_VERSION,
	    TAP_DRIVER_MINOR_VERSION, l_Adapter->m_Extension.m_TapOpens));

	ACQUIRE_MUTEX_ADAPTIVE (&l_Adapter->m_Extension.m_OpenCloseMutex, mutex_succeeded);
	if (mutex_succeeded)
	  {
	    if (l_Adapter->m_Extension.m_TapIsRunning && !l_Adapter->m_Extension.m_TapOpens)
	      {
		ResetTapAdapterState (l_Adapter);
		l_Adapter->m_Extension.m_TapOpens = 1;
		succeeded = TRUE;
	      }

	    if (succeeded)
	      {
		INCREMENT_STAT (l_Adapter->m_Extension.m_NumTapOpens);
		p_IRP->IoStatus.Status = l_Status = STATUS_SUCCESS;
		p_IRP->IoStatus.Information = 0;
	      }
	    else
	      {
		DEBUGP (("[%s] TAP is presently unavailable (m_TapOpens=%d)\n",
			 NAME (l_Adapter), l_Adapter->m_Extension.m_TapOpens));
		NOTE_ERROR ();
		p_IRP->IoStatus.Status = l_Status = STATUS_UNSUCCESSFUL;
		p_IRP->IoStatus.Information = 0;
	      }

	    RELEASE_MUTEX (&l_Adapter->m_Extension.m_OpenCloseMutex);
	  }
	else
	  {
	    DEBUGP (("[%s] TAP is presently locked (m_TapOpens=%d)\n",
		     NAME (l_Adapter), l_Adapter->m_Extension.m_TapOpens));
	    NOTE_ERROR ();
	    p_IRP->IoStatus.Status = l_Status = STATUS_UNSUCCESSFUL;
	    p_IRP->IoStatus.Information = 0;
	  }
	
	IoCompleteRequest (p_IRP, IO_NO_INCREMENT);
	break;
      }
      
      //-----------------------------------------------------------
      //  User mode thread called CloseHandle() on the tap device
      //-----------------------------------------------------------
    case IRP_MJ_CLOSE:
      {
	BOOLEAN mutex_succeeded;

	DEBUGP (("[%s] [TAP] release [%d.%d] close/cleanup request\n",
		 NAME (l_Adapter), TAP_DRIVER_MAJOR_VERSION,
		 TAP_DRIVER_MINOR_VERSION));

	ACQUIRE_MUTEX_ADAPTIVE (&l_Adapter->m_Extension.m_OpenCloseMutex, mutex_succeeded);
	if (mutex_succeeded)
	  {
	    l_Adapter->m_Extension.m_TapOpens = 0;
	    ResetTapAdapterState (l_Adapter);
	    FlushQueues (&l_Adapter->m_Extension);
	    SetMediaStatus (l_Adapter, FALSE);
	    RELEASE_MUTEX (&l_Adapter->m_Extension.m_OpenCloseMutex);
	  }
	else
	  {
	    DEBUGP (("[%s] TAP is presently locked (m_TapOpens=%d)\n",
		     NAME (l_Adapter), l_Adapter->m_Extension.m_TapOpens));
	    NOTE_ERROR ();
	    p_IRP->IoStatus.Status = l_Status = STATUS_UNSUCCESSFUL;
	    p_IRP->IoStatus.Information = 0;
	  }
	
	IoCompleteRequest (p_IRP, IO_NO_INCREMENT);
	break;
      }

      //------------------
      // Strange Request
      //------------------
    default:
      {
	//NOTE_ERROR ();
	p_IRP->IoStatus.Status = l_Status = STATUS_UNSUCCESSFUL;
	IoCompleteRequest (p_IRP, IO_NO_INCREMENT);
	break;
      }
    }

  return l_Status;
}

//=============================================================
// CompleteIRP is normally called with an adapter -> userspace
// network packet and an IRP (Pending I/O request) from userspace.
//
// The IRP will normally represent a queued overlapped read
// operation from userspace that is in a wait state.
//
// Use the ethernet packet to satisfy the IRP.
//=============================================================

NTSTATUS
CompleteIRP (IN PIRP p_IRP,
	     IN TapPacketPointer p_PacketBuffer,
	     IN CCHAR PriorityBoost)
{
  NTSTATUS l_Status = STATUS_UNSUCCESSFUL;

  int offset;
  int len;

  MYASSERT (p_IRP);
  MYASSERT (p_PacketBuffer);

  IoSetCancelRoutine (p_IRP, NULL);  // Disable cancel routine

  //-------------------------------------------
  // While p_PacketBuffer always contains a
  // full ethernet packet, including the
  // ethernet header, in point-to-point mode,
  // we only want to return the IPv4
  // component.
  //-------------------------------------------

  if (p_PacketBuffer->m_SizeFlags & TP_TUN)
    {
      offset = ETHERNET_HEADER_SIZE;
      len = (int) (p_PacketBuffer->m_SizeFlags & TP_SIZE_MASK) - ETHERNET_HEADER_SIZE;
    }
  else
    {
      offset = 0;
      len = (p_PacketBuffer->m_SizeFlags & TP_SIZE_MASK);
    }

  if (len < 0 || (int) p_IRP->IoStatus.Information < len)
    {
      p_IRP->IoStatus.Information = 0;
      p_IRP->IoStatus.Status = STATUS_BUFFER_OVERFLOW;
      NOTE_ERROR ();
    }
  else
    {
      p_IRP->IoStatus.Information = len;
      p_IRP->IoStatus.Status = l_Status = STATUS_SUCCESS;

      __try
	{
	  NdisMoveMemory (p_IRP->AssociatedIrp.SystemBuffer,
			  p_PacketBuffer->m_Data + offset,
			  len);
	}
      __except (EXCEPTION_EXECUTE_HANDLER)
	{
	  NOTE_ERROR ();
	  p_IRP->IoStatus.Status = STATUS_UNSUCCESSFUL;
	  p_IRP->IoStatus.Information = 0;
	}
    }

  __try
    {
      NdisFreeMemory (p_PacketBuffer,
		      TAP_PACKET_SIZE (p_PacketBuffer->m_SizeFlags & TP_SIZE_MASK),
		      0);
    }
  __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
  
  if (l_Status == STATUS_SUCCESS)
    {
      IoCompleteRequest (p_IRP, PriorityBoost);
    }
  else
    IoCompleteRequest (p_IRP, IO_NO_INCREMENT);

  return l_Status;
}

//==============================================
// IRPs get cancelled for a number of reasons.
//
// The TAP device could be closed by userspace
// when there are still pending read operations.
//
// The user could disable the TAP adapter in the
// network connections control panel, while the
// device is still open by a process.
//==============================================
VOID
CancelIRPCallback (IN PDEVICE_OBJECT p_DeviceObject,
		   IN PIRP p_IRP)
{
  TapAdapterPointer l_Adapter = LookupAdapterInInstanceList (p_DeviceObject);
  CancelIRP (l_Adapter ? &l_Adapter->m_Extension : NULL, p_IRP, TRUE);
}

VOID
CancelIRP (TapExtensionPointer p_Extension,
	   IN PIRP p_IRP,
	   BOOLEAN callback)
{
  BOOLEAN exists = FALSE;

  MYASSERT (p_IRP);

  if (p_Extension)
    {
      NdisAcquireSpinLock (&p_Extension->m_QueueLock);
      exists = (QueueExtract (p_Extension->m_IrpQueue, p_IRP) == p_IRP);
      NdisReleaseSpinLock (&p_Extension->m_QueueLock);
    }
  else
    exists = TRUE;

  if (exists)
    {
      IoSetCancelRoutine (p_IRP, NULL);
      p_IRP->IoStatus.Status = STATUS_CANCELLED;
      p_IRP->IoStatus.Information = 0;
    }
     
  if (callback)
    IoReleaseCancelSpinLock (p_IRP->CancelIrql);

  if (exists)
    IoCompleteRequest (p_IRP, IO_NO_INCREMENT);
}

//===========================================
// Exhaust packet, IRP, and injection queues.
//===========================================
VOID
FlushQueues (TapExtensionPointer p_Extension)
{
  PIRP l_IRP;
  TapPacketPointer l_PacketBuffer;
  InjectPacketPointer l_InjectBuffer;
  int n_IRP=0, n_Packet=0, n_Inject=0;

  MYASSERT (p_Extension);
  MYASSERT (p_Extension->m_TapDevice);

  while (TRUE)
    {
      NdisAcquireSpinLock (&p_Extension->m_QueueLock);
      l_IRP = QueuePop (p_Extension->m_IrpQueue);
      NdisReleaseSpinLock (&p_Extension->m_QueueLock);
      if (l_IRP)
	{
	  ++n_IRP;
	  CancelIRP (NULL, l_IRP, FALSE);
	}
      else
	break;
    }

  while (TRUE)
    {
      NdisAcquireSpinLock (&p_Extension->m_QueueLock);
      l_PacketBuffer = QueuePop (p_Extension->m_PacketQueue);
      NdisReleaseSpinLock (&p_Extension->m_QueueLock);
      if (l_PacketBuffer)
	{
	  ++n_Packet;
	  MemFree (l_PacketBuffer, TAP_PACKET_SIZE (l_PacketBuffer->m_SizeFlags & TP_SIZE_MASK));
	}
      else
	break;
    }

  while (TRUE)
    {
      NdisAcquireSpinLock (&p_Extension->m_InjectLock);
      l_InjectBuffer = QueuePop (p_Extension->m_InjectQueue);
      NdisReleaseSpinLock (&p_Extension->m_InjectLock);
      if (l_InjectBuffer)
	{
	  ++n_Inject;
	  INJECT_PACKET_FREE(l_InjectBuffer);
	}
      else
	break;
    }

  DEBUGP ((
	   "[%s] [TAP] FlushQueues n_IRP=[%d,%d,%d] n_Packet=[%d,%d,%d] n_Inject=[%d,%d,%d]\n",
	   p_Extension->m_TapName,
	   n_IRP,
	   p_Extension->m_IrpQueue->max_size,
	   IRP_QUEUE_SIZE,
	   n_Packet,
	   p_Extension->m_PacketQueue->max_size,
	   PACKET_QUEUE_SIZE,
	   n_Inject,
	   p_Extension->m_InjectQueue->max_size,
	   INJECT_QUEUE_SIZE
	   ));
}

//===================================================
// Tell Windows whether the TAP device should be
// considered "connected" or "disconnected".
//===================================================
VOID
SetMediaStatus (TapAdapterPointer p_Adapter, BOOLEAN state)
{
  if (p_Adapter->m_MediaState != state && !p_Adapter->m_MediaStateAlwaysConnected)
    {
      if (state)
	NdisMIndicateStatus (p_Adapter->m_MiniportAdapterHandle,
			     NDIS_STATUS_MEDIA_CONNECT, NULL, 0);
      else
	NdisMIndicateStatus (p_Adapter->m_MiniportAdapterHandle,
			     NDIS_STATUS_MEDIA_DISCONNECT, NULL, 0);

      NdisMIndicateStatusComplete (p_Adapter->m_MiniportAdapterHandle);
      p_Adapter->m_MediaState = state;
    }
}


//======================================================
// If DHCP mode is used together with tun
// mode, consider the fact that the P2P remote subnet
// might enclose the DHCP masq server address.
//======================================================
VOID
CheckIfDhcpAndTunMode (TapAdapterPointer p_Adapter)
{
  if (p_Adapter->m_tun && p_Adapter->m_dhcp_enabled)
    {
      if ((p_Adapter->m_dhcp_server_ip & p_Adapter->m_remoteNetmask) == p_Adapter->m_remoteNetwork)
	{
	  COPY_MAC (p_Adapter->m_dhcp_server_mac, p_Adapter->m_TapToUser.dest);
	  p_Adapter->m_dhcp_server_arp = FALSE;
	}
    }
}

//===================================================
// Generate an ARP reply message for specific kinds
// ARP queries.
//===================================================
BOOLEAN
ProcessARP (TapAdapterPointer p_Adapter,
	    const PARP_PACKET src,
	    const IPADDR adapter_ip,
	    const IPADDR ip_network,
	    const IPADDR ip_netmask,
	    const MACADDR mac)
{
  //-----------------------------------------------
  // Is this the kind of packet we are looking for?
  //-----------------------------------------------
  if (src->m_Proto == htons (ETH_P_ARP)
      && MAC_EQUAL (src->m_MAC_Source, p_Adapter->m_MAC)
      && MAC_EQUAL (src->m_ARP_MAC_Source, p_Adapter->m_MAC)
      && MAC_EQUAL (src->m_MAC_Destination, p_Adapter->m_MAC_Broadcast)
      && src->m_ARP_Operation == htons (ARP_REQUEST)
      && src->m_MAC_AddressType == htons (MAC_ADDR_TYPE)
      && src->m_MAC_AddressSize == sizeof (MACADDR)
      && src->m_PROTO_AddressType == htons (ETH_P_IP)
      && src->m_PROTO_AddressSize == sizeof (IPADDR)
      && src->m_ARP_IP_Source == adapter_ip
      && (src->m_ARP_IP_Destination & ip_netmask) == ip_network
      && src->m_ARP_IP_Destination != adapter_ip)
    {
      ARP_PACKET *arp = (ARP_PACKET *) MemAlloc (sizeof (ARP_PACKET), TRUE);
      if (arp)
	{
	  //----------------------------------------------
	  // Initialize ARP reply fields
	  //----------------------------------------------
	  arp->m_Proto = htons (ETH_P_ARP);
	  arp->m_MAC_AddressType = htons (MAC_ADDR_TYPE);
	  arp->m_PROTO_AddressType = htons (ETH_P_IP);
	  arp->m_MAC_AddressSize = sizeof (MACADDR);
	  arp->m_PROTO_AddressSize = sizeof (IPADDR);
	  arp->m_ARP_Operation = htons (ARP_REPLY);

	  //----------------------------------------------
	  // ARP addresses
	  //----------------------------------------------      
	  COPY_MAC (arp->m_MAC_Source, mac);
	  COPY_MAC (arp->m_MAC_Destination, p_Adapter->m_MAC);
	  COPY_MAC (arp->m_ARP_MAC_Source, mac);
	  COPY_MAC (arp->m_ARP_MAC_Destination, p_Adapter->m_MAC);
	  arp->m_ARP_IP_Source = src->m_ARP_IP_Destination;
	  arp->m_ARP_IP_Destination = adapter_ip;

	  DUMP_PACKET ("ProcessARP",
		       (unsigned char *) arp,
		       sizeof (ARP_PACKET));

	  InjectPacketDeferred (p_Adapter, (UCHAR *) arp, sizeof (ARP_PACKET));

	  MemFree (arp, sizeof (ARP_PACKET));
	}

      return TRUE;
    }
  else
    return FALSE;
}

//===============================================================
// Used in cases where internally generated packets such as
// ARP or DHCP replies must be returned to the kernel, to be
// seen as an incoming packet "arriving" on the interface.
//===============================================================

// Defer packet injection till IRQL < DISPATCH_LEVEL
VOID
InjectPacketDeferred (TapAdapterPointer p_Adapter,
		      UCHAR *packet,
		      const unsigned int len)
{
  InjectPacketPointer l_InjectBuffer;
  PVOID result;

  if (NdisAllocateMemoryWithTag (&l_InjectBuffer,
				 INJECT_PACKET_SIZE (len),
				 'IPAT') == NDIS_STATUS_SUCCESS)
    {
      l_InjectBuffer->m_Size = len;
      NdisMoveMemory (l_InjectBuffer->m_Data, packet, len);
      NdisAcquireSpinLock (&p_Adapter->m_Extension.m_InjectLock);
      result = QueuePush (p_Adapter->m_Extension.m_InjectQueue, l_InjectBuffer);
      NdisReleaseSpinLock (&p_Adapter->m_Extension.m_InjectLock);
      if (result)
	KeInsertQueueDpc (&p_Adapter->m_Extension.m_InjectDpc, p_Adapter, NULL);
      else
	INJECT_PACKET_FREE(l_InjectBuffer);
    }
}

// Handle the injection of previously deferred packets
VOID
InjectPacketDpc(KDPC *Dpc,
		PVOID DeferredContext,
		PVOID SystemArgument1,
		PVOID SystemArgument2)
{
  InjectPacketPointer l_InjectBuffer;
  TapAdapterPointer l_Adapter = (TapAdapterPointer)SystemArgument1;
  while (TRUE)
    {
      NdisAcquireSpinLock (&l_Adapter->m_Extension.m_InjectLock);
      l_InjectBuffer = QueuePop (l_Adapter->m_Extension.m_InjectQueue);
      NdisReleaseSpinLock (&l_Adapter->m_Extension.m_InjectLock);
      if (l_InjectBuffer)
	{
	  InjectPacketNow(l_Adapter, l_InjectBuffer->m_Data, l_InjectBuffer->m_Size);
	  INJECT_PACKET_FREE(l_InjectBuffer);
	}
      else
	break;
    }
}

// Do packet injection now
VOID
InjectPacketNow (TapAdapterPointer p_Adapter,
		 UCHAR *packet,
		 const unsigned int len)
{
  MYASSERT (len >= ETHERNET_HEADER_SIZE);

  __try
    {
      //------------------------------------------------------------
      // NdisMEthIndicateReceive and NdisMEthIndicateReceiveComplete
      // could potentially be called reentrantly both here and in
      // TapDeviceHook/IRP_MJ_WRITE.
      //
      // The DDK docs imply that this is okay.
      //
      // Note that reentrant behavior could only occur if the
      // non-deferred version of InjectPacket is used.
      //------------------------------------------------------------
      NdisMEthIndicateReceive
	(p_Adapter->m_MiniportAdapterHandle,
	 (NDIS_HANDLE) p_Adapter,
	 packet,
	 ETHERNET_HEADER_SIZE,
	 packet + ETHERNET_HEADER_SIZE,
	 len - ETHERNET_HEADER_SIZE,
	 len - ETHERNET_HEADER_SIZE);
      
      NdisMEthIndicateReceiveComplete (p_Adapter->m_MiniportAdapterHandle);
    }
  __except (EXCEPTION_EXECUTE_HANDLER)
    {
      DEBUGP (("[%s] NdisMEthIndicateReceive failed in InjectPacketNow\n",
	       NAME (p_Adapter)));
      NOTE_ERROR ();
    }
}

//===================================================================
// Go back to default TAP mode from Point-To-Point mode.
// Also reset (i.e. disable) DHCP Masq mode.
//===================================================================
VOID ResetTapAdapterState (TapAdapterPointer p_Adapter)
{
  // Point-To-Point
  p_Adapter->m_tun = FALSE;
  p_Adapter->m_localIP = 0;
  p_Adapter->m_remoteNetwork = 0;
  p_Adapter->m_remoteNetmask = 0;
  NdisZeroMemory (&p_Adapter->m_TapToUser, sizeof (p_Adapter->m_TapToUser));
  NdisZeroMemory (&p_Adapter->m_UserToTap, sizeof (p_Adapter->m_UserToTap));
  NdisZeroMemory (&p_Adapter->m_UserToTap_IPv6, sizeof (p_Adapter->m_UserToTap_IPv6));

  // DHCP Masq
  p_Adapter->m_dhcp_enabled = FALSE;
  p_Adapter->m_dhcp_server_arp = FALSE;
  p_Adapter->m_dhcp_user_supplied_options_buffer_len = 0;
  p_Adapter->m_dhcp_addr = 0;
  p_Adapter->m_dhcp_netmask = 0;
  p_Adapter->m_dhcp_server_ip = 0;
  p_Adapter->m_dhcp_lease_time = 0;
  p_Adapter->m_dhcp_received_discover = FALSE;
  p_Adapter->m_dhcp_bad_requests = 0;
  NdisZeroMemory (p_Adapter->m_dhcp_server_mac, sizeof (MACADDR));
}

#if ENABLE_NONADMIN

//===================================================================
// Set TAP device handle to be accessible without admin privileges.
//===================================================================
VOID AllowNonAdmin (TapExtensionPointer p_Extension)
{
  NTSTATUS stat;
  SECURITY_DESCRIPTOR sd;
  OBJECT_ATTRIBUTES oa;
  IO_STATUS_BLOCK isb;
  HANDLE hand = NULL;

  NdisZeroMemory (&sd, sizeof (sd));
  NdisZeroMemory (&oa, sizeof (oa));
  NdisZeroMemory (&isb, sizeof (isb));

  if (!p_Extension->m_CreatedUnicodeLinkName)
    {
      DEBUGP (("[TAP] AllowNonAdmin: UnicodeLinkName is uninitialized\n"));
      NOTE_ERROR ();
      return;
    }

  stat = RtlCreateSecurityDescriptor (&sd, SECURITY_DESCRIPTOR_REVISION);
  if (stat != STATUS_SUCCESS)
    {
      DEBUGP (("[TAP] AllowNonAdmin: RtlCreateSecurityDescriptor failed\n"));
      NOTE_ERROR ();
      return;
    }

  InitializeObjectAttributes (
    &oa,
    &p_Extension->m_UnicodeLinkName,
    OBJ_KERNEL_HANDLE,
    NULL,
    NULL
    );

  stat = ZwOpenFile (
    &hand,
    WRITE_DAC,
    &oa,
    &isb,
    0,
    0
    );
  if (stat != STATUS_SUCCESS)
    {
      DEBUGP (("[TAP] AllowNonAdmin: ZwOpenFile failed, status=0x%08x\n", (unsigned int)stat));
      NOTE_ERROR ();
      return;
    }

  stat = ZwSetSecurityObject (hand, DACL_SECURITY_INFORMATION, &sd);
  if (stat != STATUS_SUCCESS)
    {
      DEBUGP (("[TAP] AllowNonAdmin: ZwSetSecurityObject failed\n"));
      NOTE_ERROR ();
      return;
    }

  stat = ZwClose (hand);
  if (stat != STATUS_SUCCESS)
    {
      DEBUGP (("[TAP] AllowNonAdmin: ZwClose failed\n"));
      NOTE_ERROR ();
      return;
    }

  DEBUGP (("[TAP] AllowNonAdmin: SUCCEEDED\n"));
}

#endif

#if PACKET_TRUNCATION_CHECK

VOID
IPv4PacketSizeVerify (const UCHAR *data, ULONG length, BOOLEAN tun, const char *prefix, LONG *counter)
{
  const IPHDR *ip;
  int len = length;

  if (tun)
    {
      ip = (IPHDR *) data;
    }
  else
    {
      if (length >= sizeof (ETH_HEADER))
	{
	  const ETH_HEADER *eth = (ETH_HEADER *) data;

	  if (eth->proto != htons (ETH_P_IP))
	    return;

	  ip = (IPHDR *) (data + sizeof (ETH_HEADER));
	  len -= sizeof (ETH_HEADER);
	}
      else
	return;
    }

  if (len >= sizeof (IPHDR))
    {
      const int totlen = ntohs (ip->tot_len);

      DEBUGP (("[TAP] IPv4PacketSizeVerify %s len=%d totlen=%d\n", prefix, len, totlen));

      if (len != totlen)
	++(*counter);
    }
}

#endif

//======================================================================
//                                    End of Source
//======================================================================
