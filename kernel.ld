ENTRY(start)

SECTIONS
{
    /* Kernel load address */
    . = 0x00100000;

    /* Multiboot header MUST be first */
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
        _end = .;
    }

    .bss :
    {
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    }
}
