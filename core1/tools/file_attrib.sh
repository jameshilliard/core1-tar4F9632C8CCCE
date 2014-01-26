#!/bin/sh

if [ ! -n "$1" ];then
	echo "Usage : `basename $0` <PATH>"
	echo "`basename $0` is used to change file attribution"
	exit 0
fi
if [ ! -d "$1" ];then
	echo "Error!! Can't change to the path:$1"
	exit 1
fi
cd $1
chmod -R 644 *
chown -R 0:0 *
find . -type f -a ! -name "*.c" -a ! -name "*.h" -a ! -name "*.txt" -exec file {} \;|grep executable|cut -f1 -d:|xargs chmod 755
find . -type f -a -name "*.so" -exec chmod 755 {} \;
find . -name Configure -exec chmod 755 {} \;
cd -
