#!/bin/sh
# Generate errno constants.

set -e

# The errno program comes from moreutils.
#
# https://joeyh.name/code/moreutils/
#
# ```sh
# $ apt install moreutils
# ```
which errno >/dev/null

# The output of errno uses the form:
# SYMBOL NUMBER The text of strerror

MIN=$(errno --list | cut -d ' ' -f 2 | sort -n | head -n 1)
MAX=$(errno --list | cut -d ' ' -f 2 | sort -n | tail -n 1)
# The value of length + 1 is used so that a constant 'FOO' can be rendered as
# 'FOO:<spaces>ssize' instead of 'FOO<spaces>: ssize'.
SYMBOL_WIDTH=$(errno --list | cut -d ' ' -f 1 | awk '{ print length + 1 }' | sort -n | tail -n 1)

value() {
    SYMBOL=$1
    STRING=$2
    printf "\"[system error ${SYMBOL}] ${STRING}\""
}

STRING_WIDTH=0
for i in $(seq 0 "${MAX}"); do
    if ! errno "${i}" >/dev/null 2>&1; then
        continue
    fi

    OUTPUT=$(errno "${i}")
    SYMBOL=$(echo "${OUTPUT}" | cut -d ' ' -f 1)
    NUMBER=$(echo "${OUTPUT}" | cut -d ' ' -f 2)
    STRING=$(echo "${OUTPUT}" | cut -d ' ' -f 3-)

    WIDTH=$(value "${SYMBOL}" "${STRING}" | wc -c)
    if [ "${WIDTH}" -gt "${STRING_WIDTH}" ]; then
        STRING_WIDTH="${WIDTH}"
    fi
done

for i in $(seq "${MIN}" "${MAX}"); do
    if ! errno "${i}" >/dev/null 2>&1; then
        continue
    fi

    OUTPUT=$(errno "${i}")
    SYMBOL=$(echo "${OUTPUT}" | cut -d ' ' -f 1)
    NUMBER=$(echo "${OUTPUT}" | cut -d ' ' -f 2)
    printf "const %-${SYMBOL_WIDTH}s ssize = ${NUMBER};\n" "${SYMBOL}:"
done

printf '\n'

echo "const ERRNO_STRS = (:[][]byte)["
for i in $(seq 0 "${MAX}"); do
    COMMA=','
    if [ "${i}" = "${MAX}" ]; then
        COMMA=''
    fi

    if errno "${i}" >/dev/null 2>&1; then
        OUTPUT=$(errno "${i}")
        SYMBOL=$(echo "${OUTPUT}" | cut -d ' ' -f 1)
        NUMBER=$(echo "${OUTPUT}" | cut -d ' ' -f 2)
        STRING=$(echo "${OUTPUT}" | cut -d ' ' -f 3-)
    else
        SYMBOL="${i}"
        NUMBER="${i}"
        STRING="Undefined errno value"
    fi

    VALUE="$(value "${SYMBOL}" "${STRING}")${COMMA}"
    printf "    %-${STRING_WIDTH}s%s\n" "${VALUE}"
done
echo "];"
