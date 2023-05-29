// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "sunder.h"

void
codegen(
    bool opt_c,
    bool opt_g,
    bool opt_k,
    char const* const* opt_L,
    char const* const* opt_l,
    char const* const opt_o,
    char const* const* paths)
{
    assert(opt_o != NULL);

    char const* const backend = context()->env.SUNDER_BACKEND;
    if (cstr_eq_ignore_case(backend, "c")) {
        codegen_c(opt_c, opt_g, opt_k, opt_L, opt_l, opt_o, paths);
        return;
    }

    if (cstr_eq_ignore_case(backend, "nasm")
        || cstr_eq_ignore_case(backend, "yasm")) {
        assert(sbuf_count(paths) == 0);
        codegen_nasm(opt_c, opt_g, opt_k, opt_L, opt_l, opt_o);
        return;
    }

    fatal(NO_LOCATION, "unrecognized backend `%s`", backend);
}
