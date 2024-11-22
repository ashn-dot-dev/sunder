#!/bin/sh
set -e

export SUNDER_HOME="$(realpath ..)"
export SUNDER_SEARCH_PATH="${SUNDER_HOME}/lib"
export SUNDER_CC="$(${SUNDER_HOME}/bin/sunder-compile -e | grep SUNDER_CC | sed -n 's/^SUNDER_CC=//gp')"
export SUNDER_CFLAGS="$(${SUNDER_HOME}/bin/sunder-compile -e | grep SUNDER_CFLAGS | sed -n 's/^SUNDER_CFLAGS=//gp')"

run() {
    echo "$@"
    eval "$@"
}

#### Single-File Sunder Examples ###############################################
for f in *.sunder; do
    # Native version.
    run "${SUNDER_HOME}/bin/sunder-compile -o ${f%.sunder} ${f}"

    # Web version with Emscripten.
    if command -v emcc >/dev/null; then
        run "SUNDER_ARCH=wasm32 SUNDER_HOST=emscripten SUNDER_CC=emcc SUNDER_CFLAGS='-g0 -Os -sASSERTIONS -sSINGLE_FILE --shell-file ${SUNDER_HOME}/lib/sys/sys.wasm32-emscripten.html' ${SUNDER_HOME}/bin/sunder-compile -o ${f%.sunder}.html ${f}"
    fi
done

#### [FFI] Calling C From Sunder ###############################################
DIR=ffi/calling-c-from-sunder
run "(cd $DIR && ${SUNDER_CC} ${SUNDER_CFLAGS} -c -o c-code.o calling-c-from-sunder.c)"
run "(cd $DIR && ${SUNDER_HOME}/bin/sunder-compile -o calling-c-from-sunder calling-c-from-sunder.sunder c-code.o)"

#### [FFI] Calling Sunder From C ###############################################
DIR=ffi/calling-sunder-from-c
run "(cd $DIR && ${SUNDER_HOME}/bin/sunder-compile -c -o sunder-code.o calling-sunder-from-c.sunder)"
run "(cd $DIR && ${SUNDER_CC} ${SUNDER_CFLAGS} -o calling-sunder-from-c calling-sunder-from-c.c sunder-code.o -lm)"
