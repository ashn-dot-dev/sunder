#define _DEFAULT_SOURCE /* syscall */
#include <limits.h> /* LLONG_MIN */
#include <alloca.h> /* alloca */
#include <errno.h> /* errno */
#include <fcntl.h> /* open */
#include <stdio.h> /* fprintf */
#include <stdlib.h> /* exit */
#include <sys/mman.h> /* mmap, munmap */
#include <sys/stat.h> /* mkdir */
#include <sys/syscall.h> /* SYS_* constants */
#include <sys/types.h> /* mode_t, off_t, size_t */
#include <unistd.h> /* syscall, close, _exit, lseek, read, rmdir, write, unlink */

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

#define __sunder_true  ((_Bool)1)
#define __sunder_false ((_Bool)0)

static inline _Noreturn void
__sunder___fatal(char const* message)
{
    fprintf(stderr, "fatal: %s\n", message);
    exit(1);
}

static _Noreturn void
__sunder___fatal_integer_divide_by_zero(void)
{
    __sunder___fatal("divide by zero");
}

static _Noreturn void
__sunder___fatal_integer_out_of_range(void)
{
    __sunder___fatal("arithmetic operation produces out-of-range result");
}

static _Noreturn void
__sunder___fatal_index_out_of_bounds(void)
{
    __sunder___fatal("index out-of-bounds");
}

static __sunder_ssize
__sunder_sys_read(signed int fd, __sunder_byte* buf, size_t count)
{
    ssize_t result = read(fd, buf, count);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
__sunder_sys_write(signed int fd, __sunder_byte* buf, size_t count)
{
    ssize_t result = write(fd, buf, count);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
__sunder_sys_open(__sunder_byte* filename, signed int flags, mode_t mode)
{
    int result = open(filename, flags, mode);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
__sunder_sys_close(signed int fd)
{
    int result = close(fd);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
__sunder_sys_lseek(signed int fd, off_t offset, unsigned int whence)
{
    off_t result = lseek(fd, offset, whence);
    if (result == (off_t)-1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
__sunder_sys_mmap(void* addr, size_t len, signed int prot, signed int flags, signed int fd, off_t off)
{
    void* result = mmap(addr, len, prot, flags, fd, off);
    if (result == (void*)-1) {
        return -errno;
    }
    return (__sunder_ssize)result;
}

static __sunder_ssize
__sunder_sys_munmap(void* addr, size_t len)
{
    int result = munmap(addr, len);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static void
__sunder_sys_exit(signed int error_code)
{
    _exit(error_code);
}

struct __sunder_sys__dirent;
static __sunder_ssize
__sunder_sys_getdents(signed int fd, struct __sunder_sys__dirent* dirent, unsigned int count)
{
    // XXX: Pretty sure this is wrong... should be getdents and not getdents64.
    long result = syscall(SYS_getdents64, fd, dirent, count);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
__sunder_sys_mkdir(__sunder_byte* pathname, mode_t mode)
{
    int result = mkdir(pathname, mode);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
__sunder_sys_rmdir(__sunder_byte* pathname)
{
    int result = rmdir(pathname);
    if (result == -1) {
        return -errno;
    }
    return result;
}

static __sunder_ssize
__sunder_sys_unlink(__sunder_byte* pathname)
{
    int result = unlink(pathname);
    if (result == -1) {
        return -errno;
    }
    return result;
}

__sunder_usize __sunder_sys_argc;
__sunder_byte** __sunder_sys_argv;
__sunder_byte** __sunder_sys_envp;

static char const __sunder_dump_lookup_table[256u*2u] = {
'0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0', '7',
'0', '8', '0', '9', '0', 'A', '0', 'B', '0', 'C', '0', 'D', '0', 'E', '0', 'F',
'1', '0', '1', '1', '1', '2', '1', '3', '1', '4', '1', '5', '1', '6', '1', '7',
'1', '8', '1', '9', '1', 'A', '1', 'B', '1', 'C', '1', 'D', '1', 'E', '1', 'F',
'2', '0', '2', '1', '2', '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7',
'2', '8', '2', '9', '2', 'A', '2', 'B', '2', 'C', '2', 'D', '2', 'E', '2', 'F',
'3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3', '7',
'3', '8', '3', '9', '3', 'A', '3', 'B', '3', 'C', '3', 'D', '3', 'E', '3', 'F',
'4', '0', '4', '1', '4', '2', '4', '3', '4', '4', '4', '5', '4', '6', '4', '7',
'4', '8', '4', '9', '4', 'A', '4', 'B', '4', 'C', '4', 'D', '4', 'E', '4', 'F',
'5', '0', '5', '1', '5', '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7',
'5', '8', '5', '9', '5', 'A', '5', 'B', '5', 'C', '5', 'D', '5', 'E', '5', 'F',
'6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7',
'6', '8', '6', '9', '6', 'A', '6', 'B', '6', 'C', '6', 'D', '6', 'E', '6', 'F',
'7', '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7',
'7', '8', '7', '9', '7', 'A', '7', 'B', '7', 'C', '7', 'D', '7', 'E', '7', 'F',
'8', '0', '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7',
'8', '8', '8', '9', '8', 'A', '8', 'B', '8', 'C', '8', 'D', '8', 'E', '8', 'F',
'9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9', '7',
'9', '8', '9', '9', '9', 'A', '9', 'B', '9', 'C', '9', 'D', '9', 'E', '9', 'F',
'A', '0', 'A', '1', 'A', '2', 'A', '3', 'A', '4', 'A', '5', 'A', '6', 'A', '7',
'A', '8', 'A', '9', 'A', 'A', 'A', 'B', 'A', 'C', 'A', 'D', 'A', 'E', 'A', 'F',
'B', '0', 'B', '1', 'B', '2', 'B', '3', 'B', '4', 'B', '5', 'B', '6', 'B', '7',
'B', '8', 'B', '9', 'B', 'A', 'B', 'B', 'B', 'C', 'B', 'D', 'B', 'E', 'B', 'F',
'C', '0', 'C', '1', 'C', '2', 'C', '3', 'C', '4', 'C', '5', 'C', '6', 'C', '7',
'C', '8', 'C', '9', 'C', 'A', 'C', 'B', 'C', 'C', 'C', 'D', 'C', 'E', 'C', 'F',
'D', '0', 'D', '1', 'D', '2', 'D', '3', 'D', '4', 'D', '5', 'D', '6', 'D', '7',
'D', '8', 'D', '9', 'D', 'A', 'D', 'B', 'D', 'C', 'D', 'D', 'D', 'E', 'D', 'F',
'E', '0', 'E', '1', 'E', '2', 'E', '3', 'E', '4', 'E', '5', 'E', '6', 'E', '7',
'E', '8', 'E', '9', 'E', 'A', 'E', 'B', 'E', 'C', 'E', 'D', 'E', 'E', 'E', 'F',
'F', '0', 'F', '1', 'F', '2', 'F', '3', 'F', '4', 'F', '5', 'F', '6', 'F', '7',
'F', '8', 'F', '9', 'F', 'A', 'F', 'B', 'F', 'C', 'F', 'D', 'F', 'E', 'F', 'F',
};

static void
__sunder_dump(void const* pobj, size_t size)
{
    if (size == 0) {
        fputc('\n', stderr);
        return;
    }

    // Allocate a buffer large enough to hold size number of three-byte
    // triples. For each three-byte triple, x, bytes x[0:1] will contain the
    // two-byte hex representation of a single byte of obj, and byte x[2] will
    // contain a whitespace separator (either ' ' or '\n').
    char* const buf = alloca(size * 3u); // Locally allocated buffer.
    char* ptr = buf; // Write pointer into the locally allocated buffer.

    unsigned char const* cur = pobj;
    unsigned char const* const end = (unsigned char const*)pobj + size;
    while (cur != end) {
        char const* const repr = __sunder_dump_lookup_table + (*cur * 2u);
        ptr[0u] = repr[0u];
        ptr[1u] = repr[1u];
        ptr[2u] = ' ';
        ptr += 3u;
        cur += 1u;
    }

    // .write
    ptr[-1] = '\n';
    fprintf(stderr, "%.*s", (int)(size * 3u), buf);
}

__sunder_void
__sunder_sys__dump(void* object_addr, __sunder_usize object_size)
{
    __sunder_dump(object_addr, object_size);
}
