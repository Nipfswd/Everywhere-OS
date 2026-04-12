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

    /*
     * Initialise the framebuffer first — everything else calls Print.
     * If GRUB did not provide a framebuffer (flag bit 12 clear) we pass
     * zeros and VideoInitialize falls back to VGA text mode.
     */
    if (MbInfo && (MbInfo->Flags & MULTIBOOT_FLAG_FRAMEBUFFER)) {
        VideoInitialize(MbInfo->FramebufferAddress,
                        MbInfo->FramebufferWidth,
                        MbInfo->FramebufferHeight,
                        MbInfo->FramebufferPitch,
                        MbInfo->FramebufferBpp);
    } else {
        VideoInitialize(0, 0, 0, 0, 0);
    }

    ClearScreen();
    Print(OS_NAME " v" OS_VERSION_STRING "\n\n");

    GdtInitialize();
    PrintStatus("GDT initialisation...    ", 1);

    IdtInitialize();
    PrintStatus("IDT initialisation...    ", 1);

    PagingInitialize(MbInfo);
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