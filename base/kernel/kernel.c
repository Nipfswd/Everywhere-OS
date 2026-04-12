/*++

Module Name:

    kernel.c

Abstract:

    Main entry point for the OS kernel. Receives the multiboot info
    pointer from the boot stub and passes it to the memory manager so
    it can build a PFN database from the real physical memory map.

Author:

    Noah Juopperi <nipfswd@gmail.com>
    Clay Sanders (made first kernel) <claylikepython@yahoo.com>

Environment:

    Text-mode VGA, PC keyboard controller.

--*/

#include "inc/types.h"
#include "inc/video.h"
#include "inc/shell.h"
#include "inc/gdt.h"
#include "inc/idt.h"
#include "inc/paging.h"
#include "inc/multiboot.h"
#include "inc/mm.h"
#include "osver.h"

#define ATTR_NORMAL 0x07
#define ATTR_GREEN  0x0A
#define ATTR_RED    0x0C

static VOID
PrintStatus (
    const char *Label,
    INT         Ok
    )
{
    char Attribute;
    int  i;

    for (i = 0; Label[i]; i++) {
        PrintChar(Label[i], ATTR_NORMAL);
    }

    if (Ok) {
        Attribute = ATTR_GREEN;
        PrintChar('[', Attribute);
        PrintChar(' ', Attribute);
        PrintChar('O', Attribute);
        PrintChar('K', Attribute);
        PrintChar(' ', Attribute);
        PrintChar(']', Attribute);
    } else {
        Attribute = ATTR_RED;
        PrintChar('[', Attribute);
        PrintChar('F', Attribute);
        PrintChar('A', Attribute);
        PrintChar('I', Attribute);
        PrintChar('L', Attribute);
        PrintChar(']', Attribute);
    }

    PrintChar('\n', ATTR_NORMAL);
}

VOID
kernelMain (
    MULTIBOOT_INFO *MbInfo
    )
{
    char Buffer[128];

    ClearScreen();
    Print(OS_NAME " v" OS_VERSION_STRING "\n\n");

    GdtInitialize();
    PrintStatus("GDT initialisation...    ", 1);

    IdtInitialize();
    PrintStatus("IDT initialisation...    ", 1);

    PagingInitialize();
    PrintStatus("Paging initialisation... ", 1);

    MmInitialize(MbInfo);
    PrintStatus("MM initialisation...     ", 1);

    Print("\n");

    for (;;) {
        Print(user_name);
        Print(": ");
        GetInput(Buffer);
        ProcessCommand(Buffer);
    }
}