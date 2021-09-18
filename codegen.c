// Copyright 2021 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "sunder.h"

static struct autil_string* out = NULL;
static struct tir_function const* current_function = NULL;
static size_t unique_id = 0; // Used for generating unique names and labels.

static void
append(char const* fmt, ...);
static void
appendln(char const* fmt, ...);
static void
appendli(char const* fmt, ...);
static void
appendli_location(struct source_location const* location, char const* fmt, ...);
static void
appendch(char ch);
// NASM Dx data type (db, dw, dq, etc.).
// https://nasm.us/doc/nasmdoc3.html#section-3.2.1
static void
append_dx_type(struct type const* type);
// NASM Dx data elements forming a static initializer.
// https://nasm.us/doc/nasmdoc3.html#section-3.2.1
static void
append_dx_data(struct value const* value);

// All push_* functions align rsp to an 8-byte boundary.
static void
push(size_t size);
static void
push_address(struct address const* address);
static void
push_at_address(size_t size, struct address const* address);
// The pop function will round size up to an 8-byte boundary to match the push_*
// functions so that one push/pop pair will restore the stack to it's previous
// state.
static void
pop(size_t size);

// Register al, ax, eax, or rax based on size.
char const*
reg_a(size_t size);
// Register bl, bx, ebx, or rbx based on size.
char const*
reg_b(size_t size);

// Copy size bytes from the address in rax to to the address in rbx using rcx
// for intermediate storage. Roughly equivalent to memcpy(rbx, rax, size).
static void
copy_rax_rbx_via_rcx(size_t size);
// Copy size bytes from the address in rsp to to the address in rbx using rcx
// for intermediate storage. Roughly equivalent to memcpy(rbx, rsp, size).
static void
copy_rsp_rbx_via_rcx(size_t size);
// Copy size bytes from the address in rax to to the address in rsp using rcx
// for intermediate storage. Roughly equivalent to memcpy(rsp, rax, size).
static void
copy_rax_rsp_via_rcx(size_t size);

static void
append(char const* fmt, ...)
{
    assert(out != NULL);
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);
    autil_string_append_vfmt(out, fmt, args);
    va_end(args);
}

static void
appendln(char const* fmt, ...)
{
    assert(out != NULL);
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);
    autil_string_append_vfmt(out, fmt, args);
    va_end(args);

    autil_string_append_cstr(out, "\n");
}

static void
appendli(char const* fmt, ...)
{
    assert(out != NULL);
    assert(fmt != NULL);

    autil_string_append_cstr(out, "    ");

    va_list args;
    va_start(args, fmt);
    autil_string_append_vfmt(out, fmt, args);
    va_end(args);

    autil_string_append_cstr(out, "\n");
}

static void
appendli_location(struct source_location const* location, char const* fmt, ...)
{
    assert(out != NULL);
    assert(location != NULL);
    assert(location->path != NO_PATH);
    assert(location->line != NO_LINE);
    assert(location->psrc != NO_PSRC);

    autil_string_append_fmt(
        out, "    ; [%s:%zu] ", location->path, location->line);

    va_list args;
    va_start(args, fmt);
    autil_string_append_vfmt(out, fmt, args);
    va_end(args);

    autil_string_append_cstr(out, "\n");

    char const* const source = lookup_module(location->path)->source;
    char const* const line_start = source_line_start(source, location->psrc);
    char const* const line_end = source_line_end(source, location->psrc);

    autil_string_append_fmt(
        out, "    ;%.*s\n", (int)(line_end - line_start), line_start);
    autil_string_append_fmt(
        out, "    ;%*s^\n", (int)(location->psrc - line_start), "");
}

static void
appendch(char ch)
{
    assert(out != NULL);

    autil_string_append(out, &ch, 1u);
}

static void
append_dx_type(struct type const* type)
{
    assert(out != NULL);
    assert(type != NULL);
    assert(type->size != 0);

    switch (type->kind) {
    case TYPE_VOID: {
        UNREACHABLE();
    }
    case TYPE_BOOL: /* fallthrough */
    case TYPE_BYTE: /* fallthrough */
    case TYPE_U8: /* fallthrough */
    case TYPE_S8: /* fallthrough */
    case TYPE_U16: /* fallthrough */
    case TYPE_S16: /* fallthrough */
    case TYPE_U32: /* fallthrough */
    case TYPE_S32: /* fallthrough */
    case TYPE_U64: /* fallthrough */
    case TYPE_S64: /* fallthrough */
    case TYPE_USIZE: /* fallthrough */
    case TYPE_SSIZE: {
        append("db");
        return;
    }
    case TYPE_FUNCTION: /* fallthrough */
    case TYPE_POINTER: {
        append("dq");
        return;
    }
    case TYPE_ARRAY: {
        append_dx_type(type->data.array.base);
        return;
    }
    case TYPE_SLICE: {
        // pointer => dq
        // count   => dq
        append("dq");
        return;
    }
    }

    UNREACHABLE();
}

static void
append_dx_data(struct value const* value)
{
    assert(out != NULL);
    assert(value != NULL);
    assert(value->type->size != 0);

    switch (value->type->kind) {
    case TYPE_VOID: {
        UNREACHABLE();
    }
    case TYPE_BOOL: /* fallthrough */
    case TYPE_BYTE: /* fallthrough */
    case TYPE_U8: /* fallthrough */
    case TYPE_S8: /* fallthrough */
    case TYPE_U16: /* fallthrough */
    case TYPE_S16: /* fallthrough */
    case TYPE_U32: /* fallthrough */
    case TYPE_S32: /* fallthrough */
    case TYPE_U64: /* fallthrough */
    case TYPE_S64: /* fallthrough */
    case TYPE_USIZE: /* fallthrough */
    case TYPE_SSIZE: {
        autil_sbuf(uint8_t) const bytes = value_to_new_bytes(value);
        for (size_t i = 0; i < autil_sbuf_count(bytes); ++i) {
            if (i != 0) {
                append(", ");
            }
            append("%#x", (unsigned)bytes[i]);
        }
        autil_sbuf_fini(bytes);
        return;
    }
    case TYPE_FUNCTION: {
        append("%s", value->data.function->name);
        return;
    }
    case TYPE_POINTER: {
        struct address const* const address = &value->data.pointer;
        if (value->type->data.pointer.base->size == 0) {
            append("__nil");
            return;
        }
        assert(address->kind == ADDRESS_STATIC);
        append(
            "(%s + %zu)",
            address->data.static_.name,
            address->data.static_.offset);
        return;
    }
    case TYPE_ARRAY: {
        autil_sbuf(struct value*) elements = value->data.array.elements;
        for (size_t i = 0; i < autil_sbuf_count(elements); ++i) {
            if (i != 0) {
                append(", ");
            }
            append_dx_data(elements[i]);
        }
        return;
    }
    case TYPE_SLICE: {
        append_dx_data(value->data.slice.pointer);
        append(", ");
        append_dx_data(value->data.slice.count);
        return;
    }
    }

    UNREACHABLE();
}

static void
push(size_t size)
{
    if (size == 0) {
        return;
    }

    appendli("sub rsp, %#zx", ceil8zu(size));
}

static void
push_address(struct address const* address)
{
    assert(address != NULL);

    switch (address->kind) {
    case ADDRESS_STATIC: {
        appendli(
            "push %s + %zu",
            address->data.static_.name,
            address->data.static_.offset);
        break;
    }
    case ADDRESS_LOCAL: {
        appendli("mov rax, rbp");
        appendli("add rax, %d", address->data.local.rbp_offset);
        appendli("push rax");
        break;
    }
    }
}

static void
push_at_address(size_t size, struct address const* address)
{
    assert(address != NULL);

    push(size);

    // Usable for a memory indirection `[addr]`.
    // ADDRESS_STATIC mode: number + offset
    // ADDRESS_LOCAL mode : reg + base*scale + offset
    char* addr = NULL;
    switch (address->kind) {
    case ADDRESS_STATIC:
        addr = autil_cstr_new_fmt(
            "(%s + %zu)",
            address->data.static_.name,
            address->data.static_.offset);
        break;
    case ADDRESS_LOCAL:
        addr = autil_cstr_new_fmt("rbp + %d", address->data.local.rbp_offset);
        break;
    default:
        UNREACHABLE();
    }

    // TODO: Add unit tests for signed and unsigned integers with size 1, 2, 4,
    // 8, and >8 to make sure that this cascade of mov operations on 8, then 4,
    // then 2, then 1 byte objects behaves correctly for all cases.
    size_t cur = 0u;
    while ((size - cur) >= 8u) {
        appendli("mov rax, [%s + %#zu]", addr, cur);
        appendli("mov [rsp + %#zx], rax", cur);
        cur += 8u;
    }
    if ((size - cur) >= 4u) {
        appendli("mov eax, [%s + %#zu]", addr, cur);
        appendli("mov [rsp + %#zx], eax", cur);
        cur += 4u;
        assert((size - cur) < 4u);
    }
    if ((size - cur) >= 2u) {
        appendli("mov ax, [%s + %#zu]", addr, cur);
        appendli("mov [rsp + %#zx], ax", cur);
        cur += 2u;
        assert((size - cur) < 2u);
    }
    if ((size - cur) == 1u) {
        appendli("mov al, [%s + %#zu]", addr, cur);
        appendli("mov [rsp + %#zx], al", cur);
    }
    // NOTE: In relation to the above TODO, if the cascade logic does not work,
    // then fall back to the dumb-memcpy version that it replaced (shown below).
    /*
    for (size_t i = 0; i < size; ++i) {
        appendli("mov al, [%s + %#zu]", addr, i);
        appendli("mov [rsp + %#zx], al", i);
    }
    */

    autil_xalloc(addr, AUTIL_XALLOC_FREE);
}

static void
pop(size_t size)
{
    if (size == 0) {
        return;
    }

    appendli("add rsp, %#zx", ceil8zu(size));
}

char const*
reg_a(size_t size)
{
    switch (size) {
    case 1:
        return "al";
    case 2:
        return "ax";
    case 4:
        return "eax";
    case 8:
        return "rax";
    }
    UNREACHABLE();
}

char const*
reg_b(size_t size)
{
    switch (size) {
    case 1:
        return "bl";
    case 2:
        return "bx";
    case 4:
        return "ebx";
    case 8:
        return "rbx";
    }
    UNREACHABLE();
}

static void
copy_rax_rbx_via_rcx(size_t size)
{
    size_t cur = 0u;
    while ((size - cur) >= 8u) {
        appendli("mov rcx, [rax + %#zu]", cur);
        appendli("mov [rbx + %#zx], rcx", cur);
        cur += 8u;
    }
    if ((size - cur) >= 4u) {
        appendli("mov ecx, [rax + %#zu]", cur);
        appendli("mov [rbx + %#zx], ecx", cur);
        cur += 4u;
        assert((size - cur) < 4u);
    }
    if ((size - cur) >= 2u) {
        appendli("mov cx, [rax + %#zu]", cur);
        appendli("mov [rbx + %#zx], cx", cur);
        cur += 2u;
        assert((size - cur) < 2u);
    }
    if ((size - cur) == 1u) {
        appendli("mov cl, [rax + %#zu]", cur);
        appendli("mov [rbx + %#zx], cl", cur);
    }
}

static void
copy_rsp_rbx_via_rcx(size_t size)
{
    size_t cur = 0u;
    while ((size - cur) >= 8u) {
        appendli("mov rcx, [rsp + %#zu]", cur);
        appendli("mov [rbx + %#zx], rcx", cur);
        cur += 8u;
    }
    if ((size - cur) >= 4u) {
        appendli("mov ecx, [rsp + %#zu]", cur);
        appendli("mov [rbx + %#zx], ecx", cur);
        cur += 4u;
        assert((size - cur) < 4u);
    }
    if ((size - cur) >= 2u) {
        appendli("mov cx, [rsp + %#zu]", cur);
        appendli("mov [rbx + %#zx], cx", cur);
        cur += 2u;
        assert((size - cur) < 2u);
    }
    if ((size - cur) == 1u) {
        appendli("mov cl, [rsp + %#zu]", cur);
        appendli("mov [rbx + %#zx], cl", cur);
    }
}

static void
copy_rax_rsp_via_rcx(size_t size)
{
    size_t cur = 0u;
    while ((size - cur) >= 8u) {
        appendli("mov rcx, [rax + %#zu]", cur);
        appendli("mov [rsp + %#zx], rcx", cur);
        cur += 8u;
    }
    if ((size - cur) >= 4u) {
        appendli("mov ecx, [rax + %#zu]", cur);
        appendli("mov [rsp + %#zx], ecx", cur);
        cur += 4u;
        assert((size - cur) < 4u);
    }
    if ((size - cur) >= 2u) {
        appendli("mov cx, [rax + %#zu]", cur);
        appendli("mov [rsp + %#zx], cx", cur);
        cur += 2u;
        assert((size - cur) < 2u);
    }
    if ((size - cur) == 1u) {
        appendli("mov cl, [rax + %#zu]", cur);
        appendli("mov [rsp + %#zx], cl", cur);
    }
}

static void
codegen_static_constants(void);
static void
codegen_static_variables(void);
static void
codegen_static_functions(void);
static void
codegen_core(void);

static void
codegen_static_object(struct symbol const* symbol);
static void
codegen_static_function(struct symbol const* symbol);

static void
codegen_stmt(struct tir_stmt const* stmt);
static void
codegen_stmt_if(struct tir_stmt const* stmt);
static void
codegen_stmt_for_range(struct tir_stmt const* stmt);
static void
codegen_stmt_for_expr(struct tir_stmt const* stmt);
static void
codegen_stmt_dump(struct tir_stmt const* stmt);
static void
codegen_stmt_return(struct tir_stmt const* stmt);
static void
codegen_stmt_assign(struct tir_stmt const* stmt);
static void
codegen_stmt_expr(struct tir_stmt const* stmt);

static void
codegen_rvalue(struct tir_expr const* expr);
static void
codegen_rvalue_identifier(struct tir_expr const* expr);
static void
codegen_rvalue_boolean(struct tir_expr const* expr);
static void
codegen_rvalue_integer(struct tir_expr const* expr);
static void
codegen_rvalue_bytes(struct tir_expr const* expr);
static void
codegen_rvalue_literal_array(struct tir_expr const* expr);
static void
codegen_rvalue_literal_slice(struct tir_expr const* expr);
static void
codegen_rvalue_cast(struct tir_expr const* expr);
static void
codegen_rvalue_syscall(struct tir_expr const* expr);
static void
codegen_rvalue_call(struct tir_expr const* expr);
static void
codegen_rvalue_index(struct tir_expr const* expr);
static void
codegen_rvalue_index_lhs_array(struct tir_expr const* expr);
static void
codegen_rvalue_index_lhs_slice(struct tir_expr const* expr);
static void
codegen_rvalue_slice(struct tir_expr const* expr);
static void
codegen_rvalue_slice_lhs_array(struct tir_expr const* expr);
static void
codegen_rvalue_slice_lhs_slice(struct tir_expr const* expr);
static void
codegen_rvalue_sizeof(struct tir_expr const* expr);
static void
codegen_rvalue_unary(struct tir_expr const* expr);
static void
codegen_rvalue_binary(struct tir_expr const* expr);
static void
codegen_lvalue(struct tir_expr const* expr);
static void
codegen_lvalue_index(struct tir_expr const* expr);
static void
codegen_lvalue_index_lhs_array(struct tir_expr const* expr);
static void
codegen_lvalue_index_lhs_slice(struct tir_expr const* expr);
static void
codegen_lvalue_unary(struct tir_expr const* expr);

static void
codegen_static_constants(void)
{
    appendln("; STATIC CONSTANTS");
    appendln("section .rodata");

    struct autil_vec const* const keys =
        autil_map_keys(context()->static_symbols);
    CONTEXT_STATIC_SYMBOLS_MAP_KEY_TYPE const* iter =
        autil_vec_next_const(keys, NULL);
    for (; iter != NULL; iter = autil_vec_next_const(keys, iter)) {
        CONTEXT_STATIC_SYMBOLS_MAP_VAL_TYPE const* const psymbol =
            autil_map_lookup_const(context()->static_symbols, iter);
        struct symbol const* const symbol = *psymbol;
        assert(symbol != NULL);
        assert(symbol->address != NULL);
        assert(symbol->address->kind == ADDRESS_STATIC);
        if (symbol->kind != SYMBOL_CONSTANT) {
            continue;
        }
        codegen_static_object(symbol);
    }
}

static void
codegen_static_variables(void)
{
    appendln("; STATIC VARIABLES");
    appendln("section .data");

    struct autil_vec const* const keys =
        autil_map_keys(context()->static_symbols);
    CONTEXT_STATIC_SYMBOLS_MAP_KEY_TYPE const* iter =
        autil_vec_next_const(keys, NULL);
    for (; iter != NULL; iter = autil_vec_next_const(keys, iter)) {
        CONTEXT_STATIC_SYMBOLS_MAP_VAL_TYPE const* const psymbol =
            autil_map_lookup_const(context()->static_symbols, iter);
        struct symbol const* const symbol = *psymbol;
        assert(symbol != NULL);
        assert(symbol->address != NULL);
        assert(symbol->address->kind == ADDRESS_STATIC);
        if (symbol->kind != SYMBOL_VARIABLE) {
            continue;
        }
        codegen_static_object(symbol);
    }
}

static void
codegen_static_functions(void)
{
    appendln("; STATIC (GLOBAL) FUNCTIONS");
    appendln("section .text");

    struct autil_vec const* const keys =
        autil_map_keys(context()->static_symbols);
    CONTEXT_STATIC_SYMBOLS_MAP_KEY_TYPE const* iter =
        autil_vec_next_const(keys, NULL);
    for (; iter != NULL; iter = autil_vec_next_const(keys, iter)) {
        CONTEXT_STATIC_SYMBOLS_MAP_VAL_TYPE const* const psymbol =
            autil_map_lookup_const(context()->static_symbols, iter);
        struct symbol const* const symbol = *psymbol;
        assert(symbol != NULL);
        assert(symbol->address != NULL);
        assert(symbol->address->kind == ADDRESS_STATIC);
        if (symbol->kind != SYMBOL_FUNCTION) {
            continue;
        }
        codegen_static_function(symbol);
    }
}

static void
codegen_core(void)
{
    appendln("; BUILTIN NIL POINTER / VALUE");
    appendln("__nil: equ 0");
    appendch('\n');

    appendln("; BUILTIN DUMP SUBROUTINE");
    appendln("section .text");
    appendln("global dump");
    appendln("dump:");
    appendln("    push rbp");
    appendln("    mov rbp, rsp");
    appendch('\n');
    appendln("    mov r15, [rbp + 0x10] ; r15 = size");
    appendch('\n');
    appendln("    cmp r15, 0");
    appendln("    jne .setup");
    appendln("    mov rax, 1 ; SYS_WRITE");
    appendln("    mov rdi, 2 ; STDERR_FILENO");
    appendln("    mov rsi, __dump_nl");
    appendln("    mov rdx, 1");
    appendln("    syscall");
    appendln("    mov rsp, rbp");
    appendln("    pop rbp");
    appendln("    ret");
    appendch('\n');
    appendln(".setup:");
    appendln("    mov r14, r15 ; r14 = size * 3");
    appendln("    imul r14, 3");
    appendln("    sub rsp, r14 ; buf = rsp = alloca(size * 3)");
    appendch('\n');
    appendln("    mov r11, rsp ; ptr = r11 = buf");
    appendln("    mov r12, rbp ; cur = r12 = &obj");
    appendln("    add r12, 0x18");
    appendln("    mov r13, r12 ; end = r13 = &obj + size");
    appendln("    add r13, r15");
    appendch('\n');
    appendln(".loop:");
    appendln("    cmp r12, r13 ; while (cur != end)");
    appendln("    je .write");
    appendch('\n');
    appendln("    mov rax, [r12] ; repr = rax = dump_lookup_table + *cur * 2");
    appendln("    and rax, 0xFF");
    appendln("    imul rax, 2");
    appendln("    add rax, __dump_lookup_table");
    appendch('\n');
    appendln("    mov bl, [rax + 0] ; *ptr = repr[0]");
    appendln("    mov [r11], bl");
    appendln("    inc r11 ; ptr += 1");
    appendln("    mov bl, [rax + 1] ; *ptr = repr[1]");
    appendln("    mov [r11], bl");
    appendln("    inc r11 ; ptr += 1");
    appendln("    mov bl, 0x20 ; *ptr = ' '");
    appendln("    mov byte [r11], bl");
    appendln("    inc r11 ; ptr += 1");
    appendch('\n');
    appendln("    inc r12 ; cur += 1");
    appendln("    jmp .loop");
    appendch('\n');
    appendln(".write:");
    appendln("    dec r11 ; ptr -= 1");
    appendln("    mov byte [r11], 0x0A ; *ptr = '\\n'");
    appendch('\n');
    appendln("    ; write(STDERR_FILENO, buf, size * 3)");
    appendln("    mov rax, 1 ; SYS_WRITE");
    appendln("    mov rdi, 2 ; STDERR_FILENO");
    appendln("    mov rsi, rsp");
    appendln("    mov rdx, r14");
    appendln("    syscall");
    appendch('\n');
    appendln("    mov rsp, rbp");
    appendln("    pop rbp");
    appendln("    ret");
    appendch('\n');
    appendln("section .rodata");
    appendln("__dump_nl: db 0x0A");
    appendln("__dump_lookup_table: db \\");
    appendln("    '00', '01', '02', '03', '04', '05', '06', '07', \\");
    appendln("    '08', '09', '0A', '0B', '0C', '0D', '0E', '0F', \\");
    appendln("    '10', '11', '12', '13', '14', '15', '16', '17', \\");
    appendln("    '18', '19', '1A', '1B', '1C', '1D', '1E', '1F', \\");
    appendln("    '20', '21', '22', '23', '24', '25', '26', '27', \\");
    appendln("    '28', '29', '2A', '2B', '2C', '2D', '2E', '2F', \\");
    appendln("    '30', '31', '32', '33', '34', '35', '36', '37', \\");
    appendln("    '38', '39', '3A', '3B', '3C', '3D', '3E', '3F', \\");
    appendln("    '40', '41', '42', '43', '44', '45', '46', '47', \\");
    appendln("    '48', '49', '4A', '4B', '4C', '4D', '4E', '4F', \\");
    appendln("    '50', '51', '52', '53', '54', '55', '56', '57', \\");
    appendln("    '58', '59', '5A', '5B', '5C', '5D', '5E', '5F', \\");
    appendln("    '60', '61', '62', '63', '64', '65', '66', '67', \\");
    appendln("    '68', '69', '6A', '6B', '6C', '6D', '6E', '6F', \\");
    appendln("    '70', '71', '72', '73', '74', '75', '76', '77', \\");
    appendln("    '78', '79', '7A', '7B', '7C', '7D', '7E', '7F', \\");
    appendln("    '80', '81', '82', '83', '84', '85', '86', '87', \\");
    appendln("    '88', '89', '8A', '8B', '8C', '8D', '8E', '8F', \\");
    appendln("    '90', '91', '92', '93', '94', '95', '96', '97', \\");
    appendln("    '98', '99', '9A', '9B', '9C', '9D', '9E', '9F', \\");
    appendln("    'A0', 'A1', 'A2', 'A3', 'A4', 'A5', 'A6', 'A7', \\");
    appendln("    'A8', 'A9', 'AA', 'AB', 'AC', 'AD', 'AE', 'AF', \\");
    appendln("    'B0', 'B1', 'B2', 'B3', 'B4', 'B5', 'B6', 'B7', \\");
    appendln("    'B8', 'B9', 'BA', 'BB', 'BC', 'BD', 'BE', 'BF', \\");
    appendln("    'C0', 'C1', 'C2', 'C3', 'C4', 'C5', 'C6', 'C7', \\");
    appendln("    'C8', 'C9', 'CA', 'CB', 'CC', 'CD', 'CE', 'CF', \\");
    appendln("    'D0', 'D1', 'D2', 'D3', 'D4', 'D5', 'D6', 'D7', \\");
    appendln("    'D8', 'D9', 'DA', 'DB', 'DC', 'DD', 'DE', 'DF', \\");
    appendln("    'E0', 'E1', 'E2', 'E3', 'E4', 'E5', 'E6', 'E7', \\");
    appendln("    'E8', 'E9', 'EA', 'EB', 'EC', 'ED', 'EE', 'EF', \\");
    appendln("    'F0', 'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', \\");
    appendln("    'F8', 'F9', 'FA', 'FB', 'FC', 'FD', 'FE', 'FF'");
    appendch('\n');

    appendln("; BUILTIN OUT-OF-RANGE INTEGER RESULT HANDLER");
    appendln("section .text");
    appendln("__integer_oor_handler:");
    appendln("    push rbp");
    appendln("    mov rbp, rsp");
    appendch('\n');
    appendln("    mov rax, 1 ; SYS_WRITE");
    appendln("    mov rdi, 2 ; STDERR_FILENO");
    appendln("    mov rsi, __integer_oor_msg_start");
    appendln("    mov rdx, __integer_oor_msg_count");
    appendln("    syscall");
    appendch('\n');
    appendln("    mov rax, 60 ; exit");
    appendli("    mov rdi, 1 ; EXIT_FAILURE");
    appendln("    syscall");
    appendch('\n');
    appendln("section .rodata");
    appendln("__integer_oor_msg_start: db\\");
    appendln(
        "    \"fatal: arithmetic operation produces out-of-range result\", 0x0A");
    appendln("__integer_oor_msg_count: equ $ - __integer_oor_msg_start");
    appendch('\n');

    appendln("; BUILTIN INTEGER DIVIDE BY ZERO HANDLER");
    appendln("section .text");
    appendln("__integer_divz_handler:");
    appendln("    push rbp");
    appendln("    mov rbp, rsp");
    appendch('\n');
    appendln("    mov rax, 1 ; SYS_WRITE");
    appendln("    mov rdi, 2 ; STDERR_FILENO");
    appendln("    mov rsi, __integer_divz_msg_start");
    appendln("    mov rdx, __integer_divz_msg_count");
    appendln("    syscall");
    appendch('\n');
    appendln("    mov rax, 60 ; exit");
    appendli("    mov rdi, 1 ; EXIT_FAILURE");
    appendln("    syscall");
    appendch('\n');
    appendln("section .rodata");
    appendln("__integer_divz_msg_start: db \"fatal: divide by zero\", 0x0A");
    appendln("__integer_divz_msg_count: equ $ - __integer_divz_msg_start");
    appendch('\n');

    appendln("; BUILTIN INDEX OUT-OF-BOUNDS HANDLER");
    appendln("section .text");
    appendln("__index_oob_handler:");
    appendln("    push rbp");
    appendln("    mov rbp, rsp");
    appendch('\n');
    appendln("    mov rax, 1 ; SYS_WRITE");
    appendln("    mov rdi, 2 ; STDERR_FILENO");
    appendln("    mov rsi, __index_oob_msg_start");
    appendln("    mov rdx, __index_oob_msg_count");
    appendln("    syscall");
    appendch('\n');
    appendln("    mov rax, 60 ; exit");
    appendli("    mov rdi, 1 ; EXIT_FAILURE");
    appendln("    syscall");
    appendch('\n');
    appendln("section .rodata");
    appendln("__index_oob_msg_start: db \"fatal: index out-of-bounds\", 0x0A");
    appendln("__index_oob_msg_count: equ $ - __index_oob_msg_start");
    appendch('\n');

    appendln("; PROGRAM ENTRY POINT");
    appendln("section .text");
    appendln("global _start");
    appendln("_start:");
    appendli("call main");
    appendli("mov rax, 60 ; exit");
    appendli("mov rdi, 0 ; EXIT_SUCCESS");
    appendli("syscall");
}

static void
codegen_static_object(struct symbol const* symbol)
{
    assert(symbol != NULL);
    assert(symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_CONSTANT);
    assert(symbol->address->kind == ADDRESS_STATIC);
    assert(symbol->value != NULL);

    struct value const* const value = symbol->value;
    struct type const* const type = value->type;
    if (type->size == 0) {
        return;
    }

    assert(symbol->address->data.static_.offset == 0);
    append("%s: ", symbol->address->data.static_.name);
    append_dx_type(symbol->value->type);
    appendch(' ');
    append_dx_data(symbol->value);
    appendch('\n');
    return;
}

static void
codegen_static_function(struct symbol const* symbol)
{
    assert(symbol != NULL);
    assert(symbol->kind == SYMBOL_FUNCTION);
    assert(symbol->value != NULL);

    assert(symbol->value->type->kind == TYPE_FUNCTION);
    struct tir_function const* const function = symbol->value->data.function;

    appendln("global %s", function->name);
    appendln("%s:", function->name);
    appendli("; PROLOGUE");
    // Save previous frame pointer.
    // With this push, the stack should now be 16-byte aligned.
    appendli("push rbp");
    // Move stack pointer into current frame pointer.
    appendli("mov rbp, rsp");
    // Adjust the stack pointer to make space for locals.
    assert(function->local_stack_offset <= 0);
    appendli("add rsp, %d", function->local_stack_offset);

    assert(current_function == NULL);
    current_function = function;
    for (size_t i = 0; i < autil_sbuf_count(function->body->stmts); ++i) {
        struct tir_stmt const* const stmt = function->body->stmts[i];
        codegen_stmt(stmt);
    }
    current_function = NULL;

    appendli("; END-OF-FUNCTION");
    if (function->type->data.function.return_type == context()->builtin.void_) {
        appendli("; EPILOGUE (implicit-return)");
        appendli("mov rsp, rbp");
        appendli("pop rbp");
        appendli("ret");
    }
    else {
        appendli("; Segfault if no return statement occured.");
        appendli("mov r15, [0x0000000000000000]");
    }
    appendch('\n');
}

static void
codegen_stmt(struct tir_stmt const* stmt)
{
    assert(stmt != NULL);

    switch (stmt->kind) {
    case TIR_STMT_IF: {
        appendli_location(stmt->location, "<stmt-if>");
        codegen_stmt_if(stmt);
        return;
    }
    case TIR_STMT_FOR_RANGE: {
        appendli_location(stmt->location, "<stmt-for-range>");
        codegen_stmt_for_range(stmt);
        return;
    }
    case TIR_STMT_FOR_EXPR: {
        appendli_location(stmt->location, "<stmt-for-expr>");
        codegen_stmt_for_expr(stmt);
        return;
    }
    case TIR_STMT_DUMP: {
        appendli_location(stmt->location, "<stmt-dump>");
        codegen_stmt_dump(stmt);
        return;
    }
    case TIR_STMT_RETURN: {
        appendli_location(stmt->location, "<stmt-return>");
        codegen_stmt_return(stmt);
        return;
    }
    case TIR_STMT_ASSIGN: {
        appendli_location(stmt->location, "<stmt-assign>");
        codegen_stmt_assign(stmt);
        return;
    }
    case TIR_STMT_EXPR: {
        appendli_location(stmt->location, "<stmt-expr>");
        codegen_stmt_expr(stmt);
        return;
    }
    }

    UNREACHABLE();
}

static void
codegen_stmt_if(struct tir_stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == TIR_STMT_IF);

    size_t const stmt_id = unique_id++;
    autil_sbuf(struct tir_conditional const* const) const conditionals =
        stmt->data.if_.conditionals;
    appendln(".l%zu_stmt_if_bgn:", stmt_id);
    for (size_t i = 0; i < autil_sbuf_count(conditionals); ++i) {
        bool const is_last = i == (autil_sbuf_count(conditionals) - 1);

        if (conditionals[i]->condition != NULL) {
            appendln(".l%zu_stmt_if_%zu_condition:", stmt_id, i);
            assert(conditionals[i]->condition->type->kind == TYPE_BOOL);
            codegen_rvalue(conditionals[i]->condition);
            appendli("pop rax");
            appendli("mov rbx, 0x00");
            appendli("cmp rax, rbx");
            if (is_last) {
                appendli("je .l%zu_stmt_if_end", stmt_id);
            }
            else {
                appendli("je .l%zu_stmt_if_%zu_condition", stmt_id, i + 1);
            }
        }
        else {
            appendln(".l%zu_stmt_if_%zu_condition:", stmt_id, i);
            appendli("; else condition (always true)");
        }

        appendln(".l%zu_stmt_if_%zu_body:", stmt_id, i);
        autil_sbuf(struct tir_stmt const* const) const stmts =
            conditionals[i]->body->stmts;
        for (size_t i = 0; i < autil_sbuf_count(stmts); ++i) {
            codegen_stmt(stmts[i]);
        }
        appendli("jmp .l%zu_stmt_if_end", stmt_id);
    }
    appendln(".l%zu_stmt_if_end:", stmt_id);
}

static void
codegen_stmt_for_range(struct tir_stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == TIR_STMT_FOR_RANGE);

    assert(
        stmt->data.for_range.loop_variable->type == context()->builtin.usize);
    assert(stmt->data.for_range.begin->type == context()->builtin.usize);
    assert(stmt->data.for_range.end->type == context()->builtin.usize);
    assert(stmt->data.for_range.loop_variable->address->kind == ADDRESS_LOCAL);
    size_t const stmt_id = unique_id++;
    appendln(".l%zu_stmt_for_range_bgn:", stmt_id);
    push_address(stmt->data.for_range.loop_variable->address);
    codegen_rvalue(stmt->data.for_range.begin);
    appendli("pop rbx"); // begin
    appendli("pop rax"); // addr of loop variable
    appendli("mov [rax], rbx");
    appendln(".l%zu_stmt_for_range_condition:", stmt_id);
    push_at_address(
        stmt->data.for_range.loop_variable->type->size,
        stmt->data.for_range.loop_variable->address);
    codegen_rvalue(stmt->data.for_range.end);
    appendli("pop rbx"); // end
    appendli("pop rax"); // loop variable
    appendli("cmp rax, rbx");
    appendli("je .l%zu_stmt_for_range_end", stmt_id);
    appendln(".l%zu_stmt_for_range_body:", stmt_id);
    autil_sbuf(struct tir_stmt const* const) const stmts =
        stmt->data.for_range.body->stmts;
    for (size_t i = 0; i < autil_sbuf_count(stmts); ++i) {
        codegen_stmt(stmts[i]);
    }
    appendli(
        "inc qword [rbp + %d]",
        stmt->data.for_range.loop_variable->address->data.local.rbp_offset);
    appendli("jmp .l%zu_stmt_for_range_condition", stmt_id);
    appendln(".l%zu_stmt_for_range_end:", stmt_id);
}

static void
codegen_stmt_for_expr(struct tir_stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == TIR_STMT_FOR_EXPR);

    size_t const stmt_id = unique_id++;
    appendln(".l%zu_stmt_for_expr_bgn:", stmt_id);
    appendln(".l%zu_stmt_for_expr_condition:", stmt_id);
    assert(stmt->data.for_expr.expr->type->kind == TYPE_BOOL);
    codegen_rvalue(stmt->data.for_expr.expr);
    appendli("pop rax");
    appendli("mov rbx, 0x00");
    appendli("cmp rax, rbx");
    appendli("je .l%zu_stmt_for_expr_end", stmt_id);
    appendln(".l%zu_stmt_for_expr_body:", stmt_id);
    autil_sbuf(struct tir_stmt const* const) const stmts =
        stmt->data.for_expr.body->stmts;
    for (size_t i = 0; i < autil_sbuf_count(stmts); ++i) {
        codegen_stmt(stmts[i]);
    }
    appendli("jmp .l%zu_stmt_for_expr_condition", stmt_id);
    appendln(".l%zu_stmt_for_expr_end:", stmt_id);
}

static void
codegen_stmt_dump(struct tir_stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == TIR_STMT_DUMP);

    codegen_rvalue(stmt->data.expr);
    appendli("push %#zx", stmt->data.expr->type->size);
    appendli("call dump");
    appendli("pop rax");
    pop(stmt->data.expr->type->size);
}

static void
codegen_stmt_return(struct tir_stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == TIR_STMT_RETURN);

    if (stmt->data.return_.expr != NULL) {
        // Compute result.
        codegen_rvalue(stmt->data.return_.expr);

        // Store in return address.
        struct symbol const* const return_symbol = symbol_table_lookup(
            current_function->symbol_table, context()->interned.return_);
        // rbx := destination
        assert(return_symbol->address->kind == ADDRESS_LOCAL);
        appendli("mov rbx, rbp");
        appendli("add rbx, %d", return_symbol->address->data.local.rbp_offset);
        copy_rsp_rbx_via_rcx(return_symbol->type->size);
    }

    appendli("; EPILOGUE");
    // Restore stack pointer.
    appendli("mov rsp, rbp");
    // Restore previous frame pointer.
    appendli("pop rbp");
    // Return control to the calling routine.
    appendli("ret");
}

static void
codegen_stmt_assign(struct tir_stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == TIR_STMT_ASSIGN);

    codegen_rvalue(stmt->data.assign.rhs);
    codegen_lvalue(stmt->data.assign.lhs);

    appendli("pop rbx");
    size_t const size = stmt->data.assign.rhs->type->size;
    // TODO: Add unit tests for signed and unsigned integers with size 1, 2, 4,
    // 8, and >8 to make sure that this cascade of mov operations on 8, then 4,
    // then 2, then 1 byte objects behaves correctly for all cases.
    size_t cur = 0u;
    while ((size - cur) >= 8u) {
        appendli("mov rax, [rsp + %#zu]", cur);
        appendli("mov [rbx + %#zx], rax", cur);
        cur += 8u;
    }
    if ((size - cur) >= 4u) {
        appendli("mov eax, [rsp + %#zu]", cur);
        appendli("mov [rbx + %#zx], eax", cur);
        cur += 4u;
        assert((size - cur) < 4u);
    }
    if ((size - cur) >= 2u) {
        appendli("mov ax, [rsp + %#zu]", cur);
        appendli("mov [rbx + %#zx], ax", cur);
        cur += 2u;
        assert((size - cur) < 2u);
    }
    if ((size - cur) == 1u) {
        appendli("mov al, [rsp + %#zu]", cur);
        appendli("mov [rbx + %#zx], al", cur);
    }
    // NOTE: In relation to the above TODO, if the cascade logic does not work,
    // then fall back to the dumb-memcpy version that it replaced (shown below).
    /*
    for (size_t i = 0; i < size; ++i) {
        appendli("mov al, [rsp + %#zu]", i);
        appendli("mov [rbx + %#zx], al", i);
    }
    */
    pop(size);
}

static void
codegen_stmt_expr(struct tir_stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == TIR_STMT_EXPR);

    codegen_rvalue(stmt->data.expr);
    // Remove the (unused) result from the stack.
    pop(stmt->data.expr->type->size);
}

static void
codegen_rvalue(struct tir_expr const* expr)
{
    assert(expr != NULL);

    switch (expr->kind) {
    case TIR_EXPR_IDENTIFIER: {
        codegen_rvalue_identifier(expr);
        return;
    }
    case TIR_EXPR_BOOLEAN: {
        codegen_rvalue_boolean(expr);
        return;
    }
    case TIR_EXPR_INTEGER: {
        codegen_rvalue_integer(expr);
        return;
    }
    case TIR_EXPR_BYTES: {
        codegen_rvalue_bytes(expr);
        return;
    }
    case TIR_EXPR_LITERAL_ARRAY: {
        codegen_rvalue_literal_array(expr);
        return;
    }
    case TIR_EXPR_LITERAL_SLICE: {
        codegen_rvalue_literal_slice(expr);
        return;
    }
    case TIR_EXPR_CAST: {
        codegen_rvalue_cast(expr);
        return;
    }
    case TIR_EXPR_SYSCALL: {
        codegen_rvalue_syscall(expr);
        return;
    }
    case TIR_EXPR_CALL: {
        codegen_rvalue_call(expr);
        return;
    }
    case TIR_EXPR_INDEX: {
        codegen_rvalue_index(expr);
        return;
    }
    case TIR_EXPR_SLICE: {
        codegen_rvalue_slice(expr);
        return;
    }
    case TIR_EXPR_SIZEOF: {
        codegen_rvalue_sizeof(expr);
        return;
    }
    case TIR_EXPR_UNARY: {
        codegen_rvalue_unary(expr);
        return;
    }
    case TIR_EXPR_BINARY: {
        codegen_rvalue_binary(expr);
        return;
    }
    }

    UNREACHABLE();
}

static void
codegen_rvalue_identifier(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_IDENTIFIER);

    struct symbol const* const symbol = expr->data.identifier;
    switch (symbol->kind) {
    case SYMBOL_TYPE: /* fallthrough */
    case SYMBOL_NAMESPACE: {
        UNREACHABLE();
    }
    case SYMBOL_VARIABLE: /* fallthrough */
    case SYMBOL_CONSTANT: {
        push_at_address(symbol->type->size, symbol->address);
        return;
    }
    case SYMBOL_FUNCTION: {
        push_address(symbol->address);
        return;
    }
    }

    UNREACHABLE();
}

static void
codegen_rvalue_boolean(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_BOOLEAN);

    appendli("mov rax, %s", expr->data.boolean ? "0x01" : "0x00");
    appendli("push rax");
}

static void
codegen_rvalue_integer(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_INTEGER);

    char* const cstr = autil_bigint_to_new_cstr(expr->data.integer, NULL);

    assert(expr->type->size >= 1u);
    assert(expr->type->size <= 8u);
    appendli("mov rax, %s", cstr);
    appendli("push rax");

    autil_xalloc(cstr, AUTIL_XALLOC_FREE);
}

static void
codegen_rvalue_bytes(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_BYTES);
    assert(expr->type->kind == TYPE_SLICE);

    appendli("push %zu", expr->data.bytes.count); // count
    push_address(expr->data.bytes.address); // pointer
}

static void
codegen_rvalue_literal_array(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_LITERAL_ARRAY);
    assert(expr->type->kind == TYPE_ARRAY);

    // Make space for the array.
    push(expr->type->size);

    // One by one evaluate the rvalues for the elements of the array. Each
    // element will be at the top of the stack after being evaluated, so the
    // element is manually memcpy-ed into the correct position on the stack.
    // This process feels like it would be somewhat slow, but unfortunately it
    // seems necessary in order to keep the left-to-right evaluation order of
    // array elements. Additionally pushing/popping to and from the stack uses
    // 8-byte alignment, but arrays may have element alignment that does not
    // cleanly match the stack alignment (e.g. [count]bool).
    autil_sbuf(struct tir_expr const* const) const elements =
        expr->data.literal_array.elements;
    struct type const* const element_type = expr->type->data.array.base;
    size_t const element_size = element_type->size;
    // TODO: This loop is manually unrolled here, but should probably be turned
    // into an actual asm loop for elements with trivial initialization.
    // Basically we should account for the scenario where a brainfuck
    // interpreter allocates an array 30000 zeroed bytes, which would cause the
    // equivalent of memcpy(&my_array[index], &my_zero, 0x8) inline thousands of
    // times.
    for (size_t i = 0; i < autil_sbuf_count(elements); ++i) {
        assert(elements[i]->type == element_type);
        codegen_rvalue(elements[i]);

        appendli("mov rbx, rsp");
        appendli("add rbx, %zu", ceil8zu(element_size)); // array start
        appendli("add rbx, %zu", element_size * i); // array index
        copy_rsp_rbx_via_rcx(element_size);

        pop(element_size);
    }
}

static void
codegen_rvalue_literal_slice(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_LITERAL_SLICE);
    assert(expr->type->kind == TYPE_SLICE);

    // +---------+
    // | count   |
    // +---------+ <-- rsp + 0x8
    // | pointer |
    // +---------+ <-- rsp
    codegen_rvalue(expr->data.literal_slice.count);
    codegen_rvalue(expr->data.literal_slice.pointer);
}

static void
codegen_rvalue_cast(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_CAST);
    assert(expr->type->size >= 1u);
    assert(expr->type->size <= 8u);

    struct tir_expr const* const from = expr->data.cast.expr;
    assert(from->type->size >= 1u);
    assert(from->type->size <= 8u);
    codegen_rvalue(from);

    // Load casted-from data into an A register (al, ax, eax, or rax).
    char const* const reg = reg_a(from->type->size);
    appendli("mov %s, [rsp]", reg);

    // Perform the operation zero-extend/sign-extend the casted-from data.
    switch (from->type->kind) {
    case TYPE_BOOL: /* fallthrough */
    case TYPE_BYTE: /* fallthrough */
    case TYPE_U8: /* fallthrough */
    case TYPE_U16:
        // Move with Zero-Extend
        appendli("movzx rax, %s", reg);
        break;
    case TYPE_U32:
        // The MOVZX instruction does not have an encoding with SRC of r/m32 or
        // r/m64, but a MOV with SRC of r/m32 will zero out the upper 32 bits.
        appendli("mov rax, %s", reg);
        break;
    case TYPE_S8: /* fallthrough */
    case TYPE_S16: /* fallthrough */
    case TYPE_S32:
        // Move with Sign-Extension
        appendli("movsx rax, %s", reg);
        break;
    case TYPE_U64: /* fallthrough */
    case TYPE_S64: /* fallthrough */
    case TYPE_USIZE: /* fallthrough */
    case TYPE_SSIZE: /* fallthrough */
    case TYPE_POINTER:
        // A MOV with r/m64 has nothing to zero-extend/sign-extend.
        break;
    case TYPE_VOID: /* fallthrough */
    case TYPE_FUNCTION: /* fallthrough */
    case TYPE_ARRAY: /* fallthrough */
    case TYPE_SLICE:
        UNREACHABLE();
    }

    // Boolean values *must* be either zero or one.
    if (expr->type->kind == TYPE_BOOL) {
        appendli("and rax, 0x1");
    }

    // MOV the casted-to data back onto the stack.
    appendli("mov [rsp], rax");
}

static void
codegen_rvalue_syscall(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_SYSCALL);

    struct tir_expr const* const* const arguments =
        expr->data.syscall.arguments;
    size_t const count = autil_sbuf_count(arguments);
    for (size_t i = 0; i < count; ++i) {
        codegen_rvalue(arguments[i]);
    }

    assert(count != 0);
    assert(count <= 7);
    if (count == 7) {
        appendli("pop r9 ; syscall parameter 6");
    }
    if (count >= 6) {
        appendli("pop r8 ; syscall parameter 5");
    }
    if (count >= 5) {
        appendli("pop r10 ; syscall parameter 4");
    }
    if (count >= 4) {
        appendli("pop rdx ; syscall parameter 3");
    }
    if (count >= 3) {
        appendli("pop rsi ; syscall parameter 2");
    }
    if (count >= 2) {
        appendli("pop rdi ; syscall parameter 1");
    }
    if (count >= 1) {
        appendli("pop rax ; syscall number");
    }
    appendli("syscall");
    appendli("push rax ; syscall result");
}

static void
codegen_rvalue_call(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_CALL);

    // Push space for return value.
    struct type const* function_type = expr->data.call.function->type;
    assert(function_type->kind == TYPE_FUNCTION);
    struct type const* return_type = function_type->data.function.return_type;
    push(return_type->size);

    // Evaluate & push arguments from left to right.
    struct tir_expr const* const* const arguments = expr->data.call.arguments;
    for (size_t i = 0; i < autil_sbuf_count(arguments); ++i) {
        codegen_rvalue(arguments[i]);
    }

    // Load the function pointer and call the function.
    codegen_rvalue(expr->data.call.function);
    appendli("pop rax");
    appendli("call rax");

    // Pop arguments from right to left, leaving the return value as the top
    // element on the stack (for return values with non-zero size).
    for (size_t i = autil_sbuf_count(arguments); i--;) {
        appendli("pop rax");
    }
}

static void
codegen_rvalue_index(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_INDEX);

    if (expr->data.index.lhs->type->kind == TYPE_ARRAY) {
        codegen_rvalue_index_lhs_array(expr);
        return;
    }

    if (expr->data.index.lhs->type->kind == TYPE_SLICE) {
        codegen_rvalue_index_lhs_slice(expr);
        return;
    }

    UNREACHABLE();
}

static void
codegen_rvalue_index_lhs_array(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_INDEX);
    assert(expr->data.index.lhs->type->kind == TYPE_ARRAY);
    assert(expr->data.index.idx->type->kind == TYPE_USIZE);
    size_t const expr_id = unique_id++;

    struct type const* const lhs_type = expr->data.index.lhs->type;
    struct type const* const element_type = lhs_type->data.array.base;
    if (element_type->size == 0) {
        return;
    }

    // Push space for result.
    assert(expr->type == element_type);
    push(expr->type->size);

    if (tir_expr_is_lvalue(expr->data.index.lhs)) {
        // Array expression is an lvalue. Compute the address of the of the
        // indexed element and copy from that address into the result.
        codegen_lvalue(expr->data.index.lhs);
        codegen_rvalue(expr->data.index.idx);
        // rax := source
        // rsp := destination
        // After calculating the source address the stack pointer will point to
        // the result since space for the result space was pushed onto the
        // stack.
        appendli("pop rax"); // index
        appendli("mov rbx, %zu", lhs_type->data.array.count); // count
        appendli("cmp rax, rbx");
        appendli("jb .l%zu_index_array_rvalue_calc", expr_id);
        appendli("call __index_oob_handler");
        appendln(".l%zu_index_array_rvalue_calc:", expr_id);
        appendli("mov rbx, %zu", element_type->size); // sizeof(element_type)
        appendli("mul rbx"); // index * sizeof(element_type)
        appendli("pop rbx"); // start
        appendli("add rax, rbx"); // start + index * sizeof(element_type)
        // copy
        copy_rax_rsp_via_rcx(element_type->size);

        return;
    }

    // Array expression is an rvalue. Generate the rvalue array and rvalue
    // index. Then copy indexed element into the result.
    codegen_rvalue(expr->data.index.lhs);
    codegen_rvalue(expr->data.index.idx);
    // rax := source
    appendli("pop rax"); // index
    appendli("mov rbx, %zu", lhs_type->data.array.count); // count
    appendli("cmp rax, rbx");
    appendli("jb .l%zu_index_array_rvalue_calc", expr_id);
    appendli("call __index_oob_handler");
    appendln(".l%zu_index_array_rvalue_calc:", expr_id);
    appendli("mov rbx, %zu", element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // index * sizeof(element_type)
    appendli("add rax, rsp"); // start + index * sizeof(element_type)
    // rbx := destination
    // NOTE: The push and pop operations that manage the stack-allocated array
    // align to an 8-byte boundry, but the array itself may or may not have a
    // size cleanly divisible by 8 (as in the case of the type [3u]u16). The
    // ceil8 of the sizeof the left hand side array is used to account for any
    // extra padding at the end of the array required to bring the total push
    // size to a modulo 8 value.
    appendli("mov rbx, %zu", ceil8zu(lhs_type->size)); // aligned sizeof(array)
    appendli("add rbx, rsp"); // start + aligned sizeof(array)
    // copy
    copy_rax_rbx_via_rcx(element_type->size);

    // Pop array rvalue.
    pop(lhs_type->size);
}

static void
codegen_rvalue_index_lhs_slice(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_INDEX);
    assert(expr->data.index.lhs->type->kind == TYPE_SLICE);
    assert(expr->data.index.idx->type->kind == TYPE_USIZE);
    size_t const expr_id = unique_id++;

    struct type const* const lhs_type = expr->data.index.lhs->type;
    struct type const* const element_type = lhs_type->data.slice.base;
    if (element_type->size == 0) {
        return;
    }

    // Push space for result.
    assert(expr->type == element_type);
    push(expr->type->size);

    codegen_rvalue(expr->data.index.lhs);
    codegen_rvalue(expr->data.index.idx);
    // rax := source
    // rsp := destination
    appendli("pop rax"); // index
    appendli("mov rbx, [rsp + 8]"); // slice count
    appendli("cmp rax, rbx");
    appendli("jb .l%zu_slice_rvalue_calc", expr_id);
    appendli("call __index_oob_handler");
    appendln(".l%zu_slice_rvalue_calc:", expr_id);
    appendli("mov rbx, %zu", element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // index * sizeof(element_type)
    appendli("pop rbx"); // start
    appendli("add rax, rbx"); // start + index * sizeof(element_type)
    pop(8u); // slice count
    // copy
    copy_rax_rsp_via_rcx(element_type->size);
}

static void
codegen_rvalue_slice(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_SLICE);

    if (expr->data.slice.lhs->type->kind == TYPE_ARRAY) {
        codegen_rvalue_slice_lhs_array(expr);
        return;
    }

    if (expr->data.slice.lhs->type->kind == TYPE_SLICE) {
        codegen_rvalue_slice_lhs_slice(expr);
        return;
    }

    UNREACHABLE();
}

static void
codegen_rvalue_slice_lhs_array(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_SLICE);
    assert(expr->data.slice.lhs->type->kind == TYPE_ARRAY);
    assert(expr->data.slice.begin->type->kind == TYPE_USIZE);
    assert(expr->data.slice.end->type->kind == TYPE_USIZE);
    assert(tir_expr_is_lvalue(expr->data.slice.lhs));
    size_t const expr_id = unique_id++;

    struct type const* const lhs_type = expr->data.slice.lhs->type;
    struct type const* const element_type = lhs_type->data.array.base;

    codegen_rvalue(expr->data.slice.end);
    codegen_rvalue(expr->data.slice.begin);
    appendli("pop rax"); // begin
    appendli("pop rbx"); // end
    appendli("mov rcx, %zu", lhs_type->data.array.count); // count

    appendli("cmp rax, rbx"); // cmp begin end
    appendli("jb .l%zu_expr_slice_bgn_oob", expr_id); // jmp begin < end
    appendli("call __index_oob_handler");

    appendli(".l%zu_expr_slice_bgn_oob:", expr_id);
    appendli("cmp rax, rcx"); // cmp begin count
    appendli("jb .l%zu_expr_slice_end_oob", expr_id); // jmp begin < count
    appendli("call __index_oob_handler");

    appendli(".l%zu_expr_slice_end_oob:", expr_id);
    appendli("cmp rbx, rcx"); // cmp end count
    appendli("jbe .l%zu_expr_slice_calc", expr_id); // jmp end <= count
    appendli("call __index_oob_handler");

    appendli(".l%zu_expr_slice_calc:", expr_id);
    appendli("sub rbx, rax"); // count = end - begin
    appendli("push rbx"); // push count

    appendli("mov rbx, %zu", element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // offset = begin * sizeof(element_type)
    appendli("push rax"); // push offset

    // NOTE: The call to codegen_lvalue will push __nil for slices with elements
    // of size zero, so we don't need a special case for that here.
    codegen_lvalue(expr->data.slice.lhs);
    appendli("pop rax"); // start
    appendli("pop rbx"); // offset
    appendli("add rax, rbx"); // pointer = start + offset
    appendli("push rax"); // push pointer
}

static void
codegen_rvalue_slice_lhs_slice(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_SLICE);
    assert(expr->data.slice.lhs->type->kind == TYPE_SLICE);
    assert(expr->data.slice.begin->type->kind == TYPE_USIZE);
    assert(expr->data.slice.end->type->kind == TYPE_USIZE);
    size_t const expr_id = unique_id++;

    struct type const* const lhs_type = expr->data.slice.lhs->type;
    struct type const* const element_type = lhs_type->data.slice.base;

    codegen_rvalue(expr->data.slice.lhs);
    codegen_rvalue(expr->data.slice.end);
    codegen_rvalue(expr->data.slice.begin);
    appendli("pop rax"); // begin
    appendli("pop rbx"); // end
    appendli("pop rsi"); // start (lhs slice's pointer)
    appendli("pop rcx"); // count

    appendli("cmp rax, rbx"); // cmp begin end
    appendli("jb .l%zu_expr_slice_bgn_oob", expr_id); // jmp begin < end
    appendli("call __index_oob_handler");

    appendli(".l%zu_expr_slice_bgn_oob:", expr_id);
    appendli("cmp rax, rcx"); // cmp begin count
    appendli("jb .l%zu_expr_slice_end_oob", expr_id); // jmp begin < count
    appendli("call __index_oob_handler");

    appendli(".l%zu_expr_slice_end_oob:", expr_id);
    appendli("cmp rbx, rcx"); // cmp end count
    appendli("jbe .l%zu_expr_slice_calc", expr_id); // jmp end <= count
    appendli("call __index_oob_handler");

    appendli(".l%zu_expr_slice_calc:", expr_id);
    appendli("sub rbx, rax"); // count = end - begin
    appendli("push rbx"); // push count

    appendli("mov rbx, %zu", element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // offset = begin * sizeof(element_type)
    appendli("add rax, rsi"); // pointer = offset + start
    appendli("push rax"); // push pointer
}

static void
codegen_rvalue_sizeof(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_SIZEOF);

    assert(expr->type->kind == TYPE_USIZE);
    appendli("mov rax, %zu", expr->data.sizeof_.rhs->size);
    appendli("push rax");
}

static void
codegen_rvalue_unary(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_UNARY);

    switch (expr->data.unary.op) {
    case UOP_NOT: {
        codegen_rvalue(expr->data.unary.rhs);
        assert(expr->data.unary.rhs->type->size <= 8u);
        appendli("pop rax");
        appendli("mov rbx, 0");
        appendli("cmp rax, rbx");
        appendli("setz al");
        appendli("push rax");
        return;
    }
    case UOP_POS: {
        codegen_rvalue(expr->data.unary.rhs);
        return;
    }
    case UOP_NEG: {
        struct tir_expr const* const rhs = expr->data.unary.rhs;
        size_t const expr_id = unique_id++;

        assert(rhs->type->size <= 8u);
        codegen_rvalue(expr->data.unary.rhs);
        appendli("pop rax");
        appendln(".l%zu_expr_unary_neg_bgn:", expr_id);
        if (type_is_sinteger(rhs->type)) {
            char* const min_cstr =
                autil_bigint_to_new_cstr(rhs->type->data.integer.min, NULL);
            appendli("mov rbx, %s", min_cstr);
            autil_xalloc(min_cstr, AUTIL_XALLOC_FREE);
            appendli("cmp rax, rbx");
            appendli("jne .l%zu_expr_unary_neg_op", expr_id);
            appendli("call __integer_oor_handler");
        }
        appendln(".l%zu_expr_unary_neg_op:", expr_id);
        appendli("neg rax");
        appendli("push rax");
        appendln(".l%zu_expr_unary_neg_end:", expr_id);
        return;
    }
    case UOP_BITNOT: {
        codegen_rvalue(expr->data.unary.rhs);
        assert(expr->data.unary.rhs->type->size <= 8u);
        appendli("pop rax");
        appendli("not rax");
        appendli("push rax");
        return;
    }
    case UOP_DEREFERENCE: {
        assert(expr->data.unary.rhs->type->kind == TYPE_POINTER);
        if (expr->data.unary.rhs->type->data.pointer.base->size == 0) {
            // Dereferencing an object with zero size produces a result with
            // no size (equivalent to no result).
            return;
        }
        codegen_rvalue(expr->data.unary.rhs);
        appendli("pop rax");
        size_t const size = expr->data.unary.rhs->type->size;
        push(size);
        copy_rax_rsp_via_rcx(size);
        return;
    }
    case UOP_ADDRESSOF: {
        codegen_lvalue(expr->data.unary.rhs);
        return;
    }
    case UOP_COUNTOF: {
        // TODO: Currently the right-hand-side array or slice expression in a
        // countof expression is always evaluated since the right-hand-side
        // expression may exhibit side effects. In the future there is an
        // optimization opportunity to skip evaluation of the right-hand-side
        // expression if it can be determined that the expression does not
        // contain side effects (e.g. it is an identifier).

        if (expr->data.unary.rhs->type->kind == TYPE_ARRAY) {
            // If possible evaluate the left hand side of the expression as an
            // lvalue so that we do not push the entire contents of the array
            // onto the stack.
            if (tir_expr_is_lvalue(expr->data.unary.rhs)) {
                codegen_lvalue(expr->data.unary.rhs);
                appendli("pop rax ; discard array lvalue");
            }
            else {
                codegen_rvalue(expr->data.unary.rhs);
                pop(expr->data.unary.rhs->type->size);
            }
            appendli(
                "mov rax, %zu; array count",
                expr->data.unary.rhs->type->data.array.count);
            appendli("push rax");
            return;
        }

        if (expr->data.unary.rhs->type->kind == TYPE_SLICE) {
            codegen_rvalue(expr->data.unary.rhs);
            appendli("pop rax ; pop slice pointer word");
            return;
        }

        UNREACHABLE();
    }
    }

    UNREACHABLE();
}

static void
codegen_rvalue_binary(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_BINARY);

    switch (expr->data.binary.op) {
    case BOP_OR: {
        assert(expr->data.binary.lhs->type->kind == TYPE_BOOL);
        assert(expr->data.binary.rhs->type->kind == TYPE_BOOL);
        assert(expr->data.binary.lhs->type->size == 1u);
        assert(expr->data.binary.rhs->type->size == 1u);
        size_t const id = unique_id++;

        // TODO: Remove redundant jumps.
        appendln(".l%zu_binary_or_lhs:", id);
        codegen_rvalue(expr->data.binary.lhs);
        appendli("pop rax");
        appendli("mov rbx, 0x00");
        appendli("cmp rax, rbx");
        appendli("jne .l%zu_binary_or_true", id);
        appendli("jmp .l%zu_binary_or_rhs", id);

        appendln(".l%zu_binary_or_rhs:", id);
        codegen_rvalue(expr->data.binary.rhs);
        appendli("pop rax");
        appendli("mov rbx, 0x00");
        appendli("cmp rax, rbx");
        appendli("jne .l%zu_binary_or_true", id);
        appendli("jmp .l%zu_binary_or_false", id);

        appendln(".l%zu_binary_or_true:", id);
        appendli("push 0x01");
        appendli("jmp .l%zu_binary_or_end", id);

        appendln(".l%zu_binary_or_false:", id);
        appendli("push 0x00");
        appendli("jmp .l%zu_binary_or_end", id);

        appendln(".l%zu_binary_or_end:", id);
        return;
    }
    case BOP_AND: {
        assert(expr->data.binary.lhs->type->kind == TYPE_BOOL);
        assert(expr->data.binary.rhs->type->kind == TYPE_BOOL);
        assert(expr->data.binary.lhs->type->size == 1u);
        assert(expr->data.binary.rhs->type->size == 1u);
        size_t const id = unique_id++;

        // TODO: Remove redundant jumps.
        appendln(".l%zu_binary_and_lhs:", id);
        codegen_rvalue(expr->data.binary.lhs);
        appendli("pop rax");
        appendli("mov rbx, 0x00");
        appendli("cmp rax, rbx");
        appendli("jne .l%zu_binary_and_rhs", id);
        appendli("jmp .l%zu_binary_and_false", id);

        appendln(".l%zu_binary_and_rhs:", id);
        codegen_rvalue(expr->data.binary.rhs);
        appendli("pop rax");
        appendli("mov rbx, 0x00");
        appendli("cmp rax, rbx");
        appendli("jne .l%zu_binary_and_true", id);
        appendli("jmp .l%zu_binary_and_false", id);

        appendln(".l%zu_binary_and_true:", id);
        appendli("push 0x01");
        appendli("jmp .l%zu_binary_and_end", id);

        appendln(".l%zu_binary_and_false:", id);
        appendli("push 0x00");
        appendli("jmp .l%zu_binary_and_end", id);

        appendln(".l%zu_binary_and_end:", id);
        return;
    }
    case BOP_EQ: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);

        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp rax, rbx");
        appendli("cmove rcx, rdx");
        appendli("push rcx");
        return;
    }
    case BOP_NE: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);

        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp rax, rbx");
        appendli("cmovne rcx, rdx");
        appendli("push rcx");
        return;
    }
    case BOP_LE: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);

        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp rax, rbx");
        appendli(
            "%s rcx, rdx", type_is_sinteger(xhs_type) ? "cmovle" : "cmovbe");
        appendli("push rcx");
        return;
    }
    case BOP_LT: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);

        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp rax, rbx");
        appendli("%s rcx, rdx", type_is_sinteger(xhs_type) ? "cmovl" : "cmovb");
        appendli("push rcx");
        return;
    }
    case BOP_GE: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);

        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp rax, rbx");
        appendli(
            "%s rcx, rdx", type_is_sinteger(xhs_type) ? "cmovge" : "cmovae");
        appendli("push rcx");
        return;
    }
    case BOP_GT: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);

        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp rax, rbx");
        appendli("%s rcx, rdx", type_is_sinteger(xhs_type) ? "cmovg" : "cmova");
        appendli("push rcx");
        return;
    }
    case BOP_ADD: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;
        size_t const expr_id = unique_id++;

        char const* const lhs_reg = reg_a(xhs_type->size);
        char const* const rhs_reg = reg_b(xhs_type->size);
        char const* const jmp_not_overflow =
            type_is_sinteger(xhs_type) ? "jno" : "jnc";

        appendln(".l%zu_expr_binary_add_bgn:", expr_id);
        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("add %s, %s", lhs_reg, rhs_reg);
        appendli("push rax");
        appendli("%s .l%zu_expr_binary_add_end", jmp_not_overflow, expr_id);
        appendli("call __integer_oor_handler");
        appendln(".l%zu_expr_binary_add_end:", expr_id);
        return;
    }
    case BOP_SUB: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;
        size_t const expr_id = unique_id++;

        char const* const lhs_reg = reg_a(xhs_type->size);
        char const* const rhs_reg = reg_b(xhs_type->size);
        char const* const jmp_not_overflow =
            type_is_sinteger(xhs_type) ? "jno" : "jnc";

        appendln(".l%zu_expr_binary_add_bgn:", expr_id);
        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("sub %s, %s", lhs_reg, rhs_reg);
        appendli("push rax");
        appendli("%s .l%zu_expr_binary_add_end", jmp_not_overflow, expr_id);
        appendli("call __integer_oor_handler");
        appendln(".l%zu_expr_binary_add_end:", expr_id);
        return;
    }
    case BOP_MUL: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;
        size_t const expr_id = unique_id++;

        char const* const rhs_reg = reg_b(xhs_type->size);
        char const* const mul = type_is_sinteger(xhs_type) ? "imul" : "mul";

        appendln(".l%zu_expr_binary_mul_bgn:", expr_id);
        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("%s %s", mul, rhs_reg);
        appendli("push rax");
        appendli("jno .l%zu_expr_binary_mul_end", expr_id);
        appendli("call __integer_oor_handler");
        appendln(".l%zu_expr_binary_mul_end:", expr_id);
        return;
    }
    case BOP_DIV: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;
        size_t const expr_id = unique_id++;

        char const* const rhs_reg = reg_b(xhs_type->size);
        char const* const div = type_is_sinteger(xhs_type) ? "idiv" : "div";

        appendln(".l%zu_expr_binary_div_bgn:", expr_id);
        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rdx, 0");
        appendli("cmp rbx, 0"); // divide-by-zero check
        appendli("jne .l%zu_expr_binary_div_op", expr_id);
        appendli("call __integer_divz_handler");
        appendli(".l%zu_expr_binary_div_op:", expr_id);
        appendli("%s %s", div, rhs_reg);
        appendli("push rax");
        appendln(".l%zu_expr_binary_div_end:", expr_id);
        return;
    }
    case BOP_BITOR: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);

        appendli("pop rbx");
        appendli("pop rax");
        appendli("or rax, rbx");
        appendli("push rax");
        return;
    }
    case BOP_BITXOR: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);

        appendli("pop rbx");
        appendli("pop rax");
        appendli("xor rax, rbx");
        appendli("push rax");
        return;
    }
    case BOP_BITAND: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);

        appendli("pop rbx");
        appendli("pop rax");
        appendli("and rax, rbx");
        appendli("push rax");
        return;
    }
    }

    UNREACHABLE();
}

static void
codegen_lvalue(struct tir_expr const* expr)
{
    assert(expr != NULL);

    switch (expr->kind) {
    case TIR_EXPR_IDENTIFIER: {
        push_address(expr->data.identifier->address);
        return;
    }
    case TIR_EXPR_INDEX: {
        codegen_lvalue_index(expr);
        return;
    }
    case TIR_EXPR_UNARY: {
        codegen_lvalue_unary(expr);
        return;
    }
    case TIR_EXPR_BOOLEAN: /* fallthrough */
    case TIR_EXPR_INTEGER: /* fallthrough */
    case TIR_EXPR_BYTES: /* fallthrough */
    case TIR_EXPR_LITERAL_ARRAY: /* fallthrough */
    case TIR_EXPR_LITERAL_SLICE: /* fallthrough */
    case TIR_EXPR_CAST: /* fallthrough */
    case TIR_EXPR_SYSCALL: /* fallthrough */
    case TIR_EXPR_CALL: /* fallthrough */
    case TIR_EXPR_SLICE: /* fallthrough */
    case TIR_EXPR_SIZEOF: /* fallthrough */
    case TIR_EXPR_BINARY: {
        assert(!tir_expr_is_lvalue(expr));
        UNREACHABLE();
    }
    }

    UNREACHABLE();
}
static void
codegen_lvalue_index(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_INDEX);

    if (expr->data.index.lhs->type->kind == TYPE_ARRAY) {
        codegen_lvalue_index_lhs_array(expr);
        return;
    }

    if (expr->data.index.lhs->type->kind == TYPE_SLICE) {
        codegen_lvalue_index_lhs_slice(expr);
        return;
    }

    UNREACHABLE();
}

static void
codegen_lvalue_index_lhs_array(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_INDEX);
    assert(expr->data.index.lhs->type->kind == TYPE_ARRAY);
    assert(expr->data.index.idx->type->kind == TYPE_USIZE);

    struct type const* const lhs_type = expr->data.index.lhs->type;
    struct type const* const element_type = lhs_type->data.array.base;
    if (element_type->size == 0) {
        appendli("push __nil");
        return;
    }

    size_t const expr_id = unique_id++;
    codegen_lvalue(expr->data.index.lhs);
    codegen_rvalue(expr->data.index.idx);
    appendli("pop rax"); // index
    appendli("mov rbx, %zu", lhs_type->data.array.count); // count
    appendli("cmp rax, rbx");
    appendli("jb .l%zu_index_array_lvalue_calc", expr_id);
    appendli("call __index_oob_handler");
    appendln(".l%zu_index_array_lvalue_calc:", expr_id);
    appendli("mov rbx, %zu", element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // index * sizeof(element_type)
    appendli("pop rbx"); // start
    appendli("add rax, rbx"); // start + index * sizeof(element_type)
    appendli("push rax");
}

static void
codegen_lvalue_index_lhs_slice(struct tir_expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == TIR_EXPR_INDEX);
    assert(expr->data.index.lhs->type->kind == TYPE_SLICE);
    assert(expr->data.index.idx->type->kind == TYPE_USIZE);

    struct type const* const lhs_type = expr->data.index.lhs->type;
    struct type const* const element_type = lhs_type->data.slice.base;
    if (element_type->size == 0) {
        appendli("push __nil");
        return;
    }

    size_t const expr_id = unique_id++;
    codegen_rvalue(expr->data.index.lhs);
    codegen_rvalue(expr->data.index.idx);
    appendli("pop rax"); // index
    appendli("mov rbx, [rsp + 8]"); // slice count
    appendli("cmp rax, rbx");
    appendli("jb .l%zu_slice_lvalue_calc", expr_id);
    appendli("call __index_oob_handler");
    appendln(".l%zu_slice_lvalue_calc:", expr_id);
    appendli("mov rbx, %zu", element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // index * sizeof(element_type)
    appendli("pop rbx"); // start
    appendli("add rax, rbx"); // start + index * sizeof(element_type)
    pop(8u); // slice count
    appendli("push rax");
}

static void
codegen_lvalue_unary(struct tir_expr const* expr)
{
    assert(expr != NULL);

    switch (expr->data.unary.op) {
    case UOP_DEREFERENCE: {
        assert(expr->data.unary.rhs->type->kind == TYPE_POINTER);
        codegen_rvalue(expr->data.unary.rhs);
        return;
    }
    case UOP_NOT: /* fallthrough */
    case UOP_POS: /* fallthrough */
    case UOP_NEG: /* fallthrough */
    case UOP_BITNOT: /* fallthrough */
    case UOP_ADDRESSOF: /* fallthrough */
    case UOP_COUNTOF: {
        assert(!tir_expr_is_lvalue(expr));
        UNREACHABLE();
    }
    }

    UNREACHABLE();
}

void
codegen(char const* const opt_o, bool opt_k)
{
    assert(opt_o != NULL);

    out = autil_string_new(NULL, 0u);
    struct autil_string* const asm_path = autil_string_new_fmt("%s.asm", opt_o);
    struct autil_string* const obj_path = autil_string_new_fmt("%s.o", opt_o);

    codegen_core();
    appendch('\n');
    codegen_static_constants();
    appendch('\n');
    codegen_static_variables();
    appendch('\n');
    codegen_static_functions();

    if (autil_file_write(
            autil_string_start(asm_path),
            autil_string_start(out),
            autil_string_count(out))) {
        fatal(
            NULL,
            "unable to write file `%s` with error '%s'",
            autil_string_start(asm_path),
            strerror(errno));
    }
    autil_string_del(out);

    // clang-format off
    char const* const nasm_argv[] = {
        "nasm", "-w+error=all", "-f", "elf64", "-O0", "-g", "-F", "dwarf",
        autil_string_start(asm_path), (char const*)NULL
    };
    // clang-format on
    xspawnvpw("nasm", nasm_argv);

    // clang-format off
    char const* const ld_argv[] = {
        "ld", "-o",  opt_o, autil_string_start(obj_path), (char const*)NULL
    };
    // clang-format on
    xspawnvpw("ld", ld_argv);

    if (!opt_k) {
        // clang-format off
        char const* const rm_argv[] = {
            "rm",
            autil_string_start(asm_path),
            autil_string_start(obj_path),
            (char const*)NULL
        };
        // clang-format on
        xspawnvpw("rm", rm_argv);
    }

    autil_string_del(asm_path);
    autil_string_del(obj_path);
}
