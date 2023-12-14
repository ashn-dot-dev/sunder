// clang -std=c11 misc/sys-defs.c
#define _GNU_SOURCE

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define dump_primitive(T) ({T value; printf("%s = %s\n", #T, _Generic(value,   \
             _Bool: "_Bool",                                                   \
              char: "char",                                                    \
     unsigned char: "unsigned char",                                           \
       signed char: "signed char",                                             \
    unsigned short: "unsigned short",                                          \
      signed short: "signed short",                                            \
      unsigned int: "unsigned int",                                            \
        signed int: "signed int",                                              \
     unsigned long: "unsigned long",                                           \
       signed long: "signed long",                                             \
unsigned long long: "unsigned long long",                                      \
  signed long long: "signed long long",                                        \
             float: "float",                                                   \
            double: "double",                                                  \
       long double: "long double"));})

#define dump_structure(T) ({                                                   \
    T value;                                                                   \
    memset(&value, 0x00, sizeof(value));                                       \
    __builtin_dump_struct(&value, &printf);})                                  \

int
main(void)
{
    printf("== PRIMITIVE TYPES ==\n");
    dump_primitive(uint8_t);
    dump_primitive( int8_t);
    dump_primitive(uint16_t);
    dump_primitive( int16_t);
    dump_primitive(uint32_t);
    dump_primitive( int32_t);
    dump_primitive(uint64_t);
    dump_primitive( int64_t);
    fputc('\n', stdout);
    dump_primitive(ino_t);
    dump_primitive(mode_t);
    dump_primitive(off_t);
    dump_primitive(size_t);
    dump_primitive(ssize_t);
    dump_primitive(time_t);

    fputc('\n', stdout);

    printf("== STRUCTURE TYPES ==\n");
    dump_structure(struct timespec);
    fputc('\n', stdout);
    dump_structure(struct dirent);

    fputc('\n', stdout);

    printf("== POSIX CONSTANTS ==\n");
#define PRINT_O_VALUE(value)                                                   \
    printf("let %-12s sint = 0x%08x;\n", #value ":", value)

    PRINT_O_VALUE(O_RDONLY);
    PRINT_O_VALUE(O_WRONLY);
    PRINT_O_VALUE(O_RDWR);

    PRINT_O_VALUE(O_CREAT);
    PRINT_O_VALUE(O_TRUNC);
    PRINT_O_VALUE(O_APPEND);
#undef PRINT_O_VALUE
    fputc('\n', stdout);
#define PRINT_SEEK_VALUE(value)                                                \
    printf("let %-8s uint = 0x%01x;\n", #value ":", value)
    PRINT_SEEK_VALUE(SEEK_SET);
    PRINT_SEEK_VALUE(SEEK_CUR);
    PRINT_SEEK_VALUE(SEEK_END);
#undef PRINT_SEEK_VALUE
    fputc('\n', stdout);
    printf("let PATH_MAX: usize = %zu;\n", (size_t)PATH_MAX);
}
