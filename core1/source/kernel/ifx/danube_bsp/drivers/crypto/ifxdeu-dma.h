#include <linux/init.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/crypto.h>
#include <asm/scatterlist.h>
#include <asm/byteorder.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#ifdef CONFIG_CRYPTO_DEV_DANUBE_DMA
#include <asm/danube/danube_dma.h>
#endif

typedef struct ifx_deu_device{
    struct dma_device_info* dma_device;
    u8* dst;
    u8* src;
    int len;
    int dst_count;
    int src_count;
    int recv_count;
    int packet_size;
    int packet_num;
    wait_queue_t wait;
}_ifx_deu_device;

static _ifx_deu_device ifx_deu[1];




int deu_dma_intr_handler( struct dma_device_info* , int  );
u8* deu_dma_buffer_alloc(int , int* ,void** );
int deu_dma_buffer_free(u8* ,void* );


	
