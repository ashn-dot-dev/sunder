// Copyright 2021-2022 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sunder.h"

static enum token_kind const KEYWORDS_FIRST = TOKEN_TRUE;
static enum token_kind const KEYWORDS_LAST = TOKEN_TYPEOF;
static enum token_kind const SIGILS_FIRST = TOKEN_SHL;
static enum token_kind const SIGILS_LAST = TOKEN_SEMICOLON;
static struct vstr token_kind_vstrs[TOKEN_EOF + 1u] = {
    // Keywords
    [TOKEN_TRUE] = VSTR_INIT_STR_LITERAL("true"),
    [TOKEN_FALSE] = VSTR_INIT_STR_LITERAL("false"),
    [TOKEN_NOT] = VSTR_INIT_STR_LITERAL("not"),
    [TOKEN_OR] = VSTR_INIT_STR_LITERAL("or"),
    [TOKEN_AND] = VSTR_INIT_STR_LITERAL("and"),
    [TOKEN_NAMESPACE] = VSTR_INIT_STR_LITERAL("namespace"),
    [TOKEN_IMPORT] = VSTR_INIT_STR_LITERAL("import"),
    [TOKEN_VAR] = VSTR_INIT_STR_LITERAL("var"),
    [TOKEN_CONST] = VSTR_INIT_STR_LITERAL("const"),
    [TOKEN_FUNC] = VSTR_INIT_STR_LITERAL("func"),
    [TOKEN_STRUCT] = VSTR_INIT_STR_LITERAL("struct"),
    [TOKEN_EXTEND] = VSTR_INIT_STR_LITERAL("extend"),
    [TOKEN_ALIAS] = VSTR_INIT_STR_LITERAL("alias"),
    [TOKEN_EXTERN] = VSTR_INIT_STR_LITERAL("extern"),
    [TOKEN_DUMP] = VSTR_INIT_STR_LITERAL("dump"),
    [TOKEN_RETURN] = VSTR_INIT_STR_LITERAL("return"),
    [TOKEN_DEFER] = VSTR_INIT_STR_LITERAL("defer"),
    [TOKEN_IF] = VSTR_INIT_STR_LITERAL("if"),
    [TOKEN_ELIF] = VSTR_INIT_STR_LITERAL("elif"),
    [TOKEN_ELSE] = VSTR_INIT_STR_LITERAL("else"),
    [TOKEN_FOR] = VSTR_INIT_STR_LITERAL("for"),
    [TOKEN_IN] = VSTR_INIT_STR_LITERAL("in"),
    [TOKEN_BREAK] = VSTR_INIT_STR_LITERAL("break"),
    [TOKEN_CONTINUE] = VSTR_INIT_STR_LITERAL("continue"),
    [TOKEN_ALIGNOF] = VSTR_INIT_STR_LITERAL("alignof"),
    [TOKEN_COUNTOF] = VSTR_INIT_STR_LITERAL("countof"),
    [TOKEN_SIZEOF] = VSTR_INIT_STR_LITERAL("sizeof"),
    [TOKEN_TYPEOF] = VSTR_INIT_STR_LITERAL("typeof"),
    // Sigils
    [TOKEN_SHL] = VSTR_INIT_STR_LITERAL("<<"),
    [TOKEN_SHR] = VSTR_INIT_STR_LITERAL(">>"),
    [TOKEN_EQ] = VSTR_INIT_STR_LITERAL("=="),
    [TOKEN_NE] = VSTR_INIT_STR_LITERAL("!="),
    [TOKEN_LE] = VSTR_INIT_STR_LITERAL("<="),
    [TOKEN_LT] = VSTR_INIT_STR_LITERAL("<"),
    [TOKEN_GE] = VSTR_INIT_STR_LITERAL(">="),
    [TOKEN_GT] = VSTR_INIT_STR_LITERAL(">"),
    [TOKEN_ASSIGN] = VSTR_INIT_STR_LITERAL("="),
    [TOKEN_PLUS] = VSTR_INIT_STR_LITERAL("+"),
    [TOKEN_DASH] = VSTR_INIT_STR_LITERAL("-"),
    [TOKEN_STAR] = VSTR_INIT_STR_LITERAL("*"),
    [TOKEN_FSLASH] = VSTR_INIT_STR_LITERAL("/"),
    [TOKEN_PERCENT] = VSTR_INIT_STR_LITERAL("%"),
    [TOKEN_TILDE] = VSTR_INIT_STR_LITERAL("~"),
    [TOKEN_PIPE] = VSTR_INIT_STR_LITERAL("|"),
    [TOKEN_CARET] = VSTR_INIT_STR_LITERAL("^"),
    [TOKEN_AMPERSAND] = VSTR_INIT_STR_LITERAL("&"),
    [TOKEN_LPAREN] = VSTR_INIT_STR_LITERAL("("),
    [TOKEN_RPAREN] = VSTR_INIT_STR_LITERAL(")"),
    [TOKEN_LBRACE] = VSTR_INIT_STR_LITERAL("{"),
    [TOKEN_RBRACE] = VSTR_INIT_STR_LITERAL("}"),
    [TOKEN_LBRACKET] = VSTR_INIT_STR_LITERAL("["),
    [TOKEN_RBRACKET] = VSTR_INIT_STR_LITERAL("]"),
    [TOKEN_COMMA] = VSTR_INIT_STR_LITERAL(","),
    [TOKEN_ELLIPSIS] = VSTR_INIT_STR_LITERAL("..."),
    [TOKEN_DOT_STAR] = VSTR_INIT_STR_LITERAL(".*"),
    [TOKEN_DOT] = VSTR_INIT_STR_LITERAL("."),
    [TOKEN_COLON_COLON] = VSTR_INIT_STR_LITERAL("::"),
    [TOKEN_COLON] = VSTR_INIT_STR_LITERAL(":"),
    [TOKEN_SEMICOLON] = VSTR_INIT_STR_LITERAL(";"),
    // Identifiers and Non-Keyword Literals
    [TOKEN_IDENTIFIER] = VSTR_INIT_STR_LITERAL("identifier"),
    [TOKEN_INTEGER] = VSTR_INIT_STR_LITERAL("integer"),
    [TOKEN_CHARACTER] = VSTR_INIT_STR_LITERAL("character"),
    [TOKEN_BYTES] = VSTR_INIT_STR_LITERAL("bytes"),
    // Meta
    [TOKEN_EOF] = VSTR_INIT_STR_LITERAL("end-of-file"),
};

char const*
token_kind_to_cstr(enum token_kind kind)
{
    assert((size_t)kind <= TOKEN_EOF);
    return token_kind_vstrs[(size_t)kind].start;
}

char*
token_to_new_cstr(struct token const* token)
{
    assert(token != NULL);

    if (token->kind == TOKEN_IDENTIFIER) {
        return cstr_new_fmt(
            "identifier(%.*s)", (int)token->count, token->start);
    }
    if (token->kind == TOKEN_INTEGER) {
        return cstr_new_fmt("integer(%.*s)", (int)token->count, token->start);
    }

    return cstr_new_cstr(token_kind_to_cstr(token->kind));
}

struct lexer {
    struct module* module;
    char const* current;
    size_t current_line;
};

struct lexer*
lexer_new(struct module* module)
{
    assert(module != NULL);

    struct lexer* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));

    self->module = module;
    self->current = module->source;
    self->current_line = 1;

    return self;
}

void
lexer_del(struct lexer* self)
{
    assert(self != NULL);

    memset(self, 0x00, sizeof(*self));
    xalloc(self, XALLOC_FREE);
}

static struct token*
token_new(
    char const* start,
    size_t count,
    struct source_location location,
    enum token_kind kind)
{
    assert(start != NULL || count == 0);
    assert(count != 0 || kind == TOKEN_EOF);

    struct token* const self = xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = kind;
    self->start = start;
    self->count = count;
    self->location = location;

    freeze(self);
    return self;
}

static struct token*
token_new_identifier(
    char const* start, size_t count, struct source_location location)
{
    assert(start != NULL && count != 0);

    struct token* const token =
        token_new(start, count, location, TOKEN_IDENTIFIER);
    token->data.identifier = intern(start, count);
    return token;
}

static struct token*
token_new_integer(
    char const* start,
    size_t count,
    struct source_location location,
    struct vstr number,
    struct vstr suffix)
{
    assert(start != NULL && count != 0);
    assert(number.start != NULL && number.count != 0);
    assert(suffix.start != NULL || suffix.count == 0);

    struct bigint* const value = bigint_new_text(number.start, number.count);
    assert(value != NULL);
    bigint_freeze(value);

    struct token* const token =
        token_new(start, count, location, TOKEN_INTEGER);
    token->data.integer.value = value;
    token->data.integer.suffix = intern(suffix.start, suffix.count);
    return token;
}

static void
skip_whitespace(struct lexer* self)
{
    assert(self != NULL);

    while (safe_isspace(*self->current)) {
        self->current_line += *self->current == '\n';
        self->current += 1;
    }
}

static void
skip_comment(struct lexer* self)
{
    assert(self != NULL);

    if (*self->current != '#') {
        return;
    }

    while (*self->current != '\0' && *self->current != '\n') {
        self->current += 1;
    }
    self->current += STR_LITERAL_COUNT("\n");
    self->current_line += 1;
}

static void
skip_whitespace_and_comments(struct lexer* self)
{
    assert(self != NULL);

    while (safe_isspace(*self->current) || (*self->current == '#')) {
        skip_whitespace(self);
        skip_comment(self);
    }
}

static struct token*
lex_keyword_or_identifier(struct lexer* self, struct source_location location)
{
    assert(self != NULL);
    assert(safe_isalpha(*self->current) || *self->current == '_');

    char const* const start = self->current;
    while (safe_isalnum(*self->current) || *self->current == '_') {
        self->current += 1;
    }
    size_t const count = (size_t)(self->current - start);

    for (int i = (int)KEYWORDS_FIRST; i <= (int)KEYWORDS_LAST; ++i) {
        struct vstr const* const keyword = &token_kind_vstrs[i];
        if (count == keyword->count
            && safe_memcmp(start, keyword->start, count) == 0) {
            return token_new(start, count, location, (enum token_kind)i);
        }
    }

    return token_new_identifier(start, count, location);
}

static struct token*
lex_integer(struct lexer* self, struct source_location location)
{
    assert(self != NULL);
    assert(safe_isdigit(*self->current));

    // Prefix
    char const* const number_start = self->current;
    int (*radix_isdigit)(int c) = safe_isdigit;
    if (cstr_starts_with(self->current, "0b")) {
        self->current += STR_LITERAL_COUNT("0b");
        radix_isdigit = safe_isbdigit;
    }
    else if (cstr_starts_with(self->current, "0o")) {
        self->current += STR_LITERAL_COUNT("0o");
        radix_isdigit = safe_isodigit;
    }
    else if (cstr_starts_with(self->current, "0x")) {
        self->current += STR_LITERAL_COUNT("0x");
        radix_isdigit = safe_isxdigit;
    }

    // Digits
    if (!radix_isdigit(*self->current)) {
        struct source_location const location = {
            self->module->name, self->current_line, self->current};
        fatal(&location, "integer literal has no digits");
    }
    while (radix_isdigit(*self->current)) {
        self->current += 1;
    }
    size_t const number_count = (size_t)(self->current - number_start);

    // Suffix
    char const* const suffix_start = self->current;
    while (safe_isalnum(*self->current)) {
        self->current += 1;
    }
    size_t const suffix_count = (size_t)(self->current - suffix_start);

    char const* const start = number_start;
    size_t const count = (size_t)(self->current - start);
    struct vstr const number = {number_start, number_count};
    struct vstr const suffix = {suffix_start, suffix_count};
    struct token* const token =
        token_new_integer(start, count, location, number, suffix);

    return token;
}

// Read and return one (possibly escaped) character. Invalid characters (i.e.
// characters that are not permitted in a character or bytes literal) will
// produce a fatal error. The `what` cstring should contain the noun of the
// token being parsed: "character literal", "bytes literal", etc.
//
// This function currently only supports reading of ASCII characters and escape
// sequences as Sunder does not yet have proper support for unicode scalar
// values.
static int
advance_character(struct lexer* self, char const* what)
{
    assert(self != NULL);

    // Check for invalid characters.
    if (*self->current == '\n') {
        struct source_location const location = {
            self->module->name, self->current_line, self->current};
        fatal(&location, "end-of-line encountered in %s", what);
    }
    if (!safe_isprint(*self->current)) {
        struct source_location const location = {
            self->module->name, self->current_line, self->current};
        fatal(
            &location,
            "non-printable byte 0x%02x in %s",
            (unsigned char)*self->current,
            what);
    }

    // Non-escaped character.
    if (*self->current != '\\') {
        return *self->current++;
    }

    // Parse escape sequence.
    switch (self->current[1]) {
    case '0': {
        self->current += 2;
        return '\0';
    }
    case 't': {
        self->current += 2;
        return '\t';
    }
    case 'n': {
        self->current += 2;
        return '\n';
    }
    case '\'': {
        self->current += 2;
        return '\'';
    }
    case '\"': {
        self->current += 2;
        return '\"';
    }
    case '\\': {
        self->current += 2;
        return '\\';
    }
    case 'x': {
        if (!safe_isxdigit(self->current[2])
            || !safe_isxdigit(self->current[3])) {
            struct source_location const location = {
                self->module->name, self->current_line, self->current};
            fatal(&location, "invalid hexadecimal escape sequence");
        }
        int const result =
            ((int)strtol((char[]){(char)self->current[2], '\0'}, NULL, 16) << 4)
            | (int)strtol((char[]){(char)self->current[3], '\0'}, NULL, 16);
        self->current += 4;
        return result;
    }
    default: {
        struct source_location const location = {
            self->module->name, self->current_line, self->current};
        fatal(&location, "unknown escape sequence");
    }
    }

    UNREACHABLE();
    return 0;
}

static struct token*
lex_character(struct lexer* self, struct source_location location)
{
    assert(self != NULL);

    assert(*self->current == '\'');
    char const* const start = self->current;
    self->current += 1;

    int const character = advance_character(self, "character literal");

    // Special check for the somewhat common case of a character literal
    // appearing without a closing single quote, followed by a newline:
    //
    // var foo: byte = 'a
    //
    // A similar check to this is handled within the advance_character function,
    // but this separate check is needed since only one character is ever be
    // checked/consumed when lexing a character token:
    //
    // var foo: byte = 'a
    //                 ^^^
    //                 ||+- The newline here is not checked by
    //                 ||   advance_character, so this separate check is needed.
    //                 |+- This character is checked by advance_character.
    //                 +- Lexing starts here.
    if (*self->current == '\n') {
        struct source_location const location = {
            self->module->name, self->current_line, self->current};
        fatal(&location, "end-of-line encountered in character literal");
    }

    if (*self->current != '\'') {
        struct source_location const location = {
            self->module->name, self->current_line, start};
        fatal(&location, "invalid character literal");
    }
    self->current += 1;

    struct token* token = token_new(
        start, (size_t)(self->current - start), location, TOKEN_CHARACTER);
    token->data.character = character;

    return token;
}

static struct token*
lex_bytes(struct lexer* self, struct source_location location)
{
    assert(self != NULL);

    assert(*self->current == '\"');
    char const* const start = self->current;
    self->current += 1;

    struct string* bytes = string_new(NULL, 0);
    while (*self->current != '"') {
        string_append_fmt(
            bytes, "%c", advance_character(self, "bytes literal"));
    }

    assert(*self->current == '"');
    self->current += 1;

    string_freeze(bytes);
    struct token* token = token_new(
        start, (size_t)(self->current - start), location, TOKEN_BYTES);
    token->data.bytes = bytes;

    return token;
}

static struct token*
lex_sigil(struct lexer* self, struct source_location location)
{
    assert(self != NULL);
    assert(safe_ispunct(*self->current));

    for (int i = (int)SIGILS_FIRST; i <= (int)SIGILS_LAST; ++i) {
        char const* const sigil_start = token_kind_vstrs[i].start;
        size_t const sigil_count = token_kind_vstrs[i].count;
        if (cstr_starts_with(self->current, sigil_start)) {
            self->current += sigil_count;
            return token_new(
                sigil_start, sigil_count, location, (enum token_kind)i);
        }
    }

    char const* const start = self->current;
    size_t count = 0;
    while (safe_ispunct(start[count]) && start[count] != '#') {
        count += 1;
    }

    fatal(&location, "invalid token `%.*s`", (int)count, start);
    return NULL;
}

struct token const*
lexer_next_token(struct lexer* self)
{
    assert(self != NULL);

    skip_whitespace_and_comments(self);
    struct source_location const location = (struct source_location){
        self->module->name,
        self->current_line,
        self->current,
    };

    char const ch = *self->current;
    if (safe_isalpha(ch) || ch == '_') {
        return lex_keyword_or_identifier(self, location);
    }
    if (safe_isdigit(ch)) {
        return lex_integer(self, location);
    }
    if (ch == '\'') {
        return lex_character(self, location);
    }
    if (ch == '\"') {
        return lex_bytes(self, location);
    }
    if (safe_ispunct(ch)) {
        return lex_sigil(self, location);
    }
    if (ch == '\0') {
        return token_new(self->current, 1u, location, TOKEN_EOF);
    }

    fatal(&location, "invalid token");
    return NULL;
}
