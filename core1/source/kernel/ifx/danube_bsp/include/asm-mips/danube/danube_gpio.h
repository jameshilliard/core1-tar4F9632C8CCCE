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
 * DANUBE_GPIO.C
 *
 * Global DANUBE_GPIO driver header file
 * 21 Jun 2004 btxu  Generate from INCA-IP project
 * 21 Jun 2005 Jin-Sze.Sow@infineon.com comments edited
 *
 */
#ifndef DANUBE_GPIO_H
#define DANUBE_GPIO_H

#define OK				0

#define DANUBE_PORT_OUT_REG		0x00000010
#define DANUBE_PORT_IN_REG		0x00000014
#define DANUBE_PORT_DIR_REG		0x00000018
#define DANUBE_PORT_ALTSEL0_REG		0x0000001C
#define DANUBE_PORT_ALTSEL1_REG		0x00000020
#define DANUBE_PORT_OD_REG		0x00000024
#define DANUBE_PORT_STOFF_REG		0x00000028
#define DANUBE_PORT_PUDSEL_REG		0x0000002C
#define DANUBE_PORT_PUDEN_REG		0x00000030

#define PORT_MODULE_MEI_JTAG		0x1
#define PORT_MODULE_ID	0xff

#define NOPS				asm("nop;nop;nop;nop;nop");

#undef GPIO_DEBUG


#ifdef GPIO_DEBUG

unsigned int gpio_global_register[25]= {0,0,0,0,
					0x00000004,
					0x00000005,
					0x00000006,
					0x00000007,
					0x00000008,
					0x00000009,
					0x0000000A,
					0x0000000B,
					0x0000000C,
					0x0000000D,
					0x0000000E,
					0x0000000F,
					0x00000010,
					0x00000011,
					0x00000012,
					0x00000013,
					0x00000014,
					0x00000015,
					0x00000016,
					0x00000017,
					0x00000018
					};

#define PORT_WRITE_REG(reg,value)       gpio_global_register[((u32)reg - DANUBE_GPIO)/4] = (u32)value;
#define PORT_READ_REG(reg, value)       value = (gpio_global_register[((u32)reg - DANUBE_GPIO)/4]);	 
#else

#define PORT_WRITE_REG(reg, value)  	*((volatile u32*)(reg)) = (u32)value; 
#define PORT_READ_REG(reg, value)       value = (u32)*((volatile u32 *)(reg));

#endif
		 
#define PORT_IOC_CALL(ret,port,pin,func) 	\
	ret=danube_port_reserve_pin(port,pin,PORT_MODULE_ID); \
	if (ret == 0) ret=func(port,pin,PORT_MODULE_ID); \
	if (ret == 0) ret=danube_port_free_pin(port,pin,PORT_MODULE_ID);

#endif /* DANUBE_GPIO */
