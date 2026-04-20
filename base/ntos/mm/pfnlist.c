/*++

Copyright (c) 2026  The EverywhereOS Authors

Module Name:

    pfnlist.c

Abstract:

    Physical page frame number (PFN) database. Manages a bitmap of
    physical pages to track which 4 KB frames are free or in use.

Author:

    Noah Juopperi <nipfswd@gmail.com>
    Clay Sanders (made the first version of the kernel) <claylikepython@yahoo.com>

Environment:

    Kernel-mode only

--*/

#include "mm.h"

/* Bitmap: 1 bit per page. 0 = free, 1 = used.
   For 64 MB (16384 pages) we need 16384/8 = 2048 bytes. */
static uint8_t MmPfnBitmap[MM_MAX_PAGES / 8];
static uint32_t MmTotalPages;
static uint32_t MmFreePages;
static uint32_t MmNextSearchStart;

/*++

Routine Description:

    Initializes the page frame database. All pages start as free.

Arguments:

    total_pages - Total number of physical pages available.

Return Value:

    None.

--*/

void MmInitializePageFrameDatabase(uint32_t total_pages) {
    if (total_pages > MM_MAX_PAGES) {
        total_pages = MM_MAX_PAGES;
    }

    MmTotalPages = total_pages;
    MmFreePages = total_pages;
    MmNextSearchStart = 0;

    /* Clear bitmap: all pages free */
    for (uint32_t i = 0; i < sizeof(MmPfnBitmap); i++) {
        MmPfnBitmap[i] = 0;
    }

    /* Mark pages beyond total_pages as used (unavailable) */
    for (uint32_t pfn = total_pages; pfn < MM_MAX_PAGES; pfn++) {
        MmPfnBitmap[pfn / 8] |= (1 << (pfn % 8));
    }
}

/*++

Routine Description:

    Allocates a single physical page frame. Scans the bitmap for the
    first free page, marks it used, and returns its PFN.

Arguments:

    None.

Return Value:

    The page frame number of the allocated page, or 0xFFFFFFFF if
    no free pages are available.

--*/

uint32_t MmAllocatePage(void) {
    if (MmFreePages == 0) {
        return 0xFFFFFFFF;
    }

    for (uint32_t i = 0; i < MmTotalPages; i++) {
        uint32_t pfn = (MmNextSearchStart + i) % MmTotalPages;
        uint32_t byte_idx = pfn / 8;
        uint8_t  bit_mask = 1 << (pfn % 8);

        if (!(MmPfnBitmap[byte_idx] & bit_mask)) {
            MmPfnBitmap[byte_idx] |= bit_mask;
            MmFreePages--;
            MmNextSearchStart = (pfn + 1) % MmTotalPages;
            return pfn;
        }
    }

    return 0xFFFFFFFF;
}

/*++

Routine Description:

    Frees a previously allocated physical page frame.

Arguments:

    pfn - The page frame number to free.

Return Value:

    None.

--*/

void MmFreePage(uint32_t pfn) {
    if (pfn >= MmTotalPages) {
        return;
    }

    uint32_t byte_idx = pfn / 8;
    uint8_t  bit_mask = 1 << (pfn % 8);

    if (MmPfnBitmap[byte_idx] & bit_mask) {
        MmPfnBitmap[byte_idx] &= ~bit_mask;
        MmFreePages++;
    }
}

/*++

Routine Description:

    Returns the number of free physical pages.

Arguments:

    None.

Return Value:

    Count of free pages.

--*/

uint32_t MmGetFreePageCount(void) {
    return MmFreePages;
}
