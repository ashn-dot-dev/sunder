// Copyright 2023 The Sunder Project Authors
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "sunder.h"

static char const* DEFAULT_BACKEND = STRINGIFY(SUNDER_DEFAULT_BACKEND);

char const*
backend(void)
{
    char const* backend = getenv("SUNDER_BACKEND");
    if (backend == NULL) {
        backend = DEFAULT_BACKEND;
    }

    return intern_cstr(backend);
}

void
codegen(
    bool opt_c, bool opt_k, char const* const* opt_l, char const* const opt_o)
{
    assert(opt_o != NULL);

    if (0 == strcmp(backend(), "C") || 0 == strcmp(backend(), "c")) {
        codegen_c(opt_c, opt_k, opt_l, opt_o);
        return;
    }

    if (0 == strcmp(backend(), "nasm") || 0 == strcmp(backend(), "yasm")) {
        codegen_nasm(opt_c, opt_k, opt_l, opt_o);
        return;
    }

    fatal(NO_LOCATION, "unrecognized backend `%s`", backend());
}
