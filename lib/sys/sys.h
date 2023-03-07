#define _DEFAULT_SOURCE /* syscall */
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
