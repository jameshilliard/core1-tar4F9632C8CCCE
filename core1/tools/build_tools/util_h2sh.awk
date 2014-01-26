# 507251:tc.chen 2005/07/25 inital version : a utility to transfer form C header file to script file (*.h to *.sh) 
# dump flag 0: normal: dump next line
#	    1: dump next line ( in if condition )
#	    2: skip next line ( in if condition )
#	    3: dump next line ( in else condition )
#	    4: skip next line ( in else condition )

/^#define/ { 
	if ( dump != 2  && dump != 4 )
        {
		if (NF > 2 )
		{
			# skip comment ( /* */ and //)
			cmp_str = "echo '"$3"' | sed -n '/\\/\\//p'|grep \"//\">/dev/null"
			cmp_str1 = "echo '"$3"' | sed -n '/\\/\\*/p'|grep \"/\\*\">/dev/null"
			# skip //
			if ( system(cmp_str) == 0 )  
			{
				print $2"=\"1\"" 

			}
			# skip /* */
			else if (system(cmp_str1) == 0 )
			{
				i = 3
				break_loop = 0
				while (i<= NF)
				{
					#if ( $i != "*/")
					cmp_str = "echo '"$i"' | sed -n '/\\*\\//p'|grep \"\\*/\">/dev/null"
					if ( system(cmp_str) != 0 )
					{
						i++
					}
					else
					{
						break_loop = i+1
						i = NF+1
					}
				}
				if (break_loop > 0 && break_loop <= NF)
				{
					print $2"="$break_loop""
				}
				else
				{
					print $2"=\"1\"" 
				}
				
			} else
			{
				print $2"="$3""
			}
		} else {
			print $2"=\"1\"" 
		}
	}
} 
/^#ifdef/ { 
	str = "sed -n '/^#define "$2"/p' "FILENAME" |grep "$2" > /dev/null"
	ret = system(str)
	#found
	if ( ret == 0 )
	{
		dump = 1;
	}
	#not found
	else
	{
		dump = 2;
	}
}
/^#ifndef/ { 
	str = "sed -n '/^#define "$2"/p' "FILENAME" |grep "$2" > /dev/null"
	ret = system(str)
	#found
	if ( ret == 0 )
	{
		dump = 2;
	}
	#not found
	else
	{
		dump = 1;
	}
}
/^#else/ { 
	if ( dump == 2 )
	{
		dump = 3 
	}else if ( dump == 1)
	{
		dump = 4
	}else
	{
		print "Parsing ifx_config.h file fail.\n"
		exit 1
	}
}
/^#endif/ { 
	if ( dump > 0 ) 
	{ 
		dump = 0 
	}else
	{
		print "Parsing ifx_config.h file fail.\n"
		exit 1
	}
}
