/*++

Copyright (c) 2026  The EverywhereOS Authors

Module Name:

    allocpag.c

Abstract:

    Kernel pool (heap) allocator. Provides MmAllocatePool and MmFreePool
    for dynamic memory allocation within the kernel. Uses a simple
    first-fit free list with block coalescing.

Author:

    Noah Juopperi <nipfswd@gmail.com>
    Clay Sanders (made the first version of the kernel) <claylikepython@yahoo.com>

Environment:

    Kernel-mode only

--*/

#include "mm.h"

/* Block header placed before every allocation */
typedef struct pool_block {
    uint32_t             size;    /* size of data area (excluding header) */
    int                  free;    /* 1 = free, 0 = allocated */
    struct pool_block*   next;    /* next block in the list */
} POOL_BLOCK;

#define BLOCK_HEADER_SIZE  (sizeof(POOL_BLOCK))
#define MIN_BLOCK_SIZE     16

static POOL_BLOCK* MmPoolHead;
static uint32_t    MmPoolBase;
static uint32_t    MmPoolSize;

/*++

Routine Description:

    Initializes the kernel pool allocator with a contiguous region
    of memory.

Arguments:

    base - Physical/virtual address of the pool region.
    size - Size of the pool region in bytes.

Return Value:

    None.

--*/

void MmInitializePool(uint32_t base, uint32_t size) {
    MmPoolBase = base;
    MmPoolSize = size;

    /* Create one big free block spanning the entire pool */
    MmPoolHead = (POOL_BLOCK*)base;
    MmPoolHead->size = size - BLOCK_HEADER_SIZE;
    MmPoolHead->free = 1;
    MmPoolHead->next = (POOL_BLOCK*)0;
}

/*++

Routine Description:

    Allocates a block of memory from the kernel pool. Uses first-fit
    strategy. Splits blocks when the remainder is large enough.

Arguments:

    pool_type - Pool type (NonPagedPool or PagedPool). Currently
                only NonPagedPool is supported; parameter is accepted
                for API compatibility.

    size - Number of bytes to allocate.

Return Value:

    Pointer to the allocated memory, or NULL if the pool is exhausted.

--*/

void* MmAllocatePool(int pool_type, uint32_t size) {
    (void)pool_type;

    if (size == 0) {
        return (void*)0;
    }

    /* Align size to 4 bytes */
    size = (size + 3) & ~3;

    POOL_BLOCK* block = MmPoolHead;

    while (block) {
        if (block->free && block->size >= size) {
            /* Split if remainder is large enough for another block */
            if (block->size >= size + BLOCK_HEADER_SIZE + MIN_BLOCK_SIZE) {
                POOL_BLOCK* new_block = (POOL_BLOCK*)((uint8_t*)block +
                                         BLOCK_HEADER_SIZE + size);
                new_block->size = block->size - size - BLOCK_HEADER_SIZE;
                new_block->free = 1;
                new_block->next = block->next;

                block->size = size;
                block->next = new_block;
            }

            block->free = 0;
            return (void*)((uint8_t*)block + BLOCK_HEADER_SIZE);
        }

        block = block->next;
    }

    /* Out of memory */
    return (void*)0;
}

/*++

Routine Description:

    Frees a previously allocated pool block. Coalesces adjacent free
    blocks to reduce fragmentation.

Arguments:

    ptr - Pointer previously returned by MmAllocatePool.

Return Value:

    None.

--*/

void MmFreePool(void* ptr) {
    if (!ptr) {
        return;
    }

    POOL_BLOCK* block = (POOL_BLOCK*)((uint8_t*)ptr - BLOCK_HEADER_SIZE);
    block->free = 1;

    /* Coalesce adjacent free blocks */
    POOL_BLOCK* current = MmPoolHead;
    while (current) {
        if (current->free && current->next && current->next->free) {
            current->size += BLOCK_HEADER_SIZE + current->next->size;
            current->next = current->next->next;
            /* Don't advance - check if we can merge again */
            continue;
        }
        current = current->next;
    }
}
