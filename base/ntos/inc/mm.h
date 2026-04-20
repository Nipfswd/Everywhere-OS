/*++

Copyright (c) 2026  The EverywhereOS Authors

Module Name:

    mm.h

Abstract:

    Public Memory Manager header. Declares physical page frame allocation,
    kernel pool (heap) allocation, and MM initialization routines.

Author:

    Noah Juopperi <nipfswd@gmail.com>
    Clay Sanders (made the first version of the kernel) <claylikepython@yahoo.com>

Environment:

    Kernel-mode only

--*/

#ifndef _MM_H_
#define _MM_H_

#include <stdint.h>
#include <stddef.h>

/* Page size: 4 KB */
#define PAGE_SIZE           4096
#define PAGE_SHIFT          12

/* Maximum physical memory we track: 64 MB */
#define MM_MAX_PHYSICAL     (64 * 1024 * 1024)
#define MM_MAX_PAGES        (MM_MAX_PHYSICAL / PAGE_SIZE)

/* Round address up/down to page boundary */
#define PAGE_ALIGN_UP(x)    (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(x)  ((x) & ~(PAGE_SIZE - 1))

/* Convert between page frame numbers and addresses */
#define PFN_TO_ADDR(pfn)    ((uint32_t)(pfn) << PAGE_SHIFT)
#define ADDR_TO_PFN(addr)   ((uint32_t)(addr) >> PAGE_SHIFT)

/* Pool types */
#define NonPagedPool        0
#define PagedPool           1

/* ********** Physical Page Frame Management ********** */

void     MmInitializePageFrameDatabase(uint32_t total_pages);
uint32_t MmAllocatePage(void);
void     MmFreePage(uint32_t pfn);
uint32_t MmGetFreePageCount(void);

/* ********** Pool (Heap) Allocation ********** */

void  MmInitializePool(uint32_t base, uint32_t size);
void* MmAllocatePool(int pool_type, uint32_t size);
void  MmFreePool(void* ptr);

/* ********** System Initialization ********** */

void MmInitSystem(uint32_t* mbi);

#endif /* _MM_H_ */
