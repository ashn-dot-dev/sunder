// Copyright 2021-2022 The Sunder Project Authors
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

    struct module* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));

    self->name = intern_cstr(name);
    self->path = intern_cstr(path);

    char* const source = read_source(self->path);
    freeze(source - 1);
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

    symbol_table_freeze(self->symbols);
    symbol_table_freeze(self->exports);

    sbuf_fini(self->ordered);
    memset(self, 0x00, sizeof(*self));
    xalloc(self, XALLOC_FREE);
}

static struct context s_context = {0};

void
context_init(void)
{
    s_context.interned.empty = intern_cstr("");
    s_context.interned.builtin = intern_cstr("builtin");
    s_context.interned.return_ = intern_cstr("return");
    s_context.interned.main = intern_cstr("main");
    s_context.interned.any = intern_cstr("any");
    s_context.interned.void_ = intern_cstr("void");
    s_context.interned.bool_ = intern_cstr("bool");
    s_context.interned.byte = intern_cstr("byte");
    s_context.interned.u8 = intern_cstr("u8");
    s_context.interned.s8 = intern_cstr("s8");
    s_context.interned.u16 = intern_cstr("u16");
    s_context.interned.s16 = intern_cstr("s16");
    s_context.interned.u32 = intern_cstr("u32");
    s_context.interned.s32 = intern_cstr("s32");
    s_context.interned.u64 = intern_cstr("u64");
    s_context.interned.s64 = intern_cstr("s64");
    s_context.interned.usize = intern_cstr("usize");
    s_context.interned.ssize = intern_cstr("ssize");
    s_context.interned.integer = intern_cstr("integer");
    s_context.interned.y = intern_cstr("y");
    s_context.interned.u = intern_cstr("u");
    s_context.interned.s = intern_cstr("s");

#define INIT_BIGINT_CONSTANT(ident, str_literal)                               \
    struct bigint* const ident = bigint_new_cstr(str_literal);                 \
    bigint_freeze(ident);                                                      \
    s_context.ident = ident;
    INIT_BIGINT_CONSTANT(zero, "0")
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
        freeze(type);                                                          \
        struct symbol* const symbol =                                          \
            symbol_new_type(&s_context.builtin.location, type);                \
        freeze(symbol);                                                        \
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

/* util.c */
extern sbuf(char*) interned;

void
context_fini(void)
{
    struct context* const self = &s_context;

    for (size_t i = 0; i < sbuf_count(self->modules); ++i) {
        module_del(self->modules[i]);
    }
    sbuf_fini(self->modules);

    intern_fini();

    sbuf_fini(self->static_symbols);
    symbol_table_freeze(self->global_symbol_table);

    sbuf(struct symbol_table*) const chilling_symbol_tables =
        self->chilling_symbol_tables;
    for (size_t i = 0; i < sbuf_count(chilling_symbol_tables); ++i) {
        symbol_table_freeze(chilling_symbol_tables[i]);
    }
    sbuf_fini(self->chilling_symbol_tables);

    freeze_fini();

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
    sbuf_push(s_context.modules, module);

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

    for (size_t i = 0; i < sbuf_count(context()->modules); ++i) {
        if (context()->modules[i]->path == path) {
            return context()->modules[i];
        }
    }
    return NULL;
}

void
validate_main_is_defined_correctly(void)
{
    struct symbol const* main_symbol = NULL;
    for (size_t i = 0; i < sbuf_count(context()->static_symbols); ++i) {
        struct symbol const* const symbol = context()->static_symbols[i];
        if (symbol->name == context()->interned.main) {
            main_symbol = symbol;
            break;
        }
    }

    if (main_symbol == NULL) {
        fatal(NULL, "main is not defined");
    }

    if (main_symbol->kind != SYMBOL_FUNCTION) {
        fatal(main_symbol->location, "main is not defined as a function");
    }

    struct type const* const expected_type =
        type_unique_function(NULL, context()->builtin.void_);
    if (main_symbol->data.function->type != expected_type) {
        fatal(
            main_symbol->location,
            "main has invalid type `%s` (expected `%s`)",
            main_symbol->data.function->type->name,
            expected_type->name);
    }
}
