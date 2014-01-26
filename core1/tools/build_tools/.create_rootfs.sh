#!/bin/sh
. ./Path.sh
. ./config.sh
parse_args $@

if [ $BUILD_CLEAN -eq 1 ]; then
	if [ ! $BUILD_CONFIGURE -eq 1 ];then 
		rm -rf ${BUILD_ROOTFS_DIR}*
		rm -rf ${BUILD_ROOTFS_DIR}../rootfs.img 
		exit 0
	fi
fi

rm -f ${BUILD_ROOTFS_DIR}rootfs.img
rm -f ${BUILD_ROOTFS_DIR}etc/conf.tar.gz
cp -aRf ${TOPDIR}source/rootfs/flashdisk/* ${BUILD_ROOTFS_DIR}
cd ${BUILD_ROOTFS_DIR}/ramdisk_copy/
rm -rf dev/ ../dev/*
tar jxvf dev.tar.bz2
rm -rf dev.tar.bz2
cp -af dev/console ../dev/.
cd -
cp -af ${TOPDIR}source/rootfs/sbin/modprobe.sh ${BUILD_ROOTFS_DIR}sbin
cp -af ${BUILD_TOOLS_DIR}config.sh ${BUILD_ROOTFS_DIR}etc/rc.d/.
cp -af ${BUILD_TOOLS_DIR}model_config.sh ${BUILD_ROOTFS_DIR}etc/rc.d/.
. ${BUILD_TOOLS_DIR}config.sh
if [ $IFX_CONFIG_MALTA ]; then
	echo "# System initialization." > ${BUILD_ROOTFS_DIR}/etc/inittab
	echo "::sysinit:/etc/init.d/rcS" >> ${BUILD_ROOTFS_DIR}/etc/inittab
	echo "# Run gettys in standard runlevels" >> ${BUILD_ROOTFS_DIR}/etc/inittab
	echo "::respawn:/sbin/getty 38400 console" >> ${BUILD_ROOTFS_DIR}/etc/inittab
fi

# create system setting
cd ${TOPDIR}source/rootfs
	gzip < flash/rc.conf > rc.conf.gz
	mv rc.conf.gz ${BUILD_ROOTFS_DIR}etc
cd - 

# Strip as much info as possible
${IFX_STRIP} -R.note -R.comment ${BUILD_ROOTFS_DIR}bin/*
${IFX_STRIP} -R.note -R.comment ${BUILD_ROOTFS_DIR}sbin/*
${IFX_STRIP} -R.note -R.comment ${BUILD_ROOTFS_DIR}usr/bin/*
${IFX_STRIP} -R.note -R.comment ${BUILD_ROOTFS_DIR}usr/sbin/*
${IFX_STRIP} -R.note -R.comment ${BUILD_ROOTFS_DIR}root/*
${IFX_STRIP} -x -R.note -R.comment ${BUILD_ROOTFS_DIR}lib/*
${IFX_STRIP} -x -R.note -R.comment ${BUILD_ROOTFS_DIR}usr/lib/*
find ${BUILD_ROOTFS_DIR}lib/modules -type f -name "*.o" -exec ${IFX_STRIP} -x -R.note -R.comment {} \;
find ${BUILD_ROOTFS_DIR}usr/lib/ -type f -name "*.so*" -exec ${IFX_STRIP} -x -R.note -R.comment {} \;
find ${BUILD_ROOTFS_DIR}lib/ -type f -name "*.so*" -exec chmod 555 {} \;
find ${BUILD_ROOTFS_DIR}usr/lib/ -type f -name "*.so*" -exec chmod 555 {} \;

# Create symbolic link
cd ${BUILD_ROOTFS_DIR}
ln -sf ramdisk/var var
ln -sf ramdisk/tmp tmp
ln -sf ramdisk/flash flash
ln -sf ../proc/mounts etc/mtab
ln -sf /tmp/log ramdisk_copy/dev/log
ln -sf ../flash/passwd etc/passwd
ln -sf ../ramdisk/etc/hosts etc/hosts
ln -sf ../ramdisk/etc/resolv.conf etc/resolv.conf
ln -sf ../ramdisk/flash/rc.conf etc/rc.conf
ln -sf ../ramdisk/etc/hostapd.conf etc/hostapd.conf

if [ "$IFX_CONFIG_SNMPv1" = "1" -o "$IFX_CONFIG_SNMPv3" = "1" ]; then
	ln -sf ../ramdisk/etc/snmp etc/snmp
fi

if [ "$IFX_CONFIG_DHCP_SERVER" = "1" ]; then
	ln -sf ../ramdisk/etc/udhcpd.conf etc/udhcpd.conf
fi

if [ "$IFX_CONFIG_DNSRELAY" = "1" ]; then
	ln -sf ../ramdisk/etc/dnrd/ etc/dnrd
fi

if [ "$IFX_CONFIG_RIP" = "1" ]; then
	ln -sf ../ramdisk/etc/ripd.conf etc/ripd.conf
	ln -sf ../ramdisk/etc/zebra.conf etc/zebra.conf
fi

if [ "$IFX_CONFIG_INETD" = "1" ]; then
	ln -sf ../ramdisk/etc/inetd.conf etc/inetd.conf
fi

ln -sf ../ramdisk/etc/ilmid etc/ilmid

if [ "$IFX_CONFIG_PPPOE" = "1" -o "$IFX_CONFIG_PPPOA" = "1" ]; then
	ln -sf ../../ramdisk/etc/ppp/resolv.conf etc/ppp/resolv.conf
	ln -sf ../../ramdisk/etc/ppp/peers etc/ppp/peers
fi

cd -

# Remove unneeded files and shell script comments
./rm_unneeded_files.sh $@
./rm_comment.sh $@

# create rootfs.img
cd ${BUILD_DIR}
find . -type d | xargs chmod 755
chown 0.0 root_filesystem/ -R
${TOPDIR}tools/build_tools/squashfs2.0/squashfs-tools/mksquashfs root_filesystem/ tmp_rootfs.img -be -noappend
${TOPDIR}tools/build_tools/mkimage -A MIPS -O Linux -C gzip -T standalone -e 0x00 -a 0x00 -n "Amazon rootfs" -d tmp_rootfs.img rootfs.img
rm -f tmp_rootfs.img
chmod 644 rootfs.img

# 507141:linmars
[ -n "${TFTP_DIR}" ] && cp -af rootfs.img ${TFTP_DIR}
cd -
#}
