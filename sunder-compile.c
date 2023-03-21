// SPDX-License-Identifier: Apache-2.0
#define _XOPEN_SOURCE /* getopt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h> /* getopt */

#include "sunder.h"

// clang-format off
static char const*       path = NULL;
static bool              opt_c = false;
static bool              opt_k = false;
static sbuf(char const*) opt_L = NULL;
static sbuf(char const*) opt_l = NULL;
static char const*       opt_o = "a.out";
// clang-format on

static void
env(void);
static void
usage(void);
static void
argparse(int argc, char** argv);
static void
fini(void);

int
main(int argc, char** argv)
{
    atexit(fini);
    context_init();
    atexit(context_fini);

    argparse(argc, argv);

    load_module(path, canonical_path(path));
    if (!opt_c) {
        validate_main_is_defined_correctly();
    }

    codegen(opt_c, opt_k, opt_L, opt_l, opt_o);

    return EXIT_SUCCESS;
}

static void
env(void)
{
    printf("SUNDER_HOME=%s\n", context()->env.SUNDER_HOME);
    printf("SUNDER_BACKEND=%s\n", context()->env.SUNDER_BACKEND);
    printf("SUNDER_IMPORT_PATH=%s\n", context()->env.SUNDER_IMPORT_PATH);
    printf("SUNDER_SYSASM_PATH=%s\n", context()->env.SUNDER_SYSASM_PATH);
}

static void
usage(void)
{
    // clang-format off
    char const* const lines[] = {
   "Usage: sunder-compile [OPTION]... FILE",
   "",
   "Options:",
   "  -c        Compile and assemble, but do not link.",
   "  -e        Display the Sunder environment and exit.",
   "  -k        Keep intermediate files (.o and .asm).",
   "  -L DIR    Add DIR to the linker path.",
   "  -l OPT    Pass OPT directly to the linker.",
   "  -o OUT    Write output executable to OUT (default a.out).",
   "  -h        Display usage information and exit.",
    };
    // clang-format on
    for (size_t i = 0; i < ARRAY_COUNT(lines); ++i) {
        fprintf(stderr, "%s\n", lines[i]);
    }
}

static void
argparse(int argc, char** argv)
{
    int c = 0;
    while ((c = getopt(argc, argv, "cekL:l:o:h")) != -1) {
        switch (c) {
        case 'c': {
            opt_c = true;
            break;
        }
        case 'e': {
            env();
            exit(EXIT_SUCCESS);
            break;
        }
        case 'k': {
            opt_k = true;
            break;
        }
        case 'L': {
            sbuf_push(opt_L, optarg);
            break;
        }
        case 'l': {
            sbuf_push(opt_l, optarg);
            break;
        }
        case 'o': {
            opt_o = optarg;
            break;
        }
        case 'h': {
            usage();
            exit(EXIT_SUCCESS);
            break;
        }
        case '?': {
            exit(EXIT_FAILURE);
            break;
        }
        }
    }

    for (int i = optind; i < argc; ++i) {
        if (path != NULL) {
            fatal(NO_LOCATION, "multiple input files");
        }
        path = argv[i];
    }

    if (path == NULL) {
        fatal(NO_LOCATION, "no input file");
    }
}

static void
fini(void)
{
    sbuf_fini(opt_l);
}
