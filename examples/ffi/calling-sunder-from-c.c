#include <string.h>

extern void __sunder_println(char* start, size_t count);

int
main(void)
{
    char* str = "HELLO";
    __sunder_println(str, strlen(str));
}
