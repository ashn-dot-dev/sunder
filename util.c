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
#define ANSI_ESC_CYAN    "\x1b[36m"

#define ANSI_MSG_ERROR ANSI_ESC_BOLD ANSI_ESC_RED
// clang-format on

char*
read_source(char const* path)
{
    void* text = NULL;
    size_t text_size = 0;
    if (file_read_all(path, &text, &text_size)) {
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
    text = xalloc(text, text_size + 2);
    char* const result = (char*)text + 1;
    safe_memmove(result, text, text_size);
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
bigint_to_u8(uint8_t* res, struct bigint const* bigint)
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
bigint_to_uz(size_t* res, struct bigint const* bigint)
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
bigint_to_umax(uintmax_t* res, struct bigint const* bigint)
{
    assert(res != NULL);
    assert(bigint != NULL);

    if (bigint_cmp(bigint, BIGINT_ZERO) < 0) {
        return -1;
    }

    char* const cstr = bigint_to_new_cstr(bigint, NULL);
    errno = 0;
    uintmax_t umax = strtoumax(cstr, NULL, 0);
    int const err = errno; // save errno
    xalloc(cstr, XALLOC_FREE);
    assert(err == 0 || err == ERANGE);
    if (err == ERANGE) {
        return -1;
    }

    *res = umax;
    return 0;
}

static void
bitarr_twos_complement_neg(struct bitarr* bitarr)
{
    assert(bitarr != NULL);

    // Invert the bits...
    bitarr_compl(bitarr, bitarr);

    // ...and add one.
    int carry = 1;
    size_t const bit_count = bitarr_count(bitarr);
    for (size_t i = 0; i < bit_count; ++i) {
        int const new_digit = (carry + bitarr_get(bitarr, i)) % 2;
        int const new_carry = (carry + bitarr_get(bitarr, i)) >= 2;
        bitarr_set(bitarr, i, new_digit);
        carry = new_carry;
    }
}

int
bigint_to_bitarr(struct bitarr* res, struct bigint const* bigint)
{
    assert(res != NULL);
    assert(bigint != NULL);

    size_t const mag_bit_count = bigint_magnitude_bit_count(bigint);
    size_t const res_bit_count = bitarr_count(res);
    if (mag_bit_count > res_bit_count) {
        return -1;
    }

    // Write the magnitude to the bigint into the bit array. If the bigint is
    // negative then we adjust the bit array below using two's complement
    // arithmetic.
    for (size_t i = 0; i < res_bit_count; ++i) {
        int const bit = bigint_magnitude_bit_get(bigint, i);
        bitarr_set(res, i, bit);
    }

    // Convert two's complement unsigned (magnitude) representation into
    // negative signed representation if necessary.
    if (bigint_cmp(bigint, BIGINT_ZERO) < 0) {
        // Two's complement positive<->negative conversion.
        bitarr_twos_complement_neg(res);
    }

    return 0;
}

void
uz_to_bigint(struct bigint* res, size_t uz)
{
    assert(res != 0);

    char buf[256] = {0};
    snprintf(buf, ARRAY_COUNT(buf), "%zu", uz);

    struct bigint* const tmp = bigint_new_cstr(buf);
    bigint_assign(res, tmp);
    bigint_del(tmp);
}

void
bitarr_to_bigint(
    struct bigint* res, struct bitarr const* bitarr, bool is_signed)
{
    assert(res != NULL);
    assert(bitarr != NULL);

    size_t const bit_count = bitarr_count(bitarr);
    struct bitarr* const mag_bits = bitarr_new(bit_count);
    for (size_t i = 0; i < bit_count; ++i) {
        int const bit = bitarr_get(bitarr, i);
        bitarr_set(mag_bits, i, bit);
    }

    bool const is_neg = is_signed && bitarr_get(bitarr, bit_count - 1u);
    if (is_neg) {
        // Two's complement negative<->positive conversion.
        bitarr_twos_complement_neg(mag_bits);
    }

    bigint_assign(res, BIGINT_ZERO);
    for (size_t i = 0; i < bit_count; ++i) {
        int const bit = bitarr_get(mag_bits, i);
        bigint_magnitude_bit_set(res, i, bit);
    }

    if (is_neg) {
        bigint_neg(res, res);
    }

    bitarr_del(mag_bits);
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

    return sipool_intern_cstr(context()->sipool, resolved_path);
}

char const*
directory_path(char const* path)
{
    assert(path != NULL);

    char const* const canonical = canonical_path(path);

    char* const tmp = cstr_new_cstr(canonical);
    char const* const interned =
        sipool_intern_cstr(context()->sipool, dirname(tmp));
    xalloc(tmp, XALLOC_FREE);

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

    sbuf(char const*) files = NULL;
    struct dirent* dirent = {0};
    while ((dirent = readdir(dir)) != NULL) {
        char const* file = dirent->d_name;
        if (strcmp(file, ".") == 0 || strcmp(file, "..") == 0) {
            continue;
        }
        sbuf_push(files, sipool_intern_cstr(context()->sipool, file));
    }

    (void)closedir(dir);
    return files;
}

// USE: SUNDER_REALLOC(ptr, size)
#ifndef SUNDER_REALLOC
#    define SUNDER_REALLOC realloc
#endif

// USE: SUNDER_FREE(ptr)
#ifndef SUNDER_FREE
#    define SUNDER_FREE free
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

STATIC_ASSERT(CHAR_BIT_IS_8, CHAR_BIT == 8);

int
safe_isalnum(int c)
{
    return safe_isalpha(c) || safe_isdigit(c);
}

int
safe_isalpha(int c)
{
    return safe_isupper(c) || safe_islower(c);
}

int
safe_isblank(int c)
{
    return c == ' ' || c == '\t';
}

int
safe_iscntrl(int c)
{
    return (unsigned)c < 0x20 || c == 0x7f;
}

int
safe_isdigit(int c)
{
    return (unsigned)c - '0' < 10;
}

int
safe_isgraph(int c)
{
    return safe_isprint(c) && c != ' ';
}

int
safe_islower(int c)
{
    return (unsigned)c - 'a' < 26;
}

int
safe_isprint(int c)
{
    return 0x20 <= (unsigned)c && (unsigned)c <= 0x7e;
}

int
safe_ispunct(int c)
{
    return safe_isgraph(c) && !safe_isalnum(c);
}

int
safe_isspace(int c)
{
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t'
        || c == '\v';
}

int
safe_isupper(int c)
{
    return (unsigned)c - 'A' < 26;
}

int
safe_isbdigit(int c)
{
    return (unsigned)c - '0' < 2;
}

int
safe_isodigit(int c)
{
    return (unsigned)c - '0' < 8;
}

int
safe_isxdigit(int c)
{
    return safe_isdigit(c) || (unsigned)c - 'a' < 6 || (unsigned)c - 'A' < 6;
}

int
safe_memcmp(void const* s1, void const* s2, size_t n)
{
    assert(s1 != NULL || n == 0);
    assert(s2 != NULL || n == 0);

    if (n == 0) {
        return 0;
    }
    return memcmp(s1, s2, n);
}

void*
safe_memmove(void* dest, void const* src, size_t n)
{
    assert(dest != NULL || n == 0);
    assert(src != NULL || n == 0);

    if (n == 0) {
        return dest;
    }
    return memmove(dest, src, n);
}

void*
safe_memset(void* s, int c, size_t n)
{
    assert(s != NULL || n == 0);

    if (n == 0) {
        return s;
    }
    return memset(s, c, n);
}

void*
xalloc(void* ptr, size_t size)
{
    if (size == 0) {
        SUNDER_FREE(ptr);
        return NULL;
    }
    if ((ptr = SUNDER_REALLOC(ptr, size)) == NULL) {
        fatal(NULL, "[%s] Out of memory", __func__);
    }
    return ptr;
}

void*
xallocn(void* ptr, size_t nmemb, size_t size)
{
    size_t const sz = nmemb * size;
    if (nmemb != 0 && sz / nmemb != size) {
        fatal(NULL, "[%s] Integer overflow", __func__);
    }
    return xalloc(ptr, sz);
}

// Prepend othr_size bytes from othr onto the xalloc-allocated buffer of
// size *psize pointed to by *pdata, updating the address of *pdata if
// necessary.
static void
xalloc_prepend(void** pdata, size_t* psize, void const* othr, size_t othr_size)
{
    assert(pdata != NULL);
    assert(psize != NULL);
    assert(othr != NULL || othr_size == 0);
    if (othr_size == 0) {
        // [] + [A][B][C] => [A][B][C]
        return;
    }

    size_t const new_size = *psize + othr_size;
    void* const new_data = xalloc(*pdata, new_size);
    memmove((char*)new_data + othr_size, new_data, *psize);
    memcpy(new_data, othr, othr_size);

    *pdata = new_data;
    *psize = new_size;
}

// Append othr_size bytes from othr onto the xalloc-allocated buffer of
// size *psize pointed to by *pdata, updating the address of *pdata if
// necessary.
static void
xalloc_append(void** pdata, size_t* psize, void const* othr, size_t othr_size)
{
    assert(pdata != NULL);
    assert(psize != NULL);
    assert(othr != NULL || othr_size == 0);
    if (othr_size == 0) {
        // [A][B][C] + [] => [A][B][C]
        return;
    }

    size_t const new_size = *psize + othr_size;
    void* const new_data = xalloc(*pdata, new_size);
    memcpy((char*)new_data + *psize, othr, othr_size);

    *pdata = new_data;
    *psize = new_size;
}

int
file_read_all(char const* path, void** buf, size_t* buf_size)
{
    assert(path != NULL);
    assert(buf != NULL);
    assert(buf_size != NULL);

    FILE* const stream = fopen(path, "rb");
    if (stream == NULL) {
        return -1;
    }

    int const err = stream_read_all(stream, buf, buf_size);
    (void)fclose(stream);

    return err;
}

int
file_write_all(char const* path, void const* buf, size_t buf_size)
{
    assert(path != NULL);
    assert(buf != NULL || buf_size == 0);

    FILE* const stream = fopen(path, "wb");
    if (stream == NULL) {
        return -1;
    }

    size_t const written = fwrite(buf, 1, buf_size, stream);
    int const ferr = ferror(stream);
    int const fcls = fclose(stream);

    if (written != buf_size) {
        return -1;
    }
    if (ferr) {
        return -1;
    }
    if (fcls == EOF) {
        // According to the C99 standard:
        // > Any unwritten buffered data for the stream are delivered to the
        // > host environment to be written to the file; any unread buffered
        // > data are discarded. Whether or not the call succeeds, the stream
        // > is disassociated from the file...
        // Cautiously assume that the buffer was not fully written to disk.
        return -1;
    }

    return 0;
}

int
stream_read_all(FILE* stream, void** buf, size_t* buf_size)
{
    assert(stream != NULL);
    assert(buf != NULL);
    assert(buf_size != NULL);

    unsigned char* bf = NULL;
    size_t sz = 0;

    int c;
    while ((c = fgetc(stream)) != EOF) {
        bf = xalloc(bf, sz + 1);
        bf[sz++] = (unsigned char)c;
    }
    if (ferror(stream)) {
        xalloc(bf, XALLOC_FREE);
        return -1;
    }

    *buf = bf;
    *buf_size = sz;
    return 0;
}

char*
cstr_new(char const* start, size_t count)
{
    assert(start != NULL || count == 0);

    char* const s = xalloc(NULL, count + STR_LITERAL_COUNT("\0"));
    safe_memmove(s, start, count);
    s[count] = '\0';
    return s;
}

char*
cstr_new_cstr(char const* cstr)
{
    assert(cstr != NULL);

    size_t const count = strlen(cstr);
    char* const s = xalloc(NULL, count + STR_LITERAL_COUNT("\0"));
    return strcpy(s, cstr);
}

char*
cstr_new_fmt(char const* fmt, ...)
{
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);

    va_list copy;
    va_copy(copy, args);
    int const len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (len < 0) {
        fatal(NULL, "[%s] Formatting failure", __func__);
    }

    size_t size = (size_t)len + STR_LITERAL_COUNT("\0");
    char* const buf = xalloc(NULL, size);
    vsnprintf(buf, size, fmt, args);
    va_end(args);

    return buf;
}

int
cstr_starts_with(char const* cstr, char const* target)
{
    assert(cstr != NULL);
    assert(target != NULL);

    return strncmp(cstr, target, strlen(target)) == 0;
}

int
cstr_ends_with(char const* cstr, char const* target)
{
    assert(cstr != NULL);
    assert(target != NULL);

    return vstr_ends_with(
        VSTR_LOCAL_PTR(cstr, strlen(cstr)),
        VSTR_LOCAL_PTR(target, strlen(target)));
}

int
vstr_cmp(struct vstr const* lhs, struct vstr const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    size_t const n = lhs->count < rhs->count ? lhs->count : rhs->count;
    int const cmp = safe_memcmp(lhs->start, rhs->start, n);

    if (cmp != 0 || lhs->count == rhs->count) {
        return cmp;
    }
    return lhs->count < rhs->count ? -1 : +1;
}

int
vstr_starts_with(struct vstr const* vstr, struct vstr const* target)
{
    assert(vstr != NULL);
    assert(target != NULL);

    if (vstr->count < target->count) {
        return 0;
    }
    return safe_memcmp(vstr->start, target->start, target->count) == 0;
}

int
vstr_ends_with(struct vstr const* vstr, struct vstr const* target)
{
    assert(vstr != NULL);
    assert(target != NULL);

    if (vstr->count < target->count) {
        return 0;
    }
    char const* start = vstr->start + (vstr->count - target->count);
    return safe_memcmp(start, target->start, target->count) == 0;
}

struct sipool {
    // List of heap-allocated strings interned within this pool.
    // elements of the map member reference memory owned by this list.
    sbuf(char*) strings;
};

struct sipool*
sipool_new(void)
{
    struct sipool* const self = xalloc(NULL, sizeof(struct sipool));
    self->strings = NULL;
    return self;
}

void
sipool_del(struct sipool* self)
{
    if (self == NULL) {
        return;
    }

    for (size_t i = 0; i < sbuf_count(self->strings); ++i) {
        xalloc(self->strings[i], XALLOC_FREE);
    }
    sbuf_fini(self->strings);

    memset(self, 0x00, sizeof(*self)); // scrub
    xalloc(self, XALLOC_FREE);
}

char const*
sipool_intern(struct sipool* self, char const* start, size_t count)
{
    assert(self != NULL);
    assert(start != NULL || count == 0);

    struct vstr const vstr = {start, count};
    for (size_t i = 0; i < sbuf_count(self->strings); ++i) {
        struct vstr const element = {self->strings[i],
                                     strlen(self->strings[i])};
        if (vstr_cmp(&vstr, &element) == 0) {
            return self->strings[i];
        }
    }

    char* const str = cstr_new(start, count);
    sbuf_push(self->strings, str);

    return str;
}

char const*
sipool_intern_cstr(struct sipool* self, char const* cstr)
{
    assert(self != NULL);
    assert(cstr != NULL);

    return sipool_intern(self, cstr, strlen(cstr));
}

STATIC_ASSERT(
    SBUF_HEADER_OFFSET_IS_ALIGNED,
    SBUF__HEADER_OFFSET_ % ALIGNOF(max_align_type) == 0);

/* reserve */
void*
sbuf__rsv_(size_t elemsize, void* sbuf, size_t cap)
{
    assert(elemsize != 0);

    if (cap <= sbuf_capacity(sbuf)) {
        return sbuf;
    }

    assert(cap != 0);
    size_t const size = SBUF__HEADER_OFFSET_ + elemsize * cap;
    struct sbuf__header_* const header =
        xalloc(sbuf != NULL ? SBUF__PHEAD_MUTBL_(sbuf) : NULL, size);
    header->cnt_ = sbuf != NULL ? header->cnt_ : 0;
    header->cap_ = cap;
    return (char*)header + SBUF__HEADER_OFFSET_;
}

/* resize */
void*
sbuf__rsz_(size_t elemsize, void* sbuf, size_t cnt)
{
    assert(elemsize != 0);

    if (cnt == 0) {
        sbuf_fini(sbuf);
        return NULL;
    }

    if (cnt > sbuf_capacity(sbuf)) {
        sbuf = sbuf__rsv_(elemsize, sbuf, cnt);
    }
    assert(sbuf != NULL);
    SBUF__PHEAD_MUTBL_(sbuf)->cnt_ = cnt;
    return sbuf;
}

/* grow capacity by doubling */
void*
sbuf__grw_(size_t elemsize, void* sbuf)
{
    assert(elemsize != 0);

    size_t const cap = sbuf_capacity(sbuf);
    assert(sbuf_count(sbuf) == cap);

    static size_t const GROWTH_FACTOR = 2;
    static size_t const DEFAULT_CAPACITY = 8;
    size_t const new_cap = cap ? cap * GROWTH_FACTOR : DEFAULT_CAPACITY;
    return sbuf__rsv_(elemsize, sbuf, new_cap);
}

#define SUNDER__BITARR_WORD_TYPE_ unsigned long
#define SUNDER__BITARR_WORD_SIZE_ sizeof(SUNDER__BITARR_WORD_TYPE_)
#define SUNDER__BITARR_WORD_BITS_ (SUNDER__BITARR_WORD_SIZE_ * CHAR_BIT)
struct bitarr {
    size_t count;
    SUNDER__BITARR_WORD_TYPE_ words[];
};

static inline size_t
sunder__bitarr_word_count_(size_t count)
{
    return (count / SUNDER__BITARR_WORD_SIZE_)
        + (count % SUNDER__BITARR_WORD_SIZE_ != 0);
}

static inline size_t
sunder__bitarr_size_(size_t count)
{
    return sizeof(struct bitarr)
        + (sunder__bitarr_word_count_(count) * SUNDER__BITARR_WORD_SIZE_);
}

struct bitarr*
bitarr_new(size_t count)
{
    size_t const size = sunder__bitarr_size_(count);
    struct bitarr* const self = xalloc(NULL, size);
    memset(self, 0x00, size);

    self->count = count;
    return self;
}

void
bitarr_del(struct bitarr* self)
{
    if (self == NULL) {
        return;
    }

    size_t const size = sunder__bitarr_size_(self->count);
    memset(self, 0x00, size); // scrub
    xalloc(self, XALLOC_FREE);
}

void
bitarr_freeze(struct bitarr* self, struct freezer* freezer)
{
    assert(self != NULL);
    assert(freezer != NULL);

    freezer_register(freezer, self);
}

size_t
bitarr_count(struct bitarr const* self)
{
    assert(self != NULL);

    return self->count;
}

void
bitarr_set(struct bitarr* self, size_t n, int value)
{
    assert(self != NULL);

    if (n >= self->count) {
        fatal(NULL, "[%s] Index out of bounds (%zu)", __func__, n);
    }

    SUNDER__BITARR_WORD_TYPE_* const pword =
        &self->words[n / SUNDER__BITARR_WORD_SIZE_];
    SUNDER__BITARR_WORD_TYPE_ const mask = (SUNDER__BITARR_WORD_TYPE_)1u
        << (n % SUNDER__BITARR_WORD_SIZE_);
    *pword =
        (SUNDER__BITARR_WORD_TYPE_)(value ? *pword | mask : *pword & ~mask);
}

int
bitarr_get(struct bitarr const* self, size_t n)
{
    assert(self != NULL);

    if (n >= self->count) {
        fatal(NULL, "[%s] Index out of bounds (%zu)", __func__, n);
    }

    SUNDER__BITARR_WORD_TYPE_ const word =
        self->words[n / SUNDER__BITARR_WORD_SIZE_];
    SUNDER__BITARR_WORD_TYPE_ const mask = (SUNDER__BITARR_WORD_TYPE_)1u
        << (n % SUNDER__BITARR_WORD_SIZE_);

    return (word & mask) != 0;
}

void
bitarr_assign(struct bitarr* self, struct bitarr const* othr)
{
    assert(self != NULL);
    assert(othr != NULL);

    if (self->count != othr->count) {
        fatal(
            NULL,
            "[%s] Mismatched array counts (%zu, %zu)",
            __func__,
            self->count,
            othr->count);
    }

    assert(
        sunder__bitarr_size_(self->count) == sunder__bitarr_size_(othr->count));
    safe_memmove(self, othr, sunder__bitarr_size_(othr->count));
}

void
bitarr_compl(struct bitarr* res, struct bitarr const* rhs)
{
    assert(res != NULL);
    assert(rhs != NULL);

    if (res->count != rhs->count) {
        fatal(
            NULL,
            "[%s] Mismatched array counts (%zu, %zu)",
            __func__,
            res->count,
            rhs->count);
    }

    for (size_t i = 0; i < sunder__bitarr_word_count_(res->count); ++i) {
        res->words[i] = ~rhs->words[i];
    }
}

void
bitarr_shiftl(struct bitarr* res, struct bitarr const* lhs, size_t nbits)
{
    assert(res != NULL);
    assert(lhs != NULL);

    if (res->count != lhs->count) {
        fatal(
            NULL,
            "[%s] Mismatched array counts (%zu, %zu)",
            __func__,
            res->count,
            lhs->count);
    }

    size_t const count = bitarr_count(res);
    bitarr_assign(res, lhs);
    for (size_t n = 0; n < nbits; ++n) {
        for (size_t i = count - 1; i != 0; --i) {
            bitarr_set(res, i, bitarr_get(res, i - 1u));
        }
        bitarr_set(res, 0u, 0);
    }
}

void
bitarr_shiftr(struct bitarr* res, struct bitarr const* lhs, size_t nbits)
{
    assert(res != NULL);
    assert(lhs != NULL);

    if (res->count != lhs->count) {
        fatal(
            NULL,
            "[%s] Mismatched array counts (%zu, %zu)",
            __func__,
            res->count,
            lhs->count);
    }

    size_t const count = bitarr_count(res);
    bitarr_assign(res, lhs);
    for (size_t n = 0; n < nbits; ++n) {
        for (size_t i = 0; i < count - 1; ++i) {
            bitarr_set(res, i, bitarr_get(res, i + 1u));
        }
        bitarr_set(res, count - 1, 0);
    }
}

void
bitarr_and(
    struct bitarr* res, struct bitarr const* lhs, struct bitarr const* rhs)
{
    assert(res != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    if (res->count != lhs->count || res->count != rhs->count) {
        fatal(
            NULL,
            "[%s] Mismatched array counts (%zu, %zu, %zu)",
            __func__,
            res->count,
            lhs->count,
            rhs->count);
    }

    for (size_t i = 0; i < sunder__bitarr_word_count_(res->count); ++i) {
        res->words[i] = lhs->words[i] & rhs->words[i];
    }
}

void
bitarr_xor(
    struct bitarr* res, struct bitarr const* lhs, struct bitarr const* rhs)
{
    assert(res != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    if (res->count != lhs->count || res->count != rhs->count) {
        fatal(
            NULL,
            "[%s] Mismatched array counts (%zu, %zu, %zu)",
            __func__,
            res->count,
            lhs->count,
            rhs->count);
    }

    for (size_t i = 0; i < sunder__bitarr_word_count_(res->count); ++i) {
        res->words[i] = lhs->words[i] ^ rhs->words[i];
    }
}

void
bitarr_or(
    struct bitarr* res, struct bitarr const* lhs, struct bitarr const* rhs)
{
    assert(res != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    if (res->count != lhs->count || res->count != rhs->count) {
        fatal(
            NULL,
            "[%s] Mismatched array counts (%zu, %zu, %zu)",
            __func__,
            res->count,
            lhs->count,
            rhs->count);
    }

    for (size_t i = 0; i < sunder__bitarr_word_count_(res->count); ++i) {
        res->words[i] = lhs->words[i] | rhs->words[i];
    }
}

// Arbitrary precision integer.
// A bigint conceptually consists of the following components:
// (1) sign: The arithmetic sign of the integer (+, -, or 0).
// (2) magnitude: The absolute value of the bigint, presented through this API
//     as an infinitely long sequence of bits with little endian ordering.
//
// The internals of struct bigint are designed such that initializing an
// bigint with:
//      struct bigint foo = {0};
// or
//      struct bigint foo;
//      memset(&foo, 0x00, sizeof(foo));
// will create a bigint equal to zero without requiring heap allocation.
struct bigint {
    // -1 if the integer <  0
    //  0 if the integer == 0
    // +1 if the integer >  0
    int sign;
    // Magnitude of the integer.
    // Little endian list of bytes.
    // The integer zero will have limbs == NULL.
    uint8_t* limbs;
    // Number of limbs.
    // The integer zero will have count == 0.
    size_t count;
};
// Most arbitrary precision integer implementations use a limb size relative to
// the natural word size of the target machine. For instance the GMP mp_limb_t
// type is a typedef of either unsigned int, unsigned long int, or unsigned
// long long int depending on configuration options and the host environment.
// The autil arbitrary precision integer implementation uses eight-bit limbs to
// avoid configuration options, preprocessor checks, and/or bugs that would
// come about from having multiple potential limb sizes. On every (modern)
// machine uint8_t and uint16_t values will implicitly cast up to unsigned int
// values cleanly which makes addition and multiplication of limbs work without
// needing to think too hard.
#define SUNDER__BIGINT_LIMB_BITS_ ((size_t)8)
STATIC_ASSERT(
    correct_bits_per_limb,
    SUNDER__BIGINT_LIMB_BITS_
        == (sizeof(*((struct bigint*)0)->limbs) * CHAR_BIT));

struct bigint const* const BIGINT_ZERO =
    &(struct bigint){.sign = 0, .limbs = NULL, .count = 0u};
struct bigint const* const BIGINT_POS_ONE =
    &(struct bigint){.sign = +1, .limbs = (uint8_t[]){0x01}, .count = 1u};
struct bigint const* const BIGINT_NEG_ONE =
    &(struct bigint){.sign = -1, .limbs = (uint8_t[]){0x01}, .count = 1u};

static struct bigint const* const BIGINT_DEC =
    &(struct bigint){.sign = +1, .limbs = (uint8_t[]){0x0A}, .count = 1u};
static struct bigint const* const BIGINT_BIN =
    &(struct bigint){.sign = +1, .limbs = (uint8_t[]){0x02}, .count = 1u};
static struct bigint const* const BIGINT_OCT =
    &(struct bigint){.sign = +1, .limbs = (uint8_t[]){0x08}, .count = 1u};
static struct bigint const* const BIGINT_HEX =
    &(struct bigint){.sign = +1, .limbs = (uint8_t[]){0x10}, .count = 1u};

static void
sunder__bigint_fini_(struct bigint* self)
{
    assert(self != NULL);

    xalloc(self->limbs, XALLOC_FREE);
    memset(self, 0x00, sizeof(*self)); // scrub
}

static void
sunder__bigint_resize_(struct bigint* self, size_t count)
{
    assert(self != NULL);

    if (count <= self->count) {
        self->count = count;
        return;
    }

    size_t const nlimbs = count - self->count; // Number of limbs to add.
    self->count = count;
    self->limbs = xalloc(self->limbs, self->count);
    memset(self->limbs + self->count - nlimbs, 0x00, nlimbs);
}

static void
sunder__bigint_normalize_(struct bigint* self)
{
    assert(self != NULL);

    while ((self->count != 0) && (self->limbs[self->count - 1] == 0)) {
        self->count -= 1;
    }
    if (self->count == 0) {
        self->sign = 0;
    }
}

// Shift left by nlimbs number of limbs.
// Example:
//      -0xFFEE shifted left by nlimbs=2 becomes -0xFFEE0000 with 8-bit limbs.
static void
sunder__bigint_shiftl_limbs_(struct bigint* self, size_t nlimbs)
{
    assert(self != NULL);
    if (nlimbs == 0) {
        return;
    }

    self->count += nlimbs;
    self->limbs = xalloc(self->limbs, self->count);
    memmove((char*)self->limbs + nlimbs, self->limbs, self->count - nlimbs);
    memset(self->limbs, 0x00, nlimbs);
}

// Shift right by nlimbs number of limbs.
// Example:
//      -0xFFEE00 shifted right by nlimbs=2 becomes -0xFF with 8-bit limbs.
static void
sunder__bigint_shiftr_limbs_(struct bigint* self, size_t nlimbs)
{
    assert(self != NULL);
    if (nlimbs == 0) {
        return;
    }
    if (nlimbs > self->count) {
        fatal(
            NULL,
            "[%s] Attempted right shift of %zu limbs on bigint with %zu limbs",
            __func__,
            nlimbs,
            self->count);
    }

    memmove((char*)self->limbs, self->limbs + nlimbs, self->count - nlimbs);
    self->count -= nlimbs;
    sunder__bigint_normalize_(self);
}

// This function is "technically" part of the public API, but it is not given a
// prototype, and is intended for debugging purposes only.
// Expect this function to be removed in future versions of this header.
void
bigint_dump(struct bigint const* self)
{
    assert(self != NULL);

    int sign = self->sign;
    char signc = '0';
    if (sign == +1) {
        signc = '+';
    }
    if (sign == -1) {
        signc = '-';
    }

    FILE* const fp = stdout;
    fprintf(fp, "SIGN: '%c', COUNT: %zu, LIMBS: [", signc, self->count);
    for (size_t i = 0; i < self->count; ++i) {
        fprintf(fp, "0x%02X", (int)self->limbs[i]);
        if (i != self->count - 1) {
            fputs(", ", fp);
        }
    }
    fputs("]\n", fp);
}

struct bigint*
bigint_new(struct bigint const* othr)
{
    assert(othr != NULL);

    struct bigint* const self = xalloc(NULL, sizeof(struct bigint));
    *self = *BIGINT_ZERO;
    bigint_assign(self, othr);
    return self;
}

struct bigint*
bigint_new_cstr(char const* cstr)
{
    assert(cstr != NULL);

    return bigint_new_text(cstr, strlen(cstr));
}

struct bigint*
bigint_new_text(char const* start, size_t count)
{
    assert(start != NULL);

    struct bigint* self = NULL;
    char const* const end = start + count;

    // Default to decimal radix.
    int radix = 10;
    struct bigint const* radix_bigint = BIGINT_DEC;
    int (*radix_isdigit)(int c) = safe_isdigit;

    // Begin iterating over the string from left to right.
    char const* cur = start;
    goto sign;

sign:
    if (cur == end) {
        // No prefix, radix identifier, or digits.
        goto error;
    }
    int sign = +1;
    if (*cur == '+') {
        sign = +1;
        cur += 1;
        goto radix;
    }
    if (*cur == '-') {
        sign = -1;
        cur += 1;
        goto radix;
    }

radix:
    if ((size_t)(end - cur) < STR_LITERAL_COUNT("0x")) {
        // String is not long enough to have a radix identifier.
        goto digits;
    }
    if (cur[0] != '0') {
        goto digits;
    }

    if (cur[1] == 'b') {
        radix = 2;
        radix_bigint = BIGINT_BIN;
        radix_isdigit = safe_isbdigit;
        cur += 2;
        goto digits;
    }
    if (cur[1] == 'o') {
        radix = 8;
        radix_bigint = BIGINT_OCT;
        radix_isdigit = safe_isodigit;
        cur += 2;
        goto digits;
    }
    if (cur[1] == 'x') {
        radix = 16;
        radix_bigint = BIGINT_HEX;
        radix_isdigit = safe_isxdigit;
        cur += 2;
        goto digits;
    }

digits:
    if (cur == end) {
        // No digits.
        goto error;
    }
    char const* const digits_start = cur;
    while (cur != end) {
        if (!radix_isdigit(*cur)) {
            // Invalid digit.
            goto error;
        }
        cur += 1;
    }

    self = bigint_new(BIGINT_ZERO);
    cur = digits_start;
    while (cur != end) {
        errno = 0;
        uint8_t const digit_value =
            (uint8_t)strtol((char[]){(char)*cur, '\0'}, NULL, radix);
        if (errno != 0) {
            goto error;
        }

        struct bigint const digit_bigint = {
            .sign = +1, .limbs = (uint8_t[]){digit_value}, .count = 1u};
        bigint_mul(self, self, radix_bigint);
        bigint_add(self, self, &digit_bigint);

        cur += 1;
    }

    self->sign = sign;
    sunder__bigint_normalize_(self);
    return self;

error:
    bigint_del(self);
    return NULL;
}

void
bigint_del(struct bigint* self)
{
    if (self == NULL) {
        return;
    }

    sunder__bigint_fini_(self);
    xalloc(self, XALLOC_FREE);
}

void
bigint_freeze(struct bigint* self, struct freezer* freezer)
{
    assert(self != NULL);
    assert(freezer != NULL);

    freezer_register(freezer, self);
    freezer_register(freezer, self->limbs);
}

int
bigint_cmp(struct bigint const* lhs, struct bigint const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    if (lhs->sign > rhs->sign) {
        return +1;
    }
    if (lhs->sign < rhs->sign) {
        return -1;
    }

    assert(lhs->sign == rhs->sign);
    int const sign = lhs->sign;
    if (lhs->count > rhs->count) {
        return sign;
    }
    if (lhs->count < rhs->count) {
        return -sign;
    }

    assert(lhs->count == rhs->count);
    size_t const count = lhs->count;
    for (size_t i = 0; i < count; ++i) {
        size_t limb_idx = count - i - 1;
        uint8_t const lhs_limb = lhs->limbs[limb_idx];
        uint8_t const rhs_limb = rhs->limbs[limb_idx];

        if (lhs_limb > rhs_limb) {
            return sign;
        }
        if (lhs_limb < rhs_limb) {
            return -sign;
        }
    }

    return 0;
}

void
bigint_assign(struct bigint* self, struct bigint const* othr)
{
    assert(self != NULL);
    assert(othr != NULL);
    if (self == othr) {
        return;
    }

    self->sign = othr->sign;
    self->limbs = xalloc(self->limbs, othr->count);
    self->count = othr->count;
    if (self->sign != 0) {
        assert(self->limbs != NULL);
        assert(othr->limbs != NULL);
        memcpy(self->limbs, othr->limbs, othr->count);
    }
}

void
bigint_neg(struct bigint* res, struct bigint const* rhs)
{
    assert(res != NULL);
    assert(rhs != NULL);

    bigint_assign(res, rhs);
    // +1 * -1 == -1
    // -1 * -1 == +1
    //  0 * -1 ==  0
    res->sign *= -1;
}

void
bigint_abs(struct bigint* res, struct bigint const* rhs)
{
    assert(res != NULL);
    assert(rhs != NULL);

    bigint_assign(res, rhs);
    // +1 * +1 == +1
    // -1 * -1 == +1
    //  0 *  0 ==  0
    res->sign = rhs->sign * rhs->sign;
}

void
bigint_add(
    struct bigint* res, struct bigint const* lhs, struct bigint const* rhs)
{
    assert(res != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    // 0 + rhs == rhs
    if (lhs->sign == 0) {
        bigint_assign(res, rhs);
        return;
    }
    // lhs + 0 == lhs
    if (rhs->sign == 0) {
        bigint_assign(res, lhs);
        return;
    }
    // (+lhs) + (-rhs) == (+lhs) - (+rhs)
    if ((lhs->sign == +1) && (rhs->sign == -1)) {
        struct bigint* const RHS = bigint_new(BIGINT_ZERO);
        bigint_neg(RHS, rhs);
        bigint_sub(res, lhs, RHS);
        bigint_del(RHS);
        return;
    }
    // (-lhs) + (+rhs) == (+rhs) - (+lhs)
    if ((lhs->sign == -1) && (rhs->sign == +1)) {
        struct bigint* const LHS = bigint_new(BIGINT_ZERO);
        bigint_neg(LHS, lhs);
        bigint_sub(res, rhs, LHS);
        bigint_del(LHS);
        return;
    }

    // (+lhs) + (+rhs) == +(lhs + rhs)
    // (-lhs) + (-rhs) == -(lhs + rhs)
    assert(lhs->sign == rhs->sign);
    int const sign = lhs->sign;

    struct bigint RES = {0};
    RES.sign = sign;
    RES.count = 1 + (lhs->count > rhs->count ? lhs->count : rhs->count);
    RES.limbs = xalloc(RES.limbs, RES.count);

    unsigned carry = 0;
    for (size_t i = 0; i < RES.count; ++i) {
        unsigned const lhs_limb = i < lhs->count ? lhs->limbs[i] : 0; // upcast
        unsigned const rhs_limb = i < rhs->count ? rhs->limbs[i] : 0; // upcast
        unsigned const tot = lhs_limb + rhs_limb + carry;

        RES.limbs[i] = (uint8_t)tot;
        carry = tot > UINT8_MAX;
    }
    assert(carry == 0);

    sunder__bigint_normalize_(&RES);
    bigint_assign(res, &RES);
    sunder__bigint_fini_(&RES);
}

void
bigint_sub(
    struct bigint* res, struct bigint const* lhs, struct bigint const* rhs)
{
    assert(res != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    // 0 - rhs == -(rhs)
    if (lhs->sign == 0) {
        bigint_neg(res, rhs);
        return;
    }
    // lhs - 0 == lhs
    if (rhs->sign == 0) {
        bigint_assign(res, lhs);
        return;
    }
    // (+lhs) - (-rhs) == (+lhs) + (+rhs)
    if ((lhs->sign == +1) && (rhs->sign == -1)) {
        struct bigint* const RHS = bigint_new(BIGINT_ZERO);
        bigint_neg(RHS, rhs);
        bigint_add(res, lhs, RHS);
        bigint_del(RHS);
        return;
    }
    // (-lhs) - (+rhs) == (-lhs) + (-rhs)
    if ((lhs->sign == -1) && (rhs->sign == +1)) {
        struct bigint* const RHS = bigint_new(BIGINT_ZERO);
        bigint_neg(RHS, rhs);
        bigint_add(res, lhs, RHS);
        bigint_del(RHS);
        return;
    }

    // (+lhs) - (+rhs) == +(lhs - rhs)
    // (-lhs) - (-rhs) == -(lhs - rhs)
    assert(lhs->sign == rhs->sign);
    int const sign = lhs->sign;
    // Note that the expression (lhs - rhs) will require flipping the sign of
    // the result if the magnitude of lhs is greater than the magnitude of rhs:
    // (+5) - (+3) == +2
    // (+3) - (+5) == -2
    // (-5) - (-3) == -2
    // (-3) - (-5) == +2
    int const cmp = bigint_cmp(lhs, rhs);
    int const neg = ((sign == +1) && (cmp < 0)) || ((sign == -1) && (cmp > 0));
    if (neg) {
        struct bigint const* tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    struct bigint RES = {0};
    RES.sign = lhs->sign;
    RES.count = lhs->count > rhs->count ? lhs->count : rhs->count;
    RES.limbs = xalloc(RES.limbs, RES.count);

    unsigned borrow = 0;
    for (size_t i = 0; i < RES.count; ++i) {
        unsigned const lhs_limb = i < lhs->count ? lhs->limbs[i] : 0; // upcast
        unsigned const rhs_limb = i < rhs->count ? rhs->limbs[i] : 0; // upcast
        unsigned const tot = (lhs_limb - rhs_limb) - borrow;

        RES.limbs[i] = (uint8_t)tot;
        borrow = tot > UINT8_MAX;
    }
    assert(borrow == 0);

    if (neg) {
        bigint_neg(&RES, &RES);
    }
    sunder__bigint_normalize_(&RES);
    bigint_assign(res, &RES);
    sunder__bigint_fini_(&RES);
}

// res  = lhs * rhs
void
bigint_mul(
    struct bigint* res, struct bigint const* lhs, struct bigint const* rhs)
{
    assert(res != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    // 0 * rhs == 0
    if (lhs->sign == 0) {
        bigint_assign(res, BIGINT_ZERO);
        return;
    }
    // lhs * 0 == 0
    if (rhs->sign == 0) {
        bigint_assign(res, BIGINT_ZERO);
        return;
    }

    // Algorithm M (Multiplication of Nonnegative Integers)
    // Source: Art of Computer Programming, Volume 2: Seminumerical Algorithms
    //         (Third Edition) page. 268.
    size_t const count = lhs->count + rhs->count;
    struct bigint W = {0}; // abs(res)
    sunder__bigint_resize_(&W, count);
    uint8_t* const w = W.limbs;
    uint8_t const* const u = lhs->limbs;
    uint8_t const* const v = rhs->limbs;
    size_t const m = lhs->count;
    size_t const n = rhs->count;
    static unsigned const b = UINT8_MAX + 1;
    for (size_t j = 0; j < n; ++j) {
        if (v[j] == 0) {
            w[j + m] = 0;
            continue;
        }
        unsigned k = 0;
        for (size_t i = 0; i < m; ++i) {
            unsigned const t = (unsigned)(u[i] * v[j]) + w[i + j] + k;
            w[i + j] = (uint8_t)(t % b);
            k = t / b;
            assert(k <= b && "k will always be in the range 0 <= k < b");
        }
        w[j + m] = (uint8_t)k;
    }

    W.sign = lhs->sign * rhs->sign;
    sunder__bigint_normalize_(&W);
    bigint_assign(res, &W);
    sunder__bigint_fini_(&W);
}

void
bigint_divrem(
    struct bigint* res,
    struct bigint* rem,
    struct bigint const* lhs,
    struct bigint const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    // lhs / 0 == undefined
    if (rhs->sign == 0) {
        fatal(NULL, "[%s] Divide by zero", __func__);
    }

    // Binary Long Division Algorithm
    // Source: https://en.wikipedia.org/wiki/Division_algorithm#Long_division
    //
    // The following algorithm, the binary version of the famous long division,
    // will divide N by D, placing the quotient in Q and the remainder in R. In
    // the following code, all values are treated as unsigned integers.
    //
    // if D = 0 then error(DivisionByZeroException) end
    // Q := 0                  -- Initialize quotient and remainder to zero
    // R := 0
    // for i := n − 1 .. 0 do  -- Where n is number of bits in N
    //   R := R << 1           -- Left-shift R by 1 bit
    //   R(0) := N(i)          -- Set the least-significant bit of R equal to
    //                            bit i of the numerator
    //   if R ≥ D then
    //     R := R − D
    //     Q(i) := 1
    //   end
    // end
    struct bigint Q = {0}; // abs(res)
    struct bigint R = {0}; // abs(rem)
    struct bigint N = {0}; // abs(lhs)
    bigint_abs(&N, lhs);
    struct bigint D = {0}; // abs(rhs)
    bigint_abs(&D, rhs);
    size_t const n = bigint_magnitude_bit_count(lhs);
    for (size_t i = n - 1; i < n; --i) {
        bigint_magnitude_shiftl(&R, 1);
        bigint_magnitude_bit_set(&R, 0, bigint_magnitude_bit_get(&N, i));
        if (bigint_cmp(&R, &D) >= 0) {
            bigint_sub(&R, &R, &D);
            bigint_magnitude_bit_set(&Q, i, 1);
        }
    }

    // printf("%2d %2d\n", +7 / +3, +7 % +3); // 2  1
    // printf("%2d %2d\n", +7 / -3, +7 % -3); //-2  1
    // printf("%2d %2d\n", -7 / +3, -7 % +3); //-2 -1
    // printf("%2d %2d\n", -7 / -3, -7 % -3); // 2 -1
    // ISO-IEC-9899-1999 Section 6.5.5 - Multiplicative operators:
    // > When integers are divided, the result of the / operator is the
    // > algebraic quotient with any fractional part discarded. If the quotient
    // > a/b is representable, the expression (a/b)*b + a%b shall equal a.
    Q.sign = lhs->sign * rhs->sign;
    R.sign = lhs->sign;

    if (res != NULL) {
        sunder__bigint_normalize_(&Q);
        bigint_assign(res, &Q);
    }
    if (rem != NULL) {
        sunder__bigint_normalize_(&R);
        bigint_assign(rem, &R);
    }
    sunder__bigint_fini_(&Q);
    sunder__bigint_fini_(&R);
    sunder__bigint_fini_(&N);
    sunder__bigint_fini_(&D);
}

void
bigint_magnitude_shiftl(struct bigint* self, size_t nbits)
{
    assert(self != NULL);
    if (nbits == 0) {
        return;
    }
    if (self->sign == 0) {
        return;
    }

    sunder__bigint_shiftl_limbs_(self, nbits / SUNDER__BIGINT_LIMB_BITS_);
    for (size_t n = 0; n < nbits % SUNDER__BIGINT_LIMB_BITS_; ++n) {
        if (self->limbs[self->count - 1] & 0x80) {
            self->count += 1;
            self->limbs = xalloc(self->limbs, self->count);
            self->limbs[self->count - 1] = 0x00;
        }
        // [limb0 << 1][limb1 << 1 | msbit(limb0)][limb2 << 1 | msbit(limb1)]...
        for (size_t i = self->count - 1; i > 0; --i) {
            self->limbs[i] = (uint8_t)(self->limbs[i] << 1u);
            if (self->limbs[i - 1] & 0x80) {
                self->limbs[i] |= 0x01;
            }
        }
        self->limbs[0] = (uint8_t)(self->limbs[0] << 1u);
    }
}

void
bigint_magnitude_shiftr(struct bigint* self, size_t nbits)
{
    assert(self != NULL);
    if (nbits == 0) {
        return;
    }

    if (nbits >= bigint_magnitude_bit_count(self)) {
        bigint_assign(self, BIGINT_ZERO);
        return;
    }

    sunder__bigint_shiftr_limbs_(self, nbits / SUNDER__BIGINT_LIMB_BITS_);
    for (size_t n = 0; n < nbits % SUNDER__BIGINT_LIMB_BITS_; ++n) {
        // [limb0 >> 1 | lsbit(limb1)][limb1 >> 1 | lsbit(limb2)]...
        for (size_t i = 0; i < self->count - 1; ++i) {
            self->limbs[i] = (uint8_t)(self->limbs[i] >> 1u);
            if (self->limbs[i + 1] & 0x01) {
                self->limbs[i] |= 0x80;
            }
        }
        self->limbs[self->count - 1] =
            (uint8_t)(self->limbs[self->count - 1] >> 1u);
    }
    sunder__bigint_normalize_(self);
}

size_t
bigint_magnitude_bit_count(struct bigint const* self)
{
    assert(self != NULL);

    if (self->count == 0) {
        return 0;
    }

    uint8_t top = self->limbs[self->count - 1];
    size_t top_bit_count = 0;
    while (top != 0) {
        top_bit_count += 1;
        top = top >> 1;
    }
    return (self->count - 1) * SUNDER__BIGINT_LIMB_BITS_ + top_bit_count;
}

int
bigint_magnitude_bit_get(struct bigint const* self, size_t n)
{
    assert(self != NULL);

    if (n >= (self->count * SUNDER__BIGINT_LIMB_BITS_)) {
        return 0;
    }

    uint8_t const limb = self->limbs[n / SUNDER__BIGINT_LIMB_BITS_];
    uint8_t const mask = (uint8_t)(1u << (n % SUNDER__BIGINT_LIMB_BITS_));
    return (limb & mask) != 0;
}

void
bigint_magnitude_bit_set(struct bigint* self, size_t n, int value)
{
    assert(self != NULL);

    size_t const limb_idx = (n / SUNDER__BIGINT_LIMB_BITS_);
    if (limb_idx >= self->count) {
        if (!value) {
            // The abstact unallocated bit is already zero so re-setting it to
            // zero does not change the representation of self. Return early
            // rather than going through the trouble of resizeing and then
            // normalizing for what is essentially a NOP.
            return;
        }
        sunder__bigint_resize_(self, limb_idx + 1);
    }

    uint8_t* const plimb = self->limbs + limb_idx;
    uint8_t const mask = (uint8_t)(1 << (n % SUNDER__BIGINT_LIMB_BITS_));
    *plimb = (uint8_t)(value ? *plimb | mask : *plimb & ~mask);
    if (self->sign == 0 && value) {
        // If the integer was zero (i.e. had sign zero) before and a bit was
        // just flipped "on" then treat that integer as it if turned from the
        // integer zero to a positive integer.
        self->sign = +1;
    }
    sunder__bigint_normalize_(self);
}

// clang-format off
#define BIGINT_FMT_FLAG_HASH_  ((unsigned)0)
#define BIGINT_FMT_FLAG_ZERO_  ((unsigned)1)
#define BIGINT_FMT_FLAG_PLUS_  ((unsigned)2)
#define BIGINT_FMT_FLAG_MINUS_ ((unsigned)3)
#define BIGINT_FMT_FLAG_SPACE_ ((unsigned)4)
// clang-format on
char*
bigint_to_new_cstr(struct bigint const* self, char const* fmt)
{
    assert(self != NULL);

    // Parse format string.
    unsigned flags = 0;
    size_t width = 0;
    char specifier = 'd';
    if (fmt != NULL) {
        // Flags
        while (*fmt != '\0' && strchr("#0+- ", *fmt) != NULL) {
            flags |= (unsigned)(*fmt == '#') << BIGINT_FMT_FLAG_HASH_;
            flags |= (unsigned)(*fmt == '0') << BIGINT_FMT_FLAG_ZERO_;
            flags |= (unsigned)(*fmt == '+') << BIGINT_FMT_FLAG_PLUS_;
            flags |= (unsigned)(*fmt == '-') << BIGINT_FMT_FLAG_MINUS_;
            flags |= (unsigned)(*fmt == ' ') << BIGINT_FMT_FLAG_SPACE_;
            fmt += 1;
        }
        // Width
        char* fmt_ =
            (char*)fmt; // Needed due to broken const behavior of strtol.
        if (safe_isdigit(*fmt)) {
            width = (size_t)strtol(fmt, &fmt_, 10);
        }
        fmt = fmt_;
        // Specifier
        char const* whichspec = strchr("dboxX", *fmt);
        if (*fmt == '\0' || whichspec == NULL) {
            return NULL;
        }
        specifier = *whichspec;
        fmt += 1;
        // Invalid trailing digits.
        if (*fmt != '\0') {
            return NULL;
        }
        // Match clang/gcc behavior:
        //      "flag '0' is ignored when flag '-' is present"
        if (flags & (1u << BIGINT_FMT_FLAG_MINUS_)) {
            flags &= (unsigned)~(1u << BIGINT_FMT_FLAG_ZERO_);
        }
    }

    // Prefix.
    void* prefix = NULL;
    size_t prefix_size = 0;
    if (self->sign == +1 && (flags & (1u << BIGINT_FMT_FLAG_PLUS_))) {
        xalloc_append(&prefix, &prefix_size, "+", 1);
    }
    if (self->sign == +1 && (flags & (1u << BIGINT_FMT_FLAG_SPACE_))) {
        xalloc_append(&prefix, &prefix_size, " ", 1);
    }
    if (self->sign == -1) {
        xalloc_append(&prefix, &prefix_size, "-", 1);
    }
    if (flags & (1u << BIGINT_FMT_FLAG_HASH_)) {
        if (specifier == 'b') {
            xalloc_append(&prefix, &prefix_size, "0b", 2);
        }
        if (specifier == 'o') {
            xalloc_append(&prefix, &prefix_size, "0o", 2);
        }
        if (specifier == 'x') {
            xalloc_append(&prefix, &prefix_size, "0x", 2);
        }
        if (specifier == 'X') {
            xalloc_append(&prefix, &prefix_size, "0x", 2);
        }
    }

    // Digits.
    void* digits = NULL;
    size_t digits_size = 0;
    char digit_buf[SUNDER__BIGINT_LIMB_BITS_ + STR_LITERAL_COUNT("\0")] = {0};
    if (specifier == 'd') {
        struct bigint DEC = {0};
        struct bigint SELF = {0};
        bigint_abs(&SELF, self);
        while (bigint_cmp(&SELF, BIGINT_ZERO) != 0) {
            bigint_divrem(&SELF, &DEC, &SELF, BIGINT_DEC);
            assert(DEC.count <= 1);
            assert(DEC.limbs == NULL || DEC.limbs[0] < 10);
            sprintf(digit_buf, "%d", DEC.limbs != NULL ? (int)DEC.limbs[0] : 0);
            xalloc_prepend(&digits, &digits_size, digit_buf, strlen(digit_buf));
        }
        sunder__bigint_fini_(&DEC);
        sunder__bigint_fini_(&SELF);
    }
    else if (specifier == 'b') {
        for (size_t i = self->count - 1; i < self->count; --i) {
            sprintf(
                digit_buf,
                "%c%c%c%c%c%c%c%c",
                ((int)self->limbs[i] >> 7) & 0x01 ? '1' : '0',
                ((int)self->limbs[i] >> 6) & 0x01 ? '1' : '0',
                ((int)self->limbs[i] >> 5) & 0x01 ? '1' : '0',
                ((int)self->limbs[i] >> 4) & 0x01 ? '1' : '0',
                ((int)self->limbs[i] >> 3) & 0x01 ? '1' : '0',
                ((int)self->limbs[i] >> 2) & 0x01 ? '1' : '0',
                ((int)self->limbs[i] >> 1) & 0x01 ? '1' : '0',
                ((int)self->limbs[i] >> 0) & 0x01 ? '1' : '0');
            xalloc_append(&digits, &digits_size, digit_buf, strlen(digit_buf));
        }
    }
    else if (specifier == 'o') {
        struct bigint OCT = {0};
        struct bigint SELF = {0};
        bigint_abs(&SELF, self);
        while (bigint_cmp(&SELF, BIGINT_ZERO) != 0) {
            bigint_divrem(&SELF, &OCT, &SELF, BIGINT_OCT);
            assert(OCT.count <= 1);
            assert(OCT.limbs == NULL || OCT.limbs[0] < 8);
            sprintf(digit_buf, "%o", OCT.limbs != NULL ? (int)OCT.limbs[0] : 0);
            xalloc_prepend(&digits, &digits_size, digit_buf, strlen(digit_buf));
        }
        sunder__bigint_fini_(&OCT);
        sunder__bigint_fini_(&SELF);
    }
    else if (specifier == 'x') {
        for (size_t i = self->count - 1; i < self->count; --i) {
            sprintf(digit_buf, "%02x", (int)self->limbs[i]);
            xalloc_append(&digits, &digits_size, digit_buf, strlen(digit_buf));
        }
    }
    else if (specifier == 'X') {
        for (size_t i = self->count - 1; i < self->count; --i) {
            sprintf(digit_buf, "%02X", (int)self->limbs[i]);
            xalloc_append(&digits, &digits_size, digit_buf, strlen(digit_buf));
        }
    }
    else {
        fatal(NULL, "Unreachable!");
    }

    if (digits_size != 0) {
        // Remove leading zeros.
        size_t z = 0;
        while (z < digits_size && (((char*)digits)[z] == '0')) {
            z += 1;
        }
        digits_size -= z;
        memmove(digits, (char*)digits + z, digits_size);
    }
    else {
        // The number zero contains one digit - zero.
        xalloc_append(&digits, &digits_size, "0", 1);
    }

    // Width.
    void* widths = NULL;
    size_t widths_size = 0;
    if ((prefix_size + digits_size) < width) {
        char pad = ' ';
        if (flags & (1u << BIGINT_FMT_FLAG_ZERO_)) {
            assert(!(flags & (1u << BIGINT_FMT_FLAG_MINUS_)));
            pad = '0';
        }
        widths_size = width - (prefix_size + digits_size);
        widths = xalloc(widths, widths_size);
        memset(widths, pad, widths_size);

        if (flags & (1u << BIGINT_FMT_FLAG_ZERO_)) {
            assert(!(flags & (1u << BIGINT_FMT_FLAG_MINUS_)));
            xalloc_prepend(&digits, &digits_size, widths, widths_size);
        }
        else if (flags & (1u << BIGINT_FMT_FLAG_MINUS_)) {
            assert(!(flags & (1u << BIGINT_FMT_FLAG_ZERO_)));
            xalloc_append(&digits, &digits_size, widths, widths_size);
        }
        else {
            xalloc_prepend(&prefix, &prefix_size, widths, widths_size);
        }
    }

    void* cstr = NULL;
    size_t cstr_size = 0;
    xalloc_append(&cstr, &cstr_size, prefix, prefix_size);
    xalloc_append(&cstr, &cstr_size, digits, digits_size);
    xalloc_append(&cstr, &cstr_size, "\0", 1);
    xalloc(prefix, XALLOC_FREE);
    xalloc(digits, XALLOC_FREE);
    xalloc(widths, XALLOC_FREE);
    return cstr;
}

// Byte string with guaranteed NUL termination.
struct string {
    char* start;
    size_t count;
};
#define SUNDER_STRING_SIZE_(count_) (count_ + STR_LITERAL_COUNT("\0"))

struct string*
string_new(char const* start, size_t count)
{
    assert(start != NULL || count == 0);

    struct string* const self = xalloc(NULL, sizeof(struct string));

    self->start = xalloc(NULL, SUNDER_STRING_SIZE_(count));
    self->count = count;

    if (start != NULL) {
        memcpy(self->start, start, count);
    }
    self->start[self->count] = '\0';

    return self;
}

struct string*
string_new_cstr(char const* cstr)
{
    if (cstr == NULL) {
        cstr = "";
    }
    return string_new(cstr, strlen(cstr));
}

struct string*
string_new_fmt(char const* fmt, ...)
{
    assert(fmt != NULL);

    struct string* const self = string_new(NULL, 0);

    va_list args;
    va_start(args, fmt);
    string_append_vfmt(self, fmt, args);
    va_end(args);

    return self;
}

void
string_del(struct string* self)
{
    if (self == NULL) {
        return;
    }

    xalloc(self->start, XALLOC_FREE);
    memset(self, 0x00, sizeof(*self)); // scrub
    xalloc(self, XALLOC_FREE);
}

void
string_freeze(struct string* self, struct freezer* freezer)
{
    assert(self != NULL);
    assert(freezer != NULL);

    freezer_register(freezer, self);
    freezer_register(freezer, self->start);
}

char const*
string_start(struct string const* self)
{
    assert(self != NULL);

    return self->start;
}

size_t
string_count(struct string const* self)
{
    assert(self != NULL);

    return self->count;
}

int
string_cmp(struct string const* lhs, struct string const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    size_t const n = lhs->count < rhs->count ? lhs->count : rhs->count;
    int const cmp = safe_memcmp(lhs->start, rhs->start, n);

    if (cmp != 0 || lhs->count == rhs->count) {
        return cmp;
    }
    return lhs->count < rhs->count ? -1 : +1;
}

void
string_resize(struct string* self, size_t count)
{
    assert(self != NULL);

    if (count > self->count) {
        self->start = xalloc(self->start, SUNDER_STRING_SIZE_(count));
        char* const fill_start = self->start + SUNDER_STRING_SIZE_(self->count);
        size_t const fill_count = count - self->count;
        memset(fill_start, 0x00, fill_count); // Fill new space with NULs.
    }
    self->count = count;
    self->start[self->count] = '\0';
}

char*
string_ref(struct string* self, size_t idx)
{
    assert(self != NULL);

    if (idx >= self->count) {
        fatal(NULL, "[%s] Index out of bounds (%zu)", __func__, idx);
    }
    return &self->start[idx];
}

char const*
string_ref_const(struct string const* self, size_t idx)
{
    assert(self != NULL);

    if (idx >= self->count) {
        fatal(NULL, "[%s] Index out of bounds (%zu)", __func__, idx);
    }
    return &self->start[idx];
}

void
string_insert(struct string* self, size_t idx, char const* start, size_t count)
{
    assert(self != NULL);
    assert(start != NULL || count == 0);
    if (idx > self->count) {
        fatal(NULL, "[%s] Invalid index %zu", __func__, idx);
    }

    if (count == 0) {
        return;
    }
    size_t const mov_count = self->count - idx;
    string_resize(self, self->count + count);
    memmove(self->start + idx + count, self->start + idx, mov_count);
    memmove(self->start + idx, start, count);
}

void
string_remove(struct string* self, size_t idx, size_t count)
{
    assert(self != NULL);
    if ((idx + count) > self->count) {
        fatal(NULL, "[%s] Invalid index,count %zu,%zu", __func__, idx, count);
    }

    if (count == 0) {
        return;
    }
    memmove(self->start + idx, self->start + idx + count, self->count - count);
    string_resize(self, self->count - count);
}

void
string_append(struct string* self, char const* start, size_t count)
{
    assert(self != NULL);

    string_insert(self, string_count(self), start, count);
}

void
string_append_cstr(struct string* self, char const* cstr)
{
    assert(self != NULL);

    string_insert(self, string_count(self), cstr, strlen(cstr));
}

void
string_append_fmt(struct string* self, char const* fmt, ...)
{
    assert(self != NULL);
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);
    string_append_vfmt(self, fmt, args);
    va_end(args);
}

void
string_append_vfmt(struct string* self, char const* fmt, va_list args)
{
    assert(self != NULL);
    assert(fmt != NULL);

    va_list copy;
    va_copy(copy, args);
    int const len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (len < 0) {
        fatal(NULL, "[%s] Formatting failure", __func__);
    }

    size_t size = (size_t)len + STR_LITERAL_COUNT("\0");
    char* const buf = xalloc(NULL, size);
    vsnprintf(buf, size, fmt, args);
    string_append(self, buf, (size_t)len);
    xalloc(buf, XALLOC_FREE);
}

struct string**
string_split_on(
    struct string const* self, char const* separator, size_t separator_size)
{
    assert(self != NULL);
    sbuf(struct string*) res = NULL;

    if (separator_size == 0) {
        sbuf_push(res, string_new(self->start, self->count));
        return res;
    }

    char const* const end_of_string = self->start + self->count;
    char const* beg = self->start;
    char const* end = beg;
    while ((size_t)(end_of_string - end) >= separator_size) {
        if (memcmp(end, separator, separator_size) != 0) {
            end += 1;
            continue;
        }
        sbuf_push(res, string_new(beg, (size_t)(end - beg)));
        beg = end + separator_size;
        end = beg;
    }
    sbuf_push(res, string_new(beg, (size_t)(end - beg)));
    return res;
}

struct freezer {
    // List of heap-allocated pointers to be freed when objects are cleaned out
    // of the freezer.
    sbuf(void*) ptrs;
};

struct freezer*
freezer_new(void)
{
    struct freezer* const self = xalloc(NULL, sizeof(struct sipool));
    self->ptrs = NULL;
    return self;
}

void
freezer_del(struct freezer* self)
{
    if (self == NULL) {
        return;
    }

    for (size_t i = 0; i < sbuf_count(self->ptrs); ++i) {
        xalloc(self->ptrs[i], XALLOC_FREE);
    }
    sbuf_fini(self->ptrs);

    xalloc(self, XALLOC_FREE);
}

void
freezer_register(struct freezer* self, void* ptr)
{
    assert(self != NULL);

    sbuf_push(self->ptrs, ptr);
}
