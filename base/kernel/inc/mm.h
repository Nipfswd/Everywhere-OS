/*++

Module Name:

    mm.h

Abstract:

    Public interface to the kernel memory manager.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#pragma once

#include "types.h"
#include "multiboot.h"
#include <stdint.h>

#define MM_PROT_NONE   0x00
#define MM_PROT_READ   0x01
#define MM_PROT_WRITE  0x02
#define MM_PROT_EXEC   0x04
#define MM_PROT_KERNEL 0x08

typedef enum _MM_REGION_STATE {
    MmRegionFree      = 0,
    MmRegionReserved  = 1,
    MmRegionCommitted = 2
} MM_REGION_STATE;

typedef struct _MM_MEMORY_INFO {
    uint32_t        BaseAddress;
    uint32_t        RegionSize;
    uint8_t         Protection;
    MM_REGION_STATE State;
    INT             PagePresent;
} MM_MEMORY_INFO, *PMM_MEMORY_INFO;

struct _MM_VAD;

VOID MmInitialize(MULTIBOOT_INFO *MbInfo);
VOID MmPageFault(uint32_t ErrorCode);
INT  MmQueryVirtualMemory(struct _MM_VAD *VadRoot, uint32_t Address, PMM_MEMORY_INFO Info);