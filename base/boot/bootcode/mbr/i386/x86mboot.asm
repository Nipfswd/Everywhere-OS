;   Copyright (C) EverywhereOS
;   All Rights Reserved.
;
;
;   This is the standard boot record that will be shipped on all hard disks.
;   It contains:
;
;   1.  Code to load (and give control to) the active boot record for 1 of 4
;       possible operating systems.
;
;   2.  A partition table at the end of the boot record, followed by the
;       required signature.
;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

BITS 16
ORG 0x7C00

VOLBOOT_ORG             EQU 7C00h
SIZE_SECTOR             EQU 512

Part_Active             EQU 0
Part_StartHead          EQU 1
Part_StartSector        EQU 2
Part_StartCylinder      EQU 3
Part_Type               EQU 4
Part_EndHead            EQU 5
Part_EndSector          EQU 6
Part_EndCylinder        EQU 7
Part_AbsoluteSector     EQU 8
Part_AbsoluteSectorH    EQU 10
Part_SectorCount        EQU 12
Part_SectorCountH       EQU 14

SIZE_PART_TAB_ENT       EQU 16
NUM_PART_TAB_ENTS       EQU 4

PART_TAB_OFF            EQU (SIZE_SECTOR - 2 - (SIZE_PART_TAB_ENT * NUM_PART_TAB_ENTS))
MBR_NT_OFFSET           EQU (PART_TAB_OFF - 6)
MBR_MSG_TABLE_OFFSET    EQU (PART_TAB_OFF - 9)

UsingBackup             EQU SIZE_PART_TAB_ENT

PART_IFS                EQU 07h
PART_FAT32              EQU 0Bh
PART_FAT32_XINT13       EQU 0Ch
PART_XINT13             EQU 0Eh

BOOTSECTRAILSIGH        EQU 0AA55h
FAT32_BACKUP            EQU 6

RELOCATION_ORG          EQU 600h

reloc_delta             EQU 1Bh

start:
        xor     ax,ax
        mov     ss,ax
        mov     sp,VOLBOOT_ORG
        sti
        push    ax
        pop     es
        push    ax
        pop     ds
        cld
        mov     si,VOLBOOT_ORG + reloc_delta
        mov     di,RELOCATION_ORG + reloc_delta
        push    ax
        push    di
        mov     cx,SIZE_SECTOR - reloc_delta
        rep     movsb
        retf

        ; execution continues at 0:0600

relocated:
        mov     bp,tab
        mov     cl,NUM_PART_TAB_ENTS
        xor     ch,ch

active_loop:
        cmp     byte [bp+Part_Active],ch
        jl      check_inactive
        jne     display_bad

        add     bp,SIZE_PART_TAB_ENT
        loop    active_loop

        int     18h

check_inactive:
        mov     si,bp

inactive_loop:
        add     si,SIZE_PART_TAB_ENT
        dec     cx
        jz      StartLoad
        cmp     byte [si],ch
        je      inactive_loop

display_bad:
        mov     al,[m1]

display_msg:
        mov     ah,(RELOCATION_ORG >> 8) + 1
        mov     si,ax

display_msg1:
        lodsb
        cmp     al,0
        je      $
        mov     bx,7
        mov     ah,14
        int     10h
        jmp     display_msg1

StartLoad:
        mov     byte [bp+UsingBackup],cl
        call    ReadSector
        jnc     CheckPbr

trybackup:
        inc     byte [bp+UsingBackup]

tryfat32:
        cmp     byte [bp+Part_Type],PART_FAT32
        je      fat32backup
        cmp     byte [bp+Part_Type],PART_FAT32_XINT13
        je      fat32backup
        mov     al,[m2]
        jmp     display_msg

fat32backup:
        add     byte [bp+Part_StartSector],FAT32_BACKUP
        add     word [bp+Part_AbsoluteSector],FAT32_BACKUP
        adc     word [bp+Part_AbsoluteSectorH],0

RestartLoad:
        call    ReadSector
        jnc     CheckPbr
        mov     al,[m2]
        jmp     display_msg

CheckPbr:
        cmp     word [VOLBOOT_ORG + SIZE_SECTOR - 2],BOOTSECTRAILSIGH
        je      done

        cmp     byte [bp+UsingBackup],0
        je      trybackup
        mov     al,[m3]
        jmp     display_msg

done:
        mov     di,sp
        push    ds
        push    di
        mov     si,bp
        retf

; ---------------------------------------

ReadSector:
        mov     di,5

nonxint13:
        mov     ax,0201h
        mov     bx,VOLBOOT_ORG
        mov     cx,[bp+Part_StartSector]
        mov     dx,[bp+Part_Active]
        int     13h
        jnc     .ok
        dec     di
        jz      .fail
        xor     ah,ah
        mov     dl,[bp+Part_Active]
        int     13h
        jmp     nonxint13

.ok:
        clc
        ret

.fail:
        stc
        ret

; ---------------------------------------

_m1: db "EveryBoot: Bad partition",0
_m2: db "EveryBoot: Load error",0
_m3: db "EveryBoot: No EverywhereOS",0

; Message offset table
times (MBR_MSG_TABLE_OFFSET - ($ - $$)) db 0

m1: db (_m1 - $$) - 256
m2: db (_m2 - $$) - 256
m3: db (_m3 - $$) - 256

; NT disk signature
times (MBR_NT_OFFSET - ($ - $$)) db 0
dd 0x45565259
dw 0

; Partition table
times (PART_TAB_OFF - ($ - $$)) db 0

tab:
times (SIZE_PART_TAB_ENT * NUM_PART_TAB_ENTS) db 0

; Signature
dw BOOTSECTRAILSIGH