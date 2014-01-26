/******************************************************************************
     Copyright (c) 2004, Infineon Technologies.  All rights reserved.

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
 ******************************************************************************
   Module      : admmod.c
   Date        : 2004-09-01
   Description : JoeLin
   Remarks:

   Revision:
	MarsLin, add to support VLAN
	LanceBai, support VLAN filter, IGMP snopping, bandwith control

 *****************************************************************************/
//000001.joelin 2005/06/02 add"ADM6996_MDC_MDIO_MODE" define, 
//		if define ADM6996_MDC_MDIO_MODE==> ADM6996LC and ADM6996I will be in MDIO/MDC(SMI)(16 bit) mode,
//		amazon should contrl ADM6996 by MDC/MDIO pin
//  		if undef ADM6996_MDC_MDIO_MODE==> ADM6996  will be in EEProm(32 bit) mode,
//		amazon should contrl ADM6996 by GPIO15,16,17,18  pin
/* 507281:linmars 2005/07/28 support MDIO/EEPROM config mode */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <asm/atomic.h>
#include <asm/danube/danube.h>
#include <asm/danube/adm6996.h>
//#include <linux/amazon/adm6996.h>
//#include "ifx_swdrv.h"
/* bit masks */
#define ADM_SW_BIT_MASK_1	0x00000001
#define ADM_SW_BIT_MASK_2	0x00000002
#define ADM_SW_BIT_MASK_4	0x00000008
#define ADM_SW_BIT_MASK_10	0x00000200
#define ADM_SW_BIT_MASK_16	0x00008000
#define ADM_SW_BIT_MASK_32	0x80000000

/* delay timers */
#define ADM_SW_MDC_DOWN_DELAY	5
#define ADM_SW_MDC_UP_DELAY	5
#define ADM_SW_CS_DELAY		5

/* MDIO modes */
#define ADM_SW_MDIO_OUTPUT	1
#define ADM_SW_MDIO_INPUT	0

#define ADM_SW_MAX_PORT_NUM	5
#define ADM_SW_MAX_VLAN_NUM	15

/* registers */
#define ADM_SW_PORT0_CONF	0x1
#define ADM_SW_PORT1_CONF	0x3
#define ADM_SW_PORT2_CONF	0x5
#define ADM_SW_PORT3_CONF	0x7
#define ADM_SW_PORT4_CONF	0x8
#define ADM_SW_PORT5_CONF	0x9
#define ADM_SW_SYSCTL0		0xa
#define ADM_SW_SYSCTL1		0xb
#define ADM_SW_IGMPCTL		0xc
#define ADM_SW_VLAN_MODE	0x11
#define ADM_SW_MAC_LOCK		0x12
#define ADM_SW_VLAN0_CONF	0x13
#define ADM_SW_QUEUE_CONF1	0x25
#define ADM_SW_QUEUE_CONF2	0x26
#define ADM_SW_QUEUE_CONF3	0x27
#define ADM_SW_PORT0_PVID	0x28
#define ADM_SW_PORT1_PVID	0x29
#define ADM_SW_PORT2_PVID	0x2a
#define ADM_SW_PORT34_PVID	0x2b
#define ADM_SW_PORT5_PVID	0x2c
#define ADM_SW_PHY_RESET	0x2f
#define ADM_SW_MISC_CONF	0x30
#define ADM_SW_BNDWTH_CTL0	0x31
#define ADM_SW_BNDWTH_CTL1	0x32
#define ADM_SW_BNDWTH_CTL_ENA	0x33
#define ADM_SW_BNDWTH_EXTCTL0	0x34
#define ADM_SW_BNDWTH_EXTCTL1	0x35
#define ADM_SW_BNDWTH_EXTCTL2	0x36
#define ADM_SW_BNDWTH_EXTCTL3	0x37
#define ADM_SW_BNDWTH_EXTCTL4	0x38
#define ADM_SW_BNDWTH_EXTCTL5	0x39
#define ADM_SW_BNDWTH_EXTCTL6	0x3A

#define ADM_SW_VLAN_FILTER0_LOW 0x40
#define ADM_SW_VLAN_FILTER1_LOW 0x42
#define ADM_SW_VLAN_FILTER2_LOW 0x44
#define ADM_SW_VLAN_FILTER3_LOW 0x46
#define ADM_SW_VLAN_FILTER4_LOW 0x48
#define ADM_SW_VLAN_FILTER5_LOW 0x4A
#define ADM_SW_VLAN_FILTER6_LOW 0x4C
#define ADM_SW_VLAN_FILTER7_LOW 0x4E
#define ADM_SW_VLAN_FILTER8_LOW 0x50
#define ADM_SW_VLAN_FILTER9_LOW 0x52
#define ADM_SW_VLAN_FILTER10_LOW 0x54
#define ADM_SW_VLAN_FILTER11_LOW 0x56
#define ADM_SW_VLAN_FILTER12_LOW 0x58
#define ADM_SW_VLAN_FILTER13_LOW 0x5A
#define ADM_SW_VLAN_FILTER14_LOW 0x5C
#define ADM_SW_VLAN_FILTER15_LOW 0x5E

#define ADM_SW_VLAN_FILTER0_HIGH 0x41
#define ADM_SW_VLAN_FILTER1_HIGH 0x43
#define ADM_SW_VLAN_FILTER2_HIGH 0x45
#define ADM_SW_VLAN_FILTER3_HIGH 0x47
#define ADM_SW_VLAN_FILTER4_HIGH 0x49
#define ADM_SW_VLAN_FILTER5_HIGH 0x4B
#define ADM_SW_VLAN_FILTER6_HIGH 0x4D
#define ADM_SW_VLAN_FILTER7_HIGH 0x4F
#define ADM_SW_VLAN_FILTER8_HIGH 0x51
#define ADM_SW_VLAN_FILTER9_HIGH 0x53
#define ADM_SW_VLAN_FILTER10_HIGH 0x55
#define ADM_SW_VLAN_FILTER11_HIGH 0x57
#define ADM_SW_VLAN_FILTER12_HIGH 0x59
#define ADM_SW_VLAN_FILTER13_HIGH 0x5B
#define ADM_SW_VLAN_FILTER14_HIGH 0x5D
#define ADM_SW_VLAN_FILTER15_HIGH 0x5F


#define ADM_SW_TCPUDP_FILTER0	0x8C
#define ADM_SW_TCPUDP_FILTER1	0x8D
#define ADM_SW_TCPUDP_FILTER2	0x8E
#define ADM_SW_TCPUDP_FILTER3	0x8F
#define ADM_SW_TCPUDP_FILTER4	0x90
#define ADM_SW_TCPUDP_FILTER5	0x91
#define ADM_SW_TCPUDP_FILTER6	0x92
#define ADM_SW_TCPUDP_FILTER7	0x93
#define ADM_SW_TYPE_ACTION	0x94
#define ADM_SW_PROTO_ACTION	0x95
#define ADM_SW_TCPUDP_ACT0	0x96
#define ADM_SW_TCPUDP_ACT1	0x97
#define ADM_SW_TCPUDP_ACT2	0x98


#define ADM_SW_PORTSTS0		0xA2
#define ADM_SW_PORTSTS1		0xA3
#define ADM_SW_PORTSTS2		0xA4
#define ADM_SW_PORT0_RXCNTL	0xA8
#define ADM_SW_PORT0_RXCNTH	0xA9
#define ADM_SW_PORT1_RXCNTL	0xAC
#define ADM_SW_PORT1_RXCNTH	0xAD
#define ADM_SW_PORT2_RXCNTL	0xB0
#define ADM_SW_PORT2_RXCNTH	0xB1
#define ADM_SW_PORT3_RXCNTL	0xB4
#define ADM_SW_PORT3_RXCNTH	0xB5
#define ADM_SW_PORT4_RXCNTL	0xB6
#define ADM_SW_PORT4_RXCNTH	0xB7
#define ADM_SW_PORT5_RXCNTL	0xB8
#define ADM_SW_PORT5_RXCNTH	0xB9
#define ADM_SW_PORT0_RXBCNTL	0xBA
#define ADM_SW_PORT0_RXBCNTH	0xBB
#define ADM_SW_PORT1_RXBCNTL	0xBE
#define ADM_SW_PORT1_RXBCNTH	0xBF
#define ADM_SW_PORT2_RXBCNTL	0xC2
#define ADM_SW_PORT2_RXBCNTH	0xC3
#define ADM_SW_PORT3_RXBCNTL	0xC6
#define ADM_SW_PORT3_RXBCNTH	0xC7
#define ADM_SW_PORT4_RXBCNTL	0xC8
#define ADM_SW_PORT4_RXBCNTH	0xC9
#define ADM_SW_PORT5_RXBCNTL	0xCA
#define ADM_SW_PORT5_RXBCNTH	0xCB
#define ADM_SW_PORT0_TXCNTL	0xCC
#define ADM_SW_PORT0_TXCNTH	0xCD
#define ADM_SW_PORT1_TXCNTL	0xD0
#define ADM_SW_PORT1_TXCNTH	0xD1
#define ADM_SW_PORT2_TXCNTL	0xD4
#define ADM_SW_PORT2_TXCNTH	0xD5
#define ADM_SW_PORT3_TXCNTL	0xD8
#define ADM_SW_PORT3_TXCNTH	0xD9
#define ADM_SW_PORT4_TXCNTL	0xDA
#define ADM_SW_PORT4_TXCNTH	0xDB
#define ADM_SW_PORT5_TXCNTL	0xDC
#define ADM_SW_PORT5_TXCNTH	0xDD
#define ADM_SW_PORT0_TXBCNTL	0xDE
#define ADM_SW_PORT0_TXBCNTH	0xDF
#define ADM_SW_PORT1_TXBCNTL	0xE2
#define ADM_SW_PORT1_TXBCNTH	0xE3
#define ADM_SW_PORT2_TXBCNTL	0xE6
#define ADM_SW_PORT2_TXBCNTH	0xE7
#define ADM_SW_PORT3_TXBCNTL	0xEA
#define ADM_SW_PORT3_TXBCNTH	0xEB
#define ADM_SW_PORT4_TXBCNTL	0xEC
#define ADM_SW_PORT4_TXBCNTH	0xED
#define ADM_SW_PORT5_TXBCNTL	0xEE
#define ADM_SW_PORT5_TXBCNTH	0xEF
#define ADM_SW_PORT0_COLCNTL	0xF0
#define ADM_SW_PORT0_COLCNTH	0xF1
#define ADM_SW_PORT1_COLCNTL	0xF4
#define ADM_SW_PORT1_COLCNTH	0xF5
#define ADM_SW_PORT2_COLCNTL	0xF8
#define ADM_SW_PORT2_COLCNTH	0xF9
#define ADM_SW_PORT3_COLCNTL	0xFC
#define ADM_SW_PORT3_COLCNTH	0xFD
#define ADM_SW_PORT4_COLCNTL	0xFE
#define ADM_SW_PORT4_COLCNTH	0xFF
#define ADM_SW_PORT5_COLCNTL	0x100
#define ADM_SW_PORT5_COLCNTH	0x101

#define ADM_SW_PORT0_ERRCNTL	0x102
#define ADM_SW_PORT0_ERRCNTH	0x103
#define ADM_SW_PORT1_ERRCNTL	0x106
#define ADM_SW_PORT1_ERRCNTH	0x107
#define ADM_SW_PORT2_ERRCNTL	0x10A
#define ADM_SW_PORT2_ERRCNTH	0x10B
#define ADM_SW_PORT3_ERRCNTL	0x10E
#define ADM_SW_PORT3_ERRCNTH	0x10F
#define ADM_SW_PORT4_ERRCNTL	0x110
#define ADM_SW_PORT4_ERRCNTH	0x111
#define ADM_SW_PORT5_ERRCNTL	0x112
#define ADM_SW_PORT5_ERRCNTH	0x113




/* TCP/UDP control */
#define ADM_SW_TCPUDP_MAX	0x8	/* max TCP/UDP filter number */

/* port status */
#define ADM_SW_FC_ENA		0x8	/* Flow control enabled */
#define ADM_SW_DUP_ENA		0x4	/* Full Duplex enabled */
#define ADM_SW_100M_ENA		0x2	/* Link @ 100Mbps */
#define ADM_SW_LINK_ENA		0x1	/* Linked */

/* port modes */
#define ADM_SW_PORT_FLOWCTL	0x1	/* 802.3x flow control */
#define ADM_SW_PORT_AN		0x2	/* auto negotiation */
#define ADM_SW_PORT_100M	0x4	/* 100M */
#define ADM_SW_PORT_FULL	0x8	/* full duplex */
#define ADM_SW_PORT_TAG		0x10	/* output tag on */
#define ADM_SW_PORT_DISABLE	0x20	/* disable port */
#define ADM_SW_PORT_TOS		0x40	/* TOS first */
#define ADM_SW_PORT_PPRI	0x80	/* port based priority first */
#define ADM_SW_PORT_MDIX	0x8000	/* auto MDIX on */
#define ADM_SW_PORT_PVID_SHIFT	10
#define ADM_SW_PORT_PVID_BITS	4

#if 0
/* VLAN */
#define ADM_SW_VLAN_PORT0	0x1
#define ADM_SW_VLAN_PORT1	0x2
#define ADM_SW_VLAN_PORT2	0x10
#define ADM_SW_VLAN_PORT3	0x40
#define ADM_SW_VLAN_PORT4	0x80
#define ADM_SW_VLAN_PORT5	0x100
#endif



#define BIT_0	1<<0
#define BIT_1	1<<1
#define BIT_2	1<<2
#define BIT_3	1<<3
#define BIT_4	1<<4
#define BIT_5	1<<5
#define BIT_6	1<<6
#define BIT_7	1<<7
#define BIT_8	1<<8
#define BIT_9	1<<9
#define BIT_10 	1<<10
#define BIT_11	1<<11
#define BIT_12	1<<12
#define BIT_13	1<<13
#define BIT_14	1<<14
#define BIT_15	1<<15
#define BIT_16 	1<<16
#define BIT_17	1<<17
#define BIT_18	1<<18
#define BIT_19	1<<19
#define BIT_20	1<<20
#define BIT_21	1<<21
#define BIT_22	1<<22
#define BIT_23	1<<23
#define BIT_24	1<<24
#define BIT_25	1<<25
#define BIT_26	1<<26
#define BIT_27	1<<27
#define BIT_28	1<<28
#define BIT_29	1<<29
#define BIT_30	1<<30
#define BIT_31	1<<31



unsigned int ifx_sw_conf[ADM_SW_MAX_PORT_NUM+1] = \
	{ADM_SW_PORT0_CONF, ADM_SW_PORT1_CONF, ADM_SW_PORT2_CONF, \
	ADM_SW_PORT3_CONF, ADM_SW_PORT4_CONF, ADM_SW_PORT5_CONF};
unsigned int ifx_sw_bits[8] = \
	{0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff};
unsigned int ifx_sw_vlan_port[6] = {0, 2, 4, 6, 7, 8};
/* 507281:linmars start */
//#ifdef CONFIG_SWITCH_ADM6996_MDIO
//605112:fchang.removed #define ADM6996_MDC_MDIO_MODE 1 //000001.joelin
//#else
//#undef ADM6996_MDC_MDIO_MODE
//#endif
/* 507281:linmars end */
#define adm6996i 0
#define adm6996lc 1
#define adm6996l  2
unsigned int adm6996_mode=adm6996i;

///////////////////////start
#if 1
/* 
 * set IGMP snooping enable bit 
 * bit: 0 -> disable, 1 -> enable
 */
void ifx_sw_set_igmp(int bit)
{
	unsigned int val;
	
  if (bit)
  	ifx_sw_write(ADM_SW_IGMPCTL, ifx_sw_read(ADM_SW_IGMPCTL, &val) | BIT_1);
  else	
  	ifx_sw_write(ADM_SW_IGMPCTL, ifx_sw_read(ADM_SW_IGMPCTL, &val) & ~BIT_1);
}


/*
 * Get H/W IGMP snooping status
 * return value: 0 -> disable, 1 -> enable
 */
int ifx_sw_get_igmp(void)
{
  unsigned int val;

  return ((ifx_sw_read(ADM_SW_IGMPCTL, &val) & BIT_1) ? 1: 0);
}


void ifx_sw_set_vlanmode(int bit)
{
  unsigned int val;

  if (bit)
  	ifx_sw_write(ADM_SW_VLAN_MODE, ifx_sw_read(ADM_SW_VLAN_MODE, &val) | BIT_5);
  else	
  	ifx_sw_write(ADM_SW_VLAN_MODE, ifx_sw_read(ADM_SW_VLAN_MODE, &val) & ~BIT_5);
}


int ifx_sw_get_vlanmode(void)
{
  unsigned int val;

  return ((ifx_sw_read(ADM_SW_VLAN_MODE, &val) & BIT_5) ? 1: 0);
}



/* 
 * set IGMP proxy enable bit 
 * bit: 0 -> disable, 1 -> enable
 */
void ifx_sw_set_igmp_proxy(int bit)
{
  unsigned int val;

  if (bit)
  	ifx_sw_write(ADM_SW_IGMPCTL, ifx_sw_read(ADM_SW_IGMPCTL, &val) | BIT_0);
  else	
  	ifx_sw_write(ADM_SW_IGMPCTL, ifx_sw_read(ADM_SW_IGMPCTL, &val) & ~BIT_0);
}


int ifx_sw_get_additional_snoop_ctr(void)
{
  unsigned int val;

  ifx_sw_read(ADM_SW_SYSCTL1, &val);
  val &= 0x3000;
  val = val >> 12;
  return val;
}


//implement by lance
//set 10 to 0x0b[13:12] if enable
//set 00 to 0x0b[13:12] if disable
void ifx_sw_set_additional_snoop_ctr(int bit)
{
  unsigned int val;


  if (bit)
  	ifx_sw_write(ADM_SW_SYSCTL1, ifx_sw_read(ADM_SW_SYSCTL1, &val)  & 0xCFFF | 0x1000);
  else	
  	ifx_sw_write(ADM_SW_SYSCTL1, ifx_sw_read(ADM_SW_SYSCTL1, &val) & 0xC000);
}


/*
 * Get H/W IGMP proxy status
 * return value: 0 -> disable, 1 -> enable
 */
int ifx_sw_get_igmp_proxy(void)
{
  unsigned int val;

  return ((ifx_sw_read(ADM_SW_IGMPCTL, &val) & BIT_0) ? 1 : 0);
}

unsigned int ifx_sw_get_RXCNT(int port)
{
  unsigned int val, ret;

	switch(port)
	{
		case 0:
		ret = ifx_sw_read(ADM_SW_PORT0_RXCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT0_RXCNTH, &val)<<16;
		break;

		case 1:
		ret = ifx_sw_read(ADM_SW_PORT1_RXCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT1_RXCNTH, &val)<<16;
		break;

		case 2:
		ret = ifx_sw_read(ADM_SW_PORT2_RXCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT2_RXCNTH, &val)<<16;
		break;
		
		case 3:
		ret = ifx_sw_read(ADM_SW_PORT3_RXCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT3_RXCNTH, &val)<<16;
		break;

		case 4:
		ret = ifx_sw_read(ADM_SW_PORT4_RXCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT4_RXCNTH, &val)<<16;
		break;

		case 5:
		ret = ifx_sw_read(ADM_SW_PORT5_RXCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT5_RXCNTH, &val)<<16;
		break;
		
	}		
  return ret;
}



unsigned int ifx_sw_get_TXCNT(int port)
{
  unsigned int val;
  unsigned int ret;

	switch(port)
	{
		case 0:
		ret =  ifx_sw_read(ADM_SW_PORT0_TXCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT0_TXCNTH, &val) << 16;		
		break;

		case 1:
		ret  = ifx_sw_read(ADM_SW_PORT1_TXCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT1_TXCNTH, &val) << 16;
		break;

		case 2:
		ret = ifx_sw_read(ADM_SW_PORT2_TXCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT2_TXCNTH, &val) << 16;
		break;
		
		case 3:
		ret = ifx_sw_read(ADM_SW_PORT3_TXCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT3_TXCNTH, &val) << 16;
		break;

		case 4:
		ret = ifx_sw_read(ADM_SW_PORT4_TXCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT4_TXCNTH, &val) << 16;;
		break;

		case 5:
		ret = ifx_sw_read(ADM_SW_PORT5_TXCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT5_TXCNTH, &val) << 16;
		break;
		
	}		
  return ret;
}



unsigned int ifx_sw_get_COLCNT(int port)
{
  unsigned int val, ret;

	switch(port)
	{
		case 0:
		ret = ifx_sw_read(ADM_SW_PORT0_COLCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT0_COLCNTH, &val) << 16;
		break;

		case 1:
		ret = ifx_sw_read(ADM_SW_PORT1_COLCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT1_COLCNTH, &val) << 16;
		break;

		case 2:
		ret = ifx_sw_read(ADM_SW_PORT2_COLCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT2_COLCNTH, &val) << 16;
		break;
		
		case 3:
		ret = ifx_sw_read(ADM_SW_PORT3_COLCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT3_COLCNTH, &val) << 16;
		break;

		case 4:
		ret = ifx_sw_read(ADM_SW_PORT4_COLCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT4_COLCNTH, &val) << 16;
		break;

		case 5:
		ret = ifx_sw_read(ADM_SW_PORT5_COLCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT5_COLCNTH, &val) << 16;;
		break;		
	}		
  return ret;
}


unsigned int ifx_sw_get_ECL(int port)
{
  unsigned int val, ret;

	switch(port)
	{
		case 0:
		ret = ifx_sw_read(ADM_SW_PORT0_ERRCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT0_ERRCNTH, &val) << 16;
		break;

		case 1:
		ret = ifx_sw_read(ADM_SW_PORT1_ERRCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT1_ERRCNTH, &val) << 16;
		break;

		case 2:
		ret = ifx_sw_read(ADM_SW_PORT2_ERRCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT2_ERRCNTH, &val) << 16;
		break;
		
		case 3:
		ret = ifx_sw_read(ADM_SW_PORT3_ERRCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT3_ERRCNTH, &val) << 16;
		break;

		case 4:
		ret = ifx_sw_read(ADM_SW_PORT4_ERRCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT4_ERRCNTH, &val) << 16;
		break;

		case 5:
		ret = ifx_sw_read(ADM_SW_PORT5_ERRCNTL, &val) ;//+ ifx_sw_read(ADM_SW_PORT5_ERRCNTH &val) << 16;
		break;

		
	}		
  return ret;
}


/*
 * Set MAC proirity
 * 
 */
void adm6996_set_mac_priority(char *mac, int addmac, int pri)
{
}


/*
 * enable port bandwidth control. BIT_12 is to enable the new bandwidth control
 * port : port number
 * txrx : tx -> 1, rx -> 0
 */
void ifx_sw_ena_bndctl(int port, int txrx)
{
  unsigned int val;

  if (port < 3)
  	ifx_sw_write(ADM_SW_BNDWTH_CTL_ENA, \
  		ifx_sw_read(ADM_SW_BNDWTH_CTL_ENA, &val) | ((1 << port*2) << txrx) | BIT_12 );
  else{
  	if (txrx)	/* TX */
  		ifx_sw_write(ADM_SW_BNDWTH_CTL_ENA, \
  			ifx_sw_read(ADM_SW_BNDWTH_CTL_ENA, &val) | ((1 << 9 << (port-3)))| BIT_12  );
  	else
  		ifx_sw_write(ADM_SW_BNDWTH_CTL_ENA, \
  			ifx_sw_read(ADM_SW_BNDWTH_CTL_ENA, &val) | ((1 << 6 << (port-3)))| BIT_12  );
  }
}


/*
 * disable port bandwidth control
 * port : port number
 * txrx : tx -> 1, rx -> 0
 */
void ifx_sw_dis_bndctl(int port, int txrx)
{
  unsigned int val;
  if (port < 3)
  	ifx_sw_write(ADM_SW_BNDWTH_CTL_ENA, \
  		ifx_sw_read(ADM_SW_BNDWTH_CTL_ENA, &val) & ~((1 << port*2) << txrx) );
  else{
  	if (txrx)	/* TX */
  		ifx_sw_write(ADM_SW_BNDWTH_CTL_ENA, \
  			ifx_sw_read(ADM_SW_BNDWTH_CTL_ENA, &val) & ~((1 << 9 << (port-3))) );
  	else
  		ifx_sw_write(ADM_SW_BNDWTH_CTL_ENA, \
  			ifx_sw_read(ADM_SW_BNDWTH_CTL_ENA, &val) & ~((1 << 6 << (port-3))) );
  }
}

/* get port bandwidth
 *
 */
int ifx_sw_get_bndwth(int port, int txrx)
{
  int bandwidth = 0;
  unsigned int val;
  
  if (txrx)	/* TX */
  {
  	if (port < 2)
  	{
  		bandwidth = \
  			(((ifx_sw_read(ADM_SW_BNDWTH_CTL1, &val) >> (port*4+8)) & 0xf) + \
  			 (((ifx_sw_read(ADM_SW_BNDWTH_EXTCTL2, &val) >> (port*4+8)) & 0xf)<<4) + \
  			 (((ifx_sw_read(ADM_SW_BNDWTH_EXTCTL5, &val) >> (port*3+3)) & 0x7)<<8))*64;
  	}else
  	{
  		bandwidth = \
  			(((ifx_sw_read(ADM_SW_BNDWTH_EXTCTL0, &val) >> ((port-2)*4)) & 0xf) + \
  			 (((ifx_sw_read(ADM_SW_BNDWTH_EXTCTL3, &val) >> ((port-2)*4)) & 0xf)<<4)) * 64;
  		if (port < 4)
  			bandwidth += (((ifx_sw_read(ADM_SW_BNDWTH_EXTCTL5, &val) >> (port*3+3)) & 0x7)<<8)*64;
  		else
  			bandwidth += (((ifx_sw_read(ADM_SW_BNDWTH_EXTCTL6, &val) >> ((port-4)*3)) & 0x7)<<8)*64;
  	}
  }else
  {
  	if (port < 4)
  	{
  		bandwidth =
  			(((ifx_sw_read(ADM_SW_BNDWTH_CTL0, &val) >> (port*4)) & 0xf) + \
  			 (((ifx_sw_read(ADM_SW_BNDWTH_EXTCTL1, &val) >> (port*4)) & 0xf)<<4) + \
  			 (((ifx_sw_read(ADM_SW_BNDWTH_EXTCTL4, &val) >> (port*3)) & 0x7)<<8))*64;
  	}else
  	{
  		if (port == 4)
  			bandwidth =
  			((ifx_sw_read(ADM_SW_BNDWTH_CTL1, &val) & 0xf) + \
  			 ((ifx_sw_read(ADM_SW_BNDWTH_EXTCTL2, &val) & 0xf) << 4) + \
  			 (((ifx_sw_read(ADM_SW_BNDWTH_EXTCTL4, &val) >> (port*3)) & 0x7)<<8))*64;
  		else	/* port 5 */
			bandwidth =
  			(((ifx_sw_read(ADM_SW_BNDWTH_CTL1, &val) >> 4) & 0xf) + \
  			 (((ifx_sw_read(ADM_SW_BNDWTH_EXTCTL2, &val) >> 4) & 0xf)<<4) + \
  			 ((ifx_sw_read(ADM_SW_BNDWTH_EXTCTL5, &val) & 0x7)<<8))*64;
  	}
  }
  
  return bandwidth;
}








/* set port bandwidth
 *
 */
void ifx_sw_set_bndwth(int port, int txrx, int limit)
{
  int reg1, reg2, reg3; 
  int limit64k; 
  unsigned int val;

  if ((limit <= 0) || (limit >= 102400))
  {
  	ifx_sw_dis_bndctl(port, txrx);
  	return;
  }
    
  limit64k = limit / 64; 
  
  reg1 = limit64k & 0xf; 	/* bit 0~3 */
  reg2 = (limit64k >> 4) & 0xf;	/* bit 4~7 */
  reg3 = (limit64k >> 8) & 0x7; /* bit 10~8 */
  
  ifx_sw_ena_bndctl(port, txrx);
  
  ifx_printf(("Limit port %d %s to %d Kbps \n", port, txrx ? "TX" : "RX", limit64k*64));
  
  if (txrx)	/* TX */
  {
  	switch (port)
  	{
  		case 0:
  		case 1:
  			ifx_sw_write(ADM_SW_BNDWTH_CTL1, \
  				(ifx_sw_read(ADM_SW_BNDWTH_CTL1, &val) & ~(0xf << (port*4+8))) | (reg1 << (port*4+8)));
  			ifx_sw_write(ADM_SW_BNDWTH_EXTCTL2, \
  				(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL2, &val) & ~(0xf << (port*4+8))) | (reg2 << (port*4+8)));
  			ifx_sw_write(ADM_SW_BNDWTH_EXTCTL5, \
  				(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL5, &val) & ~(0x7 << (port*3+3))) | (reg3 << (port*3+3)));
  			break;
  		case 2:
  		case 3:
  			ifx_sw_write(ADM_SW_BNDWTH_EXTCTL0, \
  				(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL0, &val) & ~((int)0xf<<((port-2)*4))) | (reg1 << (port-2)*4));
  			ifx_sw_write(ADM_SW_BNDWTH_EXTCTL3, \
  				(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL3, &val) & ~((int)0xf<<((port-2)*4))) | (reg2 << (port-2)*4));
  			ifx_sw_write(ADM_SW_BNDWTH_EXTCTL5, \
  				(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL5, &val) & ~((int)0x7<<(port*3+3))) | (reg3 << (port*3+3)));
  			break;
  		case 4:
  		case 5:
  			ifx_sw_write(ADM_SW_BNDWTH_EXTCTL0, \
  				(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL0, &val) & ~(0xf<<((port-2)*4))) | (reg1 << (port-2)*4) );
  			ifx_sw_write(ADM_SW_BNDWTH_EXTCTL3, \
  				(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL3, &val) & ~(0xf<<((port-2)*4))) | (reg2 << (port-2)*4) );
  			ifx_sw_write(ADM_SW_BNDWTH_EXTCTL6, \
  				(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL6, &val) & ~(0x7<<((port-4)*3))) | (reg3 << (port-4)*3) );
  			break;
  		
  		default:
  			ifx_printf(("WRONG port number! (ifx_sw_set_bndwth: %s)\n", txrx ? "TX":"RX"));
  			break; 
  	}
  }else
  {
  	switch (port)
  	{
  		case 0:
  		case 1:
  		case 2:
  		case 3:
  			ifx_sw_write(ADM_SW_BNDWTH_CTL0, \
  				(ifx_sw_read(ADM_SW_BNDWTH_CTL0, &val) & ~(0xf<<(port*4))) | (reg1 << (port*4)) );
  			ifx_sw_write(ADM_SW_BNDWTH_EXTCTL1, \
  				(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL1, &val) & ~(0xf<<(port*4))) | (reg2 << (port*4)) );
  			ifx_sw_write(ADM_SW_BNDWTH_EXTCTL4, \
  				(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL4, &val) & ~(0x7<<(port*3))) | (reg3 << (port*3)) );
  			break;
  		case 4:
  		case 5:
  			ifx_sw_write(ADM_SW_BNDWTH_CTL1, \
  				(ifx_sw_read(ADM_SW_BNDWTH_CTL1, &val) & ~(0xf<<((port-4)*4))) | (reg1 << (port-4)*4) );
  			ifx_sw_write(ADM_SW_BNDWTH_EXTCTL2, \
  				(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL2, &val) & ~(0xf<<((port-4)*4))) | (reg2 << (port-4)*4) );
  			if (port == 4)
  				ifx_sw_write(ADM_SW_BNDWTH_EXTCTL4, \
  					(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL4, &val) & ~(0x7<<(port*3))) | (reg3 << (port*3)) );
  			else
  				ifx_sw_write(ADM_SW_BNDWTH_EXTCTL5, \
  					(ifx_sw_read(ADM_SW_BNDWTH_EXTCTL5, &val) & ~(0x7)) | reg3);
  			break;
  		default:
  			ifx_printf(("WRONG port number! (ifx_sw_set_bndwth: %s)\n", txrx ? "TX": "RX"));
  			break; 
  	}
  } 
  
  return ; 
}

/*
 * get TCP/UDP filter information
 * filterno -> Filter number, 0~7
 * port -> TCP/UDP service port number
 * queueno -> switch queue number
 * return value -> is the filter set? 1: yes, 0: no
 */
int ifx_sw_get_tcpudp_filter(int filterno, int *port, int *queueno)
{
  unsigned int val;
  *port = ifx_sw_read(ADM_SW_TCPUDP_FILTER0+filterno, &val);
    
  switch (filterno)
  {
  	case 0:
    	case 1:
    	case 2:
    	case 3:
    		*queueno = (ifx_sw_read(ADM_SW_TCPUDP_ACT0, &val) >> (filterno*4)) & 0x3;
    		break;
    	case 4:
    	case 5:
    	case 6:
    	case 7:
    		*queueno = (ifx_sw_read(ADM_SW_TCPUDP_ACT1, &val) >> ((filterno-4)*4)) & 0x3;
    		break;
  }
  
  return ((ifx_sw_read(ADM_SW_TCPUDP_ACT2, &val) >> filterno) & 0x1); 
}

/*
 * set TCP/UDP filter information
 * filterno -> Filter number, 0~7
 * port -> TCP/UDP service port number
 * queueno -> switch queue number
 */
void ifx_sw_set_tcpudp_filter(int filterno, int port, int queueno)
{
  unsigned int val;
  /* disable this filter, if serice port = 0! */
  if (port == 0)
  {
  	ifx_printf(("Disabling filter %d... \n", filterno)); 

  	ifx_sw_write(ADM_SW_TCPUDP_ACT2, \
  		ifx_sw_read(ADM_SW_TCPUDP_ACT2, &val) & ~(1 << filterno));
  	
  	return; 
  }
  
  ifx_printf(("Setting filter %d of port %d to queue %d... \n", \
  	filterno, port, queueno)); 

  /* set service port number */
  ifx_sw_write(ADM_SW_TCPUDP_FILTER0 + filterno, port);
  
  /* set priority queue */
  if (filterno < 3)
  {
  	ifx_sw_write(ADM_SW_TCPUDP_ACT0, \
  		(ifx_sw_read(ADM_SW_TCPUDP_ACT0, &val) & ~(0x3<<(filterno*4))) | (queueno << (filterno*4)));
  }else
  {
	ifx_sw_write(ADM_SW_TCPUDP_ACT1, \
  		(ifx_sw_read(ADM_SW_TCPUDP_ACT1, &val) & ~(0x3<<((filterno-4)*4))) | (queueno << ((filterno-4)*4)));
  }
  
  /* set tcp/udp filter enable */
  ifx_sw_write(ADM_SW_TCPUDP_ACT2, \
  	ifx_sw_read(ADM_SW_TCPUDP_ACT2, &val) | (1 << filterno));
}

/*
 * set QoS control flag. New bandwidth control enable bit @ 0x33h
 * qos -> 1: enable, 0: disable
 */
void ifx_sw_set_qos_control(int qos)
{
  unsigned int val;
  ifx_sw_write(ADM_SW_BNDWTH_CTL_ENA, \
  	(ifx_sw_read(ADM_SW_BNDWTH_CTL_ENA, &val) & ~BIT_12) | (qos << 12));
}

/*
 * get QoS control flag
 * return value -> 1: enable, 0: disable
 */
int ifx_sw_get_qos_control(void)
{
  unsigned int val;
  return ( (ifx_sw_read(ADM_SW_BNDWTH_CTL_ENA, &val) & BIT_12) ? 1 : 0); 
}


/*
 * set port prioroty queue 
 * port: port number
 * qno: queue priority number (0: low, 1: mid, 2: high, 3: highest)
 */
void ifx_sw_setq(int port, int qno)
{
    unsigned int val;
  ifx_sw_write(ifx_sw_conf[port], \
  	(ifx_sw_read(ifx_sw_conf[port], &val) & ~(0x3 << 8)) | (qno << 8));
}

/*
 * get port priority queue 
 */
int ifx_sw_getq(int port)
{
  unsigned int val;
  return ( (ifx_sw_read(ifx_sw_conf[port], &val) >> 8) & 0x3);
}
/*
 * set switch queue weight
 * 
 */
void ifx_sw_set_queue_weight(int qno, int weight)
{
  unsigned int val;
  if (qno == 0) return; 
  
  ifx_printf(("Set queue %d to priority %d\n", qno, weight));
  
  ifx_sw_write(ADM_SW_QUEUE_CONF1 + qno - 1, \
  	(ifx_sw_read(ADM_SW_QUEUE_CONF1 + qno - 1, &val) & ~(0xf << 12)) | (weight << 12));
}


/*
 * get network queue weight
 * return value -> 1: enable, 0: disable
 */
int ifx_sw_get_queue_weight(int qno)
{
  unsigned int val;
  if (qno == 0) return 0;
  
  return ( ((ifx_sw_read(ADM_SW_QUEUE_CONF1 + qno - 1, &val) >> 12) & 0xf)); 
}



/*
 * enable port flow control
 */
void ifx_sw_ena_fc(int port)
{
  unsigned int val;
  ifx_printf(("Enable flow control of port %d\n", port));
  
  ifx_sw_write(ifx_sw_conf[port], \
  	ifx_sw_read(ifx_sw_conf[port], &val) | BIT_0);
}

/*
 * disable port flow control
 */
void ifx_sw_dis_fc(int port)
{
  unsigned int val;
  ifx_printf(("Disable flow control of port %d\n", port));
  
  ifx_sw_write(ifx_sw_conf[port], \
  	ifx_sw_read(ifx_sw_conf[port], &val) & ~BIT_0);
}





int ifx_sw_get_portsts(int port, int status)
{
  unsigned int val;
  switch (port)
  {
  	case 0:
  		return (ifx_sw_read(ADM_SW_PORTSTS0, &val) & status);
  		break;
  	case 1:
  		return ((ifx_sw_read(ADM_SW_PORTSTS0, &val) >> 8) & status);
  		break;
  	
  	case 2:
  		return (ifx_sw_read(ADM_SW_PORTSTS1, &val) & status);
  		break;
  	
  	case 3:
  		return ((ifx_sw_read(ADM_SW_PORTSTS1, &val) >> 8) & status);
  		break;
  	
  	case 4:
  		return ((ifx_sw_read(ADM_SW_PORTSTS1, &val) >> 12) & status);
  		break;
  		
  	case 5:
  		if (status < 2)
  			return (ifx_sw_read(ADM_SW_PORTSTS2, &val) & status);
  		else
  			return ((ifx_sw_read(ADM_SW_PORTSTS2, &val)>>1) & status);
  			
  		break;
  	
  	default:
  		break;
  }
  
  return 0;
}

/*
 * check port bandwidth control enable
 * port : port number
 * txrx : tx -> 1, rx -> 0
 */
int ifx_sw_chk_bndctl(int port, int txrx)
{
  unsigned int val;
//printk("ifx_sw_chk_bndctl:%x\n", ADM_SW_BNDWTH_CTL_ENA >> (port*2)) >> txrx) & 1);
#if 1
  if (port < 3)
  	return ((((ifx_sw_read(ADM_SW_BNDWTH_CTL_ENA, &val)) >> (port*2)) >> txrx) & 1);
  else{
  	if (txrx)	/* TX */
  		return (((ifx_sw_read(ADM_SW_BNDWTH_CTL_ENA, &val) >> 9) >> (port-3)) & 1);
  	else
  		return (((ifx_sw_read(ADM_SW_BNDWTH_CTL_ENA, &val) >> 6) >> (port-3)) & 1);
  }
 #endif 
}


static struct proc_dir_entry * proc_adm6996_root = NULL;

/*
 * count the length of the input parameter from user space
 * return value: the length of characters that is input from the user
 *               command line
 */
static int strn_len(const char *user_buffer, unsigned int maxlen)
{
	int i = 0;

	for(; i < maxlen; i++) {
		char c;

		if (get_user(c, &user_buffer[i]))
			return -EFAULT;
		switch (c) {
		case '\"':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
		case ',':
			goto done_str;
		default:
			break;
		};
	}
done_str:
	return i;
}

/* 
 * count the space or other special characters seperated bewteen two
 * command options from the user space
 * return value: length of those seperatation characters (commonly spaces)
 */
static int count_trail_chars(const char *user_buffer, unsigned int maxlen)
{
	int i;

	for (i = 0; i < maxlen; i++) {
		char c;

		if (get_user(c, &user_buffer[i]))
			return -EFAULT;
		switch (c) {
		case '\"':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
		case '=':
		case ',':
			break;
		default:
			goto done;
		};
	}
done:
	return i;
}

/*
 * initialize the proc file system and make a dir named /proc/adm6996
 */
void ifx_sw_proc_init(void)
{
	proc_adm6996_root = proc_mkdir("adm6996", 0);
	if (!proc_adm6996_root)
	{
		printk("ADM6996 proc initialization failed! \n");
		return;
	}
	printk("ADM6996 proc initialization okay! \n");
}

/* 
 * proc file system add function for adm6996
 */
int ifx_sw_proc_addproc(char *funcname, read_proc_t *hookfuncr, write_proc_t *hookfuncw)
{
	struct proc_dir_entry *pe;
	
	if (!proc_adm6996_root)
		ifx_sw_proc_init();
	
	if (hookfuncw == NULL)
	{
		pe = create_proc_read_entry(funcname, 0, proc_adm6996_root, hookfuncr, NULL);
		if (!pe)
		{
			printk("ERROR in creating read proc entry (%s)! \n", funcname);
			return -1;
		}
	}else
	{
		pe = create_proc_entry(funcname, S_IRUGO | S_IWUGO, proc_adm6996_root);
		if (pe)
		{
			pe->read_proc = hookfuncr;
			pe->write_proc = hookfuncw;
		}else
		{
			printk("ERROR in creating proc entry (%s)! \n", funcname);
			return -1;
		}
	}
	
	return 0;
}

/*
 * bandwidth control (read from user space)
 * command line: echo "RX|TX (value0) (value1) (value2) (value3) (value4) (value5)"
 *               > /proc/adm6996/bandwidth_control
 */
static int ifx_sw_bdhctl_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
  char buf[80], *ptr; 
  int len, i = 0, port, txrx, speed;

  /* read the mode, "RX" & "TX" */
  len = strn_len(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  	
  if (copy_from_user(buf, &buffer[i], len))
  	return -EFAULT;
  buf[len] ='\0';
  i += len;
  
  if (strstr(buf, "TX")) txrx = 1;
  else if (strstr(buf, "RX")) txrx = 0;
  else return len;
  
  len = count_trail_chars(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  i += len;
  
  for (port = 0; port <= ADM_SW_MAX_PORT_NUM; port++)
  {
  	
  	len = strn_len(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	
  	if (copy_from_user(buf, &buffer[i], len))
  		return -EFAULT;
  	buf[len] ='\0';
  	i += len;
  	
  	speed = simple_strtoul(buf, &ptr, 10);
  	
  	len = count_trail_chars(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	i += len;
  	
  	/* set bandwidth control registers! */
  	ifx_sw_set_bndwth(port, txrx, speed);
  }
  
  return count;
}

/*
 * bandwidth control (output status)
 */                                                                                                    
int ifx_sw_bdhctl_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
  int len = 0, port;
  int test;

unsigned int chkdata1=0;
  
  len += sprintf(buf+len, "---=== Bandwidth status ===---\n");
  len += sprintf(buf+len, "\tRX\tTX\tValue\n");
  
 
#if 1
 for (port = 0; port <= ADM_SW_MAX_PORT_NUM; port++)  
 {
  	len += sprintf(buf+len, "Port %d\t%c\t%c\t%d\t%d\n", port, \
  		ifx_sw_chk_bndctl(port, 0) ? '*' : '-', \
  		ifx_sw_chk_bndctl(port, 1) ? '*' : '-', \
  		ifx_sw_get_bndwth(port, 0), ifx_sw_get_bndwth(port, 1));
  }
 #endif
  
  return len;
}

/*
 * TCP/UDP control (service port control) (read from user space)
 * command line: 
 */
static int ifx_sw_tcpudpctl_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
  char buf[80], *ptr; 
  int len, i = 0, port, fn, qno;

  /* read the filter number, "F#" */
  len = strn_len(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  	
  if (copy_from_user(buf, &buffer[i], len))
  	return -EFAULT;
  buf[len] ='\0';
  i += len;
  
  fn = buf[len-1] - '0';
  
  len = count_trail_chars(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  i += len;
  
  /* read the service port number */
  len = strn_len(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  	
  if (copy_from_user(buf, &buffer[i], len))
  	return -EFAULT;
  buf[len] ='\0';
  i += len;
  
  port = simple_strtoul(buf, &ptr, 10);
  
  len = count_trail_chars(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  i += len;
  
  /* read the queue number */  
  len = strn_len(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  	
  if (copy_from_user(buf, &buffer[i], len))
  	return -EFAULT;
  buf[len] ='\0';
  i += len;
  
  qno = simple_strtoul(buf, &ptr, 10);
  
  len = count_trail_chars(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  i += len;
  
  ifx_sw_set_tcpudp_filter(fn, port, qno);
    
  return count;
}

/*
 * TCP/UDP control (service port control) (output status)
 */                                                                                                    
int ifx_sw_tcpudpctl_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
  int len = 0, port = 0, queueno = 0, i, isset;
  
  len += sprintf(buf+len, "---=== TCP/UDP control status ===---\n");
  len += sprintf(buf+len, "\t\tSET\tPORT\tQUEUE\n");
  for (i = 0; i < ADM_SW_TCPUDP_MAX; i++)
  {
  	isset = ifx_sw_get_tcpudp_filter(i, &port, &queueno);
  	len += sprintf(buf+len, "Filter #%d\t%c\t%d\t%d\n", i, \
  		isset ? '*':'-', port, queueno);
  }
  
  return len;
}


/*
 * QoS control (read from user space)
 */
static int ifx_sw_qosctl_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
  char buf[80];  
  int len, i = 0, qos; 

  /* read from the user space, 1 -> enable, 0 -> disable */
  len = strn_len(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  	
  if (copy_from_user(buf, &buffer[i], len))
  	return -EFAULT;
  buf[len] ='\0';
  i += len;
  
  if (strchr(buf, '1')) qos = 1;
  else qos = 0;
  
  ifx_sw_set_qos_control(qos);
  
  return count;
}

/*
 * QoS control (output status)
 */                                                                                                    
int ifx_sw_qosctl_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
  int len = 0; 
  
  len += sprintf(buf+len, "---=== QoS control status ===---\n");
  len += sprintf(buf+len, "%d (%s)\n", ifx_sw_get_qos_control(), \
  	ifx_sw_get_qos_control()?"Enabled":"Disabled"); 
  	
  return len;
}

/*
 * Queue control (read from user space)
 */
static int ifx_sw_qctl_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
  char buf[80], *ptr;  
  int len, i = 0, n, weight; 

  /* read from the user space, 4 numbers for Q0, Q1, Q2, Q3 priority */
  for (n = 0; n < 4; n++)
  {
  	len = strn_len(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	
  	if (copy_from_user(buf, &buffer[i], len))
  		return -EFAULT;
  	buf[len] ='\0';
  	i += len;
  	weight = simple_strtoul(buf, &ptr, 10);
  	
  	ifx_sw_set_queue_weight(n, weight);
  	
  	len = count_trail_chars(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	i += len;
  }
  
  return count;
}

/*
 * Queue control (output status)
 */                                                                                                    
int ifx_sw_qctl_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
  int len = 0, q; 
  
  len += sprintf(buf+len, "---=== Queue weight status ===---\n");
  for (q=0; q<4; q++)
  	len += sprintf(buf+len, "Queue %d - %d\n", q, ifx_sw_get_queue_weight(q)); 
  	
  return len;
}

/*
 * Port control (read from user space)
 */
static int ifx_sw_pctl_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
  char buf[80], *ptr;  
  int len, i = 0, n, value, ppfc; 

  len = strn_len(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  if (copy_from_user(buf, &buffer[i], len))
  		return -EFAULT;
  buf[len] ='\0';
  i += len;
  
  if (strstr(buf, "PP")) ppfc = 1;	/* set port priority */
  else if (strstr(buf, "FC")) ppfc = 0;	/* set flow control bit */
  else return len;
  
  len = count_trail_chars(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  i += len;
  
  /* read from the user space, 4 numbers for Q0, Q1, Q2, Q3 priority */
  for (n = 0; n <= ADM_SW_MAX_PORT_NUM; n++)
  {
  	len = strn_len(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	
  	if (copy_from_user(buf, &buffer[i], len))
  		return -EFAULT;
  	buf[len] ='\0';
  	i += len;
  	value = simple_strtoul(buf, &ptr, 10);
  	
  	if (ppfc)	/* set port priority */
  	{
  		ifx_sw_setq(n, value);
  	}else		/* set flow control bit */
  	{
  		if (value)
  			ifx_sw_ena_fc(n);	/* enable flow control */
  		else
  			ifx_sw_dis_fc(n);	/* disable flow control */
  	}
  	
  	len = count_trail_chars(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	i += len;
  }
  
  return count;
}


/* 
 * report port status 
 */
int ifx_sw_pctl_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int len = 0, port, bit = 8;
	
	len += sprintf(buf+len, "---=== Port Status ===--- \n");
	len += sprintf(buf+len, "\t\tFC\tFD\t100M\tLinked\tQueue\n");
	for (port = 0; port <= ADM_SW_MAX_PORT_NUM; port ++)
	{
		bit = 8;
		len += sprintf(buf+len, "Port %d: ", port);
		while (bit >= 1)
		{
			if (ifx_sw_get_portsts(port, bit)) 
				len += sprintf(buf+len, "\t*");
			else
				len += sprintf(buf+len, "\t");
			
			bit >>= 1;
		}
		len += sprintf(buf+len, "\t%d\n", ifx_sw_getq(port));
	}

	for (port = 0; port <= ADM_SW_MAX_PORT_NUM; port ++)	
		len += sprintf(buf+len, "port :%x RX:%x TX:%x Col:%x Err:%x\n", port, ifx_sw_get_RXCNT(port), ifx_sw_get_TXCNT(port), ifx_sw_get_COLCNT(port), ifx_sw_get_ECL(port));
	
	len += sprintf(buf+len, "\n");
	*eof = 1;
	return len;
}

/*
 * IGMP snooping control (read from user space)
 */
static int ifx_sw_igmpctl_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
  char buf[80];  
  int len, i = 0; 

  /* read from the user space, 
  	1 -> snooping enable, 0 -> snopping disable 
  	PROXY 1 -> proxy enable, PROXY 0 -> proxy disable */
  
  len = strn_len(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  
  if (copy_from_user(buf, &buffer[i], len))
  	return -EFAULT;
  buf[len] ='\0';
  i += len;
  
  if (strstr(buf, "PROXY")) 
  {
  	len = count_trail_chars(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	i += len;
  	
  	len = strn_len(&buffer[i], sizeof(buf));
	  if (len < 0) return len;
	  	
	  if (copy_from_user(buf, &buffer[i], len))
	  	return -EFAULT;
	  buf[len] ='\0';
	  i += len;
  
  	ifx_sw_set_igmp_proxy(buf[len-1] - '0'); 
  }
  else if(strstr(buf, "AD_SNOOP"))
  {
	len = count_trail_chars(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	i += len;
  	
  	len = strn_len(&buffer[i], sizeof(buf));
	  if (len < 0) return len;
	  	
	  if (copy_from_user(buf, &buffer[i], len))
	  	return -EFAULT;
	  buf[len] ='\0';
	  i += len;
  
  	ifx_sw_set_additional_snoop_ctr(buf[len-1] - '0'); 
  
  }
  else
  {  
	  ifx_sw_set_igmp(buf[len-1]-'0');
  }
    
  return count;
}

/*
 * IGMP snooping control (output status)
 */                                                                                                    
int ifx_sw_igmpctl_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
  int len = 0; 
  int i;
  //IGMPMEMBERTRY entry;
  len += sprintf(buf+len, "---=== IGMP control status ===---\n");
  len += sprintf(buf+len, "Snooping: %d (%s)\n", ifx_sw_get_igmp(), \
  	ifx_sw_get_igmp()?"Enabled":"Disabled"); 

  len += sprintf(buf+len, "Additional Snooping Control: %d \n", ifx_sw_get_additional_snoop_ctr()); 
  
  
  len += sprintf(buf+len, "Proxy: %d (%s)\n", ifx_sw_get_igmp_proxy(), \
  	ifx_sw_get_igmp_proxy()?"Enabled":"Disabled"); 

  return len;
}



// vlan control

enum {
  ifx_vlan_set_mode = 1,
  ifx_vlan_set_cpu_port = 2,	
  ifx_vlan_set_filter_add = 3,
  ifx_vlan_set_filter_del = 4
};


 /*
   echo "mode [0|1]" >vlan_control
   echo "cpu    [0|1|2|3|4|5|disable]" >vlan_control
   echo "filter_add [0|1|...|15] [0|1|2|3|4|5]" >vlan_control
   echo "filter_del [0|1|...|15] [0|1|2|3|4|5]" >vlan_control
   
*/
static int ifx_sw_vlanctl_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
  char buf[80];  
  int len, i = 0, setting_type = 0; 

  /* read from the user space, 
  	1 -> snooping enable, 0 -> snopping disable 
  	PROXY 1 -> proxy enable, PROXY 0 -> proxy disable */
  
  len = strn_len(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  
  if (copy_from_user(buf, &buffer[i], len))
  	return -EFAULT;
  buf[len] ='\0';
  i += len;


  if(strstr(buf, "mode"))
  	setting_type = ifx_vlan_set_mode;
  else if(strstr(buf, "cpu"))
  	setting_type = ifx_vlan_set_cpu_port;
  else if(strstr(buf,"filter_add"))
  	setting_type = ifx_vlan_set_filter_add;
  else if(strstr(buf,"filter_del"))
  	setting_type = ifx_vlan_set_filter_del;
  
  
  	len = count_trail_chars(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	i += len;
  	
  	len = strn_len(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	
  	if (copy_from_user(buf, &buffer[i], len))
  		return -EFAULT;
  	buf[len] ='\0';
  	i += len;



	switch(setting_type)
	{
		case ifx_vlan_set_mode:
			ifx_sw_set_vlanmode(buf[len-1] - '0');
			break;

		case ifx_vlan_set_cpu_port:
		  	if(strstr(buf, "disable"))
				ifx_sw_disable_cpu_port();
			else
		  		ifx_sw_set_cpu_port(buf[len-1] - '0'); 
			break;
		case ifx_vlan_set_filter_add:
		case ifx_vlan_set_filter_del:
			{
				int filter_id,idx;
				filter_id = buf[len-1] - '0';

				//parse the members
				 for (idx = 0; idx < ADM_SW_MAX_PORT_NUM +1 ; idx++)
  				{

					len = count_trail_chars(&buffer[i], sizeof(buf));
				  	if (len < 0) 
						return len;

					//no more parameter
					if(buffer[i] == 0 || buffer[i+1] == 0)
						break;
					
					i += len;						
				  	len = strn_len(&buffer[i], sizeof(buf));

					if (len < 0)
				  	{				  		
						return len;
				  	}
					
				  	if (copy_from_user(buf, &buffer[i], len))
				  		return -EFAULT;
				  	buf[len] ='\0';
				  	i += len;
				  	
#if 0
					if(setting_type == ifx_vlan_set_filter_add)
						ifx_sw_vlan_add(buf[len-1] - '0', filter_id);
				  	else
						ifx_sw_vlan_del(buf[len-1] - '0', filter_id);
#endif				  	
				  }
  
				
			}
			
			break;
			
		default:
			printk("wrong type.....\n");
			
	}


  
    
  return count;
}

/*
 * IGMP snooping control (output status)
 */                                                                                                    
int ifx_sw_vlanctl_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
  int len = 0; 
  unsigned int i, val;
  int cpu_port;
  
  len += sprintf(buf+len, "---=== VLAN control status ===---\n");
  len += sprintf(buf+len, "vlan mode: %d (%s)\n", ifx_sw_get_vlanmode(), \
  	ifx_sw_get_vlanmode()?"tagged vlan":"port vlan"); 

  cpu_port = ifx_sw_get_cpu_port();

  if(cpu_port == 7)
    len += sprintf(buf+len, "cpu port: not exist \n");	
  else	
    len += sprintf(buf+len, "cpu port: %x \n", cpu_port);


  len += sprintf(buf+len, "---=== port lan status ===---\n");

  for(i=0; i<ADM_SW_MAX_VLAN_NUM; i++)
  {
  	len += sprintf(buf+len, "vlanfilter %d:%x %x\n", i,  ifx_sw_read(ADM_SW_VLAN_FILTER0_LOW+i*2, &val),ifx_sw_read(ADM_SW_VLAN_FILTER0_HIGH+i*2, &val));
  
  }
  return len;
}




/*
 * Device control (MAC control; read from user space)
 */
static int ifx_sw_devctl_write_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
  char buf[80], *ptr, mac[20]={0};
  int len, i = 0, n, value, ppfc, addmac = 0; 

  len = strn_len(&buffer[i], sizeof(buf));
  if (len < 0) return len;
  if (copy_from_user(buf, &buffer[i], len))
  		return -EFAULT;
  buf[len] ='\0';
  i += len;

  /* add/del MACs into control table */
  if (strstr(buf, "ADD") || strstr(buf, "DEL"))
  {
  	addmac = strstr(buf, "ADD") ? 1 : 0;
  	
  	len = count_trail_chars(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	i += len;
  	
  	len = strn_len(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	
  	if (copy_from_user(buf, &buffer[i], len))
  			return -EFAULT;
	buf[len] ='\0';
  	i += len;
  	
  	memcpy(mac, buf, len);
  	printk("len: %d, addmac: %d\n", len, addmac);
  	
  	len = count_trail_chars(&buffer[i], sizeof(buf));
  		if (len < 0) return len;
	  	i += len;
  	
  	len = strn_len(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	
  	if (copy_from_user(buf, &buffer[i], len))
  			return -EFAULT;
	buf[len] ='\0';
  	i += len;
  	
  	value = simple_strtoul(buf, &ptr, 10);
  	
  	printk("set MAC: %s as priority: %d\n", mac, value); 
  	
  	adm6996_set_mac_priority(mac, addmac, value);
  	
  }else		/* set flow control bit or port priority */
  {
	if (strstr(buf, "PP")) ppfc = 1;	/* set port priority */
	else if (strstr(buf, "FC")) ppfc = 0;	/* set flow control bit */
	else return len;
  
  	len = count_trail_chars(&buffer[i], sizeof(buf));
  	if (len < 0) return len;
  	i += len;
  
  	/* read from the user space, 4 numbers for Q0, Q1, Q2, Q3 priority */
  	for (n = 0; n <= ADM_SW_MAX_PORT_NUM; n++)
  	{
  		len = strn_len(&buffer[i], sizeof(buf));
	  	if (len < 0) return len;
  		
	  	if (copy_from_user(buf, &buffer[i], len))
  			return -EFAULT;
	  	buf[len] ='\0';
  		i += len;
	  	value = simple_strtoul(buf, &ptr, 10);
  	
  		if (ppfc)	/* set port priority */
	  	{
  			ifx_sw_setq(n, value);
	  	}else		/* set flow control bit */
  		{
	  		if (value)
  				ifx_sw_ena_fc(n);	/* enable flow control */
  			else
	  			ifx_sw_dis_fc(n);	/* disable flow control */
  		}
  	
	  	len = count_trail_chars(&buffer[i], sizeof(buf));
  		if (len < 0) return len;
	  	i += len;
	 }
  }
  
  return count;
}


/* 
 * report device control status (MAC control)
 */
int ifx_sw_devctl_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int len = 0, port, bit = 8;
	
	len += sprintf(buf+len, "---=== Port Status ===--- \n");
	len += sprintf(buf+len, "\t\tFC\tFD\t100M\tLinked\tQueue\n");
	for (port = 0; port <= ADM_SW_MAX_PORT_NUM; port ++)
	{
		bit = 8;
		len += sprintf(buf+len, "Port %d: ", port);
		while (bit >= 1)
		{
			if (ifx_sw_get_portsts(port, bit)) 
				len += sprintf(buf+len, "\t*");
			else
				len += sprintf(buf+len, "\t");
			
			bit >>= 1;
		}
		len += sprintf(buf+len, "\t%d\n", ifx_sw_getq(port));
	}
	
	*eof = 1;
	return len;
}
#endif


/*
  initialize GPIO pins.
  output mode, low
*/
void ifx_gpio_init(void)
{
 //GPIO16,17,18 direction:output
 //GPIO16,17,18 output 0
/* 
    AMAZON_SW_REG(AMAZON_GPIO_P1_DIR) |= (GPIO_MDIO|GPIO_MDCS|GPIO_MDC);
    AMAZON_SW_REG(AMAZON_GPIO_P1_OUT) =AMAZON_SW_REG(AMAZON_GPIO_P1_IN)& ~(GPIO_MDIO|GPIO_MDCS|GPIO_MDC);
*/
}

/* read one bit from mdio port */
int ifx_sw_mdio_readbit(void)
{
    //int val;

    //val = (AMAZON_SW_REG(GPIO_conf0_REG) & GPIO0_INPUT_MASK) >> 8;
    //return val;
    //GPIO16
    //return AMAZON_SW_REG(AMAZON_GPIO_P1_IN)&1;
    return 0;
}

/*
  MDIO mode selection
  1 -> output
  0 -> input

  switch input/output mode of GPIO 0
*/
void ifx_mdio_mode(int mode)
{
//    AMAZON_SW_REG(GPIO_conf0_REG) = mode ? GPIO_ENABLEBITS :
//                             ((GPIO_ENABLEBITS | MDIO_INPUT) & ~MDIO_OUTPUT_EN);
//    mode?(AMAZON_SW_REG(AMAZON_GPIO_P1_DIR)|=GPIO_MDIO):
//         (AMAZON_SW_REG(AMAZON_GPIO_P1_DIR)&=~GPIO_MDIO);
    /*int r=AMAZON_SW_REG(AMAZON_GPIO_P1_DIR);
    mode?(r|=GPIO_MDIO):(r&=~GPIO_MDIO);
    AMAZON_SW_REG(AMAZON_GPIO_P1_DIR)=r;*/
}

void ifx_mdc_hi(void)
{
    //GPIO_SET_HI(GPIO_MDC);
    //AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)|=GPIO_MDC;
    /*int r=AMAZON_SW_REG(AMAZON_GPIO_P1_OUT);
    r|=GPIO_MDC;
    AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)=r;*/

//    AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)=AMAZON_SW_REG(AMAZON_GPIO_P1_IN)|GPIO_MDC;
}

void ifx_mdio_hi(void)
{
    //GPIO_SET_HI(GPIO_MDIO);
    //AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)|=GPIO_MDIO;
    /*int r=AMAZON_SW_REG(AMAZON_GPIO_P1_OUT);
    r|=GPIO_MDIO;
    AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)=r;*/

  //  AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)=AMAZON_SW_REG(AMAZON_GPIO_P1_IN)|GPIO_MDIO;
}

void ifx_mdcs_hi(void)
{
    //GPIO_SET_HI(GPIO_MDCS);
    //AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)|=GPIO_MDCS;
    /*int r=AMAZON_SW_REG(AMAZON_GPIO_P1_OUT);
    r|=GPIO_MDCS;
    AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)=r;*/

    //AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)=AMAZON_SW_REG(AMAZON_GPIO_P1_IN)|GPIO_MDCS;
}

void ifx_mdc_lo(void)
{
    //GPIO_SET_LOW(GPIO_MDC);
    //AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)&=~GPIO_MDC;
    /*int r=AMAZON_SW_REG(AMAZON_GPIO_P1_OUT);
    r&=~GPIO_MDC;
    AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)=r;*/

    //AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)=AMAZON_SW_REG(AMAZON_GPIO_P1_IN)&(~GPIO_MDC);
}

void ifx_mdio_lo(void)
{
    //GPIO_SET_LOW(GPIO_MDIO);
    //AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)&=~GPIO_MDIO;
    /*int r=AMAZON_SW_REG(AMAZON_GPIO_P1_OUT);
    r&=~GPIO_MDIO;
    AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)=r;*/

    //AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)=AMAZON_SW_REG(AMAZON_GPIO_P1_IN)&(~GPIO_MDIO);
}

void ifx_mdcs_lo(void)
{
    //GPIO_SET_LOW(GPIO_MDCS);
    //AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)&=~GPIO_MDCS;
    /*int r=AMAZON_SW_REG(AMAZON_GPIO_P1_OUT);
    r&=~GPIO_MDCS;
    AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)=r;*/
    
    //AMAZON_SW_REG(AMAZON_GPIO_P1_OUT)=AMAZON_SW_REG(AMAZON_GPIO_P1_IN)&(~GPIO_MDCS);
}

/*
  mdc pulse
  0 -> 1 -> 0
*/
static void ifx_sw_mdc_pulse(void)
{
/*
    ifx_mdc_lo();
    udelay(ADM_SW_MDC_DOWN_DELAY);
    ifx_mdc_hi();
    udelay(ADM_SW_MDC_UP_DELAY);
    ifx_mdc_lo();
*/
}

/*
  mdc toggle
  1 -> 0
*/
static void ifx_sw_mdc_toggle(void)
{
/*
    ifx_mdc_hi();
    udelay(ADM_SW_MDC_UP_DELAY);
    ifx_mdc_lo();
    udelay(ADM_SW_MDC_DOWN_DELAY);
*/
}

/*
  enable eeprom write
  For ATC 93C66 type EEPROM; accessing ADM6996 internal EEPROM type registers
*/
static void ifx_sw_eeprom_write_enable(void)
{
}

/*
  disable eeprom write
*/
static void ifx_sw_eeprom_write_disable(void)
{
}

/*
  read registers from ADM6996
  serial registers start at 0x200 (addr bit 9 = 1b)
  EEPROM registers -> 16bits; Serial registers -> 32bits
*/
//605112:fchang.removed #ifdef ADM6996_MDC_MDIO_MODE //smi mode//000001.joelin
#ifdef CONFIG_SWITCH_ADM6996_MDIO //605112:fchang.added
static unsigned int ifx_sw_read_adm6996i_smi(unsigned int addr, unsigned int *dat)
{
   //addr=((addr<<16)|(1<<21))&0x3ff0000;
   addr=(addr<<16)&0x3ff0000;
   *DANUBE_PPE32_ETOP_MDIO_ACC =(0xC0000000|addr);
   while ((*DANUBE_PPE32_ETOP_MDIO_ACC)&0x80000000){};
   *dat=((*DANUBE_PPE32_ETOP_MDIO_ACC)&0x0FFFF);
    return *dat;
}
#endif

static int ifx_sw_read_adm6996i(unsigned int addr, unsigned int *dat)
{
    return 0;
}
//adm6996
static int ifx_sw_read_adm6996l(unsigned int addr, unsigned int *dat)
{
    return 0;
}

static unsigned int ifx_sw_read(unsigned int addr, unsigned int *dat)
{
//605112:fchang.removed #ifdef ADM6996_MDC_MDIO_MODE //smi mode ////000001.joelin
#ifdef CONFIG_SWITCH_ADM6996_MDIO //605112:fchang.added
	
	ifx_sw_read_adm6996i_smi(addr,dat);
#else
	#ifdef CONFIG_SWITCH_ADM6996_EEPROM //605112:fchang.added
	if (adm6996_mode==adm6996i) 
		ifx_sw_read_adm6996i(addr,dat);
	else 
		ifx_sw_read_adm6996l(addr,dat);
	#else //605112:fchang.added
	printk("\nWarning! no ifx_sw_read() is called\n"); //605112:fchang.added	
	#endif //605112:fchang.added

#endif		
	return *dat;
}

/*
  write register to ADM6996 eeprom registers
*/
//for adm6996i -start
//605112:fchang.removed #ifdef ADM6996_MDC_MDIO_MODE //smi mode //000001.joelin
#ifdef CONFIG_SWITCH_ADM6996_MDIO //605112:fchang.added
static int ifx_sw_write_adm6996i_smi(unsigned int addr, unsigned int dat)
{
 
   *DANUBE_PPE32_ETOP_MDIO_ACC = ((addr<<16)&0x3ff0000)|dat|0x80000000;
   while ((*DANUBE_PPE32_ETOP_MDIO_ACC )&0x80000000){};
  
    return 0;
 
}
#endif //ADM6996_MDC_MDIO_MODE //000001.joelin

static int ifx_sw_write_adm6996i(unsigned int addr, unsigned int dat)
{
    return 0;
}
//for adm6996i-end
static int ifx_sw_write_adm6996l(unsigned int addr, unsigned int dat)
{
    return 0;
}

static int ifx_sw_write(unsigned int addr, unsigned int dat)
{
//605112:fchang.removed #ifdef ADM6996_MDC_MDIO_MODE //smi mode ////000001.joelin
#ifdef CONFIG_SWITCH_ADM6996_MDIO //605112:fchang.added
	ifx_sw_write_adm6996i_smi(addr,dat);
#else	//000001.joelin
	#ifdef CONFIG_SWITCH_ADM6996_EEPROM //605112:fchang.added
	if (adm6996_mode==adm6996i) 
		ifx_sw_write_adm6996i(addr,dat);
	else 
		ifx_sw_write_adm6996l(addr,dat);
	#else //605112:fchang.added
	printk("\nWarning! NO ifx_sw_write() is called\n"); //605112:fchang.added
	#endif //605112:fchang.added
#endif	//000001.joelin
	return 0;
}

/*
  do switch PHY reset
*/
int ifx_sw_reset(void)
{
    /* reset PHY */
    ifx_sw_write(ADM_SW_PHY_RESET, 0);

    return 0;
}

/*
  check port status
*/
int ifx_check_port_status(int port)
{
    unsigned int val;

    if ((port < 0) || (port > ADM_SW_MAX_PORT_NUM))
    {
        ifx_printf(("error on port number (%d)!!\n", port));
        return -1;
    }

    ifx_sw_read(ifx_sw_conf[port], &val);
    if (ifx_sw_conf[port]%2) val >>= 16;
    /* only 16bits are effective */
    val &= 0xFFFF;

    ifx_printf(("Port %d status (%.8x): \n", port, val));

    if (val & ADM_SW_PORT_FLOWCTL)
        ifx_printf(("\t802.3x flow control supported!\n"));
    else
        ifx_printf(("\t802.3x flow control not supported!\n"));

    if (val & ADM_SW_PORT_AN)
        ifx_printf(("\tAuto negotiation ON!\n"));
    else
        ifx_printf(("\tAuto negotiation OFF!\n"));

    if (val & ADM_SW_PORT_100M)
        ifx_printf(("\tLink at 100M!\n"));
    else
        ifx_printf(("\tLink at 10M!\n"));

    if (val & ADM_SW_PORT_FULL)
        ifx_printf(("\tFull duplex!\n"));
    else
        ifx_printf(("\tHalf duplex!\n"));

    if (val & ADM_SW_PORT_DISABLE)
        ifx_printf(("\tPort disabled!\n"));
    else
        ifx_printf(("\tPort enabled!\n"));

    if (val & ADM_SW_PORT_TOS)
        ifx_printf(("\tTOS enabled!\n"));
    else
        ifx_printf(("\tTOS disabled!\n"));

    if (val & ADM_SW_PORT_PPRI)
        ifx_printf(("\tPort priority first!\n"));
    else
        ifx_printf(("\tVLAN or TOS priority first!\n"));

    if (val & ADM_SW_PORT_MDIX)
        ifx_printf(("\tAuto MDIX!\n"));
    else
        ifx_printf(("\tNo auto MDIX\n"));

    ifx_printf(("\tPVID: %d\n", \
  	    ((val >> ADM_SW_PORT_PVID_SHIFT)&ifx_sw_bits[ADM_SW_PORT_PVID_BITS])));

    return 0;
}



#if 1
/*6996i use vlan filter*/

/*
  add a port to certain vlan filter
*/
int ifx_sw_vlan_add(int port, int filter_id, int fid )
{
    int reg = 0;

	printk("filter:%x port:%x \n", filter_id, port);
    if ((port < 0) || (port > ADM_SW_MAX_PORT_NUM) || (filter_id < 0) ||
        (filter_id > ADM_SW_MAX_VLAN_NUM))
    {
        ifx_printf(("Port number or filterid ERROR!!\n"));
        return -1;
    }
	
    //ifx_sw_read(ADM_SW_VLAN0_CONF + vlanid, &reg);
    ifx_sw_read(ADM_SW_VLAN_FILTER0_LOW + filter_id *2, &reg);
    reg |= ( 1 << port) ;
    reg |= (fid << 12);		
    ifx_sw_write(ADM_SW_VLAN_FILTER0_LOW + filter_id *2, reg);

    return 0;
}


/*
  delete a given port from certain vlan
*/


int ifx_sw_vlan_del(int port, int filter_id)
{
    unsigned int reg = 0;

    if ((port < 0) || (port > ADM_SW_MAX_PORT_NUM) || (filter_id < 0) || (filter_id > ADM_SW_MAX_VLAN_NUM))
    {
        ifx_printf(("Port number or filterid ERROR!!\n"));
        return -1;
    }

	printk("ifx_sw_vlan_del filter%x port%x\n", filter_id, port);
    ifx_sw_read(ADM_SW_VLAN_FILTER0_LOW + filter_id *2, &reg);
    reg &= ~(1 << port) ;
    ifx_sw_write(ADM_SW_VLAN_FILTER0_LOW + filter_id *2, reg);

    return 0;
}
#else

/*
  initialize a VLAN
  clear all VLAN bits
*/

int ifx_sw_vlan_init(int vlanid)
{
    ifx_sw_write(ADM_SW_VLAN0_CONF + vlanid, 0);

    return 0;
}


int ifx_sw_vlan_add(int port, int vlanid)
{
    int reg = 0;

    if ((port < 0) || (port > ADM_SW_MAX_PORT_NUM) || (vlanid < 0) ||
        (vlanid > ADM_SW_MAX_VLAN_NUM))
    {
        ifx_printf(("Port number or VLAN number ERROR!!\n"));
        return -1;
    }
    ifx_sw_read(ADM_SW_VLAN0_CONF + vlanid, &reg);
    reg |= (1 << ifx_sw_vlan_port[port]);
    ifx_sw_write(ADM_SW_VLAN0_CONF + vlanid, reg);

    return 0;
}

int ifx_sw_vlan_del(int port, int vlanid)
{
    unsigned int reg = 0;

    if ((port < 0) || (port > ADM_SW_MAX_PORT_NUM) || (vlanid < 0) || (vlanid > ADM_SW_MAX_VLAN_NUM))
    {
        ifx_printf(("Port number or VLAN number ERROR!!\n"));
        return -1;
    }
    ifx_sw_read(ADM_SW_VLAN0_CONF + vlanid, &reg);
    reg &= ~(1 << ifx_sw_vlan_port[port]);
    ifx_sw_write(ADM_SW_VLAN0_CONF + vlanid, reg);

    return 0;
}
#endif

int ifx_sw_get_cpu_port()
{
    unsigned int reg = 0;

  	
    //ifx_sw_read(ADM_SW_VLAN0_CONF + vlanid, &reg);
    ifx_sw_read(ADM_SW_VLAN_MODE, &reg);

    reg &= 0xe000;	
    reg = reg >> 13 ;
    return reg;
}



int ifx_sw_set_cpu_port(int port)
{
    int reg = 0;

    if ((port < 0) || (port > ADM_SW_MAX_PORT_NUM+1)) 
    {
        ifx_printf(("Port number ERROR!!\n"));
        return -1;
    }

	printk("set cpu port :%d\n", port);
	reg |= port <<13 ;
	ifx_sw_write(ADM_SW_VLAN_MODE, reg);
	return 0;
}

int ifx_sw_disable_cpu_port()
{
    int reg = 0;

	reg |= 0x7 <<13 ;
	//printk("write :%x\n", reg);
	ifx_sw_write(ADM_SW_VLAN_MODE, reg);
    return 0;
}


/*
  default VLAN setting

  port 0~3 as untag port and PVID = 1
  VLAN1: port 0~3 and port 5 (MII)
*/
static int ifx_sw_init(void)
{
    ifx_printf(("Setting default ADM6996 registers... \n"));

    /* MAC clone, 802.1q based VLAN */
    ifx_sw_write(ADM_SW_VLAN_MODE, 0xff30);
    /* auto MDIX, PVID=1, untag */
    ifx_sw_write(ADM_SW_PORT0_CONF, 0x840f);
    ifx_sw_write(ADM_SW_PORT1_CONF, 0x840f);
    ifx_sw_write(ADM_SW_PORT2_CONF, 0x840f);
    ifx_sw_write(ADM_SW_PORT3_CONF, 0x840f);
    /* auto MDIX, PVID=2, untag */
    ifx_sw_write(ADM_SW_PORT5_CONF, 0x880f);
    /* port 0~3 & 5 as VLAN1 */
    ifx_sw_write(ADM_SW_VLAN0_CONF+1, 0x0155);

    return 0;
}


int adm_open(struct inode *node, struct file *filp)
{
    MOD_INC_USE_COUNT;
    return 0;
}

ssize_t adm_read(struct file *filep, char *buf, size_t count, loff_t *ppos)
{
    return count;
}

ssize_t adm_write(struct file *filep, const char *buf, size_t count, loff_t *ppos)
{
    return count;
}

/* close */
int adm_release(struct inode *inode, struct file *filp)
{
    MOD_DEC_USE_COUNT;
    return 0;
}

/* IOCTL function */
int adm_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long args)
{
    PREGRW uREGRW;
    unsigned int rtval;
    unsigned int val;		//6996i
    unsigned int control[6] ;	//6996i
    unsigned int status[6] ;	//6996i
    
    PMACENTRY mMACENTRY;//adm6996i
    PPROTOCOLFILTER uPROTOCOLFILTER ;///adm6996i

    if (_IOC_TYPE(cmd) != ADM_MAGIC)
    {
        printk("adm_ioctl: IOC_TYPE(%x) != ADM_MAGIC(%x)! \n", _IOC_TYPE(cmd), ADM_MAGIC);
        return (-EINVAL);
    }

    if(_IOC_NR(cmd) >= KEY_IOCTL_MAX_KEY)
    {
        printk(KERN_WARNING "adm_ioctl: IOC_NR(%x) invalid! \n", _IOC_NR(cmd));
        return (-EINVAL);
    }

    switch (cmd)
    {
        case ADM_IOCTL_REGRW:
        {
            uREGRW = (PREGRW)kmalloc(sizeof(REGRW), GFP_KERNEL);
            rtval = copy_from_user(uREGRW, (PREGRW)args, sizeof(REGRW));
            if (rtval != 0)
            {
                printk("ADM_IOCTL_REGRW: copy from user FAILED!! \n");
                return (-EFAULT);
            }

            switch(uREGRW->mode)
            {
                case REG_READ:
                    uREGRW->value = 0x12345678;//inl(uREGRW->addr);
                    copy_to_user((PREGRW)args, uREGRW, sizeof(REGRW));
                    break;
                case REG_WRITE:
                    //outl(uREGRW->value, uREGRW->addr);
                    break;

                default:
                    printk("No such Register Read/Write function!! \n");
                    return (-EFAULT);
            }
            kfree(uREGRW);
            break;
        }

        case ADM_SW_IOCTL_REGRW:
        {
            unsigned int val = 0xff;

            uREGRW = (PREGRW)kmalloc(sizeof(REGRW), GFP_KERNEL);
            rtval = copy_from_user(uREGRW, (PREGRW)args, sizeof(REGRW));
            if (rtval != 0)
            {
                printk("ADM_IOCTL_REGRW: copy from user FAILED!! \n");
                return (-EFAULT);
            }

            switch(uREGRW->mode)
            {
                case REG_READ:
                    ifx_sw_read(uREGRW->addr, &val);
                    uREGRW->value = val;
                    copy_to_user((PREGRW)args, uREGRW, sizeof(REGRW));
                    break;

                case REG_WRITE:
                    ifx_sw_write(uREGRW->addr, uREGRW->value);
                    break;
                default:
                    printk("No such Register Read/Write function!! \n");
                    return (-EFAULT);
            }
            kfree(uREGRW);
            break;
        }

        case ADM_SW_IOCTL_PORTSTS:
            for (rtval = 0; rtval < ADM_SW_MAX_PORT_NUM+1; rtval++)
                ifx_check_port_status(rtval);
            break;

        case ADM_SW_IOCTL_INIT:
            ifx_sw_init();
            break;

//adm6996i
        case ADM_SW_IOCTL_MACENTRY_ADD:
        case ADM_SW_IOCTL_MACENTRY_DEL:
        case ADM_SW_IOCTL_MACENTRY_GET_INIT:
        case ADM_SW_IOCTL_MACENTRY_GET_MORE:
                

           mMACENTRY = (PMACENTRY)kmalloc(sizeof(MACENTRY), GFP_KERNEL);
            rtval = copy_from_user(mMACENTRY, (PMACENTRY)args, sizeof(MACENTRY));
            if (rtval != 0)
            {
                printk("ADM_SW_IOCTL_MACENTRY: copy from user FAILED!! \n");
                return (-EFAULT);
            }
           control[0]=(mMACENTRY->mac_addr[1]<<8)+mMACENTRY->mac_addr[0]     ; 
           control[1]=(mMACENTRY->mac_addr[3]<<8)+mMACENTRY->mac_addr[2]      ;         
           control[2]=(mMACENTRY->mac_addr[5]<<8)+mMACENTRY->mac_addr[4]     ;
           control[3]=(mMACENTRY->fid&0xf)+((mMACENTRY->portmap&0x3f)<<4);
           if (((mMACENTRY->info_type)&0x01)) control[4]=(mMACENTRY->ctrl.info_ctrl)+0x1000; //static ,info control
           	else	control[4]=((mMACENTRY->ctrl.age_timer)&0xff);//not static ,agetimer
         	if (cmd==ADM_SW_IOCTL_MACENTRY_GET_INIT) { 	
	           //initial  the pointer to the first address	
			           val=0x8000;//busy ,status5[15]
			           while(val&0x8000){		//check busy ?
			 	          ifx_sw_read(0x125, &val);
			        	}    
			           control[5]=0x030;//initial the first address	
			           ifx_sw_write(0x11f,control[5]);
			        	       	
			           	
			           val=0x8000;//busy ,status5[15]
			           while(val&0x8000){		//check busy ?
			 	          ifx_sw_read(0x125, &val);
			        	}           	
	           	
	           }	//if (cmd==ADM_SW_IOCTL_MACENTRY_GET_INIT)								
           if (cmd==ADM_SW_IOCTL_MACENTRY_ADD) control[5]=0x07;//create a new address
           	else if (cmd==ADM_SW_IOCTL_MACENTRY_DEL) control[5]=0x01f;//erased an existed address
           	else if ((cmd==ADM_SW_IOCTL_MACENTRY_GET_INIT)||(cmd==ADM_SW_IOCTL_MACENTRY_GET_MORE)) 
           		control[5]=0x02c;//search by the mac address field
           
           val=0x8000;//busy ,status5[15]
           while(val&0x8000){		//check busy ?
 	          ifx_sw_read(0x125, &val);
        	}
        	ifx_sw_write(0x11a,control[0]);	
        	ifx_sw_write(0x11b,control[1]);	
        	ifx_sw_write(0x11c,control[2]);	
        	ifx_sw_write(0x11d,control[3]);	
        	ifx_sw_write(0x11e,control[4]);	
        	ifx_sw_write(0x11f,control[5]);	
           val=0x8000;//busy ,status5[15]
           while(val&0x8000){		//check busy ?
 	          ifx_sw_read(0x125, &val);
        	}	
           val=((val&0x7000)>>12);//result ,status5[14:12]
           mMACENTRY->result=val;
   
           if (!val) {
        		printk(" Command OK!! \n");
        		if ((cmd==ADM_SW_IOCTL_MACENTRY_GET_INIT)||(cmd==ADM_SW_IOCTL_MACENTRY_GET_MORE)) {
			           	ifx_sw_read(0x120,&(status[0]));	
			        	ifx_sw_read(0x121,&(status[1]));	
			        	ifx_sw_read(0x122,&(status[2]));	
			        	ifx_sw_read(0x123,&(status[3]));	
			        	ifx_sw_read(0x124,&(status[4]));	
			        	ifx_sw_read(0x125,&(status[5]));	
		
		           		
		        		mMACENTRY->mac_addr[0]=(status[0]&0x00ff)	;
		        		mMACENTRY->mac_addr[1]=(status[0]&0xff00)>>8    ;
		        		mMACENTRY->mac_addr[2]=(status[1]&0x00ff)    ;
		        		mMACENTRY->mac_addr[3]=(status[1]&0xff00)>>8 ;
		        		mMACENTRY->mac_addr[4]=(status[2]&0x00ff)    ;
		        		mMACENTRY->mac_addr[5]=(status[2]&0xff00)>>8 ;
		        		mMACENTRY->fid=(status[3]&0xf);
		        		mMACENTRY->portmap=((status[3]>>4)&0x3f);
		        		if (status[5]&0x2) {//static info_ctrl //status5[1]????
		        			mMACENTRY->ctrl.info_ctrl=(status[4]&0x00ff);
		  				mMACENTRY->info_type=1;
		        				}
		        		else {//not static age_timer
		        			mMACENTRY->ctrl.age_timer=(status[4]&0x00ff);
		  				mMACENTRY->info_type=0;
		        				}
//status5[13]????					mMACENTRY->occupy=(status[5]&0x02)>>1;//status5[1]
					mMACENTRY->occupy=(status[5]&0x02000)>>13;//status5[13] ???
					mMACENTRY->bad=(status[5]&0x04)>>2;//status5[2]
				}//if ((cmd==ADM_SW_IOCTL_MACENTRY_GET_INIT)||(cmd==ADM_SW_IOCTL_MACENTRY_GET_MORE)) 
			
        	}
           else if (val==0x001)  
                printk(" All Entry Used!! \n");
            else if (val==0x002) 
                printk("  Entry Not Found!! \n");
            else if (val==0x003) 
                printk(" Try Next Entry!! \n");
            else if (val==0x005)  
                printk(" Command Error!! \n");   
            else   
                printk(" UnKnown Error!! \n");
                
            copy_to_user((PMACENTRY)args, mMACENTRY,sizeof(MACENTRY));    
                
 	    break;  
 
        case ADM_SW_IOCTL_FILTER_ADD:
        case ADM_SW_IOCTL_FILTER_DEL:
        case ADM_SW_IOCTL_FILTER_GET:

            uPROTOCOLFILTER = (PPROTOCOLFILTER)kmalloc(sizeof(PROTOCOLFILTER), GFP_KERNEL);
            rtval = copy_from_user(uPROTOCOLFILTER, (PPROTOCOLFILTER)args, sizeof(PROTOCOLFILTER));
            if (rtval != 0)
            {
                printk("ADM_SW_IOCTL_FILTER_ADD: copy from user FAILED!! \n");
                return (-EFAULT);
            }
            
        	if(cmd==ADM_SW_IOCTL_FILTER_DEL) {	//delete filter
			uPROTOCOLFILTER->ip_p=00;	//delet filter
			uPROTOCOLFILTER->action=00;	//delete filter
		}					//delete filter

            ifx_sw_read(((uPROTOCOLFILTER->protocol_filter_num/2)+0x68), &val);//rx68~rx6b,protocol filter0~7	

            	if (((uPROTOCOLFILTER->protocol_filter_num)%2)==00){	
            		if(cmd==ADM_SW_IOCTL_FILTER_GET) uPROTOCOLFILTER->ip_p= val&0x00ff;//get filter ip_p
            			else val=(val&0xff00)|(uPROTOCOLFILTER->ip_p);//set filter ip_p
        	}
        	else {
        		if(cmd==ADM_SW_IOCTL_FILTER_GET) uPROTOCOLFILTER->ip_p= (val>>8);//get filter ip_p
        			else val=(val&0x00ff)|((uPROTOCOLFILTER->ip_p)<<8);//set filter ip_p
        	}	
            if(cmd!=ADM_SW_IOCTL_FILTER_GET) ifx_sw_write(((uPROTOCOLFILTER->protocol_filter_num/2)+0x68), val);//write rx68~rx6b,protocol filter0~7	
            		
            ifx_sw_read(0x95, &val);	//protocol filter action
            if(cmd==ADM_SW_IOCTL_FILTER_GET) {
            		uPROTOCOLFILTER->action= ((val>>(uPROTOCOLFILTER->protocol_filter_num*2))&0x3);//get filter action
            		copy_to_user((PPROTOCOLFILTER)args, uPROTOCOLFILTER, sizeof(PROTOCOLFILTER));
            	
            	}
            	else {
            		val=(val&(~(0x03<<(uPROTOCOLFILTER->protocol_filter_num*2))))|(((uPROTOCOLFILTER->action)&0x03)<<(uPROTOCOLFILTER->protocol_filter_num*2));
  //          		printk("%d----\n",val);
            		ifx_sw_write(0x95, val);	//write protocol filter action		
            	}
            	
            break;
//adm6996i  

        /* others */
        default:
            return -EFAULT;
    }
    /* end of switch */
    return 0;
}

/* Santosh: handle IGMP protocol filter ADD/DEL/GET */
int adm_process_protocol_filter_request (unsigned int cmd, PPROTOCOLFILTER uPROTOCOLFILTER)
{
    unsigned int val;		//6996i

	if(cmd==ADM_SW_IOCTL_FILTER_DEL) {	//delete filter
	uPROTOCOLFILTER->ip_p=00;	//delet filter
	uPROTOCOLFILTER->action=00;	//delete filter
	}					//delete filter

    ifx_sw_read(((uPROTOCOLFILTER->protocol_filter_num/2)+0x68), &val);//rx68~rx6b,protocol filter0~7	

    if (((uPROTOCOLFILTER->protocol_filter_num)%2)==00){	
    	if(cmd==ADM_SW_IOCTL_FILTER_GET) uPROTOCOLFILTER->ip_p= val&0x00ff;//get filter ip_p
        else val=(val&0xff00)|(uPROTOCOLFILTER->ip_p);//set filter ip_p
    }
    else {
    	if(cmd==ADM_SW_IOCTL_FILTER_GET) uPROTOCOLFILTER->ip_p= (val>>8);//get filter ip_p
    	else val=(val&0x00ff)|((uPROTOCOLFILTER->ip_p)<<8);//set filter ip_p
    }	
    if(cmd!=ADM_SW_IOCTL_FILTER_GET) ifx_sw_write(((uPROTOCOLFILTER->protocol_filter_num/2)+0x68), val);//write rx68~rx6b,protocol filter0~7	
            		
    	ifx_sw_read(0x95, &val);	//protocol filter action
    if(cmd==ADM_SW_IOCTL_FILTER_GET) {
       	uPROTOCOLFILTER->action= ((val>>(uPROTOCOLFILTER->protocol_filter_num*2))&0x3);//get filter action
    }
    else {
    	val=(val&(~(0x03<<(uPROTOCOLFILTER->protocol_filter_num*2))))|(((uPROTOCOLFILTER->action)&0x03)<<(uPROTOCOLFILTER->protocol_filter_num*2));
        ifx_sw_write(0x95, val);	//write protocol filter action		
    }
            	
	return 0;
}

//implement by lancebai, 
//function for igmp_membership _read
#if 0
int adm_read_igmp_membership_table_request (unsigned int entry_addr, PIGMPMEMBERTRY mIGMPENTRY)
{
	unsigned int rtval;
	unsigned int val;		//6996i
	unsigned int control[6] ;	//6996i
	unsigned int status[6] ;	//6996i
	int i;

	// printk ("adm_process_mac_table_request: enter\n");	


	val=0x8000;//busy ,status5[15]


	control[5]=0x05 <<4 ;//read data from internal igmp table	
	control[4] = entry_addr;	

	
	
	while(val&0x8000)
	{	
		//check busy ?
		ifx_sw_read(0x125, &val);
	}    

	ifx_sw_write(0x11f,control[5]);
	ifx_sw_write(0x11e,control[4]);

	while(val&0x8000)
	{	
		//check busy ?
		ifx_sw_read(0x125, &val);
	}    
	

	val=((val&0x7000)>>12);//result ,status5[14:12]
	mIGMPENTRY->result=val;	


	ifx_sw_read(0x120,&(status[0]));	
	ifx_sw_read(0x121,&(status[1]));	
	ifx_sw_read(0x122,&(status[2]));	
	ifx_sw_read(0x123,&(status[3]));	
	ifx_sw_read(0x124,&(status[4]));	
	ifx_sw_read(0x125,&(status[5]));	
	
	for(i = 0; i< 6; i++)
	{
		printk("status%x:%x \n", i, status[i]);
	}		
	// printk ("adm_process_mac_table_request: Exit\n");	
	return 0;
}
#endif

/* Santosh: function for MAC ENTRY ADD/DEL/GET */

int adm_process_mac_table_request (unsigned int cmd, PMACENTRY mMACENTRY)
{
    unsigned int rtval;
    unsigned int val;		//6996i
    unsigned int control[6] ;	//6996i
    unsigned int status[6] ;	//6996i

	// printk ("adm_process_mac_table_request: enter\n");	

    control[0]=(mMACENTRY->mac_addr[1]<<8)+mMACENTRY->mac_addr[0]     ; 
    control[1]=(mMACENTRY->mac_addr[3]<<8)+mMACENTRY->mac_addr[2]      ;         
    control[2]=(mMACENTRY->mac_addr[5]<<8)+mMACENTRY->mac_addr[4]     ;
    control[3]=(mMACENTRY->fid&0xf)+((mMACENTRY->portmap&0x3f)<<4);

    if (((mMACENTRY->info_type)&0x01)) control[4]=(mMACENTRY->ctrl.info_ctrl)+0x1000; //static ,info control
   		else	control[4]=((mMACENTRY->ctrl.age_timer)&0xff);//not static ,agetimer
        	if (cmd==ADM_SW_IOCTL_MACENTRY_GET_INIT) { 	
	          //initial  the pointer to the first address	
	           val=0x8000;//busy ,status5[15]
	           while(val&0x8000){		//check busy ?
	           ifx_sw_read(0x125, &val);
	       	}    
	        control[5]=0x030;//initial the first address	
	        ifx_sw_write(0x11f,control[5]);
			        	       	
			           	
			           val=0x8000;//busy ,status5[15]
			           while(val&0x8000){		//check busy ?
			 	          ifx_sw_read(0x125, &val);
			        	}           	
	           	
	           }	//if (cmd==ADM_SW_IOCTL_MACENTRY_GET_INIT)								
           if (cmd==ADM_SW_IOCTL_MACENTRY_ADD) control[5]=0x07;//create a new address
           	else if (cmd==ADM_SW_IOCTL_MACENTRY_DEL) control[5]=0x01f;//erased an existed address
           	else if ((cmd==ADM_SW_IOCTL_MACENTRY_GET_INIT)||(cmd==ADM_SW_IOCTL_MACENTRY_GET_MORE)) 
           		control[5]=0x02c;//search by the mac address field
           
           val=0x8000;//busy ,status5[15]
           while(val&0x8000){		//check busy ?
 	          ifx_sw_read(0x125, &val);
        	}
        	ifx_sw_write(0x11a,control[0]);	
        	ifx_sw_write(0x11b,control[1]);	
        	ifx_sw_write(0x11c,control[2]);	
        	ifx_sw_write(0x11d,control[3]);	
        	ifx_sw_write(0x11e,control[4]);	
        	ifx_sw_write(0x11f,control[5]);	
           val=0x8000;//busy ,status5[15]
           while(val&0x8000){		//check busy ?
 	          ifx_sw_read(0x125, &val);
        	}	
           val=((val&0x7000)>>12);//result ,status5[14:12]
           mMACENTRY->result=val;
   
           if (!val) {
        		printk(" Command OK!! \n");
        		if ((cmd==ADM_SW_IOCTL_MACENTRY_GET_INIT)||(cmd==ADM_SW_IOCTL_MACENTRY_GET_MORE)) {
			           	ifx_sw_read(0x120,&(status[0]));	
			        	ifx_sw_read(0x121,&(status[1]));	
			        	ifx_sw_read(0x122,&(status[2]));	
			        	ifx_sw_read(0x123,&(status[3]));	
			        	ifx_sw_read(0x124,&(status[4]));	
			        	ifx_sw_read(0x125,&(status[5]));	
		
		           		
		        		mMACENTRY->mac_addr[0]=(status[0]&0x00ff)	;
		        		mMACENTRY->mac_addr[1]=(status[0]&0xff00)>>8    ;
		        		mMACENTRY->mac_addr[2]=(status[1]&0x00ff)    ;
		        		mMACENTRY->mac_addr[3]=(status[1]&0xff00)>>8 ;
		        		mMACENTRY->mac_addr[4]=(status[2]&0x00ff)    ;
		        		mMACENTRY->mac_addr[5]=(status[2]&0xff00)>>8 ;
		        		mMACENTRY->fid=(status[3]&0xf);
		        		mMACENTRY->portmap=((status[3]>>4)&0x3f);
		        		if (status[5]&0x2) {//static info_ctrl //status5[1]????
		        			mMACENTRY->ctrl.info_ctrl=(status[4]&0x00ff);
		  				mMACENTRY->info_type=1;
		        				}
		        		else {//not static age_timer
		        			mMACENTRY->ctrl.age_timer=(status[4]&0x00ff);
		  				mMACENTRY->info_type=0;
		        				}
//status5[13]????					mMACENTRY->occupy=(status[5]&0x02)>>1;//status5[1]
					mMACENTRY->occupy=(status[5]&0x02000)>>13;//status5[13] ???
					mMACENTRY->bad=(status[5]&0x04)>>2;//status5[2]
				}//if ((cmd==ADM_SW_IOCTL_MACENTRY_GET_INIT)||(cmd==ADM_SW_IOCTL_MACENTRY_GET_MORE)) 
			
        	}
           else if (val==0x001)  
                printk(" All Entry Used!! \n");
            else if (val==0x002) 
                printk("  Entry Not Found!! \n");
            else if (val==0x003) 
                printk(" Try Next Entry!! \n");
            else if (val==0x005)  
                printk(" Command Error!! \n");   
            else   
                printk(" UnKnown Error!! \n");

	// printk ("adm_process_mac_table_request: Exit\n");	
	return 0;
}

/* Santosh: End of function for MAC ENTRY ADD/DEL*/
struct file_operations adm_ops =
{
    read: adm_read,
    write: adm_write,
    open: adm_open,
    release: adm_release,
    ioctl: adm_ioctl
};

int adm_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
    int len = 0;

    len += sprintf(buf+len, " ************ Registers ************ \n");
    *eof = 1;
    return len;
}

#if 0
struct file_operations adm_ops = {
	read: adm_read,
	write: adm_write,
	open: adm_open,
	release: adm_release,
	ioctl: adm_ioctl
};
#endif
static int __init init_adm6996_module(void)
{
    unsigned int val = 000;
    unsigned int val1 = 000;

    printk("Loading ADM6996 driver... \n");
	

    /* if running on adm5120 */
    /* set GPIO 0~2 as adm6996 control pins */
    //outl(0x003f3f00, 0x12000028);
    /* enable switch port 5 (MII) as RMII mode (5120MAC <-> 6996MAC) */
    //outl(0x18a, 0x12000030);
    /* group adm5120 port 1 ~ 5 as VLAN0, port 5 & 6(CPU) as VLAN1 */
    //outl(0x417e, 0x12000040);
    /* end adm5120 fixup */
//605112:fchang.removed #ifdef ADM6996_MDC_MDIO_MODE //smi mode //000001.joelin
#ifdef CONFIG_SWITCH_ADM6996_MDIO //605112:fchang.added
    register_chrdev(69, "adm6996", &adm_ops);
    //AMAZON_SW_REG(AMAZON_SW_MDIO_CFG) = 0x27be;
    //AMAZON_SW_REG(AMAZON_SW_EPHY) = 0xfc;
    *DANUBE_PPE32_ETOP_MDIO_CFG=0;
    *DANUBE_PPE32_ENET_MAC_CFG &= ~0x18; //ENET0 MAC Configuration
    adm6996_mode=adm6996i;
    ifx_sw_read(0xa0, &val);
    ifx_sw_read(0xa1, &val1);
    val=((val1&0x0f)<<16)|val;
    printk ("\nADM6996 SMI Mode-");
    printk ("Chip ID:%5x \n ", val);
#else    //000001.joelin
	#ifdef CONFIG_SWITCH_ADM6996_EEPROM //605112:fchang.added
    AMAZON_SW_REG(AMAZON_SW_MDIO_CFG) = 0x2c50;
    AMAZON_SW_REG(AMAZON_SW_EPHY) = 0xff;

    AMAZON_SW_REG(AMAZON_GPIO_P1_ALTSEL0) &= ~(GPIO_MDIO|GPIO_MDCS|GPIO_MDC);
    AMAZON_SW_REG(AMAZON_GPIO_P1_ALTSEL1) &= ~(GPIO_MDIO|GPIO_MDCS|GPIO_MDC);
    AMAZON_SW_REG(AMAZON_GPIO_P1_OD) |= (GPIO_MDIO|GPIO_MDCS|GPIO_MDC);
  
    ifx_gpio_init();
    register_chrdev(69, "adm6996", &adm_ops);

    /* create proc entries */
    //  create_proc_read_entry("admide", 0, NULL, admide_proc, NULL);

//joelin adm6996i support start
    adm6996_mode=adm6996i;
    ifx_sw_read(0xa0, &val);
    adm6996_mode=adm6996l;
    ifx_sw_read(0x200, &val1);
//  printk ("\n %0x \n",val1);
    if ((val&0xfff0)==0x1020) {
        printk ("\n ADM6996I .. \n");
        adm6996_mode=adm6996i;	
    }
    else if ((val1&0xffffff00)==0x71000) {//71010 or 71020
        printk ("\n ADM6996LC .. \n");
        adm6996_mode=adm6996lc;	
    }
    else  {
        printk ("\n ADM6996L .. \n");
        adm6996_mode=adm6996l;	
    }
	#else //605112:fchang.added
	printk("\nWarning! No configuration mode is specified in init_adm6996_module(), please check\n"); //605112:fchang.added
	#endif //605112:fchang.added
#endif //ADM6996_MDC_MDIO_MODE //smi mode //000001.joelin	

    if ((adm6996_mode==adm6996lc)||(adm6996_mode==adm6996i)){
#if 0	/* removed by MarsLin */
        ifx_sw_write(0x29,0xc000);
        ifx_sw_write(0x30,0x0985);
#else
        ifx_sw_read(0xa0, &val);
printk("0xa0 value = %x\n", val);
        if (val == 0x1021) // for both 6996LC and 6996I, only AB version need the patch
        {
        	
            ifx_sw_write(0x29, 0x9000);
        }		
        ifx_sw_write(0x30,0x0985);
#endif
    }
	/* create proc entries */
  ifx_sw_proc_init();  
  ifx_sw_proc_addproc("bandwidth_control", \
  	ifx_sw_bdhctl_read_proc, ifx_sw_bdhctl_write_proc);
  ifx_sw_proc_addproc("tcpudp_control", \
  	ifx_sw_tcpudpctl_read_proc, ifx_sw_tcpudpctl_write_proc);
  ifx_sw_proc_addproc("qos_control", \
  	ifx_sw_qosctl_read_proc, ifx_sw_qosctl_write_proc);
  ifx_sw_proc_addproc("queue_control", \
  	ifx_sw_qctl_read_proc, ifx_sw_qctl_write_proc);
  ifx_sw_proc_addproc("port_control", \
  	ifx_sw_pctl_read_proc, ifx_sw_pctl_write_proc);
  ifx_sw_proc_addproc("igmp_control", \
  	ifx_sw_igmpctl_read_proc, ifx_sw_igmpctl_write_proc);
  ifx_sw_proc_addproc("device_control", \
  	ifx_sw_devctl_read_proc, ifx_sw_devctl_write_proc);
    ifx_sw_proc_addproc("vlan_control", \
  	ifx_sw_vlanctl_read_proc, ifx_sw_vlanctl_write_proc);


//joelin adm6996i support end
    return 0;
}

static void cleanup_adm6996_module(void)
{
    printk("Free ADM device driver... \n");

    unregister_chrdev(69, "adm6996");

    /* remove proc entries */
    //  remove_proc_entry("admide", NULL);
}

/* MarsLin, add start */
//060620:henryhsu modify for vlan 
#if defined(CONFIG_IFX_NFEXT_DANUBE_SWITCH_PHYPORT) || defined(CONFIG_IFX_NFEXT_DANUBE_SWITCH_PHYPORT_MODULE)
    #define SET_BIT(reg, mask)		reg |= (mask)
    #define CLEAR_BIT(reg, mask)	reg &= (~mask)
static int ifx_hw_reset(void)
{
/*
    CLEAR_BIT((*AMAZON_GPIO_P0_ALTSEL0),0x2000);
    CLEAR_BIT((*AMAZON_GPIO_P0_ALTSEL1),0x2000);
    SET_BIT((*AMAZON_GPIO_P0_OD),0x2000);
    SET_BIT((*AMAZON_GPIO_P0_DIR), 0x2000);
   */
//060620:henryhsu modify for vlan 
    cleanup_adm6996_module();
    return init_adm6996_module();
    return 0;
}
int (*adm6996_hw_reset)(void) = ifx_hw_reset;
EXPORT_SYMBOL(adm6996_hw_reset);
EXPORT_SYMBOL(adm6996_mode);
int (*adm6996_sw_read)(unsigned int addr, unsigned int *data) = ifx_sw_read;
EXPORT_SYMBOL(adm6996_sw_read);
int (*adm6996_sw_write)(unsigned int addr, unsigned int data) = ifx_sw_write;
EXPORT_SYMBOL(adm6996_sw_write);
#endif
/* MarsLin, add end */
	
/* Santosh: for IGMP proxy/snooping, Begin */
EXPORT_SYMBOL (adm_process_mac_table_request);
EXPORT_SYMBOL (adm_process_protocol_filter_request);
/* Santosh: for IGMP proxy/snooping, End */

//winder	
EXPORT_SYMBOL (ifx_sw_vlan_add);
EXPORT_SYMBOL (ifx_sw_vlan_del);

MODULE_DESCRIPTION("ADMtek 6996 Driver");
MODULE_AUTHOR("Joe Lin <joe.lin@infineon.com>");
MODULE_LICENSE("GPL");

module_init(init_adm6996_module);
module_exit(cleanup_adm6996_module);

