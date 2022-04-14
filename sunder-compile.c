// Copyright 2021-2022 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#define _XOPEN_SOURCE /* getopt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h> /* getopt */

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

    load_module(path, canonical_path(path));
    codegen(opt_o, opt_k);

    return EXIT_SUCCESS;
}

static void
usage(void)
{
    // clang-format off
    char const* const lines[] = {
   "Usage: sunder-compile [OPTION]... FILE",
   "",
   "Options:",
   "  -k        Keep intermediate files (.o and .asm).",
   "  -o OUT    Write output excutable to OUT (default a.out).",
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
    while ((c = getopt(argc, argv, "ko:h")) != -1) {
        switch (c) {
        case 'k': {
            opt_k = true;
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
            fatal(NULL, "multiple input files");
        }
        path = argv[i];
    }

    if (path == NULL) {
        fatal(NULL, "no input file");
    }
}
