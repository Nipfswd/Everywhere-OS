/*++

Module Name:

    mmfault.c

Abstract:

    Page fault handler. On a #PF the CPU saves the faulting address in
    CR2 and vectors here. The handler either satisfies a demand-zero
    fault by allocating and mapping a new page, or kills the process
    for an invalid access.

Author:

    Noah Juopperi <nipfswd@gmail.com>

Environment:

    Called from the IDT #PF stub with interrupts disabled.

--*/

#include "mi.h"

/*
 * x86 #PF error code bits
 */
#define PF_ERR_PRESENT  0x01    /* 0 = not-present, 1 = protection          */
#define PF_ERR_WRITE    0x02    /* 0 = read,        1 = write               */
#define PF_ERR_USER     0x04    /* 0 = kernel,      1 = user mode           */

/*++

Routine Description:

    Reads CR2 to obtain the faulting virtual address.

--*/

static ULONG_PTR
MiReadCr2 (
    VOID
    )
{
    ULONG_PTR Value;

    __asm__ volatile ("mov %%cr2, %0" : "=r"(Value));
    return Value;
}

/*++

Routine Description:

    Maps a newly allocated physical page at VirtualAddress in the current
    page directory. Allocates a page table if none exists for this PDE.

Arguments:

    VirtualAddress - Target virtual address (page-aligned).
    Pfn            - Physical frame to map.
    PteFlags       - Hardware PTE flags (MM_PTE_*) to apply.

Return Value:

    TRUE if the mapping was established, FALSE on allocation failure.

--*/

static INT
MiMapPage (
    ULONG_PTR      VirtualAddress,
    PFN_NUMBER     Pfn,
    MMPTE_HARDWARE PteFlags
    )
{
    MMPTE_HARDWARE *PdePtr;
    MMPTE_HARDWARE  Pde;
    MMPTE_HARDWARE *PageTable;
    PFN_NUMBER      PtPfn;

    PdePtr = &MiState.PageDirectory[PDE_INDEX(VirtualAddress)];
    Pde    = *PdePtr;

    if (!(Pde & MM_PTE_PRESENT)) {
        /* Allocate a new page table */
        PtPfn = MiAllocateZeroedPage();
        if (PtPfn == PFN_INVALID) {
            return FALSE;
        }

        *PdePtr = (PFN_TO_ADDRESS(PtPfn) & MM_PTE_FRAME_MASK) |
                  MM_PTE_PRESENT | MM_PTE_WRITE | MM_PTE_USER;
        Pde     = *PdePtr;
    }

    PageTable = (MMPTE_HARDWARE *)(Pde & MM_PTE_FRAME_MASK);
    PageTable[PTE_INDEX(VirtualAddress)] =
        (PFN_TO_ADDRESS(Pfn) & MM_PTE_FRAME_MASK) |
        PteFlags                                   |
        MM_PTE_PRESENT;

    return TRUE;
}

/*++

Routine Description:

    Page fault dispatch routine. Called by the #PF IDT handler.

Arguments:

    ErrorCode - x86 page fault error code pushed by the CPU.

--*/

VOID
MmPageFault (
    ULONG ErrorCode
    )
{
    ULONG_PTR FaultAddress;
    ULONG_PTR FaultVpn;
    PMM_VAD   Vad;
    PFN_NUMBER Pfn;
    MMPTE_HARDWARE PteFlags;

    FaultAddress = MiReadCr2();
    FaultVpn     = ADDRESS_TO_PFN(PAGE_ALIGN(FaultAddress));

    /* Protection fault — no VAD can fix a present-page violation */
    if (ErrorCode & PF_ERR_PRESENT) {
        goto AccessViolation;
    }

    /* Look up the faulting address in the kernel VAD tree.
     * A real kernel would choose user vs kernel tree based on the address. */
    Vad = MiFindVad(MiState.KernelVadRoot, FaultVpn);
    if (Vad == NULL) {
        goto AccessViolation;
    }

    /* Write to a read-only region */
    if ((ErrorCode & PF_ERR_WRITE) &&
        !(Vad->Protection & MM_PROT_WRITE)) {
        goto AccessViolation;
    }

    /* Demand-zero: allocate and map a zeroed page */
    Pfn = MiAllocateZeroedPage();
    if (Pfn == PFN_INVALID) {
        /* Out of memory — treat as fatal for now */
        goto AccessViolation;
    }

    /* Build PTE flags from the VAD protection */
    PteFlags = MiProtectionToPteFlags(Vad->Protection);

    if (!MiMapPage(PAGE_ALIGN(FaultAddress), Pfn, PteFlags)) {
        MiFreePage(Pfn);
        goto AccessViolation;
    }

    /* Update the PFN record with the mapping address */
    MiGetPfn(Pfn)->u1.MappedVa = PAGE_ALIGN(FaultAddress);
    return;

AccessViolation:
    /*
     * Unhandled fault. A real kernel would raise an exception or
     * terminate the offending process. For now, halt.
     */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}