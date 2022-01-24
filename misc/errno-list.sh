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
# 'FOO:<spaces>ssize' instead of 'FOO<spaces>: ssize'.
SYMBOL_WIDTH=$(errno --list | cut -d ' ' -f 1 | awk '{ print length + 1 }' | sort -n | tail -n 1)
# The value of length + 3 is used so that there is room for the opening and
# closing double quotes around the string plus an optional comma.
STRING_WIDTH=$(errno --list | cut -d ' ' -f 3- | awk '{ print length + 3 }' | sort -n | tail -n 1)

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

echo "const ERRNO_STRINGS: [$(($MAX + 1))][]byte = (:[$(($MAX + 1))][]byte)["
for i in $(seq 0 "${MAX}"); do
    COMMA=','
    if [ "${i}" = "${MAX}" ]; then
        COMMA=''
    fi

    if ! errno "${i}" >/dev/null 2>&1; then
        SYMBOL="Undefined errno value"
        NUMBER="${i}"
        STRING="${SYMBOL}"
        printf "    %-${STRING_WIDTH}s # ${SYMBOL} (${NUMBER})\n" "\"${STRING}\"${COMMA}"
        continue
    fi

    OUTPUT=$(errno "${i}")
    SYMBOL=$(echo "${OUTPUT}" | cut -d ' ' -f 1)
    NUMBER=$(echo "${OUTPUT}" | cut -d ' ' -f 2)
    STRING=$(echo "${OUTPUT}" | cut -d ' ' -f 3-)
    printf "    %-${STRING_WIDTH}s # ${SYMBOL} (${NUMBER})\n" "\"${STRING}\"${COMMA}"

done
echo "];"
