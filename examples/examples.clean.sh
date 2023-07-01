#!/bin/sh
set -e

for f in *.sunder; do
    rm -f "${f%.sunder}"
    rm -f "${f%.sunder}.asm"
    rm -f "${f%.sunder}.o"
    rm -f "${f%.sunder}.html"
done

rm -f \
    custom-sys-asm/custom-sys-asm \
    custom-sys-asm/custom-sys-asm.asm \
    custom-sys-asm/custom-sys-asm.o

rm -f \
    ffi/calling-c-from-sunder \
    ffi/calling-sunder-from-c \
    ffi/*.asm \
    ffi/*.o
