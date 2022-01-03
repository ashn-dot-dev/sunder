// Copyright 2021 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sunder.h"

static struct type*
type_new(char const* name, size_t size, size_t align, enum type_kind kind)
{
    assert(name != NULL);

    struct type* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->name = name;
    self->size = size;
    self->align = align;
    self->kind = kind;
    return self;
}

struct type*
type_new_void(void)
{
    return type_new(context()->interned.void_, 0u, 0u, TYPE_VOID);
}

struct type*
type_new_bool(void)
{
    return type_new(context()->interned.bool_, 1u, 1u, TYPE_BOOL);
}

struct type*
type_new_byte(void)
{
    return type_new(context()->interned.byte, 1u, 1u, TYPE_BYTE);
}

struct type*
type_new_u8(void)
{
    struct type* const self = type_new(context()->interned.u8, 1u, 1u, TYPE_U8);
    self->data.integer.min = context()->u8_min;
    self->data.integer.max = context()->u8_max;
    return self;
}

struct type*
type_new_s8(void)
{
    struct type* const self = type_new(context()->interned.s8, 1u, 1u, TYPE_S8);
    self->data.integer.min = context()->s8_min;
    self->data.integer.max = context()->s8_max;
    return self;
}

struct type*
type_new_u16(void)
{
    struct type* const self =
        type_new(context()->interned.u16, 2u, 2u, TYPE_U16);
    self->data.integer.min = context()->u16_min;
    self->data.integer.max = context()->u16_max;
    return self;
}

struct type*
type_new_s16(void)
{
    struct type* const self =
        type_new(context()->interned.s16, 2u, 2u, TYPE_S16);
    self->data.integer.min = context()->s16_min;
    self->data.integer.max = context()->s16_max;
    return self;
}

struct type*
type_new_u32(void)
{
    struct type* const self =
        type_new(context()->interned.u32, 4u, 4u, TYPE_U32);
    self->data.integer.min = context()->u32_min;
    self->data.integer.max = context()->u32_max;
    return self;
}

struct type*
type_new_s32(void)
{
    struct type* const self =
        type_new(context()->interned.s32, 4u, 4u, TYPE_S32);
    self->data.integer.min = context()->s32_min;
    self->data.integer.max = context()->s32_max;
    return self;
}

struct type*
type_new_u64(void)
{
    struct type* const self =
        type_new(context()->interned.u64, 8u, 8u, TYPE_U64);
    self->data.integer.min = context()->u64_min;
    self->data.integer.max = context()->u64_max;
    return self;
}

struct type*
type_new_s64(void)
{
    struct type* const self =
        type_new(context()->interned.s64, 8u, 8u, TYPE_S64);
    self->data.integer.min = context()->s64_min;
    self->data.integer.max = context()->s64_max;
    return self;
}

struct type*
type_new_usize(void)
{
    struct type* const self =
        type_new(context()->interned.usize, 8u, 8u, TYPE_USIZE);
    self->data.integer.min = context()->usize_min;
    self->data.integer.max = context()->usize_max;
    return self;
}

struct type*
type_new_ssize(void)
{
    struct type* const self =
        type_new(context()->interned.ssize, 8u, 8u, TYPE_SSIZE);
    self->data.integer.min = context()->ssize_min;
    self->data.integer.max = context()->ssize_max;
    return self;
}

struct type*
type_new_integer(void)
{
    struct type* const self = type_new(
        context()->interned.integer,
        SIZEOF_UNSIZED,
        ALIGNOF_UNSIZED,
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

    struct autil_string* const name_string = autil_string_new_cstr("func(");
    if (autil_sbuf_count(parameter_types) != 0) {
        autil_string_append_cstr(name_string, parameter_types[0]->name);
    }
    for (size_t i = 1; i < autil_sbuf_count(parameter_types); ++i) {
        autil_string_append_fmt(name_string, ", %s", parameter_types[i]->name);
    }
    autil_string_append_fmt(name_string, ") %s", return_type->name);
    char const* const name = autil_sipool_intern(
        context()->sipool,
        autil_string_start(name_string),
        autil_string_count(name_string));
    autil_string_del(name_string);

    struct type* const self = type_new(name, 8u, 8u, TYPE_FUNCTION);
    self->data.function.parameter_types = parameter_types;
    self->data.function.return_type = return_type;
    return self;
}

struct type*
type_new_pointer(struct type const* base)
{
    assert(base != NULL);

    struct autil_string* const name_string =
        autil_string_new_fmt("*%s", base->name);
    char const* const name = autil_sipool_intern(
        context()->sipool,
        autil_string_start(name_string),
        autil_string_count(name_string));
    autil_string_del(name_string);

    struct type* const self = type_new(name, 8u, 8u, TYPE_POINTER);
    self->data.pointer.base = base;
    return self;
}

struct type*
type_new_array(size_t count, struct type const* base)
{
    assert(base != NULL);

    struct autil_string* const name_string =
        autil_string_new_fmt("[%zu]%s", count, base->name);
    char const* const name = autil_sipool_intern(
        context()->sipool,
        autil_string_start(name_string),
        autil_string_count(name_string));
    autil_string_del(name_string);

    size_t const size = count * base->size;
    assert((count == 0 || size / count == base->size) && "array size overflow");
    // https://en.cppreference.com/w/c/language/_Alignof
    // > Returns the alignment requirement of the type named by type-name. If
    // > type-name is an array type, the result is the alignment requirement of
    // > the array element type.
    size_t const align = base->align;

    struct type* const self = type_new(name, size, align, TYPE_ARRAY);
    self->data.array.count = count;
    self->data.array.base = base;
    return self;
}

struct type*
type_new_slice(struct type const* base)
{
    assert(base != NULL);

    struct autil_string* const name_string =
        autil_string_new_fmt("[]%s", base->name);
    char const* const name = autil_sipool_intern(
        context()->sipool,
        autil_string_start(name_string),
        autil_string_count(name_string));
    autil_string_del(name_string);

    struct type* const self = type_new(name, 8u * 2u, 8u, TYPE_SLICE);
    self->data.pointer.base = base;
    return self;
}

struct type*
type_new_struct(char const* name, struct symbol_table const* symbols)
{
    struct type* const self = type_new(name, 0, 0, TYPE_STRUCT);
    self->data.struct_.next_offset = 0;
    self->data.struct_.member_variables = NULL;
    self->data.struct_.symbols = symbols;
    return self;
}

void
type_struct_add_member_variable(
    struct type* self, char const* name, struct type const* type)
{
    assert(self != NULL);
    assert(name != NULL);
    assert(type != NULL);

    if (self->name == type->name) {
        fatal(
            NULL,
            "struct `%s` contains a member variable of its own type",
            self->name);
    }

    // Member variables with size zero are part of the struct, but do not
    // contribute to the size or alignment of the struct.
    if (type->size == 0) {
        struct member_variable const m = {
            .name = name,
            .type = type,
            .offset = self->data.struct_.next_offset,
        };
        autil_sbuf_push(self->data.struct_.member_variables, m);
    }

    assert(type->size != 0);
    assert(type->align != 0);

    // Increase the offset into the struct until the start of the added member
    // variable is aligned to a valid byte boundary.
    //
    // TODO: Do we need any additional calculation(s) here to account for the
    // natural alignment of the stack? Currently we are using 8-byte stack
    // alignment (even though technically x64 has a max alignment of 16), but
    // this check for "do we need to increase the next offset" does not include
    // any mention of the natural stack alignment.
    while (self->data.struct_.next_offset % type->align != 0) {
        self->data.struct_.next_offset += 1;
    }

    // Push the added member variable onto the back of the struct's list of
    // members (ordered by offset).
    struct member_variable const m = {
        .name = name,
        .type = type,
        .offset = self->data.struct_.next_offset,
    };
    autil_sbuf_push(self->data.struct_.member_variables, m);

    // Adjust the struct alignment to match the alignment of the first
    // non-zero-sized non-zero-aligned member. This case should only ever occur
    // if the size and alignment are both zero.
    assert((self->size == 0) == (self->align == 0));
    if (self->align == 0) {
        self->align = type->align;
    }

    // Adjust the struct size to fit all members plus array stride padding.
    self->size = self->data.struct_.next_offset + type->size;
    assert(self->align != 0);
    while (self->size % self->align != 0) {
        self->size += 1;
    }

    // Future member variables will search for a valid offset starting at one
    // byte past the added member variable.
    self->data.struct_.next_offset += type->size;
}

long
type_struct_member_variable_index(struct type const* self, char const* name)
{
    assert(self != NULL);
    assert(self->kind == TYPE_STRUCT);
    assert(name != NULL);

    struct member_variable const* const member_variables =
        self->data.struct_.member_variables;
    for (size_t i = 0; i < autil_sbuf_count(member_variables); ++i) {
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

struct symbol const*
type_struct_member_function_symbol(struct type const* self, char const* name)
{
    assert(self != NULL);
    assert(self->kind == TYPE_STRUCT);
    assert(name != NULL);

    return symbol_table_lookup_local(self->data.struct_.symbols, name);
}

struct function const*
type_struct_member_function(struct type const* self, char const* name)
{
    assert(self != NULL);
    assert(self->kind == TYPE_STRUCT);
    assert(name != NULL);

    struct symbol const* const symbol =
        type_struct_member_function_symbol(self, name);
    if (symbol == NULL) {
        return NULL;
    }
    if (symbol->kind != SYMBOL_FUNCTION) {
        return NULL;
    }

    assert(symbol->value->type->kind == TYPE_FUNCTION);
    return symbol->value->data.function;
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
        autil_xalloc(type, AUTIL_XALLOC_FREE);
        return existing->type;
    }

    struct symbol* const symbol =
        symbol_new_type(&context()->builtin.location, type);
    symbol_table_insert(context()->global_symbol_table, symbol->name, symbol);
    autil_freezer_register(context()->freezer, type);
    autil_freezer_register(context()->freezer, symbol);
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
        autil_xalloc(type, AUTIL_XALLOC_FREE);
        return existing->type;
    }

    struct symbol* const symbol =
        symbol_new_type(&context()->builtin.location, type);
    symbol_table_insert(context()->global_symbol_table, symbol->name, symbol);
    autil_freezer_register(context()->freezer, type);
    autil_freezer_register(context()->freezer, symbol);
    return type;
}

struct type const*
type_unique_array(size_t count, struct type const* base)
{
    assert(base != NULL);

    struct type* const type = type_new_array(count, base);
    struct symbol const* const existing =
        symbol_table_lookup(context()->global_symbol_table, type->name);
    if (existing != NULL) {
        autil_xalloc(type, AUTIL_XALLOC_FREE);
        return existing->type;
    }

    struct symbol* const symbol =
        symbol_new_type(&context()->builtin.location, type);
    symbol_table_insert(context()->global_symbol_table, symbol->name, symbol);
    autil_freezer_register(context()->freezer, type);
    autil_freezer_register(context()->freezer, symbol);
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
        autil_xalloc(type, AUTIL_XALLOC_FREE);
        return existing->type;
    }

    struct symbol* const symbol =
        symbol_new_type(&context()->builtin.location, type);
    symbol_table_insert(context()->global_symbol_table, symbol->name, symbol);
    autil_freezer_register(context()->freezer, type);
    autil_freezer_register(context()->freezer, symbol);
    return type;
}

bool
type_is_any_integer(struct type const* self)
{
    assert(self != NULL);

    enum type_kind const kind = self->kind;
    return kind == TYPE_U8 || kind == TYPE_S8 || kind == TYPE_U16
        || kind == TYPE_S16 || kind == TYPE_U32 || kind == TYPE_S32
        || kind == TYPE_U64 || kind == TYPE_S64 || kind == TYPE_USIZE
        || kind == TYPE_SSIZE || kind == TYPE_INTEGER;
}

bool
type_is_unsigned_integer(struct type const* self)
{
    assert(self != NULL);

    enum type_kind const kind = self->kind;
    return kind == TYPE_U8 || kind == TYPE_U16 || kind == TYPE_U32
        || kind == TYPE_U64 || kind == TYPE_USIZE;
}

bool
type_is_signed_integer(struct type const* self)
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
    return kind == TYPE_BOOL || kind == TYPE_BYTE || type_is_any_integer(self)
        || kind == TYPE_FUNCTION || kind == TYPE_POINTER;
}

bool
type_can_compare_order(struct type const* self)
{
    assert(self != NULL);

    enum type_kind const kind = self->kind;
    return kind == TYPE_BOOL || kind == TYPE_BYTE || type_is_any_integer(self)
        || kind == TYPE_POINTER;
}

struct address
address_init_static(char const* name, size_t offset)
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
    struct address* const self = autil_xalloc(NULL, sizeof(*self));
    *self = from;
    return self;
}

static struct symbol*
symbol_new(
    enum symbol_kind kind,
    struct source_location const* location,
    char const* name,
    struct type const* type,
    struct address const* address,
    struct value const* value,
    struct cst_decl const* decl,
    struct symbol_table* symbols)
{
    assert(location != NULL);
    assert(name != NULL);

    struct symbol* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = kind;
    self->location = location;
    self->name = name;
    self->type = type;
    self->address = address;
    self->value = value;
    self->decl = decl;
    self->symbols = symbols;
    return self;
}

struct symbol*
symbol_new_type(struct source_location const* location, struct type const* type)
{
    assert(location != NULL);
    assert(type != NULL);

    struct symbol* const self = symbol_new(
        SYMBOL_TYPE, location, type->name, type, NULL, NULL, NULL, NULL);
    return self;
}

struct symbol*
symbol_new_variable(
    struct source_location const* location,
    char const* name,
    struct type const* type,
    struct address const* address,
    struct value const* value)
{
    assert(location != NULL);
    assert(name != NULL);
    assert(type != NULL);
    assert(address != NULL);
    // TODO: Extern variables may now be static with a non-NULL value.
    // We should either update this assert or maybe add an is_extern parameter
    // to this factory function.
    //assert(address->kind != ADDRESS_STATIC || value != NULL);

    struct symbol* const self = symbol_new(
        SYMBOL_VARIABLE, location, name, type, address, value, NULL, NULL);
    return self;
}

struct symbol*
symbol_new_constant(
    struct source_location const* location,
    char const* name,
    struct type const* type,
    struct address const* address,
    struct value const* value)
{
    assert(location != NULL);
    assert(name != NULL);
    assert(type != NULL);
    assert(address != NULL);
    assert(value != NULL);

    struct symbol* const self = symbol_new(
        SYMBOL_CONSTANT, location, name, type, address, value, NULL, NULL);
    return self;
}

struct symbol*
symbol_new_function(
    struct source_location const* location,
    char const* name,
    struct type const* type,
    struct address const* address,
    struct value const* value)
{
    assert(location != NULL);
    assert(name != NULL);
    assert(type != NULL);
    assert(address != NULL);
    assert(value != NULL);

    struct symbol* const self = symbol_new(
        SYMBOL_FUNCTION, location, name, type, address, value, NULL, NULL);
    return self;
}

struct symbol*
symbol_new_template(
    struct source_location const* location,
    char const* name,
    struct cst_decl const* decl,
    struct symbol_table* symbols)
{
    assert(location != NULL);
    assert(name != NULL);
    assert(decl != NULL);
    assert(symbols != NULL);

    struct symbol* const self = symbol_new(
        SYMBOL_TEMPLATE, location, name, NULL, NULL, NULL, decl, symbols);
    return self;
}

struct symbol*
symbol_new_namespace(
    struct source_location const* location,
    char const* name,
    struct symbol_table* symbols)
{
    assert(location != NULL);
    assert(name != NULL);
    assert(symbols != NULL);

    struct symbol* const self = symbol_new(
        SYMBOL_NAMESPACE, location, name, NULL, NULL, NULL, NULL, symbols);
    return self;
}

struct symbol_table*
symbol_table_new(struct symbol_table const* parent)
{
    struct symbol_table* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->parent = parent;
    self->symbols = autil_map_new(
        sizeof(SYMBOL_MAP_KEY_TYPE),
        sizeof(SYMBOL_MAP_VAL_TYPE),
        SYMBOL_MAP_CMP_FUNC);
    return self;
}

void
symbol_table_freeze(struct symbol_table* self, struct autil_freezer* freezer)
{
    assert(self != NULL);
    assert(freezer != NULL);

    autil_freezer_register(freezer, self);
    autil_map_freeze(self->symbols, freezer);
}

void
symbol_table_insert(
    struct symbol_table* self, char const* name, struct symbol const* symbol)
{
    assert(self != NULL);
    assert(symbol != NULL);

    SYMBOL_MAP_KEY_TYPE const key = name;
    struct symbol const* const local = symbol_table_lookup_local(self, key);
    if (local != NULL) {
        fatal(
            symbol->location,
            "redeclaration of `%s` previously declared at [%s:%zu]",
            key,
            local->location->path,
            local->location->line);
    }
    SYMBOL_MAP_VAL_TYPE const val = symbol;
    autil_map_insert(self->symbols, &key, &val, NULL, NULL);
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

    SYMBOL_MAP_VAL_TYPE const* const existing =
        autil_map_lookup_const(self->symbols, &name);
    return existing != NULL ? *existing : NULL;
}

static struct stmt*
stmt_new(struct source_location const* location, enum stmt_kind kind)
{
    struct stmt* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->kind = kind;
    return self;
}

struct stmt*
stmt_new_if(struct conditional const* const* conditionals)
{
    assert(autil_sbuf_count(conditionals) > 0u);

    struct stmt* const self = stmt_new(conditionals[0]->location, STMT_IF);
    self->data.if_.conditionals = conditionals;
    return self;
}

struct stmt*
stmt_new_for_range(
    struct source_location const* location,
    struct symbol const* loop_variable,
    struct expr const* begin,
    struct expr const* end,
    struct block const* body)
{
    assert(location != NULL);
    assert(loop_variable != NULL);
    assert(loop_variable->kind == SYMBOL_VARIABLE);
    assert(loop_variable->type == context()->builtin.usize);
    assert(begin != NULL);
    assert(begin->type == context()->builtin.usize);
    assert(end != NULL);
    assert(end->type == context()->builtin.usize);
    assert(body != NULL);

    struct stmt* const self = stmt_new(location, STMT_FOR_RANGE);
    self->data.for_range.loop_variable = loop_variable;
    self->data.for_range.begin = begin;
    self->data.for_range.end = end;
    self->data.for_range.body = body;
    return self;
}

struct stmt*
stmt_new_for_expr(
    struct source_location const* location,
    struct expr const* expr,
    struct block const* body)
{
    assert(location != NULL);
    assert(expr != NULL);
    assert(body != NULL);

    struct stmt* const self = stmt_new(location, STMT_FOR_EXPR);
    self->data.for_expr.expr = expr;
    self->data.for_expr.body = body;
    return self;
}

struct stmt*
stmt_new_break(struct source_location const* location)
{
    assert(location != NULL);

    struct stmt* const self = stmt_new(location, STMT_BREAK);
    return self;
}

struct stmt*
stmt_new_continue(struct source_location const* location)
{
    assert(location != NULL);

    struct stmt* const self = stmt_new(location, STMT_CONTINUE);
    return self;
}

struct stmt*
stmt_new_dump(struct source_location const* location, struct expr const* expr)
{
    assert(location != NULL);
    assert(expr != NULL);

    struct stmt* const self = stmt_new(location, STMT_DUMP);
    self->data.dump.expr = expr;
    return self;
}

struct stmt*
stmt_new_return(struct source_location const* location, struct expr const* expr)
{
    assert(location != NULL);

    struct stmt* const self = stmt_new(location, STMT_RETURN);
    self->data.return_.expr = expr;
    return self;
}

struct stmt*
stmt_new_assign(
    struct source_location const* location,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(location != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    struct stmt* const self = stmt_new(location, STMT_ASSIGN);
    self->data.assign.lhs = lhs;
    self->data.assign.rhs = rhs;
    return self;
}

struct stmt*
stmt_new_expr(struct source_location const* location, struct expr const* expr)
{
    assert(expr != NULL);

    struct stmt* const self = stmt_new(location, STMT_EXPR);
    self->data.expr = expr;
    return self;
}

static struct expr*
expr_new(
    struct source_location const* location,
    struct type const* type,
    enum expr_kind kind)
{
    assert(location != NULL);
    assert(type != NULL);

    struct expr* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->type = type;
    self->kind = kind;
    return self;
}

struct expr*
expr_new_identifier(
    struct source_location const* location, struct symbol const* identifier)
{
    assert(location != NULL);
    assert(identifier != NULL);
    assert(identifier->kind != SYMBOL_TYPE);

    struct expr* const self =
        expr_new(location, identifier->type, EXPR_IDENTIFIER);
    self->data.identifier = identifier;
    return self;
}

struct expr*
expr_new_boolean(struct source_location const* location, bool value)
{
    assert(location != NULL);

    struct type const* const type = context()->builtin.bool_;
    struct expr* const self = expr_new(location, type, EXPR_BOOLEAN);
    self->data.boolean = value;
    return self;
}

struct expr*
expr_new_integer(
    struct source_location const* location,
    struct type const* type,
    struct autil_bigint const* value)
{
    assert(location != NULL);
    assert(type != NULL);
    assert(type->kind == TYPE_BYTE || type_is_any_integer(type));
    assert(value != NULL);

    bool const is_byte = type->kind == TYPE_BYTE;
    bool const is_sized_integer =
        type_is_any_integer(type) && type->kind != TYPE_INTEGER;

    if (is_byte && autil_bigint_cmp(value, context()->u8_min) < 0) {
        char* const lit_cstr = autil_bigint_to_new_cstr(value, NULL);
        char* const min_cstr =
            autil_bigint_to_new_cstr(context()->u8_min, NULL);
        fatal(
            location,
            "out-of-range byte literal (%s < %s)",
            lit_cstr,
            min_cstr);
    }
    if (is_byte && autil_bigint_cmp(value, context()->u8_max) > 0) {
        char* const lit_cstr = autil_bigint_to_new_cstr(value, NULL);
        char* const max_cstr =
            autil_bigint_to_new_cstr(context()->u8_max, NULL);
        fatal(
            location,
            "out-of-range byte literal (%s > %s)",
            lit_cstr,
            max_cstr);
    }
    if (is_sized_integer
        && autil_bigint_cmp(value, type->data.integer.min) < 0) {
        char* const lit_cstr = autil_bigint_to_new_cstr(value, NULL);
        char* const min_cstr =
            autil_bigint_to_new_cstr(type->data.integer.min, NULL);
        fatal(
            location,
            "out-of-range integer literal (%s < %s)",
            lit_cstr,
            min_cstr);
    }
    if (is_sized_integer
        && autil_bigint_cmp(value, type->data.integer.max) > 0) {
        char* const lit_cstr = autil_bigint_to_new_cstr(value, NULL);
        char* const max_cstr =
            autil_bigint_to_new_cstr(type->data.integer.max, NULL);
        fatal(
            location,
            "out-of-range integer literal (%s > %s)",
            lit_cstr,
            max_cstr);
    }
    struct expr* const self = expr_new(location, type, EXPR_INTEGER);
    self->data.integer = value;
    return self;
}

struct expr*
expr_new_bytes(
    struct source_location const* location,
    struct address const* address,
    size_t count)
{
    assert(location != NULL);
    assert(address != NULL);

    struct type const* const type = type_unique_slice(context()->builtin.byte);
    struct expr* const self = expr_new(location, type, EXPR_BYTES);
    self->data.bytes.address = address;
    self->data.bytes.count = count;
    return self;
}

struct expr*
expr_new_array(
    struct source_location const* location,
    struct type const* type,
    struct expr const* const* elements,
    struct expr const* ellipsis)
{
    assert(location != NULL);
    assert(type != NULL);
    assert(type->kind == TYPE_ARRAY);

    struct expr* const self = expr_new(location, type, EXPR_ARRAY);
    self->data.array.elements = elements;
    self->data.array.ellipsis = ellipsis;
    return self;
}

struct expr*
expr_new_slice(
    struct source_location const* location,
    struct type const* type,
    struct expr const* pointer,
    struct expr const* count)
{
    assert(location != NULL);
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
    struct source_location const* location,
    struct type const* type,
    struct expr const* const* member_variables)
{
    assert(location != NULL);
    assert(type != NULL);
    assert(type->kind == TYPE_STRUCT);

    struct expr* const self = expr_new(location, type, EXPR_STRUCT);
    self->data.struct_.member_variables = member_variables;
    return self;
}

struct expr*
expr_new_cast(
    struct source_location const* location,
    struct type const* type,
    struct expr const* expr)
{
    assert(location != NULL);
    assert(type != NULL);
    assert(expr != NULL);

    struct expr* const self = expr_new(location, type, EXPR_CAST);
    self->data.cast.expr = expr;
    return self;
}

struct expr*
expr_new_syscall(
    struct source_location const* location, struct expr const* const* arguments)
{
    assert(location != NULL);
    assert(arguments != NULL);

    struct expr* const self =
        expr_new(location, context()->builtin.ssize, EXPR_SYSCALL);
    self->data.syscall.arguments = arguments;
    return self;
}

struct expr*
expr_new_call(
    struct source_location const* location,
    struct expr const* function,
    struct expr const* const* arguments)
{
    assert(location != NULL);
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
    struct source_location const* location,
    struct expr const* lhs,
    struct expr const* idx)
{
    assert(location != NULL);
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
}

struct expr*
expr_new_access_slice(
    struct source_location const* location,
    struct expr const* lhs,
    struct expr const* begin,
    struct expr const* end)
{
    assert(location != NULL);
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
}

struct expr*
expr_new_access_member_variable(
    struct source_location const* location,
    struct expr const* lhs,
    struct member_variable const* member_variable)
{
    assert(location != NULL);
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
expr_new_sizeof(struct source_location const* location, struct type const* rhs)
{
    assert(location != NULL);
    assert(rhs != NULL);

    struct expr* const self =
        expr_new(location, context()->builtin.usize, EXPR_SIZEOF);
    self->data.sizeof_.rhs = rhs;
    return self;
}

struct expr*
expr_new_alignof(struct source_location const* location, struct type const* rhs)
{
    assert(location != NULL);
    assert(rhs != NULL);

    struct expr* const self =
        expr_new(location, context()->builtin.usize, EXPR_ALIGNOF);
    self->data.sizeof_.rhs = rhs;
    return self;
}

struct expr*
expr_new_unary(
    struct source_location const* location,
    struct type const* type,
    enum uop_kind op,
    struct expr const* rhs)
{
    assert(location != NULL);
    assert(type != NULL);
    assert(rhs != NULL);

    struct expr* const self = expr_new(location, type, EXPR_UNARY);
    self->data.unary.op = op;
    self->data.unary.rhs = rhs;
    return self;
}

struct expr*
expr_new_binary(
    struct source_location const* location,
    struct type const* type,
    enum bop_kind op,
    struct expr const* lhs,
    struct expr const* rhs)
{
    assert(location != NULL);
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
    case EXPR_IDENTIFIER: {
        switch (self->data.identifier->kind) {
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
    case EXPR_BOOLEAN: /* fallthrough */
    case EXPR_INTEGER: /* fallthrough */
    case EXPR_BYTES: /* fallthrough */
    case EXPR_ARRAY: /* fallthrough */
    case EXPR_SLICE: /* fallthrough */
    case EXPR_STRUCT: /* fallthrough */
    case EXPR_CAST: /* fallthrough */
    case EXPR_SYSCALL: /* fallthrough */
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

struct function*
function_new(
    char const* name, struct type const* type, struct address const* address)
{
    assert(name != NULL);
    assert(type != NULL);
    assert(type->kind == TYPE_FUNCTION);
    assert(address != NULL);
    assert(address->kind == ADDRESS_STATIC);

    struct function* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->name = name;
    self->type = type;
    self->address = address;
    return self;
}

struct conditional*
conditional_new(
    struct source_location const* location,
    struct expr const* condition,
    struct block const* body)
{
    struct conditional* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->condition = condition;
    self->body = body;
    return self;
}

struct block*
block_new(
    struct source_location const* location,
    struct symbol_table* symbol_table,
    struct stmt const* const* stmts)
{
    assert(location != NULL);
    assert(symbol_table != NULL);

    struct block* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->symbol_table = symbol_table;
    self->stmts = stmts;
    return self;
}

static struct value*
value_new(struct type const* type)
{
    assert(type != NULL);

    struct value* const self = autil_xalloc(NULL, sizeof(*self));
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
value_new_integer(struct type const* type, struct autil_bigint* integer)
{
    assert(type != NULL);
    assert(type->kind == TYPE_BYTE || type_is_any_integer(type));
    assert(integer != NULL);
    assert(
        type->kind == TYPE_INTEGER
        || autil_bigint_cmp(integer, type->data.integer.min) >= 0);
    assert(
        type->kind == TYPE_INTEGER
        || autil_bigint_cmp(integer, type->data.integer.max) <= 0);

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
    assert(
        type->data.array.count == autil_sbuf_count(elements)
        || ellipsis != NULL);
    // TODO: Should we validate that each element of the array and the ellipsis
    // are of the array element type? That might be expensive so maybe hide it
    // behind `#ifndef NDEBUG`?

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
    assert(autil_bigint_cmp(count->data.integer, AUTIL_BIGINT_ZERO) >= 0);

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
        autil_sbuf_count(type->data.struct_.member_variables);
    self->data.struct_.member_variables = NULL;
    for (size_t i = 0; i < member_variables_count; ++i) {
        autil_sbuf_push(self->data.struct_.member_variables, NULL);
    }

    return self;
}

void
value_del(struct value* self)
{
    assert(self != NULL);

    switch (self->type->kind) {
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
        autil_bigint_del(self->data.integer);
        break;
    }
    case TYPE_FUNCTION: {
        break;
    }
    case TYPE_POINTER: {
        break;
    }
    case TYPE_ARRAY: {
        autil_sbuf(struct value*) const elements = self->data.array.elements;
        for (size_t i = 0; i < autil_sbuf_count(elements); ++i) {
            value_del(elements[i]);
        }
        autil_sbuf_fini(elements);
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
            autil_sbuf_count(self->type->data.struct_.member_variables);
        for (size_t i = 0; i < member_variables_count; ++i) {
            struct value** pvalue = self->data.struct_.member_variables + i;
            if (*pvalue == NULL) {
                // Value was never initialized.
                continue;
            }
            value_del(*pvalue);
        }
        autil_sbuf_fini(self->data.struct_.member_variables);
        break;
    }
    }

    memset(self, 0x00, sizeof(*self));
    autil_xalloc(self, AUTIL_XALLOC_FREE);
}

void
value_freeze(struct value* self, struct autil_freezer* freezer)
{
    assert(self != NULL);
    assert(freezer != NULL);

    autil_freezer_register(freezer, self);
    switch (self->type->kind) {
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
        autil_bigint_freeze(self->data.integer, freezer);
        return;
    }
    case TYPE_FUNCTION: {
        return;
    }
    case TYPE_POINTER: {
        return;
    }
    case TYPE_ARRAY: {
        autil_sbuf(struct value*) const elements = self->data.array.elements;
        struct value* const ellipsis = self->data.array.ellipsis;
        autil_sbuf_freeze(elements, freezer);
        for (size_t i = 0; i < autil_sbuf_count(elements); ++i) {
            value_freeze(elements[i], freezer);
        }
        if (ellipsis != NULL) {
            value_freeze(ellipsis, freezer);
        }
        return;
    }
    case TYPE_SLICE: {
        value_freeze(self->data.slice.pointer, freezer);
        value_freeze(self->data.slice.count, freezer);
        return;
    }
    case TYPE_STRUCT: {
        size_t const member_variables_count =
            autil_sbuf_count(self->type->data.struct_.member_variables);
        for (size_t i = 0; i < member_variables_count; ++i) {
            struct value** pvalue = self->data.struct_.member_variables + i;
            assert(*pvalue != NULL); // Self not fully initialized.
            value_freeze(*pvalue, freezer);
        }
        autil_sbuf_freeze(self->data.struct_.member_variables, freezer);
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
        return value_new_integer(
            self->type, autil_bigint_new(self->data.integer));
    }
    case TYPE_FUNCTION: {
        return value_new_function(self->data.function);
    }
    case TYPE_POINTER: {
        return value_new_pointer(self->type, self->data.pointer);
    }
    case TYPE_ARRAY: {
        autil_sbuf(struct value*) const elements = self->data.array.elements;
        struct value* const ellipsis = self->data.array.ellipsis;
        autil_sbuf(struct value*) cloned_elements = NULL;
        for (size_t i = 0; i < autil_sbuf_count(elements); ++i) {
            autil_sbuf_push(cloned_elements, value_clone(elements[i]));
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
            autil_sbuf_count(self->type->data.struct_.member_variables);
        for (size_t i = 0; i < member_variables_count; ++i) {
            new->data.struct_.member_variables[i] =
                value_clone(self->data.struct_.member_variables[i]);
        }
        return new;
    }
    }

    UNREACHABLE();
    return NULL;
}

struct value const*
value_get_member(struct value const* self, char const* name)
{
    assert(self != NULL);
    assert(name != NULL);

    long const index = type_struct_member_variable_index(self->type, name);
    if (index < 0) {
        // Should never happen.
        fatal(NULL, "type `%s` has no member `%s`", self->type->name, name);
    }
    return self->data.struct_.member_variables[index];
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
        fatal(NULL, "type `%s` has no member `%s`", self->type->name, name);
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
        return autil_bigint_cmp(lhs->data.integer, rhs->data.integer) == 0;
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
        return autil_bigint_cmp(lhs->data.integer, rhs->data.integer) < 0;
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
        return autil_bigint_cmp(lhs->data.integer, rhs->data.integer) > 0;
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

    autil_sbuf(uint8_t) bytes = NULL;
    autil_sbuf_resize(bytes, value->type->size);
    autil_memset(bytes, 0x00, value->type->size);

    switch (value->type->kind) {
    case TYPE_VOID: {
        assert(autil_sbuf_count(bytes) == 0);
        return bytes;
    }
    case TYPE_BOOL: {
        assert(autil_sbuf_count(bytes) == 1);
        bytes[0] = value->data.boolean;
        return bytes;
    }
    case TYPE_BYTE: {
        assert(autil_sbuf_count(bytes) == 1);
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
        size_t const bit_count = value->type->size * 8u;
        struct autil_bitarr* const bits = autil_bitarr_new(bit_count);
        if (bigint_to_bitarr(bits, value->data.integer)) {
            // Internal compiler error. Integer is out of range.
            UNREACHABLE();
        }

        // Convert the bit array into a byte array via bit shifting and masking.
        for (size_t i = 0; i < bit_count; ++i) {
            uint8_t const mask =
                (uint8_t)(autil_bitarr_get(bits, i) << (i % 8u));
            bytes[i / 8u] |= mask;
        }

        autil_bitarr_del(bits);
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
        autil_sbuf(struct value*) const elements = value->data.array.elements;
        size_t const element_size = value->type->data.array.base->size;
        size_t offset = 0;
        for (size_t i = 0; i < autil_sbuf_count(elements); ++i) {
            autil_sbuf(uint8_t) const element_bytes =
                value_to_new_bytes(elements[i]);
            autil_memmove(bytes + offset, element_bytes, element_size);
            autil_sbuf_fini(element_bytes);
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
        for (size_t i = 0; i < autil_sbuf_count(member_variable_defs); ++i) {
            struct member_variable const* const member_def =
                value->type->data.struct_.member_variables + i;

            struct value const* const member_val =
                value->data.struct_.member_variables[i];

            autil_sbuf(uint8_t) const member_bytes =
                value_to_new_bytes(member_val);
            autil_memmove(
                bytes + member_def->offset,
                member_bytes,
                autil_sbuf_count(member_bytes));
            autil_sbuf_fini(member_bytes);
        }
        return bytes;
    }
    }

    UNREACHABLE();
    return NULL;
}
