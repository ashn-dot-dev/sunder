// Copyright 2021 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <string.h>
#include "sunder.h"

struct cst_module*
cst_module_new(
    struct cst_namespace const* namespace,
    struct cst_import const* const* imports,
    struct cst_decl const* const* decls)
{
    struct cst_module* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->namespace = namespace;
    self->imports = imports;
    self->decls = decls;
    return self;
}

struct cst_namespace*
cst_namespace_new(
    struct source_location const* location,
    struct cst_identifier const* const* identifiers)
{
    assert(location != NULL);

    struct cst_namespace* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->identifiers = identifiers;
    return self;
}

struct cst_import*
cst_import_new(struct source_location const* location, char const* path)
{
    assert(location != NULL);
    assert(path != NULL);

    struct cst_import* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->path = path;
    return self;
}

struct cst_decl*
cst_decl_new_variable(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec,
    struct cst_expr const* expr)
{
    assert(location != NULL);
    assert(identifier != NULL);
    assert(typespec != NULL);
    assert(expr != NULL);

    struct cst_decl* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_VARIABLE;
    self->location = location;
    self->name = identifier->name;
    self->data.variable.identifier = identifier;
    self->data.variable.typespec = typespec;
    self->data.variable.expr = expr;
    return self;
}

struct cst_decl*
cst_decl_new_constant(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec,
    struct cst_expr const* expr)
{
    assert(location != NULL);
    assert(identifier != NULL);
    assert(typespec != NULL);
    assert(expr != NULL);

    struct cst_decl* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_CONSTANT;
    self->location = location;
    self->name = identifier->name;
    self->data.constant.identifier = identifier;
    self->data.constant.typespec = typespec;
    self->data.constant.expr = expr;
    return self;
}

struct cst_decl*
cst_decl_new_function(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_template_parameter const* const* template_parameters,
    struct cst_function_parameter const* const* function_parameters,
    struct cst_typespec const* return_typespec,
    struct cst_block const* body)
{
    assert(location != NULL);
    assert(identifier != NULL);
    assert(return_typespec != NULL);
    assert(body != NULL);

    struct cst_decl* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_FUNCTION;
    self->location = location;
    self->name = identifier->name;
    self->data.function.identifier = identifier;
    self->data.function.template_parameters = template_parameters;
    self->data.function.function_parameters = function_parameters;
    self->data.function.return_typespec = return_typespec;
    self->data.function.body = body;
    return self;
}

struct cst_decl*
cst_decl_new_struct(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_template_parameter const* const* template_parameters,
    struct cst_member const* const* members)
{
    assert(location != NULL);
    assert(identifier != NULL);

    struct cst_decl* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_STRUCT;
    self->location = location;
    self->name = identifier->name;
    self->data.struct_.identifier = identifier;
    self->data.struct_.template_parameters = template_parameters;
    self->data.struct_.members = members;
    return self;
}

struct cst_decl*
cst_decl_new_extern_variable(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec)
{
    assert(location != NULL);
    assert(identifier != NULL);
    assert(typespec != NULL);

    struct cst_decl* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = CST_DECL_EXTERN_VARIABLE;
    self->location = location;
    self->name = identifier->name;
    self->data.extern_variable.identifier = identifier;
    self->data.extern_variable.typespec = typespec;
    return self;
}

static struct cst_stmt*
cst_stmt_new(struct source_location const* location, enum cst_stmt_kind kind)
{
    struct cst_stmt* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->kind = kind;
    return self;
}

struct cst_stmt*
cst_stmt_new_if(struct cst_conditional const* const* conditionals)
{
    assert(autil_sbuf_count(conditionals) > 0u);

    struct source_location const* const location = conditionals[0]->location;
    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_IF);
    self->data.if_.conditionals = conditionals;
    return self;
}

struct cst_stmt*
cst_stmt_new_for_range(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_expr const* begin,
    struct cst_expr const* end,
    struct cst_block const* body)
{
    assert(location != NULL);
    assert(identifier != NULL);
    assert(begin != NULL);
    assert(end != NULL);
    assert(body != NULL);

    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_FOR_RANGE);
    self->data.for_range.identifier = identifier;
    self->data.for_range.begin = begin;
    self->data.for_range.end = end;
    self->data.for_range.body = body;
    return self;
}

struct cst_stmt*
cst_stmt_new_for_expr(
    struct source_location const* location,
    struct cst_expr const* expr,
    struct cst_block const* body)
{
    assert(location != NULL);
    assert(expr != NULL);
    assert(body != NULL);

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
cst_stmt_new_break(struct source_location const* location)
{
    assert(location != NULL);

    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_BREAK);
    return self;
}

struct cst_stmt*
cst_stmt_new_continue(struct source_location const* location)
{
    assert(location != NULL);

    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_CONTINUE);
    return self;
}

struct cst_stmt*
cst_stmt_new_dump(
    struct source_location const* location, struct cst_expr const* expr)
{
    assert(location != NULL);
    assert(expr != NULL);

    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_DUMP);
    self->data.dump.expr = expr;
    return self;
}

struct cst_stmt*
cst_stmt_new_return(
    struct source_location const* location, struct cst_expr const* expr)
{
    assert(location != NULL);

    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_RETURN);
    self->data.return_.expr = expr;
    return self;
}

struct cst_stmt*
cst_stmt_new_assign(
    struct source_location const* location,
    struct cst_expr const* lhs,
    struct cst_expr const* rhs)
{
    assert(location != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    struct cst_stmt* const self = cst_stmt_new(location, CST_STMT_ASSIGN);
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
cst_expr_new(struct source_location const* location, enum cst_expr_kind kind)
{
    struct cst_expr* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->kind = kind;
    return self;
}

struct cst_expr*
cst_expr_new_identifier(struct cst_identifier const* identifier)
{
    assert(identifier != NULL);

    struct cst_expr* const self =
        cst_expr_new(identifier->location, CST_EXPR_IDENTIFIER);
    self->data.identifier = identifier;
    return self;
}

struct cst_expr*
cst_expr_new_qualified_identifier(
    struct cst_identifier const* const* identifiers)
{
    assert(autil_sbuf_count(identifiers) > 1);

    struct cst_expr* const self =
        cst_expr_new(identifiers[0]->location, CST_EXPR_QUALIFIED_IDENTIFIER);
    self->data.qualified_identifier.identifiers = identifiers;
    return self;
}

struct cst_expr*
cst_expr_new_template_instantiation(
    struct cst_identifier const* identifier,
    struct cst_template_argument const* const* arguments)
{
    assert(identifier != NULL);
    assert(autil_sbuf_count(arguments) > 0);

    struct cst_expr* const self =
        cst_expr_new(identifier->location, CST_EXPR_TEMPLATE_INSTANTIATION);
    self->data.template_instantiation.identifier = identifier;
    self->data.template_instantiation.arguments = arguments;
    return self;
}

struct cst_expr*
cst_expr_new_qualified_template_instantiation(
    struct cst_identifier const* const* identifiers,
    struct cst_template_argument const* const* arguments)
{
    assert(autil_sbuf_count(identifiers) > 1);
    assert(autil_sbuf_count(arguments) > 0);

    struct cst_expr* const self = cst_expr_new(
        identifiers[0]->location, CST_EXPR_QUALIFIED_TEMPLATE_INSTANTIATION);
    self->data.qualified_template_instantiation.identifiers = identifiers;
    self->data.qualified_template_instantiation.arguments = arguments;
    return self;
}

struct cst_expr*
cst_expr_new_boolean(struct cst_boolean const* boolean)
{
    assert(boolean != NULL);

    struct cst_expr* const self =
        cst_expr_new(boolean->location, CST_EXPR_BOOLEAN);
    self->data.boolean = boolean;
    return self;
}

struct cst_expr*
cst_expr_new_integer(struct cst_integer const* integer)
{
    assert(integer != NULL);

    struct cst_expr* const self =
        cst_expr_new(integer->location, CST_EXPR_INTEGER);
    self->data.integer = integer;
    return self;
}

struct cst_expr*
cst_expr_new_character(struct source_location const* location, int character)
{
    assert(location != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_CHARACTER);
    self->data.character = character;
    return self;
}

struct cst_expr*
cst_expr_new_bytes(
    struct source_location const* location, struct autil_string const* bytes)
{
    assert(location != NULL);
    assert(bytes != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_BYTES);
    self->data.bytes = bytes;
    return self;
}

struct cst_expr*
cst_expr_new_array(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_expr const* const* elements,
    struct cst_expr const* ellipsis)
{
    assert(location != NULL);
    assert(typespec != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_ARRAY);
    self->data.array.typespec = typespec;
    self->data.array.elements = elements;
    self->data.array.ellipsis = ellipsis;
    return self;
}

struct cst_expr*
cst_expr_new_slice(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_expr const* pointer,
    struct cst_expr const* count)
{
    assert(location != NULL);
    assert(typespec != NULL);
    assert(pointer != NULL);
    assert(count != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_SLICE);
    self->data.slice.typespec = typespec;
    self->data.slice.pointer = pointer;
    self->data.slice.count = count;
    return self;
}

struct cst_expr*
cst_expr_new_cast(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_expr const* expr)
{
    assert(location != NULL);
    assert(typespec != NULL);
    assert(expr != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_CAST);
    self->data.cast.typespec = typespec;
    self->data.cast.expr = expr;
    return self;
}

struct cst_expr*
cst_expr_new_struct(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_member_initializer const* const* initializers)
{
    assert(location != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_STRUCT);
    self->data.struct_.typespec = typespec;
    self->data.struct_.initializers = initializers;
    return self;
}

struct cst_expr*
cst_expr_new_grouped(
    struct source_location const* location, struct cst_expr const* expr)
{
    assert(expr != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_GROUPED);
    self->data.grouped.expr = expr;
    return self;
}

struct cst_expr*
cst_expr_new_syscall(
    struct source_location const* location,
    struct cst_expr const* const* arguments)
{
    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_SYSCALL);
    self->data.syscall.arguments = arguments;
    return self;
}

struct cst_expr*
cst_expr_new_call(
    struct source_location const* location,
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
    struct source_location const* location,
    struct cst_expr const* lhs,
    struct cst_expr const* idx)
{
    assert(location != NULL);
    assert(lhs != NULL);
    assert(idx != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_ACCESS_INDEX);
    self->data.access_index.lhs = lhs;
    self->data.access_index.idx = idx;
    return self;
}

struct cst_expr*
cst_expr_new_access_slice(
    struct source_location const* location,
    struct cst_expr const* lhs,
    struct cst_expr const* begin,
    struct cst_expr const* end)
{
    assert(location != NULL);
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
    struct source_location const* location,
    struct cst_expr const* lhs,
    struct cst_identifier const* identifier)
{
    assert(location != NULL);
    assert(lhs != NULL);
    assert(identifier != NULL);

    struct cst_expr* const self =
        cst_expr_new(location, CST_EXPR_ACCESS_MEMBER);
    self->data.access_member.lhs = lhs;
    self->data.access_member.identifier = identifier;
    return self;
}

struct cst_expr*
cst_expr_new_sizeof(
    struct source_location const* location, struct cst_typespec const* rhs)
{
    assert(location != NULL);
    assert(rhs != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_SIZEOF);
    self->data.sizeof_.rhs = rhs;
    return self;
}

struct cst_expr*
cst_expr_new_alignof(
    struct source_location const* location, struct cst_typespec const* rhs)
{
    assert(location != NULL);
    assert(rhs != NULL);

    struct cst_expr* const self = cst_expr_new(location, CST_EXPR_ALIGNOF);
    self->data.alignof_.rhs = rhs;
    return self;
}

struct cst_expr*
cst_expr_new_unary(struct token const* op, struct cst_expr const* rhs)
{
    assert(op != NULL);
    assert(rhs != NULL);

    struct cst_expr* const self = cst_expr_new(&op->location, CST_EXPR_UNARY);
    self->data.unary.op = op;
    self->data.unary.rhs = rhs;
    return self;
}

struct cst_expr*
cst_expr_new_binary(
    struct token const* op,
    struct cst_expr const* lhs,
    struct cst_expr const* rhs)
{
    assert(op != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    struct cst_expr* const self = cst_expr_new(&op->location, CST_EXPR_BINARY);
    self->data.binary.op = op;
    self->data.binary.lhs = lhs;
    self->data.binary.rhs = rhs;
    return self;
}

struct cst_conditional*
cst_conditional_new(
    struct source_location const* location,
    struct cst_expr const* condition,
    struct cst_block const* body)
{
    assert(location != NULL);
    assert(body != NULL);

    struct cst_conditional* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->condition = condition;
    self->body = body;
    return self;
}

struct cst_block*
cst_block_new(
    struct source_location const* location, struct cst_stmt const* const* stmts)
{
    assert(location != NULL);

    struct cst_block* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->stmts = stmts;
    return self;
}

struct cst_template_parameter*
cst_template_parameter_new(
    struct source_location const* location,
    struct cst_identifier const* identifier)
{
    assert(location != NULL);
    assert(identifier != NULL);

    struct cst_template_parameter* const self =
        autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->identifier = identifier;
    return self;
}

struct cst_template_argument*
cst_template_argument_new(
    struct source_location const* location, struct cst_typespec const* typespec)
{
    assert(location != NULL);
    assert(typespec != NULL);

    struct cst_template_argument* const self =
        autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->typespec = typespec;
    return self;
}

struct cst_function_parameter*
cst_function_parameter_new(
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec)
{
    assert(identifier != NULL);
    assert(typespec != NULL);

    struct cst_function_parameter* const self =
        autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = identifier->location;
    self->identifier = identifier;
    self->typespec = typespec;
    return self;
}

struct cst_member*
cst_member_new_variable(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec)
{
    assert(location != NULL);
    assert(identifier != NULL);
    assert(typespec != NULL);

    struct cst_member* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->name = identifier->name;
    self->kind = CST_MEMBER_VARIABLE;
    self->data.variable.identifier = identifier;
    self->data.variable.typespec = typespec;
    return self;
}

struct cst_member*
cst_member_new_function(struct cst_decl const* decl)
{
    assert(decl != NULL);
    assert(decl->kind == CST_DECL_FUNCTION);

    struct cst_member* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = decl->location;
    self->name = decl->name;
    self->kind = CST_MEMBER_FUNCTION;
    self->data.function.decl = decl;
    return self;
}

struct cst_member_initializer*
cst_member_initializer_new(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_expr const* expr)
{
    assert(location != NULL);
    assert(identifier != NULL);
    assert(expr != NULL);

    struct cst_member_initializer* const self =
        autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->identifier = identifier;
    self->expr = expr;
    return self;
}

static struct cst_typespec*
cst_typespec_new(
    struct source_location const* location, enum typespec_kind kind)
{
    assert(location != NULL);

    struct cst_typespec* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->kind = kind;
    return self;
}

struct cst_typespec*
cst_typespec_new_identifier(struct cst_identifier const* identifier)
{
    assert(identifier != NULL);

    struct cst_typespec* const self =
        cst_typespec_new(identifier->location, TYPESPEC_IDENTIFIER);
    self->data.identifier = identifier;
    return self;
}

struct cst_typespec*
cst_typespec_new_template_instantiation(
    struct cst_identifier const* identifier,
    struct cst_template_argument const* const* arguments)
{
    assert(identifier != NULL);
    assert(autil_sbuf_count(arguments) > 0);

    struct cst_typespec* const self =
        cst_typespec_new(identifier->location, TYPESPEC_TEMPLATE_INSTANTIATION);
    self->data.template_instantiation.identifier = identifier;
    self->data.template_instantiation.arguments = arguments;
    return self;
}

struct cst_typespec*
cst_typespec_new_function(
    struct source_location const* location,
    struct cst_typespec const* const* parameter_typespecs,
    struct cst_typespec const* return_typespec)
{
    assert(location != NULL);
    assert(return_typespec != NULL);

    struct cst_typespec* const self =
        cst_typespec_new(location, TYPESPEC_FUNCTION);
    self->data.function.parameter_typespecs = parameter_typespecs;
    self->data.function.return_typespec = return_typespec;
    return self;
}

struct cst_typespec*
cst_typespec_new_pointer(
    struct source_location const* location, struct cst_typespec const* base)
{
    assert(location != NULL);
    assert(base != NULL);

    struct cst_typespec* const self =
        cst_typespec_new(location, TYPESPEC_POINTER);
    self->data.pointer.base = base;
    return self;
}

struct cst_typespec*
cst_typespec_new_array(
    struct source_location const* location,
    struct cst_expr const* count,
    struct cst_typespec const* base)
{
    assert(location != NULL);
    assert(count != NULL);
    assert(base != NULL);

    struct cst_typespec* const self =
        cst_typespec_new(location, TYPESPEC_ARRAY);
    self->data.array.count = count;
    self->data.array.base = base;
    return self;
}

struct cst_typespec*
cst_typespec_new_slice(
    struct source_location const* location, struct cst_typespec const* base)
{
    assert(location != NULL);
    assert(base != NULL);

    struct cst_typespec* const self =
        cst_typespec_new(location, TYPESPEC_SLICE);
    self->data.slice.base = base;
    return self;
}

struct cst_typespec*
cst_typespec_new_typeof(
    struct source_location const* location, struct cst_expr const* expr)
{
    assert(location != NULL);
    assert(expr != NULL);

    struct cst_typespec* const self =
        cst_typespec_new(location, TYPESPEC_TYPEOF);
    self->data.typeof_.expr = expr;
    return self;
}

struct cst_identifier*
cst_identifier_new(struct source_location const* location, char const* name)
{
    assert(location != NULL);
    assert(name != NULL);

    struct cst_identifier* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->name = name;
    return self;
}

struct cst_boolean*
cst_boolean_new(struct source_location const* location, bool value)
{
    assert(location != NULL);

    struct cst_boolean* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->value = value;
    return self;
}

struct cst_integer*
cst_integer_new(
    struct source_location const* location,
    struct autil_bigint const* value,
    char const* suffix)
{
    assert(location != NULL);
    assert(suffix != NULL);

    struct cst_integer* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->location = location;
    self->value = value;
    self->suffix = suffix;
    return self;
}
