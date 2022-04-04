// Copyright 2021 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sunder.h"

struct module*
module_new(char const* name, char const* path)
{
    assert(name != NULL);
    assert(path != NULL);

    struct module* const self = sunder_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));

    self->name = sipool_intern_cstr(context()->sipool, name);
    self->path = sipool_intern_cstr(context()->sipool, path);

    char* const source = read_source(self->path);
    sunder_freezer_register(context()->freezer, source - 1);
    self->source = source;
    self->source_count = strlen(source);

    self->symbols = symbol_table_new(NULL);
    symbol_table_insert(
        self->symbols,
        context()->interned.any,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.any),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.void_,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.void_),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.bool_,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.bool_),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.byte,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.byte),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.u8,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.u8),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.s8,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.s8),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.u16,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.u16),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.s16,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.s16),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.u32,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.u32),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.s32,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.s32),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.u64,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.u64),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.s64,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.s64),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.usize,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.usize),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.ssize,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.ssize),
        false);
    symbol_table_insert(
        self->symbols,
        context()->interned.integer,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.integer),
        false);

    self->exports = symbol_table_new(NULL);

    return self;
}

void
module_del(struct module* self)
{
    assert(self != NULL);

    symbol_table_freeze(self->symbols, context()->freezer);
    symbol_table_freeze(self->exports, context()->freezer);

    sunder_sbuf_fini(self->ordered);
    memset(self, 0x00, sizeof(*self));
    sunder_xalloc(self, SUNDER_XALLOC_FREE);
}

static struct context s_context = {0};

void
context_init(void)
{
    s_context.freezer = sunder_freezer_new();

    s_context.sipool = sipool_new();
#define INTERN_STR_LITERAL(str_literal)                                        \
    sipool_intern_cstr(context()->sipool, str_literal)
    s_context.interned.empty = INTERN_STR_LITERAL("");
    s_context.interned.builtin = INTERN_STR_LITERAL("builtin");
    s_context.interned.return_ = INTERN_STR_LITERAL("return");
    s_context.interned.any = INTERN_STR_LITERAL("any");
    s_context.interned.void_ = INTERN_STR_LITERAL("void");
    s_context.interned.bool_ = INTERN_STR_LITERAL("bool");
    s_context.interned.byte = INTERN_STR_LITERAL("byte");
    s_context.interned.u8 = INTERN_STR_LITERAL("u8");
    s_context.interned.s8 = INTERN_STR_LITERAL("s8");
    s_context.interned.u16 = INTERN_STR_LITERAL("u16");
    s_context.interned.s16 = INTERN_STR_LITERAL("s16");
    s_context.interned.u32 = INTERN_STR_LITERAL("u32");
    s_context.interned.s32 = INTERN_STR_LITERAL("s32");
    s_context.interned.u64 = INTERN_STR_LITERAL("u64");
    s_context.interned.s64 = INTERN_STR_LITERAL("s64");
    s_context.interned.usize = INTERN_STR_LITERAL("usize");
    s_context.interned.ssize = INTERN_STR_LITERAL("ssize");
    s_context.interned.integer = INTERN_STR_LITERAL("integer");
    s_context.interned.y = INTERN_STR_LITERAL("y");
    s_context.interned.u = INTERN_STR_LITERAL("u");
    s_context.interned.s = INTERN_STR_LITERAL("s");
#undef INTERN_STR_LITERAL

#define INIT_BIGINT_CONSTANT(ident, str_literal)                               \
    struct bigint* const ident = bigint_new_cstr(str_literal);                 \
    bigint_freeze(ident, s_context.freezer);                                   \
    s_context.ident = ident;
    INIT_BIGINT_CONSTANT(u8_min, "+0x00")
    INIT_BIGINT_CONSTANT(u8_max, "+0xFF")
    INIT_BIGINT_CONSTANT(s8_min, "-128")
    INIT_BIGINT_CONSTANT(s8_max, "+127")
    INIT_BIGINT_CONSTANT(u16_min, "+0x0000")
    INIT_BIGINT_CONSTANT(u16_max, "+0xFFFF")
    INIT_BIGINT_CONSTANT(s16_min, "-32768")
    INIT_BIGINT_CONSTANT(s16_max, "+32767")
    INIT_BIGINT_CONSTANT(u32_min, "+0x00000000")
    INIT_BIGINT_CONSTANT(u32_max, "+0xFFFFFFFF")
    INIT_BIGINT_CONSTANT(s32_min, "-2147483648")
    INIT_BIGINT_CONSTANT(s32_max, "+2147483647")
    INIT_BIGINT_CONSTANT(u64_min, "+0x0000000000000000")
    INIT_BIGINT_CONSTANT(u64_max, "+0xFFFFFFFFFFFFFFFF")
    INIT_BIGINT_CONSTANT(s64_min, "-9223372036854775808")
    INIT_BIGINT_CONSTANT(s64_max, "+9223372036854775807")
    s_context.usize_min = s_context.u64_min;
    s_context.usize_max = s_context.u64_max;
    s_context.ssize_min = s_context.s64_min;
    s_context.ssize_max = s_context.s64_max;
#undef INIT_BIGINT_CONSTANT

    s_context.static_symbols = NULL;
    s_context.global_symbol_table = symbol_table_new(NULL);
    s_context.modules = NULL;

    s_context.builtin.location = (struct source_location){
        s_context.interned.builtin,
        NO_LINE,
        NO_PSRC,
    };
#define INIT_BUILTIN_TYPE(builtin_lvalue, /* struct type* */ t)                \
    {                                                                          \
        struct type* const type = t;                                           \
        sunder_freezer_register(s_context.freezer, type);                      \
        struct symbol* const symbol =                                          \
            symbol_new_type(&s_context.builtin.location, type);                \
        sunder_freezer_register(s_context.freezer, symbol);                    \
        symbol_table_insert(                                                   \
            s_context.global_symbol_table, symbol->name, symbol, false);       \
        builtin_lvalue = type;                                                 \
    }
    INIT_BUILTIN_TYPE(s_context.builtin.any, type_new_any());
    INIT_BUILTIN_TYPE(s_context.builtin.void_, type_new_void());
    INIT_BUILTIN_TYPE(s_context.builtin.bool_, type_new_bool());
    INIT_BUILTIN_TYPE(s_context.builtin.byte, type_new_byte());
    INIT_BUILTIN_TYPE(s_context.builtin.u8, type_new_u8());
    INIT_BUILTIN_TYPE(s_context.builtin.s8, type_new_s8());
    INIT_BUILTIN_TYPE(s_context.builtin.u16, type_new_u16());
    INIT_BUILTIN_TYPE(s_context.builtin.s16, type_new_s16());
    INIT_BUILTIN_TYPE(s_context.builtin.u32, type_new_u32());
    INIT_BUILTIN_TYPE(s_context.builtin.s32, type_new_s32());
    INIT_BUILTIN_TYPE(s_context.builtin.u64, type_new_u64());
    INIT_BUILTIN_TYPE(s_context.builtin.s64, type_new_s64());
    INIT_BUILTIN_TYPE(s_context.builtin.usize, type_new_usize());
    INIT_BUILTIN_TYPE(s_context.builtin.ssize, type_new_ssize());
    INIT_BUILTIN_TYPE(s_context.builtin.integer, type_new_integer());
#undef INIT_BUILTIN_TYPE
}

void
context_fini(void)
{
    struct context* const self = &s_context;

    for (size_t i = 0; i < sunder_sbuf_count(self->modules); ++i) {
        module_del(self->modules[i]);
    }
    sunder_sbuf_fini(self->modules);

    sipool_del(self->sipool);

    sunder_sbuf_fini(self->static_symbols);
    symbol_table_freeze(self->global_symbol_table, self->freezer);

    sunder_sbuf(struct symbol_table*) const chilling_symbol_tables =
        self->chilling_symbol_tables;
    for (size_t i = 0; i < sunder_sbuf_count(chilling_symbol_tables); ++i) {
        symbol_table_freeze(chilling_symbol_tables[i], self->freezer);
    }
    sunder_sbuf_fini(self->chilling_symbol_tables);

    sunder_freezer_del(self->freezer);

    memset(self, 0x00, sizeof(*self));
}

struct context*
context(void)
{
    return &s_context;
}

struct module const*
load_module(char const* name, char const* path)
{
    assert(path != NULL);
    assert(lookup_module(path) == NULL);

    struct module* const module = module_new(name, path);
    sunder_sbuf_push(s_context.modules, module);

    parse(module);
    order(module);
    resolve(module);

    module->loaded = true;
    return module;
}

struct module const*
lookup_module(char const* path)
{
    assert(path != NULL);

    for (size_t i = 0; i < sunder_sbuf_count(context()->modules); ++i) {
        if (context()->modules[i]->path == path) {
            return context()->modules[i];
        }
    }
    return NULL;
}
