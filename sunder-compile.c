// SPDX-License-Identifier: Apache-2.0
#define _XOPEN_SOURCE /* getopt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h> /* getopt */

#include "sunder.h"

// clang-format off
static char const*       path = NULL;
static sbuf(char const*) a_paths = NULL;
static sbuf(char const*) c_paths = NULL;
static sbuf(char const*) o_paths = NULL;
static bool              opt_c = false;
static bool              opt_g = false;
static bool              opt_k = false;
static sbuf(char const*) opt_L = NULL;
static sbuf(char const*) opt_l = NULL;
static char const*       opt_o = "a.out";
// clang-format on

// List of additional .a, .c, and .o files.
static sbuf(char const*) paths = NULL;

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

    codegen(opt_c, opt_g, opt_k, opt_L, opt_l, opt_o, paths);

    return EXIT_SUCCESS;
}

static void
env(void)
{
    printf("SUNDER_HOME=%s\n", context()->env.SUNDER_HOME);
    printf("SUNDER_ARCH=%s\n", context()->env.SUNDER_ARCH);
    printf("SUNDER_HOST=%s\n", context()->env.SUNDER_HOST);
    printf("SUNDER_BACKEND=%s\n", context()->env.SUNDER_BACKEND);
    printf("SUNDER_SEARCH_PATH=%s\n", context()->env.SUNDER_SEARCH_PATH);
    printf("SUNDER_SYSASM_PATH=%s\n", context()->env.SUNDER_SYSASM_PATH);
    printf("SUNDER_CC=%s\n", context()->env.SUNDER_CC);
    printf("SUNDER_CFLAGS=%s\n", context()->env.SUNDER_CFLAGS);
}

static void
usage(void)
{
    // clang-format off
    char const* const lines[] = {
   "Usage: sunder-compile [OPTION...] FILE",
   "",
   "Options:",
   "  -c        Compile and assemble, but do not link.",
   "  -e        Display the Sunder environment and exit.",
   "  -g        Generate debug information in output files.",
   "  -k        Keep intermediate files.",
   "  -L DIR    Add DIR to the linker path.",
   "  -l OPT    Pass OPT directly to the linker.",
   "  -o OUT    Write output file to OUT (default a.out).",
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
    while ((c = getopt(argc, argv, "cegkL:l:o:h")) != -1) {
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
        case 'g': {
            opt_g = true;
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
        if (cstr_ends_with(argv[i], ".a")) {
            sbuf_push(a_paths, argv[i]);
            sbuf_push(paths, argv[i]);
            continue;
        }
        if (cstr_ends_with(argv[i], ".c")) {
            sbuf_push(c_paths, argv[i]);
            sbuf_push(paths, argv[i]);
            continue;
        }
        if (cstr_ends_with(argv[i], ".o")) {
            sbuf_push(o_paths, argv[i]);
            sbuf_push(paths, argv[i]);
            continue;
        }

        if (path != NULL) {
            fatal(
                NO_LOCATION,
                "multiple input files (`%s` and `%s` both specified)",
                path,
                argv[i]);
        }
        path = argv[i];
    }

    if (path == NULL) {
        fatal(NO_LOCATION, "no input file");
    }

    size_t const have_a_paths = sbuf_count(a_paths) != 0;
    size_t const have_c_paths = sbuf_count(c_paths) != 0;
    size_t const have_o_paths = sbuf_count(o_paths) != 0;

    bool const invalid_a_paths_with_opt_c = opt_c && have_a_paths;
    bool const invalid_c_paths_with_opt_c = opt_c && have_c_paths;
    bool const invalid_o_paths_with_opt_c = opt_c && have_o_paths;
    if (invalid_a_paths_with_opt_c) {
        fatal(NO_LOCATION, "cannot compile .a files with -c specified");
    }
    if (invalid_c_paths_with_opt_c) {
        fatal(NO_LOCATION, "cannot compile .c files with -c specified");
    }
    if (invalid_o_paths_with_opt_c) {
        fatal(NO_LOCATION, "cannot compile .o files with -c specified");
    }

    bool const using_c_backend =
        cstr_eq_ignore_case(context()->env.SUNDER_BACKEND, "c");
    bool const invalid_a_paths_with_backend = !using_c_backend && have_a_paths;
    bool const invalid_c_paths_with_backend = !using_c_backend && have_c_paths;
    bool const invalid_o_paths_with_backend = !using_c_backend && have_o_paths;
    if (invalid_a_paths_with_backend) {
        fatal(
            NO_LOCATION,
            "cannot compile .a files with backend `%s`",
            context()->env.SUNDER_BACKEND);
    }
    if (invalid_c_paths_with_backend) {
        fatal(
            NO_LOCATION,
            "cannot compile .c files with backend `%s`",
            context()->env.SUNDER_BACKEND);
    }
    if (invalid_o_paths_with_backend) {
        fatal(
            NO_LOCATION,
            "cannot compile .o files with backend `%s`",
            context()->env.SUNDER_BACKEND);
    }
}

static void
fini(void)
{
    sbuf_fini(opt_l);
}
