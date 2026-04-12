/*++

Module Name:

    paging.c

Abstract:

    x86 paging initialisation. Allocates a page directory and initial
    page tables from a small static pool, identity-maps the first 8 MB
    of physical memory (covering the kernel image, stack, and early heap),
    maps the VESA linear framebuffer region (up to 8 MB at whatever
    physical address GRUB places it), then enables paging by setting
    CR0.PG.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "inc/paging.h"
#include "inc/multiboot.h"

#define PAGE_SIZE       0x1000
#define PAGE_PRESENT    0x01
#define PAGE_WRITE      0x02
#define PAGE_FRAME_MASK 0xFFFFF000

/*
 * Low identity map: first 8 MB (2 page tables).
 * Framebuffer map: up to 8 MB for the framebuffer (2 page tables).
 * Total static page tables: 4.
 */
#define IDENTITY_MB  8
#define PT_LOW       (IDENTITY_MB / 4)   /* 2 */
#define FB_MB        8
#define PT_FB        (FB_MB / 4)         /* 2 */
#define PT_COUNT     (PT_LOW + PT_FB)    /* 4 */

static unsigned int PageDirectory[1024]          __attribute__((aligned(4096)));
static unsigned int PageTables[PT_COUNT][1024]   __attribute__((aligned(4096)));

/* Framebuffer base stored by PagingInitialize for later use */
static unsigned int g_FbBase = 0;

static unsigned int
PagingReadCr3 (
    VOID
    )
{
    unsigned int Value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(Value));
    return Value;
}

unsigned int
PagingGetCr3 (
    VOID
    )
{
    return PagingReadCr3();
}

/*++

Routine Description:

    Initialises paging with an identity map of the first 8 MB plus a
    direct map of the VESA framebuffer region.

    MbInfo may be NULL (e.g. if called before multiboot is parsed); in
    that case only the low identity map is installed and the framebuffer
    will fault if accessed — VideoInitialize handles this by checking
    the address before writing.

--*/

VOID
PagingInitialize (
    MULTIBOOT_INFO *MbInfo
    )
{
    unsigned int i, t;
    unsigned int PhysicalAddress;
    unsigned int FbBase    = 0;
    unsigned int FbPages   = 0;
    unsigned int FbDirBase = 0;   /* first PD index for FB tables */

    /* Zero the page directory */
    for (i = 0; i < 1024; i++) {
        PageDirectory[i] = 0;
    }

    /* --- Low identity map: 0..8MB --- */
    for (t = 0; t < PT_LOW; t++) {
        for (i = 0; i < 1024; i++) {
            PhysicalAddress  = (t * 1024 + i) * PAGE_SIZE;
            PageTables[t][i] = PhysicalAddress | PAGE_PRESENT | PAGE_WRITE;
        }
        PageDirectory[t] = (unsigned int)PageTables[t] | PAGE_PRESENT | PAGE_WRITE;
    }

    /* --- Framebuffer map --- */
    if (MbInfo && (MbInfo->Flags & MULTIBOOT_FLAG_FRAMEBUFFER)) {
        FbBase  = (unsigned int)MbInfo->FramebufferAddress;
        g_FbBase = FbBase;

        /* Number of pages needed: width * height * 4 bytes, rounded up */
        {
            unsigned int FbBytes = MbInfo->FramebufferPitch * MbInfo->FramebufferHeight;
            FbPages = (FbBytes + PAGE_SIZE - 1) / PAGE_SIZE;
            if (FbPages > PT_FB * 1024) FbPages = PT_FB * 1024;
        }

        /* Page directory index where the FB starts */
        FbDirBase = FbBase >> 22;   /* top 10 bits */

        for (t = 0; t < PT_FB; t++) {
            unsigned int DirIdx = FbDirBase + t;
            if (DirIdx >= 1024) break;

            for (i = 0; i < 1024; i++) {
                unsigned int PageIdx = t * 1024 + i;
                if (PageIdx < FbPages) {
                    PhysicalAddress          = FbBase + PageIdx * PAGE_SIZE;
                    PageTables[PT_LOW + t][i] = PhysicalAddress | PAGE_PRESENT | PAGE_WRITE;
                } else {
                    PageTables[PT_LOW + t][i] = 0;
                }
            }
            PageDirectory[DirIdx] = (unsigned int)PageTables[PT_LOW + t] | PAGE_PRESENT | PAGE_WRITE;
        }
    }

    /* Load CR3 */
    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"((unsigned int)PageDirectory)
        : "memory"
    );

    /* Set CR0.PG */
    __asm__ volatile (
        "mov %%cr0,       %%eax  \n\t"
        "or  $0x80000000, %%eax  \n\t"
        "mov %%eax,       %%cr0  \n\t"
        :
        :
        : "eax", "memory"
    );

    __asm__ volatile ("jmp 1f\n1:");
}