; Stage 2 bootloader
; Loaded at memory location 0x8000

; ===================
; CODE SECTION
; ===================

bits 16    ; Assemble instructions for 16-bit mode
org 0x8000    ; Set origin address to 0x8000

main:
    cli    ; Disable maskable hardware interrupts
    xor ax, ax    ; Clear AX register
    mov ds, ax    ; Initialize data segment
    mov es, ax    ; Initialize extra segment
    mov ss, ax    ; Initialize stack segment
    mov sp, 0x7c00    ; Set stack pointer to address 0x7c00
    cld    ; Clear direction flag for forward string opeartions

    mov [boot_drive], dl    ; Save BIOS boot drive number from DL


    ;----------------
    ; Get memory map
    ;----------------

    ; Start at 0x6000
    ; Maximum entries = 128
    ; Each entry is 24 bytes
    ; 128×24=3072 bytes
    ; End at 0x6bff

    xor ebx, ebx    ; Clear EBX, first call
    mov di, 0x6000    ; Destination address offset where memory map entires will be stored
    mov byte [memory_map_count], 0    ; Initialize number of retrieved memory map entries

    .get_next_entry:
        cmp byte [memory_map_count], 128    ; Check if maximum entires is reached 
        jae .error    ; Jump to error if maximum limit of entries is reached

        mov dword [es:di + 20], 1    ; Initialize extended attributes

        mov eax, 0xe820    ; Select BIOS get memory map function
        mov ecx, 24    ; Output buffer size
        mov edx, 0x534d4150    ; Signature (SMAP)
        int 0x15    ; Call BIOS service

        jc .error    ; Jump to error if operation fails, CF = 0 means success, CF = 1 means failure

        cmp eax, 0x534d4150    ; Compare retured signature
        jne .error    ; Jump to error if returned signature do not match

        cmp ecx, 20    ; Verify that returned entry size contains atleast 20 bytes
        jb .error    ; Jump to error if returned entry size is less than 20 bytes

        inc byte [memory_map_count]    ; Count returned entry

        add di, 24    ; Next entry

        test ebx, ebx    ; Check if EBX = 0, EBX = 0 means no more entries
        jnz .get_next_entry    ; If EBX is not zero then loop
        jmp .continue

    .error:
        ; Memory map retrieval failed
        jmp hang

    .continue:

        mov dword [0x5000], 0x6000
        mov dword [0x5004], 0

        ; Memory_map_count is defined in stage 2 bootloader 
        ; It is not automatically accessible as a symbol in kernel_entry.asm
        ; So 0x5008 is defined as shared memoryt location
        mov al, [memory_map_count]
        mov [0x5008], al

        

    ;-------------
    ; Load kernel
    ;-------------

    mov si, kernel_dap    ; Point SI to DAP (disk address packet)
    mov ah, 0x42    ; Select BIOS extended disk read function
    mov dl, [boot_drive]    ; Load saved boot drive number
    int 0x13    ; Call BIOS disk service to read sectors
    
    jc hang    ; Jump to hang if the disk read fails

    ;-----------------
    ; Enable A20 line
    ;-----------------

    in al, 0x92    ; Read system control port A (system control port A = port 0x92)
    or al, 00000010b    ; Set bit 1 to enable A20 line
    and al, 11111110b    ; Clear bit 0 to prevent a fast reset
    out 0x92, al    ; Write updated value back to port 0x92

    ;----------
    ; Load GDT
    ;----------

    lgdt [gdt_descriptor]    ; Load the GDT (Global Descriptor Table) register

    ;-----------------------------
    ; Enter 32-bit protected mode
    ;-----------------------------

    mov eax, cr0    ; Load value of CR0 control register
    or eax, 1    ; Set bit 0 to enable protected mode
    mov cr0, eax    ; Write updated value to CR0

    jmp 0x08:protected_mode_entry    ; Far jump to 32-bit code segment

;-----------------------
; 32-bit protected mode
;-----------------------

bits 32    ; Assemble instructions for 32-bit mode

protected_mode_entry:

    ; Load data selector into all data segment registers
    mov ax, 0x10    ; Load data segment selector
    mov ds, ax    ; Initialize data segment
    mov es, ax    ; Initialize extra segment
    mov fs, ax    ; Initialize FS segment
    mov gs, ax    ; Initialize GS segment
    mov ss, ax    ; Initialize stack segment

    ; Initialize stack
    mov esp, 0x80000   ; Set 32-bit stack pointer
    mov ebp, esp    ; Set base pointer to stack pointer

;-------------------------
; Check long mode support
;-------------------------

;(to be implemented in future)

;------------------------
; Setup 4 KB page tables
;------------------------

; Clear page tables
mov edi, 0x1000    ; Point EDI to beginning of page tables
xor eax, eax   ; Set EAX to zero
mov ecx, 4096  ; Set number of double words to clear
cld    ; Clear direction flag for forward string opeartions
rep stosd    ; Clear 4096 double words starting at EDI

; PML4[0] -> PDPT
; Present + Read/Write
mov dword [0x1000], 0x2003 ; Point first PML4 entry to the PDPT

; PDPT[0] -> PD
mov dword [0x2000], 0x3003 ; Point first PDPT entry to the PD 

; PD[0] -> PT
mov dword [0x3000], 0x4003 ; Point first PD entry to the PT

; Map 512 x 4 KB pages
; Total = 2 MB
mov edi, 0x4000    ; Point EDI to PT
mov eax, 0x00000003    ; Set initial page address and present + read/write flags
mov ecx, 512    ; Set number of PT entries

.map_pages:
    mov dword [edi], eax    ; Store lower 32 bits of PTE
    mov dword [edi + 4], 0    ; Clear upper 32 bits of PTE
    add eax, 0x1000    ; Advance to next 4 KB physical page
    add edi, 8    ; Move to next 8 byte PTE
    loop .map_pages    ; Repeat until all 512 pages are mapped

;---------------------------------------------------------
; Enable PAE (CR4.PAE = 1, required for long mode paging)
;---------------------------------------------------------

mov eax, cr4    ; Read CR4 control register
or eax, 1 << 5    ; Set bit 5 to enable PAE (Physical Address Extension)
mov cr4, eax    ; Write updated value to CR4

;----------------------------
; Load PML4 address into CR3
;----------------------------

mov eax, 0x1000    ; Load physical address of PML4 table
mov cr3, eax    ; Set CR3 to point to PML4 table

;--------------------------
; Enable long mode in EFER
;--------------------------

mov ecx, 0xC0000080    ; Select EFER model specific register
rdmsr    ; Read current EFER value into EDX:EAX
or eax, 1 << 8    ; Set bit 8 to enable long mode
wrmsr    ; Write updated EFER value

;---------------
; Enable paging
;---------------

mov eax, cr0    ; Read CR0 control register
or eax, (1 << 31) | 1     ; Enable paging and ensure protected mode is enabled
mov cr0, eax    ; Write updated value to CR0

;-------------------------
; Far jump to 64-bit mode
;-------------------------

jmp 0x18:long_mode_entry    ; Jump to the 64-bit code segment

;------------------
; 64-bit long mode
;------------------

bits 64    ; Assemble instructions for 64-bit mode

long_mode_entry:
    xor ax, ax    ; Clear AX register
    mov ds, ax    ; Initialize data segment
    mov es, ax    ; Initialize extra segment
    mov ss, ax    ; Initialize stack segment

    mov rsp, 0x80000    ; Initialize 64-bit stack pointer
    xor rbp, rbp    ; Clear base pointer
    cld    ; Clear direction flag for forward string opeartions

    ; Transfer control to the 64-bit kernel
    mov rax, 0x10000    ; Load kernel entry address into RAX
    jmp rax    ; Transfer control to kernel

hang:
    hlt    ; Halt CPU until next interrupt
    jmp hang    ; Keep CPU halted in infinite loop

; ===================
; DATA SECTION
; ===================

boot_drive db 0    ; Reserve one byte to store BIOS boot drive number

memory_map_count db 0    ; Reserve counter for number of memory map entries

kernel_dap:
    db 0x10    ; DAP size (16 bytes)
    db 0x00    ; Reserved byte (always 0)
    dw 0x80    ; Number of sectors to read (80 sector)
    dw 0x00    ; Destination memory offset
    dw 0x1000   ; Destination memory segment
    dq 0x11    ; Starting disk sector (LBA 17)


gdt_start:    ; Start of GDT
    dq 0x0000000000000000 ; Null GDT descriptor
    dq 0x00CF9A000000FFFF ; 32-bit code segment descriptor (0x08)
    dq 0x00CF92000000FFFF ; Data segment descriptor (0x10) 
    dq 0x00AF9A000000FFFF ; 64-bit code segment descriptor (0x18)

gdt_end:    ; End of GDT

gdt_descriptor:
    dw gdt_end - gdt_start - 1    ; Store GDT size - 1
    dd gdt_start    ; Store address of GDT

times 8192 - ($ - $$) db 0    ; Pad to exactly 8192 bytes

