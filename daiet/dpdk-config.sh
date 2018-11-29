#!/bin/bash

declare -a intfs=("eno1")

cwd=$(pwd)

export RTE_SDK=$cwd/lib/dpdk
#export RTE_TARGET=x86_64-native-linuxapp-gcc
export RTE_TARGET=build

cd $RTE_SDK/$RTE_TARGET

modprobe uio
insmod kmod/igb_uio.ko

cd ../usertools

for i in "${intfs[@]}"
do
    ./dpdk-devbind.py --bind=igb_uio $i
done

cd $cwd
