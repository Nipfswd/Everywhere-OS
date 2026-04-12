/*++

Module Name:

    protect.c

Abstract:

    Page protection management. Translates MM_PROT_* flags to hardware
    PTE bits, and updates existing page table entries to change the
    protection of a mapped page.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "mi.h"

/*++

Routine Description:

    Converts MM_PROT_* protection flags to the corresponding x86 PTE
    hardware bits.

Arguments:

    Protection - MM_PROT_* flags.

Return Value:

    Bitmask of MM_PTE_* hardware flags.

--*/

UCHAR
MiProtectionToPteFlags (
    UCHAR Protection
    )
{
    UCHAR Flags;

    Flags = 0;

    if (Protection == MM_PROT_NONE) {
        return Flags;   /* No bits set — not-present */
    }

    /* Read is implied by any non-NONE protection on x86 */
    Flags |= (UCHAR)MM_PTE_PRESENT;

    if (Protection & MM_PROT_WRITE) {
        Flags |= (UCHAR)MM_PTE_WRITE;
    }

    /* User mode accessible unless the kernel flag is set */
    if (!(Protection & MM_PROT_KERNEL)) {
        Flags |= (UCHAR)MM_PTE_USER;
    }

    /*
     * x86-32 without NX has no execute-disable bit.
     * MM_PROT_EXEC is recorded in the VAD but has no PTE representation
     * here.  An NX-capable implementation would set bit 63 of the PTE
     * when exec is not requested.
     */

    return Flags;
}

/*++

Routine Description:

    Changes the hardware protection of a single mapped page in-place.
    The page must already be present; this routine does not handle
    not-present PTEs.

Arguments:

    VirtualAddress - Any address within the target page.
    Protection     - New MM_PROT_* flags.

--*/

VOID
MiSetPageProtection (
    ULONG_PTR VirtualAddress,
    UCHAR     Protection
    )
{
    MMPTE_HARDWARE *PdePtr;
    MMPTE_HARDWARE  Pde;
    MMPTE_HARDWARE *PtePtr;
    MMPTE_HARDWARE  Pte;
    MMPTE_HARDWARE  NewPte;
    UCHAR           HwFlags;

    PdePtr = &MiState.PageDirectory[PDE_INDEX(VirtualAddress)];
    Pde    = *PdePtr;

    if (!(Pde & MM_PTE_PRESENT)) {
        return;
    }

    PtePtr = (MMPTE_HARDWARE *)((Pde & MM_PTE_FRAME_MASK) +
                                PTE_INDEX(VirtualAddress) *
                                sizeof(MMPTE_HARDWARE));
    Pte = *PtePtr;

    if (!(Pte & MM_PTE_PRESENT)) {
        return;
    }

    HwFlags = MiProtectionToPteFlags(Protection);

    /* Preserve the physical frame address; replace protection bits */
    NewPte   = (Pte & MM_PTE_FRAME_MASK) | (MMPTE_HARDWARE)HwFlags;
    *PtePtr  = NewPte;

    MiFlushTlbEntry(PAGE_ALIGN(VirtualAddress));
}