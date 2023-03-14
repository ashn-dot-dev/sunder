#include <string.h>

extern void mylib_println(char* start, size_t count);

int
main(void)
{
    char* str = "HELLO";
    mylib_println(str, strlen(str));
}
