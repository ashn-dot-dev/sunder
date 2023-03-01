#define __SUNDER_STATIC_ASSERT(expr) enum {__SUNDER_STATIC_ASSERT_ ## __LINE__ = 1/!!(expr)}
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

_Bool const __sunder_true = 1;
_Bool const __sunder_false = 0;
