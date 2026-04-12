/*++

Module Name:

    freevm.c

Abstract:

    Releases a reserved or committed virtual address region. Unmaps any
    committed pages, frees the backing physical frames, and removes the
    VAD from the address space tree.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "mi.h"

VOID
MiFreeVirtualRange (
    PMM_VAD  **VadRoot,
    ULONG_PTR  StartVpn,
    ULONG_PTR  EndVpn
    )
{
    PMM_VAD        Vad;
    ULONG_PTR      Vpn;
    ULONG_PTR      Va;
    MMPTE_HARDWARE Pde;
    MMPTE_HARDWARE Pte;
    MMPTE_HARDWARE *PdePtr;
    MMPTE_HARDWARE *PtePtr;
    PFN_NUMBER      Pfn;

    if (VadRoot == NULL) {
        return;
    }

    Vad = MiFindVad(*VadRoot, StartVpn);
    if (Vad == NULL) {
        return;
    }

    for (Vpn = StartVpn; Vpn <= EndVpn; Vpn++) {
        Va     = PFN_TO_ADDRESS(Vpn);
        PdePtr = &MiState.PageDirectory[PDE_INDEX(Va)];
        Pde    = *PdePtr;

        if (!(Pde & MM_PTE_PRESENT)) {
            continue;
        }

        PtePtr = (MMPTE_HARDWARE *)((Pde & MM_PTE_FRAME_MASK) +
                                    PTE_INDEX(Va) * sizeof(MMPTE_HARDWARE));
        Pte    = *PtePtr;

        if (!(Pte & MM_PTE_PRESENT)) {
            continue;
        }

        Pfn     = ADDRESS_TO_PFN(Pte & MM_PTE_FRAME_MASK);
        *PtePtr = 0;
        MiFlushTlbEntry(Va);
        MiFreePage(Pfn);
    }

    MiRemoveVad(VadRoot, Vad);
    MiFreeVad(Vad);
}