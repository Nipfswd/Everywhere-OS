/*++

Module Name:

    queryvm.c

Abstract:

    Virtual memory query routines. Returns information about a given
    virtual address — which VAD covers it, its protection, and whether
    the page is currently committed.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "mi.h"
#include "../inc/mm.h"

INT
MmQueryVirtualMemory (
    PMM_VAD         VadRoot,
    ULONG_PTR       Address,
    PMM_MEMORY_INFO Info
    )
{
    ULONG_PTR       Vpn;
    PMM_VAD         Vad;
    MMPTE_HARDWARE *PdePtr;
    MMPTE_HARDWARE  Pde;
    MMPTE_HARDWARE *PtePtr;
    MMPTE_HARDWARE  Pte;

    if (Info == NULL) {
        return FALSE;
    }

    {
        UCHAR *p = (UCHAR *)Info;
        ULONG  i;
        for (i = 0; i < sizeof(*Info); i++) {
            p[i] = 0;
        }
    }

    Vpn = ADDRESS_TO_PFN(PAGE_ALIGN(Address));
    Vad = MiFindVad(VadRoot, Vpn);

    if (Vad == NULL) {
        Info->State = MmRegionFree;
        return FALSE;
    }

    Info->BaseAddress = PFN_TO_ADDRESS(Vad->StartVpn);
    Info->RegionSize  = (Vad->EndVpn - Vad->StartVpn + 1) * MM_PAGE_SIZE;
    Info->Protection  = Vad->Protection;
    Info->State       = (Vad->Flags & MMVAD_FLAG_COMMITTED)
                            ? MmRegionCommitted
                            : MmRegionReserved;

    PdePtr = &MiState.PageDirectory[PDE_INDEX(Address)];
    Pde    = *PdePtr;

    if (Pde & MM_PTE_PRESENT) {
        PtePtr = (MMPTE_HARDWARE *)((Pde & MM_PTE_FRAME_MASK) +
                                    PTE_INDEX(Address) *
                                    sizeof(MMPTE_HARDWARE));
        Pte = *PtePtr;

        Info->PagePresent = (Pte & MM_PTE_PRESENT) ? TRUE : FALSE;
    }

    return TRUE;
}