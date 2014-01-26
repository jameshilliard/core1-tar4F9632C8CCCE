#!/bin/sh

TOPDIR=`pwd`/../../
. ./Path.sh

CONFIG_MODEL=0

CONFIG_MODEL_NAME=""

# compatible with previous version
if [ "$1" = "-d" ] ; then
   CONFIG_MODEL=$2
fi

# support -r
if [ "$1" = "-r" ]; then
		if [ -e ${TOPDIR}config/current_config ] ; then
				CONFIG_MODEL=`cat ../../config/current_config|tr -d [:blank:]`
		else
				echo "Please run ./build_config.sh first"
				exit 1
		fi
fi

case "$CONFIG_MODEL" in
   "1")
   CONFIG_MODEL_NAME="_Core1"
   echo "#define IFX_CONFIG_CPU       \"TWINPASS-E\"" > ${TOPDIR}config/model_config
   echo "#define IFX_CONFIG_MEMORY_SIZE  \"64\"" >> ${TOPDIR}config/model_config
   echo "#define IFX_CONFIG_FLASH_SIZE \"8\"" >> ${TOPDIR}config/model_config
   echo "#define IFX_CONFIG_FUTURE_SET \"V10\"" >> ${TOPDIR}config/model_config
   cat ${TOPDIR}config/tpe1_apps_config.1.0 > ${TOPDIR}config/apps_config
   cat ${TOPDIR}config/tpe1_64_kernel_config_vz.1.0 > ${TOPDIR}config/kernel_config
   cat ${TOPDIR}config/tpe_model.h.1.0 > ${TOPDIR}config/model.h;;
esac

# save current config to file
echo $CONFIG_MODEL > ${TOPDIR}config/current_config

# append compiler flags
echo "" >> ${TOPDIR}config/kernel_config
echo "#" >> ${TOPDIR}config/kernel_config
echo "# Append Compiler Flags" >> ${TOPDIR}config/kernel_config
echo "#" >> ${TOPDIR}config/kernel_config

# append CONFIG_CPU_AMAZON_E
if [ `awk '/IFX_CONFIG_CPU/{print$3}' ${TOPDIR}config/model_config|tr -d \"` = "AMAZON_E" ]; then
	echo "CONFIG_CPU_AMAZON_E=y" >> ${TOPDIR}config/kernel_config
else
	echo "# CONFIG_CPU_AMAZON_E is not set" >> ${TOPDIR}config/kernel_config
fi
# append CONFIG_MTD_AMAZON_FLASH_SIZE
FLASH_SIZE=`awk '/IFX_CONFIG_FLASH_SIZE/{print$3}' ${TOPDIR}config/model_config|tr -d \"`
echo "CONFIG_MTD_AMAZON_FLASH_SIZE=$FLASH_SIZE" >> ${TOPDIR}config/kernel_config

case "$CONFIG_MODEL" in
   "1")
   echo "Configure to TwinPass-E: Verizon Core1 64 Meg Version 1.0";;
esac

# link file
rm -f ${TOPDIR}source/kernel/ifx/danube_bsp/include/asm-mips/danube/model.h
ln -s ${TOPDIR}config/model.h ${TOPDIR}source/kernel/ifx/danube_bsp/include/asm-mips/danube/model.h

rm -f ${TOPDIR}source/kernel/opensource/linux-2.4.31/ifx_kernel_config_danube
ln -s ${TOPDIR}config/kernel_config ${TOPDIR}source/kernel/opensource/linux-2.4.31/ifx_kernel_config_danube
rm -f ${TOPDIR}source/kernel/opensource/linux-2.4.31/.config
rm -f ${TOPDIR}source/kernel/opensource/linux-2.4.31/.depend

rm -f ${TOPDIR}source/user/ifx/IFXAPIs/include/ifx_config.h
ln -s ${TOPDIR}config/apps_config ${TOPDIR}source/user/ifx/IFXAPIs/include/ifx_config.h

rm -f $BUILD_TOOLS_DIR/ifx_config.h
ln -s ${TOPDIR}config/apps_config $BUILD_TOOLS_DIR/ifx_config.h

rm -f $BUILD_TOOLS_DIR/model_config.h
ln -s ${TOPDIR}config/model_config $BUILD_TOOLS_DIR/model_config.h

# create config.sh model_config.sh
create_config

echo "${IFX_PLATFORM_NAME}-${IFX_PACKAGE_VERSION}-${CONFIG_MODEL_NAME}" > ${KERNEL_SOURCE_DIR}.hhl_target_lspname
