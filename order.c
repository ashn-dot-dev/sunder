// Copyright 2021-2023 The Sunder Project Authors
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
    // List of declaration dependencies.
    sbuf(struct cst_decl const*) dependencies;
};
static struct orderer*
orderer_new(struct module* module);
static void
orderer_del(struct orderer* self);
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
    struct orderer* orderer, struct cst_typespec const* const* arguments);
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
        struct tldecl const* const existing =
            orderer_tldecl_lookup(self, module->cst->decls[i]->name);
        if (existing != NULL && tldecl.decl->kind != CST_DECL_EXTEND) {
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
    assert(sbuf_count(self->dependencies) == 0);

    sbuf_fini(self->tldecls);
    sbuf_fini(self->topological_order);
    sbuf_fini(self->dependencies);

    memset(self, 0x00, sizeof(*self));
    xalloc(self, XALLOC_FREE);
}

static struct tldecl*
orderer_tldecl_lookup(struct orderer* orderer, char const* name)
{
    assert(orderer != NULL);
    assert(name != NULL);

    for (size_t i = 0; i < sbuf_count(orderer->tldecls); ++i) {
        struct cst_decl const* const decl = orderer->tldecls[i].decl;
        if (decl->name == name && decl->kind != CST_DECL_EXTEND) {
            return &orderer->tldecls[i];
        }
    }

    return NULL;
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
        error(
            tldecl->decl->location,
            "circular dependency created by declaration of `%s`",
            tldecl->decl->name);
        for (size_t i = 0; i < sbuf_count(orderer->dependencies); ++i) {
            size_t j = i + 1 != sbuf_count(orderer->dependencies) ? i + 1 : 0;
            info(
                NULL,
                "declaration of `%s` (line %zu) depends on `%s` (line %zu)",
                orderer->dependencies[i]->name,
                orderer->dependencies[i]->location->line,
                orderer->dependencies[j]->name,
                orderer->dependencies[j]->location->line);
        }
        exit(EXIT_FAILURE);
    }

    // Change the state from UNORDERED to ORDERING so that cyclic dependencies
    // will be detected if another attempt is made to order this declaration.
    tldecl->state = TLDECL_ORDERING;
    // Perform ordering on the top level declaration.
    sbuf_push(orderer->dependencies, tldecl->decl);
    order_decl(orderer, tldecl->decl);
    sbuf_resize(orderer->dependencies, sbuf_count(orderer->dependencies) - 1);
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
        if (decl->data.variable.expr != NULL) {
            order_expr(orderer, decl->data.variable.expr);
        }
        return;
    }
    case CST_DECL_CONSTANT: {
        if (decl->data.constant.typespec != NULL) {
            order_typespec(orderer, decl->data.constant.typespec);
        }
        if (decl->data.constant.expr != NULL) {
            order_expr(orderer, decl->data.constant.expr);
        }
        return;
    }
    case CST_DECL_FUNCTION: {
        sbuf(struct cst_identifier const* const) const template_parameters =
            decl->data.function.template_parameters;
        if (sbuf_count(template_parameters) != 0) {
            return;
        }
        struct cst_function_parameter const* const* const function_parameters =
            decl->data.function.function_parameters;
        for (size_t i = 0; i < sbuf_count(function_parameters); ++i) {
            order_typespec(orderer, function_parameters[i]->typespec);
        }
        order_typespec(orderer, decl->data.function.return_typespec);
        return;
    }
    case CST_DECL_STRUCT: {
        sbuf(struct cst_identifier const* const) const template_parameters =
            decl->data.struct_.template_parameters;
        if (sbuf_count(template_parameters) != 0) {
            return;
        }

        // Set this struct's state to ordered to allow for self-referential
        // members. This behavior mimics the behavior of the resolve phase
        // where all structs are completed after the struct symbol has been
        // added to the relevant symbol table.
        struct tldecl* const tldecl =
            orderer_tldecl_lookup(orderer, decl->name);
        if (tldecl == NULL) {
            // The returned tldecl may be NULL if this struct declaration is
            // part of an extend declaration. We return early here since an
            // error will be reported when the extend declaration is resolved.
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
        // Extend declarations are resolved in declaration order after all
        // module-level declarations, so no ordering of the extended type
        // specifier or the extending declaration is required.
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
            order_typespec(orderer, function_parameters[i]->typespec);
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
            if (initializers[i]->expr != NULL) {
                order_expr(orderer, initializers[i]->expr);
            }
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
order_typespec(struct orderer* orderer, struct cst_typespec const* typespec)
{
    assert(orderer != NULL);
    assert(typespec != NULL);

    switch (typespec->kind) {
    case CST_TYPESPEC_SYMBOL: {
        order_symbol(orderer, typespec->data.symbol);
        return;
    }
    case CST_TYPESPEC_FUNCTION: {
        sbuf(struct cst_typespec const* const) const parameter_typespecs =
            typespec->data.function.parameter_typespecs;
        for (size_t i = 0; i < sbuf_count(parameter_typespecs); ++i) {
            order_typespec(orderer, parameter_typespecs[i]);
        }

        order_typespec(orderer, typespec->data.function.return_typespec);
        return;
    }
    case CST_TYPESPEC_ARRAY: {
        order_expr(orderer, typespec->data.array.count);
        order_typespec(orderer, typespec->data.array.base);
        return;
    }
    case CST_TYPESPEC_TYPEOF: {
        order_expr(orderer, typespec->data.typeof_.expr);
        return;
    }
    case CST_TYPESPEC_POINTER: {
        order_typespec(orderer, typespec->data.pointer.base);
        return;
    }
    case CST_TYPESPEC_SLICE: {
        order_typespec(orderer, typespec->data.slice.base);
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

    // Always attempt to order all symbol template arguments, regardless of
    // whether the symbol belongs to the current module or not, since symbol
    // template arguments may refer to symbols that *are* in this module.
    for (size_t i = 0; i < sbuf_count(symbol->elements); ++i) {
        order_template_argument_list(
            orderer, symbol->elements[i]->template_arguments);
    }

    char const* const symbol_elem0_name = symbol->elements[0]->identifier->name;
    bool const symbol_elem0_defined_in_current_module =
        orderer_tldecl_lookup(orderer, symbol_elem0_name) != NULL;
    bool const search_qualified_symbol =
        symbol->is_from_root || !symbol_elem0_defined_in_current_module;
    if (search_qualified_symbol) {
        struct cst_namespace const* const namespace =
            orderer->module->cst->namespace;
        size_t const namespace_count =
            namespace != NULL ? sbuf_count(namespace->identifiers) : 0;
        for (size_t i = 0; i < namespace_count; ++i) {
            char const* const element_name =
                symbol->elements[i]->identifier->name;
            char const* const namespace_name = namespace->identifiers[i]->name;
            if (element_name == namespace_name) {
                // Continue matching against the current module namespace.
                continue;
            }

            // Module namespace does not fully match the current module
            // namespace. Assume that the symbol refers to a construct defined
            // under a parent namespace in some other module.
            return;
        }

        // Perform ordering based on the non-prefix portion of the symbol.
        order_identifier(
            orderer, symbol->elements[namespace_count]->identifier);
        return;
    }

    // Perform ordering based on the first element of the symbol.
    assert(!symbol->is_from_root);
    order_identifier(orderer, symbol->elements[0]->identifier);
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

    // Make sure that extend declarations come *after* all other declarations.
    for (size_t i = 1; i < decl_count; ++i) {
        struct cst_decl const* const prev = orderer->tldecls[i - 1].decl;
        struct cst_decl const* const decl = orderer->tldecls[i].decl;
        if (decl->kind != CST_DECL_EXTEND && prev->kind == CST_DECL_EXTEND) {
            fatal(
                prev->location,
                "extend declaration must appear after all module-level declarations");
        }
    }

    for (size_t i = 0; i < decl_count; ++i) {
        order_tldecl(orderer, &orderer->tldecls[i]);
    }

    assert(decl_count == sbuf_count(orderer->topological_order));
    for (size_t i = 0; i < decl_count; ++i) {
        struct cst_decl const* const decl = orderer->topological_order[i];
        sbuf_push(module->ordered, decl);
    }

    orderer_del(orderer);
}
