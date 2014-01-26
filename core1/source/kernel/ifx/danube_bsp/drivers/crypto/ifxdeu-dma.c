
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>

#ifdef CONFIG_CRYPTO_DEV_DANUBE_DMA   //deudma

#include <asm/danube/danube_dma.h>
#include "ifxdeu-dma.h"

int deu_dma_intr_handler( struct dma_device_info* dma_dev, int status )
{
    // dummy function for dma driver
    return 0;
}


u8* deu_dma_buffer_alloc(int len, int* byte_offset,void** opt)
{
    // dummy function for dma driver
    return (u8 *)NULL;
}


int deu_dma_buffer_free(u8* dataptr,void* opt)
{
    // dummy function for dma driver
    return 0;
} 

#endif
