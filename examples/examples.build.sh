#!/bin/sh
set -e

export SUNDER_HOME="$(realpath ..)"
export SUNDER_IMPORT_PATH="${SUNDER_HOME}/lib"

for f in *.sunder; do
    CMD="${SUNDER_HOME}/bin/sunder-compile -o ${f%.sunder} ${f}"
    echo $CMD
    eval $CMD

    if command -v emcc >/dev/null; then
        CMD="SUNDER_BACKEND=C SUNDER_ARCH=wasm32 SUNDER_HOST=freestanding SUNDER_CC=emcc SUNDER_CFLAGS='-Os -sASSERTIONS -sSINGLE_FILE --shell-file ${SUNDER_HOME}/lib/sys/sys.wasm32.html' sunder-compile -o ${f%.sunder}.html ${f}"
        echo $CMD
        eval $CMD
    fi
done

if [ "$(uname -m)" = "x86_64" ] && command -v nasm >/dev/null; then
    export SUNDER_SYSASM_PATH=custom-sys-asm/sys.asm
    CMD="SUNDER_BACKEND=nasm ${SUNDER_HOME}/bin/sunder-compile -o custom-sys-asm/custom-sys-asm custom-sys-asm/main.sunder"
    echo $CMD
    eval $CMD
fi

if command -v clang >/dev/null; then
    CMD="clang -c -o ffi/c-code.o ffi/calling-c-from-sunder.c"
    echo $CMD
    eval $CMD
    CMD="SUNDER_BACKEND=C ${SUNDER_HOME}/bin/sunder-compile -c -o ffi/calling-c-from-sunder.o ffi/calling-c-from-sunder.sunder"
    echo $CMD
    eval $CMD
    CMD="clang -o ffi/calling-c-from-sunder -lm ffi/calling-c-from-sunder.o ffi/c-code.o"
    echo $CMD
    eval $CMD

    CMD="SUNDER_BACKEND=C ${SUNDER_HOME}/bin/sunder-compile -c -o ffi/sunder-code.o ffi/calling-sunder-from-c.sunder"
    echo $CMD
    eval $CMD
    CMD="clang -o ffi/calling-sunder-from-c -lm ffi/calling-sunder-from-c.c ffi/sunder-code.o"
    echo $CMD
    eval $CMD
fi
