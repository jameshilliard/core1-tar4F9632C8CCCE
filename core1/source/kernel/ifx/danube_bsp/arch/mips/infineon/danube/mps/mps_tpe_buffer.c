/************************************************************************
 *
 * Author - Ritesh Banerjee
 * Date   - 7th Nov, 2006
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

/*skbuff.h */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/cache.h>

#include <asm/atomic.h>
#include <asm/types.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/highmem.h>


/*skbuff.c */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/cache.h>
#include <linux/rtnetlink.h>
#include <linux/init.h>
#include <linux/highmem.h>

#include <net/protocol.h>
#include <net/dst.h>
#include <net/sock.h>
#include <net/checksum.h>

#include <asm/uaccess.h>
#include <asm/system.h>

/* mps_tpe_device.c */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/sem.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <asm/danube/ifx_types.h>
#include <asm/danube/danube.h>
//prochao+
#include <asm/danube/mps_tpe_buffer.h>
//prochao-
#include <asm/danube/mps_dualcore.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/danube/irq.h>
//prochao+
#include	"mps_tpe_device.h"
//prochao-
#if	(!defined(CONFIG_DANUBE_CORE1) && defined(MPS_USE_HOUSEKEEPING_TIMER))	//only for the CPU0
#include <asm/danube/mps_srv.h>
#endif

//prochao+, 11/27/2006, with Jeffrey's help
#define	IFX_MPS_SKBUFF_MAGIC0		37		//magic#
#define	IFX_MPS_SKBUFF_MAGIC1		38
//prochao-

#define MPS_CACHE_INVALIDATE
//#undef  MPS_CACHE_INVALIDATE

#define	_PRO_DEBUGGING_		1	//activate some debugging print msg
#undef	_PRO_DEBUGGING_
#define	_CON_DEBUGGING_		1
#undef	_CON_DEBUGGING_

/******* [ Start of Buffer and Q routines for MPS  - Ritesh  *************/

#ifdef	_PRO_DEBUGGING_
#define DEBUG_FNENTRY()		printk(KERN_INFO "(MPS) [%s] Entering function ...\n", __FUNCTION__)
#define DEBUG_FNEXIT()		printk(KERN_INFO "(MPS) [%s] Exiting function at line#[%d] ...\n", __FUNCTION__, __LINE__)
#define DPRINTK(fmt, args...)	printk(KERN_INFO "(MPS) "fmt, ##args)
#else
#define	DEBUG_FNENTRY()
#define	DEBUG_FNEXIT()
#ifdef	printk		//prochao
#define printk(fmt, args...)
#endif
#define DPRINTK(fmt, args...)
#endif

//prochao%, suggested by Ritesh, from 0 to 5, to improve CPU util
#define MPS_BOOK_KEEPING_THRESH		5	//	0

//prochao+
#ifdef	CONFIG_DANUBE_CORE1
volatile MPS_BUFF_POOL_HEAD	* volatile *mps_buff_pool_array;
#else
//prochao-
volatile MPS_BUFF_POOL_HEAD	* volatile x_mps_buff_pool_array[MPS_MAX_SELECTORS];
volatile MPS_BUFF_POOL_HEAD	* volatile *mps_buff_pool_array=x_mps_buff_pool_array;
#endif

#ifdef CONFIG_DANUBE_CORE1
#define MPS_GET_BUF_POOL(seq)		\
	(mps_buff_pool_array?  mps_buff_pool_array[(seq)]:NULL)
#else
#define MPS_GET_BUF_POOL(seq)		\
	mps_buff_pool_array[(seq)]

#endif


/* Allocation Stats */
uint32_t fast_allocs=0;
uint32_t fast_frees=0;

uint32_t fast_mallocs=0;
uint32_t fast_malloc_frees=0;


static atomic_t 	mps_list_book_keep_x = {1}; /* Exclusion for list book-keeping */

#ifndef CONFIG_DANUBE_CORE1
#if	1	//prochao+
static struct timer_list mps_timer; /* Timer for periodic checks */
#endif	//prochao-
#endif	

//#ifdef MPS_TIMER_OPTIMIZATION
int acquire_lock_nb(atomic_t *lock)
{
	if (atomic_dec_and_test(lock))
		return 1;
	else
	{
		atomic_inc(lock);
		return 0;
	}
}
void acquire_lock(atomic_t *lock)
{
	while (!atomic_dec_and_test(lock))
	{
		atomic_inc(lock);
		current->state = TASK_INTERRUPTIBLE;
		schedule();
	}
}

void release_lock(atomic_t *lock)
{
	atomic_inc(lock);
}

static LIST_HEAD(mps_buff_active_service_num_list);
static atomic_t mps_buff_list_lock = ATOMIC_INIT(1);
//#endif

void *mps_kmalloc(uint32_t size)
{
	return (kmalloc(size, GFP_ATOMIC|GFP_DMA)); /* GFP_KERNEL */
}

void *mps_inv_alloc(size_t size)
{
	void *p = kmalloc(size, GFP_KERNEL | GFP_DMA);
	dma_cache_inv(p, size);
	return _MU(p);
}

volatile MPS_BUFF_POOL_HEAD * volatile * ifx_mps_buff_pool_array_get(void)
{
//prochao+
#ifdef	CONFIG_DANUBE_CORE1
	return (mps_buff_pool_array);	//?? *mps_buff_pool_head
#else
	mps_buff_pool_array = _MU(mps_buff_pool_array);
	return (mps_buff_pool_array);
#endif
//prochao-
}

int32_t mps_queue_free(MPS_BUFF_HEAD volatile *q, int32_t free_circular_buf, MPS_FREE_FN free_fn)
{
	int rpos = 0, wpos = 0;
	int count = 0;

	if (q == NULL) {
#ifdef	_PRO_DEBUGGING_		//prochao+
		printk(KERN_DEBUG "%s(): NULL q pointer return !\n", __FUNCTION__);
#endif	//prochao-
		return 0;
	}
	wpos = q->wpos;
	rpos = q->rpos;
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO " in %s:%d, q[0x%x] q->rpos[%d],q->wpos[%d], q->maxcount[%d], #count[%d]\n",
			__FUNCTION__, __LINE__, (uint32_t)q, rpos,wpos,q->maxcount, count) ;
#endif
	if (q->circular_buf) {
		while (rpos != wpos) {
			struct sk_buff volatile * volatile skb;
			skb = q->circular_buf[rpos];
#ifdef	_PRO_DEBUGGING_
			printk(KERN_INFO "skb[0x%x] - list[0x%x], prev[0x%x], next[0x%x], destructor[0x%x]\n",
				skb, skb->list, skb->prev, skb->next, skb->destructor);
			printk(KERN_INFO "\t** len[%d], truesize[%d], head[0x%x], data[0x%x], tail[0x%x], end[0x%x]\n",
				skb->len, skb->truesize, skb->head, skb->data, skb->tail, skb->end);
#endif
			(*free_fn)((void *)q->circular_buf[rpos]);
			count++;
			rpos = MPS_NEXT_Q_POS(rpos, q->maxcount);
#ifdef	_PRO_DEBUGGING_		//prochao+
			printk(KERN_INFO " in %s:%d,q->rpos[%d],q->wpos[%d],count[%d]\n", __FUNCTION__, __LINE__,rpos,wpos,count) ;
#endif
		}
	}

	if (free_circular_buf) {
		kfree((void *)q->circular_buf);	//prochao+-, free() -> kfree()
	}

#if 0 /* [ XXX: Bug. Maxcount also reset ! - Ritesh */
	memset(q, 0x0, sizeof(*q));
#else
	q->rpos = rpos;
	wmb();
#endif

	return (count);
}


int32_t	mps_queue_init(volatile MPS_BUFF_HEAD *q, uint32_t size, uint32_t num_entries, MPS_ALLOC_FN alloc_fn)
{
	int i = 0;
	DEBUG_FNENTRY();

	if (q->circular_buf != NULL) {
		DPRINTK(" MPS Init Queue Error. Already init !\n");
		DEBUG_FNEXIT();
		return -EEXIST;
	}
	//q->circular_buf = _MU(kmalloc((num_entries + 1) * sizeof (MPS_BUF *), GFP_KERNEL|GFP_DMA));	//prochao+-
	q->circular_buf = mps_inv_alloc((num_entries + 1) * sizeof (MPS_BUF *));
	DPRINTK("q->circular_buf [0x%x] \n", (uint32_t)q->circular_buf);
	if (q->circular_buf == NULL) {
		DPRINTK(" Out of memory in %s\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return -ENOMEM;
	}
	if (size) {
		q->rpos = 0;
#if 0 /* XXX: Boundary bug */
		for (i=0; i <= num_entries; i++) 
#else
		for (i=0; i < num_entries; i++) 
#endif
		{
			struct sk_buff volatile *mps_buf;
			q->circular_buf[i] = (MPS_BUF *)(*alloc_fn)(size);
			if (q->circular_buf[i] == NULL) {
				DPRINTK(" Out of MPS_BUFs in %s\n", __FUNCTION__);
				q->maxcount=i;
				q->wpos = i;
				mps_queue_free(q, 1, (MPS_FREE_FN)dev_kfree_skb_any);
				DEBUG_FNEXIT();
				return -ENOMEM;

			}
			mps_buf = q->circular_buf[i];
#ifdef	_PRO_DEBUGGING_
			printk(KERN_INFO "mps_buf[0x%x] - list[0x%x], prev[0x%x], next[0x%x], destructor[0x%x]\n",
				mps_buf, mps_buf->list, mps_buf->prev, mps_buf->next, mps_buf->destructor);
			printk(KERN_INFO "\t** len[%d], truesize[%d], head[0x%x], data[0x%x], tail[0x%x], end[0x%x]\n",
				mps_buf->len, mps_buf->truesize, mps_buf->head, mps_buf->data, mps_buf->tail, mps_buf->end);
#endif
			MPS_FLUSH_MPS_BUF(q->circular_buf[i]);
		}
		q->wpos = num_entries;
	} else {
		memset((void *)q->circular_buf, 0x0, sizeof(MPS_BUF *) * (num_entries+1));
		q->wpos = 0;
		q->rpos = 0;
	}
	q->maxcount = num_entries;
	wmb();
	DPRINTK(" [%s] q [0x%x], q->rpos[%d], q->wpos[%d],q->maxcount[%d] \n", __FUNCTION__, (uint32_t) q, q->rpos, q->wpos,q->maxcount);
	DEBUG_FNEXIT();
	return (0);
}

#ifndef CONFIG_DANUBE_CORE1
#if	1	//prochao+
static void mps_timer_handler(unsigned long data)
{
#ifdef MPS_TIMER_OPTIMIZATION
	struct list_head *curr;
	MPS_BUFF_POOL_HEAD *mps_buff;
	unsigned int seq_no;

	if (acquire_lock_nb(&mps_buff_list_lock)) {
		if (!list_empty(&mps_buff_active_service_num_list))
			list_for_each(curr, &mps_buff_active_service_num_list) {
				mps_buff = list_entry(curr, MPS_BUFF_POOL_HEAD, list);
				seq_no = mps_buff->service_seq_num;
				ifx_cpu0_topup_q_lists(seq_no);
				ifx_cpu0_release_q_lists(seq_no);
			}
		release_lock(&mps_buff_list_lock);
	}
#else
        uint32_t i = 0;
        MPS_BUFF_POOL_HEAD* mps_buf_pool;
        for (i = 0; i < MPS_MAX_SELECTORS; i++) {
                mps_buf_pool = MPS_GET_BUF_POOL(i);     //prochao+-
                if (mps_buf_pool) {
                        ifx_cpu0_topup_q_lists(i);
                        ifx_cpu0_release_q_lists(i);
                }
        }
#if 0
	int32_t i = 0;
	/* Check if list book_keeping to be called  */
	/* XXX: Ritesh. This is way too expensive and lazy. Need to make some criteria to only check relevant q-lists */

#if 0
	for (i=0; i < MPS_MAX_SELECTORS; i++) {
		ifx_cpu0_topup_q_lists(i);
		ifx_cpu0_release_q_lists(i);
	}
#else
		ifx_cpu0_topup_q_lists(MPS_SRV_DS_DATA_ID);		//10
		ifx_cpu0_release_q_lists(MPS_SRV_DS_DATA_ID);	//10
		ifx_cpu0_topup_q_lists(MPS_SRV_US_DATA_ID);		//20
		ifx_cpu0_release_q_lists(MPS_SRV_US_DATA_ID);	//20

#endif
#endif
#endif

	mod_timer(&mps_timer, jiffies + 2);
}
#endif	//prochao-
#endif

int32_t ifx_mps_malloc_buff_pool_init(u8 mps_service_seq_num, uint32_t alloc_size, uint32_t num_alloc_nodes, uint32_t num_free_nodes)
{
	int 		cpu_id;
	MPS_BUFF_POOL_HEAD volatile	*mps_buf_pool;

	DEBUG_FNENTRY();
	DPRINTK(" MPS Sequence # [%d] in  [%s]\n", mps_service_seq_num, __FUNCTION__);
	cpu_id = ifx_mpsdrv_get_cpu_id();

	if (cpu_id != MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() not called from CPU 0 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return -EINVAL;
	}

	mps_buf_pool = MPS_GET_BUF_POOL(mps_service_seq_num);	//prochao+-

	if (mps_buf_pool == NULL) {
		DPRINTK(" Could not get MPS Buffer Pool for service seq# [%d]\n", mps_service_seq_num);
		DEBUG_FNEXIT();
		return -ENOMEM;
	}
	DPRINTK(" In [%s] mps_buf_pool [0x%x]\n", __FUNCTION__, (uint32_t) mps_buf_pool);
	mps_queue_init(mps_buf_pool->malloc_buf_free_list, 0, num_free_nodes, NULL);
	mps_queue_init(mps_buf_pool->malloc_buf_alloc_list, alloc_size, num_alloc_nodes, (MPS_ALLOC_FN)mps_kmalloc);

	return 0;
}

int32_t ifx_mps_buff_pool_init(u8 mps_service_seq_num, u8 sizes, uint32_t def_freeq_len)
{
	int	cpu_id;
	static int timer_started = 0;
#ifndef CONFIG_DANUBE_CORE1
	MPS_BUFF_POOL_HEAD volatile *mps_buf_pool;
	MPS_BUFF_POOL_HEAD volatile * volatile *pp_mps_buf_pool;
#endif

	DEBUG_FNENTRY();

	cpu_id = ifx_mpsdrv_get_cpu_id();

	if (cpu_id != MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() not called from CPU 0 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return -EINVAL;
	}
	mps_buff_pool_array = _MU(mps_buff_pool_array); /* XXX: only required once, but doesnt really matter */
#ifndef CONFIG_DANUBE_CORE1 /* [ */
	DPRINTK(" MPS Sequence # [%d]\n", mps_service_seq_num);
	pp_mps_buf_pool = &MPS_GET_BUF_POOL(mps_service_seq_num);
	if (*pp_mps_buf_pool != NULL) {
		DPRINTK(" MPS Buff Pool Init Error. Already init !\n");
		DEBUG_FNEXIT();
		return -EEXIST;
	}
	//*pp_mps_buf_pool = _MU(kmalloc(sizeof(*mps_buf_pool), GFP_KERNEL|GFP_DMA));	//prochao+-
	*pp_mps_buf_pool = mps_inv_alloc(sizeof(*mps_buf_pool));
	mps_buf_pool = *pp_mps_buf_pool;
	memset((void *)mps_buf_pool, 0x0, sizeof(*mps_buf_pool));
	DPRINTK(" mps_buf_pool [0x%x] \n", (uint32_t)mps_buf_pool);
	mps_buf_pool->sizes_count = sizes;
	//mps_buf_pool->default_free_list = _MU(kmalloc(sizeof(*(mps_buf_pool->default_free_list)), GFP_KERNEL|GFP_DMA));	//prochao+-
	mps_buf_pool->default_free_list = mps_inv_alloc(sizeof(*(mps_buf_pool->default_free_list)));
	DPRINTK(" mps_buf_pool->default_free_list [0x%x] \n", (uint32_t)mps_buf_pool->default_free_list);
	memset((void *)mps_buf_pool->default_free_list, 0x0, sizeof(*mps_buf_pool->default_free_list));
	mps_queue_init(mps_buf_pool->default_free_list, 0, def_freeq_len, (MPS_ALLOC_FN)NULL);
	//mps_buf_pool->buff_array = _MU(kmalloc(sizeof(*(mps_buf_pool->buff_array)) * sizes, GFP_KERNEL|GFP_DMA));	//prochao+-
	mps_buf_pool->buff_array = mps_inv_alloc(sizeof(*(mps_buf_pool->buff_array)) * sizes);
	DPRINTK(" mps_buf_pool->buff_array [0x%x] \n", (uint32_t)mps_buf_pool->buff_array);
	/* Init MPS_BUFF_SIZE_ENTRY */
	memset((void *)mps_buf_pool->buff_array, 0x0, sizeof(*(mps_buf_pool->buff_array)) * sizes);

#endif

#ifndef CONFIG_DANUBE_CORE1
#if	1	//prochao+
	if (!timer_started) {
		timer_started = 1;
	init_timer(&mps_timer);
	mps_timer.function = mps_timer_handler;
		mps_timer.data = 0;
	mps_timer.expires = jiffies + HZ;
	add_timer(&mps_timer);
	}
#endif	//prochao-
#endif	
	wmb();
	DEBUG_FNEXIT();
	return 0;
}

int32_t ifx_mps_queue_init(u8 mps_service_seq_num, uint32_t size, int num_entries, int32_t isfreeq)
{
	int 		cpu_id;
	MPS_BUFF_HEAD	volatile *q;
	MPS_BUFF_POOL_HEAD volatile	*mps_buf_pool;
	MPS_BUFF_SIZE_ENTRY volatile	*mps_buf_size_entry;

	DEBUG_FNENTRY();
	DPRINTK(" MPS Sequence # [%d] in  [%s]\n", mps_service_seq_num, __FUNCTION__);
	cpu_id = ifx_mpsdrv_get_cpu_id();

	if (cpu_id != MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() not called from CPU 0 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return -EINVAL;
	}

	mps_buf_pool = MPS_GET_BUF_POOL(mps_service_seq_num);	//prochao+-

	if (mps_buf_pool == NULL) {
		DPRINTK(" Could not get MPS Buffer Pool for service seq# [%d]\n", mps_service_seq_num);
		DEBUG_FNEXIT();
		return -ENOMEM;
	}
	DPRINTK(" In [%s] mps_buf_pool [0x%x]\n", __FUNCTION__, (uint32_t)mps_buf_pool);
	mps_buf_size_entry = mps_get_buf_size_entry(mps_buf_pool, size);
	
	DPRINTK(" In [%s] mps_buf_size_entry [0x%x]\n", __FUNCTION__, (uint32_t)mps_buf_size_entry);

#ifdef MPS_TIMER_OPTIMIZATION
	mps_buf_pool->service_seq_num = mps_service_seq_num;
	acquire_lock(&mps_buff_list_lock);
	DPRINTK(" Lock acquired for service seq# [%d]\n", mps_service_seq_num);
	INIT_LIST_HEAD(&mps_buf_pool->list);
	list_add_tail(&(mps_buf_pool->list), &mps_buff_active_service_num_list);
	release_lock(&mps_buff_list_lock);
#if 0 //Non blocking lock
	if (acquire_lock_nb(&mps_buff_list_lock)) {
		DPRINTK(" Lock acquired for service seq# [%d]\n", mps_service_seq_num);
		INIT_LIST_HEAD(&mps_buf_pool->list);
		list_add_tail(&(mps_buf_pool->list), &mps_buff_active_service_num_list);
		release_lock(&mps_buff_list_lock);
	}
	else {
		DPRINTK(" Lock failed for service seq# [%d]\n", mps_service_seq_num);
		return -EAGAIN;
	}
#endif
#endif

	if (!isfreeq) {
		q = mps_buf_size_entry->alloc_q;
		if (!q) {
			//q = _MU(kmalloc(sizeof(*q), GFP_KERNEL|GFP_DMA));
			q = mps_inv_alloc(sizeof(*q));
			if (!q) {
				DPRINTK(" Q head allocation failed !\n");
				DEBUG_FNEXIT();
				return -1;
			}
			memset((void *)q, 0x0, sizeof(*q));
			DPRINTK(" In [%s] alloc_q [0x%x]\n", __FUNCTION__, (uint32_t)q);
			mps_buf_size_entry->alloc_q = q;
		}
	} else {
		q = mps_buf_size_entry->free_q;
		if (!q) {
			//q = _MU(kmalloc(sizeof(*q), GFP_KERNEL|GFP_DMA));
			q = mps_inv_alloc(sizeof(*q));
			if (!q) {
				DPRINTK(" Q head allocation failed !\n");
				DEBUG_FNEXIT();
				return -1;
			}
			memset((void *)q, 0x0, sizeof(*q));
			DPRINTK(" In [%s] alloc_q [0x%x]\n", __FUNCTION__, (uint32_t)q);
			mps_buf_size_entry->free_q = q;
		}
	}
	wmb();
	DEBUG_FNEXIT();
	return (mps_queue_init(q, size, num_entries, (MPS_ALLOC_FN)dev_alloc_skb));
}

int32_t mps_buff_pool_free(u8 mps_service_seq_num)
{
	MPS_BUFF_POOL_HEAD volatile *mps_buf_pool;
	DEBUG_FNENTRY();

	mps_buf_pool = MPS_GET_BUF_POOL(mps_service_seq_num);
	if (mps_buf_pool != NULL) {
		DPRINTK(" MPS Buff Pool Init Error. Already init !\n");
		return -EEXIST;
	}
	/* XXX: Write the free routines. Unlikely they will be called. Will be
	 * required if built as a module and rmmod is to be done - Ritesh */

	DEBUG_FNEXIT();
	return (0);

}

int32_t mps_queue_tail(MPS_BUF volatile *buf, MPS_BUFF_HEAD volatile *q)
{
	unsigned long	flags;	//prochao+

	DPRINTK(" [%s] queueing to tail of q [0x%x],wpos[%d],rpos[%d]\n",
		__FUNCTION__, (uint32_t) q, q->wpos,q->rpos);
	if (!q) {
		DPRINTK(" Queue does not exist in %s\n", __FUNCTION__);
		return -EINVAL;
	}

	if (MPS_IS_Q_FULL(q)) {
		DPRINTK(" Out of queue space in %s\n", __FUNCTION__);
		return -ENOMEM;
	}
//prochao+
	local_irq_save(flags);
//prochao-
	MPS_SET_Q_ITEM(q, buf);
	MPS_FLUSH_MPS_BUF(buf);
	wmb();
	q->wpos = MPS_NEXT_Q_POS(q->wpos, q->maxcount);
	wmb();
//prochao+
	local_irq_restore(flags);
//prochao-

	return 0;
}

MPS_BUF volatile *mps_dequeue_head(MPS_BUFF_HEAD volatile *q)
{
	MPS_BUF  volatile *m_buf = NULL;
	unsigned long	flags;
	DEBUG_FNENTRY();

	DPRINTK(" [%s] q[0x%x], maxcount[%d], rpos[%d], wpos[%d]\n", __FUNCTION__, (uint32_t)q,
		q->maxcount, q->rpos, q->wpos);
	if (MPS_IS_Q_EMPTY(q)) {
#ifdef	_PRO_DEBUGGING_		//prochao+
		printk(KERN_INFO " Q Empty in (%s) - q[0x%x], q->circular_buf[0x%x]\n",__FUNCTION__, (uint32_t)q,(uint32_t)(q?q->circular_buf:NULL));
#endif
		DEBUG_FNEXIT();
		return NULL;
	}

//prochao+
	local_irq_save(flags);
//prochao-
	DPRINTK(" Going for MPS_GET_Q_ITEM() ...\n");
	m_buf = MPS_GET_Q_ITEM(q);
	/* XXX: */
	q->circular_buf[q->rpos] = 0xEEEEEEEE; /* Invalid entry */
	wmb();
	DPRINTK(" Got mps_buf [0x%x] for MPS_GET_Q_ITEM() ...\n", m_buf);
	barrier();		//prochao+-
	DPRINTK(" [%s] q[0x%x], maxcount[%d], rpos[%d], wpos[%d]\n", __FUNCTION__, (uint32_t)q,
		q->maxcount, q->rpos, q->wpos);
	q->rpos = MPS_NEXT_Q_POS(q->rpos, q->maxcount);	//prochao+-
	wmb();
	DPRINTK(" [%s] q[0x%x], maxcount[%d], rpos[%d], wpos[%d]\n", __FUNCTION__, (uint32_t)q,
		q->maxcount, q->rpos, q->wpos);
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "mps_buf[0x%x] - list[0x%x], prev[0x%x], next[0x%x], destructor[0x%x]\n",
		m_buf, m_buf->list, m_buf->prev, m_buf->next, m_buf->destructor);
	printk(KERN_INFO "\t** len[%d], truesize[%d], head[0x%x], data[0x%x], tail[0x%x], end[0x%x]\n",
		m_buf->len, m_buf->truesize, m_buf->head, m_buf->data, m_buf->tail, m_buf->end);
#endif
//prochao+
	local_irq_restore(flags);
//prochao-

	DEBUG_FNEXIT();
	return (m_buf);
}

MPS_BUFF_SIZE_ENTRY  volatile *mps_get_buf_size_entry(MPS_BUFF_POOL_HEAD volatile *pool, uint32_t size)
{
	/* XXX: Size based allocation logic later, For now, fixed size at index 0
	 * -- Ritesh, 7Nov, 2006 */
	if (size && (pool->buff_array[0].size == 0)) {
		pool->buff_array[0].size = size; /* XXX: Bug, missed out setting this parameter :-( */
#ifdef	_PRO_DEBUGGING_		//prochao+
		printk(KERN_INFO "[%s] Setting pool size to [%d]\n", __FUNCTION__, size);
#endif
		wmb();
	}

	return (&(pool->buff_array[0]));
}

MPS_BUF volatile * ifx_mps_fast_alloc_mps_buf(uint32_t size, int priority, u8 service_seq_num)
{
	int 			cpu_id;
	MPS_BUF			volatile *mps_buf;
	MPS_BUFF_HEAD		volatile * volatile q;
	MPS_BUFF_SIZE_ENTRY	volatile * volatile mps_buf_size_entry;	//prochao+-
	MPS_BUFF_POOL_HEAD	volatile * volatile mps_buf_pool;

	DEBUG_FNENTRY();
	DPRINTK(" [%s] for Sequence Id [%d]\n", __FUNCTION__, service_seq_num);

	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id == MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() called from CPU 0 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return NULL;
	}

	mps_buf_pool = MPS_GET_BUF_POOL(service_seq_num);

	if (mps_buf_pool == NULL) {
		DPRINTK(" Could not get MPS Buffer Pool for service seq# [%d]\n", service_seq_num);
		DPRINTK(" mps_buff_pool_array[%x] mps_buff_pool_array[%d %x] \n", mps_buff_pool_array, service_seq_num, mps_buff_pool_array[service_seq_num]);
		DEBUG_FNEXIT();
		return NULL;
	}

	DPRINTK(" MPS Buffer Pool for service seq# [%d] mps_buf_pool[%x]\n", service_seq_num, mps_buf_pool);
	mps_buf_size_entry = mps_get_buf_size_entry(mps_buf_pool, size);
	if (mps_buf_size_entry == NULL) {
		DPRINTK(" Could not get MPS Buffer Size Head for service"
			"seq# [%d], size[%d]\n", service_seq_num, size);
		DEBUG_FNEXIT();
		return NULL;

	}
	q = mps_buf_size_entry->alloc_q;
#ifdef	_PRO_DEBUGGING_
	printk(KERN_INFO "mps_buf_size_entry [0x%x] !\n", (uint32_t)mps_buf_size_entry);
	printk(KERN_INFO "alloc_q [0x%x] !\n", (uint32_t)q);
#endif
	mps_buf = mps_dequeue_head(q);
	if(mps_buf == NULL)
	{
#ifdef	_CON_DEBUGGING_		//prochao+
		DPRINTK(" Could not get MPS buffer, seq# [%d], size[%d]\n", service_seq_num, size);
		DEBUG_FNEXIT();
#else
		printk(KERN_DEBUG " %s_%d: Could not get MPS buffer, seq# [%d]\n", __FUNCTION__, __LINE__, service_seq_num);
#endif
		return NULL;
	}
#if	0
	dump_q_info(q, 1);
#endif
   /* Initialize the mps buffer. Currently not required since buffers are not recirculated */
#if 0
	mps_buf->truesize = size + sizeof(struct sk_buff);
//	mps_buf->data = mps_buf->head;
	mps_buf->head = mps_buf->data;			//prochao+-, just follow the original way of skbuff
  	mps_buf->tail = mps_buf->head;
	mps_buf->end  = mps_buf->data + size;
	/* Set up other state */
	mps_buf->len = 0;
	mps_buf->cloned = 0;
	mps_buf->data_len = 0;

	atomic_set(&mps_buf->users, 1); 
	atomic_set(&(skb_shinfo(mps_buf)->dataref), 1);
	skb_shinfo(mps_buf)->nr_frags = 0;
	skb_shinfo(mps_buf)->frag_list = NULL;
#endif

	/* Invalidate before use */
#ifdef MPS_CACHE_INVALIDATE
	dma_cache_inv(mps_buf, sizeof(*mps_buf));
#ifdef	_PRO_DEBUGGING_		//prochao+
	printk(KERN_INFO "mps_buf[0x%x] - list[0x%x], prev[0x%x], next[0x%x], destructor[0x%x]\n",
		mps_buf, mps_buf->list, mps_buf->prev, mps_buf->next, mps_buf->destructor);
	printk(KERN_INFO "\t** len[%d], truesize[%d], head[0x%x], data[0x%x], tail[0x%x], end[0x%x]\n",
		mps_buf->len, mps_buf->truesize, mps_buf->head, mps_buf->data, mps_buf->tail, mps_buf->end);
#endif
	dma_cache_inv(mps_buf->head, mps_buf->end - mps_buf->head);

	DPRINTK("(XXX) Invalidated skbuf->head !! \n");
#endif
#if	0	//prochao+, 12/13/2006, uses new way to identify the MPS buffer ownership without calling to set it
	mps_set_mps_buffer_identity(mps_buf, 0);
#endif	//prochao-, 12/13/2006
	fast_allocs++;
	DPRINTK(" Fast Allocs [%u]; Fast Frees [%u]\n", fast_allocs, fast_frees);

	wmb();
	DEBUG_FNEXIT();
	return (mps_buf);
}

int32_t ifx_mps_fast_free_mps_buf(MPS_BUF volatile *buf, u8 service_seq_num)
{
	int 		cpu_id;
	MPS_BUFF_HEAD	volatile *q = NULL;
	MPS_BUFF_POOL_HEAD	volatile *mps_buf_pool;
	MPS_BUFF_SIZE_ENTRY	volatile *mps_buf_size_entry;

	DEBUG_FNENTRY();
	DPRINTK(" [%s] for Sequence Id [%d]\n", __FUNCTION__, service_seq_num);
	if (buf == NULL) {
		DEBUG_FNEXIT();
		return -EINVAL;
	}

	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id == MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() called from CPU 0 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return -1;
	}
#if 1	//prochao%, 12/13/2006, /* XXX: Ritesh */
	/* Check if MPS buffer, else call normal kfree_skb_xxx() adjusting for context */
	if (ifx_mps_is_mps_buffer(buf) == 0) {
		dev_kfree_skb_any(buf);
		DEBUG_FNEXIT();
		return 0;
	}
#endif

	mps_buf_pool = MPS_GET_BUF_POOL(service_seq_num);
	if (mps_buf_pool == NULL) {
		DPRINTK(" Could not get MPS Buffer Pool for service seq# [%d]\n", service_seq_num);
		DEBUG_FNEXIT();
		return -ENOMEM;
	}

#if 0
	mps_buf_size_entry = mps_get_buf_size_entry(mps_buf_pool, 0);	//size);	//prochao+- ??????
#else
	mps_buf_size_entry = mps_get_buf_size_entry(mps_buf_pool, MPS_BUFF_SIZE(buf));
#endif

	if (mps_buf_size_entry) {
		q = mps_buf_size_entry->free_q;
	}
	if (q == NULL) {
		/* Use the default free queue for the pool */
		q = mps_buf_pool->default_free_list;
	}

	if (mps_queue_tail(buf, q) < 0) {
		DEBUG_FNEXIT();
		return -1;
	}
	fast_frees++;
	wmb();
	DPRINTK(" Fast Allocs [%u]; Fast Frees [%u]\n", fast_allocs, fast_frees);
	DEBUG_FNEXIT();
	return 0;
}

void volatile * ifx_mps_fast_alloc_malloc_buf(uint32_t size, int priority, u8 service_seq_num)
{
	int 		cpu_id;
	MPS_BUF		volatile *mps_buf;
	MPS_BUFF_HEAD	volatile *q;
	MPS_BUFF_POOL_HEAD	volatile *mps_buf_pool;

	DEBUG_FNENTRY();
	DPRINTK(" [%s] for Sequence Id [%d]\n", __FUNCTION__, service_seq_num);

	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id == MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() called from CPU 0 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return NULL;
	}

	mps_buf_pool = MPS_GET_BUF_POOL(service_seq_num);
	if (mps_buf_pool == NULL) {
#ifdef	_CON_DEBUGGING_		//prochao+
		DPRINTK(" Could not get MALLOC Buffer Pool for service seq# [%d]\n", service_seq_num);
		DEBUG_FNEXIT();
#else
		printk(KERN_DEBUG "%s_%d: Could not get MALLOC Buffer Pool for service seq# [%d]\n", __FUNCTION__, __LINE__, service_seq_num);
#endif
		return NULL;
	}

#if 0 /* [ Check if we want this later for normal kmalloc bufs also
	   If different size malloc bins are required */
	mps_buf_size_entry = mps_get_buf_size_entry(mps_buf_pool, size);
	if (mps_buf_size_entry == NULL) {
		DPRINTK(" Could not get MPS Buffer Size Head for service"
			"seq# [%d], size[%d]\n", service_seq_num, size);
		DEBUG_FNEXIT();
		return NULL;
	}
	printk(KERN_INFO "mps_buf_size_entry [0x%x] !\n", (uint32_t)mps_buf_size_entry);
#endif /* ] */
	q = mps_buf_pool->malloc_buf_alloc_list;
#ifdef	_PRO_DEBUGGING_		//prochao+
	printk(KERN_INFO "malloc_buf_alloc_list [0x%x] !\n", (uint32_t)q);
#endif
	mps_buf = (void *)mps_dequeue_head(q);
	fast_mallocs++;
	DPRINTK(" Fast Allocs [%u]; Fast Frees [%u]\n", fast_mallocs, fast_malloc_frees);
	DEBUG_FNEXIT();
	return (mps_buf);
}

int32_t ifx_mps_fast_free_malloc_buf(void volatile *buf, u8 service_seq_num)
{
	int 		cpu_id;
	MPS_BUFF_HEAD	volatile *q = NULL;
	MPS_BUFF_POOL_HEAD	volatile *mps_buf_pool;

	DEBUG_FNENTRY();
	DPRINTK(" [%s] for Sequence Id [%d]\n", __FUNCTION__, service_seq_num);
	if (buf == NULL) {
		DEBUG_FNEXIT();
		return -EINVAL;
	}

	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id == MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() called from CPU 0 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return -1;
	}
#if 0 /* For a normal malloc buffer, there is no marking */
	/* Check if MPS buffer, else call normal kfree_skb() */
	if (ifx_mps_is_mps_buffer(buf) == 0) {
		dev_kfree_skb_any(buf);
		DEBUG_FNEXIT();
		return 0;
	}
#endif /* ] */

	mps_buf_pool = MPS_GET_BUF_POOL(service_seq_num);
	if (mps_buf_pool == NULL) {
		DPRINTK(" Could not get MPS Buffer Pool for service seq# [%d]\n"
				, service_seq_num);
		DEBUG_FNEXIT();
		return -ENOMEM;
	}


#if 0 /* [ Required if we later support different size bins for malloc pools */
	mps_buf_size_entry = mps_get_buf_size_entry(mps_buf_pool, MPS_BUFF_SIZE(buf));
	if (mps_buf_size_entry) {
		q = mps_buf_size_entry->free_q;
	}
#endif
	if (q == NULL) {
		/* Use the default free queue for the pool */
		q = mps_buf_pool->malloc_buf_free_list;
	}

	if (mps_queue_tail(buf, q) < 0) {
		DEBUG_FNEXIT();
		return -1;
	}
	fast_malloc_frees++;

	DPRINTK(" Fast Allocs [%u]; Fast Frees [%u]\n", fast_allocs, fast_frees);
	DEBUG_FNEXIT();
	return 0;
}

/* Returns Number of freed Nodes. -1 on error */
int32_t ifx_cpu0_release_q_list(MPS_BUFF_HEAD volatile *q, MPS_FREE_FN free_fn)
{
//	int 		cpu_id;

#ifdef	_CON_DEBUGGING_		//prochao+
	DEBUG_FNENTRY();
#endif
#if 0	//prochao+, already checked by callee
	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id != MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() NOT called from CPU 0 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return -1;
	}
#endif	//prochao-

	if (q == NULL) {
		DEBUG_FNEXIT();
		return 0; /* XXX: Or -EINVAL ? */
	}
#ifdef	_CON_DEBUGGING_		//prochao+
	DEBUG_FNEXIT();
#endif
	return (mps_queue_free(q, 0, free_fn));
}

int32_t ifx_cpu0_release_q_lists(u8 service_seq_num)
{
	int 		i, cpu_id;
	int		count;
	uint32_t  	tot_count=0;
	MPS_BUFF_HEAD	volatile *q;
	MPS_BUFF_POOL_HEAD	volatile *mps_buf_pool;
	MPS_BUFF_SIZE_ENTRY	volatile *mps_buf_size_entry;

#ifdef	_CON_DEBUGGING_		//prochao+
	DEBUG_FNENTRY();
	DPRINTK(" Release bufs for MPS Seq # [%d]\n", service_seq_num);
#endif
	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id != MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() called from CPU 1 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return -1;
	}
	mps_buf_pool = MPS_GET_BUF_POOL(service_seq_num);
	if (mps_buf_pool == NULL) {
#ifdef	_CON_DEBUGGING_		//prochao+
		DPRINTK(" Could not get MPS Buffer Pool for service seq# [%d]\n", service_seq_num);
		DEBUG_FNEXIT();
#endif
		return -ENOMEM;
	}
	mps_buf_pool->free_hit_count ++;
	if (mps_buf_pool->free_hit_count > MPS_BOOK_KEEPING_THRESH) {
		mps_buf_pool->free_hit_count = 0;
		if (atomic_dec_and_test((atomic_t *)&mps_list_book_keep_x) == 0) {
			goto lbl_ret_ok; 
		}
	} else {
#ifdef	_CON_DEBUGGING_		//prochao+
		DEBUG_FNEXIT();
#endif
		return 0; /* Skip w/o doing anything */
	}

	for (i=0; i < mps_buf_pool->sizes_count; i++) {
		mps_buf_size_entry = &(mps_buf_pool->buff_array[i]);
		q = mps_buf_size_entry->free_q;
		count = ifx_cpu0_release_q_list(q, (MPS_FREE_FN)dev_kfree_skb_any);
		if (count > 0) {
			tot_count += count;
		}
	}
	count = ifx_cpu0_release_q_list(mps_buf_pool->default_free_list, (MPS_FREE_FN)dev_kfree_skb_any);
	if (count > 0) {
		tot_count += count;
	}
	mps_buf_pool->tot_free_count += tot_count;
	DPRINTK(" Total nodes freed from free list of MPS Seq#[%d] = [%d]\n", service_seq_num, 
			mps_buf_pool->tot_free_count);
	count = ifx_cpu0_release_q_list(mps_buf_pool->malloc_buf_free_list, (MPS_FREE_FN)kfree);

	wmb();
lbl_ret_ok:
	atomic_inc(&mps_list_book_keep_x);
#ifdef	_CON_DEBUGGING_		//prochao+
	DEBUG_FNEXIT();
#endif
	return (tot_count);
}

#if 0	//prochao+, 12/04/2006, beautifying
void dump_q_info(MPS_BUFF_HEAD volatile *q, int dump_q_items)
{
	MPS_BUFF_HEAD volatile * volatile ptr;
	MPS_BUF volatile * volatile * volatile pp_c;
	int i, max;
	ptr = q;
	rmb();

	printk(KERN_INFO "q [0x%x:0x%x] ", ptr, q);
	printk(KERN_INFO "q->rpos [%d:%d], q->wpos [%d:%d], q->maxcount[%d:%d]\n", ptr->rpos, q->rpos, ptr->wpos, q->wpos,  ptr->maxcount, q->maxcount);
	if (dump_q_items) {
		pp_c = q->circular_buf;
		rmb();
		if (ptr->maxcount < 10) {
			max = ptr->maxcount;
		} else {
			max = 10;
		}
		for (i=0; i <= max; i++) {
			printk(KERN_INFO "------> q[%d]:%c = [0x%x:0x%x]\n", i, (i == ptr->wpos)?'E':'V', pp_c[i], q->circular_buf[i]);
		}
	}
}
#endif	//prochao-

int32_t mps_topup_q_list(MPS_BUFF_HEAD volatile *q, uint32_t size, MPS_ALLOC_FN alloc_fn)
{
	int		count, org_count;
	int		rpos, wpos;
	MPS_BUF		volatile *mps_buf;
	static uint32_t 	topup_count=0;


	count = MPS_GET_Q_COUNT(q, rpos, wpos);
	org_count = count;
#ifdef	_PRO_DEBUGGING_		//prochao+
	printk(KERN_INFO " in %s:%d,q->rpos[%d],q->wpos[%d],count[%d]\n", __FUNCTION__, __LINE__,rpos,wpos,count) ;
#endif
	while (count < q->maxcount) {
		mps_buf = (MPS_BUF volatile *)(*alloc_fn)(size);
		if (!mps_buf) {
			DPRINTK(" Out of memory while topping up MPS Buf Q!\n");
			q->wpos = wpos;
			break;
		}
#ifdef	_PRO_DEBUGGING_		//prochao+
		printk(KERN_INFO "mps_buf[0x%x] - list[0x%x], prev[0x%x], next[0x%x], destructor[0x%x]\n",
			mps_buf, mps_buf->list, mps_buf->prev, mps_buf->next, mps_buf->destructor);
		printk(KERN_INFO "\t** len[%d], truesize[%d], head[0x%x], data[0x%x], tail[0x%x], end[0x%x]\n",
			mps_buf->len, mps_buf->truesize, mps_buf->head, mps_buf->data, mps_buf->tail, mps_buf->end);
#endif
		count++;

		MPS_SET_Q_ITEM(q, mps_buf);
		wpos = MPS_NEXT_Q_POS(wpos, q->maxcount);	//prochao+-
		q->wpos = wpos;
		MPS_FLUSH_MPS_BUF(mps_buf);
		wmb();
	}
	/* q->wpos = wpos; */
	wmb();
	topup_count += count - org_count;
#ifdef	_PRO_DEBUGGING_		//prochao+
	printk(KERN_INFO "XXXXXX: Total Q Allocations : [%d]\n", topup_count);
#endif

	return (count - org_count);
}

int32_t ifx_cpu0_topup_q_list(MPS_BUFF_SIZE_ENTRY volatile *q_head, MPS_ALLOC_FN alloc_fn)
{
//	int 		cpu_id;
	int		count;

#ifdef	_CON_DEBUGGING_		//prochao+
	DEBUG_FNENTRY();
#endif
#if 0	//prochao+, already checked by callee
	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id != MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() NOT called from CPU 0 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return -1;
	}
#endif	//prochao-
	if ( !q_head || q_head->alloc_q->maxcount == 0) {
		/* Q empty, not initialized */
		DEBUG_FNEXIT();
		return 0;
	}

	count = mps_topup_q_list(q_head->alloc_q, q_head->size, alloc_fn);
#ifdef	_CON_DEBUGGING_		//prochao+
	DEBUG_FNEXIT();
#endif
	return (count);
}


int32_t ifx_cpu0_topup_q_lists(u8 service_seq_num)
{
	int 		i, cpu_id;
	int			count, tot_count=0;
	MPS_BUFF_POOL_HEAD	volatile *mps_buf_pool;
	MPS_BUFF_SIZE_ENTRY	volatile *mps_buf_size_entry;

#ifdef	_CON_DEBUGGING_		//prochao+
	DEBUG_FNENTRY();
	DPRINTK(" topup for MPS Seq # [%d]\n", service_seq_num);
#endif
	cpu_id = ifx_mpsdrv_get_cpu_id();

	if (cpu_id != MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() called from CPU 1 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return -1;
	}

	mps_buf_pool = MPS_GET_BUF_POOL(service_seq_num);
	if (mps_buf_pool == NULL) {
		DPRINTK(" Could not get MPS Buffer Pool for service seq# [%d]\n",
				service_seq_num);
		DEBUG_FNEXIT();
		return -ENOMEM;
	}

	mps_buf_pool->alloc_hit_count ++;
	if (mps_buf_pool->alloc_hit_count > MPS_BOOK_KEEPING_THRESH) {
		mps_buf_pool->alloc_hit_count = 0;
		if (atomic_dec_and_test(&mps_list_book_keep_x) == 0) {
			goto lbl_ret_ok; 
		}
	} else {
#ifdef	_CON_DEBUGGING_		//prochao+
		DEBUG_FNEXIT();
#endif
		return 0; /* Skip w/o doing anything */
	}

	for (i=0; i < mps_buf_pool->sizes_count; i++) {
		mps_buf_size_entry = &(mps_buf_pool->buff_array[i]);
		count = ifx_cpu0_topup_q_list(mps_buf_size_entry, (MPS_ALLOC_FN)dev_alloc_skb);
		if (count > 0) {
			tot_count += count;
		}
	}
	wmb();
lbl_ret_ok:
#ifdef	_PRO_DEBUGGING_		//prochao+
	printk(KERN_INFO "Topup for Seq#[%d]. Count = [%d]\n", service_seq_num, tot_count);
#endif
	atomic_inc(&mps_list_book_keep_x);
	DEBUG_FNEXIT();
	return (tot_count);
}

typedef void (*SKB_DESTRUCTOR)(MPS_BUF *);
int32_t mps_identity_destructor(MPS_BUF *buf)
{
	/* This is a dummy function to identify MPS buffers on CPU1 */

	return (ifx_mpsdrv_get_cpu_id()); /* Just a dummy call */
}


int32_t ifx_mps_is_mps_buffer(MPS_BUF  volatile *buf)
{
#if	0	//prochao+, already checked before calling this function
	int cpu_id;
	DEBUG_FNENTRY();

	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id == MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() called from CPU 0 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return 0;
	}
#endif	//prochao-
	DPRINTK(" [%s] buf[0x%x],  destructor [0x%x], mps_destructor[0x%x]\n",
		__FUNCTION__, (uint32_t)buf, (uint32_t)buf->destructor, (uint32_t)mps_identity_destructor);
#if	1	//prochao+, 12/13/2006, uses new way to identify the MPS buffer ownership without calling to set it
	u32	phyaddr;

	phyaddr = PHYSADDR(buf);
	if (phyaddr < CPU1_RAM_PHYADDR_BEGIN)
	{
		DPRINTK(" buf[0x%x] an MPS Buffer !\n", (uint32_t)buf);
		DEBUG_FNEXIT();
		return 1;	// this is an MPS buffer from the CPU0
	}
#else
	//Jeffrey if (buf->destructor == (SKB_DESTRUCTOR)mps_identity_destructor) {
	if (buf->cb[IFX_MPS_SKBUFF_MAGIC0] == (u8)mps_identity_destructor &&
	    buf->cb[IFX_MPS_SKBUFF_MAGIC1] == (u8) (((u32)mps_identity_destructor)>>8) ) {
		DPRINTK(" buf[0x%x] an MPS Buffer !\n", (uint32_t)buf);
		DEBUG_FNEXIT();
		return 1;
	}
#endif
	DPRINTK(" buf[0x%x] NOT an MPS Buffer !!!\n", (uint32_t)buf);
	DEBUG_FNEXIT();

	return 0;	//not an MPS buffer from the CPU0
}

int32_t mps_set_mps_buffer_identity(MPS_BUF volatile *buf, int32_t reset)
{
#if	1	//prochao+, 12/13/2006, uses new way to identify the MPS buffer ownership without calling to set it
	return 0;	//just return
#else
	if (buf) {
		if (reset) {
			if (buf->cb[IFX_MPS_SKBUFF_MAGIC0] == (u8)(mps_identity_destructor) &&
		            buf->cb[IFX_MPS_SKBUFF_MAGIC1] == (u8)(((u32)mps_identity_destructor)>>8)){
				buf->cb[IFX_MPS_SKBUFF_MAGIC0] = MPS_CPU_0_ID;
				buf->cb[IFX_MPS_SKBUFF_MAGIC1] = MPS_CPU_0_ID;
//				MPS_CACHE_FLUSH(&buf->destructor, sizeof(*(buf->destructor)));
				DPRINTK(" Reset buf[0x%x] destructor\n", (uint32_t)buf);
			}
#if 1
			else {
				DPRINTK(" Not Reset buf[0x%x] destructor\n", (uint32_t)buf);
			}
#endif
		}
		else {
			buf->cb[IFX_MPS_SKBUFF_MAGIC0] = (u8) (mps_identity_destructor);
			buf->cb[IFX_MPS_SKBUFF_MAGIC1] = (u8) (((u32)mps_identity_destructor) >>8);
			/* Ensure cacheable skbuff header is flushed to memory */
			/* XXX: Not really required if only this CPU is  accessing this skbuff */
			DPRINTK(" Set buf[0x%x:0x%x] destructor [0x%x]\n", (uint32_t)buf,
			(uint32_t)&buf->destructor, (uint32_t)buf->destructor);
		}
#if 0 /* XXX: bug - can't do sizeof(*fnptr); must  do sizeof(fnptr) */
		MPS_CACHE_FLUSH(&buf->destructor, sizeof(*(buf->destructor)));
#else
#ifdef MPS_CACHE_INVALIDATE
		MPS_CACHE_FLUSH(&buf->cb[IFX_MPS_SKBUFF_MAGIC0], 2*sizeof(buf->cb[IFX_MPS_SKBUFF_MAGIC0]));
//		MPS_CACHE_FLUSH(&buf->cb[IFX_MPS_SKBUFF_MAGIC1], sizeof(buf->cb[IFX_MPS_SKBUFF_MAGIC1]));
#endif
#endif
	}
	return 0;
#endif
}

int32_t ifx_mps_set_mps_buffer_identity(MPS_BUF volatile *buf, int reset)
{
	int cpu_id;

	DEBUG_FNENTRY();
	cpu_id = ifx_mpsdrv_get_cpu_id();
	if (cpu_id == MPS_CPU_0_ID) {
		DPRINTK(" Error. %s() called from CPU 0 !\n", __FUNCTION__);
		DEBUG_FNEXIT();
		return -1;
	}
#if	0	//prochao+, 12/13/2006, uses new way to identify the MPS buffer ownership without calling to set it
	mps_set_mps_buffer_identity(buf, reset);
	wmb();
#endif
	DEBUG_FNEXIT();
	return 0;
}



/******* ] End of Buffer and Q routines for MPS  - Ritesh  *************/

#ifndef	CONFIG_DANUBE_CORE1
// prochao+, 11/09/2006,
EXPORT_SYMBOL(ifx_mps_buff_pool_init);
EXPORT_SYMBOL(ifx_mps_queue_init);
EXPORT_SYMBOL(ifx_cpu0_topup_q_lists);
EXPORT_SYMBOL(ifx_cpu0_release_q_lists);
#else
EXPORT_SYMBOL(ifx_mps_fast_free_mps_buf);
EXPORT_SYMBOL(ifx_mps_fast_alloc_mps_buf);
EXPORT_SYMBOL(ifx_mps_is_mps_buffer);
#endif

