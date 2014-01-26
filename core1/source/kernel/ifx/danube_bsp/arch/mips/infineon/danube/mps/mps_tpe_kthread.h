#ifndef _MPS_KTHREAD_H_
#define _MPS_KTHREAD_H_

#include <linux/config.h>
#include <linux/version.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/tqueue.h>
#include <linux/wait.h>
#include <linux/spinlock.h>		//prochao+!

#include <asm/unistd.h>
#include <asm/semaphore.h>

/* a structure to store all information we need for our thread */
// got from searching the google!
typedef struct kthread_struct
{
	/* private data */
	/* Linux task structure of thread */
	struct task_struct	*thread;
	/* Task queue need to launch thread */
	struct tq_struct	tq;
	/* function to be started as thread */
	void	(*function)(struct kthread_struct *kthread);
	/* semaphore needed on start and creation of thread. */
	struct semaphore	startstop_sem;
	/* public data */

	/* queue thread is waiting on. Gets initialized by init_kthread, can be used by thread itself. */
	//wait_queue_head_t queue;
    /* flag to tell thread whether to die or not. When the thread receives a signal,
	   it must check the value of terminate and call exit_kthread and terminate if set. */
	int		terminate;
	/* additional data to pass to kernel thread */
	void	*arg;
} mps_kthread_t;

//---------------------------------------------------------
//prochao+
#define	NUM_MPS_DATAMSG_LIST		768
//macros
#define MPS_MSG_NEXT_Q_POS(pos)		((pos) < NUM_MPS_DATAMSG_LIST ? ((pos)+1) : 0)
#define MPS_MSG_IS_Q_FULL(queue)	((MPS_MSG_NEXT_Q_POS((queue).wrt)) == ((queue).rd))
#define MPS_MSG_IS_Q_EMPTY(queue)	((queue).rd == (queue).wrt)

//data structure
typedef struct {
	void			*pMsg;
	unsigned char	seq_num;
} MPS_DATAMSG_BUF_ENTITY_T;

typedef struct {
#if 0 /* [ XXX: Bug fix - Ritesh */
	MPS_DATAMSG_BUF_ENTITY_T	dataMsg[ NUM_MPS_DATAMSG_LIST];
#else /* ][ */
	MPS_DATAMSG_BUF_ENTITY_T	dataMsg[ NUM_MPS_DATAMSG_LIST+1];
#endif /* ] */
	unsigned int			rd;
	unsigned int			wrt;	//read & write index
	wait_queue_head_t		msg_wakeuplist;
	volatile int			msg_callback_flag;
	spinlock_t				mps_semaphore_spinlock;
	/* the variable that contains the thread data */
	mps_kthread_t				rcv_thread_data;
} MPS_RXDATA_LIST_T;

// Q
//MPS_RXDATA_LIST_T		mps_RxMsgQ;

/* prototypes */
//--------------------------------------------------------------------------------------
/* start new kthread (called by creator) */
void mpsStart_kthread(void (*func)(mps_kthread_t *), mps_kthread_t *kthread);

/* stop a running thread (called by "killer") */
void mpsStop_kthread(mps_kthread_t *kthread);

/* setup thread environment (called by new thread) */
void mpsInit_kthread(mps_kthread_t *kthread, char *name);

/* cleanup thread environment (called by thread upon receiving termination signal) */
void mpsExit_kthread(mps_kthread_t *kthread);

#endif

