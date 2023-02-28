// Copyright 2021-2023 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "sunder.h"

static struct type*
type_new(
    char const* name,
    uint64_t size,
    uint64_t align,
    struct symbol_table* symbols,
    enum type_kind kind)
{
    assert(name != NULL);

    struct type* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->name = name;
    self->size = size;
    self->align = align;
    self->symbols = symbols;
    self->kind = kind;
    return self;
}

struct type*
type_new_any(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    return type_new(
        context()->interned.any,
        SIZEOF_UNSIZED,
        ALIGNOF_UNSIZED,
        symbols,
        TYPE_ANY);
}

struct type*
type_new_void(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    return type_new(context()->interned.void_, 0u, 0u, symbols, TYPE_VOID);
}

struct type*
type_new_bool(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    return type_new(context()->interned.bool_, 1u, 1u, symbols, TYPE_BOOL);
}

struct type*
type_new_byte(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    return type_new(context()->interned.byte, 1u, 1u, symbols, TYPE_BYTE);
}

struct type*
type_new_u8(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self =
        type_new(context()->interned.u8, 1u, 1u, symbols, TYPE_U8);
    self->data.integer.min = context()->u8_min;
    self->data.integer.max = context()->u8_max;
    return self;
}

struct type*
type_new_s8(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self =
        type_new(context()->interned.s8, 1u, 1u, symbols, TYPE_S8);
    self->data.integer.min = context()->s8_min;
    self->data.integer.max = context()->s8_max;
    return self;
}

struct type*
type_new_u16(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self =
        type_new(context()->interned.u16, 2u, 2u, symbols, TYPE_U16);
    self->data.integer.min = context()->u16_min;
    self->data.integer.max = context()->u16_max;
    return self;
}

struct type*
type_new_s16(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self =
        type_new(context()->interned.s16, 2u, 2u, symbols, TYPE_S16);
    self->data.integer.min = context()->s16_min;
    self->data.integer.max = context()->s16_max;
    return self;
}

struct type*
type_new_u32(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self =
        type_new(context()->interned.u32, 4u, 4u, symbols, TYPE_U32);
    self->data.integer.min = context()->u32_min;
    self->data.integer.max = context()->u32_max;
    return self;
}

struct type*
type_new_s32(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self =
        type_new(context()->interned.s32, 4u, 4u, symbols, TYPE_S32);
    self->data.integer.min = context()->s32_min;
    self->data.integer.max = context()->s32_max;
    return self;
}

struct type*
type_new_u64(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self =
        type_new(context()->interned.u64, 8u, 8u, symbols, TYPE_U64);
    self->data.integer.min = context()->u64_min;
    self->data.integer.max = context()->u64_max;
    return self;
}

struct type*
type_new_s64(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self =
        type_new(context()->interned.s64, 8u, 8u, symbols, TYPE_S64);
    self->data.integer.min = context()->s64_min;
    self->data.integer.max = context()->s64_max;
    return self;
}

struct type*
type_new_usize(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self =
        type_new(context()->interned.usize, 8u, 8u, symbols, TYPE_USIZE);
    self->data.integer.min = context()->usize_min;
    self->data.integer.max = context()->usize_max;
    return self;
}

struct type*
type_new_ssize(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self =
        type_new(context()->interned.ssize, 8u, 8u, symbols, TYPE_SSIZE);
    self->data.integer.min = context()->ssize_min;
    self->data.integer.max = context()->ssize_max;
    return self;
}

struct type*
type_new_integer(void)
{
    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self = type_new(
        context()->interned.integer,
        SIZEOF_UNSIZED,
        ALIGNOF_UNSIZED,
        symbols,
        TYPE_INTEGER);
    self->data.integer.min = NULL;
    self->data.integer.max = NULL;
    return self;
}

struct type*
type_new_function(
    struct type const* const* parameter_types, struct type const* return_type)
{
    assert(return_type != NULL);

    struct string* const name_string = string_new_cstr("func(");
    if (sbuf_count(parameter_types) != 0) {
        string_append_cstr(name_string, parameter_types[0]->name);
    }
    for (size_t i = 1; i < sbuf_count(parameter_types); ++i) {
        string_append_fmt(name_string, ", %s", parameter_types[i]->name);
    }
    string_append_fmt(name_string, ") %s", return_type->name);
    char const* const name =
        intern(string_start(name_string), string_count(name_string));
    string_del(name_string);

    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self = type_new(name, 8u, 8u, symbols, TYPE_FUNCTION);
    self->data.function.parameter_types = parameter_types;
    self->data.function.return_type = return_type;
    return self;
}

struct type*
type_new_pointer(struct type const* base)
{
    assert(base != NULL);

    struct string* const name_string = string_new_fmt("*%s", base->name);
    char const* const name =
        intern(string_start(name_string), string_count(name_string));
    string_del(name_string);

    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self = type_new(name, 8u, 8u, symbols, TYPE_POINTER);
    self->data.pointer.base = base;
    return self;
}

struct type*
type_new_array(uint64_t count, struct type const* base)
{
    assert(base != NULL);

    struct string* const name_string =
        string_new_fmt("[%" PRIu64 "]%s", count, base->name);
    char const* const name =
        intern(string_start(name_string), string_count(name_string));
    string_del(name_string);

    uint64_t const size = count * base->size;
    assert((count == 0 || size / count == base->size) && "array size overflow");
    // https://en.cppreference.com/w/c/language/_Alignof
    // > Returns the alignment requirement of the type named by type-name. If
    // > type-name is an array type, the result is the alignment requirement of
    // > the array element type.
    uint64_t const align = base->align;

    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self = type_new(name, size, align, symbols, TYPE_ARRAY);
    self->data.array.count = count;
    self->data.array.base = base;
    return self;
}

struct type*
type_new_slice(struct type const* base)
{
    assert(base != NULL);

    struct string* const name_string = string_new_fmt("[]%s", base->name);
    char const* const name =
        intern(string_start(name_string), string_count(name_string));
    string_del(name_string);

    struct symbol_table* const symbols =
        symbol_table_new(context()->global_symbol_table);
    sbuf_push(context()->chilling_symbol_tables, symbols);

    struct type* const self = type_new(name, 8u * 2u, 8u, symbols, TYPE_SLICE);
    self->data.pointer.base = base;
    return self;
}

struct type*
type_new_struct(char const* name, struct symbol_table* symbols)
{
    struct type* const self = type_new(name, 0, 0, symbols, TYPE_STRUCT);
    self->data.struct_.is_complete = false;
    self->data.struct_.member_variables = NULL;
    return self;
}

long
type_struct_member_variable_index(struct type const* self, char const* name)
{
    assert(self != NULL);
    assert(self->kind == TYPE_STRUCT);
    assert(name != NULL);

    struct member_variable const* const member_variables =
        self->data.struct_.member_variables;
    for (size_t i = 0; i < sbuf_count(member_variables); ++i) {
        if (0 == strcmp(member_variables[i].name, name)) {
            return (long)i;
        }
    }

    return -1;
}

struct member_variable const*
type_struct_member_variable(struct type const* self, char const* name)
{
    assert(self != NULL);
    assert(self->kind == TYPE_STRUCT);
    assert(name != NULL);

    long const index = type_struct_member_variable_index(self, name);
    if (index < 0) {
        return NULL;
    }
    return &self->data.struct_.member_variables[index];
}

struct type const*
type_unique_function(
    struct type const* const* parameter_types, struct type const* return_type)
{
    assert(return_type != NULL);

    struct type* const type = type_new_function(parameter_types, return_type);
    struct symbol const* const existing =
        symbol_table_lookup(context()->global_symbol_table, type->name);
    if (existing != NULL) {
        xalloc(type, XALLOC_FREE);
        return symbol_xget_type(existing);
    }

    struct symbol* const symbol =
        symbol_new_type(context()->builtin.location, type);
    symbol_table_insert(
        context()->global_symbol_table, symbol->name, symbol, false);
    freeze(type);
    freeze(symbol);
    return type;
}

struct type const*
type_unique_pointer(struct type const* base)
{
    assert(base != NULL);

    struct type* const type = type_new_pointer(base);
    struct symbol const* const existing =
        symbol_table_lookup(context()->global_symbol_table, type->name);
    if (existing != NULL) {
        xalloc(type, XALLOC_FREE);
        return symbol_xget_type(existing);
    }

    struct symbol* const symbol =
        symbol_new_type(context()->builtin.location, type);
    symbol_table_insert(
        context()->global_symbol_table, symbol->name, symbol, false);
    freeze(type);
    freeze(symbol);
    return type;
}

struct type const*
type_unique_array(
    struct source_location location, uint64_t count, struct type const* base)
{
    assert(base != NULL);

    uint64_t const size = count * base->size;
    bool const size_overflow = count != 0 && size / count != base->size;
    if (size_overflow || size > SIZEOF_MAX) {
        fatal(location, "array size exceeds the maximum allowable object size");
    }

    struct type* const type = type_new_array(count, base);
    struct symbol const* const existing =
        symbol_table_lookup(context()->global_symbol_table, type->name);
    if (existing != NULL) {
        xalloc(type, XALLOC_FREE);
        return symbol_xget_type(existing);
    }

    struct symbol* const symbol =
        symbol_new_type(context()->builtin.location, type);
    symbol_table_insert(
        context()->global_symbol_table, symbol->name, symbol, false);
    freeze(type);
    freeze(symbol);
    return type;
}

struct type const*
type_unique_slice(struct type const* base)
{
    assert(base != NULL);

    struct type* const type = type_new_slice(base);
    struct symbol const* const existing =
        symbol_table_lookup(context()->global_symbol_table, type->name);
    if (existing != NULL) {
        xalloc(type, XALLOC_FREE);
        return symbol_xget_type(existing);
    }

    struct symbol* const symbol =
        symbol_new_type(context()->builtin.location, type);
    symbol_table_insert(
        context()->global_symbol_table, symbol->name, symbol, false);
    freeze(type);
    freeze(symbol);
    return type;
}

struct symbol const*
type_member_symbol(struct type const* self, char const* name)
{
    assert(self != NULL);
    assert(name != NULL);

    return symbol_table_lookup_local(self->symbols, name);
}

struct function const*
type_member_function(struct type const* self, char const* name)
{
    assert(self != NULL);
    assert(name != NULL);

    struct symbol const* const symbol = type_member_symbol(self, name);
    if (symbol == NULL) {
        return NULL;
    }
    if (symbol->kind != SYMBOL_FUNCTION) {
        return NULL;
    }

    return symbol->data.function;
}

bool
type_is_int(struct type const* self)
{
    assert(self != NULL);

    enum type_kind const kind = self->kind;
    return kind == TYPE_U8 || kind == TYPE_S8 || kind == TYPE_U16
        || kind == TYPE_S16 || kind == TYPE_U32 || kind == TYPE_S32
        || kind == TYPE_U64 || kind == TYPE_S64 || kind == TYPE_USIZE
        || kind == TYPE_SSIZE || kind == TYPE_INTEGER;
}

bool
type_is_uint(struct type const* self)
{
    assert(self != NULL);

    enum type_kind const kind = self->kind;
    return kind == TYPE_U8 || kind == TYPE_U16 || kind == TYPE_U32
        || kind == TYPE_U64 || kind == TYPE_USIZE;
}

bool
type_is_sint(struct type const* self)
{
    assert(self != NULL);

    enum type_kind const kind = self->kind;
    return kind == TYPE_S8 || kind == TYPE_S16 || kind == TYPE_S32
        || kind == TYPE_S64 || kind == TYPE_SSIZE;
}

bool
type_can_compare_equality(struct type const* self)
{
    assert(self != NULL);

    enum type_kind const kind = self->kind;
    return kind == TYPE_BOOL || kind == TYPE_BYTE || type_is_int(self)
        || kind == TYPE_FUNCTION || kind == TYPE_POINTER;
}

bool
type_can_compare_order(struct type const* self)
{
    assert(self != NULL);

    enum type_kind const kind = self->kind;
    return kind == TYPE_BOOL || kind == TYPE_BYTE || type_is_int(self)
        || kind == TYPE_POINTER;
}

struct address
address_init_absolute(uint64_t absolute)
{
    struct address self = {0};
    self.kind = ADDRESS_ABSOLUTE;
    self.data.absolute = absolute;
    return self;
}

struct address
address_init_static(char const* name, uint64_t offset)
{
    assert(name != NULL);

    struct address self = {0};
    self.kind = ADDRESS_STATIC;
    self.data.static_.name = name;
    self.data.static_.offset = offset;
    return self;
}

struct address
address_init_local(int rbp_offset)
{
    struct address self = {0};
    self.kind = ADDRESS_LOCAL;
    self.data.local.rbp_offset = rbp_offset;
    return self;
}

struct address*
address_new(struct address from)
{
    struct address* const self = xalloc(NULL, sizeof(*self));
    *self = from;
    return self;
}

char const*
symbol_kind_to_cstr(enum symbol_kind kind)
{
    switch (kind) {
    case SYMBOL_TYPE:
        return "type";
    case SYMBOL_VARIABLE:
        return "variable";
    case SYMBOL_CONSTANT:
        return "constant";
    case SYMBOL_FUNCTION:
        return "function";
    case SYMBOL_TEMPLATE:
        return "template";
    case SYMBOL_NAMESPACE:
        return "namespace";
    }

    UNREACHABLE();
    return NULL;
}

struct symbol*
symbol_new_type(struct source_location location, struct type const* type)
{
    assert(type != NULL);

    struct symbol* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = SYMBOL_TYPE;
    self->location = location;
    self->name = type->name;
    self->data.type = type;
    return self;
}

struct symbol*
symbol_new_variable(
    struct source_location location,
    char const* name,
    struct object const* object)
{
    assert(name != NULL);
    assert(object != NULL);
    assert(!(object->is_extern && object->value != NULL));

    struct symbol* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = SYMBOL_VARIABLE;
    self->location = location;
    self->name = name;
    self->data.variable = object;
    return self;
}

struct symbol*
symbol_new_constant(
    struct source_location location,
    char const* name,
    struct object const* object)
{
    assert(name != NULL);
    assert(object != NULL);
    assert(!object->is_extern);

    struct symbol* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = SYMBOL_CONSTANT;
    self->location = location;
    self->name = name;
    self->data.constant = object;
    return self;
}

struct symbol*
symbol_new_function(
    struct source_location location,
    char const* name,
    struct function const* function)
{
    assert(function != NULL);
    assert(function->type->kind == TYPE_FUNCTION);

    struct symbol* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = SYMBOL_FUNCTION;
    self->location = location;
    self->name = name;
    self->data.function = function;
    return self;
}

struct symbol*
symbol_new_template(
    struct source_location location,
    char const* name,
    struct cst_decl const* decl,
    char const* symbol_name_prefix,
    char const* symbol_addr_prefix,
    struct symbol_table* parent_symbol_table,
    struct symbol_table* symbols)
{
    assert(name != NULL);
    assert(decl != NULL);
    assert(symbols != NULL);

    struct symbol* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = SYMBOL_TEMPLATE;
    self->location = location;
    self->name = name;
    self->data.template.decl = decl;
    self->data.template.symbol_name_prefix = symbol_name_prefix;
    self->data.template.symbol_addr_prefix = symbol_addr_prefix;
    self->data.template.parent_symbol_table = parent_symbol_table;
    self->data.template.symbols = symbols;
    return self;
}

struct symbol*
symbol_new_namespace(
    struct source_location location,
    char const* name,
    struct symbol_table* symbols)
{
    assert(name != NULL);
    assert(symbols != NULL);

    struct symbol* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = SYMBOL_NAMESPACE;
    self->location = location;
    self->name = name;
    self->data.namespace.symbols = symbols;
    return self;
}

struct address const*
symbol_get_address(struct symbol const* self)
{
    assert(self != NULL);

    switch (self->kind) {
    case SYMBOL_VARIABLE: {
        return self->data.variable->address;
    }
    case SYMBOL_CONSTANT: {
        return self->data.constant->address;
    }
    case SYMBOL_FUNCTION: {
        return self->data.function->address;
    }
    case SYMBOL_TYPE: /* fallthrough */
    case SYMBOL_TEMPLATE: /* fallthrough */
    case SYMBOL_NAMESPACE: {
        return NULL;
    }
    }

    return NULL;
}

struct type const*
symbol_xget_type(struct symbol const* self)
{
    assert(self != NULL);

    switch (self->kind) {
    case SYMBOL_TYPE: {
        return self->data.type;
    }
    case SYMBOL_VARIABLE: {
        return self->data.variable->type;
    }
    case SYMBOL_CONSTANT: {
        return self->data.constant->type;
    }
    case SYMBOL_FUNCTION: {
        return self->data.function->type;
    }
    case SYMBOL_TEMPLATE: /* fallthrough */
    case SYMBOL_NAMESPACE: {
        UNREACHABLE();
    }
    }

    return NULL;
}

struct address const*
symbol_xget_address(struct symbol const* self)
{
    assert(self != NULL);

    struct address const* const address = symbol_get_address(self);
    if (address == NULL) {
        UNREACHABLE();
    }

    return address;
}

struct value const*
symbol_xget_value(struct source_location location, struct symbol const* self)
{
    switch (self->kind) {
    case SYMBOL_VARIABLE: {
        if (self->data.variable->value == NULL) {
            UNREACHABLE();
        }
        return self->data.variable->value;
    }
    case SYMBOL_CONSTANT: {
        if (self->data.constant->value == NULL) {
            fatal(
                location,
                "constant `%s` of type `%s` is uninitialized",
                self->name,
                self->data.constant->type->name);
        }
        return self->data.constant->value;
    }
    case SYMBOL_FUNCTION: {
        return self->data.function->value;
    }
    case SYMBOL_TYPE: /* fallthrough */
    case SYMBOL_TEMPLATE: /* fallthrough */
    case SYMBOL_NAMESPACE: {
        UNREACHABLE();
    }
    }

    return NULL;
}

struct symbol_table*
symbol_table_new(struct symbol_table const* parent)
{
    struct symbol_table* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));

    self->parent = parent;
    self->elements = NULL;

    return self;
}

void
symbol_table_freeze(struct symbol_table* self)
{
    assert(self != NULL);

    freeze(self);
    sbuf_freeze(self->elements);
}

void
symbol_table_insert(
    struct symbol_table* self,
    char const* name,
    struct symbol const* symbol,
    bool allow_redeclaration)
{
    assert(self != NULL);
    assert(name != NULL);
    assert(symbol != NULL);

    if (!allow_redeclaration) {
        struct symbol const* const local =
            symbol_table_lookup_local(self, name);
        if (local != NULL) {
            fatal(
                symbol->location,
                "redeclaration of `%s` previously declared at [%s:%zu]",
                name,
                local->location.path,
                local->location.line);
        }
    }

    sbuf_push(
        self->elements,
        (struct symbol_table_element){.name = name, .symbol = symbol});
}

struct symbol const*
symbol_table_lookup(struct symbol_table const* self, char const* name)
{
    assert(self != NULL);
    assert(name != NULL);

    struct symbol const* const local = symbol_table_lookup_local(self, name);
    if (local != NULL) {
        return local;
    }
    if (self->parent != NULL) {
        return symbol_table_lookup(self->parent, name);
    }
    return NULL;
}

struct symbol const*
symbol_table_lookup_local(struct symbol_table const* self, char const* name)
{
    assert(self != NULL);
    assert(name != NULL);

    for (size_t i = sbuf_count(self->elements); i--;) {
        if (self->elements[i].name == name) {
            // XXX: Evil const cast.
            ((struct symbol*)self->elements[i].symbol)->uses += 1;
            return self->elements[i].symbol;
        }
    }

    return NULL;
}

struct block*
block_new(
    struct source_location location,
    struct symbol_table* symbol_table,
    struct stmt const* const* stmts,
    struct stmt const* defer_begin,
    struct stmt const* defer_end)
{
    assert(symbol_table != NULL);
    assert(defer_begin == NULL || defer_begin->kind == STMT_DEFER);
    assert(defer_end == NULL || defer_end->kind == STMT_DEFER);

    struct block* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->symbol_table = symbol_table;
    self->stmts = stmts;
    self->defer_begin = defer_begin;
    self->defer_end = defer_end;
    return self;
}

struct conditional*
conditional_new(
    struct source_location location,
    struct expr const* condition,
    struct block body)
{
    struct conditional* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->condition = condition;
    self->body = body;
    return self;
}

static struct stmt*
stmt_new(struct source_location location, enum stmt_kind kind)
{
    struct stmt* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->kind = kind;
    return self;
}

struct stmt*
stmt_new_defer(
    struct source_location location, struct stmt const* prev, struct block body)
{
    assert(prev == NULL || prev->kind == STMT_DEFER);

    struct stmt* const self = stmt_new(location, STMT_DEFER);
    self->data.defer.prev = prev;
    self->data.defer.body = body;
    return self;
}

struct stmt*
stmt_new_if(struct conditional const* conditionals)
{
    assert(sbuf_count(conditionals) > 0u);

    struct stmt* const self = stmt_new(conditionals[0].location, STMT_IF);
    self->data.if_.conditionals = conditionals;
    return self;
}

struct stmt*
stmt_new_for_range(
    struct source_location location,
    struct symbol const* loop_variable,
    struct expr const* begin,
    struct expr const* end,
    struct block body)
{
    assert(loop_variable != NULL);
    assert(loop_variable->kind == SYMBOL_VARIABLE);
    assert(symbol_xget_type(loop_variable) == context()->builtin.usize);
    assert(begin != NULL);
    assert(begin->type == context()->builtin.usize);
    assert(end != NULL);
    assert(end->type == context()->builtin.usize);

    struct stmt* const self = stmt_new(location, STMT_FOR_RANGE);
    self->data.for_range.loop_variable = loop_variable;
    self->data.for_range.begin = begin;
    self->data.for_range.end = end;
    self->data.for_range.body = body;
    return self;
}

struct stmt*
stmt_new_for_expr(
    struct source_location location, struct expr const* expr, struct block body)
{
    assert(expr != NULL);

    struct stmt* const self = stmt_new(location, STMT_FOR_EXPR);
    self->data.for_expr.expr = expr;
    self->data.for_expr.body = body;
    return self;
}

struct stmt*
stmt_new_break(
    struct source_location location,
    struct stmt const* defer_begin,
    struct stmt const* defer_end)
{
    assert(defer_begin == NULL || defer_begin->kind == STMT_DEFER);
    assert(defer_end == NULL || defer_end->kind == STMT_DEFER);

    struct stmt* const self = stmt_new(location, STMT_BREAK);
    self->data.break_.defer_begin = defer_begin;
    self->data.break_.defer_end = defer_end;
    return self;
}

struct stmt*
stmt_new_continue(
    struct source_location location,
    struct stmt const* defer_begin,
    struct stmt const* defer_end)
{
    assert(defer_begin == NULL || defer_begin->kind == STMT_DEFER);
    assert(defer_end == NULL || defer_end->kind == STMT_DEFER);

    struct stmt* const self = stmt_new(location, STMT_CONTINUE);
    self->data.continue_.defer_begin = defer_begin;
    self->data.continue_.defer_end = defer_end;
    return self;
}

struct stmt*
stmt_new_return(
    struct source_location location,
    struct expr const* expr,
    struct stmt const* defer)
{
    struct stmt* const self = stmt_new(location, STMT_RETURN);
    self->data.return_.expr = expr;
    self->data.return_.defer = defer;
    return self;
}

struct stmt*
stmt_new_assign(
    struct source_location location,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    struct stmt* const self = stmt_new(location, STMT_ASSIGN);
    self->data.assign.lhs = lhs;
    self->data.assign.rhs = rhs;
    return self;
}

struct stmt*
stmt_new_expr(struct source_location location, struct expr const* expr)
{
    assert(expr != NULL);

    struct stmt* const self = stmt_new(location, STMT_EXPR);
    self->data.expr = expr;
    return self;
}

static struct expr*
expr_new(
    struct source_location location,
    struct type const* type,
    enum expr_kind kind)
{
    assert(type != NULL);

    struct expr* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->type = type;
    self->kind = kind;
    return self;
}

struct expr*
expr_new_symbol(struct source_location location, struct symbol const* symbol)
{
    assert(symbol != NULL);
    assert(symbol->kind != SYMBOL_TYPE);

    struct expr* const self =
        expr_new(location, symbol_xget_type(symbol), EXPR_SYMBOL);
    self->data.symbol = symbol;
    return self;
}

struct expr*
expr_new_value(struct source_location location, struct value const* value)
{
    assert(value != NULL);

    struct expr* const self = expr_new(location, value->type, EXPR_VALUE);
    self->data.value = value;
    return self;
}

struct expr*
expr_new_bytes(
    struct source_location location,
    struct address const* address,
    size_t count)
{
    assert(address != NULL);

    struct type const* const type = type_unique_slice(context()->builtin.byte);
    struct expr* const self = expr_new(location, type, EXPR_BYTES);
    self->data.bytes.address = address;
    self->data.bytes.count = count;
    return self;
}

struct expr*
expr_new_array_list(
    struct source_location location,
    struct type const* type,
    struct expr const* const* elements,
    struct expr const* ellipsis)
{
    assert(type != NULL);
    assert(type->kind == TYPE_ARRAY);

    struct expr* const self = expr_new(location, type, EXPR_ARRAY_LIST);
    self->data.array_list.elements = elements;
    self->data.array_list.ellipsis = ellipsis;
    return self;
}

struct expr*
expr_new_slice_list(
    struct source_location location,
    struct type const* type,
    struct symbol const* array_symbol,
    struct expr const* const* elements)
{
    assert(type != NULL);
    assert(type->kind == TYPE_SLICE);

    struct expr* const self = expr_new(location, type, EXPR_SLICE_LIST);
    self->data.slice_list.array_symbol = array_symbol;
    self->data.slice_list.elements = elements;
    return self;
}

struct expr*
expr_new_slice(
    struct source_location location,
    struct type const* type,
    struct expr const* pointer,
    struct expr const* count)
{
    assert(type != NULL);
    assert(type->kind == TYPE_SLICE);
    assert(pointer != NULL);
    assert(count != NULL);

    struct expr* const self = expr_new(location, type, EXPR_SLICE);
    self->data.slice.pointer = pointer;
    self->data.slice.count = count;
    return self;
}

struct expr*
expr_new_struct(
    struct source_location location,
    struct type const* type,
    struct expr const* const* member_variables)
{
    assert(type != NULL);
    assert(type->kind == TYPE_STRUCT);

    struct expr* const self = expr_new(location, type, EXPR_STRUCT);
    self->data.struct_.member_variables = member_variables;
    return self;
}

struct expr*
expr_new_cast(
    struct source_location location,
    struct type const* type,
    struct expr const* expr)
{
    assert(type != NULL);
    assert(expr != NULL);

    struct expr* const self = expr_new(location, type, EXPR_CAST);
    self->data.cast.expr = expr;
    return self;
}

struct expr*
expr_new_call(
    struct source_location location,
    struct expr const* function,
    struct expr const* const* arguments)
{
    assert(function != NULL);
    assert(function->type->kind == TYPE_FUNCTION);

    struct type const* const type = function->type->data.function.return_type;
    struct expr* const self = expr_new(location, type, EXPR_CALL);
    self->data.call.function = function;
    self->data.call.arguments = arguments;
    return self;
}

struct expr*
expr_new_access_index(
    struct source_location location,
    struct expr const* lhs,
    struct expr const* idx)
{
    assert(lhs != NULL);
    assert(lhs->type->kind == TYPE_ARRAY || lhs->type->kind == TYPE_SLICE);
    assert(idx != NULL);

    if (lhs->type->kind == TYPE_ARRAY) {
        struct type const* const type = lhs->type->data.array.base;
        struct expr* const self = expr_new(location, type, EXPR_ACCESS_INDEX);
        self->data.access_index.lhs = lhs;
        self->data.access_index.idx = idx;
        return self;
    }

    if (lhs->type->kind == TYPE_SLICE) {
        struct type const* const type = lhs->type->data.slice.base;
        struct expr* const self = expr_new(location, type, EXPR_ACCESS_INDEX);
        self->data.access_index.lhs = lhs;
        self->data.access_index.idx = idx;
        return self;
    }

    UNREACHABLE();
    return NULL;
}

struct expr*
expr_new_access_slice(
    struct source_location location,
    struct expr const* lhs,
    struct expr const* begin,
    struct expr const* end)
{
    assert(lhs != NULL);
    assert(lhs->type->kind == TYPE_ARRAY || lhs->type->kind == TYPE_SLICE);
    assert(begin != NULL);
    assert(end != NULL);

    if (lhs->type->kind == TYPE_ARRAY) {
        struct type const* const type =
            type_unique_slice(lhs->type->data.array.base);
        struct expr* const self = expr_new(location, type, EXPR_ACCESS_SLICE);
        self->data.access_slice.lhs = lhs;
        self->data.access_slice.begin = begin;
        self->data.access_slice.end = end;
        return self;
    }

    if (lhs->type->kind == TYPE_SLICE) {
        struct type const* const type =
            type_unique_slice(lhs->type->data.slice.base);
        struct expr* const self = expr_new(location, type, EXPR_ACCESS_SLICE);
        self->data.access_slice.lhs = lhs;
        self->data.access_slice.begin = begin;
        self->data.access_slice.end = end;
        return self;
    }

    UNREACHABLE();
    return NULL;
}

struct expr*
expr_new_access_member_variable(
    struct source_location location,
    struct expr const* lhs,
    struct member_variable const* member_variable)
{
    assert(lhs != NULL);
    assert(lhs->type->kind == TYPE_STRUCT);
    assert(member_variable != NULL);

    struct expr* const self =
        expr_new(location, member_variable->type, EXPR_ACCESS_MEMBER_VARIABLE);

    self->data.access_member_variable.lhs = lhs;
    self->data.access_member_variable.member_variable = member_variable;
    return self;
}

struct expr*
expr_new_sizeof(struct source_location location, struct type const* rhs)
{
    assert(rhs != NULL);

    struct expr* const self =
        expr_new(location, context()->builtin.usize, EXPR_SIZEOF);
    self->data.sizeof_.rhs = rhs;
    return self;
}

struct expr*
expr_new_alignof(struct source_location location, struct type const* rhs)
{
    assert(rhs != NULL);

    struct expr* const self =
        expr_new(location, context()->builtin.usize, EXPR_ALIGNOF);
    self->data.sizeof_.rhs = rhs;
    return self;
}

struct expr*
expr_new_unary(
    struct source_location location,
    struct type const* type,
    enum uop_kind op,
    struct expr const* rhs)
{
    assert(type != NULL);
    assert(rhs != NULL);

    struct expr* const self = expr_new(location, type, EXPR_UNARY);
    self->data.unary.op = op;
    self->data.unary.rhs = rhs;
    return self;
}

struct expr*
expr_new_binary(
    struct source_location location,
    struct type const* type,
    enum bop_kind op,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(type != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    struct expr* const self = expr_new(location, type, EXPR_BINARY);
    self->data.binary.op = op;
    self->data.binary.lhs = lhs;
    self->data.binary.rhs = rhs;
    return self;
}

bool
expr_is_lvalue(struct expr const* self)
{
    assert(self != NULL);

    switch (self->kind) {
    case EXPR_SYMBOL: {
        switch (self->data.symbol->kind) {
        case SYMBOL_TYPE: /* fallthrough */
        case SYMBOL_TEMPLATE:
        case SYMBOL_NAMESPACE:
            UNREACHABLE();
        case SYMBOL_VARIABLE: /* fallthrough */
        case SYMBOL_CONSTANT:
            return true;
        case SYMBOL_FUNCTION:
            return false;
        }
        UNREACHABLE();
    }
    case EXPR_ACCESS_INDEX: {
        return self->data.access_index.lhs->type->kind == TYPE_SLICE
            || expr_is_lvalue(self->data.access_index.lhs);
    }
    case EXPR_ACCESS_MEMBER_VARIABLE: {
        return expr_is_lvalue(self->data.access_member_variable.lhs);
    }
    case EXPR_UNARY: {
        return self->data.unary.op == UOP_DEREFERENCE;
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
        return false;
    }
    }

    UNREACHABLE();
    return false;
}

struct object*
object_new(
    struct type const* type,
    struct address const* address,
    struct value const* value)
{
    assert(type != NULL);
    assert(address != NULL);

    struct object* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->type = type;
    self->address = address;
    self->value = value;
    self->is_extern = false;
    return self;
}

struct function*
function_new(struct type const* type, struct address const* address)
{
    assert(type != NULL);
    assert(type->kind == TYPE_FUNCTION);
    assert(address != NULL);
    assert(address->kind == ADDRESS_STATIC);

    struct function* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->type = type;
    self->address = address;
    return self;
}

static struct value*
value_new(struct type const* type)
{
    assert(type != NULL);

    struct value* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->type = type;
    return self;
}

struct value*
value_new_boolean(bool boolean)
{
    struct type const* const type = context()->builtin.bool_;
    struct value* self = value_new(type);
    self->data.boolean = boolean;
    return self;
}

struct value*
value_new_byte(uint8_t byte)
{
    struct type const* const type = context()->builtin.byte;
    struct value* self = value_new(type);
    self->data.byte = byte;
    return self;
}

struct value*
value_new_integer(struct type const* type, struct bigint* integer)
{
    assert(type != NULL);
    assert(type_is_int(type));
    assert(integer != NULL);

    bool const is_sized = type_is_uint(type) || type_is_sint(type);
    assert(!is_sized || bigint_cmp(integer, type->data.integer.min) >= 0);
    assert(!is_sized || bigint_cmp(integer, type->data.integer.max) <= 0);
    (void)is_sized;

    struct value* self = value_new(type);
    self->data.integer = integer;
    return self;
}

struct value*
value_new_function(struct function const* function)
{
    assert(function != NULL);

    struct value* self = value_new(function->type);
    self->data.function = function;
    return self;
}

struct value*
value_new_pointer(struct type const* type, struct address address)
{
    assert(type != NULL);
    assert(type->kind == TYPE_POINTER);

    struct value* self = value_new(type);
    self->data.pointer = address;
    return self;
}

struct value*
value_new_array(
    struct type const* type, struct value** elements, struct value* ellipsis)
{
    assert(type != NULL);
    assert(type->kind == TYPE_ARRAY);
    assert(type->data.array.count == sbuf_count(elements) || ellipsis != NULL);
    for (size_t i = 0; i < sbuf_count(elements); ++i) {
        assert(elements[i]->type == type->data.array.base);
    }
    if (ellipsis != NULL) {
        assert(ellipsis->type == type->data.array.base);
    }

    struct value* self = value_new(type);
    self->data.array.elements = elements;
    self->data.array.ellipsis = ellipsis;
    return self;
}

struct value*
value_new_slice(
    struct type const* type, struct value* pointer, struct value* count)
{
    assert(type != NULL);
    assert(type->kind == TYPE_SLICE);
    assert(pointer != NULL);
    assert(pointer->type->kind == TYPE_POINTER);
    assert(count != NULL);
    assert(count->type->kind == TYPE_USIZE);
    assert(bigint_cmp(count->data.integer, BIGINT_ZERO) >= 0);

    assert(type->data.slice.base == pointer->type->data.pointer.base);

    struct value* self = value_new(type);
    self->data.slice.pointer = pointer;
    self->data.slice.count = count;
    return self;
}

struct value*
value_new_struct(struct type const* type)
{
    assert(type != NULL);
    assert(type->kind == TYPE_STRUCT);

    struct value* self = value_new(type);

    size_t const member_variables_count =
        sbuf_count(type->data.struct_.member_variables);
    self->data.struct_.member_variables = NULL;
    for (size_t i = 0; i < member_variables_count; ++i) {
        sbuf_push(self->data.struct_.member_variables, NULL);
    }

    return self;
}

void
value_del(struct value* self)
{
    assert(self != NULL);

    switch (self->type->kind) {
    case TYPE_ANY: /* fallthrough */
    case TYPE_VOID: {
        UNREACHABLE();
    }
    case TYPE_BOOL: {
        break;
    }
    case TYPE_BYTE: {
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
    case TYPE_SSIZE: /* fallthrough */
    case TYPE_INTEGER: {
        bigint_del(self->data.integer);
        break;
    }
    case TYPE_FUNCTION: {
        break;
    }
    case TYPE_POINTER: {
        break;
    }
    case TYPE_ARRAY: {
        sbuf(struct value*) const elements = self->data.array.elements;
        for (size_t i = 0; i < sbuf_count(elements); ++i) {
            value_del(elements[i]);
        }
        sbuf_fini(elements);
        if (self->data.array.ellipsis != NULL) {
            value_del(self->data.array.ellipsis);
        }
        break;
    }
    case TYPE_SLICE: {
        value_del(self->data.slice.pointer);
        value_del(self->data.slice.count);
        break;
    }
    case TYPE_STRUCT: {
        size_t const member_variables_count =
            sbuf_count(self->type->data.struct_.member_variables);
        for (size_t i = 0; i < member_variables_count; ++i) {
            struct value** pvalue = self->data.struct_.member_variables + i;
            if (*pvalue == NULL) {
                // Value was never initialized.
                continue;
            }
            value_del(*pvalue);
        }
        sbuf_fini(self->data.struct_.member_variables);
        break;
    }
    }

    memset(self, 0x00, sizeof(*self));
    xalloc(self, XALLOC_FREE);
}

void
value_freeze(struct value* self)
{
    assert(self != NULL);

    freeze(self);
    switch (self->type->kind) {
    case TYPE_ANY: /* fallthrough */
    case TYPE_VOID: {
        UNREACHABLE();
    }
    case TYPE_BOOL: {
        return;
    }
    case TYPE_BYTE: {
        return;
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
    case TYPE_SSIZE: /* fallthrough */
    case TYPE_INTEGER: {
        bigint_freeze(self->data.integer);
        return;
    }
    case TYPE_FUNCTION: {
        return;
    }
    case TYPE_POINTER: {
        return;
    }
    case TYPE_ARRAY: {
        sbuf(struct value*) const elements = self->data.array.elements;
        struct value* const ellipsis = self->data.array.ellipsis;
        sbuf_freeze(elements);
        for (size_t i = 0; i < sbuf_count(elements); ++i) {
            value_freeze(elements[i]);
        }
        if (ellipsis != NULL) {
            value_freeze(ellipsis);
        }
        return;
    }
    case TYPE_SLICE: {
        value_freeze(self->data.slice.pointer);
        value_freeze(self->data.slice.count);
        return;
    }
    case TYPE_STRUCT: {
        size_t const member_variables_count =
            sbuf_count(self->type->data.struct_.member_variables);
        for (size_t i = 0; i < member_variables_count; ++i) {
            struct value* value = self->data.struct_.member_variables[i];
            if (value != NULL) {
                value_freeze(value);
            }
        }
        sbuf_freeze(self->data.struct_.member_variables);
        return;
    }
    }

    UNREACHABLE();
}

struct value*
value_clone(struct value const* self)
{
    assert(self != NULL);

    switch (self->type->kind) {
    case TYPE_ANY: /* fallthrough */
    case TYPE_VOID: {
        UNREACHABLE();
    }
    case TYPE_BOOL: {
        return value_new_boolean(self->data.boolean);
    }
    case TYPE_BYTE: {
        return value_new_byte(self->data.byte);
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
    case TYPE_SSIZE: /* fallthrough */
    case TYPE_INTEGER: {
        return value_new_integer(self->type, bigint_new(self->data.integer));
    }
    case TYPE_FUNCTION: {
        return value_new_function(self->data.function);
    }
    case TYPE_POINTER: {
        return value_new_pointer(self->type, self->data.pointer);
    }
    case TYPE_ARRAY: {
        sbuf(struct value*) const elements = self->data.array.elements;
        struct value* const ellipsis = self->data.array.ellipsis;
        sbuf(struct value*) cloned_elements = NULL;
        for (size_t i = 0; i < sbuf_count(elements); ++i) {
            sbuf_push(cloned_elements, value_clone(elements[i]));
        }
        return value_new_array(self->type, cloned_elements, ellipsis);
    }
    case TYPE_SLICE: {
        return value_new_slice(
            self->type,
            value_clone(self->data.slice.pointer),
            value_clone(self->data.slice.count));
    }
    case TYPE_STRUCT: {
        struct value* const new = value_new_struct(self->type);
        size_t const member_variables_count =
            sbuf_count(self->type->data.struct_.member_variables);
        for (size_t i = 0; i < member_variables_count; ++i) {
            new->data.struct_.member_variables[i] = NULL;
            if (self->data.struct_.member_variables[i] != NULL) {
                new->data.struct_.member_variables[i] =
                    value_clone(self->data.struct_.member_variables[i]);
            }
        }
        return new;
    }
    }

    UNREACHABLE();
    return NULL;
}

struct value const*
value_get_member_variable(
    struct source_location location, struct value const* self, char const* name)
{
    assert(self != NULL);
    assert(name != NULL);

    long const index = type_struct_member_variable_index(self->type, name);
    if (index < 0) {
        // Should never happen.
        fatal(location, "type `%s` has no member `%s`", self->type->name, name);
    }
    return self->data.struct_.member_variables[index];
}

struct value const*
value_xget_member_variable(
    struct source_location location, struct value const* self, char const* name)
{
    assert(self != NULL);
    assert(name != NULL);

    struct value const* const value =
        value_get_member_variable(location, self, name);
    if (value == NULL) {
        fatal(
            location,
            "member `%s` of type `%s` is uninitialized",
            name,
            self->type->name);
    }
    return value;
}

void
value_set_member(struct value* self, char const* name, struct value* value)
{
    assert(self != NULL);
    assert(name != NULL);
    assert(value != NULL);

    long const index = type_struct_member_variable_index(self->type, name);
    if (index < 0) {
        // Should never happen.
        fatal(
            NO_LOCATION,
            "type `%s` has no member `%s`",
            self->type->name,
            name);
    }
    struct value** const pvalue = self->data.struct_.member_variables + index;

    // De-initialize the value associated with the member if that member has
    // already been initialized.
    if (*pvalue != NULL) {
        value_del(*pvalue);
    }

    *pvalue = value;
}

bool
value_eq(struct value const* lhs, struct value const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);
    assert(lhs->type == rhs->type);

    struct type const* const type = lhs->type; // Arbitrarily use lhs.
    switch (type->kind) {
    case TYPE_ANY: {
        UNREACHABLE();
    }
    case TYPE_VOID: {
        return true;
    }
    case TYPE_BOOL: {
        return lhs->data.boolean == rhs->data.boolean;
    }
    case TYPE_BYTE: {
        return lhs->data.byte == rhs->data.byte;
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
    case TYPE_SSIZE: /* fallthrough */
    case TYPE_INTEGER: {
        return bigint_cmp(lhs->data.integer, rhs->data.integer) == 0;
    }
    case TYPE_FUNCTION: {
        return lhs->data.function == rhs->data.function;
    }
    case TYPE_POINTER: {
        // Pointer comparisons are tricky and have many edge cases to think
        // about (dangling pointers, absolute vs stack vs global addressing,
        // etc.). For now the ordering of pointers is undefined during
        // compile-time computations. In the future an easy first pass could
        // include allowing ordering operators on global pointers with the same
        // base address so that comparisons between pointers to elements in the
        // same global array would be allowed.
        UNREACHABLE(); // illegal
    }
    case TYPE_ARRAY: /* fallthrough */
    case TYPE_SLICE: /* fallthrough */
    case TYPE_STRUCT: {
        UNREACHABLE(); // illegal
    }
    }

    UNREACHABLE();
    return false;
}

bool
value_lt(struct value const* lhs, struct value const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);
    assert(lhs->type == rhs->type);

    struct type const* const type = lhs->type; // Arbitrarily use lhs.
    switch (type->kind) {
    case TYPE_ANY: {
        UNREACHABLE();
    }
    case TYPE_VOID: {
        return true;
    }
    case TYPE_BOOL: {
        return lhs->data.boolean < rhs->data.boolean;
    }
    case TYPE_BYTE: {
        return lhs->data.byte < rhs->data.byte;
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
    case TYPE_SSIZE: /* fallthrough */
    case TYPE_INTEGER: {
        return bigint_cmp(lhs->data.integer, rhs->data.integer) < 0;
    }
    case TYPE_POINTER: {
        UNREACHABLE(); // illegal (see comment in value_eq)
    }
    case TYPE_FUNCTION: /* fallthrough */
    case TYPE_ARRAY: /* fallthrough */
    case TYPE_SLICE: /* fallthrough */
    case TYPE_STRUCT: {
        UNREACHABLE(); // illegal
    }
    }

    UNREACHABLE();
    return false;
}

bool
value_gt(struct value const* lhs, struct value const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);
    assert(lhs->type == rhs->type);

    struct type const* const type = lhs->type; // Arbitrarily use lhs.
    switch (type->kind) {
    case TYPE_ANY: {
        UNREACHABLE();
    }
    case TYPE_VOID: {
        return true;
    }
    case TYPE_BOOL: {
        return lhs->data.boolean > rhs->data.boolean;
    }
    case TYPE_BYTE: {
        return lhs->data.byte > rhs->data.byte;
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
    case TYPE_SSIZE: /* fallthrough */
    case TYPE_INTEGER: {
        return bigint_cmp(lhs->data.integer, rhs->data.integer) > 0;
    }
    case TYPE_POINTER: {
        UNREACHABLE(); // illegal (see comment in value_eq)
    }
    case TYPE_FUNCTION: /* fallthrough */
    case TYPE_ARRAY: /* fallthrough */
    case TYPE_SLICE: /* fallthrough */
    case TYPE_STRUCT: {
        UNREACHABLE(); // illegal
    }
    }

    UNREACHABLE();
    return false;
}

uint8_t*
value_to_new_bytes(struct value const* value)
{
    assert(value != NULL);

    sbuf(uint8_t) bytes = NULL;
    assert(value->type->size <= SIZE_MAX);
    sbuf_resize(bytes, (size_t)value->type->size);
    safe_memset(bytes, 0x00, (size_t)value->type->size);

    switch (value->type->kind) {
    case TYPE_ANY: {
        UNREACHABLE();
    }
    case TYPE_VOID: {
        assert(sbuf_count(bytes) == 0);
        return bytes;
    }
    case TYPE_BOOL: {
        assert(sbuf_count(bytes) == 1);
        bytes[0] = value->data.boolean;
        return bytes;
    }
    case TYPE_BYTE: {
        assert(sbuf_count(bytes) == 1);
        bytes[0] = value->data.byte;
        return bytes;
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
        // Convert the bigint into a bit array.
        size_t const bit_count = (size_t)value->type->size * 8u;
        struct bitarr* const bits = bitarr_new(bit_count);
        if (bigint_to_bitarr(bits, value->data.integer)) {
            // Internal compiler error. Integer is out of range.
            UNREACHABLE();
        }

        // Convert the bit array into a byte array via bit shifting and masking.
        for (size_t i = 0; i < bit_count; ++i) {
            uint8_t const mask = (uint8_t)(bitarr_get(bits, i) << (i % 8u));
            bytes[i / 8u] |= mask;
        }

        bitarr_del(bits);
        return bytes;
    }
    case TYPE_INTEGER: {
        // Arbitrary precision integers have no meaningful byte representation.
        UNREACHABLE();
    }
    case TYPE_FUNCTION: {
        // Functions are an abstract concept with an address that is chosen by
        // the assembler/linker. There is no meaningful representation of a
        // function's address at compile time.
        UNREACHABLE();
    }
    case TYPE_POINTER: {
        // The representation of a non-absolute address is chosen by the
        // assembler/linker and has no meaningful representation at
        // compile-time.
        UNREACHABLE();
    }
    case TYPE_ARRAY: {
        sbuf(struct value*) const elements = value->data.array.elements;
        uint64_t const element_size = value->type->data.array.base->size;
        uint64_t offset = 0;
        for (size_t i = 0; i < sbuf_count(elements); ++i) {
            sbuf(uint8_t) const element_bytes = value_to_new_bytes(elements[i]);
            assert(element_size <= SIZE_MAX);
            safe_memmove(bytes + offset, element_bytes, (size_t)element_size);
            sbuf_fini(element_bytes);
            offset += element_size;
        }
        return bytes;
    }
    case TYPE_SLICE: {
        // The representation of a non-absolute address is chosen by the
        // assembler/linker and has no meaningful representation at
        // compile-time.
        UNREACHABLE();
    }
    case TYPE_STRUCT: {
        struct member_variable const* const member_variable_defs =
            value->type->data.struct_.member_variables;
        for (size_t i = 0; i < sbuf_count(member_variable_defs); ++i) {
            struct member_variable const* const member_def =
                value->type->data.struct_.member_variables + i;

            struct value const* const member_val =
                value->data.struct_.member_variables[i];

            sbuf(uint8_t) const member_bytes = value_to_new_bytes(member_val);
            safe_memmove(
                bytes + member_def->offset,
                member_bytes,
                sbuf_count(member_bytes));
            sbuf_fini(member_bytes);
        }
        return bytes;
    }
    }

    UNREACHABLE();
    return NULL;
}
