 /************************************************************************
 *
 * Copyright (c) 2006
 * Infineon Technologies AG
 * St. Martin Strasse 53; 81669 Muenchen; Germany
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 ************************************************************************/

/* Group definitions for Doxygen */
/** \addtogroup API API-Functions */
/** \addtogroup Internal Internally used functions */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/sem.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/netdevice.h>		//prochao+, 01-21-2007
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <asm/danube/ifx_types.h>
#include <asm/danube/danube.h>
//procaho+
#include <linux/spinlock.h>		//prochao+!
#include <asm/danube/mps_tpe_buffer.h>
//prochao-
#include <asm/danube/mps_dualcore.h>	//prochao+, for TP-E dual-core
//#include <asm/danube/danube_gptu.h>	//prochao+, commented out
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/smplock.h>				//prochao+, for kernel thread
#include <asm/danube/irq.h>
#include "mps_tpe_device.h"				//prochao+
#if	(!defined(CONFIG_DANUBE_CORE1) && defined(MPS_USE_HOUSEKEEPING_TIMER))	//only for the CPU0
#include <asm/danube/mps_srv.h>
#endif
// prochao+, 11/16/2006, uses the kernel thread to handle the callback functions
#include "mps_tpe_kthread.h"
// prochao-

#ifdef CONFIG_DEBUG_MINI_BOOT
#define CONFIG_DANUBE_USE_IKOS
#endif

/******************************************************************************
 * Function declarations
 ******************************************************************************/
#define	_PRO_DEBUGGING_		1	//activate some debugging print msg
#undef	_PRO_DEBUGGING_

//prochao+, 10/28/2006
#define	_MPS_MIN_MARGIN_	2	// at least 2 ub32 space margin for FIFO to handle variable-length contents
#define	_MPS_MAX_MARGIN_	4	//
//prochao-

#ifdef	_PRO_DEBUGGING_
#define DEBUG_FNENTRY()		printk(KERN_INFO "(MPS) [%s] Entering function ...\n", __FUNCTION__)
#define DEBUG_FNEXIT()		printk(KERN_INFO "(MPS) [%s] Exiting function at line#[%d] ...\n", __FUNCTION__, __LINE__)
#define DPRINTK(fmt, args...)	printk(KERN_INFO "(MPS) "fmt, ##args)
#else
#define	DEBUG_FNENTRY()
#define	DEBUG_FNEXIT()
#define DPRINTK(fmt, args...)	
#endif

//#define	MPS_USE_KT		//prochao+-

/******************************************************************************
 * Global variables
 ******************************************************************************/
extern mps_comm_dev ifx_mps_dev;
extern u32 *danube_cp1_base;

u32	*mps_tpe_cpu1_entry_base = 0;	//where keeping the trigger point

mps_comm_dev *pMPSDev = &ifx_mps_dev;
//int ifx_mpsdrv_initialising = 1;

char wifi_channel_int_name[MPSDRV_NUM_WIFI_CHANNEL][15];

//prochao+
MPS_RXDATA_LIST_T	mps_RxMsgQ,
					mps_RxCmdQ;
//prochao-

//prochao+, 11/11/2006, adds the queue for buffering the rcv'ed datamsg from the limited fifo Q
MPS_DATAMSG_LIST	mps_rcvDataMsgQ,	//list of rcv'ed data msg
					mps_rcvCmdMsgQ;		//list of rcv'ed cmd msg
//
// moved from the mps_tpe_buffer.c file
#ifndef	CONFIG_DANUBE_CORE1		//for the CPU0
static struct timer_list	mps_timer; /* Timer for periodic checks */
#endif
//prochao-

#ifdef	CONFIG_DANUBE_CORE1
extern MPS_BUFF_POOL_HEAD	**mps_buff_pool_array;
#endif

//eventually, the CONFIG_MPS_DUALCORE_11N_DRIVER macro should be created via the kernel's menuconfig settings
//#define CONFIG_MPS_DUALCORE_11N_DRIVER			//prochao%, 12/26/2006, ported from Jeffrey's approval for 11n driver

#ifdef CONFIG_MPS_DUALCORE_11N_DRIVER
#define INTERRUPT_MITIGATION
#define INTERRUPT_MITIGATION_EWMA
#else
#undef INTERRUPT_MITIGATION
#undef INTERRUPT_MITIGATION_EWMA
#endif

//#define INTERRUPT_MITIGATION
//#undef INTERRUPT_MITIGATION
#ifdef INTERRUPT_MITIGATION
static struct timer_list        intr_coalesce_timer; /* Timer for periodic checks */
#define PKT_AGGREGATE_THRESH    3
static atomic_t pkt_count;
static void intr_coalesce_timer_handler(unsigned long data);
extern void flush_bklog_q();
#endif

//#define INTERRUPT_MITIGATION_EWMA
//#undef INTERRUPT_MITIGATION_EWMA
#ifdef INTERRUPT_MITIGATION_EWMA
#include <linux/time.h>
unsigned int min_avg_ipt = 80;
unsigned int max_avg_ipt = 1500;
unsigned int tolerable_delay = 300;
//unsigned int last_interrupt_time;
//atomic_t last_interrupt_time;
volatile unsigned int last_interrupt_time;
unsigned int last_pkt_time;
extern unsigned long cycles_per_jiffy;
#define USECS_PER_JIFFY         (1000000/HZ)
unsigned int avg_ipt;
static atomic_t bklog;

#define SCALE 5
#define MAX_DEFERRED_PKTS 3
static int generate_interrupt();
#ifdef KDEBUG
unsigned count_interrupts = 0;
unsigned count_mitigated = 0;
extern unsigned mean_pkt_len;
extern unsigned count_bklog_pkts;
extern unsigned count_fail_pkts;
extern unsigned count_max_bklog_q;
#endif
#endif

#define DIV_OPTIMIZATION

/******************************************************************************
 * FIFO Managment
 ******************************************************************************/
//prochao+, 10/31/2006
void ifx_mps_cpu1_do_DnCmd_tasklet(unsigned long);
void ifx_mps_cpu1_do_DnData_tasklet(unsigned long);
void ifx_mps_cpu0_do_UpCmd_tasklet(unsigned long);
void ifx_mps_cpu0_do_UpData_tasklet(unsigned long);


#ifdef	CONFIG_DANUBE_CORE1		//for CPU1
DECLARE_TASKLET( CPU1_Cmd_Tasklet, ifx_mps_cpu1_do_DnCmd_tasklet, 0);
DECLARE_TASKLET( CPU1_Data_Tasklet, ifx_mps_cpu1_do_DnData_tasklet, 0);
#else	//for CPU0
DECLARE_TASKLET( CPU0_Cmd_Tasklet, ifx_mps_cpu0_do_UpCmd_tasklet, 0);
DECLARE_TASKLET( CPU0_Data_Tasklet, ifx_mps_cpu0_do_UpData_tasklet, 0);
#endif
//prochao-

/**
 * Clear FIFO
 * This function clears the FIFO by resetting the pointers. The data itself is
 * not cleared.
 *
 * \param   fifo    Pointer to FIFO structure
 * \ingroup Internal
 */
void ifx_mpsdrv_fifo_clear(mps_fifo *fifo)
{
	*fifo->pread_off  = 0;
	*fifo->pwrite_off = 0;
    return;
}

/**
 * Check FIFO for being not empty
 * This function checks whether the referenced FIFO contains at least
 * one unread data byte.
 *
 * \param   fifo     Pointer to FIFO structure
 * \return  1        TRUE if data to be read is available in FIFO,
 * \return  0        FALSE if FIFO is empty.
 * \ingroup Internal
 */
static bool_t ifx_mpsdrv_fifo_not_empty(mps_fifo *fifo)
{
	mps_message     	*pMsg;

	// check if current read-index is available for being read
	pMsg = (mps_message *) (fifo->pstart + *fifo->pread_off);
	// check if the current read-index entry is used for being wrap-around?
	if (pMsg->msg_next_wrapped)	// if (pMsg->msg_next_wrapped == 1)
	{
		*fifo->pread_off = 0;		//wrapped to the begin
		pMsg = (mps_message *) fifo->pstart;
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "\t%d: %s(%X): wrapped!\n", __LINE__, __FUNCTION__, (u32) fifo);
#endif
	}
	if (pMsg->msg_ready == 0)
		return FALSE ;
	else
		return TRUE;
}

/**
 * Check FIFO for free memory
 * This function returns the amount of free bytes in FIFO.
 *
 * \param   fifo     Pointer to FIFO structure
 * \return  0        The FIFO is full,
 * \return  count    The number of available bytes
 * \ingroup Internal
 */
static u32 ifx_mpsdrv_fifo_mem_available(mps_fifo *fifo)
{
    u32 retval;

	if (*fifo->pread_off > *fifo->pwrite_off)
	{
#ifdef DIV_OPTIMIZATION
		retval = (*fifo->pread_off - *fifo->pwrite_off) << 2;
#else
		retval = 4 * (*fifo->pread_off - *fifo->pwrite_off);
#endif
	}
	else
	{	//write >= read
#ifdef DIV_OPTIMIZATION
		retval = fifo->size - ((*fifo->pwrite_off - *fifo->pread_off) << 2);
#else
		retval = fifo->size - 4 * (*fifo->pwrite_off - *fifo->pread_off);
#endif
	}
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\t%d: %s(%Xh)=%d, size=%d, wrt=%d, rd=%d!\n",
		   __LINE__, __FUNCTION__, (u32) fifo, retval, fifo->size, *fifo->pwrite_off, *fifo->pread_off);
#endif
    return (retval);
}

/**
 * Check FIFO for requested amount of memory
 * This function checks whether the requested FIFO is capable to store
 * the requested amount of data bytes.
 * The selected Fifo should be a downstream direction Fifo.
 *
 * \param   fifo     Pointer to mailbox structure to be checked
 * \param   bytes    Requested data bytes
 * \return  1        TRUE if space is available in FIFO,
 * \return  0        FALSE if not enough space in FIFO.
 * \ingroup Internal
 */
bool_t ifx_mpsdrv_fifo_mem_request(mps_fifo *fifo, u32 bytes)
{
    u32 bytes_avail;

	bytes_avail = ifx_mpsdrv_fifo_mem_available(fifo);
    if (bytes_avail >= bytes)	//(bytes_avail > bytes)??
        return TRUE;
    else
        return FALSE ;
}

/**
 * Update FIFO read pointer
 * This function updates the position of the referenced FIFO. In case of
 * reaching the FIFO's end the pointer is set to the start position.
 *
 * \param   fifo      Pointer to FIFO structure
 * \param   increment Increment for read index (in 4-byte, i.e. dwords)
 * \ingroup Internal
 */
void ifx_mpsdrv_fifo_read_ptr_inc(mps_fifo *fifo, u32 increment)
{
	mps_message	*pMsg;
    mps_message_alias   *msg2;

	if (increment > fifo->size )
	{
		printk("%d: %s() invalid offset %d\n", __LINE__, __FUNCTION__, increment);
		return;
	}
	pMsg = (mps_message *) (fifo->pstart + *fifo->pread_off);
    msg2 = (mps_message_alias *) pMsg;
//	pMsg->msg_ready = 0;	//release to be available for write
    msg2->val1 &= ~0x8000;
	//increment is in 4 bytes (32-bit)
	*fifo->pread_off += increment;		//move forward

    return;
}

/**
 * Read MPS message from FIFO
 * This function reads an MPS message pointer from the referenced FIFO.
 *
 * \param   mbx            Pointer to FIFO structure
 * \return  *msg           OK, returned pointer to the MPS message
 * \return  NULL           None message
 * \ingroup Internal
 */
mps_message *ifx_mpsdrv_fifo_read(mps_fifo *fifo)
{
	mps_message	*pMsg;
//	mps_message_alias *msg2;

	pMsg = (mps_message *) (fifo->pstart + *fifo->pread_off);
//	msg2 = (mps_message_alias *) pMsg;	//current written index
	// check if the current read-index entry is used for being wrap-around?
	if (pMsg->msg_next_wrapped)	// if (pMsg->msg_next_wrapped == 1)
	{
		*fifo->pread_off = 0;		//wrapped to the begin
		pMsg = (mps_message *) fifo->pstart;
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "\t%d: %s(): wrapped!\n", __LINE__, __FUNCTION__);
#endif
	}
	// check if current read-index is available for read
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\t%d: %s(%X): ready=%d @rd=%d\n", __LINE__, __FUNCTION__, (u32) fifo, pMsg->msg_ready, *fifo->pread_off);
#endif
//	pMsg = (mps_message *) read_address;
	if (pMsg->msg_ready == 0)
		return NULL;
	//
	return pMsg;
}

/**
 * Write data word to FIFO
 * This function writes a data word (32bit) to the referenced FIFO. The word is
 * written to the position defined by the current write pointer index and the
 * offset being passed.
 *
 * \param   mbx            Pointer to FIFO structure
 * \param   data           Data word to be written
 * \param   length         Number of bytes to be added to write pointer position
 * \return  0              OK, word written
 * \return  -1             Invalid offset.
 * \ingroup Internal
 */
s32 ifx_mpsdrv_fifo_write(mps_fifo *fifo, mps_message *data, u32 length)	//offset)
{
    /* calculate write position */
	mps_message	*pWrtMsg;	//, *pNextMsg;
    u32		*pNew, *pRead;
	u32		i, msg_4bytes;
    mps_message_alias *msg2;
	mps_data_message *pMsg;	//, *pMsgW;
//	int		tmp_dbg = 0;

	if (length > fifo->size)
	{	//impossible to fill-in
		printk(KERN_DEBUG "%d: %s(%Xh) invalid length %d!\n", __LINE__, __FUNCTION__, (u32) fifo, length);
		return ERROR;
	}
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s(fifo=%Xh, dptr=%Xh, len=%d)\n", __LINE__, __FUNCTION__, (u32) fifo, (u32) data, length);
#endif
	//point to the to-be-written entry
	pWrtMsg = (mps_message *) (fifo->pstart + *fifo->pwrite_off);
	// check if this to-be-written entry is available for being written into
	if (pWrtMsg->msg_ready == 1)
	{
#ifdef	_PRO_DEBUGGING_
		printk(KERN_DEBUG "%d: %s(): full!\n", __LINE__, __FUNCTION__);
#endif
		return ERROR;
	}
	// note that the offset is in unit of bytes
#ifdef DIV_OPTIMIZATION
	msg_4bytes = (length + 3 ) >> 2;
#else
	msg_4bytes = length / 4;
	if (length % 4)
		msg_4bytes += 1;
#endif
	// this is available for take, check if this also has enough space to take, i.e. not exceed current read-index
	if (*fifo->pwrite_off < *fifo->pread_off)
	{
		if (*fifo->pwrite_off + msg_4bytes + _MPS_MIN_MARGIN_ > *fifo->pread_off)
		{	//no enough space for this one
#ifdef	_PRO_DEBUGGING_
			printk(KERN_DEBUG "%d: %s(): not enough!\n", __LINE__, __FUNCTION__);
#endif
			return ERROR;
		}
	}
	else
	{	// pwrite_off >= pread_off
		if (*fifo->pwrite_off + msg_4bytes + _MPS_MIN_MARGIN_ >= (fifo->size/4))
		{	//exceed the end, no enough space for this one plus the Min. margin, mark it and wrap to begin
            msg2 = (mps_message_alias *) pWrtMsg;	//current written index
			//pWrtMsg->msg_next_wrapped = 1;
			//prochao+, 10/27/2006, wrapped without available entry for read
            msg2->val1 = 0x4000;	//&= ~0x4000
			//prochao-
#ifdef	_PRO_DEBUGGING_
			printk(KERN_INFO "\t%d: %s(%Xh): wrapped!\n", __LINE__, __FUNCTION__, (u32) fifo);
#endif
			*fifo->pwrite_off = 0;	//wrap-around to the begin
			//wrapped to begin
			pWrtMsg = (mps_message *) fifo->pstart;
			if (pWrtMsg->msg_ready == 1)	//occupied by read-index
			{
#ifdef	_PRO_DEBUGGING_
				printk(KERN_DEBUG "%d: %s(): full!\n", __LINE__, __FUNCTION__);
#endif
				return ERROR;
			}
			//
			if (*fifo->pwrite_off + msg_4bytes + _MPS_MIN_MARGIN_ > *fifo->pread_off)
			{	//still no enough space for this one plus the min margin
#ifdef	_PRO_DEBUGGING_
				printk(KERN_DEBUG "%d: %s(): no enough!\n", __LINE__, __FUNCTION__);
#endif
				return ERROR;
			}
		}
	}
	pMsg = (mps_data_message *) pWrtMsg;
	//write into the available written index
	pNew = (u32 *) pWrtMsg;
	pRead = (u32 *) data;
	for (i = 0; i < msg_4bytes; i++)
	{
		*pNew = *pRead;
		pNew += 1;
		pRead += 1;
	}
    msg2 = (mps_message_alias *) pWrtMsg;
	//pWrtMsg->msg_ready = 1;		//marked as ready
    msg2->val1 |= 0x8000;
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\t%d: %s(%Xh): %d written @wrt=%d!\n", __LINE__, __FUNCTION__, (u32) fifo, msg_4bytes, *fifo->pwrite_off);
#endif
    //update to the next written index
	*fifo->pwrite_off += msg_4bytes;
	//post-process the next entry header for being wrapped or not, within the min margin
	msg2 = (mps_message_alias *) (fifo->pstart + *fifo->pwrite_off);
	msg2->val0 = msg2->val1 = 0;		//empty one

	return OK;
}

/**
 * MPS Structure Reset
 * This function resets the global structures into inital state
 *
 * \param   pDev     Pointer to MPS device structure
 * \return  0        OK, if initialization was successful
 * \return  -1       ERROR, allocation or semaphore access problem
 * \ingroup Internal
 */
u32 ifx_mpsdrv_reset_structures(mps_comm_dev *pDev)
{
    int i;

    for (i = 0; i < MPSDRV_NUM_WIFI_CHANNEL; i++)
    {
        ifx_mpsdrv_fifo_clear(&(pDev->wifi_mbx[i].dwstrm_fifo));
        ifx_mpsdrv_fifo_clear(&(pDev->wifi_mbx[i].upstrm_fifo));
        if (sem_getcount(pDev->wifi_mbx[i].wakeup_pending) == 0)
            up(pDev->wifi_mbx[i].wakeup_pending);
#ifdef CONFIG_PROC_FS
        pDev->wifi_mbx[i].TxnumIRQs = 0;
        pDev->wifi_mbx[i].RxnumIRQs = 0;
        pDev->wifi_mbx[i].TxnumBytes = 0;
        pDev->wifi_mbx[i].RxnumBytes = 0;
        pDev->wifi_mbx[i].upstrm_fifo.min_space = pDev->wifi_mbx[i].upstrm_fifo.size;
        pDev->wifi_mbx[i].dwstrm_fifo.min_space = pDev->wifi_mbx[i].dwstrm_fifo.size;
#endif
    }
    ifx_mpsdrv_fifo_clear(&(pDev->command_mbx.dwstrm_fifo));
    ifx_mpsdrv_fifo_clear(&(pDev->command_mbx.upstrm_fifo));
#ifdef CONFIG_PROC_FS
    pDev->command_mbx.TxnumIRQs = 0;
    pDev->command_mbx.RxnumIRQs = 0;
    pDev->command_mbx.TxnumBytes = 0;
    pDev->command_mbx.RxnumBytes = 0;
    pDev->command_mbx.upstrm_fifo.min_space = pDev->command_mbx.upstrm_fifo.size;
    pDev->command_mbx.dwstrm_fifo.min_space = pDev->command_mbx.dwstrm_fifo.size;
#endif
    if (sem_getcount(pDev->wakeup_pending) == 0)
        up(pDev->wakeup_pending);
//  down_trylock(pDev->provide_buffer);
	for (i = 0; i < MPSDRV_TOTAL_SEQ_NUMBERS; i++)
		pDev->callback_function[i] = (MPSFNPTR_T) NULL;

	return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
/**
 * Reset CPU1
 * This function causes a reset of CPU1 by clearing the CPU0 boot ready bit
 * in the reset request register RCU_RST_REQ.
 * It does not change the boot configuration registers for CPU0 or CPU1.
 *
 * \return  0        OK, cannot fail
 * \ingroup Internal
 */
s32 ifx_mpsdrv_reset(void)
{
	//should only called in the CPU0
    if (current_cpu_data.ebase_cpu_num & 0x03FF)
        return ERROR;
//    printk(KERN_INFO "DANUBE_RCU_RST_REQ = 0x%08x\n", *DANUBE_RCU_RST_REQ);
    *DANUBE_RCU_RST_REQ |= (DANUBE_RCU_RST_REQ_CPU1);
    wmb();
    //*DANUBE_MPS_VCPU_FW_AD = 0x0;
//    printk(KERN_INFO "%s DANUBE_RCU_RST_REQ(0x%08x) = 0x%08x\n",__FUNCTION__, DANUBE_RCU_RST_REQ, *DANUBE_RCU_RST_REQ);

    /* reset driver */
    ifx_mpsdrv_reset_structures(pMPSDev);

    return (OK);
}

s32 ifx_mpsdrv_release(void)
{
    //should only called in the CPU0
    if (current_cpu_data.ebase_cpu_num & 0x03FF)
        return ERROR;
    //
    *DANUBE_RCU_RST_REQ |= 0x20000000;
    *DANUBE_RCU_RST_REQ &= (~8);
    wmb();
//    printk(KERN_INFO "%s DANUBE_RCU_RST_REQ(0x%08x) = 0x%08x\n",__FUNCTION__, DANUBE_RCU_RST_REQ, *DANUBE_RCU_RST_REQ);

    return (OK);
}


/******************************************************************************
 * TwinPass-E Specific Routines
 ******************************************************************************/
/**
 * Firmware download to 2nd CPU
 * This function performs a firmware download to the coprocessor.
 *
 * \param   pMBDev    Pointer to mailbox device structure
 * \param   pFWDwnld  Pointer to firmware structure
 * \return  0         OK, firmware ready
 * \return  -1        ERROR, firmware not downloaded.
 * \ingroup Internal
 */
//s32 ifx_mpsdrv_download_firmware(mps_mbx_dev *pMBDev, mps_fw *pFWDwnld)
s32 ifx_mpsdrv_download_firmware(void *load_entry, void *exec_entry, mps_fw *pFWDwnld)	// implemented later
{
    u32 *pDwnLd;

	// Can ONLY be issued in the CPU0, need checking whether if the caller is in CPU1 or not!
	//
	printk(KERN_INFO "IFX_MPS0: Loading CPU1 firmware ...");
    ifx_mpsdrv_reset();		//make the CPU1 under reset

    memcpy((char*) load_entry, (char*) pFWDwnld->data, pFWDwnld->length);
    pDwnLd = (u32*) exec_entry;

    /* reconfigure CPU1 boot parameters for DSP restart after FW download
     *
     * - enable software boot select
     * - set boot configuration for SDRAM boot
     * - write new reset vector (firmware start address)
     * - restart program execution
     */

    ifx_mps_dev.base_global->MBX_CPU1_BOOT_CFG.MPS_BOOT_SIZE = pFWDwnld->length;
    ifx_mps_dev.base_global->MBX_CPU1_BOOT_CFG.MPS_BOOT_RVEC = (u32) pDwnLd;

    /* Activate BootROM shortcut */
    ifx_mps_dev.base_global->MBX_CPU1_BOOT_CFG.MPS_CFG_STAT |= 0x00700000;

    ifx_mpsdrv_release();	//release the reset to let the CPU1 go
	printk(KERN_INFO "IFX_MPS0: Restarting CPU1 firmware ...");

//  *DANUBE_RCU_RST_REQ |= DANUBE_RCU_RST_REQ_CPU0_BR;

    return(OK);
}


/**
 * Restart CPU1
 * This function restarts CPU1 by accessing the reset request register and
 * reinitializes the mailbox.
 *
 * \return  0        OK, successful restart
 * \return  -1       ERROR, if reset failed (currently always OK)
 * \ingroup Internal
 */
s32 ifx_mpsdrv_restart(void)
{
//	u32 reset_ctrl;

    ifx_mpsdrv_reset();
    /* start VCPU program execution */
    ifx_mpsdrv_release();

//  reset_ctrl = *DANUBE_RCU_RST_REQ;
//  *DANUBE_RCU_RST_REQ = reset_ctrl | DANUBE_RCU_RST_REQ_CPU0_BR;
    printk(KERN_INFO "IFX_MPS0: Restarting CPU1 firmware ...");
//  return ifx_mpsdrv_print_fw_version();
    return (OK);
}

//---------------------------------------------------------------------------------------------------------------
/******************************************************************************
 * Global Routines
 ******************************************************************************/
/**
 * Open MPS device
 * Open routine for the MPS device driver.
 *
 * \param   mps_device  MPS communication device structure
 * \param   pMBDev      Pointer to mailbox device structure
 * \param   bcommand    voice/command selector, TRUE -> command, FALSE -> voice
 * \return  0           OK, successfully opened
 * \return  -1          ERROR, Driver already installed
 * \ingroup Internal
 */
//s32 ifx_mpsdrv_common_open(mps_comm_dev *mps_device, mps_mbx_dev *pMBDev, bool_t bcommand)
s32 ifx_mpsdrv_common_open(mps_mbx_dev *pMBDev, bool_t flagCmnd)
{
    MPS_Ad0Reg_u Ad0Reg;

    /* device is already installed */
    if (pMBDev->Installed == TRUE)
    {
        return (ERROR);
    }

    pMBDev->Installed = TRUE;

    /* enable necessary MPS interrupts */
    Ad0Reg.val = *DANUBE_MPS_AD0ENR;
    if (flagCmnd == TRUE)
    {	/* enable upstream command interrupt */
        Ad0Reg.fld.cu_mbx = 1;
    }
    else
    {   /* enable upstream voice interrupt */
        Ad0Reg.fld.du_mbx = 1;
    }
    *DANUBE_MPS_AD0ENR = Ad0Reg.val;

    return (OK);
}

//---------------------------------------------------------------------------------------------------------------
/**
 * Close routine for MPS device driver
 * This function closes the channel assigned to the passed mailbox
 * device structure.
 *
 * \param   pMBDev   Pointer to mailbox device structure
 * \return  0        OK, will never fail
 * \ingroup Internal
 */
//??? prochao, how about the processing in the CPU1 side ???
s32 ifx_mpsdrv_common_close(mps_mbx_dev *pMBDev)
{
    MPS_Ad0Reg_u Ad0Reg;

    /* clean data structures */
    pMBDev->Installed = FALSE;

    /* Clear the downstream queues for wifi fds only */
    if (pMBDev->devID != command)
    {
        /* if all wifi channel connections are closed disable wifi channel interrupt */
        pMPSDev = pMBDev->pVCPU_DEV;
        if ( (pMPSDev->wifi_mbx[0].Installed == FALSE) && (pMPSDev->wifi_mbx[1].Installed == FALSE) )
        {	//prochao+, only up to 2 wifi channels
            /* disable upstream wifi interrupt */
            Ad0Reg.val = *DANUBE_MPS_AD0ENR;
            Ad0Reg.fld.du_mbx = 0;
            *DANUBE_MPS_AD0ENR = Ad0Reg.val;
        }
#ifdef CONFIG_PROC_FS
        pMBDev->upstrm_fifo.min_space = MBX_DATA_FIFO_SIZE;
        pMBDev->dwstrm_fifo.min_space = MBX_DATA_FIFO_SIZE;
#endif
    }
    else
    {
        /* disable upstream command interrupt */
        Ad0Reg.val = *DANUBE_MPS_AD0ENR;
        Ad0Reg.fld.cu_mbx = 0;
        *DANUBE_MPS_AD0ENR = Ad0Reg.val;
#ifdef CONFIG_PROC_FS
        pMBDev->upstrm_fifo.min_space = MBX_CMD_FIFO_SIZE;
        pMBDev->dwstrm_fifo.min_space = MBX_CMD_FIFO_SIZE;
#endif
    }
    return (OK);
}


/**
 * MPS Structure Release
 * This function releases the entire MPS data structure used for communication
 * between the CPUs.
 *
 * \param   pDev     Poiter to MPS communication structure
 * \ingroup Internal
 */
void ifx_mpsdrv_release_structures(mps_comm_dev *pDev)
{
    s32      count;

    kfree(pDev->command_mbx.sem_dev);	//release the memory allocated for semaphore

    /* Release the Message queues for the voice packets */
    for ( count=0 ; count < MPSDRV_NUM_WIFI_CHANNEL ; count++)
    {
        kfree(pDev->wifi_mbx[count].sem_dev);
        kfree(pDev->wifi_mbx[count].sem_read_fifo);

#ifdef MPS_FIFO_BLOCKING_WRITE
        kfree(pDev->wifi_mbx[count].sem_write_fifo);
#endif /* MPS_FIFO_BLOCKING_WRITE */

        kfree(pDev->wifi_mbx[count].wakeup_pending);
    }
    kfree(pDev->wakeup_pending);
}

/**
 * MPS Structure Initialization
 * This function initializes the data structures of the Multi Processor System
 * that are necessary for inter processor communication
 *
 * \param   pDev     Pointer to MPS device structure to be initialized
 * \return  0        OK, if initialization was successful
 * \return  -1       ERROR, allocation or semaphore access problem
 * \ingroup Internal
 */
//u32 ifx_mpsdrv_init_structures(mps_comm_dev *pDev)
u32 ifx_mpsdrv_initial_structures(mps_comm_dev *pDev)
{
    mps_mbx_reg		*MBX_Memory;
    unsigned long   waitloop;
//    unsigned long cycles_per_microsec;
//prochao+
	MPS_BUFF_POOL_HEAD	*pMPS_BufPoolHead;
//prochao-
    u32		i;

    /* Initialize MPS main structure */
    memset((void*) pDev, 0, sizeof(mps_comm_dev));

    pDev->base_global = (mps_mbx_reg *) DANUBE_MPS_SRAM;
    pDev->flags       = 0x00000000;
    MBX_Memory = pDev->base_global;

    /*
    * Initialize common mailbox definition area which is used by both CPUs
     * for MBX communication. These are: mailbox base address, mailbox size,
    * mailbox read index and mailbox write index. for command and voice mailbox,
    * upstream and downstream direction.
     */
	//prochao+, NOTE! Some of followings are done ONLY by the CPU0, for CPU1, they must be skipped
	/* avoid to overwrite CPU boot registers */
	if ((current_cpu_data.ebase_cpu_num & 0x03FF) == 0)
	{	//this is the CPU0
		memset((void*) MBX_Memory, 0, sizeof(mps_mbx_reg) - 2*sizeof(mps_boot_cfg_reg));
		//
		MBX_Memory->MBX_UPSTR_CMD_BASE   = (u32 *) PHYSADDR( (u32) MBX_UPSTRM_CMD_FIFO_BASE );
		MBX_Memory->MBX_UPSTR_CMD_SIZE   = MBX_CMD_FIFO_SIZE;
		MBX_Memory->MBX_DNSTR_CMD_BASE   = (u32 *) PHYSADDR( (u32) MBX_DNSTRM_CMD_FIFO_BASE );
		MBX_Memory->MBX_DNSTR_CMD_SIZE   = MBX_CMD_FIFO_SIZE;
		MBX_Memory->MBX_UPSTR_DATA_BASE  = (u32 *) PHYSADDR( (u32) MBX_UPSTRM_DATA_FIFO_BASE );
		MBX_Memory->MBX_UPSTR_DATA_SIZE  = MBX_DATA_FIFO_SIZE;
		MBX_Memory->MBX_DNSTR_DATA_BASE  = (u32 *) PHYSADDR( (u32) MBX_DNSTRM_DATA_FIFO_BASE );
		MBX_Memory->MBX_DNSTR_DATA_SIZE  = MBX_DATA_FIFO_SIZE;
		/* set read and write pointers below to the FIFO's uppermost address */
#if 1	//change to another method by prochao+
		MBX_Memory->MBX_UPSTR_CMD_READ   = 0;
		MBX_Memory->MBX_UPSTR_CMD_WRITE  = 0;
		MBX_Memory->MBX_DNSTR_CMD_READ   = 0;
		MBX_Memory->MBX_DNSTR_CMD_WRITE  = 0;
		MBX_Memory->MBX_UPSTR_DATA_READ  = 0;
		MBX_Memory->MBX_UPSTR_DATA_WRITE = 0;
		MBX_Memory->MBX_DNSTR_DATA_READ  = 0;
		MBX_Memory->MBX_DNSTR_DATA_WRITE = 0;
#else	//skipped by prochao+
		MBX_Memory->MBX_UPSTR_CMD_READ   = (MBX_Memory->MBX_UPSTR_CMD_SIZE - 4);
		MBX_Memory->MBX_UPSTR_CMD_WRITE  = (MBX_Memory->MBX_UPSTR_CMD_READ);
		MBX_Memory->MBX_DNSTR_CMD_READ   = (MBX_Memory->MBX_DNSTR_CMD_SIZE - 4);
		MBX_Memory->MBX_DNSTR_CMD_WRITE  = MBX_Memory->MBX_DNSTR_CMD_READ;
		MBX_Memory->MBX_UPSTR_DATA_READ  = (MBX_Memory->MBX_UPSTR_DATA_SIZE - 4);
		MBX_Memory->MBX_UPSTR_DATA_WRITE = MBX_Memory->MBX_UPSTR_DATA_READ;
		MBX_Memory->MBX_DNSTR_DATA_READ  = (MBX_Memory->MBX_DNSTR_DATA_SIZE - 4);
		MBX_Memory->MBX_DNSTR_DATA_WRITE = MBX_Memory->MBX_DNSTR_DATA_READ;
#endif
//prochao+, 11/08/2006, add the support to new API mechanism using the queue lists in the MPS buffer pool inside the CPU0
		pMPS_BufPoolHead = (MPS_BUFF_POOL_HEAD *) ifx_mps_buff_pool_array_get();
		i = (u32) pMPS_BufPoolHead;
		// use following 2 registers to record the inside-CPU0 MPS buffer pool head address for CPU1 to access
		MBX_Memory->MBX_CPU0_BOOT_CFG.MPS_BOOT_SIZE = ~i;
		MBX_Memory->MBX_CPU0_BOOT_CFG.MPS_CFG_STAT = i;
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%d: %s() mps_buff_pool_array{pMPS_BufPoolHead} @%Xh\n", __LINE__, __FUNCTION__, (u32) pMPS_BufPoolHead);
#endif
//prochao-
//prochao+, 09-03-2007, to support the request from core1 of housekeeping the MPS buffers free/allocated to core1
		MBX_Memory->MBX_CPU1_BOOT_CFG.MPS_CFG_STAT = 0;
		MBX_Memory->MBX_CPU0_BOOT_CFG.MPS_CP0_STATUS = 0;	//prochao+-, 13-03-2007, indication of core0 to core1
		// and use the MBX_CPU1_BOOT_CFG.MPS_BOOT_SIZE to record the return buffer pointer!
//prochao-
		//here, the CPU1 can be triggerred to start running now
		//move this inside the MPS driver, till the necessary initializations of MPS are done
		// follow partial code fragment inside the ifx_mps_reset()
//prochao+, 01/09/2007, adds the support for not kick out the core1 thru the config macro setting (from menuconfig)
#ifndef	CONFIG_MPS0_NOT_KICKOUT_CORE1
		if (mps_tpe_cpu1_entry_base)
		{	//yes, kick it
			*DANUBE_RCU_RST_REQ |= (DANUBE_RCU_RST_REQ_CPU1);
			wmb();
			// put the required info in the MBX dedicated area
			MBX_Memory->MBX_CPU1_BOOT_CFG.MPS_BOOT_SIZE = 0;	// ignored, according to the datasheet
			MBX_Memory->MBX_CPU1_BOOT_CFG.MPS_BOOT_RVEC = (u32) mps_tpe_cpu1_entry_base;
			// follow the code fragment inside the ifx_mps_release()
			*DANUBE_RCU_RST_REQ |= 0x20000000;
			wmb();		//prochao+
			*DANUBE_RCU_RST_REQ &= (~8);
			wmb();
		}
#if	1	//defined(CONFIG_DANUBE)||defined(CONFIG_BOARD_TWINPASS_E)	//for dual-core, realtime running
        waitloop = 0x28000000;	//magic# for CPU1 be ready to go!
        //prochao, 7-26-2006, for help debugging with ICE
        while (waitloop--);		//PROCHAO, should be removed once OK
#endif
#endif	//CONFIG_MPS0_NOT_KICKOUT_CORE1
//prochao-
	}
//prochao+, 11/08/2006
#ifdef	CONFIG_DANUBE_CORE1
	else
	{	//for CPU1, to record the head of MPS buffer pool
		mps_buff_pool_array = (MPS_BUFF_POOL_HEAD **) MBX_Memory->MBX_CPU0_BOOT_CFG.MPS_CFG_STAT;
		wmb();
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%d: %s() mps_buff_pool_array @%Xh\n", __LINE__, __FUNCTION__, (u32) mps_buff_pool_array);
#endif
	}
#endif
//prochao-
    /** Configure command mailbox sub structure pointers to global mailbox register addresses */
    /* set command mailbox sub structure pointers to global mailbox register addresses */
	//prochao+, change to another method
	// upstream command fifo indexes
	pDev->command_mbx.upstrm_fifo.pstart = (u32 *) KSEG1ADDR(MBX_Memory->MBX_UPSTR_CMD_BASE);
	pDev->command_mbx.upstrm_fifo.pend = pDev->command_mbx.upstrm_fifo.pstart + ((MBX_Memory->MBX_UPSTR_CMD_SIZE - 4) >> 2);
	pDev->command_mbx.upstrm_fifo.pwrite_off = (u32 *) &(MBX_Memory->MBX_UPSTR_CMD_WRITE);
	pDev->command_mbx.upstrm_fifo.pread_off  = (u32 *) &(MBX_Memory->MBX_UPSTR_CMD_READ);
	pDev->command_mbx.upstrm_fifo.size       = MBX_Memory->MBX_UPSTR_CMD_SIZE;
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: %s() pDev->command_mbx.upstrm_fifo @%Xh\n", __LINE__, __FUNCTION__, (u32) &pDev->command_mbx.upstrm_fifo);
#endif
	// downstream command fifo indexed
	pDev->command_mbx.dwstrm_fifo.pstart = (u32 *) KSEG1ADDR(MBX_Memory->MBX_DNSTR_CMD_BASE);
	pDev->command_mbx.dwstrm_fifo.pend = pDev->command_mbx.dwstrm_fifo.pstart + ((MBX_Memory->MBX_DNSTR_CMD_SIZE - 4) >> 2);
	pDev->command_mbx.dwstrm_fifo.pwrite_off = (u32 *) &(MBX_Memory->MBX_DNSTR_CMD_WRITE);
	pDev->command_mbx.dwstrm_fifo.pread_off  = (u32 *) &(MBX_Memory->MBX_DNSTR_CMD_READ);
	pDev->command_mbx.dwstrm_fifo.size       = MBX_Memory->MBX_DNSTR_CMD_SIZE;
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: %s() pDev->command_mbx.dwstrm_fifo @%Xh\n", __LINE__, __FUNCTION__, (u32) &pDev->command_mbx.dwstrm_fifo);
#endif
	//prochao-
#ifdef CONFIG_PROC_FS
	pDev->command_mbx.upstrm_fifo.min_space = MBX_Memory->MBX_UPSTR_CMD_SIZE;
	pDev->command_mbx.dwstrm_fifo.min_space = MBX_Memory->MBX_DNSTR_CMD_SIZE;
#endif
	//
    pDev->command_mbx.pVCPU_DEV = pDev;    /* global pointer reference */
    pDev->command_mbx.Installed = FALSE ;  /* current installation status */

	/* initialize the semaphores for multitasking access */
	pDev->command_mbx.sem_dev = kmalloc(sizeof(struct semaphore), GFP_KERNEL);
	sema_init(pDev->command_mbx.sem_dev, 1);
	pDev->command_mbx.sem_read_fifo = kmalloc(sizeof(struct semaphore), GFP_KERNEL);
	sema_init(pDev->command_mbx.sem_read_fifo, 0);

	memset(&pDev->command_mbx.event_mask, 0, sizeof(MbxEventRegs_s));

	/* select mechanism implemented for each queue */
	init_waitqueue_head(&pDev->command_mbx.mps_wakeuplist);

    /* configure wifi channel communication structure fields that are common to all wifi channels */
    for ( i=0; i < MPSDRV_NUM_WIFI_CHANNEL; i++ )
    {
	//change to another method by prochao+
		/* wifi upstream data mailbox area */
		pDev->wifi_mbx[i].upstrm_fifo.pstart = (u32 *) KSEG1ADDR(MBX_Memory->MBX_UPSTR_DATA_BASE);
		pDev->wifi_mbx[i].upstrm_fifo.pend = pDev->wifi_mbx[i].upstrm_fifo.pstart + ((MBX_Memory->MBX_UPSTR_DATA_SIZE - 4) >> 2);
		pDev->wifi_mbx[i].upstrm_fifo.pwrite_off  = (u32 *) &(MBX_Memory->MBX_UPSTR_DATA_WRITE);
		pDev->wifi_mbx[i].upstrm_fifo.pread_off   = (u32 *) &(MBX_Memory->MBX_UPSTR_DATA_READ);
		pDev->wifi_mbx[i].upstrm_fifo.size        = MBX_Memory->MBX_UPSTR_DATA_SIZE;
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%d: %s() pDev->wifi_mbx[%d].upstrm_fifo @%Xh\n", __LINE__, __FUNCTION__, i, (u32) &pDev->wifi_mbx[i].upstrm_fifo);
#endif
		/* wifi downstream data mailbox area */
		pDev->wifi_mbx[i].dwstrm_fifo.pstart = (u32 *) KSEG1ADDR(MBX_Memory->MBX_DNSTR_DATA_BASE);
		pDev->wifi_mbx[i].dwstrm_fifo.pend = pDev->wifi_mbx[i].dwstrm_fifo.pstart + ((MBX_Memory->MBX_DNSTR_DATA_SIZE - 4) >> 2);
		pDev->wifi_mbx[i].dwstrm_fifo.pwrite_off  = (u32 *) &(MBX_Memory->MBX_DNSTR_DATA_WRITE);
		pDev->wifi_mbx[i].dwstrm_fifo.pread_off   = (u32 *) &(MBX_Memory->MBX_DNSTR_DATA_READ);
		pDev->wifi_mbx[i].dwstrm_fifo.size        = MBX_Memory->MBX_DNSTR_DATA_SIZE;
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%d: %s() pDev->wifi_mbx[%d].dwstrm_fifo @%Xh\n", __LINE__, __FUNCTION__, i, (u32) &pDev->wifi_mbx[i].dwstrm_fifo);
#endif
#ifdef CONFIG_PROC_FS
        pDev->wifi_mbx[i].upstrm_fifo.min_space   = MBX_Memory->MBX_UPSTR_DATA_SIZE;
    	pDev->wifi_mbx[i].dwstrm_fifo.min_space   = MBX_Memory->MBX_UPSTR_DATA_SIZE;
#endif
        pDev->wifi_mbx[i].Installed     = FALSE ;   /* current mbx installation status */
        pDev->wifi_mbx[i].base_global   = (mps_mbx_reg *) VCPU_BASEADDRESS;
        pDev->wifi_mbx[i].pVCPU_DEV     = pDev; /* global pointer reference */

        /* initialize the semaphores for multitasking access */
        pDev->wifi_mbx[i].sem_dev = kmalloc(sizeof(struct semaphore), GFP_KERNEL);
		if (pDev->wifi_mbx[i].sem_dev == NULL)
			return (ERROR);
		sema_init(pDev->wifi_mbx[i].sem_dev, 1);

        /* initialize the semaphores to read from the fifo */
        pDev->wifi_mbx[i].sem_read_fifo = kmalloc(sizeof(struct semaphore), GFP_KERNEL);
        sema_init(pDev->wifi_mbx[i].sem_read_fifo, 0);

		memset(&pDev->wifi_mbx[i].event_mask, 0, sizeof(MbxEventRegs_s));
		pDev->wifi_mbx[i].wakeup_pending = kmalloc(sizeof(struct semaphore), GFP_KERNEL);
		sema_init(pDev->wifi_mbx[i].wakeup_pending, 1);

#ifdef MPS_FIFO_BLOCKING_WRITE
        pDev->wifi_mbx[i].sem_write_fifo = kmalloc(sizeof(struct semaphore), GFP_KERNEL);
        sema_init(pDev->wifi_mbx[i].sem_write_fifo, 0);
        pDev->wifi_mbx[i].bBlockWriteMB = TRUE;
#endif /* MPS_FIFO_BLOCKING_WRITE */

        /* select mechanism implemented for each queue */
        init_waitqueue_head(&pDev->wifi_mbx[i].mps_wakeuplist);
    }
	/* set channel identifiers */
	pDev->command_mbx.devID = command;
	pDev->wifi_mbx[0].devID = wifi0;
	pDev->wifi_mbx[1].devID = wifi1;
	//
	memset(&pDev->event, 0, sizeof(MbxEventRegs_s));

    pDev->wakeup_pending = kmalloc(sizeof(struct semaphore), GFP_KERNEL);
    sema_init(pDev->wakeup_pending, 1);
//prochao+, reset the array of callback functions
	for (i = 0; i < MPSDRV_TOTAL_SEQ_NUMBERS; i++)
	{
		pDev->callback_function[i] = (MPSFNPTR_T) NULL;
	}
//prochao-
//prochao+, 11/11/2006
	mps_datamsg_queue_init( &mps_rcvDataMsgQ);	//for buffering rcv'ed data messages
	mps_datamsg_queue_init( &mps_rcvCmdMsgQ);	//for buffering rcv'ed command messages
//prochao-

#ifdef INTERRUPT_MITIGATION
        atomic_set(&pkt_count, PKT_AGGREGATE_THRESH);
        init_timer(&intr_coalesce_timer);
        intr_coalesce_timer.function = intr_coalesce_timer_handler;
        intr_coalesce_timer.data = 0;
        intr_coalesce_timer.expires = jiffies + 2;
        add_timer(&intr_coalesce_timer);
#endif

#ifdef INTERRUPT_MITIGATION_EWMA
        cycles_per_microsec = cycles_per_jiffy / USECS_PER_JIFFY;
        min_avg_ipt = min_avg_ipt * cycles_per_microsec;
        max_avg_ipt = max_avg_ipt * cycles_per_microsec;
        tolerable_delay = tolerable_delay * cycles_per_microsec;
        atomic_set (&bklog, 0);

        avg_ipt = max_avg_ipt;

        last_interrupt_time = read_c0_count();
        //last_pkt_time = last_interrupt_time;
        //atomic_set(&last_interrupt_time, read_c0_count());
        last_pkt_time = read_c0_count();
        printk("min_avg_ipt = %u, max_avg_ipt = %u tolerable_delay = %u\n", min_avg_ipt, max_avg_ipt, tolerable_delay);
#endif

    return 0;
}

// return indicating which CPU located, CPU0 or CPU1
MPS_CPU_ID_T ifx_mpsdrv_get_cpu_id(void)
{
	if ((current_cpu_data.ebase_cpu_num & 0x03FF) == 0)
		return MPS_CPU_0_ID;
	else
		return MPS_CPU_1_ID;
}

//return the revision info of local MPS driver
int ifx_mpsdrv_get_fw_version(void)		//special one
{
	return MPSDRV_VERSION_NUM;
}

#ifdef	CONFIG_DANUBE_CORE1
void ifx_mpsdrv_ctrl_mps1_apps_ready_bit(u16 clr_set, u16 bitn)
{
	mps_mbx_reg		*MBX_Memory;
	u32		mps1_ready;

	MBX_Memory = (mps_mbx_reg *) DANUBE_MPS_SRAM;
	mps1_ready = MBX_Memory->MBX_CPU1_BOOT_CFG.MPS_BOOT_SIZE;
	if (MPS_MPS1_APPS_READY_FP == (mps1_ready & MPS_MPS1_APPS_READY_FP_MASK))
	{	//already set/clr before
		if (clr_set)
		{	//set the bit
			mps1_ready |= (0x0001 << bitn);
		}
		else
		{	//clear the bit
			mps1_ready &= ~(0x00001 << bitn);
		}
	}
	else
	{	//this is the 1st time to set/clr
		mps1_ready = MPS_MPS1_APPS_READY_FP;
		if (clr_set)
		{	//set the bit
			mps1_ready |= (0x0001 << bitn);
		}
	}
	//write it back
	MBX_Memory->MBX_CPU1_BOOT_CFG.MPS_BOOT_SIZE = mps1_ready;
}
#endif

u16 ifx_mpsdrv_get_mps1_apps_ready_bits(void)
{
	mps_mbx_reg		*MBX_Memory;
	u32		mps1_ready;

	MBX_Memory = (mps_mbx_reg *) DANUBE_MPS_SRAM;
	mps1_ready = MBX_Memory->MBX_CPU1_BOOT_CFG.MPS_BOOT_SIZE;
	if (MPS_MPS1_APPS_READY_FP == (mps1_ready & MPS_MPS1_APPS_READY_FP_MASK))
	{	//already set effectively
		return (u16) (mps1_ready & MPS_MPS1_APPS_READY_BITMASK);
	}
	//not set by the CPU1 yet, assume all not ready
	return 0;
}

//
s32 ifx_mpsdrv_register_callback(u8 seq_num, MPSFNPTR_T callback)
{
	if (pMPSDev->callback_function[seq_num] != NULL)
		return ERROR;
	//set
	pMPSDev->callback_function[seq_num] = callback;
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: %s(seq#=%d, callback@%Xh)\n", __LINE__, __FUNCTION__, seq_num, (u32) callback);
#endif

	return OK;
}

//
s32 ifx_mpsdrv_unregister_callback(u8 seq_num)
{
	if (pMPSDev->callback_function[seq_num] == NULL)
		return ERROR;
	//reset
	pMPSDev->callback_function[seq_num] = NULL;
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: %s(seq#=%d)\n", __LINE__, __FUNCTION__, seq_num);
#endif

	return OK;
}

/******************************************************************************
 * Mailbox Managment
 ******************************************************************************/
//prochao+, 11/11/2006, adds to use the Q list to handle the data Rx list
void mps_datamsg_queue_init(MPS_DATAMSG_LIST *q)
{
	u32		i;

	q->rpos = 0;
	q->wpos = 0;
	for (i = 0; i < MAX_MPS_DATAMSG_QLEN; i++)
	{
		q->circular_buf[i] = NULL;
	}
}

int32_t mps_dev_kfree_skb_any(MPS_BUF *buf, u8 service_seq_num)
{
#ifdef CONFIG_DANUBE_CORE1
	return ifx_mps_fast_free_mps_buf(buf, service_seq_num);
#else
	dev_kfree_skb_any(buf);
	return 0;
#endif
}

s32	mps_datamsg_enqueue_tail(mps_data_message *buf, MPS_DATAMSG_LIST *q)
{
#ifdef	_PRO_DEBUGGING_
	mps_data_message	*pMsg;

	printk(KERN_INFO "(DMSGQ) [%s] Q'ing to tail of Q[0x%x], wpos[%d], rpos[%d]\n", __FUNCTION__, (u32) q, q->wpos, q->rpos);
#endif
	if (!q)
	{
		printk(KERN_DEBUG "(DMSGQ) Q does not exist in %s\n", __FUNCTION__);
		return -EINVAL;
	}
	if (MPS_DATAMSG_IS_Q_FULL(q))
	{
		printk(KERN_DEBUG "(DMSGQ) Out of Q space in %s - Q[0x%X], wpos[%d], rpos[%d]\n", __FUNCTION__, (u32) q, q->wpos, q->rpos);
		//kishore
		mps_dev_kfree_skb_any((MPS_BUF*)(buf->data_msg_content.msg_body.msg_buf_addr), buf->data_msg_header.msg_seqno);

		return -ENOMEM;
	}
#ifdef	_PRO_DEBUGGING_
	pMsg = (mps_data_message *) buf;
	DPRINTK("[%s:%d] q [0x%x], q->rpos [%d], q->wpos [%d], maxcount[%d] \n", __FUNCTION__, __LINE__,
		(uint32_t)q, q->rpos, q->wpos, MAX_MPS_DATAMSG_QLEN );
	DPRINTK("[%s:%d] Seq#[%d], pMsg [0x%x], msg_buf_addr[0x%x], msg_buf_len[%d] \n", __FUNCTION__, __LINE__,
			pMsg->data_msg_header.msg_seqno, pMsg, pMsg->data_msg_content.msg_body.msg_buf_addr, pMsg->data_msg_content.msg_body.msg_buf_len);
#endif
	MPS_DATAMSG_SET_Q_ITEM(q, buf);
	q->wpos = MPS_DATAMSG_NEXT_Q_POS(q->wpos);
#ifdef	_PRO_DEBUGGING_
	DPRINTK("[%s:%d] q [0x%x], q->rpos [%d], q->wpos [%d], maxcount[%d] \n", __FUNCTION__, __LINE__, (uint32_t)q, q->rpos, q->wpos, MAX_MPS_DATAMSG_QLEN );
#endif
	return 0;
}

mps_data_message *mps_datamsg_dequeue_head(MPS_DATAMSG_LIST *q)
{
	mps_data_message	*m_buf = NULL;

	if (MPS_DATAMSG_IS_Q_EMPTY(q))
	{
#ifdef	_PRO_DEBUGGING_
		printk(KERN_DEBUG "(DMSGQ) Q Empty in (%s) - Q[0x%X], wpos[%d], rpos[%d]\n", __FUNCTION__, (u32) q, q->wpos, q->rpos);
#endif
		return NULL;
	}
	m_buf = MPS_DATAMSG_GET_Q_ITEM(q);
#ifdef	_PRO_DEBUGGING_
	DPRINTK("[%s:%d] q [0x%x], q->rpos [%d], q->wpos [%d], maxcount[%d] \n", __FUNCTION__, __LINE__,
			(uint32_t)q, q->rpos, q->wpos, MAX_MPS_DATAMSG_QLEN );
	DPRINTK("[%s:%d] Seq#[%d], pMsg [0x%x], msg_buf_addr[0x%x], msg_buf_len[%d] \n", __FUNCTION__, __LINE__,
			m_buf->data_msg_header.msg_seqno, m_buf,
			m_buf->data_msg_content.msg_body.msg_buf_addr, 
			m_buf->data_msg_content.msg_body.msg_buf_len);
#endif
	q->rpos = MPS_DATAMSG_NEXT_Q_POS(q->rpos);
#ifdef	_PRO_DEBUGGING_
	DPRINTK("[%s:%d] q [0x%x], q->rpos [%d], q->wpos [%d], maxcount[%d] \n", __FUNCTION__, __LINE__,
			(uint32_t)q, q->rpos, q->wpos, MAX_MPS_DATAMSG_QLEN );
#endif
	return (m_buf);
}

//prochao-

//====================================================================================
//prochao+, these prototypes are already defined in the mps_tpe_device.h file
// ---------------------------------------------
// Read command/data message from the FIFO
// the seq_num is embedded in the message body
//
// return  1		another available message to be retrieved
// return  0        OK, successful read operation,
// return  -1       ERROR, in case of read error.
//
s32 ifx_mpsdrv_mbx_rcv(mps_message *pMsg, MPS_MSG_TYPE msg_type)
{	// the timeout is considered being implemented later
	mps_fifo		*mbx;
	mps_message		*pRcvMsg;
//	u32     		flags;
	u32				i, msg_4bytes, *p32s, *p32d;
	MPS_CPU_ID_T	cpu_id;
	s32     		retval = ERROR;
//	u8				seq_num;

	//who will receive the message (probably the command or data message)
//	seq_num = pMsg->msg_seqno;
	cpu_id = ifx_mpsdrv_get_cpu_id();	//get local CPU ID#
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: enter %s(%Xh)", __LINE__, __FUNCTION__, (u32) pMsg);
#endif
	if (cpu_id == MPS_CPU_0_ID)
	{	//here is CPU0, check if any upstream command or data message from the other side, i.e. CPU1
		switch (msg_type)
		{
			case MPS_DATA_MSG_TYPE:	//upstream data
				//first, check if any upstream data message
				mbx = (mps_fifo*) &(pMPSDev->wifi_mbx[0].upstrm_fifo);
		#ifdef	_PRO_DEBUGGING_
				printk(KERN_INFO "\n%d: CPU0 read fifo (%Xh) for upData_fifo\n", __LINE__, (u32) mbx);
		#endif
				// parsing the rcv'ed message, if any
				pRcvMsg = (mps_message *) ifx_mpsdrv_fifo_read(mbx);
				if (pRcvMsg != NULL)
				{	//got one
					if (pRcvMsg->msg_ready)	// && (pRcvMsg->msg_seqno == seq_num))
					{	//found, copy it back to the space given by the pMsg
						msg_4bytes = MPS_MSGHDR_LEN + (u32) pRcvMsg->msg_len;	//total size (in bytes) for this rcv'ed message
		#ifdef CONFIG_PROC_FS
						pMPSDev->wifi_mbx[0].RxnumBytes += msg_4bytes;
		#endif
#ifdef DIV_OPTIMIZATION
						msg_4bytes = (msg_4bytes + 3 ) >> 2;
#else
						msg_4bytes = msg_4bytes / 4;
						if (pRcvMsg->msg_len % 4)
							msg_4bytes += 1;	//some paddings are added by the sender
#endif
						//copy in 4-bytes
						for (i = 0, p32s = (u32 *) pRcvMsg, p32d = (u32 *) pMsg; i < msg_4bytes; i++)
						{
							*p32d = *p32s;
							p32s += 1;
							p32d += 1;
						}
						//move forward the read index, reset the ready bit and check if this is the end (wrapped) one
						ifx_mpsdrv_fifo_read_ptr_inc( mbx, msg_4bytes);
						//
						return OK;
					}
				}
				break;
			case MPS_CMD_MSG_TYPE:	//upstream command
				// if no upstream data message, then check if any upstream command message
				mbx = (mps_fifo*) &(pMPSDev->command_mbx.upstrm_fifo);
		#ifdef	_PRO_DEBUGGING_
				printk(KERN_INFO "\n%d: CPU0 read fifo (%Xh) for upCmd_fifo\n", __LINE__, (u32) mbx);
		#endif
				// parsing the rcv'ed message, if any
				pRcvMsg = (mps_message *) ifx_mpsdrv_fifo_read(mbx);
				if (pRcvMsg != NULL)
				{	//got one
					if (pRcvMsg->msg_ready)	// && (pRcvMsg->msg_seqno == seq_num))
					{	//found, copy it back to the space given by the pMsg
						msg_4bytes = MPS_MSGHDR_LEN + (u32) pRcvMsg->msg_len;	//total size (in bytes) for this rcv'ed message
		#ifdef CONFIG_PROC_FS
						pMPSDev->command_mbx.RxnumBytes += msg_4bytes;
		#endif
#ifdef DIV_OPTIMIZATION
						msg_4bytes = (msg_4bytes + 3 ) >> 2;
#else
						msg_4bytes = msg_4bytes / 4;
						if (pRcvMsg->msg_len % 4)
							msg_4bytes += 1;	//some paddings are added by the sender
#endif
						//copy in 4-bytes
						for (i = 0, p32s = (u32 *) pRcvMsg, p32d = (u32 *) pMsg; i < msg_4bytes; i++)
						{
							*p32d = *p32s;
							p32s += 1;
							p32d += 1;
						}
						//move forward the read index, reset the ready bit and check if this is the end (wrapped) one
						ifx_mpsdrv_fifo_read_ptr_inc( mbx, msg_4bytes);
						//
						return OK;
					}
				}
				break;
			default:
				return ERROR;
		}
	}
	else
	{	//here is CPU1, check if any command or data message from the other side, i.e. CPU0
		switch (msg_type)
		{
			case MPS_DATA_MSG_TYPE:	//downstream data
				//first, check if any downstream data message
				mbx = (mps_fifo*) &(pMPSDev->wifi_mbx[1].dwstrm_fifo);
		#ifdef	_PRO_DEBUGGING_
				printk(KERN_INFO "\n%d: CPU1 read fifo (%Xh) for dnData_fifo\n", __LINE__, (u32) mbx);
		#endif
				// parsing the rcv'ed message, if any
				pRcvMsg = (mps_message *) ifx_mpsdrv_fifo_read(mbx);
				if (pRcvMsg != NULL)
				{	//got one
					if (pRcvMsg->msg_ready)	// && (pRcvMsg->msg_seqno == seq_num))
					{	//found, copy it back to the space given by the pMsg
						msg_4bytes = MPS_MSGHDR_LEN + (u32) pRcvMsg->msg_len;	//total size (in bytes) for this rcv'ed message
		#ifdef CONFIG_PROC_FS
						pMPSDev->wifi_mbx[1].RxnumBytes += msg_4bytes;
		#endif
#ifdef DIV_OPTIMIZATION
						msg_4bytes = (msg_4bytes + 3 ) >> 2;
#else
						msg_4bytes = msg_4bytes / 4;
						if (pRcvMsg->msg_len % 4)
							msg_4bytes += 1;	//some paddings are added by the sender
#endif
						//copy in 4-bytes
						for (i = 0, p32s = (u32 *) pRcvMsg, p32d = (u32 *) pMsg; i < msg_4bytes; i++)
						{
							*p32d = *p32s;
							p32s += 1;
							p32d += 1;
						}
						//move forward the read index, reset the ready bit and check if this is the end (wrapped) one
						ifx_mpsdrv_fifo_read_ptr_inc( mbx, msg_4bytes);
						//
						return OK;
					}
				}
				break;
			case MPS_CMD_MSG_TYPE:	//upstream command
				// if no downstream data message, then check if any downstream command message
				mbx = (mps_fifo*) &(pMPSDev->command_mbx.dwstrm_fifo);
		#ifdef	_PRO_DEBUGGING_
				printk(KERN_INFO "\n%d: CPU1 read fifo (%Xh) for dnCmd_fifo\n", __LINE__, (u32) mbx);
		#endif
				// parsing the rcv'ed message, if any
				pRcvMsg = (mps_message *) ifx_mpsdrv_fifo_read(mbx);
				if (pRcvMsg != NULL)
				{	//got one
					if (pRcvMsg->msg_ready)	// && (pRcvMsg->msg_seqno == seq_num))
					{	//found, copy it back to the space given by the pMsg
						msg_4bytes = MPS_MSGHDR_LEN + (u32) pRcvMsg->msg_len;	//total size (in bytes) for this rcv'ed message
		#ifdef CONFIG_PROC_FS
						pMPSDev->command_mbx.RxnumBytes += msg_4bytes;
		#endif
#ifdef DIV_OPTIMIZATION
						msg_4bytes = (msg_4bytes + 3 ) >> 2;
#else
						msg_4bytes = msg_4bytes / 4;
						if (pRcvMsg->msg_len % 4)
							msg_4bytes += 1;	//some paddings are added by the sender
#endif
						//copy in 4-bytes
						for (i = 0, p32s = (u32 *) pRcvMsg, p32d = (u32 *) pMsg; i < msg_4bytes; i++)
						{
							*p32d = *p32s;
							p32s += 1;
							p32d += 1;
						}
						//move forward the read index, reset the ready bit and check if this is the end (wrapped) one
						ifx_mpsdrv_fifo_read_ptr_inc( mbx, msg_4bytes);
						//
						return OK;
					}
				}
				break;
			default:
				return ERROR;
		}
	}
	return retval;
}

//====================================================================================
// ---------------------------------------------
// Write command/data message to the FIFO
// the seq_num is embedded in the message body
//
s32 ifx_mpsdrv_mbx_send(mps_message *pMsg)
{
	mps_fifo		*mbx;
	MPS_Ad0Reg_u	MPS_Ad0StatusReg;
	//MPS_Ad1Reg_u	MPS_Ad1StatusReg;
	u32     		MPS_CPU0_2_CPU1_IRReg=0;
	u32				msg_bytes;
	MPS_CPU_ID_T	cpu_id;
	s32     		retval = -EAGAIN;
    mps_message_alias *msg2;
	//u8				seq_num;

	msg_bytes = MPS_MSGHDR_LEN + (u32) pMsg->msg_len;	//total size (in bytes) for this message
	cpu_id = ifx_mpsdrv_get_cpu_id();	//get local CPU ID#
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: enter %s(pMsg=%Xh), msg_type=%Xh, len=%d\n", __LINE__, __FUNCTION__, (u32) pMsg, pMsg->msg_type, msg_bytes);
#endif
	// determine using the command or data mbx, also the direction (upstream [CPU1-to-CPU0] or downstream [CPU0-to-CPU1])
	switch (pMsg->msg_type)
	{	//data message
		case MPS_TYPE_UPSTREAM_DATA:
		case MPS_TYPE_DOWNSTREAM_DATA:
			//prochao+, 11/10/2006, moved here
		case MPS_TYPE_UPSTREAM_APP_CMD:
		case MPS_TYPE_DOWNSTREAM_APP_CMD:
			//prochao-
			if (cpu_id == MPS_CPU_0_ID)
			{	//here is CPU0, must be sent to the CPU1, set pointer to data downstream mailbox
				mbx = (mps_fifo*) &(pMPSDev->wifi_mbx[1].dwstrm_fifo);
				//MPS_Ad1StatusReg.val = 0x00000200;	//MPS_Ad1StatusReg.fld.dd_mbx = 1;	//(downstream data message)
				MPS_CPU0_2_CPU1_IRReg = 0x000020;
			}
			else
			{	//here is CPU1, must be sent to the CPU0, set pointer to data upstream mailbox
				mbx = (mps_fifo*) &(pMPSDev->wifi_mbx[0].upstrm_fifo);
				MPS_Ad0StatusReg.val = 0x00000002;	//MPS_Ad0StatusReg.fld.du_mbx = 1;	//(upstream data message)
			}
			break;
		//command message
		case MPS_TYPE_UPSTREAM_CMD:
		case MPS_TYPE_DOWNSTREAM_CMD:
		case MPS_TYPE_UPSTREAM_RESPONSE:
		case MPS_TYPE_DOWNSTREAM_RESPONSE:
		case MPS_TYPE_UPSTREAM_APP_RESPONSE:
		case MPS_TYPE_DOWNSTREAM_APP_RESPONSE:
			if (cpu_id == MPS_CPU_0_ID)
			{	//here is CPU0, must be sent to the CPU1, set pointer to command downstream mailbox
				mbx = (mps_fifo*) &(pMPSDev->command_mbx.dwstrm_fifo);
				//MPS_Ad1StatusReg.val = 0x00000100;	//MPS_Ad1StatusReg.fld.cd_mbx = 1;	//(downstream command message)
				MPS_CPU0_2_CPU1_IRReg = 0x000010;
			}
			else
			{	//here is CPU1, must be sent to the CPU0, set pointer to command upstream mailbox
				mbx = (mps_fifo*) &(pMPSDev->command_mbx.upstrm_fifo);
				MPS_Ad0StatusReg.val = 0x00000001;	//MPS_Ad0StatusReg.fld.cu_mbx = 1;	//(upstream command message)
			}
			break;
		default:	//error, unsupported types
			return (-ENOMSG);
	}
	/* request for the mailbox buffer memory */
	if (ifx_mpsdrv_fifo_mem_request(mbx, msg_bytes) != TRUE)
	{	//insufficient MBX buffer memory
#ifdef	_PRO_DEBUGGING_
		printk(KERN_DEBUG "[%s:%d] Not enough memory in FIFO for %d bytes\n", __FUNCTION__, __LINE__, msg_bytes);
#endif
		return retval;
	}
	//got enough MBX memory buffer for this message to send
	/* write message words to mailbox buffer starting at write pointer position and
	* update the write pointer index by the amount of written data afterwards */
    msg2 = (mps_message_alias *) pMsg;
	//pMsg->msg_ready = 1;	//marked this message as ready for sending to the other side for being handled
    msg2->val1 |= 0x8000;
	if (ifx_mpsdrv_fifo_write(mbx, pMsg, msg_bytes) != OK)	//prochao+, redefine this API i/f
	{
#ifdef	_PRO_DEBUGGING_
		printk(KERN_DEBUG "%s() failed to send!\n", __FUNCTION__);
#endif
		return -ENOMEM;
	}
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: %s() fifo @%Xh\n", __LINE__, __FUNCTION__, (u32) mbx);
#endif

#if 0
#ifdef CONFIG_PROC_FS
	/* update mailbox statistics */
	pMBDev->TxnumBytes += msg_bytes;
#endif
#endif
	// trigger the interrupt to the other side
	if (cpu_id == MPS_CPU_0_ID)	// here is the CPU0
	{	// set to interrupt the CPU1
		//*DANUBE_MPS_SAD1SR = MPS_Ad1StatusReg.val;
#ifdef INTERRUPT_MITIGATION
                switch (pMsg->msg_type)
                {       //data message
                        case MPS_TYPE_UPSTREAM_DATA:
                        case MPS_TYPE_DOWNSTREAM_DATA:
#ifdef INTERRUPT_MITIGATION_EWMA
                                if (generate_interrupt())
#else
                                if (atomic_dec_and_test(&pkt_count))
#endif
                                {
                                        atomic_set(&pkt_count, PKT_AGGREGATE_THRESH);
		*DANUBE_MPS_CPU0_2_CPU1_IRR = MPS_CPU0_2_CPU1_IRReg;
                                        //printk("Aggregated interrupt\n");
                                }
                                break;

                        default:
                                *DANUBE_MPS_CPU0_2_CPU1_IRR = MPS_CPU0_2_CPU1_IRReg;
                                //printk("Default case\n");
                                break;
                }
#else
                *DANUBE_MPS_CPU0_2_CPU1_IRR = MPS_CPU0_2_CPU1_IRReg;
#endif
	}
	else	// here is the CPU1
	{	// set to interrupt the CPU0
#ifdef INTERRUPT_MITIGATION
                switch (pMsg->msg_type)
                {       //data message
                        case MPS_TYPE_UPSTREAM_DATA:
                        case MPS_TYPE_DOWNSTREAM_DATA:
#ifdef INTERRUPT_MITIGATION_EWMA
                                if (generate_interrupt())
#else
                                if (atomic_dec_and_test(&pkt_count))
#endif
                                {
                                        atomic_set(&pkt_count, PKT_AGGREGATE_THRESH);
		*DANUBE_MPS_SAD0SR = MPS_Ad0StatusReg.val;
                                        //printk("Aggregated interrupt\n");
                                }
                                break;
                        default:
                                *DANUBE_MPS_SAD0SR = MPS_Ad0StatusReg.val;
                                //printk("Default case\n");
                                break;
                }
#else
		*DANUBE_MPS_SAD0SR = MPS_Ad0StatusReg.val;
#endif
	}
	retval = OK;

#ifdef CONFIG_PROC_FS
	if (mbx->min_space < ifx_mpsdrv_fifo_mem_available(mbx))
		mbx->min_space = ifx_mpsdrv_fifo_mem_available(mbx);
#endif /* CONFIG_PROC_FS */

	return retval;
}


#ifdef	CONFIG_DANUBE_CORE1		//for CPU1, to use the cmd1
/** Notify queue about downstream data reception
 * This function checks the channel identifier included in the header
 * of the message currently pointed to by the downstream data mailbox's read pointer.
 * It wakes up the related queue to read the received data message
 * out of the mailbox for further processing. The process is repeated
 * as long as downstream messages are avaiilable in the mailbox.
 * The function is attached to the driver's poll/select functionality.
 *
 * \param   dummy    Tasklet parameter, not used.
 * \ingroup Internal
 */
static u32 _ifx_mpsdrv_mbx_read_dsdMsg(void)
{
	mps_fifo	*mbx;
	mps_data_message	*pDataMsg, *pMsg = NULL;
	u32					msg_4bytes, num_msg = 0;
//	s32					qst;

	/* set pointer to data downstream mailbox */
	mbx = (mps_fifo*) &(pMPSDev->wifi_mbx[1].dwstrm_fifo);

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s() dnData_fifo @%Xh\n", __LINE__, __FUNCTION__, (u32) mbx);
#endif
	while (ifx_mpsdrv_fifo_not_empty( mbx ) == TRUE)
	{	// some upstream data message there
		// take it out and put it to the queue
		pMsg = (mps_data_message *) ifx_mpsdrv_fifo_read(mbx);
		msg_4bytes = MPS_MSGHDR_LEN + (u32) pMsg->data_msg_header.msg_len;	//total size (in bytes) for this rcv'ed message
#ifdef DIV_OPTIMIZATION
		msg_4bytes = (msg_4bytes + 3 ) >> 2;
#else
		msg_4bytes = msg_4bytes / 4;
		if (pMsg->data_msg_header.msg_len % 4)
			msg_4bytes += 1;	//some paddings are added by the sender
#endif
		//copy the whole cmd/data message to the allocated buffer, and enqueue that buffer
		pDataMsg = (mps_data_message *) kmalloc(sizeof(mps_data_message), GFP_ATOMIC);
		if (pDataMsg == NULL)
		{
			printk(KERN_DEBUG "\n%d: %s() - can not allocate the memory!\n", __LINE__, __FUNCTION__);
			break;
		}
		memcpy( pDataMsg, pMsg, sizeof(mps_data_message));	//just copy the whole body to save the time
		mps_datamsg_enqueue_tail( pDataMsg, &mps_rcvDataMsgQ);
		//move forward the read index
		ifx_mpsdrv_fifo_read_ptr_inc( mbx, msg_4bytes);
		//
		num_msg++;
	}
	return num_msg;
}

static void ifx_mpsdrv_mbx_data_downstream(unsigned long dummy)
{
	mps_data_message	*pMsg;
	u8				seq_num;

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s()\n", __LINE__, __FUNCTION__);
#endif
	// parsing the data message and, if necessary, trigger the callback function to handle
	while (TRUE)
	{
		pMsg = (mps_data_message *) mps_datamsg_dequeue_head( &mps_rcvDataMsgQ);
		if (pMsg == NULL)
			break;
		//
		seq_num = pMsg->data_msg_header.msg_seqno;
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%d %s()! Seq#=%xH, call_entry=%X\n", __LINE__, __FUNCTION__,
				seq_num, (u32) pMPSDev->callback_function[seq_num]);
#endif
//prochao+
#ifdef	MPS_USE_KT	// XXX: RItesh not using the kernel thread
//==========================================================================
//prochao+
		//some message comes and put it into the Q
		if (MPS_MSG_IS_Q_FULL(mps_RxMsgQ))
		{
			printk(KERN_DEBUG "%s:%d, RxMsgQ full!\n", __FUNCTION__, __LINE__);
			kfree(pMsg);	//free the allocated buffer
			return;
		}
		mps_RxMsgQ.dataMsg[ mps_RxMsgQ.wrt].pMsg = (void *) pMsg;
		mps_RxMsgQ.dataMsg[ mps_RxMsgQ.wrt].seq_num = seq_num;
		mps_RxMsgQ.wrt = MPS_MSG_NEXT_Q_POS( mps_RxMsgQ.wrt);
		//then wake-up someone to handle it
		mps_RxMsgQ.msg_callback_flag = 1;
		wake_up_interruptible( &mps_RxMsgQ.msg_wakeuplist);
//prochao-
//=========================================================================
#else
		if (pMPSDev->callback_function[seq_num] != NULL)
		{	// call its callback function to get and handle this data message
			pMPSDev->callback_function[seq_num]( pMsg);
		}
		kfree(pMsg);	//free the allocated buffer
#endif
	}

    return;
}

//prochao+, 10/31/2006
void ifx_mps_cpu1_do_DnData_tasklet(unsigned long ul_data)
{
	// call the corresponding handler to process the input downstream data
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: %s(%Xh)\n", __LINE__, __FUNCTION__, (u32) ul_data);
#endif
	ifx_mpsdrv_mbx_data_downstream( ul_data);
}
//prochao-

/** Notify queue about downstream command reception
 * This function checks the channel identifier included in the header
 * of the message currently pointed to by the downstream command mailbox's
 * read pointer. It wakes up the related queue to read the received command
 * message out of the mailbox for further processing. The process is repeated
 * as long as downstream messages are avaiilable in the mailbox.
 * The function is attached to the driver's poll/select functionality.
 *
 * \param   dummy    Tasklet parameter, not used.
 * \ingroup Internal
 */
static u32 _ifx_mpsdrv_mbx_read_dscMsg(void)
{
	mps_fifo	*mbx;
	mps_data_message	*pDataMsg, *pMsg = NULL;
	u32					msg_4bytes, num_msg = 0;
//	s32					qst;

    /* set pointer to data downstream mailbox */
	mbx = (mps_fifo *) &(pMPSDev->command_mbx.dwstrm_fifo);

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s() dnData_fifo @%Xh\n", __LINE__, __FUNCTION__, (u32) mbx);
#endif
	while (ifx_mpsdrv_fifo_not_empty( mbx ) == TRUE)
	{	// some upstream data message there
		// take it out and put it to the queue
		pMsg = (mps_data_message *) ifx_mpsdrv_fifo_read(mbx);
		msg_4bytes = MPS_MSGHDR_LEN + (u32) pMsg->data_msg_header.msg_len;	//total size (in bytes) for this rcv'ed message
#ifdef DIV_OPTIMIZATION
		msg_4bytes = (msg_4bytes + 3 ) >> 2;
#else
		msg_4bytes = msg_4bytes / 4;
		if (pMsg->data_msg_header.msg_len % 4)
			msg_4bytes += 1;	//some paddings are added by the sender
#endif
		//copy the whole cmd/data message to the allocated buffer, and enqueue that buffer
		pDataMsg = (mps_data_message *) kmalloc(sizeof(mps_data_message), GFP_ATOMIC);
		if (pDataMsg == NULL)
		{
			printk(KERN_DEBUG "\n%d: %s() - can not allocate the memory!\n", __LINE__, __FUNCTION__);
			break;
		}
		memcpy( pDataMsg, pMsg, sizeof(mps_data_message));	//just copy the whole body to save the time
		mps_datamsg_enqueue_tail( pDataMsg, &mps_rcvCmdMsgQ);
		//move forward the read index
		ifx_mpsdrv_fifo_read_ptr_inc( mbx, msg_4bytes);
		//
		num_msg++;
	}
	return num_msg;
}

static void ifx_mpsdrv_mbx_cmd_downstream(unsigned long dummy)
{
	mps_data_message	*pMsg;
	u8				seq_num;

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s()\n", __LINE__, __FUNCTION__);
#endif
	// parsing the data message and, if necessary, trigger the callback function to handle
	while (TRUE)
	{
		pMsg = (mps_data_message *) mps_datamsg_dequeue_head( &mps_rcvCmdMsgQ);
		if (pMsg == NULL)
			break;
		//
		seq_num = pMsg->data_msg_header.msg_seqno;
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%d %s()! Seq#=%xH, call_entry=%X\n", __LINE__, __FUNCTION__,
				seq_num, (u32) pMPSDev->callback_function[seq_num]);
#endif
//prochao+
#ifdef	MPS_USE_KT	//using the kernel thread
//==========================================================================
//prochao+
		//some message comes and put it into the Q
		if (MPS_MSG_IS_Q_FULL(mps_RxCmdQ))
		{
			printk(KERN_DEBUG "%s:%d, RxCmdQ full!\n", __FUNCTION__, __LINE__);
			kfree(pMsg);	//free the allocated buffer
			return;
		}
		mps_RxCmdQ.dataMsg[ mps_RxCmdQ.wrt].pMsg = (void *) pMsg;
		mps_RxCmdQ.dataMsg[ mps_RxCmdQ.wrt].seq_num = seq_num;
		mps_RxCmdQ.wrt = MPS_MSG_NEXT_Q_POS( mps_RxCmdQ.wrt);
		//then wake-up someone to handle it
		mps_RxCmdQ.msg_callback_flag = 1;
		wake_up_interruptible( &mps_RxCmdQ.msg_wakeuplist);
//prochao-
//=========================================================================
#else
		if (pMPSDev->callback_function[seq_num] != NULL)
		{	// call its callback function to get and handle this data message
			pMPSDev->callback_function[seq_num](pMsg);
		}
		kfree(pMsg);	//free the allocated buffer
#endif
	}

    return;
}

//prochao+, 10/31/2006
void ifx_mps_cpu1_do_DnCmd_tasklet(unsigned long ul_data)
{
	// call the corresponding handler to process the input downstream command
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: %s(%Xh)\n", __LINE__, __FUNCTION__, (u32) ul_data);
#endif
	ifx_mpsdrv_mbx_cmd_downstream( ul_data);
}
//prochao-

#else	//for CPU0
/** Notify queue about upstream data reception
 * This function checks the channel identifier included in the header
 * of the message currently pointed to by the upstream data mailbox's
 * read pointer. It wakes up the related queue to read the received data message
 * out of the mailbox for further processing. The process is repeated
 * as long as upstream messages are avaiilable in the mailbox.
 * The function is attached to the driver's poll/select functionality.
 *
 * \param   dummy    Tasklet parameter, not used.
 * \ingroup Internal
 */
static u32 _ifx_mpsdrv_mbx_read_usdMsg(void)
{
    mps_fifo	*mbx;
	mps_data_message	*pDataMsg, *pMsg = NULL;
	u32					msg_4bytes, num_msg = 0;
//	s32					qst;

    /* set pointer to data upstream mailbox */
    mbx = (mps_fifo*) &(pMPSDev->wifi_mbx[0].upstrm_fifo);

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s() upData_fifo @%Xh\n", __LINE__, __FUNCTION__, (u32) mbx);
#endif
	while (ifx_mpsdrv_fifo_not_empty( mbx ) == TRUE)
	{	// some upstream data message there
		// take it out and put it to the queue
		pMsg = (mps_data_message *) ifx_mpsdrv_fifo_read(mbx);
#ifdef	_PRO_DEBUGGING_
		DPRINTK("[%s:%d] upFiFo, pStart[0x%x], pReadOff [%d], pWriteOff[%d], size[%d], min_space[%d]\n", __FUNCTION__, __LINE__,
				mbx->pstart, *(mbx->pread_off), *(mbx->pwrite_off), mbx->size, mbx->min_space); 
		DPRINTK("[%s:%d] Seq#[%d], pMsg [0x%x], msg_buf_addr[0x%x], msg_buf_len[%d] \n", __FUNCTION__, __LINE__,
				pMsg->data_msg_header.msg_seqno, pMsg, pMsg->data_msg_content.msg_body.msg_buf_addr, pMsg->data_msg_content.msg_body.msg_buf_len);
#endif
		msg_4bytes = MPS_MSGHDR_LEN + (u32) pMsg->data_msg_header.msg_len;	//total size (in bytes) for this rcv'ed message
#ifdef DIV_OPTIMIZATION
		msg_4bytes = (msg_4bytes + 3 ) >> 2;
#else
		msg_4bytes = msg_4bytes / 4;
		if (pMsg->data_msg_header.msg_len % 4)
			msg_4bytes += 1;	//some paddings are added by the sender
#endif
		//copy the whole cmd/data message to the allocated buffer, and enqueue that buffer
		pDataMsg = (mps_data_message *) kmalloc(sizeof(mps_data_message), GFP_ATOMIC);
		if (pDataMsg == NULL)
		{
			printk(KERN_DEBUG "\n%d: %s() - can not allocate the memory!\n", __LINE__, __FUNCTION__);
			break;
		}
		memcpy( pDataMsg, pMsg, sizeof(mps_data_message));	//just copy the whole body to save the time
		mps_datamsg_enqueue_tail( pDataMsg, &mps_rcvDataMsgQ);
		//move forward the read index
		ifx_mpsdrv_fifo_read_ptr_inc( mbx, msg_4bytes);
		//
#ifdef	_PRO_DEBUGGING_
		DPRINTK("[%s:%d] upFiFo, pStart[0x%x], pReadOff [%d], pWriteOff[%d], size[%d], min_space[%d]\n", __FUNCTION__, __LINE__,
				mbx->pstart, *(mbx->pread_off), *(mbx->pwrite_off), mbx->size, mbx->min_space); 
#endif
		num_msg++;
	}
	return num_msg;
}

static void ifx_mpsdrv_mbx_data_upstream(unsigned long dummy)
{
	mps_data_message	*pMsg;
	u8				seq_num;

//	printk(KERN_INFO "[%s:%d] Tasklet called !\n", __FUNCTION__, __LINE__);
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s()\n", __LINE__, __FUNCTION__);
#endif
		// parsing the data message and, if necessary, trigger the callback function to handle
	while (TRUE)
	{
		pMsg = (mps_data_message *) mps_datamsg_dequeue_head( &mps_rcvDataMsgQ);
		if (pMsg == NULL)
			break;
		//
		seq_num = pMsg->data_msg_header.msg_seqno;
#ifdef	_PRO_DEBUGGING_
		DPRINTK("[%s:%d] Seq#[%d], pMsg [0x%x], msg_buf_addr[0x%x], msg_buf_len[%d] \n", __FUNCTION__, __LINE__,
				seq_num, pMsg, pMsg->data_msg_content.msg_body.msg_buf_addr, pMsg->data_msg_content.msg_body.msg_buf_len);
		printk(KERN_INFO "%d %s()! Seq#=%xH, call_entry=%X\n", __LINE__, __FUNCTION__, seq_num, (u32) pMPSDev->callback_function[seq_num]);
#endif
//prochao+
#ifdef	MPS_USE_KT	//using the kernel thread
//==========================================================================
//prochao+
		//some message comes and put it into the Q
		if (MPS_MSG_IS_Q_FULL(mps_RxMsgQ))
		{
			printk(KERN_DEBUG "%s:%d, RxMsgQ full!\n", __FUNCTION__, __LINE__);
			kfree(pMsg);	//free the allocated buffer
			return;
		}
#ifdef	_PRO_DEBUGGING_		//prochao+, 11/27/2006
		printk(KERN_INFO "[%s:%d] rd[%d],wrt[%d], maxcount[%d] \n", __FUNCTION__, __LINE__,
				mps_RxMsgQ.rd, mps_RxMsgQ.wrt, NUM_MPS_DATAMSG_LIST );
#endif	//prochao-
		mps_RxMsgQ.dataMsg[ mps_RxMsgQ.wrt].pMsg = (void *) pMsg;
		mps_RxMsgQ.dataMsg[ mps_RxMsgQ.wrt].seq_num = seq_num;
		mps_RxMsgQ.wrt = MPS_MSG_NEXT_Q_POS( mps_RxMsgQ.wrt);
#ifdef	_PRO_DEBUGGING_		//prochao+, 11/27/2006
		printk(KERN_INFO "[%s:%d] rd[%d],wrt[%d], maxcount[%d] \n", __FUNCTION__, __LINE__, mps_RxMsgQ.rd, mps_RxMsgQ.wrt, NUM_MPS_DATAMSG_LIST );
#endif	//prochao-
		//then wake-up someone to handle it
		mps_RxMsgQ.msg_callback_flag = 1;
		wake_up_interruptible( &mps_RxMsgQ.msg_wakeuplist);
//prochao-
//=========================================================================
#else
		if (pMPSDev->callback_function[seq_num] != NULL)
		{	// call its callback function to get and handle this command message
			pMPSDev->callback_function[seq_num]((mps_message *) pMsg);
		}
		kfree(pMsg);	//free the allocated buffer
#endif
	}
    return;
}

//prochao+, 10/31/2006
void ifx_mps_cpu0_do_UpData_tasklet(unsigned long ul_data)
{
	// call the corresponding handler to process the input upstream data
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: %s(%Xh)\n", __LINE__, __FUNCTION__, ul_data);
#endif
	ifx_mpsdrv_mbx_data_upstream( ul_data);
}
//prochao-

/** Notify queue about upstream command reception
 * This function checks the channel identifier included in the header
 * of the message currently pointed to by the upstream command mailbox's
 * read pointer. It wakes up the related queue to read the received command
 * message out of the mailbox for further processing. The process is repeated
 * as long as upstream messages are avaiilable in the mailbox.
 * The function is attached to the driver's poll/select functionality.
 *
 * \param   dummy    Tasklet parameter, not used.
 * \ingroup Internal
 */
static u32 _ifx_mpsdrv_mbx_read_uscMsg(void)
{
    mps_fifo	*mbx;
	mps_data_message	*pDataMsg, *pMsg = NULL;
	u32					msg_4bytes, num_msg = 0;
//	s32					qst;

    /* set pointer to command upstream mailbox */
    mbx = (mps_fifo *) &(pMPSDev->command_mbx.upstrm_fifo);

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s() upCmd_fifo @%Xh\n", __LINE__, __FUNCTION__, (u32) mbx);
#endif
	while (ifx_mpsdrv_fifo_not_empty( mbx ) == TRUE)
	{	// some upstream data message there
		// take it out and put it to the queue
		pMsg = (mps_data_message *) ifx_mpsdrv_fifo_read(mbx);
		msg_4bytes = MPS_MSGHDR_LEN + (u32) pMsg->data_msg_header.msg_len;	//total size (in bytes) for this rcv'ed message
#ifdef DIV_OPTIMIZATION
		msg_4bytes = (msg_4bytes + 3 ) >> 2;
#else
		msg_4bytes = msg_4bytes / 4;
		if (pMsg->data_msg_header.msg_len % 4)
			msg_4bytes += 1;	//some paddings are added by the sender
#endif
		//copy the whole cmd/data message to the allocated buffer, and enqueue that buffer
		pDataMsg = (mps_data_message *) kmalloc(sizeof(mps_data_message), GFP_ATOMIC);
		if (pDataMsg == NULL)
		{
			printk(KERN_DEBUG "\n%d: %s() - can not allocate the memory!\n", __LINE__, __FUNCTION__);
			break;
		}
		memcpy( pDataMsg, pMsg, sizeof(mps_data_message));	//just copy the whole body to save the time
		mps_datamsg_enqueue_tail( pDataMsg, &mps_rcvCmdMsgQ);
		//move forward the read index
		ifx_mpsdrv_fifo_read_ptr_inc( mbx, msg_4bytes);
		//
		num_msg++;
	}
	return num_msg;
}

static void ifx_mpsdrv_mbx_cmd_upstream(unsigned long dummy)
{
	mps_data_message	*pMsg;
	u8				seq_num;

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s()\n", __LINE__, __FUNCTION__);
#endif
	// parsing the data message and, if necessary, trigger the callback function to handle
	while (TRUE)
	{
		pMsg = (mps_data_message *) mps_datamsg_dequeue_head( &mps_rcvCmdMsgQ);
		if (pMsg == NULL)
			break;
		//
		seq_num = pMsg->data_msg_header.msg_seqno;
		DPRINTK("[%s:%d] Seq#[%d], pMsg [0x%x], msg_buf_addr[0x%x], msg_buf_len[%d] \n", __FUNCTION__, __LINE__,
				seq_num, pMsg->data_msg_content.msg_body.msg_buf_addr, pMsg->data_msg_content.msg_body.msg_buf_len);
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%d %s()! Seq#=%xH, call_entry=%X\n", __LINE__, __FUNCTION__, seq_num, (u32) pMPSDev->callback_function[seq_num]);
#endif
//prochao+
#ifdef	MPS_USE_KT	//using the kernel thread
//==========================================================================
//prochao+
		//some message comes and put it into the Q
		if (MPS_MSG_IS_Q_FULL(mps_RxCmdQ))
		{
			printk(KERN_DEBUG "%s:%d, RxCmdQ full!\n", __FUNCTION__, __LINE__);
			kfree(pMsg);	//free the allocated buffer
			return;
		}
#ifdef	_PRO_DEBUGGING_		//prochao+, 11/27/2006
		printk(KERN_INFO "[%s:%d] rd[%d],wrt[%d], maxcount[%d] \n", __FUNCTION__, __LINE__, mps_RxCmdQ.rd, mps_RxCmdQ.wrt, NUM_MPS_DATAMSG_LIST);
#endif
		mps_RxCmdQ.dataMsg[ mps_RxCmdQ.wrt].pMsg = (void *) pMsg;
		mps_RxCmdQ.dataMsg[ mps_RxCmdQ.wrt].seq_num = seq_num;
		mps_RxCmdQ.wrt = MPS_MSG_NEXT_Q_POS( mps_RxCmdQ.wrt);
#ifdef	_PRO_DEBUGGING_		//prochao+, 11/27/2006
		printk(KERN_INFO "[%s:%d] rd[%d],wrt[%d], maxcount[%d] \n", __FUNCTION__, __LINE__, mps_RxCmdQ.rd, mps_RxCmdQ.wrt, NUM_MPS_DATAMSG_LIST);
#endif
		//then wake-up someone to handle it
		mps_RxCmdQ.msg_callback_flag = 1;
		wake_up_interruptible( &mps_RxCmdQ.msg_wakeuplist);
//prochao-
//=========================================================================
#else
		if (pMPSDev->callback_function[seq_num] != NULL)
		{	// call its callback function to get and handle this command message
			pMPSDev->callback_function[seq_num]((mps_message *) pMsg);
		}
		kfree(pMsg);	//free the allocated buffer
#endif
    }

    return;
}

//prochao+, 10/31/2006
void ifx_mps_cpu0_do_UpCmd_tasklet(unsigned long ul_data)
{
	// call the corresponding handler to process the input upstream data
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: %s(%Xh)\n", __LINE__, __FUNCTION__, ul_data);
#endif
	ifx_mpsdrv_mbx_cmd_upstream( ul_data);
}
//prochao-

#endif

/******************************************************************************
 * Interrupt service routines
 ******************************************************************************/

#ifdef	CONFIG_DANUBE_CORE1		//for CPU1, to use the cmd1
/**
 * Downstream command interrupt handler
 * This function is called on occurrence of a command downstream interrupt.
 * Depending on the occurred interrupt the downstream command message processing is started via tasklet
 *
 * \param   irq      Interrupt number
 * \param   pDev     Pointer to MPS communication device structure
 * \param   regs     Pointer to system registers
 * \ingroup Internal
 */
void ifx_mpsdrv_cmd1_irq(int irq, mps_comm_dev *pDev, struct pt_regs *regs)
{
    u32		MPS_Cpu0to1StatusReg;
//	mps_mbx_dev		*mbx_dev = (mps_mbx_dev *) &(pMPSDev->command_mbx);

	MPS_Cpu0to1StatusReg = *DANUBE_MPS_CPU0_2_CPU1_IRDR;  /* read interrupt status */
	*DANUBE_MPS_CPU0_2_CPU1_ICR = 0x000010;  /* and acknowledge */
	mask_and_ack_danube_irq(irq);

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s(%d)+\n", __LINE__, __FUNCTION__, irq);
#endif
	if ( MPS_Cpu0to1StatusReg & 0x000010 )
	{	// some downstream command message comming
//prochao+, 13-03-2007
		mps_mbx_reg		*MBX_memory = (mps_mbx_reg *) DANUBE_MPS_SRAM;
		struct net_device	*dev;
		u32				i, mask;
		void			*stats;
		char			wifi_dev[6] = "ath0";		//NOTE: temp hardcoded here for quick

		if (MBX_memory->MBX_CPU0_BOOT_CFG.MPS_CP0_STATUS)	//any statistics request?
		{	//check for the net_device_stats or the iw_statistics and for which virtual AP interface
			stats = NULL;
			MBX_memory->MBX_CPU1_BOOT_CFG.MPS_BOOT_SIZE = 0;	//preset, just in case
			//
			mask = MBX_memory->MBX_CPU0_BOOT_CFG.MPS_CP0_STATUS;
			//determine which device is under requested
			for (i = 0; i < 4; i++)
			{	//support upto 4 virtual AP devices
				if (mask & (0x00001 << i))
					break;	//found
			}
			if (i != 4)
			{	//found one
				wifi_dev[3] = (char) ('0'+ i);
				dev = dev_get_by_name(wifi_dev);
				if (dev != NULL)
				{	//determine what statistics is being requested, request the wireless (wifi0) statistics?
					if (mask & 0x80000000)
					{	//the iw_statistics
						if (dev->get_wireless_stats != NULL)
							stats = (void *) dev->get_wireless_stats(dev);
					}
					else
					{	//request the net_device_stats of the specified device#
						if (dev->get_stats != NULL)
							stats = (void *) dev->get_stats(dev);
					}
				}
			}
			else
			{	//check if for the wifi0
				if (mask & 0x40000000)
				{
					dev = dev_get_by_name("wifi0");
					if ((dev != NULL) && (dev->get_stats != NULL))
					{   //request the wireless (wifi0) statistics, request the net_device_stats of the specified device#
						stats = (void *) dev->get_stats(dev);
					}
				}
			}
			MBX_memory->MBX_CPU1_BOOT_CFG.MPS_BOOT_SIZE = (u32) stats;	//record and inform
			MBX_memory->MBX_CPU0_BOOT_CFG.MPS_CP0_STATUS = 0;	//reset to inform the core0 to take
		}
//prochao-
//prochao+, 10/31/2006, to use the regarding tasklet to handle this
//		ifx_mpsdrv_mbx_cmd_downstream(0);
		if (_ifx_mpsdrv_mbx_read_dscMsg())	//some command msg rcv'ed
			tasklet_schedule( &CPU1_Cmd_Tasklet);
//prochao-
	}
#ifdef CONFIG_PROC_FS
	/* increase counter of read messages and bytes */
	pMPSDev->command_mbx.RxnumIRQs++;
#endif /* CONFIG_PROC_FS */
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s()-\n", __LINE__, __FUNCTION__);
#endif
    return;
}

/**
 * Downstream data interrupt handler
 * This function is called on occurrence of a data downstream interrupt.
 * Depending on the occurred interrupt the downstream data message processing is started via tasklet
 *
 * \param   irq      Interrupt number
 * \param   pDev     Pointer to MPS communication device structure
 * \param   regs     Pointer to system registers
 * \ingroup Internal
 */
void ifx_mpsdrv_data1_irq(int irq, mps_comm_dev *pDev, struct pt_regs *regs)
{
    u32		MPS_Cpu0to1StatusReg;
//	mps_mbx_dev		*mbx_dev = (mps_mbx_dev *) &(pMPSDev->command_mbx);

    MPS_Cpu0to1StatusReg = *DANUBE_MPS_CPU0_2_CPU1_IRDR;  /* read interrupt status */
    *DANUBE_MPS_CPU0_2_CPU1_ICR = 0x000020;  /* and acknowledge */
    mask_and_ack_danube_irq(irq);

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s(%d)+\n", __LINE__, __FUNCTION__, irq);
#endif
    if ( MPS_Cpu0to1StatusReg & 0x000020 )
    {	// some downstream data message coming
//prochao+, 10/31/2006, to use the regarding tasklet to handle this
//		ifx_mpsdrv_mbx_data_downstream(0);
		if (_ifx_mpsdrv_mbx_read_dsdMsg())	//some data msg rcv'ed
			tasklet_schedule( &CPU1_Data_Tasklet);
//prochao-
    }
#ifdef CONFIG_PROC_FS
	/* increase counter of read messages and bytes */
	pMPSDev->command_mbx.RxnumIRQs++;
#endif /* CONFIG_PROC_FS */
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s()-\n", __LINE__, __FUNCTION__);
#endif
    return;
}

#else	//for CPU0, to use the cmd0
/**
 * Upstream data interrupt handler
 * This function is called on occurrence of an data upstream interrupt.
 * Depending on the occured interrupt either the upstream command or data message processing is started via tasklet
 *
 * \param   irq      Interrupt number
 * \param   pDev     Pointer to MPS communication device structure
 * \param   regs     Pointer to system registers
 * \ingroup Internal
 */
void ifx_mpsdrv_cmd0_irq(int irq, mps_comm_dev *pDev, struct pt_regs *regs)
{
    MPS_Ad0Reg_u	MPS_Ad0StatusReg;
    mps_mbx_dev		*mbx_dev = (mps_mbx_dev*) &(pMPSDev->command_mbx);
//	mps_data_message	*pmsg;

    MPS_Ad0StatusReg.val = *DANUBE_MPS_RAD0SR;  /* read interrupt status */
    *DANUBE_MPS_CAD0SR = MPS_Ad0StatusReg.val;  /* and acknowledge */
    mask_and_ack_danube_irq(irq);

//	printk(KERN_INFO "[%s:%d] In ISR for CPU0 Rx \n", __FUNCTION__, __LINE__);
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s(%d)+\n", __LINE__, __FUNCTION__, irq);
#endif
	pMPSDev->event.MPS_Ad0Reg.val = MPS_Ad0StatusReg.val & mbx_dev->event_mask.MPS_Ad0Reg.val;
    if ( MPS_Ad0StatusReg.fld.du_mbx )
    {	// some upstream data message coming
//prochao+, 10/31/2006, to use the regarding tasklet to handle this
		//prochao+, 11/12/2006
		if (_ifx_mpsdrv_mbx_read_usdMsg())	//some data msg rcv'ed
			tasklet_schedule( &CPU0_Data_Tasklet);	//	ifx_mpsdrv_mbx_data_upstream(0);
		//prochao-
//prochao-
    }
    if ( MPS_Ad0StatusReg.fld.cu_mbx )
    {	// some upstream command message comming
//prochao+, 10/31/2006, to use the regarding tasklet to handle this
		//prochao+, 11/12/2006
		if (_ifx_mpsdrv_mbx_read_uscMsg())	//some command msg rcv'ed
			tasklet_schedule( &CPU0_Cmd_Tasklet);	//	ifx_mpsdrv_mbx_cmd_upstream(0);
//prochao-
    }
#ifdef CONFIG_PROC_FS
	/* increase counter of read messages and bytes */
	pMPSDev->command_mbx.RxnumIRQs++;
#endif /* CONFIG_PROC_FS */
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n%d: %s()-\n", __LINE__, __FUNCTION__);
#endif
    return;
}
#endif

//prochao+, move the HW-related IRQs initialization here
int ifx_mpsdrv_init_IRQs(mps_comm_dev *pDev)
{
	int	result;

#ifdef	CONFIG_DANUBE_CORE1		//for CPU1, to use the cmd1
	// init the required tasklets for CPU1
	//DECLARE_TASKLET( CPU1_Cmd_Tasklet, ifx_mps_cpu1_do_DnCmd_tasklet, 0);
	tasklet_init( &CPU1_Cmd_Tasklet, ifx_mps_cpu1_do_DnCmd_tasklet, 0);
	//DECLARE_TASKLET( CPU1_Data_Tasklet, ifx_mps_cpu1_do_DnData_tasklet, 0);
	tasklet_init( &CPU1_Data_Tasklet, ifx_mps_cpu1_do_DnData_tasklet, 0);
	//downstream command
	result = request_irq(INT_NUM_IM0_IRL4, (void *) ifx_mpsdrv_cmd1_irq, SA_INTERRUPT, "mps_mbx cmd1", &ifx_mps_dev);
	if (result)
		return result;
	//downstream data
	result = request_irq(INT_NUM_IM0_IRL5, (void *) ifx_mpsdrv_data1_irq, SA_INTERRUPT, "mps_mbx data1", &ifx_mps_dev);
	if (result)
		return result;
	//enable the interrupts
	*DANUBE_MPS_CPU0_2_CPU1_IER = 0x000030;		//IR5/4
	/* Enable all MPS Interrupts at ICU1     8|7|6|5|4|3|2|1|0|            */
	MPS_INTERRUPTS_ENABLE(0x0000030);	//prochao+, IM0_IRL5/4 are used

#else	//for CPU0, to use the cmd0
	//DECLARE_TASKLET( CPU0_Cmd_Tasklet, ifx_mps_cpu0_do_UpCmd_tasklet, 0);
	tasklet_init( &CPU0_Cmd_Tasklet, ifx_mps_cpu0_do_UpCmd_tasklet, 0);
	//DECLARE_TASKLET( CPU0_Data_Tasklet, ifx_mps_cpu0_do_UpData_tasklet, 0);
	tasklet_init( &CPU0_Data_Tasklet, ifx_mps_cpu0_do_UpData_tasklet, 0);
	///*INT_NUM_IM5_IRL11*/
    result = request_irq(INT_NUM_IM4_IRL18, ifx_mpsdrv_cmd0_irq, SA_INTERRUPT, "mps_mbx cmd0", &ifx_mps_dev);
    if (result)
        return result;

	//prochao+, enable the corresponding bits
	*DANUBE_MPS_AD0ENR = 0x00000003;	//upstream command/data
    /* Enable all MPS Interrupts at ICU0     22|21|20||19|18|17|16||15|14|..|            */
//    MPS_INTERRUPTS_ENABLE(0x007FC000);
	/* Enable all MPS Interrupts at ICU0     22|21|20||18||15|14|..|            */
	MPS_INTERRUPTS_ENABLE(0x00740000);	//prochao+, IM4_IRL19/17/16/15/14 are not used
#endif
	return OK;
}


//kernel thread portion
//--------------------------------------------------------------------------
/* private functions */
static void mps_kthread_launcher(void *data)
{
	mps_kthread_t	*kthread = data;
	kernel_thread((int (*)(void *)) kthread->function, (void *) kthread, 0);
}

/* public functions */
/* create a new kernel thread. Called by the creator. */
void mpsStart_kthread(void (*func)(mps_kthread_t *), mps_kthread_t *kthread)
{
	/* initialize the semaphore */
	init_MUTEX_LOCKED(&kthread->startstop_sem);
	/* store the function to be executed in the data passed to the launcher */
	kthread->function = func;
	/* create the new thread my running a task through keventd */
	/* initialize the task queue structure */
	kthread->tq.sync = 0;
	INIT_LIST_HEAD(&kthread->tq.list);
	kthread->tq.routine = mps_kthread_launcher;
	kthread->tq.data = kthread;
	/* and schedule it for execution */
	schedule_task(&kthread->tq);
	/* wait till it has reached the setup_thread routine */
	down(&kthread->startstop_sem);
}

/* stop a kernel thread. Called by the removing instance */
void mpsStop_kthread(mps_kthread_t *kthread)
{
	if (kthread->thread == NULL)
	{
		printk(KERN_INFO "%s: killing non existing thread!\n", __FUNCTION__);
		return;
	}
	lock_kernel();
	/* initialize the semaphore. */
	init_MUTEX_LOCKED(&kthread->startstop_sem);
	/* set flag to request thread termination */
	kthread->terminate = 1;
	kill_proc(kthread->thread->pid, SIGKILL, 1);
	/* block till thread terminated */
	down(&kthread->startstop_sem);
	/* release the big kernel lock */
	unlock_kernel();
	/* now we are sure the thread is in zombie state. We notify keventd to clean the process up. */
	kill_proc(2, SIGCHLD, 1);
}

/* initialize new created thread. Called by the new thread. */
void mpsInit_kthread(mps_kthread_t *kthread, char *name)
{
	lock_kernel();
	/* fill in thread structure */
	kthread->thread = current;
	/* set signal mask to what we want to respond */
	siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGINT)|sigmask(SIGTERM));
	/* initialise termination flag */
	kthread->terminate = 0;
	/* set name of this process (max 15 chars + 0 !) */
	sprintf(current->comm, name);
	/* let others run */
	unlock_kernel();
	/* tell the creator that we are ready and let him continue */
	up(&kthread->startstop_sem);
}

/* cleanup of thread. Called by the exiting thread. */
void mpsExit_kthread(mps_kthread_t *kthread)
{
	/* we are terminating */

	/* lock the kernel, the exit will unlock it */
	lock_kernel();
	kthread->thread = NULL;
	/* notify the stop_kthread() routine that we are terminating. */
	up(&kthread->startstop_sem);
	/* the kernel_thread that called clone() does a do_exit here. */
}
//prochao-

/* this is the thread function that we are executing */
static void mpsDataMsg_dispatch_thread(mps_kthread_t *kthread)
{
	void		*pdata;
	unsigned char	seq_num;

	/* setup the thread environment */
	mpsInit_kthread( kthread, "mpsDataMsg Dispatch");

#ifdef	_PRO_DEBUGGING_		//prochao+, 11/27/2006
	printk(KERN_INFO "Kernel thread: %s starts\n", __FUNCTION__);
#endif
	/* an endless loop in which we are doing our work */
	while (1)
	{	//waiting for the event that some msg received!
		wait_event_interruptible( mps_RxMsgQ.msg_wakeuplist, mps_RxMsgQ.msg_callback_flag != 0);
		mps_RxMsgQ.msg_callback_flag = 0;
		/* We need to do a memory barrier here to be sure that the flags are visible on all CPUs. */
		if (kthread->terminate)
		{   /* we received a request to terminate ourself */
			break;    
		}
//		printk(KERN_INFO "[%s:%d] Wake up kernel thread !\n", __FUNCTION__, __LINE__);
		/* this is normal work to do */
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%s: thread woke up\n", __FUNCTION__);
#endif
		//process those received messages, to read them out till such Q is empty
		while (1)
		{
			if (MPS_MSG_IS_Q_EMPTY(mps_RxMsgQ))
				break;	//wait till next one or ones
			// get one to handle
#ifdef	_PRO_DEBUGGING_		//prochao+, 11/27/2006
			printk(KERN_INFO "[%s:%d] rd[%d],wrt[%d], maxcount[%d] \n", __FUNCTION__, __LINE__, mps_RxMsgQ.rd, mps_RxMsgQ.wrt, NUM_MPS_DATAMSG_LIST);
#endif
			pdata = mps_RxMsgQ.dataMsg[ mps_RxMsgQ.rd].pMsg;
			seq_num = mps_RxMsgQ.dataMsg[ mps_RxMsgQ.rd].seq_num;
			mps_RxMsgQ.rd = MPS_MSG_NEXT_Q_POS( mps_RxMsgQ.rd);	//advance to next one to read
			DPRINTK("[%s:%d] Seq#[%d], pMsg [0x%x], msg_buf_addr[0x%x], msg_buf_len[%d] \n", __FUNCTION__, __LINE__,
					seq_num , pdata, ((mps_data_message *)pdata)->data_msg_content.msg_body.msg_buf_addr,
					((mps_data_message *)pdata)->data_msg_content.msg_body.msg_buf_len);
#ifdef	_PRO_DEBUGGING_		//prochao+, 11/27/2006
			printk(KERN_INFO "[%s:%d] rd[%d],wrt[%d], maxcount[%d] \n", __FUNCTION__, __LINE__, mps_RxMsgQ.rd, mps_RxMsgQ.wrt, NUM_MPS_DATAMSG_LIST);
#endif
			//take the message out and call its callback()
#ifdef	_PRO_DEBUGGING_
			printk(KERN_INFO "%s:%d - callback_function[%d] @%X", __FUNCTION__, __LINE__, seq_num, (u32) pMPSDev->callback_function[seq_num]);
#endif
			if (pMPSDev->callback_function[seq_num] != NULL)
			{	// call its callback function to get and handle this command message
				pMPSDev->callback_function[seq_num]((mps_data_message *) pdata);
			}
			kfree(pdata);	//free the allocated buffer
		}
	}
	/* cleanup the thread, leave */
	mpsExit_kthread(kthread);
/* returning from the thread here calls the exit functions */
}

/* this is the thread function that we are executing */
static void mpsCmdMsg_dispatch_thread(mps_kthread_t *kthread)
{
//	void		*pdata;
//	unsigned long	lockflag;
//	unsigned char	seq_num;

	/* setup the thread environment */
	mpsInit_kthread( kthread, "mpsCmdMsg Dispatch");

#ifdef	_PRO_DEBUGGING_		//prochao+, 11/27/2006
	printk(KERN_INFO "Kernel thread: %s starts\n", __FUNCTION__);
#endif
	/* an endless loop in which we are doing our work */
	while (1)
	{	//waiting for the event that some msg received! every 500ms will be timeout
//		wait_event_interruptible_timeout( mps_RxCmdQ.msg_wakeuplist, mps_RxCmdQ.msg_callback_flag != 0, HZ/2);
		wait_event_interruptible( mps_RxCmdQ.msg_wakeuplist, mps_RxCmdQ.msg_callback_flag != 0);
		mps_RxCmdQ.msg_callback_flag = 0;
		/* We need to do a memory barrier here to be sure that the flags are visible on all CPUs. */
		if (kthread->terminate)
		{   /* we received a request to terminate ourself */
			break;    
		}
		/* this is normal work to do */
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%s: thread woke up\n", __FUNCTION__);
#endif
		//process those received messages, to read them out till such Q is empty
		while (1)
		{
			if (MPS_MSG_IS_Q_EMPTY(mps_RxCmdQ))
			{	//possibly triggered by the timeout of timer
//prochao+
#ifndef	CONFIG_DANUBE_CORE1		//for the CPU0 only
//				spin_lock_irqsave(&mps_RxCmdQ.mps_semaphore_spinlock, lockflag);
				//
#if 0 /* [ XXX: Ritesh */
				ifx_cpu0_topup_q_lists(10);
				ifx_cpu0_release_q_lists(10);
				ifx_cpu0_topup_q_lists(20);
				ifx_cpu0_release_q_lists(20);
#endif /* ] */
				//
//				spin_unlock_irqrestore(&mps_RxCmdQ.mps_semaphore_spinlock, lockflag);
#ifdef	_PRO_DEBUGGING_
				printk(KERN_INFO "By timeout!\n\n");
#endif
#endif
//prochao-
				break;	//wait till next one or ones
			}
#ifdef	MPS_USE_KT	//prochao+
			// get one to handle
			pdata = mps_RxCmdQ.dataMsg[ mps_RxCmdQ.rd].pMsg;
			seq_num = mps_RxCmdQ.dataMsg[ mps_RxCmdQ.rd].seq_num;
			mps_RxCmdQ.rd = MPS_MSG_NEXT_Q_POS( mps_RxCmdQ.rd);	//advance to next one to read
			//take the message out and call its callback()
#ifdef	_PRO_DEBUGGING_
			printk(KERN_INFO "%s:%d - callback_function[%d] @%X", __FUNCTION__, __LINE__, seq_num, (u32) pMPSDev->callback_function[seq_num]);
#endif
			if (pMPSDev->callback_function[seq_num] != NULL)
			{	// call its callback function to get and handle this command message
				pMPSDev->callback_function[seq_num]((mps_data_message *) pdata);
			}
			kfree(pdata);	//free the allocated buffer
#endif	//prochao-
		}
	}
	/* cleanup the thread, leave */
	mpsExit_kthread(kthread);
/* returning from the thread here calls the exit functions */
}

#ifndef	CONFIG_DANUBE_CORE1		//for the CPU0
static void mps_timer_handler(unsigned long data)
{
#ifdef	MPS_USE_HOUSEKEEPING_TIMER
	//downstream mps data buffers
	ifx_cpu0_topup_q_lists(MPS_SRV_DS_DATA_ID);		//10
	ifx_cpu0_release_q_lists(MPS_SRV_DS_DATA_ID);	//10
	//upstream mps data buffers
	ifx_cpu0_topup_q_lists(MPS_SRV_US_DATA_ID);		//20
	ifx_cpu0_release_q_lists(MPS_SRV_US_DATA_ID);	//20
	//
//	mps_timer.expires = jiffies + HZ/2;		//every 0.5s
//	add_timer(&mps_timer);
	mod_timer(&mps_timer, jiffies + HZ/2);	//every 0.5s
	//
#else	//original one
	mps_RxCmdQ.msg_callback_flag = 1;
	wake_up_interruptible( &mps_RxCmdQ.msg_wakeuplist);		//just wake-up

	mod_timer(&mps_timer, jiffies + 2*HZ);		//every 2s
#endif
}
#endif

#ifdef INTERRUPT_MITIGATION
static void intr_coalesce_timer_handler(unsigned long data)
{
        MPS_CPU_ID_T    cpu_id;
        cpu_id = ifx_mpsdrv_get_cpu_id();       //get local CPU ID#

#ifdef INTERRUPT_MITIGATION_EWMA
        if (atomic_read(&bklog) != 0)
#else
        if (atomic_read(&pkt_count) != PKT_AGGREGATE_THRESH)
#endif
        {
                if (cpu_id == MPS_CPU_0_ID)     // here is the CPU0
                {       // set to interrupt the CPU1
                        //*DANUBE_MPS_SAD1SR = MPS_Ad1StatusReg.val;
                        atomic_set(&pkt_count, PKT_AGGREGATE_THRESH);
                        *DANUBE_MPS_CPU0_2_CPU1_IRR = 0x000020;
                        //printk("Fire from intr_coalesce_timer_handler\n");
                }
                else    // here is the CPU1
                {       // set to interrupt the CPU0
                        atomic_set(&pkt_count, PKT_AGGREGATE_THRESH);
                        *DANUBE_MPS_SAD0SR = 0x00000002;
                        //printk("Fire from intr_coalesce_timer_handler\n");
                }
#ifdef INTERRUPT_MITIGATION_EWMA
                last_interrupt_time = read_c0_count();  //race condition make atomic
                //atomic_set(&last_interrupt_time, read_c0_count());
                atomic_set(&bklog, 0);
#endif
        }
        flush_bklog_q();
        mod_timer(&intr_coalesce_timer, jiffies + 1);
}

#ifdef INTERRUPT_MITIGATION_EWMA
#define TIME_DELTA(a, b) ((a) > (b) ? ((a) - (b)) : (0xFFFFFFFF - (b) + (a)))
static int generate_interrupt()
{
        unsigned int ipt, curr_pkt_time, prev_interrupt_time;
#if KDEBUG
        static int ctr = 0, mean_ipt = 0;
        static int tmp = 0;
        static int max_bklog = 0;
#endif

        curr_pkt_time = read_c0_count(); //mips_hpt_read();
        ipt = TIME_DELTA( curr_pkt_time, last_pkt_time);
        last_pkt_time = curr_pkt_time;
        avg_ipt = avg_ipt - (avg_ipt >> SCALE) + (ipt >> SCALE);
        avg_ipt = (avg_ipt > min_avg_ipt) ? avg_ipt : min_avg_ipt;
        avg_ipt = (avg_ipt < max_avg_ipt) ? avg_ipt : max_avg_ipt;
#ifdef KDEBUG
        ctr++;
        mean_ipt = (mean_ipt + ipt) >> 1;
        if (ctr == 10000)
        {
                ctr = 0;
                printk("avg_ipt = %d mean_ipt = %d ipt=%d mean_pkt_len = %d count_interrupts = %u count_mitigated = %u bklog = %d fail = %d max_bklog = %d \n", avg_ipt, mean_ipt, ipt, mean_pkt_len, count_interrupts, count_mitigated, count_bklog_pkts, count_fail_pkts, count_max_bklog_q);
                count_mitigated = 0;
                count_interrupts = 0;
                count_bklog_pkts = 0;
                count_fail_pkts = 0;
                count_max_bklog_q = 0;
#if 0
                printk("avg_ipt = %d ipt=%d count_interrupts = %u count_mitigated = %u bklog = %d max_bklog = %d \n", avg_ipt, ipt, count_interrupts, count_mitigated, bklog, max_bklog);
                max_bklog = 0;
#endif
        }
#endif
        prev_interrupt_time = last_interrupt_time;
        if (((TIME_DELTA(curr_pkt_time, prev_interrupt_time) + avg_ipt) > tolerable_delay) || (atomic_read(&bklog) > MAX_DEFERRED_PKTS))
        {
                last_interrupt_time = curr_pkt_time;
                atomic_set(&bklog, 0);
#ifdef KDEBUG
                count_interrupts++;
#endif
                return 1;
        }
        else
        {
                atomic_inc(&bklog);
#ifdef KDEBUG
                count_mitigated++;
#if 0
                tmp = atomic_read(&bklog);
                if (tmp > max_bklog)
                        max_bklog = tmp;
#endif
#endif
                return 0;  //mitigate the interrupt
        }
}
#endif  
#endif

//
//---------------------------------------------------------
// prochao+, 11/16/2006, morning
void ifx_mpsdrv_initialize_KT(void)
{
	//RxDataMsg
	mps_RxMsgQ.rd = mps_RxMsgQ.wrt = 0;
	mps_RxMsgQ.msg_callback_flag = 0;
	init_waitqueue_head(&mps_RxMsgQ.msg_wakeuplist);
	mpsStart_kthread( mpsDataMsg_dispatch_thread, &mps_RxMsgQ.rcv_thread_data);
	//RxCmdMsg
	mps_RxCmdQ.rd = mps_RxCmdQ.wrt = 0;
	mps_RxCmdQ.msg_callback_flag = 0;
	mps_RxCmdQ.mps_semaphore_spinlock = SPIN_LOCK_UNLOCKED;
	init_waitqueue_head(&mps_RxCmdQ.msg_wakeuplist);
	mpsStart_kthread( mpsCmdMsg_dispatch_thread, &mps_RxCmdQ.rcv_thread_data);
#ifndef	CONFIG_DANUBE_CORE1		//for the CPU0
	//setup one timer for timeout
	init_timer(&mps_timer);
	mps_timer.function = mps_timer_handler;
	mps_timer.expires = jiffies + 3*HZ;		//after 3s
	add_timer(&mps_timer);
#endif
	//
	printk(KERN_INFO "%s:%d - initialized!\n", __FUNCTION__, __LINE__);
}

//---------------------------------------------------------
void ifx_mpsdrv_stop_KT(void)
{
	mpsStop_kthread(&mps_RxMsgQ.rcv_thread_data);
	mpsStop_kthread(&mps_RxCmdQ.rcv_thread_data);
}
//prochao-

//prochao+, 11/30/2006,
#if	(!defined(CONFIG_DANUBE_CORE1) && defined(MPS_USE_HOUSEKEEPING_TIMER))	//only for the CPU0
void ifx_mpsdrv_initialize_hk_timer(void)	//initialize the housekeeping timer
{
	//setup one timer for timeout
	init_timer(&mps_timer);
	mps_timer.data = 0;
	mps_timer.function = mps_timer_handler;
	mps_timer.expires = jiffies + 3*HZ;		//after 3s
	add_timer(&mps_timer);
}
#endif
//prochao-

//prochao+, 13-03-2007
#ifndef	CONFIG_DANUBE_CORE1		//in core0 only
//actually the returned pointer is pointed to the net_device_stats structure
void *ifx_mps_get_core1_wifi_stats(int devid)
{
	mps_mbx_reg		*MBX_memory = (mps_mbx_reg *) DANUBE_MPS_SRAM;
	u32				mask;

	if (devid == VNET_WIFI0_MAGIC_NUM)
	{
		mask = 0x40000000;	//indicate the wifi0
	}
	else
	{
		mask = 0x00000001 << devid;
	}
	MBX_memory->MBX_CPU0_BOOT_CFG.MPS_CP0_STATUS = mask;	//0x80000000 bit will be used to indicate device or wireless stats
	*DANUBE_MPS_CPU0_2_CPU1_IRR = 0x000010;		//issue the downstream command request intr
	//polling the status and then get the returned pointer
	while (1)
	{
		udelay(3);
		if (MBX_memory->MBX_CPU0_BOOT_CFG.MPS_CP0_STATUS == 0)	//core1 did and cleared this indication
			break;
	}
	// get the pointer in the MBX_CPU1_BOOT_CFG.MPS_BOOT_SIZE register
	return (void *) MBX_memory->MBX_CPU1_BOOT_CFG.MPS_BOOT_SIZE;
}

//the returned pointer to the iw_statistics structure
void *ifx_mps_get_core1_wireless_stats(int devid)
{
	mps_mbx_reg		*MBX_memory = (mps_mbx_reg *) DANUBE_MPS_SRAM;
	u32				mask;

	mask = 0x00000001 << devid;
	//0x80000000 bit will be used to get the wireless stats
	MBX_memory->MBX_CPU0_BOOT_CFG.MPS_CP0_STATUS = 0x80000000 | mask;
	*DANUBE_MPS_CPU0_2_CPU1_IRR = 0x000010;		//issue the downstream command request intr
	//polling the status and then get the returned pointer
	while (1)
	{
		udelay(3);
		if (MBX_memory->MBX_CPU0_BOOT_CFG.MPS_CP0_STATUS == 0)	//core1 did and cleared this indication
			break;
	}
	// get the pointer in the MBX_CPU1_BOOT_CFG.MPS_BOOT_SIZE register
	return (void *) MBX_memory->MBX_CPU1_BOOT_CFG.MPS_BOOT_SIZE;
}

EXPORT_SYMBOL(ifx_mps_get_core1_wifi_stats);
EXPORT_SYMBOL(ifx_mps_get_core1_wireless_stats);
#endif
//prochao-

