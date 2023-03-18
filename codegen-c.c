// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "sunder.h"

static struct string* out = NULL;
static unsigned indent = 0u;
static struct function const* current_function = NULL;
static struct stmt const* current_for_range_loop = NULL;

static char const* // interned
mangle(char const* cstr);
static char const* // interned
mangle_name(char const* name);
static char const* // interned
mangle_type(struct type const* type);
static char const* // interned
mangle_address(struct address const* address);
static char const* // interned
mangle_local_symbol_name(struct symbol const* symbol);
static char const* // interned
mangle_symbol(struct symbol const* symbol);

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
codegen_type_declaration(struct type const* type);
static void
codegen_type_definition(struct type const* type);
static void
codegen_static_object(struct symbol const* symbol);
static void
codegen_static_function(struct symbol const* symbol, bool prototype);

static char const* // interned
strgen_value(struct value const* value);
static char const* // interned
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
codegen_stmt_return(struct stmt const* stmt);
static void
codegen_stmt_assign(struct stmt const* stmt);
static void
codegen_stmt_expr(struct stmt const* stmt);

static char const* // interned
strgen_rvalue(struct expr const* expr);
static char const* // interned
strgen_rvalue_symbol(struct expr const* expr);
static char const* // interned
strgen_rvalue_value(struct expr const* expr);
static char const* // interned
strgen_rvalue_bytes(struct expr const* expr);
static char const* // interned
strgen_rvalue_array_list(struct expr const* expr);
static char const* // interned
strgen_rvalue_slice_list(struct expr const* expr);
static char const* // interned
strgen_rvalue_slice(struct expr const* expr);
static char const* // interned
strgen_rvalue_struct(struct expr const* expr);
static char const* // interned
strgen_rvalue_cast(struct expr const* expr);
static char const* // interned
strgen_rvalue_call(struct expr const* expr);
static char const* // interned
strgen_rvalue_access_index(struct expr const* expr);
static char const* // interned
strgen_rvalue_access_slice(struct expr const* expr);
static char const* // interned
strgen_rvalue_access_member_variable(struct expr const* expr);
static char const* // interned
strgen_rvalue_sizeof(struct expr const* expr);
static char const* // interned
strgen_rvalue_alignof(struct expr const* expr);
static char const* // interned
strgen_rvalue_unary(struct expr const* expr);
static char const* // interned
strgen_rvalue_unary_not(struct expr const* expr);
static char const* // interned
strgen_rvalue_unary_pos(struct expr const* expr);
static char const* // interned
strgen_rvalue_unary_neg(struct expr const* expr);
static char const* // interned
strgen_rvalue_unary_neg_wrapping(struct expr const* expr);
static char const* // interned
strgen_rvalue_unary_bitnot(struct expr const* expr);
static char const* // interned
strgen_rvalue_unary_dereference(struct expr const* expr);
static char const* // interned
strgen_rvalue_unary_addressof(struct expr const* expr);
static char const* // interned
strgen_rvalue_unary_startof(struct expr const* expr);
static char const* // interned
strgen_rvalue_unary_countof(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_or(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_and(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_shl(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_shr(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_eq(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_ne(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_le(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_lt(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_ge(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_gt(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_add(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_add_wrapping(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_sub(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_sub_wrapping(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_mul(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_mul_wrapping(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_div(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_rem(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_bitor(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_bitxor(struct expr const* expr);
static char const* // interned
strgen_rvalue_binary_bitand(struct expr const* expr);

static char const* // interned
strgen_lvalue(struct expr const* expr);
static char const* // interned
strgen_lvalue_symbol(struct expr const* expr);
static char const* // interned
strgen_lvalue_access_index(struct expr const* expr);
static char const* // interned
strgen_lvalue_access_member_variable(struct expr const* expr);
static char const* // interned
strgen_lvalue_unary(struct expr const* expr);

static char const*
mangle(char const* cstr)
{
    assert(cstr != NULL);

    struct string* const s = string_new(NULL, 0);

    for (char const* cur = cstr; *cur != '\0';) {
        if (cstr_starts_with(cur, "::")) {
            string_append_cstr(s, "_");
            cur += 2;
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

    char const* const interned = intern(string_start(s), string_count(s));

    string_del(s);
    return interned;
}

static char const*
mangle_name(char const* name)
{
    assert(name != NULL);

    return intern_fmt("__sunder_%s", mangle(name));
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

    return mangle_name(mangle_type_recursive(type));
}

static char const*
mangle_address(struct address const* address)
{
    assert(address != NULL);

    switch (address->kind) {
    case ADDRESS_ABSOLUTE: {
        return intern_fmt("(void*)%" PRIu64, address->data.absolute);
    }
    case ADDRESS_STATIC: {
        char const* const base =
            intern_fmt("(void*)&%s", mangle_name(address->data.static_.name));
        if (address->data.static_.offset == 0) {
            return base;
        }
        return intern_fmt(
            "(void*)((char*)%s + %" PRIu64 ")",
            base,
            address->data.static_.offset);
    }
    case ADDRESS_LOCAL: {
        return intern_fmt("(void*)&%s", mangle_name(address->data.local.name));
    }
    }

    UNREACHABLE();
}

static char const*
mangle_local_symbol_name(struct symbol const* symbol)
{
    assert(symbol != NULL);

    struct address const* const address = symbol_xget_address(symbol);
    assert(address->kind == ADDRESS_LOCAL);
    return mangle_name(address->data.local.name);
}

static char const*
mangle_symbol(struct symbol const* symbol)
{
    assert(symbol != NULL);

    struct address const* const address = symbol_xget_address(symbol);
    switch (address->kind) {
    case ADDRESS_ABSOLUTE: {
        UNREACHABLE();
    }
    case ADDRESS_STATIC: {
        assert(address->data.static_.offset == 0);
        return mangle_name(address->data.static_.name);
    }
    case ADDRESS_LOCAL: {
        return mangle_name(address->data.local.name);
    }
    }

    UNREACHABLE();
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
codegen_type_declaration(struct type const* type)
{
    if (type->kind != TYPE_STRUCT) {
        return;
    }
    if (type->size == 0 || type->size == SIZEOF_UNSIZED) {
        return;
    }
    char const* const typename = mangle_type(type);
    appendln("typedef struct %s %s; // %s", typename, typename, type->name);
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
            "typedef struct {%s elements[%" PRIu64 "];} %s; // %s",
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
        sbuf(struct member_variable const) const mvars =
            type->data.struct_.member_variables;
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
    }

    if (type->size != 0 && type->size != SIZEOF_UNSIZED) {
        char const* const typename = mangle_type(type);
        appendln(
            "_Static_assert(sizeof(%s) == %" PRId64 ", \"sizeof(%s)\");",
            typename,
            type->size,
            type->name);
        appendln(
            "_Static_assert(_Alignof(%s) == %" PRId64 ", \"alignof(%s)\");",
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
        appendln("#define %s %s", mangle_name(name), mangle(name));
        appendln("extern %s %s;", mangle_type(type), mangle(name));
        return;
    }

    if (type->size == 0) {
        // Zero-sized objects take up zero space.
        return;
    }

    if (symbol->kind == SYMBOL_CONSTANT) {
        append("__attribute__((section(\"rodata\"))) ");
    }

    assert(symbol_xget_address(symbol)->data.static_.offset == 0);
    append(
        "%s %s __asm__(\"%s\")",
        mangle_type(type),
        mangle_name(name),
        mangle(name));
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
    // XXX: function->symbol_parameters is never set for extern functions, and
    // the `if (prototype)` branching behavior here is used as a workaround
    // since extern functions will only ever have their prototypes generated.
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
            string_append_cstr(params, mangle_name(parameters[i]->name));
            params_written += 1;
        }
    }

    if (function->is_extern) {
        appendln(
            "#define %s %s",
            mangle_name(function->address->data.static_.name),
            mangle(function->address->data.static_.name));
        append(
            "%s %s(%s)",
            mangle_type(function->type->data.function.return_type),
            mangle(function->address->data.static_.name),
            params_written != 0 ? string_start(params) : "void");
    }
    else {
        append(
            "%s %s(%s)",
            mangle_type(function->type->data.function.return_type),
            mangle_name(function->address->data.static_.name),
            params_written != 0 ? string_start(params) : "void");
    }

    string_del(params);

    if (prototype) {
        char const* const name = function->address->data.static_.name;
        bool const use_unmagled_symbol =
            !function->is_extern && name != context()->interned.main;
        if (use_unmagled_symbol) {
            append(" __asm__(\"%s\")", mangle(name));
        }
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
            return intern_fmt("(float)NAN");
        }
        if (isinf(ieee754) && ieee754 < 0) {
            return intern_fmt("(float)-INFINITY");
        }
        if (isinf(ieee754) && ieee754 > 0) {
            return intern_fmt("(float)+INFINITY");
        }
        return intern_fmt("%.*ff", IEEE754_FLT_DECIMAL_DIG, ieee754);
    }
    case TYPE_F64: {
        double const ieee754 = value->data.f64;
        if (isnan(ieee754)) {
            return intern_fmt("(double)NAN");
        }
        if (isinf(ieee754) && ieee754 < 0) {
            return intern_fmt("(double)-INFINITY");
        }
        if (isinf(ieee754) && ieee754 > 0) {
            return intern_fmt("(double)+INFINITY");
        }
        return intern_fmt("%.*f", IEEE754_DBL_DECIMAL_DIG, ieee754);
    }
    case TYPE_REAL: {
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
        string_append_fmt(
            s, "(void*)%s", mangle_name(address->data.static_.name));
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
            string_append_fmt(s, "%" PRIu64 "UL", address->data.absolute);
            break;
        }
        case ADDRESS_STATIC: {
            char const* base = intern_fmt(
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
                    // Object has a non-zero size, and therefor will have a
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

        break;
    }
    case TYPE_ARRAY: {
        sbuf(struct value*) const elements = value->data.array.elements;
        struct value const* const ellipsis = value->data.array.ellipsis;
        uint64_t const count = value->type->data.array.count;
        string_append_fmt(s, "{.elements = {");
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
        string_append_cstr(s, strgen_value(value->data.slice.pointer));
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
            value->data.struct_.member_variables;
        size_t const member_variable_vals_count =
            sbuf_count(member_variable_vals);

        assert(member_variable_defs_count == member_variable_vals_count);
        size_t const member_variable_count = member_variable_vals_count;
        (void)member_variable_defs_count;

        string_append_cstr(s, "{");
        size_t members_written = 0;
        for (size_t i = 0; i < member_variable_count; ++i) {
            if (member_variable_defs[i].type->size == 0) {
                continue;
            }

            if (members_written != 0) {
                string_append_cstr(s, ", ");
            }

            if (member_variable_vals[i] != NULL) {
                string_append_cstr(s, strgen_value(member_variable_vals[i]));
            }
            else {
                string_append_cstr(
                    s, strgen_uninit(member_variable_defs[i].type));
            }
            members_written += 1;
        }
        string_append_cstr(s, "}");

        break;
    }
    }

    char const* const result = intern(string_start(s), string_count(s));
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
        string_append_cstr(s, "{/*uninit*/0}");
        break;
    }
    }

    char const* const result = intern(string_start(s), string_count(s));
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
            appendli("// %s: %s", locals[i].name, type->name);
            appendli(
                "int %s = /* zero-sized local */0;",
                mangle_name(address->data.local.name));
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

        appendli("// %s: %s", locals[i].name, type->name);
        appendli(
            "%s %s = %s;",
            mangle_type(type),
            mangle_name(address->data.local.name),
            strgen_uninit(type));
        appendli(
            "/* zero pading */memset(&%s, 0x00, sizeof(%s));",
            mangle_name(address->data.local.name),
            mangle_name(address->data.local.name));
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
        TABLE_ENTRY(STMT_RETURN, codegen_stmt_return),
        TABLE_ENTRY(STMT_ASSIGN, codegen_stmt_assign),
        TABLE_ENTRY(STMT_EXPR, codegen_stmt_expr),
#undef TABLE_ENTRY
    };

    char const* const cstr = table[stmt->kind].kind_cstr;
    appendli_location(stmt->location, "STATEMENT %s", cstr);
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
    appendli(
        "for (%s %s = %s; %s < %s; ++%s)",
        mangle_type(context()->builtin.usize),
        mangle_local_symbol_name(variable),
        strgen_rvalue(stmt->data.for_range.begin),
        mangle_local_symbol_name(variable),
        strgen_rvalue(stmt->data.for_range.end),
        mangle_local_symbol_name(variable));
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
codegen_stmt_return(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_RETURN);

    struct expr const* const expr = stmt->data.return_.expr;
    if (expr != NULL && expr->type->size != 0) {
        // Compute and store the expression result.
        appendli(
            "%s = %s;",
            mangle_name("return"),
            strgen_rvalue(stmt->data.return_.expr));
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
codegen_stmt_assign(struct stmt const* stmt)
{
    assert(stmt != NULL);
    assert(stmt->kind == STMT_ASSIGN);
    assert(stmt->data.assign.lhs->type == stmt->data.assign.rhs->type);

    if (stmt->data.assign.lhs->type->size == 0) {
        // The left and right hand sides of the assignment are evaluated, since
        // they might have side effects. However, no C assignment operation is
        // performed since there is nothing to assign.
        appendli(
            "%s; /* zero-sized assignment*/; %s;",
            strgen_lvalue(stmt->data.assign.lhs),
            strgen_rvalue(stmt->data.assign.rhs));
        return;
    }

    appendli(
        "*(%s) = %s;",
        strgen_lvalue(stmt->data.assign.lhs),
        strgen_rvalue(stmt->data.assign.rhs));
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
        TABLE_ENTRY(EXPR_STRUCT, strgen_rvalue_struct),
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
    appendli_location(expr->location, "RVALUE EXPRESSION %s", cstr);
    return table[expr->kind].function(expr);
}

static char const*
strgen_rvalue_symbol(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SYMBOL);

    if (expr->type->size == 0) {
        return intern_fmt("/* zero-sized symbol */(0)");
    }

    return mangle_symbol(expr->data.symbol);
}

static char const*
strgen_rvalue_value(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_VALUE);

    if (type_is_compound(expr->type)) {
        return intern_fmt(
            "(%s)%s",
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

    return intern_fmt(
        "(%s){.start = %s, .count = %zu}",
        mangle_type(expr->type),
        mangle_address(expr->data.bytes.address),
        expr->data.bytes.count);
}

static char const*
strgen_rvalue_array_list(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ARRAY_LIST);
    assert(expr->type->kind == TYPE_ARRAY);

    if (expr->type->size == 0) {
        return intern_fmt("/* %s */0", expr->type->name);
    }

    struct string* const s = string_new_fmt("(%s){", mangle_type(expr->type));

    sbuf(struct expr const* const) const elements =
        expr->data.array_list.elements;
    struct type const* const element_type = expr->type->data.array.base;
    for (size_t i = 0; i < sbuf_count(elements); ++i) {
        assert(elements[i]->type == element_type);
        if (i != 0) {
            string_append_cstr(s, ", ");
        }
        string_append_fmt(s, "%s", strgen_rvalue(elements[i]));
    }

    uint64_t const count = expr->type->data.array.count;
    struct expr const* const ellipsis = expr->data.array_list.ellipsis;
    assert(sbuf_count(elements) == count || ellipsis != NULL);
    for (size_t i = sbuf_count(elements); i < count; ++i) {
        if (i != 0) {
            string_append_cstr(s, ", ");
        }
        string_append_fmt(s, "%s", strgen_rvalue(ellipsis));
    }

    string_append_cstr(s, "}");

    char const* const interned = intern(string_start(s), string_count(s));
    string_del(s);
    return interned;
}

static char const*
strgen_rvalue_slice_list(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SLICE_LIST);
    assert(expr->type->kind == TYPE_SLICE);

    struct type const* const element_type = expr->type->data.slice.base;

    struct string* const s = string_new_fmt(
        "(%s){.start = (%s[]){",
        mangle_type(expr->type),
        mangle_type(element_type));

    sbuf(struct expr const* const) const elements =
        expr->data.slice_list.elements;
    for (size_t i = 0; i < sbuf_count(elements); ++i) {
        assert(elements[i]->type == element_type);
        if (i != 0) {
            string_append_cstr(s, ", ");
        }
        string_append_fmt(s, "%s", strgen_rvalue(elements[i]));
    }

    string_append_fmt(s, "}, .count = %zu}", sbuf_count(elements));

    char const* const interned = intern(string_start(s), string_count(s));
    string_del(s);
    return interned;
}

static char const*
strgen_rvalue_slice(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SLICE);
    assert(expr->type->kind == TYPE_SLICE);

    return intern_fmt(
        "(%s){.start = %s, .count = %s}",
        mangle_type(expr->type),
        strgen_rvalue(expr->data.slice.pointer),
        strgen_rvalue(expr->data.slice.count));
}

static char const*
strgen_rvalue_struct(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_STRUCT);
    assert(expr->type->kind == TYPE_STRUCT);

    sbuf(struct member_variable) const member_variable_defs =
        expr->type->data.struct_.member_variables;
    sbuf(struct expr const* const) const member_variable_exprs =
        expr->data.struct_.member_variables;
    assert(
        sbuf_count(member_variable_defs) == sbuf_count(member_variable_exprs));

    struct string* const s = string_new_fmt("({");
    for (size_t i = 0; i < sbuf_count(member_variable_defs); ++i) {
        char const* const local =
            intern_fmt("__member_%zu_%s", i, member_variable_defs[i].name);
        char const* const initname = mangle_name(local);
        char const* const typename = mangle_type(member_variable_defs[i].type);

        if (member_variable_defs[i].type->size == 0) {
            char const* const valuestr = member_variable_exprs[i] != NULL
                ? strgen_rvalue(member_variable_exprs[i])
                : "/* uninit */0";
            string_append_fmt(
                s,
                "int %s = /* zero-sized member */({%s; 0;}); ",
                initname,
                valuestr);
            continue;
        }

        char const* const valuestr = member_variable_exprs[i] != NULL
            ? strgen_rvalue(member_variable_exprs[i])
            : strgen_uninit(member_variable_defs[i].type);
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
        mangle_name("__result"),
        mangle_type(expr->type),
        strgen_uninit(expr->type));
    string_append_fmt(
        s,
        "/* zero padding */memset(&%s, 0x00, sizeof(%s)); ",
        mangle_name("__result"),
        mangle_name("__result"));
    for (size_t i = 0; i < sbuf_count(member_variable_defs); ++i) {
        if (member_variable_defs[i].type->size == 0) {
            continue;
        }
        char const* const local =
            intern_fmt("__member_%zu_%s", i, member_variable_defs[i].name);
        string_append_fmt(
            s,
            "%s.%s = %s; ",
            mangle_name("__result"),
            mangle_name(member_variable_defs[i].name),
            mangle_name(local));
    }
    string_append_fmt(s, "%s;", mangle_name("__result"));

done:
    string_append_cstr(s, "})");
    char const* const interned = intern(string_start(s), string_count(s));
    string_del(s);
    return interned;
}

static char const*
strgen_rvalue_cast(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_CAST);

    if (type_is_int(expr->type)
        && type_is_ieee754(expr->data.cast.expr->type)) {
        return intern_fmt(
            "__sunder___cast_%s_to_%s(%s)",
            mangle_type(expr->data.cast.expr->type),
            mangle_type(expr->type),
            strgen_rvalue(expr->data.cast.expr));
    }

    return intern_fmt(
        "(%s)%s", mangle_type(expr->type), strgen_rvalue(expr->data.cast.expr));
}

static char const*
strgen_rvalue_call(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_CALL);

    struct expr const* const function = expr->data.call.function;
    sbuf(struct expr const* const) const arguments = expr->data.call.arguments;

    struct string* const s = string_new_fmt("({");
    for (size_t i = 0; i < sbuf_count(arguments); ++i) {
        if (arguments[i]->type->size == 0) {
            continue;
        }

        char const* const local = intern_fmt("__argument_%zu", i + 1);
        char const* const initname = mangle_name(local);
        char const* const typename = mangle_type(arguments[i]->type);
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

        char const* const local = intern_fmt("__argument_%zu", i + 1);
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

    char const* const interned = intern(string_start(s), string_count(s));
    string_del(s);
    return interned;
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
        uint64_t const base_size =
            expr->data.access_index.lhs->type->data.array.base->size;
        char const* const lhs_type = lhs_is_zero_sized
            ? "int"
            : mangle_type(expr->data.access_index.lhs->type);
        char const* const elements = lhs_is_zero_sized
            ? intern_fmt("((%s*)0)", mangle_type(expr->type))
            : intern_fmt("%s.elements", mangle_name("__lhs"));
        return intern_fmt(
            // clang-format off
            "({%s %s = %s; %s %s = %s; if (%s >= %" PRId64 "){%s();}; *(%s*)((uintptr_t)%s + (%s * %" PRId64 "));})",
            // clang-format on
            lhs_type,
            mangle_name("__lhs"),
            strgen_rvalue(expr->data.access_index.lhs),

            mangle_type(expr->data.access_index.idx->type),
            mangle_name("__idx"),
            strgen_rvalue(expr->data.access_index.idx),

            mangle_name("__idx"),
            expr->data.access_index.lhs->type->data.array.count,
            mangle_name("__fatal_index_out_of_bounds"),

            mangle_type(expr->type),
            elements,
            mangle_name("__idx"),
            base_size);
    }

    if (expr->data.access_index.lhs->type->kind == TYPE_SLICE) {
        assert(!lhs_is_zero_sized);
        uint64_t const base_size =
            expr->data.access_index.lhs->type->data.slice.base->size;
        return intern_fmt(
            // clang-format off
            "({%s %s = %s; %s %s = %s; if (%s >= %s.count){%s();}; *(%s*)((uintptr_t)%s.start + (%s * %" PRId64 "));})",
            // clang-format on
            mangle_type(expr->data.access_index.lhs->type),
            mangle_name("__lhs"),
            strgen_rvalue(expr->data.access_index.lhs),

            mangle_type(expr->data.access_index.idx->type),
            mangle_name("__idx"),
            strgen_rvalue(expr->data.access_index.idx),

            mangle_name("__idx"),
            mangle_name("__lhs"),
            mangle_name("__fatal_index_out_of_bounds"),

            mangle_type(expr->type),
            mangle_name("__lhs"),
            mangle_name("__idx"),
            base_size);
    }

    UNREACHABLE();
}

static char const*
strgen_rvalue_access_slice(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_SLICE);

    char const* const ltype = mangle_type(expr->data.access_slice.lhs->type);
    char const* const lname = mangle_name("__lhs");
    char const* const lexpr = strgen_rvalue(expr->data.access_slice.lhs);

    char const* const btype = mangle_name("usize");
    char const* const bname = mangle_name("__b");
    char const* const bexpr = strgen_rvalue(expr->data.access_slice.begin);

    char const* const etype = mangle_name("usize");
    char const* const ename = mangle_name("__e");
    char const* const eexpr = strgen_rvalue(expr->data.access_slice.end);

    char const* const tname = mangle_type(expr->type);

    bool const lhs_is_zero_sized = expr->data.access_slice.lhs->type->size == 0;
    uint64_t const base_size = expr->type->data.slice.base->size;

    // According to the C standard, performing pointer arithmetic on a NULL
    // pointer has undefined behavior. Pointer addition is manually performed
    // with uintptr_t to avoid this undefined behavior.

    if (expr->data.access_slice.lhs->type->kind == TYPE_ARRAY) {
        char const* start = NULL;
        if (lhs_is_zero_sized) {
            start = intern_fmt(
                // clang-format off
                "(%s*)((uintptr_t)%s * %" PRId64 ")",
                // clang-format on
                mangle_type(expr->type->data.slice.base),
                bname,
                base_size);
        }
        else {
            start = intern_fmt(
                // clang-format off
                "(%s*)((uintptr_t)(%s).elements + ((uintptr_t)%s * %" PRId64 "))",
                // clang-format on
                mangle_type(expr->type->data.slice.base),
                lexpr,
                bname,
                base_size);
        }
        char const* const count = intern_fmt("%s - %s", ename, bname);
        return intern_fmt(
            // clang-format off
            "({%s %s = %s; %s %s = %s; if ((%s > %s) || (%s > %" PRId64 ") || (%s > %" PRId64 ")){%s();}; (%s){.start = %s, .count = %s};})",
            // clang-format on
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
            mangle_name("__fatal_index_out_of_bounds"),

            tname,
            start,
            count);
    }

    if (expr->data.access_slice.lhs->type->kind == TYPE_SLICE) {
        assert(!lhs_is_zero_sized);
        char const* const start = intern_fmt(
            "(%s*)((uintptr_t)%s.start + ((uintptr_t)%s * %" PRId64 "))",
            mangle_type(expr->type->data.slice.base),
            lname,
            bname,
            base_size);
        char const* const count = intern_fmt("%s - %s", ename, bname);
        return intern_fmt(
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
            mangle_name("__fatal_index_out_of_bounds"),

            tname,
            start,
            count);
    }

    UNREACHABLE();
}

static char const*
strgen_rvalue_access_member_variable(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_MEMBER_VARIABLE);
    assert(expr->data.access_member_variable.lhs->type->kind == TYPE_STRUCT);

    if (expr->type->size == 0) {
        return intern_fmt(
            "({/* zero-sized member */(%s); 0;})",
            strgen_rvalue(expr->data.access_member_variable.lhs));
    }

    return intern_fmt(
        "(%s).%s",
        strgen_rvalue(expr->data.access_member_variable.lhs),
        mangle_name(expr->data.access_member_variable.member_variable->name));
}

static char const*
strgen_rvalue_sizeof(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SIZEOF);

    return intern_fmt("%" PRId64, expr->data.sizeof_.rhs->size);
}

static char const*
strgen_rvalue_alignof(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ALIGNOF);

    return intern_fmt("%" PRId64, expr->data.alignof_.rhs->align);
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
    case UOP_ADDRESSOF: {
        return strgen_rvalue_unary_addressof(expr);
    }
    case UOP_STARTOF: {
        return strgen_rvalue_unary_startof(expr);
    }
    case UOP_COUNTOF: {
        return strgen_rvalue_unary_countof(expr);
    }
    }

    UNREACHABLE();
}

static char const*
strgen_rvalue_unary_not(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_NOT);

    return intern_fmt("!(%s)", strgen_rvalue(expr->data.unary.rhs));
}

static char const*
strgen_rvalue_unary_pos(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_POS);

    return intern_fmt("+(%s)", strgen_rvalue(expr->data.unary.rhs));
}

static char const*
strgen_rvalue_unary_neg(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_NEG);

    if (type_is_ieee754(expr->type)) {
        return intern_fmt("-(%s)", strgen_rvalue(expr->data.unary.rhs));
    }

    assert(type_is_sint(expr->data.unary.rhs->type));
    return intern_fmt(
        "({%s %s = %s; if (%s == ((%s)(LLONG_MIN >> ((sizeof(long long) - sizeof(%s))*8)))){%s();}; -(%s);})",
        mangle_type(expr->data.unary.rhs->type),
        mangle_name("__rhs"),
        strgen_rvalue(expr->data.unary.rhs),

        mangle_name("__rhs"),
        mangle_type(expr->data.unary.rhs->type),
        mangle_type(expr->data.unary.rhs->type),
        mangle_name("__fatal_out_of_range"),

        mangle_name("__rhs"));
}

static char const*
strgen_rvalue_unary_neg_wrapping(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_NEG_WRAPPING);
    assert(type_is_sint(expr->data.unary.rhs->type));

    // Negating a negative number that can not be represented as a positive
    // number (e.g. T::MIN for signed integer type T) is undefined behavior in
    // C, so implement negation "manually" using two's complement negation.
    return intern_fmt(
        "({%s %s = ~(%s); __builtin_add_overflow(%s, (%s)1, &%s); %s;})",
        mangle_type(expr->type),
        mangle_name("__result"),
        strgen_rvalue(expr->data.unary.rhs),

        mangle_name("__result"),
        mangle_type(expr->type),
        mangle_name("__result"),

        mangle_name("__result"));
}

static char const*
strgen_rvalue_unary_bitnot(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_BITNOT);

    return intern_fmt("~(%s)", strgen_rvalue(expr->data.unary.rhs));
}

static char const*
strgen_rvalue_unary_dereference(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_DEREFERENCE);
    assert(expr->data.unary.rhs->type->kind == TYPE_POINTER);

    if (expr->type->size == 0) {
        return intern_fmt(
            "({/* dereference pointer to zero-sized type */(%s); 0;})",
            strgen_rvalue(expr->data.unary.rhs));
    }
    return intern_fmt("*(%s)", strgen_rvalue(expr->data.unary.rhs));
}

static char const*
strgen_rvalue_unary_addressof(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_ADDRESSOF);

    return strgen_lvalue(expr->data.unary.rhs);
}

static char const*
strgen_rvalue_unary_startof(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);
    assert(expr->data.unary.op == UOP_STARTOF);
    assert(expr->data.unary.rhs->type->kind == TYPE_SLICE);

    return intern_fmt("(%s).start", strgen_rvalue(expr->data.unary.rhs));
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
            return intern_fmt(
                "({%s; %" PRIu64 ";})",
                strgen_lvalue(expr->data.unary.rhs),
                expr->data.unary.rhs->type->data.array.count);
        }

        return intern_fmt(
            "({%s; %" PRIu64 ";})",
            strgen_rvalue(expr->data.unary.rhs),
            expr->data.unary.rhs->type->data.array.count);
    }

    if (expr->data.unary.rhs->type->kind == TYPE_SLICE) {
        return intern_fmt("(%s).count", strgen_rvalue(expr->data.unary.rhs));
    }

    UNREACHABLE();
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
}

static char const*
strgen_rvalue_binary_or(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_OR);
    assert(expr->data.binary.lhs->type->kind == TYPE_BOOL);
    assert(expr->data.binary.rhs->type->kind == TYPE_BOOL);

    return intern_fmt(
        "((%s) || (%s))",
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

    return intern_fmt(
        "((%s) && (%s))",
        strgen_rvalue(expr->data.binary.lhs),
        strgen_rvalue(expr->data.binary.rhs));
}

static char const*
strgen_rvalue_binary_shl(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_SHL);
    assert(type_is_int(expr->data.binary.lhs->type));
    assert(expr->data.binary.rhs->type->kind == TYPE_USIZE);

    // Left shift of a negative number is undefined behavior in C, even though
    // the behavior is well defined in Sunder (-1s8 << 1u == -2s8). Always
    // perform left shifts using unsigned values and then cast back to the
    // correct type to avoid undefined behavior.
    return intern_fmt(
        "({%s %s = %s; %s %s = %s; %s < sizeof(%s)*8 ? (%s)((%s)%s << %s) : (%s)0;})",
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        mangle_name("__rhs"),
        strgen_rvalue(expr->data.binary.rhs),

        mangle_name("__rhs"),
        mangle_type(expr->data.binary.lhs->type),

        mangle_type(expr->data.binary.lhs->type),
        mangle_type(expr->data.binary.rhs->type),
        mangle_name("__lhs"),
        mangle_name("__rhs"),

        mangle_type(expr->data.binary.lhs->type));
}

static char const*
strgen_rvalue_binary_shr(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_SHR);
    assert(type_is_int(expr->data.binary.lhs->type));
    assert(expr->data.binary.rhs->type->kind == TYPE_USIZE);

    char const* const overshift = type_is_sint(expr->data.binary.lhs->type)
        ? intern_fmt("((%s < 0) ? -1 : 0)", mangle_name("__lhs"))
        : "0";
    return intern_fmt(
        // clang-format off
        "({%s %s = %s; %s %s = %s; %s < sizeof(%s)*8 ? (%s >> %s) : (%s)%s;})",
        // clang-format on
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        mangle_name("__rhs"),
        strgen_rvalue(expr->data.binary.rhs),

        mangle_name("__rhs"),
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        mangle_name("__rhs"),
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

    return intern_fmt(
        "((%s) == (%s))",
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

    return intern_fmt(
        "((%s) != (%s))",
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

    return intern_fmt(
        "((%s) <= (%s))",
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

    return intern_fmt(
        "((%s) < (%s))",
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

    return intern_fmt(
        "((%s) >= (%s))",
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

    return intern_fmt(
        "((%s) > (%s))",
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
        return intern_fmt(
            "(%s + %s)",
            strgen_rvalue(expr->data.binary.lhs),
            strgen_rvalue(expr->data.binary.rhs));
    }

    assert(type_is_int(expr->type) && expr->type->size != SIZEOF_UNSIZED);
    return intern_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        mangle_name("__rhs"),
        strgen_rvalue(expr->data.binary.rhs),

        mangle_name("__add"),
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        mangle_name("__rhs"));
}

static char const*
strgen_rvalue_binary_add_wrapping(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_ADD_WRAPPING);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
    assert(type_is_int(expr->type) && expr->type->size != SIZEOF_UNSIZED);

    return intern_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        mangle_name("__rhs"),
        strgen_rvalue(expr->data.binary.rhs),

        mangle_name("__add_wrapping"),
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        mangle_name("__rhs"));
}

static char const*
strgen_rvalue_binary_sub(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_SUB);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    if (type_is_ieee754(expr->type)) {
        return intern_fmt(
            "(%s - %s)",
            strgen_rvalue(expr->data.binary.lhs),
            strgen_rvalue(expr->data.binary.rhs));
    }

    assert(type_is_int(expr->type) && expr->type->size != SIZEOF_UNSIZED);
    return intern_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        mangle_name("__rhs"),
        strgen_rvalue(expr->data.binary.rhs),

        mangle_name("__sub"),
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        mangle_name("__rhs"));
}

static char const*
strgen_rvalue_binary_sub_wrapping(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_SUB_WRAPPING);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
    assert(type_is_int(expr->type) && expr->type->size != SIZEOF_UNSIZED);

    return intern_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        mangle_name("__rhs"),
        strgen_rvalue(expr->data.binary.rhs),

        mangle_name("__sub_wrapping"),
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        mangle_name("__rhs"));
}

static char const*
strgen_rvalue_binary_mul(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_MUL);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    if (type_is_ieee754(expr->type)) {
        return intern_fmt(
            "(%s * %s)",
            strgen_rvalue(expr->data.binary.lhs),
            strgen_rvalue(expr->data.binary.rhs));
    }

    assert(type_is_int(expr->type) && expr->type->size != SIZEOF_UNSIZED);
    return intern_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        mangle_name("__rhs"),
        strgen_rvalue(expr->data.binary.rhs),

        mangle_name("__mul"),
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        mangle_name("__rhs"));
}

static char const*
strgen_rvalue_binary_mul_wrapping(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_MUL_WRAPPING);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
    assert(type_is_int(expr->type) && expr->type->size != SIZEOF_UNSIZED);

    return intern_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        mangle_name("__rhs"),
        strgen_rvalue(expr->data.binary.rhs),

        mangle_name("__mul_wrapping"),
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        mangle_name("__rhs"));
}

static char const*
strgen_rvalue_binary_div(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_DIV);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    if (type_is_ieee754(expr->type)) {
        return intern_fmt(
            "(%s / %s)",
            strgen_rvalue(expr->data.binary.lhs),
            strgen_rvalue(expr->data.binary.rhs));
    }

    assert(type_is_int(expr->type) && expr->type->size != SIZEOF_UNSIZED);
    return intern_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        mangle_name("__rhs"),
        strgen_rvalue(expr->data.binary.rhs),

        mangle_name("__div"),
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        mangle_name("__rhs"));
}

static char const*
strgen_rvalue_binary_rem(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_REM);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);
    assert(type_is_int(expr->type) && expr->type->size != SIZEOF_UNSIZED);

    return intern_fmt(
        "({%s %s = %s; %s %s = %s; %s_%s(%s, %s);})",
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        strgen_rvalue(expr->data.binary.lhs),

        mangle_type(expr->data.binary.rhs->type),
        mangle_name("__rhs"),
        strgen_rvalue(expr->data.binary.rhs),

        mangle_name("__rem"),
        mangle_type(expr->data.binary.lhs->type),
        mangle_name("__lhs"),
        mangle_name("__rhs"));
}

static char const*
strgen_rvalue_binary_bitor(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);
    assert(expr->data.binary.op == BOP_BITOR);
    assert(expr->data.binary.lhs->type == expr->data.binary.rhs->type);

    return intern_fmt(
        "((%s) | (%s))",
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

    return intern_fmt(
        "((%s) ^ (%s))",
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

    return intern_fmt(
        "((%s) & (%s))",
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
        TABLE_ENTRY(EXPR_ACCESS_INDEX, strgen_lvalue_access_index),
        TABLE_ENTRY(EXPR_ACCESS_MEMBER_VARIABLE, strgen_lvalue_access_member_variable),
        TABLE_ENTRY(EXPR_UNARY, strgen_lvalue_unary),
#undef TABLE_ENTRY
    };
    // clang-format on

    char const* const cstr = table[expr->kind].kind_cstr;
    appendli_location(expr->location, "LVALUE EXPRESSION %s", cstr);
    return table[expr->kind].function(expr);
}

static char const*
strgen_lvalue_symbol(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SYMBOL);

    if (expr->type->size == 0) {
        return intern_fmt(
            "/* zero-sized symbol */((%s*)0)", mangle_type(expr->type));
    }

    return intern_fmt("&%s", mangle_symbol(expr->data.symbol));
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
        uint64_t const base_size =
            expr->data.access_index.lhs->type->data.array.base->size;
        char const* const lhs_type = lhs_is_zero_sized
            ? "int"
            : mangle_type(expr->data.access_index.lhs->type);
        char const* const elements = lhs_is_zero_sized
            ? intern_fmt("((%s*)0)", mangle_type(expr->type))
            : intern_fmt("%s->elements", mangle_name("__lhs"));
        return intern_fmt(
            // clang-format off
            "({%s* %s = %s; %s %s = %s; if (%s >= %" PRId64 "){%s();}; (%s*)((uintptr_t)%s + (%s * %" PRId64 "));})",
            // clang-format on
            lhs_type,
            mangle_name("__lhs"),
            strgen_lvalue(expr->data.access_index.lhs),

            mangle_type(expr->data.access_index.idx->type),
            mangle_name("__idx"),
            strgen_rvalue(expr->data.access_index.idx),

            mangle_name("__idx"),
            expr->data.access_index.lhs->type->data.array.count,
            mangle_name("__fatal_index_out_of_bounds"),

            mangle_type(expr->type),
            elements,
            mangle_name("__idx"),
            base_size);
    }

    if (expr->data.access_index.lhs->type->kind == TYPE_SLICE) {
        uint64_t const base_size =
            expr->data.access_index.lhs->type->data.slice.base->size;
        return intern_fmt(
            // clang-format off
            "({%s %s = %s; %s %s = %s; if (%s >= %s.count){%s();}; (%s*)((uintptr_t)%s.start + (%s * %" PRId64 "));})",
            // clang-format on
            mangle_type(expr->data.access_index.lhs->type),
            mangle_name("__lhs"),
            strgen_rvalue(expr->data.access_index.lhs),

            mangle_type(expr->data.access_index.idx->type),
            mangle_name("__idx"),
            strgen_rvalue(expr->data.access_index.idx),

            mangle_name("__idx"),
            mangle_name("__lhs"),
            mangle_name("__fatal_index_out_of_bounds"),

            mangle_type(expr->type),
            mangle_name("__lhs"),
            mangle_name("__idx"),
            base_size);
    }

    UNREACHABLE();
}

static char const*
strgen_lvalue_access_member_variable(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_MEMBER_VARIABLE);
    assert(expr->data.access_member_variable.lhs->type->kind == TYPE_STRUCT);

    char const* const lexpr =
        strgen_lvalue(expr->data.access_member_variable.lhs);

    return intern_fmt(
        "(%s*)(/* object */(uintptr_t)(%s) + /* offset */%" PRId64 ")",
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
    case UOP_ADDRESSOF: /* fallthrough */
    case UOP_STARTOF: /* fallthrough */
    case UOP_COUNTOF: {
        UNREACHABLE();
    }
    }

    UNREACHABLE();
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
    bool opt_c,
    bool opt_k,
    char const* const* opt_L,
    char const* const* opt_l,
    char const* const opt_o)
{
    assert(opt_o != NULL);
    assert(0 == strcmp(backend(), "C") || 0 == strcmp(backend(), "c"));

    out = string_new(NULL, 0u);
    struct string* const src_path = string_new_fmt("%s.tmp.c", opt_o);
    struct string* const obj_path = string_new_fmt("%s.tmp.o", opt_o);

    char const* const SUNDER_HOME = getenv("SUNDER_HOME");
    if (SUNDER_HOME == NULL) {
        fatal(NO_LOCATION, "missing environment variable SUNDER_HOME");
    }

    sbuf(char const*) backend_argv = NULL;
    // Currently using Clang as the C compiler as GCC produces an ASan error
    // when executing code generated for std::big_integer::init_from_str, which
    // may or may not be a false positive. Clang does not produce this error.
    // See tests/c-backend-gcc-asan-stack-use-after-scope.test.sunder for
    // additional details.
    sbuf_push(backend_argv, "clang");
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
    sbuf_push(backend_argv, "-g");
    sbuf_push(backend_argv, intern_fmt("-I%s/lib/sys", SUNDER_HOME));
    sbuf_push(backend_argv, "-std=c11");
    sbuf_push(backend_argv, "-Wall");
    sbuf_push(backend_argv, "-Wextra");
    // Workaround for a GCC bug where the universal struct zero-initializer for
    // types with nested struct objects produces a missing braces warning.
    sbuf_push(backend_argv, "-Wno-missing-braces");
    // Not useful for auto-generated C code.
    sbuf_push(backend_argv, "-Wno-parentheses-equality");
    // Sunder does not have type qualifiers.
    // GCC-specific flag.
    /* sbuf_push(backend_argv, "-Wno-discarded-qualifiers"); */
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
#if 0
    sbuf_push(backend_argv, "-fsanitize=address");
    sbuf_push(backend_argv, "-fsanitize=leak");
    sbuf_push(backend_argv, "-fsanitize=undefined");
#endif
    sbuf_push(backend_argv, string_start(src_path));
    if (!opt_c) {
        for (size_t i = 0; i < sbuf_count(opt_L); ++i) {
            sbuf_push(backend_argv, intern_fmt("-L%s", opt_L[i]));
        }
        sbuf_push(backend_argv, "-lm");
        for (size_t i = 0; i < sbuf_count(opt_l); ++i) {
            sbuf_push(backend_argv, intern_fmt("-l%s", opt_l[i]));
        }
    }
    sbuf_push(backend_argv, (char const*)NULL);

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
    string_del(out);
    if (err) {
        exit(EXIT_FAILURE);
    }
}
