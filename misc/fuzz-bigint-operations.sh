#!/bin/sh
# Run from the misc directory.
#
# NOTE: Python uses a different algorithm for div and mod where Sunder matches
# the behavior of C99, so for now negative numbers are *not* used as they will
# produce different results between the three test programs.
set -eu

MIN='0'
MAX='+99999999999999999999999999999999999999999999999999'

sunder-compile -o fuzz-bigint-operations.sunder.out fuzz-bigint-operations.sunder
c99            -o fuzz-bigint-operations.c.out      fuzz-bigint-operations.c

test() {
    LHS=$(python3 -c "import random; print(random.randint($MIN, $MAX))")
    RHS=$(python3 -c "import random; print(random.randint($MIN, $MAX))")

    echo "LHS: ${LHS}"
    echo "RHS: ${RHS}"
    set -e
    PYTHON=$(python3 fuzz-bigint-operations.py           $LHS $RHS 2>&1)
    SUNDER=$(        ./fuzz-bigint-operations.sunder.out $LHS $RHS 2>&1)
    C99SRC=$(        ./fuzz-bigint-operations.c.out      $LHS $RHS 2>&1)
    set -e

    if [ "${PYTHON}" != "${SUNDER}" -o "${PYTHON}" != "${C99SRC}" ]; then
        printf 'PYTHON:\n%s\n' "${PYTHON}"
        printf '\n'
        printf 'SUNDER:\n%s\n' "${SUNDER}"
        printf '\n'
        printf 'C99SRC:\n%s\n' "${C99SRC}"
        exit 1
    fi
}

while true; do test; done
