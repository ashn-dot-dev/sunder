#define const /* nothing */
#define restrict /* nothing */
#define _ISOC11_SOURCE /* aligned_alloc */
#include <assert.h> /* assert */
#include <ctype.h> /* isdigit */
#include <dirent.h> /* opendir, closedir, readdir */
#include <errno.h> /* errno, perror */
#include <fcntl.h> /* open */
#include <limits.h> /* CHAR_BIT, *_MIN, *_MAX */
#include <math.h> /* INFINITY, NAN, isfinite, isinf, isnan, math functions */
#include <stdbool.h> /* bool */
#include <stdint.h> /* uintptr_t */
#include <stdio.h> /* EOF, fprintf, sscanf */
#include <stdlib.h> /* aligned_alloc, free */
#include <string.h> /* memset, memcmp, strlen */
#include <sys/stat.h> /* mkdir */
#include <sys/types.h> /* mode_t, off_t, size_t, ssize_t */
#include <unistd.h> /* close, _exit, lseek, read, rmdir, write, unlink */
#undef const
#undef restrict

_Static_assert(CHAR_BIT == 8, "8-bit byte");

// clang-format off
/* void defined as a builtin */
/* bool defined in stdbool.h */
typedef          char      byte;
typedef unsigned char      u8;
typedef   signed char      s8;
typedef unsigned short     u16;
typedef   signed short     s16;
typedef unsigned int       u32;
typedef   signed int       s32;
typedef unsigned long long u64;
typedef   signed long long s64;
typedef unsigned long      usize;
typedef   signed long      ssize;
typedef float              f32;
typedef double             f64;

static bool const __sunder_true  = (bool)1;
static bool const __sunder_false = (bool)0;

static u8    const __sunder_u8_MIN    = (u8)0;
static u8    const __sunder_u8_MAX    = (u8)UCHAR_MAX;
static s8    const __sunder_s8_MIN    = (s8)SCHAR_MIN;
static s8    const __sunder_s8_MAX    = (s8)SCHAR_MAX;
static u16   const __sunder_u16_MIN   = (u16)0;
static u16   const __sunder_u16_MAX   = (u16)USHRT_MAX;
static s16   const __sunder_s16_MIN   = (s16)SHRT_MIN;
static s16   const __sunder_s16_MAX   = (s16)SHRT_MAX;
static u32   const __sunder_u32_MIN   = (u32)0;
static u32   const __sunder_u32_MAX   = (u32)UINT_MAX;
static s32   const __sunder_s32_MIN   = (s32)INT_MIN;
static s32   const __sunder_s32_MAX   = (s32)INT_MAX;
static u64   const __sunder_u64_MIN   = (u64)0;
static u64   const __sunder_u64_MAX   = (u64)ULLONG_MAX;
static s64   const __sunder_s64_MIN   = (s64)LLONG_MIN;
static s64   const __sunder_s64_MAX   = (s64)LLONG_MAX;
static usize const __sunder_usize_MIN = (usize)0;
static usize const __sunder_usize_MAX = (usize)ULONG_MAX;
static ssize const __sunder_ssize_MIN = (ssize)LONG_MIN;
static ssize const __sunder_ssize_MAX = (ssize)LONG_MAX;
// clang-format on

#define __SUNDER_IEEE754_FLT_DECIMAL_DIG 9
#define __SUNDER_IEEE754_DBL_DECIMAL_DIG 17

static inline _Noreturn void
__sunder_fatal(char* message)
{
    fprintf(stderr, "%s\n", message);
    _exit(1);
}

static _Noreturn void
__sunder_fatal_divide_by_zero(void)
{
    __sunder_fatal("fatal: divide by zero");
}

static _Noreturn void
__sunder_fatal_index_out_of_bounds(void)
{
    __sunder_fatal("fatal: index out-of-bounds");
}

static _Noreturn void
__sunder_fatal_null_pointer_dereference(void)
{
    __sunder_fatal("fatal: null pointer dereference");
}

static _Noreturn void
__sunder_fatal_out_of_range(void)
{
    __sunder_fatal("fatal: operation produces out-of-range result");
}

#ifdef __GNUC__
#    define __SUNDER_UINTEGER_ADD_DEFINITION(T)                                \
        static T __sunder_add_##T(T lhs, T rhs)                                \
        {                                                                      \
            T result;                                                          \
            if (__builtin_add_overflow(lhs, rhs, &result)) {                   \
                __sunder_fatal_out_of_range();                                 \
            }                                                                  \
            return result;                                                     \
        }
#    define __SUNDER_SINTEGER_ADD_DEFINITION(T)                                \
        __SUNDER_UINTEGER_ADD_DEFINITION(T)
#else
#    define __SUNDER_UINTEGER_ADD_DEFINITION(T)                                \
        static T __sunder_add_##T(T lhs, T rhs)                                \
        {                                                                      \
            if (lhs > __sunder_##T##_MAX - rhs) {                              \
                __sunder_fatal_out_of_range();                                 \
            }                                                                  \
            return lhs + rhs;                                                  \
        }
#    define __SUNDER_SINTEGER_ADD_DEFINITION(T)                                \
        static T __sunder_add_##T(T lhs, T rhs)                                \
        {                                                                      \
            if (lhs > 0 && rhs > __sunder_##T##_MAX - lhs) {                   \
                __sunder_fatal_out_of_range();                                 \
            }                                                                  \
            if (lhs < 0 && rhs < __sunder_##T##_MIN - lhs) {                   \
                __sunder_fatal_out_of_range();                                 \
            }                                                                  \
            return lhs + rhs;                                                  \
        }
#endif

__SUNDER_UINTEGER_ADD_DEFINITION(u8)
__SUNDER_SINTEGER_ADD_DEFINITION(s8)
__SUNDER_UINTEGER_ADD_DEFINITION(u16)
__SUNDER_SINTEGER_ADD_DEFINITION(s16)
__SUNDER_UINTEGER_ADD_DEFINITION(u32)
__SUNDER_SINTEGER_ADD_DEFINITION(s32)
__SUNDER_UINTEGER_ADD_DEFINITION(u64)
__SUNDER_SINTEGER_ADD_DEFINITION(s64)
__SUNDER_UINTEGER_ADD_DEFINITION(usize)
__SUNDER_SINTEGER_ADD_DEFINITION(ssize)

#ifdef __GNUC__
#    define __SUNDER_UINTEGER_ADD_WRAPPING_DEFINITION(T)                       \
        static T __sunder_add_wrapping_##T(T lhs, T rhs)                       \
        {                                                                      \
            T result;                                                          \
            __builtin_add_overflow(lhs, rhs, &result);                         \
            return result;                                                     \
        }
#    define __SUNDER_SINTEGER_ADD_WRAPPING_DEFINITION(T, UT)                   \
        __SUNDER_UINTEGER_ADD_WRAPPING_DEFINITION(T)
#else
#    define __SUNDER_UINTEGER_ADD_WRAPPING_DEFINITION(T)                       \
        static T __sunder_add_wrapping_##T(T lhs, T rhs)                       \
        {                                                                      \
            return lhs + rhs;                                                  \
        }
#    define __SUNDER_SINTEGER_ADD_WRAPPING_DEFINITION(T, UT)                   \
        static T __sunder_add_wrapping_##T(T lhs, T rhs)                       \
        {                                                                      \
            return (T)((UT)lhs + (UT)rhs);                                     \
        }
#endif

__SUNDER_UINTEGER_ADD_WRAPPING_DEFINITION(u8)
__SUNDER_SINTEGER_ADD_WRAPPING_DEFINITION(s8, u8)
__SUNDER_UINTEGER_ADD_WRAPPING_DEFINITION(u16)
__SUNDER_SINTEGER_ADD_WRAPPING_DEFINITION(s16, u16)
__SUNDER_UINTEGER_ADD_WRAPPING_DEFINITION(u32)
__SUNDER_SINTEGER_ADD_WRAPPING_DEFINITION(s32, u32)
__SUNDER_UINTEGER_ADD_WRAPPING_DEFINITION(u64)
__SUNDER_SINTEGER_ADD_WRAPPING_DEFINITION(s64, u64)
__SUNDER_UINTEGER_ADD_WRAPPING_DEFINITION(usize)
__SUNDER_SINTEGER_ADD_WRAPPING_DEFINITION(ssize, usize)

#ifdef __GNUC__
#    define __SUNDER_UINTEGER_SUB_DEFINITION(T)                                \
        static T __sunder_sub_##T(T lhs, T rhs)                                \
        {                                                                      \
            T result;                                                          \
            if (__builtin_sub_overflow(lhs, rhs, &result)) {                   \
                __sunder_fatal_out_of_range();                                 \
            }                                                                  \
            return result;                                                     \
        }
#    define __SUNDER_SINTEGER_SUB_DEFINITION(T)                                \
        __SUNDER_UINTEGER_SUB_DEFINITION(T)
#else
#    define __SUNDER_UINTEGER_SUB_DEFINITION(T)                                \
        static T __sunder_sub_##T(T lhs, T rhs)                                \
        {                                                                      \
            if (lhs < rhs) {                                                   \
                __sunder_fatal_out_of_range();                                 \
            }                                                                  \
            return lhs - rhs;                                                  \
        }
#    define __SUNDER_SINTEGER_SUB_DEFINITION(T)                                \
        static T __sunder_sub_##T(T lhs, T rhs)                                \
        {                                                                      \
            if (lhs < 0 && rhs > __sunder_##T##_MAX + lhs) {                   \
                __sunder_fatal_out_of_range();                                 \
            }                                                                  \
            if (lhs > 0 && rhs < __sunder_##T##_MIN + lhs) {                   \
                __sunder_fatal_out_of_range();                                 \
            }                                                                  \
            return lhs - rhs;                                                  \
        }
#endif

__SUNDER_UINTEGER_SUB_DEFINITION(u8)
__SUNDER_SINTEGER_SUB_DEFINITION(s8)
__SUNDER_UINTEGER_SUB_DEFINITION(u16)
__SUNDER_SINTEGER_SUB_DEFINITION(s16)
__SUNDER_UINTEGER_SUB_DEFINITION(u32)
__SUNDER_SINTEGER_SUB_DEFINITION(s32)
__SUNDER_UINTEGER_SUB_DEFINITION(u64)
__SUNDER_SINTEGER_SUB_DEFINITION(s64)
__SUNDER_UINTEGER_SUB_DEFINITION(usize)
__SUNDER_SINTEGER_SUB_DEFINITION(ssize)

#ifdef __GNUC__
#    define __SUNDER_UINTEGER_SUB_WRAPPING_DEFINITION(T)                       \
        static T __sunder_sub_wrapping_##T(T lhs, T rhs)                       \
        {                                                                      \
            T result;                                                          \
            __builtin_sub_overflow(lhs, rhs, &result);                         \
            return result;                                                     \
        }
#    define __SUNDER_SINTEGER_SUB_WRAPPING_DEFINITION(T, UT)                   \
        __SUNDER_UINTEGER_SUB_WRAPPING_DEFINITION(T)
#else
#    define __SUNDER_UINTEGER_SUB_WRAPPING_DEFINITION(T)                       \
        static T __sunder_sub_wrapping_##T(T lhs, T rhs)                       \
        {                                                                      \
            return lhs - rhs;                                                  \
        }
#    define __SUNDER_SINTEGER_SUB_WRAPPING_DEFINITION(T, UT)                   \
        static T __sunder_sub_wrapping_##T(T lhs, T rhs)                       \
        {                                                                      \
            return (T)((UT)lhs - (UT)rhs);                                     \
        }
#endif

__SUNDER_UINTEGER_SUB_WRAPPING_DEFINITION(u8)
__SUNDER_SINTEGER_SUB_WRAPPING_DEFINITION(s8, u8)
__SUNDER_UINTEGER_SUB_WRAPPING_DEFINITION(u16)
__SUNDER_SINTEGER_SUB_WRAPPING_DEFINITION(s16, u16)
__SUNDER_UINTEGER_SUB_WRAPPING_DEFINITION(u32)
__SUNDER_SINTEGER_SUB_WRAPPING_DEFINITION(s32, u32)
__SUNDER_UINTEGER_SUB_WRAPPING_DEFINITION(u64)
__SUNDER_SINTEGER_SUB_WRAPPING_DEFINITION(s64, u64)
__SUNDER_UINTEGER_SUB_WRAPPING_DEFINITION(usize)
__SUNDER_SINTEGER_SUB_WRAPPING_DEFINITION(ssize, usize)

#ifdef __GNUC__
#    define __SUNDER_UINTEGER_MUL_DEFINITION(T)                                \
        static T __sunder_mul_##T(T lhs, T rhs)                                \
        {                                                                      \
            T result;                                                          \
            if (__builtin_mul_overflow(lhs, rhs, &result)) {                   \
                __sunder_fatal_out_of_range();                                 \
            }                                                                  \
            return result;                                                     \
        }
#    define __SUNDER_SINTEGER_MUL_DEFINITION(T)                                \
        __SUNDER_UINTEGER_MUL_DEFINITION(T)
#else
#    define __SUNDER_UINTEGER_MUL_DEFINITION(T)                                \
        static T __sunder_mul_##T(T lhs, T rhs)                                \
        {                                                                      \
            if (lhs != 0 && rhs > __sunder_##T##_MAX / lhs) {                  \
                __sunder_fatal_out_of_range();                                 \
            }                                                                  \
            return lhs * rhs;                                                  \
        }
#    define __SUNDER_SINTEGER_MUL_DEFINITION(T)                                \
        static T __sunder_mul_##T(T lhs, T rhs)                                \
        {                                                                      \
            if (lhs == -1 && rhs == __sunder_##T##_MIN) {                      \
                __sunder_fatal_out_of_range();                                 \
            }                                                                  \
            if (rhs == -1 && lhs == __sunder_##T##_MIN) {                      \
                __sunder_fatal_out_of_range();                                 \
            }                                                                  \
                                                                               \
            if (lhs != 0 && lhs != -1) {                                       \
                if (lhs > 0 && rhs > __sunder_##T##_MAX / lhs) {               \
                    __sunder_fatal_out_of_range();                             \
                }                                                              \
                if (lhs > 0 && rhs < __sunder_##T##_MIN / lhs) {               \
                    __sunder_fatal_out_of_range();                             \
                }                                                              \
                if (lhs < 0 && rhs < __sunder_##T##_MAX / lhs) {               \
                    __sunder_fatal_out_of_range();                             \
                }                                                              \
                if (lhs < 0 && rhs > __sunder_##T##_MIN / lhs) {               \
                    __sunder_fatal_out_of_range();                             \
                }                                                              \
            }                                                                  \
            return lhs * rhs;                                                  \
        }
#endif

__SUNDER_UINTEGER_MUL_DEFINITION(u8)
__SUNDER_SINTEGER_MUL_DEFINITION(s8)
__SUNDER_UINTEGER_MUL_DEFINITION(u16)
__SUNDER_SINTEGER_MUL_DEFINITION(s16)
__SUNDER_UINTEGER_MUL_DEFINITION(u32)
__SUNDER_SINTEGER_MUL_DEFINITION(s32)
__SUNDER_UINTEGER_MUL_DEFINITION(u64)
__SUNDER_SINTEGER_MUL_DEFINITION(s64)
__SUNDER_UINTEGER_MUL_DEFINITION(usize)
__SUNDER_SINTEGER_MUL_DEFINITION(ssize)

#ifdef __GNUC__
#    define __SUNDER_UINTEGER_MUL_WRAPPING_DEFINITION(T)                       \
        static T __sunder_mul_wrapping_##T(T lhs, T rhs)                       \
        {                                                                      \
            T result;                                                          \
            __builtin_mul_overflow(lhs, rhs, &result);                         \
            return result;                                                     \
        }
#    define __SUNDER_SINTEGER_MUL_WRAPPING_DEFINITION(T, UT)                   \
        __SUNDER_UINTEGER_MUL_WRAPPING_DEFINITION(T)
#else
#    define __SUNDER_UINTEGER_MUL_WRAPPING_DEFINITION(T)                       \
        static T __sunder_mul_wrapping_##T(T lhs, T rhs)                       \
        {                                                                      \
            return lhs * rhs;                                                  \
        }
#    define __SUNDER_SINTEGER_MUL_WRAPPING_DEFINITION(T, UT)                   \
        static T __sunder_mul_wrapping_##T(T lhs, T rhs)                       \
        {                                                                      \
            return (T)((UT)lhs * (UT)rhs);                                     \
        }
#endif

__SUNDER_UINTEGER_MUL_WRAPPING_DEFINITION(u8)
__SUNDER_SINTEGER_MUL_WRAPPING_DEFINITION(s8, u8)
__SUNDER_UINTEGER_MUL_WRAPPING_DEFINITION(u16)
__SUNDER_SINTEGER_MUL_WRAPPING_DEFINITION(s16, u16)
__SUNDER_UINTEGER_MUL_WRAPPING_DEFINITION(u32)
__SUNDER_SINTEGER_MUL_WRAPPING_DEFINITION(s32, u32)
__SUNDER_UINTEGER_MUL_WRAPPING_DEFINITION(u64)
__SUNDER_SINTEGER_MUL_WRAPPING_DEFINITION(s64, u64)
__SUNDER_UINTEGER_MUL_WRAPPING_DEFINITION(usize)
__SUNDER_SINTEGER_MUL_WRAPPING_DEFINITION(ssize, usize)

#define __SUNDER_UINTEGER_DIV_DEFINITION(T)                                    \
    static T __sunder_div_##T(T lhs, T rhs)                                    \
    {                                                                          \
        if (rhs == 0) {                                                        \
            __sunder_fatal_divide_by_zero();                                   \
        }                                                                      \
        return lhs / rhs;                                                      \
    }

#define __SUNDER_SINTEGER_DIV_DEFINITION(T)                                    \
    static T __sunder_div_##T(T lhs, T rhs)                                    \
    {                                                                          \
        if (rhs == 0) {                                                        \
            __sunder_fatal_divide_by_zero();                                   \
        }                                                                      \
        if ((lhs == __sunder_##T##_MIN) && (rhs == -1)) {                      \
            __sunder_fatal_out_of_range();                                     \
        }                                                                      \
        return lhs / rhs;                                                      \
    }

#define __SUNDER_IEEE754_DIV_DEFINITION(T)                                     \
    static T __sunder_div_##T(T lhs, T rhs)                                    \
    {                                                                          \
        return lhs / rhs;                                                      \
    }

__SUNDER_UINTEGER_DIV_DEFINITION(u8)
__SUNDER_SINTEGER_DIV_DEFINITION(s8)
__SUNDER_UINTEGER_DIV_DEFINITION(u16)
__SUNDER_SINTEGER_DIV_DEFINITION(s16)
__SUNDER_UINTEGER_DIV_DEFINITION(u32)
__SUNDER_SINTEGER_DIV_DEFINITION(s32)
__SUNDER_UINTEGER_DIV_DEFINITION(u64)
__SUNDER_SINTEGER_DIV_DEFINITION(s64)
__SUNDER_UINTEGER_DIV_DEFINITION(usize)
__SUNDER_SINTEGER_DIV_DEFINITION(ssize)
__SUNDER_IEEE754_DIV_DEFINITION(f32)
__SUNDER_IEEE754_DIV_DEFINITION(f64)

#define __SUNDER_INTEGER_REM_DEFINITION(T)                                     \
    static T __sunder_rem_##T(T lhs, T rhs)                                    \
    {                                                                          \
        if (rhs == 0) {                                                        \
            __sunder_fatal_divide_by_zero();                                   \
        }                                                                      \
        return lhs % rhs;                                                      \
    }

__SUNDER_INTEGER_REM_DEFINITION(u8)
__SUNDER_INTEGER_REM_DEFINITION(s8)
__SUNDER_INTEGER_REM_DEFINITION(u16)
__SUNDER_INTEGER_REM_DEFINITION(s16)
__SUNDER_INTEGER_REM_DEFINITION(u32)
__SUNDER_INTEGER_REM_DEFINITION(s32)
__SUNDER_INTEGER_REM_DEFINITION(u64)
__SUNDER_INTEGER_REM_DEFINITION(s64)
__SUNDER_INTEGER_REM_DEFINITION(usize)
__SUNDER_INTEGER_REM_DEFINITION(ssize)

#define __SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(F, I)                      \
    static I __sunder_cast_##F##_to_##I(F f)                                   \
    {                                                                          \
        if (!isfinite(f) || f < (F)__sunder_##I##_MIN                          \
            || (F)__sunder_##I##_MAX < f) {                                    \
            __sunder_fatal_out_of_range();                                     \
        }                                                                      \
        return (I)f;                                                           \
    }

__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f32, u8)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f32, s8)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f32, u16)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f32, s16)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f32, u32)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f32, s32)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f32, u64)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f32, s64)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f32, usize)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f32, ssize)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f64, u8)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f64, s8)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f64, u16)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f64, s16)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f64, u32)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f64, s32)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f64, u64)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f64, s64)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f64, usize)
__SUNDER_CAST_IEEE754_TO_INTEGER_DEFINITION(f64, ssize)

static ssize
sys_read(signed int fd, byte* buf, size_t count)
{
    ssize_t result = read(fd, buf, count);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static ssize
sys_write(signed int fd, byte* buf, size_t count)
{
    ssize_t result = write(fd, buf, count);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static ssize
sys_open(byte* filename, signed int flags, mode_t mode)
{
    int result = open(filename, flags, mode);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static ssize
sys_close(signed int fd)
{
    int result = close(fd);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static ssize
sys_lseek(signed int fd, off_t offset, int whence)
{
    off_t result = lseek(fd, offset, whence);
    if (result == (off_t)-1) {
        return -errno;
    }
    return result;
}

static void
sys_exit(signed int error_code)
{
    _exit(error_code);
}

void*
sys_opendir(char* path)
{
    return opendir(path);
}

static int
sys_closedir(void* dir)
{
    return closedir(dir);
}

struct sys_dirent;
static struct sys_dirent*
sys_readdir(void* dir)
{
    return (struct sys_dirent*)readdir(dir);
}

static ssize
sys_mkdir(byte* pathname, mode_t mode)
{
    int result = mkdir(pathname, mode);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static ssize
sys_rmdir(byte* pathname)
{
    int result = rmdir(pathname);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static ssize
sys_unlink(byte* pathname)
{
    int result = unlink(pathname);
    if (result == -1) {
        return -errno;
    }
    return result;
}

usize sys_argc;
byte** sys_argv;
byte** sys_envp;

static int
sys_get_errno(void)
{
    return errno;
}

static void
sys_set_errno(int value)
{
    errno = value;
}

static void*
sys_allocate(usize align, usize size)
{
    if (align == 0 && size == 0) {
        return NULL; // Canonical address.
    }
    if (align == 0 && size != 0) {
        __sunder_fatal("fatal: allocation with invalid alignment");
    }

    // The size parameter must be an integral multiple of alignment.
    while (size % align != 0) {
        size += 1;
    }

    void* result = aligned_alloc(align, size);
    if (result == NULL) {
        perror(__func__);
        __sunder_fatal("fatal: allocation failure");
    }

    assert(size != 0);
    memset(result, 0x00, size);
    return result;
}

static void
sys_deallocate(void* ptr, usize align, usize size)
{
    (void)align;
    (void)size;
    free(ptr);
}

// clang-format off
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

static void
sys_dump_bytes(void* addr, usize size)
{
    if (size == 0) {
        fputc('\n', stderr);
        return;
    }

    // Allocate a buffer large enough to hold size number of three-byte
    // triples. For each three-byte triple, x, bytes x[0] and x[1] will contain
    // the two-byte hex representation of a single byte of obj, and byte x[2]
    // will contain a whitespace separator (either ' ' or '\n').
    char* buf = malloc(size * 3u);
    if (buf == NULL) {
        perror(__func__);
        __sunder_fatal("fatal: allocation failure");
    }
    char* ptr = buf;

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
    free(buf);
}

bool
sys_str_to_f32(f32* out, byte* start, usize count)
{
    if (count == strlen("infinity") && 0 == memcmp(start, "infinity", count)) {
        *out = (f32)INFINITY;
        return __sunder_true;
    }

    if (count == strlen("+infinity")
        && 0 == memcmp(start, "+infinity", count)) {
        *out = (f32)INFINITY;
        return __sunder_true;
    }

    if (count == strlen("-infinity")
        && 0 == memcmp(start, "-infinity", count)) {
        *out = (f32)-INFINITY;
        return __sunder_true;
    }

    if (count == strlen("NaN") && 0 == memcmp(start, "NaN", count)) {
        *out = (f32)NAN;
        return __sunder_true;
    }

    char buf[4096]; // NUL-terminated copy of the input string.
    if ((count + 1) > sizeof(buf)) {
        return __sunder_false;
    }
    for (size_t i = 0; i < count; ++i) {
        bool valid_character = isdigit((unsigned char)start[i])
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

bool
sys_str_to_f64(f64* out, byte* start, usize count)
{
    if (count == strlen("infinity") && 0 == memcmp(start, "infinity", count)) {
        *out = (f64)INFINITY;
        return __sunder_true;
    }

    if (count == strlen("+infinity")
        && 0 == memcmp(start, "+infinity", count)) {
        *out = (f64)INFINITY;
        return __sunder_true;
    }

    if (count == strlen("-infinity")
        && 0 == memcmp(start, "-infinity", count)) {
        *out = (f64)-INFINITY;
        return __sunder_true;
    }

    if (count == strlen("NaN") && 0 == memcmp(start, "NaN", count)) {
        *out = (f64)NAN;
        return __sunder_true;
    }

    char buf[4096]; // NUL-terminated copy of the input string.
    if ((count + 1) > sizeof(buf)) {
        return __sunder_false;
    }
    for (size_t i = 0; i < count; ++i) {
        bool valid_character = isdigit((unsigned char)start[i])
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

bool
sys_f32_to_str(byte* buf, usize buf_size, f32 f, ssize digits)
{
    if (isnan(f)) {
        return sprintf(buf, "NaN");
    }
    if (isinf(f) && f < 0) {
        return sprintf(buf, "-infinity");
    }
    if (isinf(f) && f > 0) {
        return sprintf(buf, "infinity");
    }

    assert(INT_MIN <= digits && digits <= INT_MAX);
    int d = digits < 0 ? __SUNDER_IEEE754_FLT_DECIMAL_DIG : (int)digits;
    int written = snprintf(buf, buf_size, "%.*f", d, (double)f);
    if (written < 0 && (usize)written >= buf_size) {
        return __sunder_false;
    }
    if (written > 0 && digits < 0) {
        char* cur = buf + written - 1;
        while (cur > buf && cur[0] == '0' && cur[-1] != '.') {
            *cur-- = '\0';
        }
    }
    return __sunder_true;
}

bool
sys_f64_to_str(byte* buf, usize buf_size, f64 f, ssize digits)
{
    if (isnan(f)) {
        return sprintf(buf, "NaN");
    }
    if (isinf(f) && f < 0) {
        return sprintf(buf, "-infinity");
    }
    if (isinf(f) && f > 0) {
        return sprintf(buf, "infinity");
    }

    assert(INT_MIN <= digits && digits <= INT_MAX);
    int d = digits < 0 ? __SUNDER_IEEE754_DBL_DECIMAL_DIG : (int)digits;
    int written = snprintf(buf, buf_size, "%.*f", d, f);
    if (written < 0 && (usize)written >= buf_size) {
        return __sunder_false;
    }
    if (written > 0 && digits < 0) {
        char* cur = buf + written - 1;
        while (cur > buf && cur[0] == '0' && cur[-1] != '.') {
            *cur-- = '\0';
        }
    }
    return __sunder_true;
}

#define __SUNDER_IEEE754_MATH_DEFINITIONS(function)                            \
    f32 sys_f32_##function(f32 f)                                              \
    {                                                                          \
        return function##f(f);                                                 \
    }                                                                          \
    f64 sys_f64_##function(f64 f)                                              \
    {                                                                          \
        return function(f);                                                    \
    }

#define __SUNDER_IEEE754_MATH_DEFINITIONS2(function)                           \
    f32 sys_f32_##function(f32 f1, f32 f2)                                     \
    {                                                                          \
        return function##f(f1, f2);                                            \
    }                                                                          \
    f64 sys_f64_##function(f64 f1, f64 f2)                                     \
    {                                                                          \
        return function(f1, f2);                                               \
    }

f32
sys_f32_abs(f32 f)
{
    return fabsf(f);
}

f64
sys_f64_abs(f64 f)
{
    return fabs(f);
}

f32
sys_f32_min(f32 f1, f32 f2)
{
    return fminf(f1, f2);
}

f64
sys_f64_min(f64 f1, f64 f2)
{
    return fmin(f1, f2);
}

f32
sys_f32_max(f32 f1, f32 f2)
{
    return fmaxf(f1, f2);
}

f64
sys_f64_max(f64 f1, f64 f2)
{
    return fmax(f1, f2);
}

f32
sys_f32_ln(f32 f)
{
    return logf(f);
}

f64
sys_f64_ln(f64 f)
{
    return log(f);
}

__SUNDER_IEEE754_MATH_DEFINITIONS(log2)
__SUNDER_IEEE754_MATH_DEFINITIONS(log10)

__SUNDER_IEEE754_MATH_DEFINITIONS(sqrt)
__SUNDER_IEEE754_MATH_DEFINITIONS(cbrt)
__SUNDER_IEEE754_MATH_DEFINITIONS2(hypot)
__SUNDER_IEEE754_MATH_DEFINITIONS2(pow)

__SUNDER_IEEE754_MATH_DEFINITIONS(sin)
__SUNDER_IEEE754_MATH_DEFINITIONS(cos)
__SUNDER_IEEE754_MATH_DEFINITIONS(tan)
__SUNDER_IEEE754_MATH_DEFINITIONS(asin)
__SUNDER_IEEE754_MATH_DEFINITIONS(acos)
__SUNDER_IEEE754_MATH_DEFINITIONS(atan)
__SUNDER_IEEE754_MATH_DEFINITIONS2(atan2)

__SUNDER_IEEE754_MATH_DEFINITIONS(sinh)
__SUNDER_IEEE754_MATH_DEFINITIONS(cosh)
__SUNDER_IEEE754_MATH_DEFINITIONS(tanh)
__SUNDER_IEEE754_MATH_DEFINITIONS(asinh)
__SUNDER_IEEE754_MATH_DEFINITIONS(acosh)
__SUNDER_IEEE754_MATH_DEFINITIONS(atanh)

__SUNDER_IEEE754_MATH_DEFINITIONS(ceil)
__SUNDER_IEEE754_MATH_DEFINITIONS(floor)
__SUNDER_IEEE754_MATH_DEFINITIONS(trunc)
__SUNDER_IEEE754_MATH_DEFINITIONS(round)

bool
sys_f32_is_finite(f32 f)
{
    return isfinite(f);
}

bool
sys_f64_is_finite(f64 f)
{
    return isfinite(f);
}

bool
sys_f32_is_normal(f32 f)
{
    return isnormal(f);
}

bool
sys_f64_is_normal(f64 f)
{
    return isnormal(f);
}

bool
sys_f32_is_inf(f32 f)
{
    return isinf(f);
}

bool
sys_f64_is_inf(f64 f)
{
    return isinf(f);
}

bool
sys_f32_is_nan(f32 f)
{
    return isnan(f);
}

bool
sys_f64_is_nan(f64 f)
{
    return isnan(f);
}
