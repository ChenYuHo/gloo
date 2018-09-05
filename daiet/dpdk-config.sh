#!/bin/bash

declare -a intfs=("enp1s0f0")

cwd=$(pwd)

export RTE_SDK=$cwd/dpdk
#export RTE_TARGET=x86_64-native-linuxapp-gcc
export RTE_TARGET=build

cd $RTE_SDK/$RTE_TARGET

sudo modprobe uio
sudo insmod kmod/igb_uio.ko

cd ../usertools

for i in "${intfs[@]}"
do
    sudo ./dpdk-devbind.py --bind=igb_uio $i
done

cd $cwd
