/*++

Module Name:

    paging.c

Abstract:

    x86 paging initialisation. Allocates a page directory and initial
    page tables from a small static pool, identity-maps the first 8 MB
    of physical memory (covering the kernel image, stack, and early heap),
    then enables paging by setting CR0.PG.

    After PagingInitialize returns the MMU is active and every address
    the kernel uses is still valid because physical == virtual for the
    identity-mapped range.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "inc/paging.h"

#define PAGE_SIZE       0x1000
#define PAGE_PRESENT    0x01
#define PAGE_WRITE      0x02
#define PAGE_FRAME_MASK 0xFFFFF000

/*
 * Static page directory and enough page tables to cover 8 MB.
 * Each page table covers 4 MB (1024 entries * 4 KB), so 2 tables suffice.
 * Aligned to 4 KB as required by CR3.
 */
#define IDENTITY_MB     8
#define PT_COUNT        (IDENTITY_MB / 4)

static unsigned int PageDirectory[1024] __attribute__((aligned(4096)));
static unsigned int PageTables[PT_COUNT][1024] __attribute__((aligned(4096)));

/*++

Routine Description:

    Reads the current CR3.

--*/

static unsigned int
PagingReadCr3 (
    VOID
    )
{
    unsigned int Value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(Value));
    return Value;
}

/*++

Routine Description:

    Returns the physical address of the kernel page directory.

--*/

unsigned int
PagingGetCr3 (
    VOID
    )
{
    return PagingReadCr3();
}

/*++

Routine Description:

    Initialises paging.

    1. Zeros the page directory.
    2. For each page table that covers the identity-mapped range,
       fills every PTE with physical_address | PRESENT | WRITE.
    3. Installs those page tables into the page directory.
    4. Loads CR3, sets CR0.PG | CR0.PE, executes a far jump to flush
       the prefetch queue and reload CS with the new GDT selector.

--*/

VOID
PagingInitialize (
    VOID
    )
{
    unsigned int  i;
    unsigned int  t;
    unsigned int  PhysicalAddress;

    /* Zero the page directory */
    for (i = 0; i < 1024; i++) {
        PageDirectory[i] = 0;
    }

    /* Fill page tables and wire them into the directory */
    for (t = 0; t < PT_COUNT; t++) {
        for (i = 0; i < 1024; i++) {
            PhysicalAddress          = (t * 1024 + i) * PAGE_SIZE;
            PageTables[t][i]         = PhysicalAddress | PAGE_PRESENT | PAGE_WRITE;
        }

        PageDirectory[t] = (unsigned int)PageTables[t] | PAGE_PRESENT | PAGE_WRITE;
    }

    /* Load CR3 */
    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"((unsigned int)PageDirectory)
        : "memory"
    );

    /* Set CR0.PG (bit 31) - CR0.PE (bit 0) is already set by the BIOS/GRUB */
    __asm__ volatile (
        "mov %%cr0,       %%eax  \n\t"
        "or  $0x80000000, %%eax  \n\t"
        "mov %%eax,       %%cr0  \n\t"
        :
        :
        : "eax", "memory"
    );

    /*
     * Flush the prefetch queue with a near jump.  A far jump is not
     * needed here because we are not changing CS - the GDT was already
     * reloaded by GdtInitialize.
     */
    __asm__ volatile ("jmp 1f\n1:");
}