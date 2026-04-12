/*++

Module Name:

    allocvm.c

Abstract:

    Virtual memory reservation. Locates a free virtual address range and
    creates a VAD to describe it. Physical pages are not committed here;
    that happens on first access via the page fault handler.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "mi.h"

ULONG_PTR
MiReserveVirtualRange (
    PMM_VAD  **VadRoot,
    ULONG_PTR  StartHint,
    ULONG      PageCount,
    UCHAR      Protection
    )
{
    ULONG_PTR CandidateVpn;
    ULONG_PTR EndVpn;
    PMM_VAD   Conflict;
    PMM_VAD   Vad;

    if (VadRoot == NULL || PageCount == 0) {
        return 0;
    }

    CandidateVpn = ADDRESS_TO_PFN(PAGE_ALIGN(StartHint));

    if (CandidateVpn == 0) {
        CandidateVpn = ADDRESS_TO_PFN(MM_USER_SPACE_START);
    }

    for (;;) {
        EndVpn = CandidateVpn + PageCount - 1;

        if (EndVpn < CandidateVpn ||
            PFN_TO_ADDRESS(EndVpn) > MM_USER_SPACE_END) {
            return 0;
        }

        Conflict = MiFindVadRange(*VadRoot, CandidateVpn, EndVpn);

        if (Conflict == NULL) {
            break;
        }

        CandidateVpn = Conflict->EndVpn + 1;
    }

    Vad = MiAllocateVad();
    if (Vad == NULL) {
        return 0;
    }

    Vad->StartVpn   = CandidateVpn;
    Vad->EndVpn     = EndVpn;
    Vad->Protection = Protection;
    Vad->Flags      = MMVAD_FLAG_PRIVATE;

    MiInsertVad(VadRoot, Vad);

    return PFN_TO_ADDRESS(CandidateVpn);
}