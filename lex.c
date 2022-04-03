// Copyright 2021 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <string.h>

#include "sunder.h"

static enum token_kind const KEYWORDS_FIRST = TOKEN_TRUE;
static enum token_kind const KEYWORDS_LAST = TOKEN_TYPEOF;
static enum token_kind const SIGILS_FIRST = TOKEN_EQ;
static enum token_kind const SIGILS_LAST = TOKEN_SEMICOLON;
static struct sunder_vstr token_kind_vstrs[TOKEN_EOF + 1u] = {
    // Keywords
    [TOKEN_TRUE] = SUNDER_VSTR_INIT_STR_LITERAL("true"),
    [TOKEN_FALSE] = SUNDER_VSTR_INIT_STR_LITERAL("false"),
    [TOKEN_NOT] = SUNDER_VSTR_INIT_STR_LITERAL("not"),
    [TOKEN_OR] = SUNDER_VSTR_INIT_STR_LITERAL("or"),
    [TOKEN_AND] = SUNDER_VSTR_INIT_STR_LITERAL("and"),
    [TOKEN_NAMESPACE] = SUNDER_VSTR_INIT_STR_LITERAL("namespace"),
    [TOKEN_IMPORT] = SUNDER_VSTR_INIT_STR_LITERAL("import"),
    [TOKEN_VAR] = SUNDER_VSTR_INIT_STR_LITERAL("var"),
    [TOKEN_CONST] = SUNDER_VSTR_INIT_STR_LITERAL("const"),
    [TOKEN_FUNC] = SUNDER_VSTR_INIT_STR_LITERAL("func"),
    [TOKEN_STRUCT] = SUNDER_VSTR_INIT_STR_LITERAL("struct"),
    [TOKEN_EXTEND] = SUNDER_VSTR_INIT_STR_LITERAL("extend"),
    [TOKEN_ALIAS] = SUNDER_VSTR_INIT_STR_LITERAL("alias"),
    [TOKEN_EXTERN] = SUNDER_VSTR_INIT_STR_LITERAL("extern"),
    [TOKEN_DUMP] = SUNDER_VSTR_INIT_STR_LITERAL("dump"),
    [TOKEN_RETURN] = SUNDER_VSTR_INIT_STR_LITERAL("return"),
    [TOKEN_IF] = SUNDER_VSTR_INIT_STR_LITERAL("if"),
    [TOKEN_ELIF] = SUNDER_VSTR_INIT_STR_LITERAL("elif"),
    [TOKEN_ELSE] = SUNDER_VSTR_INIT_STR_LITERAL("else"),
    [TOKEN_FOR] = SUNDER_VSTR_INIT_STR_LITERAL("for"),
    [TOKEN_IN] = SUNDER_VSTR_INIT_STR_LITERAL("in"),
    [TOKEN_BREAK] = SUNDER_VSTR_INIT_STR_LITERAL("break"),
    [TOKEN_CONTINUE] = SUNDER_VSTR_INIT_STR_LITERAL("continue"),
    [TOKEN_SYSCALL] = SUNDER_VSTR_INIT_STR_LITERAL("syscall"),
    [TOKEN_ALIGNOF] = SUNDER_VSTR_INIT_STR_LITERAL("alignof"),
    [TOKEN_COUNTOF] = SUNDER_VSTR_INIT_STR_LITERAL("countof"),
    [TOKEN_SIZEOF] = SUNDER_VSTR_INIT_STR_LITERAL("sizeof"),
    [TOKEN_TYPEOF] = SUNDER_VSTR_INIT_STR_LITERAL("typeof"),
    // Sigils
    [TOKEN_EQ] = SUNDER_VSTR_INIT_STR_LITERAL("=="),
    [TOKEN_NE] = SUNDER_VSTR_INIT_STR_LITERAL("!="),
    [TOKEN_LE] = SUNDER_VSTR_INIT_STR_LITERAL("<="),
    [TOKEN_LT] = SUNDER_VSTR_INIT_STR_LITERAL("<"),
    [TOKEN_GE] = SUNDER_VSTR_INIT_STR_LITERAL(">="),
    [TOKEN_GT] = SUNDER_VSTR_INIT_STR_LITERAL(">"),
    [TOKEN_ASSIGN] = SUNDER_VSTR_INIT_STR_LITERAL("="),
    [TOKEN_PLUS] = SUNDER_VSTR_INIT_STR_LITERAL("+"),
    [TOKEN_DASH] = SUNDER_VSTR_INIT_STR_LITERAL("-"),
    [TOKEN_STAR] = SUNDER_VSTR_INIT_STR_LITERAL("*"),
    [TOKEN_FSLASH] = SUNDER_VSTR_INIT_STR_LITERAL("/"),
    [TOKEN_TILDE] = SUNDER_VSTR_INIT_STR_LITERAL("~"),
    [TOKEN_PIPE] = SUNDER_VSTR_INIT_STR_LITERAL("|"),
    [TOKEN_CARET] = SUNDER_VSTR_INIT_STR_LITERAL("^"),
    [TOKEN_AMPERSAND] = SUNDER_VSTR_INIT_STR_LITERAL("&"),
    [TOKEN_LPAREN] = SUNDER_VSTR_INIT_STR_LITERAL("("),
    [TOKEN_RPAREN] = SUNDER_VSTR_INIT_STR_LITERAL(")"),
    [TOKEN_LBRACE] = SUNDER_VSTR_INIT_STR_LITERAL("{"),
    [TOKEN_RBRACE] = SUNDER_VSTR_INIT_STR_LITERAL("}"),
    [TOKEN_LBRACKET_LBRACKET] = SUNDER_VSTR_INIT_STR_LITERAL("[["),
    [TOKEN_RBRACKET_RBRACKET] = SUNDER_VSTR_INIT_STR_LITERAL("]]"),
    [TOKEN_LBRACKET] = SUNDER_VSTR_INIT_STR_LITERAL("["),
    [TOKEN_RBRACKET] = SUNDER_VSTR_INIT_STR_LITERAL("]"),
    [TOKEN_COMMA] = SUNDER_VSTR_INIT_STR_LITERAL(","),
    [TOKEN_ELLIPSIS] = SUNDER_VSTR_INIT_STR_LITERAL("..."),
    [TOKEN_DOT_STAR] = SUNDER_VSTR_INIT_STR_LITERAL(".*"),
    [TOKEN_DOT] = SUNDER_VSTR_INIT_STR_LITERAL("."),
    [TOKEN_COLON_COLON] = SUNDER_VSTR_INIT_STR_LITERAL("::"),
    [TOKEN_COLON] = SUNDER_VSTR_INIT_STR_LITERAL(":"),
    [TOKEN_SEMICOLON] = SUNDER_VSTR_INIT_STR_LITERAL(";"),
    // Identifiers and Non-Keyword Literals
    [TOKEN_IDENTIFIER] = SUNDER_VSTR_INIT_STR_LITERAL("identifier"),
    [TOKEN_INTEGER] = SUNDER_VSTR_INIT_STR_LITERAL("integer"),
    [TOKEN_CHARACTER] = SUNDER_VSTR_INIT_STR_LITERAL("character"),
    [TOKEN_BYTES] = SUNDER_VSTR_INIT_STR_LITERAL("bytes"),
    // Meta
    [TOKEN_EOF] = SUNDER_VSTR_INIT_STR_LITERAL("end-of-file"),
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
        return sunder_cstr_new_fmt(
            "identifier(%.*s)", (int)token->count, token->start);
    }
    if (token->kind == TOKEN_INTEGER) {
        return sunder_cstr_new_fmt(
            "integer(%.*s)", (int)token->count, token->start);
    }

    return sunder_cstr_new_cstr(token_kind_to_cstr(token->kind));
}

struct lexer {
    struct module* module;
    char const* current;
    size_t current_line;

    // Starting location of the token parsed by lexer_next_token, set at the
    // beginning of that function and read during the eventual token_new call.
    struct source_location next_token_location;
};

struct lexer*
lexer_new(struct module* module)
{
    assert(module != NULL);

    struct lexer* const self = sunder_xalloc(NULL, sizeof(*self));
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
    sunder_xalloc(self, SUNDER_XALLOC_FREE);
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

    struct token* const self = sunder_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));
    self->kind = kind;
    self->start = start;
    self->count = count;
    self->location = location;

    sunder_freezer_register(context()->freezer, self);
    return self;
}

static struct token*
token_new_identifier(
    char const* start, size_t count, struct source_location location)
{
    assert(start != NULL && count != 0);

    struct token* const token =
        token_new(start, count, location, TOKEN_IDENTIFIER);
    return token;
}

static struct token*
token_new_integer(
    char const* start,
    size_t count,
    struct source_location location,
    struct sunder_vstr number,
    struct sunder_vstr suffix)
{
    assert(start != NULL && count != 0);
    assert(number.start != NULL && number.count != 0);
    assert(suffix.start != NULL || suffix.count == 0);

    struct token* const token =
        token_new(start, count, location, TOKEN_INTEGER);
    token->data.integer.number = number;
    token->data.integer.suffix = suffix;
    return token;
}

static void
skip_whitespace(struct lexer* self)
{
    assert(self != NULL);

    while (sunder_isspace(*self->current)) {
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
    self->current += SUNDER_STR_LITERAL_COUNT("\n");
    self->current_line += 1;
}

static void
skip_whitespace_and_comments(struct lexer* self)
{
    assert(self != NULL);

    while (sunder_isspace(*self->current) || (*self->current == '#')) {
        skip_whitespace(self);
        skip_comment(self);
    }
}

static struct token*
lex_keyword_or_identifier(struct lexer* self)
{
    assert(self != NULL);
    assert(sunder_isalpha(*self->current) || *self->current == '_');

    char const* const start = self->current;
    while (sunder_isalnum(*self->current) || *self->current == '_') {
        self->current += 1;
    }
    size_t const count = (size_t)(self->current - start);
    struct sunder_vstr const symbol = {start, count};

    for (int i = (int)KEYWORDS_FIRST; i <= (int)KEYWORDS_LAST; ++i) {
        struct sunder_vstr const* const keyword = &token_kind_vstrs[i];
        if (sunder_vstr_cmp(&symbol, keyword) == 0) {
            return token_new(
                start, count, self->next_token_location, (enum token_kind)i);
        }
    }

    return token_new_identifier(start, count, self->next_token_location);
}

static struct token*
lex_integer(struct lexer* self)
{
    assert(self != NULL);
    assert(sunder_isdigit(*self->current));

    // Prefix
    char const* const number_start = self->current;
    int (*radix_isdigit)(int c) = sunder_isdigit;
    if (sunder_cstr_starts_with(self->current, "0b")) {
        self->current += SUNDER_STR_LITERAL_COUNT("0b");
        radix_isdigit = sunder_isbdigit;
    }
    else if (sunder_cstr_starts_with(self->current, "0o")) {
        self->current += SUNDER_STR_LITERAL_COUNT("0o");
        radix_isdigit = sunder_isodigit;
    }
    else if (sunder_cstr_starts_with(self->current, "0x")) {
        self->current += SUNDER_STR_LITERAL_COUNT("0x");
        radix_isdigit = sunder_isxdigit;
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
    while (sunder_isalnum(*self->current)) {
        self->current += 1;
    }
    size_t const suffix_count = (size_t)(self->current - suffix_start);

    char const* const start = number_start;
    size_t const count = (size_t)(self->current - start);
    struct sunder_vstr const number = {number_start, number_count};
    struct sunder_vstr const suffix = {suffix_start, suffix_count};
    struct token* const token = token_new_integer(
        start, count, self->next_token_location, number, suffix);

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
    if (!sunder_isprint(*self->current)) {
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
lex_character(struct lexer* self)
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
        start,
        (size_t)(self->current - start),
        self->next_token_location,
        TOKEN_CHARACTER);
    token->data.character = character;

    return token;
}

static struct token*
lex_bytes(struct lexer* self)
{
    assert(self != NULL);

    assert(*self->current == '\"');
    char const* const start = self->current;
    self->current += 1;

    struct sunder_string* bytes = sunder_string_new(NULL, 0);
    while (*self->current != '"') {
        sunder_string_append_fmt(
            bytes, "%c", advance_character(self, "bytes literal"));
    }

    assert(*self->current == '"');
    self->current += 1;

    sunder_string_freeze(bytes, context()->freezer);
    struct token* token = token_new(
        start,
        (size_t)(self->current - start),
        self->next_token_location,
        TOKEN_BYTES);
    token->data.bytes = bytes;

    return token;
}

static struct token*
lex_sigil(struct lexer* self)
{
    assert(self != NULL);
    assert(sunder_ispunct(*self->current));

    for (int i = (int)SIGILS_FIRST; i <= (int)SIGILS_LAST; ++i) {
        char const* const sigil_start = token_kind_vstrs[i].start;
        size_t const sigil_count = token_kind_vstrs[i].count;
        if (sunder_cstr_starts_with(self->current, sigil_start)) {
            self->current += sigil_count;
            return token_new(
                sigil_start,
                sigil_count,
                self->next_token_location,
                (enum token_kind)i);
        }
    }

    char const* const start = self->current;
    size_t count = 0;
    while (sunder_ispunct(start[count]) && start[count] != '#') {
        count += 1;
    }

    fatal(
        &self->next_token_location, "invalid token `%.*s`", (int)count, start);
    return NULL;
}

struct token const*
lexer_next_token(struct lexer* self)
{
    assert(self != NULL);

    skip_whitespace_and_comments(self);
    self->next_token_location = (struct source_location){
        self->module->name,
        self->current_line,
        self->current,
    };

    char const ch = *self->current;
    if (sunder_isalpha(ch) || ch == '_') {
        return lex_keyword_or_identifier(self);
    }
    if (sunder_isdigit(ch)) {
        return lex_integer(self);
    }
    if (ch == '\'') {
        return lex_character(self);
    }
    if (ch == '\"') {
        return lex_bytes(self);
    }
    if (sunder_ispunct(ch)) {
        return lex_sigil(self);
    }
    if (ch == '\0') {
        return token_new(
            self->current, 1u, self->next_token_location, TOKEN_EOF);
    }

    fatal(&self->next_token_location, "invalid token");
    return NULL;
}
