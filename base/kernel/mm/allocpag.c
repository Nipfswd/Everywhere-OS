/*++

Module Name:

    allocpag.c

Abstract:

    Physical page allocation and deallocation. Pages are drawn from the
    zeroed list first (preferred for security) then the free list.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "mi.h"

/*++

Routine Description:

    Allocates one physical page from the zeroed list. If the zeroed list
    is empty, allocates from the free list and zeros the page before
    returning it.

Return Value:

    PFN of the allocated page, or PFN_INVALID if no memory is available.

--*/

PFN_NUMBER
MiAllocateZeroedPage (
    VOID
    )
{
    PFN_NUMBER Pfn;

    /* Prefer a pre-zeroed page */
    Pfn = MiRemovePageZeroedList();
    if (Pfn != PFN_INVALID) {
        return Pfn;
    }

    /* Fall back to free list, zero it ourselves */
    Pfn = MiRemovePageFreeList();
    if (Pfn == PFN_INVALID) {
        return PFN_INVALID;
    }

    MiZeroPage(PFN_TO_ADDRESS(Pfn));
    return Pfn;
}

/*++

Routine Description:

    Allocates one physical page. The page content is not zeroed; callers
    that require zeroed memory should use MiAllocateZeroedPage.

Return Value:

    PFN of the allocated page, or PFN_INVALID if no memory is available.

--*/

PFN_NUMBER
MiAllocatePage (
    VOID
    )
{
    PFN_NUMBER Pfn;

    Pfn = MiRemovePageFreeList();
    if (Pfn != PFN_INVALID) {
        return Pfn;
    }

    /* Drain the zeroed list as a last resort */
    return MiRemovePageZeroedList();
}

/*++

Routine Description:

    Returns a physical page to the free list and decrements its share
    count. The page is not zeroed; it will be zeroed lazily by the
    zero-page thread before being placed on the zeroed list.

Arguments:

    Pfn - Frame number to free. Must currently be in the Active state.

--*/

VOID
MiFreePage (
    PFN_NUMBER Pfn
    )
{
    PMMPFN Entry;

    Entry = MiGetPfn(Pfn);

    if (Entry->ShareCount > 1) {
        Entry->ShareCount--;
        return;
    }

    Entry->ShareCount = 0;
    MiInsertPageFreeList(Pfn);
}