/*
AUTIL - ASHN'S UTILITY LIBRARY - v0.10.0
    Single header file containing functions and data structures for rapid
    application development in C99 (or later) with a focus on simplicity.
    This library has no dependencies outside of libc.


USAGE
    To create function and object definitions:
        #define AUTIL_IMPLEMENTATION
    in exactly *ONE* translation unit before including this file:
        #define AUTIL_IMPLEMENTATION
        #include "autil.h"

    To restrict function and object definitions to a single translation unit:
        #define AUTIL_API static
    before including this file:
        #define AUTIL_IMPLEMENTATION
        #define AUTIL_API static
        #include "autil.h"

    This library uses the libc functions realloc and free for dynamic memory
    management. If you would like to replace these functions with your own
    custom allocation functions:
        #define AUTIL_REALLOC custom_realloc
        #define AUTIL_FREE custom_free
    before including this file in the translation unit where
    AUTIL_IMPLEMENTATION is defined:
        void* custom_realloc(void* ptr, size_t size);
        void custom_free(void* ptr);
        #define AUTIL_IMPLEMENTATION
        #define AUTIL_REALLOC custom_realloc
        #define AUTIL_FREE custom_free
        #include "autil.h"


LICENSE
    Copyright (c) 2020 ashn <me@ashn.dev>

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
    SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
    OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
    CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// HEADER SECTION ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#ifndef AUTIL_H_INCLUDED
#define AUTIL_H_INCLUDED

#ifndef AUTIL_API
#    define AUTIL_API extern
#endif

#include <stdarg.h> /* va_list */
#include <stddef.h> /* size_t, NULL, offsetof */
#include <stdio.h> /* FILE*, printf-family */

struct autil_vstr;
struct autil_sipool;
struct autil_bitarr;
struct autil_bigint;
struct autil_string;
struct autil_vec;
struct autil_map;
struct autil_freezer;

// C99 compatible max_align_t.
// clang-format off
typedef union {
    _Bool       bool_;
    char        char_;
    short       short_;
    int         int_;
    long        long_;
    long long   long_long_;
    float       float_;
    double      double_;
    long double long_double_;
    void*       void_ptr_;
    void        (*fn_ptr_)();
} autil_max_align_type;
// clang-format on

// Produce a pointer of type TYPE* whose contents is the scalar rvalue val.
// This pointer has automatic storage duration associated with the enclosing
// block.
//
// Example:
//      int* pint = AUTIL_LOCAL_PTR(int, 42);
//      char const** pstr = AUTIL_LOCAL_PTR(char const*, "FOO");
//      printf("%d %s\n", *pint, *pstr); // 42 FOO
#define AUTIL_LOCAL_PTR(TYPE, /*val*/...) (&((TYPE){__VA_ARGS__}))

// Dereference ptr as if it were of type TYPE*.
//
// Example:
//      void* ptr = some_func();
//      int val = AUTIL_DEREF_PTR(int, ptr);
#define AUTIL_DEREF_PTR(TYPE, /*ptr*/...) (*((TYPE*)(__VA_ARGS__)))

// Number of elements in an array.
#define AUTIL_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))
// Number of characters in a string literal, excluding the NUL-terminator.
#define AUTIL_STR_LITERAL_COUNT(str_literal)                                   \
    (AUTIL_ARRAY_COUNT(str_literal) - 1)
// Number of characters in a formatted string.
#define AUTIL_FMT_COUNT(fmt, ...) ((size_t)snprintf(NULL, 0, fmt, __VA_ARGS__))

// C99 compatible _Alignof operator.
// Produces an integer constant expression.
// clang-format off
#define AUTIL_ALIGNOF(type) offsetof(struct{char _; type ty;}, ty)
// clang-format on

// C99 compatible(ish) _Static_assert.
// Macro parameter what should be a valid identifier describing the assertion.
// Flips the order of arguments from C11's _Static_assert so that assertions
// read as if they were a sentence.
// Example:
//      // Assert that we are compiling on a 64-bit machine.
//      AUTIL_STATIC_ASSERT(pointers_are_eight_bytes, sizeof(void*) == 8);
// clang-format off
#define AUTIL_STATIC_ASSERT(what, expr)                                        \
    enum {STATIC_ASSERT__ ## what = 1/!!(expr)}//;
// clang-format on

// Should return an int less than, equal to, or greater than zero if lhs is
// semantically less than, equal to, or greater than rhs, respectively.
typedef int (*autil_vpcmp_fn)(void const* lhs, void const* rhs);
// Implementations of autil_vpcmp_fn for builtin types.
AUTIL_API int
autil_void_vpcmp(void const* lhs, void const* rhs); // void (zero-sized object)
AUTIL_API int
autil_cstr_vpcmp(void const* lhs, void const* rhs); // char const*
AUTIL_API int
autil_int_vpcmp(void const* lhs, void const* rhs); // int

// Alternatives to the C99 standard library functions in ctype.h.
// These functions always use the "C" locale and will not result in undefined
// behavior if passed a value not representable by an unsigned char.
// clang-format off
AUTIL_API int autil_isalnum(int c);
AUTIL_API int autil_isalpha(int c);
AUTIL_API int autil_isblank(int c);
AUTIL_API int autil_iscntrl(int c);
AUTIL_API int autil_isdigit(int c);
AUTIL_API int autil_isgraph(int c);
AUTIL_API int autil_islower(int c);
AUTIL_API int autil_isprint(int c);
AUTIL_API int autil_ispunct(int c);
AUTIL_API int autil_isspace(int c);
AUTIL_API int autil_isupper(int c);
AUTIL_API int autil_isbdigit(int c); // Not in C99. Binary digit.
AUTIL_API int autil_isodigit(int c); // Not in C99. Octal digit.
AUTIL_API int autil_isxdigit(int c);
AUTIL_API int autil_tolower(int c);
AUTIL_API int autil_toupper(int c);
// clang-format on

// Alternatives to the C99 standard library functions in string.h.
// These functions do not result in undefined behavior when passed an invalid
// pointer argument paired with a memory-size argument of zero.
// clang-format off
AUTIL_API int autil_memcmp(void const* s1, void const* s2, size_t n);
AUTIL_API void* autil_memmove(void* dest, void const* src, size_t n);
AUTIL_API void* autil_memset(void* s, int c, size_t n);
// clang-format on

// Zero out the memory under the provided pointer parameter. The number of bytes
// to be zeroed is automatically determined by the sizeof(*ptr).
#define AUTIL_MEMZERO(ptr) autil_memset(ptr, 0x00, sizeof(*ptr))

// General purpose allocator functions with out-of-memory error checking.
// The behavior of autil_xalloc and autil_xallocn is similar to libc realloc and
// *BSD reallocarray with the following exceptions:
// (1) On allocation failure an error message will be printed followed by
//     program termination with EXIT_FAILURE status.
// (2) The call autil_xalloc(ptr, 0) is guaranteed to free the memory backing
//     ptr. A pointer returned by autil_xalloc may be freed with
//     autil_xalloc(ptr, 0) or the equivalent
//     autil_xalloc(ptr, AUTIL_XALLOC_FREE). The calls autil_xallocn(ptr, x, 0)
//     and autil_xallocn(ptr, 0, y) are equivalent to autil_xalloc(ptr, 0).
// The macro AUTIL_XALLOC_FREE may be used in place of the constant zero to
// indicate that a call autil_xalloc(ptr, AUTIL_XALLOC_FREE) is intended as a
// free operation.
AUTIL_API void*
autil_xalloc(void* ptr, size_t size);
AUTIL_API void*
autil_xallocn(void* ptr, size_t nmemb, size_t size);
#define AUTIL_XALLOC_FREE ((size_t)0)

// Write a formatted info message to stderr.
// A newline is automatically appended to the end of the formatted message.
AUTIL_API void
autil_infof(char const* fmt, ...);
// Write a formatted error message to stderr.
// A newline is automatically appended to the end of the formatted message.
AUTIL_API void
autil_errorf(char const* fmt, ...);
// Write a formatted error message to stderr and exit with EXIT_FAILURE status.
// A newline is automatically appended to the end of the formatted message.
AUTIL_API void
autil_fatalf(char const* fmt, ...);

// Read the full contents of the file specified by path.
// Memory for the read content is allocated with autil_xalloc.
// Returns zero on success.
AUTIL_API int
autil_file_read(char const* path, void** buf, size_t* buf_size);

// Write the contents of a buffer into the file specified by path.
// The file specified by path is created if it does not exist.
// Returns zero on success.
// On failure, the contents of the file specified by path is undefined.
AUTIL_API int
autil_file_write(char const* path, void const* buf, size_t buf_size);

// Read the full contents of the input stream specified by stream.
// Memory for the read content is allocated with autil_xalloc.
// Returns zero on success.
AUTIL_API int
autil_stream_read(FILE* stream, void** buf, size_t* buf_size);

// Read the contents of the input stream specified by stream until a newline or
// end-of-file is encountered.
// The line buffer will *not* have NUL termination.
// The line buffer will contain the end-of-line newline (if present).
// Memory for the read content is allocated with autil_xalloc.
// Returns zero on success.
AUTIL_API int
autil_stream_read_line(FILE* stream, void** buf, size_t* buf_size);

////////////////////////////////////////////////////////////////////////////////
//////// CSTR //////////////////////////////////////////////////////////////////

// Returns an autil_xalloc-allocated cstring of the first count bytes of start.
// This function behaves similarly to the POSIX strdupn function.
AUTIL_API char*
autil_cstr_new(char const* start, size_t count);
// Returns an autil_xalloc-allocated copy of the provided cstring.
// This function behaves similarly to the POSIX strdup function.
AUTIL_API char*
autil_cstr_new_cstr(char const* cstr);
// Returns an autil_xalloc-allocated cstring from the provided formatted text.
AUTIL_API char*
autil_cstr_new_fmt(char const* fmt, ...);
// Returns a non-zero value if cstr starts with target.
AUTIL_API int
autil_cstr_starts_with(char const* cstr, char const* target);
// Returns a non-zero value if cstr ends with target.
AUTIL_API int
autil_cstr_ends_with(char const* cstr, char const* target);

////////////////////////////////////////////////////////////////////////////////
//////// VSTR //////////////////////////////////////////////////////////////////
// Byte string view.

struct autil_vstr {
    char const* start;
    size_t count;
};

// Produce a pointer of type struct autil_vstr* constructed from the provided
// parameters. This pointer has automatic storage duration associated with the
// enclosing block.
#define AUTIL_VSTR_LOCAL_PTR(start, count) (&(struct autil_vstr){start, count})
// Produce a pointer of type struct autil_vstr* from the provided cstring
// literal. This pointer has automatic storage duration associated with the
// enclosing block.
#define AUTIL_VSTR_LOCAL_PTR_STR_LITERAL(str_literal)                          \
    AUTIL_VSTR_LOCAL_PTR(str_literal, AUTIL_STR_LITERAL_COUNT(str_literal))

// Initializer for a vstring literal from a cstring literal.
// Example:
//      static struct autil_vstr const foo =
//          AUTIL_VSTR_INIT_STR_LITERAL("foo");
// Example:
//      struct autil_vstr bar = {0};
//      // some time later...
//      bar = (struct autil_vstr)AUTIL_VSTR_INIT_STR_LITERAL("bar");
// clang-format off
#define AUTIL_VSTR_INIT_STR_LITERAL(str_literal)                               \
    {str_literal, AUTIL_STR_LITERAL_COUNT(str_literal)}
// clang-format on

// Return an int less than, equal to, or greater than zero if lhs is
// lexicographically less than, equal to, or greater than rhs, respectively.
AUTIL_API int
autil_vstr_cmp(struct autil_vstr const* lhs, struct autil_vstr const* rhs);
// Comparison function satisfying autil_vpcmp_fn.
// Parameters lhs and rhs must be of type struct autil_vstr const*.
AUTIL_API int
autil_vstr_vpcmp(void const* lhs, void const* rhs);

// Returns a non-zero value if vstr starts with target.
AUTIL_API int
autil_vstr_starts_with(
    struct autil_vstr const* vstr, struct autil_vstr const* target);
// Returns a non-zero value if vstr ends with target.
AUTIL_API int
autil_vstr_ends_with(
    struct autil_vstr const* vstr, struct autil_vstr const* target);

////////////////////////////////////////////////////////////////////////////////
//////// CSTR INTERN POOL //////////////////////////////////////////////////////

// Allocate and initialize a string intern pool.
AUTIL_API struct autil_sipool*
autil_sipool_new(void);
// Deinitialize and free the string intern pool.
// Does nothing if self == NULL.
AUTIL_API void
autil_sipool_del(struct autil_sipool* self);

// Intern the string specified by the first count bytes of start.
// Returns the canonical NUL-terminated representation of the interned string.
AUTIL_API char const*
autil_sipool_intern(struct autil_sipool* self, char const* start, size_t count);
// Intern the string specified by the provided NUL-terminated cstring.
// Returns the canonical NUL-terminated representation of the interned string.
AUTIL_API char const*
autil_sipool_intern_cstr(struct autil_sipool* self, char const* cstr);

////////////////////////////////////////////////////////////////////////////////
//////// STRETCHY BUFFER ///////////////////////////////////////////////////////
// General purpose type-safe dynamic array (a.k.a stretchy buffer).
//
// A stretchy buffer works by storing metadata about the number of allocated and
// in-use elements in a header just before the address of the buffer's first
// element. The ith element of a stretchy buffer may be accessed using the array
// index operator, sbuf[i], and a stretchy buffer containing elements of type T
// may be passed to subroutines as if it were regular array-like pointer of type
// T* or T const*. The address of a stretchy buffer may change when a resizing
// operation is performed, similar to resizing operations done with realloc, so
// the address of a stretchy buffer should not be considered stable.
//
// +--------+---------+---------+---------+--
// | HEADER | SBUF[0] | SBUF[1] | SBUF[2] | ...
// +--------+---------+---------+---------+--
//          ^
//          Pointer manipulated by the user / autil_sbuf_* macros.
//
// Example:
//      // The declaration:
//      //      TYPE* identifier = NULL;
//      // creates an empty stretchy buffer holding TYPE values.
//      // An equivalent declaration:
//      //      autil_sbuf(TYPE) identifier = NULL;
//      // may also be used in most cases.
//      int* vals = NULL;
//      printf("count == %zu\n", autil_sbuf_count(vals));  /* count == 0 */
//
//      for (int i = 0; i < 3; ++i) {
//          autil_sbuf_push(vals, (i + 1) * 2);
//      }
//      printf("count == %zu\n", autil_sbuf_count(vals)); /* count == 3 */
//      printf("vals[0] == %d\n", vals[0]); /* vals[0] == 2 */
//      printf("vals[1] == %d\n", vals[1]); /* vals[1] == 4 */
//      printf("vals[2] == %d\n", vals[2]); /* vals[2] == 6 */
//
//      printf("popped == %d\n", autil_sbuf_pop(vals)); /* popped == 6 */
//      printf("count == %zu\n", autil_sbuf_count(vals)); /* count == 2 */
//
//      // Free memory allocated to the sbuf.
//      // This is safe to call even if vals == NULL.
//      autil_sbuf_fini(vals);

// Convenience macros used to explicitly annotate a pointer as a stretchy
// buffer. Type annotations for types such as stack-allocated arrays and
// function pointers are not supported by this macro due to the complicated
// nature of C variable/type declarations.
//
// Example:
//      autil_sbuf(int) sbuf = NULL;
//      autil_sbuf_push(sbuf, 1);
#define autil_sbuf(TYPE) TYPE*
#define autil_sbuf_const(TYPE) TYPE const*

// void autil_sbuf_fini(TYPE* sbuf)
// ------------------------------------------------------------
// Free resources associated with the stretchy buffer.
// Macro parameter sbuf is evaluated multiple times.
#define autil_sbuf_fini(sbuf)                                                  \
    ((void)((sbuf) != NULL ? AUTIL__SBUF_FREE_NON_NULL_HEAD_(sbuf) : NULL))

// void autil_sbuf_freeze(TYPE* sbuf, struct autil_freezer* freezer)
// ------------------------------------------------------------
// Register resources within bigint with the provided freezer.
#define autil_sbuf_freeze(sbuf, freezer)                                       \
    ((void)((sbuf) != NULL ? AUTIL__SBUF_FREEZE_NON_NULL_HEAD_(sbuf, freezer), NULL : NULL))

// size_t autil_sbuf_count(TYPE* sbuf)
// ------------------------------------------------------------
// The number of elements in the stretchy buffer.
// Macro parameter sbuf is evaluated multiple times.
#define autil_sbuf_count(sbuf)                                                 \
    ((size_t)((sbuf) != NULL ? AUTIL__SBUF_PHEAD_CONST_(sbuf)->cnt_ : 0u))
// size_t autil_sbuf_capacity(TYPE* sbuf)
// ------------------------------------------------------------
// The number of elements the allocated in the sbuf.
// Macro parameter sbuf is evaluated multiple times.
#define autil_sbuf_capacity(sbuf)                                              \
    ((size_t)((sbuf) != NULL ? AUTIL__SBUF_PHEAD_CONST_(sbuf)->cap_ : 0u))

// void autil_sbuf_reserve(TYPE* sbuf, size_t n)
// ------------------------------------------------------------
// Update the minimum capacity of the stretchy buffer to n elements.
// Macro parameter sbuf is evaluated multiple times.
#define autil_sbuf_reserve(sbuf, /*n*/...)                                     \
    ((void)((sbuf) = autil__sbuf_rsv_(sizeof(*(sbuf)), sbuf, __VA_ARGS__)))
// void autil_sbuf_resize(TYPE* sbuf, size_t n)
// ------------------------------------------------------------
// Update the count of the stretchy buffer to n elements.
// Macro parameter sbuf is evaluated multiple times.
#define autil_sbuf_resize(sbuf, /*n*/...)                                      \
    ((void)((sbuf) = autil__sbuf_rsz_(sizeof(*(sbuf)), sbuf, __VA_ARGS__)))

// void autil_sbuf_push(TYPE* sbuf, TYPE val)
// ------------------------------------------------------------
// Append val as the last element of the stretchy buffer.
// Macro parameter sbuf is evaluated multiple times.
#define autil_sbuf_push(sbuf, /*val*/...)                                      \
    ((void)(AUTIL__SBUF_MAYBE_GROW_(sbuf), AUTIL__SBUF_APPEND_(sbuf, __VA_ARGS__)))
// TYPE autil_sbuf_pop(TYPE* sbuf)
// ------------------------------------------------------------
// Remove and return the last element of the stretchy buffer.
// This macro does *not* perform bounds checking.
// Macro parameter sbuf is evaluated multiple times.
#define autil_sbuf_pop(sbuf) ((sbuf)[--AUTIL__SBUF_PHEAD_MUTBL_(sbuf)->cnt_])

// Internal utilities that must be visible to other header/source files that
// wish to use the autil_sbuf_* API. Do not use these directly!
// clang-format off
struct autil__sbuf_header_{size_t cnt_; size_t cap_; autil_max_align_type _[];};
enum{AUTIL__SBUF_HEADER_OFFSET_ = sizeof(struct autil__sbuf_header_)};
#define AUTIL__SBUF_PHEAD_MUTBL_(sbuf_)                                        \
    ((struct autil__sbuf_header_      *)                                       \
     ((char      *)(sbuf_)-AUTIL__SBUF_HEADER_OFFSET_))
#define AUTIL__SBUF_PHEAD_CONST_(sbuf_)                                        \
    ((struct autil__sbuf_header_ const*)                                       \
     ((char const*)(sbuf_)-AUTIL__SBUF_HEADER_OFFSET_))
#define AUTIL__SBUF_FREE_NON_NULL_HEAD_(sbuf_)                                 \
    (autil_xalloc(AUTIL__SBUF_PHEAD_MUTBL_(sbuf_), AUTIL_XALLOC_FREE))
#define AUTIL__SBUF_FREEZE_NON_NULL_HEAD_(sbuf_, freezer)                      \
    (autil_freezer_register(freezer, AUTIL__SBUF_PHEAD_MUTBL_(sbuf_)))
#define AUTIL__SBUF_MAYBE_GROW_(sbuf_)                                         \
    ((autil_sbuf_count(sbuf_) == autil_sbuf_capacity(sbuf_))                   \
         ? (sbuf_) = autil__sbuf_grw_(sizeof(*(sbuf_)), sbuf_)                 \
         : (sbuf_))
#define AUTIL__SBUF_APPEND_(sbuf_, ...)                                        \
    ((sbuf_)[AUTIL__SBUF_PHEAD_MUTBL_(sbuf_)->cnt_++] = (__VA_ARGS__))
AUTIL_API void* autil__sbuf_rsv_(size_t elemsize, void* sbuf, size_t cap);
AUTIL_API void* autil__sbuf_rsz_(size_t elemsize, void* sbuf, size_t cnt);
AUTIL_API void* autil__sbuf_grw_(size_t elemsize, void* sbuf);
// clang-format on

////////////////////////////////////////////////////////////////////////////////
//////// BIT ARRAY /////////////////////////////////////////////////////////////

// Allocate and initialize a bit array with count bits.
// The bit array is initially zeroed.
AUTIL_API struct autil_bitarr*
autil_bitarr_new(size_t count);
// Deinitialize and free the bit array.
// Does nothing if self == NULL.
AUTIL_API void
autil_bitarr_del(struct autil_bitarr* self);

// Returns the number of bits in this bit array.
AUTIL_API size_t
autil_bitarr_count(struct autil_bitarr const* self);
// Set the nth bit (zero indexed) of self to value.
// Fatally exits after printing an error message if n is out of bounds.
AUTIL_API void
autil_bitarr_set(struct autil_bitarr* self, size_t n, int value);
// Returns the value (one or zero) of the nth bit (zero indexed) of self.
// Fatally exits after printing an error message if n is out of bounds.
AUTIL_API int
autil_bitarr_get(struct autil_bitarr const* self, size_t n);

// self = othr
// Fatally exits after printing an error message if the count of self is not
// equal to the count of othr.
AUTIL_API void
autil_bitarr_assign(struct autil_bitarr* self, struct autil_bitarr const* othr);

// res = ~rhs
// Fatally exits after printing an error message if the count of res and rhs are
// not equal.
AUTIL_API void
autil_bitarr_compl(struct autil_bitarr* res, struct autil_bitarr const* rhs);
// res = lhs << nbits (logical shift left)
// Fatally exits after printing an error message if the count of res and lhs are
// not equal.
AUTIL_API void
autil_bitarr_shiftl(
    struct autil_bitarr* res, struct autil_bitarr const* lhs, size_t nbits);
// res = lhs >> nbits (logical shift right)
// Fatally exits after printing an error message if the count of res and lhs are
// not equal.
AUTIL_API void
autil_bitarr_shiftr(
    struct autil_bitarr* res, struct autil_bitarr const* lhs, size_t nbits);
// res = lhs & rhs
// Fatally exits after printing an error message if the count of res, lhs, and
// rhs are not equal.
AUTIL_API void
autil_bitarr_and(
    struct autil_bitarr* res,
    struct autil_bitarr const* lhs,
    struct autil_bitarr const* rhs);
// res = lhs ^ rhs
// Fatally exits after printing an error message if the count of res, lhs, and
// rhs are not equal.
AUTIL_API void
autil_bitarr_xor(
    struct autil_bitarr* res,
    struct autil_bitarr const* lhs,
    struct autil_bitarr const* rhs);
// res = lhs | rhs
// Fatally exits after printing an error message if the count of res, lhs, and
// rhs are not equal.
AUTIL_API void
autil_bitarr_or(
    struct autil_bitarr* res,
    struct autil_bitarr const* lhs,
    struct autil_bitarr const* rhs);

////////////////////////////////////////////////////////////////////////////////
//////// BIG INTEGER ///////////////////////////////////////////////////////////
// Arbitrary precision integer.
// A bigint conceptually consists of the following components:
// (1) sign: The arithmetic sign of the integer (+, -, or 0).
// (2) magnitude: The absolute value of the bigint, presented through this API
//     as an infinitely long sequence of bits with little endian ordering.

extern struct autil_bigint const* const AUTIL_BIGINT_ZERO; // 0
extern struct autil_bigint const* const AUTIL_BIGINT_POS_ONE; // +1
extern struct autil_bigint const* const AUTIL_BIGINT_NEG_ONE; // -1

// Allocate and initialize a bigint to the specified value.
// The call autil_bigint_new(AUTIL_BIGINT_ZERO) will zero-initialize a bigint.
AUTIL_API struct autil_bigint*
autil_bigint_new(struct autil_bigint const* othr);
// Allocate and initialize a bigint from the provided NUL-terminated cstring.
// Returns NULL if the cstring could not be parsed.
//
// The cstring may begin with a plus (+) or minus (-) sign.
// In the absence of a plus or minus sign the cstring will interpreted as a
// non-negative number.
//
// The digits of the cstring may be prefixed with a radix identifier:
// 0b (binary), 0o (octal), or 0x (hexadecimal).
// In the absence of a radix identifier, the digits of the cstring will decoded
// with radix 10 (decimal).
//
// The cstring *must* not have any leading or trailing whitespace.
AUTIL_API struct autil_bigint*
autil_bigint_new_cstr(char const* cstr);
// Allocate and initialize a bigint from the provided string slice.
// Returns NULL if the string could not be parsed.
// This function uses the same string-grammar as autil_bigint_new_cstr().
AUTIL_API struct autil_bigint*
autil_bigint_new_text(char const* start, size_t count);
// Deinitialize and free the bigint.
// Does nothing if self == NULL.
AUTIL_API void
autil_bigint_del(struct autil_bigint* self);
// Register resources within the bigint with the provided freezer.
AUTIL_API void
autil_bigint_freeze(struct autil_bigint* self, struct autil_freezer* freezer);

// Return an int less than, equal to, or greater than zero if lhs is
// semantically less than, equal to, or greater than rhs, respectively.
AUTIL_API int
autil_bigint_cmp(
    struct autil_bigint const* lhs, struct autil_bigint const* rhs);

// self = othr
AUTIL_API void
autil_bigint_assign(struct autil_bigint* self, struct autil_bigint const* othr);

// res = -rhs
AUTIL_API void
autil_bigint_neg(struct autil_bigint* res, struct autil_bigint const* rhs);
// res = abs(rhs)
AUTIL_API void
autil_bigint_abs(struct autil_bigint* res, struct autil_bigint const* rhs);
// res = lhs + rhs
AUTIL_API void
autil_bigint_add(
    struct autil_bigint* res,
    struct autil_bigint const* lhs,
    struct autil_bigint const* rhs);
// res = lhs - rhs
AUTIL_API void
autil_bigint_sub(
    struct autil_bigint* res,
    struct autil_bigint const* lhs,
    struct autil_bigint const* rhs);
// res  = lhs * rhs
AUTIL_API void
autil_bigint_mul(
    struct autil_bigint* res,
    struct autil_bigint const* lhs,
    struct autil_bigint const* rhs);
// res  = lhs / rhs
// rem  = lhs % rhs
// If res is NULL then the result will not be written to res.
// If rem is NULL then the remainder will not be written to rem.
//
// This function matches the behavior of the / and % operators as defined by the
// C99 standard, satisfying the expression:
//      (lhs/rhs)*rhs + lhs%rhs == lhs
// where:
//      lhs/rhs == res
//      lhs%rhs == rem
AUTIL_API void
autil_bigint_divrem(
    struct autil_bigint* res,
    struct autil_bigint* rem,
    struct autil_bigint const* lhs,
    struct autil_bigint const* rhs);

// self.magnitude = self.magnitude << nbits (logical shift left)
// This function is sign-oblivious (the sign of self is not altered).
AUTIL_API void
autil_bigint_magnitude_shiftl(struct autil_bigint* self, size_t nbits);
// self.magnitude = self.magnitude >> nbits (logical shift right)
// This function is sign-oblivious (the sign of self is not altered).
AUTIL_API void
autil_bigint_magnitude_shiftr(struct autil_bigint* self, size_t nbits);
// Returns the number of bits required to store the magnitude of self.
// This function is sign-oblivious (the sign of self is not considered).
AUTIL_API size_t
autil_bigint_magnitude_bit_count(struct autil_bigint const* self);
// Returns the value (one or zero) of the nth bit (zero indexed) of the
// magnitude of self.
// This function is sign-oblivious (the sign of self is not considered).
AUTIL_API int
autil_bigint_magnitude_bit_get(struct autil_bigint const* self, size_t n);
// Set the nth bit (zero indexed) of the magnitude of self to value.
// This function is sign-oblivious (the sign of self is not altered).
AUTIL_API void
autil_bigint_magnitude_bit_set(struct autil_bigint* self, size_t n, int value);

// Returns an autil_xalloc-allocated cstring representation of the provided
// bigint as specified by the provided format string.
// If fmt is NULL then default formatting is used.
//
// Returns a NUL-terminated string on success.
// Returns NULL if an invalid format string was provided.
//
// Format string grammar: "[flags][width][specifier]"
// Note that the format directive character, %, is *NOT* used in the format
// string grammar.
//
// Flags (optional):
//   #      Prefix the digits of the output string with "0b", "0o", "0x", or
//          "0x" when used in conjunction with the "b", "o", "x", or "X"
//          specifiers, respectively. Note that "0x" is used for both the "x"
//          and "X" specifiers.
//   0      Left pad the output string up to the field width using zeros.
//          Default behavior is to pad with spaces.
//   +      Prefix the numeric representation of the output string with a plus
//          or minus sign (+ or -), even for positive numbers.
//          Default behavior is to only add the minus sign for negative numbers.
//   -      Left justify the output string within the provided field width.
//   space  Prefix the numeric representation of the output string with a space
//          if no sign would be written otherwise.
//
// Width (optional):
//   Decimal digit string with nonzero first digit specifying the minimum length
//   of the output string.
//
// Specifier (required):
//   d      The provided bigint will be represented using decimal notation.
//   b      The provided bigint will be represented using binary notation.
//   o      The provided bigint will be represented using octal notation.
//   x      The provided bigint will be represented using hexadecimal notation
//          with *lower case* alphanumeric digits.
//   X      The provided bigint will be represented using hexadecimal notation
//          with *UPPER CASE* alphanumeric digits.
AUTIL_API char*
autil_bigint_to_new_cstr(struct autil_bigint const* self, char const* fmt);

////////////////////////////////////////////////////////////////////////////////
//////// STRING ////////////////////////////////////////////////////////////////
// Byte string with guaranteed NUL termination.

// Allocate and initialize a string from the first count bytes of start.
AUTIL_API struct autil_string*
autil_string_new(char const* start, size_t count);
// Allocate and initialize a string from the provided NUL-terminated cstring.
// If cstr is NULL then string will be initialized to the empty string.
AUTIL_API struct autil_string*
autil_string_new_cstr(char const* cstr);
// Allocate and initialize a string from the provided formatted text.
AUTIL_API struct autil_string*
autil_string_new_fmt(char const* fmt, ...);
// Deinitialize and free the string.
// Does nothing if self == NULL.
AUTIL_API void
autil_string_del(struct autil_string* self);
// Register resources within the string with the provided freezer.
AUTIL_API void
autil_string_freeze(struct autil_string* self, struct autil_freezer* freezer);

// Pointer to the start of the underlying char array of the string.
// Returns a pointer to a NUL terminator when the count of the string is zero.
AUTIL_API char const*
autil_string_start(struct autil_string const* self);
// The number of bytes in the string *NOT* including the NUL terminator.
AUTIL_API size_t
autil_string_count(struct autil_string const* self);

// Return an int less than, equal to, or greater than zero if lhs is
// lexicographically less than, equal to, or greater than rhs, respectively.
AUTIL_API int
autil_string_cmp(
    struct autil_string const* lhs, struct autil_string const* rhs);

// Update the count of the string.
// If count is greater than the current count of the string then additional
// elements are initialized with garbage data.
AUTIL_API void
autil_string_resize(struct autil_string* self, size_t count);

// Return a pointer to the byte of the string at position idx.
// Fatally exits after printing an error message if idx is out of bounds.
AUTIL_API char*
autil_string_ref(struct autil_string* self, size_t idx);
AUTIL_API char const*
autil_string_ref_const(struct autil_string const* self, size_t idx);

// Insert count bytes of start into the string at position idx.
// Bytes with position greater than idx are moved back count bytes.
// Fatally exits after printing an error message if idx is out of bounds.
AUTIL_API void
autil_string_insert(
    struct autil_string* self, size_t idx, char const* start, size_t count);
// Remove count bytes at position idx from the string.
// Bytes with position greater than idx are moved forward count bytes.
// Fatally exits after printing an error message if the slice to be removed
// indexes out of bounds.
AUTIL_API void
autil_string_remove(struct autil_string* self, size_t idx, size_t count);

// Append count bytes of start onto the end of the string.
AUTIL_API void
autil_string_append(struct autil_string* self, char const* start, size_t count);
// Append the provided NUL-terminated cstring onto the end of the string.
AUTIL_API void
autil_string_append_cstr(struct autil_string* self, char const* cstr);
// Append the formatted text to the end of the string.
AUTIL_API void
autil_string_append_fmt(struct autil_string* self, char const* fmt, ...);
AUTIL_API void
autil_string_append_vfmt(
    struct autil_string* self, char const* fmt, va_list args);

// Trim leading and trailing whitespace from the string.
// Bytes of the string are decoded using the "C" locale.
AUTIL_API void
autil_string_trim(struct autil_string* self);

// Split the string on all occurrences of whitespace.
// Empty strings are removed from the result.
// Bytes of the string are decoded using the "C" locale.
// Parameter res will be populated with the collection of resulting strings.
// Example:
//      "A B\tC  D " ===split===> "A" "B" "C" "D"
AUTIL_API void
autil_string_split_to_vec(
    struct autil_string const* self,
    struct autil_vec /*<struct autil_string*>*/* res);
// Split the string on all occurrences of the provided separator.
// Empty strings are *NOT* removed from the result.
// Parameter res will be populated with the collection of resulting strings.
//      "ABCBB" ===split on "B"===> "A" "C" "" ""
AUTIL_API void
autil_string_split_to_vec_on(
    struct autil_string const* self,
    char const* separator,
    size_t separator_size,
    struct autil_vec /*<struct autil_string*>*/* res);
AUTIL_API void
autil_string_split_to_vec_on_vstr(
    struct autil_string const* self,
    struct autil_vstr const* separator,
    struct autil_vec /*<struct autil_string*>*/* res);
AUTIL_API void
autil_string_split_to_vec_on_cstr(
    struct autil_string const* self,
    char const* separator,
    struct autil_vec /*<struct autil_string*>*/* res);

// Wrapper functions for an autil_vec of autil_string*.
// Useful for initializing and deinitializing a vec passed to
// autil_string_split_* functions.
AUTIL_API struct autil_vec /*<struct autil_string*>*/*
autil_vec_of_string_new(void);
AUTIL_API void
autil_vec_of_string_del(struct autil_vec /*<struct autil_string*>*/* vec);

////////////////////////////////////////////////////////////////////////////////
//////// VEC ///////////////////////////////////////////////////////////////////
// General purpose generic resizeable array.
// A vec conceptually consists of the following components:
// (1) data: An array containing the elements of the vec.
// (2) count: The number of elements in the vec.
// (3) capacity: The total number of elements allocated in the data array.
//     This value is always greater than or equal to the count of the vec.

// Allocate and initialize a vec holding elements of size elemsize.
AUTIL_API struct autil_vec*
autil_vec_new(size_t elemsize);
// Deinitialize and free the vec.
// Does nothing if self == NULL.
AUTIL_API void
autil_vec_del(struct autil_vec* self);

// Pointer to the start of the underlying array of the vec.
// May return NULL when the count of the vec is zero.
AUTIL_API void const*
autil_vec_start(struct autil_vec const* self);
// The number of elements in the vec.
AUTIL_API size_t
autil_vec_count(struct autil_vec const* self);
// The number of elements allocated in the vec.
AUTIL_API size_t
autil_vec_capacity(struct autil_vec const* self);
// The sizeof of elements in the vec.
AUTIL_API size_t
autil_vec_elemsize(struct autil_vec const* self);

// Update the minimum capacity of the vec.
// The count of the vec remains unchanged.
AUTIL_API void
autil_vec_reserve(struct autil_vec* self, size_t capacity);
// Update the count of the vec.
// If count is greater than the current count of the vec then additional
// elements are initialized with garbage data.
AUTIL_API void
autil_vec_resize(struct autil_vec* self, size_t count);

// Set the value of the vec *at* position idx to the value *at* data.
// Fatally exits after printing an error message if idx is out of bounds.
//
// Example:
//      struct autil_vec* const v = autil_vec_new(sizeof(int));
//      // some time later...
//      int const foo = 0xdeadbeef;
//      autil_vec_set(v, 42u, &val);
AUTIL_API void
autil_vec_set(struct autil_vec* self, size_t idx, void const* data);
// Get a pointer to the value of the vec *at* position idx.
// Fatally exits after printing an error message if idx is out of bounds.
//
// Example:
//      struct autil_vec* const v = autil_vec_new(sizeof(int));
//      // some time later...
//      int val = AUTIL_DEREF_PTR(int, autil_vec_ref(v, 42u)
AUTIL_API void*
autil_vec_ref(struct autil_vec* self, size_t idx);
AUTIL_API void const*
autil_vec_ref_const(struct autil_vec const* self, size_t idx);

// Insert a copy of the value at data into the vec at position idx.
// Elements with position greater than or equal to idx are moved back one place.
// Fatally exits after printing an error message if idx is greater than the
// count of the vec.
AUTIL_API void
autil_vec_insert(struct autil_vec* self, size_t idx, void const* data);
// Remove the element at position idx from the vec.
// Elements with position greater than idx are moved forward one place.
// If oldelem is not NULL then the removed element will be copied into oldelem.
// Fatally exits after printing an error message if idx is out of bounds.
AUTIL_API void
autil_vec_remove(struct autil_vec* self, size_t idx, void* oldelem);

// Create or advance an iterator over elements of a vec.
// If parameter iter is NULL then a new iterator pointing to the first element
// of the vec is returned. A NULL value signaling end-of-iteration will be
// returned for a vec with no elements or a vec containing zero-sized elements.
// If parameter iter is non-NULL then the element following iter is returned.
// End-of-iteration is signaled by a NULL return value.
//
// Example (using a const-iterator):
//      struct autil_vec* const v = autil_vec_new(sizeof(TYPE));
//      // some time later...
//      TYPE const* iter = autil_vec_next_const(v, NULL);
//      for (; iter != NULL; iter = autil_vec_next_const(v, iter)) {
//          do_thing(*iter);
//      }
AUTIL_API void*
autil_vec_next(struct autil_vec* self, void const* iter);
AUTIL_API void const*
autil_vec_next_const(struct autil_vec const* self, void const* iter);

////////////////////////////////////////////////////////////////////////////////
//////// MAP ///////////////////////////////////////////////////////////////////
// General purpose generic ordered map.
// Maps keys of some key-type to values of some value-type.

// Allocate and initialize a map.
// The map will hold keys of size keysize.
// The map will hold values of size valsize.
// Keys-value pairs in the map are sorted by key using keycmp.
AUTIL_API struct autil_map*
autil_map_new(size_t keysize, size_t valsize, autil_vpcmp_fn keycmp);
// Deinitialize and free the map.
// Does nothing if self == NULL.
AUTIL_API void
autil_map_del(struct autil_map* self);

// The number of key-value pairs in the map.
AUTIL_API size_t
autil_map_count(struct autil_map const* self);
// Reference to the ordered vec of keys in the map.
AUTIL_API struct autil_vec const*
autil_map_keys(struct autil_map const* self);
// Reference to the ordered vec of values in the map.
AUTIL_API struct autil_vec const*
autil_map_vals(struct autil_map const* self);

// Retrieve a pointer to the value associated with key.
// Returns NULL if the map has no key-value pair associated with key.
AUTIL_API void*
autil_map_lookup(struct autil_map* self, void const* key);
AUTIL_API void const*
autil_map_lookup_const(struct autil_map const* self, void const* key);
// Insert a copy of the key-value pair at key/val into the map.
// If a key-value pair with key already exists in the map then the existing key
// and value are replaced with the provided key and value.
//
// If oldkey is not NULL then the removed key will be copied into oldkey.
// If oldval is not NULL then the removed value will be copied into oldval.
// Returns a non-zero value if a key-value pair associated with key was replaced
// by the provided key and value.
AUTIL_API int
autil_map_insert(
    struct autil_map* self,
    void const* key,
    void const* val,
    void* oldkey,
    void* oldval);
// Remove the key-value pair associated with key.
// If no such key-value pair exists in the map then the operation is a noop.
//
// If oldkey is not NULL then the removed key will be copied into oldkey.
// If oldval is not NULL then the removed value will be copied into oldval.
// Returns a non-zero value if a key-value pair associated with key was removed.
AUTIL_API int
autil_map_remove(
    struct autil_map* self, void const* key, void* oldkey, void* oldval);

////////////////////////////////////////////////////////////////////////////////
//////// FREEZER ///////////////////////////////////////////////////////////////

// Allocate and initialize a freezer.
AUTIL_API struct autil_freezer*
autil_freezer_new(void);
// Deinitialize and free the freezer.
// Does nothing if self == NULL.
AUTIL_API void
autil_freezer_del(struct autil_freezer* self);

// Register a pointer to autil_xalloc-allocated memory to be freed when the
// freezer is deinitialized.
AUTIL_API void
autil_freezer_register(struct autil_freezer* self, void* ptr);

#endif // AUTIL_H_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//////////////////////////// IMPLEMENTATION SECTION ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#ifdef AUTIL_IMPLEMENTATION
#undef AUTIL_IMPLEMENTATION

// USE: AUTIL_REALLOC(ptr, size)
#ifndef AUTIL_REALLOC
#    define AUTIL_REALLOC realloc
#endif

// USE: AUTIL_FREE(ptr)
#ifndef AUTIL_FREE
#    define AUTIL_FREE free
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

AUTIL_STATIC_ASSERT(CHAR_BIT_IS_8, CHAR_BIT == 8);

AUTIL_API int
autil_void_vpcmp(void const* lhs, void const* rhs)
{
    (void)lhs;
    (void)rhs;
    return 0; // The void (i.e. zero-sized object) type acts as a unit type.
}

AUTIL_API int
autil_cstr_vpcmp(void const* lhs, void const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    return strcmp(*(char const**)lhs, *(char const**)rhs);
}

AUTIL_API int
autil_int_vpcmp(void const* lhs, void const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    int const l = *(int*)lhs;
    int const r = *(int*)rhs;
    if (l < r) {
        return -1;
    }
    if (l > r) {
        return +1;
    }
    return 0;
}

AUTIL_API int
autil_isalnum(int c)
{
    return autil_isalpha(c) || autil_isdigit(c);
}

AUTIL_API int
autil_isalpha(int c)
{
    return autil_isupper(c) || autil_islower(c);
}

AUTIL_API int
autil_isblank(int c)
{
    return c == ' ' || c == '\t';
}

AUTIL_API int
autil_iscntrl(int c)
{
    return (unsigned)c < 0x20 || c == 0x7f;
}

AUTIL_API int
autil_isdigit(int c)
{
    return (unsigned)c - '0' < 10;
}

AUTIL_API int
autil_isgraph(int c)
{
    return autil_isprint(c) && c != ' ';
}

AUTIL_API int
autil_islower(int c)
{
    return (unsigned)c - 'a' < 26;
}

AUTIL_API int
autil_isprint(int c)
{
    return 0x20 <= (unsigned)c && (unsigned)c <= 0x7e;
}

AUTIL_API int
autil_ispunct(int c)
{
    return autil_isgraph(c) && !autil_isalnum(c);
}

AUTIL_API int
autil_isspace(int c)
{
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t'
        || c == '\v';
}

AUTIL_API int
autil_isupper(int c)
{
    return (unsigned)c - 'A' < 26;
}

AUTIL_API int
autil_isbdigit(int c)
{
    return (unsigned)c - '0' < 2;
}

AUTIL_API int
autil_isodigit(int c)
{
    return (unsigned)c - '0' < 8;
}

AUTIL_API int
autil_isxdigit(int c)
{
    return autil_isdigit(c) || (unsigned)c - 'a' < 6 || (unsigned)c - 'A' < 6;
}

AUTIL_API int
autil_tolower(int c)
{
    if (autil_isupper(c)) {
        return c | 0x20;
    }
    return c;
}

AUTIL_API int
autil_toupper(int c)
{
    if (autil_islower(c)) {
        return c & 0x5f;
    }
    return c;
}

AUTIL_API int
autil_memcmp(void const* s1, void const* s2, size_t n)
{
    assert(s1 != NULL || n == 0);
    assert(s2 != NULL || n == 0);

    if (n == 0) {
        return 0;
    }
    return memcmp(s1, s2, n);
}

AUTIL_API void*
autil_memmove(void* dest, void const* src, size_t n)
{
    assert(dest != NULL || n == 0);
    assert(src != NULL || n == 0);

    if (n == 0) {
        return dest;
    }
    return memmove(dest, src, n);
}

AUTIL_API void*
autil_memset(void* s, int c, size_t n)
{
    assert(s != NULL || n == 0);

    if (n == 0) {
        return s;
    }
    return memset(s, c, n);
}

AUTIL_API void*
autil_xalloc(void* ptr, size_t size)
{
    if (size == 0) {
        AUTIL_FREE(ptr);
        return NULL;
    }
    if ((ptr = AUTIL_REALLOC(ptr, size)) == NULL) {
        autil_fatalf("[%s] Out of memory", __func__);
    }
    return ptr;
}

AUTIL_API void*
autil_xallocn(void* ptr, size_t nmemb, size_t size)
{
    size_t const sz = nmemb * size;
    if (nmemb != 0 && sz / nmemb != size) {
        autil_fatalf("[%s] Integer overflow", __func__);
    }
    return autil_xalloc(ptr, sz);
}

AUTIL_API void
autil_infof(char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fputs("info: ", stderr);
    vfprintf(stderr, fmt, args);
    fputs("\n", stderr);

    va_end(args);
}

AUTIL_API void
autil_errorf(char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fputs("error: ", stderr);
    vfprintf(stderr, fmt, args);
    fputs("\n", stderr);

    va_end(args);
}

AUTIL_API void
autil_fatalf(char const* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    fputs("error: ", stderr);
    vfprintf(stderr, fmt, args);
    fputs("\n", stderr);

    va_end(args);
    exit(EXIT_FAILURE);
}

// Prepend othr_size bytes from othr onto the autil_xalloc-allocated buffer of
// size *psize pointed to by *pdata, updating the address of *pdata if
// necessary.
static void
autil_xalloc_prepend(
    void** pdata, size_t* psize, void const* othr, size_t othr_size)
{
    assert(pdata != NULL);
    assert(psize != NULL);
    assert(othr != NULL || othr_size == 0);
    if (othr_size == 0) {
        // [] + [A][B][C] => [A][B][C]
        return;
    }

    size_t const new_size = *psize + othr_size;
    void* const new_data = autil_xalloc(*pdata, new_size);
    memmove((char*)new_data + othr_size, new_data, *psize);
    memcpy(new_data, othr, othr_size);

    *pdata = new_data;
    *psize = new_size;
}

// Append othr_size bytes from othr onto the autil_xalloc-allocated buffer of
// size *psize pointed to by *pdata, updating the address of *pdata if
// necessary.
static void
autil_xalloc_append(
    void** pdata, size_t* psize, void const* othr, size_t othr_size)
{
    assert(pdata != NULL);
    assert(psize != NULL);
    assert(othr != NULL || othr_size == 0);
    if (othr_size == 0) {
        // [A][B][C] + [] => [A][B][C]
        return;
    }

    size_t const new_size = *psize + othr_size;
    void* const new_data = autil_xalloc(*pdata, new_size);
    memcpy((char*)new_data + *psize, othr, othr_size);

    *pdata = new_data;
    *psize = new_size;
}

AUTIL_API int
autil_file_read(char const* path, void** buf, size_t* buf_size)
{
    assert(path != NULL);
    assert(buf != NULL);
    assert(buf_size != NULL);

    FILE* const stream = fopen(path, "rb");
    if (stream == NULL) {
        return -1;
    }

    int const err = autil_stream_read(stream, buf, buf_size);
    (void)fclose(stream);

    return err;
}

AUTIL_API int
autil_file_write(char const* path, void const* buf, size_t buf_size)
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
        // > data are discarded. Whether or not the call succeeds, the stream is
        // > disassociated from the file...
        // Cautiously assume that the buffer was not fully written to disk.
        return -1;
    }

    return 0;
}

AUTIL_API int
autil_stream_read(FILE* stream, void** buf, size_t* buf_size)
{
    assert(stream != NULL);
    assert(buf != NULL);
    assert(buf_size != NULL);

    unsigned char* bf = NULL;
    size_t sz = 0;

    int c;
    while ((c = fgetc(stream)) != EOF) {
        bf = autil_xalloc(bf, sz + 1);
        bf[sz++] = (unsigned char)c;
    }
    if (ferror(stream)) {
        autil_xalloc(bf, AUTIL_XALLOC_FREE);
        return -1;
    }

    *buf = bf;
    *buf_size = sz;
    return 0;
}

AUTIL_API int
autil_stream_read_line(FILE* stream, void** buf, size_t* buf_size)
{
    assert(stream != NULL);
    assert(buf != NULL);
    assert(buf_size != NULL);

    unsigned char* bf = NULL;
    size_t sz = 0;

    int c;
    while ((c = fgetc(stream)) != EOF) {
        bf = autil_xalloc(bf, sz + 1);
        bf[sz++] = (unsigned char)c;
        if (c == '\n') {
            break;
        }
    }
    if (ferror(stream)) {
        autil_xalloc(bf, AUTIL_XALLOC_FREE);
        return -1;
    }
    if (feof(stream) && sz == 0) {
        return -1;
    }

    *buf = bf;
    *buf_size = sz;
    return 0;
}

AUTIL_API char*
autil_cstr_new(char const* start, size_t count)
{
    assert(start != NULL || count == 0);

    char* const s = autil_xalloc(NULL, count + AUTIL_STR_LITERAL_COUNT("\0"));
    autil_memmove(s, start, count);
    s[count] = '\0';
    return s;
}

AUTIL_API char*
autil_cstr_new_cstr(char const* cstr)
{
    assert(cstr != NULL);

    size_t const count = strlen(cstr);
    char* const s = autil_xalloc(NULL, count + AUTIL_STR_LITERAL_COUNT("\0"));
    return strcpy(s, cstr);
}

AUTIL_API char*
autil_cstr_new_fmt(char const* fmt, ...)
{
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);

    va_list copy;
    va_copy(copy, args);
    int const len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (len < 0) {
        autil_fatalf("[%s] Formatting failure", __func__);
    }

    size_t size = (size_t)len + AUTIL_STR_LITERAL_COUNT("\0");
    char* const buf = autil_xalloc(NULL, size);
    vsnprintf(buf, size, fmt, args);
    va_end(args);

    return buf;
}

AUTIL_API int
autil_cstr_starts_with(char const* cstr, char const* target)
{
    assert(cstr != NULL);
    assert(target != NULL);

    return strncmp(cstr, target, strlen(target)) == 0;
}

AUTIL_API int
autil_cstr_ends_with(char const* cstr, char const* target)
{
    assert(cstr != NULL);
    assert(target != NULL);

    return autil_vstr_ends_with(
        AUTIL_VSTR_LOCAL_PTR(cstr, strlen(cstr)),
        AUTIL_VSTR_LOCAL_PTR(target, strlen(target)));
}

AUTIL_API int
autil_vstr_cmp(struct autil_vstr const* lhs, struct autil_vstr const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    size_t const n = lhs->count < rhs->count ? lhs->count : rhs->count;
    int const cmp = autil_memcmp(lhs->start, rhs->start, n);

    if (cmp != 0 || lhs->count == rhs->count) {
        return cmp;
    }
    return lhs->count < rhs->count ? -1 : +1;
}

AUTIL_API int
autil_vstr_vpcmp(void const* lhs, void const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);
    return autil_vstr_cmp(lhs, rhs);
}

AUTIL_API int
autil_vstr_starts_with(
    struct autil_vstr const* vstr, struct autil_vstr const* target)
{
    assert(vstr != NULL);
    assert(target != NULL);

    if (vstr->count < target->count) {
        return 0;
    }
    return autil_memcmp(vstr->start, target->start, target->count) == 0;
}

AUTIL_API int
autil_vstr_ends_with(
    struct autil_vstr const* vstr, struct autil_vstr const* target)
{
    assert(vstr != NULL);
    assert(target != NULL);

    if (vstr->count < target->count) {
        return 0;
    }
    char const* start = vstr->start + (vstr->count - target->count);
    return autil_memcmp(start, target->start, target->count) == 0;
}

struct autil_sipool {
    // List of heap-allocated strings interned within this pool. The key and val
    // elements of the map member reference memory owned by this list.
    autil_sbuf(char*) strings;
    // KEY TYPE: struct autil_vstr
    // VAL TYPE: char const*
    struct autil_map* map;
};

AUTIL_API struct autil_sipool*
autil_sipool_new(void)
{
    struct autil_sipool* const self =
        autil_xalloc(NULL, sizeof(struct autil_sipool));
    self->strings = NULL;
    self->map = autil_map_new(
        sizeof(struct autil_vstr), sizeof(char const*), autil_vstr_vpcmp);
    return self;
}

AUTIL_API void
autil_sipool_del(struct autil_sipool* self)
{
    if (self == NULL) {
        return;
    }

    for (size_t i = 0; i < autil_sbuf_count(self->strings); ++i) {
        autil_xalloc(self->strings[i], AUTIL_XALLOC_FREE);
    }
    autil_sbuf_fini(self->strings);
    autil_map_del(self->map);

    memset(self, 0x00, sizeof(*self)); // scrub
    autil_xalloc(self, AUTIL_XALLOC_FREE);
}

AUTIL_API char const*
autil_sipool_intern(struct autil_sipool* self, char const* start, size_t count)
{
    assert(self != NULL);
    assert(start != NULL || count == 0);

    char const* const* const pexisting =
        autil_map_lookup_const(self->map, AUTIL_VSTR_LOCAL_PTR(start, count));
    if (pexisting != NULL) {
        return *pexisting;
    }

    char* str = autil_cstr_new(start, count);
    autil_sbuf_push(self->strings, str);
    autil_map_insert(
        self->map, AUTIL_VSTR_LOCAL_PTR(str, count), &str, NULL, NULL);
    return str;
}

AUTIL_API char const*
autil_sipool_intern_cstr(struct autil_sipool* self, char const* cstr)
{
    assert(self != NULL);
    assert(cstr != NULL);

    return autil_sipool_intern(self, cstr, strlen(cstr));
}

AUTIL_STATIC_ASSERT(
    SBUF_HEADER_OFFSET_IS_ALIGNED,
    AUTIL__SBUF_HEADER_OFFSET_ % AUTIL_ALIGNOF(autil_max_align_type) == 0);

/* reserve */
AUTIL_API void*
autil__sbuf_rsv_(size_t elemsize, void* sbuf, size_t cap)
{
    assert(elemsize != 0);

    if (cap <= autil_sbuf_capacity(sbuf)) {
        return sbuf;
    }

    assert(cap != 0);
    size_t const size = AUTIL__SBUF_HEADER_OFFSET_ + elemsize * cap;
    struct autil__sbuf_header_* const header = autil_xalloc(
        sbuf != NULL ? AUTIL__SBUF_PHEAD_MUTBL_(sbuf) : NULL, size);
    header->cnt_ = sbuf != NULL ? header->cnt_ : 0;
    header->cap_ = cap;
    return (char*)header + AUTIL__SBUF_HEADER_OFFSET_;
}

/* resize */
AUTIL_API void*
autil__sbuf_rsz_(size_t elemsize, void* sbuf, size_t cnt)
{
    assert(elemsize != 0);

    if (cnt == 0) {
        autil_sbuf_fini(sbuf);
        return NULL;
    }

    if (cnt > autil_sbuf_capacity(sbuf)) {
        sbuf = autil__sbuf_rsv_(elemsize, sbuf, cnt);
    }
    assert(sbuf != NULL);
    AUTIL__SBUF_PHEAD_MUTBL_(sbuf)->cnt_ = cnt;
    return sbuf;
}

/* grow capacity by doubling */
AUTIL_API void*
autil__sbuf_grw_(size_t elemsize, void* sbuf)
{
    assert(elemsize != 0);

    size_t const cap = autil_sbuf_capacity(sbuf);
    assert(autil_sbuf_count(sbuf) == cap);

    static size_t const GROWTH_FACTOR = 2;
    static size_t const DEFAULT_CAPACITY = 8;
    size_t const new_cap = cap ? cap * GROWTH_FACTOR : DEFAULT_CAPACITY;
    return autil__sbuf_rsv_(elemsize, sbuf, new_cap);
}

#define AUTIL__BITARR_WORD_TYPE_ unsigned long
#define AUTIL__BITARR_WORD_SIZE_ sizeof(AUTIL__BITARR_WORD_TYPE_)
#define AUTIL__BITARR_WORD_BITS_ (AUTIL__BITARR_WORD_SIZE_ * CHAR_BIT)
struct autil_bitarr {
    size_t count;
    AUTIL__BITARR_WORD_TYPE_ words[];
};

static inline size_t
autil__bitarr_word_count_(size_t count)
{
    return (count / AUTIL__BITARR_WORD_SIZE_)
        + (count % AUTIL__BITARR_WORD_SIZE_ != 0);
}

static inline size_t
autil__bitarr_size_(size_t count)
{
    return sizeof(struct autil_bitarr)
        + (autil__bitarr_word_count_(count) * AUTIL__BITARR_WORD_SIZE_);
}

AUTIL_API struct autil_bitarr*
autil_bitarr_new(size_t count)
{
    size_t const size = autil__bitarr_size_(count);
    struct autil_bitarr* const self = autil_xalloc(NULL, size);
    memset(self, 0x00, size);

    self->count = count;
    return self;
}

AUTIL_API void
autil_bitarr_del(struct autil_bitarr* self)
{
    if (self == NULL) {
        return;
    }

    size_t const size = autil__bitarr_size_(self->count);
    memset(self, 0x00, size); // scrub
    autil_xalloc(self, AUTIL_XALLOC_FREE);
}

AUTIL_API size_t
autil_bitarr_count(struct autil_bitarr const* self)
{
    assert(self != NULL);

    return self->count;
}

AUTIL_API void
autil_bitarr_set(struct autil_bitarr* self, size_t n, int value)
{
    assert(self != NULL);

    if (n >= self->count) {
        autil_fatalf("[%s] Index out of bounds (%zu)", __func__, n);
    }

    AUTIL__BITARR_WORD_TYPE_* const pword =
        &self->words[n / AUTIL__BITARR_WORD_SIZE_];
    AUTIL__BITARR_WORD_TYPE_ const mask = (AUTIL__BITARR_WORD_TYPE_)1u
        << (n % AUTIL__BITARR_WORD_SIZE_);
    *pword = (AUTIL__BITARR_WORD_TYPE_)(value ? *pword | mask : *pword & ~mask);
}

AUTIL_API int
autil_bitarr_get(struct autil_bitarr const* self, size_t n)
{
    assert(self != NULL);

    if (n >= self->count) {
        autil_fatalf("[%s] Index out of bounds (%zu)", __func__, n);
    }

    AUTIL__BITARR_WORD_TYPE_ const word =
        self->words[n / AUTIL__BITARR_WORD_SIZE_];
    AUTIL__BITARR_WORD_TYPE_ const mask = (AUTIL__BITARR_WORD_TYPE_)1u
        << (n % AUTIL__BITARR_WORD_SIZE_);

    return (word & mask) != 0;
}

AUTIL_API void
autil_bitarr_assign(struct autil_bitarr* self, struct autil_bitarr const* othr)
{
    assert(self != NULL);
    assert(othr != NULL);

    if (self->count != othr->count) {
        autil_fatalf(
            "[%s] Mismatched array counts (%zu, %zu)",
            __func__,
            self->count,
            othr->count);
    }

    assert(
        autil__bitarr_size_(self->count) == autil__bitarr_size_(othr->count));
    autil_memmove(self, othr, autil__bitarr_size_(othr->count));
}

AUTIL_API void
autil_bitarr_compl(struct autil_bitarr* res, struct autil_bitarr const* rhs)
{
    assert(res != NULL);
    assert(rhs != NULL);

    if (res->count != rhs->count) {
        autil_fatalf(
            "[%s] Mismatched array counts (%zu, %zu)",
            __func__,
            res->count,
            rhs->count);
    }

    for (size_t i = 0; i < autil__bitarr_word_count_(res->count); ++i) {
        res->words[i] = ~rhs->words[i];
    }
}

AUTIL_API void
autil_bitarr_shiftl(
    struct autil_bitarr* res, struct autil_bitarr const* lhs, size_t nbits)
{
    assert(res != NULL);
    assert(lhs != NULL);

    if (res->count != lhs->count) {
        autil_fatalf(
            "[%s] Mismatched array counts (%zu, %zu)",
            __func__,
            res->count,
            lhs->count);
    }

    size_t const count = autil_bitarr_count(res);
    autil_bitarr_assign(res, lhs);
    for (size_t n = 0; n < nbits; ++n) {
        for (size_t i = count - 1; i != 0; --i) {
            autil_bitarr_set(res, i, autil_bitarr_get(res, i - 1u));
        }
        autil_bitarr_set(res, 0u, 0);
    }
}

AUTIL_API void
autil_bitarr_shiftr(
    struct autil_bitarr* res, struct autil_bitarr const* lhs, size_t nbits)
{
    assert(res != NULL);
    assert(lhs != NULL);

    if (res->count != lhs->count) {
        autil_fatalf(
            "[%s] Mismatched array counts (%zu, %zu)",
            __func__,
            res->count,
            lhs->count);
    }

    size_t const count = autil_bitarr_count(res);
    autil_bitarr_assign(res, lhs);
    for (size_t n = 0; n < nbits; ++n) {
        for (size_t i = 0; i < count - 1; ++i) {
            autil_bitarr_set(res, i, autil_bitarr_get(res, i + 1u));
        }
        autil_bitarr_set(res, count - 1, 0);
    }
}

AUTIL_API void
autil_bitarr_and(
    struct autil_bitarr* res,
    struct autil_bitarr const* lhs,
    struct autil_bitarr const* rhs)
{
    assert(res != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    if (res->count != lhs->count || res->count != rhs->count) {
        autil_fatalf(
            "[%s] Mismatched array counts (%zu, %zu, %zu)",
            __func__,
            res->count,
            lhs->count,
            rhs->count);
    }

    for (size_t i = 0; i < autil__bitarr_word_count_(res->count); ++i) {
        res->words[i] = lhs->words[i] & rhs->words[i];
    }
}

AUTIL_API void
autil_bitarr_xor(
    struct autil_bitarr* res,
    struct autil_bitarr const* lhs,
    struct autil_bitarr const* rhs)
{
    assert(res != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    if (res->count != lhs->count || res->count != rhs->count) {
        autil_fatalf(
            "[%s] Mismatched array counts (%zu, %zu, %zu)",
            __func__,
            res->count,
            lhs->count,
            rhs->count);
    }

    for (size_t i = 0; i < autil__bitarr_word_count_(res->count); ++i) {
        res->words[i] = lhs->words[i] ^ rhs->words[i];
    }
}

AUTIL_API void
autil_bitarr_or(
    struct autil_bitarr* res,
    struct autil_bitarr const* lhs,
    struct autil_bitarr const* rhs)
{
    assert(res != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    if (res->count != lhs->count || res->count != rhs->count) {
        autil_fatalf(
            "[%s] Mismatched array counts (%zu, %zu, %zu)",
            __func__,
            res->count,
            lhs->count,
            rhs->count);
    }

    for (size_t i = 0; i < autil__bitarr_word_count_(res->count); ++i) {
        res->words[i] = lhs->words[i] | rhs->words[i];
    }
}

// The internals of struct autil_bigint are designed such that initializing an
// autil_bigint with:
//      struct autil_bigint foo = {0};
// or
//      struct autil_bigint foo;
//      memset(&foo, 0x00, sizeof(foo));
// will create a bigint equal to zero without requiring heap allocation.
struct autil_bigint {
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
#define AUTIL__BIGINT_LIMB_BITS_ ((size_t)8)
AUTIL_STATIC_ASSERT(
    correct_bits_per_limb,
    AUTIL__BIGINT_LIMB_BITS_
        == (sizeof(*((struct autil_bigint*)0)->limbs) * CHAR_BIT));

struct autil_bigint const* const AUTIL_BIGINT_ZERO =
    &(struct autil_bigint){.sign = 0, .limbs = NULL, .count = 0u};
struct autil_bigint const* const AUTIL_BIGINT_POS_ONE =
    &(struct autil_bigint){.sign = +1, .limbs = (uint8_t[]){0x01}, .count = 1u};
struct autil_bigint const* const AUTIL_BIGINT_NEG_ONE =
    &(struct autil_bigint){.sign = -1, .limbs = (uint8_t[]){0x01}, .count = 1u};

static struct autil_bigint const* const AUTIL_BIGINT_DEC =
    &(struct autil_bigint){.sign = +1, .limbs = (uint8_t[]){0x0A}, .count = 1u};
static struct autil_bigint const* const AUTIL_BIGINT_BIN =
    &(struct autil_bigint){.sign = +1, .limbs = (uint8_t[]){0x02}, .count = 1u};
static struct autil_bigint const* const AUTIL_BIGINT_OCT =
    &(struct autil_bigint){.sign = +1, .limbs = (uint8_t[]){0x08}, .count = 1u};
static struct autil_bigint const* const AUTIL_BIGINT_HEX =
    &(struct autil_bigint){.sign = +1, .limbs = (uint8_t[]){0x10}, .count = 1u};

static void
autil__bigint_fini_(struct autil_bigint* self)
{
    assert(self != NULL);

    autil_xalloc(self->limbs, AUTIL_XALLOC_FREE);
    memset(self, 0x00, sizeof(*self)); // scrub
}

static void
autil__bigint_resize_(struct autil_bigint* self, size_t count)
{
    assert(self != NULL);

    if (count <= self->count) {
        self->count = count;
        return;
    }

    size_t const nlimbs = count - self->count; // Number of limbs to add.
    self->count = count;
    self->limbs = autil_xalloc(self->limbs, self->count);
    memset(self->limbs + self->count - nlimbs, 0x00, nlimbs);
}

static void
autil__bigint_normalize_(struct autil_bigint* self)
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
autil__bigint_shiftl_limbs_(struct autil_bigint* self, size_t nlimbs)
{
    assert(self != NULL);
    if (nlimbs == 0) {
        return;
    }

    self->count += nlimbs;
    self->limbs = autil_xalloc(self->limbs, self->count);
    memmove((char*)self->limbs + nlimbs, self->limbs, self->count - nlimbs);
    memset(self->limbs, 0x00, nlimbs);
}

// Shift right by nlimbs number of limbs.
// Example:
//      -0xFFEE00 shifted right by nlimbs=2 becomes -0xFF with 8-bit limbs.
static void
autil__bigint_shiftr_limbs_(struct autil_bigint* self, size_t nlimbs)
{
    assert(self != NULL);
    if (nlimbs == 0) {
        return;
    }
    if (nlimbs > self->count) {
        autil_fatalf(
            "[%s] Attempted right shift of %zu limbs on bigint with %zu limbs",
            __func__,
            nlimbs,
            self->count);
    }

    memmove((char*)self->limbs, self->limbs + nlimbs, self->count - nlimbs);
    self->count -= nlimbs;
    autil__bigint_normalize_(self);
}

// This function is "technically" part of the public API, but it is not given a
// prototype, and is intended for debugging purposes only.
// Expect this function to be removed in future versions of this header.
AUTIL_API void
autil_bigint_dump(struct autil_bigint const* self)
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

AUTIL_API struct autil_bigint*
autil_bigint_new(struct autil_bigint const* othr)
{
    assert(othr != NULL);

    struct autil_bigint* const self =
        autil_xalloc(NULL, sizeof(struct autil_bigint));
    *self = *AUTIL_BIGINT_ZERO;
    autil_bigint_assign(self, othr);
    return self;
}

AUTIL_API struct autil_bigint*
autil_bigint_new_cstr(char const* cstr)
{
    assert(cstr != NULL);

    return autil_bigint_new_text(cstr, strlen(cstr));
}

AUTIL_API struct autil_bigint*
autil_bigint_new_text(char const* start, size_t count)
{
    assert(start != NULL);

    struct autil_bigint* self = NULL;
    char const* const end = start + count;

    // Default to decimal radix.
    int radix = 10;
    struct autil_bigint const* radix_bigint = AUTIL_BIGINT_DEC;
    int (*radix_isdigit)(int c) = autil_isdigit;

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
    if ((size_t)(end - cur) < AUTIL_STR_LITERAL_COUNT("0x")) {
        // String is not long enough to have a radix identifier.
        goto digits;
    }
    if (cur[0] != '0') {
        goto digits;
    }

    if (cur[1] == 'b') {
        radix = 2;
        radix_bigint = AUTIL_BIGINT_BIN;
        radix_isdigit = autil_isbdigit;
        cur += 2;
        goto digits;
    }
    if (cur[1] == 'o') {
        radix = 8;
        radix_bigint = AUTIL_BIGINT_OCT;
        radix_isdigit = autil_isodigit;
        cur += 2;
        goto digits;
    }
    if (cur[1] == 'x') {
        radix = 16;
        radix_bigint = AUTIL_BIGINT_HEX;
        radix_isdigit = autil_isxdigit;
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

    self = autil_bigint_new(AUTIL_BIGINT_ZERO);
    cur = digits_start;
    while (cur != end) {
        errno = 0;
        uint8_t const digit_value =
            (uint8_t)strtol((char[]){(char)*cur, '\0'}, NULL, radix);
        if (errno != 0) {
            goto error;
        }

        struct autil_bigint const digit_bigint = {
            .sign = +1, .limbs = (uint8_t[]){digit_value}, .count = 1u};
        autil_bigint_mul(self, self, radix_bigint);
        autil_bigint_add(self, self, &digit_bigint);

        cur += 1;
    }

    self->sign = sign;
    autil__bigint_normalize_(self);
    return self;

error:
    autil_bigint_del(self);
    return NULL;
}

AUTIL_API void
autil_bigint_del(struct autil_bigint* self)
{
    if (self == NULL) {
        return;
    }

    autil__bigint_fini_(self);
    autil_xalloc(self, AUTIL_XALLOC_FREE);
}

AUTIL_API void
autil_bigint_freeze(struct autil_bigint* self, struct autil_freezer* freezer)
{
    assert(self != NULL);
    assert(freezer != NULL);

    autil_freezer_register(freezer, self);
    autil_freezer_register(freezer, self->limbs);
}

AUTIL_API int
autil_bigint_cmp(struct autil_bigint const* lhs, struct autil_bigint const* rhs)
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

AUTIL_API void
autil_bigint_assign(struct autil_bigint* self, struct autil_bigint const* othr)
{
    assert(self != NULL);
    assert(othr != NULL);
    if (self == othr) {
        return;
    }

    self->sign = othr->sign;
    self->limbs = autil_xalloc(self->limbs, othr->count);
    self->count = othr->count;
    if (self->sign != 0) {
        assert(self->limbs != NULL);
        assert(othr->limbs != NULL);
        memcpy(self->limbs, othr->limbs, othr->count);
    }
}

AUTIL_API void
autil_bigint_neg(struct autil_bigint* res, struct autil_bigint const* rhs)
{
    assert(res != NULL);
    assert(rhs != NULL);

    autil_bigint_assign(res, rhs);
    // +1 * -1 == -1
    // -1 * -1 == +1
    //  0 * -1 ==  0
    res->sign *= -1;
}

AUTIL_API void
autil_bigint_abs(struct autil_bigint* res, struct autil_bigint const* rhs)
{
    assert(res != NULL);
    assert(rhs != NULL);

    autil_bigint_assign(res, rhs);
    // +1 * +1 == +1
    // -1 * -1 == +1
    //  0 *  0 ==  0
    res->sign = rhs->sign * rhs->sign;
}

AUTIL_API void
autil_bigint_add(
    struct autil_bigint* res,
    struct autil_bigint const* lhs,
    struct autil_bigint const* rhs)
{
    assert(res != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    // 0 + rhs == rhs
    if (lhs->sign == 0) {
        autil_bigint_assign(res, rhs);
        return;
    }
    // lhs + 0 == lhs
    if (rhs->sign == 0) {
        autil_bigint_assign(res, lhs);
        return;
    }
    // (+lhs) + (-rhs) == (+lhs) - (+rhs)
    if ((lhs->sign == +1) && (rhs->sign == -1)) {
        struct autil_bigint* const RHS = autil_bigint_new(AUTIL_BIGINT_ZERO);
        autil_bigint_neg(RHS, rhs);
        autil_bigint_sub(res, lhs, RHS);
        autil_bigint_del(RHS);
        return;
    }
    // (-lhs) + (+rhs) == (+rhs) - (+lhs)
    if ((lhs->sign == -1) && (rhs->sign == +1)) {
        struct autil_bigint* const LHS = autil_bigint_new(AUTIL_BIGINT_ZERO);
        autil_bigint_neg(LHS, lhs);
        autil_bigint_sub(res, rhs, LHS);
        autil_bigint_del(LHS);
        return;
    }

    // (+lhs) + (+rhs) == +(lhs + rhs)
    // (-lhs) + (-rhs) == -(lhs + rhs)
    assert(lhs->sign == rhs->sign);
    int const sign = lhs->sign;

    struct autil_bigint RES = {0};
    RES.sign = sign;
    RES.count = 1 + (lhs->count > rhs->count ? lhs->count : rhs->count);
    RES.limbs = autil_xalloc(RES.limbs, RES.count);

    unsigned carry = 0;
    for (size_t i = 0; i < RES.count; ++i) {
        unsigned const lhs_limb = i < lhs->count ? lhs->limbs[i] : 0; // upcast
        unsigned const rhs_limb = i < rhs->count ? rhs->limbs[i] : 0; // upcast
        unsigned const tot = lhs_limb + rhs_limb + carry;

        RES.limbs[i] = (uint8_t)tot;
        carry = tot > UINT8_MAX;
    }
    assert(carry == 0);

    autil__bigint_normalize_(&RES);
    autil_bigint_assign(res, &RES);
    autil__bigint_fini_(&RES);
}

AUTIL_API void
autil_bigint_sub(
    struct autil_bigint* res,
    struct autil_bigint const* lhs,
    struct autil_bigint const* rhs)
{
    assert(res != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    // 0 - rhs == -(rhs)
    if (lhs->sign == 0) {
        autil_bigint_neg(res, rhs);
        return;
    }
    // lhs - 0 == lhs
    if (rhs->sign == 0) {
        autil_bigint_assign(res, lhs);
        return;
    }
    // (+lhs) - (-rhs) == (+lhs) + (+rhs)
    if ((lhs->sign == +1) && (rhs->sign == -1)) {
        struct autil_bigint* const RHS = autil_bigint_new(AUTIL_BIGINT_ZERO);
        autil_bigint_neg(RHS, rhs);
        autil_bigint_add(res, lhs, RHS);
        autil_bigint_del(RHS);
        return;
    }
    // (-lhs) - (+rhs) == (-lhs) + (-rhs)
    if ((lhs->sign == -1) && (rhs->sign == +1)) {
        struct autil_bigint* const RHS = autil_bigint_new(AUTIL_BIGINT_ZERO);
        autil_bigint_neg(RHS, rhs);
        autil_bigint_add(res, lhs, RHS);
        autil_bigint_del(RHS);
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
    int const cmp = autil_bigint_cmp(lhs, rhs);
    int const neg = ((sign == +1) && (cmp < 0)) || ((sign == -1) && (cmp > 0));
    if (neg) {
        struct autil_bigint const* tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    struct autil_bigint RES = {0};
    RES.sign = lhs->sign;
    RES.count = lhs->count > rhs->count ? lhs->count : rhs->count;
    RES.limbs = autil_xalloc(RES.limbs, RES.count);

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
        autil_bigint_neg(&RES, &RES);
    }
    autil__bigint_normalize_(&RES);
    autil_bigint_assign(res, &RES);
    autil__bigint_fini_(&RES);
}

// res  = lhs * rhs
AUTIL_API void
autil_bigint_mul(
    struct autil_bigint* res,
    struct autil_bigint const* lhs,
    struct autil_bigint const* rhs)
{
    assert(res != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);

    // 0 * rhs == 0
    if (lhs->sign == 0) {
        autil_bigint_assign(res, AUTIL_BIGINT_ZERO);
        return;
    }
    // lhs * 0 == 0
    if (rhs->sign == 0) {
        autil_bigint_assign(res, AUTIL_BIGINT_ZERO);
        return;
    }

    // Algorithm M (Multiplication of Nonnegative Integers)
    // Source: Art of Computer Programming, Volume 2: Seminumerical Algorithms
    //         (Third Edition) page. 268.
    size_t const count = lhs->count + rhs->count;
    struct autil_bigint W = {0}; // abs(res)
    autil__bigint_resize_(&W, count);
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
    autil__bigint_normalize_(&W);
    autil_bigint_assign(res, &W);
    autil__bigint_fini_(&W);
}

AUTIL_API void
autil_bigint_divrem(
    struct autil_bigint* res,
    struct autil_bigint* rem,
    struct autil_bigint const* lhs,
    struct autil_bigint const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    // lhs / 0 == undefined
    if (rhs->sign == 0) {
        autil_fatalf("[%s] Divide by zero", __func__);
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
    struct autil_bigint Q = {0}; // abs(res)
    struct autil_bigint R = {0}; // abs(rem)
    struct autil_bigint N = {0}; // abs(lhs)
    autil_bigint_abs(&N, lhs);
    struct autil_bigint D = {0}; // abs(rhs)
    autil_bigint_abs(&D, rhs);
    size_t const n = autil_bigint_magnitude_bit_count(lhs);
    for (size_t i = n - 1; i < n; --i) {
        autil_bigint_magnitude_shiftl(&R, 1);
        autil_bigint_magnitude_bit_set(
            &R, 0, autil_bigint_magnitude_bit_get(&N, i));
        if (autil_bigint_cmp(&R, &D) >= 0) {
            autil_bigint_sub(&R, &R, &D);
            autil_bigint_magnitude_bit_set(&Q, i, 1);
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
        autil__bigint_normalize_(&Q);
        autil_bigint_assign(res, &Q);
    }
    if (rem != NULL) {
        autil__bigint_normalize_(&R);
        autil_bigint_assign(rem, &R);
    }
    autil__bigint_fini_(&Q);
    autil__bigint_fini_(&R);
    autil__bigint_fini_(&N);
    autil__bigint_fini_(&D);
}

AUTIL_API void
autil_bigint_magnitude_shiftl(struct autil_bigint* self, size_t nbits)
{
    assert(self != NULL);
    if (nbits == 0) {
        return;
    }
    if (self->sign == 0) {
        return;
    }

    autil__bigint_shiftl_limbs_(self, nbits / AUTIL__BIGINT_LIMB_BITS_);
    for (size_t n = 0; n < nbits % AUTIL__BIGINT_LIMB_BITS_; ++n) {
        if (self->limbs[self->count - 1] & 0x80) {
            self->count += 1;
            self->limbs = autil_xalloc(self->limbs, self->count);
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

AUTIL_API void
autil_bigint_magnitude_shiftr(struct autil_bigint* self, size_t nbits)
{
    assert(self != NULL);
    if (nbits == 0) {
        return;
    }

    if (nbits >= autil_bigint_magnitude_bit_count(self)) {
        autil_bigint_assign(self, AUTIL_BIGINT_ZERO);
        return;
    }

    autil__bigint_shiftr_limbs_(self, nbits / AUTIL__BIGINT_LIMB_BITS_);
    for (size_t n = 0; n < nbits % AUTIL__BIGINT_LIMB_BITS_; ++n) {
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
    autil__bigint_normalize_(self);
}

AUTIL_API size_t
autil_bigint_magnitude_bit_count(struct autil_bigint const* self)
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
    return (self->count - 1) * AUTIL__BIGINT_LIMB_BITS_ + top_bit_count;
}

AUTIL_API int
autil_bigint_magnitude_bit_get(struct autil_bigint const* self, size_t n)
{
    assert(self != NULL);

    if (n >= (self->count * AUTIL__BIGINT_LIMB_BITS_)) {
        return 0;
    }

    uint8_t const limb = self->limbs[n / AUTIL__BIGINT_LIMB_BITS_];
    uint8_t const mask = (uint8_t)(1u << (n % AUTIL__BIGINT_LIMB_BITS_));
    return (limb & mask) != 0;
}

AUTIL_API void
autil_bigint_magnitude_bit_set(struct autil_bigint* self, size_t n, int value)
{
    assert(self != NULL);

    size_t const limb_idx = (n / AUTIL__BIGINT_LIMB_BITS_);
    if (limb_idx >= self->count) {
        if (!value) {
            // The abstact unallocated bit is already zero so re-setting it to
            // zero does not change the representation of self. Return early
            // rather than going through the trouble of resizeing and then
            // normalizing for what is essentially a NOP.
            return;
        }
        autil__bigint_resize_(self, limb_idx + 1);
    }

    uint8_t* const plimb = self->limbs + limb_idx;
    uint8_t const mask = (uint8_t)(1 << (n % AUTIL__BIGINT_LIMB_BITS_));
    *plimb = (uint8_t)(value ? *plimb | mask : *plimb & ~mask);
    if (self->sign == 0 && value) {
        // If the integer was zero (i.e. had sign zero) before and a bit was
        // just flipped "on" then treat that integer as it if turned from the
        // integer zero to a positive integer.
        self->sign = +1;
    }
    autil__bigint_normalize_(self);
}

// clang-format off
#define AUTIL_BIGINT_FMT_FLAG_HASH_  ((unsigned)0)
#define AUTIL_BIGINT_FMT_FLAG_ZERO_  ((unsigned)1)
#define AUTIL_BIGINT_FMT_FLAG_PLUS_  ((unsigned)2)
#define AUTIL_BIGINT_FMT_FLAG_MINUS_ ((unsigned)3)
#define AUTIL_BIGINT_FMT_FLAG_SPACE_ ((unsigned)4)
// clang-format on
AUTIL_API char*
autil_bigint_to_new_cstr(struct autil_bigint const* self, char const* fmt)
{
    assert(self != NULL);

    // Parse format string.
    unsigned flags = 0;
    size_t width = 0;
    char specifier = 'd';
    if (fmt != NULL) {
        // Flags
        while (*fmt != '\0' && strchr("#0+- ", *fmt) != NULL) {
            flags |= (unsigned)(*fmt == '#') << AUTIL_BIGINT_FMT_FLAG_HASH_;
            flags |= (unsigned)(*fmt == '0') << AUTIL_BIGINT_FMT_FLAG_ZERO_;
            flags |= (unsigned)(*fmt == '+') << AUTIL_BIGINT_FMT_FLAG_PLUS_;
            flags |= (unsigned)(*fmt == '-') << AUTIL_BIGINT_FMT_FLAG_MINUS_;
            flags |= (unsigned)(*fmt == ' ') << AUTIL_BIGINT_FMT_FLAG_SPACE_;
            fmt += 1;
        }
        // Width
        char* fmt_ =
            (char*)fmt; // Needed due to broken const behavior of strtol.
        if (autil_isdigit(*fmt)) {
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
        if (flags & (1u << AUTIL_BIGINT_FMT_FLAG_MINUS_)) {
            flags &= (unsigned)~(1u << AUTIL_BIGINT_FMT_FLAG_ZERO_);
        }
    }

    // Prefix.
    void* prefix = NULL;
    size_t prefix_size = 0;
    if (self->sign == +1 && (flags & (1u << AUTIL_BIGINT_FMT_FLAG_PLUS_))) {
        autil_xalloc_append(&prefix, &prefix_size, "+", 1);
    }
    if (self->sign == +1 && (flags & (1u << AUTIL_BIGINT_FMT_FLAG_SPACE_))) {
        autil_xalloc_append(&prefix, &prefix_size, " ", 1);
    }
    if (self->sign == -1) {
        autil_xalloc_append(&prefix, &prefix_size, "-", 1);
    }
    if (flags & (1u << AUTIL_BIGINT_FMT_FLAG_HASH_)) {
        if (specifier == 'b') {
            autil_xalloc_append(&prefix, &prefix_size, "0b", 2);
        }
        if (specifier == 'o') {
            autil_xalloc_append(&prefix, &prefix_size, "0o", 2);
        }
        if (specifier == 'x') {
            autil_xalloc_append(&prefix, &prefix_size, "0x", 2);
        }
        if (specifier == 'X') {
            autil_xalloc_append(&prefix, &prefix_size, "0x", 2);
        }
    }

    // Digits.
    void* digits = NULL;
    size_t digits_size = 0;
    char digit_buf[AUTIL__BIGINT_LIMB_BITS_ + AUTIL_STR_LITERAL_COUNT("\0")] = {
        0};
    if (specifier == 'd') {
        struct autil_bigint DEC = {0};
        struct autil_bigint SELF = {0};
        autil_bigint_abs(&SELF, self);
        while (autil_bigint_cmp(&SELF, AUTIL_BIGINT_ZERO) != 0) {
            autil_bigint_divrem(&SELF, &DEC, &SELF, AUTIL_BIGINT_DEC);
            assert(DEC.count <= 1);
            assert(DEC.limbs == NULL || DEC.limbs[0] < 10);
            sprintf(digit_buf, "%d", DEC.limbs != NULL ? (int)DEC.limbs[0] : 0);
            autil_xalloc_prepend(
                &digits, &digits_size, digit_buf, strlen(digit_buf));
        }
        autil__bigint_fini_(&DEC);
        autil__bigint_fini_(&SELF);
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
            autil_xalloc_append(
                &digits, &digits_size, digit_buf, strlen(digit_buf));
        }
    }
    else if (specifier == 'o') {
        struct autil_bigint OCT = {0};
        struct autil_bigint SELF = {0};
        autil_bigint_abs(&SELF, self);
        while (autil_bigint_cmp(&SELF, AUTIL_BIGINT_ZERO) != 0) {
            autil_bigint_divrem(&SELF, &OCT, &SELF, AUTIL_BIGINT_OCT);
            assert(OCT.count <= 1);
            assert(OCT.limbs == NULL || OCT.limbs[0] < 8);
            sprintf(digit_buf, "%o", OCT.limbs != NULL ? (int)OCT.limbs[0] : 0);
            autil_xalloc_prepend(
                &digits, &digits_size, digit_buf, strlen(digit_buf));
        }
        autil__bigint_fini_(&OCT);
        autil__bigint_fini_(&SELF);
    }
    else if (specifier == 'x') {
        for (size_t i = self->count - 1; i < self->count; --i) {
            sprintf(digit_buf, "%02x", (int)self->limbs[i]);
            autil_xalloc_append(
                &digits, &digits_size, digit_buf, strlen(digit_buf));
        }
    }
    else if (specifier == 'X') {
        for (size_t i = self->count - 1; i < self->count; --i) {
            sprintf(digit_buf, "%02X", (int)self->limbs[i]);
            autil_xalloc_append(
                &digits, &digits_size, digit_buf, strlen(digit_buf));
        }
    }
    else {
        autil_fatalf("Unreachable!");
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
        autil_xalloc_append(&digits, &digits_size, "0", 1);
    }

    // Width.
    void* widths = NULL;
    size_t widths_size = 0;
    if ((prefix_size + digits_size) < width) {
        char pad = ' ';
        if (flags & (1u << AUTIL_BIGINT_FMT_FLAG_ZERO_)) {
            assert(!(flags & (1u << AUTIL_BIGINT_FMT_FLAG_MINUS_)));
            pad = '0';
        }
        widths_size = width - (prefix_size + digits_size);
        widths = autil_xalloc(widths, widths_size);
        memset(widths, pad, widths_size);

        if (flags & (1u << AUTIL_BIGINT_FMT_FLAG_ZERO_)) {
            assert(!(flags & (1u << AUTIL_BIGINT_FMT_FLAG_MINUS_)));
            autil_xalloc_prepend(&digits, &digits_size, widths, widths_size);
        }
        else if (flags & (1u << AUTIL_BIGINT_FMT_FLAG_MINUS_)) {
            assert(!(flags & (1u << AUTIL_BIGINT_FMT_FLAG_ZERO_)));
            autil_xalloc_append(&digits, &digits_size, widths, widths_size);
        }
        else {
            autil_xalloc_prepend(&prefix, &prefix_size, widths, widths_size);
        }
    }

    void* cstr = NULL;
    size_t cstr_size = 0;
    autil_xalloc_append(&cstr, &cstr_size, prefix, prefix_size);
    autil_xalloc_append(&cstr, &cstr_size, digits, digits_size);
    autil_xalloc_append(&cstr, &cstr_size, "\0", 1);
    autil_xalloc(prefix, AUTIL_XALLOC_FREE);
    autil_xalloc(digits, AUTIL_XALLOC_FREE);
    autil_xalloc(widths, AUTIL_XALLOC_FREE);
    return cstr;
}

struct autil_string {
    char* start;
    size_t count;
};
#define AUTIL_STRING_SIZE_(count_) (count_ + AUTIL_STR_LITERAL_COUNT("\0"))

AUTIL_API struct autil_string*
autil_string_new(char const* start, size_t count)
{
    assert(start != NULL || count == 0);

    struct autil_string* const self =
        autil_xalloc(NULL, sizeof(struct autil_string));

    self->start = autil_xalloc(NULL, AUTIL_STRING_SIZE_(count));
    self->count = count;

    if (start != NULL) {
        memcpy(self->start, start, count);
    }
    self->start[self->count] = '\0';

    return self;
}

AUTIL_API struct autil_string*
autil_string_new_cstr(char const* cstr)
{
    if (cstr == NULL) {
        cstr = "";
    }
    return autil_string_new(cstr, strlen(cstr));
}

AUTIL_API struct autil_string*
autil_string_new_fmt(char const* fmt, ...)
{
    assert(fmt != NULL);

    struct autil_string* const self = autil_string_new(NULL, 0);

    va_list args;
    va_start(args, fmt);
    autil_string_append_vfmt(self, fmt, args);
    va_end(args);

    return self;
}

AUTIL_API void
autil_string_del(struct autil_string* self)
{
    if (self == NULL) {
        return;
    }

    autil_xalloc(self->start, AUTIL_XALLOC_FREE);
    memset(self, 0x00, sizeof(*self)); // scrub
    autil_xalloc(self, AUTIL_XALLOC_FREE);
}

AUTIL_API void
autil_string_freeze(struct autil_string* self, struct autil_freezer* freezer)
{
    assert(self != NULL);
    assert(freezer != NULL);

    autil_freezer_register(freezer, self);
    autil_freezer_register(freezer, self->start);
}

AUTIL_API char const*
autil_string_start(struct autil_string const* self)
{
    assert(self != NULL);

    return self->start;
}

AUTIL_API size_t
autil_string_count(struct autil_string const* self)
{
    assert(self != NULL);

    return self->count;
}

AUTIL_API int
autil_string_cmp(struct autil_string const* lhs, struct autil_string const* rhs)
{
    assert(lhs != NULL);
    assert(rhs != NULL);

    size_t const n = lhs->count < rhs->count ? lhs->count : rhs->count;
    int const cmp = autil_memcmp(lhs->start, rhs->start, n);

    if (cmp != 0 || lhs->count == rhs->count) {
        return cmp;
    }
    return lhs->count < rhs->count ? -1 : +1;
}

AUTIL_API void
autil_string_resize(struct autil_string* self, size_t count)
{
    assert(self != NULL);

    if (count > self->count) {
        self->start = autil_xalloc(self->start, AUTIL_STRING_SIZE_(count));
        char* const fill_start = self->start + AUTIL_STRING_SIZE_(self->count);
        size_t const fill_count = count - self->count;
        memset(fill_start, 0x00, fill_count); // Fill new space with NULs.
    }
    self->count = count;
    self->start[self->count] = '\0';
}

AUTIL_API char*
autil_string_ref(struct autil_string* self, size_t idx)
{
    assert(self != NULL);

    if (idx >= self->count) {
        autil_fatalf("[%s] Index out of bounds (%zu)", __func__, idx);
    }
    return &self->start[idx];
}

AUTIL_API char const*
autil_string_ref_const(struct autil_string const* self, size_t idx)
{
    assert(self != NULL);

    if (idx >= self->count) {
        autil_fatalf("[%s] Index out of bounds (%zu)", __func__, idx);
    }
    return &self->start[idx];
}

AUTIL_API void
autil_string_insert(
    struct autil_string* self, size_t idx, char const* start, size_t count)
{
    assert(self != NULL);
    assert(start != NULL || count == 0);
    if (idx > self->count) {
        autil_fatalf("[%s] Invalid index %zu", __func__, idx);
    }

    if (count == 0) {
        return;
    }
    size_t const mov_count = self->count - idx;
    autil_string_resize(self, self->count + count);
    memmove(self->start + idx + count, self->start + idx, mov_count);
    memmove(self->start + idx, start, count);
}

AUTIL_API void
autil_string_remove(struct autil_string* self, size_t idx, size_t count)
{
    assert(self != NULL);
    if ((idx + count) > self->count) {
        autil_fatalf("[%s] Invalid index,count %zu,%zu", __func__, idx, count);
    }

    if (count == 0) {
        return;
    }
    memmove(self->start + idx, self->start + idx + count, self->count - count);
    autil_string_resize(self, self->count - count);
}

AUTIL_API void
autil_string_append(struct autil_string* self, char const* start, size_t count)
{
    assert(self != NULL);

    autil_string_insert(self, autil_string_count(self), start, count);
}

AUTIL_API void
autil_string_append_cstr(struct autil_string* self, char const* cstr)
{
    assert(self != NULL);

    autil_string_insert(self, autil_string_count(self), cstr, strlen(cstr));
}

AUTIL_API void
autil_string_append_fmt(struct autil_string* self, char const* fmt, ...)
{
    assert(self != NULL);
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);
    autil_string_append_vfmt(self, fmt, args);
    va_end(args);
}

AUTIL_API void
autil_string_append_vfmt(
    struct autil_string* self, char const* fmt, va_list args)
{
    assert(self != NULL);
    assert(fmt != NULL);

    va_list copy;
    va_copy(copy, args);
    int const len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (len < 0) {
        autil_fatalf("[%s] Formatting failure", __func__);
    }

    size_t size = (size_t)len + AUTIL_STR_LITERAL_COUNT("\0");
    char* const buf = autil_xalloc(NULL, size);
    vsnprintf(buf, size, fmt, args);
    autil_string_append(self, buf, (size_t)len);
    autil_xalloc(buf, AUTIL_XALLOC_FREE);
}

AUTIL_API void
autil_string_trim(struct autil_string* self)
{
    assert(self != NULL);
    size_t n;

    // Trim leading characters.
    n = 0;
    while (n < self->count && autil_isspace(self->start[n])) {
        n += 1;
    }
    if (n != 0) {
        autil_string_remove(self, 0, n);
    }

    // Trim trailing characters.
    n = self->count;
    while (n > 0 && autil_isspace(self->start[n - 1])) {
        n -= 1;
    }
    autil_string_resize(self, n);
}

// Should be equivalent to str.split(sep=None) from Python3.
AUTIL_API void
autil_string_split_to_vec(
    struct autil_string const* self, struct autil_vec* res)
{
    assert(self != NULL);
    assert(res != NULL);
    assert(autil_vec_elemsize(res) == sizeof(struct autil_string*));

    autil_vec_resize(res, 0);

    size_t first = 0;
    while (first < self->count) {
        if (autil_isspace(self->start[first])) {
            first += 1;
            continue;
        }

        size_t end = first;
        while (end < self->count && !autil_isspace(self->start[end])) {
            end += 1;
        }
        struct autil_string* const s =
            autil_string_new(self->start + first, end - first);
        autil_vec_insert(res, autil_vec_count(res), &s);
        first = end;
    }
}

AUTIL_API void
autil_string_split_to_vec_on(
    struct autil_string const* self,
    char const* separator,
    size_t separator_size,
    struct autil_vec* res)
{
    assert(self != NULL);
    assert(res != NULL);
    assert(autil_vec_elemsize(res) == sizeof(struct autil_string*));

    autil_vec_resize(res, 0);
    if (separator_size == 0) {
        struct autil_string* const s =
            autil_string_new(self->start, self->count);
        autil_vec_insert(res, autil_vec_count(res), &s);
        return;
    }

    char const* const end_of_string = self->start + self->count;
    char const* beg = self->start;
    char const* end = beg;
    while ((size_t)(end_of_string - end) >= separator_size) {
        if (memcmp(end, separator, separator_size) != 0) {
            end += 1;
            continue;
        }
        struct autil_string* const s =
            autil_string_new(beg, (size_t)(end - beg));
        autil_vec_insert(res, autil_vec_count(res), &s);
        beg = end + separator_size;
        end = beg;
    }
    struct autil_string* const s =
        autil_string_new(beg, (size_t)(end_of_string - beg));
    autil_vec_insert(res, autil_vec_count(res), &s);
}

AUTIL_API void
autil_string_split_to_vec_on_vstr(
    struct autil_string const* self,
    struct autil_vstr const* separator,
    struct autil_vec /* struct autil_string* */* res)
{
    assert(self != NULL);
    assert(separator != NULL);
    assert(res != NULL);
    assert(autil_vec_elemsize(res) == sizeof(struct autil_string*));

    autil_string_split_to_vec_on(self, separator->start, separator->count, res);
}

AUTIL_API void
autil_string_split_to_vec_on_cstr(
    struct autil_string const* self,
    char const* separator,
    struct autil_vec* res)
{
    assert(self != NULL);
    assert(separator != NULL);
    assert(res != NULL);
    assert(autil_vec_elemsize(res) == sizeof(struct autil_string*));

    autil_string_split_to_vec_on(self, separator, strlen(separator), res);
}

AUTIL_API struct autil_vec*
autil_vec_of_string_new(void)
{
    return autil_vec_new(sizeof(struct autil_string*));
}

AUTIL_API void
autil_vec_of_string_del(struct autil_vec* vec)
{
    for (size_t i = 0; i < autil_vec_count(vec); ++i) {
        struct autil_string** const ref = autil_vec_ref(vec, i);
        autil_string_del(*ref);
    }
    autil_vec_del(vec);
}

struct autil_vec {
    void* start;
    size_t count;
    size_t capacity;
    size_t elemsize;
};

AUTIL_API struct autil_vec*
autil_vec_new(size_t elemsize)
{
    struct autil_vec* const self = autil_xalloc(NULL, sizeof(struct autil_vec));
    self->start = NULL;
    self->count = 0;
    self->capacity = 0;
    self->elemsize = elemsize;
    return self;
}

AUTIL_API void
autil_vec_del(struct autil_vec* self)
{
    if (self == NULL) {
        return;
    }

    autil_xalloc(self->start, AUTIL_XALLOC_FREE);
    memset(self, 0x00, sizeof(*self)); // scrub
    autil_xalloc(self, AUTIL_XALLOC_FREE);
}

AUTIL_API void const*
autil_vec_start(struct autil_vec const* self)
{
    assert(self != NULL);

    return self->start;
}

AUTIL_API size_t
autil_vec_count(struct autil_vec const* self)
{
    assert(self != NULL);

    return self->count;
}

AUTIL_API size_t
autil_vec_capacity(struct autil_vec const* self)
{
    assert(self != NULL);

    return self->capacity;
}

AUTIL_API size_t
autil_vec_elemsize(struct autil_vec const* self)
{
    assert(self != NULL);

    return self->elemsize;
}

AUTIL_API void
autil_vec_reserve(struct autil_vec* self, size_t capacity)
{
    assert(self != NULL);

    if (capacity <= self->capacity) {
        return;
    }

    self->start = autil_xallocn(self->start, capacity, self->elemsize);
    self->capacity = capacity;
}

AUTIL_API void
autil_vec_resize(struct autil_vec* self, size_t count)
{
    assert(self != NULL);

    if (count > self->capacity) {
        autil_vec_reserve(self, count);
    }

    self->count = count;
}

AUTIL_API void
autil_vec_set(struct autil_vec* self, size_t idx, void const* data)
{
    assert(self != NULL);

    if (idx >= self->count) {
        autil_fatalf("[%s] Index out of bounds (%zu)", __func__, idx);
    }
    if (self->elemsize == 0) {
        return;
    }

    void* const ref = ((char*)self->start) + (idx * self->elemsize);
    memmove(ref, data, self->elemsize);
}

AUTIL_API void*
autil_vec_ref(struct autil_vec* self, size_t idx)
{
    assert(self != NULL);

    if (idx >= self->count) {
        autil_fatalf("[%s] Index out of bounds (%zu)", __func__, idx);
    }
    if (self->elemsize == 0) {
        return NULL;
    }

    return ((char*)self->start) + (idx * self->elemsize);
}

AUTIL_API void const*
autil_vec_ref_const(struct autil_vec const* self, size_t idx)
{
    assert(self != NULL);

    if (idx >= self->count) {
        autil_fatalf("[%s] Index out of bounds (%zu)", __func__, idx);
    }
    if (self->elemsize == 0) {
        return NULL;
    }

    return ((char*)self->start) + (idx * self->elemsize);
}

AUTIL_API void
autil_vec_insert(struct autil_vec* self, size_t idx, void const* data)
{
    assert(self != NULL);

    if (idx > self->count) {
        autil_fatalf("[%s] Invalid index %zu", __func__, idx);
    }
    if (self->elemsize == 0) {
        self->count += 1;
        return;
    }

    // [A][B][C][D][E]
    // [A][B][C][D][E][ ]
    if (self->count == self->capacity) {
        static size_t const GROWTH_FACTOR = 2;
        static size_t const DEFAULT_CAPACITY = 32;
        size_t const capacity =
            self->count ? self->count * GROWTH_FACTOR : DEFAULT_CAPACITY;
        autil_vec_reserve(self, capacity);
    }

    // [A][B][C][D][E][ ]
    // [A][B][ ][C][D][E]
    size_t const move_count = self->count - idx;
    size_t const move_size = move_count * self->elemsize;
    void* const move_src = ((char*)self->start) + (idx * self->elemsize);
    void* const move_dst = ((char*)move_src) + 1 * self->elemsize;
    memmove(move_dst, move_src, move_size);
    self->count += 1;

    // [A][B][ ][C][D][E]
    // [A][B][X][C][D][E]
    autil_vec_set(self, idx, data);
}

AUTIL_API void
autil_vec_remove(struct autil_vec* self, size_t idx, void* oldelem)
{
    assert(self != NULL);
    if (idx >= self->count) {
        autil_fatalf("[%s] Invalid index %zu", __func__, idx);
    }
    if (self->elemsize == 0) {
        self->count -= 1;
        return;
    }

    if (oldelem != NULL) {
        memcpy(oldelem, autil_vec_ref(self, idx), self->elemsize);
    }

    // [A][B][X][C][D][E]
    // [A][B][C][D][E][ ]
    size_t const move_count = (self->count - 1) - idx;
    size_t const move_size = move_count * self->elemsize;
    void* const move_dst = ((char*)self->start) + (idx * self->elemsize);
    void* const move_src = ((char*)move_dst) + 1 * self->elemsize;
    memmove(move_dst, move_src, move_size);
    self->count -= 1;
}

AUTIL_API void*
autil_vec_next(struct autil_vec* self, void const* iter)
{
    assert(self != NULL);

    if (self->count == 0 || self->elemsize == 0) {
        return NULL;
    }

    if (iter == NULL) {
        return autil_vec_ref(self, 0);
    }

    size_t const iter_idx =
        ((size_t)((char*)iter - (char*)self->start)) / self->elemsize;
    size_t const next_idx = iter_idx + 1;
    assert(next_idx <= self->count);
    return next_idx != self->count ? autil_vec_ref(self, next_idx) : NULL;
}

AUTIL_API void const*
autil_vec_next_const(struct autil_vec const* self, void const* iter)
{
    assert(self != NULL);

    if (self->count == 0 || self->elemsize == 0) {
        return NULL;
    }

    if (iter == NULL) {
        return autil_vec_ref_const(self, 0);
    }

    size_t const iter_idx =
        ((size_t)((char*)iter - (char*)self->start)) / self->elemsize;
    size_t const next_idx = iter_idx + 1;
    assert(next_idx <= self->count);
    return next_idx != self->count ? autil_vec_ref_const(self, next_idx) : NULL;
}

struct autil_map {
    struct autil_vec* keys;
    struct autil_vec* vals;
    autil_vpcmp_fn keycmp;
};

AUTIL_API struct autil_map*
autil_map_new(size_t keysize, size_t valsize, autil_vpcmp_fn keycmp)
{
    struct autil_map* const self = autil_xalloc(NULL, sizeof(struct autil_map));
    self->keys = autil_vec_new(keysize);
    self->vals = autil_vec_new(valsize);
    self->keycmp = keycmp;
    return self;
}

AUTIL_API void
autil_map_del(struct autil_map* self)
{
    if (self == NULL) {
        return;
    }

    autil_vec_del(self->keys);
    autil_vec_del(self->vals);
    memset(self, 0x00, sizeof(*self)); // scrub
    autil_xalloc(self, AUTIL_XALLOC_FREE);
}

AUTIL_API size_t
autil_map_count(struct autil_map const* self)
{
    assert(self != NULL);

    return autil_vec_count(self->keys);
}

AUTIL_API struct autil_vec const*
autil_map_keys(struct autil_map const* self)
{
    assert(self != NULL);

    return self->keys;
}

AUTIL_API struct autil_vec const*
autil_map_vals(struct autil_map const* self)
{
    assert(self != NULL);

    return self->vals;
}

// Returns the (positive) index of key if it exists in self.
// Returns a negative number that, if negated and subtracting one from, would be
// the index of key if inserted (I.E -1 means insert at 0 and -42 means insert
// at 41).
static long
autil_map_find_(struct autil_map const* self, void const* key)
{
    assert(self != NULL);
    assert(autil_map_count(self) <= LONG_MAX);

    if (autil_map_count(self) == 0) {
        return -1;
    }

    long bot = 0;
    long top = (long)autil_map_count(self) - 1;
    long mid;
    while (bot <= top) {
        mid = (bot + top) / 2;
        void* const midkey = autil_vec_ref(self->keys, (size_t)mid);
        int const cmp = self->keycmp(midkey, key);
        if (cmp == 0) {
            return mid;
        }
        if (cmp < 0) {
            bot = mid + 1;
            continue;
        }
        if (cmp > 0) {
            top = mid - 1;
            continue;
        }
        autil_fatalf("Unreachable!");
    }

    return (-1 * bot) - 1;
}

AUTIL_API void*
autil_map_lookup(struct autil_map* self, void const* key)
{
    assert(self != NULL);

    long const location = autil_map_find_(self, key);
    if (location < 0) {
        return NULL;
    }
    return autil_vec_ref(self->vals, (size_t)location);
}

AUTIL_API void const*
autil_map_lookup_const(struct autil_map const* self, void const* key)
{
    assert(self != NULL);

    long const location = autil_map_find_(self, key);
    if (location < 0) {
        return NULL;
    }
    return autil_vec_ref(self->vals, (size_t)location);
}

AUTIL_API int
autil_map_insert(
    struct autil_map* self,
    void const* key,
    void const* val,
    void* oldkey,
    void* oldval)
{
    assert(self != NULL);

    long const location = autil_map_find_(self, key);
    if (location < 0) {
        size_t const idx = (size_t)(-1 * location) - 1;
        autil_vec_insert(self->keys, idx, key);
        autil_vec_insert(self->vals, idx, val);
        return 0;
    }

    size_t const idx = (size_t)location;
    if (oldkey != NULL) {
        memcpy(oldkey, autil_vec_ref(self->keys, idx), self->keys->elemsize);
    }
    if (oldval != NULL) {
        memcpy(oldval, autil_vec_ref(self->vals, idx), self->vals->elemsize);
    }
    autil_vec_set(self->keys, idx, key);
    autil_vec_set(self->vals, idx, val);
    return 1;
}

AUTIL_API int
autil_map_remove(
    struct autil_map* self, void const* key, void* oldkey, void* oldval)
{
    assert(self != NULL);

    long const location = autil_map_find_(self, key);
    if (location < 0) {
        return 0;
    }

    size_t const idx = (size_t)location;
    autil_vec_remove(self->keys, idx, oldkey);
    autil_vec_remove(self->vals, idx, oldval);
    return 1;
}

struct autil_freezer {
    // List of heap-allocated pointers to be freed when objects are cleaned out
    // of the freezer.
    autil_sbuf(void*) ptrs;
};

AUTIL_API struct autil_freezer*
autil_freezer_new(void)
{
    struct autil_freezer* const self =
        autil_xalloc(NULL, sizeof(struct autil_sipool));
    self->ptrs = NULL;
    return self;
}

AUTIL_API void
autil_freezer_del(struct autil_freezer* self)
{
    if (self == NULL) {
        return;
    }

    for (size_t i = 0; i < autil_sbuf_count(self->ptrs); ++i) {
        autil_xalloc(self->ptrs[i], AUTIL_XALLOC_FREE);
    }
    autil_sbuf_fini(self->ptrs);

    autil_xalloc(self, AUTIL_XALLOC_FREE);
}

AUTIL_API void
autil_freezer_register(struct autil_freezer* self, void* ptr)
{
    assert(self != NULL);

    autil_sbuf_push(self->ptrs, ptr);
}

#endif // AUTIL_IMPLEMENTATION
