// Copyright 2021 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#define _XOPEN_SOURCE 700 /* realpath */
#include <assert.h>
#include <errno.h>
#include <limits.h> /* PATH_MAX */
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <libgen.h> /* dirname */
#include <sys/types.h> /* pid_t */
#include <sys/wait.h> /* wait* */
#include <unistd.h> /* exec*, isatty, fork */

#include "sunder.h"

// clang-format off
#define ANSI_ESC_DEFAULT "\x1b[0m"
#define ANSI_ESC_BOLD    "\x1b[1m"

#define ANSI_ESC_RED     "\x1b[31m"
#define ANSI_ESC_YELLOW  "\x1b[33m"
#define ANSI_ESC_CYAN    "\x1b[36m"

#define ANSI_MSG_DEBUG ANSI_ESC_BOLD ANSI_ESC_YELLOW
#define ANSI_MSG_ERROR ANSI_ESC_BOLD ANSI_ESC_RED
// clang-format on

char const*
source_line_start(char const* ptr)
{
    assert(ptr != NULL);

    while (ptr[-1] != '\n' && ptr[-1] != '\0') {
        ptr -= 1;
    }

    return ptr;
}

char const*
source_line_end(char const* ptr)
{
    assert(ptr != NULL);

    while (*ptr != '\n' && *ptr != '\0') {
        ptr += 1;
    }

    return ptr;
}

static void
messagev_(
    struct source_location const* location,
    char const* level_text,
    char const* level_ansi,
    char const* fmt,
    va_list args)
{
    assert(level_text != NULL);
    assert(level_ansi != NULL);
    assert(fmt != NULL);

    char const* const path = location != NULL ? location->path : NO_PATH;
    size_t const line = location != NULL ? location->line : NO_LINE;
    char const* const psrc = location != NULL ? location->psrc : NO_PSRC;

    bool const is_tty = isatty(STDERR_FILENO);

    if (path != NO_PATH || line != NO_LINE) {
        fprintf(stderr, "[");

        if (path != NO_PATH) {
            char const* const path_ansi_beg = is_tty ? ANSI_ESC_CYAN : "";
            char const* const path_ansi_end = is_tty ? ANSI_ESC_DEFAULT : "";
            fprintf(stderr, "%s%s%s", path_ansi_beg, path, path_ansi_end);
        }

        if (path != NO_PATH && line != NO_LINE) {
            fputc(':', stderr);
        }

        if (line != NO_LINE) {
            char const* const line_ansi_beg = is_tty ? ANSI_ESC_CYAN : "";
            char const* const line_ansi_end = is_tty ? ANSI_ESC_DEFAULT : "";
            fprintf(stderr, "%s%zu%s", line_ansi_beg, line, line_ansi_end);
        }

        fprintf(stderr, "] ");
    }

    char const* const level_ansi_beg = is_tty ? level_ansi : "";
    char const* const level_ansi_end = is_tty ? ANSI_ESC_DEFAULT : "";
    fprintf(stderr, "%s%s:%s ", level_ansi_beg, level_text, level_ansi_end);

    vfprintf(stderr, fmt, args);
    fputs("\n", stderr);

    if (psrc != NO_PSRC) {
        assert(path != NULL);
        char const* const line_start = source_line_start(psrc);
        char const* const line_end = source_line_end(psrc);

        fprintf(stderr, "%.*s\n", (int)(line_end - line_start), line_start);
        fprintf(stderr, "%*s^\n", (int)(psrc - line_start), "");
    }
}

void
debug(struct source_location const* location, char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    messagev_(location, "debug", ANSI_MSG_DEBUG, fmt, args);
    va_end(args);
}

void
error(struct source_location const* location, char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    messagev_(location, "error", ANSI_MSG_ERROR, fmt, args);
    va_end(args);
}

NORETURN void
fatal(struct source_location const* location, char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    messagev_(location, "error", ANSI_MSG_ERROR, fmt, args);
    va_end(args);

    exit(EXIT_FAILURE);
}

void
todo(char const* file, int line, char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[%s:%d] TODO: ", file, line);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputs("\n", stderr);
    exit(EXIT_FAILURE);
}

void
unreachable(char const* file, int line)
{
    fprintf(stderr, "[%s:%d] Unreachable!\n", file, line);
    exit(EXIT_FAILURE);
}

// TODO: Check if ((x + 7) & (-8)) is portable. Since it relies on two's
// complement it might not be strictly conforming for signed integers.
int
ceil8i(int x)
{
    while (x % 8 != 0) {
        x += 1;
    }
    return x;
}

size_t
ceil8zu(size_t x)
{
    while (x % 8u != 0u) {
        x += 1u;
    }
    return x;
}

int
bigint_to_u8(uint8_t* res, struct autil_bigint const* bigint)
{
    assert(res != NULL);
    assert(bigint != NULL);

    uintmax_t umax = 0;
    if (bigint_to_umax(&umax, bigint) || (umax > UINT8_MAX)) {
        return -1;
    }

    *res = (uint8_t)umax;
    return 0;
}

int
bigint_to_uz(size_t* res, struct autil_bigint const* bigint)
{
    assert(res != NULL);
    assert(bigint != NULL);

    uintmax_t umax = 0;
    if (bigint_to_umax(&umax, bigint) || (umax > SIZE_MAX)) {
        return -1;
    }

    *res = (size_t)umax;
    return 0;
}

int
bigint_to_umax(uintmax_t* res, struct autil_bigint const* bigint)
{
    assert(res != NULL);
    assert(bigint != NULL);

    if (autil_bigint_cmp(bigint, AUTIL_BIGINT_ZERO) < 0) {
        return -1;
    }

    char* const cstr = autil_bigint_to_new_cstr(bigint, NULL);
    errno = 0;
    uintmax_t umax = strtoumax(cstr, NULL, 0);
    int const err = errno; // save errno
    autil_xalloc(cstr, AUTIL_XALLOC_FREE);
    assert(err == 0 || err == ERANGE);
    if (err == ERANGE) {
        return -1;
    }

    *res = umax;
    return 0;
}

static void
bitarr_twos_complement_neg(struct autil_bitarr* bitarr)
{
    assert(bitarr != NULL);

    // Invert the bits...
    autil_bitarr_compl(bitarr, bitarr);

    // ...and add one.
    int carry = 1;
    size_t const bit_count = autil_bitarr_count(bitarr);
    for (size_t i = 0; i < bit_count; ++i) {
        int const new_digit = (carry + autil_bitarr_get(bitarr, i)) % 2;
        int const new_carry = (carry + autil_bitarr_get(bitarr, i)) >= 2;
        autil_bitarr_set(bitarr, i, new_digit);
        carry = new_carry;
    }
}

int
bigint_to_bitarr(struct autil_bitarr* res, struct autil_bigint const* bigint)
{
    assert(res != NULL);
    assert(bigint != NULL);

    size_t const mag_bit_count = autil_bigint_magnitude_bit_count(bigint);
    size_t const res_bit_count = autil_bitarr_count(res);
    if (mag_bit_count > res_bit_count) {
        return -1;
    }

    // Write the magnitude to the bigint into the bit array. If the bigint is
    // negative then we adjust the bit array below using two's complement
    // arithmetic.
    for (size_t i = 0; i < res_bit_count; ++i) {
        int const bit = autil_bigint_magnitude_bit_get(bigint, i);
        autil_bitarr_set(res, i, bit);
    }

    // Convert two's complement unsigned (magnitude) representation into
    // negative signed representation if necessary.
    if (autil_bigint_cmp(bigint, AUTIL_BIGINT_ZERO) < 0) {
        // Two's complement positive<->negative conversion.
        bitarr_twos_complement_neg(res);
    }

    return 0;
}

void
uz_to_bigint(struct autil_bigint* res, size_t uz)
{
    assert(res != 0);

    char buf[256] = {0};
    snprintf(buf, AUTIL_ARRAY_COUNT(buf), "%zu", uz);

    struct autil_bigint* const tmp = autil_bigint_new_cstr(buf);
    autil_bigint_assign(res, tmp);
    autil_bigint_del(tmp);
}

void
bitarr_to_bigint(
    struct autil_bigint* res, struct autil_bitarr const* bitarr, bool is_signed)
{
    assert(res != NULL);
    assert(bitarr != NULL);

    size_t const bit_count = autil_bitarr_count(bitarr);
    struct autil_bitarr* const mag_bits = autil_bitarr_new(bit_count);
    for (size_t i = 0; i < bit_count; ++i) {
        int const bit = autil_bitarr_get(bitarr, i);
        autil_bitarr_set(mag_bits, i, bit);
    }

    bool const is_neg = is_signed && autil_bitarr_get(bitarr, bit_count - 1u);
    if (is_neg) {
        // Two's complement negative<->positive conversion.
        bitarr_twos_complement_neg(mag_bits);
    }

    autil_bigint_assign(res, AUTIL_BIGINT_ZERO);
    for (size_t i = 0; i < bit_count; ++i) {
        int const bit = autil_bitarr_get(mag_bits, i);
        autil_bigint_magnitude_bit_set(res, i, bit);
    }

    if (is_neg) {
        autil_bigint_neg(res, res);
    }

    autil_bitarr_del(mag_bits);
}

int
spawnvpw(char const* const* argv)
{
    assert(argv != NULL);
    assert(argv[0] != NULL);

    pid_t const pid = fork();
    if (pid == -1) {
        fatal(NULL, "failed to fork with error '%s'", strerror(errno));
    }

    if (pid == 0) {
        // The POSIX 2017 rational section for the exec family of functions
        // notes that the neither the argv vector's elements nor the characters
        // within those elements are modified. The parameter declaration `char
        // *const argv[]` was chosen to allow for for historical compatibility.
        char* const* argv_ = (char* const*)argv;
        if (execvp(argv[0], argv_) == -1) {
            fatal(
                NULL,
                "failed to execvp '%s' with error '%s'",
                argv[0],
                strerror(errno));
        }
    }

    int status = 0;
    waitpid(pid, &status, 0);
    return status;
}

void
xspawnvpw(char const* const* argv)
{
    assert(argv != NULL);

    if (spawnvpw(argv) != 0) {
        exit(EXIT_FAILURE);
    }
}

bool
file_exists(char const* path)
{
    return access(path, F_OK) == 0;
}

char const*
canonical_path(char const* path)
{
    assert(path != NULL);

    char resolved_path[PATH_MAX] = {0};
    if (realpath(path, resolved_path) == NULL) {
        fatal(
            NULL,
            "failed resolve path '%s' with error '%s'",
            path,
            strerror(errno));
    }

    return autil_sipool_intern_cstr(context()->sipool, resolved_path);
}

char const*
directory_path(char const* path)
{
    assert(path != NULL);

    char const* const canonical = canonical_path(path);

    char* const tmp = autil_cstr_new_cstr(canonical);
    char const* const interned =
        autil_sipool_intern_cstr(context()->sipool, dirname(tmp));
    autil_xalloc(tmp, AUTIL_XALLOC_FREE);

    return interned;
}

static char*
read_source(char const* path)
{
    void* text = NULL;
    size_t text_size = 0;
    if (autil_file_read(path, &text, &text_size)) {
        struct source_location const location = {path, NO_LINE, NO_PSRC};
        fatal(
            &location,
            "failed to read '%s' with error '%s'",
            path,
            strerror(errno));
    }

    // NUL-prefix and NUL-terminator.
    // [t][e][x][t]
    // to
    // [\0][t][e][x][t][\0]
    text = autil_xalloc(text, text_size + 2);
    char* const result = (char*)text + 1;
    autil_memmove(result, text, text_size);
    result[-1] = '\0'; // NUL-prefix.
    result[text_size] = '\0'; // NUL-terminator.

    return result;
}

struct module*
module_new(char const* name, char const* path)
{
    assert(name != NULL);
    assert(path != NULL);

    struct module* const self = autil_xalloc(NULL, sizeof(*self));
    memset(self, 0x00, sizeof(*self));

    self->name = autil_sipool_intern_cstr(context()->sipool, name);
    self->path = autil_sipool_intern_cstr(context()->sipool, path);

    char* const source = read_source(self->path);
    autil_freezer_register(context()->freezer, source - 1);
    self->source = source;
    self->source_count = strlen(source);

    self->symbols = symbol_table_new(NULL);
    symbol_table_insert(
        self->symbols,
        context()->interned.void_,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.void_));
    symbol_table_insert(
        self->symbols,
        context()->interned.bool_,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.bool_));
    symbol_table_insert(
        self->symbols,
        context()->interned.byte,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.byte));
    symbol_table_insert(
        self->symbols,
        context()->interned.u8,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.u8));
    symbol_table_insert(
        self->symbols,
        context()->interned.s8,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.s8));
    symbol_table_insert(
        self->symbols,
        context()->interned.u16,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.u16));
    symbol_table_insert(
        self->symbols,
        context()->interned.s16,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.s16));
    symbol_table_insert(
        self->symbols,
        context()->interned.u32,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.u32));
    symbol_table_insert(
        self->symbols,
        context()->interned.s32,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.s32));
    symbol_table_insert(
        self->symbols,
        context()->interned.u64,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.u64));
    symbol_table_insert(
        self->symbols,
        context()->interned.s64,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.s64));
    symbol_table_insert(
        self->symbols,
        context()->interned.usize,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.usize));
    symbol_table_insert(
        self->symbols,
        context()->interned.ssize,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.ssize));
    symbol_table_insert(
        self->symbols,
        context()->interned.integer,
        symbol_table_lookup(
            context()->global_symbol_table, context()->interned.integer));

    self->exports = symbol_table_new(NULL);

    return self;
}

void
module_del(struct module* self)
{
    assert(self != NULL);

    symbol_table_freeze(self->symbols, context()->freezer);
    symbol_table_freeze(self->exports, context()->freezer);

    autil_sbuf_fini(self->ordered);
    memset(self, 0x00, sizeof(*self));
    autil_xalloc(self, AUTIL_XALLOC_FREE);
}

static struct context s_context = {0};

void
context_init(void)
{
    s_context.freezer = autil_freezer_new();

    s_context.sipool = autil_sipool_new();
#define INTERN_STR_LITERAL(str_literal)                                        \
    autil_sipool_intern_cstr(context()->sipool, str_literal)
    s_context.interned.empty = INTERN_STR_LITERAL("");
    s_context.interned.builtin = INTERN_STR_LITERAL("builtin");
    s_context.interned.return_ = INTERN_STR_LITERAL("return");
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
    struct autil_bigint* const ident = autil_bigint_new_cstr(str_literal);     \
    autil_bigint_freeze(ident, s_context.freezer);                             \
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

    s_context.static_symbols = autil_map_new(
        sizeof(CONTEXT_STATIC_SYMBOLS_MAP_KEY_TYPE),
        sizeof(CONTEXT_STATIC_SYMBOLS_MAP_VAL_TYPE),
        CONTEXT_STATIC_SYMBOLS_MAP_CMP_FUNC);
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
        autil_freezer_register(s_context.freezer, type);                       \
        struct symbol* const symbol =                                          \
            symbol_new_type(&s_context.builtin.location, type);                \
        autil_freezer_register(s_context.freezer, symbol);                     \
        symbol_table_insert(                                                   \
            s_context.global_symbol_table, symbol->name, symbol);              \
        builtin_lvalue = type;                                                 \
    }
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

    for (size_t i = 0; i < autil_sbuf_count(self->modules); ++i) {
        module_del(self->modules[i]);
    }
    autil_sbuf_fini(self->modules);

    autil_sipool_del(self->sipool);

    autil_map_del(self->static_symbols);
    symbol_table_freeze(self->global_symbol_table, self->freezer);

    autil_sbuf(struct symbol_table*) const template_symbol_tables =
        self->template_symbol_tables;
    for (size_t i = 0; i < autil_sbuf_count(template_symbol_tables); ++i) {
        symbol_table_freeze(template_symbol_tables[i], self->freezer);
    }
    autil_sbuf_fini(self->template_symbol_tables);

    autil_freezer_del(self->freezer);

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
    autil_sbuf_push(s_context.modules, module);

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

    for (size_t i = 0; i < autil_sbuf_count(context()->modules); ++i) {
        if (context()->modules[i]->path == path) {
            return context()->modules[i];
        }
    }
    return NULL;
}

#define AUTIL_IMPLEMENTATION
#include <autil/autil.h>
