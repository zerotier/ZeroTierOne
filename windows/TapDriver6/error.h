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

//-----------------
// DEBUGGING OUTPUT
//-----------------

extern const char *g_LastErrorFilename;
extern int g_LastErrorLineNumber;

// Debug info output
#define ALSO_DBGPRINT           1
#define DEBUGP_AT_DISPATCH      1

// Uncomment line below to allow packet dumps
//#define ALLOW_PACKET_DUMP       1

#define NOTE_ERROR() \
{ \
  g_LastErrorFilename = __FILE__; \
  g_LastErrorLineNumber = __LINE__; \
}

#if DBG

typedef struct
{
    unsigned int in;
    unsigned int out;
    unsigned int capacity;
    char *text;
    BOOLEAN error;
    MUTEX lock;
} DebugOutput;

VOID MyDebugPrint (const unsigned char* format, ...);

VOID PrMac (const MACADDR mac);

VOID PrIP (IPADDR ip_addr);

#ifdef ALLOW_PACKET_DUMP

VOID
DumpPacket(
    __in const char *prefix,
    __in const unsigned char *data,
    __in unsigned int len
    );

DumpPacket2(
    __in const char *prefix,
    __in const ETH_HEADER *eth,
    __in const unsigned char *data,
    __in unsigned int len
    );

#else
#define DUMP_PACKET(prefix, data, len)
#define DUMP_PACKET2(prefix, eth, data, len)
#endif

#define CAN_WE_PRINT (DEBUGP_AT_DISPATCH || KeGetCurrentIrql () < DISPATCH_LEVEL)

#if ALSO_DBGPRINT
#define DEBUGP(fmt) { MyDebugPrint fmt; if (CAN_WE_PRINT) DbgPrint fmt; }
#else
#define DEBUGP(fmt) { MyDebugPrint fmt; }
#endif

#ifdef ALLOW_PACKET_DUMP

#define DUMP_PACKET(prefix, data, len) \
  DumpPacket (prefix, data, len)

#define DUMP_PACKET2(prefix, eth, data, len) \
  DumpPacket2 (prefix, eth, data, len)

#endif

BOOLEAN
GetDebugLine (
    __in char *buf,
    __in const int len
    );

#else 

#define DEBUGP(fmt)
#define DUMP_PACKET(prefix, data, len)
#define DUMP_PACKET2(prefix, eth, data, len)

#endif
