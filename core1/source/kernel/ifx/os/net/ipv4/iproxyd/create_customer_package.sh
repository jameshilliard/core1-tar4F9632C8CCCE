#!/bin/sh

if [ ! -n "$1" ] ; then
	echo "Usage : create_customer_package.sh SOURCE_PATH"
	exit 0
fi

rm -rf *
cp $1/Makefile Makefile
cp $1/iproxyd.o iproxyd.oo

sed -i -e 's/^CONFIG_FULL_PACKAGE/#CONFIG_FULL_PACKAGE/g' Makefile
