/*
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * port_defs.h
 *
 * Local DANUBE port header file
 *
 */

/* Modification history */
/* 21Jun2004	btxu	Generate from INCA-IP project */



#ifndef PORT_DEFS_H
#define PORT_DEFS_H

#define OK	0

#define DANUBE_PORT_OUT_REG		0x00000010
#define DANUBE_PORT_IN_REG		0x00000014
#define DANUBE_PORT_DIR_REG		0x00000018
#define DANUBE_PORT_ALTSEL0_REG		0x0000001C
#define DANUBE_PORT_ALTSEL1_REG		0x00000020
#define DANUBE_PORT_OD_REG		0x00000024
#define DANUBE_PORT_STOFF_REG		0x00000028
#define DANUBE_PORT_PUDSEL_REG		0x0000002C
#define DANUBE_PORT_PUDEN_REG		0x00000030

#define PORT_MODULE_ID	0xff

#define NOPS	asm("nop;nop;nop;nop;nop")
#define PORT_WRITE_REG(reg, value)   \
	*((volatile u32*)(reg)) = (u32)value; 
#define PORT_READ_REG(reg, value)    \
	value = (u32)*((volatile u32*)(reg)); 
			 
#define PORT_IOC_CALL(ret,port,pin,func) 	\
	ret=danube_port_reserve_pin(port,pin,PORT_MODULE_ID); \
	if (ret == 0) ret=func(port,pin,PORT_MODULE_ID); \
	if (ret == 0) ret=danube_port_free_pin(port,pin,PORT_MODULE_ID);

#endif /* PORT_DEFS_H */
