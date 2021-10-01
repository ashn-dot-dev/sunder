#!/bin/sh

set -e
set -x
VERSION=$(date -u +"%Y.%m.%d")
git tag -a "${VERSION}" -m "Release ${VERSION}"
