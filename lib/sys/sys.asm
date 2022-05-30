; BUILTIN CONSTANT VALUES
; =======================
__nil: equ 0

__EXIT_SUCCESS: equ 0
__EXIT_FAILURE: equ 1

__STDIN_FILENO:  equ 0
__STDOUT_FILENO: equ 1
__STDERR_FILENO: equ 2

__SYS_READ:   equ 0;
__SYS_WRITE:  equ 1
__SYS_OPEN:   equ 2;
__SYS_CLOSE:  equ 3;
__SYS_LSEEK:  equ 8;
__SYS_MMAP:   equ 9;
__SYS_MUNMAP: equ 11;
__SYS_FORK:   equ 57;
__SYS_EXECVE: equ 59;
__SYS_EXIT:   equ 60;
__SYS_WAIT4:  equ 61;

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
; +--------------------+ <- rbp + 0x08
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

; BUILTIN INTEGER DIVIDE BY ZERO HANDLER
; ======================================
section .text
__fatal_integer_divide_by_zero:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_WRITE
    mov rdi, __STDERR_FILENO
    mov rsi, __fatal_integer_divide_by_zero_msg_start
    mov rdx, __fatal_integer_divide_by_zero_msg_count
    syscall

    mov rax, __SYS_EXIT
    mov rdi, __EXIT_FAILURE
    syscall

; BUILTIN OUT-OF-RANGE INTEGER RESULT HANDLER
; ===========================================
section .text
__fatal_integer_out_of_range:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_WRITE
    mov rdi, __STDERR_FILENO
    mov rsi, __fatal_integer_out_of_range_msg_start
    mov rdx, __fatal_integer_out_of_range_msg_count
    syscall

    mov rax, __SYS_EXIT
    mov rdi, __EXIT_FAILURE
    syscall

section .rodata
__fatal_integer_out_of_range_msg_start: db "fatal: arithmetic operation produces out-of-range result", 0x0A
__fatal_integer_out_of_range_msg_count: equ $ - __fatal_integer_out_of_range_msg_start

section .rodata
__fatal_integer_divide_by_zero_msg_start: db "fatal: divide by zero", 0x0A
__fatal_integer_divide_by_zero_msg_count: equ $ - __fatal_integer_divide_by_zero_msg_start

; BUILTIN INDEX OUT-OF-BOUNDS HANDLER
; ===================================
section .text
__fatal_index_out_of_bounds:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_WRITE
    mov rdi, __STDERR_FILENO
    mov rsi, __fatal_index_out_of_bounds_msg_start
    mov rdx, __fatal_index_out_of_bounds_msg_count
    syscall

    mov rax, __SYS_EXIT
    mov rdi, __EXIT_FAILURE
    syscall

section .rodata
__fatal_index_out_of_bounds_msg_start: db "fatal: index out-of-bounds", 0x0A
__fatal_index_out_of_bounds_msg_count: equ $ - __fatal_index_out_of_bounds_msg_start

; SYS DEFINITIONS (lib/sys/sys.sunder)
; ====================================
section .data
sys.argc: dq 0 ; extern var argc: usize;
sys.argv: dq 0 ; extern var argv: **byte;
sys.envp: dq 0 ; extern var envp: **byte;

; Linux x64 syscall kernel interface format:
; + rax => [in] syscall number
;          [out] return value (negative indicates -ERRNO)
; + rdi => [in] parameter 1
; + rsi => [in] parameter 2
; + rdx => [in] parameter 3
; + r10 => [in] parameter 4
; + r8  => [in] parameter 5
; + r9  => [in] parameter 6

; linux/fs/read_write.c:
; SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
sys.read:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_READ
    mov rdi, [rbp + 0x20] ; fd
    mov rsi, [rbp + 0x18] ; buf
    mov rdx, [rbp + 0x10] ; count
    syscall
    mov [rbp + 0x28], rax

    mov rsp, rbp
    pop rbp
    ret

; linux/fs/read_write.c:
; SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf, size_t, count)
sys.write:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_WRITE
    mov rdi, [rbp + 0x20] ; fd
    mov rsi, [rbp + 0x18] ; buf
    mov rdx, [rbp + 0x10] ; count
    syscall
    mov [rbp + 0x28], rax

    mov rsp, rbp
    pop rbp
    ret

; linux/fs/open.c:
; SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, umode_t, mode)
sys.open:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_OPEN
    mov rdi, [rbp + 0x20] ; filename
    mov rsi, [rbp + 0x18] ; flags
    mov rdx, [rbp + 0x10] ; mode
    syscall
    mov [rbp + 0x28], rax

    mov rsp, rbp
    pop rbp
    ret

; linux/fs/open.c:
; SYSCALL_DEFINE1(close, unsigned int, fd)
sys.close:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_CLOSE
    mov rdi, [rbp + 0x10] ; fd
    syscall
    mov [rbp + 0x18], rax

    mov rsp, rbp
    pop rbp
    ret

; linux/fs/read_write.c:
; SYSCALL_DEFINE3(lseek, unsigned int, fd, off_t, offset, unsigned int, whence)
sys.lseek:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_LSEEK
    mov rdi, [rbp + 0x20] ; fd
    mov rsi, [rbp + 0x18] ; offset
    mov rdx, [rbp + 0x10] ; whence
    syscall
    mov [rbp + 0x28], rax

    mov rsp, rbp
    pop rbp
    ret

; arch/x86/kernel/sys_x86_64.c:
; SYSCALL_DEFINE6(mmap, unsigned long, addr, unsigned long, len, unsigned long, prot, unsigned long, flags, unsigned long, fd, unsigned long, off)
sys.mmap:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_MMAP
    mov rdi, [rbp + 0x38] ; addr
    mov rsi, [rbp + 0x30] ; len
    mov rdx, [rbp + 0x28] ; prot
    mov r10, [rbp + 0x20] ; flags
    mov r8,  [rbp + 0x18] ; fd
    mov r9,  [rbp + 0x10] ; off
    syscall
    mov [rbp + 0x40], rax

    mov rsp, rbp
    pop rbp
    ret

; linux/mm/mmap.c:
; SYSCALL_DEFINE2(munmap, unsigned long, addr, size_t, len)
sys.munmap:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_MUNMAP
    mov rdi, [rbp + 0x18] ; addr
    mov rsi, [rbp + 0x10] ; len
    syscall
    mov [rbp + 0x20], rax

    mov rsp, rbp
    pop rbp
    ret

; linux/kernel/fork.c:
; SYSCALL_DEFINE0(fork)
sys.fork:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_FORK
    syscall
    mov [rbp + 0x10], rax

    mov rsp, rbp
    pop rbp
    ret

; linux/fs/exec.c
; SYSCALL_DEFINE3(execve, const char __user *, filename, const char __user *const __user *, argv, const char __user *const __user *, envp)
sys.execve:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_EXECVE
    mov rdi, [rbp + 0x20] ; filename
    mov rsi, [rbp + 0x18] ; argv
    mov rdx, [rbp + 0x10] ; envp
    syscall
    mov [rbp + 0x28], rax

    mov rsp, rbp
    pop rbp
    ret

; linux/kernel/exit.c:
; SYSCALL_DEFINE1(exit, int, error_code)
sys.exit:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_EXIT
    mov rdi, [rbp + 0x10] ; error_code
    syscall

; linux/kernel/exit.c:
; SYSCALL_DEFINE4(wait4, pid_t, upid, int __user *, stat_addr, int, options, struct rusage __user *, ru)
sys.wait4:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_WAIT4
    mov rdi, [rbp + 0x28] ; upid
    mov rsi, [rbp + 0x20] ; stat_addr
    mov rdx, [rbp + 0x18] ; options
    mov r10, [rbp + 0x10] ; ru
    syscall
    mov [rbp + 0x30], rax

    mov rsp, rbp
    pop rbp
    ret

; PROGRAM ENTRY POINT
; ===================
%ifdef __entry
section .text
global _start
_start:
    xor rbp, rbp        ; [SysV ABI] deepest stack frame
    mov rax, [rsp]      ; [SysV ABI] argc @ rsp
    mov [sys.argc], rax
    mov rax, rsp        ; [SysV ABI] argv @ rsp + 8
    add rax, 0x8
    mov [sys.argv], rax
    mov rax, [sys.argc] ; [SysV ABI] envp @ rsp + 8 + argc * 8 + 8
    mov rbx, 0x8
    mul rbx
    add rax, rsp
    add rax, 0x10
    mov [sys.envp], rax
    call main
    mov rax, __SYS_EXIT
    mov rdi, __EXIT_SUCCESS
    syscall
%endif
