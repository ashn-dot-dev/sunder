// Copyright 2021 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sunder.h"

// Top-Level Declaration
struct tldecl {
    enum {
        TLDECL_UNORDERED,
        TLDECL_ORDERING,
        TLDECL_ORDERED,
    } state;
    struct cst_decl const* decl;
};

struct orderer {
    struct module* module;
    struct {
        // Mapping from each top-level declaration name to its corresponding
        // tldecl structure. Initialized & populated in orderer_new.
#define TLDECL_MAP_KEY_TYPE char const*
#define TLDECL_MAP_VAL_TYPE struct tldecl
#define TLDECL_MAP_CMP_FUNC autil_cstr_vpcmp
        struct autil_map* map; // Populated in orderer_new.
        // Names of each top-level declaration in the order seen by the parser.
        // Initialized & populated in orderer_new.
        autil_sbuf(char const*) declaration_order;
        // Names of each top-level declaration, topologically sorted such that
        // any declaration with index k does not depend on any declaration with
        // index k+n for all n. Initialized as an empty sbuf in orderer_new and
        // populated during the order phase.
        autil_sbuf(char const*) topological_order;
    } tldecls;
};
static struct orderer*
orderer_new(struct module* module);
static void
orderer_del(struct orderer* self);
static void
orderer_tldecl_insert(struct orderer* orderer, struct cst_decl const* decl);
static struct tldecl*
orderer_tldecl_lookup(struct orderer* orderer, char const* name);

static void
order_tldecl(struct orderer* orderer, struct tldecl* tldecl);
static void
order_decl(struct orderer* orderer, struct cst_decl const* decl);
static void
order_expr(struct orderer* orderer, struct cst_expr const* expr);
static void
order_parameter(struct orderer* orderer, struct cst_parameter const* parameter);
static void
order_typespec(struct orderer* orderer, struct cst_typespec const* typespec);
static void
order_identifier(
    struct orderer* orderer, struct cst_identifier const* identifier);
static void
order_name(struct orderer* orderer, char const* name);

static struct orderer*
orderer_new(struct module* module)
{
    assert(module != NULL);

    struct orderer* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->module = module;
    self->tldecls.map = autil_map_new(
        sizeof(TLDECL_MAP_KEY_TYPE),
        sizeof(TLDECL_MAP_VAL_TYPE),
        TLDECL_MAP_CMP_FUNC);
    for (size_t i = 0; i < autil_sbuf_count(module->cst->decls); ++i) {
        orderer_tldecl_insert(self, module->cst->decls[i]);
    }
    return self;
}

static void
orderer_del(struct orderer* self)
{
    assert(self != NULL);

    autil_map_del(self->tldecls.map);
    autil_sbuf_fini(self->tldecls.declaration_order);
    autil_sbuf_fini(self->tldecls.topological_order);

    memset(self, 0x00, sizeof(*self));
    autil_xalloc(self, AUTIL_XALLOC_FREE);
}

static void
orderer_tldecl_insert(struct orderer* orderer, struct cst_decl const* decl)
{
    assert(orderer != NULL);
    assert(decl != NULL);

    struct tldecl const tldecl = {TLDECL_UNORDERED, decl};

    autil_sbuf_push(orderer->tldecls.declaration_order, decl->name);
    struct tldecl const* existing =
        autil_map_lookup_const(orderer->tldecls.map, &decl->name);
    if (existing != NULL) {
        fatal(
            decl->location,
            "redeclaration of `%s` previously declared at [%s:%zu]",
            existing->decl->name,
            existing->decl->location->path,
            existing->decl->location->line);
    }

    autil_map_insert(orderer->tldecls.map, &decl->name, &tldecl, NULL, NULL);
}

static struct tldecl*
orderer_tldecl_lookup(struct orderer* orderer, char const* name)
{
    assert(orderer != NULL);
    assert(name != NULL);

    return autil_map_lookup(orderer->tldecls.map, &name);
}

static void
order_tldecl(struct orderer* orderer, struct tldecl* tldecl)
{
    assert(orderer != NULL);
    assert(tldecl != NULL);

    if (tldecl->state == TLDECL_ORDERED) {
        // Top-level declaration is already ordered.
        return;
    }
    if (tldecl->state == TLDECL_ORDERING) {
        // Top-level declaration is currently in the process of being ordered.
        fatal(
            tldecl->decl->location,
            "circular dependency created by declaration of `%s`",
            tldecl->decl->name);
    }

    // Change the state from UNORDERED to ORDERING so that cyclic dependencies
    // will be detected if another attempt is made to order this declaration.
    assert(tldecl->state == TLDECL_UNORDERED);
    tldecl->state = TLDECL_ORDERING;

    order_decl(orderer, tldecl->decl);
    tldecl->state = TLDECL_ORDERED;
    autil_sbuf_push(orderer->tldecls.topological_order, tldecl->decl->name);
}

static void
order_decl(struct orderer* orderer, struct cst_decl const* decl)
{
    switch (decl->kind) {
    case CST_DECL_VARIABLE: {
        order_typespec(orderer, decl->data.variable.typespec);
        order_expr(orderer, decl->data.variable.expr);
        return;
    }
    case CST_DECL_CONSTANT: {
        order_typespec(orderer, decl->data.constant.typespec);
        order_expr(orderer, decl->data.constant.expr);
        return;
    }
    case CST_DECL_FUNCTION: {
        struct cst_parameter const* const* const parameters =
            decl->data.function.parameters;
        for (size_t i = 0; i < autil_sbuf_count(parameters); ++i) {
            order_parameter(orderer, parameters[i]);
        }
        order_typespec(orderer, decl->data.function.return_typespec);
        return;
    }
    case CST_DECL_EXTERN_VARIABLE: {
        order_typespec(orderer, decl->data.extern_variable.typespec);
        return;
    }
    }

    UNREACHABLE();
}

static void
order_expr(struct orderer* orderer, struct cst_expr const* expr)
{
    assert(orderer != NULL);
    assert(expr != NULL);

    switch (expr->kind) {
    case CST_EXPR_IDENTIFIER: {
        order_identifier(orderer, expr->data.identifier);
        return;
    }
    case CST_EXPR_QUALIFIED_IDENTIFIER: {
        // If the namespace prefix matches the module namespace then order the
        // leaf identifier of the qualified identifier. The eventual call to
        // order_name will silently ignore identifiers from other modules, but
        // will correctly order any identifiers within *this* module.
        struct cst_namespace const* const namespace =
            orderer->module->cst->namespace;
        if (namespace == NULL) {
            // Module does not have a namespace.
            return;
        }
        autil_sbuf(struct cst_identifier const* const) const identifiers =
            expr->data.qualified_identifier.identifiers;
        size_t const prefix_count = autil_sbuf_count(identifiers) - 1;
        assert(prefix_count >= 1);
        for (size_t i = 0; i < prefix_count; ++i) {
            if (identifiers[i]->name != namespace->identifiers[i]->name) {
                // Module namespace does not match qualified identifier
                // namespace.
                return;
            }
        }

        // The actual identifier.
        order_identifier(orderer, identifiers[prefix_count]);
        return;
    }
    case CST_EXPR_BOOLEAN: /* fallthrough */
    case CST_EXPR_INTEGER: /* fallthrough */
    case CST_EXPR_BYTES: {
        return;
    }
    case CST_EXPR_LITERAL_ARRAY: {
        autil_sbuf(struct cst_expr const* const) const elements =
            expr->data.literal_array.elements;
        for (size_t i = 0; i < autil_sbuf_count(elements); ++i) {
            order_expr(orderer, elements[i]);
        }
        if (expr->data.literal_array.ellipsis != NULL) {
            order_expr(orderer, expr->data.literal_array.ellipsis);
        }
        return;
    }
    case CST_EXPR_LITERAL_SLICE: {
        order_expr(orderer, expr->data.literal_slice.pointer);
        order_expr(orderer, expr->data.literal_slice.count);
        return;
    }
    case CST_EXPR_CAST: {
        order_typespec(orderer, expr->data.cast.typespec);
        order_expr(orderer, expr->data.cast.expr);
        return;
    }
    case CST_EXPR_GROUPED: {
        order_expr(orderer, expr->data.grouped.expr);
        return;
    }
    case CST_EXPR_SYSCALL: {
        autil_sbuf(struct cst_expr const* const) const arguments =
            expr->data.syscall.arguments;
        for (size_t i = 0; i < autil_sbuf_count(arguments); ++i) {
            order_expr(orderer, arguments[i]);
        }
        return;
    }
    case CST_EXPR_CALL: {
        order_expr(orderer, expr->data.call.func);
        autil_sbuf(struct cst_expr const* const) const arguments =
            expr->data.call.arguments;
        for (size_t i = 0; i < autil_sbuf_count(arguments); ++i) {
            order_expr(orderer, arguments[i]);
        }
        return;
    }
    case CST_EXPR_ACCESS_INDEX: {
        order_expr(orderer, expr->data.access_index.lhs);
        order_expr(orderer, expr->data.access_index.idx);
        return;
    }
    case CST_EXPR_ACCESS_SLICE: {
        order_expr(orderer, expr->data.access_slice.lhs);
        order_expr(orderer, expr->data.access_slice.begin);
        order_expr(orderer, expr->data.access_slice.end);
        return;
    }
    case CST_EXPR_SIZEOF: {
        order_typespec(orderer, expr->data.sizeof_.rhs);
        return;
    }
    case CST_EXPR_UNARY: {
        order_expr(orderer, expr->data.unary.rhs);
        return;
    }
    case CST_EXPR_BINARY: {
        order_expr(orderer, expr->data.binary.lhs);
        order_expr(orderer, expr->data.binary.rhs);
        return;
    }
    }

    UNREACHABLE();
}

static void
order_parameter(struct orderer* orderer, struct cst_parameter const* parameter)
{
    assert(orderer != NULL);
    assert(parameter != NULL);

    order_typespec(orderer, parameter->typespec);
}

static void
order_typespec(struct orderer* orderer, struct cst_typespec const* typespec)
{
    assert(orderer != NULL);
    assert(typespec != NULL);

    switch (typespec->kind) {
    case TYPESPEC_IDENTIFIER: {
        order_identifier(orderer, typespec->data.identifier);
        return;
    }
    case TYPESPEC_FUNCTION: {
        autil_sbuf(struct cst_typespec const* const) const parameter_typespecs =
            typespec->data.function.parameter_typespecs;
        for (size_t i = 0; i < autil_sbuf_count(parameter_typespecs); ++i) {
            order_typespec(orderer, parameter_typespecs[i]);
        }

        order_typespec(orderer, typespec->data.function.return_typespec);
        return;
    }
    case TYPESPEC_POINTER: {
        order_typespec(orderer, typespec->data.pointer.base);
        return;
    }
    case TYPESPEC_ARRAY: {
        order_expr(orderer, typespec->data.array.count);
        order_typespec(orderer, typespec->data.array.base);
        return;
    }
    case TYPESPEC_SLICE: {
        order_typespec(orderer, typespec->data.slice.base);
        return;
    }
    case TYPESPEC_TYPEOF: {
        order_expr(orderer, typespec->data.typeof.expr);
        return;
    }
    }

    UNREACHABLE();
}

static void
order_identifier(
    struct orderer* orderer, struct cst_identifier const* identifier)
{
    assert(orderer != NULL);
    assert(identifier != NULL);

    order_name(orderer, identifier->name);
}

static void
order_name(struct orderer* orderer, char const* name)
{
    assert(orderer != NULL);
    assert(name != NULL);

    struct tldecl* const tldecl = orderer_tldecl_lookup(orderer, name);
    if (tldecl == NULL) {
        // Top-level declaration with the provided name does not exist. Assume
        // that it is a builtin and allow future stages of semantic analysis to
        // raise an unknown identifier error if necessary.
        return;
    }

    order_tldecl(orderer, tldecl);
}

void
order(struct module* module)
{
    assert(module != NULL);

    struct orderer* const orderer = orderer_new(module);
    size_t const decl_count = autil_map_count(orderer->tldecls.map);

    assert(decl_count == autil_sbuf_count(orderer->tldecls.declaration_order));
    for (size_t i = 0; i < decl_count; ++i) {
        char const* const name = orderer->tldecls.declaration_order[i];
        order_tldecl(orderer, orderer_tldecl_lookup(orderer, name));
    }

    assert(decl_count == autil_sbuf_count(orderer->tldecls.topological_order));
    for (size_t i = 0; i < decl_count; ++i) {
        char const* const name = orderer->tldecls.topological_order[i];
        struct cst_decl const* const decl =
            orderer_tldecl_lookup(orderer, name)->decl;
        autil_sbuf_push(module->ordered, decl);
    }

    orderer_del(orderer);
}
