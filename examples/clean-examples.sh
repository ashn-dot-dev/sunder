#!/bin/sh
set -e

for f in *.sunder; do
    rm -f "${f%.sunder}"
    rm -f "${f%.sunder}.asm"
    rm -f "${f%.sunder}.o"
done
