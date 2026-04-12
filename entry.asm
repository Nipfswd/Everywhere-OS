[BITS 32]

section .multiboot
align 4
    dd 0x1BADB002
    dd 0x00000003
    dd -(0x1BADB002 + 0x00000003)

section .text
global start
extern kernelMain

start:
    mov  esp, 0x90000
    push ebx
    call kernelMain

.hang:
    cli
    hlt
    jmp .hang