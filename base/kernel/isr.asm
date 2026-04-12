[BITS 32]

; IDT exception stubs.
;
; Vectors that the CPU pushes an error code for: 8, 10-14, 17.
; All others get a dummy 0 pushed first so every stub arrives at
; IdtDispatch with an identical INTERRUPT_FRAME layout.
;
; Frame on entry to IdtDispatch (top = low address):
;   edi esi ebp esp ebx edx ecx eax  <- pushad
;   vector
;   error_code
;   eip cs eflags                     <- pushed by CPU

extern IdtDispatch

%macro ISR_NOERR 1
global IdtStub%1
IdtStub%1:
    push dword 0
    push dword %1
    jmp  IsrCommon
%endmacro

%macro ISR_ERR 1
global IdtStub%1
IdtStub%1:
    push dword %1
    jmp  IsrCommon
%endmacro

ISR_NOERR  0
ISR_NOERR  1
ISR_NOERR  2
ISR_NOERR  3
ISR_NOERR  4
ISR_NOERR  5
ISR_NOERR  6
ISR_NOERR  7
ISR_ERR    8
ISR_NOERR  9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

IsrCommon:
    pushad
    push esp
    call IdtDispatch
    add  esp, 4
    popad
    add  esp, 8     ; pop vector + error_code
    iret