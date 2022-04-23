#!/bin/sh

for arg in "$@"; do
case "${arg}" in
    -h|--help|help)
        echo "Usage: $(basename $0) [FILE]"
        exit 0
        ;;
    *)
        ;;
esac
done

export SUNDER_HOME="$(realpath ..)"
export SUNDER_IMPORT_PATH="${SUNDER_HOME}/lib"

TESTSRUN=0
FAILURES=0

test() {
    TEST="$1"

    echo "[= RUN ${TEST} =]"
    RECEIVED=$(\
        cd $(dirname "${TEST}") && \
        "${SUNDER_HOME}/bin/sunder-run" $(basename "${TEST}") 2>&1)
    EXPECTED=$(\
        sed -n '/^########\(########\)*/,$p' "${TEST}" |\
        sed '1d' |\
        sed 's/^# \?//g')
    if [ "${EXPECTED}" = "${RECEIVED}" ]; then
        echo '[= PASS =]'
    else
        echo "EXPECTED:"
        echo "${EXPECTED}" | sed 's/^/> /g'
        echo "RECEIVED:"
        echo "${RECEIVED}" | sed 's/^/> /g'
        echo '[= FAIL =]'
        FAILURES=$((FAILURES + 1))
    fi
    TESTSRUN=$((TESTSRUN + 1))
}

if [ "$#" -ne 0 ]; then
    TESTS="$@"
else
    TESTS=$(echo *.sunder | sort)
fi

for t in ${TESTS}; do
    test "${t}"
done

echo "TESTS RUN => ${TESTSRUN}"
echo "FAILURES  => ${FAILURES}"

[ "${FAILURES}" -eq 0 ] || exit 1
