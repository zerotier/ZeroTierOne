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

//-----------------
// DEBUGGING OUTPUT
//-----------------

#define NOTE_ERROR() \
{ \
  g_LastErrorFilename = __FILE__; \
  g_LastErrorLineNumber = __LINE__; \
}

#if DBG

typedef struct {
  unsigned int in;
  unsigned int out;
  unsigned int capacity;
  char *text;
  BOOLEAN error;
  MUTEX lock;
} DebugOutput;

VOID MyDebugPrint (const unsigned char* format, ...);

VOID MyAssert (const unsigned char *file, int line);

VOID DumpPacket (const char *prefix,
		 const unsigned char *data,
		 unsigned int len);

VOID DumpPacket2 (const char *prefix,
		  const ETH_HEADER *eth,
		  const unsigned char *data,
		  unsigned int len);

#define CAN_WE_PRINT (DEBUGP_AT_DISPATCH || KeGetCurrentIrql () < DISPATCH_LEVEL)

#if ALSO_DBGPRINT
#define DEBUGP(fmt) { MyDebugPrint fmt; if (CAN_WE_PRINT) DbgPrint fmt; }
#else
#define DEBUGP(fmt) { MyDebugPrint fmt; }
#endif

#define MYASSERT(exp) \
{ \
  if (!(exp)) \
    { \
      MyAssert(__FILE__, __LINE__); \
    } \
}

#define DUMP_PACKET(prefix, data, len) \
  DumpPacket (prefix, data, len)

#define DUMP_PACKET2(prefix, eth, data, len) \
  DumpPacket2 (prefix, eth, data, len)

#else 

#define DEBUGP(fmt)
#define MYASSERT(exp)
#define DUMP_PACKET(prefix, data, len)
#define DUMP_PACKET2(prefix, eth, data, len)

#endif
