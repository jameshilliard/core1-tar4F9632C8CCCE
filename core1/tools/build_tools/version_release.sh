#!/bin/sh

if [ $# -ge 1 ]; then
  while [ $# -ge 1 ]; do
    case $1 in
      -n)
        python next_number.py ../../source/kernel/opensource/linux-2.4.31/.hhl_target_lspname
        python next_number.py ../../source/kernel/opensource/linux-2.4.31/include/linux/version.h
        python next_number.py ../../source/rootfs/flashdisk/etc/version
        ;;
      -k)
        echo "amazon-$2" > ../../source/kernel/opensource/linux-2.4.31/.hhl_target_lspname
	sed -i -e "s/amazon-.*\"/amazon-$2\"/g" ../../source/kernel/opensource/linux-2.4.31/include/linux/version.h
        shift
        ;;
      -r)
        echo $2 > ../../source/rootfs/flashdisk/etc/version
        shift
        ;;
      -nk)
        python next_number.py ../../source/kernel/opensource/linux-2.4.31/.hhl_target_lspname
        python next_number.py ../../source/kernel/opensource/linux-2.4.31/include/version.h
        ;;
      -nr)
        python next_number.py ../../source/rootfs/flashdisk/etc/version
        ;;
      *) echo "Usage : version_release.sh [-k version_number] [-r root_number] [-nk] [-nr]"
         echo "If no arguments assigned then display current version"
         echo "-k: set-up the kernel patch version"
         echo "-r: set-up the root filesystem patch version"
         echo "-n: increase both of kernel and rootfs  patch version"
         echo "-nk: increase the kernel patch version"
         echo "-nr: increase the root filesystem patch version"
         ;;
    esac
    shift
  done
fi

VERSION=`grep ^VERSION ../../source/kernel/opensource/linux-2.4.31/Makefile|cut -f3 -d' '`
PATCHLEVEL=`grep ^PATCHLEVEL ../../source/kernel/opensource/linux-2.4.31/Makefile|cut -f3 -d' '`
SUBLEVEL=`grep ^SUBLEVEL ../../source/kernel/opensource/linux-2.4.31/Makefile|cut -f3 -d' '`
Kernel_Current="$VERSION.$PATCHLEVEL.$SUBLEVEL-`cat ../../source/kernel/opensource/linux-2.4.31/.hhl_target_lspname`"

if [ -f ../../source/rootfs/flashdisk/etc/version ]; then
  RootFS_Current="`cat ../../source/rootfs/flashdisk/etc/version`"
else
  RootFS_Current="1.0.0"
  echo $RootFS_Current > ../../source/rootfs/flashdisk/etc/version
fi

echo "Kernel: $Kernel_Current"
echo "Root File System: $RootFS_Current"

