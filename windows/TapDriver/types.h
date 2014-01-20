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

#ifndef TAP_TYPES_DEFINED
#define TAP_TYPES_DEFINED

typedef struct _Queue
{
  ULONG base;
  ULONG size;
  ULONG capacity;
  ULONG max_size;
  PVOID data[];
} Queue;

typedef struct _TapAdapter;
typedef struct _TapPacket;

typedef union _TapAdapterQuery
{
  NDIS_HARDWARE_STATUS m_HardwareStatus;
  NDIS_MEDIUM m_Medium;
  NDIS_PHYSICAL_MEDIUM m_PhysicalMedium;
  UCHAR m_MacAddress [6];
  UCHAR m_Buffer [256];
  ULONG m_Long;
  USHORT m_Short;
  UCHAR m_Byte;
}
TapAdapterQuery, *TapAdapterQueryPointer;

typedef struct _TapExtension
{
  // TAP device object and packet queues
  Queue *m_PacketQueue, *m_IrpQueue;
  PDEVICE_OBJECT m_TapDevice;
  NDIS_HANDLE m_TapDeviceHandle;
  ULONG m_TapOpens;

  // Used to lock packet queues
  NDIS_SPIN_LOCK m_QueueLock;
  BOOLEAN m_AllocatedSpinlocks;

  // Used to bracket open/close
  // state changes.
  MUTEX m_OpenCloseMutex;

  // True if device has been permanently halted
  BOOLEAN m_Halt;

  // TAP device name
  unsigned char *m_TapName;
  UNICODE_STRING m_UnicodeLinkName;
  BOOLEAN m_CreatedUnicodeLinkName;

  // Used for device status ioctl only
  const char *m_LastErrorFilename;
  int m_LastErrorLineNumber;
  LONG m_NumTapOpens;

  // Flags
  BOOLEAN m_TapIsRunning;
  BOOLEAN m_CalledTapDeviceFreeResources;
}
TapExtension, *TapExtensionPointer;

typedef struct _TapPacket
   {
#   define TAP_PACKET_SIZE(data_size) (sizeof (TapPacket) + (data_size))
#   define TP_TUN 0x80000000
#   define TP_SIZE_MASK      (~TP_TUN)
    ULONG m_SizeFlags;
    UCHAR m_Data []; // m_Data must be the last struct member
   }
TapPacket, *TapPacketPointer;

typedef struct _TapAdapter
{
# define NAME(a) ((a)->m_NameAnsi.Buffer)
  ANSI_STRING m_NameAnsi;
  MACADDR m_MAC;
  BOOLEAN m_InterfaceIsRunning;
  NDIS_HANDLE m_MiniportAdapterHandle;
  LONG m_Rx, m_Tx, m_RxErr, m_TxErr;
  NDIS_MEDIUM m_Medium;
  ULONG m_Lookahead;
  ULONG m_MTU;

  // TRUE if adapter should always be
  // "connected" even when device node
  // is not open by a userspace process.
  BOOLEAN m_MediaStateAlwaysConnected;

  // TRUE if device is "connected"
  BOOLEAN m_MediaState;

  // Adapter power state
  char m_DeviceState;

  // Help to tear down the adapter by keeping
  // some state information on allocated
  // resources.
  BOOLEAN m_CalledAdapterFreeResources;
  BOOLEAN m_RegisteredAdapterShutdownHandler;

  // Multicast list info
  NDIS_SPIN_LOCK m_MCLock;
  BOOLEAN m_MCLockAllocated;
  ULONG m_MCListSize;
  MC_LIST m_MCList;

  // Information on the TAP device
  TapExtension m_Extension;
} TapAdapter, *TapAdapterPointer;

#endif
