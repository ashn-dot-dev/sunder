; BUILTIN NIL POINTER / VALUE
__nil: equ 0

; BUILTIN DUMP SUBROUTINE
section .text
global dump
dump:
    push rbp
    mov rbp, rsp

    mov r15, [rbp + 0x10] ; r15 = size

    cmp r15, 0
    jne .setup
    mov rax, 1 ; SYS_WRITE
    mov rdi, 2 ; STDERR_FILENO
    mov rsi, __dump_nl
    mov rdx, 1
    syscall
    mov rsp, rbp
    pop rbp
    ret

.setup:
    mov r14, r15 ; r14 = size * 3
    imul r14, 3
    sub rsp, r14 ; buf = rsp = alloca(size * 3)

    mov r11, rsp ; ptr = r11 = buf
    mov r12, rbp ; cur = r12 = &obj
    add r12, 0x18
    mov r13, r12 ; end = r13 = &obj + size
    add r13, r15

.loop:
    cmp r12, r13 ; while (cur != end)
    je .write

    mov rax, [r12] ; repr = rax = dump_lookup_table + *cur * 2
    and rax, 0xFF
    imul rax, 2
    add rax, __dump_lookup_table

    mov bl, [rax + 0] ; *ptr = repr[0]
    mov [r11], bl
    inc r11 ; ptr += 1
    mov bl, [rax + 1] ; *ptr = repr[1]
    mov [r11], bl
    inc r11 ; ptr += 1
    mov bl, 0x20 ; *ptr = ' '
    mov byte [r11], bl
    inc r11 ; ptr += 1

    inc r12 ; cur += 1
    jmp .loop

.write:
    dec r11 ; ptr -= 1
    mov byte [r11], 0x0A ; *ptr = '\n'

    ; write(STDERR_FILENO, buf, size * 3)
    mov rax, 1 ; SYS_WRITE
    mov rdi, 2 ; STDERR_FILENO
    mov rsi, rsp
    mov rdx, r14
    syscall

    mov rsp, rbp
    pop rbp
    ret

section .rodata
__dump_nl: db 0x0A
__dump_lookup_table: db \
    '00', '01', '02', '03', '04', '05', '06', '07', \
    '08', '09', '0A', '0B', '0C', '0D', '0E', '0F', \
    '10', '11', '12', '13', '14', '15', '16', '17', \
    '18', '19', '1A', '1B', '1C', '1D', '1E', '1F', \
    '20', '21', '22', '23', '24', '25', '26', '27', \
    '28', '29', '2A', '2B', '2C', '2D', '2E', '2F', \
    '30', '31', '32', '33', '34', '35', '36', '37', \
    '38', '39', '3A', '3B', '3C', '3D', '3E', '3F', \
    '40', '41', '42', '43', '44', '45', '46', '47', \
    '48', '49', '4A', '4B', '4C', '4D', '4E', '4F', \
    '50', '51', '52', '53', '54', '55', '56', '57', \
    '58', '59', '5A', '5B', '5C', '5D', '5E', '5F', \
    '60', '61', '62', '63', '64', '65', '66', '67', \
    '68', '69', '6A', '6B', '6C', '6D', '6E', '6F', \
    '70', '71', '72', '73', '74', '75', '76', '77', \
    '78', '79', '7A', '7B', '7C', '7D', '7E', '7F', \
    '80', '81', '82', '83', '84', '85', '86', '87', \
    '88', '89', '8A', '8B', '8C', '8D', '8E', '8F', \
    '90', '91', '92', '93', '94', '95', '96', '97', \
    '98', '99', '9A', '9B', '9C', '9D', '9E', '9F', \
    'A0', 'A1', 'A2', 'A3', 'A4', 'A5', 'A6', 'A7', \
    'A8', 'A9', 'AA', 'AB', 'AC', 'AD', 'AE', 'AF', \
    'B0', 'B1', 'B2', 'B3', 'B4', 'B5', 'B6', 'B7', \
    'B8', 'B9', 'BA', 'BB', 'BC', 'BD', 'BE', 'BF', \
    'C0', 'C1', 'C2', 'C3', 'C4', 'C5', 'C6', 'C7', \
    'C8', 'C9', 'CA', 'CB', 'CC', 'CD', 'CE', 'CF', \
    'D0', 'D1', 'D2', 'D3', 'D4', 'D5', 'D6', 'D7', \
    'D8', 'D9', 'DA', 'DB', 'DC', 'DD', 'DE', 'DF', \
    'E0', 'E1', 'E2', 'E3', 'E4', 'E5', 'E6', 'E7', \
    'E8', 'E9', 'EA', 'EB', 'EC', 'ED', 'EE', 'EF', \
    'F0', 'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', \
    'F8', 'F9', 'FA', 'FB', 'FC', 'FD', 'FE', 'FF'

; BUILTIN OUT-OF-RANGE INTEGER RESULT HANDLER
section .text
__integer_oor_handler:
    push rbp
    mov rbp, rsp

    mov rax, 1 ; SYS_WRITE
    mov rdi, 2 ; STDERR_FILENO
    mov rsi, __integer_oor_msg_start
    mov rdx, __integer_oor_msg_count
    syscall

    mov rax, 60 ; exit
        mov rdi, 1 ; EXIT_FAILURE
    syscall

section .rodata
__integer_oor_msg_start: db\
    "fatal: arithmetic operation produces out-of-range result", 0x0A
__integer_oor_msg_count: equ $ - __integer_oor_msg_start

; BUILTIN INTEGER DIVIDE BY ZERO HANDLER
section .text
__integer_divz_handler:
    push rbp
    mov rbp, rsp

    mov rax, 1 ; SYS_WRITE
    mov rdi, 2 ; STDERR_FILENO
    mov rsi, __integer_divz_msg_start
    mov rdx, __integer_divz_msg_count
    syscall

    mov rax, 60 ; exit
        mov rdi, 1 ; EXIT_FAILURE
    syscall

section .rodata
__integer_divz_msg_start: db "fatal: divide by zero", 0x0A
__integer_divz_msg_count: equ $ - __integer_divz_msg_start

; BUILTIN INDEX OUT-OF-BOUNDS HANDLER
section .text
__index_oob_handler:
    push rbp
    mov rbp, rsp

    mov rax, 1 ; SYS_WRITE
    mov rdi, 2 ; STDERR_FILENO
    mov rsi, __index_oob_msg_start
    mov rdx, __index_oob_msg_count
    syscall

    mov rax, 60 ; exit
        mov rdi, 1 ; EXIT_FAILURE
    syscall

section .rodata
__index_oob_msg_start: db "fatal: index out-of-bounds", 0x0A
__index_oob_msg_count: equ $ - __index_oob_msg_start

; SYS DEFINITIONS
section .data
sys.argc: dq 0 ; extern var argc: usize;
sys.argv: dq 0 ; extern var argv: **byte;

; PROGRAM ENTRY POINT
section .text
global _start
_start:
    xor rbp, rbp    ; [SysV ABI] deepest stack frame
    mov rax, [rsp]  ; [SysV ABI] argc @ rsp
    mov rbx, rsp    ; [SysV ABI] argv @ rsp + 8
    add rbx, 0x8    ; ...
    mov [sys.argc], rax ; sys.argc = SysV ABI argc
    mov [sys.argv], rbx ; sys.argv = SysV ABI argv
    call main
    mov rax, 60 ; exit
    mov rdi, 0  ; EXIT_SUCCESS
    syscall
