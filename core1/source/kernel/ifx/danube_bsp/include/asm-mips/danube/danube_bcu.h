/******************************************************************************
       Copyright (c) 2002, Infineon Technologies.  All rights reserved.

                               No Warranty
   Because the program is licensed free of charge, there is no warranty for
   the program, to the extent permitted by applicable law.  Except when
   otherwise stated in writing the copyright holders and/or other parties
   provide the program "as is" without warranty of any kind, either
   expressed or implied, including, but not limited to, the implied
   warranties of merchantability and fitness for a particular purpose. The
   entire risk as to the quality and performance of the program is with
   you.  should the program prove defective, you assume the cost of all
   necessary servicing, repair or correction.

   In no event unless required by applicable law or agreed to in writing
   will any copyright holder, or any other party who may modify and/or
   redistribute the program as permitted above, be liable to you for
   damages, including any general, special, incidental or consequential
   damages arising out of the use or inability to use the program
   (including but not limited to loss of data or data being rendered
   inaccurate or losses sustained by you or third parties or a failure of
   the program to operate with any other programs), even if such holder or
   other party has been advised of the possibility of such damages.
******************************************************************************/
#ifndef DANUBE_BCU_H
#define DANUBE_BCU_H


#ifndef BCU_DEBUG
#define DANUBE_BCU_REG32(addr)	(*((volatile unsigned int*)(addr)))
#else
unsigned int bcu_global_register[65];
#define DANUBE_BCU_REG32(addr)	(bcu_global_register[(addr - DANUBE_BCU_BASE_ADDR)/4])
#endif

#define DANUBE_BCU_IOC_MAGIC             0xd0
#define DANUBE_BCU_IOC_SET_PS    	_IOW(DANUBE_BCU_IOC_MAGIC, 0,  int)
#define DANUBE_BCU_IOC_SET_DBG   	_IOW(DANUBE_BCU_IOC_MAGIC, 1,  int)
#define DANUBE_BCU_IOC_SET_TOUT         _IOW(DANUBE_BCU_IOC_MAGIC, 2,  int)
#define DANUBE_BCU_IOC_GET_PS    	_IOR(DANUBE_BCU_IOC_MAGIC, 3,  int)
#define DANUBE_BCU_IOC_GET_DBG   	_IOR(DANUBE_BCU_IOC_MAGIC, 4,  int)
#define DANUBE_BCU_IOC_GET_TOUT         _IOR(DANUBE_BCU_IOC_MAGIC, 5, int)
#define DANUBE_BCU_IOC_GET_BCU_ERR      _IOR(DANUBE_BCU_IOC_MAGIC, 6, int)
#define DANUBE_BCU_IOC_IRNEN	        _IOW(DANUBE_BCU_IOC_MAGIC, 7, int)
#define DANUBE_BCU_IOC_SET_PM           _IOW(DANUBE_BCU_IOC_MAGIC, 8,  int)
#define DANUBE_BCU_IOC_MAXNR            9


#endif //DANUBE_EBU_H


