#!/bin/bash
cd "$(dirname $(dirname "$0"))"

./scripts/install_dependencies.sh

mkdir -p build
cd build
cmake .. "-DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake" && cmake --build .
