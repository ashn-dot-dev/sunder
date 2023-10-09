#include <stdio.h>
#include <string.h>

extern int mylib_x asm("mylib_x");
extern int const mylib_y asm("mylib_y");
extern void mylib_println(char* start, size_t count) asm("mylib_println");

int
main(void)
{
    char* str = "HELLO";
    mylib_println(str, strlen(str));

    mylib_x += 1;
    printf("x + y = %d\n", mylib_x + mylib_y);
}
