#!/bin/bash
TOPDIR=`pwd`/../../
. ./Path.sh
. ./common_funcs.sh

#configure and build apps
./build_config.sh -d 1
./build_all.sh -clean -configure
ifx_error_check $?

#configure for core 1 kernel
./build_config.sh -d 1
./build_kernel.sh config_only -clean -configure

#create ramdisk for CORE-1
./.create_ramdisk.sh -clean

#build kernel for core1
./build_kernel.sh -kernel

#backup uImage for CORE-1
cp -af ${BUILD_DIR}/uImage ${BUILD_DIR}/uImage-cpu1-vz-64
