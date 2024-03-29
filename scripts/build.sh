#!/bin/bash
cd "$(dirname $(dirname "$0"))"
set -e

./scripts/installDependencies.sh

mkdir -p build
cd build
cmake .. "-DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake" && cmake --build .
