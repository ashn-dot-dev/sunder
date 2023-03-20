; BUILTIN CONSTANT VALUES
; =======================
__EXIT_SUCCESS: equ 0
__EXIT_FAILURE: equ 1

__STDIN_FILENO:  equ 0
__STDOUT_FILENO: equ 1
__STDERR_FILENO: equ 2

__SYS_READ:      equ 0
__SYS_WRITE:     equ 1
__SYS_OPEN:      equ 2
__SYS_CLOSE:     equ 3
__SYS_LSEEK:     equ 8
__SYS_MMAP:      equ 9
__SYS_MUNMAP:    equ 11
__SYS_EXIT:      equ 60
__SYS_GETDENTS   equ 78
__SYS_MKDIR      equ 83
__SYS_RMDIR      equ 84
__SYS_UNLINK     equ 87
__SYS_GETDENTS64 equ 217

; BUILTIN FATAL SUBROUTINE
; ========================
; func fatal(msg_start: *byte, msg_count: usize) void
;
; ## Stack
; +--------------------+ <- rbp + 0x20
; | msg_start          |
; +--------------------+ <- rbp + 0x18
; | msg_count          |
; +--------------------+ <- rbp + 0x10
; | return address     |
; +--------------------+ <- rbp + 0x08
; | saved rbp          |
; +--------------------+ <- rbp
section .text
__fatal:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_WRITE
    mov rdi, __STDERR_FILENO
    mov rsi, __fatal_preamble_start
    mov rdx, __fatal_preamble_count
    syscall

    mov rax, __SYS_WRITE
    mov rdi, __STDERR_FILENO
    mov rsi, [rbp + 0x18] ; msg_start
    mov rdx, [rbp + 0x10] ; msg_count
    syscall

    mov rax, __SYS_EXIT
    mov rdi, __EXIT_FAILURE
    syscall

__fatal_preamble_start: db "fatal: "
__fatal_preamble_count: equ $ - __fatal_preamble_start;

; SYS DEFINITIONS (lib/sys/sys.sunder)
; ====================================
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
section .text
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
section .text
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
section .text
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
section .text
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
section .text
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
section .text
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
section .text
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

; linux/kernel/exit.c:
; SYSCALL_DEFINE1(exit, int, error_code)
section .text
sys.exit:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_EXIT
    mov rdi, [rbp + 0x10] ; error_code
    syscall

; linux/fs/readdir.c:
; SYSCALL_DEFINE3(getdents, unsigned int, fd, struct linux_dirent __user *, dirent, unsigned int, count)
section .text
sys.getdents:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_GETDENTS
    mov rdi, [rbp + 0x20] ; fd
    mov rsi, [rbp + 0x18] ; dirent
    mov rdx, [rbp + 0x10] ; count
    syscall
    mov [rbp + 0x28], rax

    mov rsp, rbp
    pop rbp
    ret

; linux/fs/namei.c:
; SYSCALL_DEFINE2(mkdir, const char __user *, pathname, umode_t, mode)
section .text
sys.mkdir:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_MKDIR
    mov rdi, [rbp + 0x18] ; pathname
    mov rsi, [rbp + 0x10] ; mode
    syscall
    mov [rbp + 0x20], rax

    mov rsp, rbp
    pop rbp
    ret

; linux/fs/namei.c:
; SYSCALL_DEFINE1(rmdir, const char __user *, pathname)
section .text
sys.rmdir:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_RMDIR
    mov rdi, [rbp + 0x10] ; pathname
    syscall
    mov [rbp + 0x18], rax

    mov rsp, rbp
    pop rbp
    ret

; linux/fs/namei.c:
; SYSCALL_DEFINE1(unlink, const char __user *, pathname)
section .text
sys.unlink:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_UNLINK
    mov rdi, [rbp + 0x10] ; pathname
    syscall
    mov [rbp + 0x18], rax

    mov rsp, rbp
    pop rbp
    ret

; linux/fs/readdir.c:
; SYSCALL_DEFINE3(getdents64, unsigned int, fd, struct linux_dirent64 __user *, dirent, unsigned int, count)
section .text
sys.getdents64:
    push rbp
    mov rbp, rsp

    mov rax, __SYS_GETDENTS64
    mov rdi, [rbp + 0x20] ; fd
    mov rsi, [rbp + 0x18] ; dirent
    mov rdx, [rbp + 0x10] ; count
    syscall
    mov [rbp + 0x28], rax

    mov rsp, rbp
    pop rbp
    ret

section .data
sys.argc: dq 0 ; extern var argc: usize;
sys.argv: dq 0 ; extern var argv: **byte;
sys.envp: dq 0 ; extern var envp: **byte;

; SYS DUMP SUBROUTINE (lib/sys/sys.sunder)
; ========================================
; ## Stack
; +--------------------+ <- rbp + 0x20
; | object_addr        |
; +--------------------+ <- rbp + 0x18
; | object_size        |
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
; r8  := object_size
; r9  := object_addr
; r10 := buffer_size
; rsp := buffer_addr (after alloca)
; r11 := object_ptr
; r12 := object_end
; r13 := buffer_ptr
section .text
sys.dump_bytes:
    push rbp
    mov rbp, rsp

    ; r8 := object_size
    mov r8, [rbp + 0x10]

    ; If the object size is zero then emit a single "\n" and return.
    cmp r8, 0
    jne .setup
    mov rax, __SYS_WRITE
    mov rdi, __STDERR_FILENO
    mov rsi, sys._dump_nl_start
    mov rdx, sys._dump_nl_count
    syscall
    jmp .return

.setup:
    ; r9 := object_addr
    mov r9, rbp
    add r9, 0x18
    mov r9, [r9]

    ; r10 := buffer_size := object_size * 3
    mov r10, r8 ; object_size
    imul r10, 3 ; object_size * 3

    ; rsp := alloca(buffer_size)
    sub rsp, r10

    ; r11 := object_ptr
    mov r11, r9 ; object_addr

    ; r12 := object_end
    mov r12, r9 ; object_addr
    add r12, r8 ; object_addr + object_size

    ; r13 := buffer_ptr
    mov r13, rsp ; buffer_addr

.loop:
    ; for object_ptr != object_end
    cmp r11, r12
    je .write

    ; Load the address of the two byte hex digit sequence corresponding to the
    ; value of byte at the object pointer into rax.
    ;
    ; rax := seq := lookup_table + *object_ptr * 2
    movzx rax, byte [r11]           ; *object_ptr
    imul rax, 2                     ; *object_ptr * 2
    add rax, sys._dump_lookup_table ; *object_ptr * 2 + lookup_table

    ; *(buffer_ptr + 0) = *(seq + 0)
    mov bl, [rax + 0]
    mov [r13 + 0], bl

    ; *(buffer_ptr + 1) = *(seq + 1)
    mov bl, [rax + 1]
    mov [r13 + 1], bl

    ; *(buffer_ptr + 2) = ' '
    mov byte [r13 + 2], 0x20 ; ' '

    ; object_ptr += 1
    inc r11
    ; buffer_ptr += 3
    add r13, 3

    jmp .loop

.write:
    ; buffer_ptr -= 1
    dec r13
    ; *buffer_ptr = '\n'
    mov byte [r13], 0x0A ; '\n'

    ; write(STDERR_FILENO, buffer, buffer_size)
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
sys._dump_nl_start: db 0x0A
sys._dump_nl_count: equ 1
sys._dump_lookup_table: db \
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

section .text
sys.str_to_f32: call __fatal_unimplemented
sys.str_to_f64: call __fatal_unimplemented

sys.f32_to_str: call __fatal_unimplemented
sys.f64_to_str: call __fatal_unimplemented

sys.f32_sqrt: call __fatal_unimplemented
sys.f64_sqrt: call __fatal_unimplemented
sys.f32_cbrt: call __fatal_unimplemented
sys.f64_cbrt: call __fatal_unimplemented
sys.f32_hypot: call __fatal_unimplemented
sys.f64_hypot: call __fatal_unimplemented
sys.f32_pow: call __fatal_unimplemented
sys.f64_pow: call __fatal_unimplemented

sys.f32_sin: call __fatal_unimplemented
sys.f64_sin: call __fatal_unimplemented
sys.f32_cos: call __fatal_unimplemented
sys.f64_cos: call __fatal_unimplemented
sys.f32_tan: call __fatal_unimplemented
sys.f64_tan: call __fatal_unimplemented
sys.f32_asin: call __fatal_unimplemented
sys.f64_asin: call __fatal_unimplemented
sys.f32_acos: call __fatal_unimplemented
sys.f64_acos: call __fatal_unimplemented
sys.f32_atan: call __fatal_unimplemented
sys.f64_atan: call __fatal_unimplemented
sys.f32_atan2: call __fatal_unimplemented
sys.f64_atan2: call __fatal_unimplemented

sys.f32_sinh: call __fatal_unimplemented
sys.f64_sinh: call __fatal_unimplemented
sys.f32_cosh: call __fatal_unimplemented
sys.f64_cosh: call __fatal_unimplemented
sys.f32_tanh: call __fatal_unimplemented
sys.f64_tanh: call __fatal_unimplemented
sys.f32_asinh: call __fatal_unimplemented
sys.f64_asinh: call __fatal_unimplemented
sys.f32_acosh: call __fatal_unimplemented
sys.f64_acosh: call __fatal_unimplemented
sys.f32_atanh: call __fatal_unimplemented
sys.f64_atanh: call __fatal_unimplemented

sys.f32_ceil: call __fatal_unimplemented
sys.f64_ceil: call __fatal_unimplemented
sys.f32_floor: call __fatal_unimplemented
sys.f64_floor: call __fatal_unimplemented
sys.f32_trunc: call __fatal_unimplemented
sys.f64_trunc: call __fatal_unimplemented
sys.f32_round: call __fatal_unimplemented
sys.f64_round: call __fatal_unimplemented

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
