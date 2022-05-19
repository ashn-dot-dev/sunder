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
