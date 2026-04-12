ENTRY(start)

SECTIONS
{
    . = 0x00100000;

    __start = .;

    .multiboot :
    {
        KEEP(*(.multiboot))
    }

    .text :
    {
        *(.text*)
    }

    .rodata :
    {
        *(.rodata*)
    }

    .data :
    {
        *(.data*)
    }

    .bss :
    {
        *(.bss*)
        *(COMMON)
    }

    __end = .;
}
