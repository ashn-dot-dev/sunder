; BUILTIN CONSTANT VALUES
; =======================
__nil: equ 0

__EXIT_SUCCESS: equ 0
__EXIT_FAILURE: equ 1

__SYS_EXIT:  equ 60

; BUILTIN DUMP SUBROUTINE
; =======================
section .text
__dump:
    ret

; BUILTIN INTEGER DIVIDE BY ZERO HANDLER
; ======================================
section .text
__fatal_integer_divide_by_zero:
    mov rax, __SYS_EXIT
    mov rdi, __EXIT_FAILURE
    syscall

; BUILTIN OUT-OF-RANGE INTEGER RESULT HANDLER
; ===========================================
section .text
__fatal_integer_out_of_range:
    mov rax, __SYS_EXIT
    mov rdi, __EXIT_FAILURE
    syscall

; BUILTIN INDEX OUT-OF-BOUNDS HANDLER
; ===================================
section .text
__fatal_index_out_of_bounds:
    mov rax, __SYS_EXIT
    mov rdi, __EXIT_FAILURE
    syscall

; PROGRAM ENTRY POINT
; ===================
%ifdef __entry
section .text
global _start
_start:
    call main
    mov rax, __SYS_EXIT
    mov rdi, __EXIT_SUCCESS
    syscall
%endif
