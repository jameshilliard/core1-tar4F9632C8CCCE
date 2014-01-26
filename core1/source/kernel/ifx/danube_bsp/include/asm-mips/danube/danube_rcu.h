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

 * danube_rcu.h

 *

 * Global Danube rcu driver header file

 *

 */



/* Modification history */

/* 25 July 2005 jssow jinsze@gmail.com Danube project */

/* 04 Aug 2005  jssow jinsze@gmail.com Danube project */ 


#ifndef DANUBE_RCU_H
#define DANUBE_RCU_H


#undef RCU_DEBUG 

//#define RCU_DEBUG 1

#ifdef RCU_DEBUG

unsigned int rcu_global_register[16]={ 0x00000000,0x000000001, 0x00000002, 0x00000003, 0x00000004, 0x00000005, 0x00000006, 0x00000007, 0x00000008, 0x00000009, 0x0000000A , 0x0000000B, 0x0000000C, 0x0000000D, 0x0000000E, 0x0000000F}; 

#define DANUBE_RCU_REG32(addr)			rcu_global_register[(addr - DANUBE_RCU_BASE_ADDR)/4]

#define DANUBE_RCU_READ_REG(addr, reg)          reg = rcu_global_register[(addr -DANUBE_RCU_BASE_ADDR)/4]

#define DANUBE_RCU_WRITE_REG(addr, reg)         rcu_global_register[(addr - DANUBE_RCU_BASE_ADDR)/4] = reg



#else

#define DANUBE_RCU_REG32(addr)          	(*((volatile unsigned int*)(addr)))

#define DANUBE_RCU_READ_REG(addr, reg)  	reg = (*((volatile unsigned int *)(addr)))

#define DANUBE_RCU_WRITE_REG(addr, reg)         *((volatile unsigned int*)(addr)) = reg 

#endif

#define DANUBE_RCU_IOC_MAGIC              0xf0
#define DANUBE_RCU_IOC_SET_RCU_RST_REQ    _IOW( DANUBE_RCU_IOC_MAGIC, 0, danube_rst_req_parm_t)
#define DANUBE_RCU_IOC_GET_RCU_RST_REQ    _IOW( DANUBE_RCU_IOC_MAGIC, 1, danube_rst_req_parm_t)
#define DANUBE_RCU_IOC_GET_STATUS         _IOR( DANUBE_RCU_IOC_MAGIC, 2, danube_rst_stat_parm_t)
#define DANUBE_RCU_IOC_MAXNR              2

typedef union{
  struct{
    unsigned int srd0_hrst      : 1;
    unsigned int srd1_cpu0      : 1;
    unsigned int srd2_fpi       : 1;
    unsigned int srd3_cpu1      : 1;

    unsigned int srd4_usb       : 1;
    unsigned int srd5_ethmac2   : 1;
    unsigned int srd6_ahb       : 1;
    unsigned int srd7_arc_dfe   : 1;

    unsigned int srd8_ppe       : 1;
    unsigned int srd9_dma       : 1;
    unsigned int res0           : 2;

    unsigned int srd12_voice_dfeafe  : 1;
    unsigned int srd13_pci      : 1;
    unsigned int srd14_mc       : 1;
    unsigned int test_md        : 1;

    unsigned int res1           : 2;
    unsigned int boot           : 3;

    unsigned int endian         : 1;
    unsigned int sdr            : 1;

    unsigned int res2           : 4;
    unsigned int pwor           : 1;

    unsigned int srst           : 1;
    unsigned int isftrst        : 1;
    unsigned int wdtrst1        : 1;
    unsigned int wdtrst0        : 1;
   }field;
  u32 danube_rst_stat;
}danube_rst_stat_parm_t;

typedef union{
  struct{
    unsigned int rd0_hrst      : 1;
    unsigned int rd1_cpu0      : 1;
    unsigned int rd2_fpi       : 1;
    unsigned int rd3_cpu1      : 1;

    unsigned int rd4_usb       : 1;
    unsigned int rd5_ethmac2   : 1;
    unsigned int rd6_ahb       : 1;
    unsigned int rd7_arc_dfe   : 1;

    unsigned int rd8_ppe       : 1;
    unsigned int rd9_dma       : 1;
    unsigned int res0          : 1;
    unsigned int rd11_dsl_afe  : 1;

    unsigned int rd12_voice_dfeafe  : 1;
    unsigned int rd13_pci      : 1;
    unsigned int rd14_mc       : 1;
    unsigned int jtag          : 1;

    unsigned int pci_mux       : 2;
    unsigned int sdram         : 1;
    unsigned int pci_rdy       :1;

    unsigned int res1          : 10;

    unsigned int srst          : 1;
    unsigned int enmip         : 1;
   }field;
  u32 danube_rst_req;
}danube_rst_req_parm_t;

#endif /* DANUBE_RCU */



