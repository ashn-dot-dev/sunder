#include <stdio.h>

int x = 100;
int const y = 200;

void
yell(char const* str)
{
    printf("%s!\n", str);
}
