// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sunder.h"

#if defined(__GNUC__) /* GCC and Clang */
#    define WRITEF __attribute__((format(printf, 1, 2)))
#    define WRITEF_LOCATION __attribute__((format(printf, 2, 3)))
#else
#    define WRITEF /* nothing */
#    define WRITEF_LOCATION /* nothing */
#endif

#define MANGLE_PREFIX "__sunder_"

static bool debug = false;
static unsigned indent = 0u;
static struct function const* current_function = NULL;
static struct stmt const* current_for_range_loop = NULL;
FILE* out = NULL;

static char const*
strgen(char const* start, size_t count);
static char const*
strgen_cstr(char const* cstr);
static WRITEF char const*
strgen_fmt(char const* fmt, ...);

static char const*
mangle(char const* cstr);
static char const*
mangle_name(char const* name);
static char const*
mangle_type(struct type const* type);
static char const*
mangle_address(struct address const* address);
static char const*
mangle_symbol(struct symbol const* symbol);

static void
indent_incr(void);
static void
indent_decr(void);

static WRITEF void
append(char const* fmt, ...);
static WRITEF void
appendln(char const* fmt, ...);
static WRITEF void
appendli(char const* fmt, ...);
static WRITEF_LOCATION void
appendli_location(struct source_location location, char const* fmt, ...);
static void
appendch(char ch);

static void
codegen_type_declaration(struct type const* type);
static void
codegen_type_definition(struct type const* type);
static void
codegen_static_object(struct symbol const* symbol);
static void
codegen_static_function(struct symbol const* symbol, bool prototype);

static char const*
strgen_value(struct value const* value);
static char const*
strgen_uninit(struct type const* type);

static void
codegen_block(struct block const* block);

static void
codegen_defers(struct stmt const* begin, struct stmt const* end);

static void
codegen_stmt(struct stmt const* stmt);
static void
codegen_stmt_defer(struct stmt const* stmt);
static void
codegen_stmt_if(struct stmt const* stmt);
static void
codegen_stmt_for_range(struct stmt const* stmt);
static void
codegen_stmt_for_expr(struct stmt const* stmt);
static void
codegen_stmt_break(struct stmt const* stmt);
static void
codegen_stmt_continue(struct stmt const* stmt);
static void
codegen_stmt_switch(struct stmt const* stmt);
static void
codegen_stmt_return(struct stmt const* stmt);
static void
codegen_stmt_assert(struct stmt const* stmt);
static void
codegen_stmt_assign(struct stmt const* stmt);
static void
codegen_stmt_expr(struct stmt const* stmt);

static char const*
strgen_rvalue(struct expr const* expr);
static char const*
strgen_rvalue_symbol(struct expr const* expr);
static char const*
strgen_rvalue_value(struct expr const* expr);
static char const*
strgen_rvalue_bytes(struct expr const* expr);
static char const*
strgen_rvalue_array_list(struct expr const* expr);
static char const*
strgen_rvalue_slice_list(struct expr const* expr);
static char const*
strgen_rvalue_slice(struct expr const* expr);
static char const*
strgen_rvalue_init(struct expr const* expr);
static char const*
strgen_rvalue_cast(struct expr const* expr);
static char const*
strgen_rvalue_call(struct expr const* expr);
static char const*
strgen_rvalue_access_index(struct expr const* expr);
static char const*
strgen_rvalue_access_slice(struct expr const* expr);
static char const*
strgen_rvalue_access_slice_lhs_array(struct expr const* expr);
static char const*
strgen_rvalue_access_slice_lhs_slice(struct expr const* expr);
static char const*
strgen_rvalue_access_member_variable(struct expr const* expr);
static char const*
strgen_rvalue_sizeof(struct expr const* expr);
static char const*
strgen_rvalue_alignof(struct expr const* expr);
static char const*
strgen_rvalue_unary(struct expr const* expr);
static char const*
strgen_rvalue_unary_not(struct expr const* expr);
static char const*
strgen_rvalue_unary_pos(struct expr const* expr);
static char const*
strgen_rvalue_unary_neg(struct expr const* expr);
static char const*
strgen_rvalue_unary_neg_wrapping(struct expr const* expr);
static char const*
strgen_rvalue_unary_bitnot(struct expr const* expr);
static char const*
strgen_rvalue_unary_dereference(struct expr const* expr);
static char const*
strgen_rvalue_unary_addressof_lvalue(struct expr const* expr);
static char const*
strgen_rvalue_unary_addressof_rvalue(struct expr const* expr);
static char const*
strgen_rvalue_unary_startof(struct expr const* expr);
static char const*
strgen_rvalue_unary_countof(struct expr const* expr);
static char const*
strgen_rvalue_binary(struct expr const* expr);
static char const*
strgen_rvalue_binary_or(struct expr const* expr);
static char const*
strgen_rvalue_binary_and(struct expr const* expr);
static char const*
strgen_rvalue_binary_shl(struct expr const* expr);
static char const*
strgen_rvalue_binary_shr(struct expr const* expr);
static char const*
strgen_rvalue_binary_eq(struct expr const* expr);
static char const*
strgen_rvalue_binary_ne(struct expr const* expr);
static char const*
strgen_rvalue_binary_le(struct expr const* expr);
static char const*
strgen_rvalue_binary_lt(struct expr const* expr);
static char const*
strgen_rvalue_binary_ge(struct expr const* expr);
static char const*
strgen_rvalue_binary_gt(struct expr const* expr);
static char const*
strgen_rvalue_binary_add(struct expr const* expr);
static char const*
strgen_rvalue_binary_add_wrapping(struct expr const* expr);
static char const*
strgen_rvalue_binary_sub(struct expr const* expr);
static char const*
strgen_rvalue_binary_sub_wrapping(struct expr const* expr);
static char const*
strgen_rvalue_binary_mul(struct expr const* expr);
static char const*
strgen_rvalue_binary_mul_wrapping(struct expr const* expr);
static char const*
strgen_rvalue_binary_div(struct expr const* expr);
static char const*
strgen_rvalue_binary_rem(struct expr const* expr);
static char const*
strgen_rvalue_binary_bitor(struct expr const* expr);
static char const*
strgen_rvalue_binary_bitxor(struct expr const* expr);
static char const*
strgen_rvalue_binary_bitand(struct expr const* expr);

static char const*
strgen_lvalue(struct expr const* expr);
static char const*
strgen_lvalue_symbol(struct expr const* expr);
static char const*
strgen_lvalue_bytes(struct expr const* expr);
static char const*
strgen_lvalue_access_index(struct expr const* expr);
static char const*
strgen_lvalue_access_member_variable(struct expr const* expr);
static char const*
strgen_lvalue_unary(struct expr const* expr);

static char const*
strgen(char const* start, size_t count)
{
    char* const new = cstr_new(start, count);
    freeze(new);
    return new;
}

static char const*
strgen_cstr(char const* cstr)
{
    char* const new = cstr_new_cstr(cstr);
    freeze(new);
    return new;
}

static char const*
strgen_fmt(char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* const new = cstr_new_vfmt(fmt, args);
    va_end(args);

    freeze(new);
    return new;
}

static char const*
mangle(char const* cstr)
{
    assert(cstr != NULL);

    struct string* const s = string_new(NULL, 0);

    for (char const* cur = cstr; *cur != '\0';) {
        if (safe_isspace(*cur)) {
            cur += 1;
            continue;
        }

        // some::symbol
        //     ^^
        //
        // some_symbol
        //     ^
        if (cstr_starts_with(cur, "::")) {
            string_append_cstr(s, "_");
            cur += 2;
            continue;
        }

        // type[[foo, bar]]
        //     ^^
        //
        // type_TEMPLATE_BGN_foo_COMMA_bar_TEMPLATE_END
        //     ^^^^^^^^^^^^^^
        if (cstr_starts_with(cur, "[[")) {
            string_append_cstr(s, "_TEMPLATE_BGN_");
            cur += 2;
            continue;
        }

        // type[[foo, bar]]
        //               ^^
        //
        // type_TEMPLATE_BGN_foo_COMMA_bar_TEMPLATE_END
        //                                ^^^^^^^^^^^^^
        if (cstr_starts_with(cur, "]]")) {
            string_append_cstr(s, "_TEMPLATE_END");
            cur += 2;
            continue;
        }

        // type[[foo, bar]]
        //          ^
        //
        // type_TEMPLATE_BGN_foo_COMMA_bar_TEMPLATE_END
        //                      ^^^^^^^
        if (cstr_starts_with(cur, ",")) {
            string_append_cstr(s, "_COMMA_");
            cur += 1;
            continue;
        }

        // []type
        // ^^
        //
        // slice_of_type
        // ^^^^^^^^^
        if (cstr_starts_with(cur, "[]")) {
            string_append_cstr(s, "slice_of_");
            cur += 2;
            continue;
        }

        // [N]type
        // ^
        //
        // array_N_of_type
        // ^^^^^^
        if (cstr_starts_with(cur, "[")) {
            string_append_cstr(s, "array_");
            cur += 1;
            continue;
        }

        // [N]type
        //   ^
        //
        // array_N_of_type
        //        ^^^^
        if (cstr_starts_with(cur, "]")) {
            string_append_cstr(s, "_of_");
            cur += 1;
            continue;
        }

        // *type
        // ^
        //
        // pointer_to_type
        // ^^^^^^^^^^^
        if (cstr_starts_with(cur, "*")) {
            string_append_cstr(s, "pointer_to_");
            cur += 1;
            continue;
        }

        // struct { var <member>; }
        //        ^
        //
        // struct_LBRACE_VAR_<member>_RBRACE
        //       ^^^^^^^
        if (cstr_starts_with(cur, "{")) {
            string_append_cstr(s, "_LBRACE");
            cur += 1;
            continue;
        }

        // struct { var <member>; }
        //                        ^
        //
        // struct_LBRACE_VAR_<member>_RBRACE
        //                           ^^^^^^^
        if (cstr_starts_with(cur, "}")) {
            string_append_cstr(s, "_RBRACE");
            cur += 1;
            continue;
        }

        // struct { var <member-name>: <member-type>; }
        //          ^^^
        //
        // struct_LBRACE_VAR_<member-name>_TYPE_<member-type>_RBRACE
        //              ^^^^^
        if (cstr_starts_with(cur, "var ")) {
            string_append_cstr(s, "_VAR_");
            cur += 4;
            continue;
        }

        // struct { var <member-name>: <member-type>; }
        //                           ^
        //
        // struct_LBRACE_VAR_<member-name>_TYPE_<member-type>_RBRACE
        //                                ^^^^^^
        if (cstr_starts_with(cur, ":")) {
            string_append_cstr(s, "_TYPE_");
            cur += 1;
            continue;
        }

        // struct { var <member-name>: <member-type>; }
        //                                          ^
        //
        // struct_LBRACE_VAR_<member-name>_TYPE_<member-type>_RBRACE
        if (cstr_starts_with(cur, ";")) {
            cur += 1;
            continue;
        }

        if (!safe_isalnum(*cur)) {
            string_append_cstr(s, "_");
            cur += 1;
            continue;
        }

        string_append_fmt(s, "%c", *cur);
        cur += 1;
    }

    char const* const result = strgen(string_start(s), string_count(s));

    string_del(s);
    return result;
}

static char const*
mangle_name(char const* name)
{
    assert(name != NULL);

    char const* const reserved[] = {
        // Preprocessor Keywords
        /* if */ /* Sunder keyword */
        /* elif */ /* Sunder keyword */
        /* else */ /* Sunder keyword */
        "endif",
        "ifdef",
        "ifndef",
        "elifdef",
        "elifndef",
        "define",
        "undef",
        "include",
        /* embed */ /* Sunder keyword */
        "line",
        "error",
        "warning",
        "pragma",
        /* defined */ /* Sunder keyword */
        "__has_include",
        "__has_embed",
        "__has_c_attribute",

        // Non-preprocessor Pragma Control
        "_Pragma",

        // Language Keywords
        "alignas",
        /* alignof */ /* Sunder keyword */
        "auto",
        /* bool */ /* Sunder keyword */
        /* break */ /* Sunder keyword */
        /* case */ /* Sunder keyword */
        "char",
        "const",
        "constexpr",
        /* continue */ /* Sunder keyword */
        "default",
        "do",
        "double",
        /* else */ /* Sunder keyword */
        /* enum */ /* Sunder keyword */
        /* extern */ /* Sunder keyword */
        /* false */ /* Sunder keyword */
        "float",
        /* for */ /* Sunder keyword */
        "goto",
        /* if */ /* Sunder keyword */
        "inline",
        "int",
        "long",
        "nullptr",
        "register",
        "restrict",
        "return", /* Sunder keyword that is also used as a symbol name */
        "short",
        "signed",
        /* sizeof */ /* Sunder keyword */
        "static",
        "static_assert",
        /* struct */ /* Sunder keyword */
        /* switch */ /* Sunder keyword */
        "thread_local",
        /* true */ /* Sunder keyword */
        "typedef",
        /* typeof */ /* Sunder keyword */
        "typeof_unqual",
        /* union */ /* Sunder keyword */
        "unsigned",
        /* void */ /* Sunder keyword */
        "volatile",
        "while",
        "_Alignas",
        "_Alignof",
        "_Atomic",
        "_BitInt",
        "_Bool",
        "_Complex",
        "_Decimal128",
        "_Decimal32",
        "_Decimal64",
        "_Generic",
        "_Imaginary",
        "_Noreturn",
        "_Static_assert",
        "_Thread_local",

        // C Extensions
        "asm",
        "fortran",

        // C Runtime
        "main",

        // GNU C Extension Keywords
        "__FUNCTION__",
        "__PRETTY_FUNCTION__",
        "__alignof",
        "__alignof__",
        "__asm",
        "__asm__",
        "__attribute",
        "__attribute__",
        "__builtin_offsetof",
        "__builtin_va_arg",
        "__complex",
        "__complex__",
        "__const",
        "__extension__",
        "__func__",
        "__imag",
        "__imag__",
        "__inline",
        "__inline__",
        "__label__",
        "__null",
        "__real",
        "__real__",
        "__restrict",
        "__restrict__",
        "__signed",
        "__signed__",
        "__thread",
        "__typeof",
        "__volatile",
        "__volatile__",
    };

    for (size_t i = 0; i < ARRAY_COUNT(reserved); ++i) {
        if (0 == strcmp(name, reserved[i])) {
            return strgen_fmt(MANGLE_PREFIX "%s", mangle(name));
        }
    }

    return mangle(name);
}

static char const*
mangle_type_recursive(struct type const* type)
{
    assert(type != NULL);

    if (type->kind == TYPE_FUNCTION) {
        struct string* p = string_new(NULL, 0);
        sbuf(struct type const* const) ptypes =
            type->data.function.parameter_types;
        for (size_t i = 0; i < sbuf_count(ptypes); ++i) {
            string_append_fmt(
                p,
                "_parameter_%zu_%s",
                i + 1,
                mangle_type_recursive(ptypes[i]));
        }

        struct string* const s = string_new_fmt(
            "func%s_returning_%s",
            string_start(p),
            mangle_type_recursive(type->data.function.return_type));

        char const* const result = strgen(string_start(s), string_count(s));

        string_del(p);
        string_del(s);
        return result;
    }

    if (type->kind == TYPE_POINTER) {
        struct string* const s = string_new_fmt(
            "pointer_to_%s", mangle_type_recursive(type->data.pointer.base));
        char const* const result = strgen(string_start(s), string_count(s));
        string_del(s);
        return result;
    }

    if (type->kind == TYPE_ARRAY) {
        struct string* const s = string_new_fmt(
            "array_%ju_of_%s",
            type->data.array.count,
            mangle_type_recursive(type->data.array.base));
        char const* const result = strgen(string_start(s), string_count(s));
        string_del(s);
        return result;
    }

    if (type->kind == TYPE_SLICE) {
        struct string* const s = string_new_fmt(
            "slice_of_%s", mangle_type_recursive(type->data.slice.base));
        char const* const result = strgen(string_start(s), string_count(s));
        string_del(s);
        return result;
    }

    return mangle(type->name);
}

static char const*
mangle_type(struct type const* type)
{
    assert(type != NULL);

    if (type->kind != TYPE_VOID && type->size == 0) {
        return context()->interned.void_;
    }

    if (type->size == SIZEOF_UNSIZED) {
        return context()->interned.void_;
    }

    char const* const typename = mangle_type_recursive(type);

    // Type names such as "pointer_to_T" or "slice_of_T" are valid identifiers
    // in both C and Sunder. Add the mangle prefix to the type names generated
    // in this fashion to avoid potential symbol conflicts.
    bool has_readable_name = type->kind == TYPE_FUNCTION
        || type->kind == TYPE_POINTER || type->kind == TYPE_ARRAY
        || type->kind == TYPE_SLICE;
    return has_readable_name ? strgen_fmt(MANGLE_PREFIX "%s", mangle(typename))
                             : mangle_name(typename);
}

static char const*
mangle_address(struct address const* address)
{
    assert(address != NULL);

    switch (address->kind) {
    case ADDRESS_ABSOLUTE: {
        UNREACHABLE();
    }
    case ADDRESS_STATIC: {
        // Mangled static addresses in generated C produced by `mangle_address`
        // are used exclusively for symbols and static objects, and will always
        // have an offset of zero.
        assert(address->data.static_.offset == 0);
        return mangle_name(address->data.static_.name);
    }
    case ADDRESS_LOCAL: {
        return strgen_fmt(MANGLE_PREFIX "%s", mangle(address->data.local.name));
    }
    }

    UNREACHABLE();
    return NULL;
}

static char const*
mangle_symbol(struct symbol const* symbol)
{
    assert(symbol != NULL);

    return mangle_address(symbol_xget_address(symbol));
}

static void
indent_incr(void)
{
    assert(indent != UINT_MAX);
    indent += 1;
}

static void
indent_decr(void)
{
    assert(indent != 0);
    indent -= 1;
}

static void
append(char const* fmt, ...)
{
    assert(out != NULL);
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
}

static void
appendln(char const* fmt, ...)
{
    assert(out != NULL);
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fputc('\n', out);
}

static void
appendli(char const* fmt, ...)
{
    assert(out != NULL);
    assert(fmt != NULL);

    for (unsigned i = 0; i < indent; ++i) {
        fprintf(out, "    ");
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fputc('\n', out);
}

static void
appendli_location(struct source_location location, char const* fmt, ...)
{
    assert(out != NULL);
    assert(location.path != NO_PATH);
    assert(location.line != NO_LINE);
    assert(location.psrc != NO_PSRC);

    for (unsigned i = 0; i < indent; ++i) {
        fprintf(out, "    ");
    }

    fprintf(out, "/// [%s:%zu] ", location.path, location.line);

    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fputc('\n', out);

    char const* const line_start = source_line_start(location.psrc);
    char const* const line_end = source_line_end(location.psrc);

    for (unsigned i = 0; i < indent; ++i) {
        fprintf(out, "    ");
    }
    fprintf(out, "/// %.*s\n", (int)(line_end - line_start), line_start);

    for (unsigned i = 0; i < indent; ++i) {
        fprintf(out, "    ");
    }
    fprintf(out, "/// %*s^\n", (int)(location.psrc - line_start), "");
}

static void
appendch(char ch)
{
    assert(out != NULL);

    fputc(ch, out);
}

static void
codegen_type_declaration(struct type const* type)
{
    if (type->size == 0 || type->size == SIZEOF_UNSIZED) {
        return;
    }

    char const* const typename = mangle_type(type);
    if (type->kind == TYPE_STRUCT) {
        appendln("typedef struct %s %s; // %s", typename, typename, type->name);
    }
    if (type->kind == TYPE_UNION) {
        appendln("typedef union %s %s; // %s", typename, typename, type->name);
    }
}

static void
codegen_type_definition(struct type const* type)
{
    switch (type->kind) {
    case TYPE_ANY: /* fallthrough */
    case TYPE_VOID: /* fallthrough */
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
    case TYPE_SSIZE: /* fallthrough */
    case TYPE_INTEGER: /* fallthrough */
    case TYPE_F32: /* fallthrough */
    case TYPE_F64: /* fallthrough */
    case TYPE_REAL: {
        break;
    }
    case TYPE_FUNCTION: {
        struct string* params = string_new(NULL, 0);
        size_t params_written = 0;
        sbuf(struct type const* const) parameter_types =
            type->data.function.parameter_types;
        for (size_t i = 0; i < sbuf_count(parameter_types); ++i) {
            if (parameter_types[i]->size == 0) {
                continue;
            }
            if (params_written != 0) {
                string_append_cstr(params, ", ");
            }
            string_append_cstr(params, mangle_type(parameter_types[i]));
            params_written += 1;
        }
        appendln(
            "typedef %s (*%s)(%s); // %s",
            mangle_type(type->data.function.return_type),
            mangle_type(type),
            string_count(params) != 0 ? string_start(params) : "void",
            type->name);
        string_del(params);
        break;
    }
    case TYPE_POINTER: {
        // If the base type of this pointer is unrepresentable as a standalone
        // type in C, then we use `char*` so that dereferencing this type is
        // valid. The Sunder type `*any` replaces traditional use of `void*`
        // and `void const*` in C, and since the `any` type is unsized, this
        // check will not mess with `*any` types.
        if (type->data.pointer.base->size == 0) {
            char const* const typename = mangle_type(type);
            appendln("typedef char* %s; // %s", typename, type->name);
            break;
        }
        char const* const basename = mangle_type(type->data.pointer.base);
        char const* const typename = mangle_type(type);
        appendln("typedef %s* %s; // %s", basename, typename, type->name);
        break;
    }
    case TYPE_ARRAY: {
        if (type->size == 0 || type->size == SIZEOF_UNSIZED) {
            return;
        }
        char const* const basename = mangle_type(type->data.array.base);
        char const* const typename = mangle_type(type);
        appendln(
            "typedef struct {%s elements[%ju];} %s; // %s",
            basename,
            type->data.array.count,
            typename,
            type->name);
        break;
    }
    case TYPE_SLICE: {
        struct type* const starttype = type_new_pointer(type->data.slice.base);
        char const* const startname = mangle_type(starttype);
        xalloc(starttype, XALLOC_FREE);
        char const* const countname = mangle_type(context()->builtin.usize);
        char const* const typename = mangle_type(type);

        appendln(
            "typedef struct {%s start; %s count;} %s; // %s",
            startname,
            countname,
            typename,
            type->name);
        break;
    }
    case TYPE_STRUCT: {
        if (type->size == 0 || type->size == SIZEOF_UNSIZED) {
            return;
        }
        char const* const typename = mangle_type(type);
        appendln("struct %s", typename);
        appendli("{");
        indent_incr();
        uintmax_t padding_offset = 0;
        sbuf(struct member_variable const) const mvars =
            type->data.struct_.member_variables;
        for (size_t i = 0; i < sbuf_count(mvars); ++i) {
            if (mvars[i].type->size == 0
                || mvars[i].type->size == SIZEOF_UNSIZED) {
                continue;
            }
            for (; padding_offset < mvars[i].offset; ++padding_offset) {
                appendli(
                    "unsigned char %s_%ju;",
                    MANGLE_PREFIX "padding_byte",
                    padding_offset);
            }
            appendli(
                "%s %s;",
                mangle_type(mvars[i].type),
                mangle_name(mvars[i].name));
            padding_offset += mvars[i].type->size;
        }
        for (; padding_offset < type->size; ++padding_offset) {
            appendli(
                "unsigned char %s_%ju;",
                MANGLE_PREFIX "padding_byte",
                padding_offset);
        }
        indent_decr();
        appendli("};");
        break;
    }
    case TYPE_UNION: {
        if (type->size == 0 || type->size == SIZEOF_UNSIZED) {
            return;
        }
        char const* const typename = mangle_type(type);
        appendln("union %s", typename);
        appendli("{");
        indent_incr();
        appendli(
            "unsigned char %s[%ju]; /* union-sized buffer */",
            MANGLE_PREFIX "buffer",
            type->size);
        sbuf(struct member_variable const) const mvars =
            type->data.union_.member_variables;
        for (size_t i = 0; i < sbuf_count(mvars); ++i) {
            if (mvars[i].type->size == 0
                || mvars[i].type->size == SIZEOF_UNSIZED) {
                continue;
            }
            appendli(
                "%s %s;",
                mangle_type(mvars[i].type),
                mangle_name(mvars[i].name));
        }
        indent_decr();
        appendli("};");
        break;
    }
    case TYPE_ENUM: {
        if (type->size == 0 || type->size == SIZEOF_UNSIZED) {
            return;
        }
        char const* const typename = mangle_type(type);
        char const* const underlying =
            mangle_type(type->data.enum_.underlying_type);
        appendln("typedef %s %s; // enum", underlying, typename);
        break;
    }
    case TYPE_EXTERN: {
        if (type->size == 0 || type->size == SIZEOF_UNSIZED) {
            return;
        }
        char const* const typename = mangle_type(type);
        appendln("typedef void %s; // extern type", typename);
        break;
    }
    }

    if (type->size != 0 && type->size != SIZEOF_UNSIZED) {
        char const* const typename = mangle_type(type);
        appendln(
            "_Static_assert(sizeof(%s) == %ju, \"sizeof(%s)\");",
            typename,
            type->size,
            type->name);
        appendln(
            "_Static_assert(_Alignof(%s) == %ju, \"alignof(%s)\");",
            typename,
            type->align,
            type->name);
    }
}

static void
codegen_static_object(struct symbol const* symbol)
{
    assert(symbol != NULL);
    assert(symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_CONSTANT);
    assert(symbol_xget_address(symbol)->kind == ADDRESS_STATIC);

    char const* const name = symbol_xget_address(symbol)->data.static_.name;
    struct type const* const type = symbol_xget_type(symbol);
    bool const is_extern_variable =
        symbol->kind == SYMBOL_VARIABLE && symbol->data.variable->is_extern;
    if (is_extern_variable) {
        appendln("extern %s %s;", mangle_type(type), mangle(name));
        return;
    }

    if (type->size == 0) {
        // Zero-sized objects take up zero space.
        return;
    }

    if (symbol->kind == SYMBOL_CONSTANT) {
        append("const ");
    }

    assert(symbol_xget_address(symbol)->data.static_.offset == 0);
    append("%s %s", mangle_type(type), mangle_name(name));
    if (symbol->data.variable->value == NULL) {
        // Global data without an initializer is zero-initialized.
        appendln(";");
        return;
    }

    appendln(
        " = (%s)%s;",
        mangle_type(symbol_xget_type(symbol)),
        strgen_value(symbol_xget_value(NO_LOCATION, symbol)));
}

static void
codegen_static_function(struct symbol const* symbol, bool prototype)
{
    assert(symbol != NULL);
    assert(symbol->kind == SYMBOL_FUNCTION);

    assert(symbol_xget_value(NO_LOCATION, symbol)->type->kind == TYPE_FUNCTION);
    struct function const* const function =
        symbol_xget_value(NO_LOCATION, symbol)->data.function;

    if (function->is_extern && !prototype) {
        // No definition for extern functions.
        return;
    }

    struct string* params = string_new(NULL, 0);
    size_t params_written = 0;
    if (prototype) {
        sbuf(struct type const* const) const parameter_types =
            function->type->data.function.parameter_types;
        for (size_t i = 0; i < sbuf_count(parameter_types); ++i) {
            struct type const* const type = parameter_types[i];
            if (type->size == 0) {
                continue;
            }
            if (params_written != 0) {
                string_append_cstr(params, ", ");
            }
            string_append_cstr(params, mangle_type(type));
            params_written += 1;
        }
    }
    else {
        sbuf(struct symbol const* const) parameters =
            function->symbol_parameters;
        for (size_t i = 0; i < sbuf_count(parameters); ++i) {
            if (symbol_xget_type(parameters[i])->size == 0) {
                continue;
            }
            if (params_written != 0) {
                string_append_cstr(params, ", ");
            }
            string_append_cstr(
                params, mangle_type(symbol_xget_type(parameters[i])));
            string_append_cstr(params, " ");
            string_append_cstr(params, mangle_symbol(parameters[i]));
            params_written += 1;
        }
    }

    append(
        "%s%c%s(%s)",
        mangle_type(function->type->data.function.return_type),
        (prototype ? ' ' : '\n'),
        ((function->is_extern ? mangle : mangle_name)(
            function->address->data.static_.name)),
        params_written != 0 ? string_start(params) : "void");

    string_del(params);

    if (prototype) {
        appendln(";");
        return;
    }

    appendch('\n');
    assert(current_function == NULL);
    current_function = function;
    codegen_block(&function->body);
    current_function = NULL;
}

static char const*
strgen_value(struct value const* value)
{
    assert(value != NULL);

    struct string* const s = string_new(NULL, 0);

    switch (value->type->kind) {
    case TYPE_ANY: /* fallthrough */
    case TYPE_VOID: {
        UNREACHABLE();
    }
    case TYPE_BOOL: {
        char const* const identifier = value->data.boolean ? "true" : "false";
        string_append_cstr(s, mangle_name(identifier));
        break;
    }
    case TYPE_BYTE: {
        string_append_fmt(s, "0x%02x", value->data.byte);
        break;
    }
    case TYPE_U8: /* fallthrough */
    case TYPE_U16: /* fallthrough */
    case TYPE_U32: /* fallthrough */
    case TYPE_U64: /* fallthrough */
    case TYPE_USIZE: {
        char* const cstr = bigint_to_new_cstr(value->data.integer);
        string_append_fmt(s, "(%s)%sULL", mangle_type(value->type), cstr);
        xalloc(cstr, XALLOC_FREE);
        break;
    }
    case TYPE_S8: /* fallthrough */
    case TYPE_S16: /* fallthrough */
    case TYPE_S32: /* fallthrough */
    case TYPE_S64: /* fallthrough */
    case TYPE_SSIZE: {
        char* const cstr = bigint_to_new_cstr(value->data.integer);
        struct bigint const* const min = value->type->data.integer.min;
        if (bigint_cmp(value->data.integer, min) == 0) {
            struct bigint* const tmp = bigint_new(value->data.integer);
            bigint_add(tmp, tmp, BIGINT_POS_ONE);
            char* const tmp_cstr = bigint_to_new_cstr(tmp);
            string_append_fmt(
                s,
                "/* %s */((%s)%sLL - 1)",
                cstr,
                mangle_type(value->type),
                tmp_cstr);
            xalloc(tmp_cstr, XALLOC_FREE);
            bigint_del(tmp);
        }
        else {
            string_append_fmt(s, "(%s)%sLL", mangle_type(value->type), cstr);
        }
        xalloc(cstr, XALLOC_FREE);
        break;
    }
    case TYPE_INTEGER: {
        UNREACHABLE();
    }
    case TYPE_F32: {
        double const ieee754 = (double)value->data.f32;
        if (isnan(ieee754)) {
            string_append_cstr(s, "/* NAN */(0.0f / 0.0f)");
            break;
        }
        if (isinf(ieee754) && ieee754 < 0) {
            string_append_cstr(s, "/* -INFINITY */(-1.0f / 0.0f)");
            break;
        }
        if (isinf(ieee754) && ieee754 > 0) {
            string_append_cstr(s, "/* +INFINITY */(+1.0f / 0.0f)");
            break;
        }
        string_append_fmt(s, "%.*ff", IEEE754_FLT_DECIMAL_DIG, ieee754);
        break;
    }
    case TYPE_F64: {
        double const ieee754 = value->data.f64;
        if (isnan(ieee754)) {
            string_append_cstr(s, "/* NAN */(0.0 / 0.0)");
            break;
        }
        if (isinf(ieee754) && ieee754 < 0) {
            string_append_cstr(s, "/* -INFINITY */(-1.0 / 0.0)");
            break;
        }
        if (isinf(ieee754) && ieee754 > 0) {
            string_append_cstr(s, "/* +INFINITY */(+1.0f / 0.0f)");
            break;
        }
        string_append_fmt(s, "%.*f", IEEE754_DBL_DECIMAL_DIG, ieee754);
        break;
    }
    case TYPE_REAL: {
        UNREACHABLE();
    }
    case TYPE_FUNCTION: {
        struct address const* const address = value->data.function->address;
        assert(address->kind == ADDRESS_STATIC);
        assert(address->data.static_.offset == 0);
        // According to the ISO C standard, calling a function of one type
        // through a function pointer of a different (incompatible) type
        // results in undefined behavior.
        //
        // ISO/IEC 9899:2024 - Section 6.3.3.3:
        // > A pointer to a function of one type can be converted to a pointer
        // > to a function of another type and back again; the result shall
        // > compare equal to the original pointer. If a converted pointer is
        // > used to call a function whose type is not compatible with the
        // > referenced type, the behavior is undefined.
        //
        // Sunder allows implicit casting of a function with parameter types
        // and/or a return type of type `*T` to a function type where those
        // same parameter types and/or return type are of type `*any`. Function
        // values in generated C are cast to a void pointer in order to support
        // the implicit conversions that are permitted in Sunder, but are
        // disallowed in strictly conforming ISO C.
        //
        // Note that this cast to a void pointer does *not* make the generated
        // code strictly conforming. Rather, the cast is intended to silence
        // compile-time reporting of this specific undefined behavior.
        //
        // Sunder views pointer values as "just an address", matching the
        // original representation of pointers as word-sized integers within
        // the Sunder abstract machine and generated NASM-flavored x64 assembly
        // emitted when Sunder was exclusively targeting the x64 Linux
        // platform. This definition of pointer types and values does *not*
        // match the definition of pointer types and values used by the ISO C
        // standard, but is expected to work on existing Unix-like platforms
        // due to the historical representation of pointers as word-sized
        // integers.
        //
        // It is expected that the form of function<->function casting here
        // *should* be well behaved, as function<->pointer casts are used in
        // dlsym, and modern systems programming languages such as Rust
        // guarantee that all pointer types have a single word-sized
        // platform-dependent representation.
        //
        // IEEE Std 1003.1-2024 - dlsym - Under APPLICATION USAGE:
        // > Note that conversion from a void * pointer to a function pointer
        // > as in:
        // >
        // > fptr = (int (*)(int))dlsym(handle, "my_function");
        // >
        // > is not defined by the ISO C standard. This standard requires this
        // > conversion to work correctly on conforming implementations.
        //
        // Is is also expected that the representation of pointers as
        // word-sized integers also implies that two functions containing
        // pointer parameters or a pointer return type, which differ only in
        // the base type of those pointer parameters and/or pointer return
        // type, are ABI compatible, e.g. the function types:
        //
        //     func f(*s32, *s64; *f32, *f64, *s, *t, *[]byte) *bool
        //
        // and
        //
        //      func(*any, *any, *any, *any, *any, *any, *any) *any
        //
        // will be called with the same arguments and return value location in
        // the same registers and/or offsets within the called stack frame.
        string_append_fmt(s, "(void*)%s", mangle_address(address));
        break;
    }
    case TYPE_POINTER: {
        struct address const* const address = &value->data.pointer;
        if (value->type->data.pointer.base->size == 0) {
            string_append_cstr(s, "0");
            break;
        }

        switch (address->kind) {
        case ADDRESS_ABSOLUTE: {
            string_append_fmt(s, "%juUL", address->data.absolute);
            break;
        }
        case ADDRESS_STATIC: {
            char const* base = strgen_fmt(
                "(void*)&%s", mangle_name(address->data.static_.name));

            // XXX: Hack so we can detect static objects of size zero and emit
            // a NULL pointer for them instead of attempting to take the
            // address of a zero-sized object that was never defined.
            bool found = false;
            for (size_t i = 0; i < sbuf_count(context()->static_symbols); ++i) {
                struct address const* const object_address =
                    symbol_xget_address(context()->static_symbols[i]);
                if (object_address->kind != ADDRESS_STATIC) {
                    continue;
                }
                if (object_address->data.static_.name
                    != address->data.static_.name) {
                    continue;
                }

                // Found the object.
                struct type const* const object_type =
                    symbol_xget_type(context()->static_symbols[i]);
                if (object_type->size != 0) {
                    // Object has a non-zero size, and therefore will have a
                    // non-NULL address.
                    break;
                }
                string_append_fmt(s, "(void*)0");
                found = true;
                break;
            }
            if (found) {
                break;
            }

            if (address->data.static_.offset == 0) {
                string_append_fmt(s, "(void*)%s", base);
            }
            else {
                string_append_fmt(
                    s,
                    "(void*)((char*)%s + %ju)",
                    base,
                    address->data.static_.offset);
            }
            break;
        }
        case ADDRESS_LOCAL: {
            UNREACHABLE();
        }
        }

        break;
    }
    case TYPE_ARRAY: {
        sbuf(struct value*) const elements = value->data.array.elements;
        struct value const* const ellipsis = value->data.array.ellipsis;
        size_t const count = (size_t)value->type->data.array.count;
        string_append_fmt(s, "{.elements = {");

        if (value->type->data.array.base->kind == TYPE_BYTE) {
            // Peephole optimization for arrays of bytes. We know that each
            // byte will be encoded as `0xXY`, so four characters per element.
            // We also know that each byte will be followed by either the text
            // ", " or "}}", both of which consist of two characters. So we can
            // resize the string by an additional `countof(array) * (4 + 2)`
            // bytes, and write the rest of the value directly into the string
            // buffer. This optimization comes in handy when generating the
            // value for string arrays created from `embed` statements where
            // multi-MB files would otherwise produce a large number of
            // string reallocations.
            size_t const stride = STR_LITERAL_COUNT("0xXY??");
            size_t const growth = count * stride;

            size_t const index = string_count(s);
            string_resize(s, string_count(s) + growth);

            // clang-format off
            static char const LOOKUP_TABLE[16] = {
                '0', '1', '2', '3', '4', '5', '6', '7',
                '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
            };
            // clang-format on

            char* const start = (char*)string_start(s) + index;
            size_t const elements_count = sbuf_count(elements);
            for (size_t i = 0; i < elements_count; ++i) {
                char* const current = start + i * stride;
                unsigned const byte = elements[i]->data.byte;
                unsigned const hi = (byte >> 4u) & 0x0Fu;
                unsigned const lo = (byte >> 0u) & 0x0Fu;
                current[0] = '0';
                current[1] = 'x';
                current[2] = LOOKUP_TABLE[hi];
                current[3] = LOOKUP_TABLE[lo];
                current[4] = ',';
                current[5] = ' ';
            }
            if (ellipsis != NULL) {
                unsigned const byte = ellipsis->data.byte;
                unsigned const hi = (byte >> 4u) & 0x0Fu;
                unsigned const lo = (byte >> 0u) & 0x0Fu;
                for (size_t i = elements_count; i < count; ++i) {
                    char* const current = start + i * stride;
                    current[0] = '0';
                    current[1] = 'x';
                    current[2] = LOOKUP_TABLE[hi];
                    current[3] = LOOKUP_TABLE[lo];
                    current[4] = ',';
                    current[5] = ' ';
                }
            }
            ((char*)string_start(s) + string_count(s) - 2)[0] = '}';
            ((char*)string_start(s) + string_count(s) - 2)[1] = '}';
            break;
        }

        for (size_t i = 0; i < count; ++i) {
            if (i != 0) {
                string_append_cstr(s, ", ");
            }
            if (i < sbuf_count(elements)) {
                string_append_cstr(s, strgen_value(elements[i]));
            }
            else {
                assert(ellipsis != NULL);
                string_append_cstr(s, strgen_value(ellipsis));
            }
        }
        string_append_cstr(s, "}}");
        break;
    }
    case TYPE_SLICE: {
        string_append_cstr(s, "{.start = ");
        string_append_cstr(s, strgen_value(value->data.slice.start));
        string_append_cstr(s, ", .count = ");
        string_append_cstr(s, strgen_value(value->data.slice.count));
        string_append_cstr(s, "}");
        break;
    }
    case TYPE_STRUCT: {
        sbuf(struct member_variable) const member_variable_defs =
            value->type->data.struct_.member_variables;
        size_t const member_variable_defs_count =
            sbuf_count(member_variable_defs);

        sbuf(struct value*) const member_variable_vals =
            value->data.struct_.member_values;
        size_t const member_variable_vals_count =
            sbuf_count(member_variable_vals);

        assert(member_variable_defs_count == member_variable_vals_count);
        size_t const member_variable_count = member_variable_vals_count;
        (void)member_variable_defs_count;

        string_append_cstr(s, "{");
        size_t members_written = 0;
        for (size_t i = 0; i < member_variable_count; ++i) {
            struct type const* const type = member_variable_defs[i].type;
            if (type->size == 0) {
                continue;
            }

            if (members_written != 0) {
                string_append_cstr(s, ", ");
            }

            if (member_variable_vals[i] != NULL) {
                assert(member_variable_vals[i]->type == type);
                string_append_fmt(
                    s,
                    ".%s = %s",
                    mangle_name(member_variable_defs[i].name),
                    strgen_value(member_variable_vals[i]));
            }
            else {
                string_append_cstr(s, strgen_uninit(type));
            }
            members_written += 1;
        }
        string_append_cstr(s, "}");

        break;
    }
    case TYPE_UNION: {
        string_append_cstr(s, "{");
        if (value->data.union_.member_variable != NULL) {
            assert(value->data.union_.member_value != NULL);
            string_append_fmt(
                s,
                ".%s = %s",
                mangle_name(value->data.union_.member_variable->name),
                strgen_value(value->data.union_.member_value));
        }
        else {
            assert(value->type->size != 0);
            string_append_fmt(s, ".%s = {0}", MANGLE_PREFIX "buffer");
        }
        string_append_cstr(s, "}");
        break;
    }
    case TYPE_ENUM: {
        struct value underlying_value = *value;
        underlying_value.type = value->type->data.enum_.underlying_type;
        string_append_cstr(s, strgen_value(&underlying_value));
        break;
    }
    case TYPE_EXTERN: {
        UNREACHABLE();
    }
    }

    char const* const result = strgen(string_start(s), string_count(s));
    string_del(s);
    return result;
}

static char const*
strgen_uninit(struct type const* type)
{
    assert(type != NULL);

    struct string* const s = string_new(NULL, 0);

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
        string_append_cstr(s, "/* uninit */0");
        break;
    }
    case TYPE_INTEGER: {
        UNREACHABLE();
    }
    case TYPE_F32: {
        string_append_cstr(s, "/* uninit */0.0f");
        break;
    }
    case TYPE_F64: {
        string_append_cstr(s, "/* uninit */0.0");
        break;
    }
    case TYPE_REAL: {
        UNREACHABLE();
    }
    case TYPE_FUNCTION: /* fallthrough */
    case TYPE_POINTER: {
        string_append_cstr(s, "/* uninit */0");
        break;
    }
    case TYPE_ARRAY: /* fallthrough */
    case TYPE_SLICE: /* fallthrough */
    case TYPE_STRUCT: {
        string_append_cstr(s, "{/* uninit */0}");
        break;
    }
    case TYPE_UNION: {
        string_append_cstr(s, "{/* uninit */0}");
        break;
    }
    case TYPE_ENUM: {
        string_append_cstr(s, strgen_uninit(type->data.enum_.underlying_type));
        break;
    }
    case TYPE_EXTERN: {
        UNREACHABLE();
    }
    }

    char const* const result = strgen(string_start(s), string_count(s));
    string_del(s);
    return result;
}

static void
codegen_block(struct block const* block)
{
    assert(block != NULL);

    appendli("{");
    indent_incr();

    // Used to generate a final return statement for C functions that return a
    // value, so that stantic analysis of the C source does not report that
    // control reaches the end of a non-void function.
    bool generate_final_return = false;

    // Declare local variables.
    sbuf(struct symbol_table_element) locals = block->symbol_table->elements;
    for (size_t i = 0; i < sbuf_count(locals); ++i) {
        if (locals[i].symbol->kind != SYMBOL_VARIABLE) {
            continue;
        }

        struct type const* const type = symbol_xget_type(locals[i].symbol);
        struct address const* const address =
            symbol_xget_address(locals[i].symbol);
        assert(address->kind == ADDRESS_LOCAL);

        if (type->size == 0) {
            appendli("// var %s: %s", locals[i].name, type->name);
            appendli(
                "int %s = /* zero-sized local */0;", mangle_address(address));
            continue;
        }

        if (address->data.local.is_parameter) {
            continue;
        }

        if (current_for_range_loop != NULL) {
            // The loop variable is initialized and updated in the loop header,
            // and should *not* be zero-initialized at the start of this block.
            struct symbol const* const loop_variable =
                current_for_range_loop->data.for_range.loop_variable;
            if (locals[i].symbol == loop_variable) {
                continue;
            }
        }

        if (locals[i].name == context()->interned.return_) {
            assert(!generate_final_return);
            generate_final_return = true;
        }

        appendli("// var %s: %s", locals[i].name, type->name);
        appendli(
            "%s %s = %s;",
            mangle_type(type),
            mangle_address(address),
            strgen_uninit(type));
    }

    // Generate statements.
    for (size_t i = 0; i < sbuf_count(block->stmts); ++i) {
        struct stmt const* const stmt = block->stmts[i];
        codegen_stmt(stmt);
    }
    // Generate final defers.
    codegen_defers(block->defer_begin, block->defer_end);
    // Generate final return.
    if (generate_final_return) {
        appendli("return %s;", mangle_name("return"));
    }

    indent_decr();
    appendli("}");
}

static void
codegen_stmt(struct stmt const* stmt)
{
    static struct {
        char const* kind_cstr;
        void (*codegen_fn)(struct stmt const*);
    } const table[] = {
#define TABLE_ENTRY(kind, fn) [kind] = {#kind, fn}
        TABLE_ENTRY(STMT_DEFER, codegen_stmt_defer),
        TABLE_ENTRY(STMT_IF, codegen_stmt_if),
        TABLE_ENTRY(STMT_FOR_RANGE, codegen_stmt_for_range),
        TABLE_ENTRY(STMT_FOR_EXPR, codegen_stmt_for_expr),
        TABLE_ENTRY(STMT_BREAK, codegen_stmt_break),
        TABLE_ENTRY(STMT_CONTINUE, codegen_stmt_continue),
        TABLE_ENTRY(STMT_SWITCH, codegen_stmt_switch),
        TABLE_ENTRY(STMT_RETURN, codegen_stmt_return),
        TABLE_ENTRY(STMT_ASSERT, codegen_stmt_assert),
        TABLE_ENTRY(STMT_ASSIGN, codegen_stmt_assign),
        TABLE_ENTRY(STMT_EXPR, codegen_stmt_expr),
#undef TABLE_ENTRY
    };

    // Generate the statement.
    char const* const cstr = table[stmt->kind].kind_cstr;
    if (debug) {
        appendli_location(stmt->location, "STATEMENT %s", cstr);
    }
    table[stmt->kind].codegen_fn(stmt);
}

static void
codegen_stmt_defer(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_DEFER);
    (void)stmt;

    // No code generation is performed for defer statements as defers are
    // generated as equivalent lowered statements by other codegen functions.
    return;
}

static void
codegen_stmt_if(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_IF);

    sbuf(struct conditional const) const conditionals =
        stmt->data.if_.conditionals;
    for (size_t i = 0; i < sbuf_count(conditionals); ++i) {
        if (conditionals[i].condition != NULL) {
            assert(conditionals[i].condition->type->kind == TYPE_BOOL);
            appendli(
                "%s (%s)",
                i == 0 ? "if" : "else if",
                strgen_rvalue(conditionals[i].condition));
        }
        else {
            appendli("else");
        }
        codegen_block(&conditionals[i].body);
    }
}

static void
codegen_stmt_for_range(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_FOR_RANGE);

    struct stmt const* const save_current_for_range_loop =
        current_for_range_loop;
    current_for_range_loop = stmt;

    struct symbol const* const variable = stmt->data.for_range.loop_variable;
    struct address const* const address = symbol_xget_address(variable);
    assert(address->kind == ADDRESS_LOCAL);
    appendli(
        "for (%s %s = %s; %s < %s; ++%s)",
        mangle_type(symbol_xget_type(variable)),
        mangle_address(address),
        strgen_rvalue(stmt->data.for_range.begin),
        mangle_address(address),
        strgen_rvalue(stmt->data.for_range.end),
        mangle_address(address));
    codegen_block(&stmt->data.for_range.body);
    current_for_range_loop = save_current_for_range_loop;
}

static void
codegen_stmt_for_expr(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_FOR_EXPR);

    appendli("while (%s)", strgen_rvalue(stmt->data.for_expr.expr));
    codegen_block(&stmt->data.for_expr.body);
}

static void
codegen_stmt_break(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_BREAK);

    codegen_defers(stmt->data.break_.defer_begin, stmt->data.break_.defer_end);
    appendli("break;");
}

static void
codegen_stmt_continue(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_CONTINUE);

    codegen_defers(stmt->data.break_.defer_begin, stmt->data.break_.defer_end);
    appendli("continue;");
}

static void
codegen_stmt_switch(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_SWITCH);

    // Special case of a switch statement with a single `else` case.
    bool const is_single_else = sbuf_count(stmt->data.switch_.cases) == 1
        && stmt->data.switch_.cases[0].symbol == NULL;
    if (is_single_else) {
        appendli("{ /* BEGIN SWITCH (single `else` case)*/");
        // Evaluate the condition for side effects.
        appendli(
            "%s %s = %s;",
            mangle_type(stmt->data.switch_.expr->type),
            MANGLE_PREFIX "switch_expr",
            strgen_rvalue(stmt->data.switch_.expr));
        // Generate the block for the single `else` case.
        codegen_block(&stmt->data.switch_.cases[0].body);
        appendli("} /* END SWITCH */");
        return;
    }

    appendli("{ /* BEGIN SWITCH */");
    appendli(
        "%s %s = %s;",
        mangle_type(stmt->data.switch_.expr->type),
        MANGLE_PREFIX "switch_expr",
        strgen_rvalue(stmt->data.switch_.expr));
    for (size_t i = 0; i < sbuf_count(stmt->data.switch_.cases); ++i) {
        if (stmt->data.switch_.cases[i].symbol != NULL) {
            char const* const conditional = i == 0 ? "if" : "else if";
            appendli(
                "%s (%s == %s)",
                conditional,
                MANGLE_PREFIX "switch_expr",
                mangle_symbol(stmt->data.switch_.cases[i].symbol));
        }
        else {
            // The else case must come last.
            assert(i == sbuf_count(stmt->data.switch_.cases) - 1);
            appendli("else");
        }
        codegen_block(&stmt->data.switch_.cases[i].body);
    }
    appendli("} /* END SWITCH */");
}

static void
codegen_stmt_return(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_RETURN);

    struct expr const* const expr = stmt->data.return_.expr;
    if (expr != NULL) {
        if (expr->type->size == 0) {
            // Compute the expression result.
            appendli("%s;", strgen_rvalue(stmt->data.return_.expr));
        }
        else {
            // Compute and store the expression result.
            appendli(
                "%s = %s;",
                mangle_name("return"),
                strgen_rvalue(stmt->data.return_.expr));
        }
    }

    codegen_defers(stmt->data.return_.defer, NULL);

    if (symbol_xget_type(current_function->symbol_return)->size != 0) {
        appendli("return %s;", mangle_name("return"));
    }
    else {
        appendli("return;");
    }
}

static void
codegen_stmt_assert(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_ASSERT);

    appendli("if (!(%s)) {", strgen_rvalue(stmt->data.assert_.expr));
    indent_incr();
    appendli(
        "%s(%s.start);",
        MANGLE_PREFIX "fatal",
        mangle_symbol(stmt->data.assert_.slice_symbol));
    indent_decr();
    appendli("}");
}

static void
codegen_stmt_assign(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_ASSIGN);

    if (stmt->data.assign.lhs->type->size == 0) {
        // The left and right hand sides of the assignment are evaluated, since
        // they might have side effects. However, no C assignment operation is
        // performed since there is nothing to assign.
        appendli(
            "%s; /* zero-sized assignment */; %s;",
            strgen_lvalue(stmt->data.assign.lhs),
            strgen_rvalue(stmt->data.assign.rhs));
        return;
    }

    switch (stmt->data.assign.op) {
    case AOP_ASSIGN: {
        appendli(
            "*(%s) = %s;",
            strgen_lvalue(stmt->data.assign.lhs),
            strgen_rvalue(stmt->data.assign.rhs));
        return;
    }
    case AOP_ADD_ASSIGN: {
        if (type_is_ieee754(stmt->data.assign.lhs->type)) {
            appendli(
                "*(%s) += %s;",
                strgen_lvalue(stmt->data.assign.lhs),
                strgen_rvalue(stmt->data.assign.rhs));
            return;
        }

        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = %s_%s(*%s, %s);}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "add",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs");
        return;
    }
    case AOP_SUB_ASSIGN: {
        if (type_is_ieee754(stmt->data.assign.lhs->type)) {
            appendli(
                "*(%s) -= %s;",
                strgen_lvalue(stmt->data.assign.lhs),
                strgen_rvalue(stmt->data.assign.rhs));
            return;
        }

        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = %s_%s(*%s, %s);}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "sub",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs");
        return;
    }
    case AOP_MUL_ASSIGN: {
        if (type_is_ieee754(stmt->data.assign.lhs->type)) {
            appendli(
                "*(%s) *= %s;",
                strgen_lvalue(stmt->data.assign.lhs),
                strgen_rvalue(stmt->data.assign.rhs));
            return;
        }

        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = %s_%s(*%s, %s);}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "mul",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs");
        return;
    }
    case AOP_DIV_ASSIGN: {
        if (type_is_ieee754(stmt->data.assign.lhs->type)) {
            appendli(
                "*(%s) /= %s;",
                strgen_lvalue(stmt->data.assign.lhs),
                strgen_rvalue(stmt->data.assign.rhs));
            return;
        }

        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = %s_%s(*%s, %s);}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "div",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs");
        return;
    }
    case AOP_REM_ASSIGN: {
        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = %s_%s(*%s, %s);}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rem",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs");
        return;
    }
    case AOP_ADD_WRAPPING_ASSIGN: {
        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = %s_%s(*%s, %s);}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "add_wrapping",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs");
        return;
    }
    case AOP_SUB_WRAPPING_ASSIGN: {
        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = %s_%s(*%s, %s);}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "sub_wrapping",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs");
        return;
    }
    case AOP_MUL_WRAPPING_ASSIGN: {
        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = %s_%s(*%s, %s);}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "mul_wrapping",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs");
        return;
    }
    case AOP_SHL_ASSIGN: {
        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = %s < sizeof(%s)*8 ? (%s)((%s)*%s << %s) : (%s)0;}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs",
            mangle_type(stmt->data.assign.lhs->type),
            mangle_type(stmt->data.assign.lhs->type),
            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs",

            mangle_type(stmt->data.assign.lhs->type));
        return;
    }
    case AOP_SHR_ASSIGN: {
        char const* const overshift =
            type_is_sinteger(stmt->data.assign.lhs->type)
            ? strgen_fmt("((*%s < 0) ? -1 : 0)", MANGLE_PREFIX "lhs")
            : "0";
        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = %s < sizeof(%s)*8 ? (*%s >> %s) : (%s)%s;}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs",
            mangle_type(stmt->data.assign.lhs->type),
            overshift);
        return;
    }
    case AOP_BITOR_ASSIGN: {
        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = (*%s | %s);}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs");
        return;
    }
    case AOP_BITXOR_ASSIGN: {
        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = (*%s ^ %s);}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs");
        return;
    }
    case AOP_BITAND_ASSIGN: {
        appendli(
            "{%s* %s = %s; %s %s = %s; *%s = (*%s & %s);}",
            mangle_type(stmt->data.assign.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_lvalue(stmt->data.assign.lhs),

            mangle_type(stmt->data.assign.rhs->type),
            MANGLE_PREFIX "rhs",
            strgen_rvalue(stmt->data.assign.rhs),

            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "rhs");
        return;
    }
    }

    UNREACHABLE();
}

static void
codegen_stmt_expr(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_EXPR);

    appendli("%s;", strgen_rvalue(stmt->data.expr));
}

static char const*
strgen_rvalue(struct expr const* expr)
{
    assert(expr != NULL);

    // clang-format off
    static struct {
        char const* kind_cstr;
        char const* (*function)(struct expr const*);
    } const table[] = {
#define TABLE_ENTRY(kind, fn) [kind] = {#kind, fn}
        TABLE_ENTRY(EXPR_SYMBOL, strgen_rvalue_symbol),
        TABLE_ENTRY(EXPR_VALUE, strgen_rvalue_value),
        TABLE_ENTRY(EXPR_BYTES, strgen_rvalue_bytes),
        TABLE_ENTRY(EXPR_ARRAY_LIST, strgen_rvalue_array_list),
        TABLE_ENTRY(EXPR_SLICE_LIST, strgen_rvalue_slice_list),
        TABLE_ENTRY(EXPR_SLICE, strgen_rvalue_slice),
        TABLE_ENTRY(EXPR_INIT, strgen_rvalue_init),
        TABLE_ENTRY(EXPR_CAST, strgen_rvalue_cast),
        TABLE_ENTRY(EXPR_CALL, strgen_rvalue_call),
        TABLE_ENTRY(EXPR_ACCESS_INDEX, strgen_rvalue_access_index),
        TABLE_ENTRY(EXPR_ACCESS_SLICE, strgen_rvalue_access_slice),
        TABLE_ENTRY(EXPR_ACCESS_MEMBER_VARIABLE, strgen_rvalue_access_member_variable),
        TABLE_ENTRY(EXPR_SIZEOF, strgen_rvalue_sizeof),
        TABLE_ENTRY(EXPR_ALIGNOF, strgen_rvalue_alignof),
        TABLE_ENTRY(EXPR_UNARY, strgen_rvalue_unary),
        TABLE_ENTRY(EXPR_BINARY, strgen_rvalue_binary),
#undef TABLE_ENTRY
    };
    // clang-format on

    char const* const cstr = table[expr->kind].kind_cstr;
    if (debug) {
        appendli_location(expr->location, "RVALUE EXPRESSION %s", cstr);
    }
    return table[expr->kind].function(expr);
}

static char const*
strgen_rvalue_symbol(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SYMBOL);

    if (expr->type->size == 0) {
        return strgen_cstr("/* zero-sized symbol */(0)");
    }

    return mangle_symbol(expr->data.symbol);
}

static char const*
strgen_rvalue_value(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_VALUE);

    if (type_is_compound(expr->type)) {
        return strgen_fmt(
            "((%s)%s)",
            mangle_type(expr->data.value->type),
            strgen_value(expr->data.value));
    }

    return strgen_value(expr->data.value);
}

static char const*
strgen_rvalue_bytes(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BYTES);
    assert(expr->type->kind == TYPE_SLICE);

    return mangle_symbol(expr->data.bytes.slice_symbol);
}

static char const*
strgen_rvalue_array_list(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ARRAY_LIST);
    assert(expr->type->kind == TYPE_ARRAY);

    struct type const* const element_type = expr->type->data.array.base;

    sbuf(struct expr const* const) const elements =
        expr->data.array_list.elements;

    struct string* const s = string_new_cstr("({");
    char const* const typename =
        element_type->size != 0 ? mangle_type(element_type) : "int";

    for (size_t i = 0; i < sbuf_count(elements); ++i) {
        assert(elements[i]->type == element_type);
        char const* const local = strgen_fmt(MANGLE_PREFIX "element_%zu", i);
        char const* const initname = mangle_name(local);
        char const* const valuestr = strgen_rvalue(elements[i]);

        string_append_fmt(s, "%s %s = %s; ", typename, initname, valuestr);
    }

    uintmax_t const count = expr->type->data.array.count;
    struct expr const* const ellipsis = expr->data.array_list.ellipsis;
    assert(sbuf_count(elements) == count || ellipsis != NULL);
    char const* ellipsis_valuestr = NULL;
    if (ellipsis != NULL) {
        assert(ellipsis->type == element_type);
        char const* const initname = MANGLE_PREFIX "ellipsis";
        ellipsis_valuestr = strgen_rvalue(ellipsis);

        string_append_fmt(
            s, "%s %s = %s; ", typename, initname, ellipsis_valuestr);
    }

    if (expr->type->size == 0) {
        string_append_fmt(s, "/* %s */0;", expr->type->name);
    }
    else {
        string_append_fmt(s, "(%s){", mangle_type(expr->type));
        for (size_t i = 0; i < sbuf_count(elements); ++i) {
            assert(elements[i]->type == element_type);
            if (i != 0) {
                string_append_cstr(s, ", ");
            }
            char const* const local =
                strgen_fmt(MANGLE_PREFIX "element_%zu", i);
            char const* const initname = mangle_name(local);
            string_append_cstr(s, initname);
        }
        for (size_t i = sbuf_count(elements); i < count; ++i) {
            assert(ellipsis_valuestr != NULL);
            if (i != 0) {
                string_append_cstr(s, ", ");
            }
            char const* const initname = MANGLE_PREFIX "ellipsis";
            string_append_cstr(s, initname);
        }
        string_append_cstr(s, "};");
    }

    string_append_cstr(s, "})");

    char const* const result = strgen(string_start(s), string_count(s));
    string_del(s);
    return result;
}

static char const*
strgen_rvalue_slice_list(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SLICE_LIST);
    assert(expr->type->kind == TYPE_SLICE);

    struct type const* const element_type = expr->type->data.slice.base;
    sbuf(struct expr const* const) const elements =
        expr->data.slice_list.elements;

    if (element_type->size == 0) {
        struct string* const element_exprs = string_new(NULL, 0);
        for (size_t i = 0; i < sbuf_count(elements); ++i) {
            assert(elements[i]->type == element_type);
            if (i != 0) {
                string_append_cstr(element_exprs, "; ");
            }
            string_append_cstr(element_exprs, strgen_rvalue(elements[i]));
        }

        char const* const output = strgen_fmt(
            "({%s; /* slice of zero-sized type */(%s){.start = 0, .count = %zu};})",
            string_start(element_exprs),
            mangle_type(expr->type),
            sbuf_count(elements));

        string_del(element_exprs);
        return output;
    }

    struct symbol const* const array_symbol =
        expr->data.slice_list.array_symbol;
    struct type const* const array_type = symbol_xget_type(array_symbol);
    uintmax_t const array_size = array_type->size;

    struct string* const s = string_new_cstr("({");
    for (size_t i = 0; i < sbuf_count(elements); ++i) {
        assert(elements[i]->type == element_type);
        if (array_size == 0) {
            string_append_fmt(s, "%s; ", strgen_rvalue(elements[i]));
            continue;
        }

        string_append_fmt(
            s,
            "%s.elements[%zu] = %s; ",
            mangle_symbol(array_symbol),
            i,
            strgen_rvalue(elements[i]));
    }

    char const* const start = array_size != 0
        ? strgen_fmt("%s.elements", mangle_symbol(array_symbol))
        : "/* zero-sized array */0";
    string_append_fmt(
        s,
        "(%s){.start = %s, .count = %zu};})",
        mangle_type(expr->type),
        start,
        sbuf_count(elements));

    char const* const output = strgen(string_start(s), string_count(s));
    string_del(s);
    return output;
}

static char const*
strgen_rvalue_slice(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SLICE);
    assert(expr->type->kind == TYPE_SLICE);

    return strgen_fmt(
        "(%s){.start = %s, .count = %s}",
        mangle_type(expr->type),
        strgen_rvalue(expr->data.slice.start),
        strgen_rvalue(expr->data.slice.count));
}

static char const*
strgen_rvalue_init_struct(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_INIT);
    assert(expr->type->kind == TYPE_STRUCT);

    sbuf(struct member_variable) const member_variable_defs =
        expr->type->data.struct_.member_variables;
    sbuf(struct member_variable_initializer const) initializers =
        expr->data.init.initializers;
    assert(sbuf_count(member_variable_defs) == sbuf_count(initializers));

    struct string* const s = string_new_cstr("({");
    for (size_t i = 0; i < sbuf_count(initializers); ++i) {
        char const* const local = strgen_fmt(
            MANGLE_PREFIX "initializer_%s", initializers[i].variable->name);
        char const* const initname = mangle_name(local);
        char const* const typename =
            mangle_type(initializers[i].variable->type);

        if (initializers[i].variable->type->size == 0) {
            char const* const valuestr = initializers[i].expr != NULL
                ? strgen_rvalue(initializers[i].expr)
                : "/* uninit */0";
            string_append_fmt(
                s,
                "int %s = /* zero-sized member */({%s; 0;}); ",
                initname,
                valuestr);
            continue;
        }

        char const* const valuestr = initializers[i].expr != NULL
            ? strgen_rvalue(initializers[i].expr)
            : strgen_uninit(initializers[i].variable->type);
        string_append_fmt(s, "%s %s = %s; ", typename, initname, valuestr);
    }

    if (expr->type->size == 0) {
        string_append_cstr(s, "/* zero-sized struct */0;");
        goto done;
    }

    string_append_fmt(
        s,
        "%s %s = (%s)%s; ",
        mangle_type(expr->type),
        MANGLE_PREFIX "result",
        mangle_type(expr->type),
        strgen_uninit(expr->type));
    for (size_t i = 0; i < sbuf_count(member_variable_defs); ++i) {
        if (member_variable_defs[i].type->size == 0) {
            continue;
        }
        char const* const local = strgen_fmt(
            MANGLE_PREFIX "initializer_%s", member_variable_defs[i].name);
        string_append_fmt(
            s,
            "%s.%s = %s; ",
            MANGLE_PREFIX "result",
            mangle_name(member_variable_defs[i].name),
            mangle_name(local));
    }
    string_append_fmt(s, "%s;", MANGLE_PREFIX "result");

done:
    string_append_cstr(s, "})");
    char const* const result = strgen(string_start(s), string_count(s));
    string_del(s);
    return result;
}

static char const*
strgen_rvalue_init_union(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_INIT);
    assert(expr->type->kind == TYPE_UNION);

    struct string* const s = string_new_cstr("({");

    if (expr->type->size == 0) {
        string_append_cstr(s, "/* zero-sized union */0;");
        goto done;
    }

    sbuf(struct member_variable_initializer const) initializers =
        expr->data.init.initializers;
    assert(sbuf_count(initializers) == 1);
    struct member_variable_initializer const* const initializer =
        &initializers[0];

    string_append_fmt(
        s,
        "%s %s = (%s)%s; ",
        mangle_type(expr->type),
        MANGLE_PREFIX "result",
        mangle_type(expr->type),
        strgen_uninit(expr->type));
    if (initializer->variable->type->size != 0) {
        string_append_fmt(
            s,
            "%s.%s = %s; ",
            MANGLE_PREFIX "result",
            mangle_name(initializer->variable->name),
            strgen_rvalue(initializer->expr));
    }
    string_append_fmt(s, "%s;", MANGLE_PREFIX "result");

done:
    string_append_cstr(s, "})");
    char const* const result = strgen(string_start(s), string_count(s));
    string_del(s);
    return result;
}

static char const*
strgen_rvalue_init(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_INIT);
    assert(expr->type->kind == TYPE_STRUCT || expr->type->kind == TYPE_UNION);

    if (expr->type->kind == TYPE_STRUCT) {
        return strgen_rvalue_init_struct(expr);
    }
    if (expr->type->kind == TYPE_UNION) {
        return strgen_rvalue_init_union(expr);
    }

    UNREACHABLE();
    return NULL;
}

static char const*
strgen_rvalue_cast(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_CAST);

    if (type_is_integer(expr->type)
        && type_is_ieee754(expr->data.cast.expr->type)) {
        return strgen_fmt(
            MANGLE_PREFIX "cast_%s_to_%s(%s)",
            mangle_type(expr->data.cast.expr->type),
            mangle_type(expr->type),
            strgen_rvalue(expr->data.cast.expr));
    }

    return strgen_fmt(
        "(%s)%s", mangle_type(expr->type), strgen_rvalue(expr->data.cast.expr));
}

static char const*
strgen_rvalue_call(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_CALL);

    struct expr const* const function = expr->data.call.function;
    sbuf(struct expr const* const) const arguments = expr->data.call.arguments;

    struct string* const s = string_new_cstr("({");

    for (size_t i = 0; i < sbuf_count(arguments); ++i) {
        char const* const local =
            strgen_fmt(MANGLE_PREFIX "argument_%zu", i + 1);
        char const* const initname = mangle_name(local);
        char const* const typename = arguments[i]->type->size != 0
            ? mangle_type(arguments[i]->type)
            : "int";
        char const* const valuestr = strgen_rvalue(arguments[i]);

        string_append_fmt(s, "%s %s = %s; ", typename, initname, valuestr);
    }

    string_append_fmt(s, "%s(", strgen_rvalue(function));
    size_t arguments_written = 0;
    for (size_t i = 0; i < sbuf_count(arguments); ++i) {
        if (arguments[i]->type->size == 0) {
            continue;
        }
        if (arguments_written != 0) {
            string_append_cstr(s, ", ");
        }

        char const* const local =
            strgen_fmt(MANGLE_PREFIX "argument_%zu", i + 1);
        char const* const initname = mangle_name(local);
        string_append_cstr(s, initname);

        arguments_written += 1;
    }
    assert(function->type->kind == TYPE_FUNCTION);
    if (function->type->data.function.return_type->size == 0) {
        string_append_cstr(s, "), /* zero-sized return */0;");
    }
    else {
        string_append_cstr(s, ");");
    }

    string_append_cstr(s, "})");

    char const* const result = strgen(string_start(s), string_count(s));
    string_del(s);
    return result;
}

static char const*
strgen_rvalue_access_index(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);

    bool const lhs_is_zero_sized = expr->data.access_index.lhs->type->size == 0;

    // According to the C standard, performing pointer arithmetic on a NULL
    // pointer has undefined behavior. Pointer addition is manually performed
    // with uintptr_t to avoid this undefined behavior.

    if (expr->data.access_index.lhs->type->kind == TYPE_ARRAY) {
        uintmax_t const base_size =
            expr->data.access_index.lhs->type->data.array.base->size;

        char const* const lhs_type = lhs_is_zero_sized
            ? "int"
            : mangle_type(expr->data.access_index.lhs->type);

        char const* const elements = lhs_is_zero_sized
            ? strgen_fmt("((%s*)0)", mangle_type(expr->type))
            : strgen_fmt("%s.elements", MANGLE_PREFIX "lhs");

        char const* const result = base_size == 0
            ? "/* index array of zero-sized type */0"
            : strgen_fmt(
                  "*(%s*)((uintptr_t)%s + (%s * %ju))",
                  mangle_type(expr->type),
                  elements,
                  MANGLE_PREFIX "idx",
                  base_size);

        return strgen_fmt(
            "({%s %s = %s; %s %s = %s; if (%s >= %ju){%s();}; %s;})",
            lhs_type,
            MANGLE_PREFIX "lhs",
            strgen_rvalue(expr->data.access_index.lhs),

            mangle_type(expr->data.access_index.idx->type),
            MANGLE_PREFIX "idx",
            strgen_rvalue(expr->data.access_index.idx),

            MANGLE_PREFIX "idx",
            expr->data.access_index.lhs->type->data.array.count,
            MANGLE_PREFIX "fatal_index_out_of_bounds",

            result);
    }

    if (expr->data.access_index.lhs->type->kind == TYPE_SLICE) {
        assert(!lhs_is_zero_sized);
        uintmax_t const base_size =
            expr->data.access_index.lhs->type->data.slice.base->size;

        char const* const result = base_size == 0
            ? "/* index slice of zero-sized type */0"
            : strgen_fmt(
                  "*(%s*)((uintptr_t)%s.start + (%s * %ju))",
                  mangle_type(expr->type),
                  MANGLE_PREFIX "lhs",
                  MANGLE_PREFIX "idx",
                  base_size);

        return strgen_fmt(
            // clang-format off
            "({%s %s = %s; %s %s = %s; if (%s >= %s.count){%s();}; %s;})",
            // clang-format on
            mangle_type(expr->data.access_index.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_rvalue(expr->data.access_index.lhs),

            mangle_type(expr->data.access_index.idx->type),
            MANGLE_PREFIX "idx",
            strgen_rvalue(expr->data.access_index.idx),

            MANGLE_PREFIX "idx",
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "fatal_index_out_of_bounds",

            result);
    }

    UNREACHABLE();
    return NULL;
}

static char const*
strgen_rvalue_access_slice(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_SLICE);

    if (expr->data.access_slice.lhs->type->kind == TYPE_ARRAY) {
        return strgen_rvalue_access_slice_lhs_array(expr);
    }

    if (expr->data.access_slice.lhs->type->kind == TYPE_SLICE) {
        return strgen_rvalue_access_slice_lhs_slice(expr);
    }

    UNREACHABLE();
    return NULL;
}

static char const*
strgen_rvalue_access_slice_lhs_array(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_SLICE);
    assert(expr->data.access_slice.lhs->type->kind == TYPE_ARRAY);
    assert(expr_is_lvalue(expr->data.access_slice.lhs));

    char const* const lexpr = strgen_lvalue(expr->data.access_slice.lhs);

    char const* const btype = mangle_name("usize");
    char const* const bname = MANGLE_PREFIX "b";
    char const* const bexpr = strgen_rvalue(expr->data.access_slice.begin);

    char const* const etype = mangle_name("usize");
    char const* const ename = MANGLE_PREFIX "e";
    char const* const eexpr = strgen_rvalue(expr->data.access_slice.end);

    char const* const tname = mangle_type(expr->type);

    bool const lhs_is_zero_sized = expr->data.access_slice.lhs->type->size == 0;
    uintmax_t const base_size = expr->type->data.slice.base->size;

    // According to the C standard, performing pointer arithmetic on a NULL
    // pointer has undefined behavior. Pointer addition is manually performed
    // with uintptr_t to avoid this undefined behavior.

    char const* start = NULL;
    if (lhs_is_zero_sized) {
        start = strgen_fmt(
            "(%s*)((uintptr_t)%s * %ju)",
            mangle_type(expr->type->data.slice.base),
            bname,
            base_size);
    }
    else {
        start = strgen_fmt(
            "(%s*)((uintptr_t)(%s)->elements + ((uintptr_t)%s * %ju))",
            mangle_type(expr->type->data.slice.base),
            lexpr,
            bname,
            base_size);
    }
    char const* const count = strgen_fmt("%s - %s", ename, bname);
    return strgen_fmt(
        "({%s %s = %s; %s %s = %s; if ((%s > %s) || (%s > %ju) || (%s > %ju)){%s();}; (%s){.start = %s, .count = %s};})",
        btype,
        bname,
        bexpr,

        etype,
        ename,
        eexpr,

        bname,
        ename,
        bname,
        expr->data.access_slice.lhs->type->data.array.count,
        ename,
        expr->data.access_slice.lhs->type->data.array.count,
        MANGLE_PREFIX "fatal_index_out_of_bounds",

        tname,
        start,
        count);
}

static char const*
strgen_rvalue_access_slice_lhs_slice(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_SLICE);
    assert(expr->data.access_slice.lhs->type->kind == TYPE_SLICE);

    char const* const ltype = mangle_type(expr->data.access_slice.lhs->type);
    char const* const lname = MANGLE_PREFIX "lhs";
    char const* const lexpr = strgen_rvalue(expr->data.access_slice.lhs);

    char const* const btype = mangle_name("usize");
    char const* const bname = MANGLE_PREFIX "b";
    char const* const bexpr = strgen_rvalue(expr->data.access_slice.begin);

    char const* const etype = mangle_name("usize");
    char const* const ename = MANGLE_PREFIX "e";
    char const* const eexpr = strgen_rvalue(expr->data.access_slice.end);

    char const* const tname = mangle_type(expr->type);

    assert(expr->data.access_slice.lhs->type->size != 0);
    uintmax_t const base_size = expr->type->data.slice.base->size;

    // According to the C standard, performing pointer arithmetic on a NULL
    // pointer has undefined behavior. Pointer addition is manually performed
    // with uintptr_t to avoid this undefined behavior.

    char const* const start = strgen_fmt(
        "(%s*)((uintptr_t)%s.start + ((uintptr_t)%s * %ju))",
        mangle_type(expr->type->data.slice.base),
        lname,
        bname,
        base_size);
    char const* const count = strgen_fmt("%s - %s", ename, bname);
    return strgen_fmt(
        "({%s %s = %s; %s %s = %s; %s %s = %s; if ((%s > %s) || (%s > %s.count) || (%s > %s.count)){%s();}; (%s){.start = %s, .count = %s};})",
        ltype,
        lname,
        lexpr,

        btype,
        bname,
        bexpr,

        etype,
        ename,
        eexpr,

        bname,
        ename,
        bname,
        lname,
        ename,
        lname,
        MANGLE_PREFIX "fatal_index_out_of_bounds",

        tname,
        start,
        count);
}

static char const*
strgen_rvalue_access_member_variable(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_MEMBER_VARIABLE);
    assert(
        expr->data.access_member_variable.lhs->type->kind == TYPE_STRUCT
        || expr->data.access_member_variable.lhs->type->kind == TYPE_UNION);

    if (expr->type->size == 0) {
        return strgen_fmt(
            "({/* zero-sized member */(%s); 0;})",
            strgen_rvalue(expr->data.access_member_variable.lhs));
    }

    return strgen_fmt(
        "(%s).%s",
        strgen_rvalue(expr->data.access_member_variable.lhs),
        mangle_name(expr->data.access_member_variable.member_variable->name));
}

static char const*
strgen_rvalue_sizeof(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SIZEOF);

    return strgen_fmt("%ju", expr->data.sizeof_.rhs->size);
}

static char const*
strgen_rvalue_alignof(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ALIGNOF);

    return strgen_fmt("%ju", expr->data.alignof_.rhs->align);
}

static char const*
strgen_rvalue_unary(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);

    switch (expr->data.unary.op) {
    case UOP_NOT: {
        return strgen_rvalue_unary_not(expr);
    }
    case UOP_POS: {
        return strgen_rvalue_unary_pos(expr);
    }
    case UOP_NEG: {
        return strgen_rvalue_unary_neg(expr);
    }
    case UOP_NEG_WRAPPING: {
        return strgen_rvalue_unary_neg_wrapping(expr);
    }
    case UOP_BITNOT: {
        return strgen_rvalue_unary_bitnot(expr);
    }
    case UOP_DEREFERENCE: {
        return strgen_rvalue_unary_dereference(expr);
    }
    case UOP_ADDRESSOF_LVALUE: {
        return strgen_rvalue_unary_addressof_lvalue(expr);
    }
    case UOP_ADDRESSOF_RVALUE: {
        return strgen_rvalue_unary_addressof_rvalue(expr);
    }
    case UOP_STARTOF: {
        return strgen_rvalue_unary_startof(expr);
    }
    case UOP_COUNTOF: {
        return strgen_rvalue_unary_countof(expr);
    }
    }

    UNREACHABLE();
    return NULL;
}

static char const*
strgen_rvalue_unary_not(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_NOT);

    return strgen_fmt("(!%s)", strgen_rvalue(expr->data.unary.rhs));
}

static char const*
strgen_rvalue_unary_pos(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_POS);

    return strgen_fmt("(+%s)", strgen_rvalue(expr->data.unary.rhs));
}

static char const*
strgen_rvalue_unary_neg(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_NEG);

    if (type_is_ieee754(expr->type)) {
        return strgen_fmt("(-%s)", strgen_rvalue(expr->data.unary.rhs));
    }

    assert(type_is_sinteger(expr->data.unary.rhs->type));
    return strgen_fmt(
        "({%s %s = %s; if (%s == ((%s)(LLONG_MIN >> ((sizeof(long long) - sizeof(%s))*8)))){%s();}; -(%s);})",
        mangle_type(expr->data.unary.rhs->type),
        MANGLE_PREFIX "rhs",
        strgen_rvalue(expr->data.unary.rhs),

        MANGLE_PREFIX "rhs",
        mangle_type(expr->data.unary.rhs->type),
        mangle_type(expr->data.unary.rhs->type),
        MANGLE_PREFIX "fatal_out_of_range",

        MANGLE_PREFIX "rhs");
}

static char const*
strgen_rvalue_unary_neg_wrapping(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_NEG_WRAPPING);
    assert(type_is_sinteger(expr->data.unary.rhs->type));

    // Negating a negative number that can not be represented as a positive
    // number (e.g. T::MIN for signed integer type T) is undefined behavior in
    // C, so implement negation "manually" using two's complement negation.
    return strgen_fmt(
        "({%s %s = ~%s; %s_%s(%s, (%s)1);})",
        mangle_type(expr->type),
        MANGLE_PREFIX "result",
        strgen_rvalue(expr->data.unary.rhs),

        MANGLE_PREFIX "add_wrapping",
        mangle_type(expr->type),
        MANGLE_PREFIX "result",
        mangle_type(expr->type));
}

static char const*
strgen_rvalue_unary_bitnot(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_BITNOT);

    // An additional cast of the resulting `~rhs` expression is required in
    // order to prevent warnings resulting from integral promotions.
    return strgen_fmt(
        "((%s)~%s)",
        mangle_type(expr->type),
        strgen_rvalue(expr->data.unary.rhs));
}

static char const*
strgen_rvalue_unary_dereference(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_DEREFERENCE);
    assert(expr->data.unary.rhs->type->kind == TYPE_POINTER);

    if (expr->type->size == 0) {
        return strgen_fmt(
            "({/* dereference pointer to zero-sized type */(%s); 0;})",
            strgen_rvalue(expr->data.unary.rhs));
    }

    // A NULL pointer check is added to avoid undefined behavior from C
    // compilers assuming that NULL pointers are never dereferenced in
    // well-formed code.
    return strgen_fmt(
        "({%s %s = %s; if (%s == 0){%s();}; *%s;})",
        mangle_type(expr->data.unary.rhs->type),
        MANGLE_PREFIX "ptr",
        strgen_rvalue(expr->data.unary.rhs),

        MANGLE_PREFIX "ptr",
        MANGLE_PREFIX "fatal_null_pointer_dereference",

        MANGLE_PREFIX "ptr");
}

static char const*
strgen_rvalue_unary_addressof_lvalue(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_ADDRESSOF_LVALUE);

    return strgen_lvalue(expr->data.unary.rhs);
}

static char const*
strgen_rvalue_unary_addressof_rvalue(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_ADDRESSOF_RVALUE);
    assert(expr->data.unary.address->kind == ADDRESS_LOCAL);

    return strgen_fmt(
        "({%s = %s; &%s;})",
        mangle_address(expr->data.unary.address),
        strgen_rvalue(expr->data.unary.rhs),
        mangle_address(expr->data.unary.address));
}

static char const*
strgen_rvalue_unary_startof(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_STARTOF);
    assert(expr->data.unary.rhs->type->kind == TYPE_SLICE);

    return strgen_fmt("(%s).start", strgen_rvalue(expr->data.unary.rhs));
}

static char const*
strgen_rvalue_unary_countof(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_COUNTOF);

    if (expr->data.unary.rhs->type->kind == TYPE_ARRAY) {
        // If possible, evaluate the left hand side of the expression as an
        // lvalue so that we do not place the entire contents of the array on
        // the stack.
        if (expr_is_lvalue(expr->data.unary.rhs)) {
            return strgen_fmt(
                "({%s; %ju;})",
                strgen_lvalue(expr->data.unary.rhs),
                expr->data.unary.rhs->type->data.array.count);
        }

        return strgen_fmt(
            "({%s; %ju;})",
            strgen_rvalue(expr->data.unary.rhs),
            expr->data.unary.rhs->type->data.array.count);
    }

    if (expr->data.unary.rhs->type->kind == TYPE_SLICE) {
        return strgen_fmt("(%s).count", strgen_rvalue(expr->data.unary.rhs));
    }

    UNREACHABLE();
    return NULL;
}

static char const*
strgen_rvalue_binary(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);

    switch (expr->data.binary.op) {
    case BOP_OR: {
        return strgen_rvalue_binary_or(expr);
    }
    case BOP_AND: {
        return strgen_rvalue_binary_and(expr);
    }
    case BOP_SHL: {
        return strgen_rvalue_binary_shl(expr);
    }
    case BOP_SHR: {
        return strgen_rvalue_binary_shr(expr);
    }
    case BOP_EQ: {
        return strgen_rvalue_binary_eq(expr);
    }
    case BOP_NE: {
        return strgen_rvalue_binary_ne(expr);
    }
    case BOP_LE: {
        return strgen_rvalue_binary_le(expr);
    }
    case BOP_LT: {
        return strgen_rvalue_binary_lt(expr);
    }
    case BOP_GE: {
        return strgen_rvalue_binary_ge(expr);
    }
    case BOP_GT: {
        return strgen_rvalue_binary_gt(expr);
    }
    case BOP_ADD: {
        return strgen_rvalue_binary_add(expr);
    }
    case BOP_ADD_WRAPPING: {
        return strgen_rvalue_binary_add_wrapping(expr);
    }
    case BOP_SUB: {
        return strgen_rvalue_binary_sub(expr);
    }
    case BOP_SUB_WRAPPING: {
        return strgen_rvalue_binary_sub_wrapping(expr);
    }
    case BOP_MUL: {
        return strgen_rvalue_binary_mul(expr);
    }
    case BOP_MUL_WRAPPING: {
        return strgen_rvalue_binary_mul_wrapping(expr);
    }
    case BOP_DIV: {
        return strgen_rvalue_binary_div(expr);
    }
    case BOP_REM: {
        return strgen_rvalue_binary_rem(expr);
    }
    case BOP_BITOR: {
        return strgen_rvalue_binary_bitor(expr);
    }
    case BOP_BITXOR: {
        return strgen_rvalue_binary_bitxor(expr);
    }
    case BOP_BITAND: {
        return strgen_rvalue_binary_bitand(expr);
    }
    }

    UNREACHABLE();
    return NULL;
}

static char const*
strgen_rvalue_binary_or(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_OR);
    assert(expr->data.binary.lhs->type->kind == TYPE_BOOL);
    assert(expr->data.binary.rhs->type->kind == TYPE_BOOL);

    return strgen_fmt(
        "(%s || %s)",
        strgen_rvalue(expr->data.binary.lhs),
        strgen_rvalue(expr->data.binary.rhs));
}

static char const*
strgen_rvalue_binary_and(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_AND);
    assert(expr->data.binary.lhs->type->kind == TYPE_BOOL);
    assert(expr->data.binary.rhs->type->kind == TYPE_BOOL);

    return strgen_fmt(
        "(%s && %s)",
        strgen_rvalue(expr->data.binary.lhs),
        strgen_rvalue(expr->data.binary.rhs));
}

static char const*
strgen_rvalue_binary_shl(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_SHL);
    assert(type_is_integer(expr->data.binary.lhs->type));
    assert(expr->data.binary.rhs->type->kind == TYPE_USIZE);

    // Left shift of a negative number is undefined behavior in C, even though
    // the behavior is well defined in Sunder (-1s8 << 1u == -2s8). Always
    // perform left shifts using unsigned values and then cast back to the
    // correct type to avoid undefined behavior.
    return strgen_fmt(
        "({%s %s = %s; %s %s = %s; %s < sizeof(%s)*8 ? (%s)((%s)%s << %s) : (%s)0;})",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        MANGLE_PREFIX "rhs",
        strgen_rvalue(expr->data.binary.rhs),

        MANGLE_PREFIX "rhs",
        mangle_type(expr->data.binary.lhs->type),
        mangle_type(expr->data.binary.lhs->type),
        mangle_type(expr->data.binary.rhs->type),
        MANGLE_PREFIX "lhs",
        MANGLE_PREFIX "rhs",
        mangle_type(expr->data.binary.lhs->type));
}

static char const*
strgen_rvalue_binary_shr(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_SHR);
    assert(type_is_integer(expr->data.binary.lhs->type));
    assert(expr->data.binary.rhs->type->kind == TYPE_USIZE);

    char const* const overshift = type_is_sinteger(expr->data.binary.lhs->type)
        ? strgen_fmt("((%s < 0) ? -1 : 0)", MANGLE_PREFIX "lhs")
        : "0";
    return strgen_fmt(
        "({%s %s = %s; %s %s = %s; %s < sizeof(%s)*8 ? (%s >> %s) : (%s)%s;})",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        MANGLE_PREFIX "rhs",
        strgen_rvalue(expr->data.binary.rhs),

        MANGLE_PREFIX "rhs",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        MANGLE_PREFIX "rhs",
        mangle_type(expr->data.binary.lhs->type),
        overshift);
}

static char const*
strgen_rvalue_binary_eq(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_EQ);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    return strgen_fmt(
        "(%s == %s)",
        strgen_rvalue(expr->data.binary.lhs),
        strgen_rvalue(expr->data.binary.rhs));
}

static char const*
strgen_rvalue_binary_ne(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_NE);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    return strgen_fmt(
        "(%s != %s)",
        strgen_rvalue(expr->data.binary.lhs),
        strgen_rvalue(expr->data.binary.rhs));
}

static char const*
strgen_rvalue_binary_le(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_LE);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    return strgen_fmt(
        "(%s <= %s)",
        strgen_rvalue(expr->data.binary.lhs),
        strgen_rvalue(expr->data.binary.rhs));
}

static char const*
strgen_rvalue_binary_lt(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_LT);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    return strgen_fmt(
        "(%s < %s)",
        strgen_rvalue(expr->data.binary.lhs),
        strgen_rvalue(expr->data.binary.rhs));
}

static char const*
strgen_rvalue_binary_ge(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_GE);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    return strgen_fmt(
        "(%s >= %s)",
        strgen_rvalue(expr->data.binary.lhs),
        strgen_rvalue(expr->data.binary.rhs));
}

static char const*
strgen_rvalue_binary_gt(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_GT);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    return strgen_fmt(
        "(%s > %s)",
        strgen_rvalue(expr->data.binary.lhs),
        strgen_rvalue(expr->data.binary.rhs));
}

static char const*
strgen_rvalue_binary_add(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_ADD);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    if (type_is_ieee754(expr->type)) {
        return strgen_fmt(
            "(%s + %s)",
            strgen_rvalue(expr->data.binary.lhs),
            strgen_rvalue(expr->data.binary.rhs));
    }

    assert(type_is_integer(expr->type) && expr->type->size != SIZEOF_UNSIZED);
    return strgen_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        MANGLE_PREFIX "rhs",
        strgen_rvalue(expr->data.binary.rhs),

        MANGLE_PREFIX "add",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        MANGLE_PREFIX "rhs");
}

static char const*
strgen_rvalue_binary_add_wrapping(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_ADD_WRAPPING);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
    assert(type_is_integer(expr->type) && expr->type->size != SIZEOF_UNSIZED);

    return strgen_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        MANGLE_PREFIX "rhs",
        strgen_rvalue(expr->data.binary.rhs),

        MANGLE_PREFIX "add_wrapping",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        MANGLE_PREFIX "rhs");
}

static char const*
strgen_rvalue_binary_sub(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_SUB);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    if (type_is_ieee754(expr->type)) {
        return strgen_fmt(
            "(%s - %s)",
            strgen_rvalue(expr->data.binary.lhs),
            strgen_rvalue(expr->data.binary.rhs));
    }

    assert(type_is_integer(expr->type) && expr->type->size != SIZEOF_UNSIZED);
    return strgen_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        MANGLE_PREFIX "rhs",
        strgen_rvalue(expr->data.binary.rhs),

        MANGLE_PREFIX "sub",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        MANGLE_PREFIX "rhs");
}

static char const*
strgen_rvalue_binary_sub_wrapping(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_SUB_WRAPPING);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
    assert(type_is_integer(expr->type) && expr->type->size != SIZEOF_UNSIZED);

    return strgen_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        MANGLE_PREFIX "rhs",
        strgen_rvalue(expr->data.binary.rhs),

        MANGLE_PREFIX "sub_wrapping",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        MANGLE_PREFIX "rhs");
}

static char const*
strgen_rvalue_binary_mul(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_MUL);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    if (type_is_ieee754(expr->type)) {
        return strgen_fmt(
            "(%s * %s)",
            strgen_rvalue(expr->data.binary.lhs),
            strgen_rvalue(expr->data.binary.rhs));
    }

    assert(type_is_integer(expr->type) && expr->type->size != SIZEOF_UNSIZED);
    return strgen_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        MANGLE_PREFIX "rhs",
        strgen_rvalue(expr->data.binary.rhs),

        MANGLE_PREFIX "mul",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        MANGLE_PREFIX "rhs");
}

static char const*
strgen_rvalue_binary_mul_wrapping(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_MUL_WRAPPING);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
    assert(type_is_integer(expr->type) && expr->type->size != SIZEOF_UNSIZED);

    return strgen_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        MANGLE_PREFIX "rhs",
        strgen_rvalue(expr->data.binary.rhs),

        MANGLE_PREFIX "mul_wrapping",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        MANGLE_PREFIX "rhs");
}

static char const*
strgen_rvalue_binary_div(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_DIV);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    if (type_is_ieee754(expr->type)) {
        return strgen_fmt(
            "(%s / %s)",
            strgen_rvalue(expr->data.binary.lhs),
            strgen_rvalue(expr->data.binary.rhs));
    }

    assert(type_is_integer(expr->type) && expr->type->size != SIZEOF_UNSIZED);
    return strgen_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        MANGLE_PREFIX "rhs",
        strgen_rvalue(expr->data.binary.rhs),

        MANGLE_PREFIX "div",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        MANGLE_PREFIX "rhs");
}

static char const*
strgen_rvalue_binary_rem(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_REM);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
    assert(type_is_integer(expr->type) && expr->type->size != SIZEOF_UNSIZED);

    return strgen_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        MANGLE_PREFIX "rhs",
        strgen_rvalue(expr->data.binary.rhs),

        MANGLE_PREFIX "rem",
        mangle_type(expr->data.binary.lhs->type),
        MANGLE_PREFIX "lhs",
        MANGLE_PREFIX "rhs");
}

static char const*
strgen_rvalue_binary_bitor(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_BITOR);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    return strgen_fmt(
        "(%s | %s)",
        strgen_rvalue(expr->data.binary.lhs),
        strgen_rvalue(expr->data.binary.rhs));
}

static char const*
strgen_rvalue_binary_bitxor(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_BITXOR);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    return strgen_fmt(
        "(%s ^ %s)",
        strgen_rvalue(expr->data.binary.lhs),
        strgen_rvalue(expr->data.binary.rhs));
}

static char const*
strgen_rvalue_binary_bitand(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_BITAND);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    return strgen_fmt(
        "(%s & %s)",
        strgen_rvalue(expr->data.binary.lhs),
        strgen_rvalue(expr->data.binary.rhs));
}

static char const*
strgen_lvalue(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr_is_lvalue(expr));

    // clang-format off
    static struct {
        char const* kind_cstr;
        char const* (*function)(struct expr const*);
    } const table[] = {
#define TABLE_ENTRY(kind, fn) [kind] = {#kind, fn}
        TABLE_ENTRY(EXPR_SYMBOL, strgen_lvalue_symbol),
        TABLE_ENTRY(EXPR_BYTES, strgen_lvalue_bytes),
        TABLE_ENTRY(EXPR_ACCESS_INDEX, strgen_lvalue_access_index),
        TABLE_ENTRY(EXPR_ACCESS_MEMBER_VARIABLE, strgen_lvalue_access_member_variable),
        TABLE_ENTRY(EXPR_UNARY, strgen_lvalue_unary),
#undef TABLE_ENTRY
    };
    // clang-format on

    char const* const cstr = table[expr->kind].kind_cstr;
    if (debug) {
        appendli_location(expr->location, "LVALUE EXPRESSION %s", cstr);
    }
    return table[expr->kind].function(expr);
}

static char const*
strgen_lvalue_symbol(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SYMBOL);

    if (expr->type->size == 0) {
        return strgen_fmt(
            "/* zero-sized symbol */((%s*)0)", mangle_type(expr->type));
    }

    return strgen_fmt("(&%s)", mangle_symbol(expr->data.symbol));
}

static char const*
strgen_lvalue_bytes(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BYTES);

    assert(expr->type->size != 0);
    return strgen_fmt("(&%s)", mangle_symbol(expr->data.bytes.slice_symbol));
}

static char const*
strgen_lvalue_access_index(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);

    bool const lhs_is_zero_sized = expr->data.access_index.lhs->type->size == 0;

    // According to the C standard, performing pointer arithmetic on a NULL
    // pointer has undefined behavior. Pointer addition is manually performed
    // with uintptr_t to avoid this undefined behavior.

    if (expr->data.access_index.lhs->type->kind == TYPE_ARRAY) {
        uintmax_t const base_size =
            expr->data.access_index.lhs->type->data.array.base->size;
        char const* const lhs_type = lhs_is_zero_sized
            ? "int"
            : mangle_type(expr->data.access_index.lhs->type);
        char const* const elements = lhs_is_zero_sized
            ? strgen_fmt("((%s*)0)", mangle_type(expr->type))
            : strgen_fmt("%s->elements", MANGLE_PREFIX "lhs");
        return strgen_fmt(
            "({%s* %s = %s; %s %s = %s; if (%s >= %ju){%s();}; (%s*)((uintptr_t)%s + (%s * %ju));})",
            lhs_type,
            MANGLE_PREFIX "lhs",
            strgen_lvalue(expr->data.access_index.lhs),

            mangle_type(expr->data.access_index.idx->type),
            MANGLE_PREFIX "idx",
            strgen_rvalue(expr->data.access_index.idx),

            MANGLE_PREFIX "idx",
            expr->data.access_index.lhs->type->data.array.count,
            MANGLE_PREFIX "fatal_index_out_of_bounds",

            mangle_type(expr->type),
            elements,
            MANGLE_PREFIX "idx",
            base_size);
    }

    if (expr->data.access_index.lhs->type->kind == TYPE_SLICE) {
        uintmax_t const base_size =
            expr->data.access_index.lhs->type->data.slice.base->size;
        return strgen_fmt(
            "({%s %s = %s; %s %s = %s; if (%s >= %s.count){%s();}; (%s*)((uintptr_t)%s.start + (%s * %ju));})",
            mangle_type(expr->data.access_index.lhs->type),
            MANGLE_PREFIX "lhs",
            strgen_rvalue(expr->data.access_index.lhs),

            mangle_type(expr->data.access_index.idx->type),
            MANGLE_PREFIX "idx",
            strgen_rvalue(expr->data.access_index.idx),

            MANGLE_PREFIX "idx",
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "fatal_index_out_of_bounds",

            mangle_type(expr->type),
            MANGLE_PREFIX "lhs",
            MANGLE_PREFIX "idx",
            base_size);
    }

    UNREACHABLE();
    return NULL;
}

static char const*
strgen_lvalue_access_member_variable(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_MEMBER_VARIABLE);
    assert(
        expr->data.access_member_variable.lhs->type->kind == TYPE_STRUCT
        || expr->data.access_member_variable.lhs->type->kind == TYPE_UNION);

    char const* const lexpr =
        strgen_lvalue(expr->data.access_member_variable.lhs);

    return strgen_fmt(
        "(%s*)(/* object */(uintptr_t)(%s) + /* offset */%ju)",
        mangle_type(expr->type),
        lexpr,
        expr->data.access_member_variable.member_variable->offset);
}

static char const*
strgen_lvalue_unary(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr_is_lvalue(expr));

    switch (expr->data.unary.op) {
    case UOP_DEREFERENCE: {
        assert(expr->data.unary.rhs->type->kind == TYPE_POINTER);
        return strgen_rvalue(expr->data.unary.rhs);
    }
    case UOP_NOT: /* fallthrough */
    case UOP_POS: /* fallthrough */
    case UOP_NEG: /* fallthrough */
    case UOP_NEG_WRAPPING: /* fallthrough */
    case UOP_BITNOT: /* fallthrough */
    case UOP_ADDRESSOF_LVALUE: /* fallthrough */
    case UOP_ADDRESSOF_RVALUE: /* fallthrough */
    case UOP_STARTOF: /* fallthrough */
    case UOP_COUNTOF: {
        UNREACHABLE();
    }
    }

    UNREACHABLE();
    return NULL;
}

static void
codegen_defers(struct stmt const* begin, struct stmt const* end)
{
    assert(begin == NULL || begin->kind == STMT_DEFER);
    assert(end == NULL || end->kind == STMT_DEFER);

    struct stmt const* current = begin;
    while (current != end) {
        codegen_block(&current->data.defer.body);
        current = current->data.defer.prev;
    }
}

void
codegen(
    bool opt_c,
    bool opt_d,
    bool opt_g,
    bool opt_k,
    char const* const* opt_L,
    char const* const* opt_l,
    char const* const opt_o,
    char const* const* paths)
{
    assert(opt_o != NULL);

    debug = opt_g;
    struct string* const src_path = string_new_fmt("%s.tmp.c", opt_o);
    struct string* const obj_path = string_new_fmt("%s.tmp.o", opt_o);

    char const* const SUNDER_HOME = getenv("SUNDER_HOME");
    if (SUNDER_HOME == NULL) {
        fatal(NO_LOCATION, "missing environment variable SUNDER_HOME");
    }

    sbuf(char const*) backend_argv = NULL;
    sbuf_push(backend_argv, context()->env.SUNDER_CC);
    if (opt_c) {
        sbuf_push(backend_argv, "-c");
        sbuf_push(backend_argv, "-o");
        sbuf_push(backend_argv, opt_o);
    }
    else {
        sbuf_push(backend_argv, "-o");
        sbuf_push(backend_argv, opt_o);
    }
    sbuf_push(backend_argv, "-O0");
    if (opt_g) {
        sbuf_push(backend_argv, "-g");
    }
    sbuf_push(backend_argv, strgen_fmt("-I%s/lib/sys", SUNDER_HOME));
    sbuf_push(backend_argv, "-std=c11");
#if 1
    // Disable all warnings. Used to prevent unecessary warnings that would
    // break Sunder tests/workflows if emitted by GCC/Clang.
    //
    // This backend argument should be removed with `#if 0` when testing for
    // ISO C compliance with -pedantic and -pedantic-errors, as -w will prevent
    // pedantic warnings/errors from being reported.
    sbuf_push(backend_argv, "-w");
#else
    sbuf_push(backend_argv, "-Wall");
    sbuf_push(backend_argv, "-Wextra");
    // Workaround for differences in some GCC and CLANG warning names.
    sbuf_push(backend_argv, "-Wno-unknown-warning-option");
    // Workaround for a GCC bug where the universal struct zero-initializer for
    // types with nested struct objects produces a missing braces warning.
    sbuf_push(backend_argv, "-Wno-missing-braces");
    // Not useful for auto-generated C code.
    sbuf_push(backend_argv, "-Wno-parentheses-equality");
    // Sunder does not have type qualifiers.
    sbuf_push(backend_argv, "-Wno-discarded-qualifiers");
    sbuf_push(backend_argv, "-Wno-ignored-qualifiers"); // GCC-specific
    sbuf_push(
        backend_argv, "-Wno-incompatible-pointer-types-discards-qualifiers");
    // Enforced by sunder-compile warnings in the resolve phase.
    sbuf_push(backend_argv, "-Wno-unused-variable");
    // Unused functions are allowed in Sunder code.
    sbuf_push(backend_argv, "-Wno-unused-function");
    // Enforced by sunder-compile warnings in the resolve phase.
    sbuf_push(backend_argv, "-Wno-unused-parameter");
    // Sunder allows unused expressions.
    sbuf_push(backend_argv, "-Wno-unused-value");
    // Sunder allows for expressions that are always true or always false.
    sbuf_push(backend_argv, "-Wno-type-limits");
    // Sunder allows for expressions that are always true or always false.
    sbuf_push(backend_argv, "-Wno-tautological-compare");
    // Ideally, we would enable -pedantic-errors and require that generate C
    // conform to the ISO specification. However, constructs such as
    // function-to-function casting are not supported in ISO C.
    /* sbuf_push(backend_argv, "-pedantic-errors"); */
    // GCC-specific max errors
    /* sbuf_push(backend_argv, "-fmax-errors=1"); */
    // Clang-specific max errors.
    /* sbuf_push(backend_argv, "-ferror-limit=1"); */
#endif
    sbuf_push(backend_argv, string_start(src_path));
    for (size_t i = 0; i < sbuf_count(paths); ++i) {
        sbuf_push(backend_argv, paths[i]);
    }
    if (!opt_c) {
        for (size_t i = 0; i < sbuf_count(opt_L); ++i) {
            sbuf_push(backend_argv, strgen_fmt("-L%s", opt_L[i]));
        }
        sbuf_push(backend_argv, "-lm");
        for (size_t i = 0; i < sbuf_count(opt_l); ++i) {
            sbuf_push(backend_argv, strgen_fmt("-l%s", opt_l[i]));
        }
    }
    struct string* const flags = string_new_cstr(context()->env.SUNDER_CFLAGS);
    sbuf(struct string*) const split =
        string_split(flags, " ", STR_LITERAL_COUNT(" "));
    for (size_t i = 0; i < sbuf_count(split); ++i) {
        if (string_count(split[i]) != 0) {
            sbuf_push(
                backend_argv,
                strgen(string_start(split[i]), string_count(split[i])));
        }
        string_del(split[i]);
    }
    sbuf_fini(split);
    string_del(flags);
    sbuf_push(backend_argv, (char const*)NULL);

    int err = 0;

    if ((out = fopen(string_start(src_path), "wb")) == NULL) {
        err = errno;
        error(
            NO_LOCATION,
            "unable to open temporary file `%s` with error '%s'",
            string_start(src_path),
            strerror(errno));
        goto cleanup;
    }

    // Check for duplicate static addresses amongst separate symbols.
    struct static_symbol_mapping {
        struct symbol const* symbol;
        char const* mangled_address;
    };
    sbuf(struct static_symbol_mapping) static_symbol_mappings = NULL;
    for (size_t i = 0; i < sbuf_count(context()->static_symbols); ++i) {
        struct symbol const* const symbol = context()->static_symbols[i];
        assert(symbol_xget_address(symbol)->kind == ADDRESS_STATIC);
        struct static_symbol_mapping const mapping = {
            .symbol = symbol,
            .mangled_address = mangle_address(symbol_xget_address(symbol)),
        };
        sbuf_push(static_symbol_mappings, mapping);
    }
    sbuf_freeze(static_symbol_mappings);
    for (size_t i = 0; i < sbuf_count(static_symbol_mappings); ++i) {
        struct static_symbol_mapping const m_i = static_symbol_mappings[i];
        char const* const mangled_address_i = m_i.mangled_address;
        for (size_t j = i + 1; j < sbuf_count(static_symbol_mappings); ++j) {
            struct static_symbol_mapping m_j = static_symbol_mappings[j];
            char const* const mangled_address_j = m_j.mangled_address;
            assert(mangled_address_i != NULL);
            assert(mangled_address_j != NULL);
            if (0 != strcmp(mangled_address_i, mangled_address_j)) {
                continue; // No mangled static address conflict.
            }

            struct symbol const* const symbol_i = m_i.symbol;
            struct symbol const* const symbol_j = m_j.symbol;
            struct source_location const location_i = symbol_i->location;
            struct source_location const location_j = symbol_j->location;
            char const* defined_at_i = "defined internally";
            char const* defined_at_j = "defined internally";
            if (location_i.path != NO_PATH && location_i.line != NO_LINE) {
                defined_at_i = intern_fmt(
                    "defined at %s:%zu",
                    symbol_i->location.path,
                    symbol_i->location.line);
            }
            if (location_j.path != NO_PATH && location_j.line != NO_LINE) {
                defined_at_j = intern_fmt(
                    "defined at %s:%zu",
                    symbol_j->location.path,
                    symbol_j->location.line);
            }
            error(
                NO_LOCATION,
                "symbols `%s` (%s) and `%s` (%s) resolve to the same static address `%s`",
                symbol_xget_address(m_i.symbol)->data.static_.name,
                defined_at_i,
                symbol_xget_address(m_j.symbol)->data.static_.name,
                defined_at_j,
                m_i.mangled_address);
            err = true;
        }
    }
    if (err) {
        info(
            NO_LOCATION,
            "compilation of generated C code will fail due to conflicting Sunder addresses used as C identifiers");
        goto cleanup;
    }

    appendln("#include \"sys.h\"");
    appendch('\n');
    // Generate forward type declarations.
    for (size_t i = 0; i < sbuf_count(context()->types); ++i) {
        struct type const* const type = context()->types[i];
        codegen_type_declaration(type);
    }
    // Generate type definitions.
    for (size_t i = 0; i < sbuf_count(context()->types); ++i) {
        struct type const* const type = context()->types[i];
        codegen_type_definition(type);
    }
    appendch('\n');
    // Generate static function prototypes.
    for (size_t i = 0; i < sbuf_count(context()->static_symbols); ++i) {
        struct symbol const* const symbol = context()->static_symbols[i];
        assert(symbol_xget_address(symbol)->kind == ADDRESS_STATIC);
        if (symbol->kind != SYMBOL_FUNCTION) {
            continue;
        }
        codegen_static_function(symbol, true);
    }
    // Generate static object definitions.
    for (size_t i = 0; i < sbuf_count(context()->static_symbols); ++i) {
        struct symbol const* const symbol = context()->static_symbols[i];
        assert(symbol_xget_address(symbol)->kind == ADDRESS_STATIC);
        bool const is_static_object =
            symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_CONSTANT;
        if (!is_static_object) {
            continue;
        }
        codegen_static_object(symbol);
    }
    // Generate static function definitions.
    for (size_t i = 0; i < sbuf_count(context()->static_symbols); ++i) {
        struct symbol const* const symbol = context()->static_symbols[i];
        assert(symbol_xget_address(symbol)->kind == ADDRESS_STATIC);
        if (symbol->kind != SYMBOL_FUNCTION) {
            continue;
        }
        codegen_static_function(symbol, false);
    }
    if (!opt_c) {
        appendch('\n');
        appendln("int");
        appendln("main(int argc, char** argv, char** envp)");
        appendln("{");
        indent_incr();
        appendli("sys_argc = argc;");
        appendli("sys_argv = argv;");
        appendli("sys_envp = envp;");
        appendli("%s();", mangle_name(context()->interned.main));
        appendli("return 0;");
        indent_decr();
        appendln("}");
    }

    (void)fclose(out);

    if (!opt_d && (err = spawnvpw(backend_argv))) {
        goto cleanup;
    }

cleanup:
    if (!opt_k) {
        (void)remove(string_start(src_path));
    }
    if (!opt_k && !opt_c) {
        (void)remove(string_start(obj_path));
    }
    sbuf_fini(backend_argv);
    string_del(src_path);
    string_del(obj_path);
    if (err) {
        exit(EXIT_FAILURE);
    }
}
