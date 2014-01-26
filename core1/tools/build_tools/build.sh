#!/bin/sh

check_error()
{
	error_no=$? #050706:fchang
	#050706:fchang if [ $? -ne 0 ] ; then
	if [ $error_no -ne 0 ] ; then #050706:fchang
		echo "Error and Stop"
		#050706:fchang exit $?
		exit $error_no #050706:fchang
	fi
}

TOPDIR=`pwd`/../../
. ./Path.sh

# configurate system model at first
./build_config.sh -r

create_config $@

#  configurate all packages
if [ ! -f ${KERNEL_SOURCE_DIR}.depend ] ; then
	./build_kernel.sh config_only
	check_error
fi

./build_apps.sh config_only
check_error

# make all packages
./build_kernel.sh
check_error
./build_apps.sh
check_error
./build_firmware.sh
check_error
./build_image.sh
check_error
./build_uboot.sh
check_error
