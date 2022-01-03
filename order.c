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
order_template_argument_list(
    struct orderer* orderer,
    struct cst_template_argument const* const* arguments);
static void
order_template_argument(
    struct orderer* orderer, struct cst_template_argument const* argument);
static void
order_function_parameter(
    struct orderer* orderer, struct cst_function_parameter const* parameter);
static void
order_member(struct orderer* orderer, struct cst_member const* member);
static void
order_typespec(struct orderer* orderer, struct cst_typespec const* typespec);
static void
order_identifier(
    struct orderer* orderer, struct cst_identifier const* identifier);
static void
order_qualified_identifier(
    struct orderer* orderer, struct cst_identifier const* const* identifiers);
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
    // Special case for structs with member functions. Member functions may
    // self-reference the struct that they are defined within (e.g. the return
    // type of init functions). Member functions have their ordering deferred
    // until after the struct has been ordered (here), mirroring the behavior of
    // the resolve phase where member functions are resolved after the struct
    // member variables & layout have been resolved.
    if (tldecl->decl->kind == CST_DECL_STRUCT) {
        autil_sbuf(struct cst_member const* const) const members =
            tldecl->decl->data.struct_.members;
        for (size_t i = 0; i < autil_sbuf_count(members); ++i) {
            if (members[i]->kind != CST_MEMBER_FUNCTION) {
                continue;
            }
            order_decl(orderer, members[i]->data.function.decl);
        }
    }
}

static void
order_decl(struct orderer* orderer, struct cst_decl const* decl)
{
    assert(order != NULL);
    assert(decl != NULL);

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
        struct cst_template_parameter const* const* const template_parameters =
            decl->data.function.template_parameters;
        if (autil_sbuf_count(template_parameters) != 0) {
            // TODO: Not sure how we want to handle ordering of generics yet.
            // Since the MVP of generics will be used to implement
            // dependency-free functions like std::min and std::max we will
            // ignore this ordering problem for now and circle back to it at
            // some point in the future.
            //
            // See also => comment under CST_DECL_STRUCT case.
            return;
        }
        struct cst_function_parameter const* const* const function_parameters =
            decl->data.function.function_parameters;
        for (size_t i = 0; i < autil_sbuf_count(function_parameters); ++i) {
            order_function_parameter(orderer, function_parameters[i]);
        }
        order_typespec(orderer, decl->data.function.return_typespec);
        return;
    }
    case CST_DECL_STRUCT: {
        struct cst_member const* const* const members =
            decl->data.struct_.members;
        struct cst_template_parameter const* const* const template_parameters =
            decl->data.struct_.template_parameters;
        if (autil_sbuf_count(template_parameters) != 0) {
            // TODO: Not sure how we want to handle ordering of generics yet.
            // Since the MVP of generics will be used to implement
            // dependency-free functions like std::primitive and std::vec we
            // will ignore this ordering problem for now and circle back to it
            // at some point in the future.
            //
            // See also => comment under CST_DECL_FUNCTION case.
            return;
        }
        for (size_t i = 0; i < autil_sbuf_count(members); ++i) {
            order_member(orderer, members[i]);
        }
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
        order_qualified_identifier(
            orderer, expr->data.qualified_identifier.identifiers);
        return;
    }
    case CST_EXPR_TEMPLATE_INSTANTIATION: {
        order_identifier(orderer, expr->data.template_instantiation.identifier);
        order_template_argument_list(
            orderer, expr->data.template_instantiation.arguments);
        return;
    }
    case CST_EXPR_QUALIFIED_TEMPLATE_INSTANTIATION: {
        order_qualified_identifier(
            orderer, expr->data.qualified_template_instantiation.identifiers);
        order_template_argument_list(
            orderer, expr->data.template_instantiation.arguments);
        return;
    }
    case CST_EXPR_BOOLEAN: /* fallthrough */
    case CST_EXPR_INTEGER: /* fallthrough */
    case CST_EXPR_CHARACTER: /* fallthrough */
    case CST_EXPR_BYTES: {
        return;
    }
    case CST_EXPR_ARRAY: {
        autil_sbuf(struct cst_expr const* const) const elements =
            expr->data.array.elements;
        for (size_t i = 0; i < autil_sbuf_count(elements); ++i) {
            order_expr(orderer, elements[i]);
        }
        if (expr->data.array.ellipsis != NULL) {
            order_expr(orderer, expr->data.array.ellipsis);
        }
        return;
    }
    case CST_EXPR_SLICE: {
        order_expr(orderer, expr->data.slice.pointer);
        order_expr(orderer, expr->data.slice.count);
        return;
    }
    case CST_EXPR_STRUCT: {
        autil_sbuf(struct cst_member_initializer const* const)
            const initializers = expr->data.struct_.initializers;
        for (size_t i = 0; i < autil_sbuf_count(initializers); ++i) {
            order_expr(orderer, initializers[i]->expr);
        }
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
    case CST_EXPR_ACCESS_MEMBER: {
        order_expr(orderer, expr->data.access_slice.lhs);
        return;
    }
    case CST_EXPR_SIZEOF: {
        order_typespec(orderer, expr->data.sizeof_.rhs);
        return;
    }
    case CST_EXPR_ALIGNOF: {
        order_typespec(orderer, expr->data.alignof_.rhs);
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
order_template_argument_list(
    struct orderer* orderer,
    struct cst_template_argument const* const* arguments)
{
    assert(orderer != NULL);

    for (size_t i = 0; i < autil_sbuf_count(arguments); ++i) {
        order_template_argument(orderer, arguments[i]);
    }
}

static void
order_template_argument(
    struct orderer* orderer, struct cst_template_argument const* argument)
{
    assert(orderer != NULL);
    assert(argument != NULL);

    order_typespec(orderer, argument->typespec);
}

static void
order_function_parameter(
    struct orderer* orderer, struct cst_function_parameter const* parameter)
{
    assert(orderer != NULL);
    assert(parameter != NULL);

    order_typespec(orderer, parameter->typespec);
}

static void
order_member(struct orderer* orderer, struct cst_member const* member)
{
    assert(orderer != NULL);
    assert(member != NULL);

    switch (member->kind) {
    case CST_MEMBER_VARIABLE: {
        order_typespec(orderer, member->data.variable.typespec);
        return;
    }
    case CST_MEMBER_FUNCTION: {
        // Defer ordering member functions until the struct has been ordered in
        // order to account for self-referential member functions. There is a
        // special case to handle this logic in the order_tldecl function.
        return;
    }
    }

    UNREACHABLE();
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
    case TYPESPEC_TEMPLATE_INSTANTIATION: {
        order_identifier(
            orderer, typespec->data.template_instantiation.identifier);
        order_template_argument_list(
            orderer, typespec->data.template_instantiation.arguments);
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
    case TYPESPEC_ARRAY: {
        order_expr(orderer, typespec->data.array.count);
        order_typespec(orderer, typespec->data.array.base);
        return;
    }
    case TYPESPEC_TYPEOF: {
        order_expr(orderer, typespec->data.typeof_.expr);
        return;
    }
    case TYPESPEC_POINTER: /* fallthrough */
    case TYPESPEC_SLICE: {
        // Pointer and slice type specifiers are unique because the size of
        // those types will always be the same regardless of the type of their
        // base element. A pointer will always use one machine word of space (8
        // bytes on x64) and a slice will always use two machine words of space
        // (16 bytes on x64). By extension, any data structure that contains a
        // pointer or slice type will also always have the same size regardless
        // of the base element type. A struct containing a pointer member will
        // always use one machine word for that pointer member, an array of
        // slices will always use two machine words for each array element, etc.
        // Since pointer and slice base types have a size (and alignment) that
        // does *not* depend on their base element type, we may skip ordering
        // the base element type during the ordering phase.
        //
        // Not requiring information on a base element type of a pointer type
        // (or pointer component of a DIY slice type) matches the behavior of C,
        // as pointer types may be used before their base type definition has
        // been completed (e.g. in the case of self-referential structs).
        //
        //     struct x {
        //         /* var pointer: *x; */
        //         struct x* pointer;
        //     };
        //
        //     struct y {
        //         /* var slice: []y; */
        //         struct {
        //             struct y* start;
        //             size_t count;
        //         } slice;
        //     };
        return;
    }
    }

    UNREACHABLE();
}

static void
order_qualified_identifier(
    struct orderer* orderer, struct cst_identifier const* const* identifiers)
{
    assert(orderer != NULL);
    assert(autil_sbuf_count(identifiers) > 1);

    // If the namespace prefix matches the module namespace then order the leaf
    // identifier of the qualified identifier. The eventual call to order_name
    // will silently ignore identifiers from other modules, but will correctly
    // order any identifiers within *this* module.
    struct cst_namespace const* const namespace =
        orderer->module->cst->namespace;
    if (namespace == NULL) {
        // Module does not have a namespace.
        return;
    }

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
