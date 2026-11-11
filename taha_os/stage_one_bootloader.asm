; Stage 1 bootloader
; Loaded at memory location 0x7c00

; ===================
; CODE SECTION
; ===================

bits 16    ; Assemble instructions for 16-bit mode
org 0x7c00    ; Set origin address to 0x7c00

jmp 0x0000:main    ; Set CS to 0x0000 and jump to main

main:
    cli    ; Disable maskable hardware interrupts
    xor ax, ax    ; Clear AX register
    mov ds, ax    ; Initialize data segment
    mov es, ax    ; Initialize extra segment
    mov ss, ax    ; Initialize stack segment
    mov sp, 0x7c00    ; Set stack pointer to address 0x7c00
    sti    ; Enable maskable hardware interrupts
    cld     ; Clear direction flag for forward string operations

    mov [boot_drive], dl    ; Save BIOS boot drive number from DL

    ;-------------------------
    ; Load stage 2 bootloader
    ;-------------------------

    mov si, stage_two_bootloader_dap    ; Point SI to DAP (disk address packet)
    mov ah, 0x42    ; Select BIOS extended disk read function
    mov dl, [boot_drive]    ; Load saved boot drive number
    int 0x13    ; Call BIOS disk service to read sectors

    jc hang    ; Jump to hang if the disk read fails

    jmp 0x0000:0x8000    ; Jump to stage 2 bootloader loaded at memory address 0x8000

hang:
    hlt    ; Halt CPU until next interrupt
    jmp hang    ; Keep CPU halted in infinite loop

; ===================
; DATA SECTION
; ===================

boot_drive db 0    ; Reserve one byte to store BIOS boot drive number

stage_two_bootloader_dap:
    db 0x10    ; Dap size (16 bytes)
    db 0x00    ; Reserved byte (always 0)
    dw 0x10    ; Number of sectors to read (16 sectors)
    dw 0x8000    ; Destination memory offset
    dw 0x00    ; Destination memory segement
    dq 0x01    ; Starting disk sector (LBA 1)

times 510 - ($ - $$) db 0    ; Fill remaining bytes to reach 510 bytes 
dw 0xaa55    ; Boot signature