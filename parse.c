// Copyright 2021-2023 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "sunder.h"

struct parser {
    struct module* module;
    struct lexer* lexer;

    struct token const* current_token;
    struct token const* peek_token;
};
static struct parser*
parser_new(struct module* module, struct lexer* lexer);
static void
parser_del(struct parser* self);
static struct token const*
advance_token(struct parser* parser);
static bool
check_current(struct parser const* parser, enum token_kind kind);
static bool
check_peek(struct parser const* parser, enum token_kind kind);
static struct token const*
expect_current(struct parser* parser, enum token_kind kind);

static struct cst_module const*
parse_module(struct parser* parser);

static struct cst_namespace const*
parse_namespace(struct parser* parser);

static struct cst_import const*
parse_import(struct parser* parser);

static struct cst_decl const*
parse_decl(struct parser* parser);
static struct cst_decl const*
parse_decl_variable(struct parser* parser);
static struct cst_decl const*
parse_decl_constant(struct parser* parser);
static struct cst_decl const*
parse_decl_function(struct parser* parser);
static struct cst_decl const*
parse_decl_struct(struct parser* parser);
static struct cst_decl const*
parse_decl_extend(struct parser* parser);
static struct cst_decl const*
parse_decl_alias(struct parser* parser);
static struct cst_decl const*
parse_decl_extern_variable(struct parser* parser);
static struct cst_decl const*
parse_decl_extern_function(struct parser* parser);

static struct cst_stmt const*
parse_stmt(struct parser* parser);
static struct cst_stmt const*
parse_stmt_decl(struct parser* parser);
static struct cst_stmt const*
parse_stmt_defer(struct parser* parser);
static struct cst_stmt const*
parse_stmt_if(struct parser* parser);
static struct cst_stmt const*
parse_stmt_for(struct parser* parser);
static struct cst_stmt const*
parse_stmt_break(struct parser* parser);
static struct cst_stmt const*
parse_stmt_continue(struct parser* parser);
static struct cst_stmt const*
parse_stmt_return(struct parser* parser);

// Precedence levels roughly follow the precedence levels described in the
// operator precedence section of the Go Programming Language Specification.
// Sunder encourages using parentheses to disambiguate the order of evaluation
// for expressions with many operations, and an effort is made to keep the
// number of precedence levels small in comparison to the precedence levels of
// languages like C and C++.
enum precedence {
    PRECEDENCE_LOWEST,
    PRECEDENCE_OR, // or
    PRECEDENCE_AND, // and
    PRECEDENCE_COMPARE, // == != < <= > >=
    PRECEDENCE_SUM, // +% -% + - | ^
    PRECEDENCE_PRODUCT, // *% * / << >> &
    PRECEDENCE_PREFIX, // +x -x *x &x
    PRECEDENCE_POSTFIX, // foo(bar, 123) foo[42] foo.*
};
// Returns the precedence corresponding to provided token or PRECEDENCE_LOWEST
// if no precedence mapping exists.
static enum precedence
token_kind_precedence(enum token_kind kind);
static enum precedence
current_precedence(struct parser* parser);

// Parse function for a null denotation.
typedef struct cst_expr const* (*parse_nud_fn)(struct parser*);
// Parse function for a left denotation.
typedef struct cst_expr const* (*parse_led_fn)(
    struct parser*, struct cst_expr const*);
// Returns NULL if no function is associated with the provided token kind.
static parse_nud_fn
token_kind_nud(enum token_kind kind);
// Returns NULL if no function is associated with the provided token kind.
static parse_led_fn
token_kind_led(enum token_kind kind);

static struct cst_expr const*
parse_expr(struct parser* parser);
static struct cst_expr const*
parse_expr_symbol(struct parser* parser);
static struct cst_expr const*
parse_expr_boolean(struct parser* parser);
static struct cst_expr const*
parse_expr_integer(struct parser* parser);
static struct cst_expr const*
parse_expr_character(struct parser* parser);
static struct cst_expr const*
parse_expr_bytes(struct parser* parser);
static struct cst_expr const*
parse_expr_lparen(struct parser* parser);
static struct cst_expr const*
parse_expr_led_lparen(struct parser* parser, struct cst_expr const* lhs);
static struct cst_expr const*
parse_expr_led_lbracket(struct parser* parser, struct cst_expr const* lhs);
static struct cst_expr const*
parse_expr_led_dot_star(struct parser* parser, struct cst_expr const* lhs);
static struct cst_expr const*
parse_expr_led_dot(struct parser* parser, struct cst_expr const* lhs);
static struct cst_expr const*
parse_expr_sizeof(struct parser* parser);
static struct cst_expr const*
parse_expr_alignof(struct parser* parser);
static struct cst_expr const*
parse_expr_nud_unary(struct parser* parser);
static struct cst_expr const*
parse_expr_led_binary(struct parser* parser, struct cst_expr const* lhs);

static struct cst_block const*
parse_block(struct parser* parser);

static struct cst_symbol*
parse_symbol(struct parser* parser);
static struct cst_symbol_element*
parse_symbol_element(struct parser* parser);

static struct cst_identifier const* const*
parse_template_parameter_list(struct parser* parser);

static struct cst_typespec const* const*
parse_template_argument_list(struct parser* parser);

static struct cst_function_parameter const* const*
parse_function_parameter_list(struct parser* parser);
static struct cst_function_parameter const*
parse_function_parameter(struct parser* parser);

static struct cst_member const* const*
parse_member_list(struct parser* parser);
static struct cst_member const*
parse_member(struct parser* parser);
static struct cst_member const*
parse_member_variable(struct parser* parser);
static struct cst_member const*
parse_member_constant(struct parser* parser);
static struct cst_member const*
parse_member_function(struct parser* parser);

static struct cst_member_initializer const* const*
parse_member_initializer_list(struct parser* parser);
static struct cst_member_initializer const*
parse_member_initializer(struct parser* parser);

static struct cst_typespec const*
parse_typespec(struct parser* parser);
static struct cst_typespec const*
parse_typespec_symbol(struct parser* parser);
static struct cst_typespec const*
parse_typespec_function(struct parser* parser);
static struct cst_typespec const*
parse_typespec_pointer(struct parser* parser);
static struct cst_typespec const*
parse_typespec_array_or_slice(struct parser* parser);
static struct cst_typespec const*
parse_typespec_typeof(struct parser* parser);

static struct cst_identifier const*
parse_identifier(struct parser* parser);

static struct parser*
parser_new(struct module* module, struct lexer* lexer)
{
    assert(module != NULL);
    assert(lexer != NULL);

    struct parser* const self = xalloc(NULL, sizeof(*self));
    self->module = module;
    self->lexer = lexer;
    self->current_token = NULL;
    advance_token(self);
    advance_token(self);

    assert(self->current_token != NULL);
    assert(self->peek_token != NULL);
    return self;
}

static void
parser_del(struct parser* self)
{
    assert(self != NULL);

    memset(self, 0x00, sizeof(*self));
    xalloc(self, XALLOC_FREE);
}

static struct token const*
advance_token(struct parser* parser)
{
    assert(parser != NULL);

    struct token const* const current_token = parser->current_token;
    parser->current_token = parser->peek_token;
    parser->peek_token = lexer_next_token(parser->lexer);
    return current_token;
}

static bool
check_current(struct parser const* parser, enum token_kind kind)
{
    assert(parser != NULL);

    return parser->current_token->kind == kind;
}

static bool
check_peek(struct parser const* parser, enum token_kind kind)
{
    assert(parser != NULL);

    return parser->peek_token->kind == kind;
}

static struct token const*
expect_current(struct parser* parser, enum token_kind kind)
{
    assert(parser != NULL);

    if (!check_current(parser, kind)) {
        char const* const expected = token_kind_to_cstr(kind);
        char* const found = token_to_new_cstr(parser->current_token);
        fatal(
            &parser->current_token->location,
            "expected `%s`, found `%s`",
            expected,
            found);
    }
    return advance_token(parser);
}

static struct cst_module const*
parse_module(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_namespace const* namespace = NULL;
    if (check_current(parser, TOKEN_NAMESPACE)) {
        namespace = parse_namespace(parser);
    }

    sbuf(struct cst_import const*) imports = NULL;
    while (check_current(parser, TOKEN_IMPORT)) {
        sbuf_push(imports, parse_import(parser));
    }
    sbuf_freeze(imports);

    sbuf(struct cst_decl const*) decls = NULL;
    while (!check_current(parser, TOKEN_EOF)) {
        sbuf_push(decls, parse_decl(parser));
    }
    sbuf_freeze(decls);

    struct cst_module* const product =
        cst_module_new(namespace, imports, decls);

    freeze(product);
    return product;
}

static struct cst_namespace const*
parse_namespace(struct parser* parser)
{
    struct source_location const* const location =
        &expect_current(parser, TOKEN_NAMESPACE)->location;

    sbuf(struct cst_identifier const*) identifiers = NULL;
    sbuf_push(identifiers, parse_identifier(parser));
    while (!check_current(parser, TOKEN_SEMICOLON)) {
        expect_current(parser, TOKEN_COLON_COLON);
        sbuf_push(identifiers, parse_identifier(parser));
    }
    expect_current(parser, TOKEN_SEMICOLON);

    sbuf_freeze(identifiers);
    struct cst_namespace* const product =
        cst_namespace_new(location, identifiers);

    freeze(product);
    return product;
}

static struct cst_import const*
parse_import(struct parser* parser)
{
    struct source_location const* const location =
        &expect_current(parser, TOKEN_IMPORT)->location;

    struct string const* const bytes =
        expect_current(parser, TOKEN_BYTES)->data.bytes;
    char const* const path = intern_cstr(string_start(bytes));
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_import* const product = cst_import_new(location, path);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl(struct parser* parser)
{
    assert(parser != NULL);

    if (check_current(parser, TOKEN_VAR)) {
        return parse_decl_variable(parser);
    }

    if (check_current(parser, TOKEN_LET)) {
        return parse_decl_constant(parser);
    }

    if (check_current(parser, TOKEN_FUNC)) {
        return parse_decl_function(parser);
    }

    if (check_current(parser, TOKEN_STRUCT)) {
        return parse_decl_struct(parser);
    }

    if (check_current(parser, TOKEN_EXTEND)) {
        return parse_decl_extend(parser);
    }

    if (check_current(parser, TOKEN_ALIAS)) {
        return parse_decl_alias(parser);
    }

    if (check_current(parser, TOKEN_EXTERN) && check_peek(parser, TOKEN_VAR)) {
        return parse_decl_extern_variable(parser);
    }

    if (check_current(parser, TOKEN_EXTERN) && check_peek(parser, TOKEN_FUNC)) {
        return parse_decl_extern_function(parser);
    }

    fatal(
        &parser->current_token->location,
        "expected declaration, found `%s`",
        token_to_new_cstr(parser->current_token));
    return NULL;
}

static struct cst_decl const*
parse_decl_variable(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_VAR)->location;
    struct cst_identifier const* const identifier = parse_identifier(parser);
    struct cst_typespec const* typespec = NULL;
    if (check_current(parser, TOKEN_COLON)) {
        expect_current(parser, TOKEN_COLON);
        typespec = parse_typespec(parser);
    }
    expect_current(parser, TOKEN_ASSIGN);
    struct cst_expr const* expr = NULL;
    if (check_current(parser, TOKEN_UNINIT)) {
        expect_current(parser, TOKEN_UNINIT);
        if (typespec == NULL) {
            fatal(
                identifier->location,
                "uninitialized variable `%s` requires a type specifier",
                identifier->name);
        }
    }
    else {
        expr = parse_expr(parser);
    }
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_decl* const product =
        cst_decl_new_variable(location, identifier, typespec, expr);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_constant(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_LET)->location;
    struct cst_identifier const* const identifier = parse_identifier(parser);
    struct cst_typespec const* typespec = NULL;
    if (check_current(parser, TOKEN_COLON)) {
        expect_current(parser, TOKEN_COLON);
        typespec = parse_typespec(parser);
    }
    expect_current(parser, TOKEN_ASSIGN);
    struct cst_expr const* expr = NULL;
    if (check_current(parser, TOKEN_UNINIT)) {
        expect_current(parser, TOKEN_UNINIT);
        if (typespec == NULL) {
            fatal(
                identifier->location,
                "uninitialized constant `%s` requires a type specifier",
                identifier->name);
        }
    }
    else {
        expr = parse_expr(parser);
    }
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_decl* const product =
        cst_decl_new_constant(location, identifier, typespec, expr);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_function(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_FUNC)->location;
    struct cst_identifier const* const identifier = parse_identifier(parser);
    sbuf(struct cst_identifier const* const) const template_parameters =
        parse_template_parameter_list(parser);
    expect_current(parser, TOKEN_LPAREN);
    sbuf(struct cst_function_parameter const* const) const function_parameters =
        parse_function_parameter_list(parser);
    expect_current(parser, TOKEN_RPAREN);
    struct cst_typespec const* const return_typespec = parse_typespec(parser);
    struct cst_block const* const body = parse_block(parser);

    struct cst_decl* const product = cst_decl_new_function(
        location,
        identifier,
        template_parameters,
        function_parameters,
        return_typespec,
        body);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_struct(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_STRUCT)->location;
    struct cst_identifier const* const identifier = parse_identifier(parser);
    sbuf(struct cst_identifier const* const) const template_parameters =
        parse_template_parameter_list(parser);
    expect_current(parser, TOKEN_LBRACE);
    sbuf(struct cst_member const* const) members = parse_member_list(parser);
    expect_current(parser, TOKEN_RBRACE);

    struct cst_decl* const product =
        cst_decl_new_struct(location, identifier, template_parameters, members);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_extend(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_EXTEND)->location;
    struct cst_typespec const* const typespec = parse_typespec(parser);
    struct cst_decl const* const decl = parse_decl(parser);

    struct cst_decl* const product =
        cst_decl_new_extend(location, typespec, decl);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_alias(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_ALIAS)->location;
    struct cst_identifier const* const identifier = parse_identifier(parser);
    expect_current(parser, TOKEN_ASSIGN);
    struct cst_typespec const* const typespec = parse_typespec(parser);
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_decl* const product =
        cst_decl_new_alias(location, identifier, typespec);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_extern_variable(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_EXTERN)->location;
    expect_current(parser, TOKEN_VAR);
    struct cst_identifier const* const identifier = parse_identifier(parser);
    expect_current(parser, TOKEN_COLON);
    struct cst_typespec const* const typespec = parse_typespec(parser);
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_decl* const product =
        cst_decl_new_extern_variable(location, identifier, typespec);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_extern_function(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_EXTERN)->location;
    expect_current(parser, TOKEN_FUNC);
    struct cst_identifier const* const identifier = parse_identifier(parser);
    expect_current(parser, TOKEN_LPAREN);
    sbuf(struct cst_function_parameter const* const) const function_parameters =
        parse_function_parameter_list(parser);
    expect_current(parser, TOKEN_RPAREN);
    struct cst_typespec const* const return_typespec = parse_typespec(parser);
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_decl* const product = cst_decl_new_extern_function(
        location, identifier, function_parameters, return_typespec);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt(struct parser* parser)
{
    assert(parser != NULL);

    if (check_current(parser, TOKEN_VAR) || check_current(parser, TOKEN_LET)
        || check_current(parser, TOKEN_FUNC)
        || check_current(parser, TOKEN_ALIAS)) {
        return parse_stmt_decl(parser);
    }

    if (check_current(parser, TOKEN_DEFER)) {
        return parse_stmt_defer(parser);
    }

    if (check_current(parser, TOKEN_IF)) {
        return parse_stmt_if(parser);
    }

    if (check_current(parser, TOKEN_FOR)) {
        return parse_stmt_for(parser);
    }

    if (check_current(parser, TOKEN_BREAK)) {
        return parse_stmt_break(parser);
    }

    if (check_current(parser, TOKEN_CONTINUE)) {
        return parse_stmt_continue(parser);
    }

    if (check_current(parser, TOKEN_RETURN)) {
        return parse_stmt_return(parser);
    }

    struct cst_expr const* const expr = parse_expr(parser);
    if (check_current(parser, TOKEN_ASSIGN)) {
        // <stmt-assign>
        struct source_location const* const location =
            &expect_current(parser, TOKEN_ASSIGN)->location;
        struct cst_expr const* const rhs = parse_expr(parser);
        expect_current(parser, TOKEN_SEMICOLON);

        struct cst_stmt* const product =
            cst_stmt_new_assign(location, expr, rhs);
        freeze(product);
        return product;
    }

    // <stmt-expr>
    expect_current(parser, TOKEN_SEMICOLON);
    struct cst_stmt* const product = cst_stmt_new_expr(expr);
    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt_decl(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_decl const* const decl = parse_decl(parser);
    struct cst_stmt* const product = cst_stmt_new_decl(decl);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt_defer(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_DEFER)->location;

    if (check_current(parser, TOKEN_LBRACE)) {
        struct cst_block const* block = parse_block(parser);
        struct cst_stmt* const product =
            cst_stmt_new_defer_block(location, block);

        freeze(product);
        return product;
    }

    struct cst_expr const* const expr = parse_expr(parser);
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_stmt* const product = cst_stmt_new_defer_expr(location, expr);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt_if(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_IF));

    sbuf(struct cst_conditional const*) conditionals = NULL;

    struct source_location const* location =
        &expect_current(parser, TOKEN_IF)->location;
    struct cst_expr const* condition = parse_expr(parser);
    struct cst_block const* body = parse_block(parser);
    struct cst_conditional* conditional =
        cst_conditional_new(location, condition, body);
    freeze(conditional);
    sbuf_push(conditionals, conditional);

    while (check_current(parser, TOKEN_ELIF)) {
        location = &advance_token(parser)->location;
        condition = parse_expr(parser);
        body = parse_block(parser);
        conditional = cst_conditional_new(location, condition, body);
        freeze(conditional);
        sbuf_push(conditionals, conditional);
    }

    if (check_current(parser, TOKEN_ELSE)) {
        location = &advance_token(parser)->location;
        body = parse_block(parser);
        conditional = cst_conditional_new(location, NULL, body);
        freeze(conditional);
        sbuf_push(conditionals, conditional);
    }

    sbuf_freeze(conditionals);
    struct cst_stmt* const product = cst_stmt_new_if(conditionals);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt_for(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_FOR));

    struct source_location const* const location =
        &expect_current(parser, TOKEN_FOR)->location;

    // <stmt-for-range>
    if (check_current(parser, TOKEN_IDENTIFIER)
        && check_peek(parser, TOKEN_IN)) {
        struct cst_identifier const* const identifier =
            parse_identifier(parser);
        expect_current(parser, TOKEN_IN);

        struct cst_expr const* begin = parse_expr(parser);
        if (check_current(parser, TOKEN_COLON)) {
            expect_current(parser, TOKEN_COLON);
            struct cst_expr const* const end = parse_expr(parser);
            struct cst_block const* const body = parse_block(parser);

            struct cst_stmt* const product =
                cst_stmt_new_for_range(location, identifier, begin, end, body);

            freeze(product);
            return product;
        }

        struct cst_expr const* const end = begin;
        struct cst_block const* const body = parse_block(parser);

        struct cst_stmt* const product =
            cst_stmt_new_for_range(location, identifier, NULL, end, body);

        freeze(product);
        return product;
    }

    // <stmt-for-expr>
    struct cst_expr const* const expr = parse_expr(parser);
    struct cst_block const* const body = parse_block(parser);

    struct cst_stmt* const product =
        cst_stmt_new_for_expr(location, expr, body);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt_break(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_BREAK));

    struct source_location const* const location =
        &expect_current(parser, TOKEN_BREAK)->location;

    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_stmt* const product = cst_stmt_new_break(location);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt_continue(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_CONTINUE));

    struct source_location const* const location =
        &expect_current(parser, TOKEN_CONTINUE)->location;

    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_stmt* const product = cst_stmt_new_continue(location);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt_return(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_RETURN));

    struct source_location const* const location =
        &expect_current(parser, TOKEN_RETURN)->location;

    struct cst_expr const* expr = NULL;
    if (!check_current(parser, TOKEN_SEMICOLON)) {
        expr = parse_expr(parser);
    }

    expect_current(parser, TOKEN_SEMICOLON);
    struct cst_stmt* const product = cst_stmt_new_return(location, expr);

    freeze(product);
    return product;
}

static enum precedence
token_kind_precedence(enum token_kind kind)
{
    switch (kind) {
    case TOKEN_OR: {
        return PRECEDENCE_OR;
    }
    case TOKEN_AND: {
        return PRECEDENCE_AND;
    }
    case TOKEN_EQ: /* fallthrough */
    case TOKEN_NE: /* fallthrough */
    case TOKEN_LE: /* fallthrough */
    case TOKEN_LT: /* fallthrough */
    case TOKEN_GE: /* fallthrough */
    case TOKEN_GT: {
        return PRECEDENCE_COMPARE;
    }
    case TOKEN_PLUS_PERCENT: /* fallthrough */
    case TOKEN_DASH_PERCENT: /* fallthrough */
    case TOKEN_PLUS: /* fallthrough */
    case TOKEN_DASH: /* fallthrough */
    case TOKEN_PIPE: /* fallthrough */
    case TOKEN_CARET: {
        return PRECEDENCE_SUM;
    }
    case TOKEN_STAR_PERCENT: /* fallthrough */
    case TOKEN_STAR: /* fallthrough */
    case TOKEN_FSLASH: /* fallthrough */
    case TOKEN_PERCENT: /* fallthrough */
    case TOKEN_SHL: /* fallthrough */
    case TOKEN_SHR: /* fallthrough */
    case TOKEN_AMPERSAND: {
        return PRECEDENCE_PRODUCT;
    }
    case TOKEN_LPAREN: /* fallthrough */
    case TOKEN_LBRACKET: /* fallthrough */
    case TOKEN_DOT_STAR: /* fallthrough */
    case TOKEN_DOT: {
        return PRECEDENCE_POSTFIX;
    }
    default: {
        break;
    }
    }

    return PRECEDENCE_LOWEST;
}

static enum precedence
current_precedence(struct parser* parser)
{
    assert(parser != NULL);

    return token_kind_precedence(parser->current_token->kind);
}

static parse_nud_fn
token_kind_nud(enum token_kind kind)
{
    switch (kind) {
    case TOKEN_IDENTIFIER: /* fallthrough */
    case TOKEN_COLON_COLON: {
        return parse_expr_symbol;
    }
    case TOKEN_TRUE: /* fallthrough */
    case TOKEN_FALSE: {
        return parse_expr_boolean;
    }
    case TOKEN_INTEGER: {
        return parse_expr_integer;
    }
    case TOKEN_CHARACTER: {
        return parse_expr_character;
    }
    case TOKEN_BYTES: {
        return parse_expr_bytes;
    }
    case TOKEN_LPAREN: {
        return parse_expr_lparen;
    }
    case TOKEN_SIZEOF: {
        return parse_expr_sizeof;
    }
    case TOKEN_ALIGNOF: {
        return parse_expr_alignof;
    }
    case TOKEN_NOT: /* fallthrough */
    case TOKEN_STARTOF: /* fallthrough */
    case TOKEN_COUNTOF: /* fallthrough */
    case TOKEN_PLUS: /* fallthrough */
    case TOKEN_DASH: /* fallthrough */
    case TOKEN_DASH_PERCENT: /* fallthrough */
    case TOKEN_TILDE: /* fallthrough */
    case TOKEN_STAR: /* fallthrough */
    case TOKEN_AMPERSAND: {
        return parse_expr_nud_unary;
    }
    default: {
        break;
    }
    }

    return NULL;
}

static parse_led_fn
token_kind_led(enum token_kind kind)
{
    switch (kind) {
    case TOKEN_LPAREN: {
        return parse_expr_led_lparen;
    }
    case TOKEN_LBRACKET: {
        return parse_expr_led_lbracket;
    }
    case TOKEN_DOT_STAR: {
        return parse_expr_led_dot_star;
    }
    case TOKEN_DOT: {
        return parse_expr_led_dot;
    }
    case TOKEN_OR: /* fallthrough */
    case TOKEN_AND: /* fallthrough */
    case TOKEN_SHL: /* fallthrough */
    case TOKEN_SHR: /* fallthrough */
    case TOKEN_EQ: /* fallthrough */
    case TOKEN_NE: /* fallthrough */
    case TOKEN_LE: /* fallthrough */
    case TOKEN_LT: /* fallthrough */
    case TOKEN_GE: /* fallthrough */
    case TOKEN_GT: /* fallthrough */
    case TOKEN_PLUS: /* fallthrough */
    case TOKEN_PLUS_PERCENT: /* fallthrough */
    case TOKEN_DASH: /* fallthrough */
    case TOKEN_DASH_PERCENT: /* fallthrough */
    case TOKEN_STAR: /* fallthrough */
    case TOKEN_STAR_PERCENT: /* fallthrough */
    case TOKEN_FSLASH: /* fallthrough */
    case TOKEN_PERCENT: /* fallthrough */
    case TOKEN_PIPE: /* fallthrough */
    case TOKEN_CARET: /* fallthrough */
    case TOKEN_AMPERSAND: {
        return parse_expr_led_binary;
    }
    default: {
        break;
    }
    }

    return NULL;
}

static struct cst_expr const*
parse_expr_precedence(struct parser* parser, enum precedence precedence)
{
    assert(parser != NULL);

    struct token const* const nud_token = parser->current_token;
    parse_nud_fn const parse_nud = token_kind_nud(nud_token->kind);
    if (parse_nud == NULL) {
        fatal(
            &nud_token->location,
            "unrecognized prefix token `%s` in expression",
            token_kind_to_cstr(nud_token->kind));
    }

    struct cst_expr const* expr = parse_nud(parser);
    while (precedence < current_precedence(parser)) {
        parse_led_fn const parse_led =
            token_kind_led(parser->current_token->kind);
        if (parse_led == NULL) {
            return expr;
        }
        expr = parse_led(parser, expr);
    }

    return expr;
}

static struct cst_expr const*
parse_expr(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_expr const* const product =
        parse_expr_precedence(parser, PRECEDENCE_LOWEST);

    return product;
}

static struct cst_expr const*
parse_expr_symbol(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_symbol const* const symbol = parse_symbol(parser);

    struct cst_expr* const product = cst_expr_new_symbol(symbol);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_boolean(struct parser* parser)
{
    assert(parser != NULL);

    struct token const* const token = advance_token(parser);
    assert(token->kind == TOKEN_TRUE || token->kind == TOKEN_FALSE);
    struct cst_expr* const product = cst_expr_new_boolean(token);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_integer(struct parser* parser)
{
    assert(parser != NULL);

    struct token const* const token = expect_current(parser, TOKEN_INTEGER);
    struct cst_expr* const product = cst_expr_new_integer(token);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_character(struct parser* parser)
{
    assert(parser != NULL);

    struct token const* const token = expect_current(parser, TOKEN_CHARACTER);
    struct cst_expr* const product = cst_expr_new_character(token);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_bytes(struct parser* parser)
{
    assert(parser != NULL);

    struct token const* const token = expect_current(parser, TOKEN_BYTES);
    struct cst_expr* const product = cst_expr_new_bytes(token);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_lparen(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_LPAREN)->location;

    if (!check_current(parser, TOKEN_COLON)) {
        // <expr-grouped>
        struct cst_expr const* const expr = parse_expr(parser);
        expect_current(parser, TOKEN_RPAREN);
        struct cst_expr* const product = cst_expr_new_grouped(location, expr);

        freeze(product);
        return product;
    }

    expect_current(parser, TOKEN_COLON);
    struct cst_typespec const* const typespec = parse_typespec(parser);
    expect_current(parser, TOKEN_RPAREN);

    if (check_current(parser, TOKEN_LBRACKET)) {
        // <expr-array>
        expect_current(parser, TOKEN_LBRACKET);
        sbuf(struct cst_expr const*) elements = NULL;
        struct cst_expr const* ellipsis = NULL;
        while (!check_current(parser, TOKEN_RBRACKET)) {
            if (sbuf_count(elements) != 0u) {
                expect_current(parser, TOKEN_COMMA);
            }

            struct cst_expr const* const expr = parse_expr(parser);
            if (check_current(parser, TOKEN_ELLIPSIS)) {
                expect_current(parser, TOKEN_ELLIPSIS);
                ellipsis = expr;
                break;
            }

            sbuf_push(elements, expr);
        }
        sbuf_freeze(elements);
        expect_current(parser, TOKEN_RBRACKET);

        struct cst_expr* const product =
            cst_expr_new_list(location, typespec, elements, ellipsis);

        freeze(product);
        return product;
    }

    if (check_current(parser, TOKEN_LBRACE)) {
        if (check_peek(parser, TOKEN_RBRACE) || check_peek(parser, TOKEN_DOT)) {
            // <expr-struct>
            expect_current(parser, TOKEN_LBRACE);
            sbuf(struct cst_member_initializer const* const) initializers =
                parse_member_initializer_list(parser);
            expect_current(parser, TOKEN_RBRACE);

            struct cst_expr* const product =
                cst_expr_new_struct(location, typespec, initializers);

            freeze(product);
            return product;
        }

        // <expr-slice>
        expect_current(parser, TOKEN_LBRACE);
        struct cst_expr const* const pointer = parse_expr(parser);
        expect_current(parser, TOKEN_COMMA);
        struct cst_expr const* const count = parse_expr(parser);
        expect_current(parser, TOKEN_RBRACE);

        struct cst_expr* const product =
            cst_expr_new_slice(location, typespec, pointer, count);

        freeze(product);
        return product;
    }

    // <expr-cast>
    struct cst_expr const* const expr =
        parse_expr_precedence(parser, PRECEDENCE_PREFIX);

    struct cst_expr* const product =
        cst_expr_new_cast(location, typespec, expr);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_led_lparen(struct parser* parser, struct cst_expr const* lhs)
{
    assert(parser != NULL);
    assert(lhs != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_LPAREN)->location;
    sbuf(struct cst_expr const*) args = NULL;
    while (!check_current(parser, TOKEN_RPAREN)) {
        if (sbuf_count(args) != 0) {
            expect_current(parser, TOKEN_COMMA);
        }
        sbuf_push(args, parse_expr(parser));
    }
    sbuf_freeze(args);
    expect_current(parser, TOKEN_RPAREN);
    struct cst_expr* const product = cst_expr_new_call(location, lhs, args);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_led_lbracket(struct parser* parser, struct cst_expr const* lhs)
{
    assert(parser != NULL);
    assert(lhs != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_LBRACKET)->location;
    struct cst_expr const* const idx = parse_expr(parser);

    if (check_current(parser, TOKEN_COLON)) {
        // <expr-access-slice>
        expect_current(parser, TOKEN_COLON);
        struct cst_expr const* const end = parse_expr(parser);
        expect_current(parser, TOKEN_RBRACKET);

        struct cst_expr* const product =
            cst_expr_new_access_slice(location, lhs, idx, end);

        freeze(product);
        return product;
    }

    // <expr-access-index>
    expect_current(parser, TOKEN_RBRACKET);
    struct cst_expr* const product =
        cst_expr_new_access_index(location, lhs, idx);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_led_dot_star(struct parser* parser, struct cst_expr const* lhs)
{
    assert(parser != NULL);
    assert(lhs != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_DOT_STAR)->location;

    struct cst_expr* const product =
        cst_expr_new_access_dereference(location, lhs);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_led_dot(struct parser* parser, struct cst_expr const* lhs)
{
    assert(parser != NULL);
    assert(lhs != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_DOT)->location;
    struct cst_symbol_element const* member = parse_symbol_element(parser);

    struct cst_expr* const product =
        cst_expr_new_access_member(location, lhs, member);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_nud_unary(struct parser* parser)
{
    assert(parser != NULL);

    struct token const* const op = advance_token(parser);
    bool const paren = op->kind == TOKEN_STARTOF || op->kind == TOKEN_COUNTOF;

    if (paren) {
        expect_current(parser, TOKEN_LPAREN);
    }
    struct cst_expr const* const rhs =
        parse_expr_precedence(parser, PRECEDENCE_PREFIX);
    if (paren) {
        expect_current(parser, TOKEN_RPAREN);
    }

    struct cst_expr* const product = cst_expr_new_unary(op, rhs);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_sizeof(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_SIZEOF)->location;
    expect_current(parser, TOKEN_LPAREN);
    struct cst_typespec const* const rhs = parse_typespec(parser);
    expect_current(parser, TOKEN_RPAREN);

    struct cst_expr* const product = cst_expr_new_sizeof(location, rhs);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_alignof(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_ALIGNOF)->location;
    expect_current(parser, TOKEN_LPAREN);
    struct cst_typespec const* const rhs = parse_typespec(parser);
    expect_current(parser, TOKEN_RPAREN);

    struct cst_expr* const product = cst_expr_new_alignof(location, rhs);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_led_binary(struct parser* parser, struct cst_expr const* lhs)
{
    assert(parser != NULL);
    assert(lhs != NULL);

    struct token const* const op = advance_token(parser);
    struct cst_expr const* const rhs =
        parse_expr_precedence(parser, token_kind_precedence(op->kind));
    struct cst_expr* const product = cst_expr_new_binary(op, lhs, rhs);

    freeze(product);
    return product;
}

static struct cst_block const*
parse_block(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_LBRACE)->location;

    sbuf(struct cst_stmt const*) stmts = NULL;
    while (!check_current(parser, TOKEN_RBRACE)) {
        sbuf_push(stmts, parse_stmt(parser));
    }
    sbuf_freeze(stmts);
    expect_current(parser, TOKEN_RBRACE);

    struct cst_block* const product = cst_block_new(location, stmts);

    freeze(product);
    return product;
}

static struct cst_symbol*
parse_symbol(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* location = NULL;
    bool is_from_root = false;
    if (check_current(parser, TOKEN_COLON_COLON)) {
        is_from_root = true;
        location = &expect_current(parser, TOKEN_COLON_COLON)->location;
    }

    sbuf(struct cst_symbol_element const*) elements = NULL;
    sbuf_push(elements, parse_symbol_element(parser));
    if (!is_from_root) {
        location = elements[0]->location;
    }
    while (check_current(parser, TOKEN_COLON_COLON)) {
        expect_current(parser, TOKEN_COLON_COLON);
        sbuf_push(elements, parse_symbol_element(parser));
    }
    sbuf_freeze(elements);

    struct cst_symbol* const product =
        cst_symbol_new(location, is_from_root, elements);

    freeze(product);
    return product;
}

static struct cst_symbol_element*
parse_symbol_element(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_identifier const* const identifier = parse_identifier(parser);
    struct cst_typespec const* const* template_arguments = NULL;
    if (check_current(parser, TOKEN_LBRACKET)
        && check_peek(parser, TOKEN_LBRACKET)) {
        template_arguments = parse_template_argument_list(parser);
    }

    struct cst_symbol_element* const product =
        cst_symbol_element_new(identifier, template_arguments);

    freeze(product);
    return product;
}

static struct cst_identifier const* const*
parse_template_parameter_list(struct parser* parser)
{
    assert(parser != NULL);

    sbuf(struct cst_identifier const*) template_parameters = NULL;
    if (!check_current(parser, TOKEN_LBRACKET)) {
        return template_parameters;
    }

    struct token const* const lbracket = expect_current(parser, TOKEN_LBRACKET);
    expect_current(parser, TOKEN_LBRACKET);

    if (check_current(parser, TOKEN_RBRACKET)) {
        fatal(
            &lbracket->location,
            "template parameter list declared with zero parameters");
    }

    sbuf_push(template_parameters, parse_identifier(parser));
    while (check_current(parser, TOKEN_COMMA)) {
        advance_token(parser);
        sbuf_push(template_parameters, parse_identifier(parser));
    }

    expect_current(parser, TOKEN_RBRACKET);
    expect_current(parser, TOKEN_RBRACKET);

    sbuf_freeze(template_parameters);
    return template_parameters;
}

static struct cst_typespec const* const*
parse_template_argument_list(struct parser* parser)
{
    assert(parser != NULL);

    struct token const* const lbracket = expect_current(parser, TOKEN_LBRACKET);
    expect_current(parser, TOKEN_LBRACKET);

    sbuf(struct cst_typespec const*) template_arguments = NULL;
    if (check_current(parser, TOKEN_RBRACKET)) {
        fatal(
            &lbracket->location,
            "template argument list contains zero template arguments");
    }

    sbuf_push(template_arguments, parse_typespec(parser));
    while (check_current(parser, TOKEN_COMMA)) {
        advance_token(parser);
        sbuf_push(template_arguments, parse_typespec(parser));
    }

    expect_current(parser, TOKEN_RBRACKET);
    expect_current(parser, TOKEN_RBRACKET);

    sbuf_freeze(template_arguments);
    return template_arguments;
}

static struct cst_function_parameter const* const*
parse_function_parameter_list(struct parser* parser)
{
    assert(parser != NULL);

    sbuf(struct cst_function_parameter const*) function_parameters = NULL;
    if (!check_current(parser, TOKEN_IDENTIFIER)) {
        return function_parameters;
    }

    sbuf_push(function_parameters, parse_function_parameter(parser));
    while (check_current(parser, TOKEN_COMMA)) {
        advance_token(parser);
        sbuf_push(function_parameters, parse_function_parameter(parser));
    }

    sbuf_freeze(function_parameters);
    return function_parameters;
}

static struct cst_function_parameter const*
parse_function_parameter(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_identifier const* const identifier = parse_identifier(parser);
    expect_current(parser, TOKEN_COLON);
    struct cst_typespec const* const typespec = parse_typespec(parser);

    struct cst_function_parameter* const product =
        cst_function_parameter_new(identifier, typespec);

    freeze(product);
    return product;
}

static struct cst_member const* const*
parse_member_list(struct parser* parser)
{
    assert(parser != NULL);

    sbuf(struct cst_member const*) members = NULL;
    while (!check_current(parser, TOKEN_RBRACE)) {
        sbuf_push(members, parse_member(parser));
    }

    sbuf_freeze(members);
    return members;
}

static struct cst_member const*
parse_member(struct parser* parser)
{
    assert(parser != NULL);

    if (check_current(parser, TOKEN_VAR)) {
        return parse_member_variable(parser);
    }

    if (check_current(parser, TOKEN_LET)) {
        return parse_member_constant(parser);
    }

    if (check_current(parser, TOKEN_FUNC)) {
        return parse_member_function(parser);
    }

    fatal(
        &parser->current_token->location,
        "expected member variable, member constant, or member function, found `%s`",
        token_to_new_cstr(parser->current_token));
    return NULL;
}

static struct cst_member const*
parse_member_variable(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_VAR)->location;
    struct cst_identifier const* const identifier = parse_identifier(parser);
    expect_current(parser, TOKEN_COLON);
    struct cst_typespec const* const typespec = parse_typespec(parser);
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_member* const product =
        cst_member_new_variable(location, identifier, typespec);

    freeze(product);
    return product;
}

static struct cst_member const*
parse_member_constant(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_decl const* const decl = parse_decl_constant(parser);

    struct cst_member* const product = cst_member_new_constant(decl);

    freeze(product);
    return product;
}

static struct cst_member const*
parse_member_function(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_decl const* const decl = parse_decl_function(parser);

    struct cst_member* const product = cst_member_new_function(decl);

    freeze(product);
    return product;
}

static struct cst_member_initializer const* const*
parse_member_initializer_list(struct parser* parser)
{
    assert(parser != NULL);

    sbuf(struct cst_member_initializer const*) member_initializers = NULL;
    if (!check_current(parser, TOKEN_DOT)) {
        return member_initializers;
    }

    sbuf_push(member_initializers, parse_member_initializer(parser));
    while (check_current(parser, TOKEN_COMMA)) {
        advance_token(parser);
        sbuf_push(member_initializers, parse_member_initializer(parser));
    }

    sbuf_freeze(member_initializers);
    return member_initializers;
}

static struct cst_member_initializer const*
parse_member_initializer(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const* const location =
        &expect_current(parser, TOKEN_DOT)->location;
    struct cst_identifier const* identifier = parse_identifier(parser);
    expect_current(parser, TOKEN_ASSIGN);
    struct cst_expr const* expr = NULL;
    if (check_current(parser, TOKEN_UNINIT)) {
        expect_current(parser, TOKEN_UNINIT);
    }
    else {
        expr = parse_expr(parser);
    }

    struct cst_member_initializer* const product =
        cst_member_initializer_new(location, identifier, expr);

    freeze(product);
    return product;
}

static struct cst_typespec const*
parse_typespec(struct parser* parser)
{
    assert(parser != NULL);

    if (check_current(parser, TOKEN_IDENTIFIER)
        || check_current(parser, TOKEN_COLON_COLON)) {
        return parse_typespec_symbol(parser);
    }

    if (check_current(parser, TOKEN_FUNC)) {
        return parse_typespec_function(parser);
    }

    if (check_current(parser, TOKEN_STAR)) {
        return parse_typespec_pointer(parser);
    }

    if (check_current(parser, TOKEN_LBRACKET)) {
        return parse_typespec_array_or_slice(parser);
    }

    if (check_current(parser, TOKEN_TYPEOF)) {
        return parse_typespec_typeof(parser);
    }

    fatal(
        &parser->current_token->location,
        "expected type specifier, found `%s`",
        token_to_new_cstr(parser->current_token));
    return NULL;
}

static struct cst_typespec const*
parse_typespec_symbol(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_symbol const* const symbol = parse_symbol(parser);

    struct cst_typespec* const product = cst_typespec_new_symbol(symbol);

    freeze(product);
    return product;
}

static struct cst_typespec const*
parse_typespec_function(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_FUNC));

    struct source_location const* const location =
        &expect_current(parser, TOKEN_FUNC)->location;

    expect_current(parser, TOKEN_LPAREN);
    sbuf(struct cst_typespec const*) parameter_typespecs = NULL;
    if (!check_current(parser, TOKEN_RPAREN)) {
        sbuf_push(parameter_typespecs, parse_typespec(parser));
        while (check_current(parser, TOKEN_COMMA)) {
            advance_token(parser);
            sbuf_push(parameter_typespecs, parse_typespec(parser));
        }
    }
    expect_current(parser, TOKEN_RPAREN);
    sbuf_freeze(parameter_typespecs);

    struct cst_typespec const* const return_typespec = parse_typespec(parser);

    struct cst_typespec* const product = cst_typespec_new_function(
        location, parameter_typespecs, return_typespec);

    freeze(product);
    return product;
}

static struct cst_typespec const*
parse_typespec_pointer(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_STAR));

    struct source_location const* const location =
        &expect_current(parser, TOKEN_STAR)->location;

    struct cst_typespec const* const base = parse_typespec(parser);

    struct cst_typespec* const product =
        cst_typespec_new_pointer(location, base);

    freeze(product);
    return product;
}

static struct cst_typespec const*
parse_typespec_array_or_slice(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_LBRACKET));

    struct source_location const* const location =
        &expect_current(parser, TOKEN_LBRACKET)->location;

    if (check_current(parser, TOKEN_RBRACKET)) {
        // <typespec-slice>
        expect_current(parser, TOKEN_RBRACKET);
        struct cst_typespec const* const base = parse_typespec(parser);

        struct cst_typespec* const product =
            cst_typespec_new_slice(location, base);

        freeze(product);
        return product;
    }

    // <typespec-array>
    struct cst_expr const* const count = parse_expr(parser);
    expect_current(parser, TOKEN_RBRACKET);
    struct cst_typespec const* const base = parse_typespec(parser);

    struct cst_typespec* const product =
        cst_typespec_new_array(location, count, base);

    freeze(product);
    return product;
}

static struct cst_typespec const*
parse_typespec_typeof(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_TYPEOF));

    struct source_location const* const location =
        &expect_current(parser, TOKEN_TYPEOF)->location;
    expect_current(parser, TOKEN_LPAREN);
    struct cst_expr const* expr = parse_expr(parser);
    expect_current(parser, TOKEN_RPAREN);

    struct cst_typespec* const product =
        cst_typespec_new_typeof(location, expr);

    freeze(product);
    return product;
}

static struct cst_identifier const*
parse_identifier(struct parser* parser)
{
    assert(parser != NULL);

    struct token const* const token = expect_current(parser, TOKEN_IDENTIFIER);
    struct source_location const* const location = &token->location;
    struct cst_identifier* const product =
        cst_identifier_new(location, token->data.identifier);

    freeze(product);
    return product;
}

void
parse(struct module* module)
{
    assert(module != NULL);
    assert(module->cst == NULL);

    struct lexer* const lexer = lexer_new(module);
    struct parser* const parser = parser_new(module, lexer);

    module->cst = parse_module(parser);

    lexer_del(lexer);
    parser_del(parser);
}
