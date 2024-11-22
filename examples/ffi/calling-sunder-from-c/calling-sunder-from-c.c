#include <stdio.h>
#include <string.h>

extern int examplelib_x;
extern int const examplelib_y;
extern void examplelib_puts(char const* start, size_t count);
extern void examplelib_yell(char const* start, size_t count);

int
main(void)
{
    char const* lower = "hello";
    examplelib_puts(lower, strlen(lower));

    char const* upper = "HELLO";
    examplelib_yell(upper, strlen(upper));

    examplelib_x += 1;
    printf("x + y = %d\n", examplelib_x + examplelib_y);
}
