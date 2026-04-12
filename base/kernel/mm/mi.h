/*++

Module Name:

    mi.h

Abstract:

    Internal definitions for the kernel memory manager. This header is
    private to base/kernel/mm and must not be included by code outside
    that directory. The public MM interface lives in base/kernel/inc/mm.h.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#pragma once

#include <stdint.h>
#include "../inc/types.h"

/* -----------------------------------------------------------------------
 * Scalar types used throughout MM
 * --------------------------------------------------------------------- */

typedef uint32_t  PFN_NUMBER;       /* Physical Frame Number (page index)  */
typedef uint32_t  MMPTE_HARDWARE;   /* Raw 32-bit PTE value                */
typedef uint32_t  ULONG;
typedef uint16_t  USHORT;
typedef uint8_t   UCHAR;
typedef int32_t   LONG;
typedef uint32_t  ULONG_PTR;        /* Pointer-width integer (32-bit)      */
typedef ULONG_PTR PHYSICAL_ADDRESS;

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/* -----------------------------------------------------------------------
 * Address space layout  (3 GB user / 1 GB kernel split)
 * --------------------------------------------------------------------- */

#define MM_USER_SPACE_START     0x00001000UL
#define MM_USER_SPACE_END       0xBFFFFFFFUL
#define MM_KERNEL_SPACE_START   0xC0000000UL
#define MM_KERNEL_SPACE_END     0xFFFFFFFFUL

#define MM_PAGE_SIZE            0x1000UL
#define MM_PAGE_SHIFT           12
#define MM_PAGE_MASK            (~(MM_PAGE_SIZE - 1))

#define PAGE_ALIGN(addr)    ( (ULONG_PTR)(addr) & MM_PAGE_MASK )
#define PAGE_ALIGN_UP(addr) ( PAGE_ALIGN((ULONG_PTR)(addr) + MM_PAGE_SIZE - 1) )
#define ADDRESS_TO_PFN(addr) ( (PFN_NUMBER)((ULONG_PTR)(addr) >> MM_PAGE_SHIFT) )
#define PFN_TO_ADDRESS(pfn)  ( (ULONG_PTR)((pfn) << MM_PAGE_SHIFT) )

/* -----------------------------------------------------------------------
 * x86 PTE / PDE hardware flags
 * --------------------------------------------------------------------- */

#define MM_PTE_PRESENT          0x001UL
#define MM_PTE_WRITE            0x002UL
#define MM_PTE_USER             0x004UL
#define MM_PTE_WRITE_THROUGH    0x008UL
#define MM_PTE_CACHE_DISABLE    0x010UL
#define MM_PTE_ACCESSED         0x020UL
#define MM_PTE_DIRTY            0x040UL
#define MM_PTE_LARGE            0x080UL
#define MM_PTE_GLOBAL           0x100UL
#define MM_PTE_FRAME_MASK       0xFFFFF000UL

#define PTE_INDEX(va)  ( ((ULONG_PTR)(va) >> 12) & 0x3FF )
#define PDE_INDEX(va)  ( ((ULONG_PTR)(va) >> 22) & 0x3FF )

/* -----------------------------------------------------------------------
 * Page protection constants
 * --------------------------------------------------------------------- */

#define MM_PROT_NONE    0x00
#define MM_PROT_READ    0x01
#define MM_PROT_WRITE   0x02
#define MM_PROT_EXEC    0x04
#define MM_PROT_KERNEL  0x08    /* Supervisor-only access */

/* -----------------------------------------------------------------------
 * VAD (Virtual Address Descriptor)
 *
 * Each VAD describes one contiguous reserved or committed region of
 * virtual address space. VADs for a given address space are organized
 * as a binary search tree keyed by StartVpn.
 * --------------------------------------------------------------------- */

typedef struct _MM_VAD {
    ULONG_PTR           StartVpn;       /* First page number in region      */
    ULONG_PTR           EndVpn;         /* Last  page number in region      */
    UCHAR               Protection;     /* MM_PROT_* flags                  */
    UCHAR               Flags;          /* MMVAD_FLAG_*                     */
    struct _MM_VAD     *Left;
    struct _MM_VAD     *Right;
    struct _MM_VAD     *Parent;
} MM_VAD, *PMM_VAD;

#define MMVAD_FLAG_PRIVATE      0x01    /* Private (non-shared) mapping     */
#define MMVAD_FLAG_COMMITTED    0x02    /* Pages have been committed        */
#define MMVAD_FLAG_STACK        0x04    /* Region is a thread stack         */

/* -----------------------------------------------------------------------
 * PFN database entry
 *
 * One MMPFN exists per physical page managed by the memory manager.
 * --------------------------------------------------------------------- */

typedef enum _MM_PAGE_STATE {
    MmPageStateFree       = 0,
    MmPageStateZeroed     = 1,
    MmPageStateActive     = 2,
    MmPageStateTransition = 3,
    MmPageStateBad        = 4
} MM_PAGE_STATE;

typedef struct _MMPFN {
    union {
        PFN_NUMBER  Flink;      /* Next PFN on free/zeroed list             */
        ULONG_PTR   MappedVa;   /* VA this page is currently mapped at      */
    } u1;
    PFN_NUMBER      Blink;      /* Previous PFN on list                     */
    MM_PAGE_STATE   State;
    USHORT          ShareCount; /* Number of PTEs pointing to this frame    */
    UCHAR           Flags;
} MMPFN, *PMMPFN;

/* -----------------------------------------------------------------------
 * Free / zeroed page list
 * --------------------------------------------------------------------- */

typedef struct _MM_PAGE_LIST {
    PFN_NUMBER  Head;
    PFN_NUMBER  Tail;
    ULONG       Count;
} MM_PAGE_LIST, *PMM_PAGE_LIST;

#define PFN_INVALID ((PFN_NUMBER)0xFFFFFFFFUL)

/* -----------------------------------------------------------------------
 * Global MM state block (defined in mminit.c)
 * --------------------------------------------------------------------- */

typedef struct _MI_STATE {
    PFN_NUMBER          LowestPfn;
    PFN_NUMBER          HighestPfn;
    ULONG               TotalPages;
    ULONG               FreePages;

    PMMPFN              PfnDatabase;    /* Array indexed by PFN             */

    MM_PAGE_LIST        FreeList;
    MM_PAGE_LIST        ZeroedList;

    MMPTE_HARDWARE     *PageDirectory;  /* Virtual address of kernel PD     */

    PMM_VAD             KernelVadRoot;

    ULONG               Flags;
} MI_STATE, *PMI_STATE;

#define MI_FLAG_INITIALIZED 0x01

extern MI_STATE MiState;

/* -----------------------------------------------------------------------
 * Internal function prototypes
 * --------------------------------------------------------------------- */

/* allocpag.c */
PFN_NUMBER  MiAllocatePage(VOID);
PFN_NUMBER  MiAllocateZeroedPage(VOID);
VOID        MiFreePage(PFN_NUMBER Pfn);

/* pfnlist.c */
VOID        MiInitializePfnDatabase(ULONG_PTR MemoryBase, ULONG PageCount);
VOID        MiInsertPageFreeList(PFN_NUMBER Pfn);
VOID        MiInsertPageZeroedList(PFN_NUMBER Pfn);
PFN_NUMBER  MiRemovePageFreeList(VOID);
PFN_NUMBER  MiRemovePageZeroedList(VOID);
PMMPFN      MiGetPfn(PFN_NUMBER Pfn);

/* vadtree.c */
PMM_VAD     MiAllocateVad(VOID);
VOID        MiFreeVad(PMM_VAD Vad);
VOID        MiInsertVad(PMM_VAD **Root, PMM_VAD Vad);
VOID        MiRemoveVad(PMM_VAD **Root, PMM_VAD Vad);
PMM_VAD     MiFindVad(PMM_VAD Root, ULONG_PTR Vpn);
PMM_VAD     MiFindVadRange(PMM_VAD Root, ULONG_PTR StartVpn, ULONG_PTR EndVpn);

/* allocvm.c */
ULONG_PTR   MiReserveVirtualRange(PMM_VAD **VadRoot,
                                   ULONG_PTR StartHint,
                                   ULONG     PageCount,
                                   UCHAR     Protection);

/* freevm.c */
VOID        MiFreeVirtualRange(PMM_VAD **VadRoot,
                                ULONG_PTR StartVpn,
                                ULONG_PTR EndVpn);

/* protect.c */
UCHAR       MiProtectionToPteFlags(UCHAR Protection);
VOID        MiSetPageProtection(ULONG_PTR VirtualAddress, UCHAR Protection);

/* mmsup.c */
VOID        MiZeroPage(ULONG_PTR VirtualAddress);
ULONG_PTR   MiMapPhysicalPage(PFN_NUMBER Pfn);
VOID        MiFlushTlbEntry(ULONG_PTR VirtualAddress);
VOID        MiFlushTlb(VOID);