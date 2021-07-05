; Scratchpad to test things while learning NASM.
; This assembly will only work on x86-64 Linux.
;
; SYSTEM V CALLING CONVENTION
; ===========================
; REGISTER | DESCRIPTION
; ---------+------------
; rax      | caller-saved, return value
; rbx      | callee-saved
; rcx      | caller-saved, parameter 4
; rdx      | caller-saved, parameter 3
; rsi      | caller-saved, parameter 2
; rdi      | caller-saved, parameter 1
; rbp      | callee saved
; rsp      | caller-saved, stack pointer
; r8       | caller-saved, parameter 5
; r9       | caller-saved, parameter 6
; r10      | caller-saved
; r11      | caller-saved
; r12      | callee-saved
; r13      | callee-saved
; r14      | callee-saved
; r15      | callee-saved
;
; CALLING A SUBROUTINE
; ====================
;  1. Caller saves the values of caller-saved registers.
;     The caller saved-registers are rax, rcx, rdx, rdi, rdi, rsi, rsp, r8, r9,
;     r10, and r11 (rax, parameter registers, and the stack pointer).
;  2. If there are less than 6 parameters then the 6 parameters are passed to a
;     subroutine in the registers rdi, rsi, rdx, rcx, r8, and r9 in that order.
;     If there are more than 6 parameters then remaining parameters are pushed
;     onto the stack from right-to-left, meaning that the lowest parameter on
;     the stack is really the 7th parameter to a subroutine call.
;  3. The call instruction is executed.
;  4. Callee allocates space for local variables on the stack.
;  5. Callee saves the values of callee-saved registers.
;     The callee-saved registers are rbx, rbp, r12, r13, r14, and r15.
;  6. Callee executes, and the return value of the callee is put into rax.
;  7. Callee restores the values of callee-saved registers.
;  8. Callee deallocates local variable stack space.
;  9. The ret instruction is executed.
; 10. Caller restores the stack by popping off excess callee-parameters.
; 11. Caller restores the values of caller-saved registers.
;
; NOTE: The x86-64 System V ABI requires rsp to be aligned to a 16-byte boundary
; when the call instruction is executed (section 3.2.2 "The Stack Frame"). The
; call instruction pushes the return address (8 bytes) onto the stack, so when
; a subroutine begins execution the stack will be 8-byte aligned (i.e. no longer
; 16-byte aligned) and the callee must align their frame if necessary.
;
; Resources:
; + https://cs.lmu.edu/~ray/notes/nasmtutorial/
; + https://www.nasm.us/doc/
; + https://gitlab.com/mcmfb/intro_x86-64

section .rodata
EXIT_SUCCESS: equ 0
EXIT_FAILURE: equ 1
STDIN_FILENO: equ 0
STDOUT_FILENO: equ 1
STDERR_FILENO: equ 2
SYS_READ: equ 0
SYS_WRITE: equ 1
SYS_OPEN: equ 2
SYS_CLOSE: equ 3
SYS_EXIT: equ 60

hello_start: db "Hello, World!", 0x0A
hello_count: equ $ - hello_start

section .data
exit_status: dq EXIT_FAILURE

section .text
global main
main:
    push rbp
    mov rbp, rsp

    ; write(STDOUT_FILENO, hello_start, hello_count);
    mov rax, SYS_WRITE
    mov rdi, STDOUT_FILENO
    mov rsi, hello_start
    mov rdx, hello_count
    syscall

    ; exit_status = EXIT_SUCCESS;
    mov rax, EXIT_SUCCESS
    mov [exit_status], rax

    ; exit_status = subtract(5, 3)
    mov rax, 0xBADC0FFEE0DDF00D ; return value space
    push rax
    push 5 ; lhs
    push 3 ; rhs
    mov rax, subtract
    push rax
    pop rax
    call rax
    pop rax ; pop rhs
    pop rax ; pop lhs
    pop rax ; pop return value
    mov [exit_status], rax

    ; dump(0xBADCAFE01DC0FFEE, sizeof(i64))
    mov rax, 0xBADCAFE01DC0FFEE
    push rax ; push i64 literal
    push 0x8 ; push sizeof i64
    call dump
    pop rax ; pop i64 literal
    pop rax ; pop sizeof i64

    ; exit(exit_status);
    mov rax, SYS_EXIT
    mov rdi, [exit_status]
    syscall

    mov rsp, rbp
    pop rbp
    ret


; i64 subtract(i64 lhs, i64 rhs);
; Computes `lhs - rhs`.
; Stack-only calling convention, arguments and return values are pushed in left
; to right order.
global subtract
subtract:
    push rbp
    mov rbp, rsp

    ; 0x10 is the offset past the saved rbp and return address placed on the
    ; stack via the `call` instruction and function prologue. Caller stack
    ; allocation size is `i64 + i64 + i64 == 0x18` bytes mapped as such:
    ;
    ; +--------------------+ <- rbp + 0x10 + 0x18
    ; | return value       |
    ; +--------------------+
    ; | argument 1 (lhs)   |
    ; +--------------------+
    ; | argument 2 (rhs)   |
    ; +--------------------+ <- rbp + 0x10
    ; | return address     |
    ; +--------------------+
    ; | saved rbp          |
    ; +--------------------+ <- rbp
    ; | locals             |
    ;
    mov rax, [rbp + 0x10 + 0x18 - 0x10] ; lhs
    mov rbx, [rbp + 0x10 + 0x18 - 0x18] ; rhs
    sub rax, rbx
    mov [rbp + 0x10 + 0x18 - 0x8], rax ; return value

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

; void dump(T obj, i64 size);
; Push an size bytes of obj followed by the size parameter onto the stack.
; Dumps size bytes from obj to stdout (as a hexdump) followed by a newline.
section .text
global dump
dump:
    ; STACK
    ; =====
    ; +--------------------+ <- rbp + 0x18 + size
    ; | obj (high bytes)   |
    ; | ...                |
    ; | obj (low bytes)    |
    ; +--------------------+ <- rbp + 0x18
    ; | size               |
    ; +--------------------+ <- rbp + 0x10
    ; | return address     |
    ; +--------------------+
    ; | saved rbp          |
    ; +--------------------+ <- rbp
    ; | buf (high bytes)   |
    ; | ...                |
    ; | buf (low bytes)    |
    ; +--------------------+ <- rsp

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

    mov rax, [r12] ; repr = rax = dump_lookup_table + (*(uint8_t*)cur * 2)
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

section .text
global _start
_start:
    call main
    mov rax, 60
    mov rdi, EXIT_SUCCESS
    syscall
