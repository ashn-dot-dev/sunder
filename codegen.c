// Copyright 2021-2022 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "sunder.h"

char const* backend = STRINGIFY(SUNDER_DEFAULT_BACKEND);
static struct string* out = NULL;
static struct function const* current_function = NULL;
static size_t current_loop_id; // Used for generating break & continue labels.
static size_t unique_id = 0; // Used for generating unique names and labels.

// Local labels take the form:
//      .__<AST-node-type>_<unique-id>_<description>
//
// The <AST-node-type> is STMT for stmt nodes and EXPR for expr nodes.
//
// The <unique-id> is generated for each node in the call to codegen_stmt,
// push_rvalue, and push_lvalue.
//
// The <description> is used to denote what section of AST node generation is
// taking place as well as provide the necessary labels for jumps.
//
// For some stmt and expr nodes, the local labels:
//      .__<AST-node-type>_<unique_id>_bgn:
// and
//      .__<AST-node-type>_<unique_id>_end:
// are generated to denote the beginning and end of code generation for that
// node for use as a jump target.
#define LABEL_STMT ".__STMT_"
#define LABEL_EXPR ".__EXPR_"

#if defined(__GNUC__) /* GCC and Clang */
#    define APPENDF __attribute__((format(printf, 1, 2)))
#    define APPENDF_LOCATION __attribute__((format(printf, 2, 3)))
#else
#    define APPENDF /* nothing */
#    define APPENDF_LOCATION /* nothing */
#endif

static APPENDF void
append(char const* fmt, ...);
static APPENDF void
appendln(char const* fmt, ...);
static APPENDF void
appendli(char const* fmt, ...);
static APPENDF_LOCATION void
appendli_location(struct source_location const* location, char const* fmt, ...);
static void
appendch(char ch);
// Declare NASM Dx data elements forming a static initializer.
// https://nasm.us/doc/nasmdoc3.html#section-3.2.1
static void
append_dx_static_initializer(struct value const* value);

// All push_* functions align rsp to an 8-byte boundary.
static void
push(uint64_t size);
static void
push_address(struct address const* address);
static void
push_at_address(uint64_t size, struct address const* address);
// The pop function will round size up to an 8-byte boundary to match the
// push_* functions so that one push/pop pair will restore the stack to it's
// previous state.
static void
pop(uint64_t size);

// Register al, ax, eax, or rax based on size.
static char const*
reg_a(uint64_t size);
// Register bl, bx, ebx, or rbx based on size.
static char const*
reg_b(uint64_t size);
// Register cl, cx, ecx, or rcx based on size.
static char const*
reg_c(uint64_t size);

// Copy size bytes from the address in rax to to the address in rbx using rcx
// for intermediate storage. Roughly equivalent to memcpy(rbx, rax, size).
static void
copy_rax_rbx_via_rcx(uint64_t size);
// Copy size bytes from the address in rsp to to the address in rbx using rcx
// for intermediate storage. Roughly equivalent to memcpy(rbx, rsp, size).
static void
copy_rsp_rbx_via_rcx(uint64_t size);
// Copy size bytes from the address in rax to to the address in rsp using rcx
// for intermediate storage. Roughly equivalent to memcpy(rsp, rax, size).
static void
copy_rax_rsp_via_rcx(uint64_t size);

// Move the value currently in register a into the full width of rax with zero
// extend (unsigned values) or sign extend (signed values).
static void
mov_rax_reg_a_with_zero_or_sign_extend(struct type const* type);

static void
append(char const* fmt, ...)
{
    assert(out != NULL);
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);
    string_append_vfmt(out, fmt, args);
    va_end(args);
}

static void
appendln(char const* fmt, ...)
{
    assert(out != NULL);
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);
    string_append_vfmt(out, fmt, args);
    va_end(args);

    string_append_cstr(out, "\n");
}

static void
appendli(char const* fmt, ...)
{
    assert(out != NULL);
    assert(fmt != NULL);

    string_append_cstr(out, "    ");

    va_list args;
    va_start(args, fmt);
    string_append_vfmt(out, fmt, args);
    va_end(args);

    string_append_cstr(out, "\n");
}

static void
appendli_location(struct source_location const* location, char const* fmt, ...)
{
    assert(out != NULL);
    assert(location != NULL);
    assert(location->path != NO_PATH);
    assert(location->line != NO_LINE);
    assert(location->psrc != NO_PSRC);

    string_append_fmt(out, "    ; [%s:%zu] ", location->path, location->line);

    va_list args;
    va_start(args, fmt);
    string_append_vfmt(out, fmt, args);
    va_end(args);

    string_append_cstr(out, "\n");

    char const* const line_start = source_line_start(location->psrc);
    char const* const line_end = source_line_end(location->psrc);

    string_append_fmt(
        out, "    ;%.*s\n", (int)(line_end - line_start), line_start);
    string_append_fmt(
        out, "    ;%*s^\n", (int)(location->psrc - line_start), "");
}

static void
appendch(char ch)
{
    assert(out != NULL);

    string_append(out, &ch, 1u);
}

static void
append_dx_static_initializer(struct value const* value)
{
    assert(out != NULL);
    assert(value != NULL);
    assert(value->type->size != 0);
    assert(value->type->size != SIZEOF_UNSIZED);

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
        sbuf(uint8_t) const bytes = value_to_new_bytes(value);
        for (size_t i = 0; i < sbuf_count(bytes); ++i) {
            assert(value->type->size != 0);
            if (value->type->size == 1) {
                appendli(
                    "db %#x ; `%s`", (unsigned)bytes[i], value->type->name);
                continue;
            }

            // Explicitly note which byte of the value is being written.
            appendli(
                "db %#x ; `%s` byte %zu",
                (unsigned)bytes[i],
                value->type->name,
                i);
        }
        sbuf_fini(bytes);
        return;
    }
    case TYPE_INTEGER: {
        UNREACHABLE();
    }
    case TYPE_FUNCTION: {
        struct address const* const address = value->data.function->address;
        assert(address->kind == ADDRESS_STATIC);
        assert(address->data.static_.offset == 0);
        appendli("dq %s", address->data.static_.name);
        return;
    }
    case TYPE_POINTER: {
        struct address const* const address = &value->data.pointer;
        if (value->type->data.pointer.base->size == 0) {
            appendli("dq 0");
            return;
        }

        assert(address->kind == ADDRESS_STATIC);
        if (address->data.static_.offset == 0) {
            appendli("dq $%s", address->data.static_.name);
        }
        else {
            appendli(
                "dq ($%s + %" PRIu64 ")",
                address->data.static_.name,
                address->data.static_.offset);
        }
        return;
    }
    case TYPE_ARRAY: {
        sbuf(struct value*) const elements = value->data.array.elements;
        struct value const* const ellipsis = value->data.array.ellipsis;
        uint64_t const count = value->type->data.array.count;
        for (size_t i = 0; i < count; ++i) {
            if (i < sbuf_count(elements)) {
                append_dx_static_initializer(elements[i]);
            }
            else {
                assert(ellipsis != NULL);
                append_dx_static_initializer(ellipsis);
            }
        }
        return;
    }
    case TYPE_SLICE: {
        append_dx_static_initializer(value->data.slice.pointer);
        append_dx_static_initializer(value->data.slice.count);
        return;
    }
    case TYPE_STRUCT: {
        sbuf(struct member_variable) const member_variable_defs =
            value->type->data.struct_.member_variables;
        size_t const member_variable_defs_count =
            sbuf_count(member_variable_defs);

        sbuf(struct value*) const member_variable_vals =
            value->data.struct_.member_variables;
        size_t const member_variable_vals_count =
            sbuf_count(member_variable_vals);

        assert(member_variable_defs_count == member_variable_vals_count);
        size_t const member_variable_count = member_variable_vals_count;
        (void)member_variable_defs_count;

        for (size_t i = 0; i < member_variable_count; ++i) {
            struct member_variable const* const def = member_variable_defs + i;

            if (i != 0) {
                struct member_variable const* const prev_def =
                    member_variable_defs + (i - 1);
                uint64_t const padding =
                    def->offset - (prev_def->offset + prev_def->type->size);
                if (padding != 0) {
                    appendli("; padding");
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

            appendli(
                "; `%s` member variable `%s` of type `%s`",
                def->type->name,
                def->name,
                def->type->name);
            if (member_variable_vals[i] != NULL) {
                append_dx_static_initializer(member_variable_vals[i]);
            }
            else {
                for (size_t i = 0; i < def->type->size; ++i) {
                    appendli(
                        "db %#x ; (uninitialized) `%s` byte %zu",
                        0,
                        def->type->name,
                        i);
                }
            }
        }
        return;
    }
    }
}

static void
push(uint64_t size)
{
    if (size == 0) {
        appendli("; (push of size zero)");
        return;
    }

    appendli("sub rsp, %#" PRIx64, ceil8u64(size));
}

static void
push_address(struct address const* address)
{
    assert(address != NULL);

    switch (address->kind) {
    case ADDRESS_STATIC: {
        if (address->data.static_.offset == 0) {
            // More aesthetically pleasing without the `+ offset` component.
            appendli("lea rax, [$%s]", address->data.static_.name);
            appendli("push rax");
            break;
        }
        appendli(
            "lea [$%s + %" PRIu64 "]",
            address->data.static_.name,
            address->data.static_.offset);
        appendli("push rax");
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
push_at_address(uint64_t size, struct address const* address)
{
    assert(address != NULL);

    push(size);

    // Usable for a memory indirection `[addr]`.
    // ADDRESS_STATIC mode: number + offset
    // ADDRESS_LOCAL mode : reg + base*scale + offset
    char* addr = NULL;
    switch (address->kind) {
    case ADDRESS_STATIC:
        addr = cstr_new_fmt(
            "($%s + %zu)",
            address->data.static_.name,
            address->data.static_.offset);
        break;
    case ADDRESS_LOCAL:
        addr = cstr_new_fmt("rbp + %d", address->data.local.rbp_offset);
        break;
    default:
        UNREACHABLE();
    }

    uint64_t cur = 0u;
    while ((size - cur) >= 8u) {
        appendli("mov rax, [%s + %#" PRIx64 "]", addr, cur);
        appendli("mov [rsp + %#" PRIx64 "], rax", cur);
        cur += 8u;
    }
    if ((size - cur) >= 4u) {
        appendli("mov eax, [%s + %#" PRIx64 "]", addr, cur);
        appendli("mov [rsp + %#" PRIx64 "], eax", cur);
        cur += 4u;
        assert((size - cur) < 4u);
    }
    if ((size - cur) >= 2u) {
        appendli("mov ax, [%s + %#" PRIx64 "]", addr, cur);
        appendli("mov [rsp + %#" PRIx64 "], ax", cur);
        cur += 2u;
        assert((size - cur) < 2u);
    }
    if ((size - cur) == 1u) {
        appendli("mov al, [%s + %#" PRIx64 "]", addr, cur);
        appendli("mov [rsp + %#" PRIx64 "], al", cur);
    }

    xalloc(addr, XALLOC_FREE);
}

static void
pop(uint64_t size)
{
    if (size == 0) {
        appendli("; (pop of size zero)");
        return;
    }

    appendli("add rsp, %#" PRIx64, ceil8u64(size));
}

static char const*
reg_a(uint64_t size)
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
reg_b(uint64_t size)
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
reg_c(uint64_t size)
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
copy_rax_rbx_via_rcx(uint64_t size)
{
    uint64_t cur = 0u;
    while ((size - cur) >= 8u) {
        appendli("mov rcx, [rax + %#" PRIx64 "]", cur);
        appendli("mov [rbx + %#" PRIx64 "], rcx", cur);
        cur += 8u;
    }
    if ((size - cur) >= 4u) {
        appendli("mov ecx, [rax + %#" PRIx64 "]", cur);
        appendli("mov [rbx + %#" PRIx64 "], ecx", cur);
        cur += 4u;
        assert((size - cur) < 4u);
    }
    if ((size - cur) >= 2u) {
        appendli("mov cx, [rax + %#" PRIx64 "]", cur);
        appendli("mov [rbx + %#" PRIx64 "], cx", cur);
        cur += 2u;
        assert((size - cur) < 2u);
    }
    if ((size - cur) == 1u) {
        appendli("mov cl, [rax + %#" PRIx64 "]", cur);
        appendli("mov [rbx + %#" PRIx64 "], cl", cur);
    }
}

static void
copy_rsp_rbx_via_rcx(uint64_t size)
{
    uint64_t cur = 0u;
    while ((size - cur) >= 8u) {
        appendli("mov rcx, [rsp + %#" PRIx64 "]", cur);
        appendli("mov [rbx + %#" PRIx64 "], rcx", cur);
        cur += 8u;
    }
    if ((size - cur) >= 4u) {
        appendli("mov ecx, [rsp + %#" PRIx64 "]", cur);
        appendli("mov [rbx + %#" PRIx64 "], ecx", cur);
        cur += 4u;
        assert((size - cur) < 4u);
    }
    if ((size - cur) >= 2u) {
        appendli("mov cx, [rsp + %#" PRIx64 "]", cur);
        appendli("mov [rbx + %#" PRIx64 "], cx", cur);
        cur += 2u;
        assert((size - cur) < 2u);
    }
    if ((size - cur) == 1u) {
        appendli("mov cl, [rsp + %#" PRIx64 "]", cur);
        appendli("mov [rbx + %#" PRIx64 "], cl", cur);
    }
}

static void
copy_rax_rsp_via_rcx(uint64_t size)
{
    uint64_t cur = 0u;
    while ((size - cur) >= 8u) {
        appendli("mov rcx, [rax + %#" PRIx64 "]", cur);
        appendli("mov [rsp + %#" PRIx64 "], rcx", cur);
        cur += 8u;
    }
    if ((size - cur) >= 4u) {
        appendli("mov ecx, [rax + %#" PRIx64 "]", cur);
        appendli("mov [rsp + %#" PRIx64 "], ecx", cur);
        cur += 4u;
        assert((size - cur) < 4u);
    }
    if ((size - cur) >= 2u) {
        appendli("mov cx, [rax + %#" PRIx64 "]", cur);
        appendli("mov [rsp + %#" PRIx64 "], cx", cur);
        cur += 2u;
        assert((size - cur) < 2u);
    }
    if ((size - cur) == 1u) {
        appendli("mov cl, [rax + %#" PRIx64 "]", cur);
        appendli("mov [rsp + %#" PRIx64 "], cl", cur);
    }
}

static void
mov_rax_reg_a_with_zero_or_sign_extend(struct type const* type)
{
    char const* const reg = reg_a(type->size);

    switch (type->kind) {
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
        //
        // MOVSX r64, r/m8
        // MOVSX r64, r/m16
        // MOVSXD r64, r/m32
        char const* const mov = type->size == 4 ? "movsxd" : "movsx";
        appendli("%s rax, %s", mov, reg);
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
        // Functions are implemented as pointers to the function entry point,
        // so the function value stays the same when casting from one function
        // to another.
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
}

// Populates an xalloc-allocated buffer.
static void
load_sysasm(void** buf, size_t* buf_size);

static void
codegen_extern_labels(void const* sysasm_buf, size_t sysasm_buf_size);
static void
codegen_global_labels(void);
static void
codegen_static_constants(void);
static void
codegen_static_variables(void);
static void
codegen_static_functions(void);
static void
codegen_fatals(void);

static void
codegen_static_object(struct symbol const* symbol);
static void
codegen_static_function(struct symbol const* symbol);

static void
codegen_stmt(struct stmt const* stmt);
static void
codegen_stmt_defer(struct stmt const* stmt, size_t id);
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
codegen_block(struct block const* block);

static void
codegen_defers(struct stmt const* begin, struct stmt const* end);

static void
push_rvalue(struct expr const* expr);
static void
push_rvalue_symbol(struct expr const* expr, size_t id);
static void
push_rvalue_value(struct expr const* expr, size_t id);
static void
push_rvalue_bytes(struct expr const* expr, size_t id);
static void
push_rvalue_array_list(struct expr const* expr, size_t id);
static void
push_rvalue_slice_list(struct expr const* expr, size_t id);
static void
push_rvalue_slice(struct expr const* expr, size_t id);
static void
push_rvalue_struct(struct expr const* expr, size_t id);
static void
push_rvalue_cast(struct expr const* expr, size_t id);
static void
push_rvalue_call(struct expr const* expr, size_t id);
static void
push_rvalue_access_index(struct expr const* expr, size_t id);
static void
push_rvalue_access_index_lhs_array(struct expr const* expr, size_t id);
static void
push_rvalue_access_index_lhs_slice(struct expr const* expr, size_t id);
static void
push_rvalue_access_slice(struct expr const* expr, size_t id);
static void
push_rvalue_access_slice_lhs_array(struct expr const* expr, size_t id);
static void
push_rvalue_access_slice_lhs_slice(struct expr const* expr, size_t id);
static void
push_rvalue_access_member_variable(struct expr const* expr, size_t id);
static void
push_rvalue_sizeof(struct expr const* expr, size_t id);
static void
push_rvalue_alignof(struct expr const* expr, size_t id);
static void
push_rvalue_unary(struct expr const* expr, size_t id);
static void
push_rvalue_binary(struct expr const* expr, size_t id);

static void
push_lvalue(struct expr const* expr);
static void
push_lvalue_symbol(struct expr const* expr, size_t id);
static void
push_lvalue_access_index(struct expr const* expr, size_t id);
static void
push_lvalue_access_index_lhs_array(struct expr const* expr, size_t id);
static void
push_lvalue_access_index_lhs_slice(struct expr const* expr, size_t id);
static void
push_lvalue_access_member_variable(struct expr const* expr, size_t id);
static void
push_lvalue_unary(struct expr const* expr, size_t id);

static void
load_sysasm(void** buf, size_t* buf_size)
{
    struct string* path = NULL;

    char const* const SUNDER_SYSASM_PATH = getenv("SUNDER_SYSASM_PATH");
    if (SUNDER_SYSASM_PATH != NULL) {
        // User is explicitly overriding the default `sys.asm` file.
        path = string_new_fmt("%s", SUNDER_SYSASM_PATH);
    }
    else {
        // Use the default `sys.asm` file, `${SUNDER_HOME}/lib/sys/sys.asm`.
        char const* const SUNDER_HOME = getenv("SUNDER_HOME");
        if (SUNDER_HOME == NULL) {
            fatal(NULL, "missing environment variable SUNDER_HOME");
        }
        path = string_new_fmt("%s/lib/sys/sys.asm", SUNDER_HOME);
    }

    if (file_read_all(string_start(path), buf, buf_size)) {
        fatal(
            NULL,
            "failed to read '%s' with error '%s'",
            string_start(path),
            strerror(errno));
    }

    string_del(path);
}

static void
codegen_extern_labels(void const* sysasm_buf, size_t sysasm_buf_size)
{
    appendln("; FORWARD DECLARATIONS OF EXTERN OBJECT/FUNCTION LABELS");
    for (size_t i = 0; i < sbuf_count(context()->static_symbols); ++i) {
        struct symbol const* const symbol = context()->static_symbols[i];

        bool const is_extern_variable =
            symbol->kind == SYMBOL_VARIABLE && symbol->data.variable->is_extern;
        bool const is_extern_function = symbol->kind == SYMBOL_FUNCTION
            && symbol_xget_value(NULL, symbol)->data.function->body == NULL;
        if (!(is_extern_variable || is_extern_function)) {
            continue;
        }

        // From the NASM 2.14.02 manual, Section 7.5:
        // > If a variable is declared both GLOBAL and EXTERN, or if it is
        // > declared as EXTERN and then defined, it will be treated as GLOBAL.
        // > If a variable is declared both as COMMON and EXTERN, it will be
        // > treated as COMMON.
        //
        // If the chosen backend is nasm version>=2.14 then every extern
        // variable and function can be forward declared as EXTERN, and nasm
        // would adjust the symbol type if the symbol was later defined within
        // sys.asm. However, yasm does *not* support this symbol conversion. In
        // order to accommodate both assemblers, the sys.asm source is scanned
        // to see if there are any labels that match the extern address name,
        // in which case that symbol will be skipped.
        struct address const* const address = symbol_xget_address(symbol);
        assert(address->kind == ADDRESS_STATIC);
        assert(symbol_xget_address(symbol)->data.static_.offset == 0);
        char const* const label = address->data.static_.name;
        size_t const label_size = strlen(label);

        char const* cur = sysasm_buf;
        char const* const end = cur + sysasm_buf_size - label_size;
        size_t const find_count = label_size + STR_LITERAL_COUNT(":");
        bool found = false;
        for (; cur < end; ++cur) {
            if (!(cur == sysasm_buf || cur[-1] == '\n')) {
                // Not at the start of a line.
                continue;
            }

            // From the NASM 2.14.02 manual, Section 3.1:
            // > An identifier may also be prefixed with a $ to indicate that
            // > it is intended to be read as an identifier and not a reserved
            // > word; thus, if some other module you are linking with defines
            // > a symbol called eax, you can refer to $eax in NASM code to
            // > distinguish the symbol from the register.
            if (cur[0] == '$') {
                // The label could potentially take the form:
                //      $<identifier>:
                // Skip the leading '$'.
                cur += 1;
            }

            if ((size_t)(end - cur) < find_count) {
                // Not enough room to fit the label.
                break;
            }

            bool const label_matches = 0 == memcmp(cur, label, label_size);
            if (label_matches && cur[label_size] == ':') {
                // Start of the line contains the text `<label>:`.
                found = true;
                break;
            }
        }

        if (found) {
            appendln("; Extern label `%s` defined within sys asm...", label);
            continue;
        }
        appendln("extern $%s", label);
    }
}

static void
codegen_global_labels(void)
{
    appendln("; FORWARD DECLARATIONS OF GLOBAL FUNCTION LABELS");
    for (size_t i = 0; i < sbuf_count(context()->static_symbols); ++i) {
        struct symbol const* const symbol = context()->static_symbols[i];
        bool const is_global_function = symbol->kind == SYMBOL_FUNCTION
            && symbol_xget_value(NULL, symbol)->data.function->body != NULL;
        if (is_global_function) {
            struct address const* const address = symbol_xget_address(symbol);
            assert(address->kind == ADDRESS_STATIC);
            assert(address->data.static_.offset == 0);
            appendln("global $%s", address->data.static_.name);
        }
    }
}

static void
codegen_static_constants(void)
{
    appendln("; STATIC CONSTANTS");
    appendln("section .rodata");

    for (size_t i = 0; i < sbuf_count(context()->static_symbols); ++i) {
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

    for (size_t i = 0; i < sbuf_count(context()->static_symbols); ++i) {
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

    for (size_t i = 0; i < sbuf_count(context()->static_symbols); ++i) {
        struct symbol const* const symbol = context()->static_symbols[i];
        assert(symbol_xget_address(symbol)->kind == ADDRESS_STATIC);
        if (symbol->kind != SYMBOL_FUNCTION) {
            continue;
        }
        codegen_static_function(symbol);
    }
}

static void
codegen_fatals(void)
{
    // The instructions:
    //      lea rax, [addr]
    //      push rax
    // are used instead of:
    //      push addr
    // to allow for addresses in the full 64-bit address space.

    // clang-format off
    // Builtin integer divide by zero handler.
    appendln("section .text");
    appendln("__fatal_integer_divide_by_zero:");
    appendln("    lea rax, [__fatal_integer_divide_by_zero_msg_start]");
    appendln("    push rax");
    appendln("    push __fatal_integer_divide_by_zero_msg_count");
    appendln("    call __fatal");
    appendch('\n');
    appendln("section .rodata");
    appendln("__fatal_integer_divide_by_zero_msg_start: db \"divide by zero\", 0x0A");
    appendln("__fatal_integer_divide_by_zero_msg_count: equ $ - __fatal_integer_divide_by_zero_msg_start");
    appendch('\n');

    // Builtin integer out-of-range handler.
    appendln("section .text");
    appendln("__fatal_integer_out_of_range:");
    appendln("    lea rax, [__fatal_integer_out_of_range_msg_start]");
    appendln("    push rax");
    appendln("    push __fatal_integer_out_of_range_msg_count");
    appendln("    call __fatal");
    appendch('\n');
    appendln("section .rodata");
    appendln("__fatal_integer_out_of_range_msg_start: db \"arithmetic operation produces out-of-range result\", 0x0A");
    appendln("__fatal_integer_out_of_range_msg_count: equ $ - __fatal_integer_out_of_range_msg_start");
    appendch('\n');

    // Builtin index out-of-bounds handler.
    appendln("section .text");
    appendln("__fatal_index_out_of_bounds:");
    appendln("    lea rax, [__fatal_index_out_of_bounds_msg_start]");
    appendln("    push rax");
    appendln("    push __fatal_index_out_of_bounds_msg_count");
    appendln("    call __fatal");
    appendch('\n');
    appendln("section .rodata");
    appendln("__fatal_index_out_of_bounds_msg_start: db \"index out-of-bounds\", 0x0A");
    appendln("__fatal_index_out_of_bounds_msg_count: equ $ - __fatal_index_out_of_bounds_msg_start");
    // clang-format on
}

static void
codegen_static_object(struct symbol const* symbol)
{
    assert(symbol != NULL);
    assert(symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_CONSTANT);
    assert(symbol_xget_address(symbol)->kind == ADDRESS_STATIC);

    bool const is_extern_variable =
        symbol->kind == SYMBOL_VARIABLE && symbol->data.variable->is_extern;
    if (is_extern_variable) {
        // Forward label was already emitted.
        return;
    }

    struct type const* const type = symbol_xget_type(symbol);
    if (type->size == 0) {
        // Zero-sized objects should take up zero space. Attempting to take the
        // address of a zero-sized symbol should always produce a pointer with
        // the value zero.
        appendln(
            "$%s: equ 0 ; nil", symbol_xget_address(symbol)->data.static_.name);
        return;
    }

    assert(symbol_xget_address(symbol)->data.static_.offset == 0);
    append("$%s:\n", symbol_xget_address(symbol)->data.static_.name);
    if (symbol->data.variable->value != NULL) {
        // Variable is initialized.
        append_dx_static_initializer(symbol_xget_value(NULL, symbol));
    }
    else {
        // Variable is uninitialized. The initial state of the object should be
        // zerod, similar to uninitialized globals in C.
        for (size_t i = 0; i < symbol->data.variable->type->size; ++i) {
            appendli(
                "db %#x ; (uninitialized) `%s` byte %zu",
                0,
                symbol->data.variable->type->name,
                i);
        }
    }
    appendch('\n');
    return;
}

static void
codegen_static_function(struct symbol const* symbol)
{
    assert(symbol != NULL);
    assert(symbol->kind == SYMBOL_FUNCTION);

    assert(symbol_xget_value(NULL, symbol)->type->kind == TYPE_FUNCTION);
    struct function const* const function =
        symbol_xget_value(NULL, symbol)->data.function;

    bool const is_extern_function = function->body == NULL;
    if (is_extern_function) {
        // Forward label was already emitted.
        return;
    }

    struct address const* const address = symbol_xget_address(symbol);
    assert(address->kind == ADDRESS_STATIC);
    assert(address->data.static_.offset == 0);
    appendln("$%s:", address->data.static_.name);
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
    codegen_block(function->body);
    current_function = NULL;

    if (function->type->data.function.return_type == context()->builtin.void_) {
        appendli("; EPILOGUE (implicit-return)");
        appendli("mov rsp, rbp");
        appendli("pop rbp");
        appendli("ret");
    }
    appendli("; END-OF-FUNCTION");
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
        TABLE_ENTRY(STMT_DEFER, codegen_stmt_defer),
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
    appendli("; STMT %zu BEGIN", id);
    appendli_location(stmt->location, "%s (ID %zu)", cstr, id);
    table[stmt->kind].codegen_fn(stmt, id);
    appendli("; STMT %zu END", id);
}

static void
codegen_stmt_defer(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_DEFER);
    (void)stmt;
    (void)id;

    // No code generation is performed for defer statements as defers are
    // generated as equivalent lowered statements by other codegen functions.
    return;
}

static void
codegen_stmt_if(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_IF);

    sbuf(struct conditional const* const) const conditionals =
        stmt->data.if_.conditionals;
    for (size_t i = 0; i < sbuf_count(conditionals); ++i) {
        bool const is_last = i == (sbuf_count(conditionals) - 1);

        if (conditionals[i]->condition != NULL) {
            appendln("%s%zu_condition_%zu:", LABEL_STMT, id, i);
            assert(conditionals[i]->condition->type->kind == TYPE_BOOL);
            push_rvalue(conditionals[i]->condition);
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
        codegen_block(conditionals[i]->body);
        appendli("jmp %s%zu_end", LABEL_STMT, id);
    }

    appendli("%s%zu_end:", LABEL_STMT, id);
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
    push_rvalue(stmt->data.for_range.begin);
    appendli("pop rbx"); // begin
    appendli("pop rax"); // addr of loop variable
    appendli("mov [rax], rbx");
    appendln("%s%zu_condition:", LABEL_STMT, id);
    push_at_address(
        symbol_xget_type(stmt->data.for_range.loop_variable)->size,
        symbol_xget_address(stmt->data.for_range.loop_variable));
    push_rvalue(stmt->data.for_range.end);
    appendli("pop rbx"); // end
    appendli("pop rax"); // loop variable
    appendli("cmp rax, rbx");
    appendli("jnb %s%zu_end", LABEL_STMT, id);
    appendln("%s%zu_body_bgn:", LABEL_STMT, id);
    codegen_block(stmt->data.for_range.body);
    appendln("%s%zu_body_end:", LABEL_STMT, id);
    appendli(
        "inc qword [rbp + %d]",
        symbol_xget_address(stmt->data.for_range.loop_variable)
            ->data.local.rbp_offset);
    appendli("jmp %s%zu_condition", LABEL_STMT, id);

    appendli("%s%zu_end: ; used for break and continue", LABEL_STMT, id);
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
    push_rvalue(stmt->data.for_expr.expr);
    appendli("pop rax");
    appendli("mov rbx, 0x00");
    appendli("cmp al, bl");
    appendli("je %s%zu_end", LABEL_STMT, id);
    appendln("%s%zu_body_bgn:", LABEL_STMT, id);
    codegen_block(stmt->data.for_expr.body);
    appendln("%s%zu_body_end:", LABEL_STMT, id);
    appendli("jmp %s%zu_condition", LABEL_STMT, id);

    appendli("%s%zu_end: ; used for break and continue", LABEL_STMT, id);
    current_loop_id = save_current_loop_id;
}

static void
codegen_stmt_break(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_BREAK);
    (void)stmt;
    (void)id;

    codegen_defers(stmt->data.break_.defer_begin, stmt->data.break_.defer_end);
    appendli("jmp %s%zu_end", LABEL_STMT, current_loop_id);
}

static void
codegen_stmt_continue(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_CONTINUE);
    (void)stmt;
    (void)id;

    codegen_defers(stmt->data.break_.defer_begin, stmt->data.break_.defer_end);
    appendli("jmp %s%zu_body_end", LABEL_STMT, current_loop_id);
}

static void
codegen_stmt_dump(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_DUMP);
    (void)id;

    appendli(
        "; dump `%s` (%" PRIu64 " bytes)",
        stmt->data.expr->type->name,
        stmt->data.expr->type->size);
    push_rvalue(stmt->data.expr);
    appendli(
        "push %#" PRIx64 " ; push type `%s` size",
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
        push_rvalue(stmt->data.return_.expr);

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

    codegen_defers(stmt->data.return_.defer, NULL);

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

    push_rvalue(stmt->data.assign.rhs);
    push_lvalue(stmt->data.assign.lhs);

    appendli("pop rbx");
    uint64_t const size = stmt->data.assign.rhs->type->size;
    uint64_t cur = 0u;
    while ((size - cur) >= 8u) {
        appendli("mov rax, [rsp + %#" PRIx64 "]", cur);
        appendli("mov [rbx + %#" PRIx64 "], rax", cur);
        cur += 8u;
    }
    if ((size - cur) >= 4u) {
        appendli("mov eax, [rsp + %#" PRIx64 "]", cur);
        appendli("mov [rbx + %#" PRIx64 "], eax", cur);
        cur += 4u;
        assert((size - cur) < 4u);
    }
    if ((size - cur) >= 2u) {
        appendli("mov ax, [rsp + %#" PRIx64 "]", cur);
        appendli("mov [rbx + %#" PRIx64 "], ax", cur);
        cur += 2u;
        assert((size - cur) < 2u);
    }
    if ((size - cur) == 1u) {
        appendli("mov al, [rsp + %#" PRIx64 "]", cur);
        appendli("mov [rbx + %#" PRIx64 "], al", cur);
    }
    pop(size);
}

static void
codegen_stmt_expr(struct stmt const* stmt, size_t id)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_EXPR);
    (void)id;

    push_rvalue(stmt->data.expr);
    // Remove the (unused) result from the stack.
    pop(stmt->data.expr->type->size);
}

static void
codegen_block(struct block const* block)
{
    assert(block != NULL);

    for (size_t i = 0; i < sbuf_count(block->stmts); ++i) {
        struct stmt const* const stmt = block->stmts[i];
        codegen_stmt(stmt);
    }

    codegen_defers(block->defer_begin, block->defer_end);
}

static void
codegen_defers(struct stmt const* begin, struct stmt const* end)
{
    assert(begin == NULL || begin->kind == STMT_DEFER);
    assert(end == NULL || end->kind == STMT_DEFER);

    struct stmt const* current = begin;
    while (current != end) {
        codegen_block(current->data.defer.body);
        current = current->data.defer.prev;
    }
}

static void
push_rvalue(struct expr const* expr)
{
    assert(expr != NULL);

    // clang-format off
    static struct {
        char const* kind_cstr;
        void (*codegen_fn)(struct expr const*, size_t);
    } const table[] = {
#define TABLE_ENTRY(kind, fn) [kind] = {#kind, fn}
        TABLE_ENTRY(EXPR_SYMBOL, push_rvalue_symbol),
        TABLE_ENTRY(EXPR_VALUE, push_rvalue_value),
        TABLE_ENTRY(EXPR_BYTES, push_rvalue_bytes),
        TABLE_ENTRY(EXPR_ARRAY_LIST, push_rvalue_array_list),
        TABLE_ENTRY(EXPR_SLICE_LIST, push_rvalue_slice_list),
        TABLE_ENTRY(EXPR_SLICE, push_rvalue_slice),
        TABLE_ENTRY(EXPR_STRUCT, push_rvalue_struct),
        TABLE_ENTRY(EXPR_CAST, push_rvalue_cast),
        TABLE_ENTRY(EXPR_CALL, push_rvalue_call),
        TABLE_ENTRY(EXPR_ACCESS_INDEX, push_rvalue_access_index),
        TABLE_ENTRY(EXPR_ACCESS_SLICE, push_rvalue_access_slice),
        TABLE_ENTRY(EXPR_ACCESS_MEMBER_VARIABLE, push_rvalue_access_member_variable),
        TABLE_ENTRY(EXPR_SIZEOF, push_rvalue_sizeof),
        TABLE_ENTRY(EXPR_ALIGNOF, push_rvalue_alignof),
        TABLE_ENTRY(EXPR_UNARY, push_rvalue_unary),
        TABLE_ENTRY(EXPR_BINARY, push_rvalue_binary),
#undef TABLE_ENTRY
    };
    // clang-format on

    size_t const id = unique_id++;
    char const* const cstr = table[expr->kind].kind_cstr;
    appendli("; EXPR RVALUE %zu BEGIN", id);
    appendli_location(expr->location, "%s (ID %zu, RVALUE)", cstr, id);
    assert(table[expr->kind].codegen_fn != NULL);
    table[expr->kind].codegen_fn(expr, id);
    appendli("; EXPR RVALUE %zu END", id);
}

static void
push_rvalue_symbol(struct expr const* expr, size_t id)
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
push_rvalue_value(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_VALUE);
    assert(expr->type == expr->data.value->type);
    (void)id;

    struct value const* const value = expr->data.value;

    switch (value->type->kind) {
    case TYPE_BOOL: {
        appendli("mov rax, %s", value->data.boolean ? "0x01" : "0x00");
        appendli("push rax");
        return;
    }
    case TYPE_BYTE: {
        appendli("mov rax, %#x", value->data.byte);
        appendli("push rax");
        return;
    }
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
        assert(value->type->size >= 1u);
        assert(value->type->size <= 8u);
        char* const cstr = bigint_to_new_cstr(value->data.integer);
        appendli("mov rax, %s", cstr);
        appendli("push rax");
        xalloc(cstr, XALLOC_FREE);
        return;
    }
    case TYPE_VOID: /* fallthrough */
    case TYPE_FUNCTION: /* fallthrough */
    case TYPE_POINTER: /* fallthrough */
    case TYPE_ARRAY: /* fallthrough */
    case TYPE_SLICE: /* fallthrough */
    case TYPE_STRUCT: {
        // AST nodes of kind EXPR_VALUE are not produced for these types.
        UNREACHABLE();
    }
    case TYPE_ANY: /* fallthrough */
    case TYPE_INTEGER: {
        // Unsized types should never reach the code generation phase.
        UNREACHABLE();
    }
    }

    UNREACHABLE();
}

static void
push_rvalue_bytes(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BYTES);
    assert(expr->type->kind == TYPE_SLICE);
    (void)id;

    appendli("push %zu", expr->data.bytes.count); // count
    push_address(expr->data.bytes.address); // pointer
}

static void
push_rvalue_array_list(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ARRAY_LIST);
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
    sbuf(struct expr const* const) const elements =
        expr->data.array_list.elements;
    struct type const* const element_type = expr->type->data.array.base;
    uint64_t const element_size = element_type->size;
    // TODO: This loop is manually unrolled here, but should probably be turned
    // into an actual asm loop for elements with trivial initialization.
    // Basically we should account for the scenario where a brainfuck
    // interpreter allocates an array 30000 zeroed bytes, which would cause the
    // equivalent of memcpy(&my_array[index], &my_zero, 0x8) inlined thousands
    // of times.
    for (size_t i = 0; i < sbuf_count(elements); ++i) {
        assert(elements[i]->type == element_type);
        push_rvalue(elements[i]);

        appendli("mov rbx, rsp");
        appendli("add rbx, %" PRIu64, ceil8u64(element_size)); // array start
        appendli("add rbx, %" PRIu64, element_size * i); // array offset
        copy_rsp_rbx_via_rcx(element_size);

        pop(element_size);
    }

    uint64_t const count = expr->type->data.array.count;
    if (sbuf_count(elements) < count) { // ellipsis
        assert(expr->data.array_list.ellipsis != NULL);
        assert(expr->data.array_list.ellipsis->type == element_type);

        // Number of elements already filled in.
        uint64_t const completed = sbuf_count(elements);
        // Number of elements remaining to be filled in with the ellipsis.
        uint64_t const remaining = count - sbuf_count(elements);

        appendln("%s%zu_ellipsis_bgn:", LABEL_EXPR, id);
        push_rvalue(expr->data.array_list.ellipsis);
        appendli("mov rbx, rsp");
        appendli("add rbx, %" PRIu64, ceil8u64(element_size)); // array start
        appendli("add rbx, %" PRIu64, element_size * completed); // array offset
        // rbx is now the destination register.
        appendli("mov rax, %" PRIu64, remaining); // rax := counter down to zero
        appendln("%s%zu_ellipsis_condition:", LABEL_EXPR, id);
        appendli("cmp rax, 0");
        appendli("je %s%zu_ellipsis_pop", LABEL_EXPR, id);
        copy_rsp_rbx_via_rcx(element_size);
        appendli("add rbx, %" PRIu64, element_size); // ptr = ptr + 1
        appendli("dec rax");
        appendli("jmp %s%zu_ellipsis_condition", LABEL_EXPR, id);
        appendln("%s%zu_ellipsis_pop:", LABEL_EXPR, id);
        pop(element_size);
    }
}

static void
push_rvalue_slice_list(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SLICE_LIST);
    assert(expr->type->kind == TYPE_SLICE);
    (void)id;

    // Evaluate the elements of the array-slice as if they were part of an
    // array rvalue. After evaluating the pseudo-array the contents of the
    // pseudo-array are copied from the stack into the underlying array backing
    // the array-slice.
    struct expr* const array_expr = expr_new_array_list(
        expr->location,
        symbol_xget_type(expr->data.slice_list.array_symbol),
        expr->data.slice_list.elements,
        NULL);
    push_rvalue_array_list(array_expr, id);
    xalloc(array_expr, XALLOC_FREE);

    push_address(symbol_xget_address(expr->data.slice_list.array_symbol));
    appendli("pop rbx ; address of the array-slice backing array");
    copy_rsp_rbx_via_rcx(
        symbol_xget_type(expr->data.slice_list.array_symbol)->size);
    pop(symbol_xget_type(expr->data.slice_list.array_symbol)->size);

    // Evaluate a slice constructed from the backing array.
    appendli("push %zu", sbuf_count(expr->data.slice_list.elements));
    push_address(symbol_xget_address(expr->data.slice_list.array_symbol));
}

static void
push_rvalue_slice(struct expr const* expr, size_t id)
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
    push_rvalue(expr->data.slice.count);
    push_rvalue(expr->data.slice.pointer);
}

static void
push_rvalue_struct(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_STRUCT);
    assert(expr->type->kind == TYPE_STRUCT);
    (void)id;

    // Make space for the struct.
    push(expr->type->size);

    // Fill in stack bytes of the struct with zeros. Although this should not
    // really affect the user-visible portions of the language it will allow
    // for dump statements to be deterministic in the bytes they print if we
    // define padding bytes to always be zeroed for struct values created at
    // runtime.
    appendli("mov rax, 0"); // zero value
    appendli("mov rbx, rsp"); // current address
    uint64_t const words_count = ceil8u64(expr->type->size) / 8;
    for (uint64_t i = 0; i < words_count; ++i) {
        appendli("mov [rbx], rax");
        appendli("add rbx, 8");
    }

    // One by one evaluate the member variables for the elements of the struct.
    // Each member variable will be at the top of the stack after being
    // evaluated, so the member variable is manually memcpy-ed into the correct
    // position on the stack.
    sbuf(struct member_variable) const member_variable_defs =
        expr->type->data.struct_.member_variables;
    sbuf(struct expr const* const) const member_variable_exprs =
        expr->data.struct_.member_variables;
    assert(
        sbuf_count(member_variable_defs) == sbuf_count(member_variable_exprs));
    for (size_t i = 0; i < sbuf_count(member_variable_defs); ++i) {
        if (member_variable_exprs[i] == NULL) {
            // Uninitialized member variable.
            continue;
        }

        assert(member_variable_exprs[i]->type == member_variable_defs[i].type);
        push_rvalue(member_variable_exprs[i]);

        struct type const* const type = member_variable_exprs[i]->type;
        uint64_t const size = type->size;
        uint64_t const offset = member_variable_defs[i].offset;

        appendli("mov rbx, rsp");
        appendli("add rbx, %" PRIu64, ceil8u64(size)); // struct start
        appendli("add rbx, %" PRIu64, offset); // member offset
        copy_rsp_rbx_via_rcx(size);

        pop(size);
    }
}

static void
push_rvalue_cast(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_CAST);
    assert(expr->type->size >= 1u);
    assert(expr->type->size <= 8u);
    (void)id;

    struct expr const* const from = expr->data.cast.expr;
    assert(from->type->size >= 1u);
    assert(from->type->size <= 8u);
    push_rvalue(from);

    // Load casted-from data into an A register (al, ax, eax, or rax).
    char const* const reg = reg_a(from->type->size);
    appendli("mov %s, [rsp]", reg);

    // Perform the operation to zero-extend/sign-extend the casted-from data.
    mov_rax_reg_a_with_zero_or_sign_extend(from->type);

    // Boolean values *must* be either zero or one.
    if (expr->type->kind == TYPE_BOOL) {
        appendli("and rax, 0x1");
    }

    // MOV the casted-to data back onto the stack.
    appendli("mov [rsp], rax");
}

static void
push_rvalue_call(struct expr const* expr, size_t id)
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
    for (size_t i = 0; i < sbuf_count(arguments); ++i) {
        appendli(
            "; push argument %zu of type `%s`",
            i + 1,
            arguments[i]->type->name);
        push_rvalue(arguments[i]);
    }

    // Load the function pointer and call the function.
    push_rvalue(expr->data.call.function);
    appendli("pop rax");
    appendli("call rax");

    // Pop arguments from right to left, leaving the return value as the top
    // element on the stack (for return values with non-zero size).
    for (size_t i = sbuf_count(arguments); i--;) {
        appendli(
            "; discard (pop) argument %zu of type `%s`",
            i + 1,
            arguments[i]->type->name);
        pop(arguments[i]->type->size);
    }
}

static void
push_rvalue_access_index(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);

    if (expr->data.access_index.lhs->type->kind == TYPE_ARRAY) {
        push_rvalue_access_index_lhs_array(expr, id);
        return;
    }

    if (expr->data.access_index.lhs->type->kind == TYPE_SLICE) {
        push_rvalue_access_index_lhs_slice(expr, id);
        return;
    }

    UNREACHABLE();
}

static void
push_rvalue_access_index_lhs_array(struct expr const* expr, size_t id)
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
        push_lvalue(expr->data.access_index.lhs);
        push_rvalue(expr->data.access_index.idx);
        // rax := source
        // rsp := destination
        // After calculating the source address the stack pointer will point to
        // the result since space for the result space was pushed onto the
        // stack.
        appendli("pop rax"); // index
        appendli("mov rbx, %" PRIu64, lhs_type->data.array.count); // count
        appendli("cmp rax, rbx");
        appendli("jb %s%zu_op", LABEL_EXPR, id);
        appendli("call __fatal_index_out_of_bounds");
        appendln("%s%zu_op:", LABEL_EXPR, id);
        appendli(
            "mov rbx, %" PRIu64, element_type->size); // sizeof(element_type)
        appendli("mul rbx"); // index * sizeof(element_type)
        appendli("pop rbx"); // start
        appendli("add rax, rbx"); // start + index * sizeof(element_type)
        // copy
        copy_rax_rsp_via_rcx(element_type->size);

        return;
    }

    // Array expression is an rvalue. Generate the rvalue array and rvalue
    // index. Then copy indexed element into the result.
    push_rvalue(expr->data.access_index.lhs);
    push_rvalue(expr->data.access_index.idx);
    // rax := source
    appendli("pop rax"); // index
    appendli("mov rbx, %" PRIu64, lhs_type->data.array.count); // count
    appendli("cmp rax, rbx");
    appendli("jb %s%zu_op", LABEL_EXPR, id);
    appendli("call __fatal_index_out_of_bounds");
    appendln("%s%zu_op:", LABEL_EXPR, id);
    appendli("mov rbx, %" PRIu64, element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // index * sizeof(element_type)
    appendli("add rax, rsp"); // start + index * sizeof(element_type)
    // rbx := destination
    // NOTE: The push and pop operations that manage the stack-allocated array
    // align to an 8-byte boundry, but the array itself may or may not have a
    // size cleanly divisible by 8 (as in the case of the type [3u]u16). The
    // ceil8 of the sizeof the left hand side array is used to account for any
    // extra padding at the end of the array required to bring the total push
    // size to a modulo 8 value.
    appendli(
        "mov rbx, %" PRIu64, ceil8u64(lhs_type->size)); // aligned sizeof(array)
    appendli("add rbx, rsp"); // start + aligned sizeof(array)
    // copy
    copy_rax_rbx_via_rcx(element_type->size);

    // Pop array rvalue.
    pop(lhs_type->size);
}

static void
push_rvalue_access_index_lhs_slice(struct expr const* expr, size_t id)
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

    push_rvalue(expr->data.access_index.lhs);
    push_rvalue(expr->data.access_index.idx);
    // rax := source
    // rsp := destination
    appendli("pop rax"); // index
    appendli("mov rbx, [rsp + 8]"); // slice count
    appendli("cmp rax, rbx");
    appendli("jb %s%zu_op", LABEL_EXPR, id);
    appendli("call __fatal_index_out_of_bounds");
    appendln("%s%zu_op:", LABEL_EXPR, id);
    appendli("mov rbx, %" PRIu64, element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // index * sizeof(element_type)
    appendli("pop rbx"); // start
    appendli("add rax, rbx"); // start + index * sizeof(element_type)
    pop(8u); // slice count
    // copy
    copy_rax_rsp_via_rcx(element_type->size);
}

static void
push_rvalue_access_slice(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_SLICE);

    if (expr->data.access_slice.lhs->type->kind == TYPE_ARRAY) {
        push_rvalue_access_slice_lhs_array(expr, id);
        return;
    }

    if (expr->data.access_slice.lhs->type->kind == TYPE_SLICE) {
        push_rvalue_access_slice_lhs_slice(expr, id);
        return;
    }

    UNREACHABLE();
}

static void
push_rvalue_access_slice_lhs_array(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_SLICE);
    assert(expr->data.access_slice.lhs->type->kind == TYPE_ARRAY);
    assert(expr->data.access_slice.begin->type->kind == TYPE_USIZE);
    assert(expr->data.access_slice.end->type->kind == TYPE_USIZE);
    assert(expr_is_lvalue(expr->data.access_slice.lhs));

    struct type const* const lhs_type = expr->data.access_slice.lhs->type;
    struct type const* const element_type = lhs_type->data.array.base;

    push_rvalue(expr->data.access_slice.end);
    push_rvalue(expr->data.access_slice.begin);
    appendli("pop rax"); // begin
    appendli("pop rbx"); // end
    appendli("mov rcx, %" PRIu64, lhs_type->data.array.count); // count

    appendli("cmp rax, rbx"); // cmp begin end
    appendli("jbe %s%zu_oob_check_bgnidx", LABEL_EXPR, id); // jmp begin <= end
    appendli("call __fatal_index_out_of_bounds");

    appendli("%s%zu_oob_check_bgnidx:", LABEL_EXPR, id);
    appendli("cmp rax, rcx"); // cmp begin count
    appendli(
        "jbe %s%zu_oob_check_endidx", LABEL_EXPR, id); // jmp begin <= count
    appendli("call __fatal_index_out_of_bounds");

    appendli("%s%zu_oob_check_endidx:", LABEL_EXPR, id);
    appendli("cmp rbx, rcx"); // cmp end count
    appendli("jbe %s%zu_op", LABEL_EXPR, id); // jmp end <= count
    appendli("call __fatal_index_out_of_bounds");

    appendli("%s%zu_op:", LABEL_EXPR, id);
    appendli("sub rbx, rax"); // count = end - begin
    appendli("push rbx"); // push count

    appendli("mov rbx, %" PRIu64, element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // offset = begin * sizeof(element_type)
    appendli("push rax"); // push offset

    // NOTE: The call to push_lvalue will push nil for slices with elements
    // of size zero, so we don't need a special case for that here.
    push_lvalue(expr->data.access_slice.lhs);
    appendli("pop rax"); // start
    appendli("pop rbx"); // offset
    appendli("add rax, rbx"); // pointer = start + offset
    appendli("push rax"); // push pointer
}

static void
push_rvalue_access_slice_lhs_slice(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_SLICE);
    assert(expr->data.access_slice.lhs->type->kind == TYPE_SLICE);
    assert(expr->data.access_slice.begin->type->kind == TYPE_USIZE);
    assert(expr->data.access_slice.end->type->kind == TYPE_USIZE);

    struct type const* const lhs_type = expr->data.access_slice.lhs->type;
    struct type const* const element_type = lhs_type->data.slice.base;

    push_rvalue(expr->data.access_slice.lhs);
    push_rvalue(expr->data.access_slice.end);
    push_rvalue(expr->data.access_slice.begin);
    appendli("pop rax"); // begin
    appendli("pop rbx"); // end
    appendli("pop rsi"); // start (lhs slice's pointer)
    appendli("pop rcx"); // count

    appendli("cmp rax, rbx"); // cmp begin end
    appendli("jbe %s%zu_oob_check_bgnidx", LABEL_EXPR, id); // jmp begin <= end
    appendli("call __fatal_index_out_of_bounds");

    appendli("%s%zu_oob_check_bgnidx:", LABEL_EXPR, id);
    appendli("cmp rax, rcx"); // cmp begin count
    appendli(
        "jbe %s%zu_oob_check_endidx", LABEL_EXPR, id); // jmp begin <= count
    appendli("call __fatal_index_out_of_bounds");

    appendli("%s%zu_oob_check_endidx:", LABEL_EXPR, id);
    appendli("cmp rbx, rcx"); // cmp end count
    appendli("jbe %s%zu_op", LABEL_EXPR, id); // jmp end <= count
    appendli("call __fatal_index_out_of_bounds");

    appendli("%s%zu_op:", LABEL_EXPR, id);
    appendli("sub rbx, rax"); // count = end - begin
    appendli("push rbx"); // push count

    appendli("mov rbx, %" PRIu64, element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // offset = begin * sizeof(element_type)
    appendli("add rax, rsi"); // pointer = offset + start
    appendli("push rax"); // push pointer
}

static void
push_rvalue_access_member_variable(struct expr const* expr, size_t id)
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
    push_rvalue(expr->data.access_member_variable.lhs);
    appendli("mov rax, rsp ; rax := start of the object");
    appendli(
        "add rax, %" PRIu64 " ; rax := start of the member variable",
        expr->data.access_member_variable.member_variable->offset);
    appendli(
        "add rsp, %" PRIu64 " ; rsp := location of the member variable result",
        ceil8u64(expr->data.access_member_variable.lhs->type->size));
    copy_rax_rsp_via_rcx(expr->type->size);
}

static void
push_rvalue_sizeof(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SIZEOF);
    (void)id;

    assert(expr->type->kind == TYPE_USIZE);
    appendli("mov rax, %" PRIu64, expr->data.sizeof_.rhs->size);
    appendli("push rax");
}

static void
push_rvalue_alignof(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ALIGNOF);
    (void)id;

    assert(expr->type->kind == TYPE_USIZE);
    appendli("mov rax, %" PRIu64, expr->data.alignof_.rhs->align);
    appendli("push rax");
}

static void
push_rvalue_unary(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);

    switch (expr->data.unary.op) {
    case UOP_NOT: {
        assert(expr->data.unary.rhs->type->size <= 8u);

        push_rvalue(expr->data.unary.rhs);

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
        push_rvalue(expr->data.unary.rhs);
        return;
    }
    case UOP_NEG: {
        struct expr const* const rhs = expr->data.unary.rhs;

        assert(rhs->type->size <= 8u);
        push_rvalue(expr->data.unary.rhs);

        char const* rhs_reg = reg_a(expr->data.unary.rhs->type->size);
        appendli("pop rax");
        if (type_is_sint(rhs->type)) {
            char* const min_cstr =
                bigint_to_new_cstr(rhs->type->data.integer.min);
            appendli("mov rbx, %s", min_cstr);
            xalloc(min_cstr, XALLOC_FREE);
            appendli(
                "cmp %s, %s", rhs_reg, reg_b(expr->data.unary.rhs->type->size));
            appendli("jne %s%zu_op", LABEL_EXPR, id);
            appendli("call __fatal_integer_out_of_range");
        }
        appendln("%s%zu_op:", LABEL_EXPR, id);
        appendli("neg rax");
        appendli("push rax");
        return;
    }
    case UOP_BITNOT: {
        push_rvalue(expr->data.unary.rhs);
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
        push_rvalue(expr->data.unary.rhs);
        appendli("pop rax ; pointer object being dereferenced");
        uint64_t const size = expr->type->size;
        push(size);
        copy_rax_rsp_via_rcx(size);
        return;
    }
    case UOP_ADDRESSOF: {
        push_lvalue(expr->data.unary.rhs);
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
                push_lvalue(expr->data.unary.rhs);
                appendli("pop rax ; discard array lvalue");
            }
            else {
                push_rvalue(expr->data.unary.rhs);
                pop(expr->data.unary.rhs->type->size);
            }
            appendli(
                "mov rax, %" PRIu64 "; array count",
                expr->data.unary.rhs->type->data.array.count);
            appendli("push rax");
            return;
        }

        if (expr->data.unary.rhs->type->kind == TYPE_SLICE) {
            push_rvalue(expr->data.unary.rhs);
            appendli("pop rax ; pop slice pointer word");
            return;
        }

        UNREACHABLE();
    }
    }

    UNREACHABLE();
}

static void
push_rvalue_binary(struct expr const* expr, size_t id)
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
        push_rvalue(expr->data.binary.lhs);
        appendli("pop rax");
        appendli("mov rbx, 0x00");
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli("jne %s%zu_true", LABEL_EXPR, id);
        appendli("jmp %s%zu_rhs", LABEL_EXPR, id);

        appendln("%s%zu_rhs:", LABEL_EXPR, id);
        push_rvalue(expr->data.binary.rhs);
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

        appendln("%s%zu_end:", LABEL_EXPR, id);
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
        push_rvalue(expr->data.binary.lhs);
        appendli("pop rax");
        appendli("mov rbx, 0x00");
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli("jne %s%zu_rhs", LABEL_EXPR, id);
        appendli("jmp %s%zu_false", LABEL_EXPR, id);

        appendln("%s%zu_rhs:", LABEL_EXPR, id);
        push_rvalue(expr->data.binary.rhs);
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

        appendln("%s%zu_end:", LABEL_EXPR, id);
        return;
    }
    case BOP_SHL: {
        assert(type_is_int(expr->data.binary.lhs->type));
        assert(expr->data.binary.rhs->type->kind == TYPE_USIZE);

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);

        appendli("pop rcx ; shift rhs");
        appendli("mov rbx, 64");
        appendli("cmp rcx, rbx");
        appendli("jl %s%zu_shift", LABEL_EXPR, id);
        appendli("pop rbx ; discard lhs");
        appendli("push 0");
        appendli("jmp %s%zu_end", LABEL_EXPR, id);
        appendln("%s%zu_shift:", LABEL_EXPR, id);
        appendli("pop rax ; shift lhs");
        mov_rax_reg_a_with_zero_or_sign_extend(expr->data.binary.lhs->type);
        appendli("shl rax, cl");
        appendli("push rax");

        appendln("%s%zu_end:", LABEL_EXPR, id);
        return;
    }
    case BOP_SHR: {
        assert(type_is_int(expr->data.binary.lhs->type));
        assert(expr->data.binary.rhs->type->kind == TYPE_USIZE);

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);

        appendli("pop rcx ; shift rhs");
        appendli("mov rbx, 64");
        appendli("cmp rcx, rbx");
        appendli("jl %s%zu_shift", LABEL_EXPR, id);
        if (type_is_sint(expr->data.binary.lhs->type)) {
            appendli("pop rbx ; discard lhs, but cmov based on high bit");
            appendli("mov r8, 0");
            appendli("mov r9, 0xFFFFFFFFFFFFFFFF");
            appendli("cmp rbx, 0");
            appendli("cmovge rax, r8 ; non-negative integer");
            appendli("cmovl rax, r9 ; neative-integer");
            appendli("push rax");
        }
        else {
            appendli("pop rbx ; discard lhs");
            appendli("push 0");
        }
        appendli("jmp %s%zu_end", LABEL_EXPR, id);
        appendln("%s%zu_shift:", LABEL_EXPR, id);
        appendli("pop rax ; shift lhs");
        mov_rax_reg_a_with_zero_or_sign_extend(expr->data.binary.lhs->type);
        appendli(
            "%s rax, cl",
            type_is_sint(expr->data.binary.lhs->type) ? "sar" : "shr");
        appendli("push rax");

        appendln("%s%zu_end:", LABEL_EXPR, id);
        return;
    }
    case BOP_EQ: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);

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

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);

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

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);

        char const* lhs_reg = reg_a(expr->data.binary.lhs->type->size);
        char const* rhs_reg = reg_b(expr->data.binary.rhs->type->size);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli("%s rcx, rdx", type_is_sint(xhs_type) ? "cmovle" : "cmovbe");
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

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);

        char const* lhs_reg = reg_a(expr->data.binary.lhs->type->size);
        char const* rhs_reg = reg_b(expr->data.binary.rhs->type->size);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli("%s rcx, rdx", type_is_sint(xhs_type) ? "cmovl" : "cmovb");
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

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);

        char const* lhs_reg = reg_a(expr->data.binary.lhs->type->size);
        char const* rhs_reg = reg_b(expr->data.binary.rhs->type->size);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli("%s rcx, rdx", type_is_sint(xhs_type) ? "cmovge" : "cmovae");
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

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);

        char const* lhs_reg = reg_a(expr->data.binary.lhs->type->size);
        char const* rhs_reg = reg_b(expr->data.binary.rhs->type->size);
        appendli("pop rbx");
        appendli("pop rax");
        appendli("mov rcx, 0"); // result (default false)
        appendli("mov rdx, 1"); // register holding true
        appendli("cmp %s, %s", lhs_reg, rhs_reg);
        appendli("%s rcx, rdx", type_is_sint(xhs_type) ? "cmovg" : "cmova");
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
            type_is_sint(xhs_type) ? "jno" : "jnc";

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);
        appendli("pop rbx ; add rhs");
        appendli("pop rax ; add lhs");
        appendli("add %s, %s", lhs_reg, rhs_reg);
        appendli("push rax");
        appendli("%s %s%zu_end", jmp_not_overflow, LABEL_EXPR, id);
        appendli("call __fatal_integer_out_of_range");

        appendln("%s%zu_end:", LABEL_EXPR, id);
        return;
    }
    case BOP_ADD_WRAPPING: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;

        char const* const lhs_reg = reg_a(xhs_type->size);
        char const* const rhs_reg = reg_b(xhs_type->size);

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);
        appendli("pop rbx ; add wrapping rhs");
        appendli("pop rax ; add wrapping lhs");
        appendli("add %s, %s", lhs_reg, rhs_reg);
        appendli("push rax");
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
            type_is_sint(xhs_type) ? "jno" : "jnc";

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);
        appendli("pop rbx ; sub rhs");
        appendli("pop rax ; sub lhs");
        appendli("sub %s, %s", lhs_reg, rhs_reg);
        appendli("push rax");
        appendli("%s %s%zu_end", jmp_not_overflow, LABEL_EXPR, id);
        appendli("call __fatal_integer_out_of_range");

        appendln("%s%zu_end:", LABEL_EXPR, id);
        return;
    }
    case BOP_SUB_WRAPPING: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;

        char const* const lhs_reg = reg_a(xhs_type->size);
        char const* const rhs_reg = reg_b(xhs_type->size);

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);
        appendli("pop rbx ; sub wrapping rhs");
        appendli("pop rax ; sub wrapping lhs");
        appendli("sub %s, %s", lhs_reg, rhs_reg);
        appendli("push rax");
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
        char const* const mul = type_is_sint(xhs_type) ? "imul" : "mul";

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);
        appendli("pop rbx ; mul rhs");
        appendli("pop rax ; mul lhs");
        appendli("%s %s", mul, rhs_reg);
        appendli("push rax");
        appendli("jno %s%zu_end", LABEL_EXPR, id);
        appendli("call __fatal_integer_out_of_range");

        appendln("%s%zu_end:", LABEL_EXPR, id);
        return;
    }
    case BOP_MUL_WRAPPING: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;

        char const* const rhs_reg = reg_b(xhs_type->size);
        char const* const mul = type_is_sint(xhs_type) ? "imul" : "mul";

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);
        appendli("pop rbx ; mul rhs");
        appendli("pop rax ; mul lhs");
        appendli("%s %s", mul, rhs_reg);
        appendli("push rax");
        return;
    }
    case BOP_DIV: /* fallthrough */
    case BOP_REM: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
        struct type const* const xhs_type = expr->data.binary.lhs->type;

        char const* const rhs_reg = reg_b(xhs_type->size);
        char const* const div = type_is_sint(xhs_type) ? "idiv" : "div";

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);
        appendli("pop rbx ; div rhs");
        appendli("pop rax ; div lhs");
        if (type_is_sint(xhs_type)) {
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
            assert(type_is_uint(xhs_type));
            // Clear the upper portion of the dividend.
            appendli("xor rdx, rdx");
            // Zero extend the dividend into itself.
            mov_rax_reg_a_with_zero_or_sign_extend(xhs_type);
        }
        appendli("mov rcx, 0");
        appendli(
            "cmp %s, %s",
            rhs_reg,
            reg_c(xhs_type->size)); // divide-by-zero check
        appendli("jne %s%zu_op", LABEL_EXPR, id);
        appendli("call __fatal_integer_divide_by_zero");
        appendli("%s%zu_op:", LABEL_EXPR, id);
        appendli("%s %s", div, rhs_reg);

        // https://www.felixcloutier.com/x86/div
        // https://www.felixcloutier.com/x86/idiv
        //
        // Operand Size            | Dividend | Divisor | Quotient | Remainder
        // Word/byte               | AX       | r/m8    | AL       | AH
        // Doubleword/word         | DX:AX    | r/m16   | AX       | DX
        // Quadword/doubleword     | EDX:EAX  | r/m32   | EAX      | EDX
        // Doublequadword/quadword | RDX:RAX  | r/m64   | RAX      | RDX
        if (expr->data.binary.op == BOP_DIV) {
            appendli("push rax");
        }
        else {
            assert(expr->data.binary.op == BOP_REM);
            if (xhs_type->size == 1) {
                appendli("mov al, ah");
                appendli("push rax");
            }
            else {
                appendli("push rdx");
            }
        }
        return;
    }
    case BOP_BITOR: {
        assert(expr->data.binary.lhs->type->size >= 1u);
        assert(expr->data.binary.lhs->type->size <= 8u);
        assert(expr->data.binary.rhs->type->size >= 1u);
        assert(expr->data.binary.rhs->type->size <= 8u);
        assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);

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

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);

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

        push_rvalue(expr->data.binary.lhs);
        push_rvalue(expr->data.binary.rhs);

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
push_lvalue(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr_is_lvalue(expr));

    // clang-format off
    static struct {
        char const* kind_cstr;
        void (*codegen_fn)(struct expr const*, size_t);
    } const table[] = {
#define TABLE_ENTRY(kind, fn) [kind] = {#kind, fn}
        // clang format off
        TABLE_ENTRY(EXPR_SYMBOL, push_lvalue_symbol),
        TABLE_ENTRY(EXPR_ACCESS_INDEX, push_lvalue_access_index),
        TABLE_ENTRY(EXPR_ACCESS_MEMBER_VARIABLE, push_lvalue_access_member_variable),
        TABLE_ENTRY(EXPR_UNARY, push_lvalue_unary),
#undef TABLE_ENTRY
    };
    // clang-format on

    size_t const id = unique_id++;
    char const* const cstr = table[expr->kind].kind_cstr;
    appendli("; EXPR LVALUE %zu BEGIN", id);
    appendli_location(expr->location, "%s (ID %zu, LVALUE)", cstr, id);
    assert(table[expr->kind].codegen_fn != NULL);
    table[expr->kind].codegen_fn(expr, id);
    appendli("; EXPR LVALUE %zu END", id);
}

static void
push_lvalue_symbol(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SYMBOL);
    (void)id;

    if (expr->type->size == 0) {
        // Zero-sized objects should take up zero space. Attempting to take the
        // address of a zero-sized symbol should always produce a pointer with
        // the value zero.
        appendli(
            "push 0 ; address of type `%s` with size zero (nil)",
            expr->type->name);
        return;
    }
    push_address(symbol_xget_address(expr->data.symbol));
}

static void
push_lvalue_access_index(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);

    if (expr->data.access_index.lhs->type->kind == TYPE_ARRAY) {
        push_lvalue_access_index_lhs_array(expr, id);
        return;
    }

    if (expr->data.access_index.lhs->type->kind == TYPE_SLICE) {
        push_lvalue_access_index_lhs_slice(expr, id);
        return;
    }

    UNREACHABLE();
}

static void
push_lvalue_access_index_lhs_array(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);
    assert(expr->data.access_index.lhs->type->kind == TYPE_ARRAY);
    assert(expr->data.access_index.idx->type->kind == TYPE_USIZE);

    struct type const* const lhs_type = expr->data.access_index.lhs->type;
    struct type const* const element_type = lhs_type->data.array.base;
    if (element_type->size == 0) {
        appendli("push 0 ; nil");
        return;
    }

    push_lvalue(expr->data.access_index.lhs);
    push_rvalue(expr->data.access_index.idx);
    appendli("pop rax"); // index
    appendli("mov rbx, %" PRIu64, lhs_type->data.array.count); // count
    appendli("cmp rax, rbx");
    appendli("jb %s%zu_op", LABEL_EXPR, id);
    appendli("call __fatal_index_out_of_bounds");
    appendln("%s%zu_op:", LABEL_EXPR, id);
    appendli("mov rbx, %" PRIu64, element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // index * sizeof(element_type)
    appendli("pop rbx"); // start
    appendli("add rax, rbx"); // start + index * sizeof(element_type)
    appendli("push rax");
}

static void
push_lvalue_access_index_lhs_slice(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);
    assert(expr->data.access_index.lhs->type->kind == TYPE_SLICE);
    assert(expr->data.access_index.idx->type->kind == TYPE_USIZE);

    struct type const* const lhs_type = expr->data.access_index.lhs->type;
    struct type const* const element_type = lhs_type->data.slice.base;
    if (element_type->size == 0) {
        appendli("push 0 ; nil");
        return;
    }

    push_rvalue(expr->data.access_index.lhs);
    push_rvalue(expr->data.access_index.idx);
    appendli("pop rax"); // index
    appendli("mov rbx, [rsp + 8]"); // slice count
    appendli("cmp rax, rbx");
    appendli("jb %s%zu_op", LABEL_EXPR, id);
    appendli("call __fatal_index_out_of_bounds");
    appendln("%s%zu_op:", LABEL_EXPR, id);
    appendli("mov rbx, %" PRIu64, element_type->size); // sizeof(element_type)
    appendli("mul rbx"); // index * sizeof(element_type)
    appendli("pop rbx"); // start
    appendli("add rax, rbx"); // start + index * sizeof(element_type)
    pop(8u); // slice count
    appendli("push rax");
}

static void
push_lvalue_access_member_variable(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_MEMBER_VARIABLE);
    assert(expr->data.access_member_variable.lhs->type->kind == TYPE_STRUCT);
    (void)id;

    push_lvalue(expr->data.access_member_variable.lhs);
    appendli("pop rax");
    appendli(
        "add rax, %" PRIu64,
        expr->data.access_member_variable.member_variable->offset);
    appendli("push rax");
}

static void
push_lvalue_unary(struct expr const* expr, size_t id)
{
    assert(expr != NULL);
    assert(expr_is_lvalue(expr));
    (void)id;

    switch (expr->data.unary.op) {
    case UOP_DEREFERENCE: {
        assert(expr->data.unary.rhs->type->kind == TYPE_POINTER);
        push_rvalue(expr->data.unary.rhs);
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
codegen(
    bool opt_c, bool opt_k, char const* const* opt_l, char const* const opt_o)
{
    assert(opt_o != NULL);

    out = string_new(NULL, 0u);
    struct string* const asm_path = string_new_fmt("%s.asm", opt_o);
    struct string* const obj_path = string_new_fmt("%s.o", opt_o);

    char const* const SUNDER_BACKEND = getenv("SUNDER_BACKEND");
    if (SUNDER_BACKEND != NULL) {
        // User is explicitly overriding the default Sunder backend.
        backend = SUNDER_BACKEND;
    }
    sbuf(char const*) backend_argv = NULL;
    if (0 == strcmp(backend, "nasm")) {
        sbuf_push(backend_argv, "nasm");
        sbuf_push(backend_argv, "-o");
        sbuf_push(backend_argv, string_start(obj_path));
        sbuf_push(backend_argv, "-w+error=all");
        sbuf_push(backend_argv, "-f");
        sbuf_push(backend_argv, "elf64");
        sbuf_push(backend_argv, "-O0");
        sbuf_push(backend_argv, "-gdwarf");
        sbuf_push(backend_argv, string_start(asm_path));
        sbuf_push(backend_argv, (char const*)NULL);
    }
    else if (0 == strcmp(backend, "yasm")) {
        sbuf_push(backend_argv, "yasm");
        sbuf_push(backend_argv, "-o");
        sbuf_push(backend_argv, string_start(obj_path));
        sbuf_push(backend_argv, "-w+error=all");
        sbuf_push(backend_argv, "-f");
        sbuf_push(backend_argv, "elf64");
        sbuf_push(backend_argv, "-O0");
        sbuf_push(backend_argv, "-gdwarf2");
        sbuf_push(backend_argv, string_start(asm_path));
        sbuf_push(backend_argv, (char const*)NULL);
    }
    else {
        fatal(NULL, "unrecognized backend `%s`", backend);
    }

    sbuf(char const*) ld_argv = NULL;
    sbuf_push(ld_argv, "ld");
    sbuf_push(ld_argv, "-o");
    sbuf_push(ld_argv, opt_o);
    sbuf_push(ld_argv, string_start(obj_path));
    for (size_t i = 0; i < sbuf_count(opt_l); ++i) {
        sbuf_push(ld_argv, opt_l[i]);
    }
    sbuf_push(ld_argv, (char const*)NULL);

    void* sysasm_buf = NULL;
    size_t sysasm_buf_size = 0;
    load_sysasm(&sysasm_buf, &sysasm_buf_size);

    codegen_extern_labels(sysasm_buf, sysasm_buf_size);
    appendch('\n');
    codegen_global_labels();
    appendch('\n');
    if (!opt_c) {
        appendln("%%define __entry");
        appendch('\n');
    }
    append("%.*s", (int)sysasm_buf_size, (char const*)sysasm_buf);
    appendch('\n');
    codegen_fatals();
    appendch('\n');
    codegen_static_constants();
    appendch('\n');
    codegen_static_variables();
    appendch('\n');
    codegen_static_functions();

    int err = 0;
    if ((err = file_write_all(
             string_start(asm_path), string_start(out), string_count(out)))) {
        error(
            NULL,
            "unable to write file `%s` with error '%s'",
            string_start(asm_path),
            strerror(errno));
        goto cleanup;
    }

    if ((err = spawnvpw(backend_argv))) {
        goto cleanup;
    }

    if (!opt_c && (err = spawnvpw(ld_argv))) {
        goto cleanup;
    }

cleanup:
    if (!opt_k) {
        (void)remove(string_start(asm_path));
    }
    if (!opt_k && !opt_c) {
        (void)remove(string_start(obj_path));
    }
    xalloc(sysasm_buf, XALLOC_FREE);
    sbuf_fini(backend_argv);
    sbuf_fini(ld_argv);
    string_del(asm_path);
    string_del(obj_path);
    string_del(out);
    if (err) {
        exit(EXIT_FAILURE);
    }
}
