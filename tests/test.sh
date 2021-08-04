#!/bin/sh

usage() {
    cat <<EOF
Usage: $(basename $0) FILE

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

if [ "$#" -ne 1 ]; then
    echo 'error: invalid number of arguments'
    usage
    exit 1
fi
TEST_FILE="$1"

TEST_NAME=$(basename "${TEST_FILE}" .nova)
echo "[= RUN ${TEST_NAME} =]"
export NOVA_HOME="$(dirname $0)/.."
RECEIVED=$("${NOVA_HOME}/bin/nova-run" "${TEST_FILE}" 2>&1)
EXPECTED=$(\
    sed -n '/^########\(########\)*/,$p' "${TEST_FILE}" |\
    sed '1d' |\
    sed 's/^# \?//g')
if [ "${EXPECTED}" = "${RECEIVED}" ]; then
    echo '[= PASS =]' && exit 0
else
    echo "EXPECTED:"
    echo "${EXPECTED}" | sed 's/^/> /g'
    echo "RECEIVED:"
    echo "${RECEIVED}" | sed 's/^/> /g'
    echo '[= FAIL =]' && exit 1
fi
