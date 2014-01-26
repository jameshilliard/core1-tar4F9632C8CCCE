#!/bin/sh
. ./Path.sh

if [ -n config.sh ]; then
	parse_args $@
	create_config
fi
. ./config.sh

#create_firmware_fs()
#{

#165001:henryhsu:20050818:Modify for clear old firmware image.
        if [ $BUILD_CLEAN -eq 1 ]; then
                if [ ! $BUILD_CONFIGURE -eq 1 ];then
                        rm -rf ${BUILD_DIR}firmware*
                        exit 0
                fi
        fi
#165001

	if [ -n "${IFX_ADSL_FIRMWARE}" -a -f ${USER_IFXSOURCE_DIR}adsl_firmware_loader/firmware/${IFX_ADSL_FIRMWARE} ]; then
		if [ -f ${BUILD_DIR}firmware.img ]; then
			rm -f ${BUILD_DIR}firmware.img
		fi
		if [ -d ${BUILD_DIR}firmware ]; then
			rm -f ${BUILD_DIR}firmware/*
		else
			mkdir ${BUILD_DIR}firmware
		fi
        	cp -af ${USER_IFXSOURCE_DIR}adsl_firmware_loader/firmware/${IFX_ADSL_FIRMWARE} ${BUILD_DIR}firmware/modemhwe.bin
		cd ${BUILD_DIR}firmware/
		ln -s modemhwe.bin ${IFX_ADSL_FIRMWARE}
		cd -
		cd ${BUILD_DIR}
		chown 0.0 firmware/ -R
		${TOPDIR}tools/build_tools/squashfs2.0/squashfs-tools/mksquashfs firmware/ tmp_firmware.img -be
		${TOPDIR}tools/build_tools/mkimage -A MIPS -O Linux -C gzip -T standalone -e 0x00 -a 0x00 -n "Amazon firmware" -d tmp_firmware.img firmware.img
		rm -f tmp_firmware.img
		chmod 644 firmware.img
		# 507141:linmars
		[ -n "${TFTP_DIR}" ] && cp -af firmware.img ${TFTP_DIR}
		cd -
	fi
#}
