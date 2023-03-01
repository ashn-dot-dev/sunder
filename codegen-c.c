// Copyright 2023 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "sunder.h"

static struct string* out = NULL;
static unsigned indent = 0u;

static char const* // interned
mangle(char const* cstr);
static char const* // interned
mangle_name(char const* name);
static char const* // interned
mangle_type(struct type const* type);

static void
indent_incr(void);
static void
indent_decr(void);

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
appendli_location(struct source_location location, char const* fmt, ...);
static void
appendch(char ch);

static void
codegen_static_object(struct symbol const* symbol);
static void
codegen_static_function(struct symbol const* symbol, bool prototype);

static void
codegen_value(struct value const* value);
static void
codegen_uninit(struct type const* type);

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
codegen_stmt_return(struct stmt const* stmt);
static void
codegen_stmt_assign(struct stmt const* stmt);
static void
codegen_stmt_expr(struct stmt const* stmt);

static char const* // interned
mangle(char const* cstr)
{
    assert(cstr != NULL);

    struct string* const s = string_new(NULL, 0);

    for (char const* cur = cstr; *cur != '\0'; ++cur) {
        if (!safe_isalnum(*cur)) {
            string_append_cstr(s, "_");
            continue;
        }
        string_append_fmt(s, "%c", *cur);
    }

    char const* const interned = intern(string_start(s), string_count(s));

    string_del(s);
    return interned;
}

static char const* // interned
mangle_name(char const* name)
{
    assert(name != NULL);

    return intern_fmt("__sunder_%s", mangle(name));
}

static char const* // interned
mangle_type_recursive(struct type const* type)
{
    assert(type != NULL);

    if (type->kind == TYPE_FUNCTION) {
        struct string* p = string_new(NULL, 0);
        sbuf(struct type const* const) ptypes =
            type->data.function.parameter_types;
        for (size_t i = 0; i < sbuf_count(ptypes); ++i) {
            if (i != 0) {
                string_append_cstr(p, "_");
            }
            string_append_fmt(
                p, "parameter%zu_%s", i + 1, mangle_type_recursive(ptypes[i]));
        }

        struct string* const s = string_new_fmt(
            "func_%s_returning_%s",
            string_start(p),
            mangle_type_recursive(type->data.function.return_type));

        char const* const result = intern(string_start(s), string_count(s));

        string_del(p);
        string_del(s);
        return result;
    }

    if (type->kind == TYPE_POINTER) {
        struct string* const s = string_new_fmt(
            "pointer_to_%s", mangle_type_recursive(type->data.pointer.base));
        char const* const result = intern(string_start(s), string_count(s));
        string_del(s);
        return result;
    }

    if (type->kind == TYPE_ARRAY) {
        struct string* const s = string_new_fmt(
            "array_%" PRIu64 "_of_%s",
            type->data.array.count,
            mangle_type_recursive(type->data.array.base));
        char const* const result = intern(string_start(s), string_count(s));
        string_del(s);
        return result;
    }

    if (type->kind == TYPE_SLICE) {
        struct string* const s = string_new_fmt(
            "slice_of_%s", mangle_type_recursive(type->data.slice.base));
        char const* const result = intern(string_start(s), string_count(s));
        string_del(s);
        return result;
    }

    return mangle(type->name);
}

char const* // interned
mangle_type(struct type const* type)
{
    assert(type != NULL);

    if (type->kind != TYPE_VOID && type->size == 0) {
        return context()->interned.void_;
    }

    if (type->size == SIZEOF_UNSIZED) {
        return context()->interned.void_;
    }

    return mangle_name(mangle_type_recursive(type));
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

    for (unsigned i = 0; i < indent; ++i) {
        string_append_cstr(out, "    ");
    }

    va_list args;
    va_start(args, fmt);
    string_append_vfmt(out, fmt, args);
    va_end(args);

    string_append_cstr(out, "\n");
}

static void
appendli_location(struct source_location location, char const* fmt, ...)
{
    assert(out != NULL);
    assert(location.path != NO_PATH);
    assert(location.line != NO_LINE);
    assert(location.psrc != NO_PSRC);

    for (unsigned i = 0; i < indent; ++i) {
        string_append_cstr(out, "    ");
    }

    string_append_fmt(out, "/// [%s:%zu] ", location.path, location.line);

    va_list args;
    va_start(args, fmt);
    string_append_vfmt(out, fmt, args);
    va_end(args);

    string_append_cstr(out, "\n");

    char const* const line_start = source_line_start(location.psrc);
    char const* const line_end = source_line_end(location.psrc);

    for (unsigned i = 0; i < indent; ++i) {
        string_append_cstr(out, "    ");
    }
    string_append_fmt(
        out, "/// %.*s\n", (int)(line_end - line_start), line_start);

    for (unsigned i = 0; i < indent; ++i) {
        string_append_cstr(out, "    ");
    }
    string_append_fmt(out, "/// %*s^\n", (int)(location.psrc - line_start), "");
}

static void
appendch(char ch)
{
    assert(out != NULL);

    string_append(out, &ch, 1u);
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
        appendli(
            "extern %s %s;",
            mangle_type(symbol_xget_type(symbol)),
            mangle_name(symbol_xget_address(symbol)->data.static_.name));
        return;
    }

    struct type const* const type = symbol_xget_type(symbol);
    if (type->size == 0) {
        // Zero-sized objects take up zero space.
        return;
    }

    if (symbol->kind == SYMBOL_CONSTANT) {
        append("const ");
    }

    assert(symbol_xget_address(symbol)->data.static_.offset == 0);
    append(
        "%s %s",
        mangle_type(symbol_xget_type(symbol)),
        mangle_name(symbol_xget_address(symbol)->data.static_.name));
    if (symbol->data.variable->value == NULL) {
        // Global data without an initializer is zero-initialized.
        appendln(";");
        return;
    }

    append(" = ");
    codegen_value(symbol_xget_value(NO_LOCATION, symbol));
    appendln(";");
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
    sbuf(struct symbol const* const) parameters = function->symbol_parameters;
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
        string_append_cstr(params, mangle_name(parameters[i]->name));
        params_written += 1;
    }

    append(
        "%s %s(%s)",
        mangle_type(function->type->data.function.return_type),
        mangle_name(function->address->data.static_.name),
        string_count(params) != 0 ? string_start(params) : "void");
    string_del(params);

    if (prototype) {
        appendln(";");
        return;
    }

    appendln("{");
    indent_incr();
    codegen_block(&function->body);
    indent_decr();
    appendln("}");
}

static void
codegen_value(struct value const* value)
{
    switch (value->type->kind) {
    case TYPE_ANY: /* fallthrough */
    case TYPE_VOID: {
        UNREACHABLE();
    }
    case TYPE_BOOL: {
        append("%s", mangle_name(value->data.boolean ? "true" : "false"));
        return;
    }
    case TYPE_BYTE: {
        append("0x%02x", value->data.byte);
        return;
    }
    case TYPE_U8: /* fallthrough */
    case TYPE_U16: /* fallthrough */
    case TYPE_U32: /* fallthrough */
    case TYPE_U64: /* fallthrough */
    case TYPE_USIZE: {
        char* const cstr = bigint_to_new_cstr(value->data.integer);
        append("(%s)%sULL", mangle_type(value->type), cstr);
        xalloc(cstr, XALLOC_FREE);
        return;
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
            char* const s = bigint_to_new_cstr(tmp);
            append("/* %s */((%s)%sLL - 1)", cstr, mangle_type(value->type), s);
            xalloc(s, XALLOC_FREE);
            bigint_del(tmp);
        }
        else {
            append("(%s)%sLL", mangle_type(value->type), cstr);
        }
        xalloc(cstr, XALLOC_FREE);
        return;
    }
    case TYPE_INTEGER: {
        UNREACHABLE();
    }
    case TYPE_FUNCTION: {
        struct address const* const address = value->data.function->address;
        assert(address->kind == ADDRESS_STATIC);
        assert(address->data.static_.offset == 0);
        // Casting to a void pointer in order to support the implicit function
        // conversions permitted in Sunder wich are disallowed in ISO C.
        //
        // This particular form of casting should be well behaved on POSIX
        // platforms, as function<->pointer casts are used in dlsym.
        append("(void*)%s", mangle_name(address->data.static_.name));
        return;
    }
    case TYPE_POINTER: {
        struct address const* const address = &value->data.pointer;
        if (value->type->data.pointer.base->size == 0) {
            append("0");
            return;
        }

        switch (address->kind) {
        case ADDRESS_ABSOLUTE: {
            append("%" PRIu64, address->data.absolute);
            break;
        }
        case ADDRESS_STATIC: {
            char const* base = intern_fmt(
                "(void*)&%s", mangle_name(address->data.static_.name));

            // XXX: Hack so we can detect static objects of size zero and emit
            // a NULL pointer for them instead of attempting to take the
            // address of a zero-sized object that was never defined.
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
                    // Object has a non-zero size, and therefor will have a
                    // non-NULL address.
                    break;
                }
                append("(void*)0");
                return;
            }

            if (address->data.static_.offset == 0) {
                append("(void*)%s", base);
            }
            else {
                append(
                    "(void*)((char*)%s + %" PRIu64 ")",
                    base,
                    address->data.static_.offset);
            }
            break;
        }
        case ADDRESS_LOCAL: {
            UNREACHABLE();
        }
        }

        return;
    }
    case TYPE_ARRAY: {
        sbuf(struct value*) const elements = value->data.array.elements;
        struct value const* const ellipsis = value->data.array.ellipsis;
        uint64_t const count = value->type->data.array.count;
        append("{.elements = {");
        for (size_t i = 0; i < count; ++i) {
            if (i != 0) {
                append(", ");
            }
            if (i < sbuf_count(elements)) {
                codegen_value(elements[i]);
            }
            else {
                assert(ellipsis != NULL);
                codegen_value(ellipsis);
            }
        }
        append("}}");
        return;
    }
    case TYPE_SLICE: {
        append("{.start = ");
        codegen_value(value->data.slice.pointer);
        append(", .count = ");
        codegen_value(value->data.slice.count);
        append("}");
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

        append("{");
        for (size_t i = 0; i < member_variable_count; ++i) {
            if (i != 0) {
                append(", ");
            }

            if (member_variable_vals[i] != NULL) {
                codegen_value(member_variable_vals[i]);
            }
            else {
                codegen_uninit(member_variable_defs[i].type);
            }
        }
        append("}");

        return;
    }
    }
}

static void
codegen_uninit(struct type const* type)
{
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
        append("0");
        return;
    }
    case TYPE_INTEGER: {
        UNREACHABLE();
    }
    case TYPE_FUNCTION: /* fallthrough */
    case TYPE_POINTER: {
        append("0");
        return;
    }
    case TYPE_ARRAY: /* fallthrough */
    case TYPE_SLICE: /* fallthrough */
    case TYPE_STRUCT: {
        append("{0}");
        return;
    }
    }
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
        TABLE_ENTRY(STMT_RETURN, codegen_stmt_return),
        TABLE_ENTRY(STMT_ASSIGN, codegen_stmt_assign),
        TABLE_ENTRY(STMT_EXPR, codegen_stmt_expr),
#undef TABLE_ENTRY
    };

    char const* const cstr = table[stmt->kind].kind_cstr;
    appendli_location(stmt->location, "%s", cstr);
    table[stmt->kind].codegen_fn(stmt);
}

static void
codegen_stmt_defer(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_DEFER);

    // No code generation is performed for defer statements as defers are
    // generated as equivalent lowered statements by other codegen functions.
    return;
}

static void
codegen_stmt_if(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_IF);

    appendli("/* TODO */");
}

static void
codegen_stmt_for_range(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_FOR_RANGE);

    appendli("/* TODO */");
}

static void
codegen_stmt_for_expr(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_FOR_EXPR);

    appendli("/* TODO */");
}

static void
codegen_stmt_break(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_BREAK);

    appendli("/* TODO */");
}

static void
codegen_stmt_continue(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_CONTINUE);

    appendli("/* TODO */");
}

static void
codegen_stmt_return(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_RETURN);

    appendli("/* TODO */");
}

static void
codegen_stmt_assign(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_ASSIGN);

    appendli("/* TODO */");
}

static void
codegen_stmt_expr(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_EXPR);

    appendli("/* TODO */");
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
codegen_c(
    bool opt_c, bool opt_k, char const* const* opt_l, char const* const opt_o)
{
    assert(opt_o != NULL);
    assert(0 == strcmp(backend(), "C") || 0 == strcmp(backend(), "c"));

    out = string_new(NULL, 0u);
    struct string* const src_path = string_new_fmt("%s.c", opt_o);
    struct string* const obj_path = string_new_fmt("%s.o", opt_o);

    char const* const SUNDER_HOME = getenv("SUNDER_HOME");
    if (SUNDER_HOME == NULL) {
        fatal(NO_LOCATION, "missing environment variable SUNDER_HOME");
    }

    sbuf(char const*) backend_argv = NULL;
    sbuf_push(backend_argv, "gcc");
    sbuf_push(backend_argv, "-o");
    sbuf_push(backend_argv, string_start(obj_path));
    sbuf_push(backend_argv, "-O0");
    sbuf_push(backend_argv, "-g");
    sbuf_push(backend_argv, "-c");
    sbuf_push(backend_argv, intern_fmt("-I%s/lib/sys", SUNDER_HOME));
    sbuf_push(backend_argv, "-std=c11");
    sbuf_push(backend_argv, "-Wall");
    sbuf_push(backend_argv, "-Wextra");
    // Workaround for a GCC bug where the universal struct zero-initializer for
    // types with nested struct objects produces a missing braces warning.
    sbuf_push(backend_argv, "-Wno-missing-braces");
    // Ideally, we would enable -pedantic-errors and require that generate C
    // conform to the ISO specification. However, constructs such as
    // function-to-function casting are not supported in ISO C.
    /* sbuf_push(backend_argv, "-pedantic-errors"); */
    sbuf_push(backend_argv, "-fmax-errors=1");
    sbuf_push(backend_argv, string_start(src_path));
    sbuf_push(backend_argv, (char const*)NULL);

    sbuf(char const*) ld_argv = NULL;
    sbuf_push(ld_argv, "gcc");
    sbuf_push(ld_argv, "-o");
    sbuf_push(ld_argv, opt_o);
    sbuf_push(ld_argv, string_start(obj_path));
    for (size_t i = 0; i < sbuf_count(opt_l); ++i) {
        sbuf_push(backend_argv, intern_fmt("-l%s", opt_l[i]));
    }
    sbuf_push(ld_argv, (char const*)NULL);

    appendln("#include \"sys.h\"");
    appendch('\n');
    // Forward-declare structs.
    for (size_t i = 0; i < sbuf_count(context()->types); ++i) {
        struct type const* const type = context()->types[i];
        if (type->kind != TYPE_STRUCT) {
            continue;
        }
        if (type->size == 0 || type->size == SIZEOF_UNSIZED) {
            continue;
        }
        char const* const typename = mangle_type(type);
        appendln("typedef struct %s %s; // %s", typename, typename, type->name);
    }
    // Generate composite type definitions.
    for (size_t i = 0; i < sbuf_count(context()->types); ++i) {
        struct type const* const type = context()->types[i];

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
        case TYPE_INTEGER: {
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
            char const* const basename = mangle_type(type->data.pointer.base);
            char const* const typename = mangle_type(type);
            appendln("typedef %s* %s; // %s", basename, typename, type->name);
            break;
        }
        case TYPE_ARRAY: {
            if (type->size == 0 || type->size == SIZEOF_UNSIZED) {
                continue;
            }
            char const* const basename = mangle_type(type->data.array.base);
            char const* const typename = mangle_type(type);
            appendln(
                "typedef struct {%s elements[%" PRIu64 "];} %s; // %s",
                basename,
                type->data.array.count,
                typename,
                type->name);
            break;
        }
        case TYPE_SLICE: {
            struct type* const starttype =
                type_new_pointer(type->data.slice.base);
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
                continue;
            }
            char const* const typename = mangle_type(type);
            appendln("struct %s", typename);
            appendli("{");
            indent_incr();
            sbuf(struct member_variable const) const mvars =
                type->data.struct_.member_variables;
            for (size_t i = 0; i < sbuf_count(mvars); ++i) {
                if (mvars[i].type->size == 0
                    || mvars[i].type->size == SIZEOF_UNSIZED) {
                    continue;
                }
                appendli("%s %s;", mangle_type(mvars[i].type), mvars[i].name);
            }
            indent_decr();
            appendli("};");
            break;
        }
        }

        if (type->size != 0 && type->size != SIZEOF_UNSIZED) {
            char const* const typename = mangle_type(type);
            appendln(
                "_Static_assert(sizeof(%s) == %" PRId64 ", \"sizeof(%s)\");",
                typename,
                type->size,
                typename);
            appendln(
                "_Static_assert(_Alignof(%s) == %" PRId64 ", \"alignof(%s)\");",
                typename,
                type->align,
                typename);
        }
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
        if (symbol->kind != SYMBOL_VARIABLE
            && symbol->kind != SYMBOL_CONSTANT) {
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
    appendch('\n');
    appendln("int");
    appendln("main(void)");
    appendln("{");
    indent_incr();
    appendli("return 0;");
    indent_decr();
    appendln("}");

    printf("%s", string_start(out));

    int err = 0;
    if ((err = file_write_all(
             string_start(src_path), string_start(out), string_count(out)))) {
        error(
            NO_LOCATION,
            "unable to write file `%s` with error '%s'",
            string_start(src_path),
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
        (void)remove(string_start(src_path));
    }
    if (!opt_k && !opt_c) {
        (void)remove(string_start(obj_path));
    }
    sbuf_fini(backend_argv);
    sbuf_fini(ld_argv);
    string_del(src_path);
    string_del(obj_path);
    string_del(out);
    if (err) {
        exit(EXIT_FAILURE);
    }
}
