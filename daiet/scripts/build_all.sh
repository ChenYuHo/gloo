#!/bin/bash

# FLAGS: INSTALL MLX5 COLOCATED LATENCIES TIMESTAMPS TIMERS DEBUG
set -e
set -x

CWD=`pwd`
DAIET_ARGS=""
DPDK_FLAGS="-fPIC "
HOROVOD_ARGS=''

if [[ $@ == *"COLOCATED"* ]]; then
  echo "COLOCATED ON"
  DAIET_ARGS+="COLOCATED=ON "
fi
if [[ $@ == *"LATENCIES"* ]]; then
  echo "LATENCIES ON"
  DAIET_ARGS+="LATENCIES=ON "
fi
if [[ $@ == *"TIMESTAMPS"* ]]; then
  echo "TIMESTAMPS ON"
  DAIET_ARGS+="TIMESTAMPS=ON "
fi
if [[ $@ == *"TIMERS"* ]]; then
  echo "TIMERS ON"
  DAIET_ARGS+="TIMERS=ON "
fi
if [[ $@ == *"DEBUG"* ]]; then
  echo "DEBUG ON"
  DAIET_ARGS+="DEBUG=ON "
  DPDK_FLAGS+="-g -O0 "
fi
if [[ $@ == *"HOROVOD"* ]]; then
  echo "HOROVOD FLAGS SET"
  HOROVOD_ARGS+='-DUSE_MPI=1 -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=0"'
fi

# Build DPDK
cd ../lib/dpdk/
rm -rf build

if [[ $@ == *"MLX5"* ]]; then
  echo "MLX5 SUPPORT"
  sed -i 's/CONFIG_RTE_LIBRTE_MLX5_PMD=n/CONFIG_RTE_LIBRTE_MLX5_PMD=y/' config/common_base
fi

make defconfig T=x86_64-native-linuxapp-gcc
make EXTRA_CFLAGS="${DPDK_FLAGS}" -j

if [[ $@ == *"INSTALL"* ]]; then
make install
fi

cd ../..

# Build DAIET
make clean
rm -rf build
make ${DAIET_ARGS} -j

if [[ $@ == *"INSTALL"* ]]; then
make libinstall
fi

cd ..

# Build Gloo
rm -rf build
mkdir build
cd build
cmake -DUSE_REDIS=ON -DUSE_AVX=ON $HOROVOD_ARGS ..
make -j
if [[ $@ == *"INSTALL"* ]]; then
make install
fi

# Build experiments
cd ../experiments/exp1/
mkdir -p build
cd build
find . ! -name 'daiet.cfg'   ! -name '.'  ! -name '..' -exec rm -rf {} +

if [[ $@ == *"MLX5"* ]]; then
  cmake -DUSE_MLX5=ON ..
else
  cmake ..
fi

make -j

cd ../../exp2
mkdir -p build
cd build
find . ! -name 'daiet.cfg'   ! -name '.'  ! -name '..' -exec rm -rf {} +

if [[ $@ == *"MLX5"* ]]; then
  cmake -DUSE_MLX5=ON ..
else
  cmake ..
fi

make -j

# Build example
cd ../../../daiet/example
mkdir -p build
cd build
find . ! -name 'daiet.cfg'   ! -name '.'  ! -name '..' -exec rm -rf {} +

if [[ $@ == *"MLX5"* ]]; then
  cmake -DUSE_MLX5=ON ..
else
  cmake ..
fi

make -j

# Build dedicated PS
cd ../../ps
make clean
make -j
cd $CWD
