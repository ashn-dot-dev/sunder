// Copyright 2021 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#ifndef SUNDER_H_INCLUDED
#define SUNDER_H_INCLUDED

#include <stdarg.h> /* va_list */
#include <stdbool.h>
#include <stddef.h> /* size_t, NULL, offsetof */
#include <stdint.h>
#include <stdio.h> /* FILE*, printf-family */

////////////////////////////////////////////////////////////////////////////////
//////// util.c ////////////////////////////////////////////////////////////////

struct vstr;
struct sunder_sipool;
struct sunder_bitarr;
struct sunder_bigint;
struct sunder_string;
struct sunder_vec;
struct sunder_map;
struct sunder_freezer;

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
} sunder_max_align_type;
// clang-format on

// Number of elements in an array.
#define SUNDER_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))
// Number of characters in a string literal, excluding the NUL-terminator.
#define SUNDER_STR_LITERAL_COUNT(str_literal)                                  \
    (SUNDER_ARRAY_COUNT(str_literal) - 1)

// C99 compatible _Alignof operator.
// Produces an integer constant expression.
// clang-format off
#define SUNDER_ALIGNOF(type) offsetof(struct{char _; type ty;}, ty)
// clang-format on

// C99 compatible(ish) _Static_assert.
// Macro parameter what should be a valid identifier describing the assertion.
// Flips the order of arguments from C11's _Static_assert so that assertions
// read as if they were a sentence.
// Example:
//      // Assert that we are compiling on a 64-bit machine.
//      SUNDER_STATIC_ASSERT(pointers_are_eight_bytes, sizeof(void*) == 8);
// clang-format off
#define SUNDER_STATIC_ASSERT(what, expr)                                       \
    enum {STATIC_ASSERT__ ## what = 1/!!(expr)}//;
// clang-format on

// Alternatives to the C99 standard library functions in ctype.h.
// These functions always use the "C" locale and will not result in undefined
// behavior if passed a value not representable by an unsigned char.
// clang-format off
int sunder_isalnum(int c);
int sunder_isalpha(int c);
int sunder_isblank(int c);
int sunder_iscntrl(int c);
int sunder_isdigit(int c);
int sunder_isgraph(int c);
int sunder_islower(int c);
int sunder_isprint(int c);
int sunder_ispunct(int c);
int sunder_isspace(int c);
int sunder_isupper(int c);
int sunder_isbdigit(int c); // Not in C99. Binary digit.
int sunder_isodigit(int c); // Not in C99. Octal digit.
int sunder_isxdigit(int c);
int sunder_tolower(int c);
int sunder_toupper(int c);
// clang-format on

// Alternatives to the C99 standard library functions in string.h.
// These functions do not result in undefined behavior when passed an invalid
// pointer argument paired with a memory-size argument of zero.
// clang-format off
int sunder_memcmp(void const* s1, void const* s2, size_t n);
void* sunder_memmove(void* dest, void const* src, size_t n);
void* sunder_memset(void* s, int c, size_t n);
// clang-format on

// General purpose allocator functions with out-of-memory error checking. The
// behavior of sunder_xalloc and sunder_xallocn is similar to libc realloc and
// *BSD reallocarray with the following exceptions:
// (1) On allocation failure an error message will be printed followed by
//     program termination with EXIT_FAILURE status.
// (2) The call sunder_xalloc(ptr, 0) is guaranteed to free the memory backing
//     ptr. A pointer returned by sunder_xalloc may be freed with
//     sunder_xalloc(ptr, 0) or the equivalent sunder_xalloc(ptr,
//     SUNDER_XALLOC_FREE). The calls sunder_xallocn(ptr, x, 0) and
//     sunder_xallocn(ptr, 0, y) are equivalent to sunder_xalloc(ptr, 0).
// The macro SUNDER_XALLOC_FREE may be used in place of the constant zero to
// indicate that a call sunder_xalloc(ptr, SUNDER_XALLOC_FREE) is intended as a
// free operation.
void*
sunder_xalloc(void* ptr, size_t size);
void*
sunder_xallocn(void* ptr, size_t nmemb, size_t size);
#define SUNDER_XALLOC_FREE ((size_t)0)

// Read the full contents of the file specified by path.
// Memory for the read content is allocated with sunder_xalloc.
// Returns zero on success.
int
sunder_file_read(char const* path, void** buf, size_t* buf_size);

// Write the contents of a buffer into the file specified by path.
// The file specified by path is created if it does not exist.
// Returns zero on success.
// On failure, the contents of the file specified by path is undefined.
int
sunder_file_write(char const* path, void const* buf, size_t buf_size);

// Read the full contents of the input stream specified by stream.
// Memory for the read content is allocated with sunder_xalloc.
// Returns zero on success.
int
sunder_stream_read(FILE* stream, void** buf, size_t* buf_size);

// Returns an sunder_xalloc-allocated cstring of the first count bytes of start.
// This function behaves similarly to the POSIX strdupn function.
char*
cstr_new(char const* start, size_t count);
// Returns an sunder_xalloc-allocated copy of the provided cstring.
// This function behaves similarly to the POSIX strdup function.
char*
cstr_new_cstr(char const* cstr);
// Returns an sunder_xalloc-allocated cstring from the provided formatted text.
char*
cstr_new_fmt(char const* fmt, ...);
// Returns a non-zero value if cstr starts with target.
int
cstr_starts_with(char const* cstr, char const* target);
// Returns a non-zero value if cstr ends with target.
int
cstr_ends_with(char const* cstr, char const* target);

struct vstr {
    char const* start;
    size_t count;
};

// Produce a pointer of type struct vstr* constructed from the provided
// parameters. This pointer has automatic storage duration associated with the
// enclosing block.
#define VSTR_LOCAL_PTR(start, count) (&(struct vstr){start, count})
// Produce a pointer of type struct vstr* from the provided cstring literal.
// This pointer has automatic storage duration associated with the enclosing
// block.
#define VSTR_LOCAL_PTR_STR_LITERAL(str_literal)                                \
    VSTR_LOCAL_PTR(str_literal, SUNDER_STR_LITERAL_COUNT(str_literal))

// Initializer for a vstr literal from a cstr literal.
// Example:
//      static struct vstr const foo =
//          VSTR_INIT_STR_LITERAL("foo");
// Example:
//      struct vstr bar = {0};
//      // some time later...
//      bar = (struct vstr)VSTR_INIT_STR_LITERAL("bar");
// clang-format off
#define VSTR_INIT_STR_LITERAL(str_literal)                                     \
    {str_literal, SUNDER_STR_LITERAL_COUNT(str_literal)}
// clang-format on

// Return an int less than, equal to, or greater than zero if lhs is
// lexicographically less than, equal to, or greater than rhs, respectively.
int
vstr_cmp(struct vstr const* lhs, struct vstr const* rhs);
// Comparison function satisfying sunder_vpcmp_fn.
// Parameters lhs and rhs must be of type struct vstr const*.
int
vstr_vpcmp(void const* lhs, void const* rhs);

// Returns a non-zero value if vstr starts with target.
int
vstr_starts_with(struct vstr const* vstr, struct vstr const* target);
// Returns a non-zero value if vstr ends with target.
int
vstr_ends_with(struct vstr const* vstr, struct vstr const* target);

// Allocate and initialize a string intern pool.
struct sunder_sipool*
sunder_sipool_new(void);
// Deinitialize and free the string intern pool.
// Does nothing if self == NULL.
void
sunder_sipool_del(struct sunder_sipool* self);

// Intern the string specified by the first count bytes of start.
// Returns the canonical NUL-terminated representation of the interned string.
char const*
sunder_sipool_intern(
    struct sunder_sipool* self, char const* start, size_t count);
// Intern the string specified by the provided NUL-terminated cstring.
// Returns the canonical NUL-terminated representation of the interned string.
char const*
sunder_sipool_intern_cstr(struct sunder_sipool* self, char const* cstr);

// General purpose type-safe dynamic array (a.k.a stretchy buffer).
//
// A stretchy buffer works by storing metadata about the number of allocated
// and in-use elements in a header just before the address of the buffer's
// first element. The ith element of a stretchy buffer may be accessed using
// the array index operator, sbuf[i], and a stretchy buffer containing elements
// of type T may be passed to subroutines as if it were regular array-like
// pointer of type T* or T const*. The address of a stretchy buffer may change
// when a resizing operation is performed, similar to resizing operations done
// with realloc, so the address of a stretchy buffer should not be considered
// stable.
//
// +--------+---------+---------+---------+--
// | HEADER | SBUF[0] | SBUF[1] | SBUF[2] | ...
// +--------+---------+---------+---------+--
//          ^
//          Pointer manipulated by the user / sunder_sbuf_* macros.
//
// Example:
//      // The declaration:
//      //      TYPE* identifier = NULL;
//      // creates an empty stretchy buffer holding TYPE values.
//      // An equivalent declaration:
//      //      sunder_sbuf(TYPE) identifier = NULL;
//      // may also be used in most cases.
//      int* vals = NULL;
//      printf("count == %zu\n", sunder_sbuf_count(vals));  /* count == 0 */
//
//      for (int i = 0; i < 3; ++i) {
//          sunder_sbuf_push(vals, (i + 1) * 2);
//      }
//      printf("count == %zu\n", sunder_sbuf_count(vals)); /* count == 3 */
//      printf("vals[0] == %d\n", vals[0]); /* vals[0] == 2 */
//      printf("vals[1] == %d\n", vals[1]); /* vals[1] == 4 */
//      printf("vals[2] == %d\n", vals[2]); /* vals[2] == 6 */
//
//      printf("popped == %d\n", sunder_sbuf_pop(vals)); /* popped == 6 */
//      printf("count == %zu\n", sunder_sbuf_count(vals)); /* count == 2 */
//
//      // Free memory allocated to the sbuf.
//      // This is safe to call even if vals == NULL.
//      sunder_sbuf_fini(vals);

// Convenience macros used to explicitly annotate a pointer as a stretchy
// buffer. Type annotations for types such as stack-allocated arrays and
// function pointers are not supported by this macro due to the complicated
// nature of C variable/type declarations.
//
// Example:
//      sunder_sbuf(int) sbuf = NULL;
//      sunder_sbuf_push(sbuf, 1);
#define sunder_sbuf(TYPE) TYPE*

// void sunder_sbuf_fini(TYPE* sbuf)
// ------------------------------------------------------------
// Free resources associated with the stretchy buffer.
// Macro parameter sbuf is evaluated multiple times.
#define sunder_sbuf_fini(sbuf)                                                 \
    ((void)((sbuf) != NULL ? SUNDER__SBUF_FREE_NON_NULL_HEAD_(sbuf) : NULL))

// void sunder_sbuf_freeze(TYPE* sbuf, struct sunder_freezer* freezer)
// ------------------------------------------------------------
// Register resources within bigint with the provided freezer.
#define sunder_sbuf_freeze(sbuf, freezer)                                      \
    ((void)((sbuf) != NULL ? SUNDER__SBUF_FREEZE_NON_NULL_HEAD_(sbuf, freezer), NULL : NULL))

// size_t sunder_sbuf_count(TYPE* sbuf)
// ------------------------------------------------------------
// The number of elements in the stretchy buffer.
// Macro parameter sbuf is evaluated multiple times.
#define sunder_sbuf_count(sbuf)                                                \
    ((size_t)((sbuf) != NULL ? SUNDER__SBUF_PHEAD_CONST_(sbuf)->cnt_ : 0u))
// size_t sunder_sbuf_capacity(TYPE* sbuf)
// ------------------------------------------------------------
// The number of elements the allocated in the sbuf.
// Macro parameter sbuf is evaluated multiple times.
#define sunder_sbuf_capacity(sbuf)                                             \
    ((size_t)((sbuf) != NULL ? SUNDER__SBUF_PHEAD_CONST_(sbuf)->cap_ : 0u))

// void sunder_sbuf_reserve(TYPE* sbuf, size_t n)
// ------------------------------------------------------------
// Update the minimum capacity of the stretchy buffer to n elements.
// Macro parameter sbuf is evaluated multiple times.
#define sunder_sbuf_reserve(sbuf, /*n*/...)                                    \
    ((void)((sbuf) = sunder__sbuf_rsv_(sizeof(*(sbuf)), sbuf, __VA_ARGS__)))
// void sunder_sbuf_resize(TYPE* sbuf, size_t n)
// ------------------------------------------------------------
// Update the count of the stretchy buffer to n elements.
// Macro parameter sbuf is evaluated multiple times.
#define sunder_sbuf_resize(sbuf, /*n*/...)                                     \
    ((void)((sbuf) = sunder__sbuf_rsz_(sizeof(*(sbuf)), sbuf, __VA_ARGS__)))

// void sunder_sbuf_push(TYPE* sbuf, TYPE val)
// ------------------------------------------------------------
// Append val as the last element of the stretchy buffer.
// Macro parameter sbuf is evaluated multiple times.
#define sunder_sbuf_push(sbuf, /*val*/...)                                     \
    ((void)(SUNDER__SBUF_MAYBE_GROW_(sbuf), SUNDER__SBUF_APPEND_(sbuf, __VA_ARGS__)))
// TYPE sunder_sbuf_pop(TYPE* sbuf)
// ------------------------------------------------------------
// Remove and return the last element of the stretchy buffer.
// This macro does *not* perform bounds checking.
// Macro parameter sbuf is evaluated multiple times.
#define sunder_sbuf_pop(sbuf) ((sbuf)[--SUNDER__SBUF_PHEAD_MUTBL_(sbuf)->cnt_])

// Internal utilities that must be visible to other header/source files that
// wish to use the sunder_sbuf_* API. Do not use these directly!
// clang-format off
struct sunder__sbuf_header_{size_t cnt_; size_t cap_; sunder_max_align_type _[];};
enum{SUNDER__SBUF_HEADER_OFFSET_ = sizeof(struct sunder__sbuf_header_)};
#define SUNDER__SBUF_PHEAD_MUTBL_(sbuf_)                                       \
    ((struct sunder__sbuf_header_      *)                                      \
     ((char      *)(sbuf_)-SUNDER__SBUF_HEADER_OFFSET_))
#define SUNDER__SBUF_PHEAD_CONST_(sbuf_)                                       \
    ((struct sunder__sbuf_header_ const*)                                      \
     ((char const*)(sbuf_)-SUNDER__SBUF_HEADER_OFFSET_))
#define SUNDER__SBUF_FREE_NON_NULL_HEAD_(sbuf_)                                \
    (sunder_xalloc(SUNDER__SBUF_PHEAD_MUTBL_(sbuf_), SUNDER_XALLOC_FREE))
#define SUNDER__SBUF_FREEZE_NON_NULL_HEAD_(sbuf_, freezer)                     \
    (sunder_freezer_register(freezer, SUNDER__SBUF_PHEAD_MUTBL_(sbuf_)))
#define SUNDER__SBUF_MAYBE_GROW_(sbuf_)                                        \
    ((sunder_sbuf_count(sbuf_) == sunder_sbuf_capacity(sbuf_))                 \
         ? (sbuf_) = sunder__sbuf_grw_(sizeof(*(sbuf_)), sbuf_)                \
         : (sbuf_))
#define SUNDER__SBUF_APPEND_(sbuf_, ...)                                       \
    ((sbuf_)[SUNDER__SBUF_PHEAD_MUTBL_(sbuf_)->cnt_++] = (__VA_ARGS__))
void* sunder__sbuf_rsv_(size_t elemsize, void* sbuf, size_t cap);
void* sunder__sbuf_rsz_(size_t elemsize, void* sbuf, size_t cnt);
void* sunder__sbuf_grw_(size_t elemsize, void* sbuf);
// clang-format on

// Allocate and initialize a bit array with count bits.
// The bit array is initially zeroed.
struct sunder_bitarr*
sunder_bitarr_new(size_t count);
// Deinitialize and free the bit array.
// Does nothing if self == NULL.
void
sunder_bitarr_del(struct sunder_bitarr* self);
// Register resources within the bit array with the provided freezer.
void
sunder_bitarr_freeze(
    struct sunder_bitarr* self, struct sunder_freezer* freezer);

// Returns the number of bits in this bit array.
size_t
sunder_bitarr_count(struct sunder_bitarr const* self);
// Set the nth bit (zero indexed) of self to value.
// Fatally exits after printing an error message if n is out of bounds.
void
sunder_bitarr_set(struct sunder_bitarr* self, size_t n, int value);
// Returns the value (one or zero) of the nth bit (zero indexed) of self.
// Fatally exits after printing an error message if n is out of bounds.
int
sunder_bitarr_get(struct sunder_bitarr const* self, size_t n);

// self = othr
// Fatally exits after printing an error message if the count of self is not
// equal to the count of othr.
void
sunder_bitarr_assign(
    struct sunder_bitarr* self, struct sunder_bitarr const* othr);

// res = ~rhs
// Fatally exits after printing an error message if the count of res and rhs are
// not equal.
void
sunder_bitarr_compl(struct sunder_bitarr* res, struct sunder_bitarr const* rhs);
// res = lhs << nbits (logical shift left)
// Fatally exits after printing an error message if the count of res and lhs are
// not equal.
void
sunder_bitarr_shiftl(
    struct sunder_bitarr* res, struct sunder_bitarr const* lhs, size_t nbits);
// res = lhs >> nbits (logical shift right)
// Fatally exits after printing an error message if the count of res and lhs are
// not equal.
void
sunder_bitarr_shiftr(
    struct sunder_bitarr* res, struct sunder_bitarr const* lhs, size_t nbits);
// res = lhs & rhs
// Fatally exits after printing an error message if the count of res, lhs, and
// rhs are not equal.
void
sunder_bitarr_and(
    struct sunder_bitarr* res,
    struct sunder_bitarr const* lhs,
    struct sunder_bitarr const* rhs);
// res = lhs ^ rhs
// Fatally exits after printing an error message if the count of res, lhs, and
// rhs are not equal.
void
sunder_bitarr_xor(
    struct sunder_bitarr* res,
    struct sunder_bitarr const* lhs,
    struct sunder_bitarr const* rhs);
// res = lhs | rhs
// Fatally exits after printing an error message if the count of res, lhs, and
// rhs are not equal.
void
sunder_bitarr_or(
    struct sunder_bitarr* res,
    struct sunder_bitarr const* lhs,
    struct sunder_bitarr const* rhs);

extern struct sunder_bigint const* const SUNDER_BIGINT_ZERO; // 0
extern struct sunder_bigint const* const SUNDER_BIGINT_POS_ONE; // +1
extern struct sunder_bigint const* const SUNDER_BIGINT_NEG_ONE; // -1

// Allocate and initialize a bigint to the specified value.
// The call sunder_bigint_new(SUNDER_BIGINT_ZERO) will zero-initialize a bigint.
struct sunder_bigint*
sunder_bigint_new(struct sunder_bigint const* othr);
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
struct sunder_bigint*
sunder_bigint_new_cstr(char const* cstr);
// Allocate and initialize a bigint from the provided string slice.
// Returns NULL if the string could not be parsed.
// This function uses the same string-grammar as sunder_bigint_new_cstr().
struct sunder_bigint*
sunder_bigint_new_text(char const* start, size_t count);
// Deinitialize and free the bigint.
// Does nothing if self == NULL.
void
sunder_bigint_del(struct sunder_bigint* self);
// Register resources within the bigint with the provided freezer.
void
sunder_bigint_freeze(
    struct sunder_bigint* self, struct sunder_freezer* freezer);

// Return an int less than, equal to, or greater than zero if lhs is
// semantically less than, equal to, or greater than rhs, respectively.
int
sunder_bigint_cmp(
    struct sunder_bigint const* lhs, struct sunder_bigint const* rhs);

// self = othr
void
sunder_bigint_assign(
    struct sunder_bigint* self, struct sunder_bigint const* othr);

// res = -rhs
void
sunder_bigint_neg(struct sunder_bigint* res, struct sunder_bigint const* rhs);
// res = abs(rhs)
void
sunder_bigint_abs(struct sunder_bigint* res, struct sunder_bigint const* rhs);
// res = lhs + rhs
void
sunder_bigint_add(
    struct sunder_bigint* res,
    struct sunder_bigint const* lhs,
    struct sunder_bigint const* rhs);
// res = lhs - rhs
void
sunder_bigint_sub(
    struct sunder_bigint* res,
    struct sunder_bigint const* lhs,
    struct sunder_bigint const* rhs);
// res  = lhs * rhs
void
sunder_bigint_mul(
    struct sunder_bigint* res,
    struct sunder_bigint const* lhs,
    struct sunder_bigint const* rhs);
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
void
sunder_bigint_divrem(
    struct sunder_bigint* res,
    struct sunder_bigint* rem,
    struct sunder_bigint const* lhs,
    struct sunder_bigint const* rhs);

// self.magnitude = self.magnitude << nbits (logical shift left)
// This function is sign-oblivious (the sign of self is not altered).
void
sunder_bigint_magnitude_shiftl(struct sunder_bigint* self, size_t nbits);
// self.magnitude = self.magnitude >> nbits (logical shift right)
// This function is sign-oblivious (the sign of self is not altered).
void
sunder_bigint_magnitude_shiftr(struct sunder_bigint* self, size_t nbits);
// Returns the number of bits required to store the magnitude of self.
// This function is sign-oblivious (the sign of self is not considered).
size_t
sunder_bigint_magnitude_bit_count(struct sunder_bigint const* self);
// Returns the value (one or zero) of the nth bit (zero indexed) of the
// magnitude of self.
// This function is sign-oblivious (the sign of self is not considered).
int
sunder_bigint_magnitude_bit_get(struct sunder_bigint const* self, size_t n);
// Set the nth bit (zero indexed) of the magnitude of self to value.
// This function is sign-oblivious (the sign of self is not altered).
void
sunder_bigint_magnitude_bit_set(
    struct sunder_bigint* self, size_t n, int value);

// Returns an sunder_xalloc-allocated cstring representation of the provided
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
char*
sunder_bigint_to_new_cstr(struct sunder_bigint const* self, char const* fmt);

// Allocate and initialize a string from the first count bytes of start.
struct sunder_string*
sunder_string_new(char const* start, size_t count);
// Allocate and initialize a string from the provided NUL-terminated cstring.
// If cstr is NULL then string will be initialized to the empty string.
struct sunder_string*
sunder_string_new_cstr(char const* cstr);
// Allocate and initialize a string from the provided formatted text.
struct sunder_string*
sunder_string_new_fmt(char const* fmt, ...);
// Deinitialize and free the string.
// Does nothing if self == NULL.
void
sunder_string_del(struct sunder_string* self);
// Register resources within the string with the provided freezer.
void
sunder_string_freeze(
    struct sunder_string* self, struct sunder_freezer* freezer);

// Pointer to the start of the underlying char array of the string.
// Returns a pointer to a NUL terminator when the count of the string is zero.
char const*
sunder_string_start(struct sunder_string const* self);
// The number of bytes in the string *NOT* including the NUL terminator.
size_t
sunder_string_count(struct sunder_string const* self);

// Return an int less than, equal to, or greater than zero if lhs is
// lexicographically less than, equal to, or greater than rhs, respectively.
int
sunder_string_cmp(
    struct sunder_string const* lhs, struct sunder_string const* rhs);

// Update the count of the string.
// If count is greater than the current count of the string then additional
// elements are initialized with garbage data.
void
sunder_string_resize(struct sunder_string* self, size_t count);

// Return a pointer to the byte of the string at position idx.
// Fatally exits after printing an error message if idx is out of bounds.
char*
sunder_string_ref(struct sunder_string* self, size_t idx);
char const*
sunder_string_ref_const(struct sunder_string const* self, size_t idx);

// Insert count bytes of start into the string at position idx.
// Bytes with position greater than idx are moved back count bytes.
// Fatally exits after printing an error message if idx is out of bounds.
void
sunder_string_insert(
    struct sunder_string* self, size_t idx, char const* start, size_t count);
// Remove count bytes at position idx from the string.
// Bytes with position greater than idx are moved forward count bytes.
// Fatally exits after printing an error message if the slice to be removed
// indexes out of bounds.
void
sunder_string_remove(struct sunder_string* self, size_t idx, size_t count);

// Append count bytes of start onto the end of the string.
void
sunder_string_append(
    struct sunder_string* self, char const* start, size_t count);
// Append the provided NUL-terminated cstring onto the end of the string.
void
sunder_string_append_cstr(struct sunder_string* self, char const* cstr);
// Append the formatted text to the end of the string.
void
sunder_string_append_fmt(struct sunder_string* self, char const* fmt, ...);
void
sunder_string_append_vfmt(
    struct sunder_string* self, char const* fmt, va_list args);
// Split the string on all occurrences of the provided separator.
// Empty strings are *NOT* removed from the result.
// This function returns a stretchy buffer of newly allocated sunder_string
// pointers containing the results of the split.
//
// Example:
//      "ABCBB" ===split on "B"===> "A" "C" "" ""
struct sunder_string**
sunder_string_split_on(
    struct sunder_string const* self,
    char const* separator,
    size_t separator_size);

// Allocate and initialize a freezer.
struct sunder_freezer*
sunder_freezer_new(void);
// Deinitialize and free the freezer.
// Does nothing if self == NULL.
void
sunder_freezer_del(struct sunder_freezer* self);

// Register a pointer to sunder_xalloc-allocated memory to be freed when the
// freezer is deinitialized.
void
sunder_freezer_register(struct sunder_freezer* self, void* ptr);

#if __STDC_VERSION__ >= 201112L /* C11+ */
#    define NORETURN _Noreturn
#elif defined(__GNUC__) /* GCC and Clang */
#    define NORETURN __attribute__((noreturn))
#else
#    define NORETURN /* nothing */
#endif

// Returns the string contents of a file with the provided path. The produced
// string is NUL-prefixed and NUL-terminated. This function will cause a fatal
// error if the file cannot be read.
char*
read_source(char const* path);
// Returns a pointer to the first character of the line containing ptr in a
// source string produced by read_source.
char const*
source_line_start(char const* ptr);
// Returns a pointer to the end-of-line newline or NUL of the line containing
// ptr in a source string produced by read_source.
char const*
source_line_end(char const* ptr);

#define NO_PATH ((char const*)NULL)
#define NO_LINE ((size_t)0u)
#define NO_PSRC ((char const*)NULL)
#define NO_LOCATION ((struct source_location const*)NULL)
struct source_location {
    // Optional (NULL indicates no value). NOTE: Source locations produced by
    // the lexing phase will use a module's `name` (i.e. non-canonical path)
    // member for the source location path.
    char const* path;
    // Optional (zero indicates no value).
    size_t line;
    // Optional (NULL indicates no value) pointer to the source character
    // within the module specified by path. If non-NULL then a log-messages
    // will display the line in question with a caret pointing to this
    // character as such:
    // ```
    // [file.sunder:3] error: foo is not properly frobnicated
    // var foo: usize = 123u;
    //     ^
    // ```
    char const* psrc;
};

void
error(struct source_location const* location, char const* fmt, ...);

NORETURN void
fatal(struct source_location const* location, char const* fmt, ...);

NORETURN void
todo(char const* file, int line, char const* fmt, ...);
#define TODO(...) todo(__FILE__, __LINE__, __VA_ARGS__)

NORETURN void
unreachable(char const* file, int line);
#define UNREACHABLE() unreachable(__FILE__, __LINE__)

// Round up to the nearest multiple of 8.
int
ceil8i(int x);
size_t
ceil8zu(size_t x);

// Convert a bigint to a uint8_t.
// Returns zero on success.
// Returns non-zero if the provided bigint is out-of-range, in which case *res
// is left unmodified.
int
bigint_to_u8(uint8_t* res, struct sunder_bigint const* bigint);
// Convert a bigint to a size_t.
// Returns zero on success.
// Returns non-zero if the provided bigint is out-of-range, in which case *res
// is left unmodified.
int
bigint_to_uz(size_t* res, struct sunder_bigint const* bigint);
// Convert a bigint to a uintmax_t.
// Returns zero on success.
// Returns non-zero if the provided bigint is out-of-range, in which case *res
// is left unmodified.
int
bigint_to_umax(uintmax_t* res, struct sunder_bigint const* bigint);
// Convert a bigint into a two's complement bit array.
// Returns zero on success.
// Returns non-zero if the provided bigint is out-of-range would require more
// than sunder_bitarr_count(res) bits to express, in which case *res is left
// unmodified.
int
bigint_to_bitarr(struct sunder_bitarr* res, struct sunder_bigint const* bigint);

// Convert a size_t to a bigint.
// The result bigint must be pre-initialized.
void
uz_to_bigint(struct sunder_bigint* res, size_t uz);
// Convert a two's complement bit array into a bigint.
// The result bigint must be pre-initialized.
void
bitarr_to_bigint(
    struct sunder_bigint* res,
    struct sunder_bitarr const* bitarr,
    bool is_signed);

// Spawn a subprocess and wait for it to complete.
// Returns the exit status of the spawned process.
int
spawnvpw(char const* const* argv);
// Spawn a subprocess and wait for it to complete.
// Fatally exits if the exit status of the spawned process is non-zero.
void
xspawnvpw(char const* const* argv);

bool
file_exists(char const* path);
bool
file_is_directory(char const* path);

char const* // interned
canonical_path(char const* path);
char const* // interned
directory_path(char const* path);
// Excludes `.` and `..`
char const* /* interned */* /* sbuf */
directory_files(char const* path);

////////////////////////////////////////////////////////////////////////////////
//////// sunder.c //////////////////////////////////////////////////////////////
// Global compiler state.

struct module {
    // True if the module has been fully loaded/resolved.
    bool loaded;
    // The shorthand path of this module. For a module imported as:
    //      import "foo/bar.sunder";
    // this member will hold the string "foo/bar.sunder".
    char const* name; // interned
    // The canonical path of this module. For a module imported as:
    //      import "foo/bar.sunder";
    // this member will hold the string "/full/path/to/foo/bar.sunder".
    char const* path; // interned
    // NUL-prefixed, NUL-terminated text contents of the module.
    // When the module source is loaded a NUL-prefix is added to the beginning
    // of the source string at position source[-1] and source[source_count + 1]
    // so that either a forwards or backward search through the source text may
    // stop if a NUL byte is encountered.
    char const* source;
    size_t source_count;

    // Global symbols.
    struct symbol_table* symbols;
    // Exported symbols declared in this module.
    struct symbol_table* exports;

    // Concrete syntax tree for the module. Initialized to NULL and populated
    // during the parse phase.
    struct cst_module const* cst;
    // List of top level declarations topologically ordered such that
    // declaration with index k does not depend on any declaration with index
    // k+n for all n. Initialized to NULL and populated during the order phase.
    sunder_sbuf(struct cst_decl const*) ordered;
};
struct module*
module_new(char const* name, char const* path);
void
module_del(struct module* self);

struct context {
    // Context-owned automatically managed objects.
    struct sunder_freezer* freezer;

    // Interned strings.
    struct sunder_sipool* sipool;
    struct {
        // clang-format off
        char const* empty;   // ""
        char const* builtin; // "builtin"
        char const* return_; // "return"
        char const* any;     // "any"
        char const* void_;   // "void"
        char const* bool_;   // "bool"
        char const* u8;      // "u8"
        char const* s8;      // "s8"
        char const* u16;     // "u16"
        char const* s16;     // "s16"
        char const* u32;     // "u32"
        char const* s32;     // "s32"
        char const* u64;     // "u64"
        char const* s64;     // "s64"
        char const* byte;    // "byte"
        char const* usize;   // "usize"
        char const* ssize;   // "ssize"
        char const* integer; // "integer"
        char const* y;       // "y"
        char const* u;       // "u"
        char const* s;       // "s"
        // clang-format on
    } interned;

    // Integer (bigint) constants.
    struct sunder_bigint const* u8_min;
    struct sunder_bigint const* u8_max;
    struct sunder_bigint const* s8_min;
    struct sunder_bigint const* s8_max;
    struct sunder_bigint const* u16_min;
    struct sunder_bigint const* u16_max;
    struct sunder_bigint const* s16_min;
    struct sunder_bigint const* s16_max;
    struct sunder_bigint const* u32_min;
    struct sunder_bigint const* u32_max;
    struct sunder_bigint const* s32_min;
    struct sunder_bigint const* s32_max;
    struct sunder_bigint const* u64_min;
    struct sunder_bigint const* u64_max;
    struct sunder_bigint const* s64_min;
    struct sunder_bigint const* s64_max;
    struct sunder_bigint const* usize_min;
    struct sunder_bigint const* usize_max;
    struct sunder_bigint const* ssize_min;
    struct sunder_bigint const* ssize_max;

    // Language builtins.
    struct {
        struct source_location location;
        struct type const* any;
        struct type const* void_;
        struct type const* bool_;
        struct type const* byte;
        struct type const* u8;
        struct type const* s8;
        struct type const* u16;
        struct type const* s16;
        struct type const* u32;
        struct type const* s32;
        struct type const* u64;
        struct type const* s64;
        struct type const* usize;
        struct type const* ssize;
        struct type const* integer;
    } builtin;

    // List of all symbols with static storage duration.
    sunder_sbuf(struct symbol const*) static_symbols;

    // Global symbol table.
    struct symbol_table* global_symbol_table;

    // Currently loaded/loading modules.
    // TODO: Maybe make this a map from realpath to module?
    sunder_sbuf(struct module*) modules;

    // Symbol tables belonging to types and templates. These symbol tables
    // cannot be frozen until after all modules have been resolved as type
    // extensions template instantiations may be defined in modules other than
    // the module where a declaration is defined.
    //
    // TODO: We have have a chilling_symbol_tables on the resolver struct. Can
    // we merge that functionality into this context member variable and have a
    // single "chilling symbol tables" member for all symbol tables?
    sunder_sbuf(struct symbol_table*) chilling_symbol_tables;
};
void
context_init(void);
void
context_fini(void);
struct context*
context(void);

struct module const*
load_module(char const* name, char const* path);
struct module const*
lookup_module(char const* path);

////////////////////////////////////////////////////////////////////////////////
//////// lex.c /////////////////////////////////////////////////////////////////

enum token_kind {
    // Keywords
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NOT,
    TOKEN_OR,
    TOKEN_AND,
    TOKEN_NAMESPACE,
    TOKEN_IMPORT,
    TOKEN_VAR,
    TOKEN_CONST,
    TOKEN_FUNC,
    TOKEN_STRUCT,
    TOKEN_EXTEND,
    TOKEN_ALIAS,
    TOKEN_EXTERN,
    TOKEN_DUMP,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_SYSCALL,
    TOKEN_ALIGNOF,
    TOKEN_COUNTOF,
    TOKEN_SIZEOF,
    TOKEN_TYPEOF,
    // Sigils
    TOKEN_EQ, // ==
    TOKEN_NE, // !=
    TOKEN_LE, // <=
    TOKEN_LT, // <
    TOKEN_GE, // >=
    TOKEN_GT, // >
    TOKEN_ASSIGN, // =
    TOKEN_PLUS, // +
    TOKEN_DASH, // -
    TOKEN_STAR, // *
    TOKEN_FSLASH, // /
    TOKEN_TILDE, // ~
    TOKEN_PIPE, // |
    TOKEN_CARET, // ^
    TOKEN_AMPERSAND, // &
    TOKEN_LPAREN, // (
    TOKEN_RPAREN, // )
    TOKEN_LBRACE, // {
    TOKEN_RBRACE, // }
    TOKEN_LBRACKET_LBRACKET, // [[
    TOKEN_RBRACKET_RBRACKET, // ]]
    TOKEN_LBRACKET, // [
    TOKEN_RBRACKET, // ]
    TOKEN_COMMA, // ,
    TOKEN_ELLIPSIS, // ...
    TOKEN_DOT_STAR, // .*
    TOKEN_DOT, // .
    TOKEN_COLON_COLON, // :
    TOKEN_COLON, // :
    TOKEN_SEMICOLON, // ;
    // Identifiers and Non-Keyword Literals
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
    TOKEN_CHARACTER,
    TOKEN_BYTES,
    // Meta
    TOKEN_EOF,
};
char const*
token_kind_to_cstr(enum token_kind kind);

struct token {
    char const* start;
    size_t count;
    struct source_location location;

    enum token_kind kind;
    union {
        // TOKEN_INTEGER
        struct {
            struct vstr number;
            struct vstr suffix;
        } integer;
        // TOKEN_CHARACTER
        // Contains the value of the character literal.
        int character;
        // TOKEN_BYTES
        // Contains the un-escaped contents of the bytes literal.
        struct sunder_string const* bytes;
    } data;
};
char*
token_to_new_cstr(struct token const* token);

struct lexer*
lexer_new(struct module* module);
void
lexer_del(struct lexer* self);
struct token const*
lexer_next_token(struct lexer* self);

////////////////////////////////////////////////////////////////////////////////
//////// cst.c /////////////////////////////////////////////////////////////////
// Concrete syntax tree.

struct cst_module {
    struct cst_namespace const* namespace; // optional
    sunder_sbuf(struct cst_import const* const) imports;
    sunder_sbuf(struct cst_decl const* const) decls;
};
struct cst_module*
cst_module_new(
    struct cst_namespace const* namespace,
    struct cst_import const* const* imports,
    struct cst_decl const* const* decls);

struct cst_namespace {
    struct source_location const* location;
    sunder_sbuf(struct cst_identifier const* const) identifiers;
};
struct cst_namespace*
cst_namespace_new(
    struct source_location const* location,
    struct cst_identifier const* const* identifiers);

struct cst_import {
    struct source_location const* location;
    char const* path; // interned
};
struct cst_import*
cst_import_new(struct source_location const* location, char const* path);

struct cst_decl {
    struct source_location const* location;
    char const* name; // interned (from the identifier)
    enum {
        CST_DECL_VARIABLE,
        CST_DECL_CONSTANT,
        CST_DECL_FUNCTION,
        CST_DECL_STRUCT,
        CST_DECL_EXTEND,
        CST_DECL_ALIAS,
        CST_DECL_EXTERN_VARIABLE,
    } kind;
    union {
        struct {
            struct cst_identifier const* identifier;
            struct cst_typespec const* typespec; // optional
            struct cst_expr const* expr;
        } variable;
        struct {
            struct cst_identifier const* identifier;
            struct cst_typespec const* typespec; // optional
            struct cst_expr const* expr;
        } constant;
        struct {
            struct cst_identifier const* identifier;
            // A template parameter list with count zero indicates that this
            // function was declared without template parameters.
            sunder_sbuf(struct cst_template_parameter const* const)
                template_parameters;
            sunder_sbuf(struct cst_function_parameter const* const)
                function_parameters;
            struct cst_typespec const* return_typespec;
            struct cst_block const* body;
        } function;
        struct {
            struct cst_identifier const* identifier;
            // A template parameter list with count zero indicates that this
            // struct was declared without template parameters.
            sunder_sbuf(struct cst_template_parameter const* const)
                template_parameters;
            sunder_sbuf(struct cst_member const* const) members;
        } struct_;
        struct {
            struct cst_typespec const* typespec;
            struct cst_decl const* decl;
        } extend;
        struct {
            struct cst_identifier const* identifier;
            struct cst_symbol const* symbol;
        } alias;
        struct {
            struct cst_identifier const* identifier;
            struct cst_typespec const* typespec;
        } extern_variable;
    } data;
};
struct cst_decl*
cst_decl_new_variable(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec,
    struct cst_expr const* expr);
struct cst_decl*
cst_decl_new_constant(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec,
    struct cst_expr const* expr);
struct cst_decl*
cst_decl_new_function(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_template_parameter const* const* template_parameters,
    struct cst_function_parameter const* const* function_parameters,
    struct cst_typespec const* return_typespec,
    struct cst_block const* body);
struct cst_decl*
cst_decl_new_struct(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_template_parameter const* const* template_parameters,
    struct cst_member const* const* members);
struct cst_decl*
cst_decl_new_extend(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_decl const* decl);
struct cst_decl*
cst_decl_new_alias(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_symbol const* symbol);
struct cst_decl*
cst_decl_new_extern_variable(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec);

struct cst_stmt {
    struct source_location const* location;
    enum cst_stmt_kind {
        CST_STMT_DECL,
        CST_STMT_IF,
        CST_STMT_FOR_RANGE,
        CST_STMT_FOR_EXPR,
        CST_STMT_BREAK, /* no .data member */
        CST_STMT_CONTINUE, /* no .data member */
        CST_STMT_DUMP,
        CST_STMT_RETURN,
        CST_STMT_ASSIGN,
        CST_STMT_EXPR,
    } kind;
    union {
        struct cst_decl const* decl;
        struct {
            sunder_sbuf(struct cst_conditional const* const) conditionals;
        } if_;
        struct {
            struct cst_identifier const* identifier;
            struct cst_expr const* begin;
            struct cst_expr const* end;
            struct cst_block const* body;
        } for_range;
        struct {
            struct cst_expr const* expr;
            struct cst_block const* body;
        } for_expr;
        struct {
            struct cst_expr const* expr;
        } dump;
        struct {
            struct cst_expr const* expr; // optional
        } return_;
        struct {
            struct cst_expr const* lhs;
            struct cst_expr const* rhs;
        } assign;
        struct cst_expr const* expr;
    } data;
};
struct cst_stmt*
cst_stmt_new_decl(struct cst_decl const* decl);
struct cst_stmt*
cst_stmt_new_if(struct cst_conditional const* const* conditionals);
struct cst_stmt*
cst_stmt_new_for_range(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_expr const* begin,
    struct cst_expr const* end,
    struct cst_block const* body);
struct cst_stmt*
cst_stmt_new_for_expr(
    struct source_location const* location,
    struct cst_expr const* expr,
    struct cst_block const* body);
struct cst_stmt*
cst_stmt_new_break(struct source_location const* location);
struct cst_stmt*
cst_stmt_new_continue(struct source_location const* location);
struct cst_stmt*
cst_stmt_new_dump(
    struct source_location const* location, struct cst_expr const* expr);
struct cst_stmt*
cst_stmt_new_return(
    struct source_location const* location, struct cst_expr const* expr);
struct cst_stmt*
cst_stmt_new_assign(
    struct source_location const* location,
    struct cst_expr const* lhs,
    struct cst_expr const* rhs);
struct cst_stmt*
cst_stmt_new_expr(struct cst_expr const* expr);

struct cst_expr {
    struct source_location const* location;
    enum cst_expr_kind {
        // Primary Expressions
        CST_EXPR_SYMBOL,
        CST_EXPR_BOOLEAN,
        CST_EXPR_INTEGER,
        CST_EXPR_CHARACTER,
        CST_EXPR_BYTES,
        CST_EXPR_ARRAY,
        CST_EXPR_SLICE,
        CST_EXPR_ARRAY_SLICE,
        CST_EXPR_STRUCT,
        CST_EXPR_CAST,
        CST_EXPR_GROUPED,
        // Postfix Expressions
        CST_EXPR_SYSCALL,
        CST_EXPR_CALL,
        CST_EXPR_ACCESS_INDEX,
        CST_EXPR_ACCESS_SLICE,
        CST_EXPR_ACCESS_MEMBER,
        CST_EXPR_ACCESS_DEREFERENCE,
        // Prefix Unary Operator Expressions
        CST_EXPR_SIZEOF,
        CST_EXPR_ALIGNOF,
        CST_EXPR_UNARY,
        // Infix Binary Operator Expressions
        CST_EXPR_BINARY,
    } kind;
    union cst_expr_data {
        struct cst_symbol const* symbol;
        struct cst_boolean const* boolean;
        struct cst_integer const* integer;
        int character;
        struct sunder_string const* bytes;
        struct {
            struct cst_typespec const* typespec;
            sunder_sbuf(struct cst_expr const* const) elements;
            struct cst_expr const* ellipsis; // optional
        } array;
        struct {
            struct cst_typespec const* typespec;
            struct cst_expr const* pointer;
            struct cst_expr const* count;
        } slice;
        struct {
            struct cst_typespec const* typespec;
            sunder_sbuf(struct cst_expr const* const) elements;
        } array_slice;
        struct {
            struct cst_typespec const* typespec;
            sunder_sbuf(struct cst_member_initializer const* const)
                initializers;
        } struct_;
        struct {
            struct cst_typespec const* typespec;
            struct cst_expr const* expr;
        } cast;
        struct {
            struct cst_expr const* expr;
        } grouped;
        struct cst_stmt_syscall {
            sunder_sbuf(struct cst_expr const* const) arguments;
        } syscall;
        struct {
            struct cst_expr const* func;
            sunder_sbuf(struct cst_expr const* const) arguments;
        } call;
        struct {
            struct cst_expr const* lhs;
            struct cst_expr const* idx;
        } access_index;
        struct {
            struct cst_expr const* lhs;
            struct cst_expr const* begin;
            struct cst_expr const* end;
        } access_slice;
        struct {
            struct cst_expr const* lhs;
            struct cst_identifier const* identifier;
        } access_member;
        struct {
            struct cst_expr const* lhs;
        } access_dereference;
        struct {
            struct cst_typespec const* rhs;
        } sizeof_;
        struct {
            struct cst_typespec const* rhs;
        } alignof_;
        struct {
            struct token const* op;
            struct cst_expr const* rhs;
        } unary;
        struct {
            struct token const* op;
            struct cst_expr const* lhs;
            struct cst_expr const* rhs;
        } binary;
    } data;
};
struct cst_expr*
cst_expr_new_symbol(struct cst_symbol const* symbol);
struct cst_expr*
cst_expr_new_boolean(struct cst_boolean const* boolean);
struct cst_expr*
cst_expr_new_integer(struct cst_integer const* integer);
struct cst_expr*
cst_expr_new_character(struct source_location const* location, int character);
struct cst_expr*
cst_expr_new_bytes(
    struct source_location const* location, struct sunder_string const* bytes);
struct cst_expr*
cst_expr_new_array(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_expr const* const* elements,
    struct cst_expr const* ellipsis);
struct cst_expr*
cst_expr_new_slice(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_expr const* pointer,
    struct cst_expr const* count);
struct cst_expr*
cst_expr_new_array_slice(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_expr const* const* elements);
struct cst_expr*
cst_expr_new_struct(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_member_initializer const* const* initializers);
struct cst_expr*
cst_expr_new_cast(
    struct source_location const* location,
    struct cst_typespec const* typespec,
    struct cst_expr const* expr);
struct cst_expr*
cst_expr_new_grouped(
    struct source_location const* location, struct cst_expr const* expr);
struct cst_expr*
cst_expr_new_syscall(
    struct source_location const* location,
    struct cst_expr const* const* arguments);
struct cst_expr*
cst_expr_new_call(
    struct source_location const* location,
    struct cst_expr const* func,
    struct cst_expr const* const* arguments);
struct cst_expr*
cst_expr_new_access_index(
    struct source_location const* location,
    struct cst_expr const* lhs,
    struct cst_expr const* idx);
struct cst_expr*
cst_expr_new_access_slice(
    struct source_location const* location,
    struct cst_expr const* lhs,
    struct cst_expr const* begin,
    struct cst_expr const* end);
struct cst_expr*
cst_expr_new_access_member(
    struct source_location const* location,
    struct cst_expr const* lhs,
    struct cst_identifier const* identifier);
struct cst_expr*
cst_expr_new_access_dereference(
    struct source_location const* location, struct cst_expr const* lhs);
struct cst_expr*
cst_expr_new_sizeof(
    struct source_location const* location, struct cst_typespec const* rhs);
struct cst_expr*
cst_expr_new_alignof(
    struct source_location const* location, struct cst_typespec const* rhs);
struct cst_expr*
cst_expr_new_unary(struct token const* op, struct cst_expr const* rhs);
struct cst_expr*
cst_expr_new_binary(
    struct token const* op,
    struct cst_expr const* lhs,
    struct cst_expr const* rhs);

// Helper CST node that denotes a conditional expression (if, elif, etc.)
// consisting of a conditional expression and body.
struct cst_conditional {
    struct source_location const* location;
    struct cst_expr const* condition; // optional (NULL => else)
    struct cst_block const* body;
};
struct cst_conditional*
cst_conditional_new(
    struct source_location const* location,
    struct cst_expr const* condition,
    struct cst_block const* body);

struct cst_block {
    struct source_location const* location;
    sunder_sbuf(struct cst_stmt const* const) stmts;
};
struct cst_block*
cst_block_new(
    struct source_location const* location,
    struct cst_stmt const* const* stmts);

struct cst_symbol {
    struct source_location const* location;
    // True if this symbol starts with a "::", indicating that symbol lookup
    // should start from the module root symbol table instead of the current
    // scope symbol table.
    bool is_from_root;
    // Individual symbol elements separated by "::".
    sunder_sbuf(struct cst_symbol_element const* const) elements;
};
struct cst_symbol*
cst_symbol_new(
    struct source_location const* location,
    bool is_from_root,
    struct cst_symbol_element const* const* elements);

struct cst_symbol_element {
    struct source_location const* location;
    struct cst_identifier const* identifier;
    // Template argument count of zero indicates that this symbol element has no
    // template arguments.
    sunder_sbuf(struct cst_template_argument const* const) template_arguments;
};
struct cst_symbol_element*
cst_symbol_element_new(
    struct cst_identifier const* identifier,
    struct cst_template_argument const* const* template_arguments);

// TODO: After removing the colon from the <template-parameter> production, a
// template parameter just an <identifier>. Can cst_template_parameter be
// removed completely? If constant values (i.e. non-type template parameters)
// are ever added then this production would still be needed.
struct cst_template_parameter {
    struct source_location const* location;
    struct cst_identifier const* identifier;
};
struct cst_template_parameter*
cst_template_parameter_new(
    struct source_location const* location,
    struct cst_identifier const* identifier);

// TODO: After removing the colon from the <template-argument> production, a
// template argument just a <typespec>. Can cst_template_argument be removed
// completely? If constant values (i.e. non-type template parameters) are ever
// added then this production would still be needed.
struct cst_template_argument {
    struct source_location const* location;
    struct cst_typespec const* typespec;
};
struct cst_template_argument*
cst_template_argument_new(
    struct source_location const* location,
    struct cst_typespec const* typespec);

struct cst_function_parameter {
    struct source_location const* location;
    struct cst_identifier const* identifier;
    struct cst_typespec const* typespec;
};
struct cst_function_parameter*
cst_function_parameter_new(
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec);

struct cst_member {
    struct source_location const* location;
    char const* name;
    enum {
        CST_MEMBER_VARIABLE,
        CST_MEMBER_CONSTANT,
        CST_MEMBER_FUNCTION,
    } kind;
    struct {
        struct {
            struct cst_identifier const* identifier;
            struct cst_typespec const* typespec;
        } variable;
        struct {
            // CST_DECL_CONSTANT
            struct cst_decl const* decl;
        } constant;
        struct {
            // CST_DECL_FUNCTION
            struct cst_decl const* decl;
        } function;
    } data;
};
struct cst_member*
cst_member_new_variable(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_typespec const* typespec);
struct cst_member*
cst_member_new_constant(struct cst_decl const* decl);
struct cst_member*
cst_member_new_function(struct cst_decl const* decl);

struct cst_member_initializer {
    struct source_location const* location;
    struct cst_identifier const* identifier;
    struct cst_expr const* expr;
};
struct cst_member_initializer*
cst_member_initializer_new(
    struct source_location const* location,
    struct cst_identifier const* identifier,
    struct cst_expr const* expr);

// ISO/IEC 9899:1999 Section 6.7.2 - Type Specifiers
struct cst_typespec {
    struct source_location const* location;
    enum typespec_kind {
        TYPESPEC_SYMBOL,
        TYPESPEC_FUNCTION,
        TYPESPEC_POINTER,
        TYPESPEC_ARRAY,
        TYPESPEC_SLICE,
        TYPESPEC_TYPEOF
    } kind;
    union {
        struct cst_symbol const* symbol;
        struct {
            sunder_sbuf(struct cst_typespec const* const) parameter_typespecs;
            struct cst_typespec const* return_typespec;
        } function;
        struct {
            struct cst_typespec const* base;
        } pointer;
        struct {
            struct cst_expr const* count;
            struct cst_typespec const* base;
        } array;
        struct {
            struct cst_typespec const* base;
        } slice;
        struct {
            struct cst_expr const* expr;
        } typeof_;
    } data;
};
struct cst_typespec*
cst_typespec_new_symbol(struct cst_symbol const* symbol);
struct cst_typespec*
cst_typespec_new_function(
    struct source_location const* location,
    struct cst_typespec const* const* parameter_typespecs,
    struct cst_typespec const* return_typespec);
struct cst_typespec*
cst_typespec_new_pointer(
    struct source_location const* location, struct cst_typespec const* base);
struct cst_typespec*
cst_typespec_new_array(
    struct source_location const* location,
    struct cst_expr const* count,
    struct cst_typespec const* base);
struct cst_typespec*
cst_typespec_new_slice(
    struct source_location const* location, struct cst_typespec const* base);
struct cst_typespec*
cst_typespec_new_typeof(
    struct source_location const* location, struct cst_expr const* expr);

struct cst_identifier {
    struct source_location const* location;
    char const* name; // interned
};
struct cst_identifier*
cst_identifier_new(struct source_location const* location, char const* name);

struct cst_boolean {
    struct source_location const* location;
    bool value;
};
struct cst_boolean*
cst_boolean_new(struct source_location const* location, bool value);

struct cst_integer {
    struct source_location const* location;
    struct sunder_bigint const* value;
    char const* suffix; // interned
};
struct cst_integer*
cst_integer_new(
    struct source_location const* location,
    struct sunder_bigint const* value,
    char const* suffix);

////////////////////////////////////////////////////////////////////////////////
//////// parse.c ///////////////////////////////////////////////////////////////

void
parse(struct module* module);

////////////////////////////////////////////////////////////////////////////////
//////// order.c ///////////////////////////////////////////////////////////////

void
order(struct module* module);

////////////////////////////////////////////////////////////////////////////////
//////// tir.c /////////////////////////////////////////////////////////////////
// Tree-based intermediate representation.

// SIZEOF_UNSIZED and ALIGNOF_UNSIZED are given the largest possible value of a
// size_t so that checks such as assert(type->size <= 8u) in the resolve and
// code generation phases will fail for unsized types.
#define SIZEOF_UNSIZED ((size_t)SIZE_MAX)
#define ALIGNOF_UNSIZED ((size_t)SIZE_MAX)
struct type {
    char const* name; // Canonical human-readable type-name (interned)
    size_t size; // sizeof
    size_t align; // alignof
    // Symbol table corresponding to static symbols belonging to the type. This
    // symbol table does *NOT* contain symbols for struct member variables as
    // member variables are defined only on instances of a struct.
    struct symbol_table* symbols;

    enum type_kind {
        TYPE_ANY,
        TYPE_VOID,
        TYPE_BOOL,
        TYPE_BYTE,
        TYPE_U8, /* integer */
        TYPE_S8, /* integer */
        TYPE_U16, /* integer */
        TYPE_S16, /* integer */
        TYPE_U32, /* integer */
        TYPE_S32, /* integer */
        TYPE_U64, /* integer */
        TYPE_S64, /* integer */
        TYPE_USIZE, /* integer */
        TYPE_SSIZE, /* integer */
        TYPE_INTEGER, /* integer */
        TYPE_FUNCTION,
        TYPE_POINTER,
        TYPE_ARRAY,
        TYPE_SLICE,
        TYPE_STRUCT,
    } kind;
    union {
        struct {
            // NOTE: The min and max fields are "optional" in the sense that
            // they are not defined for all types satisfying the
            // type_is_any_integer function. The type kind TYPE_INTEGER will
            // have these as NULL as integers of this type have no defined size.
            struct sunder_bigint const* min; // optional
            struct sunder_bigint const* max; // optional
        } integer;
        struct {
            sunder_sbuf(struct type const* const) parameter_types;
            struct type const* return_type;
        } function;
        struct {
            struct type const* base;
        } pointer;
        struct {
            size_t count;
            struct type const* base;
        } array;
        struct {
            struct type const* base;
        } slice;
        struct {
            // Offset of the next member variable that would be added to this
            // struct. Initialized to zero upon struct creation and updated
            // every time a member variable is added to the struct.
            //
            //      # size == 0
            //      # next_offset == 0
            //      struct foo { }
            //
            //      # size == 2
            //      # next_offset == 2 (after adding x)
            //      struct foo {
            //          var x: u16 # bytes 0->1
            //      }
            //
            //      # size == 4
            //      # next_offset == 3 (after adding y)
            //      struct foo {
            //          var x: u16 # bytes 0->1
            //          var y: u8  # byte  2
            //                     # byte  3 (stride padding)
            //      }
            //
            //      # size == 16
            //      # next_offset == 16 (after adding z)
            //      struct foo {
            //          var x: u16 # bytes 0->1
            //          var y: u8  # byte  2
            //                     # bytes 3->7 (padding)
            //          var z: u64 # bytes 8->15
            //      }
            size_t next_offset;
            // List of member variables within the struct ordered by offset into
            // the struct (i.e. their declaration order).
            struct member_variable {
                char const* name; // interned
                struct type const* type;
                size_t offset;
            } /*sbuf*/ * member_variables;
        } struct_;
    } data;
};
struct type*
type_new_any(void);
struct type*
type_new_void(void);
struct type*
type_new_bool(void);
struct type*
type_new_byte(void);
struct type*
type_new_u8(void);
struct type*
type_new_s8(void);
struct type*
type_new_u16(void);
struct type*
type_new_s16(void);
struct type*
type_new_u32(void);
struct type*
type_new_s32(void);
struct type*
type_new_u64(void);
struct type*
type_new_s64(void);
struct type*
type_new_usize(void);
struct type*
type_new_ssize(void);
struct type*
type_new_integer(void);
struct type*
type_new_function(
    struct type const* const* parameter_types, struct type const* return_type);
struct type*
type_new_pointer(struct type const* base);
struct type*
type_new_array(size_t count, struct type const* base);
struct type*
type_new_slice(struct type const* base);
// Create a new struct with no members (size zero and alignment zero).
struct type*
type_new_struct(char const* name, struct symbol_table* symbols);
// Add a member variable definition to the end of the provided struct type.
void
type_struct_add_member_variable(
    struct type* self, char const* name, struct type const* type);
// Returns the index of the member variable `name` of the provided struct type.
// Returns a non-negative integer index on success.
// Returns a -1 on failure.
long
type_struct_member_variable_index(struct type const* self, char const* name);
// Returns a pointer to the member variable `name` of the provided struct type.
// Returns a pointer to the member variable success.
// Returns NULL on failure.
struct member_variable const*
type_struct_member_variable(struct type const* self, char const* name);

struct type const*
type_unique_function(
    struct type const* const* parameter_types, struct type const* return_type);
struct type const*
type_unique_pointer(struct type const* base);
struct type const*
type_unique_array(size_t count, struct type const* base);
struct type const*
type_unique_slice(struct type const* base);

// Returns the symbol of the member function `name` of the provided type.
// Returns a symbol pointer for the member function on success.
// Returns NULL on failure.
struct symbol const*
type_member_function_symbol(struct type const* self, char const* name);
// Returns a pointer to the member function `name` of the provided type.
// Returns a pointer to the member function on success.
// Returns NULL on failure.
struct function const*
type_member_function(struct type const* self, char const* name);

bool
type_is_any_integer(struct type const* self);
bool
type_is_unsigned_integer(struct type const* self);
bool
type_is_signed_integer(struct type const* self);
// Returns true if the type may be compared with the == or != operators.
bool
type_can_compare_equality(struct type const* self);
// Returns true if the type may be compared with the ==, !=, <, <=, >, and >=
// operators.
bool
type_can_compare_order(struct type const* self);

struct address {
    enum address_kind {
        ADDRESS_STATIC,
        ADDRESS_LOCAL,
    } kind;
    union {
        struct {
            // Full normalized name, including nested namespace information,
            // uniquely identifying the base region of the static storage
            // location in which this address resides.
            char const* name; // interned
            // Offset (in bytes) from the base region.
            size_t offset;
        } static_;
        struct {
            int rbp_offset;
        } local;
    } data;
};
struct address
address_init_static(char const* name, size_t offset);
struct address
address_init_local(int rbp_offset);
struct address*
address_new(struct address from);

struct symbol {
    struct source_location const* location;
    char const* name; // interned
    enum symbol_kind {
        SYMBOL_TYPE,
        SYMBOL_VARIABLE,
        SYMBOL_CONSTANT,
        SYMBOL_FUNCTION,
        SYMBOL_TEMPLATE,
        SYMBOL_NAMESPACE,
    } kind;
    union {
        struct type const* type;
        struct {
            struct type const* type;
            struct address const* address;
            // Compile-time value of the variable. Non-NULL for non-extern
            // global variables. NULL for extern global and local variables.
            struct value const* value; // Optional.
        } variable;
        struct {
            struct type const* type;
            struct address const* address; // Always ADDRESS_STATIC.
            struct value const* value;
        } constant;
        struct function const* function;
        struct {
            // Original CST of this template.
            struct cst_decl const* decl;
            // Prefix used when instantiating the template.
            char const* symbol_addr_prefix;
            // Parent symbol table to be used when creating the instance symbol
            // table for an instantiation of this template.
            struct symbol_table* parent_symbol_table;
            // Symbols corresponding to instances of this template.
            struct symbol_table* symbols;
        } template;
        struct {
            // Symbols under the namespace.
            struct symbol_table* symbols;
        } namespace;
    } data;
};
struct symbol*
symbol_new_type(
    struct source_location const* location, struct type const* type);
struct symbol*
symbol_new_variable(
    struct source_location const* location,
    char const* name,
    struct type const* type,
    struct address const* address,
    struct value const* value);
struct symbol*
symbol_new_constant(
    struct source_location const* location,
    char const* name,
    struct type const* type,
    struct address const* address,
    struct value const* value);
struct symbol*
symbol_new_function(
    struct source_location const* location, struct function const* function);
struct symbol*
symbol_new_template(
    struct source_location const* location,
    char const* name,
    struct cst_decl const* decl,
    char const* symbol_addr_prefix,
    struct symbol_table* parent_symbol_table,
    struct symbol_table* symbols);
struct symbol*
symbol_new_namespace(
    struct source_location const* location,
    char const* name,
    struct symbol_table* symbols);

struct type const*
symbol_xget_type(struct symbol const* self);
struct address const*
symbol_xget_address(struct symbol const* self);
struct value const*
symbol_xget_value(struct symbol const* self);

struct symbol_table_element {
    // Name associated with this name-to-symbol mapping within the symbol
    // table. The name is not necessarily equal to the `name` member of the
    // symbol, as multiple names may map to the same symbol via aliases.
    char const* name; // interned
    struct symbol const* symbol;
};
struct symbol_table {
    struct symbol_table const* parent; // optional (NULL => global scope)
    sunder_sbuf(struct symbol_table_element) elements;
};
struct symbol_table*
symbol_table_new(struct symbol_table const* parent);
void
symbol_table_freeze(struct symbol_table* self, struct sunder_freezer* freezer);
void
symbol_table_insert(
    struct symbol_table* self,
    char const* name,
    struct symbol const* symbol,
    bool allow_redeclaration);
// Lookup in this or any parent symbol table.
struct symbol const*
symbol_table_lookup(struct symbol_table const* self, char const* name);
// Lookup in this symbol table only.
struct symbol const*
symbol_table_lookup_local(struct symbol_table const* self, char const* name);

struct stmt {
    struct source_location const* location;
    enum stmt_kind {
        STMT_IF,
        STMT_FOR_RANGE,
        STMT_FOR_EXPR,
        STMT_BREAK, /* no .data member */
        STMT_CONTINUE, /* no .data member */
        STMT_DUMP,
        STMT_RETURN,
        STMT_ASSIGN,
        STMT_EXPR,
    } kind;
    union {
        struct {
            sunder_sbuf(struct conditional const* const) conditionals;
        } if_;
        struct {
            struct symbol const* loop_variable;
            struct expr const* begin;
            struct expr const* end;
            struct block const* body;
        } for_range;
        struct {
            struct expr const* expr;
            struct block const* body;
        } for_expr;
        struct {
            struct expr const* expr;
        } dump;
        struct {
            struct expr const* expr; // optional
        } return_;
        struct {
            struct expr const* lhs;
            struct expr const* rhs;
        } assign;
        struct expr const* expr;
    } data;
};
struct stmt*
stmt_new_if(struct conditional const* const* conditionals);
struct stmt*
stmt_new_for_range(
    struct source_location const* location,
    struct symbol const* loop_variable,
    struct expr const* begin,
    struct expr const* end,
    struct block const* body);
struct stmt*
stmt_new_for_expr(
    struct source_location const* location,
    struct expr const* expr,
    struct block const* body);
struct stmt*
stmt_new_break(struct source_location const* location);
struct stmt*
stmt_new_continue(struct source_location const* location);
struct stmt*
stmt_new_dump(struct source_location const* location, struct expr const* expr);
struct stmt*
stmt_new_return(
    struct source_location const* location, struct expr const* expr);
struct stmt*
stmt_new_assign(
    struct source_location const* location,
    struct expr const* lhs,
    struct expr const* rhs);
struct stmt*
stmt_new_expr(struct source_location const* location, struct expr const* expr);

// Minimum and maximum number of syscall arguments (including the syscall
// number) passed to a syscall expression. This is based on the Linux syscall
// convention which allows for a maximum of six parameters plus the syscall
// number to be passed via registers.
#define SYSCALL_ARGUMENTS_MIN ((size_t)1)
#define SYSCALL_ARGUMENTS_MAX ((size_t)7)
struct expr {
    struct source_location const* location;
    struct type const* type;
    enum expr_kind {
        EXPR_SYMBOL,
        EXPR_BOOLEAN,
        EXPR_INTEGER,
        EXPR_BYTES,
        EXPR_ARRAY,
        EXPR_SLICE,
        EXPR_ARRAY_SLICE,
        EXPR_STRUCT,
        EXPR_CAST,
        EXPR_SYSCALL,
        EXPR_CALL,
        EXPR_ACCESS_INDEX,
        EXPR_ACCESS_SLICE,
        EXPR_ACCESS_MEMBER_VARIABLE,
        EXPR_SIZEOF,
        EXPR_ALIGNOF,
        EXPR_UNARY,
        EXPR_BINARY,
    } kind;
    union {
        struct symbol const* symbol;
        bool boolean;
        struct sunder_bigint const* integer;
        struct {
            struct address const* address;
            size_t count;
        } bytes;
        struct {
            sunder_sbuf(struct expr const* const) elements;
            struct expr const* ellipsis; // optional
        } array;
        struct {
            struct expr const* pointer;
            struct expr const* count;
        } slice;
        struct {
            struct symbol const* array_symbol;
            sunder_sbuf(struct expr const* const) elements;
        } array_slice;
        struct {
            // List of elements corresponding the member variables defined by
            // the struct type.
            sunder_sbuf(struct expr const* const) member_variables;
        } struct_;
        struct {
            struct expr const* expr;
        } cast;
        struct {
            sunder_sbuf(struct expr const* const) arguments;
        } syscall;
        struct {
            // Expression resulting in a callable function.
            struct expr const* function;
            // Arguments to the callable function.
            sunder_sbuf(struct expr const* const) arguments;
        } call;
        struct {
            struct expr const* lhs;
            struct expr const* idx;
        } access_index;
        struct {
            struct expr const* lhs;
            struct expr const* begin;
            struct expr const* end;
        } access_slice;
        struct {
            struct expr const* lhs;
            struct member_variable const* member_variable;
        } access_member_variable;
        struct {
            struct type const* rhs;
        } sizeof_;
        struct {
            struct type const* rhs;
        } alignof_;
        struct {
            enum uop_kind {
                UOP_NOT,
                UOP_POS,
                UOP_NEG,
                UOP_BITNOT,
                UOP_DEREFERENCE,
                UOP_ADDRESSOF,
                UOP_COUNTOF,
            } op;
            // Called the "right hand side" even though the expression is
            // actually on the left hand side of the .* operator.
            struct expr const* rhs;
        } unary;
        struct {
            enum bop_kind {
                BOP_OR,
                BOP_AND,
                BOP_EQ,
                BOP_NE,
                BOP_LE,
                BOP_LT,
                BOP_GE,
                BOP_GT,
                BOP_ADD,
                BOP_SUB,
                BOP_MUL,
                BOP_DIV,
                BOP_BITOR,
                BOP_BITXOR,
                BOP_BITAND,
            } op;
            struct expr const* lhs;
            struct expr const* rhs;
        } binary;
    } data;
};
struct expr*
expr_new_symbol(
    struct source_location const* location, struct symbol const* symbol);
struct expr*
expr_new_boolean(struct source_location const* location, bool value);
struct expr*
expr_new_integer(
    struct source_location const* location,
    struct type const* type,
    struct sunder_bigint const* value);
struct expr*
expr_new_bytes(
    struct source_location const* location,
    struct address const* address,
    size_t count);
struct expr*
expr_new_array(
    struct source_location const* location,
    struct type const* type,
    struct expr const* const* elements,
    struct expr const* ellipsis);
struct expr*
expr_new_slice(
    struct source_location const* location,
    struct type const* type,
    struct expr const* pointer,
    struct expr const* count);
struct expr*
expr_new_array_slice(
    struct source_location const* location,
    struct type const* type,
    struct symbol const* array_symbol,
    struct expr const* const* elements);
struct expr*
expr_new_struct(
    struct source_location const* location,
    struct type const* type,
    struct expr const* const* member_variables);
struct expr*
expr_new_cast(
    struct source_location const* location,
    struct type const* type,
    struct expr const* expr);
struct expr*
expr_new_syscall(
    struct source_location const* location,
    struct expr const* const* arguments);
struct expr*
expr_new_call(
    struct source_location const* location,
    struct expr const* function,
    struct expr const* const* arguments);
struct expr*
expr_new_access_index(
    struct source_location const* location,
    struct expr const* lhs,
    struct expr const* idx);
struct expr*
expr_new_access_slice(
    struct source_location const* location,
    struct expr const* lhs,
    struct expr const* begin,
    struct expr const* end);
struct expr*
expr_new_access_member_variable(
    struct source_location const* location,
    struct expr const* lhs,
    struct member_variable const* member_variable);
struct expr*
expr_new_sizeof(struct source_location const* location, struct type const* rhs);
struct expr*
expr_new_alignof(
    struct source_location const* location, struct type const* rhs);
struct expr*
expr_new_unary(
    struct source_location const* location,
    struct type const* type,
    enum uop_kind op,
    struct expr const* rhs);
struct expr*
expr_new_binary(
    struct source_location const* location,
    struct type const* type,
    enum bop_kind op,
    struct expr const* lhs,
    struct expr const* rhs);
// ISO/IEC 9899:1999 Section 6.3.2.1
// https://en.cppreference.com/w/c/language/value_category
bool
expr_is_lvalue(struct expr const* self);

struct function {
    char const* name; // interned
    struct type const* type; // TYPE_FUNCTION
    struct address const* address; // ADDRESS_STATIC
    // The value associated with this function. Self-referential, this member
    // points to a value that contains a pointer to the address of this struct.
    // Initially NULL, but set shortly after the allocation and initialization
    // of this object during the resolve phase.
    struct value const* value;

    // Outermost symbol table containing symbols for function parameters, local
    // variables, and local constants in the outermost scope (i.e. body) of the
    // function. Initialized to NULL on struct creation.
    struct symbol_table const* symbol_table;
    // Initialized to NULL on struct creation.
    sunder_sbuf(struct symbol const* const) symbol_parameters;
    // Initialized to NULL on struct creation.
    struct symbol const* symbol_return;
    // Initialized to NULL on struct creation.
    struct block const* body;

    // Offset required to store all local variables in this function.
    // When the function is entered the stack pointer will be offset by this
    // amount before any expressions are pushed/popped to/from the stack during
    // intermediate calculations.
    int local_stack_offset;
};
// Creates a new incomplete (empty) function.
// The type of the function must be of kind TYPE_FUNCTION.
// The address of the function must be of kind ADDRESS_STATIC.
struct function*
function_new(
    char const* name, struct type const* type, struct address const* address);
void
function_del(struct function* self);

struct conditional {
    struct source_location const* location;
    struct expr const* condition; // optional (NULL => else)
    struct block const* body;
};
struct conditional*
conditional_new(
    struct source_location const* location,
    struct expr const* condition,
    struct block const* body);

struct block {
    struct source_location const* location;
    struct symbol_table* symbol_table; // not owned
    sunder_sbuf(struct stmt const* const) stmts;
};
struct block*
block_new(
    struct source_location const* location,
    struct symbol_table* symbol_table,
    struct stmt const* const* stmts);

struct value {
    struct type const* type;
    struct {
        bool boolean;
        uint8_t byte;
        struct sunder_bigint* integer;
        struct function const* function;
        struct address pointer;
        struct {
            // Concrete values specified for elements of the array value before
            // the optional ellipsis element. The sunder_sbuf_count of the
            // elements member may be less than countof(array), in which case
            // the ellipsis value represents the rest of the elements upto
            // the countof(array)th element.
            sunder_sbuf(struct value*) elements;
            // Value representing elements from indices within the half-open
            // range [sunder_sbuf_count(elements), countof(array)) that are
            // initialized via an ellipsis element. NULL if no ellipsis element
            // was specified in the parse tree for the array value.
            struct value* ellipsis; // optional
        } array;
        struct {
            struct value* pointer; // TYPE_POINTER
            struct value* count; // TYPE_USIZE
        } slice;
        struct {
            // The members struct is an array with length equal to the struct
            // type's member_variables stretchy buffer. The Xth element of this
            // array corresponds to the Xth member variable within the struct
            // type definition. Upon the creation of a value object, each value
            // pointer in the member_variables array will be initialized to
            // NULL, and the values of each member variable must be explicitly
            // set before the value object may be used.
            sunder_sbuf(struct value*) member_variables;
        } struct_;
    } data;
};
struct value*
value_new_boolean(bool boolean);
struct value*
value_new_byte(uint8_t byte);
struct value*
value_new_integer(struct type const* type, struct sunder_bigint* integer);
struct value*
value_new_function(struct function const* function);
struct value*
value_new_pointer(struct type const* type, struct address address);
struct value*
value_new_array(
    struct type const* type, struct value** elements, struct value* ellipsis);
struct value*
value_new_slice(
    struct type const* type, struct value* pointer, struct value* count);
struct value*
value_new_struct(struct type const* type);
void
value_del(struct value* self);
void
value_freeze(struct value* self, struct sunder_freezer* freezer);
struct value*
value_clone(struct value const* self);

// Get the value associated with the struct member `self.name`.
struct value const*
value_get_member(struct value const* self, char const* name);
// Set the value associated with the struct member `self.name`.
void
value_set_member(struct value* self, char const* name, struct value* value);

bool
value_eq(struct value const* lhs, struct value const* rhs);
bool
value_lt(struct value const* lhs, struct value const* rhs);
bool
value_gt(struct value const* lhs, struct value const* rhs);

uint8_t* // sbuf
value_to_new_bytes(struct value const* value);

////////////////////////////////////////////////////////////////////////////////
//////// resolve.c /////////////////////////////////////////////////////////////

void
resolve(struct module* module);

////////////////////////////////////////////////////////////////////////////////
//////// eval.c ////////////////////////////////////////////////////////////////

struct value*
eval_rvalue(struct expr const* expr);
struct value*
eval_lvalue(struct expr const* expr);

////////////////////////////////////////////////////////////////////////////////
//////// codegen.c /////////////////////////////////////////////////////////////

void
codegen(char const* const opt_o, bool opt_k);

#endif // SUNDER_H_INCLUDED
