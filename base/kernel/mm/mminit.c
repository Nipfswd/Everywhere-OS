/*++

Module Name:

    mminit.c

Abstract:

    Memory manager initialisation. Parses the multiboot memory map
    provided by GRUB to discover usable physical RAM, builds the PFN
    database over that range, and inserts all usable pages onto the
    free list, skipping regions occupied by the kernel image and the
    PFN database itself.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "mi.h"
#include "../inc/multiboot.h"
#include "../inc/video.h"

MI_STATE MiState;

extern uint8_t __start[];
extern uint8_t __end[];

#define PFN_DB_MAX_PAGES 32768

static MMPFN MiPfnPool[PFN_DB_MAX_PAGES];

static uint32_t
MiFloorPage (
    uint64_t Address
    )
{
    return (uint32_t)(Address & ~(uint64_t)(MM_PAGE_SIZE - 1));
}

static uint32_t
MiCeilPage (
    uint64_t Address
    )
{
    return (uint32_t)((Address + MM_PAGE_SIZE - 1) & ~(uint64_t)(MM_PAGE_SIZE - 1));
}

static VOID
MiInsertRangeFreeList (
    uint32_t RangeStart,
    uint32_t RangeEnd
    )
{
    uint32_t   Va;
    PFN_NUMBER Pfn;
    PMMPFN     Entry;

    for (Va = RangeStart; Va < RangeEnd; Va += MM_PAGE_SIZE) {
        Pfn = ADDRESS_TO_PFN(Va);

        if (Pfn < MiState.LowestPfn || Pfn > MiState.HighestPfn) {
            continue;
        }

        Entry             = MiGetPfn(Pfn);
        Entry->State      = MmPageStateFree;
        Entry->ShareCount = 0;
        Entry->Flags      = 0;
        Entry->u1.Flink   = PFN_INVALID;
        Entry->Blink      = PFN_INVALID;

        MiInsertPageFreeList(Pfn);
    }
}

static VOID
MiAddUsableRegion (
    uint32_t RegionStart,
    uint32_t RegionEnd
    )
{
    uint32_t KernStart;
    uint32_t KernEnd;
    uint32_t DbStart;
    uint32_t DbEnd;
    uint32_t SegStart;
    uint32_t SegEnd;

    KernStart = MiFloorPage((uint64_t)(uint32_t)__start);
    KernEnd   = MiCeilPage((uint64_t)(uint32_t)__end);
    DbStart   = MiFloorPage((uint64_t)(uint32_t)MiPfnPool);
    DbEnd     = MiCeilPage((uint64_t)((uint32_t)MiPfnPool + sizeof(MiPfnPool)));

    SegStart = RegionStart;

    while (SegStart < RegionEnd) {
        SegEnd = RegionEnd;

        if (SegStart < KernEnd && SegEnd > KernStart) {
            if (SegStart < KernStart) {
                MiInsertRangeFreeList(SegStart, KernStart);
            }
            SegStart = KernEnd;
            continue;
        }

        if (SegStart < DbEnd && SegEnd > DbStart) {
            if (SegStart < DbStart) {
                MiInsertRangeFreeList(SegStart, DbStart);
            }
            SegStart = DbEnd;
            continue;
        }

        MiInsertRangeFreeList(SegStart, SegEnd);
        break;
    }
}

VOID
MmInitialize (
    MULTIBOOT_INFO *MbInfo
    )
{
    MULTIBOOT_MMAP_ENTRY *Entry;
    uint32_t              MmapEnd;
    uint64_t              TotalBytes;
    uint32_t              i;

    if (MbInfo == NULL) {
        Print("MM: null multiboot info pointer\n");
        for (;;) { __asm__ volatile ("cli; hlt"); }
    }

    if (!(MbInfo->Flags & MULTIBOOT_FLAG_MMAP)) {
        Print("MM: GRUB did not provide memory map\n");
        for (;;) { __asm__ volatile ("cli; hlt"); }
    }

    MiState.LowestPfn  = PFN_INVALID;
    MiState.HighestPfn = 0;
    TotalBytes         = 0;

    MmapEnd = MbInfo->MmapAddress + MbInfo->MmapLength;

    Entry = (MULTIBOOT_MMAP_ENTRY *)MbInfo->MmapAddress;
    while ((uint32_t)Entry < MmapEnd) {
        if (Entry->Type    == MULTIBOOT_MMAP_AVAILABLE &&
            Entry->BaseAddress < 0x100000000ULL) {

            uint64_t ClampedEnd;
            uint32_t Start32;
            uint32_t End32;

            ClampedEnd = Entry->BaseAddress + Entry->Length;
            if (ClampedEnd > 0x100000000ULL) {
                ClampedEnd = 0x100000000ULL;
            }

            Start32 = MiCeilPage(Entry->BaseAddress);
            End32   = MiFloorPage(ClampedEnd);

            if (End32 > Start32) {
                PFN_NUMBER StartPfn = ADDRESS_TO_PFN(Start32);
                PFN_NUMBER EndPfn   = ADDRESS_TO_PFN(End32) - 1;

                if (MiState.LowestPfn == PFN_INVALID ||
                    StartPfn < MiState.LowestPfn) {
                    MiState.LowestPfn = StartPfn;
                }
                if (EndPfn > MiState.HighestPfn) {
                    MiState.HighestPfn = EndPfn;
                }

                TotalBytes += (End32 - Start32);
            }
        }

        Entry = (MULTIBOOT_MMAP_ENTRY *)((uint32_t)Entry + Entry->Size + 4);
    }

    if (MiState.LowestPfn == PFN_INVALID) {
        Print("MM: no usable memory regions found\n");
        for (;;) { __asm__ volatile ("cli; hlt"); }
    }

    MiState.TotalPages = (uint32_t)(TotalBytes / MM_PAGE_SIZE);

    if (MiState.TotalPages > PFN_DB_MAX_PAGES) {
        MiState.TotalPages = PFN_DB_MAX_PAGES;
        MiState.HighestPfn = MiState.LowestPfn + PFN_DB_MAX_PAGES - 1;
    }

    MiState.PfnDatabase = MiPfnPool;

    for (i = 0; i < PFN_DB_MAX_PAGES; i++) {
        MiPfnPool[i].State      = MmPageStateBad;
        MiPfnPool[i].ShareCount = 0;
        MiPfnPool[i].Flags      = 0;
        MiPfnPool[i].u1.Flink   = PFN_INVALID;
        MiPfnPool[i].Blink      = PFN_INVALID;
    }

    MiState.FreeList.Head    = PFN_INVALID;
    MiState.FreeList.Tail    = PFN_INVALID;
    MiState.FreeList.Count   = 0;
    MiState.ZeroedList.Head  = PFN_INVALID;
    MiState.ZeroedList.Tail  = PFN_INVALID;
    MiState.ZeroedList.Count = 0;
    MiState.FreePages        = 0;
    MiState.KernelVadRoot    = NULL;
    MiState.Flags            = 0;

    Entry = (MULTIBOOT_MMAP_ENTRY *)MbInfo->MmapAddress;
    while ((uint32_t)Entry < MmapEnd) {
        if (Entry->Type    == MULTIBOOT_MMAP_AVAILABLE &&
            Entry->BaseAddress < 0x100000000ULL) {

            uint64_t ClampedEnd;
            uint32_t Start32;
            uint32_t End32;

            ClampedEnd = Entry->BaseAddress + Entry->Length;
            if (ClampedEnd > 0x100000000ULL) {
                ClampedEnd = 0x100000000ULL;
            }

            Start32 = MiCeilPage(Entry->BaseAddress);
            End32   = MiFloorPage(ClampedEnd);

            if (End32 > Start32) {
                MiAddUsableRegion(Start32, End32);
            }
        }

        Entry = (MULTIBOOT_MMAP_ENTRY *)((uint32_t)Entry + Entry->Size + 4);
    }

    MiState.Flags |= MI_FLAG_INITIALIZED;
}