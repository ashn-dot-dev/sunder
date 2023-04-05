// Similar to errno-list.sh, but uses strerror directly instead of errno(1).
//
// clang misc/errno-list.c && ./a.out
// emcc -o errno-list.html misc/errno-list.c -sSINGLE_FILE --shell-file lib/sys/sys.wasm32.html
#include <stdio.h>
#include <string.h>

// Maximum errno value to check. Assumes that the POSIX system uses errno
// values ascending from zero up to this maximum value.
#define ERRNO_MAX 200

int
main(void)
{
    printf("let ERRORS = (:[][]byte)[\n");
    for (int e = 0; e <= ERRNO_MAX; ++e) {
        printf("    \"[system error %d] %s\",\n", e, strerror(e));
    }
    printf("];\n");
}
