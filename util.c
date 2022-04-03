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

#include <dirent.h> /* DIR, *dir-family */
#include <libgen.h> /* dirname */
#include <sys/stat.h> /* struct stat, stat */
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

#define ANSI_MSG_ERROR ANSI_ESC_BOLD ANSI_ESC_RED
// clang-format on

char*
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

bool
file_is_directory(char const* path)
{
    assert(path != NULL);

    struct stat statbuf = {0};
    if (stat(path, &statbuf) != 0) {
        return false;
    }
    return S_ISDIR(statbuf.st_mode);
}

char const*
canonical_path(char const* path)
{
    assert(path != NULL);

    char resolved_path[PATH_MAX] = {0};
    if (realpath(path, resolved_path) == NULL) {
        fatal(
            NULL,
            "failed to resolve path '%s' with error '%s'",
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

char const**
directory_files(char const* path)
{
    assert(path != NULL);

    DIR* const dir = opendir(path);
    if (dir == NULL) {
        fatal(
            NULL,
            "failed to open directory '%s' with error '%s'",
            path,
            strerror(errno));
    }

    autil_sbuf(char const*) files = NULL;
    struct dirent* dirent = {0};
    while ((dirent = readdir(dir)) != NULL) {
        char const* file = dirent->d_name;
        if (strcmp(file, ".") == 0 || strcmp(file, "..") == 0) {
            continue;
        }
        autil_sbuf_push(
            files, autil_sipool_intern_cstr(context()->sipool, file));
    }

    (void)closedir(dir);
    return files;
}

#define AUTIL_IMPLEMENTATION
#include <autil/autil.h>
