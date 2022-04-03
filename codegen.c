// Copyright 2021 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "sunder.h"

static struct sunder_string* out = NULL;
static struct function const* current_function = NULL;
static size_t current_loop_id; // Used for generating break & continue lables.
static size_t unique_id = 0; // Used for generating unique names and labels.

// Local labels take the form:
//      .__<TIR-node-type>_<unique-id>_<description>
//
// The <TIR-node-type> is STMT for stmt nodes and EXPR for expr nodes.
//
// The <unique-id> is generated for each node in the call to codegen_stmt,
// codegen_rvalue, and codegen_lvalue.
//
// The <description> is used to denote what section of TIR node generation is
// taking place as well as provide the necessary labels for jumps.
//
// For each stmt and expr node, the local labels:
//      .__<TIR-node-type>_<unique_id>_bgn:
// and
//      .__<TIR-node-type>_<unique_id>_end:
// are generated to denote the beginning and end of code generation for that
// node. These labels are always present and may be used for jump targets.
#define LABEL_STMT ".__STMT_"
#define LABEL_EXPR ".__EXPR_"

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
static char const*
reg_a(size_t size);
// Register bl, bx, ebx, or rbx based on size.
static char const*
reg_b(size_t size);
// Register cl, cx, ecx, or rcx based on size.
static char const*
reg_c(size_t size);

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
    sunder_string_append_vfmt(out, fmt, args);
    va_end(args);
}

static void
appendln(char const* fmt, ...)
{
    assert(out != NULL);
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);
    sunder_string_append_vfmt(out, fmt, args);
    va_end(args);

    sunder_string_append_cstr(out, "\n");
}

static void
appendli(char const* fmt, ...)
{
    assert(out != NULL);
    assert(fmt != NULL);

    sunder_string_append_cstr(out, "    ");

    va_list args;
    va_start(args, fmt);
    sunder_string_append_vfmt(out, fmt, args);
    va_end(args);

    sunder_string_append_cstr(out, "\n");
}

static void
appendli_location(struct source_location const* location, char const* fmt, ...)
{
    assert(out != NULL);
    assert(location != NULL);
    assert(location->path != NO_PATH);
    assert(location->line != NO_LINE);
    assert(location->psrc != NO_PSRC);

    sunder_string_append_fmt(
        out, "    ; [%s:%zu] ", location->path, location->line);

    va_list args;
    va_start(args, fmt);
    sunder_string_append_vfmt(out, fmt, args);
    va_end(args);

    sunder_string_append_cstr(out, "\n");

    char const* const line_start = source_line_start(location->psrc);
    char const* const line_end = source_line_end(location->psrc);

    sunder_string_append_fmt(
        out, "    ;%.*s\n", (int)(line_end - line_start), line_start);
    sunder_string_append_fmt(
        out, "    ;%*s^\n", (int)(location->psrc - line_start), "");
}

static void
appendch(char ch)
{
    assert(out != NULL);

    sunder_string_append(out, &ch, 1u);
}

static void
append_dx_type(struct type const* type)
{
    assert(out != NULL);
    assert(type != NULL);
    assert(type->size != 0);

    switch (type->kind) {
    case TYPE_ANY: /* fallthrough */
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
    case TYPE_INTEGER: {
        UNREACHABLE();
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
    case TYPE_STRUCT: {
        append("db");
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
    case TYPE_ANY: /* fallthrough */
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
        sunder_sbuf(uint8_t) const bytes = value_to_new_bytes(value);
        for (size_t i = 0; i < sunder_sbuf_count(bytes); ++i) {
            if (i != 0) {
                append(", ");
            }
            append("%#x", (unsigned)bytes[i]);
        }
        sunder_sbuf_fini(bytes);
        return;
    }
    case TYPE_INTEGER: {
        UNREACHABLE();
    }
    case TYPE_FUNCTION: {
        struct address const* const address = value->data.function->address;
        assert(address->kind == ADDRESS_STATIC);
        assert(address->data.static_.offset == 0);
        append("%s", address->data.static_.name);
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
        sunder_sbuf(struct value*) const elements = value->data.array.elements;
        struct value* const ellipsis = value->data.array.ellipsis;

        // One dimensional arrays may use NASM's times prefix to repeat the
        // ellipsis element. However if the array element is itself an array
        // then it does not appear as if there is a way to nest times prefixes,
        // so data for the entire array must be generated. Fortunately this is
        // unlikely to be a case encountered by many users as ellipsis
        // initialization is mainly used for zeroing one dimensional buffers.
        if (value->type->data.array.base->kind == TYPE_ARRAY) {
            size_t const count = value->type->data.array.count;
            for (size_t i = 0; i < count; ++i) {
                if (i != 0 && i < sunder_sbuf_count(elements)) {
                    append(", ");
                }

                if (i < sunder_sbuf_count(elements)) {
                    append_dx_data(elements[i]);
                }
                else {
                    assert(ellipsis != NULL);
                    append_dx_data(ellipsis);
                }
            }
            return;
        }

        if (sunder_sbuf_count(elements) != 0) {
            for (size_t i = 0; i < sunder_sbuf_count(elements); ++i) {
                appendch('\n');
                appendli("; element %zu", i);
                append("    ");
                append_dx_type(value->type->data.array.base);
                appendch(' ');
                append_dx_data(elements[i]);
            }
        }
        if (ellipsis != NULL) {
            size_t const times =
                value->type->data.array.count - sunder_sbuf_count(elements);
            appendch('\n');
            appendli("; ellipsis element...");
            append("    times %zu", times);
            appendch(' ');
            append_dx_type(value->type->data.array.base);
            appendch(' ');
            append_dx_data(ellipsis);
        }
        return;
    }
    case TYPE_SLICE: {
        append_dx_data(value->data.slice.pointer);
        append(", ");
        // Due to an existing bug in the kinda hacky way that append_dx_type and
        // append_dx_data work, the append_dx_data call on the pointer will
        // correctly produce the dq address, but an append_dx_data on the usize
        // count will be written out as bytes. Since both the pointer and the
        // count need to be written in their qd representation, we manually
        // write the dq value of the count here instead of making a call to
        // append_dx_data.
        char* const count_cstr = sunder_bigint_to_new_cstr(
            value->data.slice.count->data.integer, NULL);
        append("%s", count_cstr);
        sunder_xalloc(count_cstr, SUNDER_XALLOC_FREE);
        return;
    }
    case TYPE_STRUCT: {
        sunder_sbuf(struct member_variable) const member_variable_defs =
            value->type->data.struct_.member_variables;
        size_t const member_variable_defs_count =
            sunder_sbuf_count(member_variable_defs);
        appendch('\n');
        for (size_t i = 0; i < member_variable_defs_count; ++i) {
            struct member_variable const* const def = member_variable_defs + i;
            if (i != 0) {
                struct member_variable const* const prev_def =
                    member_variable_defs + (i - 1);
                size_t const padding =
                    def->offset - (prev_def->offset + prev_def->type->size);
                if (padding != 0) {
                    appendln("    ; padding");
                    append("    db ");
                    for (size_t i = 0; i < padding; ++i) {
                        if (i != 0) {
                            append(", ");
                        }
                        append("0x00");
                    }
                    appendch('\n');
                }
            }
            struct value const* const val =
                value->data.struct_.member_variables[i];

            appendln(
                "    ; member variable %s: %s", def->name, def->type->name);
            append("    ");
            append_dx_type(def->type);
            appendch(' ');
            append_dx_data(val);
            appendch('\n');
        }
        return;
    }
    }

    UNREACHABLE();
}

static void
push(size_t size)
{
    if (size == 0) {
        appendli("; (push of size zero)");
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
        addr = sunder_cstr_new_fmt(
            "(%s + %zu)",
            address->data.static_.name,
            address->data.static_.offset);
        break;
    case ADDRESS_LOCAL:
        addr = sunder_cstr_new_fmt("rbp + %d", address->data.local.rbp_offset);
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

    sunder_xalloc(addr, SUNDER_XALLOC_FREE);
}

static void
pop(size_t size)
{
    if (size == 0) {
        appendli("; (pop of size zero)");
        return;
    }

    appendli("add rsp, %#zx", ceil8zu(size));
}

static char const*
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
    return NULL;
}

static char const*
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
    return NULL;
}

static char const*
reg_c(size_t size)
{
    switch (size) {
    case 1:
        return "cl";
    case 2:
        return "cx";
    case 4:
        return "ecx";
    case 8:
        return "rcx";
    }
    UNREACHABLE();
    return NULL;
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
codegen_sys(void);

static void
codegen_static_object(struct symbol const* symbol);
static void
codegen_static_function(struct symbol const* symbol);

static void
codegen_stmt(struct stmt const* stmt);
static void
codegen_stmt_if(struct stmt const* stmt, size_t id);
static void
codegen_stmt_for_range(struct stmt const* stmt, size_t id);
static void
codegen_stmt_for_expr(struct stmt const* stmt, size_t id);
static void
codegen_stmt_break(struct stmt const* stmt, size_t id);
static void
codegen_stmt_continue(struct stmt const* stmt, size_t id);
static void
codegen_stmt_dump(struct stmt const* stmt, size_t id);
static void
codegen_stmt_return(struct stmt const* stmt, size_t id);
static void
codegen_stmt_assign(struct stmt const* stmt, size_t id);
static void
codegen_stmt_expr(struct stmt const* stmt, size_t id);

static void
codegen_rvalue(struct expr const* expr);
static void
codegen_rvalue_symbol(struct expr const* expr, size_t id);
static void
codegen_rvalue_boolean(struct expr const* expr, size_t id);
static void
codegen_rvalue_integer(struct expr const* expr, size_t id);
static void
codegen_rvalue_bytes(struct expr const* expr, size_t id);
static void
codegen_rvalue_array(struct expr const* expr, size_t id);
static void
codegen_rvalue_slice(struct expr const* expr, size_t id);
static void
codegen_rvalue_array_slice(struct expr const* expr, size_t id);
static void
codegen_rvalue_struct(struct expr const* expr, size_t id);
static void
codegen_rvalue_cast(struct expr const* expr, size_t id);
static void
codegen_rvalue_syscall(struct expr const* expr, size_t id);
static void
codegen_rvalue_call(struct expr const* expr, size_t id);
static void
codegen_rvalue_access_index(struct expr const* expr, size_t id);
static void
codegen_rvalue_access_index_lhs_array(struct expr const* expr, size_t id);
static void
codegen_rvalue_access_index_lhs_slice(struct expr const* expr, size_t id);
static void
codegen_rvalue_access_slice(struct expr const* expr, size_t id);
static void
codegen_rvalue_access_slice_lhs_array(struct expr const* expr, size_t id);
static void
codegen_rvalue_access_slice_lhs_slice(struct expr const* expr, size_t id);
static void
codegen_rvalue_access_member_variable(struct expr const* expr, size_t id);
static void
codegen_rvalue_sizeof(struct expr const* expr, size_t id);
static void
codegen_rvalue_alignof(struct expr const* expr, size_t id);
static void
codegen_rvalue_unary(struct expr const* expr, size_t id);
static void
codegen_rvalue_binary(struct expr const* expr, size_t id);

static void
codegen_lvalue(struct expr const* expr);
static void
codegen_lvalue_symbol(struct expr const* expr, size_t id);
static void
codegen_lvalue_access_index(struct expr const* expr, size_t id);
static void
codegen_lvalue_access_index_lhs_array(struct expr const* expr, size_t id);
static void
codegen_lvalue_access_index_lhs_slice(struct expr const* expr, size_t id);
static void
codegen_lvalue_access_member_variable(struct expr const* expr, size_t id);
static void
codegen_lvalue_unary(struct expr const* expr, size_t id);

static void
codegen_static_constants(void)
{
    appendln("; STATIC CONSTANTS");
    appendln("section .rodata");

    for (size_t i = 0; i < sunder_sbuf_count(context()->static_symbols); ++i) {
        struct symbol const* const symbol = context()->static_symbols[i];
        assert(symbol_xget_address(symbol)->kind == ADDRESS_STATIC);
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

    for (size_t i = 0; i < sunder_sbuf_count(context()->static_symbols); ++i) {
        struct symbol const* const symbol = context()->static_symbols[i];
        assert(symbol_xget_address(symbol)->kind == ADDRESS_STATIC);
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

    for (size_t i = 0; i < sunder_sbuf_count(context()->static_symbols); ++i) {
        struct symbol const* const symbol = context()->static_symbols[i];
        assert(symbol_xget_address(symbol)->kind == ADDRESS_STATIC);
        if (symbol->kind != SYMBOL_FUNCTION) {
            continue;
        }
        codegen_static_function(symbol);
    }
}

static void
codegen_sys(void)
{
    char const* SUNDER_HOME = getenv("SUNDER_HOME");
    if (SUNDER_HOME == NULL) {
        fatal(NULL, "missing environment variable SUNDER_HOME");
    }
    struct sunder_string* const core =
        sunder_string_new_fmt("%s/lib/sys/sys.asm", SUNDER_HOME);

    void* buf = NULL;
    size_t buf_size = 0;
    if (sunder_file_read(sunder_string_start(core), &buf, &buf_size)) {
        fatal(
            NULL,
            "failed to read '%s' with error '%s'",
            sunder_string_start(core),
            strerror(errno));
    }
    append("%.*s", (int)buf_size, (char const*)buf);

    sunder_string_del(core);
    sunder_xalloc(buf, SUNDER_XALLOC_FREE);
}

static void
codegen_static_object(struct symbol const* symbol)
{
    assert(symbol != NULL);
    assert(symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_CONSTANT);
    assert(symbol_xget_address(symbol)->kind == ADDRESS_STATIC);

    if (symbol->kind == SYMBOL_VARIABLE
        && symbol->data.variable.value == NULL) {
        // Symbol is defined externally.
        return;
    }

    struct type const* const type = symbol_xget_type(symbol);
    if (type->size == 0) {
        // Zero-sized objects should take up zero space. Attempting to take the
        // address of a zero-sized symbol should always produce a pointer with
        // the value zero.
        return;
    }

    assert(symbol_xget_address(symbol)->data.static_.offset == 0);
    append("%s:", symbol_xget_address(symbol)->data.static_.name);
    if (type->kind != TYPE_ARRAY && type->kind != TYPE_STRUCT) {
        // Only genreate the dx type for non-arrays / non-structs as
        // arrays/struct have thir own special way of initializing data from a
        // combination of explicitly specified data.
        //
        // TODO: The fact that we need this special case here means that there
        // is something not-quite-so-well thought out in the way that
        // append_dx_type and append_dx_data were planned out. Look into
        // alternative designs that provide a better abstraction.
        appendch(' ');
        append_dx_type(symbol_xget_type(symbol));
        appendch(' ');
    }
    append_dx_data(symbol_xget_value(symbol));
    appendch('\n');
    return;
}

static void
codegen_static_function(struct symbol const* symbol)
{
    assert(symbol != NULL);
    assert(symbol->kind == SYMBOL_FUNCTION);

    assert(symbol_xget_value(symbol)->type->kind == TYPE_FUNCTION);
    struct function const* const function =
        symbol_xget_value(symbol)->data.function;

    assert(symbol_xget_address(symbol)->data.static_.offset == 0);
    appendln("global %s", symbol_xget_address(symbol)->data.static_.name);
    appendln("%s:", symbol_xget_address(symbol)->data.static_.name);
    appendli("; PROLOGUE");
    // Save previous frame pointer.
    // With this push, the stack should now be 16-byte aligned.
    appendli("push rbp");
    // Move stack pointer into current frame pointer.
    appendli("mov rbp, rsp");
    // Adjust the stack pointer to make space for locals.
    assert(function->local_stack_offset <= 0);
    appendli("add rsp, %d ; local stack space", function->local_stack_offset);

    assert(current_function == NULL);
    current_function = function;
    for (size_t i = 0; i < sunder_sbuf_count(function->body->stmts); ++i) {
        struct stmt const* const stmt = function->body->stmts[i];
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
codegen_stmt(struct stmt const* stmt)
{
    assert(stmt != NULL);

    static struct {
        char const* kind_cstr;
        void (*codegen_fn)(struct stmt const*, size_t);
    } const table[] = {
#define TABLE_ENTRY(kind, fn) [kind] = {#kind, fn}
        TABLE_ENTRY(STMT_IF, codegen_stmt_if),
        TABLE_ENTRY(STMT_FOR_RANGE, codegen_stmt_for_range),
        TABLE_ENTRY(STMT_FOR_EXPR, codegen_stmt_for_expr),
        TABLE_ENTRY(STMT_BREAK, codegen_stmt_break),
        TABLE_ENTRY(STMT_CONTINUE, codegen_stmt_continue),
        TABLE_ENTRY(STMT_DUMP, codegen_stmt_dump),
        TABLE_ENTRY(STMT_RETURN, codegen_stmt_return),
        TABLE_ENTRY(STMT_ASSIGN, codegen_stmt_assign),
        TABLE_ENTRY(STMT_EXPR, codegen_stmt_expr),
#undef TABLE_ENTRY
    };

    size_t const id = unique_id++;
    char const* const cstr = table[stmt->kind].kind_cstr;
    appendln("%s%zu_bgn:", LABEL_STMT, id);
    appendli_location(stmt->location, "%s (ID %zu)", cstr, id);
    table[stmt->kind].codegen_fn(stmt, id);
    appendln("%s%zu_end:", LABEL_STMT, id);
}

static void
codegen_stmt_if(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_IF);

    sunder_sbuf(struct conditional const* const) const conditionals =
        stmt->data.if_.conditionals;
    for (size_t i = 0; i < sunder_sbuf_count(conditionals); ++i) {
        bool const is_last = i == (sunder_sbuf_count(conditionals) - 1);

        if (conditionals[i]->condition != NULL) {
            appendln("%s%zu_condition_%zu:", LABEL_STMT, id, i);
            assert(conditionals[i]->condition->type->kind == TYPE_BOOL);
            codegen_rvalue(conditionals[i]->condition);
            appendli("pop rax");
            appendli("mov rbx, 0x00");
            appendli("cmp al, bl");
            if (is_last) {
                appendli("je %s%zu_end", LABEL_STMT, id);
            }
            else {
                appendli("je %s%zu_condition_%zu", LABEL_STMT, id, i + 1);
            }
        }
        else {
            appendln("%s%zu_condition_%zu:", LABEL_STMT, id, i);
            appendli("; else condition (always true)");
        }

        appendln("%s%zu_body_%zu:", LABEL_STMT, id, i);
        sunder_sbuf(struct stmt const* const) const stmts =
            conditionals[i]->body->stmts;
        for (size_t i = 0; i < sunder_sbuf_count(stmts); ++i) {
            codegen_stmt(stmts[i]);
        }
        appendli("jmp %s%zu_end", LABEL_STMT, id);
    }
}

static void
codegen_stmt_for_range(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_FOR_RANGE);

    assert(
        symbol_xget_type(stmt->data.for_range.loop_variable)
        == context()->builtin.usize);
    assert(stmt->data.for_range.begin->type == context()->builtin.usize);
    assert(stmt->data.for_range.end->type == context()->builtin.usize);
    assert(
        symbol_xget_address(stmt->data.for_range.loop_variable)->kind
        == ADDRESS_LOCAL);

    size_t const save_current_loop_id = current_loop_id;
    current_loop_id = id;

    push_address(symbol_xget_address(stmt->data.for_range.loop_variable));
    codegen_rvalue(stmt->data.for_range.begin);
    appendli("pop rbx"); // begin
    appendli("pop rax"); // addr of loop variable
    appendli("mov [rax], rbx");
    appendln("%s%zu_condition:", LABEL_STMT, id);
    push_at_address(
        symbol_xget_type(stmt->data.for_range.loop_variable)->size,
        symbol_xget_address(stmt->data.for_range.loop_variable));
    codegen_rvalue(stmt->data.for_range.end);
    appendli("pop rbx"); // end
    appendli("pop rax"); // loop variable
    appendli("cmp rax, rbx");
    appendli("je %s%zu_end", LABEL_STMT, id);
    appendln("%s%zu_body_bgn:", LABEL_STMT, id);
    sunder_sbuf(struct stmt const* const) const stmts =
        stmt->data.for_range.body->stmts;
    for (size_t i = 0; i < sunder_sbuf_count(stmts); ++i) {
        codegen_stmt(stmts[i]);
    }
    appendln("%s%zu_body_end:", LABEL_STMT, id);
    appendli(
        "inc qword [rbp + %d]",
        symbol_xget_address(stmt->data.for_range.loop_variable)
            ->data.local.rbp_offset);
    appendli("jmp %s%zu_condition", LABEL_STMT, id);

    current_loop_id = save_current_loop_id;
}

static void
codegen_stmt_for_expr(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_FOR_EXPR);

    size_t const save_current_loop_id = current_loop_id;
    current_loop_id = id;

    appendln("%s%zu_condition:", LABEL_STMT, id);
    assert(stmt->data.for_expr.expr->type->kind == TYPE_BOOL);
    codegen_rvalue(stmt->data.for_expr.expr);
    appendli("pop rax");
    appendli("mov rbx, 0x00");
    appendli("cmp al, bl");
    appendli("je %s%zu_end", LABEL_STMT, id);
    appendln("%s%zu_body_bgn:", LABEL_STMT, id);
    sunder_sbuf(struct stmt const* const) const stmts =
        stmt->data.for_expr.body->stmts;
    for (size_t i = 0; i < sunder_sbuf_count(stmts); ++i) {
        codegen_stmt(stmts[i]);
    }
    appendln("%s%zu_body_end:", LABEL_STMT, id);
    appendli("jmp %s%zu_condition", LABEL_STMT, id);

    current_loop_id = save_current_loop_id;
}

static void
codegen_stmt_break(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_BREAK);
    (void)stmt;
    (void)id;

    appendli("jmp %s%zu_end", LABEL_STMT, current_loop_id);
}

static void
codegen_stmt_continue(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_CONTINUE);
    (void)stmt;
    (void)id;

    appendli("jmp %s%zu_body_end", LABEL_STMT, current_loop_id);
}

static void
codegen_stmt_dump(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_DUMP);
    (void)id;

    appendli(
        "; dump `%s` (%zu bytes)",
        stmt->data.expr->type->name,
        stmt->data.expr->type->size);
    codegen_rvalue(stmt->data.expr);
    appendli(
        "push %#zx ; push type `%s` size",
        stmt->data.expr->type->size,
        stmt->data.expr->type->name);
    appendli("call __dump");
    appendli("pop rax ; pop type `%s` size", stmt->data.expr->type->name);
    pop(stmt->data.expr->type->size);
}

static void
codegen_stmt_return(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_RETURN);
    (void)id;

    if (stmt->data.return_.expr != NULL) {
        // Compute result.
        codegen_rvalue(stmt->data.return_.expr);

        // Store in return address.
        struct symbol const* const return_symbol = symbol_table_lookup(
            current_function->symbol_table, context()->interned.return_);
        // rbx := destination
        assert(symbol_xget_address(return_symbol)->kind == ADDRESS_LOCAL);
        appendli("mov rbx, rbp");
        appendli(
            "add rbx, %d ; return symbol rbp offset",
            symbol_xget_address(return_symbol)->data.local.rbp_offset);
        copy_rsp_rbx_via_rcx(symbol_xget_type(return_symbol)->size);
    }

    appendli("; STMT_RETURN EPILOGUE");
    // Restore stack pointer.
    appendli("mov rsp, rbp");
    // Restore previous frame pointer.
    appendli("pop rbp");
    // Return control to the calling routine.
    appendli("ret");
}

static void
codegen_stmt_assign(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_ASSIGN);
    (void)id;

    if (stmt->data.assign.lhs->type->size == 0) {
        // NOP
        return;
    }

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
codegen_stmt_expr(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_EXPR);
    (void)id;

    codegen_rvalue(stmt->data.expr);
    // Remove the (unused) result from the stack.
    pop(stmt->data.expr->type->size);
}

static void
codegen_rvalue(struct expr const* expr)
{
    assert(expr != NULL);

    static struct {
        char const* kind_cstr;
        void (*codegen_fn)(struct expr const*, size_t);
    } const table[] = {
#define TABLE_ENTRY(kind, fn) [kind] = {#kind, fn}
        // clang-format off
        TABLE_ENTRY(EXPR_SYMBOL, codegen_rvalue_symbol),
        TABLE_ENTRY(EXPR_BOOLEAN, codegen_rvalue_boolean),
        TABLE_ENTRY(EXPR_INTEGER, codegen_rvalue_integer),
        TABLE_ENTRY(EXPR_BYTES, codegen_rvalue_bytes),
        TABLE_ENTRY(EXPR_ARRAY, codegen_rvalue_array),
        TABLE_ENTRY(EXPR_SLICE, codegen_rvalue_slice),
        TABLE_ENTRY(EXPR_ARRAY_SLICE, codegen_rvalue_array_slice),
        TABLE_ENTRY(EXPR_STRUCT, codegen_rvalue_struct),
        TABLE_ENTRY(EXPR_CAST, codegen_rvalue_cast),
        TABLE_ENTRY(EXPR_SYSCALL, codegen_rvalue_syscall),
        TABLE_ENTRY(EXPR_CALL, codegen_rvalue_call),
        TABLE_ENTRY(EXPR_ACCESS_INDEX, codegen_rvalue_access_index),
        TABLE_ENTRY(EXPR_ACCESS_SLICE, codegen_rvalue_access_slice),
        TABLE_ENTRY(EXPR_ACCESS_MEMBER_VARIABLE, codegen_rvalue_access_member_variable),
        TABLE_ENTRY(EXPR_SIZEOF, codegen_rvalue_sizeof),
        TABLE_ENTRY(EXPR_ALIGNOF, codegen_rvalue_alignof),
        TABLE_ENTRY(EXPR_UNARY, codegen_rvalue_unary),
        TABLE_ENTRY(EXPR_BINARY, codegen_rvalue_binary),
    // clang-format on
#undef TABLE_ENTRY
    };

    size_t const id = unique_id++;
    char const* const cstr = table[expr->kind].kind_cstr;
    appendln("%s%zu_bgn:", LABEL_EXPR, id);
    appendli_location(expr->location, "%s (ID %zu, RVALUE)", cstr, id);
    assert(table[expr->kind].codegen_fn != NULL);
    table[expr->kind].codegen_fn(expr, id);
    appendln("%s%zu_end:", LABEL_EXPR, id);
}

static void
codegen_rvalue_symbol(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SYMBOL);
    (void)id;

    struct symbol const* const symbol = expr->data.symbol;
    switch (symbol->kind) {
    case SYMBOL_TYPE: /* fallthrough */
    case SYMBOL_TEMPLATE: /* fallthrough */
    case SYMBOL_NAMESPACE: {
        UNREACHABLE();
    }
    case SYMBOL_VARIABLE: /* fallthrough */
    case SYMBOL_CONSTANT: {
        push_at_address(
            symbol_xget_type(symbol)->size, symbol_xget_address(symbol));
        return;
    }
    case SYMBOL_FUNCTION: {
        push_address(symbol_xget_address(symbol));
        return;
    }
    }

    UNREACHABLE();
}

static void
codegen_rvalue_boolean(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BOOLEAN);
    (void)id;

    appendli("mov rax, %s", expr->data.boolean ? "0x01" : "0x00");
    appendli("push rax");
}

static void
codegen_rvalue_integer(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_INTEGER);
    (void)id;

    char* const cstr = sunder_bigint_to_new_cstr(expr->data.integer, NULL);

    assert(expr->type->size >= 1u);
    assert(expr->type->size <= 8u);
    appendli("mov rax, %s", cstr);
    appendli("push rax");

    sunder_xalloc(cstr, SUNDER_XALLOC_FREE);
}

static void
codegen_rvalue_bytes(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BYTES);
    assert(expr->type->kind == TYPE_SLICE);
    (void)id;

    appendli("push %zu", expr->data.bytes.count); // count
    push_address(expr->data.bytes.address); // pointer
}

static void
codegen_rvalue_array(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ARRAY);
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
    sunder_sbuf(struct expr const* const) const elements =
        expr->data.array.elements;
    struct type const* const element_type = expr->type->data.array.base;
    size_t const element_size = element_type->size;
    // TODO: This loop is manually unrolled here, but should probably be turned
    // into an actual asm loop for elements with trivial initialization.
    // Basically we should account for the scenario where a brainfuck
    // interpreter allocates an array 30000 zeroed bytes, which would cause the
    // equivalent of memcpy(&my_array[index], &my_zero, 0x8) inline thousands of
    // times.
    for (size_t i = 0; i < sunder_sbuf_count(elements); ++i) {
        assert(elements[i]->type == element_type);
        codegen_rvalue(elements[i]);

        appendli("mov rbx, rsp");
        appendli("add rbx, %zu", ceil8zu(element_size)); // array start
        appendli("add rbx, %zu", element_size * i); // array offset
        copy_rsp_rbx_via_rcx(element_size);

        pop(element_size);
    }

    size_t const count = expr->type->data.array.count;
    if (sunder_sbuf_count(elements) < count) { // ellipsis
        assert(expr->data.array.ellipsis != NULL);
        assert(expr->data.array.ellipsis->type == element_type);

        // Number of elements already filled in.
        size_t const completed = sunder_sbuf_count(elements);
        // Number of elements remaining to be filled in with the ellipsis.
        size_t const remaining = count - sunder_sbuf_count(elements);

        appendln("%s%zu_ellipsis_bgn:", LABEL_EXPR, id);
        codegen_rvalue(expr->data.array.ellipsis);
        appendli("mov rbx, rsp");
        appendli("add rbx, %zu", ceil8zu(element_size)); // array start
        appendli("add rbx, %zu", element_size * completed); // array offset
        // rbx is now the destination register.
        appendli("mov rax, %zu", remaining); // rax := counter down to zero
        appendln("%s%zu_ellipsis_condition:", LABEL_EXPR, id);
        appendli("cmp rax, 0");
        appendli("je %s%zu_ellipsis_pop", LABEL_EXPR, id);
        copy_rsp_rbx_via_rcx(element_size);
        appendli("add rbx, %zu", element_size); // ptr = ptr + 1
        appendli("dec rax");
        appendli("jmp %s%zu_ellipsis_condition", LABEL_EXPR, id);
        appendln("%s%zu_ellipsis_pop:", LABEL_EXPR, id);
        pop(element_size);
        appendln("%s%zu_ellipsis_end:", LABEL_EXPR, id);
    }
}

static void
codegen_rvalue_slice(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SLICE);
    assert(expr->type->kind == TYPE_SLICE);
    (void)id;

    // +---------+
    // | count   |
    // +---------+ <-- rsp + 0x8
    // | pointer |
    // +---------+ <-- rsp
    codegen_rvalue(expr->data.slice.count);
    codegen_rvalue(expr->data.slice.pointer);
}

static void
codegen_rvalue_array_slice(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ARRAY_SLICE);
    assert(expr->type->kind == TYPE_SLICE);
    (void)id;

    // Evaluate the elements of the array-slice as if they were part of an
    // array rvalue. After evaluating the pseudo-array the contents of the
    // pseudo-array are copied from the stack into the underlying array backing
    // the array-slice.
    struct expr* const array_expr = expr_new_array(
        expr->location,
        symbol_xget_type(expr->data.array_slice.array_symbol),
        expr->data.array_slice.elements,
        NULL);
    codegen_rvalue_array(array_expr, id);
    sunder_xalloc(array_expr, SUNDER_XALLOC_FREE);

    push_address(symbol_xget_address(expr->data.array_slice.array_symbol));
    appendli("pop rbx ; address of the array-slice backing array");
    copy_rsp_rbx_via_rcx(
        symbol_xget_type(expr->data.array_slice.array_symbol)->size);
    pop(symbol_xget_type(expr->data.array_slice.array_symbol)->size);

    // Evaluate a slice constructed from the backing array.
    appendli("push %zu", sunder_sbuf_count(expr->data.array_slice.elements));
    push_address(symbol_xget_address(expr->data.array_slice.array_symbol));
}

static void
codegen_rvalue_struct(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_STRUCT);
    assert(expr->type->kind == TYPE_STRUCT);

    // Make space for the struct.
    push(expr->type->size);

    // Fill in stack bytes of the struct with zeros. Although this should not
    // really affect the user-visible portions of the language it will allow for
    // dump statements to be deterministic in the bytes they print if we define
    // padding bytes to always be zeroed for struct values created at runtime.
    appendln("%s%zu_zero_memory_bgn:", LABEL_EXPR, id);
    appendli("mov rax, 0"); // zero value
    appendli("mov rbx, rsp"); // current address
    size_t const words_count = ceil8zu(expr->type->size) / 8;
    for (size_t i = 0; i < words_count; ++i) {
        appendli("mov [rbx], rax");
        appendli("add rbx, 8");
    }
    appendln("%s%zu_zero_memory_end:", LABEL_EXPR, id);

    // One by one evaluate the member variables for the elements of the struct.
    // Each member variable will be at the top of the stack after being
    // evaluated, so the member variable is manually memcpy-ed into the correct
    // position on the stack.
    sunder_sbuf(struct member_variable) const member_variable_defs =
        expr->type->data.struct_.member_variables;
    sunder_sbuf(struct expr const* const) const member_variable_exprs =
        expr->data.struct_.member_variables;
    assert(
        sunder_sbuf_count(member_variable_defs)
        == sunder_sbuf_count(member_variable_exprs));
    for (size_t i = 0; i < sunder_sbuf_count(member_variable_defs); ++i) {
        assert(member_variable_exprs[i]->type == member_variable_defs[i].type);
        codegen_rvalue(member_variable_exprs[i]);

        struct type const* const type = member_variable_exprs[i]->type;
        size_t const size = type->size;
        size_t const offset = member_variable_defs[i].offset;

        appendli("mov rbx, rsp");
        appendli("add rbx, %zu", ceil8zu(size)); // struct start
        appendli("add rbx, %zu", offset); // member offset
        copy_rsp_rbx_via_rcx(size);

        pop(size);
    }
}

static void
codegen_rvalue_cast(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_CAST);
    assert(expr->type->size >= 1u);
    assert(expr->type->size <= 8u);
    (void)id;

    struct expr const* const from = expr->data.cast.expr;
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
    case TYPE_U16: {
        // Move with Zero-Extend
        appendli("movzx rax, %s", reg);
        break;
    }
    case TYPE_U32: {
        // The MOVZX instruction does not have an encoding with SRC of r/m32 or
        // r/m64, but a MOV with SRC of r/m32 will zero out the upper 32 bits.
        appendli("mov eax, %s", reg);
        break;
    }
    case TYPE_S8: /* fallthrough */
    case TYPE_S16: /* fallthrough */
    case TYPE_S32: {
        // Move with Sign-Extension
        appendli("movsx rax, %s", reg);
        break;
    }
    case TYPE_U64: /* fallthrough */
    case TYPE_S64: /* fallthrough */
    case TYPE_USIZE: /* fallthrough */
    case TYPE_SSIZE: /* fallthrough */
    case TYPE_POINTER: {
        // A MOV with r/m64 has nothing to zero-extend/sign-extend.
        break;
    }
    case TYPE_FUNCTION: {
        assert(expr->type->kind == TYPE_FUNCTION);
        // Functions are implemented as pointers to the function entry point, so
        // the function value stays the same when casting from one function to
        // another.
        break;
    }
    case TYPE_ANY: /* fallthrough */
    case TYPE_VOID: /* fallthrough */
    case TYPE_INTEGER: /* fallthrough */
    case TYPE_ARRAY: /* fallthrough */
    case TYPE_SLICE: /* fallthrough */
    case TYPE_STRUCT: {
        UNREACHABLE();
    }
    }

    // Boolean values *must* be either zero or one.
    if (expr->type->kind == TYPE_BOOL) {
        appendli("and rax, 0x1");
    }

    // MOV the casted-to data back onto the stack.
    appendli("mov [rsp], rax");
}

static void
codegen_rvalue_syscall(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SYSCALL);
    (void)id;

    struct expr const* const* const arguments = expr->data.syscall.arguments;
    size_t const count = sunder_sbuf_count(arguments);
    for (size_t i = 0; i < count; ++i) {
        assert(arguments[i]->type->size <= 8);
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
codegen_rvalue_call(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_CALL);
    (void)id;

    // Push space for return value.
    struct type const* function_type = expr->data.call.function->type;
    assert(function_type->kind == TYPE_FUNCTION);
    struct type const* return_type = function_type->data.function.return_type;
    appendli("; push space for return value of type `%s`", return_type->name);
    push(return_type->size);

    // Evaluate & push arguments from left to right.
    struct expr const* const* const arguments = expr->data.call.arguments;
    for (size_t i = 0; i < sunder_sbuf_count(arguments); ++i) {
        appendli(
            "; push argument %zu of type `%s`",
            i + 1,
            arguments[i]->type->name);
        codegen_rvalue(arguments[i]);
    }

    // Load the function pointer and call the function.
    codegen_rvalue(expr->data.call.function);
    appendli("pop rax");
    appendli("call rax");

    // Pop arguments from right to left, leaving the return value as the top
    // element on the stack (for return values with non-zero size).
    for (size_t i = sunder_sbuf_count(arguments); i--;) {
        appendli(
            "; discard (pop) argument %zu of type `%s`",
            i + 1,
            arguments[i]->type->name);
        pop(arguments[i]->type->size);
    }
}

static void
codegen_rvalue_access_index(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);

    if (expr->data.access_index.lhs->type->kind == TYPE_ARRAY) {
        codegen_rvalue_access_index_lhs_array(expr, id);
        return;
    }

    if (expr->data.access_index.lhs->type->kind == TYPE_SLICE) {
        codegen_rvalue_access_index_lhs_slice(expr, id);
        return;
    }

    UNREACHABLE();
}

static void
codegen_rvalue_access_index_lhs_array(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);
    assert(expr->data.access_index.lhs->type->kind == TYPE_ARRAY);
    assert(expr->data.access_index.idx->type->kind == TYPE_USIZE);

    struct type const* const lhs_type = expr->data.access_index.lhs->type;
    struct type const* const element_type = lhs_type->data.array.base;
    if (element_type->size == 0) {
        return;
    }

    // Push space for result.
    assert(expr->type == element_type);
    push(expr->type->size);

    if (expr_is_lvalue(expr->data.access_index.lhs)) {
        // Array expression is an lvalue. Compute the address of the of the
        // indexed element and copy from that address into the result.
        codegen_lvalue(expr->data.access_index.lhs);
        codegen_rvalue(expr->data.access_index.idx);
        // rax := source
        // rsp := destination
        // After calculating the source address the stack pointer will point to
        // the result since space for the result space was pushed onto the
        // stack.
        appendli("pop rax"); // index
        appendli("mov rbx, %zu", lhs_type->data.array.count); // count
        appendli("cmp rax, rbx");
        appendli("jb %s%zu_op", LABEL_EXPR, id);
        appendli("call __index_oob_handler");
        appendln("%s%zu_op:", LABEL_EXPR, id);
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
    codegen_rvalue(expr->data.access_index.lhs);
    codegen_rvalue(expr->data.access_index.idx);
    // rax := source
    appendli("pop rax"); // index
    appendli("mov rbx, %zu", lhs_type->data.array.count); // count
    appendli("cmp rax, rbx");
    appendli("jb %s%zu_op", LABEL_EXPR, id);
    appendli("call __index_oob_handler");
    appendln("%s%zu_op:", LABEL_EXPR, id);
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
codegen_rvalue_access_index_lhs_slice(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);
    assert(expr->data.access_index.lhs->type->kind == TYPE_SLICE);
    assert(expr->data.access_index.idx->type->kind == TYPE_USIZE);

    struct type const* const lhs_type = expr->data.access_index.lhs->type;
    struct type const* const element_type = lhs_type->data.slice.base;
    if (element_type->size == 0) {
        return;
    }

    // Push space for result.
    assert(expr->type == element_type);
    push(expr->type->size);

    codegen_rvalue(expr->data.access_index.lhs);
    codegen_rvalue(expr->data.access_index.idx);
    // rax := source
    // rsp := destination
    appendli("pop rax"); // index
    appendli("mov rbx, [rsp + 8]"); // slice count
    appendli("cmp rax, rbx");
    appendli("jb %s%zu_op", LABEL_EXPR, id);
    appendli("call __index_oob_handler");
    appendln("%s%zu_op:", LABEL_EXPR, id);
    appendli("mov rbx, %zu", element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // index * sizeof(element_type)
    appendli("pop rbx"); // start
    appendli("add rax, rbx"); // start + index * sizeof(element_type)
    pop(8u); // slice count
    // copy
    copy_rax_rsp_via_rcx(element_type->size);
}

static void
codegen_rvalue_access_slice(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_SLICE);

    if (expr->data.access_slice.lhs->type->kind == TYPE_ARRAY) {
        codegen_rvalue_access_slice_lhs_array(expr, id);
        return;
    }

    if (expr->data.access_slice.lhs->type->kind == TYPE_SLICE) {
        codegen_rvalue_access_slice_lhs_slice(expr, id);
        return;
    }

    UNREACHABLE();
}

static void
codegen_rvalue_access_slice_lhs_array(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_SLICE);
    assert(expr->data.access_slice.lhs->type->kind == TYPE_ARRAY);
    assert(expr->data.access_slice.begin->type->kind == TYPE_USIZE);
    assert(expr->data.access_slice.end->type->kind == TYPE_USIZE);
    assert(expr_is_lvalue(expr->data.access_slice.lhs));

    struct type const* const lhs_type = expr->data.access_slice.lhs->type;
    struct type const* const element_type = lhs_type->data.array.base;

    codegen_rvalue(expr->data.access_slice.end);
    codegen_rvalue(expr->data.access_slice.begin);
    appendli("pop rax"); // begin
    appendli("pop rbx"); // end
    appendli("mov rcx, %zu", lhs_type->data.array.count); // count

    appendli("cmp rax, rbx"); // cmp begin end
    appendli("jbe %s%zu_oob_check_bgnidx", LABEL_EXPR, id); // jmp begin <= end
    appendli("call __index_oob_handler");

    appendli("%s%zu_oob_check_bgnidx:", LABEL_EXPR, id);
    appendli("cmp rax, rcx"); // cmp begin count
    appendli(
        "jbe %s%zu_oob_check_endidx", LABEL_EXPR, id); // jmp begin <= count
    appendli("call __index_oob_handler");

    appendli("%s%zu_oob_check_endidx:", LABEL_EXPR, id);
    appendli("cmp rbx, rcx"); // cmp end count
    appendli("jbe %s%zu_op", LABEL_EXPR, id); // jmp end <= count
    appendli("call __index_oob_handler");

    appendli("%s%zu_op:", LABEL_EXPR, id);
    appendli("sub rbx, rax"); // count = end - begin
    appendli("push rbx"); // push count

    appendli("mov rbx, %zu", element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // offset = begin * sizeof(element_type)
    appendli("push rax"); // push offset

    // NOTE: The call to codegen_lvalue will push __nil for slices with elements
    // of size zero, so we don't need a special case for that here.
    codegen_lvalue(expr->data.access_slice.lhs);
    appendli("pop rax"); // start
    appendli("pop rbx"); // offset
    appendli("add rax, rbx"); // pointer = start + offset
    appendli("push rax"); // push pointer
}

static void
codegen_rvalue_access_slice_lhs_slice(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_SLICE);
    assert(expr->data.access_slice.lhs->type->kind == TYPE_SLICE);
    assert(expr->data.access_slice.begin->type->kind == TYPE_USIZE);
    assert(expr->data.access_slice.end->type->kind == TYPE_USIZE);

    struct type const* const lhs_type = expr->data.access_slice.lhs->type;
    struct type const* const element_type = lhs_type->data.slice.base;

    codegen_rvalue(expr->data.access_slice.lhs);
    codegen_rvalue(expr->data.access_slice.end);
    codegen_rvalue(expr->data.access_slice.begin);
    appendli("pop rax"); // begin
    appendli("pop rbx"); // end
    appendli("pop rsi"); // start (lhs slice's pointer)
    appendli("pop rcx"); // count

    appendli("cmp rax, rbx"); // cmp begin end
    appendli("jbe %s%zu_oob_check_bgnidx", LABEL_EXPR, id); // jmp begin <= end
    appendli("call __index_oob_handler");

    appendli("%s%zu_oob_check_bgnidx:", LABEL_EXPR, id);
    appendli("cmp rax, rcx"); // cmp begin count
    appendli(
        "jbe %s%zu_oob_check_endidx", LABEL_EXPR, id); // jmp begin <= count
    appendli("call __index_oob_handler");

    appendli("%s%zu_oob_check_endidx:", LABEL_EXPR, id);
    appendli("cmp rbx, rcx"); // cmp end count
    appendli("jbe %s%zu_op", LABEL_EXPR, id); // jmp end <= count
    appendli("call __index_oob_handler");

    appendli("%s%zu_op:", LABEL_EXPR, id);
    appendli("sub rbx, rax"); // count = end - begin
    appendli("push rbx"); // push count

    appendli("mov rbx, %zu", element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // offset = begin * sizeof(element_type)
    appendli("add rax, rsi"); // pointer = offset + start
    appendli("push rax"); // push pointer
}

static void
codegen_rvalue_access_member_variable(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_MEMBER_VARIABLE);
    assert(expr->data.access_member_variable.lhs->type->kind == TYPE_STRUCT);
    (void)id;

    // Create space on the top of the stack to hold the value of the accessed
    // member variable. Then copy the member variable data from within the
    // struct onto the top of the stack.
    //
    // Stack (start)
    //  | ... other values on the stack
    //  +-- <- initial rsp
    //  |
    //  v
    //
    // Stack (after evaluating the rvalue struct)
    //  | ... other values on the stack
    //  +-- <- initial rsp
    //  | undefined byte(s)
    //  +-- <- eventual rsp (after copying member data)
    //  | some member data  <-+
    //  +--                   |
    //  | target member data  |
    //  +--                   +- struct rvalue
    //  |... etc.             |
    //  +--                   |
    //  | some member data  <-+
    //  +-- <- current rsp
    //  |
    //  v
    //
    // Stack (after copying member data)
    //  | ... other values on the stack
    //  +-- <- initial rsp
    //  | target member data
    //  +-- <- current rsp
    //  |
    //  v
    push(expr->type->size);
    codegen_rvalue(expr->data.access_member_variable.lhs);
    appendli("mov rax, rsp ; rax := start of the object");
    appendli(
        "add rax, %zu ; rax := start of the member variable",
        expr->data.access_member_variable.member_variable->offset);
    appendli(
        "add rsp, %zu ; rsp := location of the member variable result",
        ceil8zu(expr->data.access_member_variable.lhs->type->size));
    copy_rax_rsp_via_rcx(expr->type->size);
}

static void
codegen_rvalue_sizeof(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SIZEOF);
    (void)id;

    assert(expr->type->kind == TYPE_USIZE);
    appendli("mov rax, %zu", expr->data.sizeof_.rhs->size);
    appendli("push rax");
}

static void
codegen_rvalue_alignof(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ALIGNOF);
    (void)id;

    assert(expr->type->kind == TYPE_USIZE);
    appendli("mov rax, %zu", expr->data.alignof_.rhs->align);
    appendli("push rax");
}

static void
codegen_rvalue_unary(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);

    switch (expr->data.unary.op) {
    case UOP_NOT: {
        assert(expr->data.unary.rhs->type->size <= 8u);

        codegen_rvalue(expr->data.unary.rhs);

        char const* rhs_reg = reg_a(expr->data.unary.rhs->type->size);
        appendli("pop rax");
        appendli("mov rbx, 0");
        appendli(
            "cmp %s, %s", rhs_reg, reg_b(expr->data.unary.rhs->type->size));
        appendli("setz al");
        appendli("push rax");
        return;
    }
    case UOP_POS: {
        codegen_rvalue(expr->data.unary.rhs);
        return;
    }
    case UOP_NEG: {
        struct expr const* const rhs = expr->data.unary.rhs;

        assert(rhs->type->size <= 8u);
        codegen_rvalue(expr->data.unary.rhs);

        char const* rhs_reg = reg_a(expr->data.unary.rhs->type->size);
        appendli("pop rax");
        if (type_is_signed_integer(rhs->type)) {
            char* const min_cstr =
                sunder_bigint_to_new_cstr(rhs->type->data.integer.min, NULL);
            appendli("mov rbx, %s", min_cstr);
            sunder_xalloc(min_cstr, SUNDER_XALLOC_FREE);
            appendli(
                "cmp %s, %s", rhs_reg, reg_b(expr->data.unary.rhs->type->size));
            appendli("jne %s%zu_op", LABEL_EXPR, id);
            appendli("call __integer_oor_handler");
        }
        appendln("%s%zu_op:", LABEL_EXPR, id);
        appendli("neg rax");
        appendli("push rax");
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
        appendli("pop rax ; pointer object being dereferenced");
        size_t const size = expr->type->size;
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
            if (expr_is_lvalue(expr->data.unary.rhs)) {
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
codegen_rvalue_binary(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);

    switch (expr->data.binary.op) {
    case BOP_OR: {
        assert(expr->data.binary.lhs->type->kind == TYPE_BOOL);
        assert(expr->data.binary.rhs->type->kind == TYPE_BOOL);
        assert(expr->data.binary.lhs->type->size == 1u);
        assert(expr->data.binary.rhs->type->size == 1u);

        char const* lhs_reg = reg_a(expr->data.binary.lhs->type->size);
        char const* rhs_reg = reg_b(expr->data.binary.rhs->type->size);
        appendln("%s%zu_lhs:", LABEL_EXPR, id);
        codegen_rvalue(expr->data.binary.lhs);
        appendli("pop rax");
        appendli("mov rbx, 0x00");
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli("jne %s%zu_true", LABEL_EXPR, id);
        appendli("jmp %s%zu_rhs", LABEL_EXPR, id);

        appendln("%s%zu_rhs:", LABEL_EXPR, id);
        codegen_rvalue(expr->data.binary.rhs);
        appendli("pop rax");
        appendli("mov rbx, 0x00");
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli("jne %s%zu_true", LABEL_EXPR, id);
        appendli("jmp %s%zu_false", LABEL_EXPR, id);

        appendln("%s%zu_true:", LABEL_EXPR, id);
        appendli("push 0x01");
        appendli("jmp %s%zu_end", LABEL_EXPR, id);

        appendln("%s%zu_false:", LABEL_EXPR, id);
        appendli("push 0x00");
        appendli("jmp %s%zu_end", LABEL_EXPR, id);
        return;
    }
    case BOP_AND: {
        assert(expr->data.binary.lhs->type->kind == TYPE_BOOL);
        assert(expr->data.binary.rhs->type->kind == TYPE_BOOL);
        assert(expr->data.binary.lhs->type->size == 1u);
        assert(expr->data.binary.rhs->type->size == 1u);

        char const* lhs_reg = reg_a(expr->data.binary.lhs->type->size);
        char const* rhs_reg = reg_b(expr->data.binary.rhs->type->size);
        appendln("%s%zu_lhs:", LABEL_EXPR, id);
        codegen_rvalue(expr->data.binary.lhs);
        appendli("pop rax");
        appendli("mov rbx, 0x00");
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli("jne %s%zu_rhs", LABEL_EXPR, id);
        appendli("jmp %s%zu_false", LABEL_EXPR, id);

        appendln("%s%zu_rhs:", LABEL_EXPR, id);
        codegen_rvalue(expr->data.binary.rhs);
        appendli("pop rax");
        appendli("mov rbx, 0x00");
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli("jne %s%zu_true", LABEL_EXPR, id);
        appendli("jmp %s%zu_false", LABEL_EXPR, id);

        appendln("%s%zu_true:", LABEL_EXPR, id);
        appendli("push 0x01");
        appendli("jmp %s%zu_end", LABEL_EXPR, id);

        appendln("%s%zu_false:", LABEL_EXPR, id);
        appendli("push 0x00");
        appendli("jmp %s%zu_end", LABEL_EXPR, id);
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

        char const* lhs_reg = reg_a(expr->data.binary.lhs->type->size);
        char const* rhs_reg = reg_b(expr->data.binary.rhs->type->size);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
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

        char const* lhs_reg = reg_a(expr->data.binary.lhs->type->size);
        char const* rhs_reg = reg_b(expr->data.binary.rhs->type->size);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
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

        char const* lhs_reg = reg_a(expr->data.binary.lhs->type->size);
        char const* rhs_reg = reg_b(expr->data.binary.rhs->type->size);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli(
            "%s rcx, rdx",
            type_is_signed_integer(xhs_type) ? "cmovle" : "cmovbe");
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

        char const* lhs_reg = reg_a(expr->data.binary.lhs->type->size);
        char const* rhs_reg = reg_b(expr->data.binary.rhs->type->size);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli(
            "%s rcx, rdx",
            type_is_signed_integer(xhs_type) ? "cmovl" : "cmovb");
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

        char const* lhs_reg = reg_a(expr->data.binary.lhs->type->size);
        char const* rhs_reg = reg_b(expr->data.binary.rhs->type->size);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli(
            "%s rcx, rdx",
            type_is_signed_integer(xhs_type) ? "cmovge" : "cmovae");
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

        char const* lhs_reg = reg_a(expr->data.binary.lhs->type->size);
        char const* rhs_reg = reg_b(expr->data.binary.rhs->type->size);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli(
            "%s rcx, rdx",
            type_is_signed_integer(xhs_type) ? "cmovg" : "cmova");
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

        char const* const lhs_reg = reg_a(xhs_type->size);
        char const* const rhs_reg = reg_b(xhs_type->size);
        char const* const jmp_not_overflow =
            type_is_signed_integer(xhs_type) ? "jno" : "jnc";

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);
        appendli("pop rbx ; add rhs");
        appendli("pop rax ; add lhs");
        appendli("add %s, %s", lhs_reg, rhs_reg);
        appendli("push rax");
        appendli("%s %s%zu_end", jmp_not_overflow, LABEL_EXPR, id);
        appendli("call __integer_oor_handler");
        return;
    }
    case BOP_SUB: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;

        char const* const lhs_reg = reg_a(xhs_type->size);
        char const* const rhs_reg = reg_b(xhs_type->size);
        char const* const jmp_not_overflow =
            type_is_signed_integer(xhs_type) ? "jno" : "jnc";

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);
        appendli("pop rbx ; sub rhs");
        appendli("pop rax ; sub lhs");
        appendli("sub %s, %s", lhs_reg, rhs_reg);
        appendli("push rax");
        appendli("%s %s%zu_end", jmp_not_overflow, LABEL_EXPR, id);
        appendli("call __integer_oor_handler");
        return;
    }
    case BOP_MUL: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;

        char const* const rhs_reg = reg_b(xhs_type->size);
        char const* const mul =
            type_is_signed_integer(xhs_type) ? "imul" : "mul";

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);
        appendli("pop rbx ; mul rhs");
        appendli("pop rax ; mul lhs");
        appendli("%s %s", mul, rhs_reg);
        appendli("push rax");
        appendli("jno %s%zu_end", LABEL_EXPR, id);
        appendli("call __integer_oor_handler");
        return;
    }
    case BOP_DIV: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;

        char const* const rhs_reg = reg_b(xhs_type->size);
        char const* const div =
            type_is_signed_integer(xhs_type) ? "idiv" : "div";

        codegen_rvalue(expr->data.binary.lhs);
        codegen_rvalue(expr->data.binary.rhs);
        appendli("pop rbx ; div rhs");
        appendli("pop rax ; div lhs");
        if (type_is_signed_integer(xhs_type)) {
            // Sign extend to fill the upper portion of the dividend.
            // https://www.felixcloutier.com/x86/cbw:cwde:cdqe
            // https://www.felixcloutier.com/x86/cwd:cdq:cqo
            switch (xhs_type->size) {
            case 1:
                appendli("cbw ; sign extend AL into AX");
                break;
            case 2:
                appendli("cwd ; sign extend AX into DX:AX");
                break;
            case 4:
                appendli("cdq ; sign extend EAX into EDX:EAX");
                break;
            case 8:
                appendli("cqo ; sign extend RAX into RDX:RAX");
                break;
            default:
                UNREACHABLE();
            }
        }
        else {
            assert(type_is_unsigned_integer(xhs_type));
            // Clear the upper portion of the dividend.
            appendli("xor rdx, rdx");
        }
        appendli("mov rcx, 0");
        appendli(
            "cmp %s, %s",
            rhs_reg,
            reg_c(xhs_type->size)); // divide-by-zero check
        appendli("jne %s%zu_op", LABEL_EXPR, id);
        appendli("call __integer_divz_handler");
        appendli("%s%zu_op:", LABEL_EXPR, id);
        appendli("%s %s", div, rhs_reg);
        appendli("push rax");
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
codegen_lvalue(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr_is_lvalue(expr));

    static struct {
        char const* kind_cstr;
        void (*codegen_fn)(struct expr const*, size_t);
    } const table[] = {
#define TABLE_ENTRY(kind, fn) [kind] = {#kind, fn}
        // clang format off
        TABLE_ENTRY(EXPR_SYMBOL, codegen_lvalue_symbol),
        TABLE_ENTRY(EXPR_ACCESS_INDEX, codegen_lvalue_access_index),
        TABLE_ENTRY(
            EXPR_ACCESS_MEMBER_VARIABLE, codegen_lvalue_access_member_variable),
        TABLE_ENTRY(EXPR_UNARY, codegen_lvalue_unary),
    // clang format on
#undef TABLE_ENTRY
    };

    size_t const id = unique_id++;
    char const* const cstr = table[expr->kind].kind_cstr;
    appendln("%s%zu_bgn:", LABEL_EXPR, id);
    appendli_location(expr->location, "%s (ID %zu, LVALUE)", cstr, id);
    assert(table[expr->kind].codegen_fn != NULL);
    table[expr->kind].codegen_fn(expr, id);
    appendln("%s%zu_end:", LABEL_EXPR, id);
}

static void
codegen_lvalue_symbol(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SYMBOL);
    (void)id;

    if (expr->type->size == 0) {
        // Zero-sized objects should take up zero space. Attempting to take the
        // address of a zero-sized symbol should always produce a pointer with
        // the value zero.
        appendli(
            "push __nil ; address of type `%s` with size zero",
            expr->type->name);
        return;
    }
    push_address(symbol_xget_address(expr->data.symbol));
}

static void
codegen_lvalue_access_index(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);

    if (expr->data.access_index.lhs->type->kind == TYPE_ARRAY) {
        codegen_lvalue_access_index_lhs_array(expr, id);
        return;
    }

    if (expr->data.access_index.lhs->type->kind == TYPE_SLICE) {
        codegen_lvalue_access_index_lhs_slice(expr, id);
        return;
    }

    UNREACHABLE();
}

static void
codegen_lvalue_access_index_lhs_array(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);
    assert(expr->data.access_index.lhs->type->kind == TYPE_ARRAY);
    assert(expr->data.access_index.idx->type->kind == TYPE_USIZE);

    struct type const* const lhs_type = expr->data.access_index.lhs->type;
    struct type const* const element_type = lhs_type->data.array.base;
    if (element_type->size == 0) {
        appendli("push __nil");
        return;
    }

    codegen_lvalue(expr->data.access_index.lhs);
    codegen_rvalue(expr->data.access_index.idx);
    appendli("pop rax"); // index
    appendli("mov rbx, %zu", lhs_type->data.array.count); // count
    appendli("cmp rax, rbx");
    appendli("jb %s%zu_op", LABEL_EXPR, id);
    appendli("call __index_oob_handler");
    appendln("%s%zu_op:", LABEL_EXPR, id);
    appendli("mov rbx, %zu", element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // index * sizeof(element_type)
    appendli("pop rbx"); // start
    appendli("add rax, rbx"); // start + index * sizeof(element_type)
    appendli("push rax");
}

static void
codegen_lvalue_access_index_lhs_slice(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);
    assert(expr->data.access_index.lhs->type->kind == TYPE_SLICE);
    assert(expr->data.access_index.idx->type->kind == TYPE_USIZE);

    struct type const* const lhs_type = expr->data.access_index.lhs->type;
    struct type const* const element_type = lhs_type->data.slice.base;
    if (element_type->size == 0) {
        appendli("push __nil");
        return;
    }

    codegen_rvalue(expr->data.access_index.lhs);
    codegen_rvalue(expr->data.access_index.idx);
    appendli("pop rax"); // index
    appendli("mov rbx, [rsp + 8]"); // slice count
    appendli("cmp rax, rbx");
    appendli("jb %s%zu_op", LABEL_EXPR, id);
    appendli("call __index_oob_handler");
    appendln("%s%zu_op:", LABEL_EXPR, id);
    appendli("mov rbx, %zu", element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // index * sizeof(element_type)
    appendli("pop rbx"); // start
    appendli("add rax, rbx"); // start + index * sizeof(element_type)
    pop(8u); // slice count
    appendli("push rax");
}

static void
codegen_lvalue_access_member_variable(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_MEMBER_VARIABLE);
    assert(expr->data.access_member_variable.lhs->type->kind == TYPE_STRUCT);
    (void)id;

    codegen_lvalue(expr->data.access_member_variable.lhs);
    appendli("pop rax");
    appendli(
        "add rax, %zu",
        expr->data.access_member_variable.member_variable->offset);
    appendli("push rax");
}

static void
codegen_lvalue_unary(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr_is_lvalue(expr));
    (void)id;

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
        UNREACHABLE();
    }
    }

    UNREACHABLE();
}

void
codegen(char const* const opt_o, bool opt_k)
{
    assert(opt_o != NULL);

    out = sunder_string_new(NULL, 0u);
    struct sunder_string* const asm_path = sunder_string_new_fmt("%s.asm", opt_o);
    struct sunder_string* const obj_path = sunder_string_new_fmt("%s.o", opt_o);

    codegen_sys();
    appendch('\n');
    codegen_static_constants();
    appendch('\n');
    codegen_static_variables();
    appendch('\n');
    codegen_static_functions();

    int err = 0;
    if ((err = sunder_file_write(
             sunder_string_start(asm_path),
             sunder_string_start(out),
             sunder_string_count(out)))) {
        error(
            NULL,
            "unable to write file `%s` with error '%s'",
            sunder_string_start(asm_path),
            strerror(errno));
        goto cleanup;
    }

    // clang-format off
    char const* const nasm_argv[] = {
        "nasm", "-w+error=all", "-f", "elf64", "-O0", "-g", "-F", "dwarf",
        sunder_string_start(asm_path), (char const*)NULL
    };
    // clang-format on
    if ((err = spawnvpw(nasm_argv))) {
        goto cleanup;
    }

    // clang-format off
    char const* const ld_argv[] = {
        "ld", "-o",  opt_o, sunder_string_start(obj_path), (char const*)NULL
    };
    // clang-format on
    if ((err = spawnvpw(ld_argv))) {
        goto cleanup;
    }

cleanup:
    if (!opt_k) {
        (void)remove(sunder_string_start(asm_path));
        (void)remove(sunder_string_start(obj_path));
    }
    sunder_string_del(asm_path);
    sunder_string_del(obj_path);
    sunder_string_del(out);
    if (err) {
        exit(EXIT_FAILURE);
    }
}
