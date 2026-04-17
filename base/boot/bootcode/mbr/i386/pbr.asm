; pbr.asm
BITS 16
ORG 0x7C00

start:
    xor ax, ax
    mov ds, ax

    mov si, msg

.print:
    lodsb
    test al, al
    jz $
    mov ah, 0x0E
    int 0x10
    jmp .print

msg db "EverywhereOS booted",0

times 510-($-$$) db 0
dw 0xAA55