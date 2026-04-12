/*++

Module Name:

    idt.c

Abstract:

    Interrupt Descriptor Table initialisation. Installs gates for all
    32 CPU exception vectors. Remaps the 8259 PIC so that IRQ0-IRQ15
    arrive on vectors 32-47, preventing collisions with CPU exceptions.
    All IRQs are masked after remapping; drivers unmask individually.
    Interrupts are left disabled on return.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#include "inc/idt.h"
#include "inc/io.h"
#include "inc/video.h"

#define IDT_ENTRIES  256

#define PIC1_CMD     0x20
#define PIC1_DATA    0x21
#define PIC2_CMD     0xA0
#define PIC2_DATA    0xA1
#define ICW1_INIT    0x10
#define ICW1_ICW4    0x01
#define ICW4_8086    0x01
#define IRQ0_VECTOR  32

typedef struct __attribute__((packed)) _IDT_ENTRY {
    unsigned short OffsetLow;
    unsigned short Selector;
    unsigned char  Reserved;
    unsigned char  Flags;
    unsigned short OffsetHigh;
} IDT_ENTRY;

typedef struct __attribute__((packed)) _IDT_POINTER {
    unsigned short Limit;
    unsigned int   Base;
} IDT_POINTER;

typedef struct _INTERRUPT_FRAME {
    unsigned int Edi, Esi, Ebp, Esp;
    unsigned int Ebx, Edx, Ecx, Eax;
    unsigned int Vector;
    unsigned int ErrorCode;
    unsigned int Eip, Cs, Eflags;
} INTERRUPT_FRAME;

static IDT_ENTRY   IdtTable[IDT_ENTRIES];
static IDT_POINTER IdtPtr;

extern void IdtStub0(void);   extern void IdtStub1(void);
extern void IdtStub2(void);   extern void IdtStub3(void);
extern void IdtStub4(void);   extern void IdtStub5(void);
extern void IdtStub6(void);   extern void IdtStub7(void);
extern void IdtStub8(void);   extern void IdtStub9(void);
extern void IdtStub10(void);  extern void IdtStub11(void);
extern void IdtStub12(void);  extern void IdtStub13(void);
extern void IdtStub14(void);  extern void IdtStub15(void);
extern void IdtStub16(void);  extern void IdtStub17(void);
extern void IdtStub18(void);  extern void IdtStub19(void);
extern void IdtStub20(void);  extern void IdtStub21(void);
extern void IdtStub22(void);  extern void IdtStub23(void);
extern void IdtStub24(void);  extern void IdtStub25(void);
extern void IdtStub26(void);  extern void IdtStub27(void);
extern void IdtStub28(void);  extern void IdtStub29(void);
extern void IdtStub30(void);  extern void IdtStub31(void);

static const char *ExceptionNames[32] = {
    "#DE Division by Zero",          "#DB Debug",
    "NMI Non-Maskable Interrupt",    "#BP Breakpoint",
    "#OF Overflow",                  "#BR Bound Range Exceeded",
    "#UD Invalid Opcode",            "#NM Device Not Available",
    "#DF Double Fault",              "Coprocessor Segment Overrun",
    "#TS Invalid TSS",               "#NP Segment Not Present",
    "#SS Stack Fault",               "#GP General Protection Fault",
    "#PF Page Fault",                "Reserved",
    "#MF x87 FPU Error",             "#AC Alignment Check",
    "#MC Machine Check",             "#XF SIMD Exception",
    "Reserved",                      "#CP Control Protection",
    "Reserved",                      "Reserved",
    "Reserved",                      "Reserved",
    "Reserved",                      "Reserved",
    "#HV Hypervisor Injection",      "#VC VMM Communication",
    "#SX Security Exception",        "Reserved"
};

static VOID
PrintHex (
    unsigned int Value
    )
{
    static const char Digits[] = "0123456789ABCDEF";
    char              Buffer[9];
    int               i;

    Buffer[8] = '\0';
    for (i = 7; i >= 0; i--) {
        Buffer[i] = Digits[Value & 0xF];
        Value >>= 4;
    }

    Print("0x");
    Print(Buffer);
}

static VOID
PicRemap (
    VOID
    )
{
    outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4);
    outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4);
    outb(PIC1_DATA, IRQ0_VECTOR);
    outb(PIC2_DATA, IRQ0_VECTOR + 8);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

static VOID
IdtSetGate (
    unsigned int   Vector,
    unsigned int   Handler,
    unsigned short Selector,
    unsigned char  Flags
    )
{
    if (Vector >= IDT_ENTRIES || Handler == 0) {
        return;
    }

    IdtTable[Vector].OffsetLow  = (unsigned short)(Handler & 0xFFFF);
    IdtTable[Vector].OffsetHigh = (unsigned short)(Handler >> 16);
    IdtTable[Vector].Selector   = Selector;
    IdtTable[Vector].Reserved   = 0;
    IdtTable[Vector].Flags      = Flags;
}

void
IdtDispatch (
    INTERRUPT_FRAME *Frame
    )
{
    unsigned int Cr2;

    if (Frame == 0) {
        for (;;) { __asm__ volatile ("cli; hlt"); }
    }

    if (Frame->Vector < 32) {
        Print("\n*** EXCEPTION: ");
        Print(ExceptionNames[Frame->Vector]);
        Print("\n");

        Print("  EIP=");       PrintHex(Frame->Eip);
        Print("  CS=");        PrintHex(Frame->Cs);
        Print("  EFLAGS=");    PrintHex(Frame->Eflags);
        Print("\n");
        Print("  EAX=");       PrintHex(Frame->Eax);
        Print("  EBX=");       PrintHex(Frame->Ebx);
        Print("  ECX=");       PrintHex(Frame->Ecx);
        Print("  EDX=");       PrintHex(Frame->Edx);
        Print("\n");
        Print("  ESI=");       PrintHex(Frame->Esi);
        Print("  EDI=");       PrintHex(Frame->Edi);
        Print("  EBP=");       PrintHex(Frame->Ebp);
        Print("  ESP=");       PrintHex(Frame->Esp);
        Print("\n");
        Print("  ErrorCode="); PrintHex(Frame->ErrorCode);
        Print("\n");

        if (Frame->Vector == 14) {
            __asm__ volatile ("mov %%cr2, %0" : "=r"(Cr2));
            Print("  CR2=");   PrintHex(Cr2);
            Print("\n");
        }

        for (;;) { __asm__ volatile ("cli; hlt"); }
    }
}

VOID
IdtInitialize (
    VOID
    )
{
    unsigned int i;

    for (i = 0; i < IDT_ENTRIES; i++) {
        IdtTable[i].OffsetLow  = 0;
        IdtTable[i].Selector   = 0;
        IdtTable[i].Reserved   = 0;
        IdtTable[i].Flags      = 0;
        IdtTable[i].OffsetHigh = 0;
    }

    IdtSetGate( 0, (unsigned int)IdtStub0,  0x08, 0x8E);
    IdtSetGate( 1, (unsigned int)IdtStub1,  0x08, 0x8E);
    IdtSetGate( 2, (unsigned int)IdtStub2,  0x08, 0x8E);
    IdtSetGate( 3, (unsigned int)IdtStub3,  0x08, 0x8E);
    IdtSetGate( 4, (unsigned int)IdtStub4,  0x08, 0x8E);
    IdtSetGate( 5, (unsigned int)IdtStub5,  0x08, 0x8E);
    IdtSetGate( 6, (unsigned int)IdtStub6,  0x08, 0x8E);
    IdtSetGate( 7, (unsigned int)IdtStub7,  0x08, 0x8E);
    IdtSetGate( 8, (unsigned int)IdtStub8,  0x08, 0x8E);
    IdtSetGate( 9, (unsigned int)IdtStub9,  0x08, 0x8E);
    IdtSetGate(10, (unsigned int)IdtStub10, 0x08, 0x8E);
    IdtSetGate(11, (unsigned int)IdtStub11, 0x08, 0x8E);
    IdtSetGate(12, (unsigned int)IdtStub12, 0x08, 0x8E);
    IdtSetGate(13, (unsigned int)IdtStub13, 0x08, 0x8E);
    IdtSetGate(14, (unsigned int)IdtStub14, 0x08, 0x8E);
    IdtSetGate(15, (unsigned int)IdtStub15, 0x08, 0x8E);
    IdtSetGate(16, (unsigned int)IdtStub16, 0x08, 0x8E);
    IdtSetGate(17, (unsigned int)IdtStub17, 0x08, 0x8E);
    IdtSetGate(18, (unsigned int)IdtStub18, 0x08, 0x8E);
    IdtSetGate(19, (unsigned int)IdtStub19, 0x08, 0x8E);
    IdtSetGate(20, (unsigned int)IdtStub20, 0x08, 0x8E);
    IdtSetGate(21, (unsigned int)IdtStub21, 0x08, 0x8E);
    IdtSetGate(22, (unsigned int)IdtStub22, 0x08, 0x8E);
    IdtSetGate(23, (unsigned int)IdtStub23, 0x08, 0x8E);
    IdtSetGate(24, (unsigned int)IdtStub24, 0x08, 0x8E);
    IdtSetGate(25, (unsigned int)IdtStub25, 0x08, 0x8E);
    IdtSetGate(26, (unsigned int)IdtStub26, 0x08, 0x8E);
    IdtSetGate(27, (unsigned int)IdtStub27, 0x08, 0x8E);
    IdtSetGate(28, (unsigned int)IdtStub28, 0x08, 0x8E);
    IdtSetGate(29, (unsigned int)IdtStub29, 0x08, 0x8E);
    IdtSetGate(30, (unsigned int)IdtStub30, 0x08, 0x8E);
    IdtSetGate(31, (unsigned int)IdtStub31, 0x08, 0x8E);

    IdtPtr.Limit = (unsigned short)(sizeof(IdtTable) - 1);
    IdtPtr.Base  = (unsigned int)IdtTable;

    __asm__ volatile ("lidt (%0)" : : "r"(&IdtPtr) : "memory");

    PicRemap();
}