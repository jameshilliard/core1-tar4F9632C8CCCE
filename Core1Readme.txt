UltraLine Series 3 MIPS Core 1 Build Instructions
These build instructions are for the software running on MIPS Core 1
This package has been tested on Fedora Core release 5.
This source package requires the mips-linux-uclibc-eb toolchain installed.

Steps to install and build Core 1 GPL open source software.

   1. untar the core1 tree into a working directory.
          * tar jxf core1.tar.bz2 
   2. Go to the package build directory.
          * cd core1/tools/build_tools 
   3. Run a site preparation script to configure local environment variables.
          * ./site_prep.sh
            This script only needs to be run once and will ask for the location of the 
            mips toolchain (default is the expected openrg location) and location of a
            build directory to place completed images. 
   4. Build source code tree.
          * ./build_all_core1_vz_64.sh
            This script will build all GPL open source pieces associated with Core 1. 
            A final image is placed in the selected build directory. 
            Intermediate executables can be found within the source tree. 

