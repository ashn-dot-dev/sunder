// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "sunder.h"

struct incomplete_function {
    struct cst_decl const* decl;
    char const* name; // interned
    struct function* function;
    struct symbol_table* symbol_table;
    struct template_instantiation_link const* chain; // optional
};

struct resolver {
    struct module* module;

    // Optional (NULL => no prefix).
    //
    // Prefix used when creating full symbol names (e.g <prefix>::<name>).
    char const* current_symbol_name_prefix;
    // Optional (NULL => no prefix).
    //
    // Prefix used when creating static address names (e.g. <prefix>.<name>).
    char const* current_static_addr_prefix;

    // Optional (NULL => not in a function).
    struct function* current_function;

    struct symbol_table* current_symbol_table;
    struct symbol_table* current_export_table;

    // Current local counter. Initialized to zero at the start of function
    // completion, and incremented whenever a local variable is given storage.
    unsigned current_local_counter;
    // Current offset of rbp for stack allocated data. Initialized to zero at
    // the start of function completion.
    int current_rbp_offset;

    // True if the resolver is executing within a constant declaration.
    // Currently, this is only used to tell whether slice-list backing arrays
    // should be declared as variables or constants.
    bool is_within_constant_decl;

    // True if the resolver is executing within a loop statement. Set to true
    // when a loop body is being resolved, and set to false once the loop body
    // is finished resolving.
    bool is_within_loop;

    // Optional (NULL => no defer).
    //
    // Current defer evaluated within the current loop. Used to manage defers
    // for break and continue statements.
    struct stmt const* current_loop_defer;

    // Optional (NULL => no defer).
    //
    // Pointer to the head of the current defer statement list node to be
    // evaluated.
    struct stmt const* current_defer;

    // Functions to be completed at the end of the resolve phase after all
    // top-level declarations have been resolved. Incomplete functions defer
    // having their bodies resolved so that mutually recursive functions (e.g.
    // f calls g and g calls f) have access to each others' symbols in the
    // global symbol table without requiring one function to be fully defined
    // before the other.
    //
    // NOTE: This member must *NOT* be saved/restored because template function
    // instantiations may resize the stretchy buffer.
    sbuf(struct incomplete_function const*) incomplete_functions;
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
// Reserve local storage space for an object of the provided name and type.
static struct address const*
resolver_reserve_storage_local(
    struct resolver* self, char const* name, struct type const* type);

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
// Normalize the provided name.
// Providing a zero unique_id parameter implies the symbol is the first and
// potentially only symbol with the given name and should not have the unique
// identifier appended to the normalized symbol (matches gcc behavior for
// multiple local static symbols defined with the same name within the same
// function).
// Returns the normalized name as an interned string.
static char const* // interned
normalize(char const* name, unsigned unique_id);
// Returns the normalization of the provided name via the normalize function.
// Linearly increments unique IDs starting at zero until a unique ID is found
// that does not cause a name collision.
static char const* // interned
normalize_unique(char const* name);
// Add the provided static symbol to the list of static symbols within the
// compilation context.
static void
register_static_symbol(struct symbol const* symbol);

// Finds the symbol or returns NULL if the symbol does not exist.
static struct symbol const*
get_symbol(struct resolver* resolver, struct cst_symbol const* target);
// Finds the symbol or fatally exits. Returns the symbol associated with the
// target concrete syntax tree symbol.
static struct symbol const*
xget_symbol(struct resolver* resolver, struct cst_symbol const* target);
// Finds and (if needed) instantiates the template symbol with the provided
// template arguments or fatally exits. Returns the symbol associated with the
// instantiated type.
static struct symbol const*
xget_template_instance(
    struct resolver* resolver,
    struct source_location location,
    struct symbol const* symbol,
    struct cst_type const* const* const template_arguments);

// Fatally exits if the actual type does not exactly match the expected type.
static void
verify_type_compatibility(
    struct source_location location,
    struct type const* actual,
    struct type const* expected);
// Fatally exits if provided integer-like value is out-of-range for the
// provided byte or integer type.
static void
verify_byte_or_int_in_range(
    struct source_location location,
    struct type const* type,
    struct bigint const* integer);

// Returns a newly created and registered expression node of `expr` explicitly
// casted to `type` if such an explicit cast is valid.
//
// The `location` argument is the location of the casting expression, which may
// or may not be the same as the location of casted expression.
static struct expr const*
explicit_cast(
    struct source_location location,
    struct type const* type,
    struct expr const* expr);
// Returns a newly created and registered expression node of `expr` implicitly
// casted to `type` if such an implicit cast is valid. If `expr` cannot be
// implicitly casted to `type` then expr is returned unchanged.
static struct expr const*
implicit_cast(struct type const* type, struct expr const* expr);

static void
create_static_bytes(
    struct resolver* resolver,
    struct source_location location,
    char const* bytes_start,
    size_t bytes_count,
    struct symbol const** out_array_symbol,
    struct symbol const** out_slice_symbol);

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
resolve_decl_struct(struct resolver* resolver, struct cst_decl const* decl);
static struct symbol const*
resolve_decl_union(struct resolver* resolver, struct cst_decl const* decl);
static struct symbol const*
resolve_decl_enum(struct resolver* resolver, struct cst_decl const* decl);
static struct symbol const*
resolve_decl_extend(struct resolver* resolver, struct cst_decl const* decl);
static struct symbol const*
resolve_decl_alias(struct resolver* resolver, struct cst_decl const* decl);
static struct symbol const*
resolve_decl_extern_variable(
    struct resolver* resolver, struct cst_decl const* decl);
static struct symbol const*
resolve_decl_extern_function(
    struct resolver* resolver, struct cst_decl const* decl);

static void
complete_struct(
    struct resolver* resolver,
    struct symbol const* symbol,
    struct cst_member const* const* members);
static void
complete_union(
    struct resolver* resolver,
    struct symbol const* symbol,
    struct cst_member const* const* members);
struct symbol const*
complete_enum(
    struct resolver* resolver,
    struct source_location location,
    char const* name,
    struct cst_enum_value const* const* values,
    struct cst_member const* const* member_functions);
static void
complete_function(
    struct resolver* resolver, struct incomplete_function const* incomplete);

static struct block
resolve_block(
    struct resolver* resolver,
    struct symbol_table* symbol_table,
    struct cst_block const* block);

static struct stmt const* // optional
resolve_stmt(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const* // optional
resolve_stmt_decl(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_defer_block(
    struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_defer_expr(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_if(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_when(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_for_range(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_for_expr(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_break(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_continue(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_switch(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_return(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_assert(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_assign(struct resolver* resolver, struct cst_stmt const* stmt);
static struct stmt const*
resolve_stmt_expr(struct resolver* resolver, struct cst_stmt const* stmt);

static struct expr const*
resolve_expr(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_symbol(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_boolean(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_integer(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_ieee754(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_character(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_bytes(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_list(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_slice(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_init(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_cast(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_call(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_access_index(
    struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_access_slice(
    struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_access_member(
    struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_access_dereference(
    struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_defined(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_sizeof(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_alignof(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_fileof(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_lineof(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_embed(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_unary(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_unary_logical(
    struct resolver* resolver,
    struct token op,
    enum uop_kind uop,
    struct expr const* rhs);
static struct expr const*
resolve_expr_unary_arithmetic(
    struct resolver* resolver,
    struct token op,
    enum uop_kind uop,
    struct expr const* rhs);
static struct expr const*
resolve_expr_unary_bitwise(
    struct resolver* resolver,
    struct token op,
    enum uop_kind uop,
    struct expr const* rhs);
static struct expr const*
resolve_expr_unary_dereference(
    struct resolver* resolver, struct token op, struct expr const* rhs);
static struct expr const*
resolve_expr_unary_addressof(
    struct resolver* resolver, struct token op, struct expr const* rhs);
static struct expr const*
resolve_expr_unary_startof(
    struct resolver* resolver, struct token op, struct expr const* rhs);
static struct expr const*
resolve_expr_unary_countof(
    struct resolver* resolver, struct token op, struct expr const* rhs);
static struct expr const*
resolve_expr_binary(struct resolver* resolver, struct cst_expr const* expr);
static struct expr const*
resolve_expr_binary_logical(
    struct resolver* resolver,
    struct token op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs);
static struct expr const*
resolve_expr_binary_shift(
    struct resolver* resolver,
    struct token op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs);
static struct expr const*
resolve_expr_binary_compare_equality(
    struct resolver* resolver,
    struct token op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs);
static struct expr const*
resolve_expr_binary_compare_order(
    struct resolver* resolver,
    struct token op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs);
static struct expr const*
resolve_expr_binary_arithmetic(
    struct resolver* resolver,
    struct token op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs);
static struct expr const*
resolve_expr_binary_bitwise(
    struct resolver* resolver,
    struct token op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs);

static struct type const*
resolve_type(struct resolver* resolver, struct cst_type const* type);
static struct type const*
resolve_type_symbol(struct resolver* resolver, struct cst_type const* type);
static struct type const*
resolve_type_function(struct resolver* resolver, struct cst_type const* type);
static struct type const*
resolve_type_pointer(struct resolver* resolver, struct cst_type const* type);
static struct type const*
resolve_type_array(struct resolver* resolver, struct cst_type const* type);
static struct type const*
resolve_type_slice(struct resolver* resolver, struct cst_type const* type);
static struct type const*
resolve_type_struct(struct resolver* resolver, struct cst_type const* type);
static struct type const*
resolve_type_union(struct resolver* resolver, struct cst_type const* type);
static struct type const*
resolve_type_enum(struct resolver* resolver, struct cst_type const* type);
static struct type const*
resolve_type_typeof(struct resolver* resolver, struct cst_type const* type);

static struct resolver*
resolver_new(struct module* module)
{
    assert(module != NULL);

    struct resolver* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->module = module;
    self->current_symbol_name_prefix = NULL;
    self->current_static_addr_prefix = NULL;
    self->current_function = NULL;
    self->current_symbol_table = module->symbols;
    self->current_export_table = module->exports;
    self->current_local_counter = 0;
    self->current_rbp_offset = 0x0;
    self->is_within_loop = false;
    return self;
}

static void
resolver_del(struct resolver* self)
{
    assert(self != NULL);

    sbuf_fini(self->incomplete_functions);

    memset(self, 0x00, sizeof(*self));
    xalloc(self, XALLOC_FREE);
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

    struct string* const tmp = string_new(NULL, 0);
    if (self->current_static_addr_prefix != NULL) {
        string_append_fmt(tmp, "%s.", self->current_static_addr_prefix);
    }
    string_append_cstr(tmp, name);

    char const* const normalized = normalize_unique(string_start(tmp));

    string_del(tmp);

    struct address* const address =
        address_new(address_init_static(normalized, 0u));
    freeze(address);
    return address;
}

static struct address const*
resolver_reserve_storage_local(
    struct resolver* self, char const* name, struct type const* type)
{
    assert(self != NULL);
    assert(self->current_function != NULL);
    assert(name != NULL);
    assert(type != NULL);

    self->current_rbp_offset -= (int)ceil8umax(type->size);
    if (self->current_rbp_offset < self->current_function->local_stack_offset) {
        self->current_function->local_stack_offset = self->current_rbp_offset;
    }

    name = intern_fmt("local_%u_%s", self->current_local_counter++, name);
    struct address* const address =
        address_new(address_init_local(name, self->current_rbp_offset));
    freeze(address);
    return address;
}

static char const*
qualified_name(char const* prefix, char const* name)
{
    assert(name != NULL);

    if (prefix == NULL) {
        return intern_cstr(name);
    }

    return intern_fmt("%s::%s", prefix, name);
}

static char const*
qualified_addr(char const* prefix, char const* name)
{
    assert(name != NULL);

    if (prefix == NULL) {
        return intern_cstr(name);
    }

    return intern_fmt("%s.%s", prefix, name);
}

static char const*
normalize(char const* name, unsigned unique_id)
{
    assert(name != NULL);

    // Substitute invalid assembly character symbols with replacement
    // characters within the provided name.
    struct string* const s = string_new(NULL, 0);
    for (char const* search = name; *search != '\0'; ++search) {
        if (cstr_starts_with(search, "::")) {
            string_append_cstr(s, ".");
            search += 1;
            continue;
        }

        if (safe_isalnum(*search) || *search == '_' || *search == '.') {
            string_append_fmt(s, "%c", *search);
            continue;
        }

        // Replace all non valid identifier characters with an underscore.
        string_append_cstr(s, "_");
    }
    assert(string_count(s) != 0);

    // <name>.<unique-id>
    if (unique_id != 0) {
        string_append_fmt(s, ".%u", unique_id);
    }

    char const* const interned = intern(string_start(s), string_count(s));

    string_del(s);
    return interned;
}

static char const*
normalize_unique(char const* name)
{
    assert(name != NULL);

    unsigned unique_id = 0u;
    char const* normalized = normalize(name, unique_id);
    while (true) {
        bool name_collision = false;
        // Iterating over the static symbol list from back to front was shown
        // to be more performant than iterating from front to back in practice.
        for (size_t i = sbuf_count(context()->static_symbols); i--;) {
            struct symbol const* const symbol = context()->static_symbols[i];
            struct address const* const address = symbol_xget_address(symbol);
            assert(address->kind == ADDRESS_STATIC);
            assert(address->data.static_.offset == 0);

            if (address->data.static_.name == normalized) {
                name_collision = true;
                break;
            }
        }

        if (!name_collision) {
            break; // Found a unique normalized name.
        }

        // Name collision was found. Try a different name with the next
        // sequential unique ID.
        normalized = normalize(name, ++unique_id);
    }

    return normalized;
}

static void
register_static_symbol(struct symbol const* symbol)
{
    assert(symbol != NULL);
    assert(symbol_xget_address(symbol) != NULL);
    assert(symbol_xget_address(symbol)->kind == ADDRESS_STATIC);

    sbuf_push(context()->static_symbols, symbol);
}

static struct symbol const*
get_symbol_ex(
    struct resolver* resolver,
    struct cst_symbol const* target,
    bool error_is_fatal)
{
    assert(resolver != NULL);
    assert(target != NULL);

    assert(sbuf_count(target->elements) != 0);
    struct cst_symbol_element const* const element = target->elements[0];
    char const* const name = element->identifier.name;

    struct symbol_table const* symbol_table = NULL;
    switch (target->start) {
    case CST_SYMBOL_START_NONE: {
        symbol_table = resolver->current_symbol_table;
        break;
    }
    case CST_SYMBOL_START_ROOT: {
        symbol_table = resolver->module->symbols;
        break;
    }
    case CST_SYMBOL_START_TYPE: {
        struct type const* const type =
            resolve_type_typeof(resolver, target->type);
        symbol_table = type->symbols;
        break;
    }
    }
    assert(symbol_table != NULL);

    struct symbol const* lhs = symbol_table_lookup(symbol_table, name);
    if (lhs == NULL) {
        if (!error_is_fatal) {
            return NULL;
        }
        fatal(target->location, "use of undeclared identifier `%s`", name);
    }
    if (sbuf_count(element->template_arguments) != 0) {
        lhs = xget_template_instance(
            resolver, element->location, lhs, element->template_arguments);
    }

    // Single symbol element:
    //      foo
    //      foo[[u16]]
    size_t const element_count = sbuf_count(target->elements);
    if (element_count == 1) {
        return lhs;
    }

    // Qualified symbol:
    //      foo::bar
    //      foo::bar[[u16]]
    //      foo::bar[[u16]]::baz
    //      foo::bar[[u16]]::baz::qux[[u32]]
    struct symbol const* symbol = NULL;
    for (size_t i = 1; i < element_count; ++i) {
        struct cst_symbol_element const* const element = target->elements[i];
        char const* const name = element->identifier.name;

        if (lhs->kind == SYMBOL_NAMESPACE) {
            struct symbol_table const* const symbols =
                lhs->data.namespace.symbols;
            symbol = symbol_table_lookup_local(symbols, name);
            if (symbol == NULL) {
                if (!error_is_fatal) {
                    return NULL;
                }
                fatal(
                    element->location,
                    "use of undeclared identifier `%s` within `%s`",
                    name,
                    lhs->name);
            }
            if (sbuf_count(element->template_arguments) != 0) {
                symbol = xget_template_instance(
                    resolver,
                    element->location,
                    symbol,
                    element->template_arguments);
            }
            lhs = symbol;
            continue;
        }

        if (lhs->kind == SYMBOL_TYPE) {
            struct type const* const type = symbol_xget_type(lhs);
            if (type->kind == TYPE_STRUCT && !type->data.struct_.is_complete) {
                // XXX: When ordering mutually recursive structs A and B, A is
                // marked as ordered to allow for self-referential members,
                // then A creates a dependency on B, and when B orders a member
                // depending on A it sees that A is "already ordered" even
                // though that is not entirely true. This is a special case
                // error check that accounts for this scenario.
                //
                // NOTE: We produce a fatal error here even when error_is_fatal
                // is false, because this is always a bug if it appears in user
                // code.
                fatal(
                    target->elements[i - 1]->location,
                    "use of incomplete type `%s`",
                    type->name);
            }
            symbol = symbol_table_lookup_local(type->symbols, name);
            if (symbol == NULL) {
                if (!error_is_fatal) {
                    return NULL;
                }
                fatal(
                    element->location,
                    "use of undeclared identifier `%s` within `%s`",
                    name,
                    lhs->name);
            }
            if (sbuf_count(element->template_arguments) != 0) {
                symbol = xget_template_instance(
                    resolver,
                    element->location,
                    symbol,
                    element->template_arguments);
            }
            lhs = symbol;
            continue;
        }

        if (lhs->kind == SYMBOL_TEMPLATE) {
            if (sbuf_count(element->template_arguments) == 0) {
                // NOTE: We produce a fatal error here even when error_is_fatal
                // is false, because this is always a bug if it appears in user
                // code.
                fatal(
                    element->location,
                    "template `%s` must be instantiated",
                    lhs->name);
            }
            lhs = xget_template_instance(
                resolver, element->location, lhs, element->template_arguments);
            continue;
        }

        // NOTE: We produce a fatal error here even when error_is_fatal is
        // false, because this is always a bug if it appears in user code.
        fatal(element->location, "`%s` is not a namespace or type", lhs->name);
    }

    assert(symbol != NULL);
    return symbol;
}

static struct symbol const*
get_symbol(struct resolver* resolver, struct cst_symbol const* target)
{
    assert(resolver != NULL);
    assert(target != NULL);

    return get_symbol_ex(resolver, target, false);
}

static struct symbol const*
xget_symbol(struct resolver* resolver, struct cst_symbol const* target)
{
    assert(resolver != NULL);
    assert(target != NULL);

    return get_symbol_ex(resolver, target, true);
}

static struct symbol const*
xget_template_instance(
    struct resolver* resolver,
    struct source_location location,
    struct symbol const* symbol,
    struct cst_type const* const* const template_arguments)
{
    assert(resolver != NULL);
    assert(symbol != NULL);

    switch (symbol->kind) {
    case SYMBOL_TYPE: {
        fatal(
            location,
            "attempted template instantiation of non-template type `%s`",
            symbol->name);
    }
    case SYMBOL_VARIABLE: {
        fatal(
            location,
            "attempted template instantiation of variable `%s`",
            symbol->name);
    }
    case SYMBOL_CONSTANT: {
        fatal(
            location,
            "attempted template instantiation of constant `%s`",
            symbol->name);
    }
    case SYMBOL_FUNCTION: {
        fatal(
            location,
            "attempted template instantiation of function `%s`",
            symbol->name);
    }
    case SYMBOL_TEMPLATE: {
        break;
    }
    case SYMBOL_NAMESPACE: {
        fatal(
            location,
            "attempted template instantiation of namespace `%s`",
            symbol->name);
    }
    }

    // Here we *know* that this should be a template instantiation, because
    // parsing a template list as `[[]]` will produce a parse error with the
    // message "template argument list contains zero template arguments".
    if (sbuf_count(template_arguments) == 0) {
        fatal(
            location,
            "template instantiation of `%s` requires a template argument list",
            symbol->name);
    }

    // To instantiate the function template we replace the template parameters
    // of the template declaration with the template arguments from the current
    // instantiation.
    struct cst_decl const* const decl = symbol->data.template.decl;
    assert(decl != NULL);

    // Currently, functions and structs are the only declarations that can be
    // templated, so the rest off this function will only cater to these cases.
    assert(
        decl->kind == CST_DECL_FUNCTION || decl->kind == CST_DECL_STRUCT
        || decl->kind == CST_DECL_UNION);
    if (decl->kind == CST_DECL_FUNCTION) {
        sbuf(struct cst_identifier const) const template_parameters =
            decl->data.function.template_parameters;
        size_t const template_parameters_count =
            sbuf_count(template_parameters);
        size_t const template_arguments_count = sbuf_count(template_arguments);

        if (template_parameters_count != template_arguments_count) {
            fatal(
                location,
                "expected %zu template argument(s) for template `%s` (received %zu)",
                template_parameters_count,
                symbol->name,
                template_arguments_count);
        }

        sbuf(struct type const*) template_types = NULL;
        for (size_t i = 0; i < template_arguments_count; ++i) {
            sbuf_push(
                template_types, resolve_type(resolver, template_arguments[i]));
        }
        sbuf_freeze(template_types);

        // Replace function identifier (i.e. name).
        struct string* const name_string = string_new_cstr(symbol->name);
        string_append_cstr(name_string, "[[");
        for (size_t i = 0; i < template_arguments_count; ++i) {
            if (i != 0) {
                string_append_cstr(name_string, ", ");
            }
            string_append_fmt(name_string, "%s", template_types[i]->name);
        }
        string_append_cstr(name_string, "]]");
        char const* const name_interned =
            intern_cstr(string_start(name_string));
        string_del(name_string);
        struct cst_identifier const instance_identifier =
            cst_identifier_init(location, name_interned);
        // Replace template parameters. Zero template parameters means this
        // function is no longer a template.
        sbuf(struct cst_identifier const) const instance_template_parameters =
            NULL;
        // Function parameters do not change. When the actual function is
        // resolved it will do so inside a symbol table where a template
        // parameter's name maps to the template instance's chosen type symbol.
        sbuf(struct cst_function_parameter const* const)
            const instance_function_parameters =
                decl->data.function.function_parameters;
        // Same goes for the return type specification.
        struct cst_type const* const instance_return_type =
            decl->data.function.return_type;
        // And the body is also unchanged.
        struct cst_block const instance_body = decl->data.function.body;

        // Check if a symbol corresponding to these template arguments has
        // already been created. If so then we reuse the cached symbol.
        struct symbol const* const existing_instance =
            symbol_table_lookup(symbol->data.template.symbols, name_interned);
        if (existing_instance != NULL) {
            return existing_instance;
        }

        // Create a symbol table to hold the template arguments for this
        // instance. Then add each template argument type to the symbol table,
        // mapping from the template type name to the argument type.
        struct symbol_table* const instance_symbol_table =
            symbol_table_new(symbol->data.template.parent_symbol_table);
        for (size_t i = 0; i < template_parameters_count; ++i) {
            struct symbol* const symbol = symbol_new_type(
                template_parameters[i].location, template_types[i]);
            freeze(symbol);
            symbol_table_insert(
                instance_symbol_table,
                template_parameters[i].name,
                symbol,
                false);
        }
        // Store the template function itself in addition to the template
        // arguments so that self referential functions (e.g. fibonacci) do not
        // have to fully qualify the function name.
        symbol_table_insert(instance_symbol_table, symbol->name, symbol, false);
        symbol_table_freeze(instance_symbol_table);

        // Generate the template instance concrete syntax tree.
        struct cst_decl* const instance_decl = cst_decl_new_function(
            location,
            instance_identifier,
            instance_template_parameters,
            instance_function_parameters,
            instance_return_type,
            instance_body);
        freeze(instance_decl);

        struct template_instantiation_link* link = xalloc(NULL, sizeof(*link));
        link->next = context()->template_instantiation_chain;
        link->name = decl->name;
        if (symbol->data.template.symbol_name_prefix != NULL) {
            link->name = intern_fmt(
                "%s::%s",
                symbol->data.template.symbol_name_prefix,
                instance_decl->name);
        }
        link->location = location;
        context()->template_instantiation_chain = link;
        freeze(link);

        // Resolve the actual template instance.
        char const* const save_symbol_name_prefix =
            resolver->current_symbol_name_prefix;
        char const* const save_static_addr_prefix =
            resolver->current_static_addr_prefix;
        struct symbol_table* const save_symbol_table =
            resolver->current_symbol_table;

        resolver->current_symbol_name_prefix =
            symbol->data.template.symbol_name_prefix;
        resolver->current_static_addr_prefix =
            symbol->data.template.symbol_addr_prefix;
        resolver->current_symbol_table = instance_symbol_table;
        struct symbol const* resolved_symbol =
            resolve_decl_function(resolver, instance_decl);

        resolver->current_symbol_name_prefix = save_symbol_name_prefix;
        resolver->current_static_addr_prefix = save_static_addr_prefix;
        resolver->current_symbol_table = save_symbol_table;

        // Add the unique instance to the cache of instances for the template.
        assert(resolved_symbol->kind == SYMBOL_FUNCTION);
        symbol_table_insert(
            symbol->data.template.symbols,
            name_interned,
            resolved_symbol,
            false);

        context()->template_instantiation_chain = link->next;

        return resolved_symbol;
    }
    if (decl->kind == CST_DECL_STRUCT || decl->kind == CST_DECL_UNION) {
        sbuf(struct cst_identifier const) const template_parameters =
            decl->kind == CST_DECL_STRUCT
            ? decl->data.struct_.template_parameters
            : decl->data.union_.template_parameters;
        size_t const template_parameters_count =
            sbuf_count(template_parameters);
        size_t const template_arguments_count = sbuf_count(template_arguments);

        if (template_parameters_count != template_arguments_count) {
            fatal(
                location,
                "expected %zu template argument(s) for template `%s` (received %zu)",
                template_parameters_count,
                symbol->name,
                template_arguments_count);
        }

        sbuf(struct type const*) template_types = NULL;
        for (size_t i = 0; i < template_arguments_count; ++i) {
            sbuf_push(
                template_types, resolve_type(resolver, template_arguments[i]));
        }
        sbuf_freeze(template_types);

        // Replace the type identifier (i.e. name).
        struct string* const name_string = string_new_cstr(symbol->name);
        string_append_cstr(name_string, "[[");
        for (size_t i = 0; i < template_arguments_count; ++i) {
            if (i != 0) {
                string_append_cstr(name_string, ", ");
            }
            string_append_fmt(name_string, "%s", template_types[i]->name);
        }
        string_append_cstr(name_string, "]]");
        char const* const name_interned =
            intern_cstr(string_start(name_string));
        string_del(name_string);
        struct cst_identifier const instance_identifier =
            cst_identifier_init(location, name_interned);
        // Replace template parameters. Zero template parameters means this
        // type is no longer a template.
        sbuf(struct cst_identifier const) const instance_template_parameters =
            NULL;
        // Struct/union members do not change. When the actual struct/union is
        // resolved it will do so inside a symbol table where a template
        // parameter's name maps to the template instance's chosen type symbol.
        sbuf(struct cst_member const* const) const instance_members =
            decl->kind == CST_DECL_STRUCT ? decl->data.struct_.members
                                          : decl->data.union_.members;

        // Check if a symbol corresponding to these template arguments has
        // already been created. If so then we reuse the cached symbol.
        struct symbol const* const existing_instance =
            symbol_table_lookup(symbol->data.template.symbols, name_interned);
        if (existing_instance != NULL) {
            return existing_instance;
        }

        // Create a symbol table to hold the template arguments for this
        // instance. Then add each template argument type to the symbol table,
        // mapping from the template type name to the argument type.
        struct symbol_table* const instance_symbol_table =
            symbol_table_new(symbol->data.template.parent_symbol_table);
        for (size_t i = 0; i < template_parameters_count; ++i) {
            struct symbol* const symbol = symbol_new_type(
                template_parameters[i].location, template_types[i]);
            freeze(symbol);
            symbol_table_insert(
                instance_symbol_table,
                template_parameters[i].name,
                symbol,
                false);
        }
        // Store the template struct/union itself in addition to the template
        // arguments so that self referential types (e.g. return values of
        // init functions) do not have to fully qualify the type.
        symbol_table_insert(instance_symbol_table, symbol->name, symbol, false);
        symbol_table_freeze(instance_symbol_table);

        // Generate the template instance concrete syntax tree.
        struct cst_decl* const instance_decl = decl->kind == CST_DECL_STRUCT
            ? cst_decl_new_struct(
                location,
                instance_identifier,
                instance_template_parameters,
                instance_members)
            : cst_decl_new_union(
                location,
                instance_identifier,
                instance_template_parameters,
                instance_members);
        freeze(instance_decl);

        struct template_instantiation_link* link = xalloc(NULL, sizeof(*link));
        link->next = context()->template_instantiation_chain;
        link->name = instance_decl->name;
        if (symbol->data.template.symbol_name_prefix != NULL) {
            link->name = intern_fmt(
                "%s::%s",
                symbol->data.template.symbol_name_prefix,
                instance_decl->name);
        }
        link->location = location;
        context()->template_instantiation_chain = link;
        freeze(link);

        // Resolve the actual template instance.
        char const* const save_symbol_name_prefix =
            resolver->current_symbol_name_prefix;
        char const* const save_static_addr_prefix =
            resolver->current_static_addr_prefix;
        struct symbol_table* const save_symbol_table =
            resolver->current_symbol_table;

        resolver->current_symbol_name_prefix =
            symbol->data.template.symbol_name_prefix;
        resolver->current_static_addr_prefix =
            symbol->data.template.symbol_addr_prefix;
        resolver->current_symbol_table = instance_symbol_table;
        struct symbol const* resolved_symbol = decl->kind == CST_DECL_STRUCT
            ? resolve_decl_struct(resolver, instance_decl)
            : resolve_decl_union(resolver, instance_decl);

        resolver->current_symbol_name_prefix = save_symbol_name_prefix;
        resolver->current_static_addr_prefix = save_static_addr_prefix;
        resolver->current_symbol_table = save_symbol_table;

        // Add the unique instance to the cache of instances for the template.
        assert(resolved_symbol->kind == SYMBOL_TYPE);
        symbol_table_insert(
            symbol->data.template.symbols,
            name_interned,
            resolved_symbol,
            false);

        // Now that the instance is in the cache we can complete the type. If
        // we did not add the instance to the cache first then any self
        // referential template instances would cause instance resolution to
        // enter an infinite loop.
        if (decl->kind == CST_DECL_STRUCT) {
            complete_struct(
                resolver, resolved_symbol, instance_decl->data.struct_.members);
        }
        if (decl->kind == CST_DECL_UNION) {
            complete_union(
                resolver, resolved_symbol, instance_decl->data.union_.members);
        }

        context()->template_instantiation_chain = link->next;

        return resolved_symbol;
    }

    UNREACHABLE();
    return NULL;
}

static void
verify_type_compatibility(
    struct source_location location,
    struct type const* actual,
    struct type const* expected)
{
    assert(actual != NULL);
    assert(expected != NULL);

    if (actual != expected) {
        fatal(
            location,
            "incompatible type `%s` (expected `%s`)",
            actual->name,
            expected->name);
    }
}

static void
verify_byte_or_int_in_range(
    struct source_location location,
    struct type const* type,
    struct bigint const* integer)
{
    assert(type != NULL && (type->kind == TYPE_BYTE || type_is_integer(type)));
    assert(integer != NULL);

    if (type->kind == TYPE_BYTE) {
        if (bigint_cmp(integer, context()->u8_min) < 0) {
            char* const int_cstr = bigint_to_new_cstr(integer);
            char* const min_cstr = bigint_to_new_cstr(context()->u8_min);
            fatal(location, "out-of-range byte (%s < %s)", int_cstr, min_cstr);
        }

        if (bigint_cmp(integer, context()->u8_max) > 0) {
            char* const int_cstr = bigint_to_new_cstr(integer);
            char* const max_cstr = bigint_to_new_cstr(context()->u8_max);
            fatal(location, "out-of-range byte (%s > %s)", int_cstr, max_cstr);
        }

        return;
    }

    if (type->kind == TYPE_INTEGER) {
        // Unsized integers have no min or max value.
        return;
    }

    if (bigint_cmp(integer, type->data.integer.min) < 0) {
        char* const int_cstr = bigint_to_new_cstr(integer);
        char* const min_cstr = bigint_to_new_cstr(type->data.integer.min);
        fatal(location, "out-of-range integer (%s < %s)", int_cstr, min_cstr);
    }

    if (bigint_cmp(integer, type->data.integer.max) > 0) {
        char* const int_cstr = bigint_to_new_cstr(integer);
        char* const max_cstr = bigint_to_new_cstr(type->data.integer.max);
        fatal(location, "out-of-range integer (%s > %s)", int_cstr, max_cstr);
    }
}

static struct expr const*
explicit_cast(
    struct source_location location,
    struct type const* type,
    struct expr const* expr)
{
    assert(type != NULL);
    assert(expr != NULL);

    // Casts from non-integer-non-real unsized types are always disallowed.
    if ((expr->type->kind != TYPE_INTEGER && expr->type->kind != TYPE_REAL)
        && expr->type->size == SIZEOF_UNSIZED) {
        fatal(
            location,
            "invalid cast from unsized type `%s` to `%s`",
            expr->type->name,
            type->name);
    }

    // Casts to unsized types are always disallowed.
    if (type->size == SIZEOF_UNSIZED) {
        fatal(
            location,
            "invalid cast to unsized type `%s` from `%s`",
            type->name,
            expr->type->name);
    }

    bool const cast_type_is_pointer_to_any =
        type->kind == TYPE_POINTER && type->data.pointer.base->kind == TYPE_ANY;
    bool const expr_type_is_pointer_to_any = expr->type->kind == TYPE_POINTER
        && expr->type->data.pointer.base->kind == TYPE_ANY;
    bool const valid = (type_is_integer(type) && type_is_integer(expr->type))
        || (type_is_integer(type) && expr->type->kind == TYPE_BOOL)
        || (type_is_integer(type) && expr->type->kind == TYPE_BYTE)
        || (type_is_integer(type) && expr->type->kind == TYPE_F32)
        || (type_is_integer(type) && expr->type->kind == TYPE_F64)
        || (type->kind == TYPE_F32 && type_is_integer(expr->type))
        || (type->kind == TYPE_F64 && type_is_integer(expr->type))
        || (type_is_ieee754(type) && type_is_ieee754(expr->type))
        || (type->kind == TYPE_BOOL && expr->type->kind == TYPE_BOOL)
        || (type->kind == TYPE_BOOL && expr->type->kind == TYPE_BYTE)
        || (type->kind == TYPE_BOOL && type_is_integer(expr->type))
        || (type->kind == TYPE_BYTE && expr->type->kind == TYPE_BOOL)
        || (type->kind == TYPE_BYTE && expr->type->kind == TYPE_BYTE)
        || (type->kind == TYPE_BYTE && type_is_integer(expr->type))
        || (type->kind == TYPE_POINTER && expr->type->kind == TYPE_POINTER)
        || (type->kind == TYPE_POINTER && expr->type->kind == TYPE_USIZE)
        || (cast_type_is_pointer_to_any && expr->type->kind == TYPE_FUNCTION)
        || (type->kind == TYPE_USIZE && expr->type->kind == TYPE_POINTER)
        || (type->kind == TYPE_FUNCTION && expr->type->kind == TYPE_FUNCTION)
        || (type->kind == TYPE_FUNCTION && expr_type_is_pointer_to_any)
        || (type->kind == TYPE_ENUM && expr->type->kind == TYPE_BOOL)
        || (type->kind == TYPE_BOOL && expr->type->kind == TYPE_ENUM)
        || (type->kind == TYPE_ENUM && expr->type->kind == TYPE_BYTE)
        || (type->kind == TYPE_BYTE && expr->type->kind == TYPE_ENUM)
        || (type->kind == TYPE_ENUM && type_is_integer(expr->type))
        || (type_is_integer(type) && expr->type->kind == TYPE_ENUM)
        || (type->kind == TYPE_ENUM && expr->type->kind == TYPE_ENUM);
    if (!valid) {
        fatal(
            location,
            "invalid cast from `%s` to `%s`",
            expr->type->name,
            type->name);
    }

    struct expr* resolved = expr_new_cast(location, type, expr);
    freeze(resolved);

    // Casts from function type to function type must have all parameter types
    // and the return type match, or have parameter types and the return type
    // be a T pointer to any pointer conversion.
    if (type->kind == TYPE_FUNCTION && expr->type->kind == TYPE_FUNCTION) {
        struct type const* const from = expr->type;
        if (sbuf_count(type->data.function.parameter_types)
            != sbuf_count(from->data.function.parameter_types)) {
            fatal(
                location,
                "cannot convert from `%s` to `%s` (mismatched parameter count)",
                expr->type->name,
                type->name);
        }

        size_t const param_count =
            sbuf_count(type->data.function.parameter_types);
        for (size_t i = 0; i < param_count; ++i) {
            struct type const* const type_param =
                type->data.function.parameter_types[i];
            struct type const* const from_param =
                from->data.function.parameter_types[i];

            bool const same = type_param == from_param;
            bool const non_any_ptr_to_any_ptr = type_param->kind == TYPE_POINTER
                && type_param->data.pointer.base->kind == TYPE_ANY
                && from_param->kind == TYPE_POINTER
                && from_param->data.pointer.base->kind != TYPE_ANY;
            if (!same && !non_any_ptr_to_any_ptr) {
                fatal(
                    location,
                    "cannot convert from `%s` to `%s` (mismatched parameter types `%s` and `%s`)",
                    expr->type->name,
                    type->name,
                    from_param->name,
                    type_param->name);
            }
        }

        struct type const* const type_return = type->data.function.return_type;
        struct type const* const from_return = from->data.function.return_type;
        bool const same = type_return == from_return;
        bool const non_any_ptr_to_any_ptr = type_return->kind == TYPE_POINTER
            && type_return->data.pointer.base->kind == TYPE_ANY
            && from_return->kind == TYPE_POINTER
            && from_return->data.pointer.base->kind != TYPE_ANY;
        if (!same && !non_any_ptr_to_any_ptr) {
            fatal(
                location,
                "cannot convert from `%s` to `%s` (mismatched return types `%s` and `%s`)",
                expr->type->name,
                type->name,
                from_return->name,
                type_return->name);
        }

        resolved = expr_new_cast(location, type, expr);

        freeze(resolved);
        return resolved;
    }

    // OPTIMIZATION(constant folding)
    if (type->kind == TYPE_BOOL && expr->kind == EXPR_VALUE) {
        struct value* const value = eval_rvalue(resolved);
        value_freeze(value);

        assert(value->type->kind == TYPE_BOOL);
        resolved = expr_new_value(resolved->location, value);

        freeze(resolved);
        return resolved;
    }

    // OPTIMIZATION(constant folding)
    if (type->kind == TYPE_BYTE && expr->kind == EXPR_VALUE) {
        struct value* const value = eval_rvalue(resolved);
        value_freeze(value);

        assert(value->type->kind == TYPE_BYTE);
        resolved = expr_new_value(resolved->location, value);

        freeze(resolved);
        return resolved;
    }

    // OPTIMIZATION(constant folding)
    if (type_is_integer(type) && expr->kind == EXPR_VALUE) {
        struct value* const value = eval_rvalue(resolved);
        value_freeze(value);

        assert(type_is_integer(value->type));
        resolved = expr_new_value(resolved->location, value);

        freeze(resolved);
        return resolved;
    }

    // OPTIMIZATION(constant folding)
    if (type_is_ieee754(type) && expr->kind == EXPR_VALUE) {
        struct value* const value = eval_rvalue(resolved);
        value_freeze(value);

        assert(type_is_ieee754(type));
        resolved = expr_new_value(resolved->location, value);

        freeze(resolved);
        return resolved;
    }

    // OPTIMIZATION(constant folding)
    if (type->kind == TYPE_ENUM && expr->kind == EXPR_VALUE) {
        struct value* const value = eval_rvalue(resolved);
        value_freeze(value);

        assert(value->type->kind == TYPE_ENUM);
        resolved = expr_new_value(resolved->location, value);

        freeze(resolved);
        return resolved;
    }

    return resolved;
}

static struct expr const*
implicit_cast(struct type const* type, struct expr const* expr)
{
    assert(type != NULL);
    assert(expr != NULL);

    struct type const* const from = expr->type;

    // FROM type TO type (same type).
    if (type == from) {
        return expr;
    }

    // FROM sized integer TO byte.
    if (type->kind == TYPE_BYTE && from->kind == TYPE_INTEGER) {
        return explicit_cast(expr->location, type, expr);
    }

    // FROM unsized integer TO sized integer.
    if (type_is_integer(type) && from->kind == TYPE_INTEGER) {
        return explicit_cast(expr->location, type, expr);
    }

    // FROM unsized floating point TO sized floating point.
    if (type_is_ieee754(type) && from->kind == TYPE_REAL) {
        return explicit_cast(expr->location, type, expr);
    }

    // FROM non-any pointer TO any pointer.
    bool const type_is_any_pointer =
        type->kind == TYPE_POINTER && type->data.pointer.base->kind == TYPE_ANY;
    bool const from_is_oth_pointer =
        from->kind == TYPE_POINTER && from->data.pointer.base->kind != TYPE_ANY;
    if (type_is_any_pointer && from_is_oth_pointer) {
        return explicit_cast(expr->location, type, expr);
    }

    // FROM function with typed pointers TO function with any pointers.
    if (type->kind == TYPE_FUNCTION && from->kind == TYPE_FUNCTION) {
        return explicit_cast(expr->location, type, expr);
    }

    // No implicit cast could be performed.
    return expr;
}

static void
create_static_bytes(
    struct resolver* resolver,
    struct source_location location,
    char const* bytes_start,
    size_t bytes_count,
    struct symbol const** out_array_symbol,
    struct symbol const** out_slice_symbol)
{
    assert(resolver != NULL);
    assert(bytes_start != NULL || bytes_count == 0);
    assert(out_array_symbol != NULL);
    assert(out_slice_symbol != NULL);

    // Bytes Array Object

    struct type const* const array_type = type_unique_array(
        location, bytes_count + 1 /*NUL*/, context()->builtin.byte);

    struct address const* const array_address =
        resolver_reserve_storage_static(resolver, "__bytes_array");

    sbuf(struct value*) array_elements = NULL;
    for (size_t i = 0; i < bytes_count; ++i) {
        uint8_t const byte = (uint8_t)bytes_start[i];
        sbuf_push(array_elements, value_new_byte(byte));
    }
    // Append a NUL byte to the end of every bytes literal. This NUL byte is
    // not included in the slice length, but will allow bytes literals to be
    // accessed as NUL-terminated arrays when interfacing with C code.
    sbuf_push(array_elements, value_new_byte(0x00));
    struct value* const array_value =
        value_new_array(array_type, array_elements, NULL);
    value_freeze(array_value);

    struct object* const array_object =
        object_new(array_type, array_address, array_value);
    freeze(array_object);

    struct symbol* const array_symbol = symbol_new_constant(
        location, array_address->data.static_.name, array_object);
    freeze(array_symbol);
    register_static_symbol(array_symbol);

    // Bytes Slice Object

    struct address const* const slice_address =
        resolver_reserve_storage_static(resolver, "__bytes_slice");

    struct value* const slice_start =
        value_new_pointer(context()->builtin.pointer_to_byte, *array_address);
    struct value* const slice_count = value_new_integer(
        context()->builtin.usize, bigint_new_umax(bytes_count));
    struct value* const slice_value = value_new_slice(
        context()->builtin.slice_of_byte, slice_start, slice_count);
    value_freeze(slice_value);

    struct object* const slice_object = object_new(
        context()->builtin.slice_of_byte, slice_address, slice_value);
    freeze(slice_object);

    struct symbol* const slice_symbol = symbol_new_constant(
        location, slice_address->data.static_.name, slice_object);
    freeze(slice_symbol);
    register_static_symbol(slice_symbol);

    *out_array_symbol = array_symbol;
    *out_slice_symbol = slice_symbol;
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

    for (size_t i = 0; i < sbuf_count(othr->elements); ++i) {
        char const* const name = othr->elements[i].name;
        struct symbol const* const symbol = othr->elements[i].symbol;

        if (symbol->kind == SYMBOL_NAMESPACE) {
            // Add all symbols from the namespace in the other symbol table to
            // the namespace in self's symbol table.
            struct symbol const* existing =
                symbol_table_lookup_local(self, name);
            if (existing == NULL) {
                // There is currently no symbol associated for the namespace in
                // self. Create a new namespace symbol for this purpose and
                // perform the merge.
                struct symbol_table* const table = symbol_table_new(self);
                sbuf_push(context()->chilling_symbol_tables, table);

                struct symbol* const namespace =
                    symbol_new_namespace(symbol->location, symbol->name, table);
                freeze(namespace);
                symbol_table_insert(self, name, namespace, false);
                existing = namespace;
            }

            if (existing->kind != SYMBOL_NAMESPACE) {
                // Actual name collision! Attempt to insert the symbol from the
                // other symbol table into self so that a redeclaration error
                // is generated.
                symbol_table_insert(self, name, symbol, false);
            }

            merge_symbol_table(
                resolver,
                existing->data.namespace.symbols,
                symbol->data.namespace.symbols);
            continue;
        }

        // Add the symbol if it has not been added by a previous import.
        // Perform a pointer inequality comparison so that symbols with the
        // same name that do not refer to the same symbol definition cause a
        // redeclaration error.
        struct symbol const* const existing =
            symbol_table_lookup_local(self, name);
        if (existing == NULL || existing != symbol) {
            symbol_table_insert(self, name, symbol, false);
        }
    }
}

// Returns the canonical representation of the provided search path or NULL.
static char const* // interned
canonical_search_path(char const* module_path, char const* search_path)
{
    assert(module_path != NULL);
    assert(search_path != NULL);

    char const* result = NULL;

    // Path relative to the current module.
    char const* const module_dir = directory_path(module_path);
    struct string* const tmp = string_new_fmt("%s/%s", module_dir, search_path);
    if (file_exists(string_start(tmp))) {
        result = canonical_path(string_start(tmp));
        string_del(tmp);
        return result;
    }

    // Path relative to environment-defined search path-list.
    char const* SUNDER_SEARCH_PATH = getenv("SUNDER_SEARCH_PATH");
    if (SUNDER_SEARCH_PATH == NULL) {
        string_del(tmp);
        return NULL;
    }
    string_resize(tmp, 0u);
    string_append_cstr(tmp, SUNDER_SEARCH_PATH);

    sbuf(struct string*) const imp =
        string_split(tmp, ":", STR_LITERAL_COUNT(":"));
    for (size_t i = 0; i < sbuf_count(imp); ++i) {
        struct string* const s = imp[i];

        string_resize(tmp, 0u);
        string_append_fmt(tmp, "%s/%s", string_start(s), search_path);

        if (!file_exists(string_start(tmp))) {
            continue;
        }

        result = canonical_path(string_start(tmp));
        break; // Found the module.
    }

    string_del(tmp);
    for (size_t i = 0; i < sbuf_count(imp); ++i) {
        string_del(imp[i]);
    }
    sbuf_fini(imp);
    return result;
}

static void
resolve_import_file(
    struct resolver* resolver,
    struct source_location location,
    char const* file_name,
    bool from_directory)
{
    assert(resolver != NULL);
    assert(file_name != NULL);

    char const* const path =
        canonical_search_path(resolver->module->path, file_name);
    if (path == NULL) {
        fatal(location, "failed to resolve import `%s`", file_name);
    }

    if (file_is_directory(path)) {
        sbuf(char const*) dir_contents = directory_files(path);
        for (size_t i = 0; i < sbuf_count(dir_contents); ++i) {
            struct string* const string =
                string_new_fmt("%s/%s", file_name, dir_contents[i]);
            char const* const interned = intern_cstr(string_start(string));
            string_del(string);
            if (!file_is_directory(interned)) {
                resolve_import_file(resolver, location, interned, true);
            }
        }
        sbuf_fini(dir_contents);
        return;
    }

    if (from_directory && !cstr_ends_with(file_name, ".sunder")) {
        // Ignore source files imported via a directory import if they do not
        // end in a `.sunder` extension. This will allow directories containing
        // non-Sunder files to be imported without the compiler producing an
        // error from trying to load something like `.txt` file as a Sunder
        // module.
        return;
    }

    if (from_directory && cstr_ends_with(file_name, ".test.sunder")) {
        // Ignore test files imported via a directory import.
        return;
    }

    // For a Sunder directory "foo" containing the sources files:
    //
    //      + foo.sunder
    //      + foo.amd64.sunder
    //      + foo.arm64.sunder
    //
    // the import statement:
    //
    //      import "foo";
    //
    // should import "foo.amd64.sunder" on amd64 systems, import
    // "foo.arm64.sunder" on arm64 systems, and import "foo.sunder" on all
    // other systems.
    //
    // When this function processes "foo.sunder", the filesystem is checked for
    // the existence of "foo.<arch>-<host>.sunder".
    //
    // If "foo.<arch>-<host>.sunder" exists, then "foo.sunder" is skipped. The
    // same check is performed with "foo.<arch>.sunder" and
    // "foo.<host>.sunder".
    //
    // When this function processes "foo.amd64.sunder" and "foo.arm64.sunder",
    // the text between the "foo." part of the file name and the ".sunder" file
    // extension is extracted.
    //
    // If that text does not match "<arch>-<host>, "<arch>", or "<host>", then
    // the file is skipped.
    char const* const platform =
        platform_to_cstr(context()->arch, context()->host);
    char const* const arch = arch_to_cstr(context()->arch);
    char const* const host = host_to_cstr(context()->host);
    size_t file_name_count = strlen(file_name);
    assert(file_name_count >= STR_LITERAL_COUNT(".sunder"));
    size_t stem_count = file_name_count - STR_LITERAL_COUNT(".sunder");
    char const* const stem = intern_fmt("%.*s", (int)stem_count, file_name);
    char const* const dot = strchr(stem, '.');
    bool const is_special_file = dot != NULL;
    if (from_directory && is_special_file) {
        char const* start = dot + 1;
        bool const matches_target = (0 == strcmp(start, platform))
            || (0 == strcmp(start, arch)) || (0 == strcmp(start, host));
        if (!matches_target) {
            return;
        }
    }
    if (from_directory && !is_special_file) {
        bool const platform_specific_file_exists =
            file_exists(intern_fmt("%s.%s.sunder", stem, platform))
            || file_exists(intern_fmt("%s.%s.sunder", stem, arch))
            || file_exists(intern_fmt("%s.%s.sunder", stem, host));
        if (platform_specific_file_exists) {
            return;
        }
    }

    struct module const* module = lookup_module(path);
    if (module == NULL) {
        module = load_module(file_name, path);
    }
    if (!module->loaded) {
        fatal(location, "circular dependency when importing `%s`", file_name);
    }
    merge_symbol_table(resolver, resolver->module->symbols, module->exports);
}

static void
resolve_import(struct resolver* resolver, struct cst_import const* import)
{
    assert(resolver != NULL);
    assert(import != NULL);

    resolve_import_file(resolver, import->location, import->path, false);
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
    case CST_DECL_STRUCT: /* fallthrough */
    case CST_DECL_UNION: {
        // Should have already been resolved in the initial pre-declaration of
        // all top-level structs/unions.
        UNREACHABLE();
    }
    case CST_DECL_ENUM: {
        return resolve_decl_enum(resolver, decl);
    }
    case CST_DECL_EXTEND: {
        return resolve_decl_extend(resolver, decl);
    }
    case CST_DECL_ALIAS: {
        return resolve_decl_alias(resolver, decl);
    }
    case CST_DECL_EXTERN_VARIABLE: {
        return resolve_decl_extern_variable(resolver, decl);
    }
    case CST_DECL_EXTERN_FUNCTION: {
        return resolve_decl_extern_function(resolver, decl);
    }
    }

    UNREACHABLE();
    return NULL;
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

    struct expr const* expr = NULL;
    if (decl->data.variable.expr != NULL) {
        expr = resolve_expr(resolver, decl->data.variable.expr);
    }

    assert(decl->data.variable.type != NULL || expr != NULL);
    struct type const* const type = decl->data.variable.type != NULL
        ? resolve_type(resolver, decl->data.variable.type)
        : expr->type;
    if (type->size == SIZEOF_UNSIZED) {
        fatal(
            decl->location,
            "declaration of variable with unsized type `%s`",
            type->name);
    }

    if (expr != NULL) {
        expr = implicit_cast(type, expr);
        verify_type_compatibility(expr->location, expr->type, type);
    }

    // Initialized static variables have their initial values computed at
    // compile-time, but local/non-static/uninitialized variables have their
    // value calculated/assigned at runtime.
    bool const is_static = resolver_is_global(resolver);
    struct value* value = NULL;
    if (is_static && expr != NULL) {
        value = eval_rvalue(expr);
        value_freeze(value);
    }

    struct address const* const address = is_static
        ? resolver_reserve_storage_static(resolver, decl->name)
        : resolver_reserve_storage_local(resolver, decl->name, type);

    struct object* const object = object_new(type, address, value);
    freeze(object);

    struct symbol* const symbol =
        symbol_new_variable(decl->location, decl->name, object);
    freeze(symbol);

    symbol_table_insert(
        resolver->current_symbol_table,
        symbol->name,
        symbol,
        !resolver_is_global(resolver));
    if (is_static) {
        register_static_symbol(symbol);
    }

    if (lhs != NULL) {
        struct expr* const identifier =
            expr_new_symbol(decl->data.variable.identifier.location, symbol);
        freeze(identifier);
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

    resolver->is_within_constant_decl = true;

    struct expr const* expr = NULL;
    if (decl->data.constant.expr != NULL) {
        expr = resolve_expr(resolver, decl->data.constant.expr);
    }

    struct type const* const type = decl->data.constant.type != NULL
        ? resolve_type(resolver, decl->data.constant.type)
        : expr->type;
    if (type->size == SIZEOF_UNSIZED) {
        fatal(
            decl->location,
            "declaration of constant with unsized type `%s`",
            type->name);
    }

    if (expr != NULL) {
        expr = implicit_cast(type, expr);
        verify_type_compatibility(expr->location, expr->type, type);
    }

    // Constants (globals and locals) have their values computed at
    // compile-time and therefore must always be added to the symbol table with
    // an evaluated value when initialized.
    struct value* value = NULL;
    if (expr != NULL) {
        value = eval_rvalue(expr);
        value_freeze(value);
    }

    struct address const* const address =
        resolver_reserve_storage_static(resolver, decl->name);

    struct object* const object = object_new(type, address, value);
    freeze(object);

    struct symbol* const symbol =
        symbol_new_constant(decl->location, decl->name, object);
    freeze(symbol);

    symbol_table_insert(
        resolver->current_symbol_table,
        symbol->name,
        symbol,
        !resolver_is_global(resolver));
    register_static_symbol(symbol);

    resolver->is_within_constant_decl = false;

    return symbol;
}

static struct symbol const*
resolve_decl_function(struct resolver* resolver, struct cst_decl const* decl)
{
    assert(resolver != NULL);
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_FUNCTION);

    // Check for declaration of a template function.
    sbuf(struct cst_identifier const) const template_parameters =
        decl->data.function.template_parameters;
    if (sbuf_count(template_parameters) != 0) {
        struct symbol_table* const symbols =
            symbol_table_new(resolver->current_symbol_table);
        struct symbol* const template_symbol = symbol_new_template(
            decl->location,
            decl->name,
            decl,
            resolver->current_symbol_name_prefix,
            resolver->current_static_addr_prefix,
            resolver->current_symbol_table,
            symbols);

        freeze(template_symbol);
        sbuf_push(context()->chilling_symbol_tables, symbols);
        symbol_table_insert(
            resolver->current_symbol_table,
            template_symbol->name,
            template_symbol,
            false);
        return template_symbol;
    }

    sbuf(struct cst_function_parameter const* const) const function_parameters =
        decl->data.function.function_parameters;

    // Create the type corresponding to the function.
    struct type const** parameter_types = NULL;
    sbuf_resize(parameter_types, sbuf_count(function_parameters));
    for (size_t i = 0; i < sbuf_count(function_parameters); ++i) {
        parameter_types[i] =
            resolve_type(resolver, function_parameters[i]->type);
        if (parameter_types[i]->size == SIZEOF_UNSIZED) {
            fatal(
                function_parameters[i]->type->location,
                "declaration of function parameter with unsized type `%s`",
                parameter_types[i]->name);
        }
    }
    sbuf_freeze(parameter_types);

    struct type const* const return_type =
        resolve_type(resolver, decl->data.function.return_type);
    if (return_type->size == SIZEOF_UNSIZED) {
        fatal(
            decl->data.function.return_type->location,
            "declaration of function with unsized return type `%s`",
            return_type->name);
    }

    struct type const* function_type =
        type_unique_function(parameter_types, return_type);

    struct address const* const function_address =
        resolver_reserve_storage_static(resolver, decl->name);

    // Create a new incomplete function, a value that evaluates to that
    // function, and the address of that function/value.
    struct function* const function =
        function_new(function_type, function_address);
    freeze(function);

    struct value* const value = value_new_function(function);
    value_freeze(value);
    function->value = value;

    // Add the function/value to the symbol table now so that recursive
    // functions may reference themselves.
    struct symbol* const function_symbol = symbol_new_function(
        decl->location, decl->data.function.identifier.name, function);
    freeze(function_symbol);
    symbol_table_insert(
        resolver->current_symbol_table,
        function_symbol->name,
        function_symbol,
        false);
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
    sbuf(struct symbol const*) symbol_parameters = NULL;
    sbuf_resize(symbol_parameters, sbuf_count(function_parameters));
    for (size_t i = sbuf_count(function_parameters); i--;) {
        struct source_location const location =
            function_parameters[i]->location;
        char const* const name = function_parameters[i]->identifier.name;
        struct type const* const type = parameter_types[i];
        struct address* const address = address_new(address_init_local(
            intern_fmt("argument_%u_%s", i, name), rbp_offset));
        address->data.local.is_parameter = true;
        freeze(address);

        rbp_offset += (int)ceil8umax(type->size);

        struct object* const object = object_new(type, address, NULL);
        freeze(object);

        struct symbol* const symbol =
            symbol_new_variable(location, name, object);
        freeze(symbol);

        symbol_parameters[i] = symbol;
    }
    sbuf_freeze(symbol_parameters);
    function->symbol_parameters = symbol_parameters;

    // Add the function's parameters to its outermost symbol table in order
    // from left to right so that any error message about duplicate parameter
    // symbols will list the left-most symbol as the first of the two symbols
    // added to the table.
    struct symbol_table* const symbol_table =
        symbol_table_new(resolver->current_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbol_table);
    // The function references, but does not own, its outermost symbol table.
    function->symbol_table = symbol_table;
    for (size_t i = 0; i < sbuf_count(function_parameters); ++i) {
        symbol_table_insert(
            symbol_table,
            function->symbol_parameters[i]->name,
            function->symbol_parameters[i],
            false);
    }

    // Add the function's return value to its outermost symbol table.
    struct address* const return_value_address = address_new(
        address_init_local(context()->interned.return_, rbp_offset));
    freeze(return_value_address);
    struct object* const return_value_object =
        object_new(return_type, return_value_address, NULL);
    freeze(return_value_object);
    struct symbol* const return_value_symbol = symbol_new_variable(
        decl->data.function.return_type->location,
        context()->interned.return_,
        return_value_object);
    freeze(return_value_symbol);
    symbol_table_insert(
        symbol_table, return_value_symbol->name, return_value_symbol, false);
    function->symbol_return = return_value_symbol;

    struct incomplete_function* const incomplete =
        xalloc(NULL, sizeof(*incomplete));
    *incomplete = (struct incomplete_function){
        decl,
        function_symbol->name,
        function,
        symbol_table,
        context()->template_instantiation_chain};
    freeze(incomplete);
    sbuf_push(resolver->incomplete_functions, incomplete);

    return function_symbol;
}

static struct symbol const*
resolve_decl_struct(struct resolver* resolver, struct cst_decl const* decl)
{
    assert(resolver != NULL);
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_STRUCT);

    // Check for declaration of a template struct.
    sbuf(struct cst_identifier const) const template_parameters =
        decl->data.struct_.template_parameters;
    if (sbuf_count(template_parameters) != 0) {
        struct symbol_table* const symbols =
            symbol_table_new(resolver->current_symbol_table);
        struct symbol* const template_symbol = symbol_new_template(
            decl->location,
            decl->name,
            decl,
            resolver->current_symbol_name_prefix,
            resolver->current_static_addr_prefix,
            resolver->current_symbol_table,
            symbols);

        freeze(template_symbol);
        sbuf_push(context()->chilling_symbol_tables, symbols);
        symbol_table_insert(
            resolver->current_symbol_table,
            template_symbol->name,
            template_symbol,
            false);
        return template_symbol;
    }

    struct symbol_table* const struct_symbols =
        symbol_table_new(resolver->current_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, struct_symbols);
    struct type* const type = type_new_struct(
        qualified_name(resolver->current_symbol_name_prefix, decl->name),
        struct_symbols);
    freeze(type);

    struct symbol* const symbol = symbol_new_type(decl->location, type);
    freeze(symbol);

    // Add the symbol to the current symbol table so that structs with
    // self-referential pointer and slice members may reference the type.
    symbol_table_insert(
        resolver->current_symbol_table, decl->name, symbol, false);

    sbuf(struct cst_member const* const) const members =
        decl->data.struct_.members;
    size_t const members_count = sbuf_count(members);

    // Check for duplicate member definitions.
    for (size_t i = 0; i < members_count; ++i) {
        for (size_t j = i + 1; j < members_count; ++j) {
            if (members[i]->name == members[j]->name) {
                // XXX: Calling sbuf_fini here because GCC 8.3 ASan will think
                // we leak even though we hold a valid path to the buffer.
                sbuf_fini(type->data.struct_.member_variables);

                fatal(
                    members[j]->location,
                    "duplicate definition of member `%s`",
                    members[j]->name);
            }
        }
    }

    return symbol;
}

static struct symbol const*
resolve_decl_union(struct resolver* resolver, struct cst_decl const* decl)
{
    assert(resolver != NULL);
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_UNION);

    // Check for declaration of a template union.
    sbuf(struct cst_identifier const) const template_parameters =
        decl->data.union_.template_parameters;
    if (sbuf_count(template_parameters) != 0) {
        struct symbol_table* const symbols =
            symbol_table_new(resolver->current_symbol_table);
        struct symbol* const template_symbol = symbol_new_template(
            decl->location,
            decl->name,
            decl,
            resolver->current_symbol_name_prefix,
            resolver->current_static_addr_prefix,
            resolver->current_symbol_table,
            symbols);

        freeze(template_symbol);
        sbuf_push(context()->chilling_symbol_tables, symbols);
        symbol_table_insert(
            resolver->current_symbol_table,
            template_symbol->name,
            template_symbol,
            false);
        return template_symbol;
    }

    struct symbol_table* const struct_symbols =
        symbol_table_new(resolver->current_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, struct_symbols);
    struct type* const type = type_new_union(
        qualified_name(resolver->current_symbol_name_prefix, decl->name),
        struct_symbols);
    freeze(type);

    struct symbol* const symbol = symbol_new_type(decl->location, type);
    freeze(symbol);

    // Add the symbol to the current symbol table so that unions with
    // self-referential pointer and slice members may reference the type.
    symbol_table_insert(
        resolver->current_symbol_table, decl->name, symbol, false);

    sbuf(struct cst_member const* const) const members =
        decl->data.union_.members;
    size_t const members_count = sbuf_count(members);

    // Check for duplicate member definitions.
    for (size_t i = 0; i < members_count; ++i) {
        for (size_t j = i + 1; j < members_count; ++j) {
            if (members[i]->name == members[j]->name) {
                // XXX: Calling sbuf_fini here because GCC 8.3 ASan will think
                // we leak even though we hold a valid path to the buffer.
                sbuf_fini(type->data.union_.member_variables);

                fatal(
                    members[j]->location,
                    "duplicate definition of member `%s`",
                    members[j]->name);
            }
        }
    }

    return symbol;
}

static struct symbol const*
resolve_decl_enum(struct resolver* resolver, struct cst_decl const* decl)
{
    assert(resolver != NULL);
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_ENUM);

    return complete_enum(
        resolver,
        decl->location,
        decl->name,
        decl->data.enum_.values,
        decl->data.enum_.member_functions);
}

static struct symbol const*
resolve_decl_extend(struct resolver* resolver, struct cst_decl const* decl)
{
    assert(resolver != NULL);
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_EXTEND);

    if (decl->data.extend.decl->kind != CST_DECL_CONSTANT
        && decl->data.extend.decl->kind != CST_DECL_FUNCTION) {
        fatal(
            decl->location,
            "type extension declaration must be a constant or function");
    }

    struct type const* const type =
        resolve_type(resolver, decl->data.extend.type);

    // PLAN: Create the decl in a sub-symbol table of the module namespace that
    // is created specifically for this one symbol so that name collisions
    // don't happen. Then add the symbol to the type.

    // Create a symbol table for this declaration only in order to prevent name
    // collisions and hide the created symbol from the rest of the module.
    struct symbol_table* const symbol_table =
        symbol_table_new(resolver->current_symbol_table);

    char const* const save_symbol_name_prefix =
        resolver->current_symbol_name_prefix;
    char const* const save_static_addr_prefix =
        resolver->current_static_addr_prefix;
    struct symbol_table* const save_symbol_table =
        resolver->current_symbol_table;

    resolver->current_symbol_name_prefix = type->name;
    resolver->current_static_addr_prefix = normalize(type->name, 0);
    resolver->current_symbol_table = symbol_table;
    struct symbol const* const symbol =
        resolve_decl(resolver, decl->data.extend.decl);
    symbol_table_insert(type->symbols, decl->name, symbol, false);

    resolver->current_symbol_name_prefix = save_symbol_name_prefix;
    resolver->current_static_addr_prefix = save_static_addr_prefix;
    resolver->current_symbol_table = save_symbol_table;

    symbol_table_freeze(symbol_table);
    return symbol;
}

static struct symbol const*
resolve_decl_alias(struct resolver* resolver, struct cst_decl const* decl)
{
    assert(resolver != NULL);
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_ALIAS);

    struct type const* const type =
        resolve_type(resolver, decl->data.alias.type);
    struct symbol* const symbol = symbol_new_type(decl->location, type);
    freeze(symbol);
    symbol_table_insert(
        resolver->current_symbol_table,
        decl->name,
        symbol,
        !resolver_is_global(resolver));

    return symbol;
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
        resolve_type(resolver, decl->data.extern_variable.type);
    if (type->size == SIZEOF_UNSIZED) {
        fatal(
            decl->data.extern_variable.type->location,
            "declaration of extern variable with unsized type `%s`",
            type->name);
    }

    struct symbol const* const existing = symbol_table_lookup_local(
        resolver->current_symbol_table,
        decl->data.extern_variable.identifier.name);
    if (existing != NULL && existing->kind == SYMBOL_VARIABLE
        && existing->data.variable->type == type
        && existing->data.variable->is_extern) {
        // Duplicate extern variable declaration.
        return existing;
    }

    struct address const* const address =
        resolver_reserve_storage_static(resolver, decl->name);

    struct object* const object = object_new(type, address, NULL);
    object->is_extern = true;
    freeze(object);

    struct symbol* const symbol =
        symbol_new_variable(decl->location, decl->name, object);
    freeze(symbol);

    symbol_table_insert(
        resolver->current_symbol_table, symbol->name, symbol, false);
    register_static_symbol(symbol); // Extern variables are always static.

    return symbol;
}

static struct symbol const*
resolve_decl_extern_function(
    struct resolver* resolver, struct cst_decl const* decl)
{
    assert(resolver != NULL);
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_EXTERN_FUNCTION);
    assert(resolver_is_global(resolver));

    sbuf(struct cst_function_parameter const* const) const function_parameters =
        decl->data.extern_function.function_parameters;

    // Create the type corresponding to the function.
    struct type const** parameter_types = NULL;
    sbuf_resize(parameter_types, sbuf_count(function_parameters));
    for (size_t i = 0; i < sbuf_count(function_parameters); ++i) {
        parameter_types[i] =
            resolve_type(resolver, function_parameters[i]->type);
        if (parameter_types[i]->size == SIZEOF_UNSIZED) {
            fatal(
                function_parameters[i]->type->location,
                "declaration of function parameter with unsized type `%s`",
                parameter_types[i]->name);
        }
    }
    sbuf_freeze(parameter_types);

    struct type const* const return_type =
        resolve_type(resolver, decl->data.extern_function.return_type);
    if (return_type->size == SIZEOF_UNSIZED) {
        fatal(
            decl->data.extern_function.return_type->location,
            "declaration of function with unsized return type `%s`",
            return_type->name);
    }

    struct type const* function_type =
        type_unique_function(parameter_types, return_type);

    struct symbol const* const existing = symbol_table_lookup_local(
        resolver->current_symbol_table,
        decl->data.extern_function.identifier.name);
    if (existing != NULL && existing->kind == SYMBOL_FUNCTION
        && existing->data.function->type == function_type
        && existing->data.function->is_extern) {
        // Duplicate extern function declaration.
        return existing;
    }

    struct address const* const function_address =
        resolver_reserve_storage_static(resolver, decl->name);

    // Create a new incomplete function, a value that evaluates to that
    // function, and the address of that function/value.
    struct function* const function =
        function_new(function_type, function_address);
    freeze(function);

    struct value* const value = value_new_function(function);
    value_freeze(value);
    function->value = value;

    function->is_extern = true;

    struct symbol* const symbol = symbol_new_function(
        decl->location, decl->data.extern_function.identifier.name, function);
    freeze(symbol);

    symbol_table_insert(
        resolver->current_symbol_table, symbol->name, symbol, false);
    register_static_symbol(symbol); // Extern functions are always static.

    return symbol;
}

static void
complete_struct(
    struct resolver* resolver,
    struct symbol const* symbol,
    struct cst_member const* const* members)
{
    assert(resolver != NULL);
    assert(symbol != NULL);
    assert(symbol->kind == SYMBOL_TYPE);
    assert(symbol_xget_type(symbol)->kind == TYPE_STRUCT);

    size_t const members_count = sbuf_count(members);

    // XXX: Evil const cast.
    struct type* struct_type = (struct type*)symbol_xget_type(symbol);
    // XXX: Evil const cast.
    struct symbol_table* const struct_symbols =
        (struct symbol_table*)symbol_xget_type(symbol)->symbols;

    sbuf(struct member_variable) member_variables = NULL;

    // Add all member definitions to the struct in the order that they were
    // defined in.
    char const* const save_symbol_name_prefix =
        resolver->current_symbol_name_prefix;
    char const* const save_static_addr_prefix =
        resolver->current_static_addr_prefix;
    struct symbol_table* const save_symbol_table =
        resolver->current_symbol_table;

    resolver->current_symbol_name_prefix = symbol_xget_type(symbol)->name;
    resolver->current_static_addr_prefix =
        normalize(symbol_xget_type(symbol)->name, 0);
    resolver->current_symbol_table = struct_symbols;

    // If the struct contains no member variable declarations, then the struct
    // should be marked as complete since the size and alignment will always be
    // zero. Default assume that the struct is complete, then iterate through
    // the rest of the member declarations to see if the struct should actually
    // still be marked as incomplete.
    struct_type->data.struct_.is_complete = true;
    for (size_t i = 0; i < members_count; ++i) {
        struct cst_member const* const future_member = members[i];
        if (future_member->kind == CST_MEMBER_VARIABLE) {
            struct_type->data.struct_.is_complete = false;
            break;
        }
    }
    if (struct_type->data.struct_.is_complete) {
        sbuf_push(context()->types, struct_type);
    }

    // Offset of the next member variable that would be added to this struct.
    // Updated every time a member variable is added to the struct.
    //
    //      # size == 0
    //      # next_offset == 0
    //      struct foo { }
    //
    //      # size == 2
    //      # next_offset == 2 (after adding x)
    //      struct foo {
    //          var x: u16 # bytes 0->1
    //      }
    //
    //      # size == 4
    //      # next_offset == 3 (after adding y)
    //      struct foo {
    //          var x: u16 # bytes 0->1
    //          var y: u8  # byte  2
    //                     # byte  3 (stride padding)
    //      }
    //
    //      # size == 16
    //      # next_offset == 16 (after adding z)
    //      struct foo {
    //          var x: u16 # bytes 0->1
    //          var y: u8  # byte  2
    //                     # bytes 3->7 (padding)
    //          var z: u64 # bytes 8->15
    //      }
    uintmax_t next_offset = 0;
    for (size_t i = 0; i < members_count; ++i) {
        struct cst_member const* const member = members[i];
        switch (member->kind) {
        case CST_MEMBER_VARIABLE: {
            char const* const member_name = member->name;
            struct type const* const member_type =
                resolve_type(resolver, member->data.variable.type);
            if (member_type->kind == TYPE_STRUCT
                && !member_type->data.struct_.is_complete) {
                fatal(
                    member->location,
                    "struct `%s` contains a member variable of incomplete struct type `%s`",
                    struct_type->name,
                    member_type->name);
            }
            if (member_type->kind == TYPE_UNION
                && !member_type->data.union_.is_complete) {
                fatal(
                    member->location,
                    "struct `%s` contains a member variable of incomplete union type `%s`",
                    struct_type->name,
                    member_type->name);
            }
            if (member_type->size == SIZEOF_UNSIZED) {
                fatal(
                    member->location,
                    "struct `%s` contains a member variable of unsized type `%s`",
                    struct_type->name,
                    member_type->name);
            }

            // Increase the offset into the struct until the start of the added
            // member variable is aligned to a valid byte boundary. The offset
            // is increased, even when a zero-sized member is being added in
            // order to preserve the property that (i)th member of a struct
            // will always have an address that is less than or equal to the
            // (i+1)th member of a struct.
            //
            // TODO: Ensure that this behavior matches the SystemV ABI for
            // structs with flexible array members (declared using an array of
            // size zero in Sunder) so that the size and alignment of structs
            // with flexible array members is consistent between C and Sunder.
            if (member_type->align != 0) {
                while (next_offset % member_type->align != 0) {
                    next_offset += 1;
                }
            }

            // Member variables with size zero are part of the struct, but do
            // not contribute to the size or alignment of the struct.
            if (member_type->size == 0) {
                struct member_variable const m = {
                    .name = member_name,
                    .type = member_type,
                    .offset = next_offset,
                };
                sbuf_push(member_variables, m);
                goto done_adding_member_variable;
            }

            assert(member_type->size != 0);
            assert(member_type->align != 0);

            // Push the added member variable onto the back of the struct's
            // list of members (ordered by offset).
            struct member_variable const m = {
                .name = member_name,
                .type = member_type,
                .offset = next_offset,
            };
            sbuf_push(member_variables, m);

            // Adjust the struct alignment to match the greatest alignment of
            // the struct's member variables.
            if (member_type->align >= struct_type->align) {
                struct_type->align = member_type->align;
            }

            // Adjust the struct size to fit all members plus array stride
            // padding.
            struct_type->size = next_offset + member_type->size;
            assert(struct_type->align != 0);
            while (struct_type->size % struct_type->align != 0) {
                struct_type->size += 1;
            }

            // Future member variables will search for a valid offset starting
            // at one byte past the added member variable.
            next_offset += member_type->size;

        done_adding_member_variable:;
            // If this is the last member variable declaration within the
            // struct, then the struct's final size and alignment are known,
            // so the struct can be marked as complete. Default assume that the
            // struct is complete, then iterate through the rest of the member
            // declarations to see if the struct should actually still be
            // marked as incomplete.
            struct_type->data.struct_.is_complete = true;
            for (size_t j = i + 1; j < members_count; ++j) {
                struct cst_member const* const future_member = members[j];
                if (future_member->kind == CST_MEMBER_VARIABLE) {
                    struct_type->data.struct_.is_complete = false;
                    break;
                }
            }
            if (struct_type->data.struct_.is_complete) {
                sbuf_freeze(member_variables);
                struct_type->data.struct_.member_variables = member_variables;
                sbuf_push(context()->types, struct_type);
            }
            continue;
        }
        case CST_MEMBER_CONSTANT: {
            resolve_decl_constant(resolver, member->data.constant.decl);
            continue;
        }
        case CST_MEMBER_FUNCTION: {
            resolve_decl_function(resolver, member->data.function.decl);
            continue;
        }
        }

        UNREACHABLE();
    }

    resolver->current_symbol_name_prefix = save_symbol_name_prefix;
    resolver->current_static_addr_prefix = save_static_addr_prefix;
    resolver->current_symbol_table = save_symbol_table;
}

static void
complete_union(
    struct resolver* resolver,
    struct symbol const* symbol,
    struct cst_member const* const* members)
{
    assert(resolver != NULL);
    assert(symbol != NULL);
    assert(symbol->kind == SYMBOL_TYPE);
    assert(symbol_xget_type(symbol)->kind == TYPE_UNION);

    size_t const members_count = sbuf_count(members);

    // XXX: Evil const cast.
    struct type* union_type = (struct type*)symbol_xget_type(symbol);
    // XXX: Evil const cast.
    struct symbol_table* const union_symbols =
        (struct symbol_table*)symbol_xget_type(symbol)->symbols;

    sbuf(struct member_variable) member_variables = NULL;

    // Add all member definitions to the union in the order that they were
    // defined in.
    char const* const save_symbol_name_prefix =
        resolver->current_symbol_name_prefix;
    char const* const save_static_addr_prefix =
        resolver->current_static_addr_prefix;
    struct symbol_table* const save_symbol_table =
        resolver->current_symbol_table;

    resolver->current_symbol_name_prefix = symbol_xget_type(symbol)->name;
    resolver->current_static_addr_prefix =
        normalize(symbol_xget_type(symbol)->name, 0);
    resolver->current_symbol_table = union_symbols;

    // If the union contains no member variable declarations, then the union
    // should be marked as complete since the size and alignment will always be
    // zero. Default assume that the union is complete, then iterate through
    // the rest of the member declarations to see if the union should actually
    // still be marked as incomplete.
    union_type->data.union_.is_complete = true;
    for (size_t i = 0; i < members_count; ++i) {
        struct cst_member const* const future_member = members[i];
        if (future_member->kind == CST_MEMBER_VARIABLE) {
            union_type->data.union_.is_complete = false;
            break;
        }
    }
    if (union_type->data.union_.is_complete) {
        sbuf_push(context()->types, union_type);
    }

    for (size_t i = 0; i < members_count; ++i) {
        struct cst_member const* const member = members[i];
        switch (member->kind) {
        case CST_MEMBER_VARIABLE: {
            char const* const member_name = member->name;
            struct type const* const member_type =
                resolve_type(resolver, member->data.variable.type);
            if (member_type->kind == TYPE_STRUCT
                && !member_type->data.struct_.is_complete) {
                fatal(
                    member->location,
                    "union `%s` contains a member variable of incomplete struct type `%s`",
                    union_type->name,
                    member_type->name);
            }
            if (member_type->kind == TYPE_UNION
                && !member_type->data.union_.is_complete) {
                fatal(
                    member->location,
                    "union `%s` contains a member variable of incomplete union type `%s`",
                    union_type->name,
                    member_type->name);
            }
            if (member_type->size == SIZEOF_UNSIZED) {
                fatal(
                    member->location,
                    "union `%s` contains a member variable of unsized type `%s`",
                    union_type->name,
                    member_type->name);
            }

            // Member variables with size zero are part of the union, but do
            // not contribute to the size or alignment of the union.
            if (member_type->size == 0) {
                struct member_variable const m = {
                    .name = member_name,
                    .type = member_type,
                    .offset = 0,
                };
                sbuf_push(member_variables, m);
                goto done_adding_member_variable;
            }

            assert(member_type->size != 0);
            assert(member_type->align != 0);

            // Push the added member variable onto the back of the unions's
            // list of members.
            struct member_variable const m = {
                .name = member_name,
                .type = member_type,
                .offset = 0,
            };
            sbuf_push(member_variables, m);

            // Adjust the union size and alignment to match the greatest size
            // and alignment of the unions's member variables.
            if (member_type->size >= union_type->size) {
                union_type->size = member_type->size;
            }
            if (member_type->align >= union_type->align) {
                union_type->align = member_type->align;
            }

        done_adding_member_variable:;

            // If this is the last member variable declaration within the
            // union, then the unions's final size and alignment are known, so
            // the union can be marked as complete. Default assume that the
            // union is complete, then iterate through the rest of the member
            // declarations to see if the union should actually still be marked
            // as incomplete.
            union_type->data.union_.is_complete = true;
            for (size_t j = i + 1; j < members_count; ++j) {
                struct cst_member const* const future_member = members[j];
                if (future_member->kind == CST_MEMBER_VARIABLE) {
                    union_type->data.union_.is_complete = false;
                    break;
                }
            }
            if (union_type->data.union_.is_complete) {
                sbuf_freeze(member_variables);
                union_type->data.union_.member_variables = member_variables;
                sbuf_push(context()->types, union_type);
            }
            continue;
        }
        case CST_MEMBER_CONSTANT: {
            resolve_decl_constant(resolver, member->data.constant.decl);
            continue;
        }
        case CST_MEMBER_FUNCTION: {
            resolve_decl_function(resolver, member->data.function.decl);
            continue;
        }
        }

        UNREACHABLE();
    }

    resolver->current_symbol_name_prefix = save_symbol_name_prefix;
    resolver->current_static_addr_prefix = save_static_addr_prefix;
    resolver->current_symbol_table = save_symbol_table;
}

struct symbol const*
complete_enum(
    struct resolver* resolver,
    struct source_location location,
    char const* name,
    struct cst_enum_value const* const* values,
    struct cst_member const* const* member_functions)
{
    assert(resolver != NULL);
    assert(name != NULL);

    bool const is_anonymous = cstr_starts_with(name, "enum ");

    struct symbol_table* const enum_symbols =
        symbol_table_new(resolver->current_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, enum_symbols);
    struct type* const type = type_new_enum(
        qualified_name(resolver->current_symbol_name_prefix, name),
        enum_symbols);
    freeze(type);

    struct symbol* const symbol = symbol_new_type(location, type);
    freeze(symbol);

    size_t const values_count = sbuf_count(values);

    // Check for duplicate enumerator definitions.
    for (size_t i = 0; i < values_count; ++i) {
        for (size_t j = i + 1; j < values_count; ++j) {
            if (values[i]->identifier.name == values[j]->identifier.name) {
                // XXX: Calling sbuf_fini here because GCC 8.3 ASan will think
                // we leak even though we hold a valid path to the buffer.
                sbuf_fini(type->data.union_.member_variables);

                fatal(
                    values[j]->location,
                    "duplicate definition of enum value `%s`",
                    values[j]->identifier.name);
            }
        }
    }

    // Evaluate enumerator constants.
    struct bigint* previous = bigint_new(BIGINT_NEG_ONE);
    sbuf(struct bigint*) integers = NULL;
    for (size_t i = 0; i < values_count; ++i) {
        if (values[i]->expr != NULL) {
            struct expr const* const resolved =
                resolve_expr(resolver, values[i]->expr);
            struct value* const rvalue = eval_rvalue(resolved);
            value_freeze(rvalue);
            if (!type_is_integer(rvalue->type)) {
                fatal(
                    values[i]->location,
                    "enum value with type `%s` is not an integer",
                    rvalue->type->name);
            }
            struct bigint* integer = bigint_new(rvalue->data.integer);
            sbuf_push(integers, integer);
            bigint_assign(previous, integer);
        }
        else {
            struct bigint* integer = bigint_new(previous);
            bigint_add(integer, integer, BIGINT_POS_ONE);
            sbuf_push(integers, integer);
            bigint_assign(previous, integer);
        }
    }
    sbuf_freeze(integers);
    bigint_del(previous);

    // Validate enumerator constants are within range of the underlying type.
    struct type const* const underlying = type->data.enum_.underlying_type;
    assert(type_is_integer(underlying));
    struct bigint const* const min = underlying->data.integer.min;
    assert(min != NULL);
    struct bigint const* const max = underlying->data.integer.max;
    assert(max != NULL);
    for (size_t i = 0; i < values_count; ++i) {
        if (bigint_cmp(integers[i], min) < 0) {
            char* const int_cstr = bigint_to_new_cstr(integers[i]);
            char* const min_cstr = bigint_to_new_cstr(min);
            fatal(
                values[i]->location,
                "out-of-range enum value for underlying type `%s` (%s < %s)",
                underlying->name,
                int_cstr,
                min_cstr);
        }
        if (bigint_cmp(integers[i], max) > 0) {
            char* const int_cstr = bigint_to_new_cstr(integers[i]);
            char* const max_cstr = bigint_to_new_cstr(max);
            fatal(
                values[i]->location,
                "out-of-range enum value for underlying type `%s` (%s > %s)",
                underlying->name,
                int_cstr,
                max_cstr);
        }
    }

    // Add enumerator constants to the symbol table.
    for (size_t i = 0; i < values_count; ++i) {
        struct address const* const address = resolver_reserve_storage_static(
            resolver, values[i]->identifier.name);

        struct value* const value = value_new_integer(underlying, integers[i]);
        value_freeze(value);

        struct object* const object = object_new(type, address, value);
        freeze(object);

        struct symbol* const value_symbol = symbol_new_constant(
            values[i]->location, values[i]->identifier.name, object);
        freeze(value_symbol);

        // Anonymous enums have their symbols added to the enclosing scope to
        // replicate the constants introduced by C enums, and to allow for easy
        // creation of tagged unions.
        if (is_anonymous) {
            symbol_table_insert(
                resolver->current_symbol_table,
                value_symbol->name,
                value_symbol,
                false);
            // If an anonymous enum is created at un-namespaced global scope,
            // then each enum value must be manually added to the module export
            // table, since the top-level `resolve` function call will only add
            // the symbol produced by `resolve_decl` to that symbol table.
            bool const un_namespaced_global_scope =
                resolver->module->cst->namespace == NULL
                && resolver_is_global(resolver);
            if (un_namespaced_global_scope) {
                symbol_table_insert(
                    resolver->current_export_table,
                    value_symbol->name,
                    value_symbol,
                    false);
            }
        }
        symbol_table_insert(
            enum_symbols, values[i]->identifier.name, value_symbol, false);
        sbuf_push(type->data.enum_.value_symbols, value_symbol);
        register_static_symbol(value_symbol);
    }
    sbuf_freeze(type->data.enum_.value_symbols);

    // Add the symbol to the current symbol table after all enumerators have
    // been added. We explicitly do *not* allow for self referential
    // enumerations, since the underlying type of a C enumeration may be chosen
    // *after* all enumeration constants have been defined.
    symbol_table_insert(resolver->current_symbol_table, name, symbol, false);
    sbuf_push(context()->types, type);

    // Resolve member functions.
    char const* const save_symbol_name_prefix =
        resolver->current_symbol_name_prefix;
    char const* const save_static_addr_prefix =
        resolver->current_static_addr_prefix;
    struct symbol_table* const save_symbol_table =
        resolver->current_symbol_table;
    resolver->current_symbol_name_prefix = symbol_xget_type(symbol)->name;
    resolver->current_static_addr_prefix =
        normalize(symbol_xget_type(symbol)->name, 0);
    resolver->current_symbol_table = enum_symbols;
    for (size_t i = 0; i < sbuf_count(member_functions); ++i) {
        assert(member_functions[i]->kind == CST_MEMBER_FUNCTION);
        resolve_decl_function(
            resolver, member_functions[i]->data.function.decl);
    }
    resolver->current_symbol_name_prefix = save_symbol_name_prefix;
    resolver->current_static_addr_prefix = save_static_addr_prefix;
    resolver->current_symbol_table = save_symbol_table;

    return symbol;
}

static void
complete_function(
    struct resolver* resolver, struct incomplete_function const* incomplete)
{
    assert(resolver != NULL);
    assert(incomplete != NULL);

    struct template_instantiation_link const* const save_chain =
        context()->template_instantiation_chain;
    context()->template_instantiation_chain = incomplete->chain;

    struct function* const function = incomplete->function;

    // Complete the function.
    assert(resolver->current_function == NULL);
    assert(resolver->current_local_counter == 0);
    assert(resolver->current_rbp_offset == 0x0);
    assert(!resolver->is_within_loop);
    char const* const save_symbol_name_prefix =
        resolver->current_symbol_name_prefix;
    char const* const save_static_addr_prefix =
        resolver->current_static_addr_prefix;

    resolver->current_symbol_name_prefix = incomplete->name;
    resolver->current_static_addr_prefix = function->address->data.static_.name;
    resolver->current_function = function;
    function->body = resolve_block(
        resolver,
        incomplete->symbol_table,
        &incomplete->decl->data.function.body);

    resolver->current_symbol_name_prefix = save_symbol_name_prefix;
    resolver->current_static_addr_prefix = save_static_addr_prefix;
    resolver->current_function = NULL;
    resolver->current_local_counter = 0;
    assert(resolver->current_rbp_offset == 0x0);

    // Produce an error if the last statement of a non-void returning function
    // is *not* a return statement. Even if the last statement is an if-else
    // block with a return in each arm of the statement we should still produce
    // an error as idiomatic Sunder code should use
    //
    //      if condition {
    //          return early_return_value;
    //      }
    //      return other_return_value;
    //
    // instead of
    //
    //      if condition {
    //          return first_return_value;
    //      }
    //      else {
    //          return other_return_value;
    //      }
    assert(function->type->kind == TYPE_FUNCTION);
    bool func_has_void_return =
        function->type->data.function.return_type->kind == TYPE_VOID;
    sbuf(struct stmt const* const) const stmts = function->body.stmts;
    bool const non_void_returning_func_does_not_end_with_return =
        !func_has_void_return
        && (sbuf_count(stmts) == 0
            || stmts[sbuf_count(stmts) - 1]->kind != STMT_RETURN);
    if (non_void_returning_func_does_not_end_with_return) {
        fatal(
            incomplete->decl->location,
            "Non-void-returning function does not end with a return statement");
    }

    context()->template_instantiation_chain = save_chain;
}

static struct block
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
    struct stmt const* save_current_defer = resolver->current_defer;

    sbuf(struct stmt const*) stmts = NULL;
    for (size_t i = 0; i < sbuf_count(block->stmts); ++i) {
        struct stmt const* const resolved_stmt =
            resolve_stmt(resolver, block->stmts[i]);
        if (resolved_stmt != NULL) {
            sbuf_push(stmts, resolved_stmt);
        }
    }
    sbuf_freeze(stmts);

    struct block const resolved = block_init(
        block->location,
        symbol_table,
        stmts,
        resolver->current_defer,
        save_current_defer);

    // Produce a warning if any local symbol is unused.
    for (size_t i = 0; i < sbuf_count(symbol_table->elements); ++i) {
        if (symbol_table->elements[i].name == context()->interned.return_) {
            // Ignore the `return` symbol since it is inaccessible.
            continue;
        }
        if (cstr_starts_with(symbol_table->elements[i].name, "__")) {
            // Ignore any symbol starting with two underscores. This covers the
            // case of compiler-generated symbols such as `__bytes`.
            continue;
        }
        if (cstr_ends_with(symbol_table->elements[i].name, "_")) {
            // Ignore any symbol ending with an underscore. This covers the
            // case of purposefully unused symbols such as `_` or `somevar_`.
            continue;
        }
        if (symbol_table->elements[i].symbol->uses == 0) {
            warning(
                symbol_table->elements[i].symbol->location,
                "unused %s `%s`",
                symbol_kind_to_cstr(symbol_table->elements[i].symbol->kind),
                symbol_table->elements[i].name);
            info(
                NO_LOCATION,
                "use %s `%s` in an expression or rename the %s to `%s_`",
                symbol_kind_to_cstr(symbol_table->elements[i].symbol->kind),
                symbol_table->elements[i].name,
                symbol_kind_to_cstr(symbol_table->elements[i].symbol->kind),
                symbol_table->elements[i].name);
        }
    }

    resolver->current_symbol_table = save_symbol_table;
    resolver->current_rbp_offset = save_rbp_offset;
    resolver->current_defer = save_current_defer;
    return resolved;
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
    case CST_STMT_DEFER_BLOCK: {
        return resolve_stmt_defer_block(resolver, stmt);
    }
    case CST_STMT_DEFER_EXPR: {
        return resolve_stmt_defer_expr(resolver, stmt);
    }
    case CST_STMT_IF: {
        return resolve_stmt_if(resolver, stmt);
    }
    case CST_STMT_WHEN: {
        return resolve_stmt_when(resolver, stmt);
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
    case CST_STMT_SWITCH: {
        return resolve_stmt_switch(resolver, stmt);
    }
    case CST_STMT_RETURN: {
        return resolve_stmt_return(resolver, stmt);
    }
    case CST_STMT_ASSERT: {
        return resolve_stmt_assert(resolver, stmt);
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

        if (decl->data.variable.expr != NULL) {
            struct stmt* const resolved =
                stmt_new_assign(stmt->location, lhs, rhs);

            freeze(resolved);
            return resolved;
        }

        return NULL;
    }
    case CST_DECL_CONSTANT: {
        resolve_decl_constant(resolver, decl);
        return NULL;
    }
    case CST_DECL_FUNCTION: {
        fatal(stmt->location, "nested function declaration");
        return NULL;
    }
    case CST_DECL_STRUCT: {
        fatal(decl->location, "local declaration of struct `%s`", decl->name);
        return NULL;
    }
    case CST_DECL_UNION: {
        fatal(decl->location, "local declaration of union `%s`", decl->name);
        return NULL;
    }
    case CST_DECL_ENUM: {
        fatal(decl->location, "local declaration of enum `%s`", decl->name);
        return NULL;
    }
    case CST_DECL_EXTEND: {
        fatal(
            decl->location,
            "local declaration of type extension `%s`",
            decl->name);
        return NULL;
    }
    case CST_DECL_ALIAS: {
        resolve_decl_alias(resolver, stmt->data.decl);
        return NULL;
    }
    case CST_DECL_EXTERN_VARIABLE: {
        fatal(
            decl->location,
            "local declaration of extern variable `%s`",
            decl->name);
        return NULL;
    }
    case CST_DECL_EXTERN_FUNCTION: {
        fatal(
            decl->location,
            "local declaration of extern function `%s`",
            decl->name);
        return NULL;
    }
    }

    UNREACHABLE();
    return NULL;
}

static struct stmt const*
resolve_stmt_defer_block(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_DEFER_BLOCK);

    struct symbol_table* const symbol_table =
        symbol_table_new(resolver->current_symbol_table);
    struct block const body =
        resolve_block(resolver, symbol_table, &stmt->data.defer_block);
    symbol_table_freeze(symbol_table);

    struct stmt* const resolved =
        stmt_new_defer(stmt->location, resolver->current_defer, body);
    resolver->current_defer = resolved;

    freeze(resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_defer_expr(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_DEFER_EXPR);

    // Implicitly create a block for the deferred expression.
    struct expr const* const expr =
        resolve_expr(resolver, stmt->data.defer_expr);
    struct stmt* const expr_stmt = stmt_new_expr(expr->location, expr);
    freeze(expr_stmt);
    sbuf(struct stmt const*) stmts = NULL;
    sbuf_push(stmts, expr_stmt);
    sbuf_freeze(stmts);
    struct symbol_table* const symbol_table =
        symbol_table_new(resolver->current_symbol_table);
    // Freeze the symbol table immediately since no symbols will be added.
    symbol_table_freeze(symbol_table);
    struct block const block = block_init(
        stmt->location,
        symbol_table,
        stmts,
        resolver->current_defer,
        resolver->current_defer);

    // Use the implicitly created block as the defer block.
    struct stmt* const resolved =
        stmt_new_defer(stmt->location, resolver->current_defer, block);
    resolver->current_defer = resolved;

    freeze(resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_if(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_IF);

    sbuf(struct cst_conditional const) const conditionals =
        stmt->data.if_.conditionals;
    sbuf(struct conditional) resolved_conditionals = NULL;
    sbuf_resize(resolved_conditionals, sbuf_count(conditionals));
    for (size_t i = 0; i < sbuf_count(conditionals); ++i) {
        assert(
            (conditionals[i].condition != NULL)
            || (i == (sbuf_count(conditionals) - 1)));

        struct expr const* condition = NULL;
        if (conditionals[i].condition != NULL) {
            condition = resolve_expr(resolver, conditionals[i].condition);
            if (condition->type->kind != TYPE_BOOL) {
                fatal(
                    condition->location,
                    "illegal condition with non-boolean type `%s`",
                    condition->type->name);
            }
        }

        struct symbol_table* const symbol_table =
            symbol_table_new(resolver->current_symbol_table);
        struct block const block =
            resolve_block(resolver, symbol_table, &conditionals[i].body);
        // Freeze the symbol table now that the block has been resolved and no
        // new symbols will be added.
        symbol_table_freeze(symbol_table);

        resolved_conditionals[i] =
            conditional_init(conditionals[i].location, condition, block);
    }

    sbuf_freeze(resolved_conditionals);
    struct stmt* const resolved = stmt_new_if(resolved_conditionals);

    freeze(resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_when(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_WHEN);

    struct block block = block_init(
        stmt->location, resolver->current_symbol_table, NULL, NULL, NULL);
    struct conditional conditional =
        conditional_init(stmt->location, NULL, block);

    sbuf(struct cst_conditional const) const conditionals =
        stmt->data.when.conditionals;
    for (size_t i = 0; i < sbuf_count(conditionals); ++i) {
        struct expr const* condition = NULL;
        if (conditionals[i].condition != NULL) {
            condition = resolve_expr(resolver, conditionals[i].condition);
            if (condition->type->kind != TYPE_BOOL) {
                fatal(
                    condition->location,
                    "illegal condition with non-boolean type `%s`",
                    condition->type->name);
            }
        }

        struct value* value = NULL;
        if (condition != NULL) {
            value = eval_rvalue(condition);
        }
        assert(value == NULL || value->type->kind == TYPE_BOOL);
        if (value == NULL || value->data.boolean) {
            block = resolve_block(
                resolver,
                resolver->current_symbol_table,
                &conditionals[i].body);
            conditional =
                conditional_init(conditionals[i].location, condition, block);
            break;
        }
    }

    struct stmt* const resolved = stmt_new_when(conditional);

    freeze(resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_for_range(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_FOR_RANGE);

    struct type const* loop_var_type = context()->builtin.usize;
    if (stmt->data.for_range.type != NULL) {
        loop_var_type = resolve_type(resolver, stmt->data.for_range.type);
        if (!type_is_uinteger(loop_var_type)
            && !type_is_sinteger(loop_var_type)) {
            fatal(
                stmt->data.for_range.type->location,
                "illegal for-range index type `%s`",
                loop_var_type->name);
        }
    }

    struct expr const* begin = NULL;
    if (stmt->data.for_range.begin != NULL) {
        begin = resolve_expr(resolver, stmt->data.for_range.begin);
        begin = implicit_cast(loop_var_type, begin);
    }
    else {
        struct value* const value =
            value_new_integer(loop_var_type, bigint_new(BIGINT_ZERO));
        value_freeze(value);

        struct expr* const zero = expr_new_value(stmt->location, value);
        freeze(zero);

        begin = zero;
    }
    verify_type_compatibility(begin->location, begin->type, loop_var_type);

    struct expr const* end = resolve_expr(resolver, stmt->data.for_range.end);
    end = implicit_cast(loop_var_type, end);
    verify_type_compatibility(end->location, end->type, loop_var_type);

    int const save_rbp_offset = resolver->current_rbp_offset;
    struct source_location const loop_var_location =
        stmt->data.for_range.identifier.location;
    char const* const loop_var_name = stmt->data.for_range.identifier.name;
    struct address const* const loop_var_address =
        resolver_reserve_storage_local(resolver, loop_var_name, loop_var_type);
    struct object* loop_var_object =
        object_new(loop_var_type, loop_var_address, NULL);
    freeze(loop_var_object);
    struct symbol* const loop_var_symbol =
        symbol_new_variable(loop_var_location, loop_var_name, loop_var_object);
    freeze(loop_var_symbol);

    struct symbol_table* const symbol_table =
        symbol_table_new(resolver->current_symbol_table);
    symbol_table_insert(
        symbol_table, loop_var_symbol->name, loop_var_symbol, false);

    bool const save_is_within_loop = resolver->is_within_loop;
    struct stmt const* const save_current_loop_defer =
        resolver->current_loop_defer;

    resolver->is_within_loop = true;
    resolver->current_loop_defer = resolver->current_defer;
    struct block const body =
        resolve_block(resolver, symbol_table, &stmt->data.for_range.body);
    resolver->current_rbp_offset = save_rbp_offset;
    resolver->is_within_loop = save_is_within_loop;
    resolver->current_loop_defer = save_current_loop_defer;

    // Freeze the symbol table now that the block has been resolved and no new
    // symbols will be added.
    symbol_table_freeze(symbol_table);

    struct stmt* const resolved =
        stmt_new_for_range(stmt->location, loop_var_symbol, begin, end, body);
    freeze(resolved);
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
    struct stmt const* const save_current_loop_defer =
        resolver->current_loop_defer;

    resolver->is_within_loop = true;
    resolver->current_loop_defer = resolver->current_defer;
    struct block const body =
        resolve_block(resolver, symbol_table, &stmt->data.for_expr.body);
    resolver->is_within_loop = save_is_within_loop;
    resolver->current_loop_defer = save_current_loop_defer;

    // Freeze the symbol table now that the block has been resolved and no new
    // symbols will be added.
    symbol_table_freeze(symbol_table);

    struct stmt* const resolved = stmt_new_for_expr(stmt->location, expr, body);

    freeze(resolved);
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

    struct stmt* const resolved = stmt_new_break(
        stmt->location, resolver->current_defer, resolver->current_loop_defer);

    freeze(resolved);
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

    struct stmt* const resolved = stmt_new_continue(
        stmt->location, resolver->current_defer, resolver->current_loop_defer);

    freeze(resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_switch(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_SWITCH);

    struct expr const* const expr =
        resolve_expr(resolver, stmt->data.switch_.expr);

    struct type const* const type = expr->type;
    if (type->kind != TYPE_ENUM) {
        fatal(expr->location, "expected enum type (received `%s`)", type->name);
    }
    sbuf(struct symbol const* const) const value_symbols =
        type->data.enum_.value_symbols;

    sbuf(struct switch_case) cases = NULL;
    bool contains_else = false;
    for (size_t i = 0; i < sbuf_count(stmt->data.switch_.cases); ++i) {
        struct symbol_table* const symbol_table =
            symbol_table_new(resolver->current_symbol_table);
        struct block const block = resolve_block(
            resolver, symbol_table, &stmt->data.switch_.cases[i].block);
        // Freeze the symbol table now that the block has been resolved and no
        // new symbols will be added.
        symbol_table_freeze(symbol_table);

        sbuf(struct cst_symbol const* const) const symbols =
            stmt->data.switch_.cases[i].symbols;
        if (sbuf_count(symbols) != 0) {
            for (size_t j = 0; j < sbuf_count(symbols); ++j) {
                struct symbol const* const symbol =
                    xget_symbol(resolver, symbols[j]);
                if (symbol_xget_type(symbol) != type) {
                    fatal(
                        symbols[j]->location,
                        "expected case symbol with enum type `%s` (received symbol of type `%s`)",
                        type->name,
                        symbol_xget_type(symbol)->name);
                }
                bool is_enum_value_symbol = false;
                for (size_t k = 0; k < sbuf_count(value_symbols); ++k) {
                    if (value_symbols[k] == symbol) {
                        is_enum_value_symbol = true;
                        break;
                    }
                }
                if (!is_enum_value_symbol) {
                    fatal(
                        symbols[j]->location,
                        "case symbol `%s` does not correspond to a declared value of enum type `%s`",
                        symbol->name,
                        type->name);
                }
                struct switch_case const case_ = (struct switch_case){
                    .symbol = symbol,
                    .body = block,
                };
                sbuf_push(cases, case_);
            }
        }
        else {
            // The else case must come last.
            assert(i == sbuf_count(stmt->data.switch_.cases) - 1);
            struct switch_case const case_ = (struct switch_case){
                .symbol = NULL,
                .body = block,
            };
            sbuf_push(cases, case_);
            contains_else = true;
        }
    }
    sbuf_freeze(cases);

    if (!contains_else) {
        // Emit a warning for each unhandled enum value.
        for (size_t i = 0; i < sbuf_count(value_symbols); ++i) {
            bool found = false;
            for (size_t j = 0; j < sbuf_count(cases); ++j) {
                if (value_symbols[i] == cases[j].symbol) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                warning(
                    stmt->location,
                    "value `%s` of enum `%s` is not handled in switch",
                    value_symbols[i]->name,
                    type->name);
            }
        }
    }

    struct stmt* const resolved = stmt_new_switch(stmt->location, expr, cases);

    freeze(resolved);
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
        expr = implicit_cast(return_type, expr);
        verify_type_compatibility(expr->location, expr->type, return_type);
    }
    else {
        if (context()->builtin.void_ != return_type) {
            fatal(
                stmt->location,
                "illegal return statement in function with non-void return type");
        }
    }

    struct stmt* const resolved =
        stmt_new_return(stmt->location, expr, resolver->current_defer);

    freeze(resolved);
    return resolved;
}

static struct stmt const*
resolve_stmt_assert(struct resolver* resolver, struct cst_stmt const* stmt)
{
    assert(resolver != NULL);
    assert(!resolver_is_global(resolver));
    assert(stmt != NULL);
    assert(stmt->kind == CST_STMT_ASSERT);

    struct expr const* const expr =
        resolve_expr(resolver, stmt->data.assert_.expr);
    if (expr->type->kind != TYPE_BOOL) {
        fatal(
            expr->location,
            "assert with non-boolean type `%s`",
            expr->type->name);
    }

    // Assert statement location must have a valid path and line.
    assert(stmt->location.path != NO_PATH);
    assert(stmt->location.line != NO_LINE);
    assert(stmt->location.psrc != NO_PSRC);
    char const* const line_start = source_line_start(stmt->location.psrc);
    char const* const line_end = source_line_end(stmt->location.psrc);
    struct string* const bytes = string_new_fmt(
        "[%s:%zu] assertion failure\n%.*s\n",
        stmt->location.path,
        stmt->location.line,
        (int)(line_end - line_start),
        line_start);
    char const* const bytes_start = string_start(bytes);
    size_t const bytes_count = string_count(bytes);

    struct symbol const* array_symbol = NULL;
    struct symbol const* slice_symbol = NULL;
    create_static_bytes(
        resolver,
        expr->location,
        bytes_start,
        bytes_count,
        &array_symbol,
        &slice_symbol);

    string_del(bytes);

    struct stmt* const resolved =
        stmt_new_assert(expr->location, expr, array_symbol, slice_symbol);

    freeze(resolved);
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

    if (!expr_is_lvalue(lhs)) {
        fatal(
            lhs->location,
            "left hand side of assignment statement is not an lvalue");
    }
    // Bytes literals are considered lvalues, and it is perfectly valid to
    // take their address, but assigning to one *should* crash the program.
    bool const lhs_is_bytes = lhs->kind == EXPR_BYTES;
    bool const lhs_is_constant_symbol =
        lhs->kind == EXPR_SYMBOL && lhs->data.symbol->kind == SYMBOL_CONSTANT;
    if (lhs_is_bytes || lhs_is_constant_symbol) {
        warning(
            lhs->location,
            "left hand side of assignment statement is a constant");
    }

    rhs = implicit_cast(lhs->type, rhs);
    verify_type_compatibility(stmt->location, rhs->type, lhs->type);

    struct stmt* const resolved = stmt_new_assign(stmt->location, lhs, rhs);

    freeze(resolved);
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

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);

    switch (expr->kind) {
    case CST_EXPR_SYMBOL: {
        return resolve_expr_symbol(resolver, expr);
    }
    case CST_EXPR_BOOLEAN: {
        return resolve_expr_boolean(resolver, expr);
    }
    case CST_EXPR_INTEGER: {
        return resolve_expr_integer(resolver, expr);
    }
    case CST_EXPR_IEEE754: {
        return resolve_expr_ieee754(resolver, expr);
    }
    case CST_EXPR_CHARACTER: {
        return resolve_expr_character(resolver, expr);
    }
    case CST_EXPR_BYTES: {
        return resolve_expr_bytes(resolver, expr);
    }
    case CST_EXPR_LIST: {
        return resolve_expr_list(resolver, expr);
    }
    case CST_EXPR_SLICE: {
        return resolve_expr_slice(resolver, expr);
    }
    case CST_EXPR_INIT: {
        return resolve_expr_init(resolver, expr);
    }
    case CST_EXPR_CAST: {
        return resolve_expr_cast(resolver, expr);
    }
    case CST_EXPR_GROUPED: {
        return resolve_expr(resolver, expr->data.grouped.expr);
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
    case CST_EXPR_ACCESS_MEMBER: {
        return resolve_expr_access_member(resolver, expr);
    }
    case CST_EXPR_ACCESS_DEREFERENCE: {
        return resolve_expr_access_dereference(resolver, expr);
    }
    case CST_EXPR_DEFINED: {
        return resolve_expr_defined(resolver, expr);
    }
    case CST_EXPR_SIZEOF: {
        return resolve_expr_sizeof(resolver, expr);
    }
    case CST_EXPR_ALIGNOF: {
        return resolve_expr_alignof(resolver, expr);
    }
    case CST_EXPR_FILEOF: {
        return resolve_expr_fileof(resolver, expr);
    }
    case CST_EXPR_LINEOF: {
        return resolve_expr_lineof(resolver, expr);
    }
    case CST_EXPR_EMBED: {
        return resolve_expr_embed(resolver, expr);
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
resolve_expr_symbol(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_SYMBOL);

    struct symbol const* const symbol =
        xget_symbol(resolver, expr->data.symbol);
    switch (symbol->kind) {
    case SYMBOL_TYPE: {
        fatal(
            expr->location, "use of type `%s` as an expression", symbol->name);
    }
    case SYMBOL_TEMPLATE: {
        fatal(
            expr->location,
            "use of template `%s` as an expression",
            symbol->name);
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

    struct expr* const resolved = expr_new_symbol(expr->location, symbol);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_boolean(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_BOOLEAN);
    (void)resolver;

    struct token const token = expr->data.boolean;
    struct value* const value = value_new_boolean(token.kind == TOKEN_TRUE);
    value_freeze(value);

    struct expr* const resolved = expr_new_value(expr->location, value);

    freeze(resolved);
    return resolved;
}

static struct type const*
integer_literal_suffix_to_type(
    struct source_location location, char const* /* interned */ suffix)
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

    struct token const token = expr->data.integer;

    struct bigint const* const integer = token.data.integer.value;

    struct type const* const type = integer_literal_suffix_to_type(
        expr->location, token.data.integer.suffix);

    verify_byte_or_int_in_range(expr->location, type, integer);

    if (type->kind == TYPE_BYTE) {
        uint8_t byte = 0;
        if (bigint_to_u8(&byte, integer)) {
            UNREACHABLE();
        }
        struct value* const value = value_new_byte(byte);
        value_freeze(value);

        struct expr* const resolved = expr_new_value(expr->location, value);

        freeze(resolved);
        return resolved;
    }

    struct value* const value = value_new_integer(type, bigint_new(integer));
    value_freeze(value);

    struct expr* const resolved = expr_new_value(expr->location, value);

    freeze(resolved);
    return resolved;
}

static struct type const*
ieee754_literal_suffix_to_type(
    struct source_location location, char const* /* interned */ suffix)
{
    assert(suffix != NULL);

    if (suffix == context()->interned.empty) {
        return context()->builtin.real;
    }
    if (suffix == context()->interned.f32) {
        return context()->builtin.f32;
    }
    if (suffix == context()->interned.f64) {
        return context()->builtin.f64;
    }

    fatal(location, "unknown floating point literal suffix `%s`", suffix);
    return NULL;
}

static struct expr const*
resolve_expr_ieee754(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_IEEE754);
    (void)resolver;

    struct token const token = expr->data.ieee754;

    double const ieee754 = token.data.ieee754.value;

    struct type const* const type = ieee754_literal_suffix_to_type(
        expr->location, token.data.ieee754.suffix);

    assert(type_is_ieee754(type));
    struct value* value = NULL;
    if (type->kind == TYPE_F32) {
        value = value_new_f32((float)ieee754);
        value_freeze(value);
    }
    if (type->kind == TYPE_F64) {
        value = value_new_f64(ieee754);
        value_freeze(value);
    }
    if (type->kind == TYPE_REAL) {
        value = value_new_real(ieee754);
        value_freeze(value);
    }

    assert(value != NULL);
    struct expr* const resolved = expr_new_value(expr->location, value);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_character(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_CHARACTER);
    (void)resolver;

    struct type const* const type = context()->builtin.integer;

    int character = expr->data.character.data.character;
    assert(character >= 0);

    struct value* const value =
        value_new_integer(type, bigint_new_umax((uintmax_t)character));
    value_freeze(value);

    struct expr* const resolved = expr_new_value(expr->location, value);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_bytes(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_BYTES);

    struct string const* const bytes = expr->data.bytes.data.bytes;
    char const* const bytes_start = string_start(bytes);
    size_t const bytes_count = string_count(bytes);

    struct symbol const* array_symbol = NULL;
    struct symbol const* slice_symbol = NULL;
    create_static_bytes(
        resolver,
        expr->location,
        bytes_start,
        bytes_count,
        &array_symbol,
        &slice_symbol);

    struct expr* const resolved =
        expr_new_bytes(expr->location, array_symbol, slice_symbol, bytes_count);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_list(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_LIST);

    struct type const* const type =
        resolve_type(resolver, expr->data.list.type);
    if (type->kind != TYPE_ARRAY && type->kind != TYPE_SLICE) {
        fatal(
            expr->data.list.type->location,
            "expected array or slice type (received `%s`)",
            type->name);
    }
    struct type const* const base = type->kind == TYPE_ARRAY
        ? type->data.array.base
        : type->data.slice.base;

    if (type->kind == TYPE_ARRAY) {
        sbuf(struct cst_expr const* const) elements = expr->data.list.elements;
        sbuf(struct expr const*) resolved_elements = NULL;
        for (size_t i = 0; i < sbuf_count(elements); ++i) {
            struct expr const* resolved_element =
                resolve_expr(resolver, elements[i]);
            resolved_element = implicit_cast(base, resolved_element);
            verify_type_compatibility(
                resolved_element->location, resolved_element->type, base);
            sbuf_push(resolved_elements, resolved_element);
        }
        sbuf_freeze(resolved_elements);

        struct expr const* resolved_ellipsis = NULL;
        if (expr->data.list.ellipsis != NULL) {
            resolved_ellipsis =
                resolve_expr(resolver, expr->data.list.ellipsis);
            resolved_ellipsis = implicit_cast(base, resolved_ellipsis);
            verify_type_compatibility(
                resolved_ellipsis->location, resolved_ellipsis->type, base);
        }

        if ((type->data.array.count != sbuf_count(resolved_elements))
            && resolved_ellipsis == NULL) {
            fatal(
                expr->location,
                "array of type `%s` created with %zu elements (expected %ju)",
                type->name,
                sbuf_count(resolved_elements),
                type->data.array.count);
        }

        struct expr* const resolved = expr_new_array_list(
            expr->location, type, resolved_elements, resolved_ellipsis);

        freeze(resolved);
        return resolved;
    }

    assert(type->kind == TYPE_SLICE);
    if (expr->data.list.ellipsis != NULL) {
        fatal(
            expr->data.list.ellipsis->location,
            "ellipsis element is not allowed in slice lists");
    }

    sbuf(struct cst_expr const* const) elements = expr->data.list.elements;
    size_t const elements_count = sbuf_count(elements);

    static size_t id = 0;
    struct string* const array_name_string =
        string_new_fmt("__slice_list_elements_%zu", id++);
    char const* const array_name = intern_cstr(string_start(array_name_string));
    string_del(array_name_string);

    struct type const* const array_type = type_unique_array(
        expr->location, elements_count, type->data.slice.base);

    bool const is_global = resolver_is_global(resolver);
    bool const is_static = is_global || resolver->is_within_constant_decl;
    struct address const* const array_address = is_static
        ? resolver_reserve_storage_static(resolver, array_name)
        : resolver_reserve_storage_local(resolver, array_name, array_type);

    struct value* array_value = NULL;
    if (is_static) {
        sbuf(struct expr const*) resolved_elements = NULL;
        sbuf(struct value*) resolved_values = NULL;
        for (size_t i = 0; i < sbuf_count(elements); ++i) {
            struct expr const* resolved_element =
                resolve_expr(resolver, elements[i]);
            resolved_element =
                implicit_cast(array_type->data.array.base, resolved_element);
            verify_type_compatibility(
                resolved_element->location,
                resolved_element->type,
                array_type->data.array.base);
            sbuf_push(resolved_elements, resolved_element);
            sbuf_push(resolved_values, eval_rvalue(resolved_element));
        }
        sbuf_freeze(resolved_elements);
        array_value = value_new_array(array_type, resolved_values, NULL);
        value_freeze(array_value);
    }

    struct object* const array_object =
        object_new(array_type, array_address, array_value);
    freeze(array_object);

    struct symbol* const array_symbol =
        (resolver->is_within_constant_decl
             ? symbol_new_constant
             : symbol_new_variable)(expr->location, array_name, array_object);
    if (is_static) {
        register_static_symbol(array_symbol);
    }
    freeze(array_symbol);

    symbol_table_insert(
        resolver->current_symbol_table,
        array_symbol->name,
        array_symbol,
        false);

    sbuf(struct expr const*) resolved_elements = NULL;
    for (size_t i = 0; i < elements_count; ++i) {
        struct expr const* resolved_element =
            resolve_expr(resolver, elements[i]);
        resolved_element =
            implicit_cast(type->data.slice.base, resolved_element);
        verify_type_compatibility(
            resolved_element->location,
            resolved_element->type,
            type->data.slice.base);
        sbuf_push(resolved_elements, resolved_element);
    }
    sbuf_freeze(resolved_elements);

    struct expr* const resolved = expr_new_slice_list(
        expr->location, type, array_symbol, resolved_elements);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_slice(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_SLICE);

    struct type const* const type =
        resolve_type(resolver, expr->data.slice.type);
    if (type->kind != TYPE_SLICE) {
        fatal(
            expr->data.slice.type->location,
            "expected slice type (received `%s`)",
            type->name);
    }

    struct expr const* const start =
        resolve_expr(resolver, expr->data.slice.start);
    if (start->type->kind != TYPE_POINTER) {
        fatal(
            start->location,
            "expression of type `%s` is not a pointer",
            start->type->name);
    }
    struct type const* const slice_pointer_type =
        type_unique_pointer(type->data.slice.base);
    verify_type_compatibility(start->location, start->type, slice_pointer_type);

    struct expr const* count = resolve_expr(resolver, expr->data.slice.count);
    count = implicit_cast(context()->builtin.usize, count);
    verify_type_compatibility(
        count->location, count->type, context()->builtin.usize);

    struct expr* const resolved =
        expr_new_slice(expr->location, type, start, count);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_init_struct(
    struct resolver* resolver,
    struct cst_expr const* expr,
    struct type const* const type)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_INIT);
    assert(type->kind == TYPE_STRUCT);

    if (!type->data.struct_.is_complete) {
        fatal(expr->location, "struct type `%s` is incomplete", type->name);
    }

    sbuf(struct member_variable) const member_variable_defs =
        type->data.struct_.member_variables;

    sbuf(struct cst_member_initializer const* const) const initializers =
        expr->data.init.initializers;

    // Resolve the expressions associated with each initializer element.
    // Expressions are resolved before the checks for duplicate elements,
    // missing elements, or extra elements not corresponding to a struct member
    // variable, all so that the user can receive feedback about any malformed
    // expressions *before* feedback on how the initializer list does not match
    // the struct definition.
    sbuf(struct expr const*) initializer_exprs = NULL;
    for (size_t i = 0; i < sbuf_count(initializers); ++i) {
        struct expr const* expr = NULL;
        if (initializers[i]->expr != NULL) {
            expr = resolve_expr(resolver, initializers[i]->expr);
        }

        sbuf_push(initializer_exprs, expr);
    }

    // Ordered list of member variables corresponding to the member variables
    // defined by the struct type. The list is initialized to the length of
    // struct type's member variable list with all NULLs. As the initializer
    // list is processed the NULLs are replaced with expr pointers so that
    // duplicate initializers can be detected when a non-NULL value would be
    // overwritten, and missing initializers can be detected by looking for
    // remaining NULLs after all initializer elements have been processed.
    sbuf(struct expr const*) member_variable_exprs = NULL;
    for (size_t i = 0; i < sbuf_count(member_variable_defs); ++i) {
        sbuf_push(member_variable_exprs, NULL);
    }

    // True if the nth member variable has an initializer.
    // Used for detecting duplicate initializers or lack of initializers.
    sbuf(bool) member_variable_inits = NULL;
    for (size_t i = 0; i < sbuf_count(member_variable_defs); ++i) {
        sbuf_push(member_variable_inits, false);
    }

    // XXX: Freezing these stretchy buffers here before any calls to fatal
    // because GCC 8.3 ASan will think we leak even though we hold a valid path
    // to the head of each stretchy buffer.
    sbuf_freeze(initializer_exprs);
    sbuf_freeze(member_variable_exprs);
    sbuf_freeze(member_variable_inits);

    for (size_t i = 0; i < sbuf_count(initializers); ++i) {
        char const* const initializer_name = initializers[i]->identifier.name;
        bool found = false; // Did we find the member for this initializer?

        for (size_t j = 0; j < sbuf_count(member_variable_defs); ++j) {
            if (initializer_name != member_variable_defs[j].name) {
                continue;
            }

            if (member_variable_inits[j]) {
                fatal(
                    initializers[i]->location,
                    "duplicate initializer for member variable `%s`",
                    member_variable_defs[j].name);
            }

            if (initializer_exprs[i] == NULL) {
                // Intializer is uninit.
                member_variable_inits[j] = true;
                found = true;
                break;
            }

            struct expr const* const initializer_expr = implicit_cast(
                member_variable_defs[j].type, initializer_exprs[i]);
            verify_type_compatibility(
                initializer_expr->location,
                initializer_expr->type,
                member_variable_defs[j].type);
            member_variable_exprs[j] = initializer_expr;
            member_variable_inits[j] = true;
            found = true;
            break;
        }

        if (!found) {
            fatal(
                initializers[i]->location,
                "struct `%s` does not have a member variable `%s`",
                type->name,
                initializer_name);
        }
    }

    for (size_t i = 0; i < sbuf_count(member_variable_defs); ++i) {
        if (!member_variable_inits[i]) {
            fatal(
                expr->location,
                "missing initializer for member variable `%s`",
                member_variable_defs[i].name);
        }
    }

    sbuf(struct member_variable_initializer) initializer_list = NULL;
    assert(sbuf_count(member_variable_defs) == sbuf_count(initializers));
    for (size_t i = 0; i < sbuf_count(initializers); ++i) {
        for (size_t m = 0; m < sbuf_count(member_variable_defs); ++m) {
            struct member_variable const* const v = &member_variable_defs[m];
            if (initializers[i]->identifier.name != v->name) {
                continue;
            }

            struct expr const* const e = member_variable_exprs[m];
            struct member_variable_initializer const initializer = {
                .variable = v,
                .expr = e,
            };
            sbuf_push(initializer_list, initializer);
        }
    }
    sbuf_freeze(initializer_list);

    struct expr* const resolved =
        expr_new_init(expr->location, type, initializer_list);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_init_union(
    struct resolver* resolver,
    struct cst_expr const* expr,
    struct type const* const type)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_INIT);
    assert(type->kind == TYPE_UNION);

    if (!type->data.union_.is_complete) {
        fatal(expr->location, "union type `%s` is incomplete", type->name);
    }

    sbuf(struct member_variable) const member_variable_defs =
        type->data.union_.member_variables;
    size_t const member_variable_defs_count = sbuf_count(member_variable_defs);

    sbuf(struct cst_member_initializer const* const) const initializers =
        expr->data.init.initializers;
    size_t const initializers_count = sbuf_count(initializers);

    if (member_variable_defs_count == 0 && initializers_count != 0) {
        fatal(
            expr->location,
            "union type `%s` with no members variables requires exactly zero initializers",
            type->name);
    }
    if (member_variable_defs_count != 0 && initializers_count != 1) {
        fatal(
            expr->location,
            "union type `%s` requires exactly one initializer",
            type->name);
    }

    if (initializers_count == 0) {
        struct expr* const resolved = expr_new_init(expr->location, type, NULL);

        freeze(resolved);
        return resolved;
    }

    struct cst_member_initializer const* const initializer = initializers[0];
    char const* const initializer_name = initializer->identifier.name;
    struct expr const* initializer_expr =
        resolve_expr(resolver, initializer->expr);

    struct member_variable const* member_variable = NULL;
    for (size_t i = 0; i < sbuf_count(member_variable_defs); ++i) {
        if (initializer_name == member_variable_defs[i].name) {
            member_variable = &member_variable_defs[i];
            break;
        }
    }
    if (member_variable == NULL) {
        fatal(
            initializer->location,
            "union `%s` does not have a member variable `%s`",
            type->name,
            initializer_name);
    }

    initializer_expr = implicit_cast(member_variable->type, initializer_expr);
    verify_type_compatibility(
        initializer_expr->location,
        initializer_expr->type,
        member_variable->type);

    sbuf(struct member_variable_initializer) initializer_list = NULL;
    sbuf_push(
        initializer_list,
        (struct member_variable_initializer){
            .variable = member_variable,
            .expr = initializer_expr,
        });
    sbuf_freeze(initializer_list);

    struct expr* const resolved =
        expr_new_init(expr->location, type, initializer_list);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_init(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_INIT);

    struct type const* const type =
        resolve_type(resolver, expr->data.init.type);

    if (type->kind == TYPE_STRUCT) {
        return resolve_expr_init_struct(resolver, expr, type);
    }
    if (type->kind == TYPE_UNION) {
        return resolve_expr_init_union(resolver, expr, type);
    }

    fatal(
        expr->location,
        "expected struct or union type (received `%s`)",
        type->name);
    return NULL;
}

static struct expr const*
resolve_expr_cast(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_CAST);

    struct type const* const type =
        resolve_type(resolver, expr->data.cast.type);
    struct expr const* const rhs = resolve_expr(resolver, expr->data.cast.expr);

    return explicit_cast(expr->location, type, rhs);
}

static struct expr const*
resolve_expr_call(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_CALL);

    // Member function call.
    if (expr->data.call.func->kind == CST_EXPR_ACCESS_MEMBER) {
        struct cst_expr const* const dot = expr->data.call.func;
        struct cst_expr const* const lhs = dot->data.access_member.lhs;
        char const* const name =
            dot->data.access_member.member->identifier.name;
        sbuf(struct cst_type const* const) const template_arguments =
            dot->data.access_member.member->template_arguments;

        struct expr const* const instance = resolve_expr(resolver, lhs);
        if (!expr_is_lvalue(instance)) {
            fatal(
                instance->location,
                "attempted to call member function `%s` on non-lvalue instance of type `%s`",
                name,
                instance->type->name);
        }

        if (instance->type->kind == TYPE_STRUCT) {
            struct member_variable const* const variable =
                type_struct_member_variable(instance->type, name);
            if (variable != NULL) {
                // Actually this is *not* a member function call - this is a
                // normal function invocation of a member variable that just
                // happens to have a function type.
                goto regular_function_call;
            }
        }

        struct symbol const* symbol = type_member_symbol(instance->type, name);
        if (symbol == NULL) {
            fatal(
                instance->location,
                "type `%s` has no member function `%s`",
                instance->type->name,
                name);
        }
        if (symbol->kind == SYMBOL_TEMPLATE) {
            symbol = xget_template_instance(
                resolver,
                dot->data.access_member.member->location,
                symbol,
                template_arguments);
        }

        if (symbol->kind != SYMBOL_FUNCTION) {
            fatal(
                instance->location,
                "type `%s` has no member function `%s`",
                instance->type->name,
                name);
        }
        struct function const* const function = symbol->data.function;
        struct type const* function_type = function->type;

        struct type const* const selfptr_type =
            type_unique_pointer(instance->type);

        sbuf(struct type const* const) const parameter_types =
            function_type->data.function.parameter_types;
        if (sbuf_count(parameter_types) == 0) {
            fatal(
                instance->location,
                "expected type `%s` for the first parameter of member function `%s` of type `%s`",
                selfptr_type->name,
                name,
                instance->type->name);
        }
        if (parameter_types[0] != selfptr_type) {
            fatal(
                instance->location,
                "expected type `%s` for the first parameter of member function `%s` of type `%s` (found `%s`)",
                selfptr_type->name,
                name,
                instance->type->name,
                parameter_types[0]->name);
        }
        size_t const arg_count = sbuf_count(expr->data.call.arguments);
        // Number of parameters minus one for the implicit pointer to self.
        size_t const expected_arg_count = sbuf_count(parameter_types) - 1;
        if (arg_count != expected_arg_count) {
            fatal(
                expr->location,
                "member function with type `%s` expects %zu argument(s) (%zu provided)",
                function_type->name,
                expected_arg_count,
                arg_count);
        }

        sbuf(struct expr const*) arguments = NULL;
        // Add the implicit pointer to self as the first argument.
        assert(expr_is_lvalue(instance));
        struct expr* const selfptr = expr_new_unary(
            expr->location, selfptr_type, UOP_ADDRESSOF, instance);
        freeze(selfptr);
        sbuf_push(arguments, selfptr);
        for (size_t i = 0; i < arg_count; ++i) {
            struct expr const* arg =
                resolve_expr(resolver, expr->data.call.arguments[i]);
            sbuf_push(arguments, arg);
        }
        sbuf_freeze(arguments);

        // Type-check function arguments.
        for (size_t i = 0; i < sbuf_count(arguments); ++i) {
            arguments[i] = implicit_cast(parameter_types[i], arguments[i]);
        }
        for (size_t i = 0; i < sbuf_count(parameter_types); ++i) {
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

        assert(symbol != NULL);
        assert(symbol->kind == SYMBOL_FUNCTION);
        struct expr* member_expr = expr_new_symbol(
            dot->data.access_member.member->identifier.location, symbol);
        freeze(member_expr);

        struct expr* const resolved =
            expr_new_call(expr->location, member_expr, arguments);

        freeze(resolved);
        return resolved;
    }

    // Regular function call.
regular_function_call:;
    struct expr const* const function =
        resolve_expr(resolver, expr->data.call.func);
    if (function->type->kind != TYPE_FUNCTION) {
        fatal(
            expr->location,
            "non-callable type `%s` used in function call expression",
            function->type->name);
    }

    if (sbuf_count(expr->data.call.arguments)
        != sbuf_count(function->type->data.function.parameter_types)) {
        fatal(
            expr->location,
            "function with type `%s` expects %zu argument(s) (%zu provided)",
            function->type->name,
            sbuf_count(function->type->data.function.parameter_types),
            sbuf_count(expr->data.call.arguments));
    }

    sbuf(struct expr const*) arguments = NULL;
    for (size_t i = 0; i < sbuf_count(expr->data.call.arguments); ++i) {
        struct expr const* arg =
            resolve_expr(resolver, expr->data.call.arguments[i]);
        sbuf_push(arguments, arg);
    }
    sbuf_freeze(arguments);

    // Type-check function arguments.
    sbuf(struct type const* const) const parameter_types =
        function->type->data.function.parameter_types;
    for (size_t i = 0; i < sbuf_count(arguments); ++i) {
        arguments[i] = implicit_cast(parameter_types[i], arguments[i]);
    }
    for (size_t i = 0; i < sbuf_count(parameter_types); ++i) {
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

    freeze(resolved);
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
    idx = implicit_cast(context()->builtin.usize, idx);
    if (idx->type->kind != TYPE_USIZE) {
        fatal(
            idx->location,
            "illegal index operation with index of non-usize type `%s`",
            idx->type->name);
    }

    struct expr* const resolved =
        expr_new_access_index(expr->location, lhs, idx);

    freeze(resolved);
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
    begin = implicit_cast(context()->builtin.usize, begin);
    if (begin->type->kind != TYPE_USIZE) {
        fatal(
            begin->location,
            "illegal slice operation with index of non-usize type `%s`",
            begin->type->name);
    }

    struct expr const* end =
        resolve_expr(resolver, expr->data.access_slice.end);
    end = implicit_cast(context()->builtin.usize, end);
    if (end->type->kind != TYPE_USIZE) {
        fatal(
            end->location,
            "illegal slice operation with index of non-usize type `%s`",
            end->type->name);
    }

    struct expr* const resolved =
        expr_new_access_slice(expr->location, lhs, begin, end);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_access_member(
    struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_ACCESS_MEMBER);

    struct expr const* const lhs =
        resolve_expr(resolver, expr->data.access_member.lhs);
    if (lhs->type->kind != TYPE_STRUCT && lhs->type->kind != TYPE_UNION) {
        fatal(
            lhs->location,
            "attempted member access on non-struct and non-union type `%s`",
            lhs->type->name);
    }

    char const* const member_name =
        expr->data.access_member.member->identifier.name;

    struct member_variable const* const member_variable_def =
        type_member_variable(lhs->type, member_name);
    if (member_variable_def != NULL) {
        if (sbuf_count(expr->data.access_member.member->template_arguments)
            != 0) {
            fatal(
                expr->location,
                "attempted template instantiation of member variable `%s` on type `%s`",
                member_name,
                lhs->type->name);
        }
        struct expr* const resolved = expr_new_access_member_variable(
            expr->location, lhs, member_variable_def);

        freeze(resolved);
        return resolved;
    }

    struct symbol const* const member_symbol =
        type_member_symbol(lhs->type, member_name);

    if (member_symbol != NULL && member_symbol->kind == SYMBOL_CONSTANT) {
        fatal(
            expr->location,
            "attempted to take the value of member constant `%s` on type `%s`",
            member_symbol->name,
            lhs->type->name);
    }

    if (member_symbol != NULL && member_symbol->kind == SYMBOL_FUNCTION) {
        fatal(
            expr->location,
            "attempted to take the value of member function `%s` on type `%s`",
            member_symbol->name,
            lhs->type->name);
    }

    if (member_symbol != NULL && member_symbol->kind == SYMBOL_TEMPLATE) {
        fatal(
            expr->location,
            "attempted to take the value of member template `%s` on type `%s`",
            member_symbol->name,
            lhs->type->name);
    }

    assert(member_symbol == NULL);
    fatal(
        lhs->location,
        "type `%s` has no member `%s`",
        lhs->type->name,
        member_name);
    return NULL;
}

// Basically a copy-paste of the logic from resolve_expr_unary and
// resolve_expr_unary_dereference with the unary operator fields exchanged for
// the access dereference fields in the cst_expr.
static struct expr const*
resolve_expr_access_dereference(
    struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_ACCESS_DEREFERENCE);

    struct expr const* const lhs =
        resolve_expr(resolver, expr->data.access_dereference.lhs);

    if (lhs->type->kind != TYPE_POINTER) {
        fatal(
            lhs->location,
            "cannot dereference non-pointer type `%s`",
            lhs->type->name);
    }
    struct expr* const resolved = expr_new_unary(
        expr->location, lhs->type->data.pointer.base, UOP_DEREFERENCE, lhs);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_defined(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_DEFINED);

    struct value* const value = value_new_boolean(
        get_symbol(resolver, expr->data.defined.symbol) != NULL);
    value_freeze(value);

    struct expr* const resolved = expr_new_value(expr->location, value);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_sizeof(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_SIZEOF);

    struct type const* const rhs =
        resolve_type(resolver, expr->data.sizeof_.rhs);
    if (rhs->size == SIZEOF_UNSIZED) {
        fatal(expr->location, "type `%s` has no defined size", rhs->name);
    }

    struct expr* const resolved = expr_new_sizeof(expr->location, rhs);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_alignof(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_ALIGNOF);

    struct type const* const rhs =
        resolve_type(resolver, expr->data.alignof_.rhs);
    if (rhs->align == ALIGNOF_UNSIZED) {
        fatal(expr->location, "type `%s` has no defined alignment", rhs->name);
    }

    struct expr* const resolved = expr_new_alignof(expr->location, rhs);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_fileof(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_FILEOF);

    char const* const bytes_start = expr->location.path;
    size_t const bytes_count = strlen(bytes_start);

    struct symbol const* array_symbol = NULL;
    struct symbol const* slice_symbol = NULL;
    create_static_bytes(
        resolver,
        expr->location,
        bytes_start,
        bytes_count,
        &array_symbol,
        &slice_symbol);

    struct expr* const resolved =
        expr_new_bytes(expr->location, array_symbol, slice_symbol, bytes_count);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_lineof(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_LINEOF);
    (void)resolver;

    struct value* const value = value_new_integer(
        context()->builtin.usize, bigint_new_umax(expr->location.line));
    value_freeze(value);

    struct expr* const resolved = expr_new_value(expr->location, value);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_embed(struct resolver* resolver, struct cst_expr const* expr)
{
    assert(resolver != NULL);
    assert(expr != NULL);
    assert(expr->kind == CST_EXPR_EMBED);

    char const* const path =
        canonical_search_path(resolver->module->path, expr->data.embed_.path);
    if (path == NULL) {
        fatal(
            expr->location,
            "failed to resolve embed path `%s`",
            expr->data.embed_.path);
    }

    void* bytes_start = NULL;
    size_t bytes_count = 0;
    if (file_read_all(path, &bytes_start, &bytes_count)) {
        fatal(
            expr->location,
            "failed to read '%s' with error '%s'",
            path,
            strerror(errno));
    }

    struct symbol const* array_symbol = NULL;
    struct symbol const* slice_symbol = NULL;
    create_static_bytes(
        resolver,
        expr->location,
        bytes_start,
        bytes_count,
        &array_symbol,
        &slice_symbol);

    xalloc(bytes_start, XALLOC_FREE);

    struct expr* const resolved =
        expr_new_bytes(expr->location, array_symbol, slice_symbol, bytes_count);

    freeze(resolved);
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
    struct token const op = expr->data.unary.op;
    bool const is_sign = (op.kind == TOKEN_PLUS) || (op.kind == TOKEN_DASH);
    struct cst_expr const* const cst_rhs = expr->data.unary.rhs;
    if (is_sign && cst_rhs->kind == CST_EXPR_INTEGER) {
        struct token const token = cst_rhs->data.integer;

        struct bigint const* integer = token.data.integer.value;
        if (op.kind == TOKEN_DASH) {
            struct bigint* const tmp = bigint_new(integer);
            assert(tmp != NULL);
            bigint_neg(tmp, tmp);
            bigint_freeze(tmp);
            integer = tmp;
        }

        struct type const* const type = integer_literal_suffix_to_type(
            token.location, token.data.integer.suffix);

        verify_byte_or_int_in_range(op.location, type, integer);
        struct value* const value =
            value_new_integer(type, bigint_new(integer));
        value_freeze(value);

        struct expr* const resolved = expr_new_value(op.location, value);

        freeze(resolved);
        return resolved;
    }

    // Similarly we identify the special case where a + or - token is
    // immediately followed by an IEEE754 token and combine the two into a
    // single IEEE754 expression.
    if (is_sign && cst_rhs->kind == CST_EXPR_IEEE754) {
        struct token const token = cst_rhs->data.ieee754;
        double ieee754 = token.data.ieee754.value;

        if (op.kind == TOKEN_DASH) {
            ieee754 = -ieee754;
        }

        struct type const* const type = ieee754_literal_suffix_to_type(
            token.location, token.data.ieee754.suffix);

        assert(type_is_ieee754(type));
        struct value* value = NULL;
        if (type->kind == TYPE_F32) {
            value = value_new_f32((float)ieee754);
            value_freeze(value);
        }
        if (type->kind == TYPE_F64) {
            value = value_new_f64(ieee754);
            value_freeze(value);
        }
        if (type->kind == TYPE_REAL) {
            value = value_new_real(ieee754);
            value_freeze(value);
        }

        assert(value != NULL);
        struct expr* const resolved = expr_new_value(op.location, value);

        freeze(resolved);
        return resolved;
    }

    struct expr const* const rhs = resolve_expr(resolver, cst_rhs);
    switch (op.kind) {
    case TOKEN_NOT: {
        return resolve_expr_unary_logical(resolver, op, UOP_NOT, rhs);
    }
    case TOKEN_STARTOF: {
        return resolve_expr_unary_startof(resolver, op, rhs);
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
                op.location,
                "invalid argument of type `%s` in unary `%s` expression",
                rhs->type->name,
                token_kind_to_cstr(op.kind));
        }
        return resolve_expr_unary_arithmetic(resolver, op, UOP_NEG, rhs);
    }
    case TOKEN_DASH_PERCENT: {
        if (type_is_uinteger(rhs->type)) {
            fatal(
                op.location,
                "invalid argument of type `%s` in unary `%s` expression",
                rhs->type->name,
                token_kind_to_cstr(op.kind));
        }
        return resolve_expr_unary_arithmetic(
            resolver, op, UOP_NEG_WRAPPING, rhs);
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
    struct token op,
    enum uop_kind uop,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(rhs != NULL);
    (void)resolver;

    if (rhs->type->kind != TYPE_BOOL) {
        fatal(
            op.location,
            "invalid argument of type `%s` in unary `%s` expression",
            rhs->type->name,
            token_kind_to_cstr(op.kind));
    }

    struct expr* const resolved =
        expr_new_unary(op.location, rhs->type, uop, rhs);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_unary_arithmetic(
    struct resolver* resolver,
    struct token op,
    enum uop_kind uop,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(rhs != NULL);
    (void)resolver;

    bool const type_is_valid =
        type_is_integer(rhs->type) || type_is_ieee754(rhs->type);
    if (!type_is_valid) {
        fatal(
            op.location,
            "invalid argument of type `%s` in unary `%s` expression",
            rhs->type->name,
            token_kind_to_cstr(op.kind));
    }

    struct type const* const type = rhs->type;

    // Wrapping unary expressions are only supported for sized integer types.
    bool const is_wrapping = uop == UOP_NEG_WRAPPING;
    bool const allow_wrapping =
        type_is_uinteger(type) || type_is_sinteger(type);
    if (is_wrapping && !allow_wrapping) {
        fatal(
            op.location,
            "invalid argument of type `%s` in wrapping unary `%s` expression",
            type->name,
            token_kind_to_cstr(op.kind));
    }

    struct expr* const resolved = expr_new_unary(op.location, type, uop, rhs);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_unary_bitwise(
    struct resolver* resolver,
    struct token op,
    enum uop_kind uop,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(rhs != NULL);
    (void)resolver;

    if (rhs->type->size == SIZEOF_UNSIZED) {
        fatal(
            op.location,
            "unsized type `%s` in unary `%s` expression has no bit-representation",
            rhs->type->name,
            token_kind_to_cstr(op.kind));
    }
    if (!(rhs->type->kind == TYPE_BYTE || type_is_integer(rhs->type))) {
        fatal(
            rhs->location,
            "cannot apply bitwise NOT to type `%s`",
            rhs->type->name);
    }
    struct expr* const resolved =
        expr_new_unary(op.location, rhs->type, uop, rhs);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_unary_dereference(
    struct resolver* resolver, struct token op, struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op.kind == TOKEN_STAR);
    assert(rhs != NULL);
    (void)resolver;

    if (rhs->type->kind != TYPE_POINTER) {
        fatal(
            rhs->location,
            "cannot dereference non-pointer type `%s`",
            rhs->type->name);
    }
    struct expr* const resolved = expr_new_unary(
        op.location, rhs->type->data.pointer.base, UOP_DEREFERENCE, rhs);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_unary_addressof(
    struct resolver* resolver, struct token op, struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op.kind == TOKEN_AMPERSAND);
    assert(rhs != NULL);
    (void)resolver;

    if (!expr_is_lvalue(rhs)) {
        fatal(rhs->location, "cannot take the address of a non-lvalue");
    }

    struct expr* const resolved = expr_new_unary(
        op.location, type_unique_pointer(rhs->type), UOP_ADDRESSOF, rhs);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_unary_startof(
    struct resolver* resolver, struct token op, struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op.kind == TOKEN_STARTOF);
    assert(rhs != NULL);
    (void)resolver;

    if (rhs->type->kind != TYPE_SLICE) {
        fatal(
            rhs->location,
            "expected slice type (received `%s`)",
            rhs->type->name);
    }

    struct expr* const resolved = expr_new_unary(
        op.location,
        type_unique_pointer(rhs->type->data.slice.base),
        UOP_STARTOF,
        rhs);

    freeze(resolved);
    return resolved;
}

static struct expr const*
resolve_expr_unary_countof(
    struct resolver* resolver, struct token op, struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(op.kind == TOKEN_COUNTOF);
    assert(rhs != NULL);
    (void)resolver;

    if (rhs->type->kind != TYPE_ARRAY && rhs->type->kind != TYPE_SLICE) {
        fatal(
            rhs->location,
            "expected array or slice type (received `%s`)",
            rhs->type->name);
    }

    struct expr* const resolved =
        expr_new_unary(op.location, context()->builtin.usize, UOP_COUNTOF, rhs);

    freeze(resolved);
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
    struct token const op = expr->data.binary.op;
    switch (op.kind) {
    case TOKEN_OR: {
        return resolve_expr_binary_logical(resolver, op, BOP_OR, lhs, rhs);
    }
    case TOKEN_AND: {
        return resolve_expr_binary_logical(resolver, op, BOP_AND, lhs, rhs);
    }
    case TOKEN_SHL: {
        return resolve_expr_binary_shift(resolver, op, BOP_SHL, lhs, rhs);
    }
    case TOKEN_SHR: {
        return resolve_expr_binary_shift(resolver, op, BOP_SHR, lhs, rhs);
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
    case TOKEN_PLUS_PERCENT: {
        return resolve_expr_binary_arithmetic(
            resolver, op, BOP_ADD_WRAPPING, lhs, rhs);
    }
    case TOKEN_DASH: {
        return resolve_expr_binary_arithmetic(resolver, op, BOP_SUB, lhs, rhs);
    }
    case TOKEN_DASH_PERCENT: {
        return resolve_expr_binary_arithmetic(
            resolver, op, BOP_SUB_WRAPPING, lhs, rhs);
    }
    case TOKEN_STAR: {
        return resolve_expr_binary_arithmetic(resolver, op, BOP_MUL, lhs, rhs);
    }
    case TOKEN_STAR_PERCENT: {
        return resolve_expr_binary_arithmetic(
            resolver, op, BOP_MUL_WRAPPING, lhs, rhs);
    }
    case TOKEN_FSLASH: {
        return resolve_expr_binary_arithmetic(resolver, op, BOP_DIV, lhs, rhs);
    }
    case TOKEN_PERCENT: {
        return resolve_expr_binary_arithmetic(resolver, op, BOP_REM, lhs, rhs);
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
    struct token op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    (void)resolver;

    bool const types_are_valid =
        lhs->type == rhs->type && lhs->type->kind == TYPE_BOOL;
    if (!types_are_valid) {
        fatal(
            op.location,
            "invalid arguments of types `%s` and `%s` in binary `%s` expression",
            lhs->type->name,
            rhs->type->name,
            token_kind_to_cstr(op.kind));
    }

    struct type const* const type = context()->builtin.bool_;
    struct expr* resolved = expr_new_binary(op.location, type, bop, lhs, rhs);
    freeze(resolved);

    // OPTIMIZATION(constant folding)
    if (lhs->kind == EXPR_VALUE && rhs->kind == EXPR_VALUE) {
        struct value* const value = eval_rvalue(resolved);
        value_freeze(value);

        assert(value->type->kind == TYPE_BOOL);
        resolved = expr_new_value(resolved->location, value);

        freeze(resolved);
        return resolved;
    }

    return resolved;
}

static struct expr const*
resolve_expr_binary_shift(
    struct resolver* resolver,
    struct token op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    (void)resolver;

    if (!type_is_integer(lhs->type)) {
        fatal(
            op.location,
            "invalid left-hand argument of type `%s` in binary `%s` expression",
            lhs->type->name,
            token_kind_to_cstr(op.kind));
    }
    if (lhs->type->size == SIZEOF_UNSIZED) {
        fatal(
            op.location,
            "unsized type `%s` in binary `%s` expression has no bit-representation",
            lhs->type->name,
            token_kind_to_cstr(op.kind));
    }

    rhs = implicit_cast(context()->builtin.usize, rhs);
    if (rhs->type != context()->builtin.usize) {
        fatal(
            op.location,
            "invalid non-usize right-hand argument of type `%s` in binary `%s` expression",
            rhs->type->name,
            token_kind_to_cstr(op.kind));
    }

    struct type const* const type = lhs->type;

    struct expr* const resolved =
        expr_new_binary(op.location, type, bop, lhs, rhs);
    freeze(resolved);

    return resolved;
}

static struct expr const*
resolve_expr_binary_compare_equality(
    struct resolver* resolver,
    struct token op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    (void)resolver;

    lhs = implicit_cast(rhs->type, lhs);
    rhs = implicit_cast(lhs->type, rhs);

    if (lhs->type != rhs->type) {
        fatal(
            op.location,
            "invalid arguments of types `%s` and `%s` in binary `%s` expression",
            lhs->type->name,
            rhs->type->name,
            token_kind_to_cstr(op.kind));
    }
    struct type const* const xhs_type = lhs->type;
    if (!type_can_compare_equality(xhs_type)) {
        fatal(
            op.location,
            "invalid arguments of type `%s` in binary `%s` expression",
            xhs_type->name,
            token_kind_to_cstr(op.kind));
    }

    struct expr* resolved =
        expr_new_binary(op.location, context()->builtin.bool_, bop, lhs, rhs);
    freeze(resolved);

    // OPTIMIZATION(constant folding)
    if (lhs->kind == EXPR_VALUE && rhs->kind == EXPR_VALUE) {
        struct value* const value = eval_rvalue(resolved);
        value_freeze(value);

        assert(value->type->kind == TYPE_BOOL);
        resolved = expr_new_value(resolved->location, value);

        freeze(resolved);
        return resolved;
    }

    return resolved;
}

static struct expr const*
resolve_expr_binary_compare_order(
    struct resolver* resolver,
    struct token op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    (void)resolver;

    lhs = implicit_cast(rhs->type, lhs);
    rhs = implicit_cast(lhs->type, rhs);

    if (lhs->type != rhs->type) {
        fatal(
            op.location,
            "invalid arguments of types `%s` and `%s` in binary `%s` expression",
            lhs->type->name,
            rhs->type->name,
            token_kind_to_cstr(op.kind));
    }

    struct type const* const xhs_type = lhs->type;
    if (!type_can_compare_order(xhs_type)) {
        fatal(
            op.location,
            "invalid arguments of type `%s` in binary `%s` expression",
            xhs_type->name,
            token_kind_to_cstr(op.kind));
    }

    struct expr* resolved =
        expr_new_binary(op.location, context()->builtin.bool_, bop, lhs, rhs);
    freeze(resolved);

    // OPTIMIZATION(constant folding)
    if (lhs->kind == EXPR_VALUE && rhs->kind == EXPR_VALUE) {
        struct value* const value = eval_rvalue(resolved);
        value_freeze(value);

        assert(value->type->kind == TYPE_BOOL);
        resolved = expr_new_value(resolved->location, value);

        freeze(resolved);
        return resolved;
    }

    return resolved;
}

static struct expr const*
resolve_expr_binary_arithmetic(
    struct resolver* resolver,
    struct token op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    (void)resolver;

    lhs = implicit_cast(rhs->type, lhs);
    rhs = implicit_cast(lhs->type, rhs);

    bool const types_are_valid = lhs->type == rhs->type
        && (type_is_integer(lhs->type) || type_is_ieee754(lhs->type));
    if (!types_are_valid) {
        fatal(
            op.location,
            "invalid arguments of types `%s` and `%s` in binary `%s` expression",
            lhs->type->name,
            rhs->type->name,
            token_kind_to_cstr(op.kind));
    }

    struct type const* const type = lhs->type; // Arbitrarily use lhs.

    // Wrapping binary expressions are only supported for sized integer types.
    bool const is_wrapping = (bop == BOP_ADD_WRAPPING)
        || (bop == BOP_SUB_WRAPPING) || (bop == BOP_MUL_WRAPPING);
    bool const allow_wrapping =
        type_is_uinteger(type) || type_is_sinteger(type);
    if (is_wrapping && !allow_wrapping) {
        fatal(
            op.location,
            "invalid arguments of type `%s` in wrapping binary `%s` expression",
            type->name,
            token_kind_to_cstr(op.kind));
    }

    // Remainder binary expressions are only supported for sized integer types.
    bool const is_rem = bop == BOP_REM;
    bool const allow_rem = type_is_uinteger(type) || type_is_sinteger(type);
    if (is_rem && !allow_rem) {
        assert(type->size != SIZEOF_UNSIZED);
        fatal(
            op.location,
            "invalid arguments of type `%s` in binary `%s` expression",
            type->name,
            token_kind_to_cstr(op.kind));
    }

    struct expr* resolved = expr_new_binary(op.location, type, bop, lhs, rhs);
    freeze(resolved);

    // OPTIMIZATION(constant folding)
    if (lhs->kind == EXPR_VALUE && rhs->kind == EXPR_VALUE) {
        struct value* const value = eval_rvalue(resolved);
        value_freeze(value);

        assert(type == value->type);
        resolved = expr_new_value(resolved->location, value);

        freeze(resolved);
        return resolved;
    }

    return resolved;
}

static struct expr const*
resolve_expr_binary_bitwise(
    struct resolver* resolver,
    struct token op,
    enum bop_kind bop,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(resolver != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    (void)resolver;

    lhs = implicit_cast(rhs->type, lhs);
    rhs = implicit_cast(lhs->type, rhs);

    if (lhs->type != rhs->type) {
        goto invalid_operand_types;
    }
    struct type const* type = lhs->type; // Arbitrarily use lhs.
    if (type->size == SIZEOF_UNSIZED) {
        fatal(
            op.location,
            "unsized types `%s` in binary `%s` expression have no bit-representation",
            type->name,
            token_kind_to_cstr(op.kind));
    }

    bool const valid = type->kind == TYPE_BOOL || type->kind == TYPE_BYTE
        || type_is_integer(type);
    if (!valid) {
        goto invalid_operand_types;
    }

    struct expr* resolved = expr_new_binary(op.location, type, bop, lhs, rhs);
    freeze(resolved);

    // OPTIMIZATION(constant folding)
    if (lhs->kind == EXPR_VALUE && rhs->kind == EXPR_VALUE) {
        struct value* const value = eval_rvalue(resolved);
        value_freeze(value);

        assert(type == value->type);
        resolved = expr_new_value(resolved->location, value);

        freeze(resolved);
        return resolved;
    }

    return resolved;

invalid_operand_types:
    fatal(
        op.location,
        "invalid arguments of types `%s` and `%s` in binary `%s` expression",
        lhs->type->name,
        rhs->type->name,
        token_kind_to_cstr(op.kind));
    return NULL;
}

static struct type const*
resolve_type(struct resolver* resolver, struct cst_type const* type)
{
    assert(resolver != NULL);
    assert(type != NULL);

    switch (type->kind) {
    case CST_TYPE_SYMBOL: {
        return resolve_type_symbol(resolver, type);
    }
    case CST_TYPE_FUNCTION: {
        return resolve_type_function(resolver, type);
    }
    case CST_TYPE_POINTER: {
        return resolve_type_pointer(resolver, type);
    }
    case CST_TYPE_ARRAY: {
        return resolve_type_array(resolver, type);
    }
    case CST_TYPE_SLICE: {
        return resolve_type_slice(resolver, type);
    }
    case CST_TYPE_STRUCT: {
        return resolve_type_struct(resolver, type);
    }
    case CST_TYPE_UNION: {
        return resolve_type_union(resolver, type);
    }
    case CST_TYPE_ENUM: {
        return resolve_type_enum(resolver, type);
    }
    case CST_TYPE_TYPEOF: {
        return resolve_type_typeof(resolver, type);
    }
    }

    UNREACHABLE();
    return NULL;
}

static struct type const*
resolve_type_symbol(struct resolver* resolver, struct cst_type const* type)
{
    assert(resolver != NULL);
    assert(type != NULL);
    assert(type->kind == CST_TYPE_SYMBOL);

    struct symbol const* const symbol =
        xget_symbol(resolver, type->data.symbol);
    if (symbol->kind == SYMBOL_TEMPLATE) {
        fatal(
            type->location, "template `%s` must be instantiated", symbol->name);
    }
    if (symbol->kind != SYMBOL_TYPE) {
        fatal(type->location, "identifier `%s` is not a type", symbol->name);
    }

    return symbol_xget_type(symbol);
}

static struct type const*
resolve_type_function(struct resolver* resolver, struct cst_type const* type)
{
    assert(resolver != NULL);
    assert(type != NULL);
    assert(type->kind == CST_TYPE_FUNCTION);

    sbuf(struct cst_type const* const) const parameter_types =
        type->data.function.parameter_types;

    sbuf(struct type const*) param_types = NULL;
    sbuf_resize(param_types, sbuf_count(parameter_types));
    for (size_t i = 0; i < sbuf_count(parameter_types); ++i) {
        param_types[i] = resolve_type(resolver, parameter_types[i]);
    }
    sbuf_freeze(param_types);

    struct type const* const return_type =
        resolve_type(resolver, type->data.function.return_type);

    return type_unique_function(param_types, return_type);
}

static struct type const*
resolve_type_pointer(struct resolver* resolver, struct cst_type const* type)
{
    assert(resolver != NULL);
    assert(type != NULL);
    assert(type->kind == CST_TYPE_POINTER);

    struct type const* const base =
        resolve_type(resolver, type->data.pointer.base);
    return type_unique_pointer(base);
}

static struct type const*
resolve_type_array(struct resolver* resolver, struct cst_type const* type)
{
    assert(resolver != NULL);
    assert(type != NULL);
    assert(type->kind == CST_TYPE_ARRAY);

    struct expr const* count_expr =
        resolve_expr(resolver, type->data.array.count);
    count_expr = implicit_cast(context()->builtin.usize, count_expr);

    if (count_expr->type != context()->builtin.usize) {
        fatal(
            count_expr->location,
            "illegal array count with non-usize type `%s`",
            count_expr->type->name);
    }

    struct value* const count_value = eval_rvalue(count_expr);

    assert(count_value->type == context()->builtin.usize);
    uintmax_t count = 0u;
    if (bigint_to_umax(&count, count_value->data.integer)) {
        fatal(
            count_expr->location,
            "array count too large (received %s)",
            bigint_to_new_cstr(count_value->data.integer));
    }
    value_del(count_value);

    struct type const* const base =
        resolve_type(resolver, type->data.array.base);

    return type_unique_array(type->location, count, base);
}

static struct type const*
resolve_type_slice(struct resolver* resolver, struct cst_type const* type)
{
    assert(resolver != NULL);
    assert(type != NULL);
    assert(type->kind == CST_TYPE_SLICE);

    struct type const* const base =
        resolve_type(resolver, type->data.slice.base);
    // Generate a unique type for the `start` member of the slice, so that type
    // ordering knows to generate the definition for the slice pointer before
    // generating the definition for the slice itself. This is important for
    // the C backend, where a slice type is generated as a struct with a
    // pointer, usize pair.
    (void)type_unique_pointer(base);
    return type_unique_slice(base);
}

static struct type const*
resolve_type_struct(struct resolver* resolver, struct cst_type const* type)
{
    assert(resolver != NULL);
    assert(type != NULL);
    assert(type->kind == CST_TYPE_STRUCT);

    sbuf(struct cst_member const* const) const members =
        type->data.struct_.members;
    size_t const members_count = sbuf_count(members);

    // Check for duplicate member definitions.
    for (size_t i = 0; i < members_count; ++i) {
        for (size_t j = i + 1; j < members_count; ++j) {
            if (members[i]->name == members[j]->name) {
                fatal(
                    members[j]->location,
                    "duplicate definition of member `%s`",
                    members[j]->name);
            }
        }
    }

    struct string* const name_string = string_new_cstr("struct { ");
    for (size_t i = 0; i < members_count; ++i) {
        if (i != 0) {
            string_append_cstr(name_string, " ");
        }
        assert(members[i]->kind == CST_MEMBER_VARIABLE);
        string_append_fmt(
            name_string,
            "var %s: %s;",
            members[i]->name,
            resolve_type(resolver, members[i]->data.variable.type)->name);
    }
    string_append_cstr(name_string, " }");
    char const* const name =
        intern(string_start(name_string), string_count(name_string));
    string_del(name_string);

    struct symbol const* const existing =
        symbol_table_lookup(context()->global_symbol_table, name);
    if (existing != NULL) {
        assert(existing->kind == SYMBOL_TYPE);
        return symbol_xget_type(existing);
    }

    struct symbol_table* const struct_symbols =
        symbol_table_new(resolver->current_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, struct_symbols);
    struct type* const resolved_type = type_new_struct(name, struct_symbols);
    freeze(resolved_type);

    struct symbol* const symbol = symbol_new_type(NO_LOCATION, resolved_type);
    freeze(symbol);

    complete_struct(resolver, symbol, members);

    symbol_table_insert(context()->global_symbol_table, name, symbol, false);
    return resolved_type;
}

static struct type const*
resolve_type_union(struct resolver* resolver, struct cst_type const* type)
{
    assert(resolver != NULL);
    assert(type != NULL);
    assert(type->kind == CST_TYPE_UNION);

    sbuf(struct cst_member const* const) const members =
        type->data.union_.members;
    size_t const members_count = sbuf_count(members);

    // Check for duplicate member definitions.
    for (size_t i = 0; i < members_count; ++i) {
        for (size_t j = i + 1; j < members_count; ++j) {
            if (members[i]->name == members[j]->name) {
                fatal(
                    members[j]->location,
                    "duplicate definition of member `%s`",
                    members[j]->name);
            }
        }
    }

    struct string* const name_string = string_new_cstr("union { ");
    for (size_t i = 0; i < members_count; ++i) {
        if (i != 0) {
            string_append_cstr(name_string, " ");
        }
        assert(members[i]->kind == CST_MEMBER_VARIABLE);
        string_append_fmt(
            name_string,
            "var %s: %s;",
            members[i]->name,
            resolve_type(resolver, members[i]->data.variable.type)->name);
    }
    string_append_cstr(name_string, " }");
    char const* const name =
        intern(string_start(name_string), string_count(name_string));
    string_del(name_string);

    struct symbol const* const existing =
        symbol_table_lookup(context()->global_symbol_table, name);
    if (existing != NULL) {
        assert(existing->kind == SYMBOL_TYPE);
        return symbol_xget_type(existing);
    }

    struct symbol_table* const union_symbols =
        symbol_table_new(resolver->current_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, union_symbols);
    struct type* const resolved_type = type_new_union(name, union_symbols);
    freeze(resolved_type);

    struct symbol* const symbol = symbol_new_type(NO_LOCATION, resolved_type);
    freeze(symbol);

    complete_union(resolver, symbol, members);

    symbol_table_insert(context()->global_symbol_table, name, symbol, false);
    return resolved_type;
}

static struct type const*
resolve_type_enum(struct resolver* resolver, struct cst_type const* type)
{
    assert(resolver != NULL);
    assert(type != NULL);
    assert(type->kind == CST_TYPE_ENUM);

    struct cst_enum_value const* const* values = type->data.enum_.values;
    size_t const values_count = sbuf_count(values);

    struct string* const name_string = string_new_fmt(
        "enum at %s:%zu { ", type->location.path, type->location.line);
    for (size_t i = 0; i < values_count; ++i) {
        if (i != 0) {
            string_append_cstr(name_string, " ");
        }
        string_append_fmt(name_string, "%s;", values[i]->identifier.name);
    }
    string_append_cstr(name_string, " }");
    char const* const name =
        intern(string_start(name_string), string_count(name_string));
    string_del(name_string);

    return symbol_xget_type(
        complete_enum(resolver, type->location, name, values, NULL));
}

static struct type const*
resolve_type_typeof(struct resolver* resolver, struct cst_type const* type)
{
    assert(resolver != NULL);
    assert(type != NULL);
    assert(type->kind == CST_TYPE_TYPEOF);

    struct expr const* const expr =
        resolve_expr(resolver, type->data.typeof_.expr);
    return expr->type;
}

void
resolve(struct module* module)
{
    assert(module != NULL);

    struct resolver* const resolver = resolver_new(module);

    // Module namespace.
    if (module->cst->namespace != NULL) {
        sbuf(struct cst_identifier const) const identifiers =
            module->cst->namespace->identifiers;

        char const* nsname = NULL;
        char const* nsaddr = NULL;
        for (size_t i = 0; i < sbuf_count(identifiers); ++i) {
            char const* const name = identifiers[i].name;
            struct source_location const location = identifiers[i].location;

            nsname = qualified_name(nsname, name);
            nsaddr = qualified_addr(nsaddr, name);

            struct symbol_table* const module_table =
                symbol_table_new(resolver->current_symbol_table);
            struct symbol_table* const export_table =
                symbol_table_new(resolver->current_export_table);
            sbuf_push(context()->chilling_symbol_tables, module_table);
            sbuf_push(context()->chilling_symbol_tables, export_table);

            struct symbol* const module_nssymbol =
                symbol_new_namespace(location, nsname, module_table);
            struct symbol* const export_nssymbol =
                symbol_new_namespace(location, nsname, module_table);
            freeze(module_nssymbol);
            freeze(export_nssymbol);

            symbol_table_insert(
                resolver->current_symbol_table, name, module_nssymbol, false);
            symbol_table_insert(
                resolver->current_export_table, name, export_nssymbol, false);
            resolver->current_symbol_table = module_table;
            resolver->current_export_table = export_table;
        }

        resolver->current_symbol_name_prefix = nsname;
        resolver->current_static_addr_prefix = nsaddr;
    }

    // Resolve imports.
    for (size_t i = 0; i < sbuf_count(module->cst->imports); ++i) {
        resolve_import(resolver, module->cst->imports[i]);
    }

    // Resolve top-level declarations.
    sbuf(struct cst_decl const* const) const ordered = module->ordered;
    for (size_t i = 0; i < sbuf_count(ordered); ++i) {
        // Structs and unions have their symbols created before all other
        // declarations to allow for self referential and cross referential
        // struct/union declarations. These types are then completed as needed
        // based on their topological order. This is roughly equivalent to
        // forward declaring structs/unions in C.
        struct cst_decl const* const decl = module->ordered[i];
        struct symbol const* symbol = NULL;
        switch (decl->kind) {
        case CST_DECL_VARIABLE: /* fallthrough */
        case CST_DECL_CONSTANT: /* fallthrough */
        case CST_DECL_FUNCTION: /* fallthrough */
        case CST_DECL_ENUM: /* fallthrough */
        case CST_DECL_EXTEND: /* fallthrough */
        case CST_DECL_ALIAS: /* fallthrough */
        case CST_DECL_EXTERN_VARIABLE: /* fallthrough */
        case CST_DECL_EXTERN_FUNCTION: {
            continue;
        }

        case CST_DECL_STRUCT: {
            symbol = resolve_decl_struct(resolver, decl);
            break;
        }
        case CST_DECL_UNION: {
            symbol = resolve_decl_union(resolver, decl);
            break;
        }
        }

        // If this module declares a namespace then top-level declarations will
        // have been added under the (exported) module namespace and should
        // *not* be added to the module export table or global symbol table
        // using their unqualified names.
        if (module->cst->namespace == NULL) {
            symbol_table_insert(
                resolver->current_export_table, symbol->name, symbol, false);
            symbol_table_insert(
                context()->global_symbol_table, symbol->name, symbol, false);
        }
    }
    for (size_t i = 0; i < sbuf_count(ordered); ++i) {
        struct cst_decl const* const decl = module->ordered[i];

        // If the declaration was a non-template struct then it has already
        // been resolved and must now be completed.
        if (decl->kind == CST_DECL_STRUCT) {
            struct symbol const* const symbol = symbol_table_lookup_local(
                resolver->current_symbol_table, decl->name);
            assert(symbol != NULL);
            if (symbol->kind != SYMBOL_TYPE) {
                assert(symbol->kind == SYMBOL_TEMPLATE);
                continue;
            }

            complete_struct(resolver, symbol, decl->data.struct_.members);
            continue;
        }
        // If the declaration was a non-template union then it has already
        // been resolved and must now be completed.
        if (decl->kind == CST_DECL_UNION) {
            struct symbol const* const symbol = symbol_table_lookup_local(
                resolver->current_symbol_table, decl->name);
            assert(symbol != NULL);
            if (symbol->kind != SYMBOL_TYPE) {
                assert(symbol->kind == SYMBOL_TEMPLATE);
                continue;
            }

            complete_union(resolver, symbol, decl->data.union_.members);
            continue;
        }

        struct symbol const* const symbol =
            resolve_decl(resolver, module->ordered[i]);

        // If this module declares a namespace then top-level declarations will
        // have been added under the (exported) module namespace and should
        // *not* be added to the module export table or global symbol table
        // using their unqualified names.
        //
        // Similarly, extend declarations will have been added under the
        // extended type and should *not* be added to the module export table
        // or global symbol table *ever*.
        if (module->cst->namespace == NULL
            && module->ordered[i]->kind != CST_DECL_EXTEND) {
            symbol_table_insert(
                resolver->current_export_table, decl->name, symbol, false);
            symbol_table_insert(
                context()->global_symbol_table, decl->name, symbol, false);
        }
    }

    for (size_t i = 0; i < sbuf_count(resolver->incomplete_functions); ++i) {
        complete_function(resolver, resolver->incomplete_functions[i]);
    }

    resolver_del(resolver);
}
