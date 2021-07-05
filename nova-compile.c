// Copyright 2021 The Nova Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nova.h"

static char const* path = NULL;

static void
usage(void);
static void
argparse(int argc, char** argv);

int
main(int argc, char** argv)
{
    argparse(argc, argv);

    context_init();
    atexit(context_fini);

    load_module(path);
    codegen();

    return EXIT_SUCCESS;
}

static void
usage(void)
{
    // clang-format off
    char const* const lines[] = {
    // Exactly 72 '=' characters surrounded by '/*' and '*/'. The starting '"'
    // is just before column 1 of the resulting string literal. The ending '",'
    // is just after column 72 of the resulting string literal when vertically
    // aligned with the '*/'. Keeping text within the horizontal range of the
    // '=' characters will make sure that the resulting usage text is at most
    // 72 characters wide.
    /*========================================================================*/
     "Usage: nova-compile [OPTION]... PATH",
     "Options:",
     "  -h, --help       Display usage information and exit.",
    };
    // clang-format on
    for (size_t i = 0; i < AUTIL_ARRAY_COUNT(lines); ++i) {
        fprintf(stderr, "%s\n", lines[i]);
    }
}

static void
argparse(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage();
            exit(EXIT_SUCCESS);
        }
        if (strncmp(argv[i], "-", 1) == 0 || strncmp(argv[i], "--", 2) == 0) {
            fatal(
                NO_PATH,
                NO_LINE,
                "unrecognized command line option '%s'",
                argv[i]);
        }

        if (path != NULL) {
            fatal(NO_PATH, NO_LINE, "multiple input files");
        }
        path = argv[i];
    }

    if (path == NULL) {
        fatal(NO_PATH, NO_LINE, "no input file");
    }
}
