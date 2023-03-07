#include <stdio.h> /* fprintf */
#include <stdlib.h> /* exit */

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
