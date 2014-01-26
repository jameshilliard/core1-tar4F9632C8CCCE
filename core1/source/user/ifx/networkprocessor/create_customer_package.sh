#!/bin/sh

if [ ! -n "$1" ] ; then
	echo "Usage : create_customer_package.sh SOURCE_PATH"
	exit 0
fi

rm -f *.c *.h Makefile
cp -f $1/tcpmessages .

sed -i -e 's/^CONFIG_FULL_PACKAGE/#CONFIG_FULL_PACKAGE/g' build.sh 
rm -f create_customer_package.sh
