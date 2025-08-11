// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "sunder.h"

struct parser {
    struct module* module;
    struct lexer* lexer;

    struct token current_token;
    struct token peek_token;
};
static struct parser*
parser_new(struct module* module, struct lexer* lexer);
static void
parser_del(struct parser* self);
static struct token
advance_token(struct parser* parser);
static bool
check_current(struct parser const* parser, enum token_kind kind);
static bool
check_peek(struct parser const* parser, enum token_kind kind);
static struct token
expect_current(struct parser* parser, enum token_kind kind);

static struct cst_identifier
parse_identifier(struct parser* parser);

static struct cst_block
parse_block(struct parser* parser);

static struct cst_switch_case
parse_switch_case(struct parser* parser);

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
parse_decl_union(struct parser* parser);
static struct cst_decl const*
parse_decl_enum(struct parser* parser);
static struct cst_decl const*
parse_decl_extend(struct parser* parser);
static struct cst_decl const*
parse_decl_alias(struct parser* parser);
static struct cst_decl const*
parse_decl_extern_variable(struct parser* parser);
static struct cst_decl const*
parse_decl_extern_function(struct parser* parser);
static struct cst_decl const*
parse_decl_extern_type(struct parser* parser);

static struct cst_stmt const*
parse_stmt(struct parser* parser);
static struct cst_stmt const*
parse_stmt_decl(struct parser* parser);
static struct cst_stmt const*
parse_stmt_defer(struct parser* parser);
static struct cst_stmt const*
parse_stmt_if(struct parser* parser);
static struct cst_stmt const*
parse_stmt_when(struct parser* parser);
static struct cst_stmt const*
parse_stmt_for(struct parser* parser);
static struct cst_stmt const*
parse_stmt_break(struct parser* parser);
static struct cst_stmt const*
parse_stmt_continue(struct parser* parser);
static struct cst_stmt const*
parse_stmt_switch(struct parser* parser);
static struct cst_stmt const*
parse_stmt_return(struct parser* parser);
static struct cst_stmt const*
parse_stmt_assert(struct parser* parser);

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
parse_expr_ieee754(struct parser* parser);
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
parse_expr_defined(struct parser* parser);
static struct cst_expr const*
parse_expr_sizeof(struct parser* parser);
static struct cst_expr const*
parse_expr_alignof(struct parser* parser);
static struct cst_expr const*
parse_expr_fileof(struct parser* parser);
static struct cst_expr const*
parse_expr_lineof(struct parser* parser);
static struct cst_expr const*
parse_expr_embed(struct parser* parser);
static struct cst_expr const*
parse_expr_nud_unary(struct parser* parser);
static struct cst_expr const*
parse_expr_led_binary(struct parser* parser, struct cst_expr const* lhs);

static struct cst_symbol*
parse_symbol(struct parser* parser);
static struct cst_symbol_element*
parse_symbol_element(struct parser* parser);

static struct cst_identifier const*
parse_template_parameter_list(struct parser* parser);

static struct cst_type const* const*
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
static struct cst_member const*
parse_member_alias(struct parser* parser);

static struct cst_member_initializer const* const*
parse_member_initializer_list(struct parser* parser);
static struct cst_member_initializer const*
parse_member_initializer(struct parser* parser);

static struct cst_enum_value const*
parse_enum_value(struct parser* parser);

static struct cst_type const*
parse_type(struct parser* parser);
static struct cst_type const*
parse_type_symbol(struct parser* parser);
static struct cst_type const*
parse_type_function(struct parser* parser);
static struct cst_type const*
parse_type_pointer(struct parser* parser);
static struct cst_type const*
parse_type_array_or_slice(struct parser* parser);
static struct cst_type const*
parse_type_struct(struct parser* parser);
static struct cst_type const*
parse_type_union(struct parser* parser);
static struct cst_type const*
parse_type_enum(struct parser* parser);
static struct cst_type const*
parse_type_typeof(struct parser* parser);

static struct parser*
parser_new(struct module* module, struct lexer* lexer)
{
    assert(module != NULL);
    assert(lexer != NULL);

    struct parser* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));

    self->module = module;
    self->lexer = lexer;
    advance_token(self);
    advance_token(self);

    return self;
}

static void
parser_del(struct parser* self)
{
    assert(self != NULL);

    memset(self, 0x00, sizeof(*self));
    xalloc(self, XALLOC_FREE);
}

static struct token
advance_token(struct parser* parser)
{
    assert(parser != NULL);

    struct token const current_token = parser->current_token;
    parser->current_token = parser->peek_token;
    parser->peek_token = lexer_next_token(parser->lexer);
    return current_token;
}

static bool
check_current(struct parser const* parser, enum token_kind kind)
{
    assert(parser != NULL);

    return parser->current_token.kind == kind;
}

static bool
check_peek(struct parser const* parser, enum token_kind kind)
{
    assert(parser != NULL);

    return parser->peek_token.kind == kind;
}

static struct token
expect_current(struct parser* parser, enum token_kind kind)
{
    assert(parser != NULL);

    if (!check_current(parser, kind)) {
        char const* const expected = token_kind_to_cstr(kind);
        char* const found = token_to_new_cstr(parser->current_token);
        fatal(
            parser->current_token.location,
            "expected `%s`, found `%s`",
            expected,
            found);
    }
    return advance_token(parser);
}

static struct cst_identifier
parse_identifier(struct parser* parser)
{
    assert(parser != NULL);

    struct token const token = expect_current(parser, TOKEN_IDENTIFIER);
    struct source_location const location = token.location;
    struct cst_identifier product =
        cst_identifier_init(location, token.data.identifier);

    return product;
}

static struct cst_block
parse_block(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_LBRACE).location;

    sbuf(struct cst_stmt const*) stmts = NULL;
    while (!check_current(parser, TOKEN_RBRACE)) {
        sbuf_push(stmts, parse_stmt(parser));
    }
    sbuf_freeze(stmts);
    expect_current(parser, TOKEN_RBRACE);

    struct cst_block const product = cst_block_init(location, stmts);

    return product;
}

static struct cst_switch_case
parse_switch_case(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location = parser->current_token.location;

    sbuf(struct cst_symbol const*) symbols = NULL;
    if (check_current(parser, TOKEN_ELSE)) {
        expect_current(parser, TOKEN_ELSE);
    }
    else {
        sbuf_push(symbols, parse_symbol(parser));
        while (check_current(parser, TOKEN_OR)) {
            expect_current(parser, TOKEN_OR);
            sbuf_push(symbols, parse_symbol(parser));
        }
    }
    sbuf_freeze(symbols);

    struct cst_block const block = parse_block(parser);

    struct cst_switch_case product =
        cst_switch_case_init(location, symbols, block);

    return product;
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
    struct source_location const location =
        expect_current(parser, TOKEN_NAMESPACE).location;

    sbuf(struct cst_identifier) identifiers = NULL;
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
    struct source_location const location =
        expect_current(parser, TOKEN_IMPORT).location;

    struct string const* const bytes =
        expect_current(parser, TOKEN_BYTES).data.bytes;
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

    if (check_current(parser, TOKEN_UNION)) {
        return parse_decl_union(parser);
    }

    if (check_current(parser, TOKEN_ENUM)) {
        return parse_decl_enum(parser);
    }

    if (check_current(parser, TOKEN_EXTEND)) {
        return parse_decl_extend(parser);
    }

    if (check_current(parser, TOKEN_TYPE)) {
        return parse_decl_alias(parser);
    }

    if (check_current(parser, TOKEN_EXTERN) && check_peek(parser, TOKEN_VAR)) {
        return parse_decl_extern_variable(parser);
    }

    if (check_current(parser, TOKEN_EXTERN) && check_peek(parser, TOKEN_FUNC)) {
        return parse_decl_extern_function(parser);
    }

    if (check_current(parser, TOKEN_EXTERN) && check_peek(parser, TOKEN_TYPE)) {
        return parse_decl_extern_type(parser);
    }

    fatal(
        parser->current_token.location,
        "expected declaration, found `%s`",
        token_to_new_cstr(parser->current_token));
    return NULL;
}

static struct cst_decl const*
parse_decl_variable(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_VAR).location;
    struct cst_identifier const identifier = parse_identifier(parser);
    struct cst_type const* type = NULL;
    if (check_current(parser, TOKEN_COLON)) {
        expect_current(parser, TOKEN_COLON);
        type = parse_type(parser);
    }
    expect_current(parser, TOKEN_ASSIGN);
    struct cst_expr const* expr = NULL;
    if (check_current(parser, TOKEN_UNINIT)) {
        expect_current(parser, TOKEN_UNINIT);
        if (type == NULL) {
            fatal(
                identifier.location,
                "uninitialized variable `%s` requires a type specifier",
                identifier.name);
        }
    }
    else {
        expr = parse_expr(parser);
    }
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_decl* const product =
        cst_decl_new_variable(location, identifier, type, expr);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_constant(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_LET).location;
    struct cst_identifier const identifier = parse_identifier(parser);
    struct cst_type const* type = NULL;
    if (check_current(parser, TOKEN_COLON)) {
        expect_current(parser, TOKEN_COLON);
        type = parse_type(parser);
    }
    expect_current(parser, TOKEN_ASSIGN);
    struct cst_expr const* expr = NULL;
    if (check_current(parser, TOKEN_UNINIT)) {
        expect_current(parser, TOKEN_UNINIT);
        if (type == NULL) {
            fatal(
                identifier.location,
                "uninitialized constant `%s` requires a type specifier",
                identifier.name);
        }
    }
    else {
        expr = parse_expr(parser);
    }
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_decl* const product =
        cst_decl_new_constant(location, identifier, type, expr);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_function(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_FUNC).location;
    struct cst_identifier const identifier = parse_identifier(parser);
    sbuf(struct cst_identifier const) const template_parameters =
        parse_template_parameter_list(parser);
    expect_current(parser, TOKEN_LPAREN);
    sbuf(struct cst_function_parameter const* const) const function_parameters =
        parse_function_parameter_list(parser);
    expect_current(parser, TOKEN_RPAREN);
    struct cst_type const* const return_type = parse_type(parser);
    struct cst_block const body = parse_block(parser);

    struct cst_decl* const product = cst_decl_new_function(
        location,
        identifier,
        template_parameters,
        function_parameters,
        return_type,
        body);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_struct(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_STRUCT).location;
    struct cst_identifier const identifier = parse_identifier(parser);
    sbuf(struct cst_identifier const) const template_parameters =
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
parse_decl_union(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_UNION).location;
    struct cst_identifier const identifier = parse_identifier(parser);
    sbuf(struct cst_identifier const) const template_parameters =
        parse_template_parameter_list(parser);
    expect_current(parser, TOKEN_LBRACE);
    sbuf(struct cst_member const* const) members = parse_member_list(parser);
    expect_current(parser, TOKEN_RBRACE);

    struct cst_decl* const product =
        cst_decl_new_union(location, identifier, template_parameters, members);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_enum(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_ENUM).location;
    struct cst_identifier const identifier = parse_identifier(parser);

    struct cst_type const* type = NULL;
    if (check_current(parser, TOKEN_COLON)) {
        expect_current(parser, TOKEN_COLON);
        type = parse_type(parser);
    }

    expect_current(parser, TOKEN_LBRACE);

    sbuf(struct cst_enum_value const*) values = NULL;
    while (!check_current(parser, TOKEN_RBRACE)
           && !check_current(parser, TOKEN_FUNC)) {
        sbuf_push(values, parse_enum_value(parser));
    }
    sbuf_freeze(values);

    sbuf(struct cst_member const*) member_functions = NULL;
    while (!check_current(parser, TOKEN_RBRACE)) {
        sbuf_push(member_functions, parse_member_function(parser));
    }
    sbuf_freeze(member_functions);

    expect_current(parser, TOKEN_RBRACE);

    struct cst_decl* const product =
        cst_decl_new_enum(location, identifier, type, values, member_functions);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_extend(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_EXTEND).location;
    struct cst_type const* const type = parse_type(parser);
    struct cst_decl const* const decl = parse_decl(parser);

    struct cst_decl* const product = cst_decl_new_extend(location, type, decl);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_alias(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_TYPE).location;
    struct cst_identifier const identifier = parse_identifier(parser);
    expect_current(parser, TOKEN_ASSIGN);
    struct cst_type const* const type = parse_type(parser);
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_decl* const product =
        cst_decl_new_alias(location, identifier, type);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_extern_variable(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_EXTERN).location;
    expect_current(parser, TOKEN_VAR);
    struct cst_identifier const identifier = parse_identifier(parser);
    expect_current(parser, TOKEN_COLON);
    struct cst_type const* const type = parse_type(parser);
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_decl* const product =
        cst_decl_new_extern_variable(location, identifier, type);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_extern_function(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_EXTERN).location;
    expect_current(parser, TOKEN_FUNC);
    struct cst_identifier const identifier = parse_identifier(parser);
    expect_current(parser, TOKEN_LPAREN);
    sbuf(struct cst_function_parameter const* const) const function_parameters =
        parse_function_parameter_list(parser);
    expect_current(parser, TOKEN_RPAREN);
    struct cst_type const* const return_type = parse_type(parser);
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_decl* const product = cst_decl_new_extern_function(
        location, identifier, function_parameters, return_type);

    freeze(product);
    return product;
}

static struct cst_decl const*
parse_decl_extern_type(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_EXTERN).location;
    expect_current(parser, TOKEN_TYPE);
    struct cst_identifier const identifier = parse_identifier(parser);
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_decl* const product =
        cst_decl_new_extern_type(location, identifier);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt(struct parser* parser)
{
    assert(parser != NULL);

    if (check_current(parser, TOKEN_VAR) || check_current(parser, TOKEN_LET)
        || check_current(parser, TOKEN_FUNC)
        || check_current(parser, TOKEN_TYPE)) {
        return parse_stmt_decl(parser);
    }

    if (check_current(parser, TOKEN_DEFER)) {
        return parse_stmt_defer(parser);
    }

    if (check_current(parser, TOKEN_IF)) {
        return parse_stmt_if(parser);
    }

    if (check_current(parser, TOKEN_WHEN)) {
        return parse_stmt_when(parser);
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

    if (check_current(parser, TOKEN_SWITCH)) {
        return parse_stmt_switch(parser);
    }

    if (check_current(parser, TOKEN_RETURN)) {
        return parse_stmt_return(parser);
    }

    if (check_current(parser, TOKEN_ASSERT)) {
        return parse_stmt_assert(parser);
    }

    struct cst_expr const* const expr = parse_expr(parser);
    static enum token_kind const assignment_ops[] = {
        TOKEN_ASSIGN,
        TOKEN_PLUS_ASSIGN,
        TOKEN_DASH_ASSIGN,
        TOKEN_STAR_ASSIGN,
        TOKEN_FSLASH_ASSIGN,
        TOKEN_PERCENT_ASSIGN,
        TOKEN_PLUS_PERCENT_ASSIGN,
        TOKEN_DASH_PERCENT_ASSIGN,
        TOKEN_STAR_PERCENT_ASSIGN,
        TOKEN_SHL_ASSIGN,
        TOKEN_SHR_ASSIGN,
        TOKEN_PIPE_ASSIGN,
        TOKEN_CARET_ASSIGN,
        TOKEN_AMPERSAND_ASSIGN,
    };
    for (size_t i = 0; i < ARRAY_COUNT(assignment_ops); ++i) {
        if (check_current(parser, assignment_ops[i])) {
            // <stmt-assign>
            struct token const op = expect_current(parser, assignment_ops[i]);
            struct source_location const location = op.location;
            struct cst_expr const* const rhs = parse_expr(parser);
            expect_current(parser, TOKEN_SEMICOLON);

            struct cst_stmt* const product =
                cst_stmt_new_assign(location, op, expr, rhs);
            freeze(product);
            return product;
        }
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

    struct source_location const location =
        expect_current(parser, TOKEN_DEFER).location;

    if (check_current(parser, TOKEN_LBRACE)) {
        struct cst_block const block = parse_block(parser);
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

    sbuf(struct cst_conditional) conditionals = NULL;

    struct source_location location = expect_current(parser, TOKEN_IF).location;
    struct cst_expr const* condition = parse_expr(parser);
    struct cst_block body = parse_block(parser);
    struct cst_conditional conditional =
        cst_conditional_init(location, condition, body);
    sbuf_push(conditionals, conditional);

    while (check_current(parser, TOKEN_ELIF)) {
        location = advance_token(parser).location;
        condition = parse_expr(parser);
        body = parse_block(parser);
        conditional = cst_conditional_init(location, condition, body);
        sbuf_push(conditionals, conditional);
    }

    if (check_current(parser, TOKEN_ELSE)) {
        location = advance_token(parser).location;
        body = parse_block(parser);
        conditional = cst_conditional_init(location, NULL, body);
        sbuf_push(conditionals, conditional);
    }

    sbuf_freeze(conditionals);
    struct cst_stmt* const product = cst_stmt_new_if(conditionals);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt_when(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_WHEN));

    sbuf(struct cst_conditional) conditionals = NULL;

    struct source_location location =
        expect_current(parser, TOKEN_WHEN).location;
    struct cst_expr const* condition = parse_expr(parser);
    struct cst_block body = parse_block(parser);
    struct cst_conditional conditional =
        cst_conditional_init(location, condition, body);
    sbuf_push(conditionals, conditional);

    while (check_current(parser, TOKEN_ELWHEN)) {
        location = advance_token(parser).location;
        condition = parse_expr(parser);
        body = parse_block(parser);
        conditional = cst_conditional_init(location, condition, body);
        sbuf_push(conditionals, conditional);
    }

    if (check_current(parser, TOKEN_ELSE)) {
        location = advance_token(parser).location;
        body = parse_block(parser);
        conditional = cst_conditional_init(location, NULL, body);
        sbuf_push(conditionals, conditional);
    }

    sbuf_freeze(conditionals);
    struct cst_stmt* const product = cst_stmt_new_when(conditionals);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt_for(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_FOR));

    struct source_location const location =
        expect_current(parser, TOKEN_FOR).location;

    // <stmt-for-range>
    bool const is_for_range = check_current(parser, TOKEN_IDENTIFIER)
        && (check_peek(parser, TOKEN_COLON) || check_peek(parser, TOKEN_IN));
    if (is_for_range) {
        struct cst_identifier const identifier = parse_identifier(parser);

        struct cst_type const* type = NULL;
        if (check_current(parser, TOKEN_COLON)) {
            expect_current(parser, TOKEN_COLON);
            type = parse_type(parser);
        }

        expect_current(parser, TOKEN_IN);

        struct cst_expr const* begin = parse_expr(parser);
        if (check_current(parser, TOKEN_COLON)) {
            expect_current(parser, TOKEN_COLON);
            struct cst_expr const* const end = parse_expr(parser);
            struct cst_block const body = parse_block(parser);

            struct cst_stmt* const product = cst_stmt_new_for_range(
                location, identifier, type, begin, end, body);

            freeze(product);
            return product;
        }

        struct cst_expr const* const end = begin;
        struct cst_block const body = parse_block(parser);

        struct cst_stmt* const product =
            cst_stmt_new_for_range(location, identifier, type, NULL, end, body);

        freeze(product);
        return product;
    }

    // <stmt-for-expr>
    struct cst_expr const* const expr = parse_expr(parser);
    struct cst_block const body = parse_block(parser);

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

    struct source_location const location =
        expect_current(parser, TOKEN_BREAK).location;

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

    struct source_location const location =
        expect_current(parser, TOKEN_CONTINUE).location;

    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_stmt* const product = cst_stmt_new_continue(location);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt_switch(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_SWITCH));

    struct source_location const location =
        expect_current(parser, TOKEN_SWITCH).location;

    struct cst_expr const* expr = parse_expr(parser);

    sbuf(struct cst_switch_case) cases = NULL;
    expect_current(parser, TOKEN_LBRACE);
    while (!check_current(parser, TOKEN_RBRACE)) {
        bool const is_else = check_current(parser, TOKEN_ELSE);
        sbuf_push(cases, parse_switch_case(parser));
        if (is_else) {
            // An `else` case always indicates the end of the switch statement.
            break;
        }
    }
    sbuf_freeze(cases);
    expect_current(parser, TOKEN_RBRACE);

    struct cst_stmt* const product = cst_stmt_new_switch(location, expr, cases);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt_return(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_RETURN));

    struct source_location const location =
        expect_current(parser, TOKEN_RETURN).location;

    struct cst_expr const* expr = NULL;
    if (!check_current(parser, TOKEN_SEMICOLON)) {
        expr = parse_expr(parser);
    }

    expect_current(parser, TOKEN_SEMICOLON);
    struct cst_stmt* const product = cst_stmt_new_return(location, expr);

    freeze(product);
    return product;
}

static struct cst_stmt const*
parse_stmt_assert(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_ASSERT));

    struct source_location const location =
        expect_current(parser, TOKEN_ASSERT).location;
    struct cst_expr const* const expr = parse_expr(parser);
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_stmt* const product = cst_stmt_new_assert(location, expr);
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

    return token_kind_precedence(parser->current_token.kind);
}

static parse_nud_fn
token_kind_nud(enum token_kind kind)
{
    switch (kind) {
    case TOKEN_IDENTIFIER: /* fallthrough */
    case TOKEN_TYPEOF: /* fallthrough */
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
    case TOKEN_IEEE754: {
        return parse_expr_ieee754;
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
    case TOKEN_DEFINED: {
        return parse_expr_defined;
    }
    case TOKEN_SIZEOF: {
        return parse_expr_sizeof;
    }
    case TOKEN_ALIGNOF: {
        return parse_expr_alignof;
    }
    case TOKEN_FILEOF: {
        return parse_expr_fileof;
    }
    case TOKEN_LINEOF: {
        return parse_expr_lineof;
    }
    case TOKEN_EMBED: {
        return parse_expr_embed;
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

    struct token const nud_token = parser->current_token;
    parse_nud_fn const parse_nud = token_kind_nud(nud_token.kind);
    if (parse_nud == NULL) {
        fatal(
            nud_token.location,
            "unrecognized prefix token `%s` in expression",
            token_kind_to_cstr(nud_token.kind));
    }

    struct cst_expr const* expr = parse_nud(parser);
    while (precedence < current_precedence(parser)) {
        parse_led_fn const parse_led =
            token_kind_led(parser->current_token.kind);
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

    struct token const token = advance_token(parser);
    assert(token.kind == TOKEN_TRUE || token.kind == TOKEN_FALSE);
    struct cst_expr* const product = cst_expr_new_boolean(token);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_integer(struct parser* parser)
{
    assert(parser != NULL);

    struct token const token = expect_current(parser, TOKEN_INTEGER);
    struct cst_expr* const product = cst_expr_new_integer(token);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_ieee754(struct parser* parser)
{
    assert(parser != NULL);

    struct token const token = expect_current(parser, TOKEN_IEEE754);
    struct cst_expr* const product = cst_expr_new_ieee754(token);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_character(struct parser* parser)
{
    assert(parser != NULL);

    struct token const token = expect_current(parser, TOKEN_CHARACTER);
    struct cst_expr* const product = cst_expr_new_character(token);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_bytes(struct parser* parser)
{
    assert(parser != NULL);

    struct token const token = expect_current(parser, TOKEN_BYTES);
    struct cst_expr* const product = cst_expr_new_bytes(token);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_lparen(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_LPAREN).location;

    // Special case warning for:
    //      (:::T){INITIALIZERS}
    // which is parsed as:
    //      (:: :T){INITIALIZERS}
    // instead of:
    //      (: ::T){INITIALIZERS}
    // as a user might expect.
    assert(parser->current_token.location.psrc != NULL);
    assert(parser->peek_token.location.psrc != NULL);
    bool const emit_colon_warning = check_current(parser, TOKEN_COLON_COLON)
        && check_peek(parser, TOKEN_COLON)
        && (parser->current_token.location.psrc + 2
            == parser->peek_token.location.psrc);
    if (emit_colon_warning) {
        warning(parser->current_token.location, "`:::` is parsed as `:: :`");
        info(parser->current_token.location, "write as `: ::` to disambiguate");
    }

    if (!check_current(parser, TOKEN_COLON)) {
        // <expr-grouped>
        struct cst_expr const* const expr = parse_expr(parser);
        expect_current(parser, TOKEN_RPAREN);
        struct cst_expr* const product = cst_expr_new_grouped(location, expr);

        freeze(product);
        return product;
    }

    expect_current(parser, TOKEN_COLON);
    struct cst_type const* const type = parse_type(parser);
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

            if (check_current(parser, TOKEN_RBRACKET)) {
                break;
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
            cst_expr_new_list(location, type, elements, ellipsis);

        freeze(product);
        return product;
    }

    if (check_current(parser, TOKEN_LBRACE)) {
        if (check_peek(parser, TOKEN_RBRACE) || check_peek(parser, TOKEN_DOT)) {
            // <expr-init>
            expect_current(parser, TOKEN_LBRACE);
            sbuf(struct cst_member_initializer const* const) initializers =
                parse_member_initializer_list(parser);
            expect_current(parser, TOKEN_RBRACE);

            struct cst_expr* const product =
                cst_expr_new_init(location, type, initializers);

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
            cst_expr_new_slice(location, type, pointer, count);

        freeze(product);
        return product;
    }

    // <expr-cast>
    struct cst_expr const* const expr =
        parse_expr_precedence(parser, PRECEDENCE_PREFIX);

    struct cst_expr* const product = cst_expr_new_cast(location, type, expr);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_led_lparen(struct parser* parser, struct cst_expr const* lhs)
{
    assert(parser != NULL);
    assert(lhs != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_LPAREN).location;
    sbuf(struct cst_expr const*) args = NULL;
    while (!check_current(parser, TOKEN_RPAREN)) {
        if (sbuf_count(args) != 0) {
            expect_current(parser, TOKEN_COMMA);
            if (check_current(parser, TOKEN_RPAREN)) {
                break;
            }
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

    struct source_location const location =
        expect_current(parser, TOKEN_LBRACKET).location;
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

    struct source_location const location =
        expect_current(parser, TOKEN_DOT_STAR).location;

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

    struct source_location const location =
        expect_current(parser, TOKEN_DOT).location;
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

    struct token const op = advance_token(parser);
    bool const paren = op.kind == TOKEN_STARTOF || op.kind == TOKEN_COUNTOF;

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
parse_expr_defined(struct parser* parser)
{
    assert(parser != NULL);

    expect_current(parser, TOKEN_DEFINED);
    expect_current(parser, TOKEN_LPAREN);
    struct cst_symbol const* const rhs = parse_symbol(parser);
    expect_current(parser, TOKEN_RPAREN);

    struct cst_expr* const product = cst_expr_new_defined(rhs);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_sizeof(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_SIZEOF).location;
    expect_current(parser, TOKEN_LPAREN);
    struct cst_type const* const rhs = parse_type(parser);
    expect_current(parser, TOKEN_RPAREN);

    struct cst_expr* const product = cst_expr_new_sizeof(location, rhs);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_alignof(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_ALIGNOF).location;
    expect_current(parser, TOKEN_LPAREN);
    struct cst_type const* const rhs = parse_type(parser);
    expect_current(parser, TOKEN_RPAREN);

    struct cst_expr* const product = cst_expr_new_alignof(location, rhs);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_fileof(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_FILEOF).location;
    expect_current(parser, TOKEN_LPAREN);
    expect_current(parser, TOKEN_RPAREN);

    struct cst_expr* const product = cst_expr_new_fileof(location);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_lineof(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_LINEOF).location;
    expect_current(parser, TOKEN_LPAREN);
    expect_current(parser, TOKEN_RPAREN);

    struct cst_expr* const product = cst_expr_new_lineof(location);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_embed(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_EMBED).location;
    expect_current(parser, TOKEN_LPAREN);
    struct string const* const bytes =
        expect_current(parser, TOKEN_BYTES).data.bytes;
    char const* const path = intern_cstr(string_start(bytes));
    expect_current(parser, TOKEN_RPAREN);

    struct cst_expr* const product = cst_expr_new_embed(location, path);

    freeze(product);
    return product;
}

static struct cst_expr const*
parse_expr_led_binary(struct parser* parser, struct cst_expr const* lhs)
{
    assert(parser != NULL);
    assert(lhs != NULL);

    struct token const op = advance_token(parser);
    struct cst_expr const* const rhs =
        parse_expr_precedence(parser, token_kind_precedence(op.kind));
    struct cst_expr* const product = cst_expr_new_binary(op, lhs, rhs);

    freeze(product);
    return product;
}

static struct cst_symbol*
parse_symbol(struct parser* parser)
{
    assert(parser != NULL);

    if (check_current(parser, TOKEN_COLON_COLON)) {
        enum cst_symbol_start const start = CST_SYMBOL_START_ROOT;
        struct source_location const location =
            expect_current(parser, TOKEN_COLON_COLON).location;

        sbuf(struct cst_symbol_element const*) elements = NULL;
        sbuf_push(elements, parse_symbol_element(parser));
        while (check_current(parser, TOKEN_COLON_COLON)) {
            expect_current(parser, TOKEN_COLON_COLON);
            sbuf_push(elements, parse_symbol_element(parser));
        }
        sbuf_freeze(elements);

        struct cst_symbol* const product =
            cst_symbol_new(location, start, NULL, elements);

        freeze(product);
        return product;
    }

    if (check_current(parser, TOKEN_TYPEOF)) {
        enum cst_symbol_start const start = CST_SYMBOL_START_TYPE;
        struct cst_type const* const type = parse_type(parser);
        struct source_location const location = type->location;

        expect_current(parser, TOKEN_COLON_COLON);

        sbuf(struct cst_symbol_element const*) elements = NULL;
        sbuf_push(elements, parse_symbol_element(parser));
        while (check_current(parser, TOKEN_COLON_COLON)) {
            expect_current(parser, TOKEN_COLON_COLON);
            sbuf_push(elements, parse_symbol_element(parser));
        }
        sbuf_freeze(elements);

        struct cst_symbol* const product =
            cst_symbol_new(location, start, type, elements);

        freeze(product);
        return product;
    }

    enum cst_symbol_start const start = CST_SYMBOL_START_NONE;
    sbuf(struct cst_symbol_element const*) elements = NULL;
    sbuf_push(elements, parse_symbol_element(parser));
    struct source_location const location = elements[0]->location;
    while (check_current(parser, TOKEN_COLON_COLON)) {
        expect_current(parser, TOKEN_COLON_COLON);
        sbuf_push(elements, parse_symbol_element(parser));
    }
    sbuf_freeze(elements);

    struct cst_symbol* const product =
        cst_symbol_new(location, start, NULL, elements);

    freeze(product);
    return product;
}

static struct cst_symbol_element*
parse_symbol_element(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_identifier const identifier = parse_identifier(parser);
    struct cst_type const* const* template_arguments = NULL;
    if (check_current(parser, TOKEN_LBRACKET)
        && check_peek(parser, TOKEN_LBRACKET)) {
        template_arguments = parse_template_argument_list(parser);
    }

    struct cst_symbol_element* const product =
        cst_symbol_element_new(identifier, template_arguments);

    freeze(product);
    return product;
}

static struct cst_identifier const*
parse_template_parameter_list(struct parser* parser)
{
    assert(parser != NULL);

    sbuf(struct cst_identifier) template_parameters = NULL;
    if (!check_current(parser, TOKEN_LBRACKET)) {
        return template_parameters;
    }

    struct token const lbracket = expect_current(parser, TOKEN_LBRACKET);
    expect_current(parser, TOKEN_LBRACKET);

    if (check_current(parser, TOKEN_RBRACKET)) {
        fatal(
            lbracket.location,
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

static struct cst_type const* const*
parse_template_argument_list(struct parser* parser)
{
    assert(parser != NULL);

    struct token const lbracket = expect_current(parser, TOKEN_LBRACKET);
    expect_current(parser, TOKEN_LBRACKET);

    sbuf(struct cst_type const*) template_arguments = NULL;
    if (check_current(parser, TOKEN_RBRACKET)) {
        fatal(
            lbracket.location,
            "template argument list contains zero template arguments");
    }

    sbuf_push(template_arguments, parse_type(parser));
    while (check_current(parser, TOKEN_COMMA)) {
        advance_token(parser);
        sbuf_push(template_arguments, parse_type(parser));
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

    struct cst_identifier const identifier = parse_identifier(parser);
    expect_current(parser, TOKEN_COLON);
    struct cst_type const* const type = parse_type(parser);

    struct cst_function_parameter* const product =
        cst_function_parameter_new(identifier, type);

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

    if (check_current(parser, TOKEN_TYPE)) {
        return parse_member_alias(parser);
    }

    fatal(
        parser->current_token.location,
        "expected member variable, member constant, member function, or type alias, found `%s`",
        token_to_new_cstr(parser->current_token));
    return NULL;
}

static struct cst_member const*
parse_member_variable(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_VAR).location;
    struct cst_identifier const identifier = parse_identifier(parser);
    expect_current(parser, TOKEN_COLON);
    struct cst_type const* const type = parse_type(parser);
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_member* const product =
        cst_member_new_variable(location, identifier, type);

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

static struct cst_member const*
parse_member_alias(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_decl const* const decl = parse_decl_alias(parser);

    struct cst_member* const product = cst_member_new_alias(decl);

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
        if (check_current(parser, TOKEN_RBRACE)) {
            break;
        }
        sbuf_push(member_initializers, parse_member_initializer(parser));
    }

    sbuf_freeze(member_initializers);
    return member_initializers;
}

static struct cst_member_initializer const*
parse_member_initializer(struct parser* parser)
{
    assert(parser != NULL);

    struct source_location const location =
        expect_current(parser, TOKEN_DOT).location;
    struct cst_identifier identifier = parse_identifier(parser);
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

static struct cst_enum_value const*
parse_enum_value(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_identifier const identifier = parse_identifier(parser);
    struct cst_expr const* expr = NULL;
    if (check_current(parser, TOKEN_ASSIGN)) {
        expect_current(parser, TOKEN_ASSIGN);
        expr = parse_expr(parser);
    }
    expect_current(parser, TOKEN_SEMICOLON);

    struct cst_enum_value* const product =
        cst_enum_value_new(identifier.location, identifier, expr);

    freeze(product);
    return product;
}

static struct cst_type const*
parse_type(struct parser* parser)
{
    assert(parser != NULL);

    if (check_current(parser, TOKEN_IDENTIFIER)
        || check_current(parser, TOKEN_COLON_COLON)) {
        return parse_type_symbol(parser);
    }

    if (check_current(parser, TOKEN_FUNC)) {
        return parse_type_function(parser);
    }

    if (check_current(parser, TOKEN_STAR)) {
        return parse_type_pointer(parser);
    }

    if (check_current(parser, TOKEN_LBRACKET)) {
        return parse_type_array_or_slice(parser);
    }

    if (check_current(parser, TOKEN_STRUCT)) {
        return parse_type_struct(parser);
    }

    if (check_current(parser, TOKEN_UNION)) {
        return parse_type_union(parser);
    }

    if (check_current(parser, TOKEN_ENUM)) {
        return parse_type_enum(parser);
    }

    if (check_current(parser, TOKEN_TYPEOF)) {
        return parse_type_typeof(parser);
    }

    fatal(
        parser->current_token.location,
        "expected type specifier, found `%s`",
        token_to_new_cstr(parser->current_token));
    return NULL;
}

static struct cst_type const*
parse_type_symbol(struct parser* parser)
{
    assert(parser != NULL);

    struct cst_symbol const* const symbol = parse_symbol(parser);

    struct cst_type* const product = cst_type_new_symbol(symbol);

    freeze(product);
    return product;
}

static struct cst_type const*
parse_type_function(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_FUNC));

    struct source_location const location =
        expect_current(parser, TOKEN_FUNC).location;

    expect_current(parser, TOKEN_LPAREN);
    sbuf(struct cst_type const*) parameter_types = NULL;
    if (!check_current(parser, TOKEN_RPAREN)) {
        sbuf_push(parameter_types, parse_type(parser));
        while (check_current(parser, TOKEN_COMMA)) {
            advance_token(parser);
            sbuf_push(parameter_types, parse_type(parser));
        }
    }
    expect_current(parser, TOKEN_RPAREN);
    sbuf_freeze(parameter_types);

    struct cst_type const* const return_type = parse_type(parser);

    struct cst_type* const product =
        cst_type_new_function(location, parameter_types, return_type);

    freeze(product);
    return product;
}

static struct cst_type const*
parse_type_pointer(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_STAR));

    struct source_location const location =
        expect_current(parser, TOKEN_STAR).location;

    struct cst_type const* const base = parse_type(parser);

    struct cst_type* const product = cst_type_new_pointer(location, base);

    freeze(product);
    return product;
}

static struct cst_type const*
parse_type_array_or_slice(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_LBRACKET));

    struct source_location const location =
        expect_current(parser, TOKEN_LBRACKET).location;

    if (check_current(parser, TOKEN_RBRACKET)) {
        // <type-slice>
        expect_current(parser, TOKEN_RBRACKET);
        struct cst_type const* const base = parse_type(parser);

        struct cst_type* const product = cst_type_new_slice(location, base);

        freeze(product);
        return product;
    }

    // <type-array>
    struct cst_expr const* const count = parse_expr(parser);
    expect_current(parser, TOKEN_RBRACKET);
    struct cst_type const* const base = parse_type(parser);

    struct cst_type* const product = cst_type_new_array(location, count, base);

    freeze(product);
    return product;
}

static struct cst_type const*
parse_type_struct(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_STRUCT));

    struct source_location const location =
        expect_current(parser, TOKEN_STRUCT).location;
    expect_current(parser, TOKEN_LBRACE);
    sbuf(struct cst_member const*) members = NULL;
    while (!check_current(parser, TOKEN_RBRACE)) {
        sbuf_push(members, parse_member_variable(parser));
    }
    sbuf_freeze(members);
    expect_current(parser, TOKEN_RBRACE);

    struct cst_type* const product = cst_type_new_struct(location, members);

    freeze(product);
    return product;
}

static struct cst_type const*
parse_type_union(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_UNION));

    struct source_location const location =
        expect_current(parser, TOKEN_UNION).location;
    expect_current(parser, TOKEN_LBRACE);
    sbuf(struct cst_member const*) members = NULL;
    while (!check_current(parser, TOKEN_RBRACE)) {
        sbuf_push(members, parse_member_variable(parser));
    }
    sbuf_freeze(members);
    expect_current(parser, TOKEN_RBRACE);

    struct cst_type* const product = cst_type_new_union(location, members);

    freeze(product);
    return product;
}

static struct cst_type const*
parse_type_enum(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_ENUM));

    struct source_location const location =
        expect_current(parser, TOKEN_ENUM).location;

    struct cst_type const* type = NULL;
    if (check_current(parser, TOKEN_COLON)) {
        expect_current(parser, TOKEN_COLON);
        type = parse_type(parser);
    }

    expect_current(parser, TOKEN_LBRACE);

    sbuf(struct cst_enum_value const*) values = NULL;
    while (!check_current(parser, TOKEN_RBRACE)) {
        sbuf_push(values, parse_enum_value(parser));
    }
    sbuf_freeze(values);

    expect_current(parser, TOKEN_RBRACE);

    struct cst_type* const product = cst_type_new_enum(location, type, values);

    freeze(product);
    return product;
}

static struct cst_type const*
parse_type_typeof(struct parser* parser)
{
    assert(parser != NULL);
    assert(check_current(parser, TOKEN_TYPEOF));

    struct source_location const location =
        expect_current(parser, TOKEN_TYPEOF).location;
    expect_current(parser, TOKEN_LPAREN);
    struct cst_expr const* expr = parse_expr(parser);
    expect_current(parser, TOKEN_RPAREN);

    struct cst_type* const product = cst_type_new_typeof(location, expr);

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
