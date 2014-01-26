#!/bin/sh
if [ -r /etc/rc.d/model_config.sh ]; then
	. /etc/rc.d/model_config.sh
	CPU_EXTRA_INFO=$IFX_CONFIG_CPU"-v"
	BOARD_EXTRA_INFO="`echo $IFX_CONFIG_CPU|cut -b8`"
	MODEL_EXTRA_INFO="-"${BOARD_EXTRA_INFO}${IFX_CONFIG_FLASH_SIZE}${IFX_CONFIG_MEMORY_SIZE}"-"${IFX_CONFIG_FUTURE_SET}
fi

if [ ! -s /tmp/version ] ; then
	echo BOOTLoader:`cat /dev/mtd/0|grep "U-Boot "|cut -f2 -d ' '`>/tmp/version
	echo CPU:$CPU_EXTRA_INFO`cat /proc/cpuinfo|grep CHIP_VERSION|cut -f2 -d ' '`>>/tmp/version
	echo BSP:`cat /proc/lspinfo/build_id`>>/tmp/version
#509121:tc.chen	echo Kernel:`uname -r`$MODEL_EXTRA_INFO>>/tmp/version
	echo Kernel:`uname -r` >>/tmp/version
	echo Software:`cat /etc/version`$MODEL_EXTRA_INFO>>/tmp/version
	echo Tool Chain:`/usr/sbin/upgrade|grep ToolChain|cut -d: -f2`>>/tmp/version
fi

# Winder, 20061113, removed.
#FIRMWARE_STR=`grep Firmware /tmp/version`

#if [ "$FIRMWARE_STR" = "" ] ; then
#	FIRMWARE="`cat /proc/mei/version`"
#	if [ "$FIRMWARE" != "" ] ; then
#		echo Firmware:$FIRMWARE>>/tmp/version
#	else
#	    FIRMWARE="N/A"
#	fi
#else
#	FIRMWARE="`echo $FIRMWARE_STR|cut -d: -f2-`"
#fi

if [ $# -ge 1 ]; then
	while [ $# -ge 1 ]; do
		case $1 in
		-b) VER=BOOTLoader;;
		-c) VER=CPU;;
		-f) echo `echo $FIRMWARE|cut -d" " -f1`
			exit;;
		-k) VER=Kernel;;
		-l) VER=BSP;;
		-r) VER=Software;;
		-t) VER=Tool;;
		*) echo "'$1' Error!    Usage : version [-b|-c|-f|-k|-l|-r|-t]"
			exit
		esac
		shift
		echo `grep $VER /tmp/version|cut -d: -f2|cut -d" " -f1`
  	done
else
	cat /tmp/version
	if [ ! -z "$FIRMWARE_STR" ] ; then
		echo Firmware:$FIRMWARE
	fi
fi
