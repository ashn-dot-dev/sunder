#!/bin/sh
set -e

for f in *.sunder; do
    rm -f "${f%.sunder}"
    rm -f "${f%.sunder}.o"
    rm -f "${f%.sunder}.html"
done

rm -f \
    ffi/calling-c-from-sunder/calling-c-from-sunder \
    ffi/calling-sunder-from-c \
    $(find ffi -name '*.o')
