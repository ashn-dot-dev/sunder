// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sunder.h"

static bool
integer_is_out_of_range(struct type const* type, struct bigint const* res);

static struct value*
eval_rvalue_symbol(struct expr const* expr);
static struct value*
eval_rvalue_value(struct expr const* expr);
static struct value*
eval_rvalue_bytes(struct expr const* expr);
static struct value*
eval_rvalue_array_list(struct expr const* expr);
static struct value*
eval_rvalue_slice_list(struct expr const* expr);
static struct value*
eval_rvalue_slice(struct expr const* expr);
static struct value*
eval_rvalue_struct(struct expr const* expr);
static struct value*
eval_rvalue_cast(struct expr const* expr);
static struct value*
eval_rvalue_access_index(struct expr const* expr);
static struct value*
eval_rvalue_access_slice(struct expr const* expr);
static struct value*
eval_rvalue_access_member_variable(struct expr const* expr);
static struct value*
eval_rvalue_sizeof(struct expr const* expr);
static struct value*
eval_rvalue_alignof(struct expr const* expr);
static struct value*
eval_rvalue_unary(struct expr const* expr);
static struct value*
eval_rvalue_binary(struct expr const* expr);

static struct value*
eval_lvalue_symbol(struct expr const* expr);
static struct value*
eval_lvalue_access_index(struct expr const* expr);
static struct value*
eval_lvalue_access_member_variable(struct expr const* expr);
static struct value*
eval_lvalue_unary(struct expr const* expr);

static bool
integer_is_out_of_range(struct type const* type, struct bigint const* res)
{
    assert(type != NULL);
    assert(type_is_int(type));
    assert(res != NULL);

    if (type->kind == TYPE_INTEGER) {
        assert(type->data.integer.min == NULL);
        assert(type->data.integer.max == NULL);
        // Arbitrary precision integers do not have a defined min or max value.
        return false;
    }
    return bigint_cmp(res, type->data.integer.min) < 0
        || bigint_cmp(res, type->data.integer.max) > 0;
}

struct value*
eval_rvalue(struct expr const* expr)
{
    assert(expr != NULL);

    switch (expr->kind) {
    case EXPR_SYMBOL: {
        return eval_rvalue_symbol(expr);
    }
    case EXPR_VALUE: {
        return eval_rvalue_value(expr);
    }
    case EXPR_BYTES: {
        return eval_rvalue_bytes(expr);
    }
    case EXPR_ARRAY_LIST: {
        return eval_rvalue_array_list(expr);
    }
    case EXPR_SLICE_LIST: {
        return eval_rvalue_slice_list(expr);
    }
    case EXPR_SLICE: {
        return eval_rvalue_slice(expr);
    }
    case EXPR_STRUCT: {
        return eval_rvalue_struct(expr);
    }
    case EXPR_CAST: {
        return eval_rvalue_cast(expr);
    }
    case EXPR_CALL: {
        fatal(expr->location, "constant expression contains function call");
    }
    case EXPR_ACCESS_INDEX: {
        return eval_rvalue_access_index(expr);
    }
    case EXPR_ACCESS_SLICE: {
        return eval_rvalue_access_slice(expr);
    }
    case EXPR_ACCESS_MEMBER_VARIABLE: {
        return eval_rvalue_access_member_variable(expr);
    }
    case EXPR_SIZEOF: {
        return eval_rvalue_sizeof(expr);
    }
    case EXPR_ALIGNOF: {
        return eval_rvalue_alignof(expr);
    }
    case EXPR_UNARY: {
        return eval_rvalue_unary(expr);
    }
    case EXPR_BINARY: {
        return eval_rvalue_binary(expr);
    }
    }

    UNREACHABLE();
    return NULL;
}

static struct value*
eval_rvalue_symbol(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SYMBOL);

    struct symbol const* const symbol = expr->data.symbol;
    enum symbol_kind const kind = symbol->kind;

    if (kind == SYMBOL_CONSTANT || kind == SYMBOL_FUNCTION) {
        return value_clone(symbol_xget_value(expr->location, symbol));
    }

    fatal(expr->location, "identifier `%s` is not a constant", symbol->name);
    return NULL;
}

static struct value*
eval_rvalue_value(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_VALUE);

    return value_clone(expr->data.value);
}

static struct value*
eval_rvalue_bytes(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BYTES);

    struct value* const pointer = value_new_pointer(
        type_unique_pointer(context()->builtin.byte),
        *expr->data.bytes.address);

    struct value* const count = value_new_integer(
        context()->builtin.usize, bigint_new_umax(expr->data.bytes.count));

    return value_new_slice(expr->type, pointer, count);
}

static struct value*
eval_rvalue_array_list(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ARRAY_LIST);

    sbuf(struct expr const* const) elements = expr->data.array_list.elements;
    sbuf(struct value*) evaled_elements = NULL;
    for (size_t i = 0; i < sbuf_count(elements); ++i) {
        sbuf_push(evaled_elements, eval_rvalue(elements[i]));
    }

    struct value* evaled_ellipsis = NULL;
    if (expr->data.array_list.ellipsis != NULL) {
        evaled_ellipsis = eval_rvalue(expr->data.array_list.ellipsis);
    }

    return value_new_array(expr->type, evaled_elements, evaled_ellipsis);
}

static struct value*
eval_rvalue_slice_list(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SLICE_LIST);
    assert(expr->type->kind == TYPE_SLICE);

    struct address const* const address =
        symbol_xget_address(expr->data.slice_list.array_symbol);
    assert(address->kind == ADDRESS_STATIC);
    struct value* const pointer = value_new_pointer(
        type_unique_pointer(expr->type->data.slice.base), *address);

    sbuf(struct expr const* const) const elements =
        expr->data.slice_list.elements;
    struct value* const count = value_new_integer(
        context()->builtin.usize, bigint_new_umax(sbuf_count(elements)));

    return value_new_slice(expr->type, pointer, count);
}

static struct value*
eval_rvalue_slice(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SLICE);

    struct value* const pointer = eval_rvalue(expr->data.slice.pointer);
    struct value* const count = eval_rvalue(expr->data.slice.count);
    return value_new_slice(expr->type, pointer, count);
}

static struct value*
eval_rvalue_struct(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_STRUCT);

    struct value* const value = value_new_struct(expr->type);

    sbuf(struct member_variable) const defs =
        expr->type->data.struct_.member_variables;
    sbuf(struct expr const* const) const exprs =
        expr->data.struct_.member_variables;

    assert(sbuf_count(defs) == sbuf_count(exprs));
    for (size_t i = 0; i < sbuf_count(exprs); ++i) {
        if (exprs[i] == NULL) {
            continue;
        }
        value_set_member(value, defs[i].name, eval_rvalue(exprs[i]));
    }

    return value;
}

static struct value*
eval_rvalue_cast(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_CAST);

    struct value* const from = eval_rvalue(expr->data.cast.expr);

    // Check if the value casted from is already the correct type. Also allows
    // us to assume `from->type->kind != expr->type->kind` from this point
    // forward. Illegal casts of the same type kind (e.g. cast []u16 to []u32)
    // should have produced a fatal error during the resolve phase.
    assert(from->type->kind != TYPE_ANY);
    assert(from->type->kind != TYPE_VOID);
    assert(from->type->kind != TYPE_ARRAY);
    assert(from->type->kind != TYPE_SLICE);
    assert(from->type->kind != TYPE_STRUCT);
    if (from->type->kind == expr->type->kind) {
        return from;
    }

    // The representation of a non-absolute address is chosen by the
    // assembler/linker and has no meaningful representation at compile-time.
    if (from->type->kind == TYPE_POINTER) {
        fatal(
            expr->location,
            "constant expression contains cast from pointer type `%s` to non-pointer type `%s`",
            from->type->name,
            expr->type->name);
    }

    // Casting from a compile-time known usize value to absolute address.
    if (expr->type->kind == TYPE_POINTER) {
        switch (from->type->kind) {
        case TYPE_USIZE: {
            uint64_t absolute = 0;
            if (bigint_to_u64(&absolute, from->data.integer)) {
                UNREACHABLE();
            }
            struct value* const result =
                value_new_pointer(expr->type, address_init_absolute(absolute));
            value_del(from);
            return result;
        }
        case TYPE_FUNCTION: {
            struct value* const result =
                value_new_pointer(expr->type, *from->data.function->address);
            value_del(from);
            return result;
        }
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
        case TYPE_SSIZE: /* fallthrough */
        case TYPE_INTEGER: /* fallthrough */
        case TYPE_F32: /* fallthrough */
        case TYPE_F64: /* fallthrough */
        case TYPE_POINTER: /* fallthrough */
        case TYPE_ARRAY: /* fallthrough */
        case TYPE_SLICE: /* fallthrough */
        case TYPE_STRUCT: {
            UNREACHABLE();
        }
        }
    }

    // Special cases when casting from unsized integers. Check to make sure
    // that the value of the integer expression can fit into the range of the
    // casted-to type for byte and sized integer types. Unsized integers may
    // only appear in integer constant expressions, so evaluating the rhs
    // expression should always produce a constant value.
    if (expr->type->kind == TYPE_BOOL && from->type->kind == TYPE_INTEGER) {
        struct value* const result =
            value_new_boolean(bigint_cmp(from->data.integer, BIGINT_ZERO) != 0);
        value_del(from);
        return result;
    }
    if (expr->type->kind == TYPE_BYTE && from->type->kind == TYPE_INTEGER) {
        struct bigint const* const min = context()->u8_min;
        struct bigint const* const max = context()->u8_max;

        if (bigint_cmp(from->data.integer, min) < 0) {
            fatal(
                expr->location,
                "out-of-range conversion from `%s` to `%s` (%s < %s)",
                from->type->name,
                expr->type->name,
                bigint_to_new_cstr(from->data.integer),
                bigint_to_new_cstr(min));
        }
        if (bigint_cmp(from->data.integer, max) > 0) {
            fatal(
                expr->location,
                "out-of-range conversion from `%s` to `%s` (%s > %s)",
                from->type->name,
                expr->type->name,
                bigint_to_new_cstr(from->data.integer),
                bigint_to_new_cstr(max));
        }

        uint8_t byte = 0;
        int const out_of_range = bigint_to_u8(&byte, from->data.integer);
        if (out_of_range) {
            UNREACHABLE();
        }

        struct value* const result = value_new_byte(byte);
        value_del(from);
        return result;
    }
    if (type_is_int(expr->type) && from->type->kind == TYPE_INTEGER) {
        assert(expr->type->data.integer.min != NULL);
        assert(expr->type->data.integer.max != NULL);
        struct bigint const* const min = expr->type->data.integer.min;
        struct bigint const* const max = expr->type->data.integer.max;

        if (bigint_cmp(from->data.integer, min) < 0) {
            fatal(
                expr->location,
                "out-of-range conversion from `%s` to `%s` (%s < %s)",
                from->type->name,
                expr->type->name,
                bigint_to_new_cstr(from->data.integer),
                bigint_to_new_cstr(min));
        }
        if (bigint_cmp(from->data.integer, max) > 0) {
            fatal(
                expr->location,
                "out-of-range conversion from `%s` to `%s` (%s > %s)",
                from->type->name,
                expr->type->name,
                bigint_to_new_cstr(from->data.integer),
                bigint_to_new_cstr(max));
        }

        struct value* const result =
            value_new_integer(expr->type, bigint_new(from->data.integer));
        value_del(from);
        return result;
    }

    // Cases casting between integer and IEEE754 floating point types.
    if (type_is_ieee754(expr->type) && type_is_int(from->type)) {
        assert(expr->type->kind == TYPE_F32 || expr->type->kind == TYPE_F64);

        struct bigint const* const integer = from->data.integer;
        if (from->type->kind == TYPE_INTEGER) {
            struct bigint const* const min = expr->type->kind == TYPE_F64
                ? context()->f64_integer_min
                : context()->f32_integer_min;
            struct bigint const* const max = expr->type->kind == TYPE_F64
                ? context()->f64_integer_max
                : context()->f32_integer_max;
            bool const integer_ge_min = bigint_cmp(integer, min) >= 0;
            bool const integer_le_max = bigint_cmp(integer, max) <= 0;
            bool const is_representable = integer_ge_min && integer_le_max;
            if (!is_representable) {
                fatal(
                    expr->location,
                    "constant expression contains cast from integer type `%s` to floating point type `%s` with unrepresentable value %s",
                    from->type->name,
                    expr->type->name,
                    bigint_to_new_cstr(integer));
            }
        }

        intmax_t smax = 0;
        if (bigint_to_smax(&smax, integer)) {
            UNREACHABLE();
        }

        struct value* const result = expr->type->kind == TYPE_F64
            ? value_new_f64((double)smax)
            : value_new_f32((float)smax);
        value_del(from);
        return result;
    }
    if (type_is_int(expr->type) && type_is_ieee754(from->type)) {
        assert(expr->type->kind != TYPE_INTEGER);
        assert(from->type->kind == TYPE_F32 || from->type->kind == TYPE_F64);
        double const from_as_double = from->type->kind == TYPE_F64
            ? from->data.f64
            : (double)from->data.f32;
        bool const is_finite_f32 =
            (from->type->kind == TYPE_F32 && isfinite(from->data.f32));
        bool const is_finite_f64 =
            (from->type->kind == TYPE_F64 && isfinite(from->data.f64));
        bool const is_finite = is_finite_f32 || is_finite_f64;
        if (!is_finite) {
            fatal(
                expr->location,
                "constant expression contains cast from floating point type `%s` to integer type `%s` with unrepresentable value %f",
                from->type->name,
                expr->type->name,
                from_as_double);
        }

        assert(expr->type->data.integer.min != NULL);
        assert(expr->type->data.integer.max != NULL);
        if (type_is_uint(expr->type)) {
            uintmax_t min_as_umax = 0;
            uintmax_t max_as_umax = 0;
            if (bigint_to_umax(&min_as_umax, expr->type->data.integer.min)) {
                UNREACHABLE();
            }
            if (bigint_to_umax(&max_as_umax, expr->type->data.integer.max)) {
                UNREACHABLE();
            }
            uintmax_t from_as_umax = (uintmax_t)from_as_double;
            if (from_as_umax < min_as_umax || max_as_umax < from_as_umax) {
                fatal(
                    expr->location,
                    "operation produces out-of-range result");
            }

            struct bigint* const integer = bigint_new_umax(from_as_umax);
            struct value* const result = value_new_integer(expr->type, integer);
            value_del(from);
            return result;
        }
        if (type_is_sint(expr->type)) {
            intmax_t min_as_smax = 0;
            intmax_t max_as_smax = 0;
            if (bigint_to_smax(&min_as_smax, expr->type->data.integer.min)) {
                UNREACHABLE();
            }
            if (bigint_to_smax(&max_as_smax, expr->type->data.integer.max)) {
                UNREACHABLE();
            }
            intmax_t from_as_smax = (intmax_t)from_as_double;
            if (from_as_smax < min_as_smax || max_as_smax < from_as_smax) {
                fatal(
                    expr->location,
                    "operation produces out-of-range result");
            }

            struct bigint* const integer = bigint_new_smax(from_as_smax);
            struct value* const result = value_new_integer(expr->type, integer);
            value_del(from);
            return result;
        }
        UNREACHABLE();
    }

    // Cases casting from sized types with a defined byte representation.
    sbuf(uint8_t) bytes = value_to_new_bytes(from);
    struct value* res = NULL;
    switch (expr->type->kind) {
    case TYPE_BOOL: {
        bool boolean = false;
        for (size_t i = 0; i < sbuf_count(bytes); ++i) {
            boolean |= bytes[i] != 0;
        }
        res = value_new_boolean(boolean);
        break;
    }
    case TYPE_BYTE: {
        assert(sbuf_count(bytes) >= 1);
        res = value_new_byte(bytes[0]);
        break;
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
        // Zero-extension or sign-extension bit.
        size_t bytes_count = sbuf_count(bytes);
        int const extend =
            type_is_sint(from->type) && (bytes[bytes_count - 1] & 0x80);

        size_t const bit_count = (size_t)expr->type->size * 8u;
        struct bitarr* const bits = bitarr_new(bit_count);
        for (size_t i = 0; i < bit_count; ++i) {
            if (i >= (bytes_count * 8u)) {
                bitarr_set(bits, i, extend);
                continue;
            }
            unsigned const byte = bytes[i / 8u];
            unsigned const mask = 1u << (i % 8u);
            int const bit = (byte & mask) != 0;
            bitarr_set(bits, i, bit);
        }

        struct bigint* const integer =
            bigint_new_bitarr(bits, type_is_sint(expr->type));
        bitarr_del(bits);

        res = value_new_integer(expr->type, integer);
        break;
    }
    case TYPE_ANY: /* fallthrough */
    case TYPE_VOID: /* fallthrough */
    case TYPE_INTEGER: /* fallthrough */
    case TYPE_F32: /* fallthrough */
    case TYPE_F64: /* fallthrough */
    case TYPE_FUNCTION: /* fallthrough */
    case TYPE_POINTER: /* fallthrough */
    case TYPE_ARRAY: /* fallthrough */
    case TYPE_SLICE: /* fallthrough */
    case TYPE_STRUCT: {
        UNREACHABLE();
    }
    }

    value_del(from);
    sbuf_fini(bytes);
    return res;
}

static struct value*
eval_rvalue_access_index(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);

    struct value* const lhs = eval_rvalue(expr->data.access_index.lhs);
    struct value* const idx = eval_rvalue(expr->data.access_index.idx);

    assert(idx->type->kind == TYPE_USIZE);
    struct bigint const* const idx_bigint = idx->data.integer;
    size_t idx_uz = 0u;
    if (bigint_to_uz(&idx_uz, idx_bigint)) {
        fatal(
            expr->data.access_index.idx->location,
            "index out-of-range (received %s)",
            bigint_to_new_cstr(idx_bigint));
    }

    if (lhs->type->kind == TYPE_ARRAY) {
        if (idx_uz >= lhs->type->data.array.count) {
            fatal(
                expr->data.access_index.idx->location,
                "index out-of-bounds (array count is %" PRIu64
                ", received %zu)",
                lhs->type->data.array.count,
                idx_uz);
        }

        sbuf(struct value*) const elements = lhs->data.array.elements;
        struct value* const ellipsis = lhs->data.array.ellipsis;
        assert(idx_uz < sbuf_count(elements) || ellipsis != NULL);
        struct value* const res = idx_uz < sbuf_count(elements)
            ? value_clone(elements[idx_uz])
            : value_clone(ellipsis);
        value_del(lhs);
        value_del(idx);
        return res;
    }

    if (lhs->type->kind == TYPE_SLICE) {
        // Slices are constructed from a (pointer, count) pair which makes them
        // more-or-less normal pointers with some extra fancy bookkeeping.
        // Pointers may not be dereferenced in a constant expression, so
        // similarly we do not allow indexing a slice (which is more-or-less
        // pointer dereferencing) in a constant expression.
        fatal(
            expr->location,
            "indexing with left-hand-type `%s` not supported in compile-time expressions",
            lhs->type->name);
    }

    UNREACHABLE();
    return NULL;
}

static struct value*
eval_rvalue_access_slice(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_SLICE);

    struct value* const lhs = eval_rvalue(expr->data.access_slice.lhs);
    struct value* const begin = eval_rvalue(expr->data.access_slice.begin);
    struct value* const end = eval_rvalue(expr->data.access_slice.end);

    assert(begin->type->kind == TYPE_USIZE);
    struct bigint const* const begin_bigint = begin->data.integer;
    size_t begin_uz = 0u;
    if (bigint_to_uz(&begin_uz, begin_bigint)) {
        fatal(
            expr->data.access_slice.begin->location,
            "index out-of-range (received %s)",
            bigint_to_new_cstr(begin_bigint));
    }
    assert(end->type->kind == TYPE_USIZE);
    struct bigint const* const end_bigint = end->data.integer;
    size_t end_uz = 0u;
    if (bigint_to_uz(&end_uz, end_bigint)) {
        fatal(
            expr->data.access_slice.end->location,
            "index out-of-range (received %s)",
            bigint_to_new_cstr(end_bigint));
    }

    if (lhs->type->kind == TYPE_ARRAY) {
        if (begin_uz >= lhs->type->data.array.count) {
            fatal(
                expr->data.access_slice.begin->location,
                "index out-of-bounds (array count is %" PRIu64
                ", received %zu)",
                lhs->type->data.array.count,
                begin_uz);
        }
        if (end_uz > lhs->type->data.array.count) {
            fatal(
                expr->data.access_slice.begin->location,
                "index out-of-bounds (array count is %" PRIu64
                ", received %zu)",
                lhs->type->data.array.count,
                end_uz);
        }

        struct value* const pointer = eval_lvalue(expr->data.access_slice.lhs);
        assert(pointer->type->kind == TYPE_POINTER);
        assert(pointer->data.pointer.kind == ADDRESS_STATIC);
        pointer->type = type_unique_pointer(expr->type->data.slice.base);
        pointer->data.pointer.data.static_.offset +=
            begin_uz * expr->type->data.slice.base->size;

        struct bigint* const count_bigint = bigint_new(BIGINT_ZERO);
        bigint_sub(count_bigint, end_bigint, begin_bigint);

        struct value* const count =
            value_new_integer(context()->builtin.usize, count_bigint);
        struct value* const res = value_new_slice(expr->type, pointer, count);
        value_del(lhs);
        value_del(begin);
        value_del(end);
        return res;
    }

    if (lhs->type->kind == TYPE_SLICE) {
        // Slices are constructed from a (pointer, count) pair which makes them
        // more-or-less normal pointers with some extra fancy bookkeeping.
        // Pointers may not be dereferenced in a constant expression, so
        // similarly we do not allow indexing a slice (which is more-or-less
        // pointer dereferencing) in a constant expression.
        fatal(
            expr->location,
            "slicing with left-hand-type `%s` not supported in compile-time expressions",
            lhs->type->name);
    }

    UNREACHABLE();
    return NULL;
}

static struct value*
eval_rvalue_access_member_variable(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_MEMBER_VARIABLE);

    struct value* const lhs =
        eval_rvalue(expr->data.access_member_variable.lhs);

    struct value const* const member = value_xget_member_variable(
        expr->location,
        lhs,
        expr->data.access_member_variable.member_variable->name);

    struct value* const res = value_clone(member);

    value_del(lhs);
    return res;
}

static struct value*
eval_rvalue_sizeof(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SIZEOF);

    assert(expr->type->kind == TYPE_USIZE);
    return value_new_integer(
        context()->builtin.usize,
        bigint_new_umax(expr->data.sizeof_.rhs->size));
}

static struct value*
eval_rvalue_alignof(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ALIGNOF);

    assert(expr->type->kind == TYPE_USIZE);
    return value_new_integer(
        context()->builtin.usize,
        bigint_new_umax(expr->data.alignof_.rhs->align));
}

static struct value*
eval_rvalue_unary(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);

    switch (expr->data.unary.op) {
    case UOP_NOT: {
        struct value* const rhs = eval_rvalue(expr->data.unary.rhs);
        assert(rhs->type->kind == TYPE_BOOL);
        rhs->data.boolean = !rhs->data.boolean;
        return rhs;
    }
    case UOP_POS: {
        struct value* const rhs = eval_rvalue(expr->data.unary.rhs);
        assert(type_is_int(rhs->type));
        return rhs;
    }
    case UOP_NEG: {
        struct value* const rhs = eval_rvalue(expr->data.unary.rhs);
        assert(type_is_sint(rhs->type));
        struct bigint* const integer = bigint_new(BIGINT_ZERO);
        bigint_neg(integer, rhs->data.integer);
        if (integer_is_out_of_range(expr->type, integer)) {
            fatal(
                expr->location,
                "operation produces out-of-range result (-(%s) == %s)",
                bigint_to_new_cstr(rhs->data.integer),
                bigint_to_new_cstr(integer));
        }
        value_del(rhs);
        return value_new_integer(expr->type, integer);
    }
    case UOP_NEG_WRAPPING: {
        struct value* const rhs = eval_rvalue(expr->data.unary.rhs);
        assert(type_is_sint(rhs->type));
        struct bitarr* const bitarr = bitarr_new((size_t)(rhs->type->size * 8));
        if (bigint_to_bitarr(bitarr, rhs->data.integer)) {
            UNREACHABLE();
        }
        bitarr_twos_complement_neg(bitarr, bitarr);
        struct bigint* const integer = bigint_new_bitarr(bitarr, true);
        bitarr_del(bitarr);
        value_del(rhs);
        return value_new_integer(expr->type, integer);
    }
    case UOP_BITNOT: {
        struct value* const rhs = eval_rvalue(expr->data.unary.rhs);
        assert(rhs->type->kind == TYPE_BYTE || type_is_int(rhs->type));

        if (rhs->type->kind == TYPE_BYTE) {
            rhs->data.byte = (uint8_t)~rhs->data.byte;
            return rhs;
        }

        bool const is_signed = type_is_sint(rhs->type);
        size_t const bit_count = (size_t)rhs->type->size * 8u;
        struct bitarr* const rhs_bits = bitarr_new(bit_count);
        struct bitarr* const res_bits = bitarr_new(bit_count);
        if (bigint_to_bitarr(rhs_bits, rhs->data.integer)) {
            UNREACHABLE();
        }

        for (size_t i = 0; i < bit_count; ++i) {
            int const bit = !bitarr_get(rhs_bits, i);
            bitarr_set(res_bits, i, bit);
        }
        bitarr_del(rhs_bits);

        struct bigint* const res_bigint =
            bigint_new_bitarr(res_bits, is_signed);
        bitarr_del(res_bits);

        struct value* const res = value_new_integer(rhs->type, res_bigint);
        value_del(rhs);
        return res;
    }
    case UOP_DEREFERENCE: {
        fatal(
            expr->location,
            "dereference operator not supported in compile-time expressions");
    }
    case UOP_ADDRESSOF: {
        return eval_lvalue(expr->data.unary.rhs);
    }
    case UOP_STARTOF: {
        assert(expr->type->kind == TYPE_POINTER);

        struct value* const rhs = eval_rvalue(expr->data.unary.rhs);
        assert(rhs->type->kind == TYPE_SLICE);
        assert(rhs->data.slice.pointer->type->kind == TYPE_POINTER);
        struct value* res = value_new_pointer(
            expr->type, rhs->data.slice.pointer->data.pointer);

        value_del(rhs);
        return res;
    }
    case UOP_COUNTOF: {
        assert(expr->type->kind == TYPE_USIZE);

        if (expr->data.unary.rhs->type->kind == TYPE_ARRAY) {
            return value_new_integer(
                context()->builtin.usize,
                bigint_new_umax(expr->data.unary.rhs->type->data.array.count));
        }

        if (expr->data.unary.rhs->type->kind == TYPE_SLICE) {
            struct value* const rhs = eval_rvalue(expr->data.unary.rhs);

            assert(rhs->data.slice.count->type->kind == TYPE_USIZE);
            struct value* const res = value_new_integer(
                context()->builtin.usize,
                bigint_new(rhs->data.slice.count->data.integer));

            value_del(rhs);
            return res;
        }

        UNREACHABLE();
    }
    }

    UNREACHABLE();
    return NULL;
}

static struct value*
eval_rvalue_binary(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BINARY);

    struct value* const lhs = eval_rvalue(expr->data.binary.lhs);
    struct value* const rhs = eval_rvalue(expr->data.binary.rhs);
    struct value* res = NULL;
    switch (expr->data.binary.op) {
    case BOP_OR: {
        assert(lhs->type->kind == TYPE_BOOL);
        assert(rhs->type->kind == TYPE_BOOL);
        res = value_new_boolean(lhs->data.boolean || rhs->data.boolean);
        break;
    }
    case BOP_AND: {
        assert(lhs->type->kind == TYPE_BOOL);
        assert(rhs->type->kind == TYPE_BOOL);
        res = value_new_boolean(lhs->data.boolean && rhs->data.boolean);
        break;
    }
    case BOP_SHL: {
        assert(type_is_int(lhs->type));
        assert(rhs->type->kind == TYPE_USIZE);

        bool const is_signed = type_is_sint(expr->type);

        size_t const bit_count = (size_t)rhs->type->size * 8u;
        struct bitarr* const res_bits = bitarr_new(bit_count);
        if (bigint_to_bitarr(res_bits, lhs->data.integer)) {
            UNREACHABLE();
        }

        size_t shift_count = 0u;
        if (bigint_to_uz(&shift_count, rhs->data.integer)) {
            fatal(
                expr->data.access_index.idx->location,
                "shift count out-of-range (received %s)",
                bigint_to_new_cstr(rhs->data.integer));
        }

        bitarr_shiftl(res_bits, res_bits, shift_count);
        struct bigint* const res_bigint =
            bigint_new_bitarr(res_bits, is_signed);

        res = value_new_integer(expr->type, res_bigint);
        bitarr_del(res_bits);
        break;
    }
    case BOP_SHR: {
        assert(type_is_int(lhs->type));
        assert(rhs->type->kind == TYPE_USIZE);

        bool const is_signed = type_is_sint(expr->type);
        bool const is_negative = bigint_cmp(lhs->data.integer, BIGINT_ZERO) < 0;

        size_t const bit_count = (size_t)rhs->type->size * 8u;
        struct bitarr* const res_bits = bitarr_new(bit_count);
        if (bigint_to_bitarr(res_bits, lhs->data.integer)) {
            UNREACHABLE();
        }

        size_t shift_count = 0u;
        if (bigint_to_uz(&shift_count, rhs->data.integer)) {
            fatal(
                expr->data.access_index.idx->location,
                "shift count out-of-range (received %s)",
                bigint_to_new_cstr(rhs->data.integer));
        }

        bitarr_shiftr(res_bits, res_bits, shift_count, is_negative);
        struct bigint* const res_bigint =
            bigint_new_bitarr(res_bits, is_signed);

        res = value_new_integer(expr->type, res_bigint);
        bitarr_del(res_bits);
        break;
    }
    case BOP_EQ: {
        res = value_new_boolean(value_eq(lhs, rhs));
        break;
    }
    case BOP_NE: {
        res = value_new_boolean(!value_eq(lhs, rhs));
        break;
    }
    case BOP_LE: {
        res = value_new_boolean(value_lt(lhs, rhs) || value_eq(lhs, rhs));
        break;
    }
    case BOP_LT: {
        res = value_new_boolean(value_lt(lhs, rhs));
        break;
    }
    case BOP_GE: {
        res = value_new_boolean(value_gt(lhs, rhs) || value_eq(lhs, rhs));
        break;
    }
    case BOP_GT: {
        res = value_new_boolean(value_gt(lhs, rhs));
        break;
    }
    case BOP_ADD: {
        assert(lhs->type == rhs->type);
        if (lhs->type->kind == TYPE_F32) {
            res = value_new_f32(lhs->data.f32 + rhs->data.f32);
            break;
        }
        if (lhs->type->kind == TYPE_F64) {
            res = value_new_f64(lhs->data.f64 + rhs->data.f64);
            break;
        }

        assert(type_is_int(lhs->type));
        assert(type_is_int(rhs->type));
        struct bigint* const integer = bigint_new(BIGINT_ZERO);
        bigint_add(integer, lhs->data.integer, rhs->data.integer);
        if (integer_is_out_of_range(expr->type, integer)) {
            fatal(
                expr->location,
                "operation produces out-of-range result (%s + %s == %s)",
                bigint_to_new_cstr(lhs->data.integer),
                bigint_to_new_cstr(rhs->data.integer),
                bigint_to_new_cstr(integer));
        }
        res = value_new_integer(expr->type, integer);
        break;
    }
    case BOP_ADD_WRAPPING: {
        assert(type_is_int(lhs->type));
        assert(type_is_int(rhs->type));
        struct bigint* const integer = bigint_new(BIGINT_ZERO);
        bigint_add(integer, lhs->data.integer, rhs->data.integer);
        struct bitarr* const wrapped =
            bitarr_new((size_t)(expr->type->size * 8u * 2u));
        if (bigint_to_bitarr(wrapped, integer)) {
            UNREACHABLE();
        }
        struct bitarr* const trimmed =
            bitarr_new((size_t)(expr->type->size * 8u));
        for (size_t i = 0; i < bitarr_count(trimmed); ++i) {
            bitarr_set(trimmed, i, bitarr_get(wrapped, i));
        }
        bitarr_to_bigint(integer, trimmed, type_is_sint(expr->type));
        bitarr_del(wrapped);
        bitarr_del(trimmed);
        res = value_new_integer(expr->type, integer);
        break;
    }
    case BOP_SUB: {
        assert(lhs->type == rhs->type);
        if (lhs->type->kind == TYPE_F32) {
            res = value_new_f32(lhs->data.f32 - rhs->data.f32);
            break;
        }
        if (lhs->type->kind == TYPE_F64) {
            res = value_new_f64(lhs->data.f64 - rhs->data.f64);
            break;
        }

        assert(type_is_int(lhs->type));
        assert(type_is_int(rhs->type));
        struct bigint* const integer = bigint_new(BIGINT_ZERO);
        bigint_sub(integer, lhs->data.integer, rhs->data.integer);
        if (integer_is_out_of_range(expr->type, integer)) {
            fatal(
                expr->location,
                "operation produces out-of-range result (%s - %s == %s)",
                bigint_to_new_cstr(lhs->data.integer),
                bigint_to_new_cstr(rhs->data.integer),
                bigint_to_new_cstr(integer));
        }
        res = value_new_integer(expr->type, integer);
        break;
    }
    case BOP_SUB_WRAPPING: {
        assert(type_is_int(lhs->type));
        assert(type_is_int(rhs->type));
        struct bigint* const integer = bigint_new(BIGINT_ZERO);
        bigint_sub(integer, lhs->data.integer, rhs->data.integer);
        struct bitarr* const wrapped =
            bitarr_new((size_t)(expr->type->size * 8u * 2u));
        if (bigint_to_bitarr(wrapped, integer)) {
            UNREACHABLE();
        }
        struct bitarr* const trimmed =
            bitarr_new((size_t)(expr->type->size * 8u));
        for (size_t i = 0; i < bitarr_count(trimmed); ++i) {
            bitarr_set(trimmed, i, bitarr_get(wrapped, i));
        }
        bitarr_to_bigint(integer, trimmed, type_is_sint(expr->type));
        bitarr_del(wrapped);
        bitarr_del(trimmed);
        res = value_new_integer(expr->type, integer);
        break;
    }
    case BOP_MUL: {
        assert(lhs->type == rhs->type);
        if (lhs->type->kind == TYPE_F32) {
            res = value_new_f32(lhs->data.f32 * rhs->data.f32);
            break;
        }
        if (lhs->type->kind == TYPE_F64) {
            res = value_new_f64(lhs->data.f64 * rhs->data.f64);
            break;
        }

        assert(type_is_int(lhs->type));
        assert(type_is_int(rhs->type));
        struct bigint* const integer = bigint_new(BIGINT_ZERO);
        bigint_mul(integer, lhs->data.integer, rhs->data.integer);
        if (integer_is_out_of_range(expr->type, integer)) {
            fatal(
                expr->location,
                "operation produces out-of-range result (%s * %s == %s)",
                bigint_to_new_cstr(lhs->data.integer),
                bigint_to_new_cstr(rhs->data.integer),
                bigint_to_new_cstr(integer));
        }
        res = value_new_integer(expr->type, integer);
        break;
    }
    case BOP_MUL_WRAPPING: {
        assert(type_is_int(lhs->type));
        assert(type_is_int(rhs->type));
        struct bigint* const integer = bigint_new(BIGINT_ZERO);
        bigint_mul(integer, lhs->data.integer, rhs->data.integer);
        struct bitarr* const wrapped =
            bitarr_new((size_t)(expr->type->size * 8u * 2u));
        if (bigint_to_bitarr(wrapped, integer)) {
            UNREACHABLE();
        }
        struct bitarr* const trimmed =
            bitarr_new((size_t)(expr->type->size * 8u));
        for (size_t i = 0; i < bitarr_count(trimmed); ++i) {
            bitarr_set(trimmed, i, bitarr_get(wrapped, i));
        }
        bitarr_to_bigint(integer, trimmed, type_is_sint(expr->type));
        bitarr_del(wrapped);
        bitarr_del(trimmed);
        res = value_new_integer(expr->type, integer);
        break;
    }
    case BOP_DIV: {
        assert(lhs->type == rhs->type);
        if (lhs->type->kind == TYPE_F32) {
            if (rhs->data.f32 == 0) {
                fatal(
                    expr->location,
                    "divide by zero (%.*f / %.*f)",
                    IEEE754_FLT_DECIMAL_DIG,
                    (double)lhs->data.f32,
                    IEEE754_FLT_DECIMAL_DIG,
                    (double)rhs->data.f32);
            }
            res = value_new_f32(lhs->data.f32 / rhs->data.f32);
            break;
        }
        if (lhs->type->kind == TYPE_F64) {
            if (rhs->data.f64 == 0) {
                fatal(
                    expr->location,
                    "divide by zero (%.*f / %.*f)",
                    IEEE754_DBL_DECIMAL_DIG,
                    lhs->data.f64,
                    IEEE754_DBL_DECIMAL_DIG,
                    rhs->data.f64);
            }
            res = value_new_f64(lhs->data.f64 / rhs->data.f64);
            break;
        }

        assert(type_is_int(lhs->type));
        assert(type_is_int(rhs->type));
        if (bigint_cmp(rhs->data.integer, BIGINT_ZERO) == 0) {
            fatal(
                expr->location,
                "divide by zero (%s / %s)",
                bigint_to_new_cstr(lhs->data.integer),
                bigint_to_new_cstr(rhs->data.integer));
        }
        struct bigint* const r = bigint_new(BIGINT_ZERO);
        bigint_divrem(r, NULL, lhs->data.integer, rhs->data.integer);
        res = value_new_integer(expr->type, r);
        break;
    }
    case BOP_REM: {
        assert(type_is_int(lhs->type));
        assert(type_is_int(rhs->type));
        if (bigint_cmp(rhs->data.integer, BIGINT_ZERO) == 0) {
            fatal(
                expr->location,
                "divide by zero (%s %% %s)",
                bigint_to_new_cstr(lhs->data.integer),
                bigint_to_new_cstr(rhs->data.integer));
        }
        struct bigint* const r = bigint_new(BIGINT_ZERO);
        bigint_divrem(NULL, r, lhs->data.integer, rhs->data.integer);
        res = value_new_integer(expr->type, r);
        break;
    }
    case BOP_BITOR: {
        assert(
            lhs->type->kind == TYPE_BOOL || lhs->type->kind == TYPE_BYTE
            || type_is_int(lhs->type));
        assert(
            rhs->type->kind == TYPE_BOOL || rhs->type->kind == TYPE_BYTE
            || type_is_int(rhs->type));
        assert(lhs->type->kind == rhs->type->kind);
        struct type const* const type = lhs->type;

        if (type->kind == TYPE_BOOL) {
            res = value_new_boolean(lhs->data.boolean || rhs->data.boolean);
            break;
        }

        if (type->kind == TYPE_BYTE) {
            res = value_new_byte(lhs->data.byte | rhs->data.byte);
            break;
        }

        assert(type_is_int(type));
        bool const is_signed = type_is_sint(type);
        size_t const bit_count = (size_t)type->size * 8u;
        struct bitarr* const lhs_bits = bitarr_new(bit_count);
        struct bitarr* const rhs_bits = bitarr_new(bit_count);
        struct bitarr* const res_bits = bitarr_new(bit_count);
        if (bigint_to_bitarr(lhs_bits, lhs->data.integer)) {
            UNREACHABLE();
        }
        if (bigint_to_bitarr(rhs_bits, rhs->data.integer)) {
            UNREACHABLE();
        }

        for (size_t i = 0; i < bit_count; ++i) {
            int const bit = bitarr_get(lhs_bits, i) || bitarr_get(rhs_bits, i);
            bitarr_set(res_bits, i, bit);
        }
        bitarr_del(lhs_bits);
        bitarr_del(rhs_bits);

        struct bigint* const res_bigint =
            bigint_new_bitarr(res_bits, is_signed);
        bitarr_del(res_bits);

        res = value_new_integer(type, res_bigint);
        break;
    }
    case BOP_BITXOR: {
        assert(
            lhs->type->kind == TYPE_BOOL || lhs->type->kind == TYPE_BYTE
            || type_is_int(lhs->type));
        assert(
            rhs->type->kind == TYPE_BOOL || rhs->type->kind == TYPE_BYTE
            || type_is_int(rhs->type));
        assert(lhs->type->kind == rhs->type->kind);
        struct type const* const type = lhs->type;

        if (type->kind == TYPE_BOOL) {
            res = value_new_boolean(lhs->data.boolean ^ rhs->data.boolean);
            break;
        }

        if (type->kind == TYPE_BYTE) {
            res = value_new_byte(lhs->data.byte ^ rhs->data.byte);
            break;
        }

        assert(type_is_int(type));
        bool const is_signed = type_is_sint(type);
        size_t const bit_count = (size_t)type->size * 8u;
        struct bitarr* const lhs_bits = bitarr_new(bit_count);
        struct bitarr* const rhs_bits = bitarr_new(bit_count);
        struct bitarr* const res_bits = bitarr_new(bit_count);
        if (bigint_to_bitarr(lhs_bits, lhs->data.integer)) {
            UNREACHABLE();
        }
        if (bigint_to_bitarr(rhs_bits, rhs->data.integer)) {
            UNREACHABLE();
        }

        for (size_t i = 0; i < bit_count; ++i) {
            int const bit = bitarr_get(lhs_bits, i) ^ bitarr_get(rhs_bits, i);
            bitarr_set(res_bits, i, bit);
        }
        bitarr_del(lhs_bits);
        bitarr_del(rhs_bits);

        struct bigint* const res_bigint =
            bigint_new_bitarr(res_bits, is_signed);
        bitarr_del(res_bits);

        res = value_new_integer(type, res_bigint);
        break;
    }
    case BOP_BITAND: {
        assert(
            lhs->type->kind == TYPE_BOOL || lhs->type->kind == TYPE_BYTE
            || type_is_int(lhs->type));
        assert(
            rhs->type->kind == TYPE_BOOL || rhs->type->kind == TYPE_BYTE
            || type_is_int(rhs->type));
        assert(lhs->type->kind == rhs->type->kind);
        struct type const* const type = lhs->type;

        if (type->kind == TYPE_BOOL) {
            res = value_new_boolean(lhs->data.boolean && rhs->data.boolean);
            break;
        }

        if (type->kind == TYPE_BYTE) {
            res = value_new_byte(lhs->data.byte & rhs->data.byte);
            break;
        }

        assert(type_is_int(type));
        bool const is_signed = type_is_sint(type);
        size_t const bit_count = (size_t)type->size * 8u;
        struct bitarr* const lhs_bits = bitarr_new(bit_count);
        struct bitarr* const rhs_bits = bitarr_new(bit_count);
        struct bitarr* const res_bits = bitarr_new(bit_count);
        if (bigint_to_bitarr(lhs_bits, lhs->data.integer)) {
            UNREACHABLE();
        }
        if (bigint_to_bitarr(rhs_bits, rhs->data.integer)) {
            UNREACHABLE();
        }

        for (size_t i = 0; i < bit_count; ++i) {
            int const bit = bitarr_get(lhs_bits, i) && bitarr_get(rhs_bits, i);
            bitarr_set(res_bits, i, bit);
        }
        bitarr_del(lhs_bits);
        bitarr_del(rhs_bits);

        struct bigint* const res_bigint =
            bigint_new_bitarr(res_bits, is_signed);
        bitarr_del(res_bits);

        res = value_new_integer(type, res_bigint);
        break;
    }
    }
    value_del(lhs);
    value_del(rhs);
    assert(res != NULL);
    return res;
}

struct value*
eval_lvalue(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr_is_lvalue(expr));

    switch (expr->kind) {
    case EXPR_SYMBOL: {
        return eval_lvalue_symbol(expr);
    }
    case EXPR_ACCESS_INDEX: {
        return eval_lvalue_access_index(expr);
    }
    case EXPR_ACCESS_MEMBER_VARIABLE: {
        return eval_lvalue_access_member_variable(expr);
    }
    case EXPR_UNARY: {
        return eval_lvalue_unary(expr);
    }
    case EXPR_VALUE: /* fallthrough */
    case EXPR_BYTES: /* fallthrough */
    case EXPR_ARRAY_LIST: /* fallthrough */
    case EXPR_SLICE_LIST: /* fallthrough */
    case EXPR_SLICE: /* fallthrough */
    case EXPR_STRUCT: /* fallthrough */
    case EXPR_CAST: /* fallthrough */
    case EXPR_CALL: /* fallthrough */
    case EXPR_ACCESS_SLICE: /* fallthrough */
    case EXPR_SIZEOF: /* fallthrough */
    case EXPR_ALIGNOF: /* fallthrough */
    case EXPR_BINARY: {
        UNREACHABLE();
    }
    }

    UNREACHABLE();
    return NULL;
}

static struct value*
eval_lvalue_symbol(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SYMBOL);

    struct symbol const* const symbol = expr->data.symbol;
    if (symbol_xget_address(symbol)->kind != ADDRESS_STATIC) {
        fatal(
            expr->location,
            "addressof operator applied to non-static object in compile-time expression");
    }
    struct type const* const type =
        type_unique_pointer(symbol_xget_type(symbol));
    return value_new_pointer(type, *symbol_xget_address(symbol));
}

static struct value*
eval_lvalue_access_index(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_INDEX);

    if (expr->data.access_index.lhs->type->kind == TYPE_SLICE) {
        // Disallow indexing of an lvalue slice at compile time as indexing the
        // slice would be quivalent to dereferencing an arbitrary pointer.
        fatal(
            expr->location,
            "constant expression contains lvalue slice indexing operation");
    }

    struct value* const lhs = eval_lvalue(expr->data.access_index.lhs);
    struct value* const idx = eval_rvalue(expr->data.access_index.idx);
    assert(lhs->type->kind == TYPE_POINTER);
    assert(idx->type->kind == TYPE_USIZE);
    struct type const* const array_type = lhs->type->data.pointer.base;
    assert(array_type->kind == TYPE_ARRAY);
    struct type const* const element_type = array_type->data.array.base;
    struct type const* const type = type_unique_pointer(element_type);

    size_t idx_uz = 0u;
    if (bigint_to_uz(&idx_uz, idx->data.integer)) {
        fatal(
            expr->data.access_index.idx->location,
            "index out-of-range (received %s)",
            bigint_to_new_cstr(idx->data.integer));
    }

    assert(expr->data.access_index.lhs->type->kind == TYPE_ARRAY);
    if (idx_uz >= expr->data.access_index.lhs->type->data.array.count) {
        fatal(
            expr->data.access_index.idx->location,
            "index out-of-bounds (array count is %" PRIu64 ", received %s)",
            lhs->type->data.array.count,
            bigint_to_new_cstr(idx->data.integer));
    }

    assert(lhs->data.pointer.kind == ADDRESS_STATIC);
    char const* const address_name = lhs->data.pointer.data.static_.name;
    assert(element_type->size <= SIZE_MAX);
    uint64_t const address_offset = lhs->data.pointer.data.static_.offset
        + (size_t)element_type->size * idx_uz;
    struct address const address =
        address_init_static(address_name, address_offset);
    value_del(lhs);
    value_del(idx);
    return value_new_pointer(type, address);
}

static struct value*
eval_lvalue_access_member_variable(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ACCESS_MEMBER_VARIABLE);

    struct value* const value =
        eval_lvalue(expr->data.access_member_variable.lhs);
    assert(value->type->kind == TYPE_POINTER);
    assert(value->data.pointer.kind == ADDRESS_STATIC);
    value->data.pointer.data.static_.offset +=
        expr->data.access_member_variable.member_variable->offset;
    return value;
}

static struct value*
eval_lvalue_unary(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_UNARY);

    switch (expr->data.unary.op) {
    case UOP_DEREFERENCE: {
        fatal(
            expr->location,
            "dereference operator not supported in compile-time expressions");
    }
    case UOP_NOT: /* fallthrough */
    case UOP_POS: /* fallthrough */
    case UOP_NEG: /* fallthrough */
    case UOP_NEG_WRAPPING: /* fallthrough */
    case UOP_BITNOT: /* fallthrough */
    case UOP_ADDRESSOF: /* fallthrough */
    case UOP_STARTOF:
    case UOP_COUNTOF:
        UNREACHABLE();
    }

    UNREACHABLE();
    return NULL;
}
