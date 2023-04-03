#!/bin/sh

set -e

usage() {
    echo "Usage: $(basename $0)"
    echo "Download and activate the latest version of the Emscripten toolchain."
    echo "This script should be run from the project root directory."
}

if [ $# != '0' ]; then
    usage
    exit 1
fi


if [ ! -d emsdk/ ]; then
    echo '>> Cloning the Emscripten SDK...'
    git clone https://github.com/emscripten-core/emsdk.git
fi

echo '>> Installing latest emsdk...'
(cd emsdk/ && ./emsdk install latest)

echo '>> Activating latest emsdk...'
(cd emsdk  && ./emsdk activate latest)
