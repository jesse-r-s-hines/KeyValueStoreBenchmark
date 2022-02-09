#!/bin/bash
cd "$(dirname "$0")"

./install_dependencies.sh

mkdir -p build
cd build
cmake .. "-DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake" && cmake --build .
