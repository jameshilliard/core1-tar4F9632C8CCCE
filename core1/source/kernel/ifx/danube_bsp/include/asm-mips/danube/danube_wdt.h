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
/*
 * DANUBE_WDT.C
 *
 * Global DANUBE_PMU driver header file
 * 04-Aug 2005 Jin-Sze.Sow@infineon.com comments edited
 *
 */
/* Modification history */
/* */
/* 12-july-2005 Jin-Sze.Sow@infineon.com */



#ifndef DANUBE_WDT_H
#define DANUBE_WDT_H

#ifdef WDT_DEBUG

unsigned int danube_wdt_register[3] = { 0x00000001,
                                        0, 
                                        0x00000002
                                       };

#define DANUBE_WDT_REG32(addr) 		 (danube_wdt_register[(addr - DANUBE_BIU_WDT_BASE)/4])

#else

#define DANUBE_WDT_REG32(addr) 		 (*((volatile u32*)(addr)))

#endif

/* Define for device driver code */
#define DEVICE_NAME "wdt"

/* Danube wdt ioctl control */
#define DANUBE_WDT_IOC_MAGIC             0xc0
#define DANUBE_WDT_IOC_START            _IOW(DANUBE_WDT_IOC_MAGIC, 0, int)
#define DANUBE_WDT_IOC_STOP             _IO(DANUBE_WDT_IOC_MAGIC, 1)
#define DANUBE_WDT_IOC_PING             _IO(DANUBE_WDT_IOC_MAGIC, 2)
#define DANUBE_WDT_IOC_SET_PWL          _IOW(DANUBE_WDT_IOC_MAGIC, 3, int)
#define DANUBE_WDT_IOC_SET_DSEN         _IOW(DANUBE_WDT_IOC_MAGIC, 4, int)
#define DANUBE_WDT_IOC_SET_LPEN         _IOW(DANUBE_WDT_IOC_MAGIC, 5, int)
#define DANUBE_WDT_IOC_GET_STATUS       _IOR(DANUBE_WDT_IOC_MAGIC, 6, int)
#define DANUBE_WDT_IOC_SET_CLKDIV	_IOW(DANUBE_WDT_IOC_MAGIC, 7, int) 

#define DANUBE_WDT_PW1 0x000000BE /**< First password for access */
#define DANUBE_WDT_PW2 0x000000DC /**< Second password for access */

#define DANUBE_WDT_CLKDIV0_VAL 1
#define DANUBE_WDT_CLKDIV1_VAL 64
#define DANUBE_WDT_CLKDIV2_VAL 4096
#define DANUBE_WDT_CLKDIV3_VAL 262144
#define DANUBE_WDT_CLKDIV0 0
#define DANUBE_WDT_CLKDIV1 1
#define DANUBE_WDT_CLKDIV2 2
#define DANUBE_WDT_CLKDIV3 3

#endif //DANUBE_WDT_H
