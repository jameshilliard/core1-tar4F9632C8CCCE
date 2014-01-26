#!/bin/sh
# 000001:Nirav
##303002:JackLee 2005/06/16 Fix_webUI_firmware_upgrading_causes_webpage_error_issue

#/etc/rc.d/invoke_upgrade.sh image image_type expand saveenv reboot &

if [ "$5" = "reboot" ]; then
	/etc/rc.d/free_memory.sh
	killall br2684ctld
	killall syslogd
fi
mkdir -p /tmp/newroot/bin
mkdir -p /tmp/newroot/lib
mkdir -p /tmp/newroot/dev
mv $1 /tmp/newroot/image
cp /lib/libc.so.0 /tmp/newroot/lib
cp /lib/ld-uClibc.so.0 /tmp/newroot/lib
cp /lib/libIFXAPIs.so /tmp/newroot/lib
cp /lib/libdl.so.0 /tmp/newroot/lib
cp /usr/sbin/upgrade /tmp/newroot/bin
cp -a /dev/mtd /tmp/newroot/dev/
shift
##303002:JackLee
##chroot /tmp/newroot upgrade /image $@
chroot /tmp/newroot upgrade /image $@ &

