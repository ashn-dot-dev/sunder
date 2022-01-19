; BUILTIN CONSTANT VALUES
; =======================
__nil: equ 0

__EXIT_SUCCESS: equ 0
__EXIT_FAILURE: equ 1

__STDIN_FILENO:  equ 0
__STDOUT_FILENO: equ 1
__STDERR_FILENO: equ 2

__SYS_WRITE: equ 1
__SYS_EXIT:  equ 60

; BUILTIN DUMP SUBROUTINE
; =======================
; func dump(obj: T, obj_size: usize) void
;
; ## Stack
; +--------------------+ <- rbp + 0x18 + obj_size
; | obj (high bytes)   |
; | ...                |
; | obj (low bytes)    |
; +--------------------+ <- rbp + 0x18
; | obj_size           |
; +--------------------+ <- rbp + 0x10
; | return address     |
; +--------------------+
; | saved rbp          |
; +--------------------+ <- rbp
; | buf (high bytes)   |
; | ...                |
; | buf (low bytes)    |
; +--------------------+ <- rsp
;
; ## Registers
; r8  := obj_size
; r9  := obj_addr
; r10 := buf_size
; rsp := buf_addr (after alloca)
; r11 := obj_ptr
; r12 := obj_end
; r13 := buf_ptr
section .text
__dump:
    push rbp
    mov rbp, rsp

    ; r8 = obj_size
    mov r8, [rbp + 0x10]

    ; if obj_size == 0 { write(STDERR_FILENO, "\n", 1) then return }
    cmp r8, 0
    jne .setup
    mov rax, __SYS_WRITE
    mov rdi, __STDERR_FILENO
    mov rsi, __dump_nl_start
    mov rdx, __dump_nl_count
    syscall
    jmp .return

.setup:
    ; r9 = obj_addr
    mov r9, rbp
    add r9, 0x18

    ; r10 = buf_size = obj_size * 3
    mov r10, r8 ; obj_size
    imul r10, 3 ; obj_size * 3

    ; rsp = alloca(buf_size)
    sub rsp, r10

    ; r11 = obj_ptr
    mov r11, r9 ; obj_addr

    ; r12 = obj_end
    mov r12, r9 ; obj_addr
    add r12, r8 ; obj_addr + obj_size

    ; r13 = buf_ptr
    mov r13, rsp ; buf_addr

.loop:
    ; for obj_ptr != obj_end
    cmp r11, r12
    je .write

    ; Load the address of the two byte hex digit sequence corresponding to
    ; the value of *(:byte)obj_ptr into rax.
    ; rax = seq = lookup_table + *(:byte)obj_ptr * 2
    movzx rax, byte [r11]        ; *(:byte)obj_ptr
    imul rax, 2                  ; *(:byte)obj_ptr * 2
    add rax, __dump_lookup_table ; lookup_table + *(:byte)obj_ptr * 2

    ; *((:byte)buf_ptr + 0) = *((:byte)seq + 0)
    mov bl, [rax + 0]
    mov [r13 + 0], bl

    ; *((:byte)buf_ptr + 1) = *((:byte)seq + 1)
    mov bl, [rax + 1]
    mov [r13 + 1], bl

    ; *((:byte)buf_ptr + 2) = ' '
    mov byte [r13 + 2], 0x20 ; ' '

    ; obj_ptr += 1
    inc r11
    ; buf_ptr += 3
    add r13, 3

    jmp .loop

.write:
    ; buf_ptr -= 1
    dec r13
    ; *(:byte)buf_ptr = '\n'
    mov byte [r13], 0x0A ; '\n'

    ; write(STDERR_FILENO, buf, buf_size)
    mov rax, __SYS_WRITE
    mov rdi, __STDERR_FILENO
    mov rsi, rsp
    mov rdx, r10
    syscall

.return:
    mov rsp, rbp
    pop rbp
    ret

section .rodata
__dump_nl_start: db 0x0A
__dump_nl_count: equ 1
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
; ===========================================
section .text
__integer_oor_handler:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_WRITE
    mov rdi, __STDERR_FILENO
    mov rsi, __integer_oor_msg_start
    mov rdx, __integer_oor_msg_count
    syscall

    mov rax, __SYS_EXIT
    mov rdi, __EXIT_FAILURE
    syscall

section .rodata
__integer_oor_msg_start: db \
    "fatal: arithmetic operation produces out-of-range result", 0x0A
__integer_oor_msg_count: equ $ - __integer_oor_msg_start


; BUILTIN INTEGER DIVIDE BY ZERO HANDLER
; ======================================
section .text
__integer_divz_handler:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_WRITE
    mov rdi, __STDERR_FILENO
    mov rsi, __integer_divz_msg_start
    mov rdx, __integer_divz_msg_count
    syscall

    mov rax, __SYS_EXIT
    mov rdi, __EXIT_FAILURE
    syscall

section .rodata
__integer_divz_msg_start: db "fatal: divide by zero", 0x0A
__integer_divz_msg_count: equ $ - __integer_divz_msg_start


; BUILTIN INDEX OUT-OF-BOUNDS HANDLER
; ===================================
section .text
__index_oob_handler:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_WRITE
    mov rdi, __STDERR_FILENO
    mov rsi, __index_oob_msg_start
    mov rdx, __index_oob_msg_count
    syscall

    mov rax, __SYS_EXIT
    mov rdi, __EXIT_FAILURE
    syscall

section .rodata
__index_oob_msg_start: db "fatal: index out-of-bounds", 0x0A
__index_oob_msg_count: equ $ - __index_oob_msg_start


; SYS DEFINITIONS (lib/sys/sys.sunder)
; ====================================
section .data
sys.argc: dq 0 ; extern var argc: usize;
sys.argv: dq 0 ; extern var argv: **byte;


; PROGRAM ENTRY POINT
; ===================
section .text
global _start
_start:
    xor rbp, rbp   ; [SysV ABI] deepest stack frame
    mov rax, [rsp] ; [SysV ABI] argc @ rsp
    mov rbx, rsp   ; [SysV ABI] argv @ rsp + 8
    add rbx, 0x8   ; ...
    mov [sys.argc], rax ; sys.argc = SysV ABI argc
    mov [sys.argv], rbx ; sys.argv = SysV ABI argv
    call main
    mov rax, __SYS_EXIT
    mov rdi, __EXIT_SUCCESS
    syscall
