#!/bin/sh
TOPDIR=`pwd`/../../
. ./Path.sh

IFX_CFLAGS="${IFX_CFLAGS} -O2"

build_kernel_config()
{
	cd ${KERNEL_SOURCE_DIR}
	parse_args $@
	
	if [ $BUILD_CLEAN -eq 1 ]; then
		make distclean
		rm -f ${BUILD_DIR}uImage
		[ ! $BUILD_CONFIGURE -eq 1 ] && exit 0
	fi	
	
	if [ $BUILD_CONFIGURE -eq 1 ]; then
		cp -f ifx_kernel_config_danube .config
		make oldconfig
		make dep
	fi

	IFX_BUILD_ERRNO=$?
	if [ $IFX_BUILD_ERRNO -ne 0 ]; then
		echo "ERROR: make kernel dep Fail!"
		exit $IFX_BUILD_ERRNO
	fi

	cd -
}

build_kernel_modules()
{
	#build kernel modules
	cd ${KERNEL_SOURCE_DIR}
	make modules
	IFX_BUILD_ERRNO=$?
	if [ $IFX_BUILD_ERRNO -ne 0 ]
	then
		echo "ERROR: Building Kernel module Fail!"
		exit $IFX_BUILD_ERRNO
	fi

	make modules_install INSTALL_MOD_PATH=${BUILD_ROOTFS_DIR}
	IFX_BUILD_ERRNO=$?
	if [ $IFX_BUILD_ERRNO -ne 0 ]
	then
		echo "ERROR: Installing Kernel module Fail!"
		exit $IFX_BUILD_ERRNO
	fi

	cd -

	#create modules.dep file
	find ${BUILD_ROOTFS_DIR}lib/modules -name *.o |xargs mips-linux-strip --strip-unneeded 
	rm -rf ${BUILD_ROOTFS_DIR}lib/modules/2.4.31-`cat $KERNEL_SOURCE_DIR/.hhl_target_lspname`/pcmcia
	rm -f ${BUILD_ROOTFS_DIR}lib/modules/2.4.31-`cat $KERNEL_SOURCE_DIR/.hhl_target_lspname`/modules.dep
	${BUILD_TOOLS_DIR}depmod.pl -b ${BUILD_ROOTFS_DIR}lib/modules/2.4.31-`cat $KERNEL_SOURCE_DIR/.hhl_target_lspname` -k ${KERNEL_SOURCE_DIR}/vmlinux -F ${KERNEL_SOURCE_DIR}/System.map
}

compile_kernel()
{
	cd ${KERNEL_SOURCE_DIR}
	make IFX_SMALL_FOOTPRINT=1
	IFX_BUILD_ERRNO=$?
	if [ $IFX_BUILD_ERRNO -ne 0 ]
	then
		echo "Build kernel fail!"
		exit $IFX_BUILD_ERRNO
	else
		echo "Build kernel success!"
	fi
	. ${BUILD_TOOLS_DIR}/config.sh
	if [ $IFX_CONFIG_MALTA ]; then
		make IFX_SMALL_FOOTPRINT=${IFX_SMALL_FOOTPRINT} boot
	else
		make IFX_SMALL_FOOTPRINT=1 uImage
		
		#605031:fchang.removed
		#cp -af arch/mips/infineon/amazon/basic/uImage ${BUILD_DIR}
		#605031:fchang.added
		cp -af arch/mips/infineon/danube/basic/uImage ${BUILD_DIR}
	fi
	IFX_BUILD_ERRNO=$?
	if [ $IFX_BUILD_ERRNO -ne 0 ]
	then
		echo "Build kernel fail!"
		exit $IFX_BUILD_ERRNO
	else
		echo "Build kernel success!"
	fi
	
	#605031:fchang.removed
	#[ -n "${TFTP_DIR}" ] && cp -af arch/mips/infineon/amazon/basic/uImage ${TFTP_DIR}
	#605031:fchang.added
	[ -n "${TFTP_DIR}" ] && cp -af arch/mips/infineon/danube/basic/uImage ${TFTP_DIR}

	cd -
}

build_kernel_and_modules()
{
	echo "Args are $@"
	parse_args $@
	build_kernel_config $@
	compile_kernel $@
	build_kernel_modules $@
}

case "$1" in
	-kernel)
		shift
		parse_args $@
		compile_kernel $@
		;;
	-modules)
		shift
		parse_args $@
		build_kernel_modules $@
		;;
	config_only)
		shift
		parse_args $@
		build_kernel_config $@
		;;
	*)
		build_kernel_and_modules $@
esac
