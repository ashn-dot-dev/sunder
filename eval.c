// Copyright 2021 The Nova Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "nova.h"

struct evaluator {
    struct symbol_table const* symbol_table;
};

struct evaluator*
evaluator_new(struct symbol_table const* symbol_table)
{
    assert(symbol_table != NULL);

    struct evaluator* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->symbol_table = symbol_table;

    return self;
}
void
evaluator_del(struct evaluator* self)
{
    assert(self != NULL);

    memset(self, 0x00, sizeof(*self));
    autil_xalloc(self, AUTIL_XALLOC_FREE);
}

static bool
integer_is_out_of_range(struct type const* type, struct autil_bigint const* res)
{
    assert(type != NULL);
    assert(type_is_integer(type));
    assert(res != NULL);

    return autil_bigint_cmp(res, type->data.integer.min) < 0
        || autil_bigint_cmp(res, type->data.integer.max) > 0;
}

struct value*
eval_rvalue(struct evaluator* evaluator, struct tir_expr const* expr)
{
    assert(evaluator != NULL);
    assert(expr != NULL);

    switch (expr->kind) {
    case TIR_EXPR_IDENTIFIER: {
        assert(expr->data.identifier->value != NULL);
        return value_clone(expr->data.identifier->value);
    }
    case TIR_EXPR_BOOLEAN: {
        return value_new_boolean(expr->data.boolean);
    }
    case TIR_EXPR_INTEGER: {
        struct autil_bigint const* const integer = expr->data.integer;
        if (expr->type->kind == TYPE_BYTE) {
            uint8_t byte = 0;
            int const out_of_range = bigint_to_u8(&byte, integer);
            assert(!out_of_range);
            return value_new_byte(byte);
        }
        assert(type_is_integer(expr->type));
        return value_new_integer(expr->type, autil_bigint_new(integer));
    }
    case TIR_EXPR_ARRAY: {
        autil_sbuf(struct tir_expr const* const) elements =
            expr->data.array.elements;
        autil_sbuf(struct value*) evaled_elements = NULL;
        for (size_t i = 0; i < autil_sbuf_count(elements); ++i) {
            autil_sbuf_push(
                evaled_elements, eval_rvalue(evaluator, elements[i]));
        }
        return value_new_array(expr->type, evaled_elements);
    }
    case TIR_EXPR_SLICE: {
        struct value* const pointer =
            eval_rvalue(evaluator, expr->data.slice.pointer);
        struct value* const count =
            eval_rvalue(evaluator, expr->data.slice.count);
        return value_new_slice(expr->type, pointer, count);
    }
    case TIR_EXPR_SYSCALL: {
        fatal(expr->location, "constant expression contains system call");
    }
    case TIR_EXPR_CALL: {
        fatal(expr->location, "constant expression contains function call");
    }
    case TIR_EXPR_INDEX: {
        struct value* const lhs = eval_rvalue(evaluator, expr->data.index.lhs);
        struct value* const idx = eval_rvalue(evaluator, expr->data.index.idx);

        assert(idx->type->kind == TYPE_USIZE);
        struct autil_bigint const* const idx_bigint = idx->data.integer;
        size_t idx_uz = 0u;
        if (bigint_to_uz(&idx_uz, idx_bigint)) {
            fatal(
                expr->data.index.idx->location,
                "index out-of-range (received %s)",
                autil_bigint_to_new_cstr(idx_bigint, NULL));
        }

        if (lhs->type->kind == TYPE_ARRAY) {
            if (idx_uz >= lhs->type->data.array.count) {
                fatal(
                    expr->data.index.idx->location,
                    "index out-of-bounds (array count is %zu, received %zu)",
                    lhs->type->data.array.count,
                    idx_uz);
            }
            struct value* const res =
                value_clone(lhs->data.array.elements[idx_uz]);
            value_del(lhs);
            value_del(idx);
            return res;
        }

        if (lhs->type->kind == TYPE_SLICE) {
            // Slices are constructed from a (pointer, count) pair which makes
            // them more-or-less normal pointers with some extra fancy
            // bookkeeping. Pointers may not be dereferenced in a constant
            // expression, so similarly we do not allow indexing a slice (which
            // is more-or-less pointer dereferencing) in a constant expression.
            fatal(
                expr->location,
                "slice element indexing not supported in compile-time expressions");
        }

        UNREACHABLE();
    }
    case TIR_EXPR_UNARY: {
        switch (expr->data.unary.op) {
        case UOP_NOT: {
            struct value* const rhs =
                eval_rvalue(evaluator, expr->data.unary.rhs);
            assert(rhs->type->kind == TYPE_BOOL);
            rhs->data.boolean = !rhs->data.boolean;
            return rhs;
        }
        case UOP_POS: {
            struct value* const rhs =
                eval_rvalue(evaluator, expr->data.unary.rhs);
            assert(type_is_integer(rhs->type));
            return rhs;
        }
        case UOP_NEG: {
            struct value* const rhs =
                eval_rvalue(evaluator, expr->data.unary.rhs);
            assert(type_is_integer(rhs->type));
            struct autil_bigint* const r = autil_bigint_new(AUTIL_BIGINT_ZERO);
            autil_bigint_neg(r, rhs->data.integer);
            if (integer_is_out_of_range(expr->type, r)) {
                fatal(
                    expr->location,
                    "arithmetic operation produces out-of-range result (-(%s) == %s)",
                    autil_bigint_to_new_cstr(rhs->data.integer, NULL),
                    autil_bigint_to_new_cstr(r, NULL));
            }
            value_del(rhs);
            return value_new_integer(expr->type, r);
        }
        case UOP_BITNOT: {
            struct value* const rhs =
                eval_rvalue(evaluator, expr->data.unary.rhs);
            assert(rhs->type->kind == TYPE_BYTE || type_is_integer(rhs->type));

            if (rhs->type->kind == TYPE_BYTE) {
                rhs->data.byte = (uint8_t)~rhs->data.byte;
                return rhs;
            }

            bool is_signed = type_is_sinteger(rhs->type);
            size_t bit_count = rhs->type->size * 8u;
            struct autil_bitarr* const rhs_bits = autil_bitarr_new(bit_count);
            struct autil_bitarr* const res_bits = autil_bitarr_new(bit_count);
            if (bigint_to_bitarr(rhs_bits, rhs->data.integer)) {
                UNREACHABLE();
            }

            for (size_t i = 0; i < bit_count; ++i) {
                int const bit = !autil_bitarr_get(rhs_bits, i);
                autil_bitarr_set(res_bits, i, bit);
            }
            autil_bitarr_del(rhs_bits);

            struct autil_bigint* const res_bigint =
                autil_bigint_new(AUTIL_BIGINT_ZERO);
            bitarr_to_bigint(res_bigint, res_bits, is_signed);
            autil_bitarr_del(res_bits);

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
            return eval_lvalue(evaluator, expr);
        }
        }
        UNREACHABLE();
    }
    case TIR_EXPR_BINARY: {
        struct value* const lhs = eval_rvalue(evaluator, expr->data.binary.lhs);
        struct value* const rhs = eval_rvalue(evaluator, expr->data.binary.rhs);
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
            assert(type_is_integer(lhs->type));
            assert(type_is_integer(rhs->type));
            struct autil_bigint* const r = autil_bigint_new(AUTIL_BIGINT_ZERO);
            autil_bigint_add(r, lhs->data.integer, rhs->data.integer);
            if (integer_is_out_of_range(expr->type, r)) {
                fatal(
                    expr->location,
                    "arithmetic operation produces out-of-range result (%s + %s == %s)",
                    autil_bigint_to_new_cstr(lhs->data.integer, NULL),
                    autil_bigint_to_new_cstr(rhs->data.integer, NULL),
                    autil_bigint_to_new_cstr(r, NULL));
            }
            res = value_new_integer(expr->type, r);
            break;
        }
        case BOP_SUB: {
            assert(type_is_integer(lhs->type));
            assert(type_is_integer(rhs->type));
            struct autil_bigint* const r = autil_bigint_new(AUTIL_BIGINT_ZERO);
            autil_bigint_sub(r, lhs->data.integer, rhs->data.integer);
            if (integer_is_out_of_range(expr->type, r)) {
                fatal(
                    expr->location,
                    "arithmetic operation produces out-of-range result (%s - %s == %s)",
                    autil_bigint_to_new_cstr(lhs->data.integer, NULL),
                    autil_bigint_to_new_cstr(rhs->data.integer, NULL),
                    autil_bigint_to_new_cstr(r, NULL));
            }
            res = value_new_integer(expr->type, r);
            break;
        }
        case BOP_MUL: {
            assert(type_is_integer(lhs->type));
            assert(type_is_integer(rhs->type));
            struct autil_bigint* const r = autil_bigint_new(AUTIL_BIGINT_ZERO);
            autil_bigint_mul(r, lhs->data.integer, rhs->data.integer);
            if (integer_is_out_of_range(expr->type, r)) {
                fatal(
                    expr->location,
                    "arithmetic operation produces out-of-range result (%s * %s == %s)",
                    autil_bigint_to_new_cstr(lhs->data.integer, NULL),
                    autil_bigint_to_new_cstr(rhs->data.integer, NULL),
                    autil_bigint_to_new_cstr(r, NULL));
            }
            res = value_new_integer(expr->type, r);
            break;
        }
        case BOP_DIV: {
            assert(type_is_integer(lhs->type));
            assert(type_is_integer(rhs->type));
            if (autil_bigint_cmp(rhs->data.integer, AUTIL_BIGINT_ZERO) == 0) {
                fatal(
                    expr->location,
                    "divide by zero (%s / %s)",
                    autil_bigint_to_new_cstr(lhs->data.integer, NULL),
                    autil_bigint_to_new_cstr(rhs->data.integer, NULL));
            }
            struct autil_bigint* const r = autil_bigint_new(AUTIL_BIGINT_ZERO);
            autil_bigint_divrem(r, NULL, lhs->data.integer, rhs->data.integer);
            res = value_new_integer(expr->type, r);
            break;
        }
        case BOP_BITOR: {
            assert(
                lhs->type->kind == TYPE_BOOL || lhs->type->kind == TYPE_BYTE
                || type_is_integer(lhs->type));
            assert(
                rhs->type->kind == TYPE_BOOL || rhs->type->kind == TYPE_BYTE
                || type_is_integer(rhs->type));
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

            assert(type_is_integer(type));
            bool is_signed = type_is_sinteger(type);
            size_t bit_count = type->size * 8u;
            struct autil_bitarr* const lhs_bits = autil_bitarr_new(bit_count);
            struct autil_bitarr* const rhs_bits = autil_bitarr_new(bit_count);
            struct autil_bitarr* const res_bits = autil_bitarr_new(bit_count);
            if (bigint_to_bitarr(lhs_bits, lhs->data.integer)) {
                UNREACHABLE();
            }
            if (bigint_to_bitarr(rhs_bits, rhs->data.integer)) {
                UNREACHABLE();
            }

            for (size_t i = 0; i < bit_count; ++i) {
                int const bit = autil_bitarr_get(lhs_bits, i)
                    || autil_bitarr_get(rhs_bits, i);
                autil_bitarr_set(res_bits, i, bit);
            }
            autil_bitarr_del(lhs_bits);
            autil_bitarr_del(rhs_bits);

            struct autil_bigint* const res_bigint =
                autil_bigint_new(AUTIL_BIGINT_ZERO);
            bitarr_to_bigint(res_bigint, res_bits, is_signed);
            autil_bitarr_del(res_bits);

            res = value_new_integer(type, res_bigint);
            break;
        }
        case BOP_BITXOR: {
            assert(
                lhs->type->kind == TYPE_BOOL || lhs->type->kind == TYPE_BYTE
                || type_is_integer(lhs->type));
            assert(
                rhs->type->kind == TYPE_BOOL || rhs->type->kind == TYPE_BYTE
                || type_is_integer(rhs->type));
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

            assert(type_is_integer(type));
            bool is_signed = type_is_sinteger(type);
            size_t bit_count = type->size * 8u;
            struct autil_bitarr* const lhs_bits = autil_bitarr_new(bit_count);
            struct autil_bitarr* const rhs_bits = autil_bitarr_new(bit_count);
            struct autil_bitarr* const res_bits = autil_bitarr_new(bit_count);
            if (bigint_to_bitarr(lhs_bits, lhs->data.integer)) {
                UNREACHABLE();
            }
            if (bigint_to_bitarr(rhs_bits, rhs->data.integer)) {
                UNREACHABLE();
            }

            for (size_t i = 0; i < bit_count; ++i) {
                int const bit = autil_bitarr_get(lhs_bits, i)
                    ^ autil_bitarr_get(rhs_bits, i);
                autil_bitarr_set(res_bits, i, bit);
            }
            autil_bitarr_del(lhs_bits);
            autil_bitarr_del(rhs_bits);

            struct autil_bigint* const res_bigint =
                autil_bigint_new(AUTIL_BIGINT_ZERO);
            bitarr_to_bigint(res_bigint, res_bits, is_signed);
            autil_bitarr_del(res_bits);

            res = value_new_integer(type, res_bigint);
            break;
        }
        case BOP_BITAND: {
            assert(
                lhs->type->kind == TYPE_BOOL || lhs->type->kind == TYPE_BYTE
                || type_is_integer(lhs->type));
            assert(
                rhs->type->kind == TYPE_BOOL || rhs->type->kind == TYPE_BYTE
                || type_is_integer(rhs->type));
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

            assert(type_is_integer(type));
            bool is_signed = type_is_sinteger(type);
            size_t bit_count = type->size * 8u;
            struct autil_bitarr* const lhs_bits = autil_bitarr_new(bit_count);
            struct autil_bitarr* const rhs_bits = autil_bitarr_new(bit_count);
            struct autil_bitarr* const res_bits = autil_bitarr_new(bit_count);
            if (bigint_to_bitarr(lhs_bits, lhs->data.integer)) {
                UNREACHABLE();
            }
            if (bigint_to_bitarr(rhs_bits, rhs->data.integer)) {
                UNREACHABLE();
            }

            for (size_t i = 0; i < bit_count; ++i) {
                int const bit = autil_bitarr_get(lhs_bits, i)
                    && autil_bitarr_get(rhs_bits, i);
                autil_bitarr_set(res_bits, i, bit);
            }
            autil_bitarr_del(lhs_bits);
            autil_bitarr_del(rhs_bits);

            struct autil_bigint* const res_bigint =
                autil_bigint_new(AUTIL_BIGINT_ZERO);
            bitarr_to_bigint(res_bigint, res_bits, is_signed);
            autil_bitarr_del(res_bits);

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
    }

    UNREACHABLE();
    return NULL;
}

struct value*
eval_lvalue(struct evaluator* evaluator, struct tir_expr const* expr)
{
    assert(evaluator != NULL);
    assert(expr != NULL);

    switch (expr->kind) {
    case TIR_EXPR_IDENTIFIER: {
        struct symbol const* const symbol = expr->data.identifier;
        if (symbol->address->kind != ADDRESS_STATIC) {
            fatal(
                expr->location,
                "addressof operator applied to non-static object in compile-time expression");
        }
        struct type const* const type = type_unique_pointer(symbol->type);
        return value_new_pointer(type, *symbol->address);
    }
    case TIR_EXPR_INDEX: {
        struct value* const lhs = eval_lvalue(evaluator, expr->data.index.lhs);
        struct value* const idx = eval_rvalue(evaluator, expr->data.index.idx);
        assert(lhs->type->kind == TYPE_POINTER);
        assert(idx->type->kind == TYPE_USIZE);
        struct type const* const array_type = lhs->type->data.pointer.base;
        assert(array_type->kind == TYPE_ARRAY);
        struct type const* const element_type = array_type->data.array.base;
        struct type const* const type = type_unique_pointer(element_type);

        size_t idx_uz = 0u;
        if (bigint_to_uz(&idx_uz, idx->data.integer)) {
            char* const cstr =
                autil_bigint_to_new_cstr(idx->data.integer, NULL);
            fatal(
                expr->data.index.idx->location,
                "index out-of-range (received %s)",
                cstr);
        }

        assert(expr->data.index.lhs->type->kind == TYPE_ARRAY);
        if (idx_uz >= expr->data.index.lhs->type->data.array.count) {
            char* const cstr =
                autil_bigint_to_new_cstr(idx->data.integer, NULL);
            fatal(
                expr->data.index.idx->location,
                "index out-of-bounds (array count is %zu, received %s)",
                lhs->type->data.array.count,
                cstr);
        }

        assert(lhs->data.pointer.kind == ADDRESS_STATIC);
        struct address const address = address_init_static(
            lhs->data.pointer.data.static_.name,
            lhs->data.pointer.data.static_.offset
                + element_type->size * idx_uz);
        value_del(lhs);
        value_del(idx);
        return value_new_pointer(type, address);
    }
    case TIR_EXPR_UNARY: {
        switch (expr->data.unary.op) {
        case UOP_DEREFERENCE: {
            fatal(
                expr->location,
                "dereference operator not supported in compile-time expressions");
        }
        case UOP_ADDRESSOF: {
            return eval_lvalue(evaluator, expr->data.unary.rhs);
        }
        case UOP_NOT: /* fallthrough */
        case UOP_POS: /* fallthrough */
        case UOP_NEG: /* fallthrough */
        case UOP_BITNOT: /* fallthrough */
            assert(!tir_expr_is_lvalue(expr));
            UNREACHABLE();
        }
        UNREACHABLE();
    }
    case TIR_EXPR_BOOLEAN: /* fallthrough */
    case TIR_EXPR_INTEGER: /* fallthrough */
    case TIR_EXPR_ARRAY: /* fallthrough */
    case TIR_EXPR_SLICE: /* fallthrough */
    case TIR_EXPR_SYSCALL: /* fallthrough */
    case TIR_EXPR_CALL: /* fallthrough */
    case TIR_EXPR_BINARY: {
        assert(!tir_expr_is_lvalue(expr));
        UNREACHABLE();
    }
    }

    UNREACHABLE();
    return NULL;
}
