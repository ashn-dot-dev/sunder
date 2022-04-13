// clang -std=c11 alignof.c
#include <stdalign.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define P(type) printf("%12s: %zu\n", #type, alignof(type))

int
main(void)
{
    P(uint8_t);
    P(uint16_t);
    P(uint32_t);
    P(uint64_t);
    P(uintptr_t);
    P(size_t);

    fputc('\n', stdout);

    P(float);
    P(double);
    P(long double);

    fputc('\n', stdout);

    P(uint8_t[5]);
    P(uint64_t[2]);

    fputc('\n', stdout);

    P(max_align_t);
}

// Linux x86-64
// ============
//      uint8_t: 1
//     uint16_t: 2
//     uint32_t: 4
//     uint64_t: 8
//    uintptr_t: 8
//       size_t: 8
//
//        float: 4
//       double: 8
//  long double: 16
//
//   uint8_t[5]: 1
//  uint64_t[2]: 8
//
//  max_align_t: 16
