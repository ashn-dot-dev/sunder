// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "sunder.h"

void
codegen(
    bool opt_c,
    bool opt_k,
    char const* const* opt_L,
    char const* const* opt_l,
    char const* const opt_o)
{
    assert(opt_o != NULL);

    char const* const backend = context()->env.SUNDER_BACKEND;
    if (cstr_eq_ignore_case(backend, "C")) {
        codegen_c(opt_c, opt_k, opt_L, opt_l, opt_o);
        return;
    }

    if (cstr_eq_ignore_case(backend, "nasm")
        || cstr_eq_ignore_case(backend, "yasm")) {
        codegen_nasm(opt_c, opt_k, opt_L, opt_l, opt_o);
        return;
    }

    fatal(NO_LOCATION, "unrecognized backend `%s`", backend);
}
