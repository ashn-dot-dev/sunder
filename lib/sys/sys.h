#define _DEFAULT_SOURCE /* syscall */
#include <stdio.h> /* fprintf */
#include <stdlib.h> /* exit */
#include <sys/types.h> /* mode_t, off_t, size_t */
#include <unistd.h> /* syscall */

typedef void               __sunder_void;
typedef _Bool              __sunder_bool;
typedef unsigned char      __sunder_byte;
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
    long const SYS_READ = 0;
    return syscall(SYS_READ, fd, buf, count);
}

static __sunder_ssize
__sunder_sys_write(signed int fd, __sunder_byte* buf, size_t count)
{
    long const SYS_WRITE = 1;
    return syscall(SYS_WRITE, fd, buf, count);
}

static __sunder_ssize
__sunder_sys_open(__sunder_byte* filename, signed int flags, mode_t mode)
{
    long const SYS_OPEN = 2;
    return syscall(SYS_OPEN, filename, flags, mode);
}

static __sunder_ssize
__sunder_sys_close(signed int fd)
{
    long const SYS_CLOSE = 3;
    return syscall(SYS_CLOSE, fd);
}

static __sunder_ssize
__sunder_sys_lseek(signed int fd, off_t offset, unsigned int whence)
{
    long const SYS_LSEEK = 8;
    return syscall(SYS_LSEEK, fd, offset, whence);
}

static __sunder_ssize
__sunder_sys_mmap(void* addr, size_t len, signed int prot, signed int flags, signed int fd, off_t off)
{
    long const SYS_MMAP = 9;
    return syscall(SYS_MMAP, addr, len, prot, flags, fd, off);
}

static __sunder_ssize
__sunder_sys_munmap(void* addr, size_t len)
{
    long const SYS_MUNMAP = 11;
    return syscall(SYS_MUNMAP, addr, len);
}

static void
__sunder_sys_exit(signed int error_code)
{
    long const SYS_EXIT = 60;
    syscall(SYS_EXIT, error_code);
}

struct __sunder_sys__dirent;
static __sunder_ssize
__sunder_sys_getdents(signed int fd, struct __sunder_sys__dirent* dirent, unsigned int count)
{
    long const SYS_GETDENTS = 78;
    return syscall(SYS_GETDENTS, fd, dirent, count);
}

static __sunder_ssize
__sunder_sys_mkdir(__sunder_byte* pathname, mode_t mode)
{
    long const SYS_MKDIR = 83;
    return syscall(SYS_MKDIR, pathname, mode);
}

static __sunder_ssize
__sunder_sys_rmdir(__sunder_byte* pathname)
{
    long const SYS_RMDIR = 84;
    return syscall(SYS_RMDIR, pathname);
}

static __sunder_ssize
__sunder_sys_unlink(__sunder_byte* pathname)
{
    long const SYS_UNLINK = 87;
    return syscall(SYS_UNLINK, pathname);
}

__sunder_usize __sunder_sys_argc;
__sunder_byte** __sunder_sys_argv;
__sunder_byte** __sunder_sys_envp;
