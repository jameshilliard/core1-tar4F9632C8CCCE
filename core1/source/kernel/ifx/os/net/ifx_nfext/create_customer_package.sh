#!/bin/sh

if [ ! -n "$1" ] ; then
	echo "Usage : create_customer_package.sh SOURCE_PATH"
	exit 0
fi

rm -rf *
cp $1/Makefile Makefile


#cp $1/ifx_nfext_core.o ifx_nfext_core.oo
#cp $1/ifx_nfext_ppp.o ifx_nfext_ppp.oo
#165001:henryhsu:20050818:Modify for crate CD build fail for these modules.
if [ $1/ifx_nfext_core.o ]; then
cp $1/ifx_nfext_core.o ifx_nfext_core.oo
fi

if [ $1/ifx_nfext_ppp.o ]; then
cp $1/ifx_nfext_ppp.o ifx_nfext_ppp.oo
fi

if [ $1/ifx_nfext_sw_phyport.o ]; then
cp $1/ifx_nfext_sw_phyport.o ifx_nfext_sw_phyport.oo
fi

if [ $1/ifx_nfext_vbridge.o ]; then
cp $1/ifx_nfext_vbridge.o ifx_nfext_vbridge.oo
fi
#165001

sed -i -e 's/^CONFIG_FULL_PACKAGE/#CONFIG_FULL_PACKAGE/g' Makefile
