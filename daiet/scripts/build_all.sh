#!/bin/bash

set -e
set -x
cd ../lib/dpdk/
rm -rf build
make defconfig T=x86_64-native-linuxapp-gcc
make -j
cd ../..
make clean
rm -rf build
make -j
cd ..
rm -rf build
mkdir build
cd build
cmake -DUSE_REDIS=ON -DUSE_AVX=ON ..
make -j
cd ../experiments/exp1/
mkdir -p build
cd build
rm CMakeCache.txt CMakeFiles cmake_install.cmake Makefile exp1 -rf
cmake ..
make -j
cd ../../exp2
mkdir -p build
cd build
rm CMakeCache.txt CMakeFiles cmake_install.cmake Makefile exp2 -rf
cmake ..
make -j
