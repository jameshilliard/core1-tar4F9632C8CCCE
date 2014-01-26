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

/* Group definitions for Doxygen */
/** \addtogroup IOCTL Defined IOCTLs */

#ifndef __MPS_DUALCORE_H
#define __MPS_DUALCORE_H

/**
 * MPS Mailbox Message.
 */
//prochao
typedef struct {
	u32	msg_buf_addr;
	u32	msg_buf_len;
} mps_msg_buf;		//if any, following the mps message header

typedef struct
{
//common message header, others depending on the type of message (data or command)
	u8	msg_type;		// data or command
	u8	msg_seqno;		// sequence#
	u16	msg_cmd;		// command specified
	u32	msg_prio:			8;	// priority
	u32	msg_pad:			1;	// paddings, if any
	// following field should be ignored by the upper-layer functions
	u32	msg_rsrv:			7;	// reserved
	u32 msg_ready:			1;	// this message is ready (available)
	u32 msg_next_wrapped:	1;	//prochao+, indicating the next message will be wrapped at the beginning of its fifo
	u32	msg_res6:			6;	// reserved
	u32	msg_len:			8;	// number of bytes following this length (len) field
	// data buffer list or command payload
//	mps_msg_buf	msg_buffers;
} __attribute__ ((packed)) mps_message;

//tricky way to access the message header
typedef struct {
    u32 val0;
    u32 val1;
} mps_message_alias;

#define	MPS_MSGHDR_LEN		sizeof(mps_message)

//prochao+, 10/29/2006, adds the data structure for keeping different command parameters
typedef struct {
	union {
		// xxx_IFX_ALLOC_SKB
		struct {
			unsigned int	size;
			int			priority;
		} alloc_skb;
		// xxx_IFX_SKB_CLONE & xxx_IFX_SKB_COPY
		struct {
			struct sk_buff	*skb;
			int			priority;
		} skb_clone_copy;
		// xxx_IFX_PSKB_EXPAND_HEAD
		struct {
			struct sk_buff	*skb;
			int			nhead;
			int			ntail;
			int			qfp_mask;
		} pskb_expand_head;
		// xxx_IFX_SKB_REALLOC_HEADROOM
		struct {
			struct sk_buff	*skb;
			unsigned int	headroom;
		} skb_realloc_headroom;
		// xxx_IFX_SKB_COPY_EXPAND
		struct {
			struct sk_buff	*skb;
			int			newheadroom;
			int			newtailroom;
			int			priority;
		} skb_copy_expand;
		// xxx_IFX_SKB_TRIM
		struct {
			struct sk_buff	*skb;
			unsigned int	len;
		} skb_trim;
		// xxx_IFX_DEV_ALLOC_SKB
        struct {
			unsigned int	len;
			int				priority;
		} dev_alloc_skb;
		// others to be added can be here
	} param;
} mps_cmd_param_t;

// useful abbreviations
#define	cmd_alloc_skb_param_size				param.alloc_skb.size
#define	cmd_alloc_skb_param_prio				param.alloc_skb.priority

#define	cmd_skb_clone_copy_param_skb			param.skb_clone_copy.skb
#define	cmd_skb_clone_copy_param_prio			param.skb_clone_copy.priority

#define	cmd_pskb_expand_head_param_skb			param.pskb_expand_head.skb
#define	cmd_pskb_expand_head_param_nhead		param.pskb_expand_head.nhead
#define	cmd_pskb_expand_head_param_ntail		param.pskb_expand_head.ntail
#define	cmd_pskb_expand_head_param_qfp_mask		param.pskb_expand_head.qfp_mask

#define	cmd_skb_realloc_headroom_param_skb		param.skb_realloc_headroom.skb
#define	cmd_skb_realloc_headroom_param_headroom	param.skb_realloc_headroom.headroom

#define	cmd_skb_copy_expand_param_skb			param.skb_copy_expand.skb
#define	cmd_skb_copy_expand_param_newheadroom	param.skb_copy_expand.newheadroom
#define	cmd_skb_copy_expand_param_newtailroom	param.skb_copy_expand.newtailroom
#define	cmd_skb_copy_expand_param_prio			param.skb_copy_expand.priority

#define	cmd_skb_trim_param_skb					param.skb_trim.skb
#define	cmd_skb_trim_param_len					param.skb_trim.len

#define	cmd_dev_alloc_skb_param_len				param.dev_alloc_skb.len
#define	cmd_dev_alloc_skb_param_prio			param.dev_alloc_skb.priority

//prochao-

// prochao+, 11/11/2006, moves here from inside the mps_tpe_device.h file
union msg_content
{
	mps_msg_buf		msg_body;
	u32				payload;
	int				ret_int;		//prochao+
	mps_cmd_param_t cmd_parameters;	//prochao+
	struct sk_buff	*skb;
} __attribute__ ((packed));

#define	MPS_CMD_PARAMETER_SIZE		sizeof(mps_cmd_param_t)
#define	MPS_DATA_MSG_CONTENT_SIZE	sizeof(union msg_content)	//minimum size of single mps_data_message content

typedef struct
{
	mps_message			data_msg_header;
	union msg_content	data_msg_content;	//[MAX_PAYLOAD_LEN];
} __attribute__ ((packed)) mps_data_message;
//prochao-

typedef void	(*FNPTR_T)(void *);	//prochao+- 11/12/2006
typedef void	(*MPSFNPTR_T)(mps_data_message *);	//prochao+-, 11/12/2006

//-------------------------------------------------------
//data structures defined for new dual-core MPS driver
struct data_list {
	void	*data;
	u32		len;
	struct data_list	*next;
};

typedef struct {
	int				no;		// number of data pointers
	struct data_list	*datalist;
} mps_datalist;

//=============================================================
//prochao+,
typedef enum
{
   unknown=0,			//prochao+
   command,				//as the command channel interface
   wifi0,				//as the WLAN 1st channel interface
   wifi1,				//as the WLAN 2nd channel interface
   wifi2,				//as the WLAN 3rd channel interface, if any (currently not planned yet)
   wifi3				//as the WLAN 4th channel interface, if any (currently not planned yet)
} mps_devices;

/**
 * Firmware structure.
 * This structure contains a pointer to the firmware and its length.
 */
typedef struct
{
   u32   *data;  /**< Pointer to firmware image */
   u32   length; /**< Length of firmware in bytes */
} mps_dualcore_fw;

#define CMD_VOICEREC_STATUS_PACKET  0x0
#define CMD_VOICEREC_DATA_PACKET    0x1
#define CMD_RTP_VOICE_DATA_PACKET   0x4
#define CMD_RTP_EVENT_PACKET        0x5
#define CMD_ADDRESS_PACKET          0x8
#define CMD_FAX_DATA_PACKET         0x10
#define CMD_FAX_STATUS_PACKET       0x11
#define CMD_P_PHONE_DATA_PACKET     0x12
#define CMD_P_PHONE_STATUS_PACKET   0x13
#define CMD_CID_DATA_PACKET         0x14

#define CMD_ALI_PACKET              0x1
#define CMD_COP_PACKET              0x2
#define CMD_EOP_PACKET              0x6

/******************************************************************************
 * Exported IOCTLs
 ******************************************************************************/
/** magic number */
#define IFX_MPS_MAGIC 'O'

//prochao+, not all of followings are supported for the dual-core WLAN

/**
 * Set event notification mask.
 * \param   arg Event mask
 * \ingroup IOCTL
 */
#define FIO_MPS_REG _IOW(IFX_MPS_MAGIC, 1, unsigned int)
/**
 * Mask Event Notification.
 * \ingroup IOCTL
 */
#define FIO_MPS_UNREG _IO(IFX_MPS_MAGIC, 2)
/**
 * Read Message from Mailbox.
 * \param   arg Pointer to structure #mps_message
 * \ingroup IOCTL
 */
#define FIO_MPS_MB_READ _IOR(IFX_MPS_MAGIC, 3, mps_message)
/**
 * Write Message to Mailbox.
 * \param   arg Pointer to structure #mps_message
 * \ingroup IOCTL
 */
#define FIO_MPS_MB_WRITE _IOW(IFX_MPS_MAGIC, 4, mps_message)
/**
 * Reset Voice CPU.
 * \ingroup IOCTL
 */
#define FIO_MPS_RESET _IO(IFX_MPS_MAGIC, 6)
/**
 * Restart Voice CPU.
 * \ingroup IOCTL
 */
#define FIO_MPS_RESTART _IO(IFX_MPS_MAGIC, 7)
/**
 * Read Version String.
 * \param   arg Pointer to version string.
 * \ingroup IOCTL
 */
#define FIO_MPS_GETVERSION      _IOR(IFX_MPS_MAGIC, 8, char*)
/**
 * Reset Mailbox Queue.
 * \ingroup IOCTL
 */
#define FIO_MPS_MB_RST_QUEUE _IO(IFX_MPS_MAGIC, 9)
/**
 * Download Firmware
 * \param   arg Pointer to structure #mps_fw
 * \ingroup IOCTL
 */
#define  FIO_MPS_DOWNLOAD _IO(IFX_MPS_MAGIC, 17)
/**
 * Set FIFO Blocking State.
 * \param   arg Boolean value [0=off,1=on]
 * \ingroup IOCTL
 */
#define  FIO_MPS_TXFIFO_SET _IOW(IFX_MPS_MAGIC, 18, bool_t)
/**
 * Read FIFO Blocking State.
 * \param   arg Boolean value [0=off,1=on]
 * \ingroup IOCTL
 */
#define  FIO_MPS_TXFIFO_GET _IOR(IFX_MPS_MAGIC, 19, bool_t)
/**
 * Read channel Status Register.
 * \param   arg Content of status register
 * \ingroup IOCTL
 */
#define  FIO_MPS_GET_STATUS _IOR(IFX_MPS_MAGIC, 20, u32)


typedef enum
{
   //mps_cmd_cpu0
   MPS_CMD_CPU0_CMDMSG=0x0100,
   MPS_CMD_CPU0_DATAMSG,
   MPS_CMD_CPU0_DATALISTMSG,
   MPS_CMD_CPU0_APPMSG,
   MPS_CMD_CPU0_DATA_ALLOC,
   MPS_CMD_CPU0_DATA_FREE,
   MPS_CMD_CPU0_SKB_ALLOC,
   MPS_CMD_CPU0_SKB_FREE,
   MPS_CMD_CPU0_DATALIST_ALLOC,
   MPS_CMD_CPU0_DATALIST_FREE,
   MPS_CMD_CPU0_AGGREGATOR_ALLOC,
   MPS_CMD_CPU0_AGGREGATOR_FREE,
   MPS_CMD_CPU0_AGGREGATOR_BUFFER_ALLOC,
   MPS_CMD_CPU0_AGGREGATOR_BUFFER_FREE,
   MPS_CMD_CPU0_AGGREGATOR_BUFFER_GET,
   MPS_CMD_CPU0_AGGREGATOR_BUFFER_COMMIT,
   MPS_CMD_CPU0_AGGREGATOR_GET,
   MPS_CMD_CPU0_AGGREGATOR_SET,
   MPS_CMD_CPU0_AGGREGATOR_INDEX_GET,
   MPS_CMD_CPU0_AGGREGATOR_INDEX_SET,
   MPS_CMD_CPU0_FILEBLOCK_ALLOC,
   MPS_CMD_CPU0_FILEBLOCK_FREE,
   MPS_CMD_CPU0_GET_REMOTE_VER,
// prochao+, 10/29/2006, new commands for sk_buff equivalent replacements from MPS driver APIs provisioning
	MPS_CMD_CPU0_IFX_ALLOC_SKB,
	MPS_CMD_CPU0_IFX_SKB_CLONE,
	MPS_CMD_CPU0_IFX_SKB_COPY,
	MPS_CMD_CPU0_IFX_PSKB_EXPAND_HEAD,
	MPS_CMD_CPU0_IFX_SKB_REALLOC_HEADROOM,
	MPS_CMD_CPU0_IFX_SKB_COPY_EXPAND,
	MPS_CMD_CPU0_IFX_SKB_TRIM,
	MPS_CMD_CPU0_IFX_DEV_ALLOC_SKB,
// prochao-

   //mps_cmd_cpu1
   MPS_CMD_CPU1_CMDMSG=0x0200,
   MPS_CMD_CPU1_DATAMSG,
   MPS_CMD_CPU1_DATALISTMSG,
   MPS_CMD_CPU1_APPMSG,
   MPS_CMD_CPU1_DATA_ALLOC,
   MPS_CMD_CPU1_DATA_FREE,
   MPS_CMD_CPU1_SKB_ALLOC,
   MPS_CMD_CPU1_SKB_FREE,
   MPS_CMD_CPU1_DATALIST_ALLOC,
   MPS_CMD_CPU1_DATALIST_FREE,
   MPS_CMD_CPU1_AGGREGATOR_ALLOC,
   MPS_CMD_CPU1_AGGREGATOR_FREE,
   MPS_CMD_CPU1_AGGREGATOR_BUFFER_ALLOC,
   MPS_CMD_CPU1_AGGREGATOR_BUFFER_FREE,
   MPS_CMD_CPU1_AGGREGATOR_BUFFER_GET,
   MPS_CMD_CPU1_AGGREGATOR_BUFFER_COMMIT,
   MPS_CMD_CPU1_AGGREGATOR_GET,
   MPS_CMD_CPU1_AGGREGATOR_SET,
   MPS_CMD_CPU1_AGGREGATOR_INDEX_GET,
   MPS_CMD_CPU1_AGGREGATOR_INDEX_SET,
   MPS_CMD_CPU1_FILEBLOCK_ALLOC,
   MPS_CMD_CPU1_FILEBLOCK_FREE,
   MPS_CMD_CPU1_GET_REMOTE_VER,
// prochao+, 10/29/2006, new commands for sk_buff equivalent replacements from MPS driver APIs provisioning
	MPS_CMD_CPU1_IFX_ALLOC_SKB,
	MPS_CMD_CPU1_IFX_SKB_CLONE,
	MPS_CMD_CPU1_IFX_SKB_COPY,
	MPS_CMD_CPU1_IFX_PSKB_EXPAND_HEAD,
	MPS_CMD_CPU1_IFX_SKB_REALLOC_HEADROOM,
	MPS_CMD_CPU1_IFX_SKB_COPY_EXPAND,
	MPS_CMD_CPU1_IFX_SKB_TRIM,
	MPS_CMD_CPU1_IFX_DEV_ALLOC_SKB,
// prochao-

} mps_cmd;


#define MPS_TYPE_UPSTREAM_DATA				0x01
#define MPS_TYPE_DOWNSTREAM_DATA			0x02
#define MPS_TYPE_UPSTREAM_CMD				0x03
#define MPS_TYPE_DOWNSTREAM_CMD				0x04
#define MPS_TYPE_UPSTREAM_RESPONSE			0x05
#define MPS_TYPE_DOWNSTREAM_RESPONSE		0x06
#define MPS_TYPE_UPSTREAM_APP_CMD			0x07
#define MPS_TYPE_DOWNSTREAM_APP_CMD		0x08
#define MPS_TYPE_UPSTREAM_APP_RESPONSE		0x09
#define MPS_TYPE_DOWNSTREAM_APP_RESPONSE	0x0A


/******************************************************************************
 * Register structure definitions
 ******************************************************************************/
//prochao, followings should be redefined for dual-core MPS driver ???
typedef struct  /**< Register structure for Common status registers MPS_RAD0SR, MPS_SAD0SR, MPS_CAD0SR and MPS_AD0ENR  */
{
    u32 res1      : 17;
    u32 wd_fail   : 1;
    u32 res2      : 2;
    u32 mips_ol   : 1;
    u32 data_err  : 1;
    u32 dd_mbx	  : 1;	//downstream data MBX,		//prochao+-
    u32 cd_mbx    : 1;	//downstream command MBX,	//prochao+-
    u32 res3      : 6;
    u32 du_mbx    : 1;	//upstream data MBX
    u32 cu_mbx    : 1;  //upstream command MBX
} MPS_Ad0Reg_s;


typedef union
{
    u32          val;
    MPS_Ad0Reg_s fld;
} MPS_Ad0Reg_u;


typedef struct  /**< Register structure for Common status registers MPS_RAD1SR, MPS_SAD1SR, MPS_CAD1SR and MPS_AD1ENR  */
{
	u32 res1      : 17;
	u32 wd_fail   : 1;
	u32 res2      : 2;
	u32 mips_ol   : 1;
	u32 data_err  : 1;
	u32 dd_mbx	  : 1;	//downstream data MBX,		//prochao+-
	u32 cd_mbx    : 1;	//downstream command MBX,	//prochao+-
	u32 res3      : 6;
	u32 du_mbx    : 1;	//upstream data MBX
	u32 cu_mbx    : 1;  //upstream command MBX
} MPS_Ad1Reg_s;


typedef union
{
    u32          val;
    MPS_Ad1Reg_s fld;
} MPS_Ad1Reg_u;


typedef struct   /**< Register structure for Voice Channel Status registers MPS_RVCxSR, MPS_SVCxSR, MPS_CVCxSR and MPS_VCxENR   */
{
    u32 dtmfr_dt   : 1;
    u32 dtmfr_pdt  : 1;
    u32 dtmfr_dtc  : 4;
    u32 res1       : 1;
    u32 utgus      : 1;
    u32 cptd       : 1;
    u32 rcv_ov     : 1;
    u32 dtmfg_buf  : 1;
    u32 dtmfg_req  : 1;
    u32 tg_inact   : 1;
    u32 cis_buf    : 1;
    u32 cis_req    : 1;
    u32 cis_inact  : 1;
	//prochao+
    u32 res2       : 8;
	u32	usdata     : 1;		//upstream wifi data
	u32	dsdata     : 1;		//downstream wifi data
	//prochao-
    u32 lin_req    : 1;
    u32 epq_st     : 1;
    u32 vpou_st    : 1;
    u32 vpou_jbl   : 1;
    u32 vpou_jbh   : 1;
    u32 dec_chg    : 1;
} MPS_VCStatReg_s;

typedef union
{
    u32             val;
    MPS_VCStatReg_s fld;
} MPS_VCStatReg_u;

typedef struct
{
    MPS_Ad0Reg_u    MPS_Ad0Reg;
    MPS_Ad1Reg_u    MPS_Ad1Reg;
    MPS_VCStatReg_u MPS_VCStatReg[4];
} MbxEventRegs_s;

//Data type of the data-related callback function
//i.e. (*callback_fnptr)(*pointer2buf, sizeof buf)
typedef void	(*DATAFNPTR_T)(void *bufptr, int len);	//prochao+-, 10/17/2006, moved to be in the mps_dualcore.h file

/******************************************************************************
 * Exported functions
 ******************************************************************************/
#ifdef __KERNEL__
s32 ifx_mps_open(struct inode *inode, struct file *file_p);
s32 ifx_mps_close(struct inode *inode, struct file *filp);
s32 ifx_mps_ioctl(struct inode *inode, struct file *file_p, u32 nCmd, unsigned long arg);
//
//prochao+-, for new dual-core MPS driver
int	ifx_mps_register_callback(u8 service_seq_num, void (*callback)(void *));
int	ifx_mps_unregister_callback(u8 service_seq_num);
int	ifx_mps_data_send(u8 service_seq_num, void *pdata, u32 len);
int	ifx_mps_datalist_send(u8 service_seq_num, mps_datalist plist);
int	ifx_mps_data_rcv(u8 service_seq_num, void *pdata, u32* len);
int	ifx_mps_datalist_rcv(u8 service_seq_num, mps_datalist *plist);
mps_datalist    *ifx_mps_datalist_alloc(int n);
void	ifx_mps_datalist_free(mps_datalist *plist);
void	*ifx_mps_skbuff_alloc(u32 bufsize);	//prochao, 10/15/2006, change the definition because Atheros needs the bufsize
void	ifx_mps_skbuff_free(void *pbuf);
//prochao+, 10/18/2006, to support the normal buffer alloc/free mechanism for the dual-core communication!
//following 2 APIs can be used to allocate the buffer for data/controll communicating/exchanging between both CPUs (CPU0/CPU1)
void	*ifx_mps_app_buff_kmalloc(u32 bufsize);	//using the formal kmalloc() to allocate the required size of data buffer
void	ifx_mps_app_buff_kfree(void *pbuf);     //using the formal kfree() to free the allocated buffer by above *_kmalloc()
//prochao-, 10/18/2006
int	ifx_mps_app_message_send(u8 service_seq_num, void *pmsg, u32 len);
int	ifx_mps_app_message_rcv(u8 service_seq_num, void *pmsg, u32 *len);

//-------------------------------------------------------------------------------------------------------------------
// prochao+, 10/29/2006, adds the new APIs for sk_buff provisioning equivalent replacements
//#define	ifx_mps_dev_kfree_skb	ifx_mps_skbuff_free		//can use the available one

int32_t ifx_mps_dev_kfree_skb(MPS_BUF *buf, u8 service_seq_num);	//prochao+-

#ifdef	CONFIG_DANUBE_CORE1
//struct sk_buff	*ifx_mps_alloc_skb(unsigned int size, int priority);
MPS_BUF *ifx_mps_alloc_skb(unsigned int size, int priority, u8 service_seq_num);
//struct sk_buff	*ifx_mps_skb_clone(struct sk_buff *skb, int priority);
MPS_BUF *ifx_mps_skb_clone(const MPS_BUF *skb, u8 seq_num);
//struct sk_buff	*ifx_mps_skb_copy(const struct sk_buff *skb, int priority);
MPS_BUF *ifx_mps_skb_copy(const MPS_BUF *skb, u8 service_seq_num);
//int	ifx_mps_pskb_expand_head(struct sk_buff *skb, int nhead, int ntail, int gfp_mask);
int ifx_mps_pskb_expand_head(MPS_BUF *skb, int nhead, int ntail, u8 service_seq_num);
//struct sk_buff	*ifx_mps_skb_realloc_headroom(struct sk_buff *skb, unsigned int headroom);
MPS_BUF * ifx_mps_skb_realloc_headroom(MPS_BUF *skb, unsigned int headroom, u8 service_seq_num);
//struct sk_buff	*ifx_mps_skb_copy_expand(const struct sk_buff *skb, int newheadroom, int newtailroom, int priority);
MPS_BUF *ifx_mps_skb_copy_expand(const MPS_BUF *skb, int newheadroom, int newtailroom, u8 service_seq_num);
//Following two are originally static inline functions
//void			ifx_mps__skb_trim(struct sk_buff *skb, unsigned int len);
void ifx_mps__skb_trim(MPS_BUF *skb, unsigned int len, u8 service_seq_num);
//struct sk_buff	*ifx_mps_dev_alloc_skb(unsigned int length, int priority);	//prochao+-
MPS_BUF *ifx_mps_dev_alloc_skb(unsigned int length, int priority, u8 service_seq_num);

// prochao+, 11-03-2007, to support reading the statistics of remote WiFi driver running on the 2nd core
#else
//@core0 side, needs to be exported
void *ifx_mps_get_core1_wifi_stats(int devid);	//actually the returned pointer is pointed to the net_device_stats structure
void *ifx_mps_get_core1_wireless_stats(int devid);	//returned pointer to the iw_statistics structure
// prochao-, 11-03-2007
#endif
#define	VNET_WIFI0_MAGIC_NUM		0x510708		//prochao+-
// prochao-
//-------------------------------------------------------------------------------------------------------------------

// !!!!!! following commented functions are deferred to next stage !!!!!!
//int	ifx_mps_aggregation_data_send(u8 service_seq_num, void *pdata, u32 len);
//int	ifx_mps_aggregation_data_rcv(u8 service_seq_num, void *pdata, u32 len);
//int	ifx_mps_aggregation_alloc(u8 service_seq_num);
//int	ifx_mps_aggregation_free(u8 service_seq_num);
//void	*ifx_mps_fileblock_alloc(void);
//void	ifx_mps_fileblock_free(void *pbuf);
#endif	/* __KERNEL__ */

#endif /* #ifndef __MPS_DUALCORE_H */

