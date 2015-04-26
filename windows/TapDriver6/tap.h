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
#ifndef __TAP_H
#define __TAP_H

#ifndef NDIS_SUPPORT_NDIS6
#define NDIS_SUPPORT_NDIS6 1
#define NDIS_SUPPORT_NDIS61 1
#define NDIS_WDM1 1
#define NDIS61_MINIPORT 1
#endif

#include <ntifs.h>
#include <ndis.h>
#include <ntstrsafe.h>
#include <netioapi.h>

#include "config.h"
#include "lock.h"
#include "constants.h"
#include "proto.h"
#include "mem.h"
#include "macinfo.h"
#include "error.h"
#include "endian.h"
#include "types.h"
#include "adapter.h"
#include "device.h"
#include "prototypes.h"
#include "tap-windows.h"

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

//
// The driver has exactly one instance of the TAP_GLOBAL structure.  NDIS keeps
// an opaque handle to this data, (it doesn't attempt to read or interpret this
// data), and it passes the handle back to the miniport in MiniportSetOptions
// and MiniportInitializeEx.
//
typedef struct _TAP_GLOBAL
{
    LIST_ENTRY          AdapterList;

    NDIS_RW_LOCK        Lock;

    NDIS_HANDLE         NdisDriverHandle;   // From NdisMRegisterMiniportDriver

} TAP_GLOBAL, *PTAP_GLOBAL;


// Global data
extern TAP_GLOBAL      GlobalData;

#endif // __TAP_H
