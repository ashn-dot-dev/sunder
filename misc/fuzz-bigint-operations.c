#define _XOPEN_SOURCE 700
#include "../sunder.h"
#include "../util.c"

int main(int argc, char** argv)
{
    struct bigint* const lhs = bigint_new_cstr(argv[1]);
    struct bigint* const rhs = bigint_new_cstr(argv[2]);
    struct bigint* const res = bigint_new(BIGINT_ZERO);

    bigint_add(res, lhs, rhs);
    puts(bigint_to_new_cstr(res, NULL));
    bigint_sub(res, lhs, rhs);
    puts(bigint_to_new_cstr(res, NULL));
    bigint_mul(res, lhs, rhs);
    puts(bigint_to_new_cstr(res, NULL));
    bigint_divrem(res, NULL, lhs, rhs);
    puts(bigint_to_new_cstr(res, NULL));
    bigint_divrem(NULL, res, lhs, rhs);
    puts(bigint_to_new_cstr(res, NULL));
}
