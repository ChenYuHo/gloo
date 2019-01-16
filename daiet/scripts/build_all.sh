#!/bin/bash

set -e
set -x
cd ../lib/dpdk/
rm -rf build
make defconfig T=x86_64-native-linuxapp-gcc
make EXTRA_CFLAGS="-fPIC" -j
cd ../..
make clean
rm -rf build
make -j
cd ..
rm -rf build
mkdir build
cd build
cmake -DUSE_REDIS=ON ..
make -j
cd ../experiments/exp1/
mkdir -p build
cd build
find . ! -name 'daiet.cfg'   ! -name '.'  ! -name '..' -exec rm -rf {} +
cmake -DUSE_MLX5=ON ..
make -j
cd ../../exp2
mkdir -p build
cd build
find . ! -name 'daiet.cfg'   ! -name '.'  ! -name '..' -exec rm -rf {} +
cmake -DUSE_MLX5=ON ..
make -j
