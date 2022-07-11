// Copyright 2021-2022 The Sunder Project Authors
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
    // Top-level declarations and their associated ordering state in the order
    // that they were seen by the parser. Initialized and populated within
    // order new. This stretchy buffer is not resized after it is initialized,
    // so addresses of each element are stable for the duration of the ordering
    // phase.
    sbuf(struct tldecl) tldecls;
    // Top-level declarations, topologically sorted such that any declaration
    // with index k does not depend on any declaration with index k+n for all
    // n. Initialized as an empty sbuf in orderer_new and populated during the
    // order phase.
    sbuf(struct cst_decl const*) topological_order;
};
static struct orderer*
orderer_new(struct module* module);
static void
orderer_del(struct orderer* self);
static struct tldecl*
orderer_tldecl_lookup(struct orderer* orderer, char const* name);

// Obtain a name used to identify the provided declaration within the currently
// ordering module. For all declarations apart from extend declarations, the
// returned value is the identifier associated with the declaration. Extend
// declarations, which are not true top-level declarations, and which may be
// declared multiple times using the same identifier but different extended
// types, are given a unique name based on their declaration order index.
static char const*
tldecl_name(struct cst_decl const* decl, size_t decl_order_index);

static void
order_tldecl(struct orderer* orderer, struct tldecl* tldecl);
static void
order_decl(struct orderer* orderer, struct cst_decl const* decl);
static void
order_expr(struct orderer* orderer, struct cst_expr const* expr);
static void
order_template_argument_list(
    struct orderer* orderer, struct cst_typespec const* const* arguments);
static void
order_function_parameter(
    struct orderer* orderer, struct cst_function_parameter const* parameter);
static void
order_typespec(struct orderer* orderer, struct cst_typespec const* typespec);
static void
order_symbol(struct orderer* orderer, struct cst_symbol const* symbol);
static void
order_identifier(
    struct orderer* orderer, struct cst_identifier const* identifier);
static void
order_name(struct orderer* orderer, char const* name);

static struct orderer*
orderer_new(struct module* module)
{
    assert(module != NULL);

    struct orderer* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));

    self->module = module;
    for (size_t i = 0; i < sbuf_count(module->cst->decls); ++i) {
        struct tldecl const tldecl = {TLDECL_UNORDERED, module->cst->decls[i]};
        struct tldecl const* const existing = orderer_tldecl_lookup(
            self,
            tldecl_name(module->cst->decls[i], sbuf_count(self->tldecls)));
        if (existing != NULL) {
            fatal(
                module->cst->decls[i]->location,
                "redeclaration of `%s` previously declared at [%s:%zu]",
                existing->decl->name,
                existing->decl->location->path,
                existing->decl->location->line);
        }

        sbuf_push(self->tldecls, tldecl);
    }

    return self;
}

static void
orderer_del(struct orderer* self)
{
    assert(self != NULL);

    sbuf_fini(self->tldecls);
    sbuf_fini(self->topological_order);

    memset(self, 0x00, sizeof(*self));
    xalloc(self, XALLOC_FREE);
}

static struct tldecl*
orderer_tldecl_lookup(struct orderer* orderer, char const* name)
{
    assert(orderer != NULL);
    assert(name != NULL);

    for (size_t i = 0; i < sbuf_count(orderer->tldecls); ++i) {
        if (tldecl_name(orderer->tldecls[i].decl, i) == name) {
            return &orderer->tldecls[i];
        }
    }

    return NULL;
}

static char const*
tldecl_name(struct cst_decl const* decl, size_t decl_order_index)
{
    assert(decl != NULL);

    if (decl->kind == CST_DECL_EXTEND) {
        // Extend declarations are not true top-level declarations as they are
        // not turned into a module-level symbol during the resolve phase. It
        // is possible to have multiple extend statements with the same
        // declaration name, so here we say the "name" of the extend
        // declaration takes the form "declaration N" where N is the index of
        // the declaration in the declaration-order-list.
        struct string* s = string_new_fmt("declaration %zu", decl_order_index);
        char const* const name = intern(string_start(s), string_count(s));
        string_del(s);
        return name;
    }

    return decl->name;
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
    tldecl->state = TLDECL_ORDERING;
    // Perform ordering on the top level declaration.
    order_decl(orderer, tldecl->decl);
    // Change the state from ORDERING TO ORDERED after ordering the top level
    // declaration as well as all of top level declaration's dependencies.
    tldecl->state = TLDECL_ORDERED;

    sbuf_push(orderer->topological_order, tldecl->decl);
}

static void
order_decl(struct orderer* orderer, struct cst_decl const* decl)
{
    assert(order != NULL);
    assert(decl != NULL);

    switch (decl->kind) {
    case CST_DECL_VARIABLE: {
        if (decl->data.variable.typespec != NULL) {
            order_typespec(orderer, decl->data.variable.typespec);
        }
        order_expr(orderer, decl->data.variable.expr);
        return;
    }
    case CST_DECL_CONSTANT: {
        if (decl->data.constant.typespec != NULL) {
            order_typespec(orderer, decl->data.constant.typespec);
        }
        order_expr(orderer, decl->data.constant.expr);
        return;
    }
    case CST_DECL_FUNCTION: {
        sbuf(struct cst_identifier const* const) const template_parameters =
            decl->data.function.template_parameters;
        if (sbuf_count(template_parameters) != 0) {
            // TODO: Not sure how we want to handle ordering of generic
            // functions. Ignoring the ordering of generic functions is fine
            // for now since the MVP of generics has been used to implement
            // dependency-free functions such as std::min and std::max. At some
            // point in the future we should and circle back to this and
            // re-evaluate the ordering of generics in general.
            //
            // See also => comment under CST_DECL_STRUCT case.
            return;
        }
        struct cst_function_parameter const* const* const function_parameters =
            decl->data.function.function_parameters;
        for (size_t i = 0; i < sbuf_count(function_parameters); ++i) {
            order_function_parameter(orderer, function_parameters[i]);
        }
        order_typespec(orderer, decl->data.function.return_typespec);
        return;
    }
    case CST_DECL_STRUCT: {
        sbuf(struct cst_identifier const* const) const template_parameters =
            decl->data.struct_.template_parameters;
        if (sbuf_count(template_parameters) != 0) {
            // TODO: Not sure how we want to handle ordering of generic
            // structs. Ignoring the ordering of generic structs is fine for
            // now since the MVP of generics has been used to implement
            // dependency-free structs such as std::int. At some point in the
            // future we should and circle back to this and re-evaluate the
            // ordering of generics in general.
            //
            // See also => comment under CST_DECL_FUNCTION case.
            return;
        }

        // Set this struct's state to ordered to allow for self-referential
        // members. This behavior mimics the behavior of the resolve phase
        // where all structs are completed after the struct symbol has been
        // added to the relevant symbol table.
        struct tldecl* const tldecl =
            orderer_tldecl_lookup(orderer, decl->name);
        if (tldecl == NULL) {
            // XXX: The returned tldecl may be NULL if this struct declaration
            // is part of an extend declaration. We return early here as the
            // will report this error when the extend declaration is resolved.
            return;
        }
        tldecl->state = TLDECL_ORDERED;

        // Order the struct's members.
        sbuf(struct cst_member const* const) const members =
            decl->data.struct_.members;
        for (size_t i = 0; i < sbuf_count(members); ++i) {
            struct cst_member const* const member = members[i];
            switch (member->kind) {
            case CST_MEMBER_VARIABLE: {
                order_typespec(orderer, member->data.variable.typespec);
                continue;
            }
            case CST_MEMBER_CONSTANT: {
                order_decl(orderer, member->data.constant.decl);
                continue;
            }
            case CST_MEMBER_FUNCTION: {
                order_decl(orderer, member->data.function.decl);
                continue;
            }
            default: {
                UNREACHABLE();
            }
            }
        }
        return;
    }
    case CST_DECL_EXTEND: {
        order_typespec(orderer, decl->data.extend.typespec);
        order_decl(orderer, decl->data.extend.decl);
        return;
    }
    case CST_DECL_ALIAS: {
        order_typespec(orderer, decl->data.alias.typespec);
        return;
    }
    case CST_DECL_EXTERN_VARIABLE: {
        order_typespec(orderer, decl->data.extern_variable.typespec);
        return;
    }
    case CST_DECL_EXTERN_FUNCTION: {
        struct cst_function_parameter const* const* const function_parameters =
            decl->data.extern_function.function_parameters;
        for (size_t i = 0; i < sbuf_count(function_parameters); ++i) {
            order_function_parameter(orderer, function_parameters[i]);
        }
        order_typespec(orderer, decl->data.extern_function.return_typespec);
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
    case CST_EXPR_SYMBOL: {
        order_symbol(orderer, expr->data.symbol);
        return;
    }
    case CST_EXPR_BOOLEAN: /* fallthrough */
    case CST_EXPR_INTEGER: /* fallthrough */
    case CST_EXPR_CHARACTER: /* fallthrough */
    case CST_EXPR_BYTES: {
        return;
    }
    case CST_EXPR_LIST: {
        sbuf(struct cst_expr const* const) const elements =
            expr->data.list.elements;
        for (size_t i = 0; i < sbuf_count(elements); ++i) {
            order_expr(orderer, elements[i]);
        }
        if (expr->data.list.ellipsis != NULL) {
            order_expr(orderer, expr->data.list.ellipsis);
        }
        return;
    }
    case CST_EXPR_SLICE: {
        order_expr(orderer, expr->data.slice.pointer);
        order_expr(orderer, expr->data.slice.count);
        return;
    }
    case CST_EXPR_STRUCT: {
        order_typespec(orderer, expr->data.struct_.typespec);
        sbuf(struct cst_member_initializer const* const) const initializers =
            expr->data.struct_.initializers;
        for (size_t i = 0; i < sbuf_count(initializers); ++i) {
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
    case CST_EXPR_CALL: {
        order_expr(orderer, expr->data.call.func);
        sbuf(struct cst_expr const* const) const arguments =
            expr->data.call.arguments;
        for (size_t i = 0; i < sbuf_count(arguments); ++i) {
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
        order_expr(orderer, expr->data.access_member.lhs);
        return;
    }
    case CST_EXPR_ACCESS_DEREFERENCE: {
        order_expr(orderer, expr->data.access_dereference.lhs);
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
    struct orderer* orderer, struct cst_typespec const* const* arguments)
{
    assert(orderer != NULL);

    for (size_t i = 0; i < sbuf_count(arguments); ++i) {
        order_typespec(orderer, arguments[i]);
    }
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
order_typespec(struct orderer* orderer, struct cst_typespec const* typespec)
{
    assert(orderer != NULL);
    assert(typespec != NULL);

    switch (typespec->kind) {
    case TYPESPEC_SYMBOL: {
        order_symbol(orderer, typespec->data.symbol);
        return;
    }
    case TYPESPEC_FUNCTION: {
        sbuf(struct cst_typespec const* const) const parameter_typespecs =
            typespec->data.function.parameter_typespecs;
        for (size_t i = 0; i < sbuf_count(parameter_typespecs); ++i) {
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
        // slices will always use two machine words for each array element,
        // etc. Since pointer and slice base types have a size (and alignment)
        // that does *not* depend on their base element type, we may skip
        // ordering the base element type during the ordering phase.
        //
        // Not requiring information on a base element type of a pointer type
        // (or pointer component of a DIY slice type) matches the behavior of
        // C, as pointer types may be used before their base type definition
        // has been completed (e.g. in the case of self-referential structs).
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
order_symbol(struct orderer* orderer, struct cst_symbol const* symbol)
{
    assert(orderer != NULL);
    assert(sbuf_count(symbol->elements) > 0);

    // If the namespace prefix matches the module namespace then order the
    // non-prefix portion of the symbol. The eventual call to order_name will
    // silently ignore symbols from other modules, but will correctly order any
    // symbols within *this* module.
    //
    // If this module does not have a namespace then order the symbol based on
    // its first symbol element. If that element corresponds to a declaration
    // in this module (e.g. a struct) then the symbol will be correctly
    // ordered. If that element does not correspond to a declaration in this
    // module then again the order_name will silently ignore it assuming it is
    // defined elsewhere.
    struct cst_namespace const* const namespace =
        orderer->module->cst->namespace;
    if (namespace == NULL) {
        // Module does not have a namespace. Perform ordering based on the
        // first symbol element in the symbol.
        order_identifier(orderer, symbol->elements[0]->identifier);
        order_template_argument_list(
            orderer, symbol->elements[0]->template_arguments);
        return;
    }

    size_t const namespace_count = sbuf_count(namespace->identifiers);
    assert(namespace_count >= 1);
    if (sbuf_count(symbol->elements) < namespace_count) {
        // Current symbol refers to some declaration under a parent namespace
        // or the parent namespace itself. Let the resolution of the symbol
        // occur during the resolve phase.
        return;
    }

    for (size_t i = 0; i < namespace_count; ++i) {
        bool const match = symbol->elements[i]->identifier->name
            == namespace->identifiers[i]->name;
        if (match) {
            // Continue matching against the current module namespace.
            continue;
        }

        // Module namespace does not fully match the current module namespace.
        // Assume that the symbol refers to a construct defined under a parent
        // namespace in some other module.
        return;
    }

    // Perform ordering based on the non-prefix portion of the symbol.
    order_identifier(orderer, symbol->elements[namespace_count]->identifier);
    order_template_argument_list(
        orderer, symbol->elements[namespace_count]->template_arguments);
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
    size_t const decl_count = sbuf_count(orderer->tldecls);

    for (size_t i = 0; i < decl_count; ++i) {
        struct cst_decl const* const decl = orderer->tldecls[i].decl;
        order_tldecl(
            orderer, orderer_tldecl_lookup(orderer, tldecl_name(decl, i)));
    }

    assert(decl_count == sbuf_count(orderer->topological_order));
    for (size_t i = 0; i < decl_count; ++i) {
        struct cst_decl const* const decl = orderer->topological_order[i];
        sbuf_push(module->ordered, decl);
    }

    orderer_del(orderer);
}
