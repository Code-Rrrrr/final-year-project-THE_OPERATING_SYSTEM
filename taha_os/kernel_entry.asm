bits 64

global kernel_entry    ; make kernel_entry globally visible to linker
extern kernel_main   ; tell NASM that kernel_main is defind somewhere else

kernel_entry:

    cli    ; disable maskable hardware interrupts

    ; Disable BIOS cursor
    mov dx, 0x3D4      ; VGA CRT Controller index port
    mov al, 0x0A       ; Cursor Start Register
    out dx, al

    mov dx, 0x3D5      ; VGA CRT Controller data port
    mov al, 0x20       ; Set bit 5 to disable cursor
    out dx, al

    mov edi, 0x5000    ; first argument

    and rsp, -16    ; align stack pointer to 16-byte boundary

    call kernel_main    ; jump to address of kernel main

.hang:
    hlt
    jmp .hang