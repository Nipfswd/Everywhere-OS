/*++

Module Name:

    multiboot.h

Abstract:

    Multiboot 1 specification structures. GRUB loads the kernel and
    passes a pointer to a MULTIBOOT_INFO block in EBX before jumping
    to the entry point.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#pragma once

#include <stdint.h>

#define MULTIBOOT_MAGIC         0x2BADB002

#define MULTIBOOT_FLAG_MEM      0x00000001
#define MULTIBOOT_FLAG_MMAP     0x00000040

#define MULTIBOOT_MMAP_AVAILABLE    1
#define MULTIBOOT_MMAP_RESERVED     2

typedef struct __attribute__((packed)) _MULTIBOOT_MMAP_ENTRY {
    uint32_t Size;
    uint64_t BaseAddress;
    uint64_t Length;
    uint32_t Type;
} MULTIBOOT_MMAP_ENTRY;

typedef struct __attribute__((packed)) _MULTIBOOT_INFO {
    uint32_t Flags;
    uint32_t MemLower;
    uint32_t MemUpper;
    uint32_t BootDevice;
    uint32_t CommandLine;
    uint32_t ModsCount;
    uint32_t ModsAddress;
    uint32_t Syms[4];
    uint32_t MmapLength;
    uint32_t MmapAddress;
    uint32_t DrivesLength;
    uint32_t DrivesAddress;
    uint32_t ConfigTable;
    uint32_t BootLoaderName;
    uint32_t ApmTable;
    uint32_t VbeControlInfo;
    uint32_t VbeModeInfo;
    uint16_t VbeMode;
    uint16_t VbeInterfaceSeg;
    uint16_t VbeInterfaceOff;
    uint16_t VbeInterfaceLen;
} MULTIBOOT_INFO;