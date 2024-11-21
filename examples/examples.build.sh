#!/bin/sh
set -e

export SUNDER_HOME="$(realpath ..)"
export SUNDER_SEARCH_PATH="${SUNDER_HOME}/lib"
export SUNDER_CC="$(${SUNDER_HOME}/bin/sunder-compile -e | grep SUNDER_CC | cut -d= -f2)"
export SUNDER_CFLAGS="$(${SUNDER_HOME}/bin/sunder-compile -e | grep SUNDER_CFLAGS | cut -d= -f2)"

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

CMD="${SUNDER_HOME}/bin/sunder-compile -c -o ffi/calling-c-from-sunder/calling-c-from-sunder.o ffi/calling-c-from-sunder/calling-c-from-sunder.sunder"
echo $CMD
eval $CMD
CMD="${SUNDER_CC} ${SUNDER_CFLAGS} -c -o ffi/calling-c-from-sunder/c-code.o ffi/calling-c-from-sunder/calling-c-from-sunder.c"
echo $CMD
eval $CMD
CMD="${SUNDER_CC} ${SUNDER_CFLAGS} -o ffi/calling-c-from-sunder/calling-c-from-sunder ffi/calling-c-from-sunder/calling-c-from-sunder.o ffi/calling-c-from-sunder/c-code.o -lm"
echo $CMD
eval $CMD

CMD="${SUNDER_HOME}/bin/sunder-compile -c -o ffi/sunder-code.o ffi/calling-sunder-from-c.sunder"
echo $CMD
eval $CMD
CMD="${SUNDER_CC} ${SUNDER_CFLAGS} -o ffi/calling-sunder-from-c ffi/calling-sunder-from-c.c ffi/sunder-code.o -lm"
echo $CMD
eval $CMD
