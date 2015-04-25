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

#ifndef TAP_TYPES_DEFINED
#define TAP_TYPES_DEFINED

//typedef
//struct _Queue
//{
//    ULONG base;
//    ULONG size;
//    ULONG capacity;
//    ULONG max_size;
//    PVOID data[];
//} Queue;

//typedef struct _TAP_PACKET;

//typedef struct _TapExtension
//{
//  // TAP device object and packet queues
//  Queue *m_PacketQueue, *m_IrpQueue;
//  PDEVICE_OBJECT m_TapDevice;
//  NDIS_HANDLE m_TapDeviceHandle;
//  ULONG TapFileIsOpen;
//
//  // Used to lock packet queues
//  NDIS_SPIN_LOCK m_QueueLock;
//  BOOLEAN m_AllocatedSpinlocks;
//
//  // Used to bracket open/close
//  // state changes.
//  MUTEX m_OpenCloseMutex;
//
//  // True if device has been permanently halted
//  BOOLEAN m_Halt;
//
//  // TAP device name
//  unsigned char *m_TapName;
//  UNICODE_STRING m_UnicodeLinkName;
//  BOOLEAN m_CreatedUnicodeLinkName;
//
//  // Used for device status ioctl only
//  const char *m_LastErrorFilename;
//  int m_LastErrorLineNumber;
//  LONG TapFileOpenCount;
//
//  // Flags
//  BOOLEAN TapDeviceCreated;
//  BOOLEAN m_CalledTapDeviceFreeResources;
//
//  // DPC queue for deferred packet injection
//  BOOLEAN m_InjectDpcInitialized;
//  KDPC m_InjectDpc;
//  NDIS_SPIN_LOCK m_InjectLock;
//  Queue *m_InjectQueue;
//}
//TapExtension, *TapExtensionPointer;

typedef struct _InjectPacket
   {
#   define INJECT_PACKET_SIZE(data_size) (sizeof (InjectPacket) + (data_size))
#   define INJECT_PACKET_FREE(ib)  NdisFreeMemory ((ib), INJECT_PACKET_SIZE ((ib)->m_Size), 0)
    ULONG m_Size;
    UCHAR m_Data []; // m_Data must be the last struct member
   }
InjectPacket, *InjectPacketPointer;

#endif
