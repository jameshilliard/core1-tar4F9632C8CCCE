#!/bin/sh
TOPDIR=../../
. ./Path.sh
cd ${BUILD_ROOTFS_DIR}/etc
for f in `find . -name "*" -type f`
do
	bin=`echo $f |grep ".gz"`
	if [ -z "${bin}" ]
	then
	#Added to avoid reading and writing to the same file at the sametime.
	cat $f | egrep -v "^#[^\!<>]" |egrep -v "^#$" > $f.tmp
	cp -f $f.tmp $f
	rm -f $f.tmp
	echo "Pruned : " $f
	fi
done
