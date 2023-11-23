#!/bin/sh
set -e

export SUNDER_HOME="$(realpath ..)"
export SUNDER_SEARCH_PATH="${SUNDER_HOME}/lib"

for f in *.sunder; do
    # Native version.
    CMD="${SUNDER_HOME}/bin/sunder-compile -o ${f%.sunder} ${f}"
    echo $CMD
    eval $CMD

    # Web version with Emscripten.
    if command -v emcc >/dev/null; then
        CMD="SUNDER_ARCH=wasm32 SUNDER_HOST=emscripten SUNDER_CC=emcc SUNDER_CFLAGS='-g0 -Os -sASSERTIONS -sSINGLE_FILE --shell-file ${SUNDER_HOME}/lib/sys/sys.wasm32-emscripten.html' ${SUNDER_HOME}/bin/sunder-compile -o ${f%.sunder}.html ${f}"
        echo $CMD
        eval $CMD
    fi
done

if command -v clang >/dev/null; then
    CMD="clang -c -o ffi/c-code.o ffi/calling-c-from-sunder.c"
    echo $CMD
    eval $CMD
    CMD="SUNDER_CC=clang ${SUNDER_HOME}/bin/sunder-compile -c -o ffi/calling-c-from-sunder.o ffi/calling-c-from-sunder.sunder"
    echo $CMD
    eval $CMD
    CMD="clang -o ffi/calling-c-from-sunder -lm ffi/calling-c-from-sunder.o ffi/c-code.o"
    echo $CMD
    eval $CMD

    CMD="SUNDER_CC=clang ${SUNDER_HOME}/bin/sunder-compile -c -o ffi/sunder-code.o ffi/calling-sunder-from-c.sunder"
    echo $CMD
    eval $CMD
    CMD="clang -o ffi/calling-sunder-from-c -lm ffi/calling-sunder-from-c.c ffi/sunder-code.o"
    echo $CMD
    eval $CMD
fi
