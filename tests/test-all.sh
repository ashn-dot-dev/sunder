#!/bin/sh

usage() {
    cat <<EOF
Usage: $(basename $0)

Options:
  -h --help     Print this help message and exit.
EOF
}

while test "$#" -gt 0; do
case "$1" in
    -h|--help)
        usage
        exit 0
        ;;
    *)
        break
        ;;
esac
done

if [ "$#" -ne 0 ]; then
    echo 'error: invalid number of arguments'
    usage
    exit 1
fi

TESTSRUN=0
FAILURES=0
TESTS=$(echo *.sunder $(find std/ -name '*.sunder') | sort)
for t in ${TESTS}; do
    sh test.sh "${t}"
    RESULT="$?"

    TESTSRUN=$((TESTSRUN + 1))
    if [ "${RESULT}" -ne 0 ]; then
        FAILURES=$((FAILURES + 1))
    fi
done

echo "TESTS RUN => ${TESTSRUN}"
echo "FAILURES  => ${FAILURES}"

[ "${FAILURES}" -eq 0 ] || exit 1
