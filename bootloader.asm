; Bootloader for Everywhere OS
; Minimal BIOS bootloader that loads and executes the kernel

[BITS 16]
[ORG 0x7C00]

KERNEL_OFFSET equ 0x1000        ; Kernel loaded at 0x1000

boot:
    cli                          ; Disable interrupts
    cld                          ; Clear direction flag
    
    mov bp, 0x9000              ; Set stack pointer
    mov sp, bp
    
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    ; Load kernel from disk (for now, just enter protected mode)
    call enable_a20
    
    lgdt [gdt_descriptor]        ; Load GDT
    
    ; Enter protected mode
    mov eax, cr0
    or eax, 1                    ; Set PE bit
    mov cr0, eax
    
    ; Far jump to 32-bit code
    jmp 0x08:start32
    
enable_a20:
    ; Enable A20 line
    mov ax, 0x2401
    int 0x15
    ret

[BITS 32]

start32:
    ; Set up 32-bit segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov ebp, 0x90000
    mov esp, ebp
    
    ; Jump to kernel
    jmp KERNEL_OFFSET
    
    hlt

; GDT (Global Descriptor Table)
align 4, db 0
gdt:
    ; GDT entry 0: null descriptor
    dq 0
    
    ; GDT entry 1: code descriptor
    dw 0xFFFF                   ; limit 0:15
    dw 0x0000                   ; base 0:15
    db 0x00                     ; base 16:23
    db 0x9A                     ; access byte (present, DPL=0, code, readable)
    db 0xCF                     ; limit 16:19 and flags (4KB granularity, 32-bit)
    db 0x00                     ; base 24:31
    
    ; GDT entry 2: data descriptor
    dw 0xFFFF                   ; limit 0:15
    dw 0x0000                   ; base 0:15
    db 0x00                     ; base 16:23
    db 0x92                     ; access byte (present, DPL=0, data, writable)
    db 0xCF                     ; limit 16:19 and flags
    db 0x00                     ; base 24:31

gdt_descriptor:
    dw $ - gdt - 1              ; GDT limit
    dd gdt                      ; GDT base
    
; Bootloader signature
align 512, db 0
    db 0x55, 0xAA               ; Boot signature at 0x7DFE
