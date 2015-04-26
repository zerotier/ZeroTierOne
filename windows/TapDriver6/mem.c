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

//------------------
// Memory Management
//------------------

#include "tap.h"

PVOID
MemAlloc(
    __in ULONG p_Size,
    __in BOOLEAN zero
    )
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
                {
                    NdisZeroMemory (l_Return, p_Size);
                }
            }
            else
            {
                l_Return = NULL;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            l_Return = NULL;
        }
    }

    return l_Return;
}

VOID
MemFree(
    __in PVOID p_Addr,
    __in ULONG p_Size
    )
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

//======================================================================
// TAP Packet Queue Support
//======================================================================

VOID
tapPacketQueueInsertTail(
    __in PTAP_PACKET_QUEUE  TapPacketQueue,
    __in PTAP_PACKET        TapPacket
    )
{
    KIRQL  irql;

    KeAcquireSpinLock(&TapPacketQueue->QueueLock,&irql);

    InsertTailList(&TapPacketQueue->Queue,&TapPacket->QueueLink);

    // BUGBUG!!! Enforce PACKET_QUEUE_SIZE queue count limit???
    // For NDIS 6 there is no per-packet status, so this will need to
    // be handled on per-NBL basis in AdapterSendNetBufferLists...

    // Update counts
    ++TapPacketQueue->Count;

    if(TapPacketQueue->Count > TapPacketQueue->MaxCount)
    {
        TapPacketQueue->MaxCount = TapPacketQueue->Count;

        DEBUGP (("[TAP] tapPacketQueueInsertTail: New MAX queued packet count = %d\n",
            TapPacketQueue->MaxCount));
    }

    KeReleaseSpinLock(&TapPacketQueue->QueueLock,irql);
}

// Call with QueueLock held
PTAP_PACKET
tapPacketRemoveHeadLocked(
    __in PTAP_PACKET_QUEUE  TapPacketQueue
    )
{
    PTAP_PACKET     tapPacket = NULL;
    PLIST_ENTRY     listEntry;

    listEntry = RemoveHeadList(&TapPacketQueue->Queue);

    if(listEntry != &TapPacketQueue->Queue)
    {
        tapPacket = CONTAINING_RECORD(listEntry, TAP_PACKET, QueueLink);

        // Update counts
        --TapPacketQueue->Count;
    }

    return tapPacket;
}

PTAP_PACKET
tapPacketRemoveHead(
    __in PTAP_PACKET_QUEUE  TapPacketQueue
    )
{
    PTAP_PACKET     tapPacket = NULL;
    KIRQL           irql;

    KeAcquireSpinLock(&TapPacketQueue->QueueLock,&irql);

    tapPacket = tapPacketRemoveHeadLocked(TapPacketQueue);

    KeReleaseSpinLock(&TapPacketQueue->QueueLock,irql);

    return tapPacket;
}

VOID
tapPacketQueueInitialize(
    __in PTAP_PACKET_QUEUE  TapPacketQueue
    )
{
    KeInitializeSpinLock(&TapPacketQueue->QueueLock);

    NdisInitializeListHead(&TapPacketQueue->Queue);
}

//======================================================================
// TAP Cancel-Safe Queue Support
//======================================================================

VOID
tapIrpCsqInsert (
    __in struct _IO_CSQ    *Csq,
    __in PIRP              Irp
    )
{
    PTAP_IRP_CSQ          tapIrpCsq;

    tapIrpCsq = (PTAP_IRP_CSQ )Csq;

    InsertTailList(
        &tapIrpCsq->Queue,
        &Irp->Tail.Overlay.ListEntry
        );

    // Update counts
    ++tapIrpCsq->Count;

    if(tapIrpCsq->Count > tapIrpCsq->MaxCount)
    {
        tapIrpCsq->MaxCount = tapIrpCsq->Count;

        DEBUGP (("[TAP] tapIrpCsqInsert: New MAX queued IRP count = %d\n",
            tapIrpCsq->MaxCount));
    }
}

VOID
tapIrpCsqRemoveIrp(
    __in PIO_CSQ Csq,
    __in PIRP    Irp
    )
{
    PTAP_IRP_CSQ          tapIrpCsq;

    tapIrpCsq = (PTAP_IRP_CSQ )Csq;

    // Update counts
    --tapIrpCsq->Count;

    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}


PIRP
tapIrpCsqPeekNextIrp(
    __in PIO_CSQ Csq,
    __in PIRP    Irp,
    __in PVOID   PeekContext
    )
{
    PTAP_IRP_CSQ          tapIrpCsq;
    PIRP                    nextIrp = NULL;
    PLIST_ENTRY             nextEntry;
    PLIST_ENTRY             listHead;
    PIO_STACK_LOCATION      irpStack;

    tapIrpCsq = (PTAP_IRP_CSQ )Csq;

    listHead = &tapIrpCsq->Queue;

    //
    // If the IRP is NULL, we will start peeking from the listhead, else
    // we will start from that IRP onwards. This is done under the
    // assumption that new IRPs are always inserted at the tail.
    //

    if (Irp == NULL)
    {
        nextEntry = listHead->Flink;
    }
    else
    {
        nextEntry = Irp->Tail.Overlay.ListEntry.Flink;
    }

    while(nextEntry != listHead)
    {
        nextIrp = CONTAINING_RECORD(nextEntry, IRP, Tail.Overlay.ListEntry);

        irpStack = IoGetCurrentIrpStackLocation(nextIrp);

        //
        // If context is present, continue until you find a matching one.
        // Else you break out as you got next one.
        //
        if (PeekContext)
        {
            if (irpStack->FileObject == (PFILE_OBJECT) PeekContext)
            {
                break;
            }
        }
        else
        {
            break;
        }

        nextIrp = NULL;
        nextEntry = nextEntry->Flink;
    }

    return nextIrp;
}

//
// tapIrpCsqAcquireQueueLock modifies the execution level of the current processor.
// 
// KeAcquireSpinLock raises the execution level to Dispatch Level and stores
// the current execution level in the Irql parameter to be restored at a later
// time.  KeAcqurieSpinLock also requires us to be running at no higher than
// Dispatch level when it is called.
//
// The annotations reflect these changes and requirments.
//

__drv_raisesIRQL(DISPATCH_LEVEL)
__drv_maxIRQL(DISPATCH_LEVEL)
VOID
tapIrpCsqAcquireQueueLock(
     __in PIO_CSQ Csq,
     __out PKIRQL  Irql
    )
{
    PTAP_IRP_CSQ          tapIrpCsq;

    tapIrpCsq = (PTAP_IRP_CSQ )Csq;

    //
    // Suppressing because the address below csq is valid since it's
    // part of TAP_ADAPTER_CONTEXT structure.
    //
#pragma prefast(suppress: __WARNING_BUFFER_UNDERFLOW, "Underflow using expression 'adapter->PendingReadCsqQueueLock'")
    KeAcquireSpinLock(&tapIrpCsq->QueueLock, Irql);
}

//
// tapIrpCsqReleaseQueueLock modifies the execution level of the current processor.
// 
// KeReleaseSpinLock assumes we already hold the spin lock and are therefore
// running at Dispatch level.  It will use the Irql parameter saved in a
// previous call to KeAcquireSpinLock to return the thread back to it's original
// execution level.
//
// The annotations reflect these changes and requirments.
//

__drv_requiresIRQL(DISPATCH_LEVEL)
VOID
tapIrpCsqReleaseQueueLock(
     __in PIO_CSQ Csq,
     __in KIRQL   Irql
    )
{
    PTAP_IRP_CSQ          tapIrpCsq;

    tapIrpCsq = (PTAP_IRP_CSQ )Csq;

    //
    // Suppressing because the address below csq is valid since it's
    // part of TAP_ADAPTER_CONTEXT structure.
    //
#pragma prefast(suppress: __WARNING_BUFFER_UNDERFLOW, "Underflow using expression 'adapter->PendingReadCsqQueueLock'")
    KeReleaseSpinLock(&tapIrpCsq->QueueLock, Irql);
}

VOID
tapIrpCsqCompleteCanceledIrp(
    __in  PIO_CSQ             pCsq,
    __in  PIRP                Irp
    )
{
    UNREFERENCED_PARAMETER(pCsq);

    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

VOID
tapIrpCsqInitialize(
    __in PTAP_IRP_CSQ  TapIrpCsq
    )
{
    KeInitializeSpinLock(&TapIrpCsq->QueueLock);

    NdisInitializeListHead(&TapIrpCsq->Queue);

    IoCsqInitialize(
        &TapIrpCsq->CsqQueue,
        tapIrpCsqInsert,
        tapIrpCsqRemoveIrp,
        tapIrpCsqPeekNextIrp,
        tapIrpCsqAcquireQueueLock,
        tapIrpCsqReleaseQueueLock,
        tapIrpCsqCompleteCanceledIrp
        );
}

VOID
tapIrpCsqFlush(
    __in PTAP_IRP_CSQ  TapIrpCsq
    )
{
    PIRP    pendingIrp;

    //
    // Flush the pending read IRP queue.
    //
    pendingIrp = IoCsqRemoveNextIrp(
                    &TapIrpCsq->CsqQueue,
                    NULL
                    );

    while(pendingIrp) 
    {
        // Cancel the IRP
        pendingIrp->IoStatus.Information = 0;
        pendingIrp->IoStatus.Status = STATUS_CANCELLED;
        IoCompleteRequest(pendingIrp, IO_NO_INCREMENT);

        pendingIrp = IoCsqRemoveNextIrp(
                        &TapIrpCsq->CsqQueue,
                        NULL
                        );
    }

    ASSERT(IsListEmpty(&TapIrpCsq->Queue));
}
