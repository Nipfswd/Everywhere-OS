/*++

Module Name:

    mmsup.c

Abstract:

    Miscellaneous memory manager support routines: page zeroing,
    TLB invalidation, and temporary physical-to-virtual mapping.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "mi.h"

/*++

Routine Description:

    Zeroes one page at the given virtual address.

Arguments:

    VirtualAddress - Page-aligned virtual address to zero.

--*/

VOID
MiZeroPage (
    ULONG_PTR VirtualAddress
    )
{
    ULONG *p;
    ULONG  i;

    p = (ULONG *)VirtualAddress;

    for (i = 0; i < MM_PAGE_SIZE / sizeof(ULONG); i++) {
        p[i] = 0;
    }
}

/*++

Routine Description:

    Flushes the TLB entry for a single virtual address using INVLPG.

Arguments:

    VirtualAddress - Address whose TLB entry is to be invalidated.

--*/

VOID
MiFlushTlbEntry (
    ULONG_PTR VirtualAddress
    )
{
    __asm__ volatile ("invlpg (%0)" : : "r"(VirtualAddress) : "memory");
}

/*++

Routine Description:

    Flushes the entire TLB by reloading CR3 with its current value.

--*/

VOID
MiFlushTlb (
    VOID
    )
{
    ULONG_PTR Cr3;

    __asm__ volatile (
        "mov %%cr3, %0  \n\t"
        "mov %0, %%cr3  \n\t"
        : "=r"(Cr3)
        :
        : "memory"
    );
}

/*++

Routine Description:

    Returns the virtual address at which a physical page is accessible.

    In this kernel physical memory is identity-mapped in the region
    [0x00100000, end-of-RAM), so the virtual address is the same as the
    physical address. A more sophisticated implementation would maintain
    a dedicated hyperspace-style window.

Arguments:

    Pfn - Physical frame number.

Return Value:

    Virtual address that maps the frame.

--*/

ULONG_PTR
MiMapPhysicalPage (
    PFN_NUMBER Pfn
    )
{
    return PFN_TO_ADDRESS(Pfn);
}