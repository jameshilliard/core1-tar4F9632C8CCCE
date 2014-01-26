#!/bin/sh
. ./Path.sh
. ./config.sh
parse_args $@

#if [ $BUILD_CLEAN -eq 1 ]; then
#	echo "remove old ramdisk filesystem"
#	rm -rf ${BUILD_RAMDISK_DIR}*
#fi
rm -rf ${BUILD_RAMDISK_DIR}*
install -d ${BUILD_RAMDISK_DIR}etc
install -d ${BUILD_RAMDISK_DIR}root
install -d ${BUILD_RAMDISK_DIR}usr/sbin
cp -aRf ${BUILD_ROOTFS_DIR}etc/rc.d ${BUILD_RAMDISK_DIR}etc
cp -aRf ${BUILD_ROOTFS_DIR}etc/init.d ${BUILD_RAMDISK_DIR}etc
cp -aRf ${BUILD_ROOTFS_DIR}etc/fstab ${BUILD_RAMDISK_DIR}etc/
cp -aRf ${BUILD_ROOTFS_DIR}lib ${BUILD_RAMDISK_DIR}
cp -aRf ${BUILD_ROOTFS_DIR}bin ${BUILD_RAMDISK_DIR}
cp -aRf ${BUILD_ROOTFS_DIR}sbin ${BUILD_RAMDISK_DIR}
cp -aRf ${BUILD_ROOTFS_DIR}dev ${BUILD_RAMDISK_DIR}
cp -aRf ${BUILD_ROOTFS_DIR}proc ${BUILD_RAMDISK_DIR}
cp -aRf ${BUILD_ROOTFS_DIR}root/test1.sh ${BUILD_RAMDISK_DIR}root/
cp -aRf ${BUILD_ROOTFS_DIR}usr/sbin/wlanconfig ${BUILD_RAMDISK_DIR}usr/sbin/
cp -aRf ${BUILD_ROOTFS_DIR}sbin ${BUILD_RAMDISK_DIR}
cp -aRf ${BUILD_ROOTFS_DIR}ramdisk_copy/dev/* ${BUILD_RAMDISK_DIR}/dev/
cp -aRf ${BUILD_ROOTFS_DIR}ramdisk_copy/etc/* ${BUILD_RAMDISK_DIR}/etc/
cp -aRf ${BUILD_ROOTFS_DIR}ramdisk_copy/mnt ${BUILD_RAMDISK_DIR}
cp -aRf ${BUILD_ROOTFS_DIR}ramdisk_copy/tmp ${BUILD_RAMDISK_DIR}
cp -aRf ${BUILD_ROOTFS_DIR}ramdisk_copy/var ${BUILD_RAMDISK_DIR}
rm -f ${BUILD_RAMDISK_DIR}lib/libifx_httpd.so
rm -f ${BUILD_RAMDISK_DIR}lib/libIFXAPIs.s

#generate inittab
echo "# System initialization." > ${BUILD_RAMDISK_DIR}/etc/inittab
echo "::sysinit:/etc/init.d/rcS" >> ${BUILD_RAMDISK_DIR}/etc/inittab
echo "# Run gettys in standard runlevels" >> ${BUILD_RAMDISK_DIR}/etc/inittab
echo "::askfirst:/bin/sh" >> ${BUILD_RAMDISK_DIR}/etc/inittab

#generate rcS
echo "#!/bin/sh" > ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "/bin/mount -a" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
if [ $BUILD_NO_WIRELESS -eq 1 ]; then
echo "insmod  /lib/modules/mps1_wifi.oo" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "insmod /lib/modules/wlan.o" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "insmod /lib/modules/ath_hal.o" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "insmod /lib/modules/ath_rate_atheros.o" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "insmod /lib/modules/ath_dfs.o" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "insmod /lib/modules/ath_pci.o" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "sleep 1" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "insmod /lib/modules/wlan_wep.o" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "insmod /lib/modules/wlan_tkip.o" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "insmod /lib/modules/wlan_ccmp.o" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "insmod /lib/modules/wlan_xauth.o" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "insmod /lib/modules/wlan_acl.o" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "insmod /lib/modules/wlan_scan_ap.o" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "sleep 1" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
fi
echo "echo +------------------------------------------+" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "echo | Linux on TWINPASS (Core1) by Infineon CPE|" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS
echo "echo +------------------------------------------+" >> ${BUILD_RAMDISK_DIR}/etc/init.d/rcS

# Strip as much info as possible
${IFX_STRIP} -R.note -R.comment ${BUILD_RAMDISK_DIR}bin/*
${IFX_STRIP} -R.note -R.comment ${BUILD_RAMDISK_DIR}sbin/*
#${IFX_STRIP} -R.note -R.comment ${BUILD_RAMDISK_DIR}usr/bin/*
#${IFX_STRIP} -R.note -R.comment ${BUILD_RAMDISK_DIR}usr/sbin/*
#${IFX_STRIP} -R.note -R.comment ${BUILD_RAMDISK_DIR}root/*
${IFX_STRIP} -x -R.note -R.comment ${BUILD_RAMDISK_DIR}lib/*
#${IFX_STRIP} -x -R.note -R.comment ${BUILD_RAMDISK_DIR}usr/lib/*
find ${BUILD_RAMDISK_DIR}lib/modules -type f -name "*.o" -exec ${IFX_STRIP} -x -R.note -R.comment {} \;
#find ${BUILD_RAMDISK_DIR}usr/lib/ -type f -name "*.so*" -exec ${IFX_STRIP} -x -R.note -R.comment {} \;
find ${BUILD_RAMDISK_DIR}lib/ -type f -name "*.so*" -exec chmod 555 {} \;
#find ${BUILD_RAMDISK_DIR}usr/lib/ -type f -name "*.so*" -exec chmod 555 {} \;

#create ramdisk.gz
cd ${BUILD_DIR}
chown 0.0 ${BUILD_RAMDISK_DIR} -R
mkdir ramdisk_tmp_mount
dd if=/dev/zero of=./ramdisk bs=1024 count=4096
/sbin/mkfs.ext2 -F ramdisk
mount -o loop -t ext2 ./ramdisk ./ramdisk_tmp_mount
cp -aRf ${BUILD_RAMDISK_DIR}/* ./ramdisk_tmp_mount
sudo umount ./ramdisk_tmp_mount
rm -fr ./ramdisk_tmp_mount
rm -f ramdisk.gz
gzip ramdisk
rm -f ${KERNEL_SOURCE_DIR}/arch/mips/ramdisk/ramdisk.*
cp -f ramdisk.gz ${KERNEL_SOURCE_DIR}/arch/mips/ramdisk


