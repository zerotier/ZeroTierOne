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

#define INSTANCE_KEY(a) ((PVOID)((a)->m_Extension.m_TapDevice))

#define N_INSTANCE_BUCKETS 256

typedef struct _INSTANCE {
  struct _INSTANCE *next;
  TapAdapterPointer m_Adapter;
} INSTANCE;

typedef struct {
  INSTANCE *list;
  MUTEX lock;
} INSTANCE_BUCKET;

typedef struct {
  INSTANCE_BUCKET buckets[N_INSTANCE_BUCKETS];
} INSTANCE_HASH;

INSTANCE_HASH *g_InstanceHash = NULL;

// must return a hash >= 0 and < N_INSTANCE_BUCKETS
int
InstanceHashValue (PVOID addr)
{
  UCHAR *p = (UCHAR *) &addr;

  if (sizeof (addr) == 4)
    return p[0] ^ p[1] ^ p[2] ^ p[3];
  else if (sizeof (addr) == 8)
    return p[0] ^ p[1] ^ p[2] ^ p[3] ^ p[4] ^ p[5] ^ p[6] ^ p[7];
  else
    {
      MYASSERT (0);
    }
}

BOOLEAN
InitInstanceList (VOID)
{
  MYASSERT (g_InstanceHash == NULL);
  g_InstanceHash = MemAlloc (sizeof (INSTANCE_HASH), TRUE);
  if (g_InstanceHash)
    {
      int i;
      for (i = 0; i < N_INSTANCE_BUCKETS; ++i)
	INIT_MUTEX (&g_InstanceHash->buckets[i].lock);
      return TRUE;
    }
  else
    return FALSE;
}

int
NInstances (VOID)
{
  int i, n = 0;

  if (g_InstanceHash)
    {
      for (i = 0; i < N_INSTANCE_BUCKETS; ++i)
	{
	  BOOLEAN got_lock;
	  INSTANCE_BUCKET *ib = &g_InstanceHash->buckets[i];
	  ACQUIRE_MUTEX_ADAPTIVE (&ib->lock, got_lock);

	  if (got_lock)
	    {
	      INSTANCE *current;
	      for (current = ib->list; current != NULL; current = current->next)
		++n;
	      RELEASE_MUTEX (&ib->lock);
	    }
	  else
	    return -1;
	}
    }

  return n;
}

int
InstanceMaxBucketSize (VOID)
{
  int i, n = 0;

  if (g_InstanceHash)
    {
      for (i = 0; i < N_INSTANCE_BUCKETS; ++i)
	{
	  BOOLEAN got_lock;
	  int bucket_size = 0;
	  INSTANCE_BUCKET *ib = &g_InstanceHash->buckets[i];
	  ACQUIRE_MUTEX_ADAPTIVE (&ib->lock, got_lock);

	  if (got_lock)
	    {
	      INSTANCE *current;
	      for (current = ib->list; current != NULL; current = current->next)
		  ++bucket_size;
	      if (bucket_size > n)
		n = bucket_size;
	      RELEASE_MUTEX (&ib->lock);
	    }
	  else
	    return -1;
	}
    }

  return n;
}

VOID
FreeInstanceList (VOID)
{
  if (g_InstanceHash)
    {
      MYASSERT (NInstances() == 0);
      MemFree (g_InstanceHash, sizeof (INSTANCE_HASH));
      g_InstanceHash = NULL;
    }
}

BOOLEAN
AddAdapterToInstanceList (TapAdapterPointer p_Adapter)
{
  BOOLEAN got_lock;
  BOOLEAN ret = FALSE;
  const int hash = InstanceHashValue(INSTANCE_KEY(p_Adapter));
  INSTANCE_BUCKET *ib = &g_InstanceHash->buckets[hash];

  DEBUGP (("[TAP] AddAdapterToInstanceList hash=%d\n", hash));

  ACQUIRE_MUTEX_ADAPTIVE (&ib->lock, got_lock);

  if (got_lock)
    {
      INSTANCE *i = MemAlloc (sizeof (INSTANCE), FALSE);
      if (i)
	{
	  MYASSERT (p_Adapter);
	  i->m_Adapter = p_Adapter;
	  i->next = ib->list;
	  ib->list = i;
	  ret = TRUE;
	}
      RELEASE_MUTEX (&ib->lock);
    }

  return ret;
}

BOOLEAN
RemoveAdapterFromInstanceList (TapAdapterPointer p_Adapter)
{
  BOOLEAN got_lock;
  BOOLEAN ret = FALSE;
  INSTANCE_BUCKET *ib = &g_InstanceHash->buckets[InstanceHashValue(INSTANCE_KEY(p_Adapter))];

  ACQUIRE_MUTEX_ADAPTIVE (&ib->lock, got_lock);

  if (got_lock)
    {
      INSTANCE *current, *prev=NULL;
      for (current = ib->list; current != NULL; current = current->next)
	{
	  if (current->m_Adapter == p_Adapter) // found match
	    {
	      if (prev)
		prev->next = current->next;
	      else
		ib->list = current->next;
	      MemFree (current->m_Adapter, sizeof (TapAdapter));
	      MemFree (current, sizeof (INSTANCE));
	      ret = TRUE;
	      break;
	    }
	  prev = current;
	}
      RELEASE_MUTEX (&ib->lock);
    }

  return ret;
}

TapAdapterPointer
LookupAdapterInInstanceList (PDEVICE_OBJECT p_DeviceObject)
{
  BOOLEAN got_lock;
  TapAdapterPointer ret = NULL;
  INSTANCE_BUCKET *ib = &g_InstanceHash->buckets[InstanceHashValue((PVOID)p_DeviceObject)];

  ACQUIRE_MUTEX_ADAPTIVE (&ib->lock, got_lock);

  if (got_lock)
    {
      INSTANCE *current, *prev=NULL;
      for (current = ib->list; current != NULL; current = current->next)
	{
	  if (p_DeviceObject == INSTANCE_KEY (current->m_Adapter)) // found match
	    {
	      // move it to head of list
	      if (prev)
		{
		  prev->next = current->next;
		  current->next = ib->list;
		  ib->list = current;
		}
	      ret = ib->list->m_Adapter;
	      break;
	    }
	  prev = current;
	}
      RELEASE_MUTEX (&ib->lock);
    }

  return ret;
}
