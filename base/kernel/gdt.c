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
    if (Index < 0 || Index >= GDT_ENTRIES) {
        return;
    }

    GdtTable[Index].BaseLow     = (unsigned short)(Base  & 0xFFFF);
    GdtTable[Index].BaseMid     = (unsigned char)((Base  >> 16) & 0xFF);
    GdtTable[Index].BaseHigh    = (unsigned char)((Base  >> 24) & 0xFF);
    GdtTable[Index].LimitLow    = (unsigned short)(Limit & 0xFFFF);
    GdtTable[Index].Granularity = (unsigned char)(((Limit >> 16) & 0x0F) |
                                                  (Granularity   & 0xF0));
    GdtTable[Index].Access      = Access;
}

static VOID
GdtFlush (
    VOID
    )
{
    __asm__ volatile (
        "lgdt  (%0)         \n\t"
        "mov   $0x10, %%ax  \n\t"
        "mov   %%ax,  %%ds  \n\t"
        "mov   %%ax,  %%es  \n\t"
        "mov   %%ax,  %%fs  \n\t"
        "mov   %%ax,  %%gs  \n\t"
        "mov   %%ax,  %%ss  \n\t"
        "ljmp  $0x08, $1f   \n\t"
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
    GdtSetEntry(0, 0, 0,          0x00, 0x00);
    GdtSetEntry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    GdtSetEntry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    GdtSetEntry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    GdtSetEntry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    GdtSetEntry(5, 0, 0,          0x00, 0x00);

    GdtPtr.Limit = (unsigned short)(sizeof(GdtTable) - 1);
    GdtPtr.Base  = (unsigned int)GdtTable;

    GdtFlush();
}