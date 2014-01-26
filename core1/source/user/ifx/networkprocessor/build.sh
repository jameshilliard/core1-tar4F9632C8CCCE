#!/bin/sh
TOPDIR=`pwd`/../../../../
. ${TOPDIR}tools/build_tools/Path.sh
APPS_NAME="tcpmessages"

echo "----------------------------------------------------------------------"
echo "------------------------ build tcpmessages ---------------------------"
echo "----------------------------------------------------------------------"

CONFIG_FULL_PACKAGE=y
if [ "$CONFIG_FULL_PACKAGE" == "y" ]; then

parse_args $@

if [ "$1" = "config_only" ] ;then
	exit 0
fi

if [ $BUILD_CLEAN -eq 1 ]; then
	make clean
	[ ! $BUILD_CONFIGURE -eq 1 ] && exit 0
fi

make AR=${IFX_AR} AS=${IFX_AS} LD=${IFX_LD} NM=${IFX_NM} CC=${IFX_CC} BUILDCC=${IFX_HOSTCC} GCC=${IFX_CC} CXX=${IFX_CXX} CPP=${IFX_CPP} RANLIB=${IFX_RANLIB} IFX_CFLAGS="${IFX_CFLAGS}" IFX_LDFLAGS="${IFX_LDFLAGS}" all
ifx_error_check $? 

install -d ${BUILD_ROOTFS_DIR}root/
cp -af tcpmessages ${BUILD_ROOTFS_DIR}root/.
ifx_error_check $? 

#cp -af tcpmessages ${IFX_BINARY_DIR}root/.

else

  install -d ${BUILD_ROOTFS_DIR}root/
  cp -af tcpmessages ${BUILD_ROOTFS_DIR}root/.

fi
