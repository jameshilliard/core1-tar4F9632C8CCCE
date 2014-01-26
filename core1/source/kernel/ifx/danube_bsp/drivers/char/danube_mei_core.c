/*
 * ########################################################################
 *
 *  This program is free softwavre; you can distribute it and/or modify it
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
 *
 */
 
/* ===========================================================================
 *
 * File Name:   danube_mei.c
 * Author :     Taicheng
 *
 * ===========================================================================
 *
 * Project: Danube
 *
 * ===========================================================================
 * Contents:This file implements the MEI driver for Danube ADSL/ADSL2+
 *  controller.
 *  
 * ===========================================================================
 * References: 
 *
 */

/*
 * ===========================================================================
 *                           INCLUDE FILES
 * ===========================================================================
 */

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <asm/semaphore.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include <linux/list.h>
#include <linux/delay.h>

char DANUBE_MEI_VERSION[]="0.99.01";
//#define DANUBE_MEI_DEBUG
#undef DANUBE_MEI_DMSG
#ifdef DANUBE_MEI_DEBUG
#define DANUBE_MEI_DMSG(fmt, args...) printk( KERN_INFO  "%s: " fmt,__FUNCTION__, ## args)
#else
#define DANUBE_MEI_DMSG(fmt, args...) 
#endif

#define DANUBE_MEI_EMSG(fmt, args...) printk( KERN_ERR  "%s: " fmt,__FUNCTION__, ## args)

#define DANUBE_MEI_CMV_EXTRA //WINHOST debug

#undef DANUBE_CLEAR_EOC
// for ARC memory access
#define WHILE_DELAY 20000
#define DANUBE_DMA_DEBUG_MUTEX


#define IMAGE_SWAP
#define BOOT_SWAP
#define HEADER_SWAP

//TODO
//#define DFE_LOOPBACK // testing code
#ifdef DFE_LOOPBACK
//#define DFE_MEM_TEST
//#define DFE_PING_TEST
#define DFE_ATM_LOOPBACK
#endif

#undef DATA_LED_ON_MODE
#undef IFX_DYING_GASP

#ifdef CONFIG_PROC_FS
#define PROC_ITEMS 8
#define MEI_DIRNAME     "mei"
#endif

//  Block size per BAR
#define SDRAM_SEGMENT_SIZE	(64*1024)
// Number of Bar registers
#define MAX_BAR_REGISTERS	(17)

#define XDATA_REGISTER		(15)

#include <asm/danube/danube.h>
#include <asm/danube/irq.h>
#include <asm/danube/danube_mei.h>
#include <asm/danube/danube_mei_app.h>
#include <asm/danube/danube_mei_ioctl.h>
#include <asm/danube/danube_mei_app_ioctl.h>
#include <asm/danube/port.h>
#include <asm/danube/danube_gpio.h>

#ifdef CONFIG_DEVFS_FS
#define DANUBE_DEVNAME	"danube"
#endif
#define DANUBE_MEI_DEVNAME "mei"

#ifdef DFE_LOOPBACK
#ifndef UINT32 
#define UINT32 unsigned long
#endif
#ifdef DFE_PING_TEST
#include "dsp_xmem_arb_rand_em.h"
#endif
#ifdef DFE_MEM_TEST
#include "aai_mem_test.h"
#endif
#ifdef DFE_ATM_LOOPBACK
#include "aai_lpbk_dyn_rate.h"
#endif
#endif

/************************************************************************
 *  Function declaration
 ************************************************************************/
extern void mask_and_ack_danube_irq(unsigned int irq_nr);
static void meiLongwordWrite(u32 ul_address, u32 ul_data);
static void meiLongwordRead(u32 ul_address, u32 *pul_data);
static MEI_ERROR meiDMAWrite(u32 destaddr, u32 *databuff, u32 databuffsize);
static MEI_ERROR meiDMARead(u32 srcaddr, u32 *databuff, u32 databuffsize);
static void meiControlModeSwitch(int mode);
static void meiPollForDbgDone(void);
static MEI_ERROR _meiDebugLongWordRead(u32 DEC_mode, u32 address, u32 *data);
static MEI_ERROR _meiDebugLongWordWrite(u32 DEC_mode, u32 address, u32 data);
MEI_ERROR meiDebugWrite(u32 destaddr, u32 *databuff, u32 databuffsize);
static MEI_ERROR meiDebugRead(u32 srcaddr, u32 *databuff, u32 databuffsize);
static MEI_ERROR meiMailboxWrite(u16 *msgsrcbuffer, u16 msgsize);
static MEI_ERROR meiDownloadBootCode(void);
static MEI_ERROR meiHaltArc(void);
static MEI_ERROR meiRunArc(void);
static MEI_ERROR meiRunAdslModem(void);
static int meiGetPage( u32 Page, u32 data, u32 MaxSize, u32 *Buffer, u32 *Dest);
void makeCMV(u8 opcode, u8 group, u16 address, u16 index, int size, u16 * data,u16 *CMVMSG);
MEI_ERROR meiCMV(u16 * request, int reply, u16 *response);
static void meiMailboxInterruptsDisable(void);
static void meiMailboxInterruptsEnable(void);
static void mei_interrupt_arcmsgav(int int1, void * void0, struct pt_regs * regs);
static int update_bar_register(int nTotalBar);
#ifdef IFX_DYING_GASP
static int lop_poll(void *unused);
static int lop_poll_init(void);
#endif
static ssize_t mei_write(struct file *, const char *, size_t, loff_t *);
static int free_image_buffer(int type);
static int alloc_processor_memory(unsigned long size,smmu_mem_info_t *adsl_mem_info);
static ssize_t mei_write(struct file * filp, const char * buf, size_t size, loff_t * loff);
int mei_ioctl(struct inode * ino, struct file * fil, unsigned int command, unsigned long lon);

#ifdef CONFIG_DEVFS_FS
static devfs_handle_t danube_base_dir;
static devfs_handle_t mei_devfs_handle;
#endif
#ifdef CONFIG_PROC_FS
static int proc_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos);
static ssize_t proc_write(struct file * file, const char * buffer, size_t count, loff_t *ppos);
#endif

#ifdef CONFIG_DANUBE_MEI_MIB			
int mei_mib_ioctl(struct inode * ino, struct file * fil, unsigned int command, unsigned long lon);
int mei_mib_adsl_link_up(void);
int mei_mib_adsl_link_down(void);
int danube_mei_mib_init(void);
int danube_mei_mib_cleanup(void);
#endif	
#ifdef CONFIG_DANUBE_MEI_LED
extern int firmware_support_led; //joelin version check 	for adsl led	
int danube_mei_led_init(void);
int danube_mei_led_cleanup(void);
#endif
// for clearEoC 
#ifdef DANUBE_CLEAR_EOC
int ifx_pop_eoc(struct sk_buff * pkt);
extern void ifx_push_eoc(struct sk_buff * pkt);
#endif

/************************************************************************
 *  variable declaration
 ************************************************************************/
static smmu_mem_info_t adsl_mem_info[MAX_BAR_REGISTERS];
static unsigned long image_size=0;
#ifdef IFX_DYING_GASP
static wait_queue_head_t wait_queue_dying_gasp;	//dying gasp
static meidebug lop_debugwr;				//dying gasp
static int lop_poll_shutdown = 0;
static struct completion lop_thread_exit;//wait kernel_thread exit
#endif //IFX_DYING_GASP
static struct timeval time_disconnect,time_showtime;
static u16 unavailable_seconds=0;

#ifdef DANUBE_CLEAR_EOC
static struct list_head clreoc_list;
static danube_clreoc_pkt * clreoc_pkt;
static wait_queue_head_t wait_queue_clreoc;
static void * clreoc_command_pkt=NULL;
static int clreoc_max_tx_len=0;
#endif

int showtime=0;
static int major=DANUBE_MEI_MAJOR;
struct semaphore mei_sema;

// Mei to ARC CMV count, reply count, ARC Indicator count
static int indicator_count=0;
static int cmv_count=0;
static int reply_count=0;
static u16 Recent_indicator[MSG_LENGTH];
static int reset_arc_flag = 0;

// Used in interrupt handler as flags
static int arcmsgav=0;
static int cmv_reply=0;
static int cmv_waiting=0;
//  to wait for arc cmv reply, sleep on wait_queue_arcmsgav;
static wait_queue_head_t wait_queue_arcmsgav;

// CMV mailbox messages
// ARC to MEI message
u16 CMV_RxMsg[MSG_LENGTH]__attribute__ ((aligned(4)));
// MEI to ARC message
u16 CMV_TxMsg[MSG_LENGTH]__attribute__ ((aligned(4)));

static u32 * mei_arc_swap_buff=NULL;   	//  holding swap pages
static ARC_IMG_HDR * img_hdr;
static int arc_halt_flag=0;
static int nBar=0;  // total bars to be used.

static u32 loop_diagnostics_mode=0;
wait_queue_head_t wait_queue_loop_diagnostic;
int loop_diagnostics_completed=0;
#ifdef CONFIG_DANUBE_MEI_MIB			
u32 adsl_mode,adsl_mode_extend; // adsl mode : adsl/ 2/ 2+
#endif

#define ME_HDLC_IDLE 0
#define ME_HDLC_INVALID_MSG 1
#define ME_HDLC_MSG_QUEUED 2
#define ME_HDLC_MSG_SENT 3
#define ME_HDLC_RESP_RCVD 4
#define ME_HDLC_RESP_TIMEOUT 5
#define ME_HDLC_RX_BUF_OVERFLOW 6
#define ME_HDLC_UNRESOLVED 1
#define ME_HDLC_RESOLVED 2

static struct file_operations mei_operations = {
        write:         	mei_write,
        ioctl:         	mei_ioctl,
};

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *meidir;
static struct file_operations proc_operations = {
	read:	proc_read,
	write:	proc_write,
};
static reg_entry_t regs[PROC_ITEMS]; //total items to be monitored by /proc/mei
#define NUM_OF_REG_ENTRY	(sizeof(regs)/sizeof(reg_entry_t))
#endif

#ifdef DFE_LOOPBACK
unsigned char got_int=0;
#endif

/////////////////               mei access Rd/Wr methods       ///////////////
/**
 * Write a value to register 
 * This function writes a value to danube register
 * 
 * \param  	ul_address	The address to write
 * \param  	ul_data		The value to write
 * \ingroup	Internal
 */
static void meiLongwordWrite(u32 ul_address, u32 ul_data)
{
	//*((volatile u32 *)ul_address) = ul_data;
	DANUBE_WRITE_REGISTER_L(ul_data,ul_address);
	asm("SYNC");
	return;
} //	end of "meiLongwordWrite(..."

/**
 * Read the danube register 
 * This function read the value from danube register
 * 
 * \param  	ul_address	The address to write
 * \param  	pul_data	Pointer to the data
 * \ingroup	Internal
 */
static void meiLongwordRead(u32 ul_address, u32 *pul_data)
{
	//*pul_data = *((volatile u32 *)ul_address);
	*pul_data = DANUBE_READ_REGISTER_L(ul_address);
	asm("SYNC");
	return;
} //	end of "meiLongwordRead(..."

/**
 * Write several DWORD datas to ARC memory via ARC DMA interface
 * This function writes several DWORD datas to ARC memory via DMA interface.
 * 
 * \param  	destaddr	The address to write
 * \param  	databuff	Pointer to the data buffer
 * \param  	databuffsize	Number of DWORDs to write
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR meiDMAWrite(u32 destaddr, u32 *databuff, u32 databuffsize)
{
	u32 *p = databuff;
	u32 temp;
	u32 flags;

	if( destaddr & 3)
		return MEI_FAILURE;

#ifdef DANUBE_DMA_DEBUG_MUTEX
	save_flags(flags);
	cli();
#endif
		
	DANUBE_MEI_DMSG("destaddr=%X,size=%d\n",destaddr,databuffsize);
	//	Set the write transfer address
	meiLongwordWrite((u32)MEI_XFR_ADDR, destaddr);

	//	Write the data pushed across DMA
	while (databuffsize--)
	{
		temp = *p;
		if(databuff==(u32 *)CMV_TxMsg)	
			MEI_HALF_WORD_SWAP(temp);
		meiLongwordWrite((u32)MEI_DATA_XFR, temp);
		p++;
	} // 	end of "while(..."

#ifdef DANUBE_DMA_DEBUG_MUTEX
	restore_flags(flags);	
#endif
	
	return MEI_SUCCESS;

} //	end of "meiDMAWrite(..."

/**
 * Read several DWORD datas from ARC memory via ARC DMA interface
 * This function reads several DWORD datas from ARC memory via DMA interface.
 * 
 * \param  	srcaddr		The address to read
 * \param  	databuff	Pointer to the data buffer
 * \param  	databuffsize	Number of DWORDs to read
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR meiDMARead(u32 srcaddr, u32 *databuff, u32 databuffsize)
{
	u32 *p = databuff;
	u32 temp;
	u32 flags;
	DANUBE_MEI_DMSG("destaddr=%X,size=%X\n",srcaddr,databuffsize);
	if( srcaddr & 3)
		return MEI_FAILURE;

#ifdef DANUBE_DMA_DEBUG_MUTEX
	save_flags(flags);
	cli();
#endif

	//	Set the read transfer address
	meiLongwordWrite((u32)MEI_XFR_ADDR, srcaddr);

	//	Read the data popped across DMA
	while (databuffsize--)
	{
		meiLongwordRead((u32)MEI_DATA_XFR, &temp);
		if(databuff==(u32 *)CMV_RxMsg)	// swap half word
			MEI_HALF_WORD_SWAP(temp);
		*p=temp;
		p++;
	} // 	end of "while(..."

#ifdef DANUBE_DMA_DEBUG_MUTEX
	restore_flags(flags);
#endif

	return MEI_SUCCESS;

} //	end of "meiDMARead(..."

/**
 * Switch the ARC control mode
 * This function switchs the ARC control mode to JTAG mode or MEI mode
 * 
 * \param  	mode		The mode want to switch: JTAG_MASTER_MODE or MEI_MASTER_MODE.
 * \ingroup	Internal
 */
static void meiControlModeSwitch(int mode)
{
	u32 temp = 0x0;
	meiLongwordRead((u32)MEI_DBG_MASTER, &temp);
	switch (mode)
	{
		case JTAG_MASTER_MODE:
			temp &= ~(HOST_MSTR);
		break;
		case MEI_MASTER_MODE:
			temp |= (HOST_MSTR);
		break;
		default:
		DANUBE_MEI_EMSG("meiControlModeSwitch: unkonwn mode [%d]\n",mode);
		return ;
	}
	meiLongwordWrite((u32)MEI_DBG_MASTER, temp);
}

/**
 * Poll for transaction complete signal
 * This function polls and waits for transaction complete signal.
 * 
 * \ingroup	Internal
 */
static void meiPollForDbgDone(void)
{
	u32	query = 0;
	int 	i=0;
	while (i<WHILE_DELAY)
	{
		meiLongwordRead((u32)ARC_TO_MEI_INT, &query);
		query &= (ARC_TO_MEI_DBG_DONE);
		if(query)
			break;
		i++;
		if(i==WHILE_DELAY){
			DANUBE_MEI_EMSG("\n\n PollforDbg fail");
		}
	} 
   	meiLongwordWrite((u32)ARC_TO_MEI_INT,  ARC_TO_MEI_DBG_DONE);  // to clear this interrupt
} //	end of "meiPollForDbgDone(..."

/**
 * ARC Debug Memory Access for a single DWORD reading.
 * This function used for direct, address-based access to ARC memory.
 * 
 * \param  	DEC_mode	ARC memory space to used
 * \param  	address	  	Address to read
 * \param  	data	  	Pointer to data
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR _meiDebugLongWordRead(u32 DEC_mode, u32 address, u32 *data)
{
	meiLongwordWrite((u32)MEI_DEBUG_DEC, DEC_mode);
	meiLongwordWrite((u32)MEI_DEBUG_RAD, address);
	meiPollForDbgDone();
	meiLongwordRead((u32)MEI_DEBUG_DATA, data);
	return MEI_SUCCESS;
}

/**
 * ARC Debug Memory Access for a single DWORD writing.
 * This function used for direct, address-based access to ARC memory.
 * 
 * \param  	DEC_mode	ARC memory space to used
 * \param  	address	  	The address to write
 * \param  	data	  	The data to write
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR _meiDebugLongWordWrite(u32 DEC_mode, u32 address, u32 data)
{
	meiLongwordWrite((u32)MEI_DEBUG_DEC, DEC_mode);
	meiLongwordWrite((u32)MEI_DEBUG_WAD, address);
	meiLongwordWrite((u32)MEI_DEBUG_DATA, data);
	meiPollForDbgDone();
	return MEI_SUCCESS;
}

/**
 * ARC Debug Memory Access for writing.
 * This function used for direct, address-based access to ARC memory.
 * 
 * \param  	destaddr	The address to ead
 * \param  	databuffer  	Pointer to data
 * \param	databuffsize	The number of DWORDs to read
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */

MEI_ERROR meiDebugWrite(u32 destaddr, u32 *databuff, u32 databuffsize)
{
	u32 i;
	u32 temp = 0x0;
	u32 address = 0x0;
	u32 *buffer = 0x0;
	u32 flags;
	
#ifdef DANUBE_DMA_DEBUG_MUTEX
	save_flags(flags);
	cli();
#endif
	
	//	Open the debug port before DMP memory write
	meiControlModeSwitch(MEI_MASTER_MODE);

	meiLongwordWrite((u32)MEI_DEBUG_DEC, MEI_DEBUG_DEC_DMP1_MASK);

	//	For the requested length, write the address and write the data
	address = destaddr;
	buffer = databuff;
	for (i=0; i < databuffsize; i++)
	{
		temp=*buffer;
		_meiDebugLongWordWrite(MEI_DEBUG_DEC_DMP1_MASK,address,temp);
		address += 4;
		buffer++;
	} //	end of "for(..."

	//	Close the debug port after DMP memory write
	meiControlModeSwitch(JTAG_MASTER_MODE);

#ifdef DANUBE_DMA_DEBUG_MUTEX
	restore_flags(flags);
#endif

	//	Return
	return MEI_SUCCESS;

} //	end of "meiDebugWrite(..."

/**
 * ARC Debug Memory Access for reading.
 * This function used for direct, address-based access to ARC memory.
 * 
 * \param  	srcaddr	  	The address to read
 * \param  	databuffer  	Pointer to data
 * \param	databuffsize	The number of DWORDs to read
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR meiDebugRead(u32 srcaddr, u32 *databuff, u32 databuffsize)
{
	u32 i;
	u32 temp = 0x0;
	u32 address = 0x0;
	u32 *buffer = 0x0;
	u32 flags;
	
#ifdef DANUBE_DMA_DEBUG_MUTEX
	save_flags(flags);
	cli();
#endif	

	//	Open the debug port before DMP memory read
	meiControlModeSwitch(MEI_MASTER_MODE);

	meiLongwordWrite((u32)MEI_DEBUG_DEC, MEI_DEBUG_DEC_DMP2_MASK);

	//	For the requested length, write the address and read the data
	address = srcaddr;
	buffer = databuff;
	for (i=0; i<databuffsize; i++)
	{
		_meiDebugLongWordRead(MEI_DEBUG_DEC_DMP2_MASK,address,&temp);
		*buffer=temp;
		address += 4;
		buffer++;
	} //	end of "for(..."

	//	Close the debug port after DMP memory read
	meiControlModeSwitch(JTAG_MASTER_MODE);
		
#ifdef DANUBE_DMA_DEBUG_MUTEX
	restore_flags(flags);
#endif

	//	Return
	return MEI_SUCCESS;

} //	end of "meiDebugRead(..."

/**
 * Send a message to ARC MailBox.
 * This function sends a message to ARC Mailbox via ARC DMA interface.
 * 
 * \param  	msgsrcbuffer  	Pointer to message.
 * \param	msgsize		The number of words to write.
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR meiMailboxWrite(u16 *msgsrcbuffer, u16 msgsize)
{
	int i;
	u32 arc_mailbox_status = 0x0;
	u32 temp=0;
	MEI_ERROR meiMailboxError = MEI_SUCCESS;

	//	Write to mailbox
	meiMailboxError = meiDMAWrite(MEI_TO_ARC_MAILBOX, (u32*)msgsrcbuffer, msgsize/2);
	meiMailboxError = meiDMAWrite(MEI_TO_ARC_MAILBOXR, (u32 *)(&temp), 1); 

	//	Notify arc that mailbox write completed
	cmv_waiting=1;
	meiLongwordWrite((u32)MEI_TO_ARC_INT, MEI_TO_ARC_MSGAV);
	
	i=0;
  	while(i<WHILE_DELAY){ // wait for ARC to clear the bit
		meiLongwordRead((u32)MEI_TO_ARC_INT, &arc_mailbox_status);
		if((arc_mailbox_status & MEI_TO_ARC_MSGAV) != MEI_TO_ARC_MSGAV)
			break;
		i++;
		if(i==WHILE_DELAY){
			DANUBE_MEI_EMSG("\n\n MEI_TO_ARC_MSGAV not cleared by ARC");
			meiMailboxError = MEI_FAILURE;
		}	
	}      
		
	//	Return
	return meiMailboxError;

} //	end of "meiMailboxWrite(..."

/**
 * Read a message from ARC MailBox.
 * This function reads a message from ARC Mailbox via ARC DMA interface.
 * 
 * \param  	msgsrcbuffer  	Pointer to message.
 * \param	msgsize		The number of words to read
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR meiMailboxRead(u16 *msgdestbuffer, u16 msgsize)
{
	MEI_ERROR meiMailboxError = MEI_SUCCESS;
	//	Read from mailbox
	meiMailboxError = meiDMARead(ARC_TO_MEI_MAILBOX, (u32*)msgdestbuffer, msgsize/2);

	//	Notify arc that mailbox read completed
	meiLongwordWrite((u32)ARC_TO_MEI_INT, ARC_TO_MEI_MSGAV);

	//	Return
	return meiMailboxError;

} //	end of "meiMailboxRead(..."

/**
 * Download boot pages to ARC.
 * This function downloads boot pages to ARC.
 * 
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR meiDownloadBootPages(void)
{
	int boot_loop;
	int page_size;
	u32 dest_addr;
	
	/*
	**	DMA the boot code page(s)
	*/
#ifndef HEADER_SWAP
	for( boot_loop = 1; boot_loop < le32_to_cpu(img_hdr->count); boot_loop++)
#else
	for( boot_loop = 1; boot_loop < (img_hdr->count); boot_loop++)
#endif
	{
#ifndef HEADER_SWAP
		if( le32_to_cpu(img_hdr->page[boot_loop].p_size) & BOOT_FLAG )
#else
		if( (img_hdr->page[boot_loop].p_size) & BOOT_FLAG )
#endif
		{
			page_size = meiGetPage( boot_loop, GET_PROG, MAXSWAPSIZE, mei_arc_swap_buff, &dest_addr);
			if( page_size > 0)
			{
				meiDMAWrite(dest_addr, mei_arc_swap_buff, page_size);
			}
		}
#ifndef HEADER_SWAP
		if( le32_to_cpu(img_hdr->page[boot_loop].d_size) & BOOT_FLAG)
#else
		if( (img_hdr->page[boot_loop].d_size) & BOOT_FLAG)
#endif
		{
			page_size = meiGetPage( boot_loop, GET_DATA, MAXSWAPSIZE, mei_arc_swap_buff, &dest_addr);
			if( page_size > 0)
			{
				meiDMAWrite( dest_addr, mei_arc_swap_buff, page_size);
			}
		}
	}
	return MEI_SUCCESS;
}

/**
 * Download boot code to ARC.
 * This function downloads boot code to ARC.
 * 
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR meiDownloadBootCode(void)
{
	u32 arc_debug_data = ACL_CLK_MODE_ENABLE; //0x10

	printk("%s %d\n",__FUNCTION__,__LINE__);
	meiMailboxInterruptsDisable();

	//	Switch arc control from JTAG mode to MEI mode
	meiControlModeSwitch(MEI_MASTER_MODE);
	//enable ac_clk signal	
	_meiDebugLongWordRead(MEI_DEBUG_DEC_DMP1_MASK,CRI_CCR0,&arc_debug_data);
	arc_debug_data |=ACL_CLK_MODE_ENABLE;	
	_meiDebugLongWordWrite(MEI_DEBUG_DEC_DMP1_MASK,CRI_CCR0,arc_debug_data);
	//Switch arc control from MEI mode to JTAG mode
	meiControlModeSwitch(JTAG_MASTER_MODE);

	meiDownloadBootPages();

	return MEI_SUCCESS;

} //	end of "meiDownloadBootCode(..."
//#endif

/**
 * Halt the ARC.
 * This function halts the ARC.
 * 
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR meiHaltArc(void)
{
	u32 arc_debug_data = 0x0;

	DANUBE_MEI_DMSG("Line=%X\n",__LINE__);
	//	Switch arc control from JTAG mode to MEI mode
	meiControlModeSwitch(MEI_MASTER_MODE);
	_meiDebugLongWordRead(MEI_DEBUG_DEC_AUX_MASK,ARC_DEBUG,&arc_debug_data);
	arc_debug_data |= (BIT1);
	_meiDebugLongWordWrite(MEI_DEBUG_DEC_AUX_MASK,ARC_DEBUG,arc_debug_data);
	//	Switch arc control from MEI mode to JTAG mode
	meiControlModeSwitch(JTAG_MASTER_MODE);
	arc_halt_flag=1;
	//	Return
	return MEI_SUCCESS;

} //	end of "meiHalt(..."

/**
 * Run the ARC.
 * This function runs the ARC.
 * 
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR meiRunArc(void)
{
	u32 arc_debug_data = 0x0;

	DANUBE_MEI_DMSG("Line=%X\n",__LINE__);
	
	//	Switch arc control from JTAG mode to MEI mode- write '1' to bit0
	meiControlModeSwitch(MEI_MASTER_MODE);
	_meiDebugLongWordRead(MEI_DEBUG_DEC_AUX_MASK,AUX_STATUS,&arc_debug_data);

	//	Write debug data reg with content ANDd with 0xFDFFFFFF (halt bit cleared)
	arc_debug_data &= ~(BIT25);
	_meiDebugLongWordWrite(MEI_DEBUG_DEC_AUX_MASK,AUX_STATUS,arc_debug_data);

	//	Switch arc control from MEI mode to JTAG mode- write '0' to bit0
	meiControlModeSwitch(JTAG_MASTER_MODE);
	//	Enable mask for arc codeswap interrupts
	meiMailboxInterruptsEnable();
	arc_halt_flag=0;

	//	Return
	return MEI_SUCCESS;

} //	end of "meiActivate(..."

/**
 * Reset the ARC.
 * This function resets the ARC.
 * 
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR meiResetARC(void)
{
		
	u32 arc_debug_data = 0; 
	
	DANUBE_MEI_DMSG("Line=%d\n",__LINE__);
	
	meiLongwordRead((u32)DANUBE_RCU_REQ,&arc_debug_data);
	meiLongwordWrite((u32)DANUBE_RCU_REQ,arc_debug_data |DANBUE_RCU_RST_REQ_DFE |DANBUE_RCU_RST_REQ_AFE);
	mdelay(10);
	meiLongwordWrite((u32)DANUBE_RCU_REQ,arc_debug_data);

	// reset ARC
	meiLongwordWrite((u32)MEI_RST_CONTROL, SOFT_RESET);
	mdelay(10);
	meiLongwordWrite((u32)MEI_RST_CONTROL, 0);

	meiMailboxInterruptsDisable();	
	sema_init(&mei_sema, 1);
	reset_arc_flag = 1;
	return MEI_SUCCESS;
}
	
/**
 * Reset the ARC, download boot codes, and run the ARC.
 * This function resets the ARC, downloads boot codes to ARC, and runs the ARC.
 * 
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
static MEI_ERROR meiRunAdslModem(void)
{
	int nSize=0,idx=0;

	DANUBE_MEI_DMSG("Line=%d\n",__LINE__);
	img_hdr=(ARC_IMG_HDR *)adsl_mem_info[0].address;
#if	defined(HEADER_SWAP)
	if ((img_hdr->count) * sizeof(ARC_SWP_PAGE_HDR) > SDRAM_SEGMENT_SIZE )
#else //define(HEADER_SWAP)
	if (le32_to_cpu(img_hdr->count) * sizeof(ARC_SWP_PAGE_HDR) > SDRAM_SEGMENT_SIZE )
#endif //define(HEADER_SWAP)
	{
		DANUBE_MEI_EMSG("segment_size is smaller than firmware header size\n");
		return -1;
	}
	// check image size 
	for (idx=0;idx<MAX_BAR_REGISTERS;idx++)
	{
		nSize+=adsl_mem_info[idx].nCopy;
	}
	if (nSize != image_size)
	{
		DANUBE_MEI_EMSG("Firmware download is not completed. \nPlease download firmware again!\n");
		return -1;
	}
	// TODO: check crc
	///
	if (reset_arc_flag == 0)
	{
		u32 arc_debug_data;

		meiResetARC();
		meiControlModeSwitch(MEI_MASTER_MODE);
		//enable ac_clk signal	
		_meiDebugLongWordRead(MEI_DEBUG_DEC_DMP1_MASK,CRI_CCR0,&arc_debug_data);
		arc_debug_data |=ACL_CLK_MODE_ENABLE;		
		_meiDebugLongWordWrite(MEI_DEBUG_DEC_DMP1_MASK,CRI_CCR0,arc_debug_data);	
		meiControlModeSwitch(JTAG_MASTER_MODE);
		meiHaltArc();			
		update_bar_register(nBar);			
	}
	reset_arc_flag = 0;
	if (arc_halt_flag==0)
        {
		meiHaltArc();	
        }
	DANUBE_MEI_DMSG("Starting to meiDownloadBootCode\n");
	
        meiDownloadBootCode();

	meiRunArc();
		
		
	return MEI_SUCCESS;
}

/**
 * Get the page's data pointer
 * This function caculats the data address from the firmware header.
 * 
 * \param	Page		The page number.
 * \param	data		Data page or program page.
 * \param	MaxSize		The maximum size to read.
 * \param	Buffer		Pointer to data.
 * \param	Dest		Pointer to the destination address.
 * \return	The number of bytes to read.
 * \ingroup	Internal
 */
static int meiGetPage( u32 Page, u32 data, u32 MaxSize, u32 *Buffer, u32 *Dest)
{
	u32	size;
	u32	i;
	u32	*p;
	u32	idx,offset,nBar=0;

	if( Page > img_hdr->count)
		return -2;
	/*
	**	Get program or data size, depending on "data" flag
	*/
#ifndef HEADER_SWAP
	size = (data == GET_DATA) ? le32_to_cpu(img_hdr->page[ Page].d_size) : le32_to_cpu(img_hdr->page[ Page].p_size);
#else
	size = (data == GET_DATA) ? (img_hdr->page[ Page].d_size) : (img_hdr->page[ Page].p_size);
#endif
	size &= BOOT_FLAG_MASK; 	//	Clear boot bit!
	if( size > MaxSize)
		return -1;

	if( size == 0)
		return 0;
	/*
	**	Get program or data offset, depending on "data" flag
	*/
#ifndef HEADER_SWAP
	i = data ? le32_to_cpu(img_hdr->page[ Page].d_offset) : le32_to_cpu(img_hdr->page[ Page].p_offset);
#else
	i = data ? (img_hdr->page[ Page].d_offset) : (img_hdr->page[ Page].p_offset);
#endif

	/*
	**	Copy data/program to buffer
	*/


	idx = i/SDRAM_SEGMENT_SIZE;
	offset=i%SDRAM_SEGMENT_SIZE;
	p = (u32 *)((u8 *)adsl_mem_info[idx].address + offset);
	
	for(i = 0; i < size; i++)
	{
		if (offset+i*4 - (nBar*SDRAM_SEGMENT_SIZE) >= SDRAM_SEGMENT_SIZE)
		{
			idx++;
			nBar++;
			p = (u32 *)((u8 *)KSEG1ADDR(adsl_mem_info[idx].address));
		}
		Buffer[i] = *p++;
#ifdef BOOT_SWAP
#ifndef IMAGE_SWAP
		Buffer[i] = le32_to_cpu(Buffer[i]);
#endif
#endif
	}

	/*
	**	Pass back data/program destination address
	*/
#ifndef HEADER_SWAP	
	*Dest = data ? le32_to_cpu(img_hdr->page[Page].d_dest) : le32_to_cpu(img_hdr->page[Page].p_dest);
#else
	*Dest = data ? (img_hdr->page[Page].d_dest) : (img_hdr->page[Page].p_dest);
#endif

	return size;
}

////////////////makeCMV(Opcode, Group, Address, Index, Size, Data), CMV in u16 TxMessage[MSG_LENGTH]///////////////////////////

/**
 * Compose a message.
 * This function compose a message from opcode, group, address, index, size, and data
 * 
 * \param	opcode		The message opcode
 * \param	group		The message group number
 * \param	address		The message address.
 * \param	index		The message index.
 * \param	size		The number of words to read/write.
 * \param	data		The pointer to data.
 * \param	CMVMSG		The pointer to message buffer.
 * \ingroup	Internal
 */
void makeCMV(u8 opcode, u8 group, u16 address, u16 index, int size, u16 * data,u16 *CMVMSG)
{
	memset(CMVMSG, 0, MSG_LENGTH*2);
	CMVMSG[0]= (opcode<<4) + (size&0xf);
	CMVMSG[1]= (((index==0)?0:1)<<7) + (group&0x7f);
	CMVMSG[2]= address;
	CMVMSG[3]= index;
	if(opcode == H2D_CMV_WRITE)
		memcpy(CMVMSG+4, data, size*2);
	return;
}


/**
 * Send a message to ARC and read the response
 * This function sends a message to arc, waits the response, and reads the responses.
 * 
 * \param	request		Pointer to the request
 * \param	reply		Wait reply or not.
 * \param	response	Pointer to the response
 * \return	MEI_SUCCESS or MEI_FAILURE
 * \ingroup	Internal
 */
MEI_ERROR meiCMV(u16 * request, int reply, u16 *response)            // write cmv to arc, if reply needed, wait for reply
{
  	MEI_ERROR meierror;
  	wait_queue_t wait;
  
  	cmv_reply=reply;
  	memcpy(CMV_TxMsg,request,MSG_LENGTH*2);
  
  	meierror = meiMailboxWrite(CMV_TxMsg, MSG_LENGTH);

  	if(meierror != MEI_SUCCESS){
          	DANUBE_MEI_EMSG("\n\n MailboxWrite Fail.");
          	return meierror;
  	}
  	else{
          	cmv_count++;
	}

  	if(cmv_reply == NO_REPLY)
          	return MEI_SUCCESS;

  	init_waitqueue_entry(&wait, current);
  	add_wait_queue(&wait_queue_arcmsgav, &wait);
  	set_current_state(TASK_INTERRUPTIBLE);

  	if(arcmsgav==1){
          	set_current_state(TASK_RUNNING);
          	remove_wait_queue(&wait_queue_arcmsgav, &wait);
  	}
  	else{
          	schedule_timeout(CMV_TIMEOUT);
        	remove_wait_queue(&wait_queue_arcmsgav, &wait);
  	}
	if(arcmsgav==0){//CMV_timeout
		cmv_waiting=0;
		arcmsgav=0;
		DANUBE_MEI_EMSG("\nmeiCMV: MEI_MAILBOX_TIMEOUT\n");
		return MEI_MAILBOX_TIMEOUT; 	
	}
	else{
  		arcmsgav=0;
 	 	reply_count++;
 	 	memcpy(response,CMV_RxMsg,MSG_LENGTH*2);
  		return MEI_SUCCESS;
	}
 	return MEI_SUCCESS;
}

/////////////////////          Interrupt handler     /////////////////////////
/**
 * Disable ARC to MEI interrupt
 * 
 * \ingroup	Internal
 */
static void meiMailboxInterruptsDisable(void)
{
	meiLongwordWrite((u32)ARC_TO_MEI_INT_MASK, 0x0);
} //	end of "meiMailboxInterruptsDisable(..."

/**
 * Eable ARC to MEI interrupt
 * 
 * \ingroup	Internal
 */
static void meiMailboxInterruptsEnable(void)
{
	meiLongwordWrite((u32)ARC_TO_MEI_INT_MASK, MSGAV_EN); 
} //	end of "meiMailboxInterruptsEnable(..."


/**
 * MEI interrupt handler
 * 
 * \param int1	
 * \param void0
 * \param regs	Pointer to the structure of danube mips registers
 * \ingroup	Internal
 */
static void mei_interrupt_arcmsgav(int int1, void * void0, struct pt_regs * regs)
{
        u32 scratch;

	DANUBE_MEI_DMSG("LINE=%d\n",__LINE__);
	
#if defined(DFE_LOOPBACK) && defined(DFE_PING_TEST)
	dfe_loopback_irq_handler();
        return;
#endif	//DFE_LOOPBACK
	
        meiDebugRead(ARC_MEI_MAILBOXR, &scratch, 1);
        if(scratch & OMB_CODESWAP_MESSAGE_MSG_TYPE_MASK)
        {
		DANUBE_MEI_EMSG("\n\n Receive Code Swap Request interrupt!!!");
		return;
	}
        else{    // normal message
		meiMailboxRead(CMV_RxMsg, MSG_LENGTH);
#if 0
		{
			int msg_idx=0;
			printk("got interrupt\n");
			for(msg_idx=0;msg_idx<MSG_LENGTH;msg_idx++)
				{
				printk("%04X ",CMV_RxMsg[msg_idx]);
				if(msg_idx%8==7)
							printk("\n");
				}
					printk("\n");
		}
#endif
		if(cmv_waiting==1)
		{
                      arcmsgav=1;
                      cmv_waiting=0;
                      wake_up_interruptible(&wait_queue_arcmsgav);
		}
		else{
			indicator_count++;
			memcpy((char *)Recent_indicator, (char *)CMV_RxMsg,  MSG_LENGTH *2);
			if(((CMV_RxMsg[0]&0xff0)>>4)==D2H_AUTONOMOUS_MODEM_READY_MSG) // arc ready
			{	//check ARC ready message
				
				DANUBE_MEI_DMSG("Got MODEM_READY_MSG\n");
				up(&mei_sema);	// allow cmv access
			}			
		}
        }

	mask_and_ack_danube_irq(DANUBE_MEI_INT);
        return;
}

////////////////////////hdlc ////////////////

/**
 * Get the hdlc status
 * 
 * \return	HDLC status
 * \ingroup	Internal
 */
static unsigned int ifx_me_hdlc_status(void)
{
	u16 CMVMSG[MSG_LENGTH]; 
	int ret;

	if (showtime!=1)
		return -ENETRESET;
	
	makeCMV(H2D_CMV_READ, STAT, 14, 0, 1, NULL,CMVMSG);	//Get HDLC status 
	ret = mei_ioctl((struct inode *)0,NULL, DANUBE_MEI_CMV_WINHOST, (unsigned long)CMVMSG);
	if (ret != 0)
	{
		return -EIO;
	}
	return CMVMSG[4]&0x0F;
}

/**
 * Check if the me is reslved.
 * 
 * \param	status		the me status
 * \return	ME_HDLC_UNRESOLVED or ME_HDLC_RESOLVED
 * \ingroup	Internal
 */
int ifx_me_is_resloved(int status)
{
	u16 CMVMSG[MSG_LENGTH];
	int ret;
	
	if (status == ME_HDLC_MSG_QUEUED || status == ME_HDLC_MSG_SENT)
		return ME_HDLC_UNRESOLVED;
	if (status == ME_HDLC_IDLE)
	{
		makeCMV(H2D_CMV_READ, CNTL, 2, 0, 1, NULL,CMVMSG);	//Get ME-HDLC Control
		ret = mei_ioctl((struct inode *)0,NULL, DANUBE_MEI_CMV_WINHOST, (unsigned long)CMVMSG);
		if (ret != 0)
		{
			return IFX_POP_EOC_FAIL;
		}
		if (CMVMSG[4]&(1<<0))
		{
			return ME_HDLC_UNRESOLVED;
		}
		
	}
	return ME_HDLC_RESOLVED;
}

static int _ifx_mei_hdlc_send(unsigned char *hdlc_pkt,int len,int max_length)
{
	int ret;
	u16 CMVMSG[MSG_LENGTH];
	u16 data=0;
	u16 pkt_len=len;
	if (pkt_len > max_length)
	{
		printk("Exceed maximum eoc message length\n");
		return -ENOBUFS;
	}
	//while(pkt_len > 0)
	{		
		makeCMV(H2D_CMV_WRITE, INFO, 81, 0, (pkt_len+1)/2,(u16 *)hdlc_pkt,CMVMSG);	//Write clear eoc message to ARC
		ret = mei_ioctl((struct inode *)0,NULL, DANUBE_MEI_CMV_WINHOST, (unsigned long)CMVMSG);
		if (ret != 0)
		{
			return -EIO;
		}
		
		makeCMV(H2D_CMV_WRITE, INFO, 83, 2, 1,&pkt_len,CMVMSG);	//Update tx message length
		ret = mei_ioctl((struct inode *)0,NULL, DANUBE_MEI_CMV_WINHOST, (unsigned long)CMVMSG);
		if (ret != 0)
		{
			return -EIO;
		}
		
		data = (1<<0);
		makeCMV(H2D_CMV_WRITE, CNTL, 2, 0, 1,&data,CMVMSG);	//Start to send
		ret = mei_ioctl((struct inode *)0,NULL, DANUBE_MEI_CMV_WINHOST, (unsigned long)CMVMSG);
		if (ret != 0)
		{
			return -EIO;
		}
		return 0;
	}
}

/**
 * Send hdlc packets
 * 
 * \param	hdlc_pkt	Pointer to hdlc packet
 * \param	hdlc_pkt_len	The number of bytes to send
 * \return	success or failure.
 * \ingroup	Internal
 */
int ifx_mei_hdlc_send(char *hdlc_pkt,int hdlc_pkt_len)
{
	int hdlc_status=0;
	u16 CMVMSG[MSG_LENGTH];
	int max_hdlc_tx_length=0,ret=0,retry=0;
	
	while(retry<10)
	{
		hdlc_status = ifx_me_hdlc_status();
		if (ifx_me_is_resloved(hdlc_status)==ME_HDLC_RESOLVED) // arc ready to send HDLC message
		{
			makeCMV(H2D_CMV_READ, INFO, 83, 0, 1, NULL,CMVMSG);	//Get Maximum Allowed HDLC Tx Message Length
			ret = mei_ioctl((struct inode *)0,NULL, DANUBE_MEI_CMV_WINHOST, (unsigned long)CMVMSG);
			if (ret != 0)
			{
				return -EIO;
			}
			max_hdlc_tx_length = CMVMSG[4];
			ret = _ifx_mei_hdlc_send(hdlc_pkt,hdlc_pkt_len,max_hdlc_tx_length);
			return ret;
		}
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(10);
	}
	return -EBUSY;
}

/**
 * Read the hdlc packets
 * 
 * \param	hdlc_pkt	Pointer to hdlc packet
 * \param	hdlc_pkt_len	The maximum number of bytes to read
 * \return	The number of bytes which reads.
 * \ingroup	Internal
 */
int ifx_mei_hdlc_read(char *hdlc_pkt,int max_hdlc_pkt_len)
{
	u16 CMVMSG[MSG_LENGTH]; 
	int msg_read_len,ret=0,pkt_len=0,retry = 0;
		
	while(retry<10)
	{
		ret = ifx_me_hdlc_status();
		if (ret == ME_HDLC_RESP_RCVD)
		{
			int current_size=0;
			makeCMV(H2D_CMV_READ, INFO, 83, 3, 1, NULL,CMVMSG);	//Get EoC packet length
			ret = mei_ioctl((struct inode *)0,NULL, DANUBE_MEI_CMV_WINHOST, (unsigned long)CMVMSG);
			if (ret != 0)
			{
				return -EIO;
			}
	
			pkt_len = CMVMSG[4];
			if (pkt_len > max_hdlc_pkt_len)
			{
				ret = -ENOMEM;
				goto error;
			}
			while( current_size < pkt_len)
			{
				if (pkt_len - current_size >(MSG_LENGTH*2-8))
					msg_read_len = (MSG_LENGTH*2-8);
				else
					msg_read_len = pkt_len - (current_size);
				makeCMV(H2D_CMV_READ, INFO, 82, 0 + (current_size/2), (msg_read_len+1)/2, NULL,CMVMSG);	//Get hdlc packet
				ret = mei_ioctl((struct inode *)0,NULL, DANUBE_MEI_CMV_WINHOST, (unsigned long)CMVMSG);
				if (ret != 0)
				{
					goto error;
				}
				memcpy(hdlc_pkt+current_size,&CMVMSG[4],msg_read_len);
				current_size +=msg_read_len;
			}
			ret = current_size;
			break;
		}else
		{
			ret = -ENODATA;
		}
		
		retry++;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(10);
		
	}
	return ret;
error:
	
	return ret;
}

////////////////////////hdlc ////////////////

#ifdef DANUBE_CLEAR_EOC
/////////////////////// clearEoC, int ifx_pop_eoc(sk_buff * pkt)  //////////
int ifx_pop_eoc(struct sk_buff * pkt)
{
	danube_clreoc_pkt * current;
	if(showtime!=1){
		dev_kfree_skb(pkt);
		return IFX_POP_EOC_FAIL;
	}
	if((pkt->len)>clreoc_max_tx_len){
		dev_kfree_skb(pkt);
		return IFX_POP_EOC_FAIL;
	}
	current = list_entry(clreoc_list.next, danube_clreoc_pkt, list);
	while(1){
		if(current->len==0){
			memcpy(current->command, pkt->data, pkt->len);
			current->len=pkt->len;
			break;
		}
		else{
			if((current->list).next==&clreoc_list){
				dev_kfree_skb(pkt);
				return IFX_POP_EOC_FAIL;	//buffer full
			}
			current = list_entry((current->list).next,danube_clreoc_pkt, list);
		}
	}	
	wake_up_interruptible(&wait_queue_clreoc);
	
	dev_kfree_skb(pkt);
	return IFX_POP_EOC_DONE;
}	
#endif

#ifdef IFX_DYING_GASP
static int lop_poll(void *unused)
{
	
	struct task_struct *tsk = current;
	daemonize();
	strcpy(tsk->comm, "kloppoll");
	sigfillset(&tsk->blocked);

	while(lop_poll_shutdown==0)
	{
		interruptible_sleep_on_timeout(&wait_queue_dying_gasp, 1); 
#ifdef CONFIG_CPU_DANUBE_E //000003:fchang
		if(showtime&&((*DANUBE_GPIO_P0_IN)&BIT2)==0x0)	{//000003:fchang
#else //000003:fchang
		if(showtime&&((*DANUBE_GPIO_P1_IN)&BIT15)==0x0)	{//000003:fchang
#endif //CONFIG_CPU_DANUBE_E
			mei_ioctl((struct inode *)0,NULL, DANUBE_MEI_WRITEDEBUG, (unsigned long)&lop_debugwr);
			DANUBE_MEI_EMSG("send dying gasp..\n");}
	}
	complete_and_exit(&lop_thread_exit,0);
	return 0;	
	}
static int lop_poll_init(void)
{
	lop_poll_shutdown = 0;
	kernel_thread(lop_poll, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);
	return 0;
}

#endif //IFX_DYING_GASP

//////////////////////  Driver Structure	///////////////////////

/**
 * Free the memory for ARC firmware
 * 
 * \param	type	Free all memory or free the unused memory after showtime
 * \ingroup	Internal
 */
static int free_image_buffer(int type)
{
	int idx=0;
	for (idx=0;idx < MAX_BAR_REGISTERS;idx++)
	{
		if (type == FREE_ALL || adsl_mem_info[idx].type == type)
		{
			if (adsl_mem_info[idx].size > 0)
			{
				kfree(adsl_mem_info[idx].address);
				adsl_mem_info[idx].address=0;
				adsl_mem_info[idx].size=0;
				adsl_mem_info[idx].type=0;
				adsl_mem_info[idx].nCopy=0;
			}
		}
		DANUBE_MEI_DMSG("meminfo[%d].type=%d,size=%d,addr=%X\n",idx,adsl_mem_info[idx].type,adsl_mem_info[idx].size,adsl_mem_info[idx].address);
	}
	return 0;
}

/**
 * Allocate memory for ARC firmware
 * 
 * \param	size		The number of bytes to allocate.
 * \param	adsl_mem_info	Pointer to firmware information.
 * \ingroup	Internal
 */
static int alloc_processor_memory(unsigned long size,smmu_mem_info_t *adsl_mem_info)
{
	char *mem_ptr=NULL;
	int idx=0;
	long total_size=0;
	long img_size = size;
	int err=0;

	// Alloc Swap Pages
	while( img_size > 0 && idx < MAX_BAR_REGISTERS)
	{			
		// skip bar15 for XDATA usage.
#ifndef DFE_LOOPBACK		
		if(idx == XDATA_REGISTER)
			idx++;
#endif		
		if(idx == MAX_BAR_REGISTERS-1)
		{
			//allocate 1MB memory for bar16
			mem_ptr = kmalloc(1024*1024,GFP_ATOMIC);
			adsl_mem_info[idx].size = 1024*1024;
		}else 
		{			
			mem_ptr = kmalloc(SDRAM_SEGMENT_SIZE,GFP_ATOMIC);
			adsl_mem_info[idx].size = SDRAM_SEGMENT_SIZE;
		}
		if (mem_ptr == NULL)
		{
			DANUBE_MEI_EMSG("kmalloc memory fail!\n");
			err=-ENOMEM;
			goto allocate_error;
		}
		adsl_mem_info[idx].address = mem_ptr;

		img_size -= SDRAM_SEGMENT_SIZE;
		total_size +=SDRAM_SEGMENT_SIZE;
		idx++;
		DANUBE_MEI_DMSG("alloc memory idx=%d,img_size=%d\n",idx,img_size);
	}
	if (img_size >0)
	{
		DANUBE_MEI_EMSG("Image size is too large!\n");
		err=-EFBIG;
		goto allocate_error;
	}
	err = idx;
	return err;
	
allocate_error:
	free_image_buffer(FREE_ALL);
	return err;
}

/**
 * Program the BAR registers
 * 
 * \param	nTotalBar	The number of bar to program.
 * \ingroup	Internal
 */
static int update_bar_register(int nTotalBar)
{
	int idx=0;
		
	for (idx=0;idx<nTotalBar;idx++)
	{
		//skip XDATA register
		if (idx == XDATA_REGISTER)
			idx++;
		meiLongwordWrite((u32)MEI_XMEM_BAR_BASE+idx*4,(((uint32_t)adsl_mem_info[idx].address)&0x0FFFFFFF));
		DANUBE_MEI_DMSG("BAR%d=%08X, addr=%08X\n",idx,(((uint32_t)adsl_mem_info[idx].address)&0x0FFFFFFF),(((uint32_t)adsl_mem_info[idx].address)));
	}
	for (idx=nTotalBar;idx<MAX_BAR_REGISTERS;idx++)
	{
		if (idx == XDATA_REGISTER)
			idx++;
		meiLongwordWrite((u32)MEI_XMEM_BAR_BASE+idx*4,(((uint32_t)adsl_mem_info[nTotalBar-1].address)&0x0FFFFFFF));			
	}

	meiLongwordWrite((u32)MEI_XMEM_BAR_BASE+XDATA_REGISTER*4,(((uint32_t)adsl_mem_info[XDATA_REGISTER].address)&0x0FFFFFFF));
	// update MEI_XDATA_BASE_SH
	DANUBE_MEI_DMSG("update bar15 register with %08lX\n",((unsigned long)adsl_mem_info[XDATA_REGISTER].address) & 0x0FFFFFFF);
	meiLongwordWrite((u32)MEI_XDATA_BASE_SH, ((unsigned long)adsl_mem_info[XDATA_REGISTER].address) & 0x0FFFFFFF);
	return MEI_SUCCESS;
}

/**
 * Copy the firmware to BARs memory.
 * 
 * \param	filp		Pointer to the file structure.
 * \param	buf		Pointer to the data.
 * \param	size		The number of bytes to copy.
 * \param	loff		The file offset.
 * \return	The current file position.
 * \ingroup	Internal
 */
static ssize_t mei_write(struct file * filp, const char * buf, size_t size, loff_t * loff)
{
	ARC_IMG_HDR img_hdr_tmp,*img_hdr;
	
	size_t nRead=0,nCopy=0;
	char *mem_ptr;
	ssize_t retval = -ENOMEM;
	int idx=0;

	if ( *loff == 0)
	{
		if (size < sizeof(img_hdr))
		{
			DANUBE_MEI_EMSG("Firmware size is too small!\n");
			return retval;
		}
		copy_from_user((char *)&img_hdr_tmp, buf, sizeof(img_hdr_tmp));
		image_size = le32_to_cpu(img_hdr_tmp.size)+ 8;// header of image_size and crc are not included.
		if (image_size > 1024*1024)
		{
			DANUBE_MEI_EMSG("Firmware size is too large!\n");
			return retval;			
		}
		// check if arc is halt
		if (arc_halt_flag!=1)
		{	
			printk("start to reset arc\n");
			meiResetARC();
			meiHaltArc();
		}
	
		// reset part of PPE 
		*(unsigned long *)(DANUBE_PPE32_SRST) = 0xC30;
		*(unsigned long *)(DANUBE_PPE32_SRST) = 0xFFF;

		free_image_buffer(FREE_ALL); //free all

		retval = alloc_processor_memory(image_size,adsl_mem_info);
		if (retval<0)
		{
			DANUBE_MEI_EMSG("Error: No memory space left.\n");
			goto error;
		}

		for (idx=0;idx<retval;idx++)
		{
			//skip XDATA register
			if (idx == XDATA_REGISTER)
				idx++;
			if (idx*SDRAM_SEGMENT_SIZE < le32_to_cpu(img_hdr_tmp.page[0].p_offset))
			{
				adsl_mem_info[idx].type = FREE_RELOAD;
			}
			else
			{
				adsl_mem_info[idx].type = FREE_SHOWTIME  ;
			}

		}
		nBar=retval;

		img_hdr=(ARC_IMG_HDR *)adsl_mem_info[0].address;
		printk("update_img_hdr = %08lX\n",(unsigned long)img_hdr);

		adsl_mem_info[XDATA_REGISTER].address = kmalloc(SDRAM_SEGMENT_SIZE,GFP_ATOMIC);
		adsl_mem_info[XDATA_REGISTER].size = SDRAM_SEGMENT_SIZE;
		if (adsl_mem_info[XDATA_REGISTER].address == NULL)
		{
			DANUBE_MEI_EMSG("kmalloc memory fail!\n");
			retval=-ENOMEM;
			goto error;
		}
		adsl_mem_info[XDATA_REGISTER].type=FREE_RELOAD;
		update_bar_register(nBar);

		
	}else if (image_size ==0)
	{
		DANUBE_MEI_EMSG("Error: Firmware size=0! \n");
			goto error;
	}else
	{
		if (arc_halt_flag==0)
		{
			DANUBE_MEI_EMSG("Please download the firmware from the beginning of the firmware!\n");
			goto error;
		}
	}

	nRead = 0;
	while(nRead<size)
	{
		long offset = ((long)(*loff)+nRead)%SDRAM_SEGMENT_SIZE;
		idx = (((long)(*loff))+nRead)/SDRAM_SEGMENT_SIZE;
		mem_ptr=(char *)KSEG1ADDR((unsigned long)(adsl_mem_info[idx].address) +offset);
		if ( (size-nRead+offset) > SDRAM_SEGMENT_SIZE)
			nCopy = SDRAM_SEGMENT_SIZE-offset;
		else
			nCopy = size - nRead;
		copy_from_user(mem_ptr , buf+nRead, nCopy);
#ifdef IMAGE_SWAP		
		for(offset=0;offset<(nCopy/4);offset++)
		{
			((unsigned long *)mem_ptr)[offset]=le32_to_cpu(((unsigned long *)mem_ptr)[offset]);
		}
#endif	//IMAGE_SWAP	
		nRead +=nCopy;
		adsl_mem_info[idx].nCopy +=nCopy;
	}
	
#if	( defined(HEADER_SWAP) && !defined(IMAGE_SWAP)) || (defined(IMAGE_SWAP) && !defined(HEADER_SWAP))
	if (*loff==0)
	{

		for(idx=0;idx<(sizeof(ARC_IMG_HDR)+ (le32_to_cpu(img_hdr_tmp.count) -1)*sizeof(ARC_SWP_PAGE_HDR))/4;idx++)
		{
			((unsigned long *)img_hdr)[idx]=le32_to_cpu(((unsigned long *)img_hdr)[idx]);
		}
	}
#endif //( defined(HEADER_SWAP) && !defined(IMAGE_SWAP)) || (defined(IMAGE_SWAP) && !defined(HEADER_SWAP))
	DANUBE_MEI_DMSG("size=%X,loff=%08X\n",size,(unsigned long)*loff);

	*loff += size;
	return size;
error:
	free_image_buffer(FREE_ALL);

	return retval;
}

/**
 * MEI IO controls for user space accessing
 * 
 * \param	ino		Pointer to the stucture of inode.
 * \param	fil		Pointer to the stucture of file.
 * \param	command		The ioctl command.
 * \param	lon		The address of data.
 * \return	Success or failure.
 * \ingroup	Internal
 */
int mei_ioctl(struct inode * ino, struct file * fil, unsigned int command, unsigned long lon)
{
        int i;

        int meierr=MEI_SUCCESS;
	meireg regrdwr;
	meidebug debugrdwr;
	u32 arc_debug_data,reg_data;
#ifdef DANUBE_CLEAR_EOC
	struct sk_buff * eoc_skb;
#endif //DANUBE_CLEAR_EOC
	u16 RxMessage[MSG_LENGTH]__attribute__ ((aligned(4)));
	u16 TxMessage[MSG_LENGTH]__attribute__ ((aligned(4)));
	
	int from_kernel = 0;//joelin
	if (ino == (struct inode *)0) from_kernel = 1;//joelin
	if (command < DANUBE_MEI_START)
	{
#ifdef CONFIG_DANUBE_MEI_MIB			
		return mei_mib_ioctl(ino,fil,command,lon);
#endif //CONFIG_DANUBE_MEI_MIB

		DANUBE_MEI_EMSG("No such ioctl command (0x%X)! MEI ADSL MIB is not supported!\n",command);
		return -ENOIOCTLCMD;			
	}else
	{
		switch(command){
                case DANUBE_MEI_START:
                
#ifdef DANUBE_CLEAR_EOC
			if(clreoc_command_pkt !=NULL){
				kfree(clreoc_command_pkt);
				clreoc_command_pkt =NULL;
			}
			for(i=0;i<CLREOC_BUFF_SIZE;i++)
				clreoc_pkt[i].len=0;	//flush all remaining clreoc commands in buffer
#endif
			showtime=0;
			loop_diagnostics_completed = 0;
			if (time_disconnect.tv_sec == 0)
				do_gettimeofday(&time_disconnect);

			if(down_interruptible(&mei_sema))	//disable CMV access until ARC ready
			{
				DANUBE_MEI_EMSG("-ERESTARTSYS\n");
        	        	return -ERESTARTSYS;
			}

			meiMailboxInterruptsDisable(); //disable all MEI interrupts
			if(mei_arc_swap_buff == NULL){
				mei_arc_swap_buff = (u32 *)kmalloc(MAXSWAPSIZE*4, GFP_KERNEL);
				if(mei_arc_swap_buff==NULL){
					DANUBE_MEI_EMSG("\n\n malloc fail for codeswap buff");
					meierr=MEI_FAILURE;
				}
			}
                        if(meiRunAdslModem() != MEI_SUCCESS){
                                DANUBE_MEI_EMSG("\n\n meiRunAdslModem()  error...");
                                meierr=MEI_FAILURE;
                        }     

      			break;

		case DANUBE_MEI_SHOWTIME:
				if(down_interruptible(&mei_sema))
                			return -ERESTARTSYS;
				
				do_gettimeofday(&time_showtime);
				unavailable_seconds += time_showtime.tv_sec - time_disconnect.tv_sec;
				time_disconnect.tv_sec = 0;
				
#ifdef DANUBE_CLEAR_EOC
				// clreoc stuff
			makeCMV(H2D_CMV_READ, INFO, 83, 0, 1, NULL, TxMessage); //maximum allowed tx message length, in bytes
			if(meiCMV(TxMessage, YES_REPLY,RxMessage)!=MEI_SUCCESS){
					DANUBE_MEI_EMSG("\n\nCMV fail, Group 3 Address 83 Index 0");
				}
				else{
					clreoc_max_tx_len = (int)RxMessage[4];
					clreoc_command_pkt = kmalloc((clreoc_max_tx_len*CLREOC_BUFF_SIZE), GFP_KERNEL);
					if(clreoc_command_pkt == NULL){
						DANUBE_MEI_EMSG("kmalloc error for clreoc_command_pkt\n\n");
						up(&mei_sema);
						return -1;
					}
					for(i=0;i<CLREOC_BUFF_SIZE;i++){
						clreoc_pkt[i].command = (u8 *)(((u8 *)clreoc_command_pkt) + (clreoc_max_tx_len*i));
						clreoc_pkt[i].len=0;
					}	
				}
#endif

#ifdef CONFIG_DANUBE_MEI_MIB
			  	mei_mib_adsl_link_up();
#endif

//dying gasp -start	
#ifdef IFX_DYING_GASP
				lop_debugwr.buffer[0]=0xffffffff;		//dying gasp
				lop_debugwr.iCount=1;				//dying gasp
			makeCMV(H2D_CMV_READ, INFO, 66, 4, 1, NULL,TxMessage);
			if(meiCMV(TxMessage, YES_REPLY,RxMessage)!=MEI_SUCCESS){
					DANUBE_MEI_EMSG("\n\nCMV fail, Group 3 Address 66 Index 4");
				}
				lop_debugwr.iAddress=(u32)RxMessage[4];
			makeCMV(H2D_CMV_READ, INFO, 66, 5, 1, NULL,TxMessage);
			if(meiCMV(TxMessage, YES_REPLY,RxMessage)!=MEI_SUCCESS){
					DANUBE_MEI_EMSG("\n\nCMV fail, Group 3 Address 66 Index 5");
				}
				lop_debugwr.iAddress+=((u32)RxMessage[4])<<16;
//dying gasp -end				
#endif	// IFX_DYING_GASP			
//joelin 04/16/2005-start
			makeCMV(H2D_CMV_WRITE, PLAM, 10, 0, 1, &unavailable_seconds,TxMessage);
			if(meiCMV(TxMessage, YES_REPLY,RxMessage)!=MEI_SUCCESS){
					DANUBE_MEI_EMSG("\n\nCMV fail, Group 7 Address 10 Index 0");
				}

//joelin 04/16/2005-end		
				showtime=1;
				free_image_buffer(FREE_SHOWTIME);
				up(&mei_sema);
			break;
		
		case DANUBE_MEI_HALT:
			if(arc_halt_flag==0)
			{
				meiResetARC();
				meiHaltArc();
			}
			break;
		case DANUBE_MEI_RUN:
			if(arc_halt_flag==1)
			{
				meiRunArc();
			}
			break;
		case DANUBE_MEI_CMV_WINHOST:
			if(down_interruptible(&mei_sema))
                		return -ERESTARTSYS;

			if (!from_kernel )	
				copy_from_user((char *)TxMessage, (char *)lon, MSG_LENGTH*2);//joelin
			else
				memcpy(TxMessage,(char *)lon,MSG_LENGTH*2);
				
		
			if(meiCMV(TxMessage, YES_REPLY,RxMessage)!=MEI_SUCCESS){
        			DANUBE_MEI_EMSG("\n\nWINHOST CMV fail :TxMessage:%X %X %X %X, RxMessage:%X %X %X %X %X\n",TxMessage[0],TxMessage[1],TxMessage[2],TxMessage[3],RxMessage[0],RxMessage[1],RxMessage[2],RxMessage[3],RxMessage[4]);
				meierr = MEI_FAILURE;	
			}
			else 
			{
				if (!from_kernel )	//joelin
					copy_to_user((char *)lon, (char *)RxMessage, MSG_LENGTH*2);
				else
					memcpy((char *)lon,(char *)RxMessage,MSG_LENGTH*2);
			}
				
			up(&mei_sema);	
			break;
#ifdef DANUBE_MEI_CMV_EXTRA
		case DANUBE_MEI_CMV_READ:
			copy_from_user((char *)(&regrdwr), (char *)lon, sizeof(meireg));
			meiLongwordRead((u32)regrdwr.iAddress, &(regrdwr.iData));

			copy_to_user((char *)lon, (char *)(&regrdwr), sizeof(meireg));
			break;

		case DANUBE_MEI_CMV_WRITE:
			copy_from_user((char *)(&regrdwr), (char *)lon, sizeof(meireg));
			meiLongwordWrite((u32)regrdwr.iAddress, regrdwr.iData);
			break;

		case DANUBE_MEI_REMOTE:
			copy_from_user((char *)(&i), (char *)lon, sizeof(int));
			if(i==0){
				meiMailboxInterruptsEnable();
					
				up(&mei_sema);
			}
			else if(i==1){
				meiMailboxInterruptsDisable();
				if(down_interruptible(&mei_sema))
                			return -ERESTARTSYS;
			}
			else{
				DANUBE_MEI_EMSG("\n\n DANUBE_MEI_REMOTE argument error");
				meierr=MEI_FAILURE;
			}		
			break;

		case DANUBE_MEI_READDEBUG:
		case DANUBE_MEI_WRITEDEBUG:
			if(down_interruptible(&mei_sema))
                		return -ERESTARTSYS;
#ifdef IFX_DYING_GASP			
			if (!from_kernel) 
				copy_from_user((char *)(&debugrdwr), (char *)lon, sizeof(debugrdwr));//dying gasp
			else 
				memcpy((char *)(&debugrdwr), (char *)lon,  sizeof(debugrdwr));
#else //IFX_DYING_GASP	
			copy_from_user((char *)(&debugrdwr), (char *)lon, sizeof(debugrdwr));

#endif //IFX_DYING_GASP							

			
			if(command==DANUBE_MEI_READDEBUG)
				meiDebugRead(debugrdwr.iAddress, debugrdwr.buffer, debugrdwr.iCount);
			else
				meiDebugWrite(debugrdwr.iAddress, debugrdwr.buffer, debugrdwr.iCount);	
				
#ifdef IFX_DYING_GASP				
			if (!from_kernel) 
				copy_to_user((char *)lon, (char*)(&debugrdwr), sizeof(debugrdwr));//dying gasp
#else //IFX_DYING_GASP	
			copy_to_user((char *)lon, (char*)(&debugrdwr), sizeof(debugrdwr));
#endif //IFX_DYING_GASP	
			up(&mei_sema);
			
			break;
		case DANUBE_MEI_RESET:			
		case DANUBE_MEI_REBOOT:
		
#ifdef CONFIG_DANUBE_MEI_MIB			
			mei_mib_adsl_link_down();
#endif
			meiResetARC();
			meiControlModeSwitch(MEI_MASTER_MODE);
			//enable ac_clk signal	
			_meiDebugLongWordRead(MEI_DEBUG_DEC_DMP1_MASK,CRI_CCR0,&arc_debug_data);
			arc_debug_data |=ACL_CLK_MODE_ENABLE;		
			_meiDebugLongWordWrite(MEI_DEBUG_DEC_DMP1_MASK,CRI_CCR0,arc_debug_data);	
			meiControlModeSwitch(JTAG_MASTER_MODE);
			meiHaltArc();			
			update_bar_register(nBar);
			break;
		case DANUBE_MEI_DOWNLOAD:
			// DMA the boot code page(s)
			DANUBE_MEI_DMSG("Start download pages");
			meiDownloadBootPages();
			break;
#endif //DANUBE_MEI_CMV_EXTRA
		//for clearEoC
#ifdef DANUBE_CLEAR_EOC
		case DANUBE_MEI_GET_EOC_LEN:
			while(1){
				current_clreoc = list_entry(clreoc_list.next, danube_clreoc_pkt, list);
				if((current_clreoc->len)>0){
					copy_to_user((char *)lon, (char*)(&(current_clreoc->len)), 4);
					break;	
				}
				else//wait for eoc data from higher layer
				{
					interruptible_sleep_on(&wait_queue_clreoc);	
			}
			}
			break;
		case DANUBE_MEI_GET_EOC_DATA:
			current_clreoc = list_entry(clreoc_list.next, danube_clreoc_pkt, list);
			if((current_clreoc->len)>0){
				copy_to_user((char*)lon, (char*)(current_clreoc->command), current_clreoc->len);
				meierr=1;
				list_del(clreoc_list.next);	//remove and add to end of list
				current_clreoc->len = 0;
				list_add_tail(&(current_clreoc->list), &clreoc_list);
			}
			else
				meierr=-EPERM;
			break;
		case DANUBE_MEI_EOC_SEND:
			copy_from_user((char *)(&debugrdwr), (char *)lon, sizeof(debugrdwr));
			eoc_skb = dev_alloc_skb(debugrdwr.iCount*4);
			if(eoc_skb==NULL){
				DANUBE_MEI_EMSG("\n\nskb alloc fail");
				break;
			}
			
			eoc_skb->len=debugrdwr.iCount*4;
			memcpy(skb_put(eoc_skb, debugrdwr.iCount*4), (char *)debugrdwr.buffer, debugrdwr.iCount*4);
			
			ifx_push_eoc(eoc_skb);	//pass data to higher layer
			break;
#endif // DANUBE_CLEAR_EOC
		case DANUBE_MEI_JTAG_ENABLE:
			DANUBE_MEI_EMSG("ARC JTAG Enable.\n");
			//reserve gpio 9, 10, 11, 14, 19 for ARC JTAG
			meierr = danube_port_reserve_pin(0,9,PORT_MODULE_MEI_JTAG);	
			if ( meierr < 0 )
			{
				DANUBE_MEI_EMSG("Reserve GPIO 9 Fail.\n");
				break;
			}
			meierr = danube_port_reserve_pin(0,10,PORT_MODULE_MEI_JTAG);	
			if ( meierr < 0 )
			{
				DANUBE_MEI_EMSG("Reserve GPIO 10 Fail.\n");
				break;
			}
			meierr = danube_port_reserve_pin(0,11,PORT_MODULE_MEI_JTAG);	
			if ( meierr < 0 )
			{
				DANUBE_MEI_EMSG("Reserve GPIO 11 Fail.\n");
				break;
			}
			meierr = danube_port_reserve_pin(0,14,PORT_MODULE_MEI_JTAG);	
			if ( meierr < 0 )
			{
				DANUBE_MEI_EMSG("Reserve GPIO 14 Fail.\n");
				break;
			}
			meierr = danube_port_reserve_pin(1,3,PORT_MODULE_MEI_JTAG);	
			if ( meierr < 0 )
			{
				DANUBE_MEI_EMSG("Reserve GPIO 19 Fail.\n");
				break;
			}

        		*(DANUBE_GPIO_P0_DIR) = (*DANUBE_GPIO_P0_DIR)&(~0x800);  // set gpio11 to input
        		*(DANUBE_GPIO_P0_ALTSEL0) = ((*DANUBE_GPIO_P0_ALTSEL0)&(~0x800));
        		*(DANUBE_GPIO_P0_ALTSEL1) = ((*DANUBE_GPIO_P0_ALTSEL1)&(~0x800));
        		*DANUBE_GPIO_P0_OD = (*DANUBE_GPIO_P0_OD)|0x800;

			//enable ARC JTAG
			meiLongwordRead((u32)DANUBE_RCU_REQ,&reg_data);
			meiLongwordWrite((u32)DANUBE_RCU_REQ,reg_data | DANUBE_RCU_RST_REQ_ARC_JTAG);
			
			break;
		case GET_ADSL_LOOP_DIAGNOSTICS_MODE:
			copy_to_user((char *)lon, (char *)&loop_diagnostics_mode, sizeof(int));	
			break;
		case LOOP_DIAGNOSTIC_MODE_COMPLETE:
			loop_diagnostics_completed = 1;
#ifdef CONFIG_DANUBE_MEI_MIB			
			// read adsl mode
			makeCMV(H2D_CMV_READ, STAT, 1, 0, 1, NULL,TxMessage); 
			if(meiCMV(TxMessage, YES_REPLY, RxMessage)!=MEI_SUCCESS){
#ifdef DANUBE_MEI_DEBUG_ON
				printk("\n\nCMV fail, Group STAT Address 1 Index 0");
#endif
			}
			adsl_mode = RxMessage[4];
	
			makeCMV(H2D_CMV_READ, STAT, 17, 0, 1, NULL, TxMessage); 
			if(meiCMV(TxMessage, YES_REPLY, RxMessage)!=MEI_SUCCESS){
#ifdef DANUBE_MEI_DEBUG_ON
				printk("\n\nCMV fail, Group STAT Address 1 Index 0");
#endif
			}
			adsl_mode_extend = RxMessage[4];
#endif
			wake_up_interruptible(&wait_queue_loop_diagnostic);	
			break;
		case SET_ADSL_LOOP_DIAGNOSTICS_MODE:
			if (lon != loop_diagnostics_mode)
			{
				loop_diagnostics_completed = 0;
				loop_diagnostics_mode = lon;
	
				mei_ioctl((struct inode *)0,NULL, DANUBE_MEI_REBOOT, (unsigned long)NULL);
				
			}
			break;
		case DANUBE_MEI_MIB_DAEMON:
		default:
			DANUBE_MEI_EMSG("The ioctl command(0x%X is not supported!\n",command);
			meierr = -ENOIOCTLCMD;
        }
     }
        return meierr;
}//mei_ioctl



////////////////////     procfs debug    ///////////////////////////

#ifdef CONFIG_PROC_FS
static int proc_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos)
{
        int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[64];
	int count=0;
	int i;
	u32 version=0;
	reg_entry_t* current_reg=NULL;
	u16 RxMessage[MSG_LENGTH]__attribute__ ((aligned(4)));
	u16 TxMessage[MSG_LENGTH]__attribute__ ((aligned(4)));
	
	for (i=0;i<NUM_OF_REG_ENTRY;i++) {
		if (regs[i].low_ino==i_ino) {
			current_reg = &regs[i];
			break;
		}
	}
	if (current_reg==NULL)
		return -EINVAL;
	
	if (current_reg->flag == (int *) 8){
		///proc/mei/version
		//format:
		//Firmware version: major.minor.sub_version.int_version.rel_state.spl_appl
		///Firmware Date Time Code: date/month min:hour
		if (*ppos>0) /* Assume reading completed in previous read*/
			return 0;               // indicates end of file
		if(down_interruptible(&mei_sema))
                	return -ERESTARTSYS;
		
		if (indicator_count < 1)
		{
			up(&mei_sema);
			return -EAGAIN;
		}
		//major:bits 0-7 
		//minor:bits 8-15
		makeCMV(H2D_CMV_READ, INFO, 54, 0, 1, NULL,TxMessage);
		if(meiCMV(TxMessage, YES_REPLY,RxMessage)!=MEI_SUCCESS)
		{
			up(&mei_sema);
			return -EIO;
		}
		version = RxMessage[4];
		count = sprintf(outputbuf, "%d.%d.",(version)&0xff,(version>>8)&0xff);
		
		//sub_version:bits 4-7
		//int_version:bits 0-3
		//spl_appl:bits 8-13
		//rel_state:bits 14-15
		makeCMV(H2D_CMV_READ, INFO, 54, 1, 1, NULL,TxMessage);
		if(meiCMV(TxMessage, YES_REPLY,RxMessage)!=MEI_SUCCESS)
		{
			up(&mei_sema);
			return -EFAULT;
		}
		version =RxMessage[4];
		count += sprintf(outputbuf+count, "%d.%d.%d.%d",
				(version>>4)&0xf, 
				version&0xf,
				(version>>14)&0x3, 
				(version>>8)&0x3f);	
#ifdef 	CONFIG_DANUBE_MEI_LED			
// version check -start	for adsl led			
		if ((((version>>4)&0xf)==2)&&((version&0xf)>=3)&&((version&0xf)<7)) 
			firmware_support_led=1;
		else if ((((version>>4)&0xf)==2)&&((version&0xf)>=7))
			firmware_support_led=2;
		else if (((version>>4)&0xf)>2) 
			firmware_support_led=2;
		else 
			firmware_support_led=2;
// version check -end	
#endif	
		//Date:bits 0-7
		//Month:bits 8-15
		makeCMV(H2D_CMV_READ, INFO, 55, 0, 1, NULL,TxMessage);
		if(meiCMV(TxMessage, YES_REPLY,RxMessage)!=MEI_SUCCESS){
			up(&mei_sema);
			return -EIO;
		}
		version = RxMessage[4];
		
		//Hour:bits 0-7
		//Minute:bits 8-15
		makeCMV(H2D_CMV_READ, INFO, 55, 1, 1, NULL,TxMessage);
		if(meiCMV(TxMessage, YES_REPLY,RxMessage)!=MEI_SUCCESS){
			up(&mei_sema);
			return -EFAULT;
		}
		version += (RxMessage[4]<<16);
		count += sprintf(outputbuf+count, " %d/%d %d:%d\n"
				,version&0xff
				,(version>>8)&0xff
				,(version>>25)&0xff
				,(version>>16)&0xff);				
		up(&mei_sema);	
		
		*ppos+=count;
	}else if(current_reg->flag != (int *)Recent_indicator){
        	if (*ppos>0) /* Assume reading completed in previous read*/
			return 0;               // indicates end of file
		count = sprintf(outputbuf, "0x%08X\n\n", *(current_reg->flag));
	        *ppos+=count;
	        if (count>nbytes)  /* Assume output can be read at one time */
			return -EINVAL;
        }else{
        	if((int)(*ppos)/((int)7)==16)
                	return 0;  // indicate end of the message
        	count = sprintf(outputbuf, "0x%04X\n\n", *(((u16 *)(current_reg->flag))+ (int)(*ppos)/((int)7)));
                *ppos+=count;
	}
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static ssize_t proc_write(struct file * file, const char * buffer, size_t count, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	reg_entry_t* current_reg=NULL;
	int i;
	unsigned long newRegValue;
	char *endp;

	for (i=0;i<NUM_OF_REG_ENTRY;i++) {
		if (regs[i].low_ino==i_ino) {
			current_reg = &regs[i];
			break;
		}
	}
	if ((current_reg==NULL) || (current_reg->flag == (int *)Recent_indicator))
		return -EINVAL;

	newRegValue = simple_strtoul(buffer,&endp,0);
	*(current_reg->flag)=(int)newRegValue;
	return (count+endp-buffer);
}
#endif //CONFIG_PROC_FS

//TODO, for loopback test
#ifdef DFE_LOOPBACK
#define mte_reg_base	(0x4800*4+0x20000)
 
/* Iridia Registers Address Constants */
#define MTE_Reg(r)    	(int)(mte_reg_base + (r*4))
 
#define IT_AMODE       	MTE_Reg(0x0004)

#define OMBOX_BASE 	0xDF80
#define OMBOX1 	(OMBOX_BASE+0x4)
#define IMBOX_BASE 	0xDFC0

#define TIMER_DELAY   	(1024)
#define BC0_BYTES     	(32)
#define BC1_BYTES     	(30)
#define NUM_MB        	(12)
#define TIMEOUT_VALUE 	2000

static void BFMWait (u32 cycle) {
  	u32 i;
  	for (i = 0 ; i< cycle ; i++)
		; 
}

static void WriteRegLong(u32 addr, u32 data){
  //*((volatile u32 *)(addr)) =  data; 
	DANUBE_WRITE_REGISTER_L(data,addr);
}

static u32 ReadRegLong (u32 addr) {
  	// u32	rd_val;
  	//rd_val = *((volatile u32 *)(addr));
	// return rd_val;
	return DANUBE_READ_REGISTER_L(addr);
}

/* This routine writes the mailbox with the data in an input array */
static void WriteMbox(u32 *mboxarray,u32 size) {
  	meiDebugWrite(IMBOX_BASE,mboxarray,size);
  	printk("write to %X\n",IMBOX_BASE);
  	meiLongwordWrite((u32)MEI_TO_ARC_INT, MEI_TO_ARC_MSGAV);
}

/* This routine reads the output mailbox and places the results into an array */
static void ReadMbox(u32 *mboxarray,u32 size) {
  	meiDebugRead(OMBOX_BASE,mboxarray,size);
  	printk("read from %X\n",OMBOX_BASE);
}

static void MEIWriteARCValue(u32 address, u32 value)
{
  	u32 i,check = 0;

  	/* Write address register */
	DANUBE_WRITE_REGISTER_L(address,MEI_DEBUG_WAD);

 	/* Write data register */
	DANUBE_WRITE_REGISTER_L(value,MEI_DEBUG_DATA);

  	/* wait until complete - timeout at 40*/
  	for (i=0;i<40;i++) {
		check = DANUBE_READ_REGISTER_L(ARC_TO_MEI_INT);
		
    		if ((check & ARC_TO_MEI_DBG_DONE)) 
			break;
    	}
  	/* clear the flag */
	DANUBE_WRITE_REGISTER_L(ARC_TO_MEI_DBG_DONE,ARC_TO_MEI_INT);
}

void arc_code_page_download(uint32_t arc_code_length,uint32_t *start_address)
{
	int count;
	DANUBE_MEI_DMSG("try to download pages,size=%d\n",arc_code_length);
	meiControlModeSwitch(MEI_MASTER_MODE);
	if (arc_halt_flag==0)
	{
	meiHaltArc();	
	}
	meiLongwordWrite((u32)MEI_XFR_ADDR, 0);
	for(count=0;count < arc_code_length;count++)
	{
		meiLongwordWrite((u32)MEI_DATA_XFR, *(start_address+count));
	}
	meiControlModeSwitch(JTAG_MASTER_MODE);
}
static int load_jump_table(unsigned long addr)
{
	int i;
	uint32_t addr_le,addr_be;
	uint32_t jump_table[32];
	for (i=0;i<16;i++)
	{
		addr_le = i*8 + addr;
		addr_be = ((addr_le >>16) & 0xffff);
		addr_be |= ((addr_le & 0xffff) << 16);
		jump_table[i*2+0]=0x0f802020;
		jump_table[i*2+1]=addr_be;
		//printk("jt %X %08X %08X\n",i,jump_table[i*2+0],jump_table[i*2+1]);
	}
	arc_code_page_download(32,&jump_table[0]);
	return 0;
}

void dfe_loopback_irq_handler(void)
{
	uint32_t rd_mbox[10];	
		
	memset(&rd_mbox[0],0,10*4);
	ReadMbox(&rd_mbox[0], 6);
	if (rd_mbox[0]== 0x0)
	{
		DANUBE_MEI_DMSG("Get ARC_ACK\n");
		got_int = 1;
	}else if (rd_mbox[0]== 0x5)
	{
		DANUBE_MEI_DMSG("Get ARC_BUSY\n");
			got_int = 2;
	}else if (rd_mbox[0]== 0x3)
	{
		DANUBE_MEI_DMSG("Get ARC_EDONE\n");
		if (rd_mbox[1]== 0x0)
		{
				got_int = 3;
			DANUBE_MEI_DMSG("Get E_MEMTEST\n");
			if (rd_mbox[2]!= 0x1)
			{
					got_int = 4;
				DANUBE_MEI_DMSG("Get Result %X\n",rd_mbox[2]);
			}
		}
	}
	meiLongwordWrite((u32)ARC_TO_MEI_INT,  ARC_TO_MEI_DBG_DONE);
	mask_and_ack_danube_irq(DANUBE_MEI_INT);
	disable_irq(DANUBE_MEI_INT);
	//got_int = 1;
        return;
}

static void wait_mem_test_result(void)
{
	uint32_t mbox[5];
	mbox[0]=0;
	DANUBE_MEI_DMSG("Waiting Starting\n");
	while (mbox[0]==0)
	{
		ReadMbox(&mbox[0],5);
	}
	DANUBE_MEI_DMSG("Try to get mem test result.\n");
	ReadMbox(&mbox[0],5);
	if (mbox[0]==0xA)
	{
		DANUBE_MEI_DMSG("Success.\n");
	}else if (mbox[0] == 0xA)
	{
		DANUBE_MEI_DMSG("Fail,address %X,except data %X,receive data %X\n",mbox[1],mbox[2],mbox[3]);
	}
	else
	{
		DANUBE_MEI_DMSG("Fail\n");
	}
}

static int arc_ping_testing(void)
{
#define MEI_PING 0x00000001
	uint32_t wr_mbox[10],rd_mbox[10];
	int i;
	for (i=0; i<10; i++)
	{
    		wr_mbox[i] = 0;
    		rd_mbox[i] = 0;
    	}

	DANUBE_MEI_DMSG("send ping msg\n");
	wr_mbox[0]=MEI_PING;
	WriteMbox(&wr_mbox[0],10);
	
	while(got_int==0)
	{
		schedule();
	}

	DANUBE_MEI_DMSG("send start event\n");
	got_int = 0;

	wr_mbox[0]=0x4;
	wr_mbox[1]=0;
	wr_mbox[2]=0;
	wr_mbox[3]=(uint32_t)0xf5acc307e;
	wr_mbox[4]=5;
	wr_mbox[5]=2;
	wr_mbox[6]= 0x1c000;
	wr_mbox[7]= 64;
	wr_mbox[8]=0;
	wr_mbox[9]=0;
	WriteMbox(&wr_mbox[0], 10);
	enable_irq(DANUBE_MEI_INT);
	//printk("meiMailboxWrite ret=%d\n",i);
	meiLongwordWrite((u32)MEI_TO_ARC_INT, MEI_TO_ARC_MSGAV);
	printk("sleeping\n");
	while(1)
	{
		if(got_int > 0)
		{
			
			
			if(got_int>3)
			printk("got_int >>>> 3\n");
			else 
			printk("got int = %d\n",got_int);
			got_int = 0;
			//schedule();
			enable_irq(DANUBE_MEI_INT);
		}
		//mbox_read(&rd_mbox[0],6);
		//printk("rd_mbox[0]=%X %X %X %X %X %X\n",rd_mbox[0],rd_mbox[1],rd_mbox[2],rd_mbox[3],rd_mbox[4],rd_mbox[5]);
		schedule();
	}
}

static MEI_ERROR DFE_Loopback_Test(void)
{
	int i=0;
	u32 arc_debug_data=0,temp;

	meiResetARC();
	// start the clock
	arc_debug_data = ACL_CLK_MODE_ENABLE; 
	meiDebugWrite(CRI_CCR0,&arc_debug_data,1);

#if defined( DFE_PING_TEST )|| defined( DFE_ATM_LOOPBACK) 
	// WriteARCreg(AUX_XMEM_LTEST,0);
	meiControlModeSwitch(MEI_MASTER_MODE);
#define AUX_XMEM_LTEST 0x128
	_meiDebugLongWordWrite(MEI_DEBUG_DEC_AUX_MASK,AUX_XMEM_LTEST,0);
	meiControlModeSwitch(JTAG_MASTER_MODE);
	
	// WriteARCreg(AUX_XDMA_GAP,0);	
	meiControlModeSwitch(MEI_MASTER_MODE);
#define AUX_XDMA_GAP 0x114
	_meiDebugLongWordWrite(MEI_DEBUG_DEC_AUX_MASK,AUX_XDMA_GAP,0);
	meiControlModeSwitch(JTAG_MASTER_MODE);

	meiControlModeSwitch(MEI_MASTER_MODE);
	temp=0;
	_meiDebugLongWordWrite(MEI_DEBUG_DEC_AUX_MASK,(u32)MEI_XDATA_BASE_SH,temp);
	meiControlModeSwitch(JTAG_MASTER_MODE);
	

	i = alloc_processor_memory(SDRAM_SEGMENT_SIZE*16,adsl_mem_info);
	if (i >=0) {
		int idx;

		for (idx=0;idx<i;idx++)
		{
			adsl_mem_info[idx].type = FREE_RELOAD;
			DANUBE_WRITE_REGISTER_L((((uint32_t)adsl_mem_info[idx].address)&0x0fffffff),MEI_XMEM_BAR_BASE+idx*4);
			DANUBE_MEI_DMSG("bar%d(%X)=%X\n",idx,MEI_XMEM_BAR_BASE+idx*4,(((uint32_t)adsl_mem_info[idx].address)&0x0fffffff));
			memset((u8 *)adsl_mem_info[idx].address,0,SDRAM_SEGMENT_SIZE);
	}
		
		meiLongwordWrite((u32)MEI_XDATA_BASE_SH, ((unsigned long)adsl_mem_info[XDATA_REGISTER].address) & 0x0FFFFFFF);

	}else{
		DANUBE_MEI_EMSG("cannot load image: no memory\n\n");
		return MEI_FAILURE;
}
		//WriteARCreg(AUX_IC_CTRL,2);
	meiControlModeSwitch(MEI_MASTER_MODE);
#define AUX_IC_CTRL 0x11
	_meiDebugLongWordWrite(MEI_DEBUG_DEC_AUX_MASK,AUX_IC_CTRL,2);
	meiControlModeSwitch(JTAG_MASTER_MODE);

	meiHaltArc();
	
#ifdef DFE_PING_TEST		
	
	DANUBE_MEI_DMSG("ping test image size=%d\n",sizeof(code_array));
	memcpy((u8 *)(adsl_mem_info[0].address+0x1004),&code_array[0],sizeof(code_array));
	load_jump_table(0x80000+0x1004);

#endif	//DFE_PING_TEST

	DANUBE_MEI_DMSG("ARC ping test code download complete\n");
#endif //defined( DFE_PING_TEST )|| defined( DFE_ATM_LOOPBACK)
#ifdef DFE_MEM_TEST
	meiLongwordWrite((u32)ARC_TO_MEI_INT_MASK, MSGAV_EN); 
	
	arc_code_page_download(1537,&mem_test_code_array[0]);
	DANUBE_MEI_DMSG("ARC mem test code download complete\n");
#endif //DFE_MEM_TEST
#ifdef DFE_ATM_LOOPBACK
        arc_debug_data = 0xf;
	arc_code_page_download(1077,&code_array[0]);
        // Start Iridia IT_AMODE (in dmp access) why is it required?
        meiDebugWrite(0x32010,&arc_debug_data,1);
#endif //DFE_ATM_LOOPBACK
	meiMailboxInterruptsEnable();
	meiRunArc();
	
#ifdef DFE_PING_TEST
	arc_ping_testing();
#endif //DFE_PING_TEST
#ifdef DFE_MEM_TEST
	wait_mem_test_result();
#endif //DFE_MEM_TEST

	free_image_buffer(FREE_ALL);
	return MEI_SUCCESS;
}

#endif //DFE_LOOPBACK
//end of TODO, for loopback test


////////////////////////////////////////////////////////////////////////////

int __init danube_mei_init_module(void)
{
        struct proc_dir_entry *entry;
        int i;
	u32 temp;
	do_gettimeofday(&time_disconnect);
#ifdef CONFIG_DEVFS_FS
	char buf[10];
#endif

        printk("Danube MEI version:%s\n", DANUBE_MEI_VERSION);
//dying gasp-start	
#ifdef IFX_DYING_GASP

//000003:fchang Start
#ifdef CONFIG_CPU_DANUBE_E
	//GPIO31 :dying gasp event indication
	//	(1) logic high: dying gasp event is false (default)
	//	(2) logic low: dying gasp event is true
	CLEAR_BIT((*DANUBE_GPIO_P0_DIR), BIT2);
	CLEAR_BIT((*DANUBE_GPIO_P0_ALTSEL0), BIT2);
	CLEAR_BIT((*DANUBE_GPIO_P0_ALTSEL1), BIT2);
	SET_BIT((*DANUBE_GPIO_P0_OD), BIT2);
	asm("SYNC");			
#else //000003:fchang End

	//GPIO31 :dying gasp event indication
	//	(1) logic high: dying gasp event is false (default)
	//	(2) logic low: dying gasp event is true
	CLEAR_BIT((*DANUBE_GPIO_P1_DIR), BIT15);
	CLEAR_BIT((*DANUBE_GPIO_P1_ALTSEL0), BIT15);
	CLEAR_BIT((*DANUBE_GPIO_P1_ALTSEL1), BIT15);
	SET_BIT((*DANUBE_GPIO_P1_OD), BIT15);

	asm("SYNC");	
#endif //000003:fchang

#endif //IFX_DYING_GASP	
//dying gasp -end        
	
        reg_entry_t regs_temp[PROC_ITEMS] =    // Items being debugged
        {
        /*  {   flag,          	        name,              description } */
	    {&arcmsgav,	             "arcmsgav", 	"arc to mei message ", 0 },
            {&cmv_reply, 	     "cmv_reply", 	"cmv needs reply", 0},
            {&cmv_waiting, 	     "cmv_waiting", 	"waiting for cmv reply from arc", 0},
            {&indicator_count,       "indicator_count", "ARC to MEI indicator count", 0},
            {&cmv_count, 	     "cmv_count", 	"MEI to ARC CMVs", 0},
            {&reply_count, 	     "reply_count", 	"ARC to MEI Reply", 0},
            {(int *)Recent_indicator,"Recent_indicator","most recent indicator", 0},
	    {(int *)8, 		     "version", 	"version of firmware", 0},
        };
        memcpy((char *)regs, (char *)regs_temp, sizeof(regs_temp));

        sema_init(&mei_sema, 1);  // semaphore initialization, mutex
	
        init_waitqueue_head(&wait_queue_arcmsgav);     	// for ARCMSGAV
        init_waitqueue_head(&wait_queue_loop_diagnostic);		// for loop diagnostic function

	memset(&adsl_mem_info[0],0,sizeof(smmu_mem_info_t)*MAX_BAR_REGISTERS);

#ifdef CONFIG_DANUBE_MEI_LED
	danube_mei_led_init();
#endif
#ifdef IFX_DYING_GASP	
	init_waitqueue_head(&wait_queue_dying_gasp);	// IFX_DYING_GASP
	init_completion(&lop_thread_exit);
	lop_poll_init();				// IFX_DYING_GASP 
#endif	//IFX_DYING_GASP
 
#ifdef CONFIG_DANUBE_MEI_MIB
	danube_mei_mib_init();
#endif	
	
#ifdef DANUBE_CLEAR_EOC
	init_waitqueue_head(&wait_queue_clreoc);	// for clreoc_wr function
	// initialize clreoc list
	clreoc_pkt = (danube_clreoc_pkt *)kmalloc((sizeof(danube_clreoc_pkt)*CLREOC_BUFF_SIZE), GFP_KERNEL);
	if(clreoc_pkt == NULL){
		DANUBE_MEI_EMSG("kmalloc error for clreoc_pkt\n\n");
		return -1;
	}
	memset(clreoc_pkt, 0, (sizeof(danube_clreoc_pkt)*CLREOC_BUFF_SIZE));
	INIT_LIST_HEAD(&clreoc_list);
	for(i=0;i<CLREOC_BUFF_SIZE;i++)
		list_add_tail(&(clreoc_pkt[i].list), &clreoc_list);
#endif

	// power up mei 
	temp = DANUBE_READ_REGISTER_L(DANUBE_PMU_PWDCR);
	temp &= 0xffff7dbe;
	DANUBE_WRITE_REGISTER_L(temp,DANUBE_PMU_PWDCR);
	
#ifdef CONFIG_DEVFS_FS
	memset(&danube_base_dir,0,sizeof(danube_base_dir));
	memset(&mei_devfs_handle,0,sizeof(mei_devfs_handle));
	danube_base_dir = devfs_mk_dir( NULL, DANUBE_DEVNAME, NULL);
	if (danube_base_dir != NULL)
	{
		sprintf(buf,"%s",DANUBE_MEI_DEVNAME);
		if ((mei_devfs_handle = devfs_register( danube_base_dir, buf, DEVFS_FL_DEFAULT,major,0,S_IFCHR | S_IRUGO | S_IWUGO, &mei_operations, (void *)0)) == NULL)	
		{
			DANUBE_MEI_EMSG("Register mei devfs error.\n");
			return -ENODEV;
		}
	}else
	{
		DANUBE_MEI_EMSG("Create %s fail.\n",DANUBE_DEVNAME);
		return -ENODEV;
	}
#else
        if (register_chrdev(major, DANUBE_MEI_DEVNAME, &mei_operations)!=0) {
                DANUBE_MEI_EMSG("\n\n unable to register major for danube_mei!!!");
		return -ENODEV;
        }
#endif
        if (request_irq(DANUBE_MEI_INT, mei_interrupt_arcmsgav,0, "danube_mei_arcmsgav", NULL)!=0){
                DANUBE_MEI_EMSG("\n\n unable to register irq(%d) for danube_mei!!!",DANUBE_MEI_INT);
                return -1;
        }
	enable_irq(DANUBE_MEI_INT);
#ifdef CONFIG_PROC_FS
        // procfs
        meidir=proc_mkdir(MEI_DIRNAME, &proc_root);
        if ( meidir == NULL) {
		DANUBE_MEI_EMSG(": can't create /proc/" MEI_DIRNAME "\n\n");
		return(-ENOMEM);
        }

        for(i=0;i<NUM_OF_REG_ENTRY;i++) {
		entry = create_proc_entry(regs[i].name,
				S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
				meidir);
		if(entry) {
			regs[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_operations;
		} else {
			DANUBE_MEI_EMSG(": can't create /proc/" MEI_DIRNAME "/%s\n\n", regs[i].name);
			return(-ENOMEM);
		}
        }
#endif //CONFIG_PROC_FS

        ///////////////////////////////// register net device ////////////////////////////
#ifdef DFE_LOOPBACK
	DFE_Loopback_Test();
#endif //DFE_LOOPBACK
        return 0;
}

void __exit danube_mei_cleanup_module(void)
{
        int i;

#ifdef CONFIG_DANUBE_MEI_LED
	danube_mei_led_cleanup();
#endif
        showtime=0;//joelin,clear task

#ifdef IFX_DYING_GASP
	lop_poll_shutdown = 1;
        wake_up_interruptible(&wait_queue_dying_gasp); //wake up and clean dying gasp poll thread
	wait_for_completion(&lop_thread_exit);//waiting dying gasp poll thread exit
#endif

#ifdef CONFIG_PROC_FS
        for(i=0;i<NUM_OF_REG_ENTRY;i++)
		remove_proc_entry(regs[i].name, meidir);

        remove_proc_entry(MEI_DIRNAME, &proc_root);
#endif //CONFIG_PROC_FS
	
        disable_irq(DANUBE_MEI_INT);
        free_irq(DANUBE_MEI_INT, NULL);
#ifdef CONFIG_DEVFS_FS
	devfs_unregister(mei_devfs_handle);
#else
        unregister_chrdev(major, "danube_mei");
#endif

#ifdef CONFIG_DANUBE_MEI_MIB
	danube_mei_mib_cleanup();
#endif
#ifdef DANUBE_CLEAR_EOC
	kfree(clreoc_pkt);
#endif
	
	free_image_buffer(FREE_ALL);
        return;
}

EXPORT_SYMBOL(meiDebugRead);
EXPORT_SYMBOL(meiDebugWrite);
#ifdef DANUBE_CLEAR_EOC
EXPORT_SYMBOL(ifx_pop_eoc);
#endif
EXPORT_SYMBOL(ifx_mei_hdlc_send);
EXPORT_SYMBOL(ifx_mei_hdlc_read);

MODULE_LICENSE("GPL");

module_init(danube_mei_init_module);
module_exit(danube_mei_cleanup_module);
