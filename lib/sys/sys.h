#define const /* nothing */
#define _GNU_SOURCE /* getddents64 */
#include <alloca.h> /* alloca */
#include <ctype.h> /* isdigit */
#include <dirent.h> /* getdents64 */
#include <errno.h> /* errno */
#include <fcntl.h> /* open */
#include <float.h> /* DBL_DECIMAL_DIG, FLT_DECIMAL_DIG */
#include <limits.h> /* CHAR_BIT, *_MIN, *_MAX */
#include <math.h> /* isfinite */
#include <stdint.h> /* uintptr_t */
#include <stdio.h> /* EOF, fprintf, sscanf */
#include <string.h> /* memset */
#include <sys/mman.h> /* mmap, munmap */
#include <sys/stat.h> /* mkdir */
#include <sys/types.h> /* mode_t, off_t, size_t, ssize_t */
#include <unistd.h> /* close, _exit, lseek, read, rmdir, write, unlink */

_Static_assert(CHAR_BIT == 8, "8-bit byte");

#ifndef __STDC_IEC_559__
#    error "IEEE-754 floating point is not fully supported"
#endif

// clang-format off
typedef void               __sunder_void;
typedef _Bool              __sunder_bool;
typedef          char      __sunder_byte;
typedef unsigned char      __sunder_u8;
typedef   signed char      __sunder_s8;
typedef unsigned short     __sunder_u16;
typedef   signed short     __sunder_s16;
typedef unsigned int       __sunder_u32;
typedef   signed int       __sunder_s32;
typedef unsigned long long __sunder_u64;
typedef   signed long long __sunder_s64;
typedef unsigned long      __sunder_usize;
typedef   signed long      __sunder_ssize;
typedef float              __sunder_f32;
typedef double             __sunder_f64;

#define __sunder_true  ((_Bool)1)
#define __sunder_false ((_Bool)0)

#define __sunder_u8___MIN    ((__sunder_u8)0)
#define __sunder_u8___MAX    ((__sunder_u8)UCHAR_MAX)
#define __sunder_s8___MIN    ((__sunder_s8)SCHAR_MIN)
#define __sunder_s8___MAX    ((__sunder_s8)SCHAR_MAX)
#define __sunder_u16___MIN   ((__sunder_u16)0)
#define __sunder_u16___MAX   ((__sunder_u16)USHRT_MAX)
#define __sunder_s16___MIN   ((__sunder_s16)SHRT_MIN)
#define __sunder_s16___MAX   ((__sunder_s16)SHRT_MAX)
#define __sunder_u32___MIN   ((__sunder_u32)0)
#define __sunder_u32___MAX   ((__sunder_u32)UINT_MAX)
#define __sunder_s32___MIN   ((__sunder_s32)INT_MIN)
#define __sunder_s32___MAX   ((__sunder_s32)INT_MAX)
#define __sunder_u64___MIN   ((__sunder_u64)0)
#define __sunder_u64___MAX   ((__sunder_u64)ULLONG_MAX)
#define __sunder_s64___MIN   ((__sunder_s64)LLONG_MIN)
#define __sunder_s64___MAX   ((__sunder_s64)LLONG_MAX)
#define __sunder_usize___MIN ((__sunder_usize)0)
#define __sunder_usize___MAX ((__sunder_usize)ULONG_MAX)
#define __sunder_ssize___MIN ((__sunder_ssize)LONG_MIN)
#define __sunder_ssize___MAX ((__sunder_ssize)LONG_MAX)
// clang-format on

static inline _Noreturn void
__sunder___fatal(char* message)
{
    fprintf(stderr, "fatal: %s\n", message);
    _exit(1);
}

static _Noreturn void
__sunder___fatal_divide_by_zero(void)
{
    __sunder___fatal("divide by zero");
}

static _Noreturn void
__sunder___fatal_index_out_of_bounds(void)
{
    __sunder___fatal("index out-of-bounds");
}

static _Noreturn void
__sunder___fatal_out_of_range(void)
{
    __sunder___fatal("operation produces out-of-range result");
}

#define __SUNDER_INTEGER_ADD_DEFINITION(T)                                     \
    static T __sunder___add_##T(T lhs, T rhs)                                  \
    {                                                                          \
        T result;                                                              \
        if (__builtin_add_overflow(lhs, rhs, &result)) {                       \
            __sunder___fatal_out_of_range();                                   \
        }                                                                      \
        return result;                                                         \
    }

__SUNDER_INTEGER_ADD_DEFINITION(__sunder_u8)
__SUNDER_INTEGER_ADD_DEFINITION(__sunder_s8)
__SUNDER_INTEGER_ADD_DEFINITION(__sunder_u16)
__SUNDER_INTEGER_ADD_DEFINITION(__sunder_s16)
__SUNDER_INTEGER_ADD_DEFINITION(__sunder_u32)
__SUNDER_INTEGER_ADD_DEFINITION(__sunder_s32)
__SUNDER_INTEGER_ADD_DEFINITION(__sunder_u64)
__SUNDER_INTEGER_ADD_DEFINITION(__sunder_s64)
__SUNDER_INTEGER_ADD_DEFINITION(__sunder_usize)
__SUNDER_INTEGER_ADD_DEFINITION(__sunder_ssize)

#define __SUNDER_INTEGER_ADD_WRAPPING_DEFINITION(T)                            \
    static T __sunder___add_wrapping_##T(T lhs, T rhs)                         \
    {                                                                          \
        T result;                                                              \
        __builtin_add_overflow(lhs, rhs, &result);                             \
        return result;                                                         \
    }

__SUNDER_INTEGER_ADD_WRAPPING_DEFINITION(__sunder_u8)
__SUNDER_INTEGER_ADD_WRAPPING_DEFINITION(__sunder_s8)
__SUNDER_INTEGER_ADD_WRAPPING_DEFINITION(__sunder_u16)
__SUNDER_INTEGER_ADD_WRAPPING_DEFINITION(__sunder_s16)
__SUNDER_INTEGER_ADD_WRAPPING_DEFINITION(__sunder_u32)
__SUNDER_INTEGER_ADD_WRAPPING_DEFINITION(__sunder_s32)
__SUNDER_INTEGER_ADD_WRAPPING_DEFINITION(__sunder_u64)
__SUNDER_INTEGER_ADD_WRAPPING_DEFINITION(__sunder_s64)
__SUNDER_INTEGER_ADD_WRAPPING_DEFINITION(__sunder_usize)
__SUNDER_INTEGER_ADD_WRAPPING_DEFINITION(__sunder_ssize)

#define __SUNDER_INTEGER_SUB_DEFINITION(T)                                     \
    static T __sunder___sub_##T(T lhs, T rhs)                                  \
    {                                                                          \
        T result;                                                              \
        if (__builtin_sub_overflow(lhs, rhs, &result)) {                       \
            __sunder___fatal_out_of_range();                                   \
        }                                                                      \
        return result;                                                         \
    }

__SUNDER_INTEGER_SUB_DEFINITION(__sunder_u8)
__SUNDER_INTEGER_SUB_DEFINITION(__sunder_s8)
__SUNDER_INTEGER_SUB_DEFINITION(__sunder_u16)
__SUNDER_INTEGER_SUB_DEFINITION(__sunder_s16)
__SUNDER_INTEGER_SUB_DEFINITION(__sunder_u32)
__SUNDER_INTEGER_SUB_DEFINITION(__sunder_s32)
__SUNDER_INTEGER_SUB_DEFINITION(__sunder_u64)
__SUNDER_INTEGER_SUB_DEFINITION(__sunder_s64)
__SUNDER_INTEGER_SUB_DEFINITION(__sunder_usize)
__SUNDER_INTEGER_SUB_DEFINITION(__sunder_ssize)

#define __SUNDER_INTEGER_SUB_WRAPPING_DEFINITION(T)                            \
    static T __sunder___sub_wrapping_##T(T lhs, T rhs)                         \
    {                                                                          \
        T result;                                                              \
        __builtin_sub_overflow(lhs, rhs, &result);                             \
        return result;                                                         \
    }

__SUNDER_INTEGER_SUB_WRAPPING_DEFINITION(__sunder_u8)
__SUNDER_INTEGER_SUB_WRAPPING_DEFINITION(__sunder_s8)
__SUNDER_INTEGER_SUB_WRAPPING_DEFINITION(__sunder_u16)
__SUNDER_INTEGER_SUB_WRAPPING_DEFINITION(__sunder_s16)
__SUNDER_INTEGER_SUB_WRAPPING_DEFINITION(__sunder_u32)
__SUNDER_INTEGER_SUB_WRAPPING_DEFINITION(__sunder_s32)
__SUNDER_INTEGER_SUB_WRAPPING_DEFINITION(__sunder_u64)
__SUNDER_INTEGER_SUB_WRAPPING_DEFINITION(__sunder_s64)
__SUNDER_INTEGER_SUB_WRAPPING_DEFINITION(__sunder_usize)
__SUNDER_INTEGER_SUB_WRAPPING_DEFINITION(__sunder_ssize)

#define __SUNDER_INTEGER_MUL_DEFINITION(T)                                     \
    static T __sunder___mul_##T(T lhs, T rhs)                                  \
    {                                                                          \
        T result;                                                              \
        if (__builtin_mul_overflow(lhs, rhs, &result)) {                       \
            __sunder___fatal_out_of_range();                                   \
        }                                                                      \
        return result;                                                         \
    }

__SUNDER_INTEGER_MUL_DEFINITION(__sunder_u8)
__SUNDER_INTEGER_MUL_DEFINITION(__sunder_s8)
__SUNDER_INTEGER_MUL_DEFINITION(__sunder_u16)
__SUNDER_INTEGER_MUL_DEFINITION(__sunder_s16)
__SUNDER_INTEGER_MUL_DEFINITION(__sunder_u32)
__SUNDER_INTEGER_MUL_DEFINITION(__sunder_s32)
__SUNDER_INTEGER_MUL_DEFINITION(__sunder_u64)
__SUNDER_INTEGER_MUL_DEFINITION(__sunder_s64)
__SUNDER_INTEGER_MUL_DEFINITION(__sunder_usize)
__SUNDER_INTEGER_MUL_DEFINITION(__sunder_ssize)

#define __SUNDER_INTEGER_MUL_WRAPPING_DEFINITION(T)                            \
    static T __sunder___mul_wrapping_##T(T lhs, T rhs)                         \
    {                                                                          \
        T result;                                                              \
        __builtin_mul_overflow(lhs, rhs, &result);                             \
        return result;                                                         \
    }

__SUNDER_INTEGER_MUL_WRAPPING_DEFINITION(__sunder_u8)
__SUNDER_INTEGER_MUL_WRAPPING_DEFINITION(__sunder_s8)
__SUNDER_INTEGER_MUL_WRAPPING_DEFINITION(__sunder_u16)
__SUNDER_INTEGER_MUL_WRAPPING_DEFINITION(__sunder_s16)
__SUNDER_INTEGER_MUL_WRAPPING_DEFINITION(__sunder_u32)
__SUNDER_INTEGER_MUL_WRAPPING_DEFINITION(__sunder_s32)
__SUNDER_INTEGER_MUL_WRAPPING_DEFINITION(__sunder_u64)
__SUNDER_INTEGER_MUL_WRAPPING_DEFINITION(__sunder_s64)
__SUNDER_INTEGER_MUL_WRAPPING_DEFINITION(__sunder_usize)
__SUNDER_INTEGER_MUL_WRAPPING_DEFINITION(__sunder_ssize)

#define __SUNDER_DIV_DEFINITION(T)                                             \
    static T __sunder___div_##T(T lhs, T rhs)                                  \
    {                                                                          \
        if (rhs == 0) {                                                        \
            __sunder___fatal_divide_by_zero();                                 \
        }                                                                      \
        return lhs / rhs;                                                      \
    }

__SUNDER_DIV_DEFINITION(__sunder_u8)
__SUNDER_DIV_DEFINITION(__sunder_s8)
__SUNDER_DIV_DEFINITION(__sunder_u16)
__SUNDER_DIV_DEFINITION(__sunder_s16)
__SUNDER_DIV_DEFINITION(__sunder_u32)
__SUNDER_DIV_DEFINITION(__sunder_s32)
__SUNDER_DIV_DEFINITION(__sunder_u64)
__SUNDER_DIV_DEFINITION(__sunder_s64)
__SUNDER_DIV_DEFINITION(__sunder_usize)
__SUNDER_DIV_DEFINITION(__sunder_ssize)
__SUNDER_DIV_DEFINITION(__sunder_f32)
__SUNDER_DIV_DEFINITION(__sunder_f64)

#define __SUNDER_INTEGER_REM_DEFINITION(T)                                     \
    static T __sunder___rem_##T(T lhs, T rhs)                                  \
    {                                                                          \
        if (rhs == 0) {                                                        \
            __sunder___fatal_divide_by_zero();                                 \
        }                                                                      \
        return lhs % rhs;                                                      \
    }

__SUNDER_INTEGER_REM_DEFINITION(__sunder_u8)
__SUNDER_INTEGER_REM_DEFINITION(__sunder_s8)
__SUNDER_INTEGER_REM_DEFINITION(__sunder_u16)
__SUNDER_INTEGER_REM_DEFINITION(__sunder_s16)
__SUNDER_INTEGER_REM_DEFINITION(__sunder_u32)
__SUNDER_INTEGER_REM_DEFINITION(__sunder_s32)
__SUNDER_INTEGER_REM_DEFINITION(__sunder_u64)
__SUNDER_INTEGER_REM_DEFINITION(__sunder_s64)
__SUNDER_INTEGER_REM_DEFINITION(__sunder_usize)
__SUNDER_INTEGER_REM_DEFINITION(__sunder_ssize)

#define __SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(F, I)                      \
    static I __sunder___cast_##F##_to_##I(F f)                                 \
    {                                                                          \
        if (!isfinite(f) || f < (F)I##___MIN || (F)I##___MAX < f) {            \
            __sunder___fatal_out_of_range();                                   \
        }                                                                      \
        return (I)f;                                                           \
    }

__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f32, __sunder_u8)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f32, __sunder_s8)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f32, __sunder_u16)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f32, __sunder_s16)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f32, __sunder_u32)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f32, __sunder_s32)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f32, __sunder_u64)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f32, __sunder_s64)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f32, __sunder_usize)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f32, __sunder_ssize)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f64, __sunder_u8)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f64, __sunder_s8)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f64, __sunder_u16)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f64, __sunder_s16)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f64, __sunder_u32)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f64, __sunder_s32)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f64, __sunder_u64)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f64, __sunder_s64)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f64, __sunder_usize)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(__sunder_f64, __sunder_ssize)

static __sunder_ssize
sys_read(signed int fd, __sunder_byte* buf, size_t count)
{
    ssize_t result = read(fd, buf, count);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
sys_write(signed int fd, __sunder_byte* buf, size_t count)
{
    ssize_t result = write(fd, buf, count);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
sys_open(__sunder_byte* filename, signed int flags, mode_t mode)
{
    int result = open(filename, flags, mode);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
sys_close(signed int fd)
{
    int result = close(fd);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
sys_lseek(signed int fd, off_t offset, unsigned int whence)
{
    off_t result = lseek(fd, offset, whence);
    if (result == (off_t)-1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
sys_mmap(
    void* addr,
    size_t len,
    signed int prot,
    signed int flags,
    signed int fd,
    off_t off)
{
    void* result = mmap(addr, len, prot, flags, fd, off);
    if (result == (void*)-1) {
        return -errno;
    }
    return (__sunder_ssize)result;
}

static __sunder_ssize
sys_munmap(void* addr, size_t len)
{
    int result = munmap(addr, len);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static void
sys_exit(signed int error_code)
{
    _exit(error_code);
}

struct __sunder_sys_dirent64;
static __sunder_ssize
sys_getdents64(
    signed int fd, struct __sunder_sys_dirent64* dirent, unsigned int count)
{
    ssize_t result = getdents64(fd, dirent, count);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
sys_mkdir(__sunder_byte* pathname, mode_t mode)
{
    int result = mkdir(pathname, mode);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
sys_rmdir(__sunder_byte* pathname)
{
    int result = rmdir(pathname);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
sys_unlink(__sunder_byte* pathname)
{
    int result = unlink(pathname);
    if (result == -1) {
        return -errno;
    }
    return result;
}

__sunder_usize sys_argc;
__sunder_byte** sys_argv;
__sunder_byte** sys_envp;

// clang-format off
__attribute__((section("rodata")))
static char sys_dump_bytes_lookup_table[256u * 2u] = {
    '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0',
    '7', '0', '8', '0', '9', '0', 'A', '0', 'B', '0', 'C', '0', 'D', '0', 'E',
    '0', 'F', '1', '0', '1', '1', '1', '2', '1', '3', '1', '4', '1', '5', '1',
    '6', '1', '7', '1', '8', '1', '9', '1', 'A', '1', 'B', '1', 'C', '1', 'D',
    '1', 'E', '1', 'F', '2', '0', '2', '1', '2', '2', '2', '3', '2', '4', '2',
    '5', '2', '6', '2', '7', '2', '8', '2', '9', '2', 'A', '2', 'B', '2', 'C',
    '2', 'D', '2', 'E', '2', 'F', '3', '0', '3', '1', '3', '2', '3', '3', '3',
    '4', '3', '5', '3', '6', '3', '7', '3', '8', '3', '9', '3', 'A', '3', 'B',
    '3', 'C', '3', 'D', '3', 'E', '3', 'F', '4', '0', '4', '1', '4', '2', '4',
    '3', '4', '4', '4', '5', '4', '6', '4', '7', '4', '8', '4', '9', '4', 'A',
    '4', 'B', '4', 'C', '4', 'D', '4', 'E', '4', 'F', '5', '0', '5', '1', '5',
    '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
    '5', 'A', '5', 'B', '5', 'C', '5', 'D', '5', 'E', '5', 'F', '6', '0', '6',
    '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7', '6', '8',
    '6', '9', '6', 'A', '6', 'B', '6', 'C', '6', 'D', '6', 'E', '6', 'F', '7',
    '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7',
    '7', '8', '7', '9', '7', 'A', '7', 'B', '7', 'C', '7', 'D', '7', 'E', '7',
    'F', '8', '0', '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6',
    '8', '7', '8', '8', '8', '9', '8', 'A', '8', 'B', '8', 'C', '8', 'D', '8',
    'E', '8', 'F', '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5',
    '9', '6', '9', '7', '9', '8', '9', '9', '9', 'A', '9', 'B', '9', 'C', '9',
    'D', '9', 'E', '9', 'F', 'A', '0', 'A', '1', 'A', '2', 'A', '3', 'A', '4',
    'A', '5', 'A', '6', 'A', '7', 'A', '8', 'A', '9', 'A', 'A', 'A', 'B', 'A',
    'C', 'A', 'D', 'A', 'E', 'A', 'F', 'B', '0', 'B', '1', 'B', '2', 'B', '3',
    'B', '4', 'B', '5', 'B', '6', 'B', '7', 'B', '8', 'B', '9', 'B', 'A', 'B',
    'B', 'B', 'C', 'B', 'D', 'B', 'E', 'B', 'F', 'C', '0', 'C', '1', 'C', '2',
    'C', '3', 'C', '4', 'C', '5', 'C', '6', 'C', '7', 'C', '8', 'C', '9', 'C',
    'A', 'C', 'B', 'C', 'C', 'C', 'D', 'C', 'E', 'C', 'F', 'D', '0', 'D', '1',
    'D', '2', 'D', '3', 'D', '4', 'D', '5', 'D', '6', 'D', '7', 'D', '8', 'D',
    '9', 'D', 'A', 'D', 'B', 'D', 'C', 'D', 'D', 'D', 'E', 'D', 'F', 'E', '0',
    'E', '1', 'E', '2', 'E', '3', 'E', '4', 'E', '5', 'E', '6', 'E', '7', 'E',
    '8', 'E', '9', 'E', 'A', 'E', 'B', 'E', 'C', 'E', 'D', 'E', 'E', 'E', 'F',
    'F', '0', 'F', '1', 'F', '2', 'F', '3', 'F', '4', 'F', '5', 'F', '6', 'F',
    '7', 'F', '8', 'F', '9', 'F', 'A', 'F', 'B', 'F', 'C', 'F', 'D', 'F', 'E',
    'F', 'F',
};
// clang-format on

__sunder_void
sys_dump_bytes(void* addr, __sunder_usize size)
{
    if (size == 0) {
        fputc('\n', stderr);
        return;
    }

    // Allocate a buffer large enough to hold size number of three-byte
    // triples. For each three-byte triple, x, bytes x[0:1] will contain the
    // two-byte hex representation of a single byte of obj, and byte x[2] will
    // contain a whitespace separator (either ' ' or '\n').
    char* buf = alloca(size * 3u); // Locally allocated buffer.
    char* ptr = buf; // Write pointer into the locally allocated buffer.

    unsigned char* cur = addr;
    unsigned char* end = (unsigned char*)addr + size;
    while (cur != end) {
        char* repr = sys_dump_bytes_lookup_table + (*cur * 2u);
        ptr[0u] = repr[0u];
        ptr[1u] = repr[1u];
        ptr[2u] = ' ';
        ptr += 3u;
        cur += 1u;
    }

    ptr[-1] = '\n';
    fprintf(stderr, "%.*s", (int)(size * 3u), buf);
}

__sunder_bool
sys_str_to_f32(__sunder_f32* out, __sunder_byte* start, __sunder_usize count)
{
    char* buf = alloca(count + 1);
    for (size_t i = 0; i < count; ++i) {
        __sunder_bool valid_character = isdigit((unsigned char)start[i])
            || start[i] == '.' || start[i] == '+' || start[i] == '-';
        if (!valid_character) {
            return __sunder_false;
        }
        buf[i] = start[i];
    }
    buf[count] = '\0';

    float f;
    int scanned = sscanf(buf, "%f", &f);
    if (scanned == EOF) {
        return __sunder_false;
    }

    *out = f;
    return __sunder_true;
}

__sunder_bool
sys_str_to_f64(__sunder_f64* out, __sunder_byte* start, __sunder_usize count)
{
    char* buf = alloca(count + 1);
    for (size_t i = 0; i < count; ++i) {
        __sunder_bool valid_character = isdigit((unsigned char)start[i])
            || start[i] == '.' || start[i] == '+' || start[i] == '-';
        if (!valid_character) {
            return __sunder_false;
        }
        buf[i] = start[i];
    }
    buf[count] = '\0';

    double f;
    int scanned = sscanf(buf, "%lf", &f);
    if (scanned == EOF) {
        return __sunder_false;
    }

    *out = f;
    return __sunder_true;
}

__sunder_bool
sys_f32_to_str(__sunder_byte* buf, __sunder_f32 f)
{
    return sprintf(buf, "%.*f", FLT_DECIMAL_DIG, (double)f) >= 0;
}

__sunder_bool
sys_f64_to_str(__sunder_byte* buf, __sunder_f64 f)
{
    return sprintf(buf, "%.*f", DBL_DECIMAL_DIG, f) >= 0;
}
