// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <string.h>
#include "sunder.h"

struct cst_identifier
cst_identifier_init(struct source_location location, char const* name)
{
    assert(name != NULL);

    return (struct cst_identifier){
        .location = location,
        .name = name,
    };
}

struct cst_block
cst_block_init(
    struct source_location location, struct cst_stmt const* const* stmts)
{
    return (struct cst_block){
        .location = location,
        .stmts = stmts,
    };
}

struct cst_switch_case
cst_switch_case_init(
    struct source_location location,
    struct cst_symbol const* const* symbols,
    struct cst_block block)
{
    return (struct cst_switch_case){
        .location = location,
        .symbols = symbols,
        .block = block,
    };
}

struct cst_conditional
cst_conditional_init(
    struct source_location location,
    struct cst_expr const* condition,
    struct cst_block body)
{
    return (struct cst_conditional){
        .location = location,
        .condition = condition,
        .body = body,
    };
}

struct cst_module*
cst_module_new(
    struct cst_namespace const* namespace,
    struct cst_import const* const* imports,
    struct cst_decl const* const* decls)
{
    struct cst_module* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->namespace = namespace;
    self->imports = imports;
    self->decls = decls;
    return self;
}

struct cst_namespace*
cst_namespace_new(
    struct source_location location, struct cst_identifier const* identifiers)
{
    struct cst_namespace* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->identifiers = identifiers;
    return self;
}

struct cst_import*
cst_import_new(struct source_location location, char const* path)
{
    assert(path != NULL);

    struct cst_import* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->path = path;
    return self;
}

struct cst_decl*
cst_decl_new_variable(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_type const* type,
    struct cst_expr const* expr)
{
    struct cst_decl* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_VARIABLE;
    self->location = location;
    self->name = identifier.name;
    self->data.variable.identifier = identifier;
    self->data.variable.type = type;
    self->data.variable.expr = expr;
    return self;
}

struct cst_decl*
cst_decl_new_constant(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_type const* type,
    struct cst_expr const* expr)
{
    struct cst_decl* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_CONSTANT;
    self->location = location;
    self->name = identifier.name;
    self->data.constant.identifier = identifier;
    self->data.constant.type = type;
    self->data.constant.expr = expr;
    return self;
}

struct cst_decl*
cst_decl_new_function(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_identifier const* template_parameters,
    struct cst_function_parameter const* const* function_parameters,
    struct cst_type const* return_type,
    struct cst_block body)
{
    assert(return_type != NULL);

    struct cst_decl* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_FUNCTION;
    self->location = location;
    self->name = identifier.name;
    self->data.function.identifier = identifier;
    self->data.function.template_parameters = template_parameters;
    self->data.function.function_parameters = function_parameters;
    self->data.function.return_type = return_type;
    self->data.function.body = body;
    return self;
}

struct cst_decl*
cst_decl_new_struct(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_identifier const* template_parameters,
    struct cst_member const* const* members)
{
    struct cst_decl* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_STRUCT;
    self->location = location;
    self->name = identifier.name;
    self->data.struct_.identifier = identifier;
    self->data.struct_.template_parameters = template_parameters;
    self->data.struct_.members = members;
    return self;
}

struct cst_decl*
cst_decl_new_union(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_identifier const* template_parameters,
    struct cst_member const* const* members)
{
    struct cst_decl* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_UNION;
    self->location = location;
    self->name = identifier.name;
    self->data.union_.identifier = identifier;
    self->data.union_.template_parameters = template_parameters;
    self->data.union_.members = members;
    return self;
}

struct cst_decl*
cst_decl_new_enum(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_type const* type,
    struct cst_enum_value const* const* values,
    struct cst_member const* const* member_functions)
{
    struct cst_decl* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_ENUM;
    self->location = location;
    self->name = identifier.name;
    self->data.enum_.identifier = identifier;
    self->data.enum_.type = type;
    self->data.enum_.values = values;
    self->data.enum_.member_functions = member_functions;
    return self;
}

struct cst_decl*
cst_decl_new_extend(
    struct source_location location,
    struct cst_type const* type,
    struct cst_decl const* decl)
{
    assert(type != NULL);
    assert(decl != NULL);

    struct cst_decl* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_EXTEND;
    self->location = location;
    self->name = decl->name;
    self->data.extend.type = type;
    self->data.extend.decl = decl;
    return self;
}

struct cst_decl*
cst_decl_new_alias(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_type const* type)
{
    assert(type != NULL);

    struct cst_decl* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_ALIAS;
    self->location = location;
    self->name = identifier.name;
    self->data.alias.identifier = identifier;
    self->data.alias.type = type;
    return self;
}

struct cst_decl*
cst_decl_new_extern_variable(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_type const* type)
{
    assert(type != NULL);

    struct cst_decl* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_EXTERN_VARIABLE;
    self->location = location;
    self->name = identifier.name;
    self->data.extern_variable.identifier = identifier;
    self->data.extern_variable.type = type;
    return self;
}

struct cst_decl*
cst_decl_new_extern_function(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_function_parameter const* const* function_parameters,
    struct cst_type const* return_type)
{
    assert(return_type != NULL);

    struct cst_decl* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_EXTERN_FUNCTION;
    self->location = location;
    self->name = identifier.name;
    self->data.extern_function.identifier = identifier;
    self->data.extern_function.function_parameters = function_parameters;
    self->data.extern_function.return_type = return_type;
    return self;
}

struct cst_decl*
cst_decl_new_extern_type(
    struct source_location location, struct cst_identifier identifier)
{
    struct cst_decl* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_EXTERN_TYPE;
    self->location = location;
    self->name = identifier.name;
    self->data.extern_type.identifier = identifier;
    return self;
}

static struct cst_stmt*
cst_stmt_new(struct source_location location, enum cst_stmt_kind kind)
{
    struct cst_stmt* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->kind = kind;
    return self;
}

struct cst_stmt*
cst_stmt_new_if(struct cst_conditional const* conditionals)
{
    assert(sbuf_count(conditionals) > 0u);

    struct source_location const location = conditionals[0].location;
    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_IF);
    self->data.if_.conditionals = conditionals;
    return self;
}

struct cst_stmt*
cst_stmt_new_when(struct cst_conditional const* conditionals)
{
    assert(sbuf_count(conditionals) > 0u);

    struct source_location const location = conditionals[0].location;
    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_WHEN);
    self->data.when.conditionals = conditionals;
    return self;
}

struct cst_stmt*
cst_stmt_new_for_range(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_type const* type,
    struct cst_expr const* begin,
    struct cst_expr const* end,
    struct cst_block body)
{
    assert(end != NULL);

    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_FOR_RANGE);
    self->data.for_range.identifier = identifier;
    self->data.for_range.type = type;
    self->data.for_range.begin = begin;
    self->data.for_range.end = end;
    self->data.for_range.body = body;
    return self;
}

struct cst_stmt*
cst_stmt_new_for_expr(
    struct source_location location,
    struct cst_expr const* expr,
    struct cst_block body)
{
    assert(expr != NULL);

    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_FOR_EXPR);
    self->data.for_expr.expr = expr;
    self->data.for_expr.body = body;
    return self;
}

struct cst_stmt*
cst_stmt_new_decl(struct cst_decl const* decl)
{
    assert(decl != NULL);

    struct cst_stmt* const self = cst_stmt_new(decl->location, CST_STMT_DECL);
    self->data.decl = decl;
    return self;
}

struct cst_stmt*
cst_stmt_new_defer_block(
    struct source_location location, struct cst_block block)
{
    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_DEFER_BLOCK);
    self->data.defer_block = block;
    return self;
}

struct cst_stmt*
cst_stmt_new_defer_expr(
    struct source_location location, struct cst_expr const* expr)
{
    assert(expr != NULL);

    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_DEFER_EXPR);
    self->data.defer_expr = expr;
    return self;
}

struct cst_stmt*
cst_stmt_new_break(struct source_location location)
{
    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_BREAK);
    return self;
}

struct cst_stmt*
cst_stmt_new_continue(struct source_location location)
{
    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_CONTINUE);
    return self;
}

struct cst_stmt*
cst_stmt_new_switch(
    struct source_location location,
    struct cst_expr const* expr,
    struct cst_switch_case const* cases)
{
    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_SWITCH);
    self->data.switch_.expr = expr;
    self->data.switch_.cases = cases;
    return self;
}

struct cst_stmt*
cst_stmt_new_return(
    struct source_location location, struct cst_expr const* expr)
{
    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_RETURN);
    self->data.return_.expr = expr;
    return self;
}

struct cst_stmt*
cst_stmt_new_assert(
    struct source_location location, struct cst_expr const* expr)
{
    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_ASSERT);
    self->data.assert_.expr = expr;
    return self;
}

struct cst_stmt*
cst_stmt_new_assign(
    struct source_location location,
    struct token op,
    struct cst_expr const* lhs,
    struct cst_expr const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_ASSIGN);
    self->data.assign.op = op;
    self->data.assign.lhs = lhs;
    self->data.assign.rhs = rhs;
    return self;
}

struct cst_stmt*
cst_stmt_new_expr(struct cst_expr const* expr)
{
    assert(expr != NULL);

    struct cst_stmt* const self = cst_stmt_new(expr->location, CST_STMT_EXPR);
    self->data.expr = expr;
    return self;
}

static struct cst_expr*
cst_expr_new(struct source_location location, enum cst_expr_kind kind)
{
    struct cst_expr* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->kind = kind;
    return self;
}

struct cst_expr*
cst_expr_new_symbol(struct cst_symbol const* symbol)
{
    assert(symbol != NULL);

    struct cst_expr* const self =
        cst_expr_new(symbol->location, CST_EXPR_SYMBOL);
    self->data.symbol = symbol;
    return self;
}

struct cst_expr*
cst_expr_new_boolean(struct token token)
{
    assert(token.kind == TOKEN_TRUE || token.kind == TOKEN_FALSE);

    struct cst_expr* const self =
        cst_expr_new(token.location, CST_EXPR_BOOLEAN);
    self->data.boolean = token;
    return self;
}

struct cst_expr*
cst_expr_new_integer(struct token token)
{
    assert(token.kind == TOKEN_INTEGER);

    struct cst_expr* const self =
        cst_expr_new(token.location, CST_EXPR_INTEGER);
    self->data.integer = token;
    return self;
}

struct cst_expr*
cst_expr_new_ieee754(struct token token)
{
    assert(token.kind == TOKEN_IEEE754);

    struct cst_expr* const self =
        cst_expr_new(token.location, CST_EXPR_IEEE754);
    self->data.ieee754 = token;
    return self;
}

struct cst_expr*
cst_expr_new_character(struct token token)
{
    assert(token.kind == TOKEN_CHARACTER);

    struct cst_expr* const self =
        cst_expr_new(token.location, CST_EXPR_CHARACTER);
    self->data.character = token;
    return self;
}

struct cst_expr*
cst_expr_new_bytes(struct token token)
{
    assert(token.kind == TOKEN_BYTES);

    struct cst_expr* const self = cst_expr_new(token.location, CST_EXPR_BYTES);
    self->data.bytes = token;
    return self;
}

struct cst_expr*
cst_expr_new_list(
    struct source_location location,
    struct cst_type const* type,
    struct cst_expr const* const* elements,
    struct cst_expr const* ellipsis)
{
    assert(type != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_LIST);
    self->data.list.type = type;
    self->data.list.elements = elements;
    self->data.list.ellipsis = ellipsis;
    return self;
}

struct cst_expr*
cst_expr_new_slice(
    struct source_location location,
    struct cst_type const* type,
    struct cst_expr const* start,
    struct cst_expr const* count)
{
    assert(type != NULL);
    assert(start != NULL);
    assert(count != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_SLICE);
    self->data.slice.type = type;
    self->data.slice.start = start;
    self->data.slice.count = count;
    return self;
}

struct cst_expr*
cst_expr_new_init(
    struct source_location location,
    struct cst_type const* type,
    struct cst_member_initializer const* const* initializers)
{
    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_INIT);
    self->data.init.type = type;
    self->data.init.initializers = initializers;
    return self;
}

struct cst_expr*
cst_expr_new_cast(
    struct source_location location,
    struct cst_type const* type,
    struct cst_expr const* expr)
{
    assert(type != NULL);
    assert(expr != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_CAST);
    self->data.cast.type = type;
    self->data.cast.expr = expr;
    return self;
}

struct cst_expr*
cst_expr_new_grouped(
    struct source_location location, struct cst_expr const* expr)
{
    assert(expr != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_GROUPED);
    self->data.grouped.expr = expr;
    return self;
}

struct cst_expr*
cst_expr_new_call(
    struct source_location location,
    struct cst_expr const* func,
    struct cst_expr const* const* arguments)
{
    assert(func != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_CALL);
    self->data.call.func = func;
    self->data.call.arguments = arguments;
    return self;
}

struct cst_expr*
cst_expr_new_access_index(
    struct source_location location,
    struct cst_expr const* lhs,
    struct cst_expr const* idx)
{
    assert(lhs != NULL);
    assert(idx != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_ACCESS_INDEX);
    self->data.access_index.lhs = lhs;
    self->data.access_index.idx = idx;
    return self;
}

struct cst_expr*
cst_expr_new_access_slice(
    struct source_location location,
    struct cst_expr const* lhs,
    struct cst_expr const* begin,
    struct cst_expr const* end)
{
    assert(lhs != NULL);
    assert(begin != NULL);
    assert(end != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_ACCESS_SLICE);
    self->data.access_slice.lhs = lhs;
    self->data.access_slice.begin = begin;
    self->data.access_slice.end = end;
    return self;
}

struct cst_expr*
cst_expr_new_access_member(
    struct source_location location,
    struct cst_expr const* lhs,
    struct cst_symbol_element const* member)
{
    assert(lhs != NULL);
    assert(member != NULL);

    struct cst_expr* const self =
        cst_expr_new(location, CST_EXPR_ACCESS_MEMBER);
    self->data.access_member.lhs = lhs;
    self->data.access_member.member = member;
    return self;
}

struct cst_expr*
cst_expr_new_access_dereference(
    struct source_location location, struct cst_expr const* lhs)
{
    assert(lhs != NULL);

    struct cst_expr* const self =
        cst_expr_new(location, CST_EXPR_ACCESS_DEREFERENCE);
    self->data.access_member.lhs = lhs;
    return self;
}

struct cst_expr*
cst_expr_new_defined(struct cst_symbol const* symbol)
{
    assert(symbol != NULL);

    struct cst_expr* const self =
        cst_expr_new(symbol->location, CST_EXPR_DEFINED);
    self->data.defined.symbol = symbol;
    return self;
}

struct cst_expr*
cst_expr_new_sizeof(struct source_location location, struct cst_type const* rhs)
{
    assert(rhs != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_SIZEOF);
    self->data.sizeof_.rhs = rhs;
    return self;
}

struct cst_expr*
cst_expr_new_alignof(
    struct source_location location, struct cst_type const* rhs)
{
    assert(rhs != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_ALIGNOF);
    self->data.alignof_.rhs = rhs;
    return self;
}

struct cst_expr*
cst_expr_new_fileof(struct source_location location)
{
    return cst_expr_new(location, CST_EXPR_FILEOF);
}

struct cst_expr*
cst_expr_new_lineof(struct source_location location)
{
    return cst_expr_new(location, CST_EXPR_LINEOF);
}

struct cst_expr*
cst_expr_new_embed(struct source_location location, char const* path)
{
    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_EMBED);
    self->data.embed_.path = path;
    return self;
}

struct cst_expr*
cst_expr_new_unary(struct token op, struct cst_expr const* rhs)
{
    assert(rhs != NULL);

    struct cst_expr* const self = cst_expr_new(op.location, CST_EXPR_UNARY);
    self->data.unary.op = op;
    self->data.unary.rhs = rhs;
    return self;
}

struct cst_expr*
cst_expr_new_binary(
    struct token op, struct cst_expr const* lhs, struct cst_expr const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    struct cst_expr* const self = cst_expr_new(op.location, CST_EXPR_BINARY);
    self->data.binary.op = op;
    self->data.binary.lhs = lhs;
    self->data.binary.rhs = rhs;
    return self;
}

struct cst_symbol*
cst_symbol_new(
    struct source_location location,
    enum cst_symbol_start start,
    struct cst_type const* type,
    struct cst_symbol_element const* const* elements)
{
    assert(sbuf_count(elements) > 0);
    assert(start == CST_SYMBOL_START_TYPE || type == NULL);

    struct cst_symbol* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->start = start;
    self->type = type;
    self->elements = elements;
    return self;
}

struct cst_symbol_element*
cst_symbol_element_new(
    struct cst_identifier identifier,
    struct cst_type const* const* template_arguments)
{
    struct cst_symbol_element* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = identifier.location;
    self->identifier = identifier;
    self->template_arguments = template_arguments;
    return self;
}

struct cst_function_parameter*
cst_function_parameter_new(
    struct cst_identifier identifier, struct cst_type const* type)
{
    assert(type != NULL);

    struct cst_function_parameter* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = identifier.location;
    self->identifier = identifier;
    self->type = type;
    return self;
}

struct cst_member*
cst_member_new_variable(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_type const* type)
{
    assert(type != NULL);

    struct cst_member* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->name = identifier.name;
    self->kind = CST_MEMBER_VARIABLE;
    self->data.variable.identifier = identifier;
    self->data.variable.type = type;
    return self;
}

struct cst_member*
cst_member_new_constant(struct cst_decl const* decl)
{
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_CONSTANT);

    struct cst_member* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = decl->location;
    self->name = decl->name;
    self->kind = CST_MEMBER_CONSTANT;
    self->data.constant.decl = decl;
    return self;
}

struct cst_member*
cst_member_new_function(struct cst_decl const* decl)
{
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_FUNCTION);

    struct cst_member* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = decl->location;
    self->name = decl->name;
    self->kind = CST_MEMBER_FUNCTION;
    self->data.function.decl = decl;
    return self;
}

struct cst_member*
cst_member_new_alias(struct cst_decl const* decl)
{
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_ALIAS);

    struct cst_member* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = decl->location;
    self->name = decl->name;
    self->kind = CST_MEMBER_ALIAS;
    self->data.alias.decl = decl;
    return self;
}

struct cst_member_initializer*
cst_member_initializer_new(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_expr const* expr)
{
    struct cst_member_initializer* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->identifier = identifier;
    self->expr = expr;
    return self;
}

struct cst_enum_value*
cst_enum_value_new(
    struct source_location location,
    struct cst_identifier identifier,
    struct cst_expr const* expr)
{
    struct cst_enum_value* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->identifier = identifier;
    self->expr = expr;
    return self;
}

static struct cst_type*
cst_type_new(struct source_location location, enum cst_type_kind kind)
{
    struct cst_type* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->kind = kind;
    return self;
}

struct cst_type*
cst_type_new_symbol(struct cst_symbol const* symbol)
{
    assert(symbol != NULL);

    struct cst_type* const self =
        cst_type_new(symbol->location, CST_TYPE_SYMBOL);
    self->data.symbol = symbol;
    return self;
}

struct cst_type*
cst_type_new_function(
    struct source_location location,
    struct cst_type const* const* parameter_types,
    struct cst_type const* return_type)
{
    assert(return_type != NULL);

    struct cst_type* const self = cst_type_new(location, CST_TYPE_FUNCTION);
    self->data.function.parameter_types = parameter_types;
    self->data.function.return_type = return_type;
    return self;
}

struct cst_type*
cst_type_new_pointer(
    struct source_location location, struct cst_type const* base)
{
    assert(base != NULL);

    struct cst_type* const self = cst_type_new(location, CST_TYPE_POINTER);
    self->data.pointer.base = base;
    return self;
}

struct cst_type*
cst_type_new_array(
    struct source_location location,
    struct cst_expr const* count,
    struct cst_type const* base)
{
    assert(count != NULL);
    assert(base != NULL);

    struct cst_type* const self = cst_type_new(location, CST_TYPE_ARRAY);
    self->data.array.count = count;
    self->data.array.base = base;
    return self;
}

struct cst_type*
cst_type_new_slice(struct source_location location, struct cst_type const* base)
{
    assert(base != NULL);

    struct cst_type* const self = cst_type_new(location, CST_TYPE_SLICE);
    self->data.slice.base = base;
    return self;
}

struct cst_type*
cst_type_new_struct(
    struct source_location location, struct cst_member const* const* members)
{
    struct cst_type* const self = cst_type_new(location, CST_TYPE_STRUCT);
    self->data.struct_.members = members;
    return self;
}

struct cst_type*
cst_type_new_union(
    struct source_location location, struct cst_member const* const* members)
{
    struct cst_type* const self = cst_type_new(location, CST_TYPE_UNION);
    self->data.union_.members = members;
    return self;
}

struct cst_type*
cst_type_new_enum(
    struct source_location location,
    struct cst_type const* type,
    struct cst_enum_value const* const* values)
{
    struct cst_type* const self = cst_type_new(location, CST_TYPE_ENUM);
    self->data.enum_.type = type;
    self->data.enum_.values = values;
    return self;
}

struct cst_type*
cst_type_new_typeof(
    struct source_location location, struct cst_expr const* expr)
{
    assert(expr != NULL);

    struct cst_type* const self = cst_type_new(location, CST_TYPE_TYPEOF);
    self->data.typeof_.expr = expr;
    return self;
}
