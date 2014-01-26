#define MPS_SRV_DS_DATA_ID	10
#define MPS_SRV_DS_MSG_ID	11
#define MPS_SRV_US_DATA_ID	20
#define MPS_SRV_US_MSG_ID	21

#define IFX_OK 0
/* not defined in ifx mps document */
#define IFX_EFAIL
#define IFX_EBUSY
#define IFX_EINVAL
//procha0+, double as suggested by Ritesh
#define MPS_ENTRY_NUM	512		//256		//180
//prochao-
#define MPS_QUEUE_SIZE	3328

//extern int ifx_mps_register_callback(unsigned char, void (*)(void *, unsigned int));
extern int ifx_mps_unregister_callback(unsigned char);
//extern int ifx_mps_data_send(unsigned char, void *, int); ernest remove
extern void* ifx_mps_skbuff_alloc(unsigned int);
extern void ifx_mps_skbuff_free(void *);
//extern int32_t ifx_mps_is_mps_buffer(struct sk_buff *); ernest remove
extern struct sk_buff *ifx_mps_skb_copy(const struct sk_buff *skb,u8 service_seq_num);
extern struct sk_buff *ifx_mps_skb_clone(const struct sk_buff *skb,u8 service_seq_num);
//extern int ifx_mps_skb_copy_expand(struct sk_buff *skb,int nhead, int ntail, u8 service_seq_num); ernest remove
extern int ifx_mps_pskb_expand_head(struct sk_buff *skb,int nhead, int ntail, u8 service_seq_num);
//extern struct sk_buff *ifx_mps_skb_realloc_headroom(struct sk_buff *skb,unsigned int headroom, u8 service_seq_num); ernest 2006-11-29 
extern int ifx_mps_skb_trim(struct sk_buff *skb,unsigned int len,u8 service_seq_num);
//ernest 2006-11-29 

#define mps_dev_kfree_skb(a)	mps_kfree_skb(a)
//#define mps_dev_kfree_skb(a)	dev_kfree_skb(a)

#define MPS_SEQ_NUM 10
extern int mps_netif_receive_skb(struct sk_buff *);

#if	1	//prochao+, 11/27/2006
#define mps_dev_alloc_skb(len)		mps_dev_alloc_skb1(len)
#else	//prochao%
#define mps_dev_alloc_skb(len)	\
	mps_dev_alloc_skb1(len), printk(KERN_INFO "%s:%d  Alloc skb Len=%8d\n", __func__, __LINE__, (u32)(len))
#endif	//prochao-

static inline struct sk_buff *mps_dev_alloc_skb1(unsigned int length)
{
    struct sk_buff *skb;
#if	0	//prochao+, 01/15/2007, in Jungo, try to fix the worse upstream throughput (using the iperf)
    skb = (MPS_BUF*)ifx_mps_fast_alloc_mps_buf(length, GFP_ATOMIC, MPS_SEQ_NUM);
#else
	skb = (MPS_BUF*)ifx_mps_fast_alloc_mps_buf(length, GFP_ATOMIC, MPS_SRV_US_DATA_ID);
#endif
    return skb;
}

#if	1	//prochao+, 11/27/2006
#define mps_kfree_skb(skb)	\
	do {			\
		mps_kfree_skb1(skb);	\
	} while(0)
#else	//prochao%
#if 1 
#define mps_kfree_skb(skb)	\
	do {			\
		printk(KERN_INFO "%s:%d Free skb=%08x\n", __func__, __LINE__, (u32)skb); \
		mps_kfree_skb1(skb);	\
	}while(0)
#else
#define mps_kfree_skb(skb)	\
	do {			\
		printk(KERN_INFO "%s:%d Free skb=%08x\n", __func__, __LINE__, (u32)skb);\
		dev_kfree_skb(skb);	\
	}while(0)
#endif
#endif	//prochao-
		
static inline void mps_kfree_skb1(struct sk_buff *skb)
{
	int ret; //ernest 2006-11-29 
//    printk(KERN_INFO "%s:%d skb=%08x\n", __func__, __LINE__, (u32)skb);
    ret = ifx_mps_fast_free_mps_buf((MPS_BUF volatile *)skb, MPS_SRV_DS_DATA_ID);
//    printk(KERN_INFO "%s:%d skb=%08x\n", __func__, __LINE__, (u32)skb);
}
