#!/bin/sh
set -e

for f in *.sunder; do
    rm -f "${f%.sunder}"
done
