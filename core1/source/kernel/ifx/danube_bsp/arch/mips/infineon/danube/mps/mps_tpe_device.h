/************************************************************************
 *
 * Copyright (c) 2005
 * Infineon Technologies AG
 * St. Martin Strasse 53; 81669 Muenchen; Germany
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 ************************************************************************/



#ifndef __MPS_TPE_DEVICE_H
#define __MPS_TPE_DEVICE_H

//prochao, the FW version and revision log
#define	MPSDRV_VERSION_NUM		10005	//i.e. v1.00.02
//-------------------------------------<< Revision History Log >>--------------------------------------
// Revision#		Description
// ..........		..................................................................................
// v1.00.00			Created from the MPS driver of Danube
// v1.00.02
//
//=====================================================================================================

#define MPS_BASEADDRESS 0xBF107000
#define MPS_RAD0SR      MPS_BASEADDRESS + 0x0004	//????? 0x0040 ?????

#define MPS_RAD0SR_DU   (1<<0)
#define MPS_RAD0SR_CU   (1<<1)

#define MBX_BASEADDRESS 0xBF200000
#define VCPU_BASEADDRESS 0xBF208000 /*0xBF108000*/
/*---------------------------------------------------------------------------*/
//prochao, the actual address depends on CPU0 (MPS0) or CPU1 (MPS1)
// DANUBE_ICU = 0xBF880200
#ifdef	CONFIG_DANUBE_CORE1		//for CPU1, to use the cmd1
#define MPS_INTERRUPTS_ENABLE(X)  *((volatile u32*) DANUBE_ICU_IM0_IER) |= X;
#define MPS_INTERRUPTS_DISABLE(X) *((volatile u32*) DANUBE_ICU_IM0_IER) &= ~X;
#define MPS_INTERRUPTS_CLEAR(X)   *((volatile u32*) DANUBE_ICU_IM0_ISR) = X;
#define MPS_INTERRUPTS_SET(X)     *((volatile u32*) DANUBE_ICU_IM0_IRSR) = X;  /*  |= ? */

#else	//for CPU0
#define MPS_INTERRUPTS_ENABLE(X)  *((volatile u32*) DANUBE_ICU_IM4_IER) |= X;
#define MPS_INTERRUPTS_DISABLE(X) *((volatile u32*) DANUBE_ICU_IM4_IER) &= ~X;
#define MPS_INTERRUPTS_CLEAR(X)   *((volatile u32*) DANUBE_ICU_IM4_ISR) = X;
#define MPS_INTERRUPTS_SET(X)     *((volatile u32*) DANUBE_ICU_IM4_IRSR) = X;  /*  |= ? */
#endif

/*---------------------------------------------------------------------------*/

#define SETBIT(reg, mask)    *reg |= (mask)
#define CLEARBIT(reg, mask)  *reg &= (~mask)

/*---------------------------------------------------------------------------*/
/* Interrupt register values for ICU                                         */
/*---------------------------------------------------------------------------*/
#define DANUBE_MPS_VC0_IR0 (1 << 0)   /* Voice channel 0 */
#define DANUBE_MPS_VC1_IR1 (1 << 1)   /* Voice channel 1 */
#define DANUBE_MPS_VC2_IR2 (1 << 2)   /* Voice channel 2 */
#define DANUBE_MPS_VC3_IR3 (1 << 3)   /* Voice channel 3 */
#define DANUBE_MPS_AD0_IR4 (1 << 4)   /* AFE/DFE Status 0 */
#define DANUBE_MPS_AD1_IR5 (1 << 5)   /* AFE/DFE Status 1 */
#define DANUBE_MPS_VC_AD_ALL_IR6 (1 << 6)  /* ored VC and AD interrupts */
#define DANUBE_MPS_SEM_IR7 (1 << 7)   /* Semaphore Interrupt */
#define DANUBE_MPS_GLB_IR8 (1 << 8)   /* Global Interrupt */

/*---------------------------------------------------------------------------*/
/* Mailbox definitions                                                       */
/*---------------------------------------------------------------------------*/

#define MBX_CMD_FIFO_SIZE			64 /**< Size of command FIFO in bytes */
#define MBX_DATA_FIFO_SIZE			128
#define MBX_DEFINITION_AREA_SIZE	32
#define MBX_RW_POINTER_AREA_SIZE	32

/* base addresses for command and voice mailboxes (upstream and downstream ) */
#define MBX_UPSTRM_CMD_FIFO_BASE   (MBX_BASEADDRESS + MBX_DEFINITION_AREA_SIZE + MBX_RW_POINTER_AREA_SIZE)
#define MBX_DNSTRM_CMD_FIFO_BASE   (MBX_UPSTRM_CMD_FIFO_BASE + MBX_CMD_FIFO_SIZE)
#define MBX_UPSTRM_DATA_FIFO_BASE  (MBX_DNSTRM_CMD_FIFO_BASE + MBX_CMD_FIFO_SIZE)
#define MBX_DNSTRM_DATA_FIFO_BASE  (MBX_UPSTRM_DATA_FIFO_BASE + MBX_DATA_FIFO_SIZE)

#define MBX_DATA_WORDS			96


#define NUM_VOICE_CHANNEL		4 /**< nr of voice channels  */
#define NR_CMD_MAILBOXES		2    /**< nr of command mailboxes  */

#define MAX_UPSTRM_DATAWORDS	30
#define MAX_FIFO_WRITE_RETRIES	80

//prochao+, 0901
#define MPSDRV_NUM_WIFI_CHANNEL		2 /** # of wifi channels, currently support 1 channel only  */
#define	MPSDRV_MIN_SEQ_NUMBER		1
#define	MPSDRV_MAX_SEQ_NUMBER		255
#define	MPSDRV_TOTAL_SEQ_NUMBERS	(MPSDRV_MAX_SEQ_NUMBER + 1)

/*---------------------------------------------------------------------------*/
/* MPS buffer provision management structure definitions                   */
/*---------------------------------------------------------------------------*/

#define MPS_BUFFER_INITIAL                 36
#define MPS_BUFFER_THRESHOLD               24

#define MPS_DEFAULT_PROVISION_SEGMENTS_PER_MSG 12

#define MPS_MAX_PROVISION_SEGMENTS_PER_MSG 60
#define MPS_BUFFER_MAX_LEVEL               64

#define MPS_MEM_SEG_DATASIZE 512
#define MAX_MEM_SEG_DATASIZE 4095

//==============================================================================
// prochao+, 11/11/2006, creates the queue/macros for DataRxList (Q) processing
#define	MAX_MPS_DATAMSG_QLEN		1024
//
typedef struct mps_dataMsg_list {
	/* These two members must be first. */
	mps_data_message	*circular_buf[MAX_MPS_DATAMSG_QLEN+1];	//ring Q
	u32					rpos, wpos;		//read/write index
} MPS_DATAMSG_LIST;

//some useful macros
#if	0	//prochao+, 11/13/2006
#define MPS_DATAMSG_NEXT_Q_POS(pos)	(((pos) + 1) % MAX_MPS_DATAMSG_QLEN)
#else	//prochao%
#define MPS_DATAMSG_NEXT_Q_POS(pos)	((pos) < MAX_MPS_DATAMSG_QLEN ? ((pos)+1) : 0)
#endif	//prochao-

#define MPS_DATAMSG_SET_Q_ITEM(queue, node) 	\
	(queue)->circular_buf[(queue)->wpos] = (node)

#define MPS_DATAMSG_GET_Q_ITEM(queue) 	\
	(queue)->circular_buf[(queue)->rpos]

#define MPS_DATAMSG_IS_Q_FULL(queue)		\
	((queue) && ((MPS_DATAMSG_NEXT_Q_POS((queue)->wpos)) == ((queue)->rpos)))

#define MPS_DATAMSG_IS_Q_EMPTY(queue)		\
	(!(queue) || (queue)->rpos == (queue)->wpos)

#define MPS_DATAMSG_GET_Q_COUNT(queue)	\
	(queue)->rpos <= ((queue)->wpos) ? \
		((queue)->wpos - (queue)->rpos): ((queue)->wpos + 1 +  MAX_MPS_DATAMSG_QLEN - (queue)->rpos)

//exported APIs
void	mps_datamsg_queue_init(MPS_DATAMSG_LIST *q);
s32		mps_datamsg_enqueue_tail(mps_data_message *buf, MPS_DATAMSG_LIST *q);
mps_data_message	*mps_datamsg_dequeue_head(MPS_DATAMSG_LIST *q);

// prochao-
//==============================================================================

//prochao+, 11/02/2006
enum {
	MPS_CMD_NULL_CALLBACK = 0,
	MPS_CMD_SKBUFF_ALLOC_CALLBACK,		//1
	MPS_CMD_SKBUFF_FREE_CALLBACK,		//2
	//
	MPS_CMD_MAX_NUM			// cmd number exceeding this legal value is error!
};
//prochao-

//prochao, moved here
/**
 * Firmware structure.
 * This structure contains a pointer to the firmware and its length.
 */
typedef struct
{
   u32   *data;  /**< Pointer to firmware image */
   u32   length; /**< Length of firmware in bytes */
} mps_fw;

/*
* MPS CPU ID Types
-*/
typedef enum {
	MPS_CPU_0_ID = 0,
	MPS_CPU_1_ID
} MPS_CPU_ID_T;

//prochao+
typedef enum {
	MPS_DATA_MSG_TYPE = 0,	//upstream/downstream (CPU1 <-> CPU0) data message
	MPS_CMD_MSG_TYPE,		//upstream/downstream (CPU1 <-> CPU0) command message
	//
	MAX_MPS_MSG_TYPE_ID		//use to protect from invalid ones
} MPS_MSG_TYPE;
//prochao-

/*---------------------------------------------------------------------------*/
/* FIFO structure                                                            */
/*---------------------------------------------------------------------------*/

typedef struct
{
    volatile u32 *pstart;     /**< Pointer to FIFO's read/write start address */
    volatile u32 *pend;       /**< Pointer to FIFO's read/write end address */
    volatile u32 *pwrite_off; /**< Pointer to FIFO's write index location */
    volatile u32 *pread_off;  /**< Pointer to FIFO's read index location */
    volatile u32  size;       /**< FIFO size */
    volatile u32  min_space;  /**< FIFO size */
} mps_fifo;


typedef struct
{
    volatile u32      MPS_BOOT_RVEC;   /**< CPU reset vector */
    volatile u32      MPS_BOOT_NVEC;   /**<  */
    volatile u32      MPS_BOOT_EVEC;   /**<  */
    volatile u32      MPS_CP0_STATUS;  /**<  */
    volatile u32      MPS_CP0_EEPC;    /**<  */
    volatile u32      MPS_CP0_EPC;     /**<  */
    volatile u32      MPS_BOOT_SIZE;   /**<  */
    volatile u32      MPS_CFG_STAT;    /**<  */

} mps_boot_cfg_reg;


/*
 * This structure represents the MPS mailbox definition area that is shared
 * by CCPU and VCPU. It comprises the mailboxes' base addresses and sizes in bytes as well as the
 *
 *
 */
typedef struct
{
    volatile u32*     MBX_UPSTR_CMD_BASE;  /**< Upstream Command FIFO Base Address */
    volatile u32      MBX_UPSTR_CMD_SIZE;  /**< Upstream Command FIFO size in byte */
    volatile u32*     MBX_DNSTR_CMD_BASE;  /**< Downstream Command FIFO Base Address */
    volatile u32      MBX_DNSTR_CMD_SIZE;  /**< Downstream Command FIFO size in byte */
    volatile u32*     MBX_UPSTR_DATA_BASE; /**< Upstream Data FIFO Base Address */
    volatile u32      MBX_UPSTR_DATA_SIZE; /**< Upstream Data FIFO size in byte */
    volatile u32*     MBX_DNSTR_DATA_BASE; /**< Downstream Data FIFO Base Address */
    volatile u32      MBX_DNSTR_DATA_SIZE; /**< Downstream Data FIFO size in byte */
    volatile u32      MBX_UPSTR_CMD_READ;  /**< Upstream Command FIFO Read Index */
    volatile u32      MBX_UPSTR_CMD_WRITE; /**< Upstream Command FIFO Write Index */
    volatile u32      MBX_DNSTR_CMD_READ;  /**< Downstream Command FIFO Read Index */
    volatile u32      MBX_DNSTR_CMD_WRITE; /**< Downstream Command FIFO Write Index */
    volatile u32      MBX_UPSTR_DATA_READ;  /**< Upstream Data FIFO Read Index */
    volatile u32      MBX_UPSTR_DATA_WRITE; /**< Upstream Data FIFO Write Index */
    volatile u32      MBX_DNSTR_DATA_READ;  /**< Downstream Data FIFO Read Index */
    volatile u32      MBX_DNSTR_DATA_WRITE; /**< Downstream Data FIFO Write Index */
    volatile u32      MBX_DATA[MBX_DATA_WORDS];
    mps_boot_cfg_reg  MBX_CPU0_BOOT_CFG;    /**< CPU0 Boot Configuration */
    mps_boot_cfg_reg  MBX_CPU1_BOOT_CFG;    /**< CPU1 Boot Configuration */
} mps_mbx_reg;
/*---------------------------------------------------------------------------*/
//prochao+, 0901

/*---------------------------------------------------------------------------*/
/* Device connection structure                                               */
/*---------------------------------------------------------------------------*/

/**
 * MPS Mailbox Device Structure. This Structure holds top level
 * parameters of the mailboxes used to allow the communication
 * between the control CPU and the Voice CPU
 */
typedef struct
{
    /* void pointer to the base device driver structure */
    void               *pVCPU_DEV;

    /* Mutex semaphore to access the device */
    struct semaphore   *sem_dev;

    /* Mutex semaphore to access the device */
    struct semaphore   *sem_read_fifo;

    /* Wakeuplist for the select mechanism */
    wait_queue_head_t  mps_wakeuplist;

    /* Base Address of the Global Register Access */
    mps_mbx_reg        *base_global;

    mps_fifo		upstrm_fifo;  /**< Data exchange FIFO for write (downstream) */
    mps_fifo		dwstrm_fifo;  /**< Data exchange FIFO for read (upstream) */

#ifdef MPS_FIFO_BLOCKING_WRITE
    struct semaphore *sem_write_fifo;
    volatile bool_t  full_write_fifo;
    /* variable if the driver should block on write to the transmit FIFO */
    bool_t           bBlockWriteMB;
#endif /* MPS_FIFO_BLOCKING_WRITE */

#ifdef CONFIG_PROC_FS
    /* interrupt counter */
    volatile u32     RxnumIRQs;
    volatile u32     TxnumIRQs;
    volatile u32     RxnumMiss;
    volatile u32     TxnumMiss;
    volatile u32     RxnumBytes;
    volatile u32     TxnumBytes;
#endif /* CONFIG_PROC_FS */

    mps_devices      devID;  /**< Device ID  1->command
                                             2->wifi 0
                                             3->wifi 1
                                             4->wifi 2
                                             5->wifi 3 */
    bool_t           Installed;
	//moved following callback functions array to inside the MPS device structure
//	FNPTR_T			callback_function[ MPSDRV_TOTAL_SEQ_NUMBERS];	//array of function pointers, indexed with the sequence#
    MbxEventRegs_s	event_mask;	//??? duplicated ???

    struct semaphore	*wakeup_pending;
} mps_mbx_dev;
/*---------------------------------------------------------------------------*/


//prochao+, 11/30/2006, adds one timer tasklet to do the housekeeping task
#define	MPS_USE_HOUSEKEEPING_TIMER		1
//prochao-

/*---------------------------------------------------------------------------*/
/* Device structure                                                          */
/*---------------------------------------------------------------------------*/

/**
 * Dual-core MPS Device Structure.
 * This Structure represents the communication device that
 * provides the resources for the communication between CPU0 and
 * CPU1
 */
typedef struct
{
    mps_mbx_reg			*base_global; /**< global register pointer for the ISR */
    u32	           		flags;        /**< Pointer to private date of the specific handler */
	//
    mps_mbx_dev    		wifi_mbx[ MPSDRV_NUM_WIFI_CHANNEL];   /**< Data upstream and downstream mailboxes */
    mps_mbx_dev    		command_mbx;                   /**< Command upstream and downstream mailbox */
	//prochao+
	MPSFNPTR_T			callback_function[ MPSDRV_TOTAL_SEQ_NUMBERS];	//array of function pointers, indexed with the sequence#
    MbxEventRegs_s   	event;		/**< global structure holding the interrupt status */
	//
    struct semaphore*	wakeup_pending;
//  struct semaphore*	provide_buffer;
} mps_comm_dev;

/*---------------------------------------------------------------------------*/
s32 ifx_mpsdrv_reset(void);
s32 ifx_mpsdrv_release(void);
//u32 ifx_mpsdrv_fifo_mem_available(mps_fifo *mbx_fifo);

/*---------------------------------------------------------------------------*/
s32 ifx_mpsdrv_common_open(mps_mbx_dev *pMBDev, bool_t flagCmnd);
s32 ifx_mpsdrv_common_close(mps_mbx_dev *pMBDev);
u32 ifx_mpsdrv_initial_structures(mps_comm_dev *pDev);
int ifx_mpsdrv_init_IRQs(mps_comm_dev *pDev);			//added for HW IRQs of each channel
void ifx_mpsdrv_release_structures(mps_comm_dev *pDev);

s32 ifx_mpsdrv_register_callback(u8 seq_num, void (*callback)(mps_data_message *));
s32 ifx_mpsdrv_unregister_callback(u8 seq_num);

// the seq_num is embedded in the message body
s32 ifx_mpsdrv_mbx_rcv(mps_message *pMsg, MPS_MSG_TYPE type);
s32 ifx_mpsdrv_mbx_send(mps_message *pMsg);

s32 ifx_mpsdrv_restart(void);
//parameters definition should be more clear between load_entry, exec_entry, and pFWDwnld.
s32 ifx_mpsdrv_download_firmware(void *load_entry, void *exec_entry, mps_fw *pFWDwnld);	// implemented later

MPS_CPU_ID_T ifx_mpsdrv_get_cpu_id(void);
int ifx_mpsdrv_get_fw_version(void);	//returned number as N*10000 + nn*100 + bb, indicating version N.nn.bb
//prochao+, 12/21/2006
#define	MPS_MPS1_APPS_READY_FP		0x4F6B0000		//"Ok" with 16-bit
#define	MPS_MPS1_APPS_READY_FP_MASK	0xFFFF0000
#define	MPS_MPS1_APPS_READY_BITMASK	0x0000FFFF

#ifdef	CONFIG_DANUBE_CORE1
void ifx_mpsdrv_ctrl_mps1_apps_ready_bit(u16 clr_set, u16 bitn);
#endif
u16 ifx_mpsdrv_get_mps1_apps_ready_bits(void);
//prochao-,
//
// prochao+
void ifx_mpsdrv_initialize_KT(void);
void ifx_mpsdrv_stop_KT(void);
#if	(!defined(CONFIG_DANUBE_CORE1) && defined(MPS_USE_HOUSEKEEPING_TIMER))
void ifx_mpsdrv_initialize_hk_timer(void);	//initialize the housekeeping timer
#endif
//prochao-

#endif /* __MPS_TPE_DEVICE_H */

