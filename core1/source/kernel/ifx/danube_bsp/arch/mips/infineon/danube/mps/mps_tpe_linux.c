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

#ifdef CONFIG_DEBUG_MINI_BOOT
	#define IKOS_MINI_BOOT
#endif

#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/ioctl.h>
//prochao+, 11/10/2006
#include <asm/io.h>
//prochao-
#include <linux/version.h>
#include <linux/delay.h>
//
#include <linux/skbuff.h>
#include <linux/spinlock.h>		//prochao+!

#ifdef	CONFIG_DANUBE_CORE1		//prochao+-
	#include <net/dst.h>
#endif

#include <asm/irq.h>
#include <asm/danube/irq.h>

#include <asm/danube/ifx_types.h>
#include <asm/danube/danube.h>

//prochao+, 11/08/2006
#include <asm/danube/mps_tpe_buffer.h>
//prochao-
#include <asm/danube/mps_dualcore.h>
#include "mps_tpe_device.h"

#ifdef CONFIG_DEVFS_FS
	#include <linux/devfs_fs_kernel.h>
#else
typedef void *devfs_handle_t;
#endif

//prochao+, 10/27/2006, adds to help debugging
#define	_PRO_DEBUGGING_		1
#undef	_PRO_DEBUGGING_
//prochao-
//
#define EFAIL -1

//#define MAX_PAYLOAD_LEN 126   modify to 32 for test
#define MAX_PAYLOAD_LEN		6	//32 -> 6, prochao+-
#define MAX_IP_DATA			1400
#define MPS_DATA_SIZE		512

#ifdef	_DEBUGGING_
	#define DEBUG_FNENTRY()		printk(KERN_INFO "(MPS) [%s] Entering function ...\n", __FUNCTION__)
	#define DEBUG_FNEXIT()		printk(KERN_INFO "(MPS) [%s] Exiting function at line#[%d] ...\n", __FUNCTION__, __LINE__)
	#define DPRINTK(fmt, args...)	printk(KERN_INFO "(MPS) "fmt, ##args)
#else
	#define	DEBUG_FNENTRY()
	#define	DEBUG_FNEXIT()
	#define DPRINTK(fmt, args...)
#endif

//eventually, the CONFIG_MPS_FLOW_CONTROL macro should be created via the kernel's menuconfig settings
//#define	CONFIG_MPS_FLOW_CONTROL	//prochao%, 12/26/2006, ported from Jeffrey's approval for 11n driver

#ifdef CONFIG_MPS_FLOW_CONTROL
#include <linux/list.h>
static atomic_t bklog_pkts;
static atomic_t bklog_q_lock;
static int ifx_mps_data_send2(u8 service_sequence_number, void* datapointer, u32 len);
static struct timer_list flush_bklog_timer;
static void flush_bklog_timer_handler(unsigned long data);

typedef struct
{
        struct list_head list;
        u8 service_sequence_number;
        void* datapointer;
        u32 len;
}mps_data_list;
static LIST_HEAD(bklog_mps_data_list);

#ifdef KDEBUG
unsigned mean_pkt_len = 0;
unsigned count_bklog_pkts = 0;
unsigned count_fail_pkts = 0;
unsigned count_max_bklog_q = 0;
#endif
#endif


typedef struct
{
	u32                 service_sequence_number;
	spinlock_t          cmd_seq_semaphore_spinlock;
//prochao+, 11/01/2006
	spinlock_t          data_seq_semaphore_spinlock;
//prochao-
	mps_data_message    msg_data; //msg_data
	wait_queue_head_t   cmd_wakeuplist;
//prochao+, 10/31/2006
	wait_queue_head_t   data_wakeuplist;
	volatile int        data_callback_flag;
//prochao-
	//prochao+, 10/27/2006, adds following callback_flag to use the sleeping/waking-up mechanism instead
	volatile int        cmd_callback_flag;
	//prochao-
//prochao+, 11/02/2006
	volatile int        polling_cmd_callback_flag;
//prochao-
//	FNPTR_T             callback_function; //array of function pointers, indexed with the sequence#
} mps_callback_element;

unsigned int    cmd_wakeuplist_balance, data_wakeuplist_balance;

//Data type of the data-related callback function
//typedef void	(*DATAFNPTR_T)(mps_data_message*, int len);	//prochao+-, 10/17/2006, moved to be in the mps_dualcore.h file

DATAFNPTR_T data_callback_funct[MPSDRV_TOTAL_SEQ_NUMBERS];
mps_callback_element  mps_cmd_control;

s32 ifx_mps_open(struct inode *inode, struct file *file_p);
s32 ifx_mps_close(struct inode *inode, struct file *filp);
s32 ifx_mps_ioctl(struct inode *inode, struct file *file_p, u32 nCmd, unsigned long arg);


int ifx_mps_register_callback(u8 service_sequence_number,  FNPTR_T callback);



#ifdef MODULE
MODULE_AUTHOR("Infineon Technologies AG");
MODULE_DESCRIPTION("MPS/Dual-Core driver for TwinPass-E");
MODULE_SUPPORTED_DEVICE("TWINPASS-E MIPS24KEc");
MODULE_LICENSE("GPL");
#endif

static u8 ifx_mps_major_id = 0;
MODULE_PARM(ifx_mps_major_id, "b");
MODULE_PARM_DESC(ifx_mps_major_id, "Major ID of device");

char ifx_mps_dev_name[10];
#define IFX_MPS_DEV_NAME       "ifx_mps"
#define IFX_MPS_VER_STR        "1.5.0"		//"1.5.0"
#define IFX_MPS_INFO_STR \
"@(#)TWINPASS-E MIPS24KEc MPS mailbox driver, Version "IFX_MPS_VER_STR
char voice_channel_int_name[4][15];

/* the driver callbacks */
static struct file_operations ifx_mps_fops =
{
	owner:
	THIS_MODULE,
	//poll:
	//ifx_mps_poll,
	ioctl:
	ifx_mps_ioctl,
	open:
	ifx_mps_open,
	release:
	ifx_mps_close
};

/* device structure */
mps_comm_dev ifx_mps_dev = {};
//prochao+
//static spinlock_t	mps_api_spinlock = SPIN_LOCK_UNLOCKED;
// prochao-

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *ifx_mps_proc_dir;
//static struct proc_dir_entry *ifx_mps_proc_file;

	#ifdef CONFIG_DANUBE_MPS_PROC_DEBUG
		#define MPS_FIRMWARE_BUFFER_SIZE	512*1024
		#define MPS_FW_START_TAG			"IFX-START-FW-NOW"
		#define MPS_FW_INIT_TAG				"IFX-INITIALIZE-VCPU-HARDWARE"
		#define MPS_FW_BUFFER_TAG			"IFX-PROVIDE-BUFFERS"
		#define MPS_FW_OPEN_VOICE_TAG		"IFX-OPEN-VOICE0-MBX"
		#define MPS_FW_REGISTER_CALLBACK_TAG	"IFX-REGISTER-CALLBACK-VOICE0"
		#define MPS_FW_SEND_MESSAGE_TAG			"IFX-SEND-MESSAGE-VOICE0"
		#define MPS_FW_RESTART_TAG				"IFX-RESTART-VCPU-NOW"
		#define MPS_FW_ENABLE_PACKET_LOOP_TAG	"IFX-ENABLE-PACKET-LOOP"
		#define MPS_FW_DISABLE_PACKET_LOOP_TAG	"IFX-DISABLE-PACKET-LOOP"
//static char ifx_mps_firmware_buffer[MPS_FIRMWARE_BUFFER_SIZE];
//static mps_fw ifx_mps_firmware_struct;
//static int ifx_mps_firmware_buffer_pos=0;
static u32 ifx_mps_rtp_voice_data_count=0;

char teststr[256] = "this is a test str\n";
	#endif /* CONFIG_DANUBE_MPS_PROC_DEBUG */

#endif /* CONFIG_PROC_FS */

static char ifx_mps_device_version[] = IFX_MPS_VER_STR;

#define	DANUBE_MPS_VOICE_STATUS_CLEAR 0xC3FFFFFF




u32 ifx_mps_fifo_mem_available(mps_fifo *fifo)
{
	u32 retval;

	retval = (fifo->size - 1 - (*fifo->pread_off - *fifo->pwrite_off)) & (fifo->size - 1);
	return(retval);
}


//should be mps_device


//ifx_mps_mbx_write_cmd
//ifx_mps_mbx_read


//dir = 0 send
//dir = 1 rcv
// prochao+, 10/15/2006, adds the 3rd arg for optional parameter which is required for specific command
// for example, the skbuff_alloc command that needs the bufsize to be given
// prochao-
int ifx_cmd_process(int dir, int cmd, u32 optParam)
{
	int ret = 0;
	mps_data_message    rw;
	unsigned long       lockflag;
	//prochao+, 11/05/2006, supports commands for additional MPS driver APIs
	mps_cmd_param_t     *pcmd_param;

	pcmd_param = (mps_cmd_param_t *) optParam;	//if used
	//prochao-

	memset(&rw, 0, sizeof(mps_data_message));

	if (dir == 0)
	{
#if	1	//prochao+, 11/02/2006, using the spinlock instead
		spin_lock_irqsave(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif	//prochao-
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "\n%d: %s(dir=%d, cmd=%Xh, optP=%Xh)\n", __LINE__, __FUNCTION__, dir, cmd, optParam);
#endif
		switch (cmd)
		{
			case MPS_CMD_CPU1_DATALIST_ALLOC:
				rw.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				//prochao+, 10/15/2006
				rw.data_msg_content.payload = optParam;	// the number of datalist to be allocated
				//prochao-
				rw.data_msg_header.msg_len = 4;	//0;	//prochao+-
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_DATALIST_ALLOC;
				break;

			case MPS_CMD_CPU1_DATALIST_FREE:
				rw.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				//prochao+, 10/15/2006
				rw.data_msg_content.payload = optParam;	// the pointer to the datalist to be free
				//prochao-
				rw.data_msg_header.msg_len = 4;	//0;	//prochao+-
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_DATALIST_FREE;
				break;

			case MPS_CMD_CPU1_SKB_ALLOC:
				rw.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				//prochao+, 10/15/2006
				rw.data_msg_content.payload = optParam;	// the size of skbuff to be allocated
				//prochao-
				rw.data_msg_header.msg_len = 4;	//0;	//prochao+-
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_SKB_ALLOC;
				break;

			case MPS_CMD_CPU1_SKB_FREE:
				rw.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				//prochao+, 10/16/2006
				rw.data_msg_content.payload = optParam;	// the pointer of skbuff to be free
				//prochao-
				rw.data_msg_header.msg_len = 4;	//0;	//prochao+
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_SKB_FREE;
				break;

				//prochao+, 10/18/2006, supports the normal kernel data buffer alloc/free (i.e. kmalloc/kfree)
			case MPS_CMD_CPU1_DATA_ALLOC:
				rw.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				rw.data_msg_content.payload = optParam;	// the size of data buffer to be allocated
				rw.data_msg_header.msg_len = 4;	//0;
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_DATA_ALLOC;
				break;

			case MPS_CMD_CPU1_DATA_FREE:
				rw.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				rw.data_msg_content.payload = optParam;	// the pointer of data buffer to be free
				rw.data_msg_header.msg_len = 4;	//0;	//prochao+
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_DATA_FREE;
				break;
				//prochao-, 10/18/2006
			case MPS_CMD_CPU0_GET_REMOTE_VER:
			case MPS_CMD_CPU1_GET_REMOTE_VER:

				if (ifx_mpsdrv_get_cpu_id() == MPS_CPU_0_ID)
					rw.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				else
					rw.data_msg_header.msg_type	= MPS_TYPE_UPSTREAM_CMD;

				rw.data_msg_header.msg_seqno = 0;
				rw.data_msg_header.msg_len = 0;
				rw.data_msg_header.msg_cmd = cmd;
				break;
				//prochao+, 10/30/2006, commands for new APIs
			case MPS_CMD_CPU1_IFX_ALLOC_SKB:
				rw.data_msg_header.msg_type = MPS_TYPE_UPSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				rw.data_msg_content.cmd_parameters.cmd_alloc_skb_param_size = pcmd_param->cmd_alloc_skb_param_size;
				rw.data_msg_content.cmd_parameters.cmd_alloc_skb_param_prio = pcmd_param->cmd_alloc_skb_param_prio;
				rw.data_msg_header.msg_len = 8;	//prochao+
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_IFX_ALLOC_SKB;
				break;
			case MPS_CMD_CPU1_IFX_SKB_CLONE:
				rw.data_msg_header.msg_type = MPS_TYPE_UPSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				rw.data_msg_content.cmd_parameters.cmd_skb_clone_copy_param_skb = pcmd_param->cmd_skb_clone_copy_param_skb;
				rw.data_msg_content.cmd_parameters.cmd_skb_clone_copy_param_prio = pcmd_param->cmd_skb_clone_copy_param_prio;
				rw.data_msg_header.msg_len = 8;	//prochao+
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_IFX_SKB_CLONE;
				break;
			case MPS_CMD_CPU1_IFX_SKB_COPY:
				rw.data_msg_header.msg_type = MPS_TYPE_UPSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				rw.data_msg_content.cmd_parameters.cmd_skb_clone_copy_param_skb = pcmd_param->cmd_skb_clone_copy_param_skb;
				rw.data_msg_content.cmd_parameters.cmd_skb_clone_copy_param_prio = pcmd_param->cmd_skb_clone_copy_param_prio;
				rw.data_msg_header.msg_len = 8;	//prochao+
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_IFX_SKB_COPY;
				break;
			case MPS_CMD_CPU1_IFX_PSKB_EXPAND_HEAD:
				rw.data_msg_header.msg_type = MPS_TYPE_UPSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				rw.data_msg_content.cmd_parameters.cmd_pskb_expand_head_param_skb = pcmd_param->cmd_pskb_expand_head_param_skb;
				rw.data_msg_content.cmd_parameters.cmd_pskb_expand_head_param_nhead = pcmd_param->cmd_pskb_expand_head_param_nhead;
				rw.data_msg_content.cmd_parameters.cmd_pskb_expand_head_param_ntail = pcmd_param->cmd_pskb_expand_head_param_ntail;
				rw.data_msg_content.cmd_parameters.cmd_pskb_expand_head_param_qfp_mask = pcmd_param->cmd_pskb_expand_head_param_qfp_mask;
				rw.data_msg_header.msg_len = 16;	//prochao+
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_IFX_PSKB_EXPAND_HEAD;
				break;
			case MPS_CMD_CPU1_IFX_SKB_REALLOC_HEADROOM:
				rw.data_msg_header.msg_type = MPS_TYPE_UPSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				rw.data_msg_content.cmd_parameters.cmd_skb_realloc_headroom_param_skb = pcmd_param->cmd_skb_realloc_headroom_param_skb;
				rw.data_msg_content.cmd_parameters.cmd_skb_realloc_headroom_param_headroom = pcmd_param->cmd_skb_realloc_headroom_param_headroom;
				rw.data_msg_header.msg_len = 8;	//prochao+
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_IFX_SKB_REALLOC_HEADROOM;
				break;
			case MPS_CMD_CPU1_IFX_SKB_COPY_EXPAND:
				rw.data_msg_header.msg_type = MPS_TYPE_UPSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				rw.data_msg_content.cmd_parameters.cmd_skb_copy_expand_param_skb = pcmd_param->cmd_skb_copy_expand_param_skb;
				rw.data_msg_content.cmd_parameters.cmd_skb_copy_expand_param_newheadroom = pcmd_param->cmd_skb_copy_expand_param_newheadroom;
				rw.data_msg_content.cmd_parameters.cmd_skb_copy_expand_param_newtailroom = pcmd_param->cmd_skb_copy_expand_param_newtailroom;
				rw.data_msg_content.cmd_parameters.cmd_skb_copy_expand_param_prio = pcmd_param->cmd_skb_copy_expand_param_prio;
				rw.data_msg_header.msg_len = 16;	//prochao+
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_IFX_SKB_COPY_EXPAND;
				break;
			case MPS_CMD_CPU1_IFX_SKB_TRIM:	//?????? one more parameter, realloc, needed?
				rw.data_msg_header.msg_type = MPS_TYPE_UPSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				rw.data_msg_content.cmd_parameters.cmd_skb_trim_param_skb = pcmd_param->cmd_skb_trim_param_skb;
				rw.data_msg_content.cmd_parameters.cmd_skb_trim_param_len = pcmd_param->cmd_skb_trim_param_len;
				rw.data_msg_header.msg_len = 8;	//prochao+
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_IFX_SKB_TRIM;
				break;
			case MPS_CMD_CPU1_IFX_DEV_ALLOC_SKB:
				rw.data_msg_header.msg_type = MPS_TYPE_UPSTREAM_CMD;
				rw.data_msg_header.msg_seqno = 0;
				rw.data_msg_content.cmd_parameters.cmd_dev_alloc_skb_param_len = pcmd_param->cmd_dev_alloc_skb_param_len;
				rw.data_msg_content.cmd_parameters.cmd_dev_alloc_skb_param_prio = pcmd_param->cmd_dev_alloc_skb_param_prio;
				rw.data_msg_header.msg_len = 8;	//prochao+
				rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_IFX_DEV_ALLOC_SKB;
				break;
				//
			default:	//should not be here
#if	1			//prochao+, 11/02/2006, using the spinlock instead
				spin_unlock_irqrestore(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif			//prochao-
#ifdef	_PRO_DEBUGGING_
				printk(KERN_DEBUG "%d: %s(): unsupported cmd %Xh\n", __LINE__, __FUNCTION__, cmd);
#endif
				return -1;
				//prochao-
		}
		ret = ifx_mpsdrv_mbx_send((mps_message*) (&rw));
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%d: %s() ifx_mpsdrv_mbx_send(%Xh) => %d\n", __LINE__, __FUNCTION__, (u32) &rw, ret);
#endif
#if	1	//prochao+, 11/02/2006, using the spinlock instead
		spin_unlock_irqrestore(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif	//prochao-
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%d: after ifx_mpsdrv_mbx_send()\n", __LINE__);
#endif
	}
	else
	{
		//recieving cmd
	}
	return ret;
}



int ifx_mps_get_version(int cpu_id)
{
	int local_cpu_id;
	local_cpu_id = ifx_mpsdrv_get_cpu_id();

	if (local_cpu_id == cpu_id)
	{
		return ifx_mpsdrv_get_fw_version();
	}
	else
	{
		//ask mps_device to get remote version

		memset(&mps_cmd_control.msg_data, 0, sizeof(mps_cmd_control.msg_data));

		if (local_cpu_id == 0)
			ifx_cmd_process(0,  MPS_CMD_CPU0_GET_REMOTE_VER, 0);

		else if (local_cpu_id == 1)
			ifx_cmd_process(0,  MPS_CMD_CPU1_GET_REMOTE_VER, 0);
//prochao+
		cmd_wakeuplist_balance += 1;
		wait_event_interruptible( mps_cmd_control.cmd_wakeuplist, mps_cmd_control.cmd_callback_flag != 0);
		mps_cmd_control.cmd_callback_flag = 0;
//prochao-
		if (mps_cmd_control.msg_data.data_msg_content.payload)
			return mps_cmd_control.msg_data.data_msg_content.payload;
	}
	//should not reach here
	return 0; //if return 0, some error occurred
}

void flush_bklog_q()
{
#ifdef CONFIG_MPS_FLOW_CONTROL
        if (atomic_read(&bklog_pkts) != 0)
                ifx_mps_send_from_bklog_q();
#endif
        return;
}

#ifdef CONFIG_MPS_FLOW_CONTROL
static int ifx_mps_data_enqueue(u8 service_sequence_number, void* datapointer, u32 len)
{
        mps_data_list *mps_data_list_elem;
        mps_data_list_elem = kmalloc(sizeof(mps_data_list), GFP_ATOMIC);  
        if (mps_data_list_elem)
        {
                mps_data_list_elem->service_sequence_number = service_sequence_number;
                mps_data_list_elem->datapointer = datapointer;
                mps_data_list_elem->len = len;
                INIT_LIST_HEAD(&mps_data_list_elem->list);
                list_add_tail(&(mps_data_list_elem->list), &bklog_mps_data_list);
                //bklog_pkts++;
                atomic_inc(&bklog_pkts);
#ifdef KDEBUG
                if (atomic_read(&bklog_pkts) > count_max_bklog_q)
                        count_max_bklog_q = atomic_read(&bklog_pkts);
#endif
        }
        else
                return EFAIL;
        //printk("Enqueue into backlog queue bklog_pkts = %d\n", atomic_read(&bklog_pkts));
        return OK;
}
static int ifx_mps_send_from_bklog_q()
{
        struct list_head *curr, *next;
        mps_data_list * elem;

        if (!atomic_dec_and_test(&bklog_q_lock))
        {
                atomic_inc(&bklog_q_lock);
                return  EFAIL;
        }
        list_for_each_safe(curr, next, &bklog_mps_data_list)
        {
                elem = list_entry(curr, mps_data_list, list);
                if (ifx_mps_data_send2(elem->service_sequence_number, elem->datapointer, elem->len) == OK)
                {
                        //printk("Sent from Backlog queue\n");
                        list_del(curr);
                        kfree(elem);
                        atomic_dec(&bklog_pkts);
                        //bklog_pkts--;
#ifdef KDEBUG
                        count_bklog_pkts++;
#endif
                }
                else
				{
                	atomic_inc(&bklog_q_lock);
                        return OK;
				}
        }
        atomic_inc(&bklog_q_lock);
        return OK;
}

static void flush_bklog_timer_handler(unsigned long data)
{
        if (atomic_read(&bklog_pkts) != 0)
                ifx_mps_send_from_bklog_q();
        mod_timer(&flush_bklog_timer, jiffies + 1);
}
int ifx_mps_data_send(u8 service_sequence_number, void* datapointer, u32 len)
{
#ifdef KDEBUG
        mean_pkt_len = (mean_pkt_len + ((struct sk_buff*)datapointer)->len) >> 1;
#endif
        if (list_empty(&bklog_mps_data_list))
        {
                if (ifx_mps_data_send2(service_sequence_number, datapointer, len) == OK)
                        return OK;
        }
#ifdef KDEBUG
        count_fail_pkts++;
#endif
        if(ifx_mps_data_enqueue(service_sequence_number, datapointer, len) == EFAIL)
                return EFAIL;
        return ifx_mps_send_from_bklog_q();
}
#endif

#ifdef CONFIG_MPS_FLOW_CONTROL
static int ifx_mps_data_send2(u8 service_sequence_number, void* datapointer, u32 len)
#else
int ifx_mps_data_send(u8 service_sequence_number, void* datapointer, u32 len)
#endif
{
	int cpu_id;
	mps_data_message    rw ;//= (*)pmessage;
	int retval;
	unsigned long   lockflag;
	MPS_BUF *skb;

	memset(&rw, 0, sizeof(mps_data_message));
#if	1	//prochao+, 11/02/2006, using the spinlock instead
	spin_lock_irqsave(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif	//prochao-
	skb = datapointer;
#ifdef	_PRO_DEBUGGING_
	DPRINTK("(XXX) data len [%d]; sizeof(skb) [%d] \n", len, sizeof(struct sk_buff));
	DPRINTK("skb[0x%x] - list[0x%x], prev[0x%x], next[0x%x], destructor[0x%x]\n", skb, skb->list, skb->prev, skb->next, skb->destructor);
	DPRINTK("\t** len[%d], truesize[%d], head[0x%x], data[0x%x], tail[0x%x], end[0x%x]\n",
			skb->len, skb->truesize, skb->head, skb->data, skb->tail, skb->end);
#endif
//prochao+, 11/10/2006, for sending, need to flush cache back to the memory
	dma_cache_wback_inv((unsigned long) datapointer, len);
#ifdef	_PRO_DEBUGGING_
	DPRINTK("(XXX) After Cache flush !\n");
	DPRINTK("skb[0x%x] - list[0x%x], prev[0x%x], next[0x%x], destructor[0x%x]\n",
			skb, skb->list, skb->prev, skb->next, skb->destructor);
	DPRINTK("\t** len[%d], truesize[%d], head[0x%x], data[0x%x], tail[0x%x], end[0x%x]\n",
			skb->len, skb->truesize, skb->head, skb->data, skb->tail, skb->end);
#endif
//prochao-
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s(seq#=%d, dptr=%Xh, len=%d)\n", __LINE__, __FUNCTION__, service_sequence_number, (u32) datapointer, len);
#endif
	cpu_id = ifx_mpsdrv_get_cpu_id();
	//check which cpu
	if (cpu_id == MPS_CPU_0_ID)
	{
		rw.data_msg_header.msg_cmd = MPS_CMD_CPU0_DATAMSG;
		rw.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_DATA;
	}
	else	//must be CPU1
	{
		rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_DATAMSG;
		rw.data_msg_header.msg_type = MPS_TYPE_UPSTREAM_DATA;
		//MPS_BUF
#if	0	//prochao+, 12/13/2006, uses new way to identify the MPS buffer ownership without calling to set it
		mps_set_mps_buffer_identity((MPS_BUF* )datapointer, 1);
#endif	//prochao-, 12/13/2006
	}
	rw.data_msg_header.msg_seqno = service_sequence_number;
	rw.data_msg_header.msg_len = sizeof(mps_msg_buf);
	rw.data_msg_content.msg_body.msg_buf_addr= (u32) datapointer;
	rw.data_msg_content.msg_body.msg_buf_len = len;

	retval = ifx_mpsdrv_mbx_send((mps_message*) (&rw));
	//MPS_BUF
	if (cpu_id == MPS_CPU_0_ID)
	{
		ifx_cpu0_release_q_lists(service_sequence_number);
		ifx_cpu0_topup_q_lists(service_sequence_number);
	}
	else	//must be CPU1
	{

	}

#if	1	//prochao+, 11/02/2006, using the spinlock instead
	spin_unlock_irqrestore(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif	//prochao-
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: exit %s()\n", __LINE__, __FUNCTION__);
#endif
	if (retval!=OK)
		return EFAIL;

	return 0;
}


// return  1		another available message with the same sequence number
// return  0        OK, successful read operation,
// return  -1       ERROR, in case of read error.
int ifx_mps_data_rcv(u8 service_sequence_number, void* datapointer, u32* len)
{
	int result;
	mps_data_message *rw = (mps_data_message*) datapointer;
	unsigned long   lockflag;

#if	1	//prochao+, 11/02/2006, using the spinlock instead
	spin_lock_irqsave(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif	//prochao-
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s(seq#=%d, dptr=%Xh, len=%d)\n", __LINE__, __FUNCTION__,
		   service_sequence_number, (u32) datapointer, *len);
#endif
	memset(&rw, 0, sizeof(mps_data_message));
	rw->data_msg_header.msg_seqno = service_sequence_number;
	result = ifx_mpsdrv_mbx_rcv((mps_message *) rw, MPS_DATA_MSG_TYPE);	//NULL

#if	1	//prochao+, 11/02/2006, using the spinlock instead
	spin_unlock_irqrestore(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif	//prochao-
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: exit %s()\n", __LINE__, __FUNCTION__);
#endif
	return result;
}


int ifx_mps_datalist_send(u8 service_sequence_number, mps_datalist datapointer_list)
{
	int cpu_id;
	mps_data_message rw ;//= (*)pmessage;
	int retval;
	unsigned long   lockflag;

#if	1	//prochao+, 11/02/2006, using the spinlock instead
	spin_lock_irqsave(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif	//prochao-
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s(seq#=%d, dptr=%Xh)\n", __LINE__, __FUNCTION__, service_sequence_number, datapointer_list);
#endif
	memset(&rw, 0, sizeof(mps_data_message));
	rw.data_msg_header.msg_seqno = service_sequence_number;
	cpu_id = ifx_mpsdrv_get_cpu_id();

	//check which cpu
	if (cpu_id == MPS_CPU_0_ID)
	{
		rw.data_msg_header.msg_cmd = MPS_CMD_CPU0_DATALISTMSG;
		rw.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_DATA;
	}
	else
	{
		rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_DATALISTMSG;
		rw.data_msg_header.msg_type = MPS_TYPE_UPSTREAM_DATA;
	}
	rw.data_msg_header.msg_len = sizeof(mps_msg_buf);
	rw.data_msg_content.msg_body.msg_buf_addr = (u32)datapointer_list.datalist;
	rw.data_msg_content.msg_body.msg_buf_len= sizeof(datapointer_list.datalist)*datapointer_list.no;
//prochao+, 11/10/2006, for sending, need to flush cache back to the memory
	dma_cache_wback_inv(rw.data_msg_content.msg_body.msg_buf_addr, rw.data_msg_content.msg_body.msg_buf_len);
//prochao-
	retval = ifx_mpsdrv_mbx_send((mps_message*) (&rw));

#if	1	//prochao+, 11/02/2006, using the spinlock instead
	spin_unlock_irqrestore(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: exit %s()\n", __LINE__, __FUNCTION__);
#endif

	return retval;
}


int ifx_mps_datalist_rcv(u8 service_sequence_number, mps_datalist* pdatapointer_list)
{
	int result;
	mps_data_message *rw = (mps_data_message*) pdatapointer_list;

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s(seq#=%d, dptr=%Xh)\n", __LINE__, __FUNCTION__, service_sequence_number, (u32) pdatapointer_list);
#endif
	memset(rw, 0, sizeof(mps_data_message));
	rw->data_msg_header.msg_seqno = service_sequence_number;
	result = ifx_mpsdrv_mbx_rcv((mps_message *) rw, MPS_DATA_MSG_TYPE);	//NULL

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: exit %s()\n", __LINE__, __FUNCTION__);
#endif
	return result;
}


mps_datalist* ifx_mps_cpu0_datalist_alloc(int n)
{
	int i;
	mps_datalist* pdatapointer_list = kmalloc(sizeof(mps_datalist), GFP_KERNEL);
	struct data_list  *list_ptr, *r;

	//prochao! ?????? should do checking ??????
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s(n=%d), pdptr=%Xh\n", __LINE__, __FUNCTION__, n, (u32) pdatapointer_list);
#endif
	pdatapointer_list->datalist = kmalloc(sizeof(struct data_list), GFP_KERNEL);
	pdatapointer_list->datalist->data = kmalloc(MPS_DATA_SIZE, GFP_KERNEL);
	r=pdatapointer_list->datalist;

	for (i = 0; i < n; i++)
	{
		list_ptr = kmalloc(sizeof(struct data_list), GFP_KERNEL);
		list_ptr->data = kmalloc(MPS_DATA_SIZE, GFP_KERNEL);
		list_ptr->len = 0;

		r->next = list_ptr;
		r = list_ptr;
	}
	r->next = NULL;
	pdatapointer_list->no = n;
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: exit %s()\n", __LINE__, __FUNCTION__);
#endif

	return pdatapointer_list;
}



void ifx_mps_cpu0_datalist_free(mps_datalist* pdatapointer_list)
{
	int i;

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s(dptr=%Xh)\n", __LINE__, __FUNCTION__, (u32) pdatapointer_list);
#endif
	//prochao! should do checking ??????
	for (i = 0; i <pdatapointer_list->no; i++)
	{
		kfree((char*)pdatapointer_list->datalist->data);
		pdatapointer_list->datalist->len = 0;
		pdatapointer_list->datalist = pdatapointer_list->datalist->next;

	}
	pdatapointer_list->no = 0;
	kfree((char*)pdatapointer_list);
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: exit %s()\n", __LINE__, __FUNCTION__);
#endif

	return;
}


mps_datalist* ifx_mps_datalist_alloc(int N)
{
	int cpu_id;

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s(N=%d)\n", __LINE__, __FUNCTION__, N);
#endif
	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id == 0)
		return(ifx_mps_cpu0_datalist_alloc(N));
	else
	{
		//tell mps_device to call ifx_mps_cpu0_datalist_alloc
		memset(&mps_cmd_control.msg_data, 0, sizeof(mps_cmd_control.msg_data));
		ifx_cmd_process(0, MPS_CMD_CPU1_DATALIST_ALLOC, (u32) N); //0);	//prochao+-, 10/15/2006
//prochao+-
		cmd_wakeuplist_balance += 1;
		wait_event_interruptible( mps_cmd_control.cmd_wakeuplist, mps_cmd_control.cmd_callback_flag != 0);
		mps_cmd_control.cmd_callback_flag = 0;
//prochao-
		if (mps_cmd_control.msg_data.data_msg_content.msg_body.msg_buf_addr)
			return(void*)mps_cmd_control.msg_data.data_msg_content.msg_body.msg_buf_addr;
	}
	return NULL; //reach here because of something wrong
}

void ifx_mps_datalist_free(mps_datalist* pdatapointer_list)
{
	int cpu_id;
	cpu_id = ifx_mpsdrv_get_cpu_id();

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s(dptr=%Xh)\n", __LINE__, __FUNCTION__, (u32) pdatapointer_list);
#endif
	if (cpu_id == 0)
		ifx_mps_cpu0_datalist_free(pdatapointer_list);
	else
	{
		//tell mps_device to call ifx_mps_cpu0_datalist_free
		ifx_cmd_process(0, MPS_CMD_CPU1_DATALIST_FREE, (u32) pdatapointer_list); //0);	//prochao+-, 10/15/2006
	}
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: exit %s()\n", __LINE__, __FUNCTION__);
#endif
}

void* ifx_mps_cpu0_skbuff_alloc(u32 bufsize)
{
	struct sk_buff *skb;


	skb = alloc_skb(bufsize, GFP_ATOMIC);
	if (skb == NULL)
		return NULL;
	else
		return((void*)skb);


}

void ifx_mps_cpu0_skbuff_free(void* datapointer)
{
	kfree_skb(datapointer);
}


void* ifx_mps_skbuff_alloc(u32 bufsize)
{
	int     cpu_id, st;
//	unsigned long	lockflag;
	static unsigned long    rm_alloc_cnt = 0;

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n\n%d: enter %s(bufsize=%d)\n", __LINE__, __FUNCTION__, bufsize);
#endif
	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id == 0)
	{
		return(ifx_mps_cpu0_skbuff_alloc( bufsize));
	}
	else if (cpu_id==1)
	{
		memset(&mps_cmd_control.msg_data, 0, sizeof(mps_cmd_control.msg_data));
		st = ifx_cmd_process(0, MPS_CMD_CPU1_SKB_ALLOC, bufsize);
		rm_alloc_cnt += 1;
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "\n%d: %d! Wait event after ifx_cmd_process()=%d\n", __LINE__, (u32) rm_alloc_cnt, st);
#endif
//prochao+-
		cmd_wakeuplist_balance += 1;
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "\n%d: cmd_wakeuplist_balance=%d!\n", __LINE__, cmd_wakeuplist_balance);
#endif
		wait_event_interruptible( mps_cmd_control.cmd_wakeuplist, mps_cmd_control.cmd_callback_flag != 0);
		mps_cmd_control.cmd_callback_flag = 0;
//prochao-
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%d: exit %s() => %Xh\n", __LINE__, __FUNCTION__, mps_cmd_control.msg_data.data_msg_content.msg_body.msg_buf_addr);
#endif
		if (mps_cmd_control.msg_data.data_msg_content.msg_body.msg_buf_addr)
			return(void*)(mps_cmd_control.msg_data.data_msg_content.msg_body.msg_buf_addr);
	}
	return 0;
}


void ifx_mps_skbuff_free(void* datapointer)
{
	int     cpu_id, st;
	static unsigned long    rm_free_cnt = 0;

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "\n\n%d: enter %s(dptr=%Xh)\n", __LINE__, __FUNCTION__, (u32) datapointer);
#endif
	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id == 0)
	{
		return(ifx_mps_cpu0_skbuff_free(datapointer));
	}
	else if (cpu_id==1)
	{
		//tell mps_device to call ifx_mps_cpu0_datalist_free
		st = ifx_cmd_process(0, MPS_CMD_CPU1_SKB_FREE, (u32) datapointer); //prochao+-
		rm_free_cnt += 1;
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "\n%d: %s(%Xh) %d! after ifx_cmd_process()=%d\n", __LINE__, __FUNCTION__, (u32) datapointer, (u32) rm_free_cnt, st);
#endif
	}
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: exit %s()\n", __LINE__, __FUNCTION__);
#endif
}

//prochao+, 10/18/2006,
void    *ifx_mps_cpu0_buff_alloc(u32 bufsize)
{
	void *pbuf;

	pbuf = kmalloc(bufsize, GFP_ATOMIC);
	return pbuf;
}

//
void    ifx_mps_cpu0_buff_free(void* datapointer)
{
	kfree(datapointer);
}

//using the formal kmalloc() to allocate the required size of data buffer
void    *ifx_mps_app_buff_kmalloc(u32 bufsize)
{
	int cpu_id;

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s(bufsize=%d)\n", __LINE__, __FUNCTION__, bufsize);
#endif
	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id == MPS_CPU_0_ID)
	{
		return(ifx_mps_cpu0_buff_alloc( bufsize));
	}
	else if (cpu_id == MPS_CPU_1_ID)
	{
		memset(&mps_cmd_control.msg_data, 0, sizeof(mps_cmd_control.msg_data));

		ifx_cmd_process(0, MPS_CMD_CPU1_DATA_ALLOC, bufsize);
//prochao+-
		cmd_wakeuplist_balance += 1;
		wait_event_interruptible( mps_cmd_control.cmd_wakeuplist, mps_cmd_control.cmd_callback_flag != 0);
		mps_cmd_control.cmd_callback_flag = 0;
//prochao-
#ifdef	_PRO_DEBUGGING_
		printk(KERN_INFO "%d: exit %s() => %Xh\n", __LINE__, __FUNCTION__, mps_cmd_control.msg_data.data_msg_content.msg_body.msg_buf_addr);
#endif
		if (mps_cmd_control.msg_data.data_msg_content.msg_body.msg_buf_addr)
			return(void *)(mps_cmd_control.msg_data.data_msg_content.msg_body.msg_buf_addr);
	}
	return 0;
}

//using the formal kfree() to free the allocated buffer by above *_kmalloc()
void  ifx_mps_app_buff_kfree(void *pbuf)
{
	int cpu_id;

	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id == MPS_CPU_0_ID)
	{
		return(ifx_mps_cpu0_buff_free(pbuf));
	}
	else if (cpu_id == MPS_CPU_1_ID)
	{ //tell mps_device to call ifx_mps_cpu0_datalist_free
		ifx_cmd_process(0, MPS_CMD_CPU1_DATA_FREE, (u32) pbuf);
	}
}
//prochao-, 10/18/2006

#ifdef	CONFIG_DANUBE_CORE1
//prochao+, 11/08/2006
static void copy_skb_header(struct sk_buff *new, const struct sk_buff *old)
{
	/*
	 *	Shift between the two data areas in bytes
	 */
	unsigned long offset = new->data - old->data;

	new->list=NULL;
	new->sk=NULL;
	new->dev=old->dev;
	new->real_dev=old->real_dev;
	new->priority=old->priority;
	new->protocol=old->protocol;
	new->dst=dst_clone(old->dst);
	new->h.raw=old->h.raw+offset;
	new->nh.raw=old->nh.raw+offset;
	new->mac.raw=old->mac.raw+offset;
	memcpy(new->cb, old->cb, sizeof(old->cb));
	atomic_set(&new->users, 1);
	new->pkt_type=old->pkt_type;
	new->stamp=old->stamp;
	new->destructor = NULL;
	new->security=old->security;
#ifdef CONFIG_NETFILTER
	new->nfmark=old->nfmark;
	new->nfcache=old->nfcache;
	new->nfct=old->nfct;
	nf_conntrack_get(new->nfct);
#ifdef CONFIG_NETFILTER_DEBUG
	new->nf_debug=old->nf_debug;
#endif
//060620:henryhsu Move vlan code from Amazon to Danube
#if defined(CONFIG_IFX_NFEXT_VBRIDGE) || defined(CONFIG_IFX_NFEXT_VBRIDGE_MODULE)
	memcpy(new->ifxcb, old->ifxcb, sizeof(old->ifxcb));
#endif
#if defined(CONFIG_BRIDGE_NF_EBTABLES) || defined(CONFIG_BRIDGE_NF_EBTABLES_MODULE)
	new->nf_bridge=old->nf_bridge;
	nf_bridge_get(new->nf_bridge);
#endif
#endif
#ifdef CONFIG_NET_SCHED
	new->tc_index = old->tc_index;
#endif
}
//prochao-
//-------------------------------------------------------------------------------------------------------------------
// prochao+, 10/29/2006, adds the new APIs for sk_buff provisioning equivalent replacements
//#define	ifx_mps_dev_kfree_skb	ifx_mps_skbuff_free		//can use the available one
//

static void mps_skb_reserve(MPS_BUF *skb, unsigned int len)
{
	skb->data+=len;
	skb->tail+=len;
}

	#define MPS_SKB_LINEAR_ASSERT(skb) do { if (mps_skb_is_nonlinear(skb)) out_of_line_bug(); } while (0)

static inline int mps_skb_is_nonlinear(MPS_BUF *skb)
{
	return skb->data_len;
}


static unsigned char *mps_skb_put(MPS_BUF *skb, unsigned int len)
{
	unsigned char *tmp=skb->tail;
	SKB_LINEAR_ASSERT(skb);
	skb->tail+=len;
	skb->len+=len;
	if (skb->tail>skb->end)
	{
		printk("%s(%Xh, %d): mps_skb_put panic", __FUNCTION__, (u32) skb, len);
		BUG();
	}
	return tmp;
}



int32_t ifx_mps_dev_kfree_skb(MPS_BUF *buf, u8 service_seq_num)
{
	return ifx_mps_fast_free_mps_buf(buf, service_seq_num);
}

//replaced with Hsiren's one
	#if	1
//prochao+
MPS_BUF *ifx_mps_alloc_skb(unsigned int size, int priority, u8 service_seq_num)
{
	MPS_BUF *mps_buf;

	mps_buf = ifx_mps_fast_alloc_mps_buf(size, priority, service_seq_num);

	return mps_buf;
}
//prochao-
	#else
struct sk_buff *ifx_mps_alloc_skb(unsigned int size, int priority)
{
	int     cpu_id;
	mps_cmd_param_t cmd_param;

	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id == 0)
	{
		return(ifx_mps_cpu0_skbuff_alloc( size));
	}
	else if (cpu_id==1)
	{
		//
		memset(&mps_cmd_control.msg_data, 0, sizeof(mps_cmd_control.msg_data));
		// NEEDs to extend the ifx_cmd_process() to accept more parameters
		//prochao+, 11/05/2006,
		cmd_param.cmd_alloc_skb_param_size = size;
		cmd_param.cmd_alloc_skb_param_prio= priority;
		ifx_cmd_process(0, MPS_CMD_CPU1_IFX_ALLOC_SKB, (u32) &cmd_param);
		cmd_wakeuplist_balance += 1;
		wait_event_interruptible( mps_cmd_control.cmd_wakeuplist, mps_cmd_control.cmd_callback_flag != 0);
		mps_cmd_control.cmd_callback_flag = 0;
		//returned buffer address, if not NULL
		return(mps_cmd_control.msg_data.data_msg_content.skb);
	}
	return NULL;
}
	#endif

//replaced with Hsiren's one
	#if	1
//prochao+
MPS_BUF *ifx_mps_skb_clone(const MPS_BUF *skb, u8 seq_num)
{
	MPS_BUF *mps_buf;

	mps_buf = ifx_mps_skb_copy(skb, seq_num);

	return mps_buf;
}
//prochao-
	#else
struct sk_buff *ifx_mps_skb_clone(struct sk_buff *skb, int priority)
{
	mps_cmd_param_t cmd_param;

	memset(&mps_cmd_control.msg_data, 0, sizeof(mps_cmd_control.msg_data));
	// NEEDs to extend the ifx_cmd_process() to accept more parameters
	//prochao+, 11/05/2006,
	cmd_param.cmd_skb_clone_copy_param_skb = skb;
	cmd_param.cmd_skb_clone_copy_param_prio = priority;
	ifx_cmd_process(0, MPS_CMD_CPU1_IFX_SKB_CLONE, (u32) &cmd_param);
	cmd_wakeuplist_balance += 1;
	wait_event_interruptible( mps_cmd_control.cmd_wakeuplist, mps_cmd_control.cmd_callback_flag != 0);
	mps_cmd_control.cmd_callback_flag = 0;
	//returned buffer address, if not NULL
	return(mps_cmd_control.msg_data.data_msg_content.skb);
}
	#endif

//replaced with Hsiren's one
MPS_BUF *ifx_mps_skb_copy(const MPS_BUF *skb, u8 service_seq_num)
{
	MPS_BUF *n;
	int headerlen = skb->data-skb->head;

	/*
	 *	Allocate the copy buffer
	 */

	if (!ifx_is_mps_buffer((MPS_BUF *)skb))
		return skb_copy(skb,GFP_ATOMIC);
	n=ifx_mps_fast_alloc_mps_buf(skb->end - skb->head + skb->data_len, GFP_ATOMIC, service_seq_num);	//prochao+-
	if (n==NULL)
		return NULL;

	/* Set the data pointer */
	mps_skb_reserve(n,headerlen);
	/* Set the tail pointer and length */
	skb_put(n,skb->len);
	n->csum = skb->csum;
	n->ip_summed = skb->ip_summed;

	if (mps_skb_copy_bits(skb, -headerlen, n->head, headerlen+skb->len))
		BUG();

	copy_skb_header(n, skb);

	return n;
}

//replaced with Hsiren's one
	#if	1
int ifx_mps_pskb_expand_head(MPS_BUF *skb, int nhead, int ntail, u8 service_seq_num)
{
	MPS_BUF *n;
	int size = nhead + (skb->end - skb->head) + ntail;
	int headerlen = skb->data-skb->head;
	unsigned char   *head;

	/*
	 *	Allocate the copy buffer
	 */
	if (!ifx_is_mps_buffer(skb))
		return pskb_expand_head(skb,nhead,ntail,GFP_ATOMIC);
	n = ifx_mps_fast_alloc_mps_buf(size, GFP_ATOMIC, service_seq_num);	//prochao+-
	if (n == NULL)
		goto nodata;

	/* Set the data pointer */
	mps_skb_reserve(n,nhead);
	/* Set the tail pointer and length */
	skb_put(n,skb->len);
	n->csum = skb->csum;
	n->ip_summed = skb->ip_summed;


	if (mps_skb_copy_bits(skb, -headerlen, n->head, headerlen+skb->len))
		BUG();

	copy_skb_header(n, skb);

	n->cloned = 1;
	skb->cloned = 1;
	head = n->head;
	n->head = skb->head;
	skb->head = head;

	ifx_mps_fast_free_mps_buf(n,service_seq_num);

	return 0;

	nodata:
	return -ENOMEM;
}
	#else
int ifx_mps_pskb_expand_head(struct sk_buff *skb, int nhead, int ntail, int qfp_mask)
{
	mps_cmd_param_t cmd_param;

	memset(&mps_cmd_control.msg_data, 0, sizeof(mps_cmd_control.msg_data));
	// NEEDs to extend the ifx_cmd_process() to accept more parameters
	//prochao+, 11/05/2006,
	cmd_param.cmd_pskb_expand_head_param_skb = skb;
	cmd_param.cmd_pskb_expand_head_param_nhead = nhead;
	cmd_param.cmd_pskb_expand_head_param_ntail = ntail;
	cmd_param.cmd_pskb_expand_head_param_qfp_mask = qfp_mask;
	ifx_cmd_process(0, MPS_CMD_CPU1_IFX_PSKB_EXPAND_HEAD, (u32) &cmd_param);
	cmd_wakeuplist_balance += 1;
	wait_event_interruptible( mps_cmd_control.cmd_wakeuplist, mps_cmd_control.cmd_callback_flag != 0);
	mps_cmd_control.cmd_callback_flag = 0;
	//
	return mps_cmd_control.msg_data.data_msg_content.ret_int;
}
	#endif

//replaced with Hsiren's one
	#if 1
MPS_BUF * ifx_mps_skb_realloc_headroom(MPS_BUF *skb, unsigned int headroom, u8 service_seq_num)
{
	MPS_BUF *n;
	int delta = headroom - skb_headroom(skb);
	int size;
	int headerlen = skb->data-skb->head;

	/*
	 *	Allocate the copy buffer
	 */
	if (!ifx_is_mps_buffer(skb))
		return skb_realloc_headroom(skb,headroom);
	size = (delta > 0 ? delta : 0) + (skb->end - skb->head);
	n=ifx_mps_fast_alloc_mps_buf(size, GFP_ATOMIC, service_seq_num);	//prochao+-
	if (n==NULL)
		return NULL;

	/* Set the data pointer */
	if (delta)
		mps_skb_reserve(n,headroom);
	else
		mps_skb_reserve(n,skb_headroom(skb));
	/* Set the tail pointer and length */
	skb_put(n,skb->len);
	n->csum = skb->csum;
	n->ip_summed = skb->ip_summed;

	if (mps_skb_copy_bits(skb, -headerlen, n->head, headerlen+skb->len))
		BUG();

	copy_skb_header(n, skb);

	return n;
}
	#else
struct sk_buff *ifx_mps_skb_realloc_headroom(struct sk_buff *skb, unsigned int headroom)
{
	mps_cmd_param_t cmd_param;

	memset(&mps_cmd_control.msg_data, 0, sizeof(mps_cmd_control.msg_data));
	// NEEDs to extend the ifx_cmd_process() to accept more parameters
	//prochao+, 11/05/2006,
	cmd_param.cmd_skb_realloc_headroom_param_skb = skb;
	cmd_param.cmd_skb_realloc_headroom_param_headroom = headroom;
	ifx_cmd_process(0, MPS_CMD_CPU1_IFX_SKB_REALLOC_HEADROOM, (u32) &cmd_param);
	cmd_wakeuplist_balance += 1;
	wait_event_interruptible( mps_cmd_control.cmd_wakeuplist, mps_cmd_control.cmd_callback_flag != 0);
	mps_cmd_control.cmd_callback_flag = 0;
	//returned buffer address, if not NULL
	return(mps_cmd_control.msg_data.data_msg_content.skb);
}
	#endif

//replaced with Hsiren's one
	#if 1
MPS_BUF *ifx_mps_skb_copy_expand(const MPS_BUF *skb,
								 int newheadroom,
								 int newtailroom,
								 u8 service_seq_num)
{
	MPS_BUF *n;
//	int headerlen = skb->data-skb->head;

	/*
	 *	Allocate the copy buffer
	 */

	if (!ifx_is_mps_buffer(skb))
		return skb_copy_expand(skb,newheadroom,newtailroom,GFP_ATOMIC);
	n=ifx_mps_fast_alloc_mps_buf(newheadroom+newtailroom+skb->end - skb->head + skb->data_len, GFP_ATOMIC, service_seq_num);
	if (n==NULL)
		return NULL;

	mps_skb_reserve(n,newheadroom);
	skb_put(n,skb->len);

	/* Copy the linear data and header. */
	if (skb_copy_bits(skb, -newheadroom, n->head, newheadroom + skb->len))
		BUG();

	copy_skb_header(n, skb);
	return n;
}
	#else
struct sk_buff *ifx_mps_skb_copy_expand(const struct sk_buff *skb, int newheadroom, int newtailroom, int priority)
{
	mps_cmd_param_t cmd_param;

	memset(&mps_cmd_control.msg_data, 0, sizeof(mps_cmd_control.msg_data));
	// NEEDs to extend the ifx_cmd_process() to accept more parameters
	//prochao+, 11/05/2006,
	cmd_param.cmd_skb_copy_expand_param_skb = skb;
	cmd_param.cmd_skb_copy_expand_param_newheadroom = newheadroom;
	cmd_param.cmd_skb_copy_expand_param_newtailroom = newtailroom;
	cmd_param.cmd_skb_copy_expand_param_prio = priority;
	ifx_cmd_process(0, MPS_CMD_CPU1_IFX_SKB_COPY_EXPAND, (u32) &cmd_param);
	cmd_wakeuplist_balance += 1;
	wait_event_interruptible( mps_cmd_control.cmd_wakeuplist, mps_cmd_control.cmd_callback_flag != 0);
	mps_cmd_control.cmd_callback_flag = 0;
	//returned buffer address, if not NULL
	return(mps_cmd_control.msg_data.data_msg_content.skb);
}
	#endif

//replaced with Hsiren's one
	#if 1
static inline unsigned int mps_skb_headlen(const MPS_BUF *skb)
{
	return skb->len - skb->data_len;
}

int ___mps_pskb_trim(MPS_BUF *skb, unsigned int len, int realloc,u8 service_seq_num)
{
	int offset = mps_skb_headlen(skb);
//	int nfrags = skb_shinfo(skb)->nr_frags;
//	int i;

	if (skb_shinfo(skb)->nr_frags > 0)
		printk(KERN_INFO "%d, %s(%Xh): skb->nr_frags: %d", __LINE__, __FUNCTION__, (u32) skb, skb_shinfo(skb)->nr_frags);
#if 0
	for (i=0; i<nfrags; i++)
	{
		int end = offset + skb_shinfo(skb)->frags[i].size;
		if (end > len)
		{
			if (skb_cloned(skb))
			{
				if (!realloc)
					BUG();
				if (ifx_mps_pskb_expand_head(skb, 0, 0, service_seq_num))
					return -ENOMEM;
			}
			if (len <= offset)
			{
				/* ?????????? */
				put_page(skb_shinfo(skb)->frags[i].page);
				skb_shinfo(skb)->nr_frags--;
			}
			else
			{
				skb_shinfo(skb)->frags[i].size = len-offset;
			}
		}
		offset = end;
	}
#endif

	if (offset < len)
	{
		skb->data_len -= skb->len - len;
		skb->len = len;
	}
	else
	{
		if (len <= mps_skb_headlen(skb))
		{
			skb->len = len;
			skb->data_len = 0;
			skb->tail = skb->data + len;
			if (skb_shinfo(skb)->frag_list != NULL)
			{
				printk(KERN_INFO "___mps_pskb_trim frag_list NOT NULL");
			}
#if 0
			if (skb_shinfo(skb)->frag_list && !skb_cloned(skb))
				skb_drop_fraglist(skb);
#endif
		}
		else
		{
			skb->data_len -= skb->len - len;
			skb->len = len;
		}
	}

	return 0;
}

void __mps_skb_trim(MPS_BUF *skb, unsigned int len, u8 service_seq_num)
{
	if (!skb->data_len)
	{
		skb->len = len;
		skb->tail = skb->data+len;
	}
	else
	{
		___mps_pskb_trim(skb, len, 0, service_seq_num);
	}
}

void ifx_mps_skb_trim(MPS_BUF *skb, unsigned int len, u8 service_seq_num)
{
	if (skb->len > len)
	{
		__mps_skb_trim(skb, len,service_seq_num);
	}
}

	#else
//Following two are originally static inline functions
void ifx_mps_skb_trim(struct sk_buff *skb, unsigned int len)
{
	mps_cmd_param_t cmd_param;

	memset(&mps_cmd_control.msg_data, 0, sizeof(mps_cmd_control.msg_data));
	// NEEDs to extend the ifx_cmd_process() to accept more parameters
	//prochao+, 11/05/2006,
	cmd_param.cmd_skb_trim_param_skb = skb;
	cmd_param.cmd_skb_trim_param_len = len;
	ifx_cmd_process(0, MPS_CMD_CPU1_IFX_SKB_TRIM, (u32) &cmd_param);
	cmd_wakeuplist_balance += 1;
	wait_event_interruptible( mps_cmd_control.cmd_wakeuplist, mps_cmd_control.cmd_callback_flag != 0);
	mps_cmd_control.cmd_callback_flag = 0;
	return;
}
	#endif

//replaced with Hsiren's one
	#if 1
MPS_BUF *ifx_mps_dev_alloc_skb(unsigned int length, int priority, u8 service_seq_num)
{
	return ifx_mps_fast_alloc_mps_buf(length, priority, service_seq_num);
}
	#else
struct sk_buff *ifx_mps_dev_alloc_skb(unsigned int length, int priority)
{
	mps_cmd_param_t cmd_param;

	memset(&mps_cmd_control.msg_data, 0, sizeof(mps_cmd_control.msg_data));
	// NEEDs to extend the ifx_cmd_process() to accept more parameters
	//prochao+, 11/05/2006,
	cmd_param.cmd_dev_alloc_skb_param_len = length;
	cmd_param.cmd_dev_alloc_skb_param_prio = priority;
	ifx_cmd_process(0, MPS_CMD_CPU1_IFX_DEV_ALLOC_SKB, (u32) &cmd_param);
	cmd_wakeuplist_balance += 1;
	wait_event_interruptible( mps_cmd_control.cmd_wakeuplist, mps_cmd_control.cmd_callback_flag != 0);
	mps_cmd_control.cmd_callback_flag = 0;
	//returned buffer address, if not NULL
	return(mps_cmd_control.msg_data.data_msg_content.skb);
}
	#endif

//prochao+, added by using Hsiren's one
int mps_skb_copy_bits(const struct sk_buff *skb, int offset, void *to, int len)
{
	int copy;
	int start = skb->len - skb->data_len;

	if (offset > (int)skb->len-len)
		goto fault;

	/* Copy header. */
	if ((copy = start-offset) > 0)
	{
		if (copy > len)
			copy = len;
		memcpy(to, skb->data + offset, copy);
		if ((len -= copy) == 0)
			return 0;
		offset += copy;
		to += copy;
	}

	if (skb_shinfo(skb)->nr_frags > 0)
		printk("mps_skb_copy_bits nr_frags: %d", skb_shinfo(skb)->nr_frags);

	if (skb_shinfo(skb)->frag_list != NULL)
		printk("mps_skb_copy_bits frag_list NOT NULL");

	if (len == 0)
		return 0;

	fault:
	return -EFAULT;
}

// prochao-, 10/29/2006, adds new APIs for sk_buff
//-------------------------------------------------------------------------------------------------------------------
#endif	//CONFIG_DANUBE_CORE1

//
int ifx_mps_app_message_send(u8 service_sequence_number, void* pmessage, u32 len)
{
	int cpu_id;
	mps_data_message rw ;//= (*)pmessage;
	unsigned long   lockflag;

#if	1	//prochao+, 11/02/2006, using the spinlock instead
	spin_lock_irqsave(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif	//prochao-
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s(seq#=%d, pmsg=%Xh, len=%d)\n", __LINE__, __FUNCTION__, service_sequence_number, (u32) pmessage, len);
#endif
//prochao+, 11/10/2006, for sending, need to flush cache back to the memory
	dma_cache_wback_inv((unsigned long) pmessage, len);
//prochao-
	memset(&rw, 0, sizeof(mps_data_message));
	//check which cpu
	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id ==0)
		rw.data_msg_header.msg_cmd = MPS_CMD_CPU0_APPMSG;
	else if (cpu_id ==1)
		rw.data_msg_header.msg_cmd = MPS_CMD_CPU1_APPMSG;

	rw.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_APP_CMD;

	rw.data_msg_header.msg_seqno = service_sequence_number;
	rw.data_msg_header.msg_len = sizeof(mps_msg_buf);
	rw.data_msg_content.msg_body.msg_buf_addr = (u32) pmessage;
	rw.data_msg_content.msg_body.msg_buf_len = len;

	ifx_mpsdrv_mbx_send((mps_message*) (&rw));

#if	1	//prochao+, 11/02/2006, using the spinlock instead
	spin_unlock_irqrestore(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif	//prochao-
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: exit %s()\n", __LINE__, __FUNCTION__);
#endif

	return 0;
}

int ifx_mps_app_message_rcv(u8 service_sequence_number, void* pmessage, u32* len)
{
	mps_data_message *rw = (mps_data_message*) pmessage;

	rw->data_msg_header.msg_seqno = service_sequence_number;
	ifx_mpsdrv_mbx_rcv((mps_message *) rw, MPS_DATA_MSG_TYPE); //NULL

	return 0;
}

void ifx_mbx_data_rcv_callback(mps_message *pmsg)
{
//	mps_data_message *rw = kmalloc(sizeof(mps_data_message), GFP_KERNEL);
	mps_data_message    *rw;
//	int len;
//	unsigned long	lockflag;
	int cpu_id;
	MPS_BUF *skb;

	rw = (mps_data_message *) pmsg;
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s(pMsg=%X)\n", __LINE__, __FUNCTION__, (u32) rw);
#endif
	DPRINTK("[%s:%d] Seq#[%d], pMsg [0x%x], msg_buf_addr[0x%x], msg_buf_len[%d] \n", __FUNCTION__, __LINE__, pmsg->msg_seqno,
			pmsg, rw->data_msg_content.msg_body.msg_buf_addr, rw->data_msg_content.msg_body.msg_buf_len);
	cpu_id = ifx_mpsdrv_get_cpu_id();
//	len = rw->data_msg_header.msg_len;
	switch (rw->data_msg_header.msg_cmd)
	{
		case MPS_CMD_CPU0_DATAMSG:
		case MPS_CMD_CPU1_DATAMSG:
//prochao+, 11/10/2006, for rcving, need to invalidate the cache from the memory
			skb = (MPS_BUF *) rw->data_msg_content.msg_body.msg_buf_addr;

			DPRINTK("(XXX) msg_buf_len [%d]; sizeof(skb) [%d]\n", rw->data_msg_content.msg_body.msg_buf_len, sizeof(struct sk_buff));
			DPRINTK("skb[0x%x] - list[0x%x], prev[0x%x], next[0x%x], destructor[0x%x]\n", skb, skb->list, skb->prev, skb->next, skb->destructor);
			DPRINTK("\t** len[%d], truesize[%d], head[0x%x], data[0x%x], tail[0x%x], end[0x%x]\n", skb->len, skb->truesize, skb->head, skb->data, skb->tail, skb->end);
#if 1 /* XXX: dma_cache_inv - Ritesh */
			dma_cache_inv(rw->data_msg_content.msg_body.msg_buf_addr, rw->data_msg_content.msg_body.msg_buf_len);
#endif /* ] */
			DPRINTK("(XXX) after cache invalidate !\n");
			DPRINTK("skb[0x%x] - list[0x%x], prev[0x%x], next[0x%x], destructor[0x%x]\n", skb, skb->list, skb->prev, skb->next, skb->destructor);
			DPRINTK("\t** len[%d], truesize[%d], head[0x%x], data[0x%x], tail[0x%x], end[0x%x]\n", skb->len, skb->truesize, skb->head, skb->data, skb->tail, skb->end);
//prochao-
//MPS_BUF, prochao+, if here is the CPU1, the data packet must be from the CPU0, i.e. via MPS
#if	0	//prochao+, 12/13/2006, uses new way to identify the MPS buffer ownership without calling to set it
			if (cpu_id == MPS_CPU_1_ID)	//CPU1
			{
				mps_set_mps_buffer_identity((MPS_BUF* ) rw->data_msg_content.msg_body.msg_buf_addr, 0);
			}
#endif	//prochao-, 12/13/2006
#ifdef	_PRO_DEBUGGING_
			printk(KERN_INFO "%d: %s(): data_callback_fn(%Xh, %d)!\n", __LINE__, __FUNCTION__,
				   rw->data_msg_content.msg_body.msg_buf_addr, rw->data_msg_content.msg_body.msg_buf_len);
#endif
			data_callback_funct[rw->data_msg_header.msg_seqno]((mps_data_message *) rw->data_msg_content.msg_body.msg_buf_addr,
															   rw->data_msg_content.msg_body.msg_buf_len);
			//data_callback_funct[rw.data_msg_header.msg_seqno]();
#ifdef	_PRO_DEBUGGING_
			printk(KERN_INFO "%d: %s(): from data_callback_fn()!\n", __LINE__, __FUNCTION__);
#endif
//MPS_BUF
			if (cpu_id == MPS_CPU_0_ID)
			{
				ifx_cpu0_release_q_lists(rw->data_msg_header.msg_seqno);
				ifx_cpu0_topup_q_lists(rw->data_msg_header.msg_seqno);
			}
			break;

		case MPS_CMD_CPU0_APPMSG:
		case MPS_CMD_CPU1_APPMSG:
		case MPS_CMD_CPU0_DATALISTMSG:
		case MPS_CMD_CPU1_DATALISTMSG:
//prochao+, 11/10/2006, for rcving, need to invalidate the cache from the memory
			dma_cache_inv(rw->data_msg_content.msg_body.msg_buf_addr, rw->data_msg_content.msg_body.msg_buf_len);
//prochao-
			data_callback_funct[rw->data_msg_header.msg_seqno]((mps_data_message *) rw->data_msg_content.msg_body.msg_buf_addr,
															   rw->data_msg_content.msg_body.msg_buf_len);
			//data_callback_funct[rw.data_msg_header.msg_seqno]();
#ifdef	_PRO_DEBUGGING_
			printk(KERN_INFO "%d: %s(): from data_callback_fn()!\n", __LINE__, __FUNCTION__);
#endif
			break;
		default:
			printk(KERN_INFO "unkonwn mps msg\n");
			break;
	}
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "Exiting [%s]\n", __FUNCTION__);
#endif
	//prochao+
//	kfree(rw);
	//prochao-
}


void ifx_mps_cmd_rcv_callback(mps_data_message *pRcvMsg)
{
	mps_data_message    *pmsg, rw_response;
	unsigned long   lockflag;

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: enter %s()\n", __LINE__, __FUNCTION__);
#endif
	pmsg = (mps_data_message *) pRcvMsg;
//	memset((mps_message*)&rw, 0, sizeof(rw_response));
	memset((mps_message*)&rw_response, 0, sizeof(rw_response));
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: %s(), msg_cmd:%xH!\n", __LINE__, __FUNCTION__, pmsg->data_msg_header.msg_cmd);
#endif
	if (pmsg->data_msg_header.msg_cmd > 0x1000)
	{	//response case
		switch (pmsg->data_msg_header.msg_cmd - 0x1000)
		{
			//			case MPS_CMD_CPU0_DATALIST_ALLOC:	//commented out by prochao
			case MPS_CMD_CPU1_DATALIST_ALLOC: //prochao+
			case MPS_CMD_CPU1_SKB_ALLOC: //prochao+, 10/15/2006, should be the MPS_CMD_CPU1_SKB_ALLOC command
				//prochao+, 10/18/2006,
			case MPS_CMD_CPU1_DATA_ALLOC:
				//prochao-
//				mps_cmd_control.msg_data = rw;
				memcpy( &mps_cmd_control.msg_data, pmsg, sizeof(mps_data_message));
				//prochao+, 10/27/2006, wakeup the waiting process
				mps_cmd_control.cmd_callback_flag = 1;
				//prochao-
				cmd_wakeuplist_balance -= 1;
				wake_up_interruptible(&(mps_cmd_control.cmd_wakeuplist));
				break;
			case MPS_CMD_CPU0_GET_REMOTE_VER:
			case MPS_CMD_CPU1_GET_REMOTE_VER:
//				mps_cmd_control.msg_data = rw;
				memcpy( &mps_cmd_control.msg_data, pmsg, sizeof(mps_data_message));
				//prochao+, 10/27/2006, wakeup the waiting process
				mps_cmd_control.cmd_callback_flag = 1;
				//prochao-
				cmd_wakeuplist_balance -= 1;
				wake_up_interruptible(&(mps_cmd_control.cmd_wakeuplist));
				break;
				//prochao+, 11/06/2206, for commands to support addtional MPS driver APIs, ??????
			case MPS_CMD_CPU1_IFX_ALLOC_SKB:
			case MPS_CMD_CPU1_IFX_SKB_CLONE:
			case MPS_CMD_CPU1_IFX_SKB_COPY:
			case MPS_CMD_CPU1_IFX_PSKB_EXPAND_HEAD:
			case MPS_CMD_CPU1_IFX_SKB_REALLOC_HEADROOM:
			case MPS_CMD_CPU1_IFX_SKB_COPY_EXPAND:
			case MPS_CMD_CPU1_IFX_SKB_TRIM:
			case MPS_CMD_CPU1_IFX_DEV_ALLOC_SKB:
//				mps_cmd_control.msg_data = rw;
				memcpy( &mps_cmd_control.msg_data, pmsg, sizeof(mps_data_message));
				mps_cmd_control.cmd_callback_flag = 1;
				cmd_wakeuplist_balance -= 1;
				wake_up_interruptible(&(mps_cmd_control.cmd_wakeuplist));
				break;
				//prochao-
			default:
				printk(KERN_INFO "%s unknown MPS response %Xh!!!\n", __FUNCTION__, pmsg->data_msg_header.msg_cmd);
				break;
		}
	}
	else
	{	//request case
		int     get_valid_response = 1;
		int             retval;
		u32             bufsize;
		struct sk_buff  *skb;

		switch (pmsg->data_msg_header.msg_cmd)
		{
			//case MPS_CMD_CPU0_DATALIST_ALLOC:	//commented out by prochao
			case MPS_CMD_CPU1_DATALIST_ALLOC: //prochao+
				rw_response.data_msg_content.msg_body.msg_buf_addr =
				(u32) ifx_mps_cpu0_datalist_alloc((int) pmsg->data_msg_content.payload);
				//??? the size of this datalist ???
				rw_response.data_msg_header.msg_len = sizeof(mps_msg_buf);
				rw_response.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				break;

			//case MPS_CMD_CPU0_DATALIST_FREE:	//commented out by prochao
			case MPS_CMD_CPU1_DATALIST_FREE: //prochao+
				ifx_mps_cpu0_datalist_free((mps_datalist *) pmsg->data_msg_content.payload);
				break;

			//case MPS_CMD_CPU0_SKB_ALLOC:	//prochao+, 10/15/2006, should be no this here, might be typo by Lance?
			case MPS_CMD_CPU1_SKB_ALLOC: //prochao+
				bufsize = pmsg->data_msg_content.payload;
				// prochao-
				rw_response.data_msg_content.msg_body.msg_buf_addr = (u32) ifx_mps_cpu0_skbuff_alloc( bufsize);
				rw_response.data_msg_content.msg_body.msg_buf_len = bufsize;	//prochao+
				rw_response.data_msg_header.msg_len = sizeof(mps_msg_buf);
				rw_response.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				break;

			case MPS_CMD_CPU1_SKB_FREE:
				//prochao+, 10/15/2006
				ifx_mps_cpu0_skbuff_free((void *) pmsg->data_msg_content.payload); // the skbuff to be free
				//prochao-
				break;
				//prochao+, 10/18/2006, supports the normal kernel data buffer kmalloc/kfree
			case MPS_CMD_CPU1_DATA_ALLOC:
				bufsize = pmsg->data_msg_content.payload;
				rw_response.data_msg_content.msg_body.msg_buf_addr = (u32) ifx_mps_cpu0_buff_alloc( bufsize);
				rw_response.data_msg_content.msg_body.msg_buf_len = bufsize;	//prochao+
				rw_response.data_msg_header.msg_len = sizeof(mps_msg_buf);
				rw_response.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				break;
			case MPS_CMD_CPU1_DATA_FREE:
				ifx_mps_cpu0_buff_free((void *) pmsg->data_msg_content.payload); // the allocated kernel databuf to be free
				break;
				//prochao-
			case MPS_CMD_CPU0_GET_REMOTE_VER:
			case MPS_CMD_CPU1_GET_REMOTE_VER:
				if (pmsg->data_msg_header.msg_cmd == MPS_CMD_CPU0_GET_REMOTE_VER)
					rw_response.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				else
					rw_response.data_msg_header.msg_type = MPS_TYPE_UPSTREAM_CMD;

				rw_response.data_msg_content.payload= ifx_mpsdrv_get_fw_version();
				rw_response.data_msg_header.msg_len = sizeof(u32);
				break;
				//prochao+, 11/05/2206, for commands to support addtional MPS driver APIs
			case MPS_CMD_CPU1_IFX_ALLOC_SKB:
				rw_response.data_msg_content.skb
				= alloc_skb(pmsg->data_msg_content.cmd_parameters.cmd_alloc_skb_param_size,
							pmsg->data_msg_content.cmd_parameters.cmd_alloc_skb_param_prio);
				//
				rw_response.data_msg_header.msg_len = 4;	//sizeof(struct sk_buff *);
				rw_response.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				break;
			case MPS_CMD_CPU1_IFX_SKB_CLONE:
				skb = skb_clone(pmsg->data_msg_content.cmd_parameters.cmd_skb_clone_copy_param_skb,
								pmsg->data_msg_content.cmd_parameters.cmd_skb_clone_copy_param_prio);
				rw_response.data_msg_content.skb = skb;
				//
				rw_response.data_msg_header.msg_len = 4;
				rw_response.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				break;
			case MPS_CMD_CPU1_IFX_SKB_COPY:
				skb = skb_copy( pmsg->data_msg_content.cmd_parameters.cmd_skb_clone_copy_param_skb,
								pmsg->data_msg_content.cmd_parameters.cmd_skb_clone_copy_param_prio);
				rw_response.data_msg_content.skb = skb;
				//
				rw_response.data_msg_header.msg_len = 4;
				rw_response.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				break;
			case MPS_CMD_CPU1_IFX_PSKB_EXPAND_HEAD:
				retval = pskb_expand_head(pmsg->data_msg_content.cmd_parameters.cmd_pskb_expand_head_param_skb,
										  pmsg->data_msg_content.cmd_parameters.cmd_pskb_expand_head_param_nhead,
										  pmsg->data_msg_content.cmd_parameters.cmd_pskb_expand_head_param_ntail,
										  pmsg->data_msg_content.cmd_parameters.cmd_pskb_expand_head_param_qfp_mask);
				rw_response.data_msg_content.ret_int = retval;
				//
				rw_response.data_msg_header.msg_len = sizeof(int);
				rw_response.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				break;
			case MPS_CMD_CPU1_IFX_SKB_REALLOC_HEADROOM:
				skb = skb_realloc_headroom(pmsg->data_msg_content.cmd_parameters.cmd_skb_realloc_headroom_param_skb,
										   pmsg->data_msg_content.cmd_parameters.cmd_skb_realloc_headroom_param_headroom);
				rw_response.data_msg_content.skb = skb;
				//
				rw_response.data_msg_header.msg_len = 4;
				rw_response.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				break;
			case MPS_CMD_CPU1_IFX_SKB_COPY_EXPAND:
				skb = skb_copy_expand( pmsg->data_msg_content.cmd_parameters.cmd_skb_copy_expand_param_skb,
									   pmsg->data_msg_content.cmd_parameters.cmd_skb_copy_expand_param_newheadroom,
									   pmsg->data_msg_content.cmd_parameters.cmd_skb_copy_expand_param_newtailroom,
									   pmsg->data_msg_content.cmd_parameters.cmd_skb_copy_expand_param_prio);
				rw_response.data_msg_content.skb = skb;
				//
				rw_response.data_msg_header.msg_len = 4;
				rw_response.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				break;
			case MPS_CMD_CPU1_IFX_DEV_ALLOC_SKB:
				skb = alloc_skb(pmsg->data_msg_content.cmd_parameters.cmd_dev_alloc_skb_param_len + 16,
								pmsg->data_msg_content.cmd_parameters.cmd_dev_alloc_skb_param_prio);
				if (skb)
				{	//?????? followings should be inline!
					skb_reserve(skb, 16);	//this is running on the CPU0
				}
				rw_response.data_msg_content.skb = skb;
				//
				rw_response.data_msg_header.msg_len = 4;
				rw_response.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				break;
			case MPS_CMD_CPU1_IFX_SKB_TRIM:	//?????? how to perform this one without the realloc parameter?
				skb = pmsg->data_msg_content.cmd_parameters.cmd_skb_trim_param_skb;
				bufsize = pmsg->data_msg_content.cmd_parameters.cmd_skb_trim_param_len;
				__skb_trim(skb, bufsize);
				//
				rw_response.data_msg_header.msg_len = 0;
				rw_response.data_msg_header.msg_type = MPS_TYPE_DOWNSTREAM_CMD;
				break;
				//prochao-
			default:
				get_valid_response = 0;
				printk(KERN_INFO "%s - received unknown mps cmd (%x)\n", __FUNCTION__, pmsg->data_msg_header.msg_cmd);
				break;
		}
#if	1	//prochao+, 11/02/2006, using the spinlock instead
		spin_lock_irqsave(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif	//prochao-
		rw_response.data_msg_header.msg_seqno = pmsg->data_msg_header.msg_seqno;
		rw_response.data_msg_header.msg_cmd = pmsg->data_msg_header.msg_cmd + 0x1000;
		if (get_valid_response)
		{
#ifdef	_PRO_DEBUGGING_
			printk(KERN_INFO "%d: reply by ifx_mpsdrv_mbx_send() - seq#=%d, cmd=%Xh\n", __LINE__,
				   rw_response.data_msg_header.msg_seqno, rw_response.data_msg_header.msg_cmd);
#endif
			ifx_mpsdrv_mbx_send((mps_message *) &rw_response);
		}
		//
#if	1	//prochao+, 11/02/2006, using the spinlock instead
		spin_unlock_irqrestore(&mps_cmd_control.cmd_seq_semaphore_spinlock, lockflag);
#endif	//prochao-
	}
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%d: exit %s()\n", __LINE__, __FUNCTION__);
#endif
	return;
}


/**
 * Open MPS device.
 * Open the device from user mode (e.g. application) or kernel mode. An inode
 * value of 1..5 indicates a kernel mode access. In such a case the inode value
 * is used as minor ID.
 *
 * \param   inode   Pointer to device inode
 * \param   file_p  Pointer to file descriptor
 * \return  0       OK, device opened
 * \return  EMFILE  Device already open
 * \return  EINVAL  Invalid minor ID
 * \ingroup API
 */
s32 ifx_mps_open(struct inode *inode, struct file *file_p)
{
	mps_comm_dev   *pDev = &ifx_mps_dev;
	mps_mbx_dev    *pMBDev;
	bool_t         bcommand = FALSE;
	int            from_kernel = 0;
	mps_devices        num;

#ifdef CONFIG_DANUBE_MPS_PROC_DEBUG
	printk(KERN_INFO "%s(): is excuted!\n", __FUNCTION__);
#endif
	/* Check whether called from user or kernel mode */
	if ( (inode == (struct inode *)1) || (inode == (struct inode *)2)
		 || (inode == (struct inode *)3) || (inode == (struct inode *)4)
		 || (inode == (struct inode *)5) )
	{
		from_kernel = 1;
		num = (int)inode;
	}
	else
	{
		num = (mps_devices)MINOR(inode->i_rdev);  /* the real device */
	}

#ifdef CONFIG_DANUBE_MPS_PROC_DEBUG
	printk(KERN_INFO "ifx_mps_open.....\n");
#endif
	/* check the device number */
	switch (num)
	{
		case command:
			pMBDev = &(pDev->command_mbx);
			bcommand = TRUE;
			break;
		case wifi0:
			pMBDev = &(pDev->wifi_mbx[0]);
			break;
		case wifi1:
			pMBDev = &pDev->wifi_mbx[1];
			break;
		case wifi2:
			pMBDev = &pDev->wifi_mbx[2];
			break;
		case wifi3:
			pMBDev = &pDev->wifi_mbx[3];
			break;
		default:
			printk(KERN_INFO "IFX_MPS ERROR: max. device number exceed!\n");
			return -EINVAL;
	}

	if ((OK) == ifx_mpsdrv_common_open(pMBDev, bcommand))
	{
		if (!from_kernel)
		{
			/* installation was successfull */
			/* and use filp->private_data to point to the device data */
			file_p->private_data = pMBDev;
#ifdef MODULE
			/* increment module use counter */
			MOD_INC_USE_COUNT;
#endif

		}
		return 0;
	}
	else
	{
		/* installation failed */
		printk(KERN_INFO "IFX_MPS ERROR: Device is already open!\n");
		return -EMFILE;
	}

	return 0;
}

/**
 * Close MPS device.
 * Close the device from user mode (e.g. application) or kernel mode. An inode
 * value of 1..5 indicates a kernel mode access. In such a case the inode value
 * is used as minor ID.
 *
 * \param   inode   Pointer to device inode
 * \param   filp    Pointer to file descriptor
 * \return  0       OK, device closed
 * \return  ENODEV  Device invalid
 * \return  EINVAL  Invalid minor ID
 * \ingroup API
 */
s32 ifx_mps_close(struct inode *inode, struct file *filp)
{
	mps_mbx_dev *pMBDev;
	int            from_kernel = 0;

#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "%s(): is excuted !\n", __FUNCTION__);
#endif
	/* Check whether called from user or kernel mode */
	if ((inode == (struct inode *)1) || (inode == (struct inode *)2) ||
		(inode == (struct inode *)3) || (inode == (struct inode *)4) ||
		(inode == (struct inode *)5) )
	{
		from_kernel = 1;
		switch ((int)inode)
		{
			case command:
				pMBDev = &ifx_mps_dev.command_mbx;
				break;
			case wifi0:
				pMBDev = &ifx_mps_dev.wifi_mbx[0];
				break;
			case wifi1:
				pMBDev = &ifx_mps_dev.wifi_mbx[1];
				break;
			case wifi2:
				pMBDev = &ifx_mps_dev.wifi_mbx[2];
				break;
			case wifi3:
				pMBDev = &ifx_mps_dev.wifi_mbx[3];
				break;
			default:
				return(-EINVAL);
		}
	}
	else
	{
		pMBDev = filp->private_data;
	}

	if (NULL != pMBDev)
	{
		/* device is still available */
		ifx_mpsdrv_common_close(pMBDev);

#ifdef MODULE
		if (!from_kernel)
		{
			/* increment module use counter */
			MOD_DEC_USE_COUNT;
		}
#endif
		return 0;
	}
	else
	{
		/* something went totally wrong */
		printk("IFX_MPS ERROR: pMBDev pointer is NULL!\n");
		return -ENODEV;
	}

	return 0;
}

/**
 * Poll handler.
 * The select function of the driver. A user space program may sleep until
 * the driver wakes it up.
 *
 * \param   file_p  File structure of device
 * \param   wait    Internal table of poll wait queues
 * \return  mask    If new data is available the POLLPRI bit is set,
 *                  triggering an exception indication. If the device pointer
 *                  is null POLLERR is set.
 * \ingroup API
 */
#if 0
static u32 ifx_mps_poll(struct file *file_p, poll_table *wait)
{
	mps_mbx_dev    *pMBDev = file_p->private_data;
	unsigned int   mask;

	/* add to poll queue */
	poll_wait(file_p, &(pMBDev->mps_wakeuplist), wait);

	mask = 0;

	/* upstream queue */
	if (*pMBDev->upstrm_fifo.pwrite_off != *pMBDev->upstrm_fifo.pread_off)
	{
		if ((pMBDev->devID == command) || (sem_getcount(pMBDev->wakeup_pending)==0))
		{
			/* queue is not empty */
			mask = POLLIN | POLLRDNORM;
		}
	}

	/* downstream queue */
	if (ifx_mps_fifo_mem_available(&pMBDev->dwstrm_fifo) != 0)
	{
		/* queue is not full */
		mask |= POLLOUT | POLLWRNORM;
	}
	if (   (ifx_mps_dev.event.MPS_Ad0Reg.val & pMBDev->event_mask.MPS_Ad0Reg.val)
		   || (ifx_mps_dev.event.MPS_Ad1Reg.val & pMBDev->event_mask.MPS_Ad1Reg.val)
		   || (ifx_mps_dev.event.MPS_VCStatReg[0].val & pMBDev->event_mask.MPS_VCStatReg[0].val)
		   || (ifx_mps_dev.event.MPS_VCStatReg[1].val & pMBDev->event_mask.MPS_VCStatReg[1].val)
		   || (ifx_mps_dev.event.MPS_VCStatReg[2].val & pMBDev->event_mask.MPS_VCStatReg[2].val)
		   || (ifx_mps_dev.event.MPS_VCStatReg[3].val & pMBDev->event_mask.MPS_VCStatReg[3].val))
	{
		mask |= POLLPRI;
	}
	return mask;
}
#endif

/**
 * MPS IOCTL handler.
 * An inode value of 1..5 indicates a kernel mode access. In such a case the
 * inode value is used as minor ID.
 * The following IOCTLs are supported for the MPS device.
 * - #FIO_MPS_EVENT_REG
 * - #FIO_MPS_EVENT_UNREG
 * - #FIO_MPS_MB_READ
 * - #FIO_MPS_MB_WRITE
 * - #FIO_MPS_DOWNLOAD
 * - #FIO_MPS_GETVERSION
 * - #FIO_MPS_MB_RST_QUEUE
 * - #FIO_MPS_RESET
 * - #FIO_MPS_RESTART
 * - #FIO_MPS_GET_STATUS
 *
 * If MPS_FIFO_BLOCKING_WRITE is defined the following commands are also
 * available.
 * - #FIO_MPS_TXFIFO_SET
 * - #FIO_MPS_TXFIFO_GET
 *
 * \param   inode        Inode of device
 * \param   file_p       File structure of device
 * \param   nCmd         IOCTL command
 * \param   arg          Argument for some IOCTL commands
 * \return  0            Setting the LED bits was successfull
 * \return  -EINVAL      Invalid minor ID
 * \return  -ENOIOCTLCMD Invalid command
 * \ingroup API
 */
s32 ifx_mps_ioctl(struct inode *inode, struct file *file_p,
				  u32 nCmd, unsigned long arg)
{
	int             retvalue = -EINVAL;
//	mps_message     rw_struct;
	mps_mbx_dev     *pMBDev;
	int             from_kernel = 0;

	if ((inode == (struct inode *)1) || (inode == (struct inode *)2) ||
		(inode == (struct inode *)3) || (inode == (struct inode *)4) ||
		(inode == (struct inode *)5) )
	{
		from_kernel = 1;
		switch ((int)inode)
		{
			case command:
				pMBDev = &ifx_mps_dev.command_mbx;
				break;
			case wifi0:
				pMBDev = &ifx_mps_dev.wifi_mbx[0];
				break;
			case wifi1:
				pMBDev = &ifx_mps_dev.wifi_mbx[1];
				break;
			case wifi2:
				pMBDev = &ifx_mps_dev.wifi_mbx[2];
				break;
			case wifi3:
				pMBDev = &ifx_mps_dev.wifi_mbx[3];
				break;
			default:
				return(-EINVAL);
		}
	}
	else
	{
		pMBDev = file_p->private_data;
	}

	switch (nCmd)
	{

#if 0
		case FIO_MPS_REG:
			{
				MbxEventRegs_s events;
				copy_from_user((char*)&events, (char*)arg, sizeof(MbxEventRegs_s));
				retvalue = ifx_mps_register_event_callback(pMBDev->devID, &events, NULL);
				if (retvalue == OK)
				{
					retvalue = ifx_mps_event_activation(pMBDev->devID, &events);
				}
				break;
			}
		case FIO_MPS_EVENT_UNREG:
			{
				MbxEventRegs_s events;
				events.MPS_Ad0Reg.val = 0;
				events.MPS_Ad1Reg.val = 0;
				events.MPS_VCStatReg[0].val = 0;
				events.MPS_VCStatReg[1].val = 0;
				events.MPS_VCStatReg[2].val = 0;
				events.MPS_VCStatReg[3].val = 0;
				ifx_mps_event_activation(pMBDev->devID, &events);
				retvalue = ifx_mps_unregister_event_callback(pMBDev->devID);
				break;
			}

		case FIO_MPS_MB_READ:
#ifdef	_PRO_DEBUGGING_
			/* Read the data from mailbox stored in local FIFO */
			printk(KERN_INFO KERN_DEBUG "IFX_MPS: ioctl MB read\n");
#endif
			if (from_kernel)
			{
				retvalue = ifx_mpsdrv_mbx_read(pMBDev,(mps_message*)arg, 0);
			}
			else
			{
				u32 *pUserBuf;
				/* Initialize destination and copy mps_message from usermode */
				memset(&rw_struct, 0, sizeof(mps_message));
				copy_from_user((char*)&rw_struct, (char*)arg, sizeof(mps_message));

				pUserBuf = (u32*)rw_struct.pData;  /* Remember usermode buffer */

				/* read data from from upstream mailbox FIFO */
				retvalue = ifx_mpsdrv_mbx_read(pMBDev,&rw_struct, 0);
				if (retvalue)
					return -ENOMSG;

				/* Copy data to usermode buffer... */
				copy_to_user((char*)pUserBuf, (char*)rw_struct.pData, rw_struct.nDataBytes);
				ifx_mpsdrv_bufman_free(rw_struct.pData);

				/* ... and finally restore the buffer pointer and copy mps_message back! */
				rw_struct.pData = (u8*)pUserBuf;
				copy_to_user((char*)arg, (char*)&rw_struct, sizeof(mps_message));
			}
			break;

		case FIO_MPS_MB_WRITE:
#ifdef	_PRO_DEBUGGING_
			/* Write data to send to the mailbox into the local FIFO */
			printk(KERN_DEBUG "IFX_MPS: ioctl MB write\n");
#endif
			if (from_kernel)
			{
				if (pMBDev->devID == command)
				{
					return( ifx_mpsdrv_mbx_write_cmd(pMBDev, (mps_message*)arg) );
				}
				else
				{
					return( ifx_mpsdrv_mbx_write_data( pMBDev, (mps_message*)arg) );
				}
			}
			else
			{
				u32 *pUserBuf;
				copy_from_user((char*)&rw_struct, (char*)arg, sizeof(mps_message));

				/* Remember usermode buffer */
				pUserBuf = (u32*)rw_struct.pData;
				/* Allocate kernelmode buffer for writing data */
				rw_struct.pData = ifx_mpsdrv_bufman_malloc(rw_struct.nDataBytes, GFP_KERNEL);
				if (rw_struct.pData == NULL)
				{
					return(-ENOMEM);
				}

				/* copy data to kernelmode buffer and write to mailbox FIFO */
				copy_from_user((char*)rw_struct.pData, (char*)pUserBuf, rw_struct.nDataBytes);

				if (pMBDev->devID == command)
				{
					retvalue = ifx_mpsdrv_mbx_write_cmd(pMBDev, &rw_struct);
					ifx_mpsdrv_bufman_free(rw_struct.pData);
				}
				else
				{
					retvalue = ifx_mpsdrv_mbx_write_data(pMBDev, &rw_struct);
				}
				/* ... and finally restore the buffer pointer and copy mps_message back! */
				rw_struct.pData = (u8*)pUserBuf;
				copy_to_user((char*)arg, (char*)&rw_struct, sizeof(mps_message));
			}
			break;

		case FIO_MPS_DOWNLOAD:
			/* Download firmware file */
			ifx_mps_init_gpt_danube();
			if (pMBDev->devID == command)
			{
				if (from_kernel)
				{
					printk(KERN_INFO "IFX_MPS: Download firmware (size %d bytes)... ", ((mps_fw*)arg)->length);

					retvalue = ifx_mps_download_firmware(pMBDev,(mps_fw*)arg);
				}
				else
				{
					u32 *pUserBuffer;
					mps_fw dwnld_struct;

					copy_from_user((char*)&dwnld_struct, (char*)arg, sizeof(mps_fw));
					printk(KERN_INFO "IFX_MPS: Download firmware (size %d bytes)... ", dwnld_struct.length);
					pUserBuffer = dwnld_struct.data;
					dwnld_struct.data = vmalloc(dwnld_struct.length);
					retvalue = copy_from_user((char*)dwnld_struct.data,(char*)pUserBuffer, dwnld_struct.length);
					retvalue = ifx_mps_download_firmware(pMBDev, &dwnld_struct);
					vfree(dwnld_struct.data);
				}
				if (retvalue != 0)
				{
					printk(" error (%i)!\n", retvalue);
				}
				else
				{
					retvalue=ifx_mps_print_fw_version();
					ifx_mps_bufman_init();
				}
			}
			else
			{
				retvalue = -EINVAL;
			}
			break;
#endif

		case FIO_MPS_GETVERSION:
			if (from_kernel)
			{
				memcpy((char*)arg, (char*)ifx_mps_device_version, strlen(ifx_mps_device_version));
			}
			else
			{
				copy_to_user((char*)arg, (char*)ifx_mps_device_version, strlen(ifx_mps_device_version));
			}
			retvalue = OK;
			break;

		case  FIO_MPS_RESET:
			/* Reset of the DSP */
			retvalue = ifx_mpsdrv_reset();
			break;
#if 0
		case  FIO_MPS_RESTART:
			/* Restart of the DSP */
			retvalue = ifx_mpsdrv_restart();
			if (retvalue == 0)
				ifx_mpsdrv_bufman_init(); //no need for TP-E
			break;

#ifdef MPS_FIFO_BLOCKING_WRITE
		case  FIO_MPS_TXFIFO_SET:
			/* Set the mailbox TX FIFO blocking mode */

			if (pMBDev->devID == command)
			{
				retvalue = -EINVAL;	 /* not supported for this command MB */
			}
			else
			{
				if (arg > 0)
				{
					pMBDev->bBlockWriteMB = TRUE;
				}
				else
				{
					pMBDev->bBlockWriteMB = FALSE;
					Sem_Unlock(pMBDev->sem_write_fifo);
				}
				retvalue = OK;
			}
			break;

		case FIO_MPS_TXFIFO_GET:
			/* Get the mailbox TX FIFO to blocking */

			if (pMBDev->devID == command)
			{
				retvalue = -EINVAL;
			}
			else
			{
				if (!from_kernel)
				{
					copy_to_user((char*)arg, (char*)&pMBDev->bBlockWriteMB, sizeof(bool_t));
				}
				retvalue = OK;
			}
			break;
#endif /* MPS_FIFO_BLOCKING_WRITE */
		case FIO_MPS_GET_STATUS:
			{
				u32 flags;

				save_and_cli(flags);
				/* get the status of the channel */
				if (!from_kernel)
					copy_to_user((char*)arg, (char*)&(ifx_mps_dev.event), sizeof(MbxEventRegs_s));
				if (pMBDev->devID == command)
				{
					ifx_mps_dev.event.MPS_Ad0Reg.val = 0;
					ifx_mps_dev.event.MPS_Ad1Reg.val = 0;
				}
				else
				{
					switch (pMBDev->devID)
					{
						case voice0:
							ifx_mps_dev.event.MPS_VCStatReg[0].val = 0;
							break;
						case voice1:
							ifx_mps_dev.event.MPS_VCStatReg[1].val = 0;
							break;
						case voice2:
							ifx_mps_dev.event.MPS_VCStatReg[2].val = 0;
							break;
						case voice3:
							ifx_mps_dev.event.MPS_VCStatReg[3].val = 0;
							break;
					}
				}
				restore_flags(flags);
				retvalue = OK;
				break;
			}
#endif

		default:
			printk(KERN_INFO "IFX_MPS_Ioctl: Invalid IOCTL handle %d passed.\n", nCmd);
			retvalue = -ENOIOCTLCMD;
			break ;
	}

	return retvalue;
}

/**
 * Register data callback.
 * Allows the upper layer to register a callback function either for
 * downstream (tranmsit mailbox space available) or for upstream (read data
 * available)
 *
 * \param   type     DSP device entity ( 1 - command, 2 - voice0, 3 - voice1,
 *                   4 - voice2, 5 - voice3 )
 * \param   dir      Direction (1 - upstream, 2 - downstream)
 * \param   callback Callback function to register
 * \return  0        OK, callback registered successfully
 * \return  ENXIO    Wrong DSP device entity (only 1-5 supported)
 * \return  EBUSY    Callback already registered
 * \return  EINVAL   Callback parameter null
 * \ingroup API
 */

int ifx_mps_register_callback(u8 seq, FNPTR_T callback)
{

#ifdef CONFIG_DANUBE_MPS_PROC_DEBUG
	printk(KERN_INFO "%s(): is excuted!\n", __FUNCTION__);
#endif
//prochao+
	if (seq == 0)	//reserved
		return ERROR;
//prochao-
	if (data_callback_funct[seq] != NULL)
		return ERROR;
	//set
	data_callback_funct[seq] = (DATAFNPTR_T) callback;

	ifx_mpsdrv_register_callback(seq, (MPSFNPTR_T) ifx_mbx_data_rcv_callback);

	return 0;
}

int ifx_mps_unregister_callback(u8 seq)
{

#ifdef CONFIG_DANUBE_MPS_PROC_DEBUG
	printk(KERN_INFO "%s(): is excuted!\n", __FUNCTION__);
#endif
//prochao+
	if (seq == 0)	//reserved
		return ERROR;
//prochao-
	if (data_callback_funct[seq] == NULL)
		return ERROR;
	//reset
	data_callback_funct[seq] = NULL;

	ifx_mpsdrv_unregister_callback(seq);

	return OK;
}


#if CONFIG_PROC_FS

/**
 * Create MPS version proc file output.
 * This function creates the output for the MPS version proc file
 *
 * \param   buf      Buffer to write the string to
 * \return  len      Lenght of data in buffer
 * \ingroup Internal
 */
static int ifx_mps_get_version_proc(char *buf)
{
	int len;

	len = sprintf(buf, "%s\n", &IFX_MPS_INFO_STR[4]);

	len += sprintf(buf+len,"Compiled on %s, %s for Linux kernel %s\n",
				   __DATE__, __TIME__, UTS_RELEASE);

#ifdef CONFIG_DANUBE_MPS_PROC_DEBUG
	len += sprintf(buf+len,"Supported debug tags:\n");
	len += sprintf(buf+len,"%s = Initialize hardware for voice CPU\n",MPS_FW_INIT_TAG);
	len += sprintf(buf+len,"%s = Start firmware\n",MPS_FW_START_TAG);
	len += sprintf(buf+len,"%s = Send buffer provisioning message\n",MPS_FW_BUFFER_TAG);
	len += sprintf(buf+len,"%s = Opening voice0 mailbox\n",MPS_FW_OPEN_VOICE_TAG);
	len += sprintf(buf+len,"%s = Register voice0 data callback\n",MPS_FW_REGISTER_CALLBACK_TAG);
	len += sprintf(buf+len,"%s = Send message to voice0 mailbox\n",MPS_FW_SEND_MESSAGE_TAG);
	len += sprintf(buf+len,"%s = Restart voice CPU\n",MPS_FW_RESTART_TAG);
	len += sprintf(buf+len,"%s = Packet loop enable\n",MPS_FW_ENABLE_PACKET_LOOP_TAG);
	len += sprintf(buf+len,"%s = Packet loop disable\n",MPS_FW_DISABLE_PACKET_LOOP_TAG);

	len += sprintf(buf+len,"%d Packets received\n",ifx_mps_rtp_voice_data_count);

#endif /* CONFIG_DANUBE_MPS_PROC_DEBUG */

	return len;
}

/**
 * Create MPS status proc file output.
 * This function creates the output for the MPS status proc file
 *
 * \param   buf      Buffer to write the string to
 * \return  len      Lenght of data in buffer
 * \ingroup Internal
 */

	#if 0
static int ifx_mps_get_status_proc(char *buf)
{
	int len, i;


	len = sprintf(buf, "Open files: %d\n", MOD_IN_USE);

	/* Print internals of the command mailbox fifo */
	len += sprintf(buf+len, "\n * CMD *\t\tUP\t\tDO\t%s\n",(ifx_mps_dev.command_mb.Installed == TRUE)?"(active)":"(idle)");
	len += sprintf(buf+len,
				   "   Size: \t  %8d\t  %8d\n",
				   ifx_mps_dev.command_mb.upstrm_fifo.size,
				   ifx_mps_dev.command_mb.dwstrm_fifo.size);

	len += sprintf(buf+len,
				   "   Fill: \t  %8d\t  %8d\n",
				   ifx_mps_dev.command_mb.upstrm_fifo.size - 1 -
				   ifx_mps_fifo_mem_available(&ifx_mps_dev.command_mb.upstrm_fifo),
				   ifx_mps_dev.command_mb.dwstrm_fifo.size - 1 -
				   ifx_mps_fifo_mem_available(&ifx_mps_dev.command_mb.dwstrm_fifo));

	len += sprintf(buf+len,
				   "   Free: \t  %8d\t  %8d\n",
				   ifx_mps_fifo_mem_available(&ifx_mps_dev.command_mb.upstrm_fifo),
				   ifx_mps_fifo_mem_available(&ifx_mps_dev.command_mb.dwstrm_fifo));

	len += sprintf(buf+len,
				   "   Start:\t0x%08X\t0x%08X"
				   "\n   Write:\t0x%08X\t0x%08X"
				   "\n   Read: \t0x%08X\t0x%08X"
				   "\n   End:  \t0x%08X\t0x%08X\n",
				   (u32)ifx_mps_dev.command_mb.upstrm_fifo.pstart,
				   (u32)ifx_mps_dev.command_mb.dwstrm_fifo.pstart,
				   (u32)ifx_mps_dev.command_mb.upstrm_fifo.pend +
				   (u32)*ifx_mps_dev.command_mb.upstrm_fifo.pwrite_off,
				   (u32)ifx_mps_dev.command_mb.dwstrm_fifo.pend +
				   (u32)*ifx_mps_dev.command_mb.dwstrm_fifo.pwrite_off,
				   (u32)ifx_mps_dev.command_mb.upstrm_fifo.pend +
				   (u32)*ifx_mps_dev.command_mb.upstrm_fifo.pread_off,
				   (u32)ifx_mps_dev.command_mb.dwstrm_fifo.pend +
				   (u32)*ifx_mps_dev.command_mb.dwstrm_fifo.pread_off,
				   (u32)ifx_mps_dev.command_mb.upstrm_fifo.pend,
				   (u32)ifx_mps_dev.command_mb.dwstrm_fifo.pend);

	/* Printout the number of interrupts and fifo misses */
	len += sprintf(buf+len,
				   "   Ints: \t  %8d\t  %8d\n",
				   ifx_mps_dev.command_mb.RxnumIRQs,
				   ifx_mps_dev.command_mb.TxnumIRQs);

	len += sprintf(buf+len,
				   "   Bytes:\t  %8d\t  %8d\n",
				   ifx_mps_dev.command_mb.RxnumBytes,
				   ifx_mps_dev.command_mb.TxnumBytes);


	len += sprintf(buf+len, "\n * VOICE *\t\tUP\t\tDO\n");
	len += sprintf(buf+len,
				   "   Size: \t  %8d\t  %8d\n",
				   ifx_mps_dev.voice_mb[0].upstrm_fifo.size,
				   ifx_mps_dev.voice_mb[0].dwstrm_fifo.size);

	len += sprintf(buf+len,
				   "   Fill: \t  %8d\t  %8d\n",
				   ifx_mps_dev.voice_mb[0].upstrm_fifo.size - 1 -
				   ifx_mps_fifo_mem_available(&ifx_mps_dev.voice_mb[0].upstrm_fifo),
				   ifx_mps_dev.voice_mb[0].dwstrm_fifo.size - 1 -
				   ifx_mps_fifo_mem_available(&ifx_mps_dev.voice_mb[0].dwstrm_fifo));

	len += sprintf(buf+len,
				   "   Free: \t  %8d\t  %8d\n",
				   ifx_mps_fifo_mem_available(&ifx_mps_dev.voice_mb[0].upstrm_fifo),
				   ifx_mps_fifo_mem_available(&ifx_mps_dev.voice_mb[0].dwstrm_fifo));

	len += sprintf(buf+len,
				   "   Start:\t0x%08X\t0x%08X"
				   "\n   Write:\t0x%08X\t0x%08X"
				   "\n   Read: \t0x%08X\t0x%08X"
				   "\n   End:  \t0x%08X\t0x%08X\n",
				   (u32)ifx_mps_dev.voice_mb[0].upstrm_fifo.pstart,
				   (u32)ifx_mps_dev.voice_mb[0].dwstrm_fifo.pstart,
				   (u32)ifx_mps_dev.voice_mb[0].upstrm_fifo.pend +
				   (u32)*ifx_mps_dev.voice_mb[0].upstrm_fifo.pwrite_off,
				   (u32)ifx_mps_dev.voice_mb[0].dwstrm_fifo.pend +
				   (u32)*ifx_mps_dev.voice_mb[0].dwstrm_fifo.pwrite_off,
				   (u32)ifx_mps_dev.voice_mb[0].upstrm_fifo.pend +
				   (u32)*ifx_mps_dev.voice_mb[0].upstrm_fifo.pread_off,
				   (u32)ifx_mps_dev.voice_mb[0].dwstrm_fifo.pend +
				   (u32)*ifx_mps_dev.voice_mb[0].dwstrm_fifo.pread_off,
				   (u32)ifx_mps_dev.voice_mb[0].upstrm_fifo.pend,
				   (u32)ifx_mps_dev.voice_mb[0].dwstrm_fifo.pend);


	for (i = 0; i < 4; i++)
	{
		len += sprintf(buf+len, "\n * CH%i *\t\tUP\t\tDO\t%s\n", i,(ifx_mps_dev.voice_mb[i].Installed == TRUE)?"(active)":"(idle)");
		/* Printout the number of interrupts and fifo misses */
		len += sprintf(buf+len,
					   "   Ints: \t  %8d\t  %8d\n",
					   ifx_mps_dev.voice_mb[i].RxnumIRQs,
					   ifx_mps_dev.voice_mb[i].TxnumIRQs);

		len += sprintf(buf+len,
					   "   Bytes: \t  %8d\t  %8d\n",
					   ifx_mps_dev.voice_mb[i].RxnumBytes,
					   ifx_mps_dev.voice_mb[i].TxnumBytes);
#if 1
		len += sprintf(buf+len,
					   "   minLv: \t  %8d\t  %8d\n",
					   ifx_mps_dev.voice_mb[i].upstrm_fifo.min_space,
					   ifx_mps_dev.voice_mb[i].dwstrm_fifo.min_space);
#endif
	}
	return len;
}

	#endif
/**
 * Create MPS proc file output.
 * This function creates the output for the MPS proc file according to the
 * function specified in the data parameter, which is setup during registration.
 *
 * \param   buf      Buffer to write the string to
 * \param   start    not used (Linux internal)
 * \param   offset   not used (Linux internal)
 * \param   count    not used (Linux internal)
 * \param   eof      Set to 1 when all data is stored in buffer
 * \param   data     not used (Linux internal)
 * \return  len      Lenght of data in buffer
 * \ingroup Internal
 */
static int ifx_mps_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len;

	int (*fn)(char *buf);

	if ( data != NULL )
	{
		fn = data;
		len = fn(page);
	}
	else
		return 0;

	if ( len <= off+count )
		*eof = 1;
	*start = page + off;
	len -= off;
	if ( len>count )
		len = count;
	if ( len<0 )
		len = 0;
	return len;
}

typedef enum
{
	mps_proc_setbit = 1,
	mps_proc_clrbit
}mps_proc_cmd;

	#if 1
static int strn_len(const char *user_buffer, unsigned int maxlen)
{
	int i = 0;

	for (; i < maxlen; i++)
	{
		char c;

		if (get_user(c, &user_buffer[i]))
			return -EFAULT;
		switch (c)
		{
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
static int count_trail_chars(const char *user_buffer, unsigned int maxlen)
{
	int i;

	for (i = 0; i < maxlen; i++)
	{
		char c;

		if (get_user(c, &user_buffer[i]))
			return -EFAULT;
		switch (c)
		{
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


int ifx_mps_proc_addproc(char *funcname, read_proc_t *hookfuncr, write_proc_t *hookfuncw)
{
	struct proc_dir_entry *pe;

	if (!ifx_mps_proc_dir)
		return -1;

	if (hookfuncw == NULL)
	{
		pe = create_proc_read_entry(funcname, 0, ifx_mps_proc_dir, hookfuncr, NULL);
		if (!pe)
		{
			printk(KERN_INFO "ERROR in creating read proc entry (%s)! \n", funcname);
			return -1;
		}
	}
	else
	{
		pe = create_proc_entry(funcname, S_IRUGO | S_IWUGO, ifx_mps_proc_dir);
		if (pe)
		{
			pe->read_proc = hookfuncr;
			pe->write_proc = hookfuncw;
		}
		else
		{
			printk(KERN_INFO "ERROR in creating proc entry (%s)! \n", funcname);
			return -1;
		}
	}

	return 0;
}

#ifdef	CONFIG_DANUBE_CORE1	//for CPU1
// echo "SET_BIT|CLR_BIT n" > /proc/driver/ifx_mps/mps1_apps_ready
static int ifx_mps_set_mps1rdy_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
	char	buf[80], *ptr;
	int		len, i = 0;
	int		proc_cmd=0;
	u16		bitn;

	len = strn_len(&buffer[i], sizeof(buf));
	if (len < 0) return len;

	if (copy_from_user(buf, &buffer[i], len))
		return -EFAULT;
	buf[len] ='\0';
	i += len;

	if (strstr(buf, "SET_BIT"))
	{
		proc_cmd = mps_proc_setbit;
	}
	else if (strstr(buf, "CLR_BIT"))
	{
		proc_cmd = mps_proc_clrbit;
	}
	else
	{
		printk("ERROR: unrecognized command!\n");
		printk("SET_BIT | CLR_BIT  bitN\n");
		return -EFAULT;
	}

	len = count_trail_chars(&buffer[i], sizeof(buf));
	if (len < 0) return len;
	i += len;

	len = strn_len(&buffer[i], sizeof(buf));
	if (len < 0) return len;

	if (copy_from_user(buf, &buffer[i], len))
		return -EFAULT;
	buf[len] ='\0';
	i += len;

	bitn = (u16) simple_strtoul(buf, &ptr, 10);
	if (bitn >= 16)
	{
		printk("ERROR: bitN exceeds 15 (should be 0~15)!\n");
		return -EFAULT;
	}

	switch (proc_cmd)
	{
		case mps_proc_setbit:
			ifx_mpsdrv_ctrl_mps1_apps_ready_bit( 1, bitn);
			break;
		case mps_proc_clrbit:
			ifx_mpsdrv_ctrl_mps1_apps_ready_bit( 0, bitn);
			break;
#if 0	//prochao+, 12/21/2006
		case mps_proc_get_version:
			{
				int result;
				result = ifx_mps_get_version(cpu_direction);
				printk(KERN_INFO "version:%d\n", result);
			}
			break;
#endif	//prochao-
	}

	return count;
}
#endif

// readout the mps1 ready bits info
int ifx_mps_get_mps1rdy_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int	len = 0, i;
	u16	mps1_ready;

	mps1_ready = ifx_mpsdrv_get_mps1_apps_ready_bits();
	len += sprintf(buf+len, "-----<MPS1_APPS_READY_BITS>-----\n");
	len += sprintf(buf+len, "-F-E-D-C-B-A-9-8-7-6-5-4-3-2-1-0\n");
	for (i = 15; i >= 0; i--)
	{
		if (mps1_ready & (0x0001 << i))
		{
			len += sprintf(buf+len, " 1");
		}
		else
		{
			len += sprintf(buf+len, " 0");
		}
	}
	len += sprintf(buf+len, "\n");

	return len;
}
//

	#endif

#endif /* CONFIG_PROC_FS */

/**
 * Initialize the module.
 * This is the initialization function of the MPS module.
 *
 * \return  0        OK, module initialized
 * \return  EPERM    Reset of CPU1 failed
 * \return  ENOMEM   No memory left for structures
 * \ingroup Internal
 */
#if !defined( IKOS_MINI_BOOT )
static int __init ifx_mps_init_module(void)
#else
int __init ifx_mps_init_module(void)
#endif
{
	int result;

//	devfs_handle_t mps_dir;
	//int i;

	printk("%s\n", &IFX_MPS_INFO_STR[4]);
	printk("(c) Copyright 2006, Infineon Technologies AG\n");

#ifndef CONFIG_DEVFS_FS
	sprintf(ifx_mps_dev_name, IFX_MPS_DEV_NAME);
	result = register_chrdev(ifx_mps_major_id , ifx_mps_dev_name, &ifx_mps_fops);

	if ( result < 0 )
	{
		printk(KERN_DEBUG "IFX_MPS: can't get major %d\n", ifx_mps_major_id);
		return result;
	}
	memset(&mps_cmd_control, 0, sizeof(mps_callback_element));
	init_waitqueue_head(&mps_cmd_control.cmd_wakeuplist);
	//prochao+
	init_waitqueue_head(&mps_cmd_control.data_wakeuplist);
	cmd_wakeuplist_balance = data_wakeuplist_balance = 0;
	//prochao-
	mps_cmd_control.cmd_seq_semaphore_spinlock = SPIN_LOCK_UNLOCKED;
//	sema_init(mps_cmd_control.cmd_seq_semaphore_spinlock, 1);
	//prochao+
	mps_cmd_control.data_seq_semaphore_spinlock = SPIN_LOCK_UNLOCKED;
//	sema_init(mps_cmd_control.data_seq_semaphore_spinlock, 1);
	mps_cmd_control.cmd_callback_flag = 0;	//actually already set to 0 in the above memset()
	mps_cmd_control.data_callback_flag = 0;
	//prochao-
	//prochao+, 11/02/2006
	mps_cmd_control.polling_cmd_callback_flag = 0;
	//prochao-

	/* dynamic major                       */
	if ( ifx_mps_major_id == 0 )
		ifx_mps_major_id = result;
#else
	sprintf(ifx_mps_dev_name, IFX_MPS_DEV_NAME);
	result = devfs_register_chrdev(ifx_mps_major_id , ifx_mps_dev_name, &ifx_mps_fops);

	if ( result < 0 )
	{
		printk(KERN_DEBUG "IFX_MPS: can't get major %d\n", ifx_mps_major_id);
		return result;
	}

	/* dynamic major                       */
	if ( ifx_mps_major_id == 0 )
		ifx_mps_major_id = result;

	mps_dir = devfs_mk_dir(NULL, ifx_mps_dev_name, NULL);
	if (!mps_dir)
		return -EBUSY; /* problem */

	devfs_register(mps_dir, "cmd", DEVFS_FL_NONE, ifx_mps_major_id, 1, S_IFCHR | S_IRUGO | S_IWUGO, &ifx_mps_fops, NULL);
	devfs_register(mps_dir, "wifi0", DEVFS_FL_NONE, ifx_mps_major_id, 2, S_IFCHR | S_IRUGO | S_IWUGO, &ifx_mps_fops, NULL);
	devfs_register(mps_dir, "wifi1", DEVFS_FL_NONE, ifx_mps_major_id, 3, S_IFCHR | S_IRUGO | S_IWUGO, &ifx_mps_fops, NULL);
	//    devfs_register(mps_dir, "wifi2", DEVFS_FL_NONE, ifx_mps_major_id, 4, S_IFCHR | S_IRUGO | S_IWUGO, &ifx_mps_fops, NULL);
	//    devfs_register(mps_dir, "wifi3", DEVFS_FL_NONE, ifx_mps_major_id, 5, S_IFCHR | S_IRUGO | S_IWUGO, &ifx_mps_fops, NULL);

#endif /* !CONFIG_DEVFS_FS */
	/* init the device driver structure*/
#if 1
	if (0 != ifx_mpsdrv_initial_structures(&ifx_mps_dev))
		return -ENOMEM;
#if 0   //done in the driver at initial time
	/* reset the device before initializing the device driver */
	if ( (ERROR) == ifx_mpsdrv_reset() )
	{
		printk("IFX_MPS: Hardware reset failed!\n");
		return -EPERM;
	}
#endif
//prochao+
#ifdef MPS_USE_KT
	ifx_mpsdrv_initialize_KT();		//initialize the kernel threads
#endif
//prochao-
//prochao+, 11/30/2006
#if	(!defined(CONFIG_DANUBE_CORE1) && defined(MPS_USE_HOUSEKEEPING_TIMER))	//only for the CPU0
	ifx_mpsdrv_initialize_hk_timer();	//initialize the housekeeping timer
#endif
//prochao-
	result = ifx_mpsdrv_init_IRQs(&ifx_mps_dev);
	/* Enable all MPS Interrupts at ICU0     22|21|20||19|18|17|16||15|14|..|            */
	//MPS_INTERRUPTS_ENABLE(0x007FC000);    //done inside the xxx_init_IRQs()
	if (result)
		return result;
	ifx_mpsdrv_register_callback(0, ifx_mps_cmd_rcv_callback);
#endif

#if CONFIG_PROC_FS
	/* install the proc entry */
	printk("IFX_MPS: using proc fs\n");

	ifx_mps_proc_dir = proc_mkdir("driver/" IFX_MPS_DEV_NAME, NULL);
	if ( ifx_mps_proc_dir != NULL )
	{
		create_proc_read_entry("version" , S_IFREG|S_IRUGO, ifx_mps_proc_dir, ifx_mps_read_proc, (void *)ifx_mps_get_version_proc);
//prochao+, 12/21/2006
#ifdef	CONFIG_DANUBE_CORE1	//for CPU1
		ifx_mps_proc_addproc("mps1_apps_ready", ifx_mps_get_mps1rdy_proc, ifx_mps_set_mps1rdy_proc);
#else	//for CPU0
		create_proc_read_entry("mps1_apps_ready" , S_IFREG|S_IRUGO, ifx_mps_proc_dir, ifx_mps_read_proc, (void *)ifx_mps_get_mps1rdy_proc);
//		ifx_mps_proc_addproc("mps1_apps_ready", NULL, ifx_mps_set_mps1rdy_proc);		//prochao%, 12/20/2006
#endif
//prochao-
#if 0
		create_proc_read_entry("status" , S_IFREG|S_IRUGO, ifx_mps_proc_dir, ifx_mps_read_proc, (void *)ifx_mps_get_status_proc);
#endif
	}
	else
	{
		printk(KERN_INFO "IFX_MPS: cannot create proc entry\n");
	}
#endif

#ifdef CONFIG_MPS_FLOW_CONTROL
        atomic_set(&bklog_pkts, 0);
        atomic_set(&bklog_q_lock, 1);
        init_timer(&flush_bklog_timer);
        flush_bklog_timer.function = flush_bklog_timer_handler;
        flush_bklog_timer.data = 0;
        flush_bklog_timer.expires = jiffies + (1*HZ) ;
        add_timer(&flush_bklog_timer); 
#endif

	return 0;
}


/**
 * Cleanup MPS module.
 * Clean up the module for unloading.
 *
 * \ingroup Internal
 */
static void __exit ifx_mps_cleanup_module(void)
{
	/* disable Interrupts at ICU0 */
	MPS_INTERRUPTS_DISABLE(DANUBE_MPS_AD0_IR4);	 /* Disable DFE/AFE 0 Interrupts */
	/* disable all MPS interrupts */
	*DANUBE_MPS_SAD0SR = 0x00000000;

	unregister_chrdev(ifx_mps_major_id , IFX_MPS_DEV_NAME);

	/* release all interrupts at the system */
	free_irq(/*INT_NUM_IM5_IRL11*/INT_NUM_IM4_IRL18, &ifx_mps_dev);
//prochao+
	ifx_mpsdrv_stop_KT();	//stop the kernel threads
//prochao-
	/* release the memory usage of the device driver structure */
	ifx_mpsdrv_release_structures(&ifx_mps_dev);

#if CONFIG_PROC_FS
#ifdef CONFIG_DANUBE_MPS_PROC_DEBUG
	remove_proc_entry("firmware", ifx_mps_proc_dir);
#endif /* CONFIG_DANUBE_MPS_PROC_DEBUG */
	remove_proc_entry("version", ifx_mps_proc_dir);
	remove_proc_entry("status", ifx_mps_proc_dir);
	remove_proc_entry("driver/" IFX_MPS_DEV_NAME, NULL);
#endif

	printk(KERN_INFO "IFX_MPS: cleanup done\n");
}



module_init(ifx_mps_init_module);
module_exit(ifx_mps_cleanup_module);

#if !defined( IKOS_MINI_BOOT )
	#ifndef DEBUG
EXPORT_SYMBOL(ifx_mps_register_callback);
EXPORT_SYMBOL(ifx_mps_unregister_callback);
EXPORT_SYMBOL(ifx_mps_data_send);
EXPORT_SYMBOL(ifx_mps_datalist_send);
EXPORT_SYMBOL(ifx_mps_data_rcv);
EXPORT_SYMBOL(ifx_mps_datalist_rcv);
EXPORT_SYMBOL(ifx_mps_datalist_alloc);
EXPORT_SYMBOL(ifx_mps_datalist_free);
EXPORT_SYMBOL(ifx_mps_skbuff_alloc);
EXPORT_SYMBOL(ifx_mps_skbuff_free);
//prochao+, 10/19/2006
EXPORT_SYMBOL(ifx_mps_app_buff_kmalloc);
EXPORT_SYMBOL(ifx_mps_app_buff_kfree);
//prochao-
EXPORT_SYMBOL(ifx_mps_app_message_send);
EXPORT_SYMBOL(ifx_mps_app_message_rcv);
//
//-------------------------------------------------------------------------------------------------------------------
		#ifdef	CONFIG_DANUBE_CORE1
// prochao+, 10/29/2006, adds the new APIs for sk_buff provisioning equivalent replacements
EXPORT_SYMBOL(ifx_mps_dev_alloc_skb);
EXPORT_SYMBOL(ifx_mps_dev_kfree_skb);
EXPORT_SYMBOL(ifx_mps_alloc_skb);
EXPORT_SYMBOL(ifx_mps_skb_clone);
EXPORT_SYMBOL(ifx_mps_skb_copy);
EXPORT_SYMBOL(ifx_mps_pskb_expand_head);
EXPORT_SYMBOL(ifx_mps_skb_realloc_headroom);
EXPORT_SYMBOL(ifx_mps_skb_copy_expand);
EXPORT_SYMBOL(ifx_mps_skb_trim);
// prochao-
		#endif
//-------------------------------------------------------------------------------------------------------------------
EXPORT_SYMBOL(ifx_mps_ioctl);
EXPORT_SYMBOL(ifx_mps_open);
EXPORT_SYMBOL(ifx_mps_close);
	#endif
#endif


