#include <stdint.h>
#include <stdio.h>

int
main(void)
{
    int8_t lhs = -1;
    size_t rhs = 1;
    printf("lhs = 0x%02hhX\n", lhs);
    printf("rhs = 0x%02zX\n", rhs);
    int8_t res = lhs << rhs;
    printf("res = 0x%02hhX\n", res);

    printf("0x%02hhd\n", (int8_t)0xfe);
    printf("0x%02hhd\n", (uint8_t)((uint8_t)0xff << 1));
    printf("%02hhu\n", (uint8_t)((uint8_t)0xff << 1));
}
