[BITS 32]

section .multiboot
align 4
    dd 0x1BADB002           ; magic
    dd 0x00000007           ; flags: mem + bootdev + video mode
    dd -(0x1BADB002 + 0x00000007) ; checksum
    dd 0                    ; header_addr   (unused, flags[16] not set)
    dd 0                    ; load_addr
    dd 0                    ; load_end_addr
    dd 0                    ; bss_end_addr
    dd 0                    ; entry_addr
    dd 0                    ; mode_type: 0 = linear framebuffer
    dd 1024                 ; width
    dd 768                  ; height
    dd 32                   ; depth (bpp)

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