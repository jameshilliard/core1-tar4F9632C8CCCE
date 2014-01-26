#!/bin/sh
TOPDIR=`pwd`/../../
. ./Path.sh
. ./module_select.sh

build_ifx_apis()
{
	if [ ! -d ${USER_SOURCE_DIR}ifx/IFXAPIs/src ] ; then
		echo "ERROR!! ${USER_SOURCE_DIR}ifx/IFXAPIs/src not found!!"
		exit 1
	fi
	cd ${USER_SOURCE_DIR}ifx/IFXAPIs/src
	./build.sh $@
	IFX_BUILD_ERRNO=$?
	if [ $IFX_BUILD_ERRNO -ne 0 ]
	then
		echo "Build IFX Command fail!"
		exit $IFX_BUILD_ERRNO
	else
		echo "Build IFX Command success!"
	fi
	cd -
}

build_opensource_apps()
{
	for i in ${OPENSOURCE_APPS_SUBDIRS}
	do
		if [ ! -d ${USER_OPENSOURCE_DIR}$i ] ; then
			echo "ERROR!! ${USER_OPENSOURCE_DIR}$i not found!!"
			exit 1
		fi
		cd ${USER_OPENSOURCE_DIR}$i
		./build.sh $@
		IFX_BUILD_ERRNO=$?
		if [ $IFX_BUILD_ERRNO -ne 0 ]
		then
			echo "Build $i fail!"
			exit $IFX_BUILD_ERRNO
		else
			echo "Build $i success!"
		fi
	cd -
	done
}

build_ifx_apps()
{
	for i in ${IFX_APPS_SUBDIRS}
	do
		if [ ! -d ${USER_IFXSOURCE_DIR}$i ] ; then
			echo "ERROR!! ${USER_IFXSOURCE_DIR}$i not found!!"
			exit 1
		fi
		cd ${USER_IFXSOURCE_DIR}$i
		
		# 605262:fchang.added.start Skip this becasue it requires the hal in source format
		if [ "$i" = "atheros_wireless" ]; then
			continue
		fi
		# 605262:fchang.added.end

		./build.sh $@
		IFX_BUILD_ERRNO=$?
		if [ $IFX_BUILD_ERRNO -ne 0 ]
		then
			echo "Build $i fail!"
			exit $IFX_BUILD_ERRNO
		else
			echo "Build $i success!"
		fi
		cd -
	done
}

remove_unused_files()
{
	#delete files
	if [ $BUILD_ROOTFS_DIR ]; then
		#rm -rf ${BUILD_ROOTFS_DIR}etc/ppp
		rm -f ${BUILD_ROOTFS_DIR}lib/*.a
		rm -f ${BUILD_ROOTFS_DIR}lib/*.la
		rm -f ${BUILD_ROOTFS_DIR}lib/libgcc*
		rm -f ${BUILD_ROOTFS_DIR}lib/libstdc*
		rm -rf ${BUILD_ROOTFS_DIR}usr/etc
		rm -rf ${BUILD_ROOTFS_DIR}usr/include
		rm -rf ${BUILD_ROOTFS_DIR}usr/info
		rm -rf ${BUILD_ROOTFS_DIR}usr/man
		rm -f ${BUILD_ROOTFS_DIR}usr/lib/*.a
		rm -f ${BUILD_ROOTFS_DIR}usr/lib/*.la
		rm -rf ${BUILD_ROOTFS_DIR}usr/share
	fi
}

copy_uclibc_to_rootfs()
{
	IFX_PKG_LIB="ld-uClibc*.so* libc*.so* libcrypt*.so* libdl*.so*  libm*.so* libnsl*.so* libpthread*.so* libresolv*.so* librt*.so* libuClibc*.so* libutil*.so* ld-uClibc*.so* libcrypt*.so* libdl*.so* libm*.so* libnsl*.so* libpthread*.so* libresolv*.so* librt*.so* libuClibc*.so* libutil*.so* "

	for file in ${IFX_PKG_LIB}
	do
		echo "File is ${file}"
		echo "${TOPDIR}/source/opt_uclibc_lib/lib/${file} DST = ${BUILD_ROOTFS_DIR}lib/."
		cp -avf ${TOPDIR}/source/opt_uclibc_lib/lib/${file} ${BUILD_ROOTFS_DIR}lib/.
	done

}

build_apps()
{
#	build_ifx_apis $@
	build_opensource_apps $@
#	build_ifx_apps $@
	copy_uclibc_to_rootfs
	mips-linux-strip ${BUILD_ROOTFS_DIR}bin/*
	mips-linux-strip ${BUILD_ROOTFS_DIR}sbin/*
	mips-linux-strip ${BUILD_ROOTFS_DIR}usr/bin/*
	mips-linux-strip ${BUILD_ROOTFS_DIR}usr/sbin/*
	remove_unused_files
}

case "$1" in
	ifx_api)
		shift
		build_ifx_apis $@
		;;
	ifx_apps)
		shift
		build_ifx_apps $@
		;;
	opensource_apps)
		shift
		build_opensource_apps $@
		;;
	config_only)
		create_config $@
		build_ifx_apis config_only
		build_opensource_apps config_only
		build_ifx_apps config_only
		;;
	*)
		parse_args $@
		create_config $@
		build_apps $@
esac
