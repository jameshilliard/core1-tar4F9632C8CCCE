#!/bin/sh

## File Name: site_prep.sh 
## Date     : 2005/05/13
##

## 507141:linmars: fix root user build problem
## 603091:fchang 2006/3/9 Modify the tool chain path for LXDB 1.1
## 603092:fchang 2006/3/9 Correct the tool-chain path for LXDB 1.1, the TOOLCHAIN_BIN_DIR will be specified in toolchain-env.sh instead

##
## CONFIGURATIONS
##
SP_DEBUG_MODE=NO

SP_CFG_FILENAME=.site_config
SP_CFGTMP_FILENAME=.site_config.tmp

SP_USERNAME=`whoami`
SP_PLATFORM_NAME=`cat .ifx_platform_name`
SP_PACKAGE_VERSION=`cat .ifx_package_version`

SP_DEFAULT_CWD=/opt/infineon/amazon/tools/build_tools/
# 603091:fchang.removed SP_DEFAULT_TOOLCHAIN_DIR=/opt/uclibc-toolchain/gcc-3.3.6/toolchain-mips/bin/
# 603091:fchang.added
SP_DEFAULT_TOOLCHAIN_DIR=/usr/local/openrg/mips-linux-uclibc-eb/bin

SP_DEFAULT_CROSS_COMPILER_PREFIX=mips-linux-
SP_DEFAULT_BUILD_DIR="`getent passwd | grep $SP_USERNAME`"
SP_DEFAULT_TARGET=mips-linux
SP_DEFAULT_HOST=i386-redhat-linux
SP_DEFAULT_BUILD=i386-pc-linux-gnu

##
## SUB-ROUTINE
##

# print message without new-line
if [ x"`echo -n`" = "x-n" ] ; then
	prompt_n_nl() {
		echo "$*\c" > /dev/tty
	}
else
	prompt_n_nl() {
		echo -n "$*" > /dev/tty
	}
fi

cd_no_echo() {
	cd "$*" 1>/dev/null
}

prompt_nl() {
	echo "$*"
}

prompt_string() {
	message="$1"
	default="$2"
	ok="no"
	while [ $ok != "yes" ] ; do
		if [ "$default" != "" ] ; then
			prompt_n_nl "$message [$default] "
			read < /dev/tty answer
			if [ "$answer" = "" ] ; then
				answer="$default"
				ok="yes"
				prompt_nl $answer
				continue
			fi
		else
			prompt_n_nl "$message "
			read < /dev/tty answer
			if [ "$answer" = "" ] ; then
				ok="no"
				continue
			fi
		fi
		prompt_nl $answer
		ok="yes"
	done
}

absolute_path() {
	D=`dirname "$1"`
	N=`basename "$1"`
	if [ "$N" != ".." ] ; then
		cd_no_echo "$D"
		if [ "$N" != "." ]; then
			prompt_nl "`pwd`/$N/"
		else
			prompt_nl "`pwd`/"
		fi
		cd_no_echo -
	else
		cd_no_echo "$D"
		absolute_path "$D"
		cd_no_echo -
	fi
}

sp_error_check() {
	if [ $1 != 0 ] ; then
	        echo "Site prepare fail, errno: $1"
		exit $1
	fi
}

# save parameters
sp_save_parameter() {
	name=$1
	value=$2
	echo "$1=$2" >> $SP_CFGTMP_FILENAME
}

# check CWD
sp_check_dir() {
	CURR_DIR=`pwd`
	if [ "`basename $CURR_DIR`" != "build_tools" ] ; then
		prompt_nl "$SP_USERNAME: Your working directory is not an site preparsion directory"
		prompt_nl "    (e.q. $SP_DEFAULT_CWD)"
		exit 1
	fi
}

## 507141:linmars start
sp_check_default_build_dir()
{
	for i in $SP_DEFAULT_BUILD_DIR
	do
		if [ "`echo $i|cut -d: -f1`" == "$SP_USERNAME" ]; then
			SP_DEFAULT_BUILD_DIR="`echo $i|cut -d: -f6`/build"
			break
		fi
	done
}
## 507141:linmars end

# basic configuration
sp_basic_config() {
	sp_save_parameter "IFX_PLATFORM_NAME" "$SP_PLATFORM_NAME"
	sp_save_parameter "IFX_PACKAGE_VERSION" "$SP_PACKAGE_VERSION"
	TOPDIR=`absolute_path "../../."`
	sp_save_parameter "TOPDIR" "$TOPDIR"
	TOOLCHAIN_DIR=`prompt_string "Please enter toolchain base directory: " "$SP_DEFAULT_TOOLCHAIN_DIR"`
	TOOLCHAIN_DIR=`absolute_path $TOOLCHAIN_DIR`
	sp_save_parameter "TOOLCHAIN_DIR" "$TOOLCHAIN_DIR"
	CROSS_COMPILER_PREFIX=`prompt_string "Please enter toolchain compiler prefix: " "$SP_DEFAULT_CROSS_COMPILER_PREFIX"`
	sp_save_parameter "CROSS_COMPILER_PREFIX" "$CROSS_COMPILER_PREFIX"
	# 507141:linmars
	sp_check_default_build_dir
	BUILD_DIR=`prompt_string "Please enter temporary directory for building system: " "$SP_DEFAULT_BUILD_DIR"`
	[ ! -e $BUILD_DIR ] && install -d $BUILD_DIR
	BUILD_DIR=`absolute_path "$BUILD_DIR"`
	sp_save_parameter "BUILD_DIR" "$BUILD_DIR"
	BUILD_ROOTFS_DIR=${BUILD_DIR}root_filesystem/
	if [ ! -e $BUILD_ROOTFS_DIR ]; then
		install -d $BUILD_ROOTFS_DIR
	fi
	sp_save_parameter "BUILD_ROOTFS_DIR" "$BUILD_ROOTFS_DIR"
	#120506: pliu for core1
	BUILD_RAMDISK_DIR=${BUILD_DIR}ramdisk_filesystem/
	if [ ! -e $BUILD_RAMDISK_DIR ]; then
		install -d $BUILD_RAMDISK_DIR
	fi
	sp_save_parameter "BUILD_RAMDISK_DIR" "$BUILD_RAMDISK_DIR"	
	# 605042:fchang.removed
	#UBOOT_SOURCE_DIR=${TOPDIR}source/u-boot/
	# 605042:fchang.added
	UBOOT_SOURCE_DIR=${TOPDIR}source/twinpass-uboot/
	
	sp_save_parameter "UBOOT_SOURCE_DIR" "$UBOOT_SOURCE_DIR"
	USER_SOURCE_DIR=${TOPDIR}source/user/
	sp_save_parameter "USER_SOURCE_DIR" "$USER_SOURCE_DIR"
	USER_IFXSOURCE_DIR=${USER_SOURCE_DIR}ifx/
	sp_save_parameter "USER_IFXSOURCE_DIR" "$USER_IFXSOURCE_DIR"
	USER_OPENSOURCE_DIR=${USER_SOURCE_DIR}opensource/
	sp_save_parameter "USER_OPENSOURCE_DIR" "$USER_OPENSOURCE_DIR"
	IFX_APIS_DIR=${USER_IFXSOURCE_DIR}IFXAPIs/
	sp_save_parameter "IFX_APIS_DIR" "$IFX_APIS_DIR"
	KERNEL_SOURCE_DIR=${TOPDIR}source/kernel/opensource/linux-2.4.31/
	sp_save_parameter "KERNEL_SOURCE_DIR" "$KERNEL_SOURCE_DIR"
	KERNEL_IFX_DIR=${TOPDIR}source/kernel/ifx/
	sp_save_parameter "KERNEL_IFX_DIR" "$KERNEL_IFX_DIR"
	BUILD_TOOLS_DIR=${TOPDIR}tools/build_tools/
	sp_save_parameter "BUILD_TOOLS_DIR" "$BUILD_TOOLS_DIR"
	IFX_ROOTFS_DIR=${TOPDIR}source/rootfs/
	sp_save_parameter "IFX_ROOTFS_DIR" "$IFX_ROOTFS_DIR"
	TOOLCHAIN_BIN_DIR=`absolute_path "${TOOLCHAIN_DIR}../bin"`
	
	# 603092:fchang.removed	
	# 603092:fchang.removed PATH=$TOOLCHAIN_BIN_DIR:$BUILD_TOOLS_DIR:$PATH
	# 603092:fchang.removed sp_save_parameter "PATH" "$PATH"
	
	# 603092:fchang.added.start
	PATH=$BUILD_TOOLS_DIR:$PATH
        sp_save_parameter "PATH" "$PATH"
	# 603092:fchang.added.end	

	IFX_CC=${CROSS_COMPILER_PREFIX}gcc
	sp_save_parameter "IFX_CC" "$IFX_CC"
	IFX_AR=${CROSS_COMPILER_PREFIX}ar
	sp_save_parameter "IFX_AR" "$IFX_AR"
	IFX_AS=${CROSS_COMPILER_PREFIX}as
	sp_save_parameter "IFX_AS" "$IFX_AS"
	IFX_LD=${CROSS_COMPILER_PREFIX}ld
	sp_save_parameter "IFX_LD" "$IFX_LD"
	IFX_NM=${CROSS_COMPILER_PREFIX}nm
	sp_save_parameter "IFX_NM" "$IFX_NM"
	IFX_STRIP=${CROSS_COMPILER_PREFIX}strip
	sp_save_parameter "IFX_STRIP" "$IFX_STRIP"
	IFX_RANLIB=${CROSS_COMPILER_PREFIX}ranlib
	sp_save_parameter "IFX_RANLIB" "$IFX_RANLIB"
	IFX_CXX=${CROSS_COMPILER_PREFIX}g++ 
	sp_save_parameter "IFX_CXX" "$IFX_CXX"
	IFX_CPP=${CROSS_COMPILER_PREFIX}cpp
	sp_save_parameter "IFX_CPP" "$IFX_CPP"
	IFX_OBJCOPY=${CROSS_COMPILER_PREFIX}objcopy
	sp_save_parameter "IFX_OBJCOPY" "$IFX_OBJCOPY"
	IFX_OBJDUMP=${CROSS_COMPILER_PREFIX}objdump
	sp_save_parameter "IFX_OBJDUMP" "$IFX_OBJDUMP"
	IFX_HOSTCC=gcc
	sp_save_parameter "IFX_HOSTCC" "$IFX_HOSTCC"
	SP_DEFAULT_IFX_CFLAGS="\"-Os -mips32 -mtune=4kc -I${IFX_APIS_DIR}include\""
	IFX_CFLAGS="$SP_DEFAULT_IFX_CFLAGS"
	sp_save_parameter "IFX_CFLAGS" "$IFX_CFLAGS"
	SP_DEFAULT_IFX_LDFLAGS="\"-L${BUILD_ROOTFS_DIR}lib\""
	IFX_LDFLAGS="$SP_DEFAULT_IFX_LDFLAGS"
	sp_save_parameter "IFX_LDFLAGS" "$IFX_LDFLAGS"
	TARGET=$SP_DEFAULT_TARGET
	sp_save_parameter "TARGET" "$TARGET"
	HOST=$SP_DEFAULT_HOST
	sp_save_parameter "HOST" "$HOST"
	BUILD=$SP_DEFAULT_BUILD
	sp_save_parameter "BUILD" "$BUILD"
}

sp_utility_prepare() {
	echo "$SP_PACKAGE_VERSION" > ${IFX_ROOTFS_DIR}flashdisk/etc/version
        ./build_config.sh -d 1
	if [ ! -x squashfs2.0/mksquashfs ] ; then
		cd squashfs2.0/squashfs-tools/
		make
		cd -
	fi
	if [ ! -x mkimage ] ; then
		cd $UBOOT_SOURCE_DIR
		
		# 605042:fchang.removed
		#make amazon_config
		#make mkimage
		# 605042:fchang.added
		. ${TOPDIR}tools/build_tools/Path.sh
		. ${TOPDIR}tools/build_tools/model_config.sh
		make twinpass_config
		make all

		cd -
		cp -af ${UBOOT_SOURCE_DIR}tools/mkimage .
	fi
}

sp_post_operation () {
	echo "#!/bin/sh" > Path.sh
	cat "$SP_CFG_FILENAME" >> Path.sh
	echo ". ${BUILD_TOOLS_DIR}common_funcs.sh" >> Path.sh

	# 603092:fchang.added
	echo ". ${TOOLCHAIN_BIN_DIR}toolchain-env.sh" >> Path.sh

	chmod 755 Path.sh
}

site_prepare() {
	touch "$SP_CFGTMP_FILENAME"
	sp_check_dir
	sp_basic_config
	mv "$SP_CFGTMP_FILENAME" "$SP_CFG_FILENAME"
	sp_post_operation
	sp_utility_prepare
}

#if [ ! -e /usr/bin/fakeroot ]; then
#	prompt_nl "ERROR!! Install fakeroot first!"
#	exit 1
#fi

rm -f $SP_CFG_FILENAME

if [ ! -e "$SP_CFG_FILENAME" ] ; then
	site_prepare
fi

