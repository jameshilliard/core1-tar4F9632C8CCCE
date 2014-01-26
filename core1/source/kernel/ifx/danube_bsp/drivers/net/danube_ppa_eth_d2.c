/******************************************************************************
**
** FILE NAME    : danube_ppa_eth_d2.c
** PROJECT      : Twinpass-E
** MODULES     	: ETH Interface / PPA (Routing Acceleration)
**
** DATE         : 12 SEP 2006
** AUTHOR       : Xu Liang
** DESCRIPTION  : ETH Driver (MII0 & MII1) with Routing Acceleration Firmware
** COPYRIGHT    : 	Copyright (c) 2006
**			Infineon Technologies AG
**			Am Campeon 1-12, 85579 Neubiberg, Germany
**
**    This program is free software; you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or
**    (at your option) any later version.
**
** HISTORY
** $Date        $Author         $Comment
** 12 SEP 2006  Xu Liang        Initiate Version
** 26 OCT 2006  Xu Liang        Add GPL header.
*******************************************************************************/



/*
 * ####################################
 *              Head File
 * ####################################
 */

/*
 *  Common Head File
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/atmdev.h>
#include <linux/init.h>
#include <linux/etherdevice.h>  /*  eth_type_trans  */
#include <linux/ethtool.h>      /*  ethtool_cmd     */
#include <linux/if_ether.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <asm/irq.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <linux/errno.h>

/*
 *  Chip Specific Head File
 */
#include <asm/danube/irq.h>
#include <asm/danube/danube_dma.h>
#include <asm/danube/danube_eth_d2.h>



/*
 * ####################################
 *              Board Type
 * ####################################
 */

#define BOARD_DANUBE                            1
#define BOARD_TWINPATH_E                        2



/*
 * ####################################
 *              Definition
 * ####################################
 */

#define ENABLE_D2                               1

#define ENABLE_DOWNSTREAM_FAST_PATH             1

#define BOARD_CONFIG                            BOARD_TWINPATH_E
//#define BOARD_CONFIG                            BOARD_DANUBE

#define THROTTLE_UPPER_LAYER_PACKET             0

#define ENABLE_ETH_DEBUG                        1

#define ENABLE_ETH_ASSERT                       1

#define DEBUG_DUMP_SKB_RX                       1

#define DEBUG_DUMP_SKB_TX                       1

#define DEBUG_DUMP_FLAG_HEADER                  0

#define DEBUG_DUMP_INIT                         0

#define DEBUG_HALT_PP32                         0

#define DEBUG_FIRMWARE_TABLES_PROC              0

#define DEBUG_FIRMWARE_PROC                     1

#define DEBUG_MEM_PROC                          1

#define DEBUG_DMA_PROC_UPSTREAM                 1

#define DEBUG_DMA_PROC_DOWNSTREAM               1

#define DEBUG_PP32_PROC                         1

#define ENABLE_PROBE_TRANSCEIVER                0

#define ENABLE_HW_FLOWCONTROL                   1

#define PPE_MAILBOX_IGU1_INT                    INT_NUM_IM2_IRL24

#define MY_ETH0_ADDR                            my_ethaddr

#define MY_ETH1_ADDR                            (my_ethaddr + MAX_ADDR_LEN)

#if defined(ENABLE_D2) && ENABLE_D2
  #include <asm/danube/danube_ppa_eth_fw_d2.h>
  #include <asm/danube/danube_ppa_eth_fw_d3.h>
#else
  #include <asm/danube/danube_eth_fw_d2.h>
#endif

//  default board related configuration
#define ENABLE_DANUBE_BOARD                     1
#define MII0_MODE_SETUP                         REV_MII_MODE
#define MII1_MODE_SETUP                         MII_MODE
#define ENABLE_MII1_TX_CLK_INVERSION            1

//  specific board related configuration
#if defined (BOARD_CONFIG)
  #if BOARD_CONFIG == BOARD_TWINPATH_E
    #undef  ENABLE_DANUBE_BOARD
    #define ENABLE_TWINPATH_E_BOARD             1

    #undef  MII1_MODE_SETUP
    #define MII1_MODE_SETUP                     REV_MII_MODE

    #undef  ENABLE_MII1_TX_CLK_INVERSION
    #define ENABLE_MII1_TX_CLK_INVERSION        0
  #endif
#endif

#if !defined(CONFIG_NET_HW_FLOWCONTROL)
  #undef  ENABLE_HW_FLOWCONTROL
  #define ENABLE_HW_FLOWCONTROL                 0
#endif

#if defined(ENABLE_ETH_DEBUG) && ENABLE_ETH_DEBUG
  #define ENABLE_DEBUG_PRINT                    1
  #define DISABLE_INLINE                        1
#else
  #define ENABLE_DEBUG_PRINT                    0
  #define DISABLE_INLINE                        0
#endif

#if !defined(DISABLE_INLINE) || !DISABLE_INLINE
  #define INLINE                                inline
#else
  #define INLINE
#endif

#if defined(ENABLE_DEBUG_PRINT) && ENABLE_DEBUG_PRINT
  #undef  dbg
  #define dbg(format, arg...)                   do { if ( dbg_enable ) printk(KERN_WARNING __FILE__ ":%d:%s: " format "\n", __LINE__, __FUNCTION__, ##arg); } while ( 0 )
#else
  #if !defined(dbg)
    #define dbg(format, arg...)
  #endif
#endif

#if defined(ENABLE_ETH_ASSERT) && ENABLE_ETH_ASSERT
  #define ETH_ASSERT(cond, format, arg...)      do { if ( dbg_enable && !(cond) ) printk(KERN_ERR __FILE__ ":%d:%s: " format "\n", __LINE__, __FUNCTION__, ##arg); } while ( 0 )
#else
  #define ETH_ASSERT(cond, format, arg...)
#endif

#if (defined(DEBUG_DUMP_SKB_RX) && DEBUG_DUMP_SKB_RX) || (defined(DEBUG_DUMP_SKB_TX) && DEBUG_DUMP_SKB_TX)
  #define DEBUG_DUMP_SKB                        1
#else
  #define DEBUG_DUMP_SKB                        0
#endif

#if defined(DEBUG_DUMP_SKB) && DEBUG_DUMP_SKB
  #define DUMP_SKB_LEN                          ~0
#endif

#if (defined(DEBUG_DUMP_SKB) && DEBUG_DUMP_SKB)                     \
    || (defined(DEBUG_DUMP_FLAG_HEADER) && DEBUG_DUMP_FLAG_HEADER)  \
    || (defined(DEBUG_DUMP_INIT) && DEBUG_DUMP_INIT)                \
    || (defined(ENABLE_DEBUG_PRINT) && ENABLE_DEBUG_PRINT)          \
    || (defined(ENABLE_ETH_ASSERT) && ENABLE_ETH_ASSERT)
  #define ENABLE_DBG_PROC                       1
#else
  #define ENABLE_DBG_PROC                       0
#endif

#if (defined(DEBUG_DMA_PROC_UPSTREAM) && DEBUG_DMA_PROC_UPSTREAM) || (defined(DEBUG_DMA_PROC_DOWNSTREAM) && DEBUG_DMA_PROC_DOWNSTREAM)
  #define DEBUG_DMA_PROC                        1
#else
  #define DEBUG_DMA_PROC                        0
#endif

/*
 *  Eth Mode
 */
#define MII_MODE                                1
#define REV_MII_MODE                            2

/*
 *  Default MII1 Hardware Configuration
 */
#define CDM_CFG_DEFAULT                         0x00000000
#define SB_MST_SEL_DEFAULT                      0x00000003
#if 0
  #define ENETS1_CFG_DEFAULT                    (0x00007037 | (RX_HEAD_MAC_ADDR_ALIGNMENT << 18) | (1 << 30))
#else
  #define ENETS1_CFG_DEFAULT                    (0x00007037 | (RX_HEAD_MAC_ADDR_ALIGNMENT << 18))
#endif
#define ENETF1_CFG_DEFAULT                      0x00007010
#define ENETS1_PGCNT_DEFAULT                    0x00020000
#define ENETS1_PKTCNT_DEFAULT                   0x00000200
#define ENETF1_PGCNT_DEFAULT                    0x00020000
#define ENETF1_PKTCNT_DEFAULT                   0x00000200
#define ENETS1_COS_CFG_DEFAULT                  0x00000002  // This enables multiple DMA channels when packets with VLAN is injected. COS mapping is through ETOP_IG_VLAN_COS; It is already preconfigured.
#define ENETS1_DBA_DEFAULT                      0x00000400
#define ENETS1_CBA_DEFAULT                      0x00000AE0
#define ENETF1_DBA_DEFAULT                      0x00001600
#define ENETF1_CBA_DEFAULT                      0x00001800

/*
 *  Constant Definition
 */
#define ETH_WATCHDOG_TIMEOUT                    (10 * HZ)
//#define ETOP_MDIO_PHY1_ADDR                     1
#define ETOP_MDIO_DELAY                         1
#define IDLE_CYCLE_NUMBER                       30000
#define ETH1_TX_TOTAL_CHANNEL_USED              MAX_TX_EMA_CHANNEL_NUMBER

#define DMA_PACKET_SIZE                         1568

#define FWCODE_ROUTING_ACC_D2                   2
#define FWCODE_BRIDGING_ACC_D3                  3

/*
 *  EMA TX Channel Parameters
 */
#define MAX_TX_EMA_CHANNEL_NUMBER               4
#define EMA_ALIGNMENT                           4

/*
 *  Ethernet Frame Definitions
 */
#define ETH_MAC_HEADER_LENGTH                   ETH_HLEN
#define ETH_CRC_LENGTH                          4
#define ETH_MIN_FRAME_LENGTH                    (ETH_ZLEN + ETH_CRC_LENGTH)
#define ETH_MAX_FRAME_LENGTH                    (ETH_FRAME_LEN + ETH_CRC_LENGTH)
#define ETH_MAX_DATA_LENGTH                     ETH_DATA_LEN

/*
 *  TX Frame Definitions
 */
#define MAX_TX_PACKET_ALIGN_BYTES               (EMA_ALIGNMENT - 1)
#define MAX_TX_PACKET_PADDING_BYTES             (EMA_ALIGNMENT - 1)
#define MIN_TX_PACKET_LENGTH                    ETH_ZLEN

/*
 *  EMA Settings
 */
#define EMA_CMD_BUF_LEN                         0x0040
#define EMA_CMD_BASE_ADDR                       (0x1580 << 2)
#define EMA_DATA_BUF_LEN                        0x0100
#define EMA_DATA_BASE_ADDR                      (0x1900 << 2)
#define EMA_WRITE_BURST                         0x02
#define EMA_READ_BURST                          0x02

/*
 *  Firmware Settings
 */
#define WAN_ROUT_NUM                            192
#define LAN_ROUT_NUM                            (384 - WAN_ROUT_NUM)
#define LAN_ROUT_OFF                            WAN_ROUT_NUM

#define BRIDGING_ENTRY_NUM                      256

#define DMA_RX_CH2_DESC_NUM                     24
#define DMA_TX_CH2_DESC_NUM                     DMA_RX_CH2_DESC_NUM
#if defined(ENABLE_D2) && ENABLE_D2 && (defined(ENABLE_DOWNSTREAM_FAST_PATH) && ENABLE_DOWNSTREAM_FAST_PATH)
  #define DMA_RX_CH1_DESC_NUM                   32
#else
  #define DMA_RX_CH1_DESC_NUM                   64
#endif
#define ETH1_TX_DESC_NUM                        32

/*
 *  Bits Operation
 */
#define GET_BITS(x, msb, lsb)                   (((x) & ((1 << ((msb) + 1)) - 1)) >> (lsb))
#define SET_BITS(x, msb, lsb, value)            (((x) & ~(((1 << ((msb) + 1)) - 1) ^ ((1 << (lsb)) - 1))) | (((value) & ((1 << (1 + (msb) - (lsb))) - 1)) << (lsb)))

/*
 *  FPI Configuration Bus Register and Memory Address Mapping
 */
#define DANUBE_PPE                              (KSEG1 + 0x1E180000)
#define PP32_DEBUG_REG_ADDR(x)                  ((volatile u32*)(DANUBE_PPE + (((x) + 0x0000) << 2)))
#define PPM_INT_REG_ADDR(x)                     ((volatile u32*)(DANUBE_PPE + (((x) + 0x0030) << 2)))
#define PP32_INTERNAL_RES_ADDR(x)               ((volatile u32*)(DANUBE_PPE + (((x) + 0x0040) << 2)))
#define PPE_CLOCK_CONTROL_ADDR(x)               ((volatile u32*)(DANUBE_PPE + (((x) + 0x0100) << 2)))
#define CDM_CODE_MEMORY_RAM0_ADDR(x)            ((volatile u32*)(DANUBE_PPE + (((x) + 0x1000) << 2)))
#define CDM_CODE_MEMORY_RAM1_ADDR(x)            ((volatile u32*)(DANUBE_PPE + (((x) + 0x2000) << 2)))
#define PPE_REG_ADDR(x)                         ((volatile u32*)(DANUBE_PPE + (((x) + 0x4000) << 2)))
#define PP32_DATA_MEMORY_RAM1_ADDR(x)           ((volatile u32*)(DANUBE_PPE + (((x) + 0x5000) << 2)))
#define PPM_INT_UNIT_ADDR(x)                    ((volatile u32*)(DANUBE_PPE + (((x) + 0x6000) << 2)))
#define PPM_TIMER0_ADDR(x)                      ((volatile u32*)(DANUBE_PPE + (((x) + 0x6100) << 2)))
#define PPM_TASK_IND_REG_ADDR(x)                ((volatile u32*)(DANUBE_PPE + (((x) + 0x6200) << 2)))
#define PPS_BRK_ADDR(x)                         ((volatile u32*)(DANUBE_PPE + (((x) + 0x6300) << 2)))
#define PPM_TIMER1_ADDR(x)                      ((volatile u32*)(DANUBE_PPE + (((x) + 0x6400) << 2)))
#define SB_RAM0_ADDR(x)                         ((volatile u32*)(DANUBE_PPE + (((x) + 0x8000) << 2)))
#define SB_RAM1_ADDR(x)                         ((volatile u32*)(DANUBE_PPE + (((x) + 0x8400) << 2)))
#define SB_RAM2_ADDR(x)                         ((volatile u32*)(DANUBE_PPE + (((x) + 0x8C00) << 2)))
#define SB_RAM3_ADDR(x)                         ((volatile u32*)(DANUBE_PPE + (((x) + 0x9600) << 2)))
#define QSB_CONF_REG(x)                         ((volatile u32*)(DANUBE_PPE + (((x) + 0xC000) << 2)))

/*
 *  DWORD-Length of Memory Blocks
 */
#define PP32_DEBUG_REG_DWLEN                    0x0030
#define PPM_INT_REG_DWLEN                       0x0010
#define PP32_INTERNAL_RES_DWLEN                 0x00C0
#define PPE_CLOCK_CONTROL_DWLEN                 0x0F00
#define CDM_CODE_MEMORY_RAM0_DWLEN              0x1000
#define CDM_CODE_MEMORY_RAM1_DWLEN              0x0800
#define PPE_REG_DWLEN                           0x1000
#define PP32_DATA_MEMORY_RAM1_DWLEN             CDM_CODE_MEMORY_RAM1_DWLEN
#define PPM_INT_UNIT_DWLEN                      0x0100
#define PPM_TIMER0_DWLEN                        0x0100
#define PPM_TASK_IND_REG_DWLEN                  0x0100
#define PPS_BRK_DWLEN                           0x0100
#define PPM_TIMER1_DWLEN                        0x0100
#define SB_RAM0_DWLEN                           0x0400
#define SB_RAM1_DWLEN                           0x0800
#define SB_RAM2_DWLEN                           0x0A00
#define SB_RAM3_DWLEN                           0x0400
#define QSB_CONF_REG_DWLEN                      0x0100

/*
 *  Host-PPE Communication Data Address Mapping
 */
#define ETX1_DMACH_ON                           PP32_DATA_MEMORY_RAM1_ADDR(0x010E)
#define ETH_PORTS_CFG                           ((volatile struct eth_ports_cfg *)PP32_DATA_MEMORY_RAM1_ADDR(0x0114))
#define LAN_ROUT_TBL_CFG                        ((volatile struct lan_rout_tbl_cfg *)PP32_DATA_MEMORY_RAM1_ADDR(0x0116))
#define WAN_ROUT_TBL_CFG                        ((volatile struct wan_rout_tbl_cfg *)PP32_DATA_MEMORY_RAM1_ADDR(0x0117))
#define GEN_MODE_CFG                            ((volatile struct gen_mode_cfg *)PP32_DATA_MEMORY_RAM1_ADDR(0x0118))
#define BRG_TBL_CFG                             PP32_DATA_MEMORY_RAM1_ADDR(0x0119)
#define ETH_DEFAULT_DEST_LIST(i)                PP32_DATA_MEMORY_RAM1_ADDR(0x011A + (i))
#define ETH1_TX_MIB_TBL                         ((volatile struct eth1_tx_mib_tbl *)PP32_DATA_MEMORY_RAM1_ADDR(0x0130))
//#define WAN_DMA_TX_CH_CFG_TBL(i)                PP32_DATA_MEMORY_RAM1_ADDR(0x0150 + (i) * 3)
#define VLAN_CFG_TBL(i)                         PP32_DATA_MEMORY_RAM1_ADDR(0x0170 + (i))
#define PPPOE_CFG_TBL(i)                        PP32_DATA_MEMORY_RAM1_ADDR(0x0180 + (i))
#define MTU_CFG_TBL(i)                          PP32_DATA_MEMORY_RAM1_ADDR(0x0190 + (i))
#define LAN_ROUT_MAC_CFG_TBL(i)                 PP32_DATA_MEMORY_RAM1_ADDR(0x01A0 + (i) * 2)
#define WAN_ROUT_MAC_CFG_TBL(i)                 PP32_DATA_MEMORY_RAM1_ADDR(0x01B0 + (i) * 2)
#define WAN_ROUT_FORWARD_ACTION_TBL(i)          ((volatile struct wan_rout_forward_action_tbl *)PP32_DATA_MEMORY_RAM1_ADDR(0x01C0 + (i) * 4))
#define LAN_ROUT_FORWARD_ACTION_TBL(i)          ((volatile struct lan_rout_forward_action_tbl *)PP32_DATA_MEMORY_RAM1_ADDR(0x01C0 + ((i) + LAN_ROUT_TBL_CFG->lan_rout_off) * 4))
#define LAN_RX_MIB_TBL                          ((volatile struct lan_rx_mib_tbl *)PP32_DATA_MEMORY_RAM1_ADDR(0x07C0))
#define WAN_RX_MIB_TBL                          ((volatile struct wan_rx_mib_tbl *)PP32_DATA_MEMORY_RAM1_ADDR(0x07D0))
#define BRIDGING_ETH_MIB_TBL(i)                 ((volatile struct bridging_eth_mib_tbl *)PP32_DATA_MEMORY_RAM1_ADDR(0x07C0 + (i) * 0x10))
#define ROUT_FWD_HIT_STAT_TBL(i)                PP32_DATA_MEMORY_RAM1_ADDR(0x07E0 + (i))
//#define BRG_FWD_HIT_STAT_TBL(i)                 ROUT_FWD_HIT_STAT_TBL(i)
#define MC_ROUT_FWD_HIT_STAT_TBL(i)             PP32_DATA_MEMORY_RAM1_ADDR(0x07F0 + (i))
#define FW_VER_ID                               ((volatile struct fw_ver_id *)SB_RAM0_ADDR(0x0001))
#define WAN_ROUT_MULTICAST_TBL(i)               ((volatile struct wan_rout_multicast_tbl *)SB_RAM0_ADDR(0x0010 + (i) * 2))
#define WAN_ROUT_FORWARD_COMPARE_TBL(i)         ((volatile struct rout_forward_compare_tbl *)SB_RAM0_ADDR(0x0090 + (i) * 4))
#define LAN_ROUT_FORWARD_COMPARE_TBL(i)         ((volatile struct rout_forward_compare_tbl *)SB_RAM0_ADDR(0x0090 + ((i) + LAN_ROUT_TBL_CFG->lan_rout_off) * 4))
#define BRIDGING_FORWARD_TBL(i)                 (SB_RAM0_ADDR(0x0090 + (i) * 4))
#define DMA_RX_CH2_DESC_BASE                    SB_RAM0_ADDR(0x0B90)
#define DMA_TX_CH2_DESC_BASE                    SB_RAM0_ADDR(0x0BC0)
#define DMA_RX_CH1_DESC_BASE                    SB_RAM0_ADDR(0x1400)
#define EMA_TX_CH_DESC_BASE(i)                  SB_RAM0_ADDR(0x1800 + (i) * 2 * ETH1_TX_DESC_NUM)

/*
 *  Share Buffer Registers
 */
#define SB_MST_SEL                              PPE_REG_ADDR(0x0304)

/*
 *    ETOP MDIO Registers
 */
#define ETOP_MDIO_CFG                           PPE_REG_ADDR(0x0600)
#define ETOP_MDIO_ACC                           PPE_REG_ADDR(0x0601)
#define ETOP_CFG                                PPE_REG_ADDR(0x0602)
#define ETOP_IG_VLAN_COS                        PPE_REG_ADDR(0x0603)
#define ETOP_IG_DSCP_COSx(x)                    PPE_REG_ADDR(0x0607 - ((x) & 0x03))
#define ETOP_IG_PLEN_CTRL0                      PPE_REG_ADDR(0x0608)
//#define ETOP_IG_PLEN_CTRL1                      PPE_REG_ADDR(0x0609)
#define ETOP_ISR                                PPE_REG_ADDR(0x060A)
#define ETOP_IER                                PPE_REG_ADDR(0x060B)
#define ETOP_VPID                               PPE_REG_ADDR(0x060C)
#define ENET_MAC_CFG(i)                         PPE_REG_ADDR(0x0610 + ((i) ? 0x40 : 0x00))
#define ENETS_DBA(i)                            PPE_REG_ADDR(0x0612 + ((i) ? 0x40 : 0x00))
#define ENETS_CBA(i)                            PPE_REG_ADDR(0x0613 + ((i) ? 0x40 : 0x00))
#define ENETS_CFG(i)                            PPE_REG_ADDR(0x0614 + ((i) ? 0x40 : 0x00))
#define ENETS_PGCNT(i)                          PPE_REG_ADDR(0x0615 + ((i) ? 0x40 : 0x00))
#define ENETS_PKTCNT(i)                         PPE_REG_ADDR(0x0616 + ((i) ? 0x40 : 0x00))
#define ENETS_BUF_CTRL(i)                       PPE_REG_ADDR(0x0617 + ((i) ? 0x40 : 0x00))
#define ENETS_COS_CFG(i)                        PPE_REG_ADDR(0x0618 + ((i) ? 0x40 : 0x00))
#define ENETS_IGDROP(i)                         PPE_REG_ADDR(0x0619 + ((i) ? 0x40 : 0x00))
#define ENETS_IGERR(i)                          PPE_REG_ADDR(0x061A + ((i) ? 0x40 : 0x00))
#define ENETS_MAC_DA0(i)                        PPE_REG_ADDR(0x061B + ((i) ? 0x40 : 0x00))
#define ENETS_MAC_DA1(i)                        PPE_REG_ADDR(0x061C + ((i) ? 0x40 : 0x00))
#define ENETF_DBA(i)                            PPE_REG_ADDR(0x0630 + ((i) ? 0x40 : 0x00))
#define ENETF_CBA(i)                            PPE_REG_ADDR(0x0631 + ((i) ? 0x40 : 0x00))
#define ENETF_CFG(i)                            PPE_REG_ADDR(0x0632 + ((i) ? 0x40 : 0x00))
#define ENETF_PGCNT(i)                          PPE_REG_ADDR(0x0633 + ((i) ? 0x40 : 0x00))
#define ENETF_PKTCNT(i)                         PPE_REG_ADDR(0x0634 + ((i) ? 0x40 : 0x00))
#define ENETF_HFCTRL(i)                         PPE_REG_ADDR(0x0635 + ((i) ? 0x40 : 0x00))
#define ENETF_TXCTRL(i)                         PPE_REG_ADDR(0x0636 + ((i) ? 0x40 : 0x00))
#define ENETF_VLCOS0(i)                         PPE_REG_ADDR(0x0638 + ((i) ? 0x40 : 0x00))
#define ENETF_VLCOS1(i)                         PPE_REG_ADDR(0x0639 + ((i) ? 0x40 : 0x00))
#define ENETF_VLCOS2(i)                         PPE_REG_ADDR(0x063A + ((i) ? 0x40 : 0x00))
#define ENETF_VLCOS3(i)                         PPE_REG_ADDR(0x063B + ((i) ? 0x40 : 0x00))
#define ENETF_EGCOL(i)                          PPE_REG_ADDR(0x063C + ((i) ? 0x40 : 0x00))
#define ENETF_EGDROP(i)                         PPE_REG_ADDR(0x063D + ((i) ? 0x40 : 0x00))

/*
 *  DPlus Registers
 */
#define DPLUS_TXDB                              PPE_REG_ADDR(0x0700)
#define DPLUS_TXCB                              PPE_REG_ADDR(0x0701)
#define DPLUS_TXCFG                             PPE_REG_ADDR(0x0702)
#define DPLUS_TXPGCNT                           PPE_REG_ADDR(0x0703)
#define DPLUS_RXDB                              PPE_REG_ADDR(0x0710)
#define DPLUS_RXCB                              PPE_REG_ADDR(0x0711)
#define DPLUS_RXCFG                             PPE_REG_ADDR(0x0712)
#define DPLUS_RXPGCNT                           PPE_REG_ADDR(0x0713)

/*
 *  EMA Registers
 */
#define EMA_CMDCFG                              PPE_REG_ADDR(0x0A00)
#define EMA_DATACFG                             PPE_REG_ADDR(0x0A01)
#define EMA_CMDCNT                              PPE_REG_ADDR(0x0A02)
#define EMA_DATACNT                             PPE_REG_ADDR(0x0A03)
#define EMA_ISR                                 PPE_REG_ADDR(0x0A04)
#define EMA_IER                                 PPE_REG_ADDR(0x0A05)
#define EMA_CFG                                 PPE_REG_ADDR(0x0A06)
#define EMA_SUBID                               PPE_REG_ADDR(0x0A07)

/*
 *  PP32 Debug Control Register
 */
#define PP32_DBG_CTRL                           PP32_DEBUG_REG_ADDR(0x0000)

#define DBG_CTRL_START_SET(value)               ((value) ? (1 << 0) : 0)
#define DBG_CTRL_STOP_SET(value)                ((value) ? (1 << 1) : 0)
#define DBG_CTRL_STEP_SET(value)                ((value) ? (1 << 2) : 0)

/*
 *  PP32 Registers
 */
#define PP32_HALT_STAT                          PP32_DEBUG_REG_ADDR(0x0001)

#define PP32_BRK_SRC                            PP32_DEBUG_REG_ADDR(0x0002)

#define PP32_DBG_PC_MIN(i)                      PP32_DEBUG_REG_ADDR(0x0010 + (i))
#define PP32_DBG_PC_MAX(i)                      PP32_DEBUG_REG_ADDR(0x0014 + (i))
#define PP32_DBG_DATA_MIN(i)                    PP32_DEBUG_REG_ADDR(0x0018 + (i))
#define PP32_DBG_DATA_MAX(i)                    PP32_DEBUG_REG_ADDR(0x001A + (i))
#define PP32_DBG_DATA_VAL(i)                    PP32_DEBUG_REG_ADDR(0x001C + (i))

#define PP32_DBG_CUR_PC                         PP32_DEBUG_REG_ADDR(0x0080)

#define PP32_DBG_TASK_NO                        PP32_DEBUG_REG_ADDR(0x0081)

/*
 *    Code/Data Memory (CDM) Interface Configuration Register
 */
#define CDM_CFG                                 PPE_REG_ADDR(0x0100)

#define CDM_CFG_RAM1_SET(value)                 SET_BITS(0, 3, 2, value)
#define CDM_CFG_RAM0_SET(value)                 ((value) ? (1 << 1) : 0)

/*
 *  ETOP MDIO Configuration Register
 */
#define ETOP_MDIO_CFG_SMRST(value)      ((value) ? (1 << 13) : 0)
#define ETOP_MDIO_CFG_PHYA(i, value)    ((i) ? SET_BITS(0, 12, 8, (value)) : SET_BITS(0, 7, 3, (value)))
#define ETOP_MDIO_CFG_UMM(i, value)     ((value) ? ((i) ? (1 << 2) : (1 << 1)) : 0)

#define ETOP_MDIO_CFG_MASK(i)           (ETOP_MDIO_CFG_SMRST(1) | ETOP_MDIO_CFG_PHYA(i, 0x1F) | ETOP_MDIO_CFG_UMM(i, 1))

/*
 *  ENet MAC Configuration Register
 */
#define ENET_MAC_CFG_CRC(i)             (*ENET_MAC_CFG(i) & (0x01 << 11))
#define ENET_MAC_CFG_DUPLEX(i)          (*ENET_MAC_CFG(i) & (0x01 << 2))
#define ENET_MAC_CFG_SPEED(i)           (*ENET_MAC_CFG(i) & (0x01 << 1))
#define ENET_MAC_CFG_LINK(i)            (*ENET_MAC_CFG(i) & 0x01)

#define ENET_MAC_CFG_CRC_OFF(i)         do { *ENET_MAC_CFG(i) &= ~(1 << 11); } while (0)
#define ENET_MAC_CFG_CRC_ON(i)          do { *ENET_MAC_CFG(i) |= 1 << 11; } while (0)
#define ENET_MAC_CFG_DUPLEX_HALF(i)     do { *ENET_MAC_CFG(i) &= ~(1 << 2); } while (0)
#define ENET_MAC_CFG_DUPLEX_FULL(i)     do { *ENET_MAC_CFG(i) |= 1 << 2; } while (0)
#define ENET_MAC_CFG_SPEED_10M(i)       do { *ENET_MAC_CFG(i) &= ~(1 << 1); } while (0)
#define ENET_MAC_CFG_SPEED_100M(i)      do { *ENET_MAC_CFG(i) |= 1 << 1; } while (0)
#define ENET_MAC_CFG_LINK_NOT_OK(i)     do { *ENET_MAC_CFG(i) &= ~1; } while (0)
#define ENET_MAC_CFG_LINK_OK(i)         do { *ENET_MAC_CFG(i) |= 1; } while (0)

/*
 *  ENets Configuration Register
 */
#define ENETS_CFG_VL2_SET               (1 << 29)
#define ENETS_CFG_VL2_CLEAR             ~(1 << 29)
#define ENETS_CFG_FTUC_SET              (1 << 28)
#define ENETS_CFG_FTUC_CLEAR            ~(1 << 28)
#define ENETS_CFG_DPBC_SET              (1 << 27)
#define ENETS_CFG_DPBC_CLEAR            ~(1 << 27)
#define ENETS_CFG_DPMC_SET              (1 << 26)
#define ENETS_CFG_DPMC_CLEAR            ~(1 << 26)

/*
 *  ENets Classification Configuration Register
 */
#define ENETS_COS_CFG_VLAN_SET          (1 << 1)
#define ENETS_COS_CFG_VLAN_CLEAR        ~(1 << 1)
#define ENETS_COS_CFG_DSCP_SET          (1 << 0)
#define ENETS_COS_CFG_DSCP_CLEAR        ~(1 << 0)

/*
 *  Mailbox IGU1 Registers
 */
#define MBOX_IGU1_ISRS                          PPE_REG_ADDR(0x0204)
#define MBOX_IGU1_ISRC                          PPE_REG_ADDR(0x0205)
#define MBOX_IGU1_ISR                           PPE_REG_ADDR(0x0206)
#define MBOX_IGU1_IER                           PPE_REG_ADDR(0x0207)



/*
 * ####################################
 *              Data Type
 * ####################################
 */

/*
 *  Host-PPE Communication Data Structure
 */
#if defined(__BIG_ENDIAN)
  struct eth_ports_cfg {
    unsigned int    wan_vlanid          :12;
    unsigned int    res1                :16;
    unsigned int    eth1_type           :2;
    unsigned int    eth0_type           :2;
  };

  struct lan_rout_tbl_cfg {
  #if defined(ENABLE_D2) && ENABLE_D2
    unsigned int    lan_rout_num        :9;
    unsigned int    lan_rout_off        :9;
    unsigned int    res1                :4;
  #else
    unsigned int    lan_rout_num        :8;
    unsigned int    lan_rout_off        :8;
    unsigned int    res1                :6;
  #endif
    unsigned int    lan_tcpudp_ver_en   :1;
    unsigned int    lan_ip_ver_en       :1;
    unsigned int    lan_tcpudp_err_drop :1;
    unsigned int    lan_rout_drop       :1;
    unsigned int    res2                :6;
  };

  struct wan_rout_tbl_cfg {
  #if defined(ENABLE_D2) && ENABLE_D2
    unsigned int    wan_rout_num        :9;
    unsigned int    wan_rout_mc_num     :7;
    unsigned int    res1                :4;
  #else
    unsigned int    wan_rout_num        :8;
    unsigned int    wan_rout_mc_num     :8;
    unsigned int    res1                :4;
  #endif
    unsigned int    wan_rout_mc_drop    :1;
    unsigned int    res2                :1;
    unsigned int    wan_tcpdup_ver_en   :1;
    unsigned int    wan_ip_ver_en       :1;
    unsigned int    wan_tcpudp_err_drop :1;
    unsigned int    wan_rout_drop       :1;
    unsigned int    res3                :6;
  };

  struct gen_mode_cfg {
    unsigned int    cpu1_fast_mode      :1;
    unsigned int    eth1_fast_mode      :1;
    unsigned int    res1                :6;
    unsigned int    dplus_wfq           :8;
    unsigned int    eth1_wfq            :8;
    unsigned int    wan_acc_mode        :2;
    unsigned int    lan_acc_mode        :2;
    unsigned int    res2                :4;
  };

  struct wan_rout_forward_action_tbl {
    //  0h
    unsigned int    wan_new_dest_port   :16;
    unsigned int    wan_new_dest_mac54  :16;
    //  4h
    unsigned int    wan_new_dest_mac30  :32;
    //  8h
    unsigned int    wan_new_dest_ip     :32;
    //  Ch
    unsigned int    rout_type           :2;
    unsigned int    new_dscp            :6;
    unsigned int    mtu_ix              :3;
    unsigned int    vlan_ins            :1;
    unsigned int    vlan_rm             :1;
    unsigned int    vlan_ix             :3;
    unsigned int    dest_list           :8;
    unsigned int    pppoe_mode          :1;
    unsigned int    pppoe_ix            :3;
    unsigned int    new_dscp_en         :1;
    unsigned int    dest_chid           :3;
  };

  struct lan_rout_forward_action_tbl {
    //  0h
    unsigned int    lan_new_src_port    :16;
    unsigned int    lan_new_dest_mac54  :16;
    //  4h
    unsigned int    lan_new_dest_mac30  :32;
    //  8h
    unsigned int    lan_new_src_ip      :32;
    //  Ch
    unsigned int    rout_type           :2;
    unsigned int    new_dscp            :6;
    unsigned int    mtu_ix              :3;
    unsigned int    vlan_ins            :1;
    unsigned int    vlan_rm             :1;
    unsigned int    vlan_ix             :3;
    unsigned int    dest_list           :8;
    unsigned int    pppoe_mode          :1;
    unsigned int    pppoe_ix            :3;
    unsigned int    new_dscp_en         :1;
    unsigned int    dest_chid           :3;
  };

  struct fw_ver_id {
    unsigned int    family              :4;
    unsigned int    fwtype              :4;
    unsigned int    interface           :4;
    unsigned int    fwmode              :4;
    unsigned int    major               :8;
    unsigned int    minor               :8;
  };

  struct wan_rout_multicast_tbl {
    //  0h
    unsigned int    new_src_mac_en      :1;
    unsigned int    res1                :10;
    unsigned int    vlan_ins            :1;
    unsigned int    vlan_rm             :1;
    unsigned int    vlan_ix             :3;
    unsigned int    dest_list           :8;
    unsigned int    pppoe_mode          :1;
    unsigned int    new_src_mac_ix      :3;
    unsigned int    res3                :1;
    unsigned int    dest_tx_chid        :3;
    //  4h
    unsigned int    wan_dest_ip         :32;
  };

  struct rout_forward_compare_tbl {
    //  0h
    unsigned int    src_ip              :32;
    //  4h
    unsigned int    dest_ip             :32;
    //  8h
    unsigned int    src_port            :16;
    unsigned int    dest_port           :16;
    //  Ch
    unsigned int    res1                :32;
  };

  struct tx_descriptor {
    //  0 - 3h
    unsigned int    own         :1;
    unsigned int    c           :1;
    unsigned int    sop         :1;
    unsigned int    eop         :1;
    unsigned int    byteoff     :5;
    unsigned int    res1        :5;
    unsigned int    iscell      :1;
    unsigned int    clp         :1;
    unsigned int    datalen     :16;
    //  4 - 7h
    unsigned int    res2        :4;
    unsigned int    dataptr     :28;
  };

    //  flag_header must have same fields as bridging_flag_header in 4-7h
  struct flag_header {
    //  0 - 3h
    unsigned int    rout_fwd_vld:1;
    unsigned int    rout_mc_vld :1;
    unsigned int    res1        :2;
    unsigned int    tcpudp_err  :1;
    unsigned int    tcpudp_chk  :1;
    unsigned int    is_udp      :1;
    unsigned int    is_tcp      :1;
    unsigned int    res2        :3; //  1h
    unsigned int    ip_offset   :5;
    unsigned int    is_pppoes   :1; //  2h
    unsigned int    res3        :1;
    unsigned int    is_ipv4     :1;
    unsigned int    is_vlan     :2;
    unsigned int    res4        :2;
    unsigned int    rout_index  :9;
    //  4 - 7h
    unsigned int    dest_list   :8;
    unsigned int    src_itf     :1; //  5h
    unsigned int    src_dir     :1;
    unsigned int    acc_done    :1;
    unsigned int    res5        :1;
    unsigned int    tcp_fin     :1;
    unsigned int    dest_chid   :3;
    unsigned int    res6        :8; //  6h
    unsigned int    res7        :3; //  7h
    unsigned int    pl_byteoff  :5;
  };

  struct bridging_flag_header {
    //  0 - 3h
    unsigned int    temp_dest_list  :8;
    unsigned int    res1            :24;//  1h
    //  4 - 7h
    unsigned int    dest_list       :8;
    unsigned int    src_itf         :1; //  5h
    unsigned int    src_dir         :1;
    unsigned int    acc_done        :1;
    unsigned int    res2            :2;
    unsigned int    dest_chid       :3;
    unsigned int    res3            :8; //  6h
    unsigned int    res4            :3; //  7h
    unsigned int    pl_byteoff      :5;
  };
#else
#endif

struct eth1_tx_mib_tbl {
    unsigned long   etx_total_pdu;
    unsigned long   etx_total_bytes;
};

struct wan_rx_mib_tbl {
    unsigned long   wrx_fast_uc_tcp_pkts;   //  0
    unsigned long   wrx_fast_uc_udp_pkts;
    unsigned long   wrx_fast_mc_tcp_pkts;
    unsigned long   wrx_fast_mc_udp_pkts;
    unsigned long   wrx_fast_drop_tcp_pkts; //  4
    unsigned long   wrx_fast_drop_udp_pkts;
    unsigned long   res1;
    unsigned long   res2;
    unsigned long   wrx_drop_pkts;          //  8
};

struct lan_rx_mib_tbl {
    unsigned long   lrx_fast_tcp_pkts;      //  0
    unsigned long   lrx_fast_udp_pkts;
    unsigned long   res1;
    unsigned long   res2;
    unsigned long   lrx_fast_drop_tcp_pkts; //  4
    unsigned long   lrx_fast_drop_udp_pkts;
    unsigned long   res3;
    unsigned long   res4;
    unsigned long   lrx_drop_pkts;          //  8
};

struct bridging_eth_mib_tbl {
    unsigned long   ig_fast_pkts;       //  0
    unsigned long   ig_fast_bytes;
    unsigned long   ig_cpu_pkts;
    unsigned long   ig_cpu_bytes;
    unsigned long   ig_drop_pkts;       //  4
    unsigned long   ig_drop_bytes;
    unsigned long   eg_fast_pkts;
    unsigned long   eg_fast_bytes;
};

struct eth_dev {
    struct  net_device_stats        stats;

    u32                             enets_igerr;
    u32                             enets_igdrop;
    u32                             enetf_egcol;
    u32                             enetf_egdrop;

    u32                             rx_packets;
    u32                             rx_bytes;
    u32                             rx_errors;
    u32                             rx_dropped;
    u32                             tx_packets;
    u32                             tx_bytes;
    u32                             tx_errors;
    u32                             tx_dropped;

    u32                             rx_fastpath_packets;

#if defined(ENABLE_HW_FLOWCONTROL) && ENABLE_HW_FLOWCONTROL
    int                             fc_bit;                     /*  net wakeup callback                     */
#endif
};

struct eth_dev_tx_ch {
    struct tx_descriptor           *base;
    unsigned int                    alloc_pos;
    struct sk_buff                **skb_pointers;

    u32                             in_fast_path;
    struct semaphore                lock;
};



/*
 * ####################################
 *             Declaration
 * ####################################
 */

/*
 *  Network Operations
 */
static int eth_init(struct net_device *);
static struct net_device_stats *eth_get_stats(struct net_device *);
static int eth_open(struct net_device *);
static int eth_stop(struct net_device *);
static int eth0_hard_start_xmit(struct sk_buff *, struct net_device *);
static int eth1_hard_start_xmit(struct sk_buff *, struct net_device *);
static int eth_set_mac_address(struct net_device *, void *);
static int eth_ioctl(struct net_device *, struct ifreq *, int);
static int eth_change_mtu(struct net_device *, int);
static void eth0_tx_timeout(struct net_device *);
static void eth1_tx_timeout(struct net_device *);

/*
 *  Network operations help functions
 */
static INLINE int get_port(struct net_device *);
static INLINE int get_pair_port(struct net_device *);
static INLINE int eth1_alloc_tx_desc(int, int *);
static INLINE int eth0_xmit(struct sk_buff *, int);
static INLINE int eth1_xmit(struct sk_buff *, int);
#if defined(ENABLE_HW_FLOWCONTROL) && ENABLE_HW_FLOWCONTROL
  static void eth_xon(struct net_device *);
#endif
#if defined(ENABLE_PROBE_TRANSCEIVER) && ENABLE_PROBE_TRANSCEIVER
  static INLINE int probe_transceiver(struct net_device *);
#endif

/*
 *  Buffer manage functions
 */
static INLINE struct sk_buff *alloc_skb_rx(void);
#if 0
  static INLINE struct sk_buff *alloc_skb_tx(unsigned int);
#endif

/*
 *  Mailbox handler
 */
static void mailbox_irq_handler(int, void *, struct pt_regs *);

/*
 *  DMA interface functions
 */
static u8 *dma_buffer_alloc(int, int *, void **);
static int dma_buffer_free(u8 *, void *);
static int dma_int_handler(struct dma_device_info *, int);
static INLINE int dma_rx_int_handler(struct dma_device_info *);

/*
 *  ioctl help functions
 */
static INLINE int ethtool_ioctl(struct net_device *, struct ifreq *);
static INLINE void set_vlan_cos(struct vlan_cos_req *);
static INLINE void set_dscp_cos(struct dscp_cos_req *);

/*
 *  Hardware init functions
 */
static INLINE void clear_share_buffer(void);
static INLINE void clear_cdm(void);
static INLINE void init_pmu(void);
static INLINE void init_gpio(int);
static INLINE void init_etop(int, int, int);
static INLINE void start_etop(void);
static INLINE void init_ema(void);
static INLINE void init_mailbox(void);
static INLINE void board_init(void);

/*
 *  PP32 specific functions
 */
static INLINE int pp32_download_code(const u32 *, unsigned int, const u32 *, unsigned int);
static INLINE int pp32_specific_init(int, void *);
static INLINE int pp32_start(int);
static INLINE void pp32_stop(void);

/*
 *  Init & clean-up functions
 */
static INLINE int init_local_variables(void);
static INLINE void init_communication_data_structures(int);
static INLINE int alloc_dma(void);
static INLINE void clear_local_variables(void);
static INLINE void free_dma(void);
static INLINE void ethaddr_setup(unsigned int, char *);

/*
 *  Proc File
 */
static INLINE void proc_file_create(void);
static INLINE void proc_file_delete(void);
static int proc_read_mib(char *, char **, off_t, int, int *, void *);
static int proc_write_mib(struct file *, const char *, unsigned long, void *);
#if defined(DEBUG_FIRMWARE_TABLES_PROC) && DEBUG_FIRMWARE_TABLES_PROC
  static int proc_read_route(char *, char **, off_t, int, int *, void *);
  static int proc_write_route(struct file *, const char *, unsigned long, void *);
  static int proc_read_genconf(char *, char **, off_t, int, int *, void *);
  static int proc_write_genconf(struct file *, const char *, unsigned long, void *);
  static int proc_read_vlan(char *, char **, off_t, int, int *, void *);
  static int proc_write_vlan(struct file *, const char *, unsigned long, void *);
  static int proc_read_pppoe(char *, char **, off_t, int, int *, void *);
  static int proc_write_pppoe(struct file *, const char *, unsigned long, void *);
  static int proc_read_mtu(char *, char **, off_t, int, int *, void *);
  static int proc_write_mtu(struct file *, const char *, unsigned long, void *);
  static int proc_read_hit(char *, char **, off_t, int, int *, void *);
  static int proc_write_hit(struct file *, const char *, unsigned long, void *);
  static int proc_read_mac(char *, char **, off_t, int, int *, void *);
  static int proc_write_mac(struct file *, const char *, unsigned long, void *);
#endif
#if defined(ENABLE_DBG_PROC) && ENABLE_DBG_PROC
  static int proc_read_dbg(char *, char **, off_t, int, int *, void *);
  static int proc_write_dbg(struct file *, const char *, unsigned long, void *);
#endif
#if defined(DEBUG_FIRMWARE_PROC) && DEBUG_FIRMWARE_PROC
  static int proc_read_fw(char *, char **, off_t, int, int *, void *);
#endif
#if defined(DEBUG_MEM_PROC) && DEBUG_MEM_PROC
  static int proc_write_mem(struct file *, const char *, unsigned long, void *);
#endif
#if defined(DEBUG_DMA_PROC) && DEBUG_DMA_PROC
  static int proc_read_dma(char *, char **, off_t, int, int *, void *);
  static int proc_write_dma(struct file *, const char *, unsigned long, void *);
#endif
#if defined(DEBUG_PP32_PROC) && DEBUG_PP32_PROC
  static int proc_read_pp32(char *, char **, off_t, int, int *, void *);
  static int proc_write_pp32(struct file *, const char *, unsigned long, void *);
#endif
static INLINE int stricmp(const char *, const char *);
#if defined(DEBUG_FIRMWARE_TABLES_PROC) && DEBUG_FIRMWARE_TABLES_PROC
static INLINE int strincmp(const char *, const char *, int);
#endif
static INLINE int get_token(char **, char **, int *, int *);
static INLINE int get_number(char **, int *, int);
#if defined(DEBUG_FIRMWARE_TABLES_PROC) && DEBUG_FIRMWARE_TABLES_PROC
static INLINE void get_ip_port(char **, int *, unsigned int *);
static INLINE void get_mac(char **, int *, unsigned int *);
#endif
static INLINE void ignore_space(char **, int *);
#if defined(DEBUG_FIRMWARE_TABLES_PROC) && DEBUG_FIRMWARE_TABLES_PROC
static INLINE int print_wan_route(char *, int, struct rout_forward_compare_tbl *, struct wan_rout_forward_action_tbl *);
static INLINE int print_lan_route(char *, int, struct rout_forward_compare_tbl *, struct lan_rout_forward_action_tbl *);
#endif
#if defined(DEBUG_DMA_PROC) && DEBUG_DMA_PROC
  static INLINE int print_dma_desc(char *, int, int, int);
#endif
#if defined(DEBUG_DMA_PROC_UPSTREAM) && DEBUG_DMA_PROC_UPSTREAM
  static INLINE int print_ema_desc(char *, int, int, int);
#endif

/*
 *  Debug functions
 */
#if defined(DEBUG_DUMP_SKB) && DEBUG_DUMP_SKB
  static INLINE void dump_skb(struct sk_buff *, u32, char *, int);
#endif
#if defined(DEBUG_DUMP_FLAG_HEADER) && DEBUG_DUMP_FLAG_HEADER
  static INLINE void dump_flag_header(int, struct flag_header *, char *, int);
#endif
#if defined(DEBUG_DUMP_INIT) && DEBUG_DUMP_INIT
  static INLINE void dump_init(void);
#endif
#if 0
  static void force_dump_skb(struct sk_buff *, u32, char *, int);
  static void force_dump_flag_header(struct flag_header *, char *, int);
#endif



/*
 * ####################################
 *            Local Variable
 * ####################################
 */

static int g_fwcode = FWCODE_ROUTING_ACC_D2;
//static int g_fwcode = FWCODE_BRIDGING_ACC_D3;

static struct sk_buff         **eth1_skb_pointers_addr;
static u32                      eth1_dev_tx_irq;
static struct eth_dev_tx_ch     eth1_tx_ch[ETH1_TX_TOTAL_CHANNEL_USED];

static int                      f_dma_opened;
static struct dma_device_info  *dma_device;
static struct sk_buff          *dma_ch2_skb_pointers[DMA_TX_CH2_DESC_NUM];
static u32                      dma_ch2_tx_not_run;

static struct eth_dev eth_dev[2];

static struct net_device eth_net_dev[2] = {
    {
        name:   "eth0",
        init:   eth_init,
    },
    {
        name:   "eth1",
        init:   eth_init,
    }
};

static u8 my_ethaddr[MAX_ADDR_LEN * 2] = {0};

static struct proc_dir_entry *g_eth_proc_dir;

#if defined(ENABLE_DBG_PROC) && ENABLE_DBG_PROC
  static u32 dbg_enable;
#endif

#if defined(DEBUG_DMA_PROC) && DEBUG_DMA_PROC
  static u32 dbg_dma_enable = 0x03;
#endif

#if defined(CONFIG_IFX_NFEXT_SWITCH_PHYPORT) || defined(CONFIG_IFX_NFEXT_SWITCH_PHYPORT_MODULE)
  int (*amazon_sw_phyport_rx)(struct net_device *, struct sk_buff *) = NULL;
  EXPORT_SYMBOL(amazon_sw_phyport_rx);

  struct net_device *amazon_sw_for_phyport = &(eth_net_dev[0]);
  EXPORT_SYMBOL(amazon_sw_for_phyport);
#endif



/*
 * ####################################
 *           Global Variable
 * ####################################
 */



/*
 * ####################################
 *            Local Function
 * ####################################
 */

static int eth_init(struct net_device *dev)
{
    int port;
    u8 *ethaddr;
    u32 val;
    int i;

    if ( dev == eth_net_dev )
    {
        port = 0;
        ethaddr = MY_ETH0_ADDR;
    }
    else
    {
        port = 1;
        ethaddr = MY_ETH1_ADDR;
    }

    ether_setup(dev);   /*  assign some members */

    /*  hook network operations */
    dev->get_stats       = eth_get_stats;
    dev->open            = eth_open;
    dev->stop            = eth_stop;
    dev->hard_start_xmit = port ? eth1_hard_start_xmit : eth0_hard_start_xmit;
    dev->set_mac_address = eth_set_mac_address;
    dev->do_ioctl        = eth_ioctl;
    dev->change_mtu      = eth_change_mtu;
    dev->tx_timeout      = port ? eth1_tx_timeout : eth0_tx_timeout;
    dev->watchdog_timeo  = ETH_WATCHDOG_TIMEOUT;
    dev->priv            = eth_dev + port;

    SET_MODULE_OWNER(dev);

    /*  read MAC address from the MAC table and put them into device    */
    val = 0;
    for ( i = 0; i < 6; i++ )
        val += ethaddr[i];
    if ( val == 0 )
    {
        /*  ethaddr not set in u-boot   */
        dev->dev_addr[0] = 0x00;
		dev->dev_addr[1] = 0x20;
		dev->dev_addr[2] = 0xda;
		dev->dev_addr[3] = 0x86;
		dev->dev_addr[4] = 0x23;
		dev->dev_addr[5] = 0x74 + port;
	}
	else
	{
	    for ( i = 0; i < 6; i++ )
	        dev->dev_addr[i] = ethaddr[i];
	}

	return 0;   //  Qi Ming set 1 here in Switch driver
}

static struct net_device_stats *eth_get_stats(struct net_device *dev)
{
    int port;

    port = get_port(dev);

    eth_dev[port].enets_igerr  += *ENETS_IGERR(port);
    eth_dev[port].enets_igdrop += *ENETS_IGDROP(port);
    eth_dev[port].enetf_egcol  += *ENETF_EGCOL(port);
    eth_dev[port].enetf_egdrop += *ENETF_EGDROP(port);

    switch ( g_fwcode )
    {
    case FWCODE_ROUTING_ACC_D2:
        if ( port == 0 )
        {
#if defined(ENABLE_D2) && ENABLE_D2 && (defined(ENABLE_DOWNSTREAM_FAST_PATH) && ENABLE_DOWNSTREAM_FAST_PATH)
            eth_dev[0].stats.rx_packets = eth_dev[0].rx_packets + LAN_RX_MIB_TBL->lrx_fast_tcp_pkts + LAN_RX_MIB_TBL->lrx_fast_udp_pkts;
#else
            eth_dev[0].stats.rx_packets = eth_dev[0].rx_packets + eth_dev[0].rx_fastpath_packets;
#endif
            eth_dev[0].stats.rx_bytes   = eth_dev[0].rx_bytes;

            eth_dev[0].stats.rx_errors  = eth_dev[0].rx_errors  + eth_dev[0].enets_igerr  + LAN_RX_MIB_TBL->lrx_drop_pkts;
#if defined(ENABLE_D2) && ENABLE_D2 && (defined(ENABLE_DOWNSTREAM_FAST_PATH) && ENABLE_DOWNSTREAM_FAST_PATH)
            eth_dev[0].stats.rx_dropped = eth_dev[0].rx_dropped + eth_dev[0].enets_igdrop + LAN_RX_MIB_TBL->lrx_fast_drop_tcp_pkts + LAN_RX_MIB_TBL->lrx_fast_drop_udp_pkts;
#else
            eth_dev[0].stats.rx_dropped = eth_dev[0].rx_dropped + eth_dev[0].enets_igdrop + LAN_RX_MIB_TBL->lrx_fast_drop_tcp_pkts + LAN_RX_MIB_TBL->lrx_fast_drop_udp_pkts + (LAN_RX_MIB_TBL->lrx_fast_tcp_pkts + LAN_RX_MIB_TBL->lrx_fast_udp_pkts - eth_dev[0].rx_fastpath_packets);
#endif

            eth_dev[0].stats.tx_packets = eth_dev[0].tx_packets;
            eth_dev[0].stats.tx_bytes   = eth_dev[0].tx_bytes;
            eth_dev[0].stats.tx_errors  = eth_dev[0].tx_errors  + eth_dev[0].enetf_egcol;
            eth_dev[0].stats.tx_dropped = eth_dev[0].tx_dropped + eth_dev[0].enetf_egdrop;
        }
        else
        {
            eth_dev[1].stats.rx_packets = eth_dev[1].rx_packets + WAN_RX_MIB_TBL->wrx_fast_uc_tcp_pkts + WAN_RX_MIB_TBL->wrx_fast_uc_udp_pkts + WAN_RX_MIB_TBL->wrx_fast_mc_tcp_pkts + WAN_RX_MIB_TBL->wrx_fast_mc_udp_pkts;
            eth_dev[1].stats.rx_bytes   = eth_dev[1].rx_bytes;

            eth_dev[1].stats.rx_errors  = eth_dev[1].rx_errors  + eth_dev[1].enets_igerr  + WAN_RX_MIB_TBL->wrx_drop_pkts;
            eth_dev[1].stats.rx_dropped = eth_dev[1].rx_dropped + eth_dev[1].enets_igdrop + WAN_RX_MIB_TBL->wrx_fast_drop_tcp_pkts + WAN_RX_MIB_TBL->wrx_fast_drop_udp_pkts;

            eth_dev[1].stats.tx_packets = eth_dev[1].tx_packets;    // + ETH1_TX_MIB_TBL->etx_total_pdu;
            eth_dev[1].stats.tx_bytes   = eth_dev[1].tx_bytes;      //   + ETH1_TX_MIB_TBL->etx_total_bytes;
            eth_dev[1].stats.tx_errors  = eth_dev[1].tx_errors  + eth_dev[1].enetf_egcol;
            eth_dev[1].stats.tx_dropped = eth_dev[1].tx_dropped + eth_dev[1].enetf_egdrop;
        }
        break;

    case FWCODE_BRIDGING_ACC_D3:
        eth_dev[port].stats.rx_packets = eth_dev[port].rx_packets + BRIDGING_ETH_MIB_TBL(port)->ig_fast_pkts;
        eth_dev[port].stats.rx_bytes   = eth_dev[port].rx_bytes + BRIDGING_ETH_MIB_TBL(port)->ig_fast_bytes;
        eth_dev[port].stats.rx_errors  = eth_dev[port].rx_errors  + eth_dev[port].enets_igerr;
        eth_dev[port].stats.rx_dropped = eth_dev[port].rx_dropped + eth_dev[port].enets_igdrop + BRIDGING_ETH_MIB_TBL(port)->ig_drop_pkts + (eth_dev[port].rx_packets - BRIDGING_ETH_MIB_TBL(port)->ig_cpu_pkts);
        eth_dev[port].stats.tx_packets = eth_dev[port].tx_packets + BRIDGING_ETH_MIB_TBL(port)->eg_fast_pkts;
        eth_dev[port].stats.tx_bytes   = eth_dev[port].tx_bytes + BRIDGING_ETH_MIB_TBL(port)->eg_fast_bytes;
        eth_dev[port].stats.tx_errors  = eth_dev[port].tx_errors  + eth_dev[port].enetf_egcol;
        eth_dev[port].stats.tx_dropped = eth_dev[port].tx_dropped + eth_dev[port].enetf_egdrop;
        break;
    }

    return &eth_dev[port].stats;
}

static int eth_open(struct net_device *dev)
{
    int i;

    MOD_INC_USE_COUNT;

#if defined(ENABLE_PROBE_TRANSCEIVER) && ENABLE_PROBE_TRANSCEIVER
    if ( !probe_transceiver(dev) )
    {
        printk("%s cannot work because of hardware problem\n", dev->name);
        MOD_DEC_USE_COUNT;
        return -1;
    }
#endif  //  defined(ENABLE_PROBE_TRANSCEIVER) && ENABLE_PROBE_TRANSCEIVER
    dbg("%s", dev->name);

#if defined(ENABLE_HW_FLOWCONTROL) && ENABLE_HW_FLOWCONTROL
    if ( (eth_dev[get_port(dev)].fc_bit = netdev_register_fc(dev, eth_xon)) == 0 )
        printk("Hardware Flow Control register fails\n");
#endif

    if ( !f_dma_opened )
    {
        ETH_ASSERT((u32)dma_device >= 0x80000000, "dma_device = 0x%08X", (u32)dma_device);

        for ( i = 0; i < dma_device->max_rx_chan_num; i++ )
        {
            ETH_ASSERT((u32)dma_device->rx_chan[i] >= 0x80000000, "dma_device->rx_chan[%d] = 0x%08X", i, (u32)dma_device->rx_chan[i]);
            ETH_ASSERT(dma_device->rx_chan[i]->control == DANUBE_DMA_CH_ON, "dma_device->rx_chan[i]->control = %d", dma_device->rx_chan[i]->control);

            if ( dma_device->rx_chan[i]->control == DANUBE_DMA_CH_ON )
            {
                ETH_ASSERT((u32)dma_device->rx_chan[i]->open >= 0x80000000, "dma_device->rx_chan[%d]->open = 0x%08X", i, (u32)dma_device->rx_chan[i]->open);

                if ( i == 1 || i == 2 )
                    dma_device->rx_chan[i]->dir = 1;
                dma_device->rx_chan[i]->open(dma_device->rx_chan[i]);
                if ( i == 1 || i == 2 )
                    dma_device->rx_chan[i]->dir = -1;
            }
        }
    }
    f_dma_opened |= 1 << get_port(dev);

    netif_start_queue(dev);

    return 0;
}

static int eth_stop(struct net_device *dev)
{
#if defined(ENABLE_HW_FLOWCONTROL) && ENABLE_HW_FLOWCONTROL
    int port;
#endif
    int i;

    f_dma_opened &= ~(1 << get_port(dev));
    if ( !f_dma_opened )
        for ( i = 0; i < dma_device->max_rx_chan_num; i++ )
            dma_device->rx_chan[i]->close(dma_device->rx_chan[i]);

#if defined(ENABLE_HW_FLOWCONTROL) && ENABLE_HW_FLOWCONTROL
    port = get_port(dev);
    if ( eth_dev[port].fc_bit )
        netdev_unregister_fc(eth_dev[port].fc_bit);
#endif

    netif_stop_queue(dev);
    MOD_DEC_USE_COUNT;

    return 0;
}

static int eth0_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    return eth0_xmit(skb, 3);
}

static int eth1_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    return eth1_xmit(skb, 3);
}

static int eth_set_mac_address(struct net_device *dev, void *p)
{
    struct sockaddr *addr = (struct sockaddr *)p;

    printk("%s: change MAC from %02X:%02X:%02X:%02X:%02X:%02X to %02X:%02X:%02X:%02X:%02X:%02X\n", dev->name,
        dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2], dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5],
        addr->sa_data[0], addr->sa_data[1], addr->sa_data[2], addr->sa_data[3], addr->sa_data[4], addr->sa_data[5]);

    memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

    return 0;
}

static int eth_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    int port;

    port = get_port(dev);
    switch ( cmd )
    {
    case SIOCETHTOOL:
        return ethtool_ioctl(dev, ifr);
    case SET_VLAN_COS:
        {
            struct vlan_cos_req vlan_cos_req;

            if ( copy_from_user(&vlan_cos_req, ifr->ifr_data, sizeof(struct vlan_cos_req)) )
                return -EFAULT;
            set_vlan_cos(&vlan_cos_req);
        }
        break;
    case SET_DSCP_COS:
        {
            struct dscp_cos_req dscp_cos_req;

            if ( copy_from_user(&dscp_cos_req, ifr->ifr_data, sizeof(struct dscp_cos_req)) )
                return -EFAULT;
            set_dscp_cos(&dscp_cos_req);
        }
        break;
    case ENABLE_VLAN_CLASSIFICATION:
        *ENETS_COS_CFG(port) |= ENETS_COS_CFG_VLAN_SET;    break;
    case DISABLE_VLAN_CLASSIFICATION:
        *ENETS_COS_CFG(port) &= ENETS_COS_CFG_VLAN_CLEAR;  break;
    case ENABLE_DSCP_CLASSIFICATION:
        *ENETS_COS_CFG(port) |= ENETS_COS_CFG_DSCP_SET;    break;
    case DISABLE_DSCP_CLASSIFICATION:
        *ENETS_COS_CFG(port) &= ENETS_COS_CFG_DSCP_CLEAR;  break;
    case VLAN_CLASS_FIRST:
        *ENETS_CFG(port) &= ENETS_CFG_FTUC_CLEAR;          break;
    case VLAN_CLASS_SECOND:
        *ENETS_CFG(port) |= ENETS_CFG_VL2_SET;             break;
    case PASS_UNICAST_PACKETS:
        *ENETS_CFG(port) &= ENETS_CFG_FTUC_CLEAR;          break;
    case FILTER_UNICAST_PACKETS:
        *ENETS_CFG(port) |= ENETS_CFG_FTUC_SET;            break;
    case KEEP_BROADCAST_PACKETS:
        *ENETS_CFG(port) &= ENETS_CFG_DPBC_CLEAR;          break;
    case DROP_BROADCAST_PACKETS:
        *ENETS_CFG(port) |= ENETS_CFG_DPBC_SET;            break;
    case KEEP_MULTICAST_PACKETS:
        *ENETS_CFG(port) &= ENETS_CFG_DPMC_CLEAR;          break;
    case DROP_MULTICAST_PACKETS:
        *ENETS_CFG(port) |= ENETS_CFG_DPMC_SET;
    default:
        return -EOPNOTSUPP;
    }

    return 0;
}

static int eth_change_mtu(struct net_device *dev, int new_mtu)
{
    printk(KERN_ERR __FILE__ ":%d:%s: not implemented\n", __LINE__, __FUNCTION__);

    return 0;
}

static void eth0_tx_timeout(struct net_device *dev)
{
    int i;

    //TODO:must restart the TX channels

    eth_dev[0].tx_errors++;

    for ( i = 0; i < dma_device->max_tx_chan_num; i++ )
    {
        if ( i == 2 )
            continue;

        dma_device->tx_chan[i]->disable_irq(dma_device->tx_chan[i]);
    }

    netif_wake_queue(dev);

    return;
}

static void eth1_tx_timeout(struct net_device *dev)
{
    u32 sys_flag;

    ETH_ASSERT(dev == eth_net_dev + 1, "dev != &eth_net_dev[1]");

    //  must restart TX channel (pending)

    /*  disable TX irq, release skb when sending new packet */
    local_irq_save(sys_flag);
    *MBOX_IGU1_IER = eth1_dev_tx_irq = 0;
    local_irq_restore(sys_flag);

    /*  wake up TX queue    */
    netif_wake_queue(dev);

    return;
}

static INLINE int get_port(struct net_device *dev)
{
    return dev == eth_net_dev ? 0 : 1;
}

static INLINE int get_pair_port(struct net_device *dev)
{
    return dev == eth_net_dev ? 1 : 0;
}

/*
 *  Description:
 *    Allocate a TX descriptor for DMA channel.
 *  Input:
 *    ch     --- int, connection ID
 *    f_full --- int *, a pointer to get descriptor full flag
 *               1: full, 0: not full
 *  Output:
 *    int    --- negative value: descriptor is used up.
 *               else:           index of descriptor relative to the first one
 *                               of this channel.
 */
static INLINE int eth1_alloc_tx_desc(int ch, int *f_full)
{
    int desc_pos;
    struct eth_dev_tx_ch *pch;
    int need_lock;

    ETH_ASSERT(f_full, "pointer \"f_full\" must be valid!");
    *f_full = 1;

    pch = eth1_tx_ch + ch;
    if ( !pch->in_fast_path && !in_irq() )
    {
        need_lock = 1;
        down(&pch->lock);
    }
    else
        need_lock = 0;

    desc_pos = pch->alloc_pos;
    if ( !pch->base[desc_pos].own )    //  hold by MIPS
    {
        if ( ++(pch->alloc_pos) == ETH1_TX_DESC_NUM )
            pch->alloc_pos = 0;

        if ( !pch->base[pch->alloc_pos].own )    //  hold by MIPS
            *f_full = 0;
    }
    else
        desc_pos = -1;

    if ( need_lock )
        up(&pch->lock);

    return desc_pos;
}

static INLINE int eth0_xmit(struct sk_buff *skb, int ch)
{
    int len;

    len = skb->len < MIN_TX_PACKET_LENGTH ? MIN_TX_PACKET_LENGTH : skb->len;

    dma_device->current_tx_chan = ch;

#if defined(DEBUG_DUMP_SKB_TX) && DEBUG_DUMP_SKB_TX
    dump_skb(skb, DUMP_SKB_LEN, "eth0_xmit", ch);
#endif

    eth_net_dev[0].trans_start = jiffies;

    eth_dev[0].tx_packets++;
    eth_dev[0].tx_bytes += len;

    if ( dma_device_write(dma_device,
                          skb->data,
                          len,
                          skb)
         != len )
    {
        dev_kfree_skb_any(skb);
        eth_dev[0].tx_errors++;
	    eth_dev[0].tx_dropped++;
    }

    return 0;
}

static INLINE int eth1_xmit(struct sk_buff *skb, int ch)
{
    int f_full;
    int desc_pos;
#if 0
    struct tx_descriptor reg_desc = {0};
#endif
    struct tx_descriptor *desc;

#if defined(DEBUG_DUMP_SKB_TX) && DEBUG_DUMP_SKB_TX
    dump_skb(skb, DUMP_SKB_LEN, "eth1_xmit", ch);
#endif

#if defined(ENABLE_D2) && ENABLE_D2
    if ( ch == 1 )
        ch = 0;
#endif

    eth_dev[1].tx_packets++;
    eth_dev[1].tx_bytes += skb->len;

    //  allocate descriptor
    desc_pos = eth1_alloc_tx_desc(ch, &f_full);
    if ( f_full )
    {
        u32 sys_flag;
        u32 bit;

//        printk("eth1_xmit: f_full\n");

        bit = 1 << (ch + 16);

        local_irq_save(sys_flag);
        *MBOX_IGU1_ISRC = bit;
        if ( !eth1_dev_tx_irq )
            enable_irq(PPE_MAILBOX_IGU1_INT);
        eth1_dev_tx_irq |= bit;
        if ( eth1_dev_tx_irq == (((1 << ETH1_TX_TOTAL_CHANNEL_USED) - 1) << 16) )
        {
            eth_net_dev[1].trans_start = jiffies;
            netif_stop_queue(eth_net_dev + 1);
        }
        *MBOX_IGU1_IER = eth1_dev_tx_irq;
        local_irq_restore(sys_flag);
    }
    if ( desc_pos < 0 )
    {
        eth_dev[1].tx_dropped++;

        dev_kfree_skb_any(skb);

        return 0;
    }

    //  free existing pointer and update send pointer
    dev_kfree_skb_any(eth1_tx_ch[ch].skb_pointers[desc_pos]);
    eth1_tx_ch[ch].skb_pointers[desc_pos] = skb;

#if 0
    reg_desc.own     = 1;
    reg_desc.c       = 1;
    reg_desc.sop     = 1;
    reg_desc.eop     = 1;
    reg_desc.byteoff = (u32)skb->data & 3;
    reg_desc.datalen = skb->len < MIN_TX_PACKET_LENGTH ? MIN_TX_PACKET_LENGTH : skb->len;   //  if packet length is less than 60, pad it to 60 bytes
    reg_desc.dataptr = (u32)skb->data >> 2;

    desc = eth1_tx_ch[ch].base + desc_pos;

    //  write back cache and write descriptor to memory
    dma_cache_wback((unsigned long)skb->data, skb->len);
    *((u32*)desc + 1) = *((u32*)(&reg_desc) + 1);
    *(u32*)desc = *(u32*)(&reg_desc);
#else
    //  write back cache
    dma_cache_wback((unsigned long)skb->data, skb->len);

    desc = eth1_tx_ch[ch].base + desc_pos;
    *((u32*)desc + 1) = (u32)skb->data >> 2;
    *(u32*)desc = 0xF0000000 | (((u32)skb->data & 3) << 23) | (skb->len < MIN_TX_PACKET_LENGTH ? MIN_TX_PACKET_LENGTH : skb->len);
#endif

    eth_net_dev[1].trans_start = jiffies;

    return 0;
}

#if defined(ENABLE_HW_FLOWCONTROL) && ENABLE_HW_FLOWCONTROL
static void eth_xon(struct net_device *dev)
{
    clear_bit(eth_dev[get_port(dev)].fc_bit, &netdev_fc_xoff);
    if ( netif_running(dev) || netif_running(eth_net_dev + get_pair_port(dev)) )
    {
        if ( dma_device->rx_chan[0]->control == DANUBE_DMA_CH_ON )
                dma_device->rx_chan[0]->open(dma_device->rx_chan[0]);
        if ( dma_device->rx_chan[3]->control == DANUBE_DMA_CH_ON )
                dma_device->rx_chan[3]->open(dma_device->rx_chan[3]);
    }
}
#endif

#if defined(ENABLE_PROBE_TRANSCEIVER) && ENABLE_PROBE_TRANSCEIVER
/*
 *  Description:
 *    Setup ethernet hardware in init process.
 *  Input:
 *    dev --- struct net_device *, device to be setup.
 *  Output:
 *    int --- 0:    Success
 *            else: Error Code (-EIO, link is not OK)
 */
static INLINE int probe_transceiver(struct net_device *dev)
{
    int port;

    port = get_port(dev);

    *ENETS_MAC_DA0(port) = (dev->dev_addr[0] << 24) | (dev->dev_addr[1] << 16) | (dev->dev_addr[2] << 8) | dev->dev_addr[3];
    *ENETS_MAC_DA1(port) = (dev->dev_addr[4] << 24) | (dev->dev_addr[3] << 16);

    if ( !ENET_MAC_CFG_LINK(port) )
    {
        *ETOP_MDIO_CFG = (*ETOP_MDIO_CFG & ~ETOP_MDIO_CFG_MASK(port))
                         | ETOP_MDIO_CFG_SMRST(port)
                         | ETOP_MDIO_CFG_PHYA(port, port)
                         | ETOP_MDIO_CFG_UMM(port, 1);

        udelay(ETOP_MDIO_DELAY);

        if ( !ENET_MAC_CFG_LINK(port) )
            return -EIO;
    }

    return 0;
}
#endif

/*
 *  Description:
 *    Allocate a sk_buff for RX path using. The size is maximum packet size
 *    plus maximum overhead size.
 *  Input:
 *    none
 *  Output:
 *    sk_buff* --- 0:    Failed
 *                 else: Pointer to sk_buff
 */
static INLINE struct sk_buff *alloc_skb_rx(void)
{
    struct sk_buff *skb;

    skb = dev_alloc_skb(DMA_PACKET_SIZE + 16);
    if ( skb && ((u32)skb->data & 15) != 0 )
    {
//        printk("alloc_skb_rx: skb->data = %08X\n", (u32)skb->data);
        skb_reserve(skb, ~((u32)skb->data + 15) & 15);
    }

    return skb;
}

#if 0
/*
 *  Description:
 *    Allocate a sk_buff for TX path using.
 *  Input:
 *    size     --- unsigned int, size of the buffer
 *  Output:
 *    sk_buff* --- 0:    Failed
 *                 else: Pointer to sk_buff
 */
static INLINE struct sk_buff *alloc_skb_tx(unsigned int size)
{
    struct sk_buff *skb;

    skb = dev_alloc_skb(size + 16);
    if ( skb && ((u32)skb->data & 15) != 0 )
        skb_reserve(skb, ~((u32)skb->data + 15) & 15);
    return skb;
}
#endif

/*
 *  Description:
 *    Handle IRQ of mailbox and despatch to relative handler.
 *  Input:
 *    irq    --- int, IRQ number
 *    dev_id --- void *, argument passed when registering IRQ handler
 *    regs   --- struct pt_regs *, registers' value before jumping into handler
 *  Output:
 *    none
 */
static void mailbox_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    u32 tx_irq;

#if 0
    static u32 count = 0;
    printk("mailbox_irq_handler: count = %d, *MBOX_IGU1_IER = %08X, *MBOX_IGU1_ISR = %08X, eth1_dev_tx_irq = %08X, tx_irq = %08X\n",
                                 ++count,    *MBOX_IGU1_IER,        *MBOX_IGU1_ISR,        eth1_dev_tx_irq,        *MBOX_IGU1_ISR & eth1_dev_tx_irq);
#endif

    tx_irq = *MBOX_IGU1_ISR & eth1_dev_tx_irq;
//    *MBOX_IGU1_ISRC = *MBOX_IGU1_ISR;
    *MBOX_IGU1_ISRC = tx_irq;

    if ( dma_ch2_tx_not_run && (tx_irq & dma_ch2_tx_not_run) &&  (*DMA_TX_CH2_DESC_BASE & 0x80000000) )
    {
//        printk("dma_ch2_tx_not_run\n");
        dma_ch2_tx_not_run = 0;

        dma_device->tx_chan[2]->open(dma_device->tx_chan[2]);

        eth1_dev_tx_irq &= ((1 << ETH1_TX_TOTAL_CHANNEL_USED) - 1) << 16;
        *MBOX_IGU1_IER = eth1_dev_tx_irq;

        tx_irq &= ((1 << ETH1_TX_TOTAL_CHANNEL_USED) - 1) << 16;
    }

    if ( tx_irq )
    {
//        *MBOX_IGU1_ISRC = tx_irq;

        eth1_dev_tx_irq ^= tx_irq;
        *MBOX_IGU1_IER = eth1_dev_tx_irq;

        netif_wake_queue(&eth_net_dev[1]);
    }

    if ( !eth1_dev_tx_irq )
        disable_irq(PPE_MAILBOX_IGU1_INT);
}

static u8 *dma_buffer_alloc(int len, int *byte_offset, void **opt)
{
    u8 *buf;
    struct sk_buff *skb;

    skb = alloc_skb_rx();
    if ( !skb )
        return NULL;

    buf = (u8 *)skb->data;
    *(u32 *)opt = (u32)skb;
    *byte_offset = 0;
    return buf;
}

static int dma_buffer_free(u8 *dataptr, void *opt)
{
    if ( opt )
        dev_kfree_skb_any((struct sk_buff *)opt);
    else if ( dataptr )
        kfree(dataptr);

    return 0;
}

static int dma_int_handler(struct dma_device_info *dma_dev, int status)
{
    int ret = 0;
    int i;

    switch ( status )
    {
    case RCV_INT:
        ret = dma_rx_int_handler(dma_dev);
        break;
    case TX_BUF_FULL_INT:
        dbg("eth0 TX buffer full!");
        netif_stop_queue(&eth_net_dev[0]);
        for( i = 0; i < dma_device->max_tx_chan_num; i++ )
        {
            if ( i == 2 )
                continue;

            if ( dma_device->tx_chan[i]->control == DANUBE_DMA_CH_ON )
         	    dma_device->tx_chan[i]->enable_irq(dma_device->tx_chan[i]);
        }
        break;
    case TRANSMIT_CPT_INT:
        dbg("eth0 TX buffer released!");
	    for( i = 0; i < dma_device->max_tx_chan_num; i++ )
        {
            if ( i == 2 )
                continue;

            dma_device->tx_chan[i]->disable_irq(dma_device->tx_chan[i]);
        }
        netif_wake_queue(&eth_net_dev[0]);
        break;
    default:
        dbg("unkown DMA interrupt event - %d", status);
    }

    return ret;
}

static INLINE int dma_rx_int_handler(struct dma_device_info *dma_dev)
{
#if 1
    // use bit process of flag header to accelerate

    struct sk_buff *skb = NULL;
    int len;
    u32 header_val;
    u32 off;
    struct net_device *ndev;
    struct eth_dev *edev;
    u8 *buf;
  #if defined(THROTTLE_UPPER_LAYER_PACKET) && THROTTLE_UPPER_LAYER_PACKET
    u32 *ptr;
  #endif

    len = dma_device_read(dma_dev, &buf, (void **)&skb);

  #if defined(THROTTLE_UPPER_LAYER_PACKET) && THROTTLE_UPPER_LAYER_PACKET
    ptr = (u32*)skb->data;
  #endif

    if ( (u32)skb < 0x80000000 )
    {
        printk(KERN_ERR __FILE__ ":%d:%s: can not restore skb pointer (ch %d) --- skb = 0x%08X\n", __LINE__, __FUNCTION__, dma_dev->current_rx_chan, (u32)skb);
        eth_dev[0].rx_errors++;
        return 0;
    }

    //  work around due to some memory problem
    skb->data = skb->tail = buf;

    if ( len > (u32)skb->end - (u32)skb->data )
    {
  #if defined(DEBUG_DUMP_FLAG_HEADER) && DEBUG_DUMP_FLAG_HEADER
        dump_flag_header(g_fwcode, (struct flag_header *)skb->data, "dma_rx_int_handler", dma_dev->current_rx_chan);
  #endif

        printk(KERN_ERR __FILE__ ":%d:%s: packet is too large: %d\n", __LINE__, __FUNCTION__, len);
        dev_kfree_skb_any(skb);
        eth_dev[0].rx_errors++;
        return 0;
    }

  #if defined(DEBUG_DUMP_FLAG_HEADER) && DEBUG_DUMP_FLAG_HEADER
    dump_flag_header(g_fwcode, (struct flag_header *)skb->data, "dma_rx_int_handler", dma_dev->current_rx_chan);
  #endif

    header_val = *((u32 *)skb->data + 1);
  #if 0
    printk("\n");
    printk("1. header_val = %08X\n", header_val);
    printk("2. header_val & %X = %08X\n", ((1 << 5) - 1), header_val & ((1 << 5) - 1));
    printk("3. header_val & %X = %08X\n", header_val & ((1 << 21) | (1 << 19) | 0xFF000000));
    printk("4. (header_val >> 23) & 0x01 = %08X\n", (header_val >> 23) & 0x01);
    printk("5. (header_val >> 16) & 0x07 = %08X\n", (header_val >> 16) & 0x07);
  #endif

    //  pl_byteoff
    off = header_val & ((1 << 5) - 1);

    len -= sizeof(struct flag_header) + off + ETH_CRC_LENGTH;

    skb->data += sizeof(struct flag_header) + off;
    skb->tail = skb->data + len;
    skb->len  = len;

  #if defined(DEBUG_DUMP_SKB_RX) && DEBUG_DUMP_SKB_RX
    dump_skb(skb, DUMP_SKB_LEN, "dma_rx_int_handler", dma_dev->current_rx_chan);
  #endif

    if ( (header_val & ((1 << 21) | (1 << 19) | 0xFF000000)) == 0x02200000 )    //  D3 (bridging) does not have "tcp_fin"
    {
        eth_dev[0].rx_fastpath_packets++;
        eth_dev[0].rx_bytes += len;

        off  = (header_val >> 16) & 0x07;
        eth1_tx_ch[off].in_fast_path = 1;
        eth1_xmit(skb, off);
        eth1_tx_ch[off].in_fast_path = 0;

        return 0;
    }

  #if 0
    off = (header_val >> 23) & 0x01;

    edev = eth_dev + off;

    edev->rx_packets++;
    edev->rx_bytes += len;

    if ( off == 0 )
    {
        off = (header_val >> 16) & 0x07;
        eth1_tx_ch[off].in_fast_path = 1;
        eth1_xmit(skb, off);
        eth1_tx_ch[off].in_fast_path = 0;
    }
    else
        eth0_xmit(skb, (header_val >> 16) & 0x07);

    return 0;
  #endif

    off = (header_val >> 23) & 0x01;

    edev = eth_dev + off;
    ndev = eth_net_dev + off;

    if ( netif_running(ndev) )
    {
  #if defined(ENABLE_DANUBE_BOARD) && ENABLE_DANUBE_BOARD
    #if defined(CONFIG_IFX_NFEXT_SWITCH_PHYPORT) || defined(CONFIG_IFX_NFEXT_SWITCH_PHYPORT_MODULE)
        if ( off == 0 )
        {
            if ( amazon_sw_phyport_rx(ndev, skb) < 0 )
            {
                edev->rx_errors++;
                return 0;
            }
        }
        else
    #endif
  #endif
        {
            skb->dev = ndev;
            skb->protocol = eth_type_trans(skb, ndev);
        }

  #if defined(THROTTLE_UPPER_LAYER_PACKET) && THROTTLE_UPPER_LAYER_PACKET
    #if 1
        if (
      #if 0 && (defined(ENABLE_DBG_PROC) && ENABLE_DBG_PROC)
             dbg_enable &&
      #endif
             edev == eth_dev )
        {
            char str[512];
            int strlen;
            int i;
            u32 *p;

            strlen  = sprintf(str,            "eth0 (ch %d): buf = %08X, skb->data = %08X", dma_dev->current_rx_chan, (u32)buf, (u32)ptr);
            p = (u32)buf < (u32)ptr ? (u32*)buf : ptr;
            strlen += sprintf(str + strlen, "\n   %08X:", (u32)p);
            for ( i = 0; i < 8; i++ )
                strlen += sprintf(str + strlen, " %08X", *p++);
            strlen += sprintf(str + strlen, "\n   %08X:", (u32)p);
            for ( i = 0; i < 8; i++ )
                strlen += sprintf(str + strlen, " %08X", *p++);
            strlen += sprintf(str + strlen, "\n   %08X:", (u32)p);
            for ( i = 0; i < 8; i++ )
                strlen += sprintf(str + strlen, " %08X", *p++);
            str[strlen++] = '\n';
            str[strlen] = 0;
            printk(str);
        }
    #endif

        edev->rx_packets++;
        edev->rx_bytes += len;
        dev_kfree_skb_any(skb);
        return 0;
  #else
        if ( netif_rx(skb) == NET_RX_DROP )
        {
    #if defined(ENABLE_HW_FLOWCONTROL) && ENABLE_HW_FLOWCONTROL
            if ( edev->fc_bit && !test_and_set_bit(edev->fc_bit, &netdev_fc_xoff) )
            {
                dma_device->rx_chan[0]->close(dma_device->rx_chan[0]);
                dma_device->rx_chan[3]->close(dma_device->rx_chan[3]);
            }
    #endif

            edev->rx_dropped++;
        }
        else
        {
            edev->rx_packets++;
            edev->rx_bytes += len;
        }

        return 0;
  #endif
    }
    else
    {
        edev->rx_dropped++;
        dev_kfree_skb_any(skb);
        return 0;
    }

#else
    struct sk_buff *skb = NULL;
    u8 *buf = NULL;
    int len;
    struct flag_header *header;
    struct net_device *ndev;
    struct eth_dev *edev;

    len = dma_device_read(dma_dev, &buf, (void **)&skb);

    if ( (u32)skb < 0x80000000 )
    {
        dbg("can not restore skb pointer --- skb = 0x%08X", (u32)skb);
        skb = NULL;
        goto DMA_RX_INT_HANDLER_ERR;
    }

    if ( len > (u32)skb->end - (u32)skb->data )
    {
  #if defined(DEBUG_DUMP_FLAG_HEADER) && DEBUG_DUMP_FLAG_HEADER
        dump_flag_header(g_fwcode, (struct flag_header *)skb->data, "dma_rx_int_handler", dma_dev->current_rx_chan);
  #endif

        dbg("packet is too large: %d", len);
        goto DMA_RX_INT_HANDLER_ERR;
    }

    header = (struct flag_header *)skb->data;

  #if defined(DEBUG_DUMP_FLAG_HEADER) && DEBUG_DUMP_FLAG_HEADER
    dump_flag_header(g_fwcode, header, "dma_rx_int_handler", dma_dev->current_rx_chan);
  #endif

    len -= sizeof(struct flag_header) + header->pl_byteoff + ETH_CRC_LENGTH;

  #if 0
    skb_reserve(skb, sizeof(struct flag_header) + header->pl_byteoff);
    skb_put(skb, len);
  #else
    skb->data += sizeof(struct flag_header) + header->pl_byteoff;
    skb->tail = skb->data + len;
    skb->len  = len;
  #endif

  #if defined(DEBUG_DUMP_SKB_RX) && DEBUG_DUMP_SKB_RX
    dump_skb(skb, DUMP_SKB_LEN, "dma_rx_int_handler", dma_dev->current_rx_chan);
  #endif

    dbg("header->acc_done(%d), header->tcp_fin(%d), header->dest_list(%X)", header->acc_done, header->tcp_fin, header->dest_list);

    if ( header->acc_done && !header->tcp_fin && header->dest_list == 0x02 )    //  D3 (bridging) does not have "tcp_fin"
    {
        dbg("eth1_xmit");

        eth_dev[0].rx_fastpath_packets++;
        eth_dev[0].rx_bytes += len;

        eth1_tx_ch[header->dest_chid].in_fast_path = 1;
        eth1_xmit(skb, header->dest_chid);
        eth1_tx_ch[header->dest_chid].in_fast_path = 0;

        return 0;
    }

  #if 0
    edev = eth_dev + header->src_itf;

    edev->rx_fastpath_packets++;
    edev->rx_bytes += len;

    eth1_tx_ch[header->dest_chid].in_fast_path = 1;
    eth1_xmit(skb, header->dest_chid);
    eth1_tx_ch[header->dest_chid].in_fast_path = 0;

    return 0;
  #endif

    edev = eth_dev + header->src_itf;
    ndev = eth_net_dev + header->src_itf;

    if ( netif_running(ndev) )
    {
        dbg("netif_rx");

        skb->dev = ndev;
        skb->protocol = eth_type_trans(skb, ndev);

  #if defined(THROTTLE_UPPER_LAYER_PACKET) && THROTTLE_UPPER_LAYER_PACKET
        edev->rx_packets++;
        edev->rx_bytes += len;

        if ( edev == eth_dev )
        {
            char str[128];
            int strlen;
            int i;
            u32 *p;

            strlen = sprintf(str, "eth0:");
            for ( i = 0, p = (u32*)header; i < 8; i++ )
                strlen += sprintf(str + strlen, " %08X", *p++);
            str[strlen++] = '\n';
            str[strlen] = 0;
            printk(str);
        }

        dev_kfree_skb_any(skb);
  #else
        if ( netif_rx(skb) == NET_RX_DROP )
        {
    #if defined(ENABLE_HW_FLOWCONTROL) && ENABLE_HW_FLOWCONTROL
            if ( edev->fc_bit && !test_and_set_bit(edev->fc_bit, &netdev_fc_xoff) )
            {
                dma_device->rx_chan[0]->close(dma_device->rx_chan[0]);
                dma_device->rx_chan[3]->close(dma_device->rx_chan[3]);
            }
    #endif

            edev->rx_dropped++;
        }
        else
        {
            edev->rx_packets++;
            edev->rx_bytes += len;
        }
  #endif
    }
    else
    {
        edev->rx_dropped++;

        dev_kfree_skb_any(skb);
    }

    return 0;

DMA_RX_INT_HANDLER_ERR:
    if ( skb )
        dev_kfree_skb_any(skb);
    eth_dev[0].rx_errors++;

    return 0;
#endif
}

/*
 *  Description:
 *    Handle ioctl command SIOCETHTOOL.
 *  Input:
 *    dev --- struct net_device *, device responsing to the command.
 *    ifr --- struct ifreq *, interface request structure to pass parameters
 *            or result.
 *  Output:
 *    int --- 0:    Success
 *            else: Error Code (-EFAULT, -EOPNOTSUPP)
 */
static INLINE int ethtool_ioctl(struct net_device *dev, struct ifreq *ifr)
{
    int port;
    struct ethtool_cmd cmd;

    if ( copy_from_user(&cmd, ifr->ifr_data, sizeof(cmd)) )
        return -EFAULT;

    port = get_port(dev);

    switch ( cmd.cmd )
    {
    case ETHTOOL_GSET:      /*  get hardware information        */
        {
            memset(&cmd, 0, sizeof(cmd));

            cmd.supported   = SUPPORTED_Autoneg | SUPPORTED_TP | SUPPORTED_MII |
                              SUPPORTED_10baseT_Half | SUPPORTED_10baseT_Full |
                              SUPPORTED_100baseT_Half | SUPPORTED_100baseT_Full;
            cmd.port        = PORT_MII;
            cmd.transceiver = XCVR_EXTERNAL;
            cmd.phy_address = port;
            cmd.speed       = ENET_MAC_CFG_SPEED(port) ? SPEED_100 : SPEED_10;
            cmd.duplex      = ENET_MAC_CFG_DUPLEX(port) ? DUPLEX_FULL : DUPLEX_HALF;

            if ( (*ETOP_MDIO_CFG & ETOP_MDIO_CFG_UMM(port, 1)) )
            {
                /*  auto negotiate  */
                cmd.autoneg = AUTONEG_ENABLE;
                cmd.advertising |= ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full |
                                   ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full;
            }
            else
            {
                cmd.autoneg = AUTONEG_DISABLE;
                cmd.advertising &= ~(ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full |
                                     ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full);
            }

            if ( copy_to_user(ifr->ifr_data, &cmd, sizeof(cmd)) )
                return -EFAULT;
        }
        break;
    case ETHTOOL_SSET:      /*  force the speed and duplex mode */
        {
            if ( !capable(CAP_NET_ADMIN) )
                return -EPERM;

            if ( cmd.autoneg == AUTONEG_ENABLE )
            {
                /*  set property and start autonegotiation                                  */
                /*  have to set mdio advertisement register and restart autonegotiation     */
                /*  which is a very rare case, put it to future development if necessary.   */
            }
            else
            {
                /*  set property without autonegotiation    */
                *ETOP_MDIO_CFG &= ~ETOP_MDIO_CFG_UMM(port, 1);

                /*  set speed   */
                if ( cmd.speed == SPEED_10 )
                    ENET_MAC_CFG_SPEED_10M(port);
                else if ( cmd.speed == SPEED_100 )
                    ENET_MAC_CFG_SPEED_100M(port);

                /*  set duplex  */
                if ( cmd.duplex == DUPLEX_HALF )
                    ENET_MAC_CFG_DUPLEX_HALF(port);
                else if ( cmd.duplex == DUPLEX_FULL )
                    ENET_MAC_CFG_DUPLEX_FULL(port);

                ENET_MAC_CFG_LINK_OK(port);
            }
        }
        break;
    case ETHTOOL_GDRVINFO:  /*  get driver information          */
        {
            struct ethtool_drvinfo info;
            char str[32];

            memset(&info, 0, sizeof(info));
            strncpy(info.driver, "Danube Eth Driver", sizeof(info.driver) - 1);
            sprintf(str, "%d.%d.%d", FW_VER_ID->family, FW_VER_ID->major, FW_VER_ID->minor);
            strncpy(info.fw_version, str, sizeof(info.fw_version) - 1);
            strncpy(info.bus_info, "N/A", sizeof(info.bus_info) - 1);
            info.regdump_len = 0;
            info.eedump_len = 0;
            info.testinfo_len = 0;
            if ( copy_to_user(ifr->ifr_data, &info, sizeof(info)) )
                return -EFAULT;
        }
        break;
    case ETHTOOL_NWAY_RST:  /*  restart auto negotiation        */
        *ETOP_MDIO_CFG |= ETOP_MDIO_CFG_SMRST(port) | ETOP_MDIO_CFG_UMM(port, 1);
        break;
    default:
        return -EOPNOTSUPP;
    }

    return 0;
}

/*
 *  Description:
 *    Specify ETOP ingress VLAN priority's class of service mapping.
 *  Input:
 *    req --- struct vlan_cos_req *, pass parameters such as priority and class
 *            of service mapping.
 *  Output:
 *    none
 */
static INLINE void set_vlan_cos(struct vlan_cos_req *req)
{
     *ETOP_IG_VLAN_COS = SET_BITS(*ETOP_IG_VLAN_COS, (req->pri << 1) + 1, req->pri << 1, req->cos_value);
}

/*
 *  Description:
 *    Specify ETOP ingress VLAN differential service control protocol's class of
 *    service mapping.
 *  Input:
 *    req --- struct dscp_cos_req *, pass parameters such as differential
 *            service control protocol and class of service mapping.
 *  Output:
 *    none
 */
static INLINE void set_dscp_cos(struct dscp_cos_req *req)
{
    *ETOP_IG_DSCP_COSx(req->dscp >> 4) = SET_BITS(*ETOP_IG_DSCP_COSx(req->dscp >> 4), ((req->dscp & 0x0F) << 1) + 1, (req->dscp & 0x0F) << 1, req->cos_value);
}

static INLINE void clear_share_buffer(void)
{
    volatile u32 *p = SB_RAM0_ADDR(0);
    unsigned int i;

    /*  write all zeros only    */
    for ( i = 0; i < SB_RAM0_DWLEN + SB_RAM1_DWLEN + SB_RAM2_DWLEN + SB_RAM3_DWLEN; i++ )
        *p++ = 0;

    //  Configure share buffer master selection
    *SB_MST_SEL |= 0x03;
}

static INLINE void clear_cdm(void)
{
    volatile u32 *dest;
    int i;

    //  Both blocks are set to code memory
    *CDM_CFG = CDM_CFG_RAM1_SET(0x01) | CDM_CFG_RAM0_SET(0x00);
    for ( i = 0; i < 1000; i++ );

    dest = CDM_CODE_MEMORY_RAM0_ADDR(0);
    for ( i = 0; i < CDM_CODE_MEMORY_RAM0_DWLEN + CDM_CODE_MEMORY_RAM1_DWLEN; i++ )
        *dest++ = 0;
}

static INLINE void init_pmu(void)
{
    //  Enable PPE in PMU
#if 0
    *(unsigned long *)0xBF10201C &= ~((1 << 15) | (1 << 13));
#else
    *(unsigned long *)0xBF10201C &= ~((1 << 15) | (1 << 13) | (1 << 12) | (1 << 5));
#endif
}

static INLINE void init_gpio(int mii1_mode)
{
    *(unsigned long *)0xBE100B1C = (*(unsigned long *)0xBE100B1C & 0x00009859) | 0x00006786;
    *(unsigned long *)0xBE100B20 = (*(unsigned long *)0xBE100B20 & 0x00009859) | 0x000067a6;
    *(unsigned long *)0xBE100B4C = (*(unsigned long *)0xBE100B4C & 0x00000fa7) | 0x0000e058;
	*(unsigned long *)0xBE100B50 = (*(unsigned long *)0xBE100B50 & 0x00000fa7) | 0x0000f058;

	if ( mii1_mode == MII_MODE )
	{
	    *(unsigned long *)0xBE100B18 = (*(unsigned long *)0xBE100B18 & 0x00009859) | 0x00006020;
		*(unsigned long *)0xBE100B48 = (*(unsigned long *)0xBE100B48 & 0x00000fa7) | 0x00007000;
		*(unsigned long *)0xBE100B24 = *(unsigned long *)0xBE100B24 | 0x00006020;
		*(unsigned long *)0xBE100B54 = *(unsigned long *)0xBE100B54 | 0x00007000;
		dbg("MII1 = MII_MODE");
	}
	else
	{
	    *(unsigned long *)0xBE100B18 = (*(unsigned long *)0xBE100B18 & 0x00009859) | 0x00000786;
		*(unsigned long *)0xBE100B48 = (*(unsigned long *)0xBE100B48 & 0x00000fa7) | 0x00008058;
		*(unsigned long *)0xBE100B24 = *(unsigned long *)0xBE100B24 | 0x00000786;
		*(unsigned long *)0xBE100B54 = *(unsigned long *)0xBE100B54 | 0x00000058;
		dbg("MII1 = REV_MII_MODE");
	}

    dbg("DANUBE_GPIO_P0_ALTSEL0(0xBE100B1C) = 0x%08X", *(unsigned int *)0xBE100B1C);
	dbg("DANUBE_GPIO_P0_ALTSEL1(0xBE100B20) = 0x%08X", *(unsigned int *)0xBE100B20);
	dbg("DANUBE_GPIO_P1_ALTSEL0(0xBE100B4C) = 0x%08X", *(unsigned int *)0xBE100B4C);
	dbg("DANUBE_GPIO_P1_ALTSEL1(0xBE100B50) = 0x%08X", *(unsigned int *)0xBE100B50);
	dbg("DANUBE_GPIO_P0_DIR(0xBE100B18)     = 0x%08X", *(unsigned int *)0xBE100B18);
	dbg("DANUBE_GPIO_P1_DIR(0xBE100B48)     = 0x%08X", *(unsigned int *)0xBE100B48);
	dbg("DANUBE_GPIO_P0_OD(0xBE100B24)      = 0x%08X", *(unsigned int *)0xBE100B24);
	dbg("DANUBE_GPIO_P1_OD(0xBE100B54)      = 0x%08X", *(unsigned int *)0xBE100B54);
}

static INLINE void init_etop(int fwcode, int mii0_mode, int mii1_mode)
{
    int i;

    //  reset ETOP
    *((volatile u32 *)(0xBF203000 + 0x0010)) |= (1 << 8);
    for ( i = 0; i < 0x1000; i++ );

    //  disable both eth0 and eth1
#if !defined(ENABLE_MII1_TX_CLK_INVERSION) || !ENABLE_MII1_TX_CLK_INVERSION
    *ETOP_CFG = (*ETOP_CFG | (1 << 0) | (1 << 3)) & ~((1 << 6) | (1 << 8) | (1 << 7) | (1 << 9) | (1 << 10) | (1 << 11));
#else
    *ETOP_CFG = (*ETOP_CFG | (1 << 0) | (1 << 3) | (1 << 11)) & ~((1 << 6) | (1 << 8) | (1 << 7) | (1 << 9) | (1 << 10));
#endif

#if 0   //  not necessary for Danube board after TX Threshold changed
  #if defined(ENABLE_DANUBE_BOARD) && ENABLE_DANUBE_BOARD
    //  set MII0 to turbo mode
    *ETOP_CFG |= 1 << 2;
  #endif
#endif

    //  set MII or revMII mode
    if ( mii0_mode == REV_MII_MODE )
        *ETOP_CFG |= 1 << 1;
    else
        *ETOP_CFG &= ~(1 << 1);
    if ( mii1_mode == REV_MII_MODE )
        *ETOP_CFG |= 1 << 4;
    else
        *ETOP_CFG &= ~(1 << 4);

    //  set packet length
    *ETOP_IG_PLEN_CTRL0 = 0x004005EE;

    *ETOP_MDIO_CFG    = 0x00;

    *ENET_MAC_CFG(0)  = 0x0807;

    *ENETS_DBA(0)     = 0x0C00;
    *ENETS_CBA(0)     = 0x1200;
    if ( fwcode == FWCODE_ROUTING_ACC_D2 )
        *ENETS_CFG(0) = 0x00585050;
    else
        *ENETS_CFG(0) = 0x00305050;
    *ENETS_PGCNT(0)   = 0x00020000;
    *ENETS_PKTCNT(0)  = 0x0200;

    *ENETF_DBA(0)     = 0x1250;
    *ENETF_CBA(0)     = 0x13F0;
    *ENETF_CFG(0)     = 0x700D;

    *ENET_MAC_CFG(1)  = 0x0807;

    *ENETS_DBA(1)     = 0x0690;
    *ENETS_CBA(1)     = 0x17B0;
#if defined(ENABLE_D2) && ENABLE_D2
    if ( fwcode == FWCODE_ROUTING_ACC_D2 )
        *ENETS_CFG(1) = 0x00585050;
    else
        *ENETS_CFG(1) = 0x00305050;
#else
    *ENETS_CFG(1)     = 0x00385050;
#endif
    *ENETS_PGCNT(1)   = ENETS1_PGCNT_DEFAULT;
    *ENETS_PKTCNT(1)  = ENETS1_PKTCNT_DEFAULT;
    *ENETS_COS_CFG(1) = ENETS1_COS_CFG_DEFAULT;

    *ENETF_DBA(1)     = 0x1600;
    *ENETF_CBA(1)     = 0x17A0;
    *ENETF_CFG(1)     = 0x700D;
    *ENETF_PGCNT(1)   = ENETF1_PGCNT_DEFAULT;
    *ENETF_PKTCNT(1)  = ENETF1_PKTCNT_DEFAULT;

    //  tx page is 13, start transmit threshold is 10
    *ENETF_TXCTRL(0)  = 0x0A;
    *ENETF_TXCTRL(1)  = 0x0A;

    *DPLUS_TXCFG      = 0x000D;
    *DPLUS_TXDB       = 0x1250;
    *DPLUS_TXCB       = 0x13F0;
    *DPLUS_TXCFG      = 0xF00D;
    *DPLUS_RXCFG      = 0x5050;
    *DPLUS_RXPGCNT    = 0x00040000;

    //  enable both eth0 and eth1
//    *ETOP_CFG = (*ETOP_CFG & ~((1 << 0) | (1 << 3))) | ((1 << 6) | (1 << 8) | (1 << 7) | (1 << 9));
}

static INLINE void start_etop(void)
{
#if 0
    u32 enets_cfg0, enets_cfg1;
    int i;

    //  set page num to 0
    enets_cfg0 = *ENETS_CFG(0);
    enets_cfg1 = *ENETS_CFG(1);
    *ENETS_CFG(0) = enets_cfg0 & ~0xFF;
    *ENETS_CFG(1) = enets_cfg1 & ~0xFF;
//    printk("+++ 1. *ENETS_CFG(0) = %08X, *ENETS_CFG(1) = %08X\n", *ENETS_CFG(0), *ENETS_CFG(1));
    //  enable both eth0 and eth1
    *ETOP_CFG = (*ETOP_CFG & ~((1 << 0) | (1 << 3))) | ((1 << 6) | (1 << 8) | (1 << 7) | (1 << 9));
    //  delay
    for ( i = 0; i < 0x1000; i++ );
    //  recover page num
    *ENETS_CFG(0) = enets_cfg0;
    *ENETS_CFG(1) = enets_cfg1;
//    printk("+++ 2. *ENETS_CFG(0) = %08X, *ENETS_CFG(1) = %08X\n", *ENETS_CFG(0), *ENETS_CFG(1));
#else
    //  enable both eth0 and eth1
    *ETOP_CFG = (*ETOP_CFG & ~((1 << 0) | (1 << 3))) | ((1 << 6) | (1 << 8) | (1 << 7) | (1 << 9));
#endif
}

static INLINE void init_ema(void)
{
    *EMA_CMDCFG  = (EMA_CMD_BUF_LEN << 16) | (EMA_CMD_BASE_ADDR >> 2);
    *EMA_DATACFG = (EMA_DATA_BUF_LEN << 16) | (EMA_DATA_BASE_ADDR >> 2);
    *EMA_IER     = 0x000000FF;
    *EMA_CFG     = EMA_READ_BURST | (EMA_WRITE_BURST << 2);
}

static INLINE void init_mailbox(void)
{
    *MBOX_IGU1_ISRC = 0xFFFFFFFF;
    *MBOX_IGU1_IER  = 0x00000000;   //  Don't need to enable RX interrupt, DMA driver handle RX path.
}

#if defined(ENABLE_DANUBE_BOARD) && ENABLE_DANUBE_BOARD
static INLINE void board_init(void)
{
}
#elif defined(ENABLE_TWINPATH_E_BOARD) && ENABLE_TWINPATH_E_BOARD
extern int ifx_sw_vlan_add(int, int, int);
extern int ifx_sw_vlan_del(int, int);
static INLINE void board_init(void)
{

#if 0
  #if 1

    int i, j;

    for(i = 0; i< 6; i++)
        for(j = 0; j<6; j++)
            ifx_sw_vlan_del(i, j);

    ifx_sw_vlan_add(1, 1, 0);
    ifx_sw_vlan_add(2, 2, 0);
    ifx_sw_vlan_add(4, 4, 0);
    ifx_sw_vlan_add(2, 1, 0);
    ifx_sw_vlan_add(4, 1, 0);
    ifx_sw_vlan_add(1, 2, 0);
    ifx_sw_vlan_add(2, 2, 0);
    ifx_sw_vlan_add(4, 2, 0);
    ifx_sw_vlan_add(1, 4, 0);
    ifx_sw_vlan_add(2, 4, 0);
    ifx_sw_vlan_add(4, 4, 0);

    ifx_sw_vlan_add(3, 3, 1);
    ifx_sw_vlan_add(5, 5, 1);
    ifx_sw_vlan_add(3, 5, 1);
    ifx_sw_vlan_add(5, 3, 1);

  #else

    int i, j;

    for(i = 0; i< 6; i++)
        for(j = 0; j<6; j++)
            ifx_sw_vlan_del(i, j);

    ifx_sw_vlan_add(1, 1);
    ifx_sw_vlan_add(2, 2);
    ifx_sw_vlan_add(4, 4);
    ifx_sw_vlan_add(2, 1);
    ifx_sw_vlan_add(4, 1);
    ifx_sw_vlan_add(1, 2);
    ifx_sw_vlan_add(2, 2);
    ifx_sw_vlan_add(4, 2);
    ifx_sw_vlan_add(1, 4);
    ifx_sw_vlan_add(2, 4);
    ifx_sw_vlan_add(4, 4);

    ifx_sw_vlan_add(3, 3);
    ifx_sw_vlan_add(5, 5);
    ifx_sw_vlan_add(3, 5);
    ifx_sw_vlan_add(5, 3);

  #endif
 #endif 
}
#endif

/*
 *  Description:
 *    Download PPE firmware binary code.
 *  Input:
 *    src       --- u32 *, binary code buffer
 *    dword_len --- unsigned int, binary code length in DWORD (32-bit)
 *  Output:
 *    int       --- 0:    Success
 *                  else: Error Code
 */
static INLINE int pp32_download_code(const u32 *code_src, unsigned int code_dword_len, const u32 *data_src, unsigned int data_dword_len)
{
    volatile u32 *dest;
    int i;

    if ( code_src == 0 || ((unsigned long)code_src & 0x03) != 0
        || data_src == 0 || ((unsigned long)data_src & 0x03) != 0
        || (code_dword_len > 0x1000 && data_dword_len > 0) )
        return -EINVAL;

    /*  set PPE code memory to FPI bus access mode  */
    if ( code_dword_len <= 0x1000 )
        *CDM_CFG = CDM_CFG_RAM1_SET(0x00) | CDM_CFG_RAM0_SET(0x00);
    else
        *CDM_CFG = CDM_CFG_RAM1_SET(0x01) | CDM_CFG_RAM0_SET(0x00);
    for ( i = 0; i < 1000; i++ );

    dbg("code_dword_len = 0x%X, data_dword_len = 0x%X", code_dword_len, data_dword_len);

    /*  copy code   */
    dest = CDM_CODE_MEMORY_RAM0_ADDR(0);
    while ( code_dword_len-- > 0 )
        *dest++ = *code_src++;

    /*  copy data   */
    dest = PP32_DATA_MEMORY_RAM1_ADDR(0);
    while ( data_dword_len-- > 0 )
        *dest++ = *data_src++;

    return 0;
}

/*
 *  Description:
 *    Do PP32 specific initialization.
 *  Input:
 *    data --- void *, specific parameter passed in.
 *  Output:
 *    int  --- 0:    Success
 *             else: Error Code
 */
static INLINE int pp32_specific_init(int fwcode, void *data)
{
    return 0;
}

/*
 *  Description:
 *    Initialize and start up PP32.
 *  Input:
 *    none
 *  Output:
 *    int  --- 0:    Success
 *             else: Error Code
 */
static INLINE int pp32_start(int fwcode)
{
    int ret;
    register int i;

    /*  download firmware   */
    if ( fwcode == FWCODE_ROUTING_ACC_D2 )
        ret = pp32_download_code(firmware_binary_code, sizeof(firmware_binary_code) / sizeof(*firmware_binary_code), firmware_binary_data, sizeof(firmware_binary_data) / sizeof(*firmware_binary_data));
    else
        ret = pp32_download_code(bridging_firmware_binary_code, sizeof(bridging_firmware_binary_code) / sizeof(*bridging_firmware_binary_code), bridging_firmware_binary_data, sizeof(bridging_firmware_binary_data) / sizeof(*bridging_firmware_binary_data));
    if ( ret )
        return ret;

    /*  firmware specific initialization    */
    ret = pp32_specific_init(fwcode, NULL);
    if ( ret )
        return ret;

    /*  run PP32    */
#if !defined(DEBUG_HALT_PP32) || !DEBUG_HALT_PP32
    *PP32_DBG_CTRL = DBG_CTRL_START_SET(1);
#endif
    /*  idle for a while to let PP32 init itself    */
    for ( i = 0; i < IDLE_CYCLE_NUMBER; i++ );

    return 0;
}

/*
 *  Description:
 *    Halt PP32.
 *  Input:
 *    none
 *  Output:
 *    none
 */
static INLINE void pp32_stop(void)
{
    /*  halt PP32   */
    *PP32_DBG_CTRL = DBG_CTRL_STOP_SET(1);
}

static INLINE int init_local_variables(void)
{
    int i;

    eth1_skb_pointers_addr = kmalloc(ETH1_TX_TOTAL_CHANNEL_USED * ETH1_TX_DESC_NUM * sizeof(struct sk_buff *), GFP_KERNEL);
    if ( !eth1_skb_pointers_addr )
        goto ETH1_TX_SKB_POINTERS_ALLOCATE_FAIL;

    for ( i = 0; i < ETH1_TX_TOTAL_CHANNEL_USED * ETH1_TX_DESC_NUM; i++ )
    {
        eth1_skb_pointers_addr[i] = dev_alloc_skb(DMA_PACKET_SIZE + 16);
        if ( !eth1_skb_pointers_addr[i] )
            goto INIT_LOCAL_VAR_DEV_ALLOC_SKB_FAIL;
    }

    eth1_dev_tx_irq = 0;

    for ( i = 0; i < ETH1_TX_TOTAL_CHANNEL_USED; i++ )
    {
        eth1_tx_ch[i].base          = (struct tx_descriptor *)EMA_TX_CH_DESC_BASE(i);
        eth1_tx_ch[i].alloc_pos     = 0;
        eth1_tx_ch[i].skb_pointers  = eth1_skb_pointers_addr + i * ETH1_TX_DESC_NUM;
        eth1_tx_ch[i].in_fast_path  = 0;
        sema_init(&eth1_tx_ch[i].lock, 1);
    }

    memset(eth_dev, 0, sizeof(eth_dev));

#if defined(ENABLE_DBG_PROC) && ENABLE_DBG_PROC
    dbg_enable = 0;
#endif

    return 0;

INIT_LOCAL_VAR_DEV_ALLOC_SKB_FAIL:
    while ( i-- )
        dev_kfree_skb_any(eth1_skb_pointers_addr[i]);
    kfree(eth1_skb_pointers_addr);
ETH1_TX_SKB_POINTERS_ALLOCATE_FAIL:
    return -ENOMEM;
}

static INLINE void init_communication_data_structures(int fwcode)
{
    int i;

    *CDM_CFG = CDM_CFG_RAM1_SET(0x00) | CDM_CFG_RAM0_SET(0x00);

    for ( i = 0; i < 1000; i++ );

    switch ( fwcode )
    {
    case FWCODE_ROUTING_ACC_D2:
        *(u32*)ETX1_DMACH_ON    = (1 << ETH1_TX_TOTAL_CHANNEL_USED) - 1;
#if defined(ENABLE_D2) && ENABLE_D2
        *(u32*)ETH_PORTS_CFG    = 0x00000004;
//        *(u32*)LAN_ROUT_TBL_CFG = 0x60300000;
//        *(u32*)WAN_ROUT_TBL_CFG = 0x60200000;
        *(u32*)LAN_ROUT_TBL_CFG = 0x57B44000;
        *(u32*)WAN_ROUT_TBL_CFG = 0x57A20000;
        *(u32*)GEN_MODE_CFG     = 0x400103A0;
#else
        *(u32*)ETH_PORTS_CFG    = 0x00000008;
        *(u32*)LAN_ROUT_TBL_CFG = 0xC0C00000;
        *(u32*)WAN_ROUT_TBL_CFG = 0xC0000000;
        *(u32*)GEN_MODE_CFG     = 0x000303A0;
#endif

        for ( i = 0; i < 8; i++ )
            *MTU_CFG_TBL(i)     = ETH_MAX_DATA_LENGTH;

        for ( i = 0; i < 8; i++ )
        {
            *LAN_ROUT_MAC_CFG_TBL(i)       = 0x00556677;
            *(LAN_ROUT_MAC_CFG_TBL(i) + 1) = 0x88000000;
        }

        for ( i = 0; i < 8; i++ )
        {
            *WAN_ROUT_MAC_CFG_TBL(i)       = 0x00112233;
            *(WAN_ROUT_MAC_CFG_TBL(i) + 1) = 0x44000000;
        }

#if 0

        /*
         *  WAN
         */

        *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(0))        = 0xC901010B;
        *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(0) + 1)    = 0x44020215;
        *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(0) + 2)    = 0x00290FD3;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(0))         = 0x00330061;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(0) + 1)     = 0x61616100;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(0) + 2)     = 0xC001441F;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(0) + 3)     = 0xC0000100;

        *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(1))        = 0xCA01010C;
        *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(1) + 1)    = 0x45020216;
        *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(1) + 2)    = 0x002A0FD4;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(1))         = 0x00340062;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(1) + 1)     = 0x62626200;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(1) + 2)     = 0xC0014420;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(1) + 3)     = 0xC0000100;

        *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(128))      = 0xCB01010D;
        *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(128) + 1)  = 0x46020217;
        *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(128) + 2)  = 0x002B0FD5;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(128))       = 0x00350063;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(128) + 1)   = 0x63636300;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(128) + 2)   = 0xC0014421;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(128) + 3)   = 0xC0000100;

        *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(174))      = 0xCC01010E;
        *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(174) + 1)  = 0x47020218;
        *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(174) + 2)  = 0x002C0FD6;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(174))       = 0x00360064;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(174) + 1)   = 0x64646400;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(174) + 2)   = 0xC0014422;
        *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(174) + 3)   = 0xC0000100;

        /*
         *  LAN
         */

        *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(0))        = 0xC001441F;
        *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(0) + 1)    = 0xC901010B;
        *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(0) + 2)    = 0x00330029;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(0))         = 0x0FD30081;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(0) + 1)     = 0x81818100;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(0) + 2)     = 0x44010115;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(0) + 3)     = 0xC0000200;

        *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(1))        = 0xC0014420;
        *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(1) + 1)    = 0xCA01010C;
        *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(1) + 2)    = 0x0034002A;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(1))         = 0x0FD40082;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(1) + 1)     = 0x82828200;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(1) + 2)     = 0x45010116;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(1) + 3)     = 0xC0000200;

        *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(128))      = 0xC0014421;
        *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(128) + 1)  = 0xCB01010D;
        *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(128) + 2)  = 0x0035002B;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(128))       = 0x0FD50083;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(128) + 1)   = 0x83838300;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(128) + 2)   = 0x4601010D;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(128) + 3)   = 0xC0000200;

        *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(174))      = 0xC0014422;
        *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(174) + 1)  = 0xCC01010E;
        *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(174) + 2)  = 0x0036002C;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(174))       = 0x0FD60084;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(174) + 1)   = 0x84848400;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(174) + 2)   = 0x4701010E;
        *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(174) + 3)   = 0xC0000200;

#endif

        break;

    case FWCODE_BRIDGING_ACC_D3:
        *(u32*)ETX1_DMACH_ON            = (1 << ETH1_TX_TOTAL_CHANNEL_USED) - 1;
        *(u32*)GEN_MODE_CFG             = 0x400003A0;
        *(u32*)BRG_TBL_CFG              = BRIDGING_ENTRY_NUM << 23; //  0x80000000
        *(u32*)ETH_DEFAULT_DEST_LIST(0) = 0x00040404;
        *(u32*)ETH_DEFAULT_DEST_LIST(1) = 0x00040404;
        for ( i = 0; i < BRIDGING_ENTRY_NUM; i++ )
        {
            BRIDGING_FORWARD_TBL(i)[0] = 0xFFFFFFFF;
            BRIDGING_FORWARD_TBL(i)[1] = 0xFFFFFFFF;
            BRIDGING_FORWARD_TBL(i)[2] = 0xFFFFFFFF;
            BRIDGING_FORWARD_TBL(i)[3] = 0xFFFFFFFF;
        }

#if 0
        //  broadcast
        BRIDGING_FORWARD_TBL(0)[0] = 0xFFFFFFFF;
        BRIDGING_FORWARD_TBL(0)[1] = 0xFFFF0000;
        BRIDGING_FORWARD_TBL(0)[2] = 0x00000000;
        BRIDGING_FORWARD_TBL(0)[3] = 0x00000700;

        //  linux PC
        BRIDGING_FORWARD_TBL(1)[0] = 0x000D5601;
        BRIDGING_FORWARD_TBL(1)[1] = 0xFEE20000;
        BRIDGING_FORWARD_TBL(1)[2] = 0x00000000;
        BRIDGING_FORWARD_TBL(1)[3] = 0x00000100;

        //  laptop
        BRIDGING_FORWARD_TBL(2)[0] = 0x00112514;
        BRIDGING_FORWARD_TBL(2)[1] = 0xEBB50000;
        BRIDGING_FORWARD_TBL(2)[2] = 0x00000000;
        BRIDGING_FORWARD_TBL(2)[3] = 0x00000100;

        //  test PC
        BRIDGING_FORWARD_TBL(3)[0] = 0x00045A85;
        BRIDGING_FORWARD_TBL(3)[1] = 0x2D790000;
        BRIDGING_FORWARD_TBL(3)[2] = 0x00000000;
        BRIDGING_FORWARD_TBL(3)[3] = 0x00000210;
#endif

        break;
    }

    //printk(KERN_ERR __FILE__ ":%d:%s: partially implemented\n", __LINE__, __FUNCTION__);
}

static INLINE int alloc_dma(void)
{
    struct sk_buff **skb;
    volatile u32 *p;
    int i;

    for ( i = 0; i < DMA_TX_CH2_DESC_NUM; i++ )
        if ( !(dma_ch2_skb_pointers[i] = alloc_skb_rx()) )
            goto DMA_CH2_SKB_ALLOC_FAIL;

    dma_device = dma_device_reserve("PPE");
    if ( !dma_device )
        goto DMA_CH2_SKB_ALLOC_FAIL;

    dma_device->buffer_alloc    = dma_buffer_alloc;
    dma_device->buffer_free     = dma_buffer_free;
    dma_device->intr_handler    = dma_int_handler;
    dma_device->max_rx_chan_num = 4;
    dma_device->max_tx_chan_num = 4;

    for ( i = 0; i < dma_device->max_rx_chan_num; i++ )
    {
        dma_device->rx_chan[i]->packet_size = DMA_PACKET_SIZE;
        dma_device->rx_chan[i]->control     = DANUBE_DMA_CH_ON;
    }
#if 0
    dbg("DMA_RX_CH2_DESC_BASE    = 0x%08X", DMA_RX_CH2_DESC_BASE);
    dbg("DMA_TX_CH2_DESC_BASE    = 0x%08X", DMA_TX_CH2_DESC_BASE);
    dbg("DMA_RX_CH1_DESC_BASE    = 0x%08X", DMA_RX_CH1_DESC_BASE);
    dbg("EMA_TX_CH_DESC_BASE(0)  = 0x%08X", EMA_TX_CH_DESC_BASE(0));
    dbg("EMA_TX_CH_DESC_BASE(1)  = 0x%08X", EMA_TX_CH_DESC_BASE(1));
    dbg("EMA_TX_CH_DESC_BASE(2)  = 0x%08X", EMA_TX_CH_DESC_BASE(2));
    dbg("EMA_TX_CH_DESC_BASE(3)  = 0x%08X", EMA_TX_CH_DESC_BASE(3));
#endif
    dma_device->rx_chan[1]->desc_base = (int)DMA_RX_CH1_DESC_BASE;
    dma_device->rx_chan[1]->desc_len  = DMA_RX_CH1_DESC_NUM;
    dma_device->rx_chan[2]->desc_base = (int)DMA_RX_CH2_DESC_BASE;
    dma_device->rx_chan[2]->desc_len  = DMA_RX_CH2_DESC_NUM;


    for ( i = 0; i < dma_device->max_tx_chan_num; i++ )
    {
        dma_device->tx_chan[i]->control     = DANUBE_DMA_CH_ON;
    }
    dma_device->tx_chan[2]->desc_base = (int)DMA_TX_CH2_DESC_BASE;
    dma_device->tx_chan[2]->desc_len  = DMA_TX_CH2_DESC_NUM;

    dma_device_register(dma_device);

    p = DMA_TX_CH2_DESC_BASE;
    skb = dma_ch2_skb_pointers;
    for ( i = 0; i < DMA_TX_CH2_DESC_NUM; i++ )
    {
        *p++ = 0x30000000;
        *p++ = (u32)(*skb)->data & 0x1FFFFFFF;
        skb++;
    }
//    dma_device->tx_chan[2]->open(dma_device->tx_chan[2]);

    f_dma_opened = 0;
    dma_ch2_tx_not_run = 1;

#if defined(ENABLE_D2) && ENABLE_D2
    p = EMA_TX_CH_DESC_BASE(1);
    skb = eth1_tx_ch[1].skb_pointers;
    for ( i = 0; i < ETH1_TX_DESC_NUM; i++ )
    {
        *p++ = 0x30000000;
        *p++ = ((u32)(*skb)->data & 0x1FFFFFFF) >> 2;
        skb++;
    }
#endif

    return 0;

DMA_CH2_SKB_ALLOC_FAIL:
    while ( i-- )
        dev_kfree_skb_any(dma_ch2_skb_pointers[i]);
    return -ENOMEM;
}

static INLINE void clear_local_variables(void)
{
    int i;

    if ( eth1_skb_pointers_addr )
    {
        for ( i = 0; i < ETH1_TX_TOTAL_CHANNEL_USED * ETH1_TX_DESC_NUM; i++ )
            if ( eth1_skb_pointers_addr[i] )
                dev_kfree_skb_any(eth1_skb_pointers_addr[i]);
        kfree(eth1_skb_pointers_addr);
    }
}

static INLINE void free_dma(void)
{
    struct sk_buff **skb;
    int i;

    if ( dma_device )
    {
        dma_device_unregister(dma_device);
        dma_device_release(dma_device);
        dma_device = NULL;

        skb = dma_ch2_skb_pointers;
        for ( i = 0; i < DMA_TX_CH2_DESC_NUM; i++ )
        {
            if ( *skb )
            {
                dev_kfree_skb_any(*skb);
                *skb = NULL;
            }
            skb++;
        }
    }
}

static INLINE void ethaddr_setup(unsigned int port, char *line)
{
    u8 *p;
    char *ep;
    int i;

    p = port ? MY_ETH1_ADDR : MY_ETH0_ADDR;
    memset(p, 0, MAX_ADDR_LEN * sizeof(*p));
    for ( i = 0; i < 6; i++ )
   {
        p[i] = line ? simple_strtoul(line, &ep, 16) : 0;
        if ( line )
            line = *ep ? ep + 1 : ep;
    }
    dbg("eth%d mac address %02X-%02X-%02X-%02X-%02X-%02X\n",
        port ? 1 : 0,
        p[0], p[1], p[2],
        p[3], p[4], p[5]);
}

static INLINE void proc_file_create(void)
{
    struct proc_dir_entry *res;

    g_eth_proc_dir = proc_mkdir("eth", NULL);

    res = create_proc_read_entry("mib",
                                  0,
                                  g_eth_proc_dir,
                                  proc_read_mib,
                                  NULL);
    if ( res )
        res->write_proc = proc_write_mib;

#if defined(DEBUG_FIRMWARE_TABLES_PROC) && DEBUG_FIRMWARE_TABLES_PROC

    res = create_proc_read_entry("route",
                                  0,
                                  g_eth_proc_dir,
                                  proc_read_route,
                                  NULL);
    if ( res )
        res->write_proc = proc_write_route;

    res = create_proc_read_entry("genconf",
                                  0,
                                  g_eth_proc_dir,
                                  proc_read_genconf,
                                  NULL);
    if ( res )
        res->write_proc = proc_write_genconf;

    res = create_proc_read_entry("vlan",
                                  0,
                                  g_eth_proc_dir,
                                  proc_read_vlan,
                                  NULL);
    if ( res )
        res->write_proc = proc_write_vlan;

    res = create_proc_read_entry("pppoe",
                                  0,
                                  g_eth_proc_dir,
                                  proc_read_pppoe,
                                  NULL);
    if ( res )
        res->write_proc = proc_write_pppoe;

    res = create_proc_read_entry("mtu",
                                  0,
                                  g_eth_proc_dir,
                                  proc_read_mtu,
                                  NULL);
    if ( res )
        res->write_proc = proc_write_mtu;

    res = create_proc_read_entry("hit",
                                  0,
                                  g_eth_proc_dir,
                                  proc_read_hit,
                                  NULL);
    if ( res )
        res->write_proc = proc_write_hit;

    res = create_proc_read_entry("mac",
                                  0,
                                  g_eth_proc_dir,
                                  proc_read_mac,
                                  NULL);
    if ( res )
        res->write_proc = proc_write_mac;

#endif

#if defined(ENABLE_DBG_PROC) && ENABLE_DBG_PROC
    res = create_proc_read_entry("dbg",
                                  0,
                                  g_eth_proc_dir,
                                  proc_read_dbg,
                                  NULL);
    if ( res )
        res->write_proc = proc_write_dbg;
#endif

#if defined(DEBUG_FIRMWARE_PROC) && DEBUG_FIRMWARE_PROC
    res = create_proc_read_entry("fw",
                                  0,
                                  g_eth_proc_dir,
                                  proc_read_fw,
                                  NULL);
#endif

#if defined(DEBUG_MEM_PROC) && DEBUG_MEM_PROC
    res = create_proc_read_entry("mem",
                                  0,
                                  g_eth_proc_dir,
                                  NULL,
                                  NULL);
    if ( res )
        res->write_proc = proc_write_mem;
#endif

#if defined(DEBUG_DMA_PROC) && DEBUG_DMA_PROC
    res = create_proc_read_entry("dma",
                                  0,
                                  g_eth_proc_dir,
                                  proc_read_dma,
                                  NULL);
    if ( res )
        res->write_proc = proc_write_dma;
#endif

#if defined(DEBUG_PP32_PROC) && DEBUG_PP32_PROC
    res = create_proc_read_entry("pp32",
                                  0,
                                  g_eth_proc_dir,
                                  proc_read_pp32,
                                  NULL);
    if ( res )
        res->write_proc = proc_write_pp32;
#endif
}

static INLINE void proc_file_delete(void)
{
#if defined(DEBUG_MEM_PROC) && DEBUG_MEM_PROC
    remove_proc_entry("dma",
                      g_eth_proc_dir);
#endif

#if defined(DEBUG_MEM_PROC) && DEBUG_MEM_PROC
    remove_proc_entry("mem",
                      g_eth_proc_dir);
#endif

#if defined(DEBUG_FIRMWARE_PROC) && DEBUG_FIRMWARE_PROC
    remove_proc_entry("fw",
                      g_eth_proc_dir);
#endif

#if defined(ENABLE_DBG_PROC) && ENABLE_DBG_PROC
    remove_proc_entry("dbg",
                      g_eth_proc_dir);
#endif

#if defined(DEBUG_FIRMWARE_TABLES_PROC) && DEBUG_FIRMWARE_TABLES_PROC

    remove_proc_entry("mac",
                      g_eth_proc_dir);

    remove_proc_entry("hit",
                      g_eth_proc_dir);

    remove_proc_entry("mtu",
                      g_eth_proc_dir);

    remove_proc_entry("pppoe",
                      g_eth_proc_dir);

    remove_proc_entry("vlan",
                      g_eth_proc_dir);

    remove_proc_entry("genconf",
                      g_eth_proc_dir);

    remove_proc_entry("route",
                      g_eth_proc_dir);

#endif

    remove_proc_entry("mib",
                      g_eth_proc_dir);

    remove_proc_entry("eth", NULL);
}

static int proc_read_mib(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    int i;

    MOD_INC_USE_COUNT;

    for ( i = 0; i < 2; i++ )
    {
        eth_get_stats(eth_net_dev + i);

        len += sprintf(page + off + len,         "Port %d\n", i);
        len += sprintf(page + off + len,         "  Hardware\n");
        len += sprintf(page + off + len,         "    enets_igerr:  %u\n", eth_dev[i].enets_igerr);
        len += sprintf(page + off + len,         "    enets_igdrop: %u\n", eth_dev[i].enets_igdrop);
        len += sprintf(page + off + len,         "    enetf_egcol:  %u\n", eth_dev[i].enetf_egcol);
        len += sprintf(page + off + len,         "    enetf_egdrop: %u\n", eth_dev[i].enetf_egdrop);
        len += sprintf(page + off + len,         "  Firmware\n");
        switch ( g_fwcode )
        {
        case FWCODE_ROUTING_ACC_D2:
            if ( i == 0 )
            {
                len += sprintf(page + off + len, "    lrx_fast_tcp_pkts:      %lu\n", LAN_RX_MIB_TBL->lrx_fast_tcp_pkts);
                len += sprintf(page + off + len, "    lrx_fast_udp_pkts:      %lu\n", LAN_RX_MIB_TBL->lrx_fast_udp_pkts);
                len += sprintf(page + off + len, "    lrx_fast_drop_tcp_pkts: %lu\n", LAN_RX_MIB_TBL->lrx_fast_drop_tcp_pkts);
                len += sprintf(page + off + len, "    lrx_fast_drop_udp_pkts: %lu\n", LAN_RX_MIB_TBL->lrx_fast_drop_udp_pkts);
                len += sprintf(page + off + len, "    lrx_drop_pkts:          %lu\n", LAN_RX_MIB_TBL->lrx_drop_pkts);
            }else
            {
                len += sprintf(page + off + len, "    wrx_fast_uc_tcp_pkts:   %lu\n", WAN_RX_MIB_TBL->wrx_fast_uc_tcp_pkts);
                len += sprintf(page + off + len, "    wrx_fast_uc_udp_pkts:   %lu\n", WAN_RX_MIB_TBL->wrx_fast_uc_udp_pkts);
                len += sprintf(page + off + len, "    wrx_fast_mc_tcp_pkts:   %lu\n", WAN_RX_MIB_TBL->wrx_fast_mc_tcp_pkts);
                len += sprintf(page + off + len, "    wrx_fast_mc_udp_pkts:   %lu\n", WAN_RX_MIB_TBL->wrx_fast_mc_udp_pkts);
                len += sprintf(page + off + len, "    wrx_fast_drop_tcp_pkts: %lu\n", WAN_RX_MIB_TBL->wrx_fast_drop_tcp_pkts);
                len += sprintf(page + off + len, "    wrx_fast_drop_udp_pkts: %lu\n", WAN_RX_MIB_TBL->wrx_fast_drop_udp_pkts);
                len += sprintf(page + off + len, "    wrx_drop_pkts:          %lu\n", WAN_RX_MIB_TBL->wrx_drop_pkts);
            }
            break;
        case FWCODE_BRIDGING_ACC_D3:
            len += sprintf(page + off + len,     "    ig_fast_pkts:  %lu\n", BRIDGING_ETH_MIB_TBL(i)->ig_fast_pkts);
            len += sprintf(page + off + len,     "    ig_fast_bytes: %lu\n", BRIDGING_ETH_MIB_TBL(i)->ig_fast_bytes);
            len += sprintf(page + off + len,     "    ig_cpu_pkts:   %lu\n", BRIDGING_ETH_MIB_TBL(i)->ig_cpu_pkts);
            len += sprintf(page + off + len,     "    ig_cpu_bytes:  %lu\n", BRIDGING_ETH_MIB_TBL(i)->ig_cpu_bytes);
            len += sprintf(page + off + len,     "    ig_drop_pkts:  %lu\n", BRIDGING_ETH_MIB_TBL(i)->ig_drop_pkts);
            len += sprintf(page + off + len,     "    ig_drop_bytes: %lu\n", BRIDGING_ETH_MIB_TBL(i)->ig_drop_bytes);
            len += sprintf(page + off + len,     "    eg_fast_pkts:  %lu\n", BRIDGING_ETH_MIB_TBL(i)->eg_fast_pkts);
            len += sprintf(page + off + len,     "    eg_fast_bytes: %lu\n", BRIDGING_ETH_MIB_TBL(i)->eg_fast_bytes);
            break;
        }
        len += sprintf(page + off + len,         "  Driver\n");
        len += sprintf(page + off + len,         "    rx_packets: %u\n", i ? eth_dev[i].rx_packets : eth_dev[i].rx_packets + eth_dev[i].rx_fastpath_packets);
        len += sprintf(page + off + len,         "    rx_bytes:   %u\n", eth_dev[i].rx_bytes);
        len += sprintf(page + off + len,         "    rx_errors:  %u\n", eth_dev[i].rx_errors);
        len += sprintf(page + off + len,         "    rx_dropped: %u\n", eth_dev[i].rx_dropped);
        len += sprintf(page + off + len,         "    tx_packets: %u\n", eth_dev[i].tx_packets);
        len += sprintf(page + off + len,         "    tx_bytes:   %u\n", eth_dev[i].tx_bytes);
        len += sprintf(page + off + len,         "    tx_errors:  %u\n", eth_dev[i].tx_errors);
        len += sprintf(page + off + len,         "    tx_dropped: %u\n", eth_dev[i].tx_dropped);
        len += sprintf(page + off + len,         "  Total\n");
        len += sprintf(page + off + len,         "    rx_packets: %lu\n", eth_dev[i].stats.rx_packets);
        len += sprintf(page + off + len,         "    rx_bytes:   %lu\n", eth_dev[i].stats.rx_bytes);
        len += sprintf(page + off + len,         "    rx_errors:  %lu\n", eth_dev[i].stats.rx_errors);
        len += sprintf(page + off + len,         "    rx_dropped: %lu\n", eth_dev[i].stats.rx_dropped);
        len += sprintf(page + off + len,         "    tx_packets: %lu\n", eth_dev[i].stats.tx_packets);
        len += sprintf(page + off + len,         "    tx_bytes:   %lu\n", eth_dev[i].stats.tx_bytes);
        len += sprintf(page + off + len,         "    tx_errors:  %lu\n", eth_dev[i].stats.tx_errors);
        len += sprintf(page + off + len,         "    tx_dropped: %lu\n", eth_dev[i].stats.tx_dropped);
    }

    MOD_DEC_USE_COUNT;

    *eof = 1;

    return len;
}

static int proc_write_mib(struct file *file, const char *buf, unsigned long count, void *data)
{
    char str[64];
    char *p;
    int len, rlen;
    u32 eth_clear;

    MOD_INC_USE_COUNT;

    len = count < sizeof(str) ? count : sizeof(str) - 1;
    rlen = len - copy_from_user(str, buf, len);
    while ( rlen && str[rlen - 1] <= ' ' )
        rlen--;
    str[rlen] = 0;
    for ( p = str; *p && *p <= ' '; p++, rlen-- );
    if ( !*p )
    {
        MOD_DEC_USE_COUNT;
        return 0;
    }

    eth_clear = 0;
    if ( stricmp(p, "clear") == 0 || stricmp(p, "clear all") == 0
        || stricmp(p, "clean") == 0 || stricmp(p, "clean all") == 0 )
        eth_clear = 3;
    else if ( stricmp(p, "clear eth0") == 0 || stricmp(p, "clear 0") == 0
            || stricmp(p, "clean eth0") == 0 || stricmp(p, "clean 0") == 0 )
        eth_clear = 1;
    else if ( stricmp(p, "clear eth1") == 0 || stricmp(p, "clear 1") == 0
            || stricmp(p, "clean eth1") == 0 || stricmp(p, "clean 1") == 0 )
        eth_clear = 2;
    switch ( g_fwcode )
    {
    case FWCODE_ROUTING_ACC_D2:
    if ( (eth_clear & 1) )
    {
        eth_get_stats(eth_net_dev);
        memset(eth_dev, 0, (u32)&eth_dev->rx_fastpath_packets - (u32)eth_dev + sizeof(u32));
        memset((void *)LAN_RX_MIB_TBL, 0, sizeof(struct lan_rx_mib_tbl));
    }
    if ( (eth_clear & 2) )
    {
        eth_get_stats(eth_net_dev + 1);
        memset(eth_dev + 1, 0, (u32)&eth_dev[1].rx_fastpath_packets - (u32)(eth_dev + 1) + sizeof(u32));
        memset((void *)WAN_RX_MIB_TBL, 0, sizeof(struct wan_rx_mib_tbl));
    }
        break;

    case FWCODE_BRIDGING_ACC_D3:
        if ( (eth_clear & 1) )
        {
            eth_get_stats(eth_net_dev);
            memset(eth_dev, 0, (u32)&eth_dev->rx_fastpath_packets - (u32)eth_dev + sizeof(u32));
            memset((void *)BRIDGING_ETH_MIB_TBL(0), 0, sizeof(struct bridging_eth_mib_tbl));
        }
        if ( (eth_clear & 2) )
        {
            eth_get_stats(eth_net_dev + 1);
            memset(eth_dev + 1, 0, (u32)&eth_dev[1].rx_fastpath_packets - (u32)(eth_dev + 1) + sizeof(u32));
            memset((void *)BRIDGING_ETH_MIB_TBL(1), 0, sizeof(struct bridging_eth_mib_tbl));
        }
        break;
    }

    MOD_DEC_USE_COUNT;

    return count;
}

#if defined(DEBUG_FIRMWARE_TABLES_PROC) && DEBUG_FIRMWARE_TABLES_PROC

static int proc_read_route(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    struct rout_forward_compare_tbl *pcompare;
    struct wan_rout_forward_action_tbl *pwaction;
    struct lan_rout_forward_action_tbl *plaction;
    int len = 0;
    int len_max = off + count;
    char *pstr;
    char str[512];
    int llen;
    int i;

    MOD_INC_USE_COUNT;

    pstr = *start = page;

    llen = sprintf(str, "Wan Routing Table\n");
    if ( len <= off && len + llen > off )
    {
        memcpy(pstr, str + off - len, len + llen - off);
        pstr += len + llen - off;
    }
    else if ( len > off )
    {
        memcpy(pstr, str, llen);
        pstr += llen;
    }
    len += llen;

    pcompare = (struct rout_forward_compare_tbl *)WAN_ROUT_FORWARD_COMPARE_TBL(0);
    pwaction = (struct wan_rout_forward_action_tbl *)WAN_ROUT_FORWARD_ACTION_TBL(0);
//    for ( i = 0; i < WAN_ROUT_NUM; i++ )
    for ( i = 0; i < WAN_ROUT_TBL_CFG->wan_rout_num; i++ )
    {
        if ( *(u32*)pcompare && *((u32*)pcompare + 1) )
        {
            llen = print_wan_route(str, i, pcompare, pwaction);
            if ( len <= off && len + llen > off )
            {
                memcpy(pstr, str + off - len, len + llen - off);
                pstr += len + llen - off;
            }
            else if ( len > off )
            {
                memcpy(pstr, str, llen);
                pstr += llen;
            }
            len += llen;
            if ( len >= len_max )
                goto PROC_READ_ROUTE_OVERRUN_END;
        }

        pcompare++;
        pwaction++;
    }

    llen = sprintf(str, "Lan Routing Table\n");
    if ( len <= off && len + llen > off )
    {
        memcpy(pstr, str + off - len, len + llen - off);
        pstr += len + llen - off;
    }
    else if ( len > off )
    {
        memcpy(pstr, str, llen);
        pstr += llen;
    }
    len += llen;
    if ( len >= len_max )
        goto PROC_READ_ROUTE_OVERRUN_END;

    pcompare = (struct rout_forward_compare_tbl *)LAN_ROUT_FORWARD_COMPARE_TBL(0);
    plaction = (struct lan_rout_forward_action_tbl *)LAN_ROUT_FORWARD_ACTION_TBL(0);
//    for ( i = 0; i < LAN_ROUT_NUM; i++ )
    for ( i = 0; i < LAN_ROUT_TBL_CFG->lan_rout_num; i++ )
    {
        if ( *(u32*)pcompare && *((u32*)pcompare + 1) )
        {
            llen = print_lan_route(str, i, pcompare, plaction);
            if ( len <= off && len + llen > off )
            {
                memcpy(pstr, str + off - len, len + llen - off);
                pstr += len + llen - off;
            }
            else if ( len > off )
            {
                memcpy(pstr, str, llen);
                pstr += llen;
            }
            len += llen;
            if ( len >= len_max )
                goto PROC_READ_ROUTE_OVERRUN_END;
        }

        pcompare++;
        plaction++;
    }

    *eof = 1;

    MOD_DEC_USE_COUNT;

    return len - off;

PROC_READ_ROUTE_OVERRUN_END:
    MOD_DEC_USE_COUNT;

    return len - llen - off;
}

static int proc_write_route(struct file *file, const char *buf, unsigned long count, void *data)
{
    static const char *command[] = {
        "add",      //  0
        "del",      //  1
        "LAN",      //  2
        "WAN",      //  3
        "new",      //  4
        "src",      //  5
        "dest",     //  6
        "MAC",      //  7
        "route",    //  8
        "type",     //  9
        "DSCP",     //  10
        "MTU",      //  11
        "index",    //  12
        "VLAN",     //  13
        "insert",   //  14
        "remove",   //  15
        "list",     //  16
        "PPPoE",    //  17
        "mode",     //  18
        "ch",       //  19
        "id",       //  20
        "delete",   //  21
        "disable",  //  22
        "enable",   //  23
        "transparent",  //  24
        "termination",  //  25
        "NULL",     //  26
        "IPv4",     //  27
        "NAT",      //  28
        "NAPT",     //  29
        "entry",    //  30
        "help",     //  31
    };

    static const char *dest_list[] = {"ETH0", "ETH1", "CPU0", "EXT_INT1", "EXT_INT2", "EXT_INT3", "EXT_INT4", "EXT_INT5"};
    static const int dest_list_strlen[] = {4, 4, 4, 8, 8, 8, 8, 8};

    int state;              //  1: new,
    int prev_cmd;
    int operation;          //  1: add, 2: delete
    int type;               //  1: LAN, 2: WAN, 0: auto detect
    int entry;
    struct rout_forward_compare_tbl compare_tbl;
    struct wan_rout_forward_action_tbl action_tbl;
    unsigned int val[6];
    char local_buf[1024];
    int len;
    char *p1, *p2;
    int colon;
    int i, j;
    u32 mask;
    u32 bit;
    u32 *pu1, *pu2;

    MOD_INC_USE_COUNT;

    len = sizeof(local_buf) < count ? sizeof(local_buf) : count;
    len = len - copy_from_user(local_buf, buf, len);
    local_buf[len] = 0;

    state = 0;
    prev_cmd = 0;
    operation = 0;
    type = 0;
    entry = -1;

    memset(&compare_tbl, 0, sizeof(compare_tbl));
    memset(&action_tbl, 0, sizeof(action_tbl));

    p1 = local_buf;
    colon = 1;
    while ( get_token(&p1, &p2, &len, &colon) )
    {
        for ( i = 0; i < sizeof(command) / sizeof(*command); i++ )
            if ( stricmp(p1, command[i]) == 0 )
            {
                switch ( i )
                {
                case 0:
                    if ( !state && !operation )
                    {
                        operation = 1;
//                      printk("add\n");
                    }
                    state = prev_cmd = 0;
                    break;
                case 1:
                case 21:
                    if ( !state && !operation )
                    {
                        operation = 2;
//                      printk("delete\n");
                    }
                    state = prev_cmd = 0;
                    break;
                case 2:
                    if ( !state && !type )
                    {
                        type = 1;
//                      printk("lan\n");
                    }
                    state = prev_cmd = 0;
                    break;
                case 3:
                    if ( !state && !type )
                    {
                        type = 2;
//                      printk("wan\n");
                    }
                    state = prev_cmd = 0;
                    break;
                case 4:
                    if ( !state )
                    {
                        state = 1;
                        prev_cmd = 4;
                    }
                    else
                        state = prev_cmd = 0;
                    break;
                case 5:
                    if ( state == 1 )
                    {
                        if ( prev_cmd == 4 )
                        {
                            //  new src
                            if ( !type )
                                type = 1;

                            get_ip_port(&p2, &len, val);
//                          printk("new src: %d.%d.%d.%d:%d\n", val[0], val[1], val[2], val[3], val[4]);
                            action_tbl.wan_new_dest_ip = (val[0] << 24) | (val[1] << 16) | (val[2] << 8) | val[3];
                            action_tbl.wan_new_dest_port = val[4];
                        }
                        else
                            state = 0;
                    }
                    if ( state == 0 )
                    {
                        //  src
                        get_ip_port(&p2, &len, val);
//                      printk("src: %d.%d.%d.%d:%d\n", val[0], val[1], val[2], val[3], val[4]);
                        compare_tbl.src_ip = (val[0] << 24) | (val[1] << 16) | (val[2] << 8) | val[3];
                        compare_tbl.src_port = val[4];
                    }
                    state = prev_cmd = 0;
                    break;
                case 6:
                    if ( state == 1 )
                    {
                        if ( prev_cmd == 4 )
                        {
                            //  new dest
                            if ( !type )
                                type = 2;

                            get_ip_port(&p2, &len, val);
//                          printk("new dest: %d.%d.%d.%d:%d\n", val[0], val[1], val[2], val[3], val[4]);
                            action_tbl.wan_new_dest_ip = (val[0] << 24) | (val[1] << 16) | (val[2] << 8) | val[3];
                            action_tbl.wan_new_dest_port = val[4];
                        }
                        else
                            state = 0;
                    }
                    if ( state == 0 )
                    {
                        if ( !colon )
                        {
                            int llen;

                            llen = len;
                            p1 = p2;
                            while ( llen && *p1 <= ' ' )
                            {
                                llen--;
                                p1++;
                            }
                            if ( llen && (*p1 == ':' || (*p1 >= '0' && *p1 <= '9')) )
                                colon = 1;
                        }
                        if ( colon )
                        {
                            //  dest
                            get_ip_port(&p2, &len, val);
//                          printk("dest: %d.%d.%d.%d:%d\n", val[0], val[1], val[2], val[3], val[4]);
                            compare_tbl.dest_ip = (val[0] << 24) | (val[1] << 16) | (val[2] << 8) | val[3];
                            compare_tbl.dest_port = val[4];
                        }
                        else
                        {
                            state = 1;
                            prev_cmd = 6;
                            break;
                        }
                    }
                    state = prev_cmd = 0;
                    break;
                case 7:
                    if ( state == 1 && prev_cmd == 4 )
                    {
                        //  new MAC
                        get_mac(&p2, &len, val);
//                      printk("new MAC: %02X.%02X.%02X.%02X:%02X:%02X\n", val[0], val[1], val[2], val[3], val[4], val[5]);
                        action_tbl.wan_new_dest_mac54 = (val[0] << 8) | val[1];
                        action_tbl.wan_new_dest_mac30 = (val[2] << 24) | (val[3] << 16) | (val[4] << 8) | val[5];
                    }
                    state = prev_cmd = 0;
                    break;
                case 8:
                    if ( !state )
                    {
                        state = 1;
                        prev_cmd = 8;
                    }
                    else
                        state = prev_cmd = 0;
                    break;
                case 9:
                    if ( state == 1 && prev_cmd == 8 )
                    {
                        state = 2;
                        prev_cmd = 9;
                        ignore_space(&p2, &len);
                    }
                    else
                        state = prev_cmd = 0;
                    break;
                case 10:
                    if ( state == 1 && prev_cmd == 4 )
                    {
                        ignore_space(&p2, &len);
                        if ( len && *p2 >= '0' && *p2 <= '9' )
                        {
                            //  new DSCP
                            val[0] = get_number(&p2, &len, 0);
//                          printk("new DSCP: %d\n", val[0]);
                            if ( !action_tbl.new_dscp_en )
                            {
                                action_tbl.new_dscp_en = 1;
                                action_tbl.new_dscp = val[0];
                            }
                        }
                        else if ( (len == 8 || (len > 8 && (p2[8] <= ' ' || p2[8] == ','))) && strincmp(p2, "original", 8) == 0 )
                        {
                            p2 += 8;
                            len -= 8;
                            //  new DSCP original
//                          printk("new DSCP: original\n");
                            //  the reset value is 0, so don't need to do anything
                        }
                    }
                    state = prev_cmd = 0;
                    break;
                case 11:
                    if ( !state )
                    {
                        state = 1;
                        prev_cmd = 11;
                    }
                    else
                        state = prev_cmd = 0;
                    break;
                case 12:
                    if ( state == 1 )
                    {
                        if ( prev_cmd == 11 )
                        {
                            //  MTU index
                            ignore_space(&p2, &len);
                            val[0] = get_number(&p2, &len, 0);
//                          printk("MTU index: %d\n", val[0]);
                            action_tbl.mtu_ix = val[0];
                        }
                        else if ( prev_cmd == 13 )
                        {
                            //  VLAN insert enable
                            //  VLAN index
                            ignore_space(&p2, &len);
                            val[0] = get_number(&p2, &len, 0);
//                          printk("VLAN insert: enable, index %d\n", val[0]);
                            if ( !action_tbl.vlan_ins )
                            {
                                action_tbl.vlan_ins = 1;
                                action_tbl.vlan_ix = val[0];
                            }
                        }
                        else if ( prev_cmd == 17 )
                        {
                            //  PPPoE index
                            ignore_space(&p2, &len);
                            val[0] = get_number(&p2, &len, 0);
//                          printk("PPPoE index: %d\n", val[0]);
                            action_tbl.pppoe_ix = val[0];
                        }
                    }
                    state = prev_cmd = 0;
                    break;
                case 13:
                    if ( !state )
                    {
                        state = 1;
                        prev_cmd = 13;
                    }
                    else
                        state = prev_cmd = 0;
                    break;
                case 14:
                    if ( state == 1 && prev_cmd == 13 )
                    {
                        state = 2;
                        prev_cmd = 14;
                        ignore_space(&p2, &len);
                    }
                    else
                        state = prev_cmd = 0;
                    break;
                case 15:
                    if ( state == 1 && prev_cmd == 13 )
                    {
                        state = 2;
                        prev_cmd = 15;
                        ignore_space(&p2, &len);
                    }
                    else
                        state = prev_cmd = 0;
                    break;
                case 16:
                    if ( state == 1 && prev_cmd == 6 )
                    {
                        mask = 0;
                        do
                        {
                            ignore_space(&p2, &len);
                            if ( !len )
                                break;
                            for ( j = 0, bit = 1; j < sizeof(dest_list) / sizeof(*dest_list); j++, bit <<= 1 )
                                if ( (len == dest_list_strlen[j] || (len > dest_list_strlen[j] && (p2[dest_list_strlen[j]] <= ' ' || p2[dest_list_strlen[j]] == ','))) && strincmp(p2, dest_list[j], dest_list_strlen[j]) == 0 )
                                {
                                    p2 += dest_list_strlen[j];
                                    len -= dest_list_strlen[j];
                                    mask |= bit;
                                    break;
                                }
                        } while ( j < sizeof(dest_list) / sizeof(*dest_list) );
//                      if ( mask )
//                      {
//                          //  dest list
//                          printk("dest list:");
//                          for ( j = 0, bit = 1; j < sizeof(dest_list) / sizeof(*dest_list); j++, bit <<= 1 )
//                              if ( (mask & bit) )
//                              {
//                                  printk(" %s", dest_list[j]);
//                              }
//                          printk("\n");
//                      }
//                      else
//                          printk("dest list: none\n");
                        action_tbl.dest_list = mask;
                    }
                    state = prev_cmd = 0;
                    break;
                case 17:
                    if ( !state )
                    {
                        state = 1;
                        prev_cmd = 17;
                    }
                    else
                        state = prev_cmd = 0;
                    break;
                case 18:
                    if ( state == 1 && prev_cmd == 17 )
                    {
                        state = 2;
                        prev_cmd = 18;
                        ignore_space(&p2, &len);
                    }
                    else
                        state = prev_cmd = 0;
                    break;
                case 19:
                    if ( state == 1 && prev_cmd == 6 )
                    {
                        state = 2;
                        prev_cmd = 19;
                    }
                    else
                        state = prev_cmd = 0;
                    break;
                case 20:
                    if ( state == 2 && prev_cmd == 19 )
                    {
                        //  dest ch id
                        ignore_space(&p2, &len);
                        val[0] = get_number(&p2, &len, 0);
//                      printk("dest ch id: %d\n", val[0]);
                        action_tbl.dest_chid = val[0];
                    }
                    state = prev_cmd = 0;
                    break;
                case 22:
                case 23:
                    if ( state == 2 )
                    {
                        if ( prev_cmd == 14 )
                        {
                            //  VLAN insert
//                          printk("VLAN insert: %s (%d)", command[i], i - 22);
                            if ( (i - 22) )
                            {
                                ignore_space(&p2, &len);
                                if ( len > 5 && (p2[5] <= ' ' || p2[5] == ':') && strincmp(p2, "index", 5) == 0 )
                                {
                                    p2 += 6;
                                    len -= 6;
                                    //  VLAN index
                                    ignore_space(&p2, &len);
                                    val[0] = get_number(&p2, &len, 0);
//                                  printk(", index %d", val[0]);
                                    if ( !action_tbl.vlan_ins )
                                    {
                                        action_tbl.vlan_ins = 1;
                                        action_tbl.vlan_ix = val[0];
                                    }
                                }
                            }
//                          printk("\n");
                        }
                        else if ( prev_cmd == 15 )
                        {
                            //  VLAN remove
//                          printk("VLAN remove: %s (%d)\n", command[i], i - 22);
                            if ( (i - 22) && !action_tbl.vlan_rm )
                                action_tbl.vlan_rm = 1;
                        }
                    }
                    state = prev_cmd = 0;
                    break;
                case 24:
                case 25:
                    if ( state == 2 && prev_cmd == 18 )
                    {
                        //  PPPoE mode
//                      printk("PPPoE mode: %s (%d)\n", command[i], i - 24);
                        action_tbl.pppoe_mode = i - 24;
                    }
                    state = prev_cmd = 0;
                    break;
                case 26:
                case 27:
                case 28:
                case 29:
                    if ( state == 2 && prev_cmd == 9 )
                    {
                        //  route type
//                      printk("route type: %s (%d)\n", command[i], i - 26);
                        action_tbl.rout_type = i - 26;
                    }
                    state = prev_cmd = 0;
                    break;
                case 30:
                    if ( !state )
                    {
                        if ( entry < 0 )
                        {
                            ignore_space(&p2, &len);
                            if ( len && *p2 >= '0' && *p2 <= '9' )
                            {
                                entry = get_number(&p2, &len, 0);
                                //  entry
//                              printk("entry: %d\n", entry);
                            }
                        }
                    }
                    state = prev_cmd = 0;
                    break;
                case 31:
                    printk("add\n");
                    printk("  LAN/WAN entry ???\n");
                    printk("    compare\n");
                    printk("      src:  ???.???.???.???:????\n");
                    printk("      dest: ???.???.???.???:????\n");
                    printk("    action\n");
                    printk("      new src/dest:???.???.???.???:????\n");
                    printk("      new MAC:     ??:??:??:??:??:?? (HEX)\n");
                    printk("      route type:  NULL/IPv4/NAT/NAPT\n");
                    printk("      new DSCP:    original/??\n");
                    printk("      MTU index:   ??\n");
                    printk("      VLAN insert: disable/enable, index ??\n");
                    printk("      VLAN remove: disable/enable\n");
                    printk("      dest list:   ETH0/ETH1/CPU0/EXT_INT?\n");
                    printk("      PPPoE mode:  transparent/termination\n");
                    printk("      PPPoE index: ??\n");
                    printk("      dest ch id:  ??\n");
                    printk("\n");
                    printk("delete\n");
                    printk("  LAN/WAN entry ???\n");
                    printk("    compare\n");
                    printk("      src:  ???.???.???.???:????\n");
                    printk("      dest: ???.???.???.???:????\n");
                    printk("\n");
                default:
                    state = prev_cmd = 0;
                }

                break;
            }

        if ( i == sizeof(command) / sizeof(*command) )
            state = prev_cmd = 0;

        p1 = p2;
        colon = 1;
    }

    if ( operation == 2 )
    {
        //  delete
        pu1 = (u32*)&compare_tbl;
        pu2 = NULL;
        if ( entry < 0 )
        {
            //  search the entry number
            if ( *pu1 && pu1[1] )
            {
                if ( (!type || type == 1) )
                {
                    //  LAN
//                    for ( entry = 0; entry < LAN_ROUT_NUM; entry++ )
                    for ( entry = 0; entry < LAN_ROUT_TBL_CFG->lan_rout_num; entry++ )
                    {
                        pu2 = (u32*)LAN_ROUT_FORWARD_COMPARE_TBL(entry);
                        if ( *pu2 == *pu1 && pu2[1] == pu1[1] && pu2[2] == pu1[2] )
                            break;
                    }
//                    if ( entry == LAN_ROUT_NUM )
                    if ( entry == LAN_ROUT_TBL_CFG->lan_rout_num )
                        pu2 = NULL;
                }
                if ( (!type && !pu2) || type == 2 )
                {
                    //  WAN
//                    for ( entry = 0; entry < WAN_ROUT_NUM; entry++ )
                    for ( entry = 0; entry < WAN_ROUT_TBL_CFG->wan_rout_num; entry++ )
                    {
                        pu2 = (u32*)WAN_ROUT_FORWARD_COMPARE_TBL(entry);
                        if ( *pu2 == *pu1 && pu2[1] == pu1[1] && pu2[2] == pu1[2] )
                            break;
                    }
//                    if ( entry == WAN_ROUT_NUM )
                    if ( entry == WAN_ROUT_TBL_CFG->wan_rout_num )
                        pu2 = NULL;
                }
            }
        }
        else
        {
            if ( *pu1 && pu1[1] )
            {
                //  check compare
//                if ( (!type || type == 1) && entry < LAN_ROUT_NUM )
                if ( (!type || type == 1) && entry < LAN_ROUT_TBL_CFG->lan_rout_num )
                {
                    //  LAN
                    pu2 = (u32*)LAN_ROUT_FORWARD_COMPARE_TBL(entry);
                    if ( *pu2 != *pu1 || pu2[1] != pu1[1] || pu2[2] != pu1[2] )
                            pu2 = NULL;
                }
//                if ( ((!type && !pu2) || type == 2) && entry < WAN_ROUT_NUM )
                if ( ((!type && !pu2) || type == 2) && entry < WAN_ROUT_TBL_CFG->wan_rout_num )
                {
                    //  WAN
                    pu2 = (u32*)WAN_ROUT_FORWARD_COMPARE_TBL(entry);
                    if ( *pu2 != *pu1 || pu2[1] != pu1[1] || pu2[2] != pu1[2] )
                        pu2 = NULL;
                }
            }
            else if ( !*pu1 && !pu1[1] )
            {
//                if ( type == 1 && entry < LAN_ROUT_NUM )
                if ( type == 1 && entry < LAN_ROUT_TBL_CFG->lan_rout_num )
                    pu2 = (u32*)LAN_ROUT_FORWARD_COMPARE_TBL(entry);
//                else if ( type == 2 && entry < WAN_ROUT_NUM )
                else if ( type == 2 && entry < WAN_ROUT_TBL_CFG->wan_rout_num )
                    pu2 = (u32*)WAN_ROUT_FORWARD_COMPARE_TBL(entry);
            }
        }
        if ( pu2 )
        {
            *pu2 = 0;
            *(pu2 + 1) = 0;
            *(pu2 + 2) = 0;
            *(pu2 + 3) = 0;
        }
    }
    else if ( operation == 1 && type && *(u32*)&compare_tbl && *((u32*)&compare_tbl + 1) )
    {
        pu2 = NULL;
        if ( entry < 0 )
        {
            int max_entry;

            //  add
            pu1 = (u32*)&compare_tbl;
            pu2 = type == 1 ? (u32*)LAN_ROUT_FORWARD_COMPARE_TBL(0) : (u32*)WAN_ROUT_FORWARD_COMPARE_TBL(0);
//            max_entry = type == 1 ? LAN_ROUT_NUM : WAN_ROUT_NUM;
            max_entry = type == 1 ? LAN_ROUT_TBL_CFG->lan_rout_num : WAN_ROUT_TBL_CFG->wan_rout_num;
            for ( entry = 0; entry < max_entry; entry++, pu2 += 4 )
                if ( *pu2 == *pu1 && pu2[1] == pu1[1] && pu2[2] == pu1[2] )
                    break;
            if ( entry == max_entry )
            {
                pu2 = type == 1 ? (u32*)LAN_ROUT_FORWARD_COMPARE_TBL(0) : (u32*)WAN_ROUT_FORWARD_COMPARE_TBL(0);
                for ( entry = 0; entry < max_entry; entry++, pu2 += 4 )
                    if ( !*pu2 && !pu2[1] )
                        break;
                if ( entry == max_entry )
                    entry = -1;
            }
        }
        else
        {
            int max_entry;
            int tmp_entry;

            //  replace
            pu1 = (u32*)&compare_tbl;
            pu2 = type == 1 ? (u32*)LAN_ROUT_FORWARD_COMPARE_TBL(0) : (u32*)WAN_ROUT_FORWARD_COMPARE_TBL(0);
//            max_entry = type == 1 ? LAN_ROUT_NUM : WAN_ROUT_NUM;
            max_entry = type == 1 ? LAN_ROUT_TBL_CFG->lan_rout_num : WAN_ROUT_TBL_CFG->wan_rout_num;
            for ( tmp_entry = 0; tmp_entry < max_entry; tmp_entry++, pu2 += 4 )
                if ( *pu2 == *pu1 && pu2[1] == pu1[1] && pu2[2] == pu1[2] )
                    break;
            if ( tmp_entry < max_entry )
                entry = tmp_entry;
//            else if ( !(type == 1 && entry < LAN_ROUT_NUM) && !(type == 2 && entry < WAN_ROUT_NUM) )
            else if ( !(type == 1 && entry < LAN_ROUT_TBL_CFG->lan_rout_num) && !(type == 2 && entry < WAN_ROUT_TBL_CFG->wan_rout_num) )
                entry = -1;
        }

        if ( entry >= 0 )
        {
            //  add or replace
            if ( type == 1 )
            {
                pu1 = (u32*)LAN_ROUT_FORWARD_COMPARE_TBL(entry);
                pu2 = (u32*)LAN_ROUT_FORWARD_ACTION_TBL(entry);
            }
            else
            {
                pu1 = (u32*)WAN_ROUT_FORWARD_COMPARE_TBL(entry);
                pu2 = (u32*)WAN_ROUT_FORWARD_ACTION_TBL(entry);
            }
            memcpy(pu1, &compare_tbl, sizeof(compare_tbl));
            memcpy(pu2, &action_tbl, sizeof(action_tbl));
        }
    }

    MOD_DEC_USE_COUNT;

    return count;
}

static int proc_read_genconf(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    int len_max = off + count;
    char *pstr;
    char str[2048];
    int llen = 0;
    int i;
    unsigned long bit;
    char *ppst[2];

    MOD_INC_USE_COUNT;

    pstr = *start = page;

    llen += sprintf(str, "ETX1_DMACH_ON:");
    for ( i = 0, bit = 1; i < 4; i++, bit <<= 1 )
        llen += sprintf(str + llen, " %d - %s", i + 1, (*ETX1_DMACH_ON & bit) ? "on " : "off");
    llen += sprintf(str + llen, "\n");

    switch ( ETH_PORTS_CFG->eth1_type )
    {
    case 0: ppst[0] = "LAN"; break;
    case 1: ppst[0] = "WAN"; break;
    case 2: ppst[0] = "Mix"; break;
    default: ppst[0] = "Unkown";
    }
    switch ( ETH_PORTS_CFG->eth0_type )
    {
    case 0: ppst[1] = "LAN"; break;
    case 1: ppst[1] = "WAN"; break;
    case 2: ppst[1] = "Mix"; break;
    default: ppst[1] = "Unkown";
    }
    llen += sprintf(str + llen, "ETH_PORTS_CFG: WAN VLAN ID - %d, ETH1 Type - %s, ETH0 Type - %s\n", ETH_PORTS_CFG->wan_vlanid, ppst[0], ppst[1]);

    llen += sprintf(str + llen, "LAN_ROUT_TBL_CFG: num - %d, off - %d, TCP/UDP ver - %s, IP ver - %s, TCP/UDP err drop - %s, no hit drop - %s\n",
                                                   LAN_ROUT_TBL_CFG->lan_rout_num,
                                                   LAN_ROUT_TBL_CFG->lan_rout_off,
                                                   LAN_ROUT_TBL_CFG->lan_tcpudp_ver_en ? "on " : "off",
                                                   LAN_ROUT_TBL_CFG->lan_ip_ver_en ? "on " : "off",
                                                   LAN_ROUT_TBL_CFG->lan_tcpudp_err_drop ? "on " : "off",
                                                   LAN_ROUT_TBL_CFG->lan_rout_drop  ? "on " : "off"
                                                   );

    llen += sprintf(str + llen, "WAN_ROUT_TBL_CFG: num - %d, MC num - %d, MC drop - %s, TCP/UDP ver - %s, IP ver - %s, TCP/UDP err drop - %s, no hit drop - %s\n",
                                                   WAN_ROUT_TBL_CFG->wan_rout_num,
                                                   WAN_ROUT_TBL_CFG->wan_rout_mc_num,
                                                   WAN_ROUT_TBL_CFG->wan_rout_mc_drop ? "on " : "off",
                                                   WAN_ROUT_TBL_CFG->wan_tcpdup_ver_en ? "on " : "off",
                                                   WAN_ROUT_TBL_CFG->wan_ip_ver_en ? "on " : "off",
                                                   WAN_ROUT_TBL_CFG->wan_tcpudp_err_drop ? "on " : "off",
                                                   WAN_ROUT_TBL_CFG->wan_rout_drop  ? "on " : "off"
                                                   );

    switch ( GEN_MODE_CFG->wan_acc_mode )
    {
    case 0: ppst[0] = "null"; break;
    case 2: ppst[0] = "routing"; break;
    default: ppst[0] = "unknown";
    }
    switch ( GEN_MODE_CFG->lan_acc_mode )
    {
    case 0: ppst[1] = "null"; break;
    case 2: ppst[1] = "routing"; break;
    default: ppst[1] = "unknown";
    }
    llen += sprintf(str + llen, "GEN_MODE_CFG: CPU1 fast mode - %s, ETH1 fast mode - %s, DPLUS wfq - %d, ETH1 wfq - %d, WAN mode - %s, LAN mode - %s\n",
                                GEN_MODE_CFG->cpu1_fast_mode ? "direct" : "indirect",
                                GEN_MODE_CFG->eth1_fast_mode ? "direct" : "indirect",
                                GEN_MODE_CFG->dplus_wfq,
                                GEN_MODE_CFG->eth1_wfq,
                                ppst[0],
                                ppst[1]
                                );

    if ( len <= off && len + llen > off )
    {
        memcpy(pstr, str + off - len, len + llen - off);
        pstr += len + llen - off;
    }
    else if ( len > off )
    {
        memcpy(pstr, str, llen);
        pstr += llen;
    }
    len += llen;
    if ( len >= len_max )
        goto PROC_READ_GENCONF_OVERRUN_END;

    *eof = 1;

    MOD_DEC_USE_COUNT;

    return len - off;

PROC_READ_GENCONF_OVERRUN_END:
    MOD_DEC_USE_COUNT;

    return len - llen - off;
}

static int proc_write_genconf(struct file *file, const char *buf, unsigned long count, void *data)
{
    return count;
}

static int proc_read_vlan(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    int len_max = off + count;
    char *pstr;
    char str[512];
    int llen;
    int i;

    MOD_INC_USE_COUNT;

    pstr = *start = page;

    llen = sprintf(pstr, "VLAN Table:\n");
    pstr += llen;
    len += llen;

    llen = sprintf(pstr, "  No. Prio Cfi  ID\n");
    pstr += llen;
    len += llen;

    for ( i = 0; i < 8; i++ )
    {
        llen = sprintf(str, "  %d: %4d %3d %4d\n", i + 1, *VLAN_CFG_TBL(i) >> 13, (*VLAN_CFG_TBL(i) >> 12) & 0x01, *VLAN_CFG_TBL(i) & 0x0FFF);
        if ( len <= off && len + llen > off )
        {
            memcpy(pstr, str + off - len, len + llen - off);
            pstr += len + llen - off;
        }
        else if ( len > off )
        {
            memcpy(pstr, str, llen);
            pstr += llen;
        }
        len += llen;
        if ( len >= len_max )
            goto PROC_READ_VLAN_OVERRUN_END;
    }

    *eof = 1;

    MOD_DEC_USE_COUNT;

    return len - off;

PROC_READ_VLAN_OVERRUN_END:
    MOD_DEC_USE_COUNT;

    return len - llen - off;
}

static int proc_write_vlan(struct file *file, const char *buf, unsigned long count, void *data)
{
    return count;
}

static int proc_read_pppoe(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    int len_max = off + count;
    char *pstr;
    char str[512];
    int llen;
    int i;

    MOD_INC_USE_COUNT;

    pstr = *start = page;

    llen = sprintf(pstr, "PPPoE Table (Session ID):\n");
    pstr += llen;
    len += llen;

    for ( i = 0; i < 8; i++ )
    {
        llen = sprintf(str, "  %d: %d\n", i + 1, *PPPOE_CFG_TBL(i));
        if ( len <= off && len + llen > off )
        {
            memcpy(pstr, str + off - len, len + llen - off);
            pstr += len + llen - off;
        }
        else if ( len > off )
        {
            memcpy(pstr, str, llen);
            pstr += llen;
        }
        len += llen;
        if ( len >= len_max )
            goto PROC_READ_PPPOE_OVERRUN_END;
    }

    *eof = 1;

    MOD_DEC_USE_COUNT;

    return len - off;

PROC_READ_PPPOE_OVERRUN_END:
    MOD_DEC_USE_COUNT;

    return len - llen - off;
}

static int proc_write_pppoe(struct file *file, const char *buf, unsigned long count, void *data)
{
    return count;
}

static int proc_read_mtu(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    int len_max = off + count;
    char *pstr;
    char str[512];
    int llen;
    int i;

    MOD_INC_USE_COUNT;

    pstr = *start = page;

    llen = sprintf(pstr, "MTU Table:\n");
    pstr += llen;
    len += llen;

    for ( i = 0; i < 8; i++ )
    {
        llen = sprintf(str, "  %d: %d\n", i + 1, *MTU_CFG_TBL(i));
        if ( len <= off && len + llen > off )
        {
            memcpy(pstr, str + off - len, len + llen - off);
            pstr += len + llen - off;
        }
        else if ( len > off )
        {
            memcpy(pstr, str, llen);
            pstr += llen;
        }
        len += llen;
        if ( len >= len_max )
            goto PROC_READ_MTU_OVERRUN_END;
    }

    *eof = 1;

    MOD_DEC_USE_COUNT;

    return len - off;

PROC_READ_MTU_OVERRUN_END:
    MOD_DEC_USE_COUNT;

    return len - llen - off;
}

static int proc_write_mtu(struct file *file, const char *buf, unsigned long count, void *data)
{
    return count;
}

static int proc_read_hit(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    int len_max = off + count;
    char *pstr;
    char str[1024];
    int llen;
    int i;
    int n;
    unsigned long bit;

    MOD_INC_USE_COUNT;

    pstr = *start = page;

    llen = sprintf(pstr, "Unicast Hit Table:\n");
    pstr += llen;
    len += llen;

#if 0
    for ( i = 0; i < 12; i++ )
    {
        llen = sprintf(str, "  %3d - %3d:", i * 32 + 1, (i + 1) * 32);

        for ( bit = 1; bit; bit <<= 1 )
            llen += sprintf(str + llen, " %d", (*ROUT_FWD_HIT_STAT_TBL(i) & bit) ? 1 : 0);

        llen += sprintf(str + llen, "\n");

        if ( len <= off && len + llen > off )
        {
            memcpy(pstr, str + off - len, len + llen - off);
            pstr += len + llen - off;
        }
        else if ( len > off )
        {
            memcpy(pstr, str, llen);
            pstr += llen;
        }
        len += llen;
        if ( len >= len_max )
            goto PROC_READ_HIT_OVERRUN_END;
    }
#else
    llen = sprintf(pstr, "             1 2 3 4 5 6 7 8 9 10\n");
    pstr += llen;
    len += llen;

    n = 1;
    for ( i = 0; i < 12; i++ )
        for ( bit = 0x80000000; bit; bit >>= 1 )
        {
            if ( n % 10 == 1 )
                llen = sprintf(str, "  %3d - %3d:", n, n + 9);

            llen += sprintf(str + llen, " %d", (*ROUT_FWD_HIT_STAT_TBL(i) & bit) ? 1 : 0);

            if ( n++ % 10 == 0 )
            {
                llen += sprintf(str + llen, "\n");

                if ( len <= off && len + llen > off )
                {
                    memcpy(pstr, str + off - len, len + llen - off);
                    pstr += len + llen - off;
                }
                else if ( len > off )
                {
                    memcpy(pstr, str, llen);
                    pstr += llen;
                }
                len += llen;
                if ( len >= len_max )
                    goto PROC_READ_HIT_OVERRUN_END;
            }
        }

    if ( n % 10 != 0 )
    {
        llen += sprintf(str + llen, "\n");

        if ( len <= off && len + llen > off )
        {
            memcpy(pstr, str + off - len, len + llen - off);
            pstr += len + llen - off;
        }
        else if ( len > off )
        {
            memcpy(pstr, str, llen);
            pstr += llen;
        }
        len += llen;
        if ( len >= len_max )
            goto PROC_READ_HIT_OVERRUN_END;
    }

#endif

    *eof = 1;

    MOD_DEC_USE_COUNT;

    return len - off;

PROC_READ_HIT_OVERRUN_END:
    MOD_DEC_USE_COUNT;

    return len - llen - off;
}

static int proc_write_hit(struct file *file, const char *buf, unsigned long count, void *data)
{
    return count;
}

static int proc_read_mac(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    int len_max = off + count;
    char *pstr;
    char str[512];
    int llen;
    int i;
    unsigned int mac52, mac10;

    MOD_INC_USE_COUNT;

    pstr = *start = page;

    llen = sprintf(pstr, "MAC Table:\n");
    pstr += llen;
    len += llen;

    llen = sprintf(pstr, "  WAN\n");
    pstr += llen;
    len += llen;

    for ( i = 0; i < 8; i++ )
    {
        mac52 = *WAN_ROUT_MAC_CFG_TBL(i);
        mac10 = *(WAN_ROUT_MAC_CFG_TBL(i) + 1);

        llen = sprintf(str, "    %d: %02X:%02X:%02X:%02X:%02X:%02X\n", i + 1, mac52 >> 24, (mac52 >> 16) & 0xFF, (mac52 >> 8) & 0xFF, mac52 & 0xFF, mac10 >> 24, (mac10 >> 16) & 0xFF);
        if ( len <= off && len + llen > off )
        {
            memcpy(pstr, str + off - len, len + llen - off);
            pstr += len + llen - off;
        }
        else if ( len > off )
        {
            memcpy(pstr, str, llen);
            pstr += llen;
        }
        len += llen;
        if ( len >= len_max )
            goto PROC_READ_MAC_OVERRUN_END;
    }

    llen = sprintf(str, "  LAN\n");
    if ( len <= off && len + llen > off )
    {
        memcpy(pstr, str + off - len, len + llen - off);
        pstr += len + llen - off;
    }
    else if ( len > off )
    {
        memcpy(pstr, str, llen);
        pstr += llen;
    }
    len += llen;
    if ( len >= len_max )
        goto PROC_READ_MAC_OVERRUN_END;

    for ( i = 0; i < 8; i++ )
    {
        mac52 = *LAN_ROUT_MAC_CFG_TBL(i);
        mac10 = *(LAN_ROUT_MAC_CFG_TBL(i) + 1);

        llen = sprintf(str, "    %d: %02X:%02X:%02X:%02X:%02X:%02X\n", i + 1, mac52 >> 24, (mac52 >> 16) & 0xFF, (mac52 >> 8) & 0xFF, mac52 & 0xFF, mac10 >> 24, (mac10 >> 16) & 0xFF);
        if ( len <= off && len + llen > off )
        {
            memcpy(pstr, str + off - len, len + llen - off);
            pstr += len + llen - off;
        }
        else if ( len > off )
        {
            memcpy(pstr, str, llen);
            pstr += llen;
        }
        len += llen;
        if ( len >= len_max )
            goto PROC_READ_MAC_OVERRUN_END;
    }

    *eof = 1;

    MOD_DEC_USE_COUNT;

    return len - off;

PROC_READ_MAC_OVERRUN_END:
    MOD_DEC_USE_COUNT;

    return len - llen - off;
}

static int proc_write_mac(struct file *file, const char *buf, unsigned long count, void *data)
{
    return count;
}

#endif

#if defined(ENABLE_DBG_PROC) && ENABLE_DBG_PROC
static int proc_read_dbg(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;

    MOD_INC_USE_COUNT;

    if ( dbg_enable )
        len += sprintf(page + off + len, "debug enabled (dbg_enable = %08X)\n", dbg_enable);
    else
        len += sprintf(page + off + len, "debug disabled\n");

    MOD_DEC_USE_COUNT;

    *eof = 1;

    return len;
}

static int proc_write_dbg(struct file *file, const char *buf, unsigned long count, void *data)
{
    char str[64];
    char *p;

    int len, rlen;

    MOD_INC_USE_COUNT;

    len = count < sizeof(str) ? count : sizeof(str) - 1;
    rlen = len - copy_from_user(str, buf, len);
    while ( rlen && str[rlen - 1] <= ' ' )
        rlen--;
    str[rlen] = 0;
    for ( p = str; *p && *p <= ' '; p++, rlen-- );
    if ( !*p )
    {
        MOD_DEC_USE_COUNT;
        return 0;
    }

    if ( stricmp(str, "enable") == 0 )
        dbg_enable = 1;
    else
        dbg_enable = 0;

    MOD_DEC_USE_COUNT;

    return count;
}
#endif

#if defined(DEBUG_FIRMWARE_PROC) && DEBUG_FIRMWARE_PROC
static int proc_read_fw(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;

    MOD_INC_USE_COUNT;

    len += sprintf(page + off + len, "Firmware\n");
    len += sprintf(page + off + len, "  ACC_ERX_PID        = %08X\n", *(u32*)0xBE194200);
    len += sprintf(page + off + len, "  ACC_ERX_PORT_TIMES = %08X\n", *(u32*)0xBE194204);
    len += sprintf(page + off + len, "  SLL_ISSUED         = %08X\n", *(u32*)0xBE194208);
    len += sprintf(page + off + len, "  BMC_ISSUED         = %08X\n", *(u32*)0xBE19420C);

    len += sprintf(page + off + len, "  PRESEARCH_RDPTR    = %08X, %08X\n", *(u32*)0xBE194210, *(u32*)0xBE194214);

    len += sprintf(page + off + len, "  SLL_ERX_PID        = %08X\n", *(u32*)0xBE19421C);
    len += sprintf(page + off + len, "  SLL_PKT_CNT        = %08X, %08X\n", *(u32*)0xBE194220, *(u32*)0xBE194224);
    len += sprintf(page + off + len, "  SLL_RDPTR          = %08X, %08X\n", *(u32*)0xBE194228, *(u32*)0xBE19422C);

    len += sprintf(page + off + len, "  EDIT_PKT_CNT       = %08X, %08X\n", *(u32*)0xBE194234, *(u32*)0xBE194238);
    len += sprintf(page + off + len, "  EDIT_RDPTR         = %08X, %08X\n", *(u32*)0xBE19423C, *(u32*)0xBE194240);

    len += sprintf(page + off + len, "  DPLUSRX_PKT_CNT    = %08X, %08X\n", *(u32*)0xBE194248, *(u32*)0xBE19424C);
    len += sprintf(page + off + len, "  ENETS_RDPTR        = %08X, %08X\n", *(u32*)0xBE194250, *(u32*)0xBE194254);

    len += sprintf(page + off + len, "  POSTSEARCH_RDPTR   = %08X, %08X\n", *(u32*)0xBE19425C, *(u32*)0xBE194260);

    MOD_DEC_USE_COUNT;

    *eof = 1;

    return len;
}
#endif

#if defined(DEBUG_MEM_PROC) && DEBUG_MEM_PROC
static int proc_write_mem(struct file *file, const char *buf, unsigned long count, void *data)
{
    char *p1, *p2;
    int len;
    int colon;
    unsigned long *p;
    char local_buf[1024];
    int i, n;

    MOD_INC_USE_COUNT;

    len = sizeof(local_buf) < count ? sizeof(local_buf) : count;
    len = len - copy_from_user(local_buf, buf, len);
    local_buf[len] = 0;

    p1 = local_buf;
    colon = 1;
   while ( get_token(&p1, &p2, &len, &colon) )
    {
        if ( stricmp(p1, "w") == 0 || stricmp(p1, "write") == 0 || stricmp(p1, "r") == 0 || stricmp(p1, "read") == 0 )
            break;

        p1 = p2;
        colon = 1;
    }

    if ( *p1 == 'w' )
    {
        ignore_space(&p2, &len);
        p = (unsigned long *)get_number(&p2, &len, 1);
        if ( (u32)p >= KSEG0 )
            while ( 1 )
            {
                ignore_space(&p2, &len);
                if ( !len || !((*p2 >= '0' && *p2 <= '9') || (*p2 >= 'a' && *p2 <= 'f') || (*p2 >= 'A' && *p2 <= 'F')) )
                    break;

                *p++ = (u32)get_number(&p2, &len, 1);
            }
    }
    else if ( *p1 == 'r' )
    {
        ignore_space(&p2, &len);
        p = (unsigned long *)get_number(&p2, &len, 1);
        if ( (u32)p >= KSEG0 )
        {
            ignore_space(&p2, &len);
            n = (int)get_number(&p2, &len, 0);
            if ( n )
            {
                n += (((int)p >> 2) & 0x03);
                p = (unsigned long *)((u32)p & ~0x0F);
                for ( i = 0; i < n; i++ )
                {
                    if ( (i & 0x03) == 0 )
                        printk("%08X:", (u32)p);
                    printk(" %08X", (u32)*p++);
                    if ( (i & 0x03) == 0x03 )
                        printk("\n");
                }
                if ( (n & 0x03) != 0x00 )
                    printk("\n");
            }
        }
    }

    MOD_DEC_USE_COUNT;

    return count;
}
#endif

#if defined(DEBUG_DMA_PROC) && DEBUG_DMA_PROC
static int proc_read_dma(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    int len_max = off + count;
    char *pstr;
    char str[512];
    int llen;
    int i;

    MOD_INC_USE_COUNT;

    pstr = *start = page;

  #if defined(DEBUG_DMA_PROC_UPSTREAM) && DEBUG_DMA_PROC_UPSTREAM
    if ( (dbg_dma_enable & 0x01) )
    {
    llen = sprintf(str, "DMA (eth0 -> eth1)\n");
    if ( len <= off && len + llen > off )
    {
        memcpy(pstr, str + off - len, len + llen - off);
        pstr += len + llen - off;
    }
    else if ( len > off )
    {
        memcpy(pstr, str, llen);
        pstr += llen;
    }
    len += llen;
    if ( len >= len_max )
        goto PROC_READ_DMA_OVERRUN_END;

    llen = sprintf(str, "  rx\n");
    if ( len <= off && len + llen > off )
    {
        memcpy(pstr, str + off - len, len + llen - off);
        pstr += len + llen - off;
    }
    else if ( len > off )
    {
        memcpy(pstr, str, llen);
        pstr += llen;
    }
    len += llen;
    if ( len >= len_max )
        goto PROC_READ_DMA_OVERRUN_END;

    for ( i = 0; i < DMA_RX_CH1_DESC_NUM; i++ )
    {
        llen = print_dma_desc(str, 1, i, 1);
        if ( len <= off && len + llen > off )
        {
            memcpy(pstr, str + off - len, len + llen - off);
            pstr += len + llen - off;
        }
        else if ( len > off )
        {
            memcpy(pstr, str, llen);
            pstr += llen;
        }
        len += llen;
        if ( len >= len_max )
            goto PROC_READ_DMA_OVERRUN_END;
    }

    llen = sprintf(str, "  tx\n");
    if ( len <= off && len + llen > off )
    {
        memcpy(pstr, str + off - len, len + llen - off);
        pstr += len + llen - off;
    }
    else if ( len > off )
    {
        memcpy(pstr, str, llen);
        pstr += llen;
    }
    len += llen;
    if ( len >= len_max )
        goto PROC_READ_DMA_OVERRUN_END;

    for ( i = 0; i < ETH1_TX_DESC_NUM; i++ )
    {
        llen = print_ema_desc(str, 1, i, 0);
        if ( len <= off && len + llen > off )
        {
            memcpy(pstr, str + off - len, len + llen - off);
            pstr += len + llen - off;
        }
        else if ( len > off )
        {
            memcpy(pstr, str, llen);
            pstr += llen;
        }
        len += llen;
        if ( len >= len_max )
            goto PROC_READ_DMA_OVERRUN_END;
    }
    }
  #endif

  #if defined(DEBUG_DMA_PROC_DOWNSTREAM) && DEBUG_DMA_PROC_DOWNSTREAM
    if ( (dbg_dma_enable & 0x02) )
    {
    llen = sprintf(str, "DMA (eth1 -> eth0)\n");
    if ( len <= off && len + llen > off )
    {
        memcpy(pstr, str + off - len, len + llen - off);
        pstr += len + llen - off;
    }
    else if ( len > off )
    {
        memcpy(pstr, str, llen);
        pstr += llen;
    }
    len += llen;
    if ( len >= len_max )
        goto PROC_READ_DMA_OVERRUN_END;

    llen = sprintf(str, "  rx\n");
    if ( len <= off && len + llen > off )
    {
        memcpy(pstr, str + off - len, len + llen - off);
        pstr += len + llen - off;
    }
    else if ( len > off )
    {
        memcpy(pstr, str, llen);
        pstr += llen;
    }
    len += llen;
    if ( len >= len_max )
        goto PROC_READ_DMA_OVERRUN_END;

    for ( i = 0; i < DMA_RX_CH2_DESC_NUM; i++ )
    {
        llen = print_dma_desc(str, 2, i, 1);
        if ( len <= off && len + llen > off )
        {
            memcpy(pstr, str + off - len, len + llen - off);
            pstr += len + llen - off;
        }
        else if ( len > off )
        {
            memcpy(pstr, str, llen);
            pstr += llen;
        }
        len += llen;
        if ( len >= len_max )
            goto PROC_READ_DMA_OVERRUN_END;
    }

    llen = sprintf(str, "  tx\n");
    if ( len <= off && len + llen > off )
    {
        memcpy(pstr, str + off - len, len + llen - off);
        pstr += len + llen - off;
    }
    else if ( len > off )
    {
        memcpy(pstr, str, llen);
        pstr += llen;
    }
    len += llen;
    if ( len >= len_max )
        goto PROC_READ_DMA_OVERRUN_END;

    for ( i = 0; i < DMA_TX_CH2_DESC_NUM; i++ )
    {
        llen = print_dma_desc(str, 2, i, 0);
        if ( len <= off && len + llen > off )
        {
            memcpy(pstr, str + off - len, len + llen - off);
            pstr += len + llen - off;
        }
        else if ( len > off )
        {
            memcpy(pstr, str, llen);
            pstr += llen;
        }
        len += llen;
        if ( len >= len_max )
            goto PROC_READ_DMA_OVERRUN_END;
    }
    }
  #endif

    *eof = 1;

    MOD_DEC_USE_COUNT;

    return len - off;

PROC_READ_DMA_OVERRUN_END:
    MOD_DEC_USE_COUNT;

    return len - llen - off;
}

static int proc_write_dma(struct file *file, const char *buf, unsigned long count, void *data)
{
    char str[64];
    char *p;

    int len, rlen;

    MOD_INC_USE_COUNT;

    len = count < sizeof(str) ? count : sizeof(str) - 1;
    rlen = len - copy_from_user(str, buf, len);
    while ( rlen && str[rlen - 1] <= ' ' )
        rlen--;
    str[rlen] = 0;
    for ( p = str; *p && *p <= ' '; p++, rlen-- );
    if ( !*p )
    {
        MOD_DEC_USE_COUNT;
        return 0;
    }

    if ( stricmp(str, "enable") == 0 )
    {
        if ( !dbg_dma_enable )
            dbg_dma_enable = 0x03;
    }
    else if ( stricmp(str, "disable") == 0 )
        dbg_dma_enable = 0x00;
    else if ( stricmp(str, "enable up") == 0 || stricmp(str, "enable upstream") == 0 )
        dbg_dma_enable |= 0x01;
    else if ( stricmp(str, "disable up") == 0 || stricmp(str, "disable upstream") == 0 )
        dbg_dma_enable &= ~0x01;
    else if ( stricmp(str, "enable down") == 0 || stricmp(str, "enable downstream") == 0 )
        dbg_dma_enable |= 0x02;
    else if ( stricmp(str, "disable down") == 0 || stricmp(str, "disable downstream") == 0 )
        dbg_dma_enable &= ~0x02;
    else
    {
        printk("echo \"<command>\" > /proc/eth/dma\n");
        printk("  command:\n");
        printk("    enable [up/down]  - enable up/down stream dma dump\n");
        printk("    disable [up/down] - disable up/down stream dma dump\n");
    }

    MOD_DEC_USE_COUNT;

    return count;
}
#endif

#if defined(DEBUG_PP32_PROC) && DEBUG_PP32_PROC
static int proc_read_pp32(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    static const char *halt_stat[] = {
        "reset",
        "break in line",
        "stop",
        "step",
        "code",
        "data0",
        "data1"
    };
    static const char *brk_src_data[] = {
        "off",
        "read",
        "write",
        "read/write",
        "write_equal",
        "N/A",
        "N/A",
        "N/A"
    };
    static const char *brk_src_code[] = {
        "off",
        "on"
    };

    int len = 0;
    int i;
    int k;
    unsigned long bit;

    MOD_INC_USE_COUNT;

    len += sprintf(page + off + len, "Task No %d, PC %04x\n", *PP32_DBG_TASK_NO & 0x03, *PP32_DBG_CUR_PC & 0xFFFF);

    if ( !(*PP32_HALT_STAT & 0x01) )
        len += sprintf(page + off + len, "  Halt State: Running\n");
    else
    {
        len += sprintf(page + off + len, "  Halt State: Stopped");
        k = 0;
        for ( bit = 2, i = 0; bit <= (1 << 7); bit <<= 1, i++ )
            if ( (*PP32_HALT_STAT & bit) )
            {
                if ( !k )
                {
                    len += sprintf(page + off + len, ", ");
                    k++;
                }
                else
                    len += sprintf(page + off + len, " | ");
                len += sprintf(page + off + len, halt_stat[i]);
            }

        len += sprintf(page + off + len, "\n");
    }

    len += sprintf(page + off + len, "  Break Src:  data1 - %s, data0 - %s, pc3 - %s, pc2 - %s, pc1 - %s, pc0 - %s\n",
                                                    brk_src_data[(*PP32_BRK_SRC >> 11) & 0x07], brk_src_data[(*PP32_BRK_SRC >> 8) & 0x07], brk_src_code[(*PP32_BRK_SRC >> 3) & 0x01], brk_src_code[(*PP32_BRK_SRC >> 2) & 0x01], brk_src_code[(*PP32_BRK_SRC >> 1) & 0x01], brk_src_code[*PP32_BRK_SRC & 0x01]);

    for ( i = 0; i < 4; i++ )
        len += sprintf(page + off + len, "    pc%d:      %04x - %04x\n", i, *PP32_DBG_PC_MIN(i), *PP32_DBG_PC_MAX(i));

    for ( i = 0; i < 2; i++ )
        len += sprintf(page + off + len, "    data%d:    %04x - %04x (%08x)\n", i, *PP32_DBG_DATA_MIN(i), *PP32_DBG_DATA_MAX(i), *PP32_DBG_DATA_VAL(i));

    MOD_DEC_USE_COUNT;

    *eof = 1;

    return len;
}

static int proc_write_pp32(struct file *file, const char *buf, unsigned long count, void *data)
{
    char str[64];
    char *p;

    int len, rlen;

    MOD_INC_USE_COUNT;

    len = count < sizeof(str) ? count : sizeof(str) - 1;
    rlen = len - copy_from_user(str, buf, len);
    while ( rlen && str[rlen - 1] <= ' ' )
        rlen--;
    str[rlen] = 0;
    for ( p = str; *p && *p <= ' '; p++, rlen-- );
    if ( !*p )
    {
        MOD_DEC_USE_COUNT;
        return 0;
    }

    if ( stricmp(str, "start") == 0 )
        *PP32_DBG_CTRL = DBG_CTRL_START_SET(1);
    else if ( stricmp(str, "stop") == 0 )
        *PP32_DBG_CTRL = DBG_CTRL_STOP_SET(1);
    else if ( stricmp(str, "step") == 0 )
        *PP32_DBG_CTRL = DBG_CTRL_STEP_SET(1);
    else
    {
        printk("echo \"<command>\" > /proc/eth/etop\n");
        printk("  command:\n");
        printk("    start - run pp32\n");
        printk("    stop  - stop pp32\n");
        printk("    step  - run pp32 with one step only\n");
        printk("    help  - print this screen\n");
    }

    MOD_DEC_USE_COUNT;

    return count;
}
#endif

static INLINE int stricmp(const char *p1, const char *p2)
{
    int c1, c2;

    while ( *p1 && *p2 )
    {
        c1 = *p1 >= 'A' && *p1 <= 'Z' ? *p1 + 'a' - 'A' : *p1;
        c2 = *p2 >= 'A' && *p2 <= 'Z' ? *p2 + 'a' - 'A' : *p2;
        if ( (c1 -= c2) )
            return c1;
        p1++;
        p2++;
    }

    return *p1 - *p2;
}

#if defined(DEBUG_FIRMWARE_TABLES_PROC) && DEBUG_FIRMWARE_TABLES_PROC
static INLINE int strincmp(const char *p1, const char *p2, int n)
{
    int c1 = 0, c2;

    while ( n && *p1 && *p2 )
    {
        c1 = *p1 >= 'A' && *p1 <= 'Z' ? *p1 + 'a' - 'A' : *p1;
        c2 = *p2 >= 'A' && *p2 <= 'Z' ? *p2 + 'a' - 'A' : *p2;
        if ( (c1 -= c2) )
            return c1;
        p1++;
        p2++;
        n--;
    }

    return n ? *p1 - *p2 : c1;
}
#endif

static INLINE int get_token(char **p1, char **p2, int *len, int *colon)
{
    int tlen = 0;

    while ( *len && !((**p1 >= 'A' && **p1 <= 'Z') || (**p1 >= 'a' && **p1<= 'z')) )
    {
        (*p1)++;
        (*len)--;
    }
    if ( !*len )
        return 0;

    if ( *colon )
    {
        *colon = 0;
        *p2 = *p1;
        while ( *len && **p2 > ' ' && **p2 != ',' )
        {
            if ( **p2 == ':' )
            {
                *colon = 1;
                break;
            }
            (*p2)++;
            (*len)--;
            tlen++;
        }
        **p2 = 0;
    }
    else
    {
        *p2 = *p1;
        while ( *len && **p2 > ' ' && **p2 != ',' )
        {
            (*p2)++;
            (*len)--;
            tlen++;
        }
        **p2 = 0;
    }

    return tlen;
}

static INLINE int get_number(char **p, int *len, int is_hex)
{
    int ret = 0;
    int n = 0;

    if ( is_hex )
    {
        while ( *len && ((**p >= '0' && **p <= '9') || (**p >= 'a' && **p <= 'f') || (**p >= 'A' && **p <= 'F')) )
        {
            if ( **p >= '0' && **p <= '9' )
                n = **p - '0';
            else if ( **p >= 'a' && **p <= 'f' )
                n = **p - 'a' + 10;
            else if ( **p >= 'A' && **p <= 'F' )
                n = **p - 'A' + 10;
            ret = (ret << 4) | n;
            (*p)++;
            (*len)--;
        }
    }
    else
    {
        while ( *len && **p >= '0' && **p <= '9' )
        {
            n = **p - '0';
            ret = ret * 10 + n;
            (*p)++;
            (*len)--;
        }
    }

    return ret;
}

#if defined(DEBUG_FIRMWARE_TABLES_PROC) && DEBUG_FIRMWARE_TABLES_PROC
static INLINE void get_ip_port(char **p, int *len, unsigned int *val)
{
    int i;

    memset(val, 0, sizeof(*val) * 5);

    for ( i = 0; i < 5; i++ )
    {
        ignore_space(p, len);
        if ( !*len )
            break;
        val[i] = get_number(p, len, 0);
    }
}

static INLINE void get_mac(char **p, int *len, unsigned int *val)
{
    int i;

    memset(val, 0, sizeof(*val) * 6);

    for ( i = 0; i < 6; i++ )
    {
        ignore_space(p, len);
        if ( !*len )
            break;
        val[i] = get_number(p, len, 1);
    }
}
#endif

static INLINE void ignore_space(char **p, int *len)
{
    while ( *len && (**p <= ' ' || **p == ':' || **p == '.' || **p == ',') )
    {
        (*p)++;
        (*len)--;
    }
}

#if defined(DEBUG_FIRMWARE_TABLES_PROC) && DEBUG_FIRMWARE_TABLES_PROC
static INLINE int print_wan_route(char *buf, int i, struct rout_forward_compare_tbl *pcompare, struct wan_rout_forward_action_tbl *pwaction)
{
    static const char *dest_list[] = {"ETH0", "ETH1", "CPU0", "EXT_INT1", "EXT_INT2", "EXT_INT3", "EXT_INT4", "EXT_INT5"};

    int len = 0;
    u32 bit;
    int j, k;

    len += sprintf(buf + len,          "  entry %d\n", i);
    len += sprintf(buf + len,          "    compare\n");
    len += sprintf(buf + len,          "      src:  %d.%d.%d.%d:%d\n", pcompare->src_ip >> 24,  (pcompare->src_ip >> 16) & 0xFF,  (pcompare->src_ip >> 8) & 0xFF,  pcompare->src_ip & 0xFF, pcompare->src_port);
    len += sprintf(buf + len,          "      dest: %d.%d.%d.%d:%d\n", pcompare->dest_ip >> 24, (pcompare->dest_ip >> 16) & 0xFF, (pcompare->dest_ip >> 8) & 0xFF, pcompare->dest_ip & 0xFF, pcompare->dest_port);
    len += sprintf(buf + len,          "    action\n");
    len += sprintf(buf + len,          "      new dest:    %d.%d.%d.%d:%d\n", pwaction->wan_new_dest_ip >> 24, (pwaction->wan_new_dest_ip >> 16) & 0xFF, (pwaction->wan_new_dest_ip >> 8) & 0xFF, pwaction->wan_new_dest_ip & 0xFF, pwaction->wan_new_dest_port);
    len += sprintf(buf + len,          "      new MAC :    %02X:%02X:%02X:%02X:%02X:%02X\n", (pwaction->wan_new_dest_mac54 >> 8) & 0xFF, pwaction->wan_new_dest_mac54 & 0xFF, pwaction->wan_new_dest_mac30 >> 24, (pwaction->wan_new_dest_mac30 >> 16) & 0xFF, (pwaction->wan_new_dest_mac30 >> 8) & 0xFF, pwaction->wan_new_dest_mac30 & 0xFF);
    switch ( pwaction->rout_type )
    {
    case 1:  len += sprintf(buf + len, "      route type:  IPv4\n"); break;
    case 2:  len += sprintf(buf + len, "      route type:  NAT\n");  break;
    case 3:  len += sprintf(buf + len, "      route type:  NAPT\n"); break;
    default: len += sprintf(buf + len, "      route type:  NULL\n");
    }
    if ( pwaction->new_dscp_en )
        len += sprintf(buf + len,      "      new DSCP:    %d\n", pwaction->new_dscp);
    else
        len += sprintf(buf + len,      "      new DSCP:    original (not modified)\n");
    len += sprintf(buf + len,          "      MTU index:   %d\n", pwaction->mtu_ix);
    if ( pwaction->vlan_ins )
        len += sprintf(buf + len,      "      VLAN insert: enable, index %d\n", pwaction->vlan_ix);
    else
        len += sprintf(buf + len,      "      VLAN insert: disable\n");
    len += sprintf(buf + len,          "      VLAN remove: %s\n", pwaction->vlan_rm ? "enable" : "disable");
    if ( !pwaction->dest_list )
        len += sprintf(buf + len,      "      dest list:   none\n");
    else
    {
        len += sprintf(buf + len,      "      dest list:   ");
        for ( bit = 1, j = k = 0; bit < 1 << 8; bit <<= 1, j++ )
            if ( (pwaction->dest_list & bit) )
            {
                if ( k )
                    len += sprintf(buf + len, ", ");
                len += sprintf(buf + len, dest_list[j]);
                k = 1;
            }
        len += sprintf(buf + len, "\n");
    }
    if ( pwaction->pppoe_mode )
        len += sprintf(buf + len,      "      PPPoE mode:  termination\n");
    else
        len += sprintf(buf + len,      "      PPPoE mode:  transparent\n");
    len += sprintf(buf + len,          "      PPPoE index: %d\n", pwaction->pppoe_ix);
    len += sprintf(buf + len,          "      dest ch id:  %d\n", pwaction->dest_chid);

    return len;
}

static INLINE int print_lan_route(char *buf, int i, struct rout_forward_compare_tbl *pcompare, struct lan_rout_forward_action_tbl *plaction)
{
    static const char *dest_list[] = {"ETH0", "ETH1", "CPU0", "EXT_INT1", "EXT_INT2", "EXT_INT3", "EXT_INT4", "EXT_INT5"};

    int len = 0;
    u32 bit;
    int j, k;

    len += sprintf(buf + len,          "  entry %d\n", i);
    len += sprintf(buf + len,          "    compare\n");
    len += sprintf(buf + len,          "      src:  %d.%d.%d.%d:%d\n", pcompare->src_ip >> 24,  (pcompare->src_ip >> 16) & 0xFF,  (pcompare->src_ip >> 8) & 0xFF,  pcompare->src_ip & 0xFF, pcompare->src_port);
    len += sprintf(buf + len,          "      dest: %d.%d.%d.%d:%d\n", pcompare->dest_ip >> 24, (pcompare->dest_ip >> 16) & 0xFF, (pcompare->dest_ip >> 8) & 0xFF, pcompare->dest_ip & 0xFF, pcompare->dest_port);
    len += sprintf(buf + len,          "    action\n");
    len += sprintf(buf + len,          "      new src:     %d.%d.%d.%d:%d\n", plaction->lan_new_src_ip >> 24, (plaction->lan_new_src_ip >> 16) & 0xFF, (plaction->lan_new_src_ip >> 8) & 0xFF, plaction->lan_new_src_ip & 0xFF, plaction->lan_new_src_port);
    len += sprintf(buf + len,          "      new MAC:     %02X:%02X:%02X:%02X:%02X:%02X\n", (plaction->lan_new_dest_mac54 >> 8) & 0xFF, plaction->lan_new_dest_mac54 & 0xFF, plaction->lan_new_dest_mac30 >> 24, (plaction->lan_new_dest_mac30 >> 16) & 0xFF, (plaction->lan_new_dest_mac30 >> 8) & 0xFF, plaction->lan_new_dest_mac30 & 0xFF);
    switch ( plaction->rout_type )
    {
    case 1:  len += sprintf(buf + len, "      route type:  IPv4\n"); break;
    case 2:  len += sprintf(buf + len, "      route type:  NAT\n");  break;
    case 3:  len += sprintf(buf + len, "      route type:  NAPT\n"); break;
    default: len += sprintf(buf + len, "      route type:  NULL\n");
    }
    if ( plaction->new_dscp_en )
        len += sprintf(buf + len,      "      new DSCP:    %d\n", plaction->new_dscp);
    else
        len += sprintf(buf + len,      "      new DSCP:    original (not modified)\n");
    len += sprintf(buf + len,          "      MTU index:   %d\n", plaction->mtu_ix);
    if ( plaction->vlan_ins )
        len += sprintf(buf + len,      "      VLAN insert: enable, index %d\n", plaction->vlan_ix);
    else
        len += sprintf(buf + len,      "      VLAN insert: disable\n");
    len += sprintf(buf + len,          "      VLAN remove: %s\n", plaction->vlan_rm ? "enable" : "disable");
    if ( !plaction->dest_list )
        len += sprintf(buf + len,      "      dest list:   none\n");
    else
    {
        len += sprintf(buf + len,      "      dest list:   ");
        for ( bit = 1, j = k = 0; bit < 1 << 8; bit <<= 1, j++ )
            if ( (plaction->dest_list & bit) )
            {
                if ( k )
                    len += sprintf(buf + len, ", ");
                len += sprintf(buf + len, dest_list[j]);
                k = 1;
            }
        len += sprintf(buf + len, "\n");
    }
    if ( plaction->pppoe_mode )
        len += sprintf(buf + len,      "      PPPoE mode:  termination\n");
    else
        len += sprintf(buf + len,      "      PPPoE mode:  transparent\n");
    len += sprintf(buf + len,          "      PPPoE index: %d\n", plaction->pppoe_ix);
    len += sprintf(buf + len,          "      dest ch id:  %d\n", plaction->dest_chid);

    return len;
}
#endif

#if defined(DEBUG_DMA_PROC) && DEBUG_DMA_PROC
static INLINE int print_dma_desc(char *buf, int channel, int desc, int is_rx)
{
    volatile u32 *pdesc = NULL;
    volatile _tx_desc *ptx;
    int len = 0;

    if ( is_rx )
    {
        if ( channel == 1 )
            pdesc = DMA_RX_CH1_DESC_BASE;
        else if ( channel == 2 )
            pdesc = DMA_RX_CH2_DESC_BASE;
    }
    else
    {
        if ( channel == 2 )
            pdesc = DMA_TX_CH2_DESC_BASE;
    }
    if ( !pdesc )
        return 0;

    ptx = (volatile _tx_desc *)&pdesc[desc * 2];
    len += sprintf(buf + len, "    %2d (%08X). %08X %08X, own %d, c %d, sop %d, eop %d, off %d, addr %08X, len %d\n",
                                        desc, (u32)(pdesc + desc * 2), pdesc[desc * 2], pdesc[desc * 2 + 1],
                                        ptx->status.field.OWN, ptx->status.field.C, ptx->status.field.SoP, ptx->status.field.EoP,
                                        ptx->status.field.byte_offset, (u32)ptx->Data_Pointer | KSEG1, ptx->status.field.data_length);
//    len += sprintf(buf + len, "    %2d. %08X %08X\n", desc, pdesc[desc * 2], pdesc[desc * 2 + 1]);
//    len += sprintf(buf + len,  "        own %d, c %d, sop %d, eop %d\n", ptx->status.field.OWN, ptx->status.field.C, ptx->status.field.SoP, ptx->status.field.EoP);
//    len += sprintf(buf + len,  "        off %d, addr %08X, len %d\n", ptx->status.field.byte_offset, (u32)ptx->Data_Pointer | KSEG1, ptx->status.field.data_length);

    return len;
}
#endif

#if defined(DEBUG_DMA_PROC_UPSTREAM) && DEBUG_DMA_PROC_UPSTREAM
static INLINE int print_ema_desc(char *buf, int channel, int desc, int is_rx)
{
    volatile u32 *pdesc = NULL;
    volatile struct tx_descriptor *ptx;
    int len = 0;

    if ( !is_rx )
    {
        pdesc = EMA_TX_CH_DESC_BASE(channel);
    }
    if ( !pdesc )
        return 0;

    ptx = (volatile struct tx_descriptor *)&pdesc[desc * 2];
    len += sprintf(buf + len, "    %2d (%08X). %08X %08X, own %d, c %d, sop %d, eop %d, off %d, addr %08X, len %d\n",
                                        desc, (u32)(pdesc + desc * 2), pdesc[desc * 2], pdesc[desc * 2 + 1],
                                        ptx->own, ptx->c, ptx->sop, ptx->eop,
                                        ptx->byteoff, ((u32)ptx->dataptr << 2) | KSEG1, ptx->datalen);
//    len += sprintf(buf + len, "    %2d. %08X %08X\n", desc, pdesc[desc * 2], pdesc[desc * 2 + 1]);
//    len += sprintf(buf + len,  "        own %d, c %d, sop %d, eop %d\n", ptx->own, ptx->c, ptx->sop, ptx->eop);
//    len += sprintf(buf + len,  "        off %d, addr %08X, len %d\n", ptx->byteoff, ((u32)ptx->dataptr << 2) | KSEG1, ptx->datalen);

    return len;
}
#endif

#if defined(DEBUG_DUMP_SKB) && DEBUG_DUMP_SKB
static INLINE void dump_skb(struct sk_buff *skb, u32 len, char *title, int ch)
{
    int i;

    if ( !dbg_enable )
        return;

  #if 0
    if ( title[0] == 'e' )
    {
        //  TX

        if ( !skb->dst )
            printk("dump_skb: skb->dst == NULL\n");
        else if ( !skb->dst->hh )
            printk("dump_skb: skb->dst->hh == NULL\n");
//        else if ( skb->dst->hh->hh_type != htons(ETH_P_802_3) )
//            printk("dump_skb: skb->dst->hh->hh_type (%04X) != ETH_P_802_3\n", skb->dst->hh->hh_type);
        else
        {
            struct ethhdr *eth;

            eth = (struct ethhdr*)(((u8*)skb->dst->hh->hh_data) + (HH_DATA_OFF(sizeof(*eth))));

            printk("dump_skb: hh (proto %04X) %02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x\n",
                ntohs(eth->h_proto),
                eth->h_source[0],
                eth->h_source[1],
                eth->h_source[2],
                eth->h_source[3],
                eth->h_source[4],
                eth->h_source[5],
                eth->h_dest[0],
                eth->h_dest[1],
                eth->h_dest[2],
                eth->h_dest[3],
                eth->h_dest[4],
                eth->h_dest[5]);
        }
    }
    else
    {
        //  RX

        if ( skb->data[6] != 0x00 || skb->data[7] != 0x0d || skb->data[8] != 0x56
            || skb->data[9] != 0x01 || skb->data[10] != 0xfe || skb->data[11] != 0xe2 )
            return;
    }
  #endif

    if ( skb->len < len )
        len = skb->len;

    if ( ch >= 0 )
        printk("%s (ch %d)\n", title, ch);
    else
        printk("%s\n", title);
    printk("  skb->data = %08X, skb->tail = %08X, skb->len = %d\n", (u32)skb->data, (u32)skb->tail, (int)skb->len);
    for ( i = 1; i <= len; i++ )
    {
        if ( i % 16 == 1 )
            printk("  %4d:", i - 1);
        printk(" %02X", (int)(*((char*)skb->data + i - 1) & 0xFF));
        if ( i % 16 == 0 )
            printk("\n");
    }
    if ( (i - 1) % 16 != 0 )
        printk("\n");
}
#endif

#if defined(DEBUG_DUMP_FLAG_HEADER) && DEBUG_DUMP_FLAG_HEADER
static INLINE void dump_flag_header(int fwcode, struct flag_header *header, char *title, int ch)
{
    struct bridging_flag_header *bheader;

    if ( !dbg_enable )
        return;

    if ( ch >= 0 )
        printk("%s (ch %d)\n", title, ch);
    else
        printk("%s\n", title);

    switch ( fwcode )
    {
    case FWCODE_ROUTING_ACC_D2:
        printk("  rout_fwd_vld = %Xh\n", (u32)header->rout_fwd_vld);
        printk("  rout_mc_vld  = %Xh\n", (u32)header->rout_mc_vld);
        printk("  tcpudp_err   = %Xh\n", (u32)header->tcpudp_err);
        printk("  tcpudp_chk   = %Xh\n", (u32)header->tcpudp_chk);
        printk("  is_udp       = %Xh\n", (u32)header->is_udp);
        printk("  is_tcp       = %Xh\n", (u32)header->is_tcp);
        printk("  ip_offset    = %Xh\n", (u32)header->ip_offset);
        printk("  is_pppoes    = %Xh\n", (u32)header->is_pppoes);
        printk("  is_ipv4      = %Xh\n", (u32)header->is_ipv4);
        printk("  is_vlan      = %Xh\n", (u32)header->is_vlan);
        printk("  rout_index   = %Xh\n", (u32)header->rout_index);
        printk("  dest_list    = %Xh\n", (u32)header->dest_list);
        printk("  src_itf      = %Xh\n", (u32)header->src_itf);
        printk("  src_dir      = %Xh\n", (u32)header->src_dir);
        printk("  acc_done     = %Xh\n", (u32)header->acc_done);
        printk("  tcp_fin      = %Xh\n", (u32)header->tcp_fin);
        printk("  dest_chid    = %Xh\n", (u32)header->dest_chid);
        printk("  pl_byteoff   = %Xh\n", (u32)header->pl_byteoff);
        break;
    case FWCODE_BRIDGING_ACC_D3:
        bheader = (struct bridging_flag_header *)header;
        printk("  temp_dest_list = %Xh\n", (u32)bheader->temp_dest_list);
        printk("  dest_list      = %Xh\n", (u32)bheader->dest_list);
        printk("  src_itf        = %Xh\n", (u32)bheader->src_itf);
        printk("  src_dir        = %Xh\n", (u32)bheader->src_dir);
        printk("  acc_done       = %Xh\n", (u32)bheader->acc_done);
        printk("  dest_chid      = %Xh\n", (u32)header->dest_chid);
        printk("  pl_byteoff     = %Xh\n", (u32)header->pl_byteoff);
    }
}
#endif

#if defined(DEBUG_DUMP_INIT) && DEBUG_DUMP_INIT
static INLINE void dump_init(void)
{
    int i;

    if ( !dbg_enable )
        return;

    printk("Share Buffer Conf:\n");
    printk("  SB_MST_SEL(%08X) = 0x%08X\n", (u32)SB_MST_SEL, *SB_MST_SEL);

    printk("ETOP:\n");
    printk("  ETOP_MDIO_CFG        = 0x%08X\n", *ETOP_MDIO_CFG);
    printk("  ETOP_MDIO_ACC        = 0x%08X\n", *ETOP_MDIO_ACC);
    printk("  ETOP_CFG             = 0x%08X\n", *ETOP_CFG);
    printk("  ETOP_IG_VLAN_COS     = 0x%08X\n", *ETOP_IG_VLAN_COS);
    printk("  ETOP_IG_DSCP_COSx(0) = 0x%08X\n", *ETOP_IG_DSCP_COSx(0));
    printk("  ETOP_IG_DSCP_COSx(1) = 0x%08X\n", *ETOP_IG_DSCP_COSx(1));
    printk("  ETOP_IG_DSCP_COSx(2) = 0x%08X\n", *ETOP_IG_DSCP_COSx(2));
    printk("  ETOP_IG_DSCP_COSx(3) = 0x%08X\n", *ETOP_IG_DSCP_COSx(3));
    printk("  ETOP_IG_PLEN_CTRL0   = 0x%08X\n", *ETOP_IG_PLEN_CTRL0);
    printk("  ETOP_ISR             = 0x%08X\n", *ETOP_ISR);
    printk("  ETOP_IER             = 0x%08X\n", *ETOP_IER);
    printk("  ETOP_VPID            = 0x%08X\n", *ETOP_VPID);

    for ( i = 0; i < 2; i++ )
    {
        printk("ENET%d:\n", i);
        printk("  ENET_MAC_CFG(%d)      = 0x%08X\n", i, *ENET_MAC_CFG(i));
        printk("  ENETS_DBA(%d)         = 0x%08X\n", i, *ENETS_DBA(i));
        printk("  ENETS_CBA(%d)         = 0x%08X\n", i, *ENETS_CBA(i));
        printk("  ENETS_CFG(%d)         = 0x%08X\n", i, *ENETS_CFG(i));
        printk("  ENETS_PGCNT(%d)       = 0x%08X\n", i, *ENETS_PGCNT(i));
        printk("  ENETS_PKTCNT(%d)      = 0x%08X\n", i, *ENETS_PKTCNT(i));
        printk("  ENETS_BUF_CTRL(%d)    = 0x%08X\n", i, *ENETS_BUF_CTRL(i));
        printk("  ENETS_COS_CFG(%d)     = 0x%08X\n", i, *ENETS_COS_CFG(i));
        printk("  ENETS_IGDROP(%d)      = 0x%08X\n", i, *ENETS_IGDROP(i));
        printk("  ENETS_IGERR(%d)       = 0x%08X\n", i, *ENETS_IGERR(i));
        printk("  ENETS_MAC_DA0(%d)     = 0x%08X\n", i, *ENETS_MAC_DA0(i));
        printk("  ENETS_MAC_DA1(%d)     = 0x%08X\n", i, *ENETS_MAC_DA1(i));
        printk("  ENETF_DBA(%d)         = 0x%08X\n", i, *ENETF_DBA(i));
        printk("  ENETF_CBA(%d)         = 0x%08X\n", i, *ENETF_CBA(i));
        printk("  ENETF_CFG(%d)         = 0x%08X\n", i, *ENETF_CFG(i));
        printk("  ENETF_PGCNT(%d)       = 0x%08X\n", i, *ENETF_PGCNT(i));
        printk("  ENETF_PKTCNT(%d)      = 0x%08X\n", i, *ENETF_PKTCNT(i));
        printk("  ENETF_HFCTRL(%d)      = 0x%08X\n", i, *ENETF_HFCTRL(i));
        printk("  ENETF_TXCTRL(%d)      = 0x%08X\n", i, *ENETF_TXCTRL(i));
        printk("  ENETF_VLCOS0(%d)      = 0x%08X\n", i, *ENETF_VLCOS0(i));
        printk("  ENETF_VLCOS1(%d)      = 0x%08X\n", i, *ENETF_VLCOS1(i));
        printk("  ENETF_VLCOS2(%d)      = 0x%08X\n", i, *ENETF_VLCOS2(i));
        printk("  ENETF_VLCOS3(%d)      = 0x%08X\n", i, *ENETF_VLCOS3(i));
        printk("  ENETF_EGCOL(%d)       = 0x%08X\n", i, *ENETF_EGCOL(i));
        printk("  ENETF_EGDROP(%d)      = 0x%08X\n", i, *ENETF_EGDROP(i));
    }

    printk("DPLUS:\n");
    printk("  DPLUS_TXDB           = 0x%08X\n", *DPLUS_TXDB);
    printk("  DPLUS_TXCB           = 0x%08X\n", *DPLUS_TXCB);
    printk("  DPLUS_TXCFG          = 0x%08X\n", *DPLUS_TXCFG);
    printk("  DPLUS_TXPGCNT        = 0x%08X\n", *DPLUS_TXPGCNT);
    printk("  DPLUS_RXDB           = 0x%08X\n", *DPLUS_RXDB);
    printk("  DPLUS_RXCB           = 0x%08X\n", *DPLUS_RXCB);
    printk("  DPLUS_RXCFG          = 0x%08X\n", *DPLUS_RXCFG);
    printk("  DPLUS_RXPGCNT        = 0x%08X\n", *DPLUS_RXPGCNT);

    printk("Communication:\n");
    printk("  FW_VER_ID(%08X)  = 0x%08X\n", (u32)FW_VER_ID, *(u32*)FW_VER_ID);
    printk("  General Configuration(%08X):\n", (u32)ETX1_DMACH_ON);
    printk("    ETX1_DMACH_ON      = 0x%08X\n", *(u32*)ETX1_DMACH_ON);
    printk("    ETH_PORTS_CFG      = 0x%08X\n", *(u32*)ETH_PORTS_CFG);
    printk("    LAN_ROUT_TBL_CFG   = 0x%08X\n", *(u32*)LAN_ROUT_TBL_CFG);
    printk("    WAN_ROUT_TBL_CFG   = 0x%08X\n", *(u32*)WAN_ROUT_TBL_CFG);
    printk("    GEN_MODE_CFG       = 0x%08X\n", *(u32*)GEN_MODE_CFG);
    printk("  VLAN_CFG_TBL(%08X):\n", (u32)VLAN_CFG_TBL(0));
    for ( i = 0; i < 8; i++ )
        printk("    Entry(%d)           = 0x%08X\n", i, *(u32*)VLAN_CFG_TBL(i));
    printk("  PPPOE_CFG_TBL(%08X):\n", (u32)PPPOE_CFG_TBL(0));
    for ( i = 0; i < 8; i++ )
        printk("    Entry(%d)           = 0x%08X\n", i, *(u32*)PPPOE_CFG_TBL(i));
    printk("  MTU_CFG_TBL(%08X):\n", (u32)MTU_CFG_TBL(0));
    for ( i = 0; i < 8; i++ )
        printk("    Entry(%d)           = 0x%08X\n", i, *(u32*)MTU_CFG_TBL(i));
    printk("  ROUT_FWD_HIT_STAT_TBL(%08X):\n", (u32)ROUT_FWD_HIT_STAT_TBL(0));
    for ( i = 0; i < 8; i++ )
        printk("    DWORD(%d)           = 0x%08X\n", i, *(u32*)ROUT_FWD_HIT_STAT_TBL(i));
    printk("  WAN_ROUT_MAC_CFG_TBL(%08X):\n", (u32)WAN_ROUT_MAC_CFG_TBL(0));
    for ( i = 0; i < 8; i++ )
    {
        printk("    Entry(%d)           = 0x%08X\n", i, *(u32*)WAN_ROUT_MAC_CFG_TBL(i));
        printk("                       = 0x%08X\n", *((u32*)WAN_ROUT_MAC_CFG_TBL(i) + 1));
    }
    printk("  WAN_ROUT_FORWARD_COMPARE_TBL(%08X) & WAN_ROUT_FORWARD_ACTION_TBL(%08X):\n", (u32)WAN_ROUT_FORWARD_COMPARE_TBL(0), (u32)WAN_ROUT_FORWARD_ACTION_TBL(0));
    for ( i = 0; i < 5; i++ )
    {
        int k;

        switch ( i )
        {
        case 2:  k = 128; break;
        case 3:  k = 192; break;
        default: k = i;
        }

        printk("    Entry(%d)\n", k);
        printk("      Compare          = 0x%08X\n", *(u32*)WAN_ROUT_FORWARD_COMPARE_TBL(k));
        printk("                       = 0x%08X\n", *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(k) + 1));
        printk("                       = 0x%08X\n", *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(k) + 2));
        printk("                       = 0x%08X\n", *((u32*)WAN_ROUT_FORWARD_COMPARE_TBL(k) + 3));
        printk("      Action           = 0x%08X\n", *(u32*)WAN_ROUT_FORWARD_ACTION_TBL(k));
        printk("                       = 0x%08X\n", *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(k) + 1));
        printk("                       = 0x%08X\n", *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(k) + 2));
        printk("                       = 0x%08X\n", *((u32*)WAN_ROUT_FORWARD_ACTION_TBL(k) + 3));
    }

    printk("  LAN_ROUT_FORWARD_COMPARE_TBL(%08X) & LAN_ROUT_FORWARD_ACTION_TBL(%08X):\n", (u32)LAN_ROUT_FORWARD_COMPARE_TBL(0), (u32)LAN_ROUT_FORWARD_ACTION_TBL(0));
    for ( i = 0; i < 5; i++ )
    {
        int k;

        switch ( i )
        {
        case 2:  k = 128; break;
        case 3:  k = 192; break;
        default: k = i;
        }

        printk("    Entry(%d)\n", k);
        printk("      Compare          = 0x%08X\n", *(u32*)LAN_ROUT_FORWARD_COMPARE_TBL(k));
        printk("                       = 0x%08X\n", *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(k) + 1));
        printk("                       = 0x%08X\n", *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(k) + 2));
        printk("                       = 0x%08X\n", *((u32*)LAN_ROUT_FORWARD_COMPARE_TBL(k) + 3));
        printk("      Action           = 0x%08X\n", *(u32*)LAN_ROUT_FORWARD_ACTION_TBL(k));
        printk("                       = 0x%08X\n", *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(k) + 1));
        printk("                       = 0x%08X\n", *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(k) + 2));
        printk("                       = 0x%08X\n", *((u32*)LAN_ROUT_FORWARD_ACTION_TBL(k) + 3));
    }
}
#endif

#if 0
static void force_dump_skb(struct sk_buff *skb, u32 len, char *title, int ch)
{
    int i;

    if ( skb->len < len )
        len = skb->len;

    if ( ch >= 0 )
        printk("%s (ch %d)\n", title, ch);
    else
        printk("%s\n", title);
    printk("  skb->data = %08X, skb->tail = %08X, skb->len = %d\n", (u32)skb->data, (u32)skb->tail, (int)skb->len);
    for ( i = 1; i <= len; i++ )
    {
        if ( i % 16 == 1 )
            printk("  %4d:", i - 1);
        printk(" %02X", (int)(*((char*)skb->data + i - 1) & 0xFF));
        if ( i % 16 == 0 )
            printk("\n");
    }
    if ( (i - 1) % 16 != 0 )
        printk("\n");
}

static void force_dump_flag_header(struct flag_header *header, char *title, int ch)
{
    if ( ch >= 0 )
        printk("%s (ch %d)\n", title, ch);
    else
        printk("%s\n", title);
    printk("  rout_fwd_vld = %Xh\n", (u32)header->rout_fwd_vld);
    printk("  rout_mc_vld  = %Xh\n", (u32)header->rout_mc_vld);
    printk("  tcpudp_err   = %Xh\n", (u32)header->tcpudp_err);
    printk("  tcpudp_chk   = %Xh\n", (u32)header->tcpudp_chk);
    printk("  is_udp       = %Xh\n", (u32)header->is_udp);
    printk("  is_tcp       = %Xh\n", (u32)header->is_tcp);
    printk("  ip_offset    = %Xh\n", (u32)header->ip_offset);
    printk("  is_pppoes    = %Xh\n", (u32)header->is_pppoes);
    printk("  is_ipv4      = %Xh\n", (u32)header->is_ipv4);
    printk("  is_vlan      = %Xh\n", (u32)header->is_vlan);
    printk("  rout_index   = %Xh\n", (u32)header->rout_index);
    printk("  dest_list    = %Xh\n", (u32)header->dest_list);
    printk("  src_itf      = %Xh\n", (u32)header->src_itf);
    printk("  src_dir      = %Xh\n", (u32)header->src_dir);
    printk("  acc_done     = %Xh\n", (u32)header->acc_done);
    printk("  tcp_fin      = %Xh\n", (u32)header->tcp_fin);
    printk("  dest_chid    = %Xh\n", (u32)header->dest_chid);
    printk("  pl_byteoff   = %Xh\n", (u32)header->pl_byteoff);
}
#endif



/*
 * ####################################
 *           Global Function
 * ####################################
 */

int danube_eth_download_firmware(int fwcode)
{
    int ret = 0;
    int f_eth_opened;
    int i;

    if ( g_fwcode == fwcode )
        return 0;

    f_eth_opened = f_dma_opened;
    if ( (f_eth_opened & 0x01) )
        eth_stop(&eth_net_dev[0]);
    if ( (f_eth_opened & 0x02) )
        eth_stop(&eth_net_dev[1]);
    for ( i = 0; i < 0x1000; i++ );

    //  clear
    pp32_stop();

    free_dma();

    if ( eth1_dev_tx_irq )  //  in case disable twice
        disable_irq(PPE_MAILBOX_IGU1_INT);

    clear_local_variables();

    //  fwcode
    g_fwcode = fwcode;

    //  init
    ret |= init_local_variables();

    init_etop(g_fwcode, MII0_MODE_SETUP, MII1_MODE_SETUP);

    init_ema();

    init_mailbox();
    eth1_dev_tx_irq = *MBOX_IGU1_IER &  (((1 << ETH1_TX_TOTAL_CHANNEL_USED) - 1) << 16);

    clear_share_buffer();

    clear_cdm();

    init_communication_data_structures(g_fwcode);

    ret |= alloc_dma();
    eth1_dev_tx_irq |= dma_ch2_tx_not_run;

    ret |= pp32_start(g_fwcode);

    //  careful, PPE firmware may set some registers, recover them here
    *MBOX_IGU1_IER = eth1_dev_tx_irq;

    //  everything is ready, enable hardware to take package
    start_etop();

    for ( i = 0; i < 0x1000; i++ );
    if ( (f_eth_opened & 0x01) )
        eth_open(&eth_net_dev[0]);
    if ( (f_eth_opened & 0x02) )
        eth_open(&eth_net_dev[1]);
    for ( i = 0; i < 0x1000; i++ );

    //  This function will call handler, so it must be put in the end.
    enable_irq(PPE_MAILBOX_IGU1_INT);

    if ( !ret )
        printk("eth: downloading firmware succeeded (firmware version %d.%d.%d.%d.%d.%d)\n", (int)FW_VER_ID->family, (int)FW_VER_ID->fwtype, (int)FW_VER_ID->interface, (int)FW_VER_ID->fwmode, (int)FW_VER_ID->major, (int)FW_VER_ID->minor);
    else
        printk("eth: downloading firmware failed\n");

//    printk("dma_ch2_tx_not_run = %d, MBOX_IGU1_IER (%08x) = %08X\n", dma_ch2_tx_not_run, (u32)MBOX_IGU1_IER, *MBOX_IGU1_IER);

    return ret;
}



/*
 * ####################################
 *           Init/Cleanup API
 * ####################################
 */

int __init danube_eth_init(void)
{
    int ret;
    int i;

    printk("Loading ETH driver... ");

    ret = init_local_variables();
    if ( ret )
        goto INIT_LOCAL_VARIABLES_FAIL;

    init_pmu();

    init_gpio(MII1_MODE_SETUP);

    init_etop(g_fwcode, MII0_MODE_SETUP, MII1_MODE_SETUP);

    init_ema();

    init_mailbox();
    eth1_dev_tx_irq = *MBOX_IGU1_IER &  (((1 << ETH1_TX_TOTAL_CHANNEL_USED) - 1) << 16);

    clear_share_buffer();

    clear_cdm();

    init_communication_data_structures(g_fwcode);

    //  create device
    for ( i = 0; i < 2; i++ )
    {
        ret = register_netdev(eth_net_dev + i);
        if ( ret )
            goto REGISTER_NETDEV_FAIL;
    }

    ret = request_irq(PPE_MAILBOX_IGU1_INT, mailbox_irq_handler, SA_INTERRUPT, "eth1_ema_isr", NULL);
    if ( ret )
        goto REQUEST_IRQ_PPE_MAILBOX_IGU1_INT_FAIL;

    ret = alloc_dma();
    if ( ret )
        goto ALLOC_DMA_FAIL;
    eth1_dev_tx_irq |= dma_ch2_tx_not_run;

    ret = pp32_start(g_fwcode);
    if ( ret )
        goto PP32_START_FAIL;

    //  careful, PPE firmware may set some registers, recover them here
    *MBOX_IGU1_IER = eth1_dev_tx_irq;

    //  create proc file
    proc_file_create();

#if defined(DEBUG_DUMP_INIT) && DEBUG_DUMP_INIT
    dump_init();
#endif

    //  board specific init
    board_init();

    //  everything is ready, enable hardware to take package
    start_etop();

    printk("init succeeded (firmware version %d.%d.%d.%d.%d.%d)\n", (int)FW_VER_ID->family, (int)FW_VER_ID->fwtype, (int)FW_VER_ID->interface, (int)FW_VER_ID->fwmode, (int)FW_VER_ID->major, (int)FW_VER_ID->minor);
    return 0;

PP32_START_FAIL:
    free_dma();
ALLOC_DMA_FAIL:
    free_irq(PPE_MAILBOX_IGU1_INT, NULL);
REQUEST_IRQ_PPE_MAILBOX_IGU1_INT_FAIL:
REGISTER_NETDEV_FAIL:
    while ( i-- )
        unregister_netdev(eth_net_dev + i);
    clear_local_variables();
INIT_LOCAL_VARIABLES_FAIL:
    printk("init failed!\n");
    return ret;
}

void __exit danube_eth_exit(void)
{
    int i;

    proc_file_delete();

    pp32_stop();

    free_dma();

    free_irq(PPE_MAILBOX_IGU1_INT, NULL);

    for ( i = 0; i < 2; i++ )
        unregister_netdev(eth_net_dev + i);

    clear_local_variables();
}

#ifdef CONFIG_DANUBE_PPA
static int __init danube_eth0addr_setup(char *line)
{
    ethaddr_setup(0, line);

    return 0;
}

static int __init danube_eth1addr_setup(char *line)
{
    ethaddr_setup(1, line);

    return 0;
}
#endif

EXPORT_SYMBOL(danube_eth_download_firmware);

module_init(danube_eth_init);
module_exit(danube_eth_exit);
#ifdef CONFIG_DANUBE_PPA
  __setup("ethaddr=", danube_eth0addr_setup);
  __setup("eth1addr=", danube_eth1addr_setup);
#endif
