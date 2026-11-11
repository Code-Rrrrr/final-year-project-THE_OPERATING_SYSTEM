; Kernel
; Loaded at memory loacation 0x10000

bits 64
org 0x10000

kernel_entry:
    ; Add two numbers.
    mov rax, 2
    mov rbx, 3
    add rax, rbx
    mov r12, rax

    ; VGA text buffer: physical 0xB8000.
    mov rdi, 0xB8000
    lea rsi, [rel message]

.print:
    lodsb
    test al, al
    jz .print_result

    mov ah, 0x0F       ; White foreground, black background
    mov [rdi], ax
    add rdi, 2
    jmp .print

.print_result:
    mov rax, r12
    add al, '0'        ; This example's result is one digit.
    mov ah, 0x0F
    mov [rdi], ax

.halt:
    cli
    hlt
    jmp .halt

message db '2 + 3 = ', 0

times 512-($-$$) db 0