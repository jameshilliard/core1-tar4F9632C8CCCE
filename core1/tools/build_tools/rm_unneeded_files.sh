#!/bin/sh
# 000004:tc.chen 2005/06/17 add CLI 
TOPDIR=../../
. ./Path.sh
. ./config.sh
parse_args $@

#delete files
if [ -n "$BUILD_ROOTFS_DIR" ]; then
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

	# remove unneeded modules
	if [ "$IFX_CONFIG_DEBUG" != "1" ]; then
		rm -f ${BUILD_ROOTFS_DIR}root/modemhwe.bin
		#050711:fchang rm -f ${BUILD_ROOTFS_DIR}root/tcpmessages
		#050711:fchang rm -f ${BUILD_ROOTFS_DIR}root/translate
		rm -f ${BUILD_ROOTFS_DIR}root/run_voip.sh
		rm -f ${BUILD_ROOTFS_DIR}root/run_tcp.sh
		#050711:fchang rm -f ${BUILD_ROOTFS_DIR}root/amazon_debugread
		rm -f ${BUILD_ROOTFS_DIR}amazon_show_firmware_date
		rm -f ${BUILD_ROOTFS_DIR}amazon_autoboot_daemon-PCM
		rm -f ${BUILD_ROOTFS_DIR}bin/ldd
	fi

	if [ "$IFX_CONFIG_DDNS" != "1" ]; then
		rm -f ${BUILD_ROOTFS_DIR}usr/bin/ez-ipupdate
	fi

	if [ "$IFX_CONFIG_EBTABLES" != "1" ]; then
		rm -f ${BUILD_ROOTFS_DIR}usr/sbin/ebtables
		rm -f ${BUILD_ROOTFS_DIR}lib/libebtables.so
	fi

	rm -f ${BUILD_ROOTFS_DIR}etc/conf.tgz
	rm -f ${BUILD_ROOTFS_DIR}ramdisk.tar.gz

# 000004:tc.chen start
	if [ "$IFX_CONFIG_CLI" != "1" ];then
		rm -f ${BUILD_ROOTFS_DIR}etc/rc.d/init.d/cli
		rm -rf ${BUILD_ROOTFS_DIR}usr/cli
		rm -f ${BUILD_ROOTFS_DIR}usr/sbin/cli
	fi
# 000004:tc.chen end

	if [ "$IFX_CONFIG_VOIP" != "1" ];then
		rm -f ${BUILD_ROOTFS_DIR}usr/web/voip*
	fi

	rm -f ${BUILD_ROOTFS_DIR}usr/web/upnp*

	if [ "$IFX_CONFIG_CLIP" != "1" ]; then
		rm -f ${BUILD_ROOTFS_DIR}etc/rc.d/create_clip_if
		rm -f ${BUILD_ROOTFS_DIR}etc/rc.d/delete_clip_if
	fi

	if [ "$IFX_CONFIG_ALGS" != "1" ]; then 
		rm -f ${BUILD_ROOTFS_DIR}usr/web/napt_algs.asp
		rm -f ${BUILD_ROOTFS_DIR}etc/rc.d/init.d/algs*
		rm -f ${BUILD_ROOTFS_DIR}usr/sbin/nfappcfg
	fi

	if [ "$IFX_CONFIG_DHCP_SERVER" = "1" ]; then
		rm -f ${BUILD_ROOTFS_DIR}usr/web/lan_nodhcp.asp
	else
		rm -f ${BUILD_ROOTFS_DIR}usr/web/lan.asp
	fi

	if [ "$IFX_CONFIG_DDNS" != "1" ]; then
		rm -f ${BUILD_ROOTFS_DIR}usr/web/ddns_config.asp
		rm -f ${BUILD_ROOTFS_DIR}etc/rc.d/init.d/ddns*
	fi

	if [ "$IFX_CONFIG_INETD" = "1" ]; then
		rm -f ${BUILD_ROOTFS_DIR}etc/rc.d/init.d/ftpd*
		rm -f ${BUILD_ROOTFS_DIR}etc/rc.d/init.d/ssh*
		rm -f ${BUILD_ROOTFS_DIR}etc/rc.d/init.d/telnetd*
		rm -f ${BUILD_ROOTFS_DIR}etc/rc.d/init.d/tftpd*
	fi

	if [ "$IFX_CONFIG_NTPCLIENT" != "1" ]; then
		rm -f ${BUILD_ROOTFS_DIR}usr/web/system_time.asp
		rm -f ${BUILD_ROOTFS_DIR}etc/rc.d/init.d/ntp*
	fi

	if [ "$IFX_CONFIG_DNSRELAY" != "1" ]; then
		rm -f ${BUILD_ROOTFS_DIR}etc/init.d/dnrd*
		rm -f ${BUILD_ROOTFS_DIR}usr/sbin/dnrd
		rm -f ${BUILD_ROOTFS_DIR}ramdisk_copy/etc/dnrd*
	fi

	rm -f ${BUILD_ROOTFS_DIR}root/amazon_mib_daemon

	if [ ${IFX_CONFIG_WIRELESS} -ne 1 ]; then
		rm -f ${BUILD_ROOTFS_DIR}etc/rc.d/rc.bringup_wireless*
		rm -f ${BUILD_ROOTFS_DIR}usr/web/wireless*
	fi 

	if [ "$IFX_CONFIG_RIP" != "1" ]; then
		rm -f ${BUILD_ROOTFS_DIR}ramdisk_copy/etc/zebra.conf
		#Added to avoid reading and writing to the same file at the sametime.
		cat ${BUILD_ROOTFS_DIR}ramdisk_copy/etc/ripd.conf | grep -v "redistribute kernel" > ${BUILD_ROOTFS_DIR}ramdisk_copy/etc/ripd.conf.tmp
		cp -f ${BUILD_ROOTFS_DIR}ramdisk_copy/etc/ripd.conf.tmp ${BUILD_ROOTFS_DIR}ramdisk_copy/etc/ripd.conf
		rm -f  ${BUILD_ROOTFS_DIR}ramdisk_copy/etc/ripd.conf.tmp
		cat ${BUILD_ROOTFS_DIR}etc/init.d/ripd_start | grep -v "zebra" > ${BUILD_ROOTFS_DIR}etc/init.d/ripd_start.tmp
		cp -f ${BUILD_ROOTFS_DIR}etc/init.d/ripd_start.tmp ${BUILD_ROOTFS_DIR}etc/init.d/ripd_start
		rm -f ${BUILD_ROOTFS_DIR}etc/init.d/ripd_start.tmp 
		cat ${BUILD_ROOTFS_DIR}etc/init.d/ripd_stop | grep -v "zebra" > ${BUILD_ROOTFS_DIR}etc/init.d/ripd_stop.tmp
		cp -f ${BUILD_ROOTFS_DIR}etc/init.d/ripd_stop.tmp ${BUILD_ROOTFS_DIR}etc/init.d/ripd_stop
		rm -f ${BUILD_ROOTFS_DIR}etc/init.d/ripd_stop.tmp 
	fi

	rm -rf ${BUILD_ROOTFS_DIR}usr/web/temp_unneeded

	if [ "$IFX_CONFIG_NTPCLIENT" != "1" ]; then
		rm -r ${BUILD_ROOTFS_DIR}usr/sbin/ntpclient
	fi

	rm -f ${BUILD_ROOTFS_DIR}usr/sbin/flash_upgrade				#Nirav : upgrade utility does the required stuff

rm -f ${BUILD_ROOTFS_DIR}usr/lib/lib*a
rm -f ${BUILD_ROOTFS_DIR}lib/lib*a
find ${BUILD_ROOTFS_DIR} -type f -name "*~" | xargs rm -f
fi

