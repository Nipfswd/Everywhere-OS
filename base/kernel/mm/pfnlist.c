/*++

Module Name:

    pfnlist.c

Abstract:

    PFN (Page Frame Number) database management. Maintains the free and
    zeroed page lists that back all physical page allocation.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "mi.h"

/*++

Routine Description:

    Returns a pointer to the MMPFN entry for a given physical frame.

Arguments:

    Pfn - Physical frame number to look up.

Return Value:

    Pointer to the corresponding MMPFN entry.

--*/

PMMPFN
MiGetPfn (
    PFN_NUMBER Pfn
    )
{
    return &MiState.PfnDatabase[Pfn - MiState.LowestPfn];
}

/*++

Routine Description:

    Appends a page to the tail of the free list.

Arguments:

    Pfn - Frame number to insert.

--*/

VOID
MiInsertPageFreeList (
    PFN_NUMBER Pfn
    )
{
    PMMPFN Entry;

    Entry         = MiGetPfn(Pfn);
    Entry->State  = MmPageStateFree;
    Entry->u1.Flink = PFN_INVALID;
    Entry->Blink    = MiState.FreeList.Tail;

    if (MiState.FreeList.Tail != PFN_INVALID) {
        MiGetPfn(MiState.FreeList.Tail)->u1.Flink = Pfn;
    } else {
        MiState.FreeList.Head = Pfn;
    }

    MiState.FreeList.Tail = Pfn;
    MiState.FreeList.Count++;
    MiState.FreePages++;
}

/*++

Routine Description:

    Appends a page to the tail of the zeroed list.

Arguments:

    Pfn - Frame number to insert. The caller must have zeroed the page.

--*/

VOID
MiInsertPageZeroedList (
    PFN_NUMBER Pfn
    )
{
    PMMPFN Entry;

    Entry           = MiGetPfn(Pfn);
    Entry->State    = MmPageStateZeroed;
    Entry->u1.Flink = PFN_INVALID;
    Entry->Blink    = MiState.ZeroedList.Tail;

    if (MiState.ZeroedList.Tail != PFN_INVALID) {
        MiGetPfn(MiState.ZeroedList.Tail)->u1.Flink = Pfn;
    } else {
        MiState.ZeroedList.Head = Pfn;
    }

    MiState.ZeroedList.Tail = Pfn;
    MiState.ZeroedList.Count++;
    MiState.FreePages++;
}

/*++

Routine Description:

    Removes and returns the head of the free list.

Return Value:

    PFN of the removed page, or PFN_INVALID if the list is empty.

--*/

PFN_NUMBER
MiRemovePageFreeList (
    VOID
    )
{
    PFN_NUMBER Head;
    PMMPFN     Entry;

    if (MiState.FreeList.Head == PFN_INVALID) {
        return PFN_INVALID;
    }

    Head  = MiState.FreeList.Head;
    Entry = MiGetPfn(Head);

    MiState.FreeList.Head = Entry->u1.Flink;

    if (MiState.FreeList.Head != PFN_INVALID) {
        MiGetPfn(MiState.FreeList.Head)->Blink = PFN_INVALID;
    } else {
        MiState.FreeList.Tail = PFN_INVALID;
    }

    MiState.FreeList.Count--;
    MiState.FreePages--;

    Entry->State      = MmPageStateActive;
    Entry->u1.Flink   = PFN_INVALID;
    Entry->Blink      = PFN_INVALID;
    Entry->ShareCount = 1;

    return Head;
}

/*++

Routine Description:

    Removes and returns the head of the zeroed list.

Return Value:

    PFN of the removed page, or PFN_INVALID if the list is empty.

--*/

PFN_NUMBER
MiRemovePageZeroedList (
    VOID
    )
{
    PFN_NUMBER Head;
    PMMPFN     Entry;

    if (MiState.ZeroedList.Head == PFN_INVALID) {
        return PFN_INVALID;
    }

    Head  = MiState.ZeroedList.Head;
    Entry = MiGetPfn(Head);

    MiState.ZeroedList.Head = Entry->u1.Flink;

    if (MiState.ZeroedList.Head != PFN_INVALID) {
        MiGetPfn(MiState.ZeroedList.Head)->Blink = PFN_INVALID;
    } else {
        MiState.ZeroedList.Tail = PFN_INVALID;
    }

    MiState.ZeroedList.Count--;
    MiState.FreePages--;

    Entry->State      = MmPageStateActive;
    Entry->u1.Flink   = PFN_INVALID;
    Entry->Blink      = PFN_INVALID;
    Entry->ShareCount = 1;

    return Head;
}