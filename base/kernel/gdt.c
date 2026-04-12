/*++

Module Name:

    gdt.c

Abstract:

    Global Descriptor Table initialisation. Installs a flat 32-bit
    protected-mode GDT with null, kernel code, kernel data, user code,
    user data, and TSS descriptors, then reloads all segment registers.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "inc/gdt.h"

#define GDT_ENTRIES 6

/*
 * A GDT entry is 8 bytes.  The layout is spread across two 32-bit
 * fields in the way the x86 architects intended to inconvenience everyone.
 *
 *  31       24 23 22 21 20 19    16 15 14 13 12 11     8 7        0
 *  [ base 31:24 | G |DB| 0| A |lim19:16| P | DPL | S | type | base 23:16 ]
 *  [                base 15:0               |          limit 15:0         ]
 */

typedef struct __attribute__((packed)) _GDT_ENTRY {
    unsigned short LimitLow;
    unsigned short BaseLow;
    unsigned char  BaseMid;
    unsigned char  Access;
    unsigned char  Granularity;
    unsigned char  BaseHigh;
} GDT_ENTRY;

typedef struct __attribute__((packed)) _GDT_POINTER {
    unsigned short Limit;
    unsigned int   Base;
} GDT_POINTER;

static GDT_ENTRY   GdtTable[GDT_ENTRIES];
static GDT_POINTER GdtPtr;

static VOID
GdtSetEntry (
    int           Index,
    unsigned int  Base,
    unsigned int  Limit,
    unsigned char Access,
    unsigned char Granularity
    )
{
    GdtTable[Index].BaseLow    = (unsigned short)(Base & 0xFFFF);
    GdtTable[Index].BaseMid    = (unsigned char)((Base >> 16) & 0xFF);
    GdtTable[Index].BaseHigh   = (unsigned char)((Base >> 24) & 0xFF);
    GdtTable[Index].LimitLow   = (unsigned short)(Limit & 0xFFFF);
    GdtTable[Index].Granularity = (unsigned char)(((Limit >> 16) & 0x0F) |
                                                  (Granularity   & 0xF0));
    GdtTable[Index].Access     = Access;
}

/*
 * Reload CS via a far return and reload the data segment registers.
 * Must be a separate asm block so the compiler has already written
 * GdtPtr before we execute lgdt.
 */
static VOID
GdtFlush (
    VOID
    )
{
    __asm__ volatile (
        "lgdt (%0)          \n\t"
        "mov  $0x10, %%ax   \n\t"   /* GDT_KERNEL_DATA */
        "mov  %%ax,  %%ds   \n\t"
        "mov  %%ax,  %%es   \n\t"
        "mov  %%ax,  %%fs   \n\t"
        "mov  %%ax,  %%gs   \n\t"
        "mov  %%ax,  %%ss   \n\t"
        "ljmp $0x08, $1f    \n\t"   /* GDT_KERNEL_CODE - far jump reloads CS */
        "1:                 \n\t"
        :
        : "r"(&GdtPtr)
        : "eax", "memory"
    );
}

VOID
GdtInitialize (
    VOID
    )
{
    /* 0x00 - null descriptor (required by the architecture) */
    GdtSetEntry(0, 0, 0, 0, 0);

    /* 0x08 - kernel code: base 0, limit 4 GB, DPL 0, execute/read */
    GdtSetEntry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* 0x10 - kernel data: base 0, limit 4 GB, DPL 0, read/write */
    GdtSetEntry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* 0x18 - user code: base 0, limit 4 GB, DPL 3, execute/read */
    GdtSetEntry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    /* 0x20 - user data: base 0, limit 4 GB, DPL 3, read/write */
    GdtSetEntry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    /* 0x28 - TSS placeholder (zero until a TSS is installed) */
    GdtSetEntry(5, 0, 0, 0, 0);

    GdtPtr.Limit = (unsigned short)(sizeof(GdtTable) - 1);
    GdtPtr.Base  = (unsigned int)GdtTable;

    GdtFlush();
}