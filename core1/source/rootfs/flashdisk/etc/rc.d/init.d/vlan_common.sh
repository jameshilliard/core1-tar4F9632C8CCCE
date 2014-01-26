#!/bin/sh
#509141: add vlan common script

FUNCNAME=`basename $1`

case "$FUNCNAME" in
"vbridge_get_ifname")
	index="$2"
	if [ $index -eq 0 ]; then
		echo "br0"
	elif [ $index -gt 0 -a $index -lt 16 ]; then
		echo "nas$index"
	elif [ $index -ge 16 -a $index -le 20 ]; then
		echo "swport`expr $index - 16`"
	else
		echo ""
	fi
	;;
"vbridge_get_cfg")
	index="$2"
	total=$vb_pbvgs_groups
	found=0
	vid=0
	i=1
	while [ "$i" -le "$vb_pbvgs_groups" -a "$found" -eq 0 ]
	do
		eval group='$'vb_pbvgs_groups_$i
		if [ `echo $group | cut -f$index -d"_"` -eq 1 ]; then
			vid=$i
			found=1
		fi
		i=`expr $i + 1`
	done
	if [ "$found" -eq 1 ]; then
		echo "$vid 0 0"
	else
		echo ""
	fi
	;;
*)
	echo ""
	;;
esac
