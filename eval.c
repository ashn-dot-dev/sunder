// Copyright 2021-2022 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sunder.h"

static bool
integer_is_out_of_range(struct type const* type, struct bigint const* res);

static struct value*
eval_rvalue_symbol(struct expr const* expr);
static struct value*
eval_rvalue_boolean(struct expr const* expr);
static struct value*
eval_rvalue_integer(struct expr const* expr);
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
    assert(type_is_any_integer(type));
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
    case EXPR_BOOLEAN: {
        return eval_rvalue_boolean(expr);
    }
    case EXPR_INTEGER: {
        return eval_rvalue_integer(expr);
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
        return value_clone(symbol_xget_value(symbol));
    }

    fatal(expr->location, "identifier `%s` is not a constant", symbol->name);
}

static struct value*
eval_rvalue_boolean(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BOOLEAN);

    return value_new_boolean(expr->data.boolean);
}

static struct value*
eval_rvalue_integer(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_INTEGER);

    struct bigint const* const integer = expr->data.integer;

    if (expr->type->kind == TYPE_BYTE) {
        uint8_t byte = 0;
        int const out_of_range = bigint_to_u8(&byte, integer);
        if (out_of_range) {
            UNREACHABLE();
        }
        return value_new_byte(byte);
    }

    assert(type_is_any_integer(expr->type));
    return value_new_integer(expr->type, bigint_new(integer));
}

static struct value*
eval_rvalue_bytes(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_BYTES);

    struct value* const pointer = value_new_pointer(
        type_unique_pointer(context()->builtin.byte),
        *expr->data.bytes.address);

    struct bigint* const count_bigint = bigint_new(BIGINT_ZERO);
    uz_to_bigint(count_bigint, expr->data.bytes.count);
    struct value* const count =
        value_new_integer(context()->builtin.usize, count_bigint);

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
    struct value* const count =
        value_new_integer(context()->builtin.usize, bigint_new(BIGINT_ZERO));
    uz_to_bigint(count->data.integer, sbuf_count(elements));

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

    sbuf(struct member_variable) const member_variable_defs =
        expr->type->data.struct_.member_variables;
    sbuf(struct expr const* const) const member_variable_exprs =
        expr->data.struct_.member_variables;
    assert(
        sbuf_count(member_variable_defs) == sbuf_count(member_variable_exprs));

    for (size_t i = 0; i < sbuf_count(member_variable_exprs); ++i) {
        value_set_member(
            value,
            member_variable_defs[i].name,
            eval_rvalue(member_variable_exprs[i]));
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
    // us to assume from->type != expr->type from this point forward.
    if (from->type->kind == expr->type->kind) {
        return from;
    }

    // The representation of a non-absolute address is chosen by the
    // assembler/linker and has no meaningful representation at compile-time.
    // Absolute addresses are *not* supported at the language level, so it is a
    // hard error to cast to/from a pointer type.
    //
    // TODO: There is a case to be made for casting a pointer of type T1 to a
    // pointer of type T2 in a compile time expression as long as the language
    // continues to disallow pointer dereference in compile-time expressions. In
    // the future check if this a valid/common enough use case to include at the
    // language level.
    if (from->type->kind == TYPE_POINTER) {
        fatal(
            expr->location,
            "constant expression contains cast from pointer type");
    }
    if (expr->type->kind == TYPE_POINTER) {
        fatal(
            expr->location,
            "constant expression contains cast to pointer type");
    }

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
        int const extend = type_is_signed_integer(from->type)
            && (bytes[bytes_count - 1] & 0x80);

        size_t const bit_count = expr->type->size * 8u;
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

        struct bigint* const integer = bigint_new(BIGINT_ZERO);
        bitarr_to_bigint(integer, bits, type_is_signed_integer(expr->type));
        bitarr_del(bits);

        res = value_new_integer(expr->type, integer);
        break;
    }
    case TYPE_ANY: /* fallthrough */
    case TYPE_VOID: /* fallthrough */
    case TYPE_INTEGER: /* fallthrough */
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
            bigint_to_new_cstr(idx_bigint, NULL));
    }

    if (lhs->type->kind == TYPE_ARRAY) {
        if (idx_uz >= lhs->type->data.array.count) {
            fatal(
                expr->data.access_index.idx->location,
                "index out-of-bounds (array count is %zu, received %zu)",
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
            bigint_to_new_cstr(begin_bigint, NULL));
    }
    assert(end->type->kind == TYPE_USIZE);
    struct bigint const* const end_bigint = end->data.integer;
    size_t end_uz = 0u;
    if (bigint_to_uz(&end_uz, end_bigint)) {
        fatal(
            expr->data.access_slice.end->location,
            "index out-of-range (received %s)",
            bigint_to_new_cstr(end_bigint, NULL));
    }

    if (lhs->type->kind == TYPE_ARRAY) {
        if (begin_uz >= lhs->type->data.array.count) {
            fatal(
                expr->data.access_slice.begin->location,
                "index out-of-bounds (array count is %zu, received %zu)",
                lhs->type->data.array.count,
                begin_uz);
        }
        if (end_uz > lhs->type->data.array.count) {
            fatal(
                expr->data.access_slice.begin->location,
                "index out-of-bounds (array count is %zu, received %zu)",
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

    long const index = type_struct_member_variable_index(
        lhs->type, expr->data.access_member_variable.member_variable->name);
    assert(index >= 0);

    struct value* const res =
        value_clone(lhs->data.struct_.member_variables[index]);

    value_del(lhs);
    return res;
}

static struct value*
eval_rvalue_sizeof(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_SIZEOF);

    struct bigint* const size_bigint = bigint_new(BIGINT_ZERO);
    uz_to_bigint(size_bigint, expr->data.sizeof_.rhs->size);

    assert(expr->type->kind == TYPE_USIZE);
    return value_new_integer(context()->builtin.usize, size_bigint);
}

static struct value*
eval_rvalue_alignof(struct expr const* expr)
{
    assert(expr != NULL);
    assert(expr->kind == EXPR_ALIGNOF);

    struct bigint* const size_bigint = bigint_new(BIGINT_ZERO);
    uz_to_bigint(size_bigint, expr->data.alignof_.rhs->align);

    assert(expr->type->kind == TYPE_USIZE);
    return value_new_integer(context()->builtin.usize, size_bigint);
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
        assert(type_is_any_integer(rhs->type));
        return rhs;
    }
    case UOP_NEG: {
        struct value* const rhs = eval_rvalue(expr->data.unary.rhs);
        assert(type_is_any_integer(rhs->type));
        struct bigint* const r = bigint_new(BIGINT_ZERO);
        bigint_neg(r, rhs->data.integer);
        if (integer_is_out_of_range(expr->type, r)) {
            fatal(
                expr->location,
                "arithmetic operation produces out-of-range result (-(%s) == %s)",
                bigint_to_new_cstr(rhs->data.integer, NULL),
                bigint_to_new_cstr(r, NULL));
        }
        value_del(rhs);
        return value_new_integer(expr->type, r);
    }
    case UOP_BITNOT: {
        struct value* const rhs = eval_rvalue(expr->data.unary.rhs);
        assert(rhs->type->kind == TYPE_BYTE || type_is_any_integer(rhs->type));

        if (rhs->type->kind == TYPE_BYTE) {
            rhs->data.byte = (uint8_t)~rhs->data.byte;
            return rhs;
        }

        bool const is_signed = type_is_signed_integer(rhs->type);
        size_t const bit_count = rhs->type->size * 8u;
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

        struct bigint* const res_bigint = bigint_new(BIGINT_ZERO);
        bitarr_to_bigint(res_bigint, res_bits, is_signed);
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
    case UOP_COUNTOF: {
        assert(expr->type->kind == TYPE_USIZE);
        struct value* const res = value_new_integer(
            context()->builtin.usize, bigint_new(BIGINT_ZERO));

        struct value* const rhs = eval_rvalue(expr->data.unary.rhs);
        switch (rhs->type->kind) {
        case TYPE_ARRAY: {
            size_t const count_uz = rhs->type->data.array.count;
            uz_to_bigint(res->data.integer, count_uz);
            break;
        }
        case TYPE_SLICE: {
            assert(rhs->data.slice.count->type->kind == TYPE_USIZE);
            struct bigint const* const count_bigint =
                rhs->data.slice.count->data.integer;
            bigint_assign(res->data.integer, count_bigint);
            break;
        }
        default:
            UNREACHABLE();
        }
        value_del(rhs);

        return res;
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
        assert(type_is_any_integer(lhs->type));
        assert(rhs->type->kind == TYPE_USIZE);

        bool const is_signed = type_is_signed_integer(expr->type);
        //bool const is_negative = bigint_cmp(lhs->data.integer, BIGINT_ZERO) < 0;

        size_t const bit_count = rhs->type->size * 8u;
        struct bitarr* const res_bits = bitarr_new(bit_count);
        if (bigint_to_bitarr(res_bits, lhs->data.integer)) {
            UNREACHABLE();
        }

        size_t shift_count = 0u;
        if (bigint_to_uz(&shift_count, rhs->data.integer)) {
            fatal(
                expr->data.access_index.idx->location,
                "shift count out-of-range (received %s)",
                bigint_to_new_cstr(rhs->data.integer, NULL));
        }

        bitarr_shiftl(res_bits, res_bits, shift_count);
        struct bigint* const res_bigint = bigint_new(BIGINT_ZERO);
        bitarr_to_bigint(res_bigint, res_bits, is_signed);

        res = value_new_integer(expr->type, res_bigint);
        bitarr_del(res_bits);
        break;
    }
    case BOP_SHR: {
        assert(type_is_any_integer(lhs->type));
        assert(rhs->type->kind == TYPE_USIZE);

        bool const is_signed = type_is_signed_integer(expr->type);
        bool const is_negative = bigint_cmp(lhs->data.integer, BIGINT_ZERO) < 0;

        size_t const bit_count = rhs->type->size * 8u;
        struct bitarr* const res_bits = bitarr_new(bit_count);
        if (bigint_to_bitarr(res_bits, lhs->data.integer)) {
            UNREACHABLE();
        }

        size_t shift_count = 0u;
        if (bigint_to_uz(&shift_count, rhs->data.integer)) {
            fatal(
                expr->data.access_index.idx->location,
                "shift count out-of-range (received %s)",
                bigint_to_new_cstr(rhs->data.integer, NULL));
        }

        bitarr_shiftr(res_bits, res_bits, shift_count, is_negative);
        struct bigint* const res_bigint = bigint_new(BIGINT_ZERO);
        bitarr_to_bigint(res_bigint, res_bits, is_signed);

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
        assert(type_is_any_integer(lhs->type));
        assert(type_is_any_integer(rhs->type));
        struct bigint* const r = bigint_new(BIGINT_ZERO);
        bigint_add(r, lhs->data.integer, rhs->data.integer);
        if (integer_is_out_of_range(expr->type, r)) {
            fatal(
                expr->location,
                "arithmetic operation produces out-of-range result (%s + %s == %s)",
                bigint_to_new_cstr(lhs->data.integer, NULL),
                bigint_to_new_cstr(rhs->data.integer, NULL),
                bigint_to_new_cstr(r, NULL));
        }
        res = value_new_integer(expr->type, r);
        break;
    }
    case BOP_SUB: {
        assert(type_is_any_integer(lhs->type));
        assert(type_is_any_integer(rhs->type));
        struct bigint* const r = bigint_new(BIGINT_ZERO);
        bigint_sub(r, lhs->data.integer, rhs->data.integer);
        if (integer_is_out_of_range(expr->type, r)) {
            fatal(
                expr->location,
                "arithmetic operation produces out-of-range result (%s - %s == %s)",
                bigint_to_new_cstr(lhs->data.integer, NULL),
                bigint_to_new_cstr(rhs->data.integer, NULL),
                bigint_to_new_cstr(r, NULL));
        }
        res = value_new_integer(expr->type, r);
        break;
    }
    case BOP_MUL: {
        assert(type_is_any_integer(lhs->type));
        assert(type_is_any_integer(rhs->type));
        struct bigint* const r = bigint_new(BIGINT_ZERO);
        bigint_mul(r, lhs->data.integer, rhs->data.integer);
        if (integer_is_out_of_range(expr->type, r)) {
            fatal(
                expr->location,
                "arithmetic operation produces out-of-range result (%s * %s == %s)",
                bigint_to_new_cstr(lhs->data.integer, NULL),
                bigint_to_new_cstr(rhs->data.integer, NULL),
                bigint_to_new_cstr(r, NULL));
        }
        res = value_new_integer(expr->type, r);
        break;
    }
    case BOP_DIV: {
        assert(type_is_any_integer(lhs->type));
        assert(type_is_any_integer(rhs->type));
        if (bigint_cmp(rhs->data.integer, BIGINT_ZERO) == 0) {
            fatal(
                expr->location,
                "divide by zero (%s / %s)",
                bigint_to_new_cstr(lhs->data.integer, NULL),
                bigint_to_new_cstr(rhs->data.integer, NULL));
        }
        struct bigint* const r = bigint_new(BIGINT_ZERO);
        bigint_divrem(r, NULL, lhs->data.integer, rhs->data.integer);
        res = value_new_integer(expr->type, r);
        break;
    }
    case BOP_BITOR: {
        assert(
            lhs->type->kind == TYPE_BOOL || lhs->type->kind == TYPE_BYTE
            || type_is_any_integer(lhs->type));
        assert(
            rhs->type->kind == TYPE_BOOL || rhs->type->kind == TYPE_BYTE
            || type_is_any_integer(rhs->type));
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

        assert(type_is_any_integer(type));
        bool const is_signed = type_is_signed_integer(type);
        size_t const bit_count = type->size * 8u;
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

        struct bigint* const res_bigint = bigint_new(BIGINT_ZERO);
        bitarr_to_bigint(res_bigint, res_bits, is_signed);
        bitarr_del(res_bits);

        res = value_new_integer(type, res_bigint);
        break;
    }
    case BOP_BITXOR: {
        assert(
            lhs->type->kind == TYPE_BOOL || lhs->type->kind == TYPE_BYTE
            || type_is_any_integer(lhs->type));
        assert(
            rhs->type->kind == TYPE_BOOL || rhs->type->kind == TYPE_BYTE
            || type_is_any_integer(rhs->type));
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

        assert(type_is_any_integer(type));
        bool const is_signed = type_is_signed_integer(type);
        size_t const bit_count = type->size * 8u;
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

        struct bigint* const res_bigint = bigint_new(BIGINT_ZERO);
        bitarr_to_bigint(res_bigint, res_bits, is_signed);
        bitarr_del(res_bits);

        res = value_new_integer(type, res_bigint);
        break;
    }
    case BOP_BITAND: {
        assert(
            lhs->type->kind == TYPE_BOOL || lhs->type->kind == TYPE_BYTE
            || type_is_any_integer(lhs->type));
        assert(
            rhs->type->kind == TYPE_BOOL || rhs->type->kind == TYPE_BYTE
            || type_is_any_integer(rhs->type));
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

        assert(type_is_any_integer(type));
        bool const is_signed = type_is_signed_integer(type);
        size_t const bit_count = type->size * 8u;
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

        struct bigint* const res_bigint = bigint_new(BIGINT_ZERO);
        bitarr_to_bigint(res_bigint, res_bits, is_signed);
        bitarr_del(res_bits);

        res = value_new_integer(type, res_bigint);
        break;
    }
    default:
        UNREACHABLE();
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
    case EXPR_BOOLEAN: /* fallthrough */
    case EXPR_INTEGER: /* fallthrough */
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
            bigint_to_new_cstr(idx->data.integer, NULL));
    }

    assert(expr->data.access_index.lhs->type->kind == TYPE_ARRAY);
    if (idx_uz >= expr->data.access_index.lhs->type->data.array.count) {
        fatal(
            expr->data.access_index.idx->location,
            "index out-of-bounds (array count is %zu, received %s)",
            lhs->type->data.array.count,
            bigint_to_new_cstr(idx->data.integer, NULL));
    }

    assert(lhs->data.pointer.kind == ADDRESS_STATIC);
    struct address const address = address_init_static(
        lhs->data.pointer.data.static_.name,
        lhs->data.pointer.data.static_.offset + element_type->size * idx_uz);
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
    case UOP_BITNOT: /* fallthrough */
    case UOP_ADDRESSOF: /* fallthrough */
    case UOP_COUNTOF:
        UNREACHABLE();
    }

    UNREACHABLE();
    return NULL;
}
