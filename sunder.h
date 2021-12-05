// Copyright 2021 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#ifndef SUNDER_H_INCLUDED
#define SUNDER_H_INCLUDED
#include <stdbool.h>
#include <stdint.h>
#include <autil/autil.h>

////////////////////////////////////////////////////////////////////////////////
//////// sunder.c //////////////////////////////////////////////////////////////
// Global compiler state and miscellaneous utilities.

#if __STDC_VERSION__ >= 201112L /* C11+ */
#    define NORETURN _Noreturn
#elif defined(__GNUC__) /* GCC and Clang */
#    define NORETURN __attribute__((noreturn))
#else
#    define NORETURN /* nothing */
#endif

// Returns a pointer to the first character of the line containing ptr in some
// NUL-terminated source string.
char const*
source_line_start(char const* ptr);
// Returns a pointer to the end-of-line newline or NUL of the line containing
// ptr in some NUL-terminated source string.
char const*
source_line_end(char const* ptr);

#define NO_PATH ((char const*)NULL)
#define NO_LINE ((size_t)0u)
#define NO_PSRC ((char const*)NULL)
#define NO_LOCATION ((struct source_location const*)NULL)
struct source_location {
    // Optional (NULL indicates no value).
    // NOTE: Source locations produced by the lexing phase will use a module's
    // `name` (i.e. non-canonical path) member for the source location path.
    char const* path;
    // Optional (zero indicates no value).
    size_t line;
    // Optional (NULL indicates no value) pointer to the source character within
    // the module specified by path. If non-NULL then a log-messages will
    // display the line in question with a caret pointing to this character as
    // such:
    // ```
    // [file.sunder:3] error: foo is not properly frobnicated
    // var foo: usize = 123u;
    //     ^
    // ```
    char const* psrc;
};

void
debug(struct source_location const* location, char const* fmt, ...);
void
error(struct source_location const* location, char const* fmt, ...);
NORETURN void
fatal(struct source_location const* location, char const* fmt, ...);

NORETURN void
todo(char const* file, int line, char const* fmt, ...);
#define TODO(...) todo(__FILE__, __LINE__, __VA_ARGS__)

NORETURN void
unreachable(char const* file, int line);
#define UNREACHABLE() unreachable(__FILE__, __LINE__)

// Round up to the nearest multiple of 8.
int
ceil8i(int x);
size_t
ceil8zu(size_t x);

// Convert a bigint to a uint8_t.
// Returns zero on success.
// Returns non-zero if the provided bigint is out-of-range, in which case *res
// is left unmodified.
int
bigint_to_u8(uint8_t* res, struct autil_bigint const* bigint);
// Convert a bigint to a size_t.
// Returns zero on success.
// Returns non-zero if the provided bigint is out-of-range, in which case *res
// is left unmodified.
int
bigint_to_uz(size_t* res, struct autil_bigint const* bigint);
// Convert a bigint to a uintmax_t.
// Returns zero on success.
// Returns non-zero if the provided bigint is out-of-range, in which case *res
// is left unmodified.
int
bigint_to_umax(uintmax_t* res, struct autil_bigint const* bigint);
// Convert a bigint into a two's complement bit array.
// Returns zero on success.
// Returns non-zero if the provided bigint is out-of-range would require more
// than autil_bitarr_count(res) bits to express, in which case *res is left
// unmodified.
int
bigint_to_bitarr(struct autil_bitarr* res, struct autil_bigint const* bigint);

// Convert a size_t to a bigint.
// The result bigint must be pre-initialized.
void
uz_to_bigint(struct autil_bigint* res, size_t uz);
// Convert a two's complement bit array into a bigint.
// The result bigint must be pre-initialized.
void
bitarr_to_bigint(
    struct autil_bigint* res,
    struct autil_bitarr const* bitarr,
    bool is_signed);

// Spawn a subprocess and wait for it to complete.
// Returns the exit status of the spawned process.
int
spawnvpw(char const* const* argv);
// Spawn a subprocess and wait for it to complete.
// Fatally exits if the exit status of the spawned process is non-zero.
void
xspawnvpw(char const* const* argv);

bool
file_exists(char const* path);

char const* // interned
canonical_path(char const* path);
char const* // interned
directory_path(char const* path);

struct module {
    // True if the module has been fully loaded/resolved.
    bool loaded;
    // The shorthand path of this module. For a module imported as:
    //      import "foo/bar.sunder";
    // this member will hold the string "foo/bar.sunder".
    char const* name; // interned
    // The canonical path of this module. For a module imported as:
    //      import "foo/bar.sunder";
    // this member will hold the string "/full/path/to/foo/bar.sunder".
    char const* path; // interned
    // NUL-prefixed, NUL-terminated text contents of the module.
    // When the module source is loaded a NUL-prefix is added to the beginning
    // of the source string at position source[-1] and source[source_count + 1]
    // so that either a forwards or backward search through the source text may
    // stop if a NUL byte is encountered.
    char const* source;
    size_t source_count;

    // Global symbols.
    struct symbol_table* symbols;
    // Exported symbols declared in this module.
    struct symbol_table* exports;

    // Concrete syntax tree for the module. Initialized to NULL and populated
    // during the parse phase.
    struct cst_module const* cst;
    // List of top level declarations topologically ordered such that
    // declaration with index k does not depend on any declaration with index
    // k+n for all n. Initialized to NULL and populated during the order phase.
    autil_sbuf(struct cst_decl const*) ordered;
};
struct module*
module_new(char const* name, char const* path);
void
module_del(struct module* self);

struct context {
    // Context-owned automatically managed objects.
    struct autil_freezer* freezer;

    // Interned strings.
    struct autil_sipool* sipool;
    struct {
        // clang-format off
        char const* empty;   // ""
        char const* builtin; // "builtin"
        char const* return_; // "return"
        char const* void_;   // "void"
        char const* bool_;   // "bool"
        char const* u8;      // "u8"
        char const* s8;      // "s8"
        char const* u16;     // "u16"
        char const* s16;     // "s16"
        char const* u32;     // "u32"
        char const* s32;     // "s32"
        char const* u64;     // "u64"
        char const* s64;     // "s64"
        char const* byte;    // "byte"
        char const* usize;   // "usize"
        char const* ssize;   // "ssize"
        char const* integer; // "integer"
        char const* y;       // "y"
        char const* u;       // "u"
        char const* s;       // "s"
        // clang-format on
    } interned;

    // Integer (bigint) constants.
    struct autil_bigint const* u8_min;
    struct autil_bigint const* u8_max;
    struct autil_bigint const* s8_min;
    struct autil_bigint const* s8_max;
    struct autil_bigint const* u16_min;
    struct autil_bigint const* u16_max;
    struct autil_bigint const* s16_min;
    struct autil_bigint const* s16_max;
    struct autil_bigint const* u32_min;
    struct autil_bigint const* u32_max;
    struct autil_bigint const* s32_min;
    struct autil_bigint const* s32_max;
    struct autil_bigint const* u64_min;
    struct autil_bigint const* u64_max;
    struct autil_bigint const* s64_min;
    struct autil_bigint const* s64_max;
    struct autil_bigint const* usize_min;
    struct autil_bigint const* usize_max;
    struct autil_bigint const* ssize_min;
    struct autil_bigint const* ssize_max;

    // Language builtins.
    struct {
        struct source_location location;
        struct type const* void_;
        struct type const* bool_;
        struct type const* byte;
        struct type const* u8;
        struct type const* s8;
        struct type const* u16;
        struct type const* s16;
        struct type const* u32;
        struct type const* s32;
        struct type const* u64;
        struct type const* s64;
        struct type const* usize;
        struct type const* ssize;
        struct type const* integer;
    } builtin;

    // Map containing all symbols with static storage duration.
#define CONTEXT_STATIC_SYMBOLS_MAP_KEY_TYPE char const*
#define CONTEXT_STATIC_SYMBOLS_MAP_VAL_TYPE struct symbol const*
#define CONTEXT_STATIC_SYMBOLS_MAP_CMP_FUNC autil_cstr_vpcmp
    struct autil_map* static_symbols;

    // Global symbol table.
    struct symbol_table* global_symbol_table;

    // Currently loaded/loading modules.
    // TODO: Maybe make this a map from realpath to module?
    autil_sbuf(struct module*) modules;
};
void
context_init(void);
void
context_fini(void);
struct context const*
context(void);

struct module const*
load_module(char const* name, char const* path);
struct module const*
lookup_module(char const* path);

////////////////////////////////////////////////////////////////////////////////
//////// lex.c /////////////////////////////////////////////////////////////////

enum token_kind {
    // Keywords
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NOT,
    TOKEN_OR,
    TOKEN_AND,
    TOKEN_NAMESPACE,
    TOKEN_IMPORT,
    TOKEN_VAR,
    TOKEN_CONST,
    TOKEN_FUNC,
    TOKEN_EXTERN,
    TOKEN_DUMP,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_SYSCALL,
    TOKEN_ALIGNOF,
    TOKEN_COUNTOF,
    TOKEN_SIZEOF,
    TOKEN_TYPEOF,
    // Sigils
    TOKEN_EQ, // ==
    TOKEN_NE, // !=
    TOKEN_LE, // <=
    TOKEN_LT, // <
    TOKEN_GE, // >=
    TOKEN_GT, // >
    TOKEN_ASSIGN, // =
    TOKEN_PLUS, // +
    TOKEN_DASH, // -
    TOKEN_STAR, // *
    TOKEN_FSLASH, // /
    TOKEN_TILDE, // ~
    TOKEN_PIPE, // |
    TOKEN_CARET, // ^
    TOKEN_AMPERSAND, // &
    TOKEN_LPAREN, // (
    TOKEN_RPAREN, // )
    TOKEN_LBRACE, // {
    TOKEN_RBRACE, // }
    TOKEN_LBRACKET, // [
    TOKEN_RBRACKET, // ]
    TOKEN_COMMA, // ,
    TOKEN_ELLIPSIS, // ...
    TOKEN_DOT, // .
    TOKEN_COLON_COLON, // :
    TOKEN_COLON, // :
    TOKEN_SEMICOLON, // ;
    // Identifiers and Non-Keyword Literals
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
    TOKEN_BYTES,
    // Meta
    TOKEN_EOF,
};
char const*
token_kind_to_cstr(enum token_kind kind);

struct token {
    char const* start;
    size_t count;
    struct source_location location;

    enum token_kind kind;
    union {
        // TOKEN_INTEGER
        struct {
            struct autil_vstr number;
            struct autil_vstr suffix;
        } integer;
        // TOKEN_BYTES
        // Contains the unescaped bytes of literal.
        struct autil_string const* bytes;
    } data;
};
char*
token_to_new_cstr(struct token const* token);

struct lexer*
lexer_new(struct module* module);
void
lexer_del(struct lexer* self);
struct token const*
lexer_next_token(struct lexer* self);

////////////////////////////////////////////////////////////////////////////////
//////// cst.c /////////////////////////////////////////////////////////////////
// Concrete syntax tree.

struct cst_module {
    struct cst_namespace const* namespace; // optional
    autil_sbuf(struct cst_import const* const) imports;
    autil_sbuf(struct cst_decl const* const) decls;
};
struct cst_module*
cst_module_new(
    struct cst_namespace const* namespace,
    struct cst_import const* const* imports,
    struct cst_decl const* const* decls);

struct cst_namespace {
    struct source_location const* location;
    autil_sbuf(struct cst_identifier const* const) identifiers;
};
struct cst_namespace*
cst_namespace_new(
    struct source_location const* location,
    struct cst_identifier const* const* identifiers);

struct cst_import {
    struct source_location const* location;
    char const* path; // interned
};
struct cst_import*
cst_import_new(struct source_location const* location, char const* path);

struct cst_decl {
    struct source_location const* location;
    char const* name; // interned (from the identifier)
    enum {
        CST_DECL_VARIABLE,
        CST_DECL_CONSTANT,
        CST_DECL_FUNCTION,
        CST_DECL_EXTERN_VARIABLE,
    } kind;
    union {
        struct {
            struct cst_identifier const* identifier;
            struct cst_typespec const* typespec;
            struct cst_expr const* expr;
        } variable;
        struct {
            struct cst_identifier const* identifier;
            struct cst_typespec const* typespec;
            struct cst_expr const* expr;
        } constant;
        struct {
            struct cst_identifier const* identifier;
            autil_sbuf(struct cst_parameter const* const) parameters;
            struct cst_typespec const* return_typespec;
            struct cst_block const* body;
        } function;
        struct {
            struct cst_identifier const* identifier;
            struct cst_typespec const* typespec;
        } extern_variable;
    } data;
};
struct cst_decl*
cst_decl_new_variable(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec,
    struct cst_expr const* expr);
struct cst_decl*
cst_decl_new_constant(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec,
    struct cst_expr const* expr);
struct cst_decl*
cst_decl_new_func(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_parameter const* const* paramseters,
    struct cst_typespec const* return_typespec,
    struct cst_block const* body);
struct cst_decl*
cst_decl_new_extern_variable(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec);

struct cst_stmt {
    struct source_location const* location;
    enum cst_stmt_kind {
        CST_STMT_DECL,
        CST_STMT_IF,
        CST_STMT_FOR_RANGE,
        CST_STMT_FOR_EXPR,
        CST_STMT_BREAK, /* no .data member */
        CST_STMT_CONTINUE, /* no .data member */
        CST_STMT_DUMP,
        CST_STMT_RETURN,
        CST_STMT_ASSIGN,
        CST_STMT_EXPR,
    } kind;
    union {
        struct cst_decl const* decl;
        struct {
            autil_sbuf(struct cst_conditional const* const) conditionals;
        } if_;
        struct {
            struct cst_identifier const* identifier;
            struct cst_expr const* begin;
            struct cst_expr const* end;
            struct cst_block const* body;
        } for_range;
        struct {
            struct cst_expr const* expr;
            struct cst_block const* body;
        } for_expr;
        struct {
            struct cst_expr const* expr;
        } dump;
        struct {
            struct cst_expr const* expr; // optional
        } return_;
        struct {
            struct cst_expr const* lhs;
            struct cst_expr const* rhs;
        } assign;
        struct cst_expr const* expr;
    } data;
};
struct cst_stmt*
cst_stmt_new_decl(struct cst_decl const* decl);
struct cst_stmt*
cst_stmt_new_if(struct cst_conditional const* const* conditionals);
struct cst_stmt*
cst_stmt_new_for_range(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_expr const* begin,
    struct cst_expr const* end,
    struct cst_block const* body);
struct cst_stmt*
cst_stmt_new_for_expr(
    struct source_location const* location,
    struct cst_expr const* expr,
    struct cst_block const* body);
struct cst_stmt*
cst_stmt_new_break(struct source_location const* location);
struct cst_stmt*
cst_stmt_new_continue(struct source_location const* location);
struct cst_stmt*
cst_stmt_new_dump(
    struct source_location const* location, struct cst_expr const* expr);
struct cst_stmt*
cst_stmt_new_return(
    struct source_location const* location, struct cst_expr const* expr);
struct cst_stmt*
cst_stmt_new_assign(
    struct source_location const* location,
    struct cst_expr const* lhs,
    struct cst_expr const* rhs);
struct cst_stmt*
cst_stmt_new_expr(struct cst_expr const* expr);

struct cst_expr {
    struct source_location const* location;
    enum cst_expr_kind {
        // Primary Expressions
        CST_EXPR_IDENTIFIER,
        CST_EXPR_QUALIFIED_IDENTIFIER,
        CST_EXPR_BOOLEAN,
        CST_EXPR_INTEGER,
        CST_EXPR_BYTES,
        CST_EXPR_ARRAY,
        CST_EXPR_SLICE,
        CST_EXPR_CAST,
        CST_EXPR_GROUPED,
        // Postfix Expressions
        CST_EXPR_SYSCALL,
        CST_EXPR_CALL,
        CST_EXPR_ACCESS_INDEX,
        CST_EXPR_ACCESS_SLICE,
        // Prefix Unary Operator Expressions
        CST_EXPR_SIZEOF,
        CST_EXPR_ALIGNOF,
        CST_EXPR_UNARY,
        // Infix Binary Operator Expressions
        CST_EXPR_BINARY,
    } kind;
    union cst_expr_data {
        struct cst_identifier const* identifier;
        struct {
            autil_sbuf(struct cst_identifier const* const) identifiers;
        } qualified_identifier;
        struct cst_boolean const* boolean;
        struct cst_integer const* integer;
        struct autil_string const* bytes;
        struct {
            struct cst_typespec const* typespec;
            autil_sbuf(struct cst_expr const* const) elements;
            struct cst_expr const* ellipsis; // optional
        } array;
        struct {
            struct cst_typespec const* typespec;
            struct cst_expr const* pointer;
            struct cst_expr const* count;
        } slice;
        struct {
            struct cst_typespec const* typespec;
            struct cst_expr const* expr;
        } cast;
        struct {
            struct cst_expr const* expr;
        } grouped;
        struct cst_stmt_syscall {
            autil_sbuf(struct cst_expr const* const) arguments;
        } syscall;
        struct {
            struct cst_expr const* func;
            autil_sbuf(struct cst_expr const* const) arguments;
        } call;
        struct {
            struct cst_expr const* lhs;
            struct cst_expr const* idx;
        } access_index;
        struct {
            struct cst_expr const* lhs;
            struct cst_expr const* begin;
            struct cst_expr const* end;
        } access_slice;
        struct {
            struct cst_typespec const* rhs;
        } sizeof_;
        struct {
            struct cst_typespec const* rhs;
        } alignof_;
        struct {
            struct token const* op;
            struct cst_expr const* rhs;
        } unary;
        struct {
            struct token const* op;
            struct cst_expr const* lhs;
            struct cst_expr const* rhs;
        } binary;
    } data;
};
struct cst_expr*
cst_expr_new_identifier(struct cst_identifier const* identifier);
struct cst_expr*
cst_expr_new_qualified_identifier(
    struct cst_identifier const* const* identifiers);
struct cst_expr*
cst_expr_new_boolean(struct cst_boolean const* boolean);
struct cst_expr*
cst_expr_new_integer(struct cst_integer const* integer);
struct cst_expr*
cst_expr_new_bytes(
    struct source_location const* location, struct autil_string const* bytes);
struct cst_expr*
cst_expr_new_array(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_expr const* const* elements,
    struct cst_expr const* ellipsis);
struct cst_expr*
cst_expr_new_slice(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_expr const* pointer,
    struct cst_expr const* count);
struct cst_expr*
cst_expr_new_cast(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_expr const* expr);
struct cst_expr*
cst_expr_new_grouped(
    struct source_location const* location, struct cst_expr const* expr);
struct cst_expr*
cst_expr_new_syscall(
    struct source_location const* location,
    struct cst_expr const* const* arguments);
struct cst_expr*
cst_expr_new_call(
    struct cst_expr const* func, struct cst_expr const* const* arguments);
struct cst_expr*
cst_expr_new_access_index(
    struct source_location const* location,
    struct cst_expr const* lhs,
    struct cst_expr const* idx);
struct cst_expr*
cst_expr_new_access_slice(
    struct source_location const* location,
    struct cst_expr const* lhs,
    struct cst_expr const* begin,
    struct cst_expr const* end);
struct cst_expr*
cst_expr_new_sizeof(
    struct source_location const* location, struct cst_typespec const* rhs);
struct cst_expr*
cst_expr_new_alignof(
    struct source_location const* location, struct cst_typespec const* rhs);
struct cst_expr*
cst_expr_new_unary(struct token const* op, struct cst_expr const* rhs);
struct cst_expr*
cst_expr_new_binary(
    struct token const* op,
    struct cst_expr const* lhs,
    struct cst_expr const* rhs);

// Helper CST node that denotes a conditional expression (if, elif, etc.)
// consisting of a conditional expression and body.
struct cst_conditional {
    struct source_location const* location;
    struct cst_expr const* condition; // optional (NULL => else)
    struct cst_block const* body;
};
struct cst_conditional*
cst_conditional_new(
    struct source_location const* location,
    struct cst_expr const* condition,
    struct cst_block const* body);

struct cst_block {
    struct source_location const* location;
    autil_sbuf(struct cst_stmt const* const) stmts;
};
struct cst_block*
cst_block_new(
    struct source_location const* location,
    struct cst_stmt const* const* stmts);

struct cst_parameter {
    struct source_location const* location;
    struct cst_identifier const* identifier;
    struct cst_typespec const* typespec;
};
struct cst_parameter*
cst_parameter_new(
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec);

// ISO/IEC 9899:1999 Section 6.7.2 - Type Specifiers
struct cst_typespec {
    struct source_location const* location;
    enum typespec_kind {
        TYPESPEC_IDENTIFIER,
        TYPESPEC_FUNCTION,
        TYPESPEC_POINTER,
        TYPESPEC_ARRAY,
        TYPESPEC_SLICE,
        TYPESPEC_TYPEOF
    } kind;
    union {
        struct cst_identifier const* identifier;
        struct {
            autil_sbuf(struct cst_typespec const* const) parameter_typespecs;
            struct cst_typespec const* return_typespec;
        } function;
        struct {
            struct cst_typespec const* base;
        } pointer;
        struct {
            struct cst_expr const* count;
            struct cst_typespec const* base;
        } array;
        struct {
            struct cst_typespec const* base;
        } slice;
        struct {
            struct cst_expr const* expr;
        } typeof_;
    } data;
};
struct cst_typespec*
cst_typespec_new_identifier(struct cst_identifier const* identifier);
struct cst_typespec*
cst_typespec_new_function(
    struct source_location const* location,
    struct cst_typespec const* const* parameter_typespecs,
    struct cst_typespec const* return_typespec);
struct cst_typespec*
cst_typespec_new_pointer(
    struct source_location const* location, struct cst_typespec const* base);
struct cst_typespec*
cst_typespec_new_array(
    struct source_location const* location,
    struct cst_expr const* count,
    struct cst_typespec const* base);
struct cst_typespec*
cst_typespec_new_slice(
    struct source_location const* location, struct cst_typespec const* base);
struct cst_typespec*
cst_typespec_new_typeof(
    struct source_location const* location, struct cst_expr const* expr);

struct cst_identifier {
    struct source_location const* location;
    char const* name; // interned
};
struct cst_identifier*
cst_identifier_new(struct source_location const* location, char const* name);

struct cst_boolean {
    struct source_location const* location;
    bool value;
};
struct cst_boolean*
cst_boolean_new(struct source_location const* location, bool value);

struct cst_integer {
    struct source_location const* location;
    struct autil_bigint const* value;
    char const* suffix; // interned
};
struct cst_integer*
cst_integer_new(
    struct source_location const* location,
    struct autil_bigint const* value,
    char const* suffix);

////////////////////////////////////////////////////////////////////////////////
//////// parse.c ///////////////////////////////////////////////////////////////

void
parse(struct module* module);

////////////////////////////////////////////////////////////////////////////////
//////// order.c ///////////////////////////////////////////////////////////////

void
order(struct module* module);

////////////////////////////////////////////////////////////////////////////////
//////// tir.c /////////////////////////////////////////////////////////////////
// Tree-based intermediate representation.

// SIZEOF_UNSIZED and ALIGNOF_UNSIZED are given the largest possible value of a
// size_t so that checks such as assert(type->size <= 8u) in the resolve and
// code generation phases will fail for unsized types.
#define SIZEOF_UNSIZED ((size_t)SIZE_MAX)
#define ALIGNOF_UNSIZED ((size_t)SIZE_MAX)
struct type {
    char const* name; // Canonical human-readable type-name (interned)
    size_t size; // sizeof
    size_t align; // alignof

    enum type_kind {
        TYPE_VOID,
        TYPE_BOOL,
        TYPE_BYTE,
        TYPE_U8, /* integer */
        TYPE_S8, /* integer */
        TYPE_U16, /* integer */
        TYPE_S16, /* integer */
        TYPE_U32, /* integer */
        TYPE_S32, /* integer */
        TYPE_U64, /* integer */
        TYPE_S64, /* integer */
        TYPE_USIZE, /* integer */
        TYPE_SSIZE, /* integer */
        TYPE_UNSIZED_INTEGER, /* integer, untyped */
        TYPE_FUNCTION,
        TYPE_POINTER,
        TYPE_ARRAY,
        TYPE_SLICE,
    } kind;
    union {
        struct {
            // NOTE: The min and max fields are "optional" in the sense that
            // they are not necessarily defined for all types satisfying the
            // type_is_integer function. The type kind TYPE_UNSIZED_INTEGER
            // will have these as NULL as integers of this type have no defined
            // size.
            struct autil_bigint const* min; // optional
            struct autil_bigint const* max; // optional
        } integer;
        struct {
            autil_sbuf(struct type const* const) parameter_types;
            struct type const* return_type;
        } function;
        struct {
            struct type const* base;
        } pointer;
        struct {
            size_t count;
            struct type const* base;
        } array;
        struct {
            struct type const* base;
        } slice;
    } data;
};
struct type*
type_new_void(void);
struct type*
type_new_bool(void);
struct type*
type_new_byte(void);
struct type*
type_new_u8(void);
struct type*
type_new_s8(void);
struct type*
type_new_u16(void);
struct type*
type_new_s16(void);
struct type*
type_new_u32(void);
struct type*
type_new_s32(void);
struct type*
type_new_u64(void);
struct type*
type_new_s64(void);
struct type*
type_new_usize(void);
struct type*
type_new_ssize(void);
struct type*
type_new_integer(void);
struct type*
type_new_function(
    struct type const* const* parameter_types, struct type const* return_type);
struct type*
type_new_pointer(struct type const* base);
struct type*
type_new_array(size_t count, struct type const* base);
struct type*
type_new_slice(struct type const* base);

struct type const*
type_unique_function(
    struct type const* const* parameter_types, struct type const* return_type);
struct type const*
type_unique_pointer(struct type const* base);
struct type const*
type_unique_array(size_t count, struct type const* base);
struct type const*
type_unique_slice(struct type const* base);

bool
type_is_integer(struct type const* self);
bool
type_is_uinteger(struct type const* self);
bool
type_is_sinteger(struct type const* self);
// Returns true if the type may be compared with the == or != operators.
bool
type_can_compare_equality(struct type const* self);
// Returns true if the type may be compared with the ==, !=, <, <=, >, and >=
// operators.
bool
type_can_compare_order(struct type const* self);

struct address {
    enum address_kind {
        ADDRESS_STATIC,
        ADDRESS_LOCAL,
    } kind;
    union {
        struct {
            // Full normalized name, including nested namespace information,
            // uniquely identifying the base region of the static storage
            // location in which this address resides.
            char const* name; // interned
            // Offset (in bytes) from the base region.
            size_t offset;
        } static_;
        struct {
            int rbp_offset;
        } local;
    } data;
};
struct address
address_init_static(char const* name, size_t offset);
struct address
address_init_local(int rbp_offset);
struct address*
address_new(struct address from);

struct symbol {
    enum symbol_kind {
        SYMBOL_TYPE,
        SYMBOL_VARIABLE,
        SYMBOL_CONSTANT,
        SYMBOL_FUNCTION,
        SYMBOL_NAMESPACE,
    } kind;
    struct source_location const* location;
    char const* name; // interned
    bool is_extern;
    // SYMBOL_TYPE      => The type itself.
    // SYMBOL_VARIABLE  => The type of the variable.
    // SYMBOL_CONSTANT  => The type of the constant.
    // SYMBOL_FUNCTION  => The type of the function (always TYPE_FUNCTION).
    // SYMBOL_NAMESPACE => NULL.
    struct type const* type;
    // SYMBOL_TYPE      => NULL.
    // SYMBOL_VARIABLE  => ADDRESS_STATIC or ADDRESS_LOCAL.
    // SYMBOL_CONSTANT  => ADDRESS_STATIC or ADDRESS_LOCAL.
    // SYMBOL_FUNCTION  => ADDRESS_STATIC.
    // SYMBOL_NAMESPACE => NULL.
    struct address const* address;
    // SYMBOL_TYPE      => NULL.
    // SYMBOL_VARIABLE  => Compile-type-value of the variable
    //                     (non-extern globals only).
    // SYMBOL_CONSTANT  => Compile-time value of the constant.
    // SYMBOL_FUNCTION  => Compile-time value of the function.
    // SYMBOL_NAMESPACE => NULL.
    struct value const* value;
    // SYMBOL_TYPE      => NULL.
    // SYMBOL_VARIABLE  => NULL.
    // SYMBOL_CONSTANT  => NULL.
    // SYMBOL_FUNCTION  => NULL.
    // SYMBOL_NAMESPACE => Symbols under the namespace.
    struct symbol_table* symbols;
};
struct symbol*
symbol_new_type(
    struct source_location const* location, struct type const* type);
struct symbol*
symbol_new_variable(
    struct source_location const* location,
    char const* name,
    struct type const* type,
    struct address const* address,
    struct value const* value);
struct symbol*
symbol_new_constant(
    struct source_location const* location,
    char const* name,
    struct type const* type,
    struct address const* address,
    struct value const* value);
struct symbol*
symbol_new_function(
    struct source_location const* location,
    char const* name,
    struct type const* type,
    struct address const* address,
    struct value const* value);
struct symbol*
symbol_new_namespace(
    struct source_location const* location,
    char const* name,
    struct symbol_table* symbols);

struct symbol_table {
    struct symbol_table const* parent; // optional (NULL => global scope)
    // Mapping from cstring to symbol. The cstring key corresponding to the
    // key-value pair (cstring, symbol) is not necessarily equal to the `name`
    // member of the symbol, such as in the case of the symbol with name
    // "foo.bar" with the mapping ("foo", symbol "foo.bar") in the namespace
    // symbol table of `foo`.
#define SYMBOL_MAP_KEY_TYPE char const*
#define SYMBOL_MAP_VAL_TYPE struct symbol const*
#define SYMBOL_MAP_CMP_FUNC autil_cstr_vpcmp
    struct autil_map* symbols;
};
struct symbol_table*
symbol_table_new(struct symbol_table const* parent);
void
symbol_table_freeze(struct symbol_table* self, struct autil_freezer* freezer);
void
symbol_table_insert(
    struct symbol_table* self, char const* name, struct symbol const* symbol);
// Lookup in this or any parent symbol table.
struct symbol const*
symbol_table_lookup(struct symbol_table const* self, char const* name);
// Lookup in this symbol table only.
struct symbol const*
symbol_table_lookup_local(struct symbol_table const* self, char const* name);

struct stmt {
    struct source_location const* location;
    enum stmt_kind {
        STMT_IF,
        STMT_FOR_RANGE,
        STMT_FOR_EXPR,
        STMT_BREAK, /* no .data member */
        STMT_CONTINUE, /* no .data member */
        STMT_DUMP,
        STMT_RETURN,
        STMT_ASSIGN,
        STMT_EXPR,
    } kind;
    union {
        struct {
            autil_sbuf(struct conditional const* const) conditionals;
        } if_;
        struct {
            struct symbol const* loop_variable;
            struct expr const* begin;
            struct expr const* end;
            struct block const* body;
        } for_range;
        struct {
            struct expr const* expr;
            struct block const* body;
        } for_expr;
        struct {
            struct expr const* expr;
        } dump;
        struct {
            struct expr const* expr; // optional
        } return_;
        struct {
            struct expr const* lhs;
            struct expr const* rhs;
        } assign;
        struct expr const* expr;
    } data;
};
struct stmt*
stmt_new_if(struct conditional const* const* conditionals);
struct stmt*
stmt_new_for_range(
    struct source_location const* location,
    struct symbol const* loop_variable,
    struct expr const* begin,
    struct expr const* end,
    struct block const* body);
struct stmt*
stmt_new_for_expr(
    struct source_location const* location,
    struct expr const* expr,
    struct block const* body);
struct stmt*
stmt_new_break(struct source_location const* location);
struct stmt*
stmt_new_continue(struct source_location const* location);
struct stmt*
stmt_new_dump(struct source_location const* location, struct expr const* expr);
struct stmt*
stmt_new_return(
    struct source_location const* location, struct expr const* expr);
struct stmt*
stmt_new_assign(
    struct source_location const* location,
    struct expr const* lhs,
    struct expr const* rhs);
struct stmt*
stmt_new_expr(struct source_location const* location, struct expr const* expr);

// Minimum and maximum number of syscall arguments (including the syscall
// number) passed to a syscall expression. This is based on the Linux syscall
// convention which allows for a maximum of six parameters plus the syscall
// number to be passed via registers.
#define SYSCALL_ARGUMENTS_MIN ((size_t)1)
#define SYSCALL_ARGUMENTS_MAX ((size_t)7)
struct expr {
    struct source_location const* location;
    struct type const* type;
    enum expr_kind {
        EXPR_IDENTIFIER,
        EXPR_BOOLEAN,
        EXPR_INTEGER,
        EXPR_BYTES,
        EXPR_ARRAY,
        EXPR_SLICE,
        EXPR_CAST,
        EXPR_SYSCALL,
        EXPR_CALL,
        EXPR_ACCESS_INDEX,
        EXPR_ACCESS_SLICE,
        EXPR_SIZEOF,
        EXPR_ALIGNOF,
        EXPR_UNARY,
        EXPR_BINARY,
    } kind;
    union {
        struct symbol const* identifier;
        bool boolean;
        struct autil_bigint const* integer;
        struct {
            struct address const* address;
            size_t count;
        } bytes;
        struct {
            autil_sbuf(struct expr const* const) elements;
            struct expr const* ellipsis; // optional
        } array;
        struct {
            struct expr const* pointer;
            struct expr const* count;
        } slice;
        struct {
            struct expr const* expr;
        } cast;
        struct {
            autil_sbuf(struct expr const* const) arguments;
        } syscall;
        struct {
            // Expression resulting in a callable function.
            struct expr const* function;
            // Arguments to the callable function.
            autil_sbuf(struct expr const* const) arguments;
        } call;
        struct {
            struct expr const* lhs;
            struct expr const* idx;
        } access_index;
        struct {
            struct expr const* lhs;
            struct expr const* begin;
            struct expr const* end;
        } access_slice;
        struct {
            struct type const* rhs;
        } sizeof_;
        struct {
            struct type const* rhs;
        } alignof_;
        struct {
            enum uop_kind {
                UOP_NOT,
                UOP_POS,
                UOP_NEG,
                UOP_BITNOT,
                UOP_DEREFERENCE,
                UOP_ADDRESSOF,
                UOP_COUNTOF,
            } op;
            struct expr const* rhs;
        } unary;
        struct {
            enum bop_kind {
                BOP_OR,
                BOP_AND,
                BOP_EQ,
                BOP_NE,
                BOP_LE,
                BOP_LT,
                BOP_GE,
                BOP_GT,
                BOP_ADD,
                BOP_SUB,
                BOP_MUL,
                BOP_DIV,
                BOP_BITOR,
                BOP_BITXOR,
                BOP_BITAND,
            } op;
            struct expr const* lhs;
            struct expr const* rhs;
        } binary;
    } data;
};
struct expr*
expr_new_identifier(
    struct source_location const* location, struct symbol const* identifier);
struct expr*
expr_new_boolean(struct source_location const* location, bool value);
struct expr*
expr_new_integer(
    struct source_location const* location,
    struct type const* type,
    struct autil_bigint const* value);
struct expr*
expr_new_bytes(
    struct source_location const* location,
    struct address const* address,
    size_t count);
struct expr*
expr_new_array(
    struct source_location const* location,
    struct type const* type,
    struct expr const* const* elements,
    struct expr const* ellipsis);
struct expr*
expr_new_slice(
    struct source_location const* location,
    struct type const* type,
    struct expr const* pointer,
    struct expr const* count);
struct expr*
expr_new_cast(
    struct source_location const* location,
    struct type const* type,
    struct expr const* expr);
struct expr*
expr_new_syscall(
    struct source_location const* location,
    struct expr const* const* arguments);
struct expr*
expr_new_call(
    struct source_location const* location,
    struct expr const* function,
    struct expr const* const* arguments);
struct expr*
expr_new_access_index(
    struct source_location const* location,
    struct expr const* lhs,
    struct expr const* idx);
struct expr*
expr_new_access_slice(
    struct source_location const* location,
    struct expr const* lhs,
    struct expr const* begin,
    struct expr const* end);
struct expr*
expr_new_sizeof(struct source_location const* location, struct type const* rhs);
struct expr*
expr_new_alignof(struct source_location const* location, struct type const* rhs);
struct expr*
expr_new_unary(
    struct source_location const* location,
    struct type const* type,
    enum uop_kind op,
    struct expr const* rhs);
struct expr*
expr_new_binary(
    struct source_location const* location,
    struct type const* type,
    enum bop_kind op,
    struct expr const* lhs,
    struct expr const* rhs);
// ISO/IEC 9899:1999 Section 6.3.2.1
// https://en.cppreference.com/w/c/language/value_category
bool
expr_is_lvalue(struct expr const* self);

struct function {
    char const* name; // interned
    struct type const* type; // TYPE_FUNCTION
    struct address const* address; // ADDRESS_STATIC

    // Outermost symbol table containing symbols for function parameters, local
    // variables, and local constants in the outermost scope (i.e. body) of the
    // function. Initialized to NULL on struct creation.
    struct symbol_table const* symbol_table;
    // Initialized to NULL on struct creation.
    autil_sbuf(struct symbol const* const) symbol_parameters;
    // Initialized to NULL on struct creation.
    struct symbol const* symbol_return;
    // Initialized to NULL on struct creation.
    struct block const* body;

    // Offset required to store all local variables in this function.
    // When the function is entered the stack pointer will be offset by this
    // amount before any expressions are pushed/popped to/from the stack during
    // intermediate calculations.
    int local_stack_offset;
};
// Creates a new incomplete (empty) function.
// The type of the function must be of kind TYPE_FUNCTION.
// The address of the function must be of kind ADDRESS_STATIC.
struct function*
function_new(
    char const* name, struct type const* type, struct address const* address);
void
function_del(struct function* self);

struct conditional {
    struct source_location const* location;
    struct expr const* condition; // optional (NULL => else)
    struct block const* body;
};
struct conditional*
conditional_new(
    struct source_location const* location,
    struct expr const* condition,
    struct block const* body);

struct block {
    struct source_location const* location;
    struct symbol_table* symbol_table; // not owned
    autil_sbuf(struct stmt const* const) stmts;
};
struct block*
block_new(
    struct source_location const* location,
    struct symbol_table* symbol_table,
    struct stmt const* const* stmts);

struct value {
    struct type const* type;
    struct {
        bool boolean;
        uint8_t byte;
        struct autil_bigint* integer;
        struct function const* function;
        struct address pointer;
        struct {
            // Concrete values specified for elements of the array value before
            // the optional ellipsis element. The autil_sbuf_count of the
            // elements member may be less than countof(array), in which case
            // the ellipsis value represents the rest of the elements upto
            // the countof(array)th element.
            autil_sbuf(struct value*) elements;
            // Value representing elements from indices within the half-open
            // range [autil_sbuf_count(elements), countof(array)) that are
            // initialized via an ellipsis element. NULL if no ellipsis element
            // was specified in the parse tree for the array value.
            struct value* ellipsis; // optional
        } array;
        struct {
            struct value* pointer; // TYPE_POINTER
            struct value* count; // TYPE_USIZE
        } slice;
    } data;
};
struct value*
value_new_boolean(bool boolean);
struct value*
value_new_byte(uint8_t byte);
struct value*
value_new_integer(struct type const* type, struct autil_bigint* integer);
struct value*
value_new_function(struct function const* function);
struct value*
value_new_pointer(struct type const* type, struct address address);
struct value*
value_new_array(
    struct type const* type, struct value** elements, struct value* ellipsis);
struct value*
value_new_slice(
    struct type const* type, struct value* pointer, struct value* count);
void
value_del(struct value* self);
void
value_freeze(struct value* self, struct autil_freezer* freezer);
struct value*
value_clone(struct value const* self);

bool
value_eq(struct value const* lhs, struct value const* rhs);
bool
value_lt(struct value const* lhs, struct value const* rhs);
bool
value_gt(struct value const* lhs, struct value const* rhs);

uint8_t* // sbuf
value_to_new_bytes(struct value const* value);

////////////////////////////////////////////////////////////////////////////////
//////// resolve.c /////////////////////////////////////////////////////////////

void
resolve(struct module* module);

////////////////////////////////////////////////////////////////////////////////
//////// eval.c ////////////////////////////////////////////////////////////////

struct value*
eval_rvalue(struct expr const* expr);
struct value*
eval_lvalue(struct expr const* expr);

////////////////////////////////////////////////////////////////////////////////
//////// codegen.c /////////////////////////////////////////////////////////////

void
codegen(char const* const opt_o, bool opt_k);

#endif // SUNDER_H_INCLUDED
