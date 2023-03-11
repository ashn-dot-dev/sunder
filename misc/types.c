// musl-gcc -std=c99 -static -g misc/types.c
//
// Used for inspecting the layout of POSIX types in GDB to produce the
// definitions in lib/sys/sys.sunder.

#define _XOPEN_SOURCE 700

#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

int
main()
{
    void* const unused = malloc(0); /* GDB needs malloc for printf */
    (void)unused;

    uint8_t uint8_t_value;
    uint16_t uint16_t_value;
    uint32_t uint32_t_value;
    uint64_t uint64_t_value;

    int8_t int8_t_value;
    int16_t int16_t_value;
    int32_t int32_t_value;
    int64_t int64_t_value;

    /* POSIX-2017 sys/resource.h */
    struct rusage rusage_value;

    /* POSIX-2017 sys/time.h */
    struct timespec timespec_value;

    /* POSIX-2017 sys/types.h */
    blkcnt_t blkcnt_t_value;
    blksize_t blksize_t_value;
    clock_t clock_t_value;
    clockid_t clockid_t_value;
    dev_t dev_t_value;
    fsblkcnt_t fsblkcnt_t_value;
    fsfilcnt_t fsfilcnt_t_value;
    gid_t gid_t_value;
    id_t id_t_value;
    ino_t ino_t_value;
    key_t key_t_value;
    mode_t mode_t_value;
    nlink_t nlink_t_value;
    off_t off_t_value;
    pid_t pid_t_value;
    size_t size_t_value;
    ssize_t ssize_t_value;
    suseconds_t suseconds_t_value;
    time_t time_t_value;
    timer_t timer_t_value;
    uid_t uid_t_value;

    /* time.h */
    struct timeval timeval_value;
}
