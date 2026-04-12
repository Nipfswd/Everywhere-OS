/*++

Module Name:

    mm.h

Abstract:

    Public interface to the kernel memory manager.
    Include this header from code outside of base/kernel/mm/.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#pragma once

#include "types.h"
#include <stdint.h>

typedef uint32_t ULONG;
typedef uint32_t ULONG_PTR;
typedef uint8_t  UCHAR;
typedef int      INT;

/*
 * Public Page protection flags
*/

#define MM_PROT_NONE    0x00
#define MM_PROT_READ    0x01
#define MM_PROT_WRITE   0x02
#define MM_PROT_EXEC    0x04
#define MM_PROT_KERNEL  0x08

/*
 * Region state values returned by MmQueryVirtualMemory
*/

typedef enum _MM_REGION_STATE {
    MmRegionFree      = 0,
    MmRegionReserved  = 1,
    MmRegionCommitted = 2
} MM_REGION_STATE;

/*
 * MmQueryVirtualMemory output structure
*/

typedef struct _MM_MEMORY_INFO {
    ULONG_PTR       BaseAddress;    /* Start of region                      */
    ULONG           RegionSize;     /* Size in bytes                        */
    UCHAR           Protection;     /* MM_PROT_* flags                      */
    MM_REGION_STATE State;          /* Free / reserved / committed          */
    INT             PagePresent;    /* TRUE if the queried page is in TLB   */
} MM_MEMORY_INFO, *PMM_MEMORY_INFO;

/* -----------------------------------------------------------------------
 * Public MM entry points
 * --------------------------------------------------------------------- */

/*
 * MmInitialize
 *
 *   Called once at boot. PageDirectoryPhysical is the physical address of
 *   the page directory already loaded into CR3 by the boot stub.
 */
VOID MmInitialize(ULONG_PTR PageDirectoryPhysical);

/*
 * MmPageFault
 *
 *   Called by the #PF IDT stub. ErrorCode is the value pushed by the CPU.
 */
VOID MmPageFault(ULONG ErrorCode);

/*
 * MmQueryVirtualMemory
 *
 *   Queries the region containing Address. Returns TRUE if Address is
 *   within a reserved region.
 */
INT MmQueryVirtualMemory(void *VadRoot,
                          ULONG_PTR Address,
                          PMM_MEMORY_INFO Info);