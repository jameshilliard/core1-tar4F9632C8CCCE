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

#ifndef	_MPS_TPE_BUFFER_	//prochao+
#define _MPS_TPE_BUFFER_

#include <linux/skbuff.h>

//prochao+, 12/13/2006, adds the macro used to identify the allocated buffer is from CPU0 or CPU1 by its physical address range
#ifndef CONFIG_TWINPASS_64_MEG
#define	CPU1_RAM_PHYADDR_BEGIN		0x01400000		//CPU1 occupies the 2nd 16MB, so before that, is for CPU0
#else
#define	CPU1_RAM_PHYADDR_BEGIN		0x03400000		//CPU1 occupies the 2nd 16MB, so before that, is for CPU0
#endif
//prochao-

typedef struct sk_buff	MPS_BUF;


typedef struct mps_buff_head {
	/* These two members must be first. */
	MPS_BUF volatile	* volatile * volatile circular_buf;
	volatile uint32_t	maxcount;
	volatile uint32_t	rpos, wpos;
}MPS_BUFF_HEAD;


typedef struct mps_bufsize_entry {
	volatile MPS_BUFF_HEAD 	* volatile alloc_q;
	volatile MPS_BUFF_HEAD 	* volatile free_q;
	volatile uint32_t 	size; /* Size of the allocated buffers. For future */
}MPS_BUFF_SIZE_ENTRY;

//added from Kishore's additions
//#define CONFIG_MPS_TIMER_OPTIMIZATION		//should be controlled thru the menuconfig setting
typedef struct mps_buff_pool_head {
#ifdef CONFIG_MPS_TIMER_OPTIMIZATION
	struct list_head		list;
	uint32_t			service_seq_num;
#endif
	volatile int32_t		sizes_count;
	volatile uint32_t		free_hit_count; /* for internal use */
	volatile uint32_t		alloc_hit_count; /* for internal use */
	MPS_BUFF_SIZE_ENTRY  volatile	* volatile buff_array;
	MPS_BUFF_HEAD 	     volatile   * volatile default_free_list; /* Default MPS_BUF free list */
	MPS_BUFF_HEAD	     volatile   * volatile malloc_buf_alloc_list;  /* Normal Malloc buf alloc list */
	MPS_BUFF_HEAD	     volatile   * volatile malloc_buf_free_list;   /* Normal Malloc buf free list */
	uint32_t	     volatile   malloc_buf_size;
	uint32_t			tot_free_count;	 /* Total MPS_BUFs released from free lists for seq# */
}MPS_BUFF_POOL_HEAD;

#define MPS_MAX_SELECTORS	256

#define MPS_BUFF_SIZE(buf)  (buf)->truesize

#if 1
#define  MPS_UNCACHED_ADDR(a)	KSEG1ADDR(a)
#else
#define  MPS_UNCACHED_ADDR(a)	a
#endif

#define _MU(a)		(void *)MPS_UNCACHED_ADDR(a)

#define MPS_CACHE_FLUSH(a,s)	dma_cache_wback((uint32_t)(a), (uint32_t)(s))

#define MPS_FLUSH_MPS_BUF(p)					\
	do {							\
	MPS_CACHE_FLUSH((p), sizeof(*(p)));			\
	MPS_CACHE_FLUSH((p)->head, ((p)->end - (p)->head + 1));	\
	}while(0)

typedef void *(*MPS_ALLOC_FN)(uint32_t size);
typedef void (*MPS_FREE_FN)(void *ptr);

//prochao+, 11/13/2006
#if	0	
#define MPS_NEXT_Q_POS(pos, max)	\
	(((pos)+1) % (max))
#else	//prochao%
#define MPS_NEXT_Q_POS(pos, max)	\
	((pos) < (max) ? ((pos)+1) : 0)
#endif
//prochao-

#if	0 
/*
#define MPS_SET_Q_ITEM(queue, node) 		\
	do {					\
		MPS_BUFF_HEAD *uqueue;		\
		MPS_BUF **cbuf;			\
		uqueue = MPS_UNCACHED_ADDR(queue);	\
		unode = MPS_UNCACHED_ADDR(node);	\
		cbuf = MPS_UNCACHED_ADDR(&(uqueue->circular_buf[uqueue->wpos]));\
		*cbuf = unode;	\
	}while(0)
*/
#define MPS_SET_Q_ITEM(queue, node) 		\
	(((MPS_BUF**)_MU(((MPS_BUFF_HEAD*)_MU(queue))->circular_buf))[((MPS_BUFF_HEAD*)_MU(queue))->rpos]  = ((MPS_BUF*)_MU(node)));	\
	wmb()
#else
#if	1	//prochao+, 11/27/2006
#define MPS_SET_Q_ITEM(queue, node) 		\
	(queue)->circular_buf[(queue)->wpos] = (node);	\
	dma_cache_wback((void *)((queue)->circular_buf[(queue)->wpos]), sizeof (node))
#else
#define MPS_SET_Q_ITEM(queue, node) 		\
	printk(KERN_INFO "q[0x%x], q->circular_buf[0x%x], q->wpos[%d:0x%x]\n", (uint32_t)(queue), (uint32_t)(queue)->circular_buf,	\
		(queue)->wpos, (uint32_t)(queue)->circular_buf[(queue)->wpos]);	\
	(queue)->circular_buf[(queue)->wpos] = (node);	\
	dma_cache_wback((void *)((queue)->circular_buf[(queue)->wpos]), sizeof (node))
#endif
#endif

#if	0 
#define MPS_GET_Q_ITEM(queue) 	\
	((MPS_BUF*)(((MPS_BUF**)_MU(((MPS_BUFF_HEAD*)_MU(queue))->circular_buf))[((MPS_BUFF_HEAD*)_MU(queue))->rpos]))
#else
#define MPS_GET_Q_ITEM(queue) 	\
	(queue)->circular_buf[(queue)->rpos]
#endif

#if	0 
#define MPS_IS_Q_FULL(queue)					\
	((queue) && (((MPS_NEXT_Q_POS(((MPS_BUFF_HEAD*)_MU(queue))->wpos,	\
			((MPS_BUFF_HEAD*)_MU(queue))->maxcount)) == ((MPS_BUFF_HEAD*)_MU(queue))->rpos)))
#else
#define MPS_IS_Q_FULL(queue)					\
	((queue) && (((MPS_NEXT_Q_POS((queue)->wpos, (queue)->maxcount)) == (queue)->rpos)))
#endif

#if	0 
#define MPS_IS_Q_EMPTY(queue)		\
	(!(queue) || (((MPS_BUFF_HEAD*)_MU(queue))->rpos == ((MPS_BUFF_HEAD*)_MU(queue))->wpos))
#else
#define MPS_IS_Q_EMPTY(queue)		\
	(!(queue) || (queue)->rpos == (queue)->wpos)
#endif

#if	0 
#define MPS_GET_Q_COUNT(queue)		\
	(((MPS_BUFF_HEAD*)_MU(queue))->rpos <= ((MPS_BUFF_HEAD*)_MU(queue))->wpos) ?			\
		(((MPS_BUFF_HEAD*)_MU(queue))->wpos - ((MPS_BUFF_HEAD*)_MU(queue))->rpos) :		\
		(((MPS_BUFF_HEAD*)_MU(queue))->wpos + 1 + ((MPS_BUFF_HEAD*)_MU(queue))->maxcount - 		\
		((MPS_BUFF_HEAD*)_MU(queue))->rpos)
#else
#if 0 /* [ Bug fix */
#define MPS_GET_Q_COUNT(queue)		\
	(queue)->rpos <= ((queue)->wpos) ? ((queue)->wpos - (queue)->rpos):	\
		((queue)->wpos + (queue)->maxcount - (queue)->rpos)
#else
#define MPS_GET_Q_COUNT(queue, rpos, wpos)		\
({					\
	int cnt=0;			\
	rpos = (queue)->rpos;		\
	wpos = (queue)->wpos;		\
	cnt = ((rpos <= wpos) ? (wpos - rpos):	\
		(wpos + 1 +  (queue)->maxcount - rpos));	\
	cnt;					\
})
#endif
#endif

/*
 * Exported API
 */

/* CPU 1 [ */

MPS_BUF volatile* ifx_mps_fast_alloc_mps_buf(uint32_t size, int priority, u8 service_seq_num);

int32_t ifx_mps_fast_free_mps_buf(MPS_BUF volatile *buf, u8 service_seq_num);
int32_t ifx_mps_is_mps_buffer(MPS_BUF volatile *buf);
#define ifx_is_mps_buffer ifx_mps_is_mps_buffer

int32_t ifx_mps_set_mps_buffer_identity(MPS_BUF volatile *buf, int32_t reset);
#define ifx_set_mps_buffer ifx_mps_set_mps_buffer

void volatile * ifx_mps_fast_alloc_malloc_buf(uint32_t size, int priority, u8 service_seq_num);

int32_t ifx_mps_fast_free_malloc_buf(void volatile *buf, u8 service_seq_num);

/* CPU 1 ] */

/* [ CPU 0 */

volatile MPS_BUFF_POOL_HEAD * volatile *ifx_mps_buff_pool_array_get(void);

/* ifx_mps_buff_pool_init(WLAN_id, 1, 100); */
int32_t ifx_mps_buff_pool_init(u8 mps_service_seq_num, u8 sizes, uint32_t def_freeq_len);

int32_t ifx_mps_malloc_buff_pool_init(u8 mps_service_seq_num, uint32_t alloc_size, uint32_t num_alloc_nodes, uint32_t num_free_nodes);

/* ifx_mps_queue_init(WLAN_id, 3200, 50); */
int32_t ifx_mps_queue_init(u8 mps_service_seq_num, uint32_t size, int32_t num_entries, int32_t isfreeq);

/* Returns Number of freed Nodes. -1 on error */
int32_t ifx_cpu0_release_q_list(MPS_BUFF_HEAD volatile *q, MPS_FREE_FN free_fn);

int32_t ifx_cpu0_release_q_lists(u8 service_seq_num);

int32_t ifx_cpu0_topup_q_list(MPS_BUFF_SIZE_ENTRY volatile *q_head, MPS_ALLOC_FN alloc_fn);

int32_t ifx_cpu0_topup_q_lists(u8 service_seq_num);

/* ] CPU 0 */

/*
 * Local support Functions - for now
 */

int32_t mps_queue_free(MPS_BUFF_HEAD volatile *q, int32_t free_circular_buf, MPS_FREE_FN free_fn);

int32_t	mps_queue_init(volatile MPS_BUFF_HEAD *q, uint32_t size, uint32_t num_entries, MPS_ALLOC_FN alloc_fn);


int32_t mps_buff_pool_free(u8 mps_service_seq_num);

int32_t mps_queue_tail(MPS_BUF volatile *buf, MPS_BUFF_HEAD volatile *q);

MPS_BUF volatile *mps_dequeue_head(MPS_BUFF_HEAD volatile *q);

MPS_BUFF_SIZE_ENTRY volatile *mps_get_buf_size_entry(MPS_BUFF_POOL_HEAD volatile *pool, uint32_t size);

int32_t mps_set_mps_buffer_identity(MPS_BUF volatile *buf, int32_t reset);

#endif	//_MPS_TPE_BUFFER_	//prochao+

