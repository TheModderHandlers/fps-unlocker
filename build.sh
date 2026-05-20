#!/bin/bash
set -e

cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw32.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build -j
cp ./build/unlocker.dll ./fps-unlocker/unlocker.dll
