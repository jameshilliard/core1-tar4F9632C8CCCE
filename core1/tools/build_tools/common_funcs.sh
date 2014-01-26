#!/bin/sh
# 507251:tc.chen 2005/07/25 fix parse ifx_config.h file worng with "//" keyword

build_help()
{
	echo "Usage: $0 [-clean -configure]"
	echo "-clean: clean binary and object files"
	echo "-configure: use make configure to configure the complie options."
}

parse_args()
{
	echo parse_args
	echo $0 $1 $2 $3 $4 $5
	BUILD_CLEAN=0
	BUILD_CONFIGURE=0
	while [ $# -ge 1 ]; do
		case $1 in
		-clean) 
			echo clean
			BUILD_CLEAN=1;;
		-configure) 
			echo configure
			BUILD_CONFIGURE=1;;
		config_only)
		    ;;
		*) build_help ;;
		esac
		shift
	done
}

ifx_error_check()
{
	if [ $1 -ne 0 ]
	then
        echo "ERROR: building $APPS_NAME fail, errno: $1"
        exit $1
	fi
}

create_config()
{
#507251:tc.chen	awk '/^#define/ { if (length($3)>0 && $3 != "//" )  {print $2"="$3""}  else {print $2"=\"1\""}}' ifx_config.h > config.sh
	awk -f util_h2sh.awk ifx_config.h > config.sh
#507251:tc.chen	awk '/^#define/ { if (length($3)>0)  {print $2"="$3""}  else {print $2"=\"1\""}}' model_config.h > model_config.sh
	awk -f util_h2sh.awk model_config.h > model_config.sh
	chmod +x config.sh
	chmod +x model_config.sh
}

