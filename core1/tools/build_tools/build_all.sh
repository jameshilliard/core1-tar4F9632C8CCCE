#!/bin/sh

# 605122:fchang 2006/5/12 Build the kernel image before making the root file system

TOPDIR=`pwd`/../../
. ./Path.sh

create_rootfs_dir()
{
	mkdir -p ${BUILD_ROOTFS_DIR}
	mkdir -p ${BUILD_ROOTFS_DIR}lib
	mkdir -p ${BUILD_ROOTFS_DIR}bin
	mkdir -p ${BUILD_ROOTFS_DIR}sbin
	mkdir -p ${BUILD_ROOTFS_DIR}usr
	mkdir -p ${BUILD_ROOTFS_DIR}usr/bin
	mkdir -p ${BUILD_ROOTFS_DIR}usr/sbin
}

build_user_apps()
{
	#build user space applications
	./build_apps.sh $@
	IFX_BUILD_ERRNO=$?
	if [ $IFX_BUILD_ERRNO -ne 0 ]
	then
		echo "ERROR: Building Applications Fail!"
		exit $IFX_BUILD_ERRNO
	fi
}

build_kernel()
{
	#build user space applications
	./build_kernel.sh $@
	IFX_BUILD_ERRNO=$?
	if [ $IFX_BUILD_ERRNO -ne 0 ]
	then
		echo "ERROR: Building Kernel Fail!"
		exit $IFX_BUILD_ERRNO
	fi
}

build_amazon_image()
{
	#create rootfs
	./build_image.sh $@
	IFX_BUILD_ERRNO=$?
	if [ $IFX_BUILD_ERRNO -ne 0 ]
	then
		echo "ERROR: Build image Fail!"
		exit $IFX_BUILD_ERRNO
	fi
}

build_all()
{
	parse_args $@
	if [ $BUILD_CLEAN -eq 1 ]; then
		./build_clean.sh
		[ ! $BUILD_CONFIGURE -eq 1 ] && exit 0
	fi
	
	./build_config.sh -r

	if [ ${BUILD_DIR} ]; then
		rm -rf ${BUILD_ROOTFS_DIR}
	fi

	create_config
	mkdir ${BUILD_ROOTFS_DIR}
	cp -aRf ${TOPDIR}source/rootfs/flashdisk/* ${BUILD_ROOTFS_DIR}
	# 605122:fchang.removed
	# build_user_apps $@

	build_kernel $@

	# 605122:fchang.added
	build_user_apps $@	

	build_amazon_image $@
#	./build_firmware.sh $@
#	./build_uboot.sh $@
}

build_all $@
