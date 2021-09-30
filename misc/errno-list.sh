#!/bin/sh
# Generate errno constants.

set -e

# The errno program comes from moreutils.
# https://joeyh.name/code/moreutils/
# apt install moreutils
which errno >/dev/null

# The output of errno uses the form:
# SYMBOL NUMBER The text of strerror

MIN=$(errno --list | cut -d ' ' -f 2 | sort -n | head -n 1)
MAX=$(errno --list | cut -d ' ' -f 2 | sort -n | tail -n 1)
# The value of length + 1 is used so that a constant 'FOO' can be rendered as
# 'FOO:<spaces>s32' instead of 'FOO<spaces>: s32'.
SYMBOL_WIDTH=$(errno --list | cut -d ' ' -f 1 | awk '{ print length + 1 }' | sort -n | tail -n 1)

for i in $(seq "${MIN}" "${MAX}"); do
    if ! errno "${i}" >/dev/null 2>&1; then
        continue
    fi

    OUTPUT=$(errno "${i}")
    SYMBOL=$(echo "${OUTPUT}" | cut -d ' ' -f 1)
    NUMBER=$(echo "${OUTPUT}" | cut -d ' ' -f 2)
    printf "const %-${SYMBOL_WIDTH}s s32 = ${NUMBER};\n" "${SYMBOL}:"
done
