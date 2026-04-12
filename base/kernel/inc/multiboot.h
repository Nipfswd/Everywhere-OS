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

#define MULTIBOOT_MAGIC             0x2BADB002

#define MULTIBOOT_FLAG_MEM          0x00000001
#define MULTIBOOT_FLAG_MMAP         0x00000040
#define MULTIBOOT_FLAG_VBE          0x00000800
#define MULTIBOOT_FLAG_FRAMEBUFFER  0x00001000

#define MULTIBOOT_MMAP_AVAILABLE    1
#define MULTIBOOT_MMAP_RESERVED     2

#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED  0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB      1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT 2

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
    uint64_t FramebufferAddress;
    uint32_t FramebufferPitch;
    uint32_t FramebufferWidth;
    uint32_t FramebufferHeight;
    uint8_t  FramebufferBpp;
    uint8_t  FramebufferType;
    uint8_t  ColorInfo[6];
} MULTIBOOT_INFO;