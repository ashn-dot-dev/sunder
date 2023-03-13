#!/bin/sh
set -e

export SUNDER_HOME="$(realpath ..)"
export SUNDER_IMPORT_PATH="${SUNDER_HOME}/lib"

for f in *.sunder; do
    CMD="${SUNDER_HOME}/bin/sunder-compile -o ${f%.sunder} ${f}"
    echo $CMD
    eval $CMD
done

export SUNDER_SYSASM_PATH=custom-sys-asm/sys.asm
CMD="${SUNDER_HOME}/bin/sunder-compile -o custom-sys-asm/custom-sys-asm custom-sys-asm/main.sunder"
echo $CMD
eval $CMD

if command -v clang >/dev/null; then
    CMD="SUNDER_BACKEND=C ${SUNDER_HOME}/bin/sunder-compile -o ffi/calling-c-from-sunder ffi/calling-c-from-sunder.sunder"
    echo $CMD
    eval $CMD

    CMD="SUNDER_BACKEND=C ${SUNDER_HOME}/bin/sunder-compile -c -o ffi/sunder-code ffi/calling-sunder-from-c.sunder"
    echo $CMD
    eval $CMD
    CMD="clang -o ffi/calling-sunder-from-c ffi/calling-sunder-from-c.c ffi/sunder-code.o"
    echo $CMD
    eval $CMD
fi
