; Entry point for Everywhere OS kernel with Multiboot header
; 32-bit initialization code before calling kernelMain

[bits 32]

; ------------------------------------------------------------
; Multiboot header constants
; ------------------------------------------------------------

MULTIBOOT_MAGIC    equ 0x1BADB002
MULTIBOOT_FLAGS    equ 0x00000003
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

; ------------------------------------------------------------
; Multiboot header (must be in the first 8 KiB of the file)
; ------------------------------------------------------------

section .multiboot
align 4
multiboot_header:
    dd MULTIBOOT_MAGIC        ; magic
    dd MULTIBOOT_FLAGS        ; flags
    dd MULTIBOOT_CHECKSUM     ; checksum

    dd multiboot_header       ; header_addr
    dd _start                 ; load_addr
    dd _end                   ; load_end_addr
    dd _bss_end               ; bss_end_addr
    dd start                  ; entry_addr

; ------------------------------------------------------------
; Code section
; ------------------------------------------------------------

section .text
global start
extern kernelMain

start:
    ; We are in 32-bit protected mode, stack set by bootloader
    call kernelMain

.hang:
    cli
    hlt
    jmp .hang

; ------------------------------------------------------------
; Symbols for linker / Multiboot fields
; ------------------------------------------------------------

section .data
global _start
_start:

section .bss
global _end
global _bss_end
_end:
_bss_end:
