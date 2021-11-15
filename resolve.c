// Copyright 2021 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sunder.h"

struct incomplete_function {
    struct cst_decl const* decl;
    struct function* function;
    struct symbol_table* symbol_table;
};

struct resolver {
    struct module* module;
    // TODO: Currently current_symbol_name_prefix is unused, but it *should* be
    // used to produce the full name of symbols as they are constructed via the
    // symbol factory functions.
    char const* current_symbol_name_prefix; // Optional (NULL => no prefix).
    char const* current_static_addr_prefix; // Optional (NULL => no prefix).
    struct function* current_function; // NULL if not in a function.
    struct symbol_table* current_symbol_table;
    struct symbol_table* current_export_table;
    // Current offset of rbp for stack allocated data. Initialized to zero at
    // the start of function completion.
    int current_rbp_offset;
    // True if the statements being processed are inside a loop. Se to true when
    // a loop body is being resolved, and set to false once the loop body is
    // finished resolving.
    bool is_within_loop;

    // Functions to be completed at the end of the resolve phase after all
    // top-level declarations have been resolved. Incomplete functions defer
    // having their body's resolved so that mutually recursive functions (e.g. f
    // calls g and g calls f) have access to each others' symbols in the global
    // symbol table without requiring one function to be fully defined before
    // the other.
    autil_sbuf(struct incomplete_function) incomplete_functions;

    // List of symbol tables that need to be frozen after the module has been
    // fully resolved, used for namespaces that may have many symbols added to
    // them over the course of the resolve phase.
    autil_sbuf(struct symbol_table*) chilling_symbol_tables;
};
static struct resolver*
resolver_new(struct module* module);
static void
resolver_del(struct resolver* self);
// Returns true if resolution being performed in the global scope.
static bool
resolver_is_global(struct resolver const* self);
// Reserve static storage space for an object with the provided name.
static struct address const*
resolver_reserve_storage_static(struct resolver* self, char const* name);
// Reserve local storage space space for an object of the provided type.
static struct address const*
resolver_reserve_storage_local(struct resolver* self, struct type const* type);

// Produce the fully qualified name (e.g. prefix::name).
// Providing a NULL prefix parameter implies no prefix.
// Returns the qualified name as an interned string.
static char const* // interned
qualified_name(char const* prefix, char const* name);
// Produce the fully qualified address/elf-symbol (e.g. prefix.name).
// Providing a NULL prefix parameter implies no prefix.
// Returns the qualified address as an interned string.
static char const* // interned
qualified_addr(char const* prefix, char const* name);
// Normalize the provided name with the provided prefix.
// Providing a NULL prefix parameter implies no prefix.
// Providing a zero unique_id parameter implies the symbol is the first and
// potentially only symbol with the given name and should not have the unique
// identifier appended to the normalized symbol (matches gcc behavior for
// multiple local static symbols defined with the same name within the same
// function).
// Returns the normalized name as an interned string.
static char const* // interned
normalize(char const* prefix, char const* name, unsigned unique_id);
// Returns the normalization of the provided name within the provided prefix via
// the normalize function. Linearly increments unique IDs starting at zero until
// a unique ID is found that does not cause a name collision.
static char const* // interned
normalize_unique(char const* prefix, char const* name);
// Add the provided static symbol to the map of static symbols within the
// compilation context.
static void
register_static_symbol(struct symbol const* symbol);

static void
check_type_compatibility(
    struct source_location const* location,
    struct type const* actual,
    struct type const* expected);

// Returns a newly created and registered expression node of expr implicitly
// casted to type if such an implicit cast is valid. If expr cannot be
// implicitly casted to type then expr is returned unchanged.
//
// The attempted implicit cast is "shallow" in the sense that it will not
// recursively traverse the expression tree when casting, so currently immediate
// values (literals) are the only valid expr targets.
//
// This function is intended for use when casting untyped literals to an
// expression that would require a typed literal (e.g. integer->usize).
// Sub-expressions with integer literal constants are constant folded during the
// resolve phase, so the expression `123 + 456 * 2` *should* be folded to the
// integer literal constant `615` long before this function would be called on
// it, so for most cases the sequence:
// ```
// struct expr const* expr = resolve_expr(ice);
// expr = shallow_implicit_cast(type, expr);
// ```
// will correctly perform a tree-rewrite of the integer constance sub-expression
// `ice` casted to `type`.
static struct expr const*
shallow_implicit_cast(struct type const* type, struct expr const* expr);

static void
resolve_import(struct resolver* resolver, struct cst_import const* import);

static struct symbol const*
resolve_decl(struct resolver* resolver, struct cst_decl const* decl);
static struct symbol const*
resolve_decl_variable(
    struct resolver* resolver,
    struct cst_decl const* decl,
    struct expr const** /*optional*/ lhs,
    struct expr const** /*optional*/ rhs);
static struct symbol const*
resolve_decl_constant(struct resolver* resolver, struct cst_decl const* decl);
static struct symbol const*
resolve_decl_function(struct resolver* resolver, struct cst_decl const* decl);
static struct symbol const*
resolve_decl_extern_variable(
    struct resolver* resolver, struct cst_decl const* decl);

static void
complete_function(
    struct resolver* resolver, struct incomplete_function const* incomplete);

static struct stmt const* // optional
resolve_stmt(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const* // optional
resolve_stmt_decl(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_if(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_for_range(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_for_expr(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_break(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_continue(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_dump(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_return(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_assign(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_expr(struct resolver* resolver, struct cst_stmt const* stmt);

static struct expr const*
resolve_expr(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_identifier(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_qualified_identifier(
    struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_boolean(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_integer(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_bytes(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_array(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_slice(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_cast(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_syscall(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_call(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_access_index(
    struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_access_slice(
    struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_sizeof(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_unary(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_unary_logical(
    struct resolver* resolver,
    struct token const* op,
    enum uop_kind uop,
    struct expr const* rhs);
static struct expr const*
resolve_expr_unary_arithmetic(
    struct resolver* resolver,
    struct token const* op,
    enum uop_kind uop,
    struct expr const* rhs);
static struct expr const*
resolve_expr_unary_bitwise(
    struct resolver* resolver,
    struct token const* op,
    enum uop_kind uop,
    struct expr const* rhs);
static struct expr const*
resolve_expr_unary_dereference(
    struct resolver* resolver, struct token const* op, struct expr const* rhs);
static struct expr const*
resolve_expr_unary_addressof(
    struct resolver* resolver, struct token const* op, struct expr const* rhs);
static struct expr const*
resolve_expr_unary_countof(
    struct resolver* resolver, struct token const* op, struct expr const* rhs);
static struct expr const*
resolve_expr_binary(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_binary_logical(
    struct resolver* resolver,
    struct token const* op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs);
static struct expr const*
resolve_expr_binary_compare_equality(
    struct resolver* resolver,
    struct token const* op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs);
static struct expr const*
resolve_expr_binary_compare_order(
    struct resolver* resolver,
    struct token const* op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs);
static struct expr const*
resolve_expr_binary_arithmetic(
    struct resolver* resolver,
    struct token const* op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs);
static struct expr const*
resolve_expr_binary_bitwise(
    struct resolver* resolver,
    struct token const* op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs);

static struct block const*
resolve_block(
    struct resolver* resolver,
    struct symbol_table* symbol_table,
    struct cst_block const* block);

static struct type const*
resolve_typespec(
    struct resolver* resolver, struct cst_typespec const* typespec);

static struct resolver*
resolver_new(struct module* module)
{
    assert(module != NULL);

    struct resolver* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->module = module;
    self->current_static_addr_prefix = NULL;
    self->current_function = NULL;
    self->current_symbol_table = module->symbols;
    self->current_export_table = module->exports;
    self->current_rbp_offset = 0x0;
    self->is_within_loop = false;
    return self;
}

static void
resolver_del(struct resolver* self)
{
    assert(self != NULL);

    autil_sbuf_fini(self->incomplete_functions);

    memset(self, 0x00, sizeof(*self));
    autil_xalloc(self, AUTIL_XALLOC_FREE);
}

static bool
resolver_is_global(struct resolver const* self)
{
    assert(self != NULL);

    return self->current_function == NULL;
}

static struct address const*
resolver_reserve_storage_static(struct resolver* self, char const* name)
{
    assert(self != NULL);
    assert(name != NULL);

    char const* const name_normalized =
        normalize_unique(self->current_static_addr_prefix, name);
    struct address* const address =
        address_new(address_init_static(name_normalized, 0u));
    autil_freezer_register(context()->freezer, address);
    return address;
}

static struct address const*
resolver_reserve_storage_local(struct resolver* self, struct type const* type)
{
    assert(self != NULL);
    assert(self->current_function != NULL);
    assert(type != NULL);

    self->current_rbp_offset -= (int)ceil8zu(type->size);
    if (self->current_rbp_offset < self->current_function->local_stack_offset) {
        self->current_function->local_stack_offset = self->current_rbp_offset;
    }

    struct address* const address =
        address_new(address_init_local(self->current_rbp_offset));
    autil_freezer_register(context()->freezer, address);
    return address;
}

static char const*
qualified_name(char const* prefix, char const* name)
{
    assert(name != NULL);

    struct autil_string* const s = autil_string_new(NULL, 0);

    // <prefix>::
    if (prefix != NULL) {
        autil_string_append_fmt(s, "%s::", prefix);
    }
    // <prefix>::<name>
    autil_string_append_cstr(s, name);

    char const* const interned = autil_sipool_intern(
        context()->sipool, autil_string_start(s), autil_string_count(s));

    autil_string_del(s);
    return interned;
}

static char const*
qualified_addr(char const* prefix, char const* name)
{
    assert(name != NULL);

    struct autil_string* const s = autil_string_new(NULL, 0);

    // <prefix>.
    if (prefix != NULL) {
        autil_string_append_fmt(s, "%s.", prefix);
    }
    // <prefix>.<name>
    autil_string_append_cstr(s, name);

    char const* const interned = autil_sipool_intern(
        context()->sipool, autil_string_start(s), autil_string_count(s));

    autil_string_del(s);
    return interned;
}

static char const*
normalize(char const* prefix, char const* name, unsigned unique_id)
{
    assert(name != NULL);

    struct autil_string* const s = autil_string_new(NULL, 0);

    // <prefix>.
    if (prefix != NULL) {
        autil_string_append_fmt(s, "%s.", prefix);
    }
    // <prefix>.<name>
    autil_string_append_cstr(s, name);
    // <prefix>.name>.<unique-id>
    if (unique_id != 0) {
        autil_string_append_fmt(s, ".%u", unique_id);
    }

    char const* const interned = autil_sipool_intern(
        context()->sipool, autil_string_start(s), autil_string_count(s));

    autil_string_del(s);
    return interned;
}

static char const*
normalize_unique(char const* prefix, char const* name)
{
    assert(name != NULL);

    unsigned unique_id = 0u;
    char const* normalized = normalize(prefix, name, unique_id);
    while (autil_map_lookup(context()->static_symbols, &normalized) != NULL) {
        normalized = normalize(prefix, name, ++unique_id);
    }
    return normalized;
}

static void
register_static_symbol(struct symbol const* symbol)
{
    assert(symbol != NULL);
    assert(symbol->address != NULL);
    assert(symbol->address->kind == ADDRESS_STATIC);

    int const exists = autil_map_insert(
        context()->static_symbols,
        &symbol->address->data.static_.name,
        &symbol,
        NULL,
        NULL);
    if (exists) {
        fatal(
            symbol->location,
            "[ICE %s] normalized symbol name `%s` already exists",
            __func__,
            symbol->address->data.static_.name);
    }
}

static void
check_type_compatibility(
    struct source_location const* location,
    struct type const* actual,
    struct type const* expected)
{
    if (actual != expected) {
        fatal(
            location,
            "incompatible type `%s` (expected `%s`)",
            actual->name,
            expected->name);
    }
}

static struct expr const*
shallow_implicit_cast(struct type const* type, struct expr const* expr)
{
    assert(type != NULL);
    assert(expr != NULL);

    // FROM type TO type (same type).
    if (type->kind == expr->type->kind) {
        return expr;
    }

    // FROM untyped integer TO byte.
    if (type->kind == TYPE_BYTE && expr->type->kind == TYPE_UNSIZED_INTEGER) {
        struct autil_bigint const* const min = context()->u8_min;
        struct autil_bigint const* const max = context()->u8_max;

        if (autil_bigint_cmp(expr->data.integer, min) < 0) {
            fatal(
                expr->location,
                "out-of-range conversion from `%s` to `%s` (%s < %s)",
                expr->type->name,
                type->name,
                autil_bigint_to_new_cstr(expr->data.integer, NULL),
                autil_bigint_to_new_cstr(min, NULL));
        }
        if (autil_bigint_cmp(expr->data.integer, max) > 0) {
            fatal(
                expr->location,
                "out-of-range conversion from `%s` to `%s` (%s > %s)",
                expr->type->name,
                type->name,
                autil_bigint_to_new_cstr(expr->data.integer, NULL),
                autil_bigint_to_new_cstr(max, NULL));
        }

        struct expr* const result =
            expr_new_integer(expr->location, type, expr->data.integer);

        autil_freezer_register(context()->freezer, result);
        return result;
    }

    // FROM untyped integer TO typed integer.
    if (type_is_integer(type) && type->kind != TYPE_UNSIZED_INTEGER
        && expr->type->kind == TYPE_UNSIZED_INTEGER) {
        assert(type->data.integer.min != NULL);
        assert(type->data.integer.max != NULL);
        struct autil_bigint const* const min = type->data.integer.min;
        struct autil_bigint const* const max = type->data.integer.max;

        if (autil_bigint_cmp(expr->data.integer, min) < 0) {
            fatal(
                expr->location,
                "out-of-range conversion from `%s` to `%s` (%s < %s)",
                expr->type->name,
                type->name,
                autil_bigint_to_new_cstr(expr->data.integer, NULL),
                autil_bigint_to_new_cstr(min, NULL));
        }
        if (autil_bigint_cmp(expr->data.integer, max) > 0) {
            fatal(
                expr->location,
                "out-of-range conversion from `%s` to `%s` (%s > %s)",
                expr->type->name,
                type->name,
                autil_bigint_to_new_cstr(expr->data.integer, NULL),
                autil_bigint_to_new_cstr(max, NULL));
        }

        struct expr* const result =
            expr_new_integer(expr->location, type, expr->data.integer);

        autil_freezer_register(context()->freezer, result);
        return result;
    }

    // No implicit cast could be performed.
    return expr;
}

static void
merge_symbol_table(
    struct resolver* resolver,
    struct symbol_table* self,
    struct symbol_table const* othr)
{
    assert(resolver != NULL);
    assert(self != NULL);
    assert(othr != NULL);

    struct autil_vec const* const keys = autil_map_keys(othr->symbols);
    SYMBOL_MAP_KEY_TYPE const* iter = autil_vec_next_const(keys, NULL);
    for (; iter != NULL; iter = autil_vec_next_const(keys, iter)) {
        SYMBOL_MAP_VAL_TYPE const* const psymbol =
            autil_map_lookup_const(othr->symbols, iter);
        assert(psymbol != NULL);

        struct symbol const* const symbol = *psymbol;
        assert(symbol != NULL);

        if (symbol->kind == SYMBOL_NAMESPACE) {
            // Add all symbols from the namespace in the other symbol table to
            // the namespace in self's symbol table.
            struct symbol const* existing =
                symbol_table_lookup_local(self, *iter);
            if (existing == NULL) {
                // There is currently no symbol associated for the namespace in
                // self. Create a new namespace symbol for this purpose and
                // perform the merge.
                struct symbol_table* const table = symbol_table_new(self);
                autil_sbuf_push(resolver->chilling_symbol_tables, table);

                struct symbol* const namespace =
                    symbol_new_namespace(symbol->location, symbol->name, table);
                autil_freezer_register(context()->freezer, namespace);
                symbol_table_insert(self, *iter, namespace);
                existing = namespace;
            }

            if (existing->kind != SYMBOL_NAMESPACE) {
                // Actual name collision! Attempt to insert the symbol from the
                // other symbol table into self so that a redeclaration error is
                // generated.
                symbol_table_insert(self, *iter, symbol);
            }

            merge_symbol_table(resolver, existing->symbols, symbol->symbols);
            continue;
        }

        symbol_table_insert(self, *iter, symbol);
    }
}

// Returns the canonical representation of the provided import path or NULL.
char const* // interned
canonical_import_path(char const* module_path, char const* import_path)
{
    assert(module_path != NULL);
    assert(import_path != NULL);

    char const* result = NULL;

    // Path relative to the current module.
    char const* const module_dir = directory_path(module_path);
    struct autil_string* const tmp =
        autil_string_new_fmt("%s/%s", module_dir, import_path);
    if (file_exists(autil_string_start(tmp))) {
        result = canonical_path(autil_string_start(tmp));
        autil_string_del(tmp);
        return result;
    }

    // Path relative to environment-defined import path-list.
    char const* SUNDER_IMPORT_PATH = getenv("SUNDER_IMPORT_PATH");
    if (SUNDER_IMPORT_PATH == NULL) {
        autil_string_del(tmp);
        return NULL;
    }
    autil_string_resize(tmp, 0u);
    autil_string_append_cstr(tmp, SUNDER_IMPORT_PATH);
    struct autil_vec* const vec = autil_vec_of_string_new();
    autil_string_split_to_vec_on_cstr(tmp, ":", vec);
    for (size_t i = 0; i < autil_vec_count(vec); ++i) {
        struct autil_string* const* const ps = autil_vec_ref_const(vec, i);

        autil_string_resize(tmp, 0u);
        autil_string_append_fmt(
            tmp, "%s/%s", autil_string_start(*ps), import_path);

        if (!file_exists(autil_string_start(tmp))) {
            continue;
        }

        result = canonical_path(autil_string_start(tmp));
        break; // Found the module.
    }

    autil_string_del(tmp);
    autil_vec_of_string_del(vec);
    return result;
}

static void
resolve_import(struct resolver* resolver, struct cst_import const* import)
{
    assert(resolver != NULL);
    assert(import != NULL);

    char const* const path =
        canonical_import_path(resolver->module->path, import->path);
    if (path == NULL) {
        fatal(import->location, "failed to resolve import `%s`", import->path);
    }

    struct module const* module = lookup_module(path);
    if (module == NULL) {
        module = load_module(import->path, path);
    }
    if (!module->loaded) {
        fatal(
            import->location,
            "circular dependency when importing `%s`",
            import->path);
    }

    merge_symbol_table(resolver, resolver->module->symbols, module->exports);
}

static struct symbol const*
resolve_decl(struct resolver* resolver, struct cst_decl const* decl)
{
    assert(resolver != NULL);
    assert(decl != NULL);

    switch (decl->kind) {
    case CST_DECL_VARIABLE: {
        return resolve_decl_variable(resolver, decl, NULL, NULL);
    }
    case CST_DECL_CONSTANT: {
        return resolve_decl_constant(resolver, decl);
    }
    case CST_DECL_FUNCTION: {
        return resolve_decl_function(resolver, decl);
    }
    case CST_DECL_EXTERN_VARIABLE: {
        return resolve_decl_extern_variable(resolver, decl);
    }
    }

    UNREACHABLE();
}

static struct symbol const*
resolve_decl_variable(
    struct resolver* resolver,
    struct cst_decl const* decl,
    struct expr const** lhs,
    struct expr const** rhs)
{
    assert(resolver != NULL);
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_VARIABLE);

    struct expr const* expr = resolve_expr(resolver, decl->data.variable.expr);

    struct type const* const type =
        resolve_typespec(resolver, decl->data.variable.typespec);
    if (type->size == SIZEOF_UNSIZED) {
        fatal(
            decl->data.variable.typespec->location,
            "declaration of variable with unsized type `%s`",
            type->name);
    }

    expr = shallow_implicit_cast(type, expr);
    check_type_compatibility(expr->location, expr->type, type);

    // Global/static variables have their initial values computed at
    // compile-time, but local/non-static variables have their value
    // calculated/assigned at runtime when the value is placed on the stack.
    bool is_static = resolver_is_global(resolver);
    struct value* value = NULL;
    if (is_static) {
        value = eval_rvalue(expr);
        value_freeze(value, context()->freezer);
    }

    struct address const* const address = is_static
        ? resolver_reserve_storage_static(resolver, decl->name)
        : resolver_reserve_storage_local(resolver, type);

    struct symbol* const symbol =
        symbol_new_variable(decl->location, decl->name, type, address, value);
    autil_freezer_register(context()->freezer, symbol);

    symbol_table_insert(resolver->current_symbol_table, symbol->name, symbol);
    if (is_static) {
        register_static_symbol(symbol);
    }

    if (lhs != NULL) {
        struct expr* const identifier = expr_new_identifier(
            decl->data.variable.identifier->location, symbol);
        autil_freezer_register(context()->freezer, identifier);
        *lhs = identifier;
    }
    if (rhs != NULL) {
        *rhs = expr;
    }
    return symbol;
}

static struct symbol const*
resolve_decl_constant(struct resolver* resolver, struct cst_decl const* decl)
{
    assert(resolver != NULL);
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_CONSTANT);

    struct expr const* expr = resolve_expr(resolver, decl->data.constant.expr);

    struct type const* const type =
        resolve_typespec(resolver, decl->data.constant.typespec);
    if (type->size == SIZEOF_UNSIZED) {
        fatal(
            decl->data.constant.typespec->location,
            "declaration of constant with unsized type `%s`",
            type->name);
    }

    expr = shallow_implicit_cast(type, expr);
    check_type_compatibility(expr->location, expr->type, type);

    // Constants (globals and locals) have their values computed at compile-time
    // and therefore must always be added to the symbol table with an evaluated
    // value.
    struct value* const value = eval_rvalue(expr);
    value_freeze(value, context()->freezer);

    struct address const* const address =
        resolver_reserve_storage_static(resolver, decl->name);

    struct symbol* const symbol =
        symbol_new_constant(decl->location, decl->name, type, address, value);
    autil_freezer_register(context()->freezer, symbol);

    symbol_table_insert(resolver->current_symbol_table, symbol->name, symbol);
    register_static_symbol(symbol);
    return symbol;
}

static struct symbol const*
resolve_decl_function(struct resolver* resolver, struct cst_decl const* decl)
{
    assert(resolver != NULL);
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_FUNCTION);
    assert(resolver_is_global(resolver));

    autil_sbuf(struct cst_parameter const* const) const parameters =
        decl->data.function.parameters;

    // Create the type corresponding to the function.
    struct type const** parameter_types = NULL;
    autil_sbuf_resize(parameter_types, autil_sbuf_count(parameters));
    for (size_t i = 0; i < autil_sbuf_count(parameters); ++i) {
        parameter_types[i] =
            resolve_typespec(resolver, parameters[i]->typespec);
        if (parameter_types[i]->size == SIZEOF_UNSIZED) {
            fatal(
                parameters[i]->typespec->location,
                "declaration of function parameter with unsized type `%s`",
                parameter_types[i]->name);
        }
    }
    autil_sbuf_freeze(parameter_types, context()->freezer);

    struct type const* const return_type =
        resolve_typespec(resolver, decl->data.function.return_typespec);
    if (return_type->size == SIZEOF_UNSIZED) {
        fatal(
            decl->data.function.return_typespec->location,
            "declaration of function with unsized return type `%s`",
            return_type->name);
    }

    struct type const* function_type =
        type_unique_function(parameter_types, return_type);

    struct address const* const address =
        resolver_reserve_storage_static(resolver, decl->name);

    // Create a new incomplete function, a value that evaluates to that
    // function, and the address of that function/value.
    struct function* const function = function_new(
        decl->data.function.identifier->name, function_type, address);
    autil_freezer_register(context()->freezer, function);

    struct value* const value = value_new_function(function);
    value_freeze(value, context()->freezer);

    // Add the function/value to the symbol table now so that recursive
    // functions may reference themselves.
    struct symbol* const function_symbol = symbol_new_function(
        decl->location, decl->name, function_type, address, value);
    autil_freezer_register(context()->freezer, function_symbol);
    symbol_table_insert(
        resolver->current_symbol_table, function_symbol->name, function_symbol);
    register_static_symbol(function_symbol);

    // Executing a call instruction pushes the return address (0x8 bytes) onto
    // the stack. Inside the function the prelude saves the previous value of
    // rbp (0x8 bytes) by pushing it on the stack. So in total there are 0x8 +
    // 0x8 = 0x10 bytes between the current rbp (saved from the stack pointer)
    // and the region of the stack containing function parameters.
    // XXX: Currently the compiler assumes 0x8 byte stack alignment and does
    // *NOT* pad the stack to be 0x10 byte-aligned as required by some ABIs.
    int rbp_offset = 0x10; // Saved rbp + return address.
    // Resolve the function's parameters in order from lowest->highest on the
    // stack (i.e. right to left), adjusting the rbp_offset for each parameter
    // along the way.
    autil_sbuf(struct symbol const*) symbol_parameters = NULL;
    autil_sbuf_resize(symbol_parameters, autil_sbuf_count(parameters));
    for (size_t i = autil_sbuf_count(parameters); i--;) {
        struct source_location const* const location = parameters[i]->location;
        char const* const name = parameters[i]->identifier->name;
        struct type const* const type = parameter_types[i];
        struct address* const address =
            address_new(address_init_local(rbp_offset));
        autil_freezer_register(context()->freezer, address);

        rbp_offset += (int)ceil8zu(type->size);
        struct symbol* const symbol =
            symbol_new_variable(location, name, type, address, NULL);
        autil_freezer_register(context()->freezer, symbol);

        symbol_parameters[i] = symbol;
    }
    autil_sbuf_freeze(symbol_parameters, context()->freezer);
    function->symbol_parameters = symbol_parameters;

    // Add the function's parameters to its outermost symbol table in order
    // from left to right so that any error message about duplicate parameter
    // symbols will list the left-most symbol as the first of the two symbols
    // added to the table.
    struct symbol_table* const symbol_table =
        symbol_table_new(resolver->current_symbol_table);
    // The function references, but does not own, its outermost symbol table.
    function->symbol_table = symbol_table;
    for (size_t i = 0; i < autil_sbuf_count(parameters); ++i) {
        symbol_table_insert(
            symbol_table,
            function->symbol_parameters[i]->name,
            function->symbol_parameters[i]);
    }

    // Add the function's return value to it's outermost symbol table.
    struct address* const return_value_address =
        address_new(address_init_local(rbp_offset));
    autil_freezer_register(context()->freezer, return_value_address);
    struct symbol* const return_value_symbol = symbol_new_variable(
        decl->data.function.return_typespec->location,
        context()->interned.return_,
        return_type,
        return_value_address,
        NULL);
    autil_freezer_register(context()->freezer, return_value_symbol);
    symbol_table_insert(
        symbol_table, return_value_symbol->name, return_value_symbol);
    function->symbol_return = return_value_symbol;

    struct incomplete_function const incomplete = {
        decl,
        function,
        symbol_table,
    };
    autil_sbuf_push(resolver->incomplete_functions, incomplete);

    return function_symbol;
}

static struct symbol const*
resolve_decl_extern_variable(
    struct resolver* resolver, struct cst_decl const* decl)
{
    assert(resolver != NULL);
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_EXTERN_VARIABLE);
    assert(resolver_is_global(resolver));

    struct type const* const type =
        resolve_typespec(resolver, decl->data.variable.typespec);
    if (type->size == SIZEOF_UNSIZED) {
        fatal(
            decl->data.variable.typespec->location,
            "declaration of extern variable with unsized type `%s`",
            type->name);
    }

    struct address const* const address =
        resolver_reserve_storage_static(resolver, decl->name);

    struct symbol* const symbol =
        symbol_new_variable(decl->location, decl->name, type, address, NULL);
    symbol->is_extern = true;
    autil_freezer_register(context()->freezer, symbol);

    symbol_table_insert(resolver->current_symbol_table, symbol->name, symbol);
    register_static_symbol(symbol); // Extern variables are always static.

    return symbol;
}

static void
complete_function(
    struct resolver* resolver, struct incomplete_function const* incomplete)
{
    assert(resolver != NULL);
    assert(incomplete != NULL);

    // Complete the function.
    assert(resolver->current_function == NULL);
    assert(resolver->current_rbp_offset == 0x0);
    assert(!resolver->is_within_loop);
    char const* const save_symbol_name_prefix =
        resolver->current_symbol_name_prefix;
    char const* const save_static_addr_prefix =
        resolver->current_static_addr_prefix;
    resolver->current_symbol_name_prefix = NULL; // local identifiers
    resolver->current_static_addr_prefix = incomplete->function->name;
    resolver->current_function = incomplete->function;
    incomplete->function->body = resolve_block(
        resolver,
        incomplete->symbol_table,
        incomplete->decl->data.function.body);
    resolver->current_symbol_name_prefix = save_symbol_name_prefix;
    resolver->current_static_addr_prefix = save_static_addr_prefix;
    resolver->current_function = NULL;
    assert(resolver->current_rbp_offset == 0x0);

    // Freeze the symbol table now that the function has been completed and no
    // new symbols will be added.
    symbol_table_freeze(incomplete->symbol_table, context()->freezer);
}

static struct stmt const*
resolve_stmt(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);

    switch (stmt->kind) {
    case CST_STMT_DECL: {
        return resolve_stmt_decl(resolver, stmt);
    }
    case CST_STMT_IF: {
        return resolve_stmt_if(resolver, stmt);
    }
    case CST_STMT_FOR_RANGE: {
        return resolve_stmt_for_range(resolver, stmt);
    }
    case CST_STMT_FOR_EXPR: {
        return resolve_stmt_for_expr(resolver, stmt);
    }
    case CST_STMT_BREAK: {
        return resolve_stmt_break(resolver, stmt);
    }
    case CST_STMT_CONTINUE: {
        return resolve_stmt_continue(resolver, stmt);
    }
    case CST_STMT_DUMP: {
        return resolve_stmt_dump(resolver, stmt);
    }
    case CST_STMT_RETURN: {
        return resolve_stmt_return(resolver, stmt);
    }
    case CST_STMT_ASSIGN: {
        return resolve_stmt_assign(resolver, stmt);
    }
    case CST_STMT_EXPR: {
        return resolve_stmt_expr(resolver, stmt);
    }
    }

    UNREACHABLE();
    return NULL;
}

static struct stmt const*
resolve_stmt_decl(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_DECL);

    struct cst_decl const* const decl = stmt->data.decl;
    switch (decl->kind) {
    case CST_DECL_VARIABLE: {
        struct expr const* lhs = NULL;
        struct expr const* rhs = NULL;
        resolve_decl_variable(resolver, decl, &lhs, &rhs);
        struct stmt* const resolved = stmt_new_assign(stmt->location, lhs, rhs);

        autil_freezer_register(context()->freezer, resolved);
        return resolved;
    }
    case CST_DECL_CONSTANT: {
        resolve_decl_constant(resolver, decl);
        return NULL;
    }
    case CST_DECL_FUNCTION: {
        fatal(stmt->location, "nested function declaration");
        return NULL;
    }
    case CST_DECL_EXTERN_VARIABLE: {
        fatal(
            decl->location,
            "local declaration of extern variable `%s`",
            decl->name);
        return NULL;
    }
    }

    UNREACHABLE();
    return NULL;
}

static struct stmt const*
resolve_stmt_if(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_IF);

    autil_sbuf(struct cst_conditional const* const) const conditionals =
        stmt->data.if_.conditionals;
    autil_sbuf(struct conditional const*) resolved_conditionals = NULL;
    autil_sbuf_resize(resolved_conditionals, autil_sbuf_count(conditionals));
    for (size_t i = 0; i < autil_sbuf_count(conditionals); ++i) {
        assert(
            (conditionals[i]->condition != NULL)
            || (i == (autil_sbuf_count(conditionals) - 1)));

        struct expr const* condition = NULL;
        if (conditionals[i]->condition != NULL) {
            condition = resolve_expr(resolver, conditionals[i]->condition);
            if (condition->type->kind != TYPE_BOOL) {
                fatal(
                    condition->location,
                    "illegal condition with non-boolean type `%s`",
                    condition->type->name);
            }
        }

        struct symbol_table* const symbol_table =
            symbol_table_new(resolver->current_symbol_table);
        struct block const* const block =
            resolve_block(resolver, symbol_table, conditionals[i]->body);
        // Freeze the symbol table now that the block has been resolved and no
        // new symbols will be added.
        symbol_table_freeze(symbol_table, context()->freezer);

        struct conditional* const resolved_conditional =
            conditional_new(conditionals[i]->location, condition, block);
        autil_freezer_register(context()->freezer, resolved_conditional);
        resolved_conditionals[i] = resolved_conditional;
    }

    autil_sbuf_freeze(resolved_conditionals, context()->freezer);
    struct stmt* const resolved = stmt_new_if(resolved_conditionals);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_for_range(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_FOR_RANGE);

    struct expr const* begin =
        resolve_expr(resolver, stmt->data.for_range.begin);
    begin = shallow_implicit_cast(context()->builtin.usize, begin);
    if (begin->type != context()->builtin.usize) {
        fatal(
            begin->location,
            "illegal range-begin-expression with non-usize type `%s`",
            begin->type->name);
    }

    struct expr const* end = resolve_expr(resolver, stmt->data.for_range.end);
    end = shallow_implicit_cast(context()->builtin.usize, end);
    if (end->type != context()->builtin.usize) {
        fatal(
            end->location,
            "illegal range-end-expression with non-usize type `%s`",
            end->type->name);
    }

    int const save_rbp_offset = resolver->current_rbp_offset;
    struct source_location const* const loop_var_location =
        stmt->data.for_range.identifier->location;
    char const* const loop_var_name = stmt->data.for_range.identifier->name;
    struct type const* const loop_var_type = context()->builtin.usize;
    struct address const* const loop_var_address =
        resolver_reserve_storage_local(resolver, loop_var_type);
    struct symbol* const loop_var_symbol = symbol_new_variable(
        loop_var_location,
        loop_var_name,
        loop_var_type,
        loop_var_address,
        NULL);
    autil_freezer_register(context()->freezer, loop_var_symbol);

    struct symbol_table* const symbol_table =
        symbol_table_new(resolver->current_symbol_table);
    symbol_table_insert(symbol_table, loop_var_symbol->name, loop_var_symbol);

    bool const save_is_within_loop = resolver->is_within_loop;
    resolver->is_within_loop = true;
    struct block const* const body =
        resolve_block(resolver, symbol_table, stmt->data.for_range.body);
    resolver->current_rbp_offset = save_rbp_offset;
    resolver->is_within_loop = save_is_within_loop;

    // Freeze the symbol table now that the block has been resolved and no new
    // symbols will be added.
    symbol_table_freeze(symbol_table, context()->freezer);

    struct stmt* const resolved =
        stmt_new_for_range(stmt->location, loop_var_symbol, begin, end, body);
    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_for_expr(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_FOR_EXPR);

    struct expr const* const expr =
        resolve_expr(resolver, stmt->data.for_expr.expr);
    if (expr->type->kind != TYPE_BOOL) {
        fatal(
            expr->location,
            "illegal condition with non-boolean type `%s`",
            expr->type->name);
    }

    struct symbol_table* const symbol_table =
        symbol_table_new(resolver->current_symbol_table);

    bool const save_is_within_loop = resolver->is_within_loop;
    resolver->is_within_loop = true;
    struct block const* const body =
        resolve_block(resolver, symbol_table, stmt->data.for_expr.body);
    resolver->is_within_loop = save_is_within_loop;

    // Freeze the symbol table now that the block has been resolved and no new
    // symbols will be added.
    symbol_table_freeze(symbol_table, context()->freezer);

    struct stmt* const resolved = stmt_new_for_expr(stmt->location, expr, body);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_break(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_BREAK);

    if (!resolver->is_within_loop) {
        fatal(stmt->location, "break statement outside of loop");
    }

    struct stmt* const resolved = stmt_new_break(stmt->location);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_continue(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_CONTINUE);

    if (!resolver->is_within_loop) {
        fatal(stmt->location, "continue statement outside of loop");
    }

    struct stmt* const resolved = stmt_new_continue(stmt->location);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_dump(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_DUMP);

    struct expr const* expr = resolve_expr(resolver, stmt->data.dump.expr);
    if (expr->type->size == SIZEOF_UNSIZED) {
        fatal(
            stmt->location, "type `%s` has no defined size", expr->type->name);
    }

    struct stmt* const resolved = stmt_new_dump(stmt->location, expr);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_return(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_RETURN);

    struct type const* const return_type =
        resolver->current_function->type->data.function.return_type;
    struct expr const* expr = NULL;
    if (stmt->data.return_.expr != NULL) {
        expr = resolve_expr(resolver, stmt->data.return_.expr);
        expr = shallow_implicit_cast(return_type, expr);
        check_type_compatibility(expr->location, expr->type, return_type);
    }
    else {
        if (context()->builtin.void_ != return_type) {
            fatal(
                stmt->location,
                "illegal return statement in function with non-void return type");
        }
    }

    struct stmt* const resolved = stmt_new_return(stmt->location, expr);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_assign(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_ASSIGN);

    struct expr const* const lhs =
        resolve_expr(resolver, stmt->data.assign.lhs);
    struct expr const* rhs = resolve_expr(resolver, stmt->data.assign.rhs);

    // TODO: Rather than query if lhs is an lvalue, perhaps there could function
    // `validate_expr_is_lvalue` in resolve.c which traverses the expression
    // tree and emits an error with more context about *why* a specific
    // expression is not an lvalue. Currently it's up to the user to figure out
    // *why* lhs is not an lvalue, and better information could ease debugging.
    if (!expr_is_lvalue(lhs)) {
        fatal(
            lhs->location,
            "left hand side of assignment statement is not an lvalue");
    }

    rhs = shallow_implicit_cast(lhs->type, rhs);
    check_type_compatibility(stmt->location, rhs->type, lhs->type);

    struct stmt* const resolved = stmt_new_assign(stmt->location, lhs, rhs);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_expr(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_EXPR);

    struct expr const* const expr = resolve_expr(resolver, stmt->data.expr);

    if (expr->type->size == SIZEOF_UNSIZED) {
        fatal(
            expr->location,
            "statement-expression produces result of unsized type `%s`",
            expr->type->name);
    }
    struct stmt* const resolved = stmt_new_expr(stmt->location, expr);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);

    switch (expr->kind) {
    case CST_EXPR_IDENTIFIER: {
        return resolve_expr_identifier(resolver, expr);
    }
    case CST_EXPR_QUALIFIED_IDENTIFIER: {
        return resolve_expr_qualified_identifier(resolver, expr);
    }
    case CST_EXPR_BOOLEAN: {
        return resolve_expr_boolean(resolver, expr);
    }
    case CST_EXPR_INTEGER: {
        return resolve_expr_integer(resolver, expr);
    }
    case CST_EXPR_BYTES: {
        return resolve_expr_bytes(resolver, expr);
    }
    case CST_EXPR_ARRAY: {
        return resolve_expr_array(resolver, expr);
    }
    case CST_EXPR_SLICE: {
        return resolve_expr_slice(resolver, expr);
    }
    case CST_EXPR_CAST: {
        return resolve_expr_cast(resolver, expr);
    }
    case CST_EXPR_GROUPED: {
        return resolve_expr(resolver, expr->data.grouped.expr);
    }
    case CST_EXPR_SYSCALL: {
        return resolve_expr_syscall(resolver, expr);
    }
    case CST_EXPR_CALL: {
        return resolve_expr_call(resolver, expr);
    }
    case CST_EXPR_ACCESS_INDEX: {
        return resolve_expr_access_index(resolver, expr);
    }
    case CST_EXPR_ACCESS_SLICE: {
        return resolve_expr_access_slice(resolver, expr);
    }
    case CST_EXPR_SIZEOF: {
        return resolve_expr_sizeof(resolver, expr);
    }
    case CST_EXPR_UNARY: {
        return resolve_expr_unary(resolver, expr);
    }
    case CST_EXPR_BINARY: {
        return resolve_expr_binary(resolver, expr);
    }
    }

    UNREACHABLE();
    return NULL;
}

static struct expr const*
resolve_expr_identifier(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_IDENTIFIER);

    char const* const name = expr->data.identifier->name;
    struct symbol const* const symbol =
        symbol_table_lookup(resolver->current_symbol_table, name);
    if (symbol == NULL) {
        fatal(expr->location, "use of undeclared identifier `%s`", name);
    }

    switch (symbol->kind) {
    case SYMBOL_TYPE: {
        fatal(
            expr->location, "use of type `%s` as an expression", symbol->name);
    }
    case SYMBOL_NAMESPACE: {
        fatal(
            expr->location,
            "use of namespace `%s` as an expression",
            symbol->name);
    }
    case SYMBOL_VARIABLE: /* fallthrough */
    case SYMBOL_CONSTANT: /* fallthrough */
    case SYMBOL_FUNCTION: {
        // Variables, constants, and functions may be used in an identifier
        // expression.
        break;
    }
    }

    struct expr* const resolved = expr_new_identifier(expr->location, symbol);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_qualified_identifier(
    struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_QUALIFIED_IDENTIFIER);

    autil_sbuf(struct cst_identifier const* const) const identifiers =
        expr->data.qualified_identifier.identifiers;
    // lhs::name
    // ^---+---^
    //     ^
    //     symbol
    char const* name = identifiers[0]->name;
    struct symbol const* lhs =
        symbol_table_lookup(resolver->current_symbol_table, name);
    if (lhs == NULL) {
        fatal(expr->location, "use of undeclared identifier `%s`", name);
    }

    struct symbol const* symbol = NULL;
    for (size_t i = 1; i < autil_sbuf_count(identifiers); ++i) {
        if (lhs->kind != SYMBOL_NAMESPACE) {
            fatal(expr->location, "`%s` is not a namespace", lhs->name);
        }

        name = identifiers[i]->name;
        symbol = symbol_table_lookup(lhs->symbols, name);
        if (symbol == NULL) {
            fatal(
                expr->location,
                "use of undeclared identifier `%s` within `%s`",
                name,
                lhs->name);
        }

        lhs = symbol;
    }

    switch (symbol->kind) {
    case SYMBOL_TYPE: {
        fatal(
            expr->location, "use of type `%s` as an expression", symbol->name);
    }
    case SYMBOL_NAMESPACE: {
        fatal(
            expr->location,
            "use of namespace `%s` as an expression",
            symbol->name);
    }
    case SYMBOL_VARIABLE: /* fallthrough */
    case SYMBOL_CONSTANT: /* fallthrough */
    case SYMBOL_FUNCTION: {
        // Variables, constants, and functions may be used in an identifier
        // expression.
        break;
    }
    }

    struct expr* const resolved = expr_new_identifier(expr->location, symbol);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_boolean(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_BOOLEAN);
    (void)resolver;

    bool const value = expr->data.boolean->value;
    struct expr* const resolved = expr_new_boolean(expr->location, value);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct type const*
integer_literal_suffix_to_type(
    struct source_location const* location, char const* suffix)
{
    assert(suffix != NULL);

    if (suffix == context()->interned.empty) {
        return context()->builtin.integer;
    }
    if (suffix == context()->interned.y) {
        return context()->builtin.byte;
    }
    if (suffix == context()->interned.u8) {
        return context()->builtin.u8;
    }
    if (suffix == context()->interned.s8) {
        return context()->builtin.s8;
    }
    if (suffix == context()->interned.u16) {
        return context()->builtin.u16;
    }
    if (suffix == context()->interned.s16) {
        return context()->builtin.s16;
    }
    if (suffix == context()->interned.u32) {
        return context()->builtin.u32;
    }
    if (suffix == context()->interned.s32) {
        return context()->builtin.s32;
    }
    if (suffix == context()->interned.u64) {
        return context()->builtin.u64;
    }
    if (suffix == context()->interned.s64) {
        return context()->builtin.s64;
    }
    if (suffix == context()->interned.u) {
        return context()->builtin.usize;
    }
    if (suffix == context()->interned.s) {
        return context()->builtin.ssize;
    }

    fatal(location, "unknown integer literal suffix `%s`", suffix);
    return NULL;
}

static struct expr const*
resolve_expr_integer(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_INTEGER);
    (void)resolver;

    struct cst_integer const* const cst_integer = expr->data.integer;
    struct autil_bigint const* const value = cst_integer->value;
    struct type const* const type = integer_literal_suffix_to_type(
        cst_integer->location, cst_integer->suffix);

    struct expr* const resolved = expr_new_integer(expr->location, type, value);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_bytes(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_BYTES);
    (void)resolver;

    struct address const* const address =
        resolver_reserve_storage_static(resolver, "__bytes");

    size_t const count = autil_string_count(expr->data.bytes);
    struct type const* const type =
        type_unique_array(count, context()->builtin.byte);
    // TODO: Allocating a value for each and every byte in the bytes literal
    // feels wasteful. It may be worth investigating some specific ascii or
    // asciiz static object that would use the expr's autil_string directly and
    // then generate a readable string in the output assembly during the codegen
    // phase.
    autil_sbuf(struct value*) elements = NULL;
    for (size_t i = 0; i < count; ++i) {
        uint8_t const byte =
            (uint8_t)*autil_string_ref_const(expr->data.bytes, i);
        autil_sbuf_push(elements, value_new_byte(byte));
    }
    struct value* const value = value_new_array(type, elements, NULL);
    value_freeze(value, context()->freezer);

    struct symbol* const symbol = symbol_new_constant(
        expr->location, address->data.static_.name, type, address, value);
    autil_freezer_register(context()->freezer, symbol);
    register_static_symbol(symbol);

    struct expr* const resolved =
        expr_new_bytes(expr->location, address, count);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_array(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_ARRAY);

    struct type const* const type =
        resolve_typespec(resolver, expr->data.array.typespec);
    if (type->kind != TYPE_ARRAY) {
        fatal(
            expr->data.array.typespec->location,
            "expected array type (received `%s`)",
            type->name);
    }

    autil_sbuf(struct cst_expr const* const) elements =
        expr->data.array.elements;
    autil_sbuf(struct expr const*) resolved_elements = NULL;
    for (size_t i = 0; i < autil_sbuf_count(elements); ++i) {
        struct expr const* resolved_element =
            resolve_expr(resolver, elements[i]);
        resolved_element =
            shallow_implicit_cast(type->data.array.base, resolved_element);
        check_type_compatibility(
            resolved_element->location,
            resolved_element->type,
            type->data.array.base);
        autil_sbuf_push(resolved_elements, resolved_element);
    }
    autil_sbuf_freeze(resolved_elements, context()->freezer);

    struct expr const* resolved_ellipsis = NULL;
    if (expr->data.array.ellipsis != NULL) {
        resolved_ellipsis = resolve_expr(resolver, expr->data.array.ellipsis);
        resolved_ellipsis =
            shallow_implicit_cast(type->data.array.base, resolved_ellipsis);
        check_type_compatibility(
            resolved_ellipsis->location,
            resolved_ellipsis->type,
            type->data.array.base);
    }

    if ((type->data.array.count != autil_sbuf_count(resolved_elements))
        && resolved_ellipsis == NULL) {
        fatal(
            expr->location,
            "array of type `%s` created with %zu elements (expected %zu)",
            type->name,
            autil_sbuf_count(resolved_elements),
            type->data.array.count);
    }

    struct expr* const resolved = expr_new_array(
        expr->location, type, resolved_elements, resolved_ellipsis);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_slice(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_SLICE);

    struct type const* const type =
        resolve_typespec(resolver, expr->data.slice.typespec);
    if (type->kind != TYPE_SLICE) {
        fatal(
            expr->data.slice.typespec->location,
            "expected slice type (received `%s`)",
            type->name);
    }

    struct expr const* const pointer =
        resolve_expr(resolver, expr->data.slice.pointer);
    if (pointer->type->kind != TYPE_POINTER) {
        fatal(
            pointer->location,
            "expression of type `%s` is not a pointer",
            pointer->type->name);
    }
    struct type const* const slice_pointer_type =
        type_unique_pointer(type->data.slice.base);
    check_type_compatibility(
        pointer->location, pointer->type, slice_pointer_type);

    struct expr const* count = resolve_expr(resolver, expr->data.slice.count);
    count = shallow_implicit_cast(context()->builtin.usize, count);
    check_type_compatibility(
        count->location, count->type, context()->builtin.usize);

    struct expr* const resolved =
        expr_new_slice(expr->location, type, pointer, count);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_cast(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_CAST);

    struct type const* const type =
        resolve_typespec(resolver, expr->data.cast.typespec);
    struct expr const* const rhs = resolve_expr(resolver, expr->data.cast.expr);

    // TODO: Casts to and from unsized integers are not permitted because it is
    // unclear how we should handle modulo operations when a casted-from value
    // is narrowed by the cast. Investigate what the reasonable behavior should
    // be in this situation before the operation is solidified in the language
    // for this and other (future?) unsized types.
    if (rhs->type->size == SIZEOF_UNSIZED) {
        fatal(
            rhs->location,
            "invalid cast from unsized type `%s` to `%s`",
            rhs->type->name,
            type->name);
    }
    if (type->size == SIZEOF_UNSIZED) {
        fatal(
            rhs->location,
            "invalid cast to unsized type `%s` from `%s`",
            type->name,
            rhs->type->name);
    }

    bool const valid = (type_is_integer(type) && type_is_integer(rhs->type))
        || (type->kind == TYPE_BOOL && rhs->type->kind == TYPE_BYTE)
        || (type->kind == TYPE_BYTE && rhs->type->kind == TYPE_BOOL)
        || (type->kind == TYPE_BOOL && type_is_integer(rhs->type))
        || (type_is_integer(type) && rhs->type->kind == TYPE_BOOL)
        || (type->kind == TYPE_BYTE && type_is_integer(rhs->type))
        || (type_is_integer(type) && rhs->type->kind == TYPE_BYTE)
        || (type->kind == TYPE_POINTER && rhs->type->kind == TYPE_USIZE)
        || (type->kind == TYPE_USIZE && rhs->type->kind == TYPE_POINTER)
        || (type->kind == TYPE_POINTER && rhs->type->kind == TYPE_POINTER);
    if (!valid) {
        fatal(
            rhs->location,
            "invalid cast from `%s` to `%s`",
            rhs->type->name,
            type->name);
    }

    struct expr* const resolved = expr_new_cast(expr->location, type, rhs);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_syscall(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_SYSCALL);

    autil_sbuf(struct cst_expr const* const) arguments =
        expr->data.syscall.arguments;
    size_t const arguments_count = autil_sbuf_count(arguments);

    // Sanity-check assert. The parser should have reported a fatal error if
    // fewer than SYSCALL_ARGUMENTS_MIN were provided.
    assert(arguments_count >= SYSCALL_ARGUMENTS_MIN);

    if (arguments_count > SYSCALL_ARGUMENTS_MAX) {
        fatal(
            expr->location,
            "%zu syscall arguments provided (maximum %zu allowed)",
            arguments_count,
            SYSCALL_ARGUMENTS_MAX);
    }

    autil_sbuf(struct expr const*) exprs = NULL;
    for (size_t i = 0; i < autil_sbuf_count(arguments); ++i) {
        struct expr const* const arg = resolve_expr(resolver, arguments[i]);
        if (arg->type->size == SIZEOF_UNSIZED) {
            fatal(
                arg->location,
                "unsized type `%s` in syscall expression",
                arg->type->name);
        }

        bool const valid_type =
            type_is_integer(arg->type) || arg->type->kind == TYPE_POINTER;
        if (!valid_type) {
            fatal(
                arg->location,
                "expected integer or pointer type (received `%s`)",
                arg->type->name);
        }
        autil_sbuf_push(exprs, arg);
    }
    autil_sbuf_freeze(exprs, context()->freezer);
    struct expr* const resolved = expr_new_syscall(expr->location, exprs);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_call(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_CALL);

    struct expr const* const function =
        resolve_expr(resolver, expr->data.call.func);
    if (function->type->kind != TYPE_FUNCTION) {
        fatal(
            expr->location,
            "non-callable type `%s` used in function call expression",
            function->type->name);
    }

    if (autil_sbuf_count(expr->data.call.arguments)
        != autil_sbuf_count(function->type->data.function.parameter_types)) {
        fatal(
            expr->location,
            "function with type `%s` expects %zu argument(s) (%zu provided)",
            function->type->name,
            autil_sbuf_count(function->type->data.function.parameter_types),
            autil_sbuf_count(expr->data.call.arguments));
    }

    autil_sbuf(struct expr const*) arguments = NULL;
    for (size_t i = 0; i < autil_sbuf_count(expr->data.call.arguments); ++i) {
        struct expr const* arg =
            resolve_expr(resolver, expr->data.call.arguments[i]);
        autil_sbuf_push(arguments, arg);
    }
    autil_sbuf_freeze(arguments, context()->freezer);

    // Type-check function arguments.
    autil_sbuf(struct type const* const) const parameter_types =
        function->type->data.function.parameter_types;
    for (size_t i = 0; i < autil_sbuf_count(arguments); ++i) {
        arguments[i] = shallow_implicit_cast(parameter_types[i], arguments[i]);
    }
    for (size_t i = 0; i < autil_sbuf_count(parameter_types); ++i) {
        struct type const* const expected = parameter_types[i];
        struct type const* const received = arguments[i]->type;
        if (received != expected) {
            fatal(
                arguments[i]->location,
                "incompatible argument type `%s` (expected `%s`)",
                received->name,
                expected->name);
        }
    }

    struct expr* const resolved =
        expr_new_call(expr->location, function, arguments);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_access_index(
    struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_ACCESS_INDEX);

    struct expr const* const lhs =
        resolve_expr(resolver, expr->data.access_index.lhs);
    if (lhs->type->kind != TYPE_ARRAY && lhs->type->kind != TYPE_SLICE) {
        fatal(
            lhs->location,
            "illegal index operation with left-hand-side of type `%s`",
            lhs->type->name);
    }

    struct expr const* idx =
        resolve_expr(resolver, expr->data.access_index.idx);
    idx = shallow_implicit_cast(context()->builtin.usize, idx);
    if (idx->type->kind != TYPE_USIZE) {
        fatal(
            idx->location,
            "illegal index operation with index of non-usize type `%s`",
            idx->type->name);
    }

    struct expr* const resolved =
        expr_new_access_index(expr->location, lhs, idx);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_access_slice(
    struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_ACCESS_SLICE);

    struct expr const* const lhs =
        resolve_expr(resolver, expr->data.access_slice.lhs);
    if (lhs->type->kind != TYPE_ARRAY && lhs->type->kind != TYPE_SLICE) {
        fatal(
            lhs->location,
            "illegal slice operation with left-hand-side of type `%s`",
            lhs->type->name);
    }
    if (lhs->type->kind == TYPE_ARRAY && !expr_is_lvalue(lhs)) {
        fatal(
            lhs->location,
            "left hand side of slice operation is an rvalue array");
    }

    struct expr const* begin =
        resolve_expr(resolver, expr->data.access_slice.begin);
    begin = shallow_implicit_cast(context()->builtin.usize, begin);
    if (begin->type->kind != TYPE_USIZE) {
        fatal(
            begin->location,
            "illegal slice operation with index of non-usize type `%s`",
            begin->type->name);
    }

    struct expr const* end =
        resolve_expr(resolver, expr->data.access_slice.end);
    end = shallow_implicit_cast(context()->builtin.usize, end);
    if (end->type->kind != TYPE_USIZE) {
        fatal(
            end->location,
            "illegal slice operation with index of non-usize type `%s`",
            end->type->name);
    }

    struct expr* const resolved =
        expr_new_access_slice(expr->location, lhs, begin, end);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_sizeof(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);

    struct type const* const rhs =
        resolve_typespec(resolver, expr->data.sizeof_.rhs);
    if (rhs->size == SIZEOF_UNSIZED) {
        fatal(expr->location, "type `%s` has no defined size", rhs->name);
    }

    struct expr* const resolved = expr_new_sizeof(expr->location, rhs);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_unary(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_UNARY);

    // While a human would identify the integer expression -128s8 as the hex
    // byte 0x80, the parser identifies the integer expression -128s8 as the
    // unary negation (via the unary - operator) of the integer literal 128s8.
    // Positive 128 is an out-of-range value for an integer of type s8 (the max
    // being  positive 127) even though the intended value of -128 *is* within
    // the range of an s8. Here we identify the special case where a + or -
    // token is immediately followed by an integer token and combine the two
    // into a single integer expression.
    struct token const* const op = expr->data.unary.op;
    bool const is_sign = (op->kind == TOKEN_PLUS) || (op->kind == TOKEN_DASH);
    struct cst_expr const* const cst_rhs = expr->data.unary.rhs;
    if (is_sign && cst_rhs->kind == CST_EXPR_INTEGER) {
        struct cst_integer const* const cst_integer = cst_rhs->data.integer;
        struct autil_bigint const* value = cst_integer->value;
        if (op->kind == TOKEN_DASH) {
            struct autil_bigint* const tmp = autil_bigint_new(value);
            autil_bigint_neg(tmp, value);
            autil_bigint_freeze(tmp, context()->freezer);
            value = tmp;
        }
        struct type const* const type = integer_literal_suffix_to_type(
            cst_integer->location, cst_integer->suffix);

        struct expr* const resolved =
            expr_new_integer(&op->location, type, value);

        autil_freezer_register(context()->freezer, resolved);
        return resolved;
    }

    struct expr const* const rhs = resolve_expr(resolver, cst_rhs);
    switch (op->kind) {
    case TOKEN_NOT: {
        return resolve_expr_unary_logical(resolver, op, UOP_NOT, rhs);
    }
    case TOKEN_COUNTOF: {
        return resolve_expr_unary_countof(resolver, op, rhs);
    }
    case TOKEN_PLUS: {
        return resolve_expr_unary_arithmetic(resolver, op, UOP_POS, rhs);
    }
    case TOKEN_DASH: {
        if (type_is_uinteger(rhs->type)) {
            fatal(
                &op->location,
                "invalid argument of type `%s` in unary `%s` expression",
                rhs->type->name,
                token_kind_to_cstr(op->kind));
        }
        return resolve_expr_unary_arithmetic(resolver, op, UOP_NEG, rhs);
    }
    case TOKEN_TILDE: {
        return resolve_expr_unary_bitwise(resolver, op, UOP_BITNOT, rhs);
    }
    case TOKEN_STAR: {
        return resolve_expr_unary_dereference(resolver, op, rhs);
    }
    case TOKEN_AMPERSAND: {
        return resolve_expr_unary_addressof(resolver, op, rhs);
    }
    default: {
        UNREACHABLE();
    }
    }

    UNREACHABLE();
    return NULL;
}

static struct expr const*
resolve_expr_unary_logical(
    struct resolver* resolver,
    struct token const* op,
    enum uop_kind uop,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op != NULL);
    assert(rhs != NULL);
    (void)resolver;

    if (rhs->type->kind != TYPE_BOOL) {
        fatal(
            &op->location,
            "invalid argument of type `%s` in unary `%s` expression",
            rhs->type->name,
            token_kind_to_cstr(op->kind));
    }

    struct expr* const resolved =
        expr_new_unary(&op->location, rhs->type, uop, rhs);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_unary_arithmetic(
    struct resolver* resolver,
    struct token const* op,
    enum uop_kind uop,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op != NULL);
    assert(rhs != NULL);
    (void)resolver;

    if (!type_is_integer(rhs->type)) {
        fatal(
            &op->location,
            "invalid argument of type `%s` in unary `%s` expression",
            rhs->type->name,
            token_kind_to_cstr(op->kind));
    }

    struct expr* const resolved =
        expr_new_unary(&op->location, rhs->type, uop, rhs);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_unary_bitwise(
    struct resolver* resolver,
    struct token const* op,
    enum uop_kind uop,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op != NULL);
    assert(rhs != NULL);
    (void)resolver;

    if (!(rhs->type->kind == TYPE_BYTE || type_is_integer(rhs->type))) {
        fatal(
            rhs->location,
            "cannot apply bitwise NOT to type `%s`",
            rhs->type->name);
    }
    struct expr* const resolved =
        expr_new_unary(&op->location, rhs->type, uop, rhs);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_unary_dereference(
    struct resolver* resolver, struct token const* op, struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op != NULL);
    assert(op->kind == TOKEN_STAR);
    assert(rhs != NULL);
    (void)resolver;

    if (rhs->type->kind != TYPE_POINTER) {
        fatal(
            rhs->location,
            "cannot dereference non-pointer type `%s`",
            rhs->type->name);
    }
    struct expr* const resolved = expr_new_unary(
        &op->location, rhs->type->data.pointer.base, UOP_DEREFERENCE, rhs);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_unary_addressof(
    struct resolver* resolver, struct token const* op, struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op != NULL);
    assert(op->kind == TOKEN_AMPERSAND);
    assert(rhs != NULL);
    (void)resolver;

    if (!expr_is_lvalue(rhs)) {
        fatal(rhs->location, "cannot take the address of a non-lvalue");
    }

    struct expr* const resolved = expr_new_unary(
        &op->location, type_unique_pointer(rhs->type), UOP_ADDRESSOF, rhs);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_unary_countof(
    struct resolver* resolver, struct token const* op, struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op != NULL);
    assert(op->kind == TOKEN_COUNTOF);
    assert(rhs != NULL);
    (void)resolver;

    if (rhs->type->kind != TYPE_ARRAY && rhs->type->kind != TYPE_SLICE) {
        fatal(
            rhs->location,
            "expected array or slice type (received `%s`)",
            rhs->type->name);
    }

    struct expr* const resolved = expr_new_unary(
        &op->location, context()->builtin.usize, UOP_COUNTOF, rhs);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_binary(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_BINARY);

    struct expr const* const lhs =
        resolve_expr(resolver, expr->data.binary.lhs);
    struct expr const* const rhs =
        resolve_expr(resolver, expr->data.binary.rhs);
    struct token const* const op = expr->data.binary.op;
    switch (op->kind) {
    case TOKEN_OR: {
        return resolve_expr_binary_logical(resolver, op, BOP_OR, lhs, rhs);
    }
    case TOKEN_AND: {
        return resolve_expr_binary_logical(resolver, op, BOP_AND, lhs, rhs);
    }
    case TOKEN_EQ: {
        return resolve_expr_binary_compare_equality(
            resolver, op, BOP_EQ, lhs, rhs);
    }
    case TOKEN_NE: {
        return resolve_expr_binary_compare_equality(
            resolver, op, BOP_NE, lhs, rhs);
    }
    case TOKEN_LE: {
        return resolve_expr_binary_compare_order(
            resolver, op, BOP_LE, lhs, rhs);
    }
    case TOKEN_LT: {
        return resolve_expr_binary_compare_order(
            resolver, op, BOP_LT, lhs, rhs);
    }
    case TOKEN_GE: {
        return resolve_expr_binary_compare_order(
            resolver, op, BOP_GE, lhs, rhs);
    }
    case TOKEN_GT: {
        return resolve_expr_binary_compare_order(
            resolver, op, BOP_GT, lhs, rhs);
    }
    case TOKEN_PLUS: {
        return resolve_expr_binary_arithmetic(resolver, op, BOP_ADD, lhs, rhs);
    }
    case TOKEN_DASH: {
        return resolve_expr_binary_arithmetic(resolver, op, BOP_SUB, lhs, rhs);
    }
    case TOKEN_STAR: {
        return resolve_expr_binary_arithmetic(resolver, op, BOP_MUL, lhs, rhs);
    }
    case TOKEN_FSLASH: {
        return resolve_expr_binary_arithmetic(resolver, op, BOP_DIV, lhs, rhs);
    }
    case TOKEN_PIPE: {
        return resolve_expr_binary_bitwise(resolver, op, BOP_BITOR, lhs, rhs);
    }
    case TOKEN_CARET: {
        return resolve_expr_binary_bitwise(resolver, op, BOP_BITXOR, lhs, rhs);
    }
    case TOKEN_AMPERSAND: {
        return resolve_expr_binary_bitwise(resolver, op, BOP_BITAND, lhs, rhs);
    }
    default: {
        UNREACHABLE();
    }
    }

    UNREACHABLE();
    return NULL;
}

static struct expr const*
resolve_expr_binary_logical(
    struct resolver* resolver,
    struct token const* op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    (void)resolver;

    bool const valid = lhs->type == rhs->type && lhs->type->kind == TYPE_BOOL
        && rhs->type->kind == TYPE_BOOL;
    if (!valid) {
        fatal(
            &op->location,
            "invalid arguments of types `%s` and `%s` in binary `%s` expression",
            lhs->type->name,
            rhs->type->name,
            token_kind_to_cstr(op->kind));
    }

    struct type const* const type = context()->builtin.bool_;
    struct expr* const resolved =
        expr_new_binary(&op->location, type, bop, lhs, rhs);

    autil_freezer_register(context()->freezer, resolved);
    return resolved;
}

static struct expr const*
resolve_expr_binary_compare_equality(
    struct resolver* resolver,
    struct token const* op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    (void)resolver;

    lhs = shallow_implicit_cast(rhs->type, lhs);
    rhs = shallow_implicit_cast(lhs->type, rhs);

    if (lhs->type != rhs->type) {
        fatal(
            &op->location,
            "invalid arguments of types `%s` and `%s` in binary `%s` expression",
            lhs->type->name,
            rhs->type->name,
            token_kind_to_cstr(op->kind));
    }
    struct type const* const xhs_type = lhs->type;
    if (!type_can_compare_equality(xhs_type)) {
        fatal(
            &op->location,
            "invalid arguments of type `%s` in binary `%s` expression",
            xhs_type->name,
            token_kind_to_cstr(op->kind));
    }

    struct expr* resolved =
        expr_new_binary(&op->location, context()->builtin.bool_, bop, lhs, rhs);
    autil_freezer_register(context()->freezer, resolved);

    // Constant fold integer literal constant expression.
    if (lhs->kind == EXPR_INTEGER && rhs->kind == EXPR_INTEGER) {
        lhs = shallow_implicit_cast(rhs->type, lhs);
        rhs = shallow_implicit_cast(lhs->type, rhs);

        struct value* const value = eval_rvalue(resolved);
        value_freeze(value, context()->freezer);

        assert(value->type->kind == TYPE_BOOL);
        resolved = expr_new_boolean(resolved->location, value->data.boolean);
        autil_freezer_register(context()->freezer, resolved);
    }

    return resolved;
}

static struct expr const*
resolve_expr_binary_compare_order(
    struct resolver* resolver,
    struct token const* op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    (void)resolver;

    lhs = shallow_implicit_cast(rhs->type, lhs);
    rhs = shallow_implicit_cast(lhs->type, rhs);

    if (lhs->type != rhs->type) {
        fatal(
            &op->location,
            "invalid arguments of types `%s` and `%s` in binary `%s` expression",
            lhs->type->name,
            rhs->type->name,
            token_kind_to_cstr(op->kind));
    }

    struct type const* const xhs_type = lhs->type;
    if (!type_can_compare_order(xhs_type)) {
        fatal(
            &op->location,
            "invalid arguments of type `%s` in binary `%s` expression",
            xhs_type->name,
            token_kind_to_cstr(op->kind));
    }

    struct expr* resolved =
        expr_new_binary(&op->location, context()->builtin.bool_, bop, lhs, rhs);
    autil_freezer_register(context()->freezer, resolved);

    // Constant fold integer literal constant expression.
    if (lhs->kind == EXPR_INTEGER && rhs->kind == EXPR_INTEGER) {
        lhs = shallow_implicit_cast(rhs->type, lhs);
        rhs = shallow_implicit_cast(lhs->type, rhs);

        struct value* const value = eval_rvalue(resolved);
        value_freeze(value, context()->freezer);

        assert(value->type->kind == TYPE_BOOL);
        resolved = expr_new_boolean(resolved->location, value->data.boolean);
        autil_freezer_register(context()->freezer, resolved);
    }

    return resolved;
}

static struct expr const*
resolve_expr_binary_arithmetic(
    struct resolver* resolver,
    struct token const* op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    (void)resolver;

    lhs = shallow_implicit_cast(rhs->type, lhs);
    rhs = shallow_implicit_cast(lhs->type, rhs);

    bool const valid = lhs->type == rhs->type && type_is_integer(lhs->type)
        && type_is_integer(rhs->type);
    if (!valid) {
        fatal(
            &op->location,
            "invalid arguments of types `%s` and `%s` in binary `%s` expression",
            lhs->type->name,
            rhs->type->name,
            token_kind_to_cstr(op->kind));
    }

    struct type const* const type = lhs->type; // Arbitrarily use lhs.
    struct expr* resolved = expr_new_binary(&op->location, type, bop, lhs, rhs);
    autil_freezer_register(context()->freezer, resolved);

    // Constant fold integer literal constant expression.
    if (lhs->kind == EXPR_INTEGER && rhs->kind == EXPR_INTEGER) {
        lhs = shallow_implicit_cast(rhs->type, lhs);
        rhs = shallow_implicit_cast(lhs->type, rhs);

        struct value* const value = eval_rvalue(resolved);
        value_freeze(value, context()->freezer);

        assert(type_is_integer(value->type));
        resolved = expr_new_integer(
            resolved->location, resolved->type, value->data.integer);
        autil_freezer_register(context()->freezer, resolved);
    }

    return resolved;
}

static struct expr const*
resolve_expr_binary_bitwise(
    struct resolver* resolver,
    struct token const* op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    (void)resolver;

    lhs = shallow_implicit_cast(rhs->type, lhs);
    rhs = shallow_implicit_cast(lhs->type, rhs);

    if (lhs->type != rhs->type) {
        goto invalid_operand_types;
    }
    struct type const* type = lhs->type; // Arbitrarily use lhs.
    if (type->size == SIZEOF_UNSIZED) {
        fatal(
            &op->location,
            "unsized types `%s` in binary `%s` expression have no bit-representation",
            type->name,
            token_kind_to_cstr(op->kind));
    }

    bool const valid = type->kind == TYPE_BOOL || type->kind == TYPE_BYTE
        || type_is_integer(type);
    if (!valid) {
        goto invalid_operand_types;
    }

    struct expr* resolved = expr_new_binary(&op->location, type, bop, lhs, rhs);
    autil_freezer_register(context()->freezer, resolved);

    // Constant fold integer literal constant expression.
    if (lhs->kind == EXPR_INTEGER && rhs->kind == EXPR_INTEGER) {
        lhs = shallow_implicit_cast(rhs->type, lhs);
        rhs = shallow_implicit_cast(lhs->type, rhs);

        struct value* const value = eval_rvalue(resolved);
        value_freeze(value, context()->freezer);

        assert(type_is_integer(value->type));
        resolved = expr_new_integer(
            resolved->location, resolved->type, value->data.integer);
        autil_freezer_register(context()->freezer, resolved);
    }

    return resolved;

invalid_operand_types:
    fatal(
        &op->location,
        "invalid arguments of types `%s` and `%s` in binary `%s` expression",
        lhs->type->name,
        rhs->type->name,
        token_kind_to_cstr(op->kind));
}

static struct block const*
resolve_block(
    struct resolver* resolver,
    struct symbol_table* symbol_table,
    struct cst_block const* block)
{
    assert(resolver->current_function != NULL);
    assert(symbol_table != NULL);

    struct symbol_table* const save_symbol_table =
        resolver->current_symbol_table;
    resolver->current_symbol_table = symbol_table;
    int const save_rbp_offset = resolver->current_rbp_offset;

    autil_sbuf(struct stmt const*) stmts = NULL;
    for (size_t i = 0; i < autil_sbuf_count(block->stmts); ++i) {
        struct stmt const* const resolved_stmt =
            resolve_stmt(resolver, block->stmts[i]);
        if (resolved_stmt != NULL) {
            autil_sbuf_push(stmts, resolved_stmt);
        }
    }
    autil_sbuf_freeze(stmts, context()->freezer);

    struct block* const resolved =
        block_new(block->location, symbol_table, stmts);

    autil_freezer_register(context()->freezer, resolved);
    resolver->current_symbol_table = save_symbol_table;
    resolver->current_rbp_offset = save_rbp_offset;
    return resolved;
}

static struct type const*
resolve_typespec(struct resolver* resolver, struct cst_typespec const* typespec)
{
    assert(resolver != NULL);
    assert(typespec != NULL);

    switch (typespec->kind) {
    case TYPESPEC_IDENTIFIER: {
        char const* const name = typespec->data.identifier->name;
        struct symbol const* const symbol =
            symbol_table_lookup(resolver->current_symbol_table, name);

        if (symbol == NULL) {
            fatal(
                typespec->location, "use of undeclared identifier `%s`", name);
        }
        if (symbol->kind != SYMBOL_TYPE) {
            fatal(typespec->location, "identifier `%s` is not a type", name);
        }

        return symbol->type;
    }
    case TYPESPEC_FUNCTION: {
        autil_sbuf(struct cst_typespec const* const) const parameter_typespecs =
            typespec->data.function.parameter_typespecs;

        autil_sbuf(struct type const*) parameter_types = NULL;
        autil_sbuf_resize(
            parameter_types, autil_sbuf_count(parameter_typespecs));
        for (size_t i = 0; i < autil_sbuf_count(parameter_typespecs); ++i) {
            parameter_types[i] =
                resolve_typespec(resolver, parameter_typespecs[i]);
        }
        autil_sbuf_freeze(parameter_types, context()->freezer);

        struct type const* const return_type =
            resolve_typespec(resolver, typespec->data.function.return_typespec);

        return type_unique_function(parameter_types, return_type);
    }
    case TYPESPEC_POINTER: {
        struct type const* const base =
            resolve_typespec(resolver, typespec->data.pointer.base);
        return type_unique_pointer(base);
    }
    case TYPESPEC_ARRAY: {
        struct expr const* count_expr =
            resolve_expr(resolver, typespec->data.array.count);
        count_expr =
            shallow_implicit_cast(context()->builtin.usize, count_expr);

        if (count_expr->type != context()->builtin.usize) {
            fatal(
                count_expr->location,
                "illegal array count with non-usize type `%s`",
                count_expr->type->name);
        }

        struct value* const count_value = eval_rvalue(count_expr);

        assert(count_value->type == context()->builtin.usize);
        size_t count = 0u;
        if (bigint_to_uz(&count, count_value->data.integer)) {
            fatal(
                count_expr->location,
                "array count too large (received %s)",
                autil_bigint_to_new_cstr(count_value->data.integer, NULL));
        }
        value_del(count_value);

        struct type const* const base =
            resolve_typespec(resolver, typespec->data.array.base);
        return type_unique_array(count, base);
    }
    case TYPESPEC_SLICE: {
        struct type const* const base =
            resolve_typespec(resolver, typespec->data.slice.base);
        return type_unique_slice(base);
    }
    case TYPESPEC_TYPEOF: {
        struct expr const* const expr =
            resolve_expr(resolver, typespec->data.typeof_.expr);
        return expr->type;
    }
    }

    UNREACHABLE();
    return NULL;
}

void
resolve(struct module* module)
{
    assert(module != NULL);

    struct resolver* const resolver = resolver_new(module);

    // Module namespace.
    if (module->cst->namespace != NULL) {
        autil_sbuf(struct cst_identifier const* const) const identifiers =
            module->cst->namespace->identifiers;

        char const* nsname = NULL;
        char const* nsaddr = NULL;
        for (size_t i = 0; i < autil_sbuf_count(identifiers); ++i) {
            char const* const name = identifiers[i]->name;
            struct source_location const* const location =
                identifiers[i]->location;

            nsname = qualified_name(nsname, name);
            nsaddr = qualified_addr(nsaddr, name);

            struct symbol_table* const module_table =
                symbol_table_new(resolver->current_symbol_table);
            struct symbol_table* const export_table =
                symbol_table_new(resolver->current_export_table);
            autil_sbuf_push(resolver->chilling_symbol_tables, module_table);
            autil_sbuf_push(resolver->chilling_symbol_tables, export_table);

            struct symbol* const module_nssymbol =
                symbol_new_namespace(location, nsname, module_table);
            struct symbol* const export_nssymbol =
                symbol_new_namespace(location, nsname, module_table);
            autil_freezer_register(context()->freezer, module_nssymbol);
            autil_freezer_register(context()->freezer, export_nssymbol);

            symbol_table_insert(
                resolver->current_symbol_table, name, module_nssymbol);
            symbol_table_insert(
                resolver->current_export_table, name, export_nssymbol);
            resolver->current_symbol_table = module_table;
            resolver->current_export_table = export_table;
        }

        resolver->current_symbol_name_prefix = nsname;
        resolver->current_static_addr_prefix = nsaddr;
    }

    // Imports.
    for (size_t i = 0; i < autil_sbuf_count(module->cst->imports); ++i) {
        resolve_import(resolver, module->cst->imports[i]);
    }

    // Top-level declarations.
    autil_sbuf(struct cst_decl const* const) const ordered = module->ordered;
    for (size_t i = 0; i < autil_sbuf_count(ordered); ++i) {
        struct symbol const* const symbol =
            resolve_decl(resolver, module->ordered[i]);
        // If this module declares a namespace then top-level declarations will
        // have been added under the (exported) module namespace and should
        // *not* be added to the module export table or global symbol table
        // using their unqualified names.
        if (module->cst->namespace == NULL) {
            symbol_table_insert(
                resolver->current_export_table, symbol->name, symbol);
            symbol_table_insert(
                context()->global_symbol_table, symbol->name, symbol);
        }
    }

    autil_sbuf(struct incomplete_function const) incomplete =
        resolver->incomplete_functions;
    for (size_t i = 0; i < autil_sbuf_count(incomplete); ++i) {
        complete_function(resolver, &incomplete[i]);
    }

    size_t const chilling_symbol_tables_count =
        autil_sbuf_count(resolver->chilling_symbol_tables);
    for (size_t i = 0; i < chilling_symbol_tables_count; ++i) {
        symbol_table_freeze(
            resolver->chilling_symbol_tables[i], context()->freezer);
    }
    autil_sbuf_fini(resolver->chilling_symbol_tables);

    resolver_del(resolver);
}
