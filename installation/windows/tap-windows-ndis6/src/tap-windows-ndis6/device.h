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

#ifndef __TAP_DEVICE_H_
#define __TAP_DEVICE_H_

//======================================================================
// TAP Prototypes for standard Win32 device I/O entry points
//======================================================================

__drv_dispatchType(IRP_MJ_CREATE)
DRIVER_DISPATCH TapDeviceCreate;

__drv_dispatchType(IRP_MJ_READ)
DRIVER_DISPATCH TapDeviceRead;

__drv_dispatchType(IRP_MJ_WRITE)
DRIVER_DISPATCH TapDeviceWrite;

__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH TapDeviceControl;

__drv_dispatchType(IRP_MJ_CLEANUP)
DRIVER_DISPATCH TapDeviceCleanup;

__drv_dispatchType(IRP_MJ_CLOSE)
DRIVER_DISPATCH TapDeviceClose;

#endif // __TAP_DEVICE_H_