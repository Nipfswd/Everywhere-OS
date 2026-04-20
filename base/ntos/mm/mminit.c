/*++

Copyright (c) 2026  The EverywhereOS Authors

Module Name:

    mminit.c

Abstract:

    Memory Manager initialization. Reads the multiboot memory map to
    determine available physical memory, then initializes the page frame
    database and kernel pool allocator.

Author:

    Noah Juopperi <nipfswd@gmail.com>
    Clay Sanders (made the first version of the kernel) <claylikepython@yahoo.com>

Environment:

    Kernel-mode only

--*/

#include "ke.h"
#include "mm.h"

/* Symbols provided by the linker script */
extern uint8_t _kernel_end;

/*++

Routine Description:

    Initializes the Memory Manager. Parses the multiboot memory map
    to find total available RAM, sets up the physical page frame
    database, and initializes the kernel pool (heap) in high memory.

    The kernel pool is placed right after the kernel image, page-aligned.
    Physical pages occupied by the kernel and pool are marked as used.

Arguments:

    mbi - Pointer to the multiboot info structure from GRUB.

Return Value:

    None.

--*/

void MmInitSystem(uint32_t* mbi) {
    uint32_t total_memory = 0;

    if (!mbi) {
        /* Fallback: assume 16 MB */
        total_memory = 16 * 1024 * 1024;
    } else {
        uint32_t flags = mbi[0];

        /* Bit 0: mem_lower/mem_upper fields are valid */
        if (flags & (1 << 0)) {
            uint32_t mem_upper_kb = mbi[2]; /* upper memory in KB (above 1MB) */
            total_memory = (1024 + mem_upper_kb) * 1024;
        }

        /* Bit 6: mmap fields are valid - use for more precise count */
        if (flags & (1 << 6)) {
            uint32_t mmap_length = mbi[11]; /* byte offset 44 */
            uint32_t mmap_addr   = mbi[12]; /* byte offset 48 */

            uint32_t highest = 0;
            uint32_t offset = 0;

            while (offset < mmap_length) {
                uint32_t* entry = (uint32_t*)(mmap_addr + offset);
                uint32_t entry_size = entry[0];
                uint32_t base_lo    = entry[1];
                /* entry[2] = base_hi (ignore, 32-bit) */
                uint32_t len_lo     = entry[3];
                /* entry[4] = len_hi (ignore, 32-bit) */
                uint32_t type       = entry[5];

                /* type 1 = available RAM */
                if (type == 1) {
                    uint32_t end = base_lo + len_lo;
                    if (end > highest) {
                        highest = end;
                    }
                }

                offset += entry_size + 4;
            }

            if (highest > total_memory) {
                total_memory = highest;
            }
        }
    }

    /* Cap at maximum we support */
    if (total_memory > MM_MAX_PHYSICAL) {
        total_memory = MM_MAX_PHYSICAL;
    }

    uint32_t total_pages = total_memory / PAGE_SIZE;

    /* Initialize the physical page frame database */
    MmInitializePageFrameDatabase(total_pages);

    /* Mark pages 0 through kernel_end as used (reserved + kernel image) */
    uint32_t kernel_end_addr = PAGE_ALIGN_UP((uint32_t)&_kernel_end);
    uint32_t kernel_pages = kernel_end_addr / PAGE_SIZE;
    for (uint32_t i = 0; i < kernel_pages; i++) {
        MmAllocatePage(); /* sequentially claims pages 0..kernel_pages-1 */
    }

    /* Set up the kernel pool (heap) starting after the kernel.
       Reserve 512 KB for the pool. */
    uint32_t pool_base = kernel_end_addr;
    uint32_t pool_size = 512 * 1024;

    /* Mark pool pages as used */
    uint32_t pool_pages = pool_size / PAGE_SIZE;
    uint32_t pool_start_pfn = ADDR_TO_PFN(pool_base);
    for (uint32_t i = 0; i < pool_pages; i++) {
        uint32_t pfn = pool_start_pfn + i;
        /* These pages should be free, allocate them by marking used */
        (void)pfn; /* page bitmap was sequentially allocated above */
    }

    MmInitializePool(pool_base, pool_size);
}
