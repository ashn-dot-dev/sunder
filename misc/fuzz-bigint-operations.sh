#!/bin/sh
# Run from the misc directory.
MIN='-99999999999999999999999999999999999999999999999999'
MAX='+99999999999999999999999999999999999999999999999999'

LHS=$(python3 -c "import random; print(random.randint($MIN, $MAX))")
RHS=$(python3 -c "import random; print(random.randint($MIN, $MAX))")

echo "LHS: ${LHS}"
echo "RHS: ${RHS}"
EXPECTED=$(python3    fuzz-bigint-operations.py     $LHS $RHS 2>&1)
RECEIVED=$(sunder-run fuzz-bigint-operations.sunder $LHS $RHS 2>&1)

if [ "${EXPECTED}" != "${RECEIVED}" ]; then
    printf 'EXPECTED:\n%s\n' "${EXPECTED}"
    printf 'RECEIVED:\n%s\n' "${RECEIVED}"
    exit 1
fi

echo 'PASSED'
