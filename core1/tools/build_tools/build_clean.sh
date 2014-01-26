#!/bin/sh

TOPDIR=`pwd`/../../
. ./Path.sh
. ./module_select.sh

rm -rf ${BUILD_ROOTFS_DIR}/*

cd ${KERNEL_SOURCE_DIR}
make distclean
find ../../ -name ".*.flags" -exec rm -f {} \;
find ../../ -name ".depend" -exec rm -f {} \;
cd -

if [ -f ${USER_SOURCE_DIR}ifx/IFXAPIs/src/build.sh ] ; then
cd ${USER_SOURCE_DIR}ifx/IFXAPIs/src
	./build.sh -clean
cd -
fi

for i in ${OPENSOURCE_APPS_SUBDIRS} ; do
	if [ ! -d ${USER_OPENSOURCE_DIR}/$i ] ; then
		echo "NO  ${USER_OPENSOURCE_DIR}/$i !!!"
		exit 1
	fi
	cd ${USER_OPENSOURCE_DIR}/$i
	./build.sh -clean
	cd -
done

for i in ${IFX_APPS_SUBDIRS} ; do
	if [ ! -d ${USER_IFXSOURCE_DIR}/$i ] ; then
                echo "NO  ${USER_IFXSOURCE_DIR}/$i !!!"
                exit 1
        fi
	cd ${USER_IFXSOURCE_DIR}/$i
	./build.sh -clean
	cd -
done

find $TOPDIR/source -name autom4te.cache |xargs rm -rf
find $USER_OPENSOURCE_DIR -name "*.o" |xargs rm -f
find $USER_IFXSOURCE_DIR -name "*.o" |xargs rm -f
find $KERNEL_SOURCE_DIR -name "*.o" |xargs rm -f
find $KERNEL_IFX_DIR -name "*.o" |xargs rm -f

#./build_uboot.sh -clean
