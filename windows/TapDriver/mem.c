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

//------------------
// Memory Management
//------------------

PVOID
MemAlloc (ULONG p_Size, BOOLEAN zero)
{
  PVOID l_Return = NULL;

  if (p_Size)
    {
      __try
      {
	if (NdisAllocateMemoryWithTag (&l_Return, p_Size, 'APAT')
	    == NDIS_STATUS_SUCCESS)
	  {
	    if (zero)
	      NdisZeroMemory (l_Return, p_Size);
	  }
	else
	  l_Return = NULL;
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
	l_Return = NULL;
      }
    }

  return l_Return;
}

VOID
MemFree (PVOID p_Addr, ULONG p_Size)
{
  if (p_Addr && p_Size)
    {
      __try
      {
#if DBG
	NdisZeroMemory (p_Addr, p_Size);
#endif
	NdisFreeMemory (p_Addr, p_Size, 0);
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
      }
    }
}

/*
 * Circular queue management routines.
 */

#define QUEUE_BYTE_ALLOCATION(size) \
  (sizeof (Queue) + (size * sizeof (PVOID)))

#define QUEUE_ADD_INDEX(var, inc) \
{ \
  var += inc; \
  if (var >= q->capacity) \
    var -= q->capacity; \
  MYASSERT (var < q->capacity); \
}

#define QUEUE_SANITY_CHECK() \
  MYASSERT (q != NULL && q->base < q->capacity && q->size <= q->capacity)

#define QueueCount(q) (q->size)

#define UPDATE_MAX_SIZE() \
{ \
  if (q->size > q->max_size) \
    q->max_size = q->size; \
}

Queue *
QueueInit (ULONG capacity)
{
  Queue *q;

  MYASSERT (capacity > 0);
  q = (Queue *) MemAlloc (QUEUE_BYTE_ALLOCATION (capacity), TRUE);
  if (!q)
    return NULL;

  q->base = q->size = 0;
  q->capacity = capacity;
  q->max_size = 0;
  return q;
}

VOID
QueueFree (Queue *q)
{
  if (q)
    {
      QUEUE_SANITY_CHECK ();
      MemFree (q, QUEUE_BYTE_ALLOCATION (q->capacity));
    }
}

PVOID
QueuePush (Queue *q, PVOID item)
{
  ULONG dest;
  QUEUE_SANITY_CHECK ();
  if (q->size == q->capacity)
    return NULL;
  dest = q->base;
  QUEUE_ADD_INDEX (dest, q->size);
  q->data[dest] = item;
  ++q->size;
  UPDATE_MAX_SIZE();
  return item;
}

PVOID
QueuePop (Queue *q)
{
  ULONG oldbase;
  QUEUE_SANITY_CHECK ();
  if (!q->size)
    return NULL;
  oldbase = q->base;
  QUEUE_ADD_INDEX (q->base, 1);
  --q->size;
  UPDATE_MAX_SIZE();
  return q->data[oldbase];
}

PVOID
QueueExtract (Queue *q, PVOID item)
{
  ULONG src, dest, count, n;
  QUEUE_SANITY_CHECK ();
  n = 0;
  src = dest = q->base;
  count = q->size;
  while (count--)
    {
      if (item == q->data[src])
	{
	  ++n;
	  --q->size;
	}
      else
	{
	  q->data[dest] = q->data[src];
	  QUEUE_ADD_INDEX (dest, 1);	  
	}
      QUEUE_ADD_INDEX (src, 1);
    }
  if (n)
    return item;
  else
    return NULL;
}

#undef QUEUE_BYTE_ALLOCATION
#undef QUEUE_ADD_INDEX
#undef QUEUE_SANITY_CHECK
#undef UPDATE_MAX_SIZE
