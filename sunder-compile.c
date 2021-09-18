// Copyright 2021 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sunder.h"

// clang-format off
static char const* path = NULL;
static bool        opt_k = false;
static char const* opt_o = "a.out";
// clang-format on

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
    codegen(opt_o, opt_k);

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
     "Usage: sunder-compile [OPTION]... PATH",
     "Options:",
     "  -h, --help        Display usage information and exit.",
     "  -k, --keep        Keep intermediate files (.o and .asm).",
     "  -o FILE           Write output to FILE.",
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
        if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--keep") == 0) {
            opt_k = true;
            continue;
        }
        if (strcmp(argv[i], "-o") == 0) {
            if (++i == argc) {
                fatal(NULL, "-o FILE, FILE argument not specified");
            }
            if (autil_cstr_ends_with(argv[i], "/")) {
                fatal(NULL, "-o FILE, invalid FILE path");
            }
            opt_o = argv[i];
            continue;
        }
        if (strncmp(argv[i], "-", 1) == 0 || strncmp(argv[i], "--", 2) == 0) {
            fatal(NULL, "unrecognized command line option `%s`", argv[i]);
        }

        if (path != NULL) {
            fatal(NULL, "multiple input files");
        }
        path = argv[i];
    }

    if (path == NULL) {
        fatal(NULL, "no input file");
    }
}
