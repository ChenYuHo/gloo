#!/bin/bash

# FLAGS: INSTALL MLX5 COLOCATED LATENCIES TIMESTAMPS TIMERS DEBUG HOROVOD
set -e
set -x

CWD=`pwd`
DPDK_ARGS='-fPIC '
DAIET_ARGS=''
EXP_ARGS=''
PS_ARGS=''
HOROVOD_ARGS=''

if [[ $@ == *'MLX5'* ]]; then
  echo 'MLX5 SUPPORT'
  EXP_ARGS+='-DUSE_MLX5=1 '
fi
if [[ $@ == *'COLOCATED'* ]]; then
  echo 'COLOCATED SET'
  DAIET_ARGS+='COLOCATED=ON '
fi
if [[ $@ == *'LATENCIES'* ]]; then
  echo 'LATENCIES SET'
  DAIET_ARGS+='LATENCIES=ON '
fi
if [[ $@ == *'TIMESTAMPS'* ]]; then
  echo 'TIMESTAMPS SET'
  DAIET_ARGS+='TIMESTAMPS=ON '
fi
if [[ $@ == *'TIMERS'* ]]; then
  echo 'TIMERS SET'
  DAIET_ARGS+='TIMERS=ON '
fi
if [[ $@ == *'DEBUG'* ]]; then
  echo 'DEBUG SET'
  DAIET_ARGS+='DEBUG=ON '
  DPDK_ARGS+='-g -O0 '
  PS_ARGS+='DEBUG=ON '
  EXP_ARGS+='-DDEBUG=1'
fi
if [[ $@ == *'HOROVOD'* ]]; then
  echo 'HOROVOD SET'
  HOROVOD_ARGS+='-DUSE_MPI=1 -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=0"'
  EXP_ARGS+='-DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=0"'
fi

# Build DPDK
cd ../lib/dpdk/
rm -rf build

if [[ $@ == *'MLX5'* ]]; then
  sed -i 's/CONFIG_RTE_LIBRTE_MLX5_PMD=n/CONFIG_RTE_LIBRTE_MLX5_PMD=y/' config/common_base
else
  sed -i 's/CONFIG_RTE_LIBRTE_MLX5_PMD=y/CONFIG_RTE_LIBRTE_MLX5_PMD=n/' config/common_base
fi

make defconfig T=x86_64-native-linuxapp-gcc
make EXTRA_CFLAGS='${DPDK_ARGS}' -j

if [[ $@ == *'INSTALL'* ]]; then
  make install
fi

cd ../..

# Build DAIET
make clean
rm -rf build
make ${DAIET_ARGS} -j

if [[ $@ == *'INSTALL'* ]]; then
  make libinstall
fi

cd ..

# Build Gloo
rm -rf build
mkdir build
cd build

if [[ $@ == *'DEBUG'* ]]; then
  CXXFLAGS='-g -O0' cmake -DUSE_REDIS=1 -DUSE_AVX=1 $HOROVOD_ARGS ..
else
  cmake -DUSE_REDIS=1 -DUSE_AVX=1 $HOROVOD_ARGS ..
fi

make -j

if [[ $@ == *'INSTALL'* ]]; then
  make install
fi

# Build experiments
cd ../experiments/exp1/
mkdir -p build
cd build
find . ! -name 'daiet.cfg'   ! -name '.'  ! -name '..' -exec rm -rf {} +

cmake ${EXP_ARGS} ..

make -j

cd ../../exp2
mkdir -p build
cd build
find . ! -name 'daiet.cfg'   ! -name '.'  ! -name '..' -exec rm -rf {} +

cmake ${EXP_ARGS} ..

make -j

# Build example
cd ../../../daiet/example
mkdir -p build
cd build
find . ! -name 'daiet.cfg'   ! -name '.'  ! -name '..' -exec rm -rf {} +

cmake ${EXP_ARGS} ..

make -j

# Build dedicated PS
cd ../../ps
make clean
make ${PS_ARGS} -j
cd $CWD
