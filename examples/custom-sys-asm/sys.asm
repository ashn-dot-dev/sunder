; BUILTIN DUMP SUBROUTINE
; =======================
section .text
__dump:
    ret

; BUILTIN FATAL SUBROUTINE
; ========================
section .text
__fatal:
    mov rax, 60 ; SYS_EXIT
    mov rdi, 1  ; EXIT_FAILURE
    syscall

; PROGRAM ENTRY POINT
; ===================
%ifdef __entry
section .text
global _start
_start:
    call main
    mov rax, 60 ; SYS_EXIT
    mov rdi, 0  ; EXIT_SUCCESS
    syscall
%endif
