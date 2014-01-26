/*
 * Cryptographic API.
 *
 * Support for Infineon DEU hardware crypto engine.
 *
 * Copyright (c) 2005  Johannes Doering <info@com-style.de>, INFINEON
 *
 * Key expansion routine taken from crypto/aes.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ---------------------------------------------------------------------------
 * Copyright (c) 2002, Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
 * All rights reserved.
 *
 * LICENSE TERMS
 *
 * The free distribution and use of this software in both source and binary
 * form is allowed (with or without changes) provided that:
 *
 *   1. distributions of this source code include the above copyright
 *      notice, this list of conditions and the following disclaimer;
 *
 *   2. distributions in binary form include the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other associated materials;
 *
 *   3. the copyright holder's name is not used to endorse products
 *      built using this software without specific written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this product
 * may be distributed under the terms of the GNU General Public License (GPL),
 * in which case the provisions of the GPL apply INSTEAD OF those given above.
 *
 * DISCLAIMER
 *
 * This software is provided 'as is' with no explicit or implied warranties
 * in respect of its properties, including, but not limited to, correctness
 * and/or fitness for purpose.
 * ---------------------------------------------------------------------------
 * Change Log:
 * yclee 15 Jun 2006: tidy code; add local_irq_save() & local_irq_restore()
 * ---------------------------------------------------------------------------
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/crypto.h>
#include <linux/interrupt.h>
#include <asm/byteorder.h>
#include <linux/delay.h>

#define INCA_IP_AES_AES			(KSEG1 + 0x18000880)
#define AES_MIN_KEY_SIZE		16	/* in uint8_t units */
#define AES_MAX_KEY_SIZE		32	/* ditto */
#define AES_BLOCK_SIZE			16	/* ditto */
#define AES_EXTENDED_KEY_SIZE	64	/* in uint32_t units */
#define AES_EXTENDED_KEY_SIZE_B (AES_EXTENDED_KEY_SIZE * sizeof(uint32_t))
#define DEU_AESIR				INT_NUM_IM0_IRL28

//#define CRYPTO_DEBUG
#ifdef CONFIG_CRYPTO_DEV_INCAIP1_AES
#include <asm/incaip/inca-ip.h>
#include <asm/incaip/inca-ip-deu-structs.h>
#define AES_START INCA_IP_AES_AES
#endif
#ifdef CONFIG_CRYPTO_DEV_INCAIP2_AES
#include <asm/incaip2/incaip2-deu.h>
#include <asm/incaip2/incaip2-deu-structs.h>
#define AES_START AES_CON
#endif
#ifdef CONFIG_CRYPTO_DEV_DANUBE_AES
#include <asm/danube/danube.h>
#include <asm/danube/danube_deu.h>
#include <asm/danube/danube_deu_structs.h>
#define AES_START	DANUBE_AES_CON
#endif
#ifdef CONFIG_CRYPTO_DEV_DANUBE_DMA
#include "ifxdeu-dma.h"
#include <asm/danube/danube_dma.h>
#include <asm/danube/irq.h>
#endif
#ifdef CONFIG_CRYPTO_DEV_DANUBE_DMA


void swapu32(u32 *addri, u32 *addrj)
{
  /* addri ist eine Variable, die die Adresse einer (einen
  "Zeiger" auf eine) Speicherstelle für einen int-Wert
  enthält. *addri ist dann der Wert selbst */

  /* Hilfsvariable */
  u32 h;

  /* So wie man einer Variablen etwas zuweisen kann, kann
  man auch einer Speicherstelle, auf die ein Zeiger zeigt,
  etwas zuweisen. Also kann die Konstruktion " * Zeiger "
  auf der linken Seite der Zuweisung stehen */
	h = *addri;
	*addri = *addrj;
	*addrj = h;
}


u8 *aes_deu_dma_buffer_alloc(int len, int *byte_offset, void **opt)
{
	u8	*buffer = NULL;

	buffer = (u8 *) KSEG1ADDR(__get_free_page(GFP_DMA));
	memset(buffer, 0x55, len);
	*byte_offset = 0;
	return buffer;
}
#endif // CONFIG_CRYPTO_DEV_DANUBE_DMA
struct aes_ctx
{
	uint32_t	e_data[AES_EXTENDED_KEY_SIZE + 4];
	uint32_t	d_data[AES_EXTENDED_KEY_SIZE + 4];
	uint32_t	*E;
	uint32_t	*D;
	int			key_length;
};

extern int	disable_multiblock;



void deu_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	// empty function
}


static int aes_set_key(void *ctx_arg, const uint8_t *in_key, unsigned int key_len, uint32_t *flags)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	volatile struct aes_t	*aes = (struct aes_t *) AES_START;
	volatile register u32	tmp;
	unsigned long			flag;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	local_irq_save(flag);

	// start crypto engine with write to ILR
	aes->controlr.SM = 1;


//#ifndef CONFIG_CRYPTO_DEV_DANUBE_DMA
	aes->controlr.ARS = 1;
//#endif

	/* 128, 192 or 256 bit key length */
	aes->controlr.K = key_len / 8 - 2;

	if(key_len != 16 && key_len != 24 && key_len != 32)
	{
		*flags |= CRYPTO_TFM_RES_BAD_KEY_LEN;
		return -EINVAL;
	}

	if(key_len == 128 / 8)
	{
		aes->K3R = *((u32 *) in_key + 0);
		aes->K2R = *((u32 *) in_key + 1);
		aes->K1R = *((u32 *) in_key + 2);
		aes->K0R = *((u32 *) in_key + 3);
	}
	else if(key_len == 192 / 8)
	{
		aes->K5R = *((u32 *) in_key + 0);
		aes->K4R = *((u32 *) in_key + 1);
		aes->K3R = *((u32 *) in_key + 2);
		aes->K2R = *((u32 *) in_key + 3);
		aes->K1R = *((u32 *) in_key + 4);
		aes->K0R = *((u32 *) in_key + 5);
	}
	else if(key_len == 256 / 8)
	{
		aes->K7R = *((u32 *) in_key + 0);
		aes->K6R = *((u32 *) in_key + 1);
		aes->K5R = *((u32 *) in_key + 2);
		aes->K4R = *((u32 *) in_key + 3);
		aes->K3R = *((u32 *) in_key + 4);
		aes->K2R = *((u32 *) in_key + 5);
		aes->K1R = *((u32 *) in_key + 6);
		aes->K0R = *((u32 *) in_key + 7);
	}
	else
		return -EINVAL;

	/* let HW pre-process DEcryption key in any case (even if
	   ENcryption is used). Key Valid (KV) bit is then only
	   checked in decryption routine! */
	aes->controlr.PNK = 1;

	while(aes->controlr.BUS)
	{
		// this will not take long
	}

	if(!aes->controlr.KV) return -EINVAL;

#ifdef CONFIG_CRYPTO_DEV_DANUBE_DMA
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	struct dma_device_info		*dma_device;
	volatile struct deu_dma_t	*dma = (struct deu_dma_t *) DMA_CON;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	dma_device = dma_device_reserve("DEU");

	if(!dma_device) return -1;

	ifx_deu[0].dma_device = dma_device;

	dma->controlr.ALGO = 1; //AES
	dma->controlr.BS = 0;
	dma->controlr.EN = 1;
#endif // CONFIG_CRYPTO_DEV_DANUBE_DMA

	local_irq_restore(flag);

	return 0;
}


static void aes_ifxdeu(void *ctx_arg, uint8_t *out_arg, const uint8_t *in_arg,
			uint8_t *iv_arg, size_t nbytes, int encdec,
			int mode)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	volatile struct aes_t	*aes = (struct aes_t *) AES_START;
	volatile register u32	tmp;
	int						i = 0;
	int						nblocks = 0;
	int						size = 16;
	unsigned long			flag;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	local_irq_save(flag);

#ifdef CRYPTO_DEBUG
	printk("hardware crypto in aes_ifxdeu\n");
	printk("hardware is running\n");
	printk("encdec %x\n", encdec);
	printk("mode %x\n", mode);
	printk("nbytes %u\n", nbytes);
#endif
	aes->controlr.E_D = !encdec;				//encryption
	aes->controlr.O = mode - 1;					//0 ECB 1 CBC 2 OFB 3 CFB 4 CTR	hexdump(prin,sizeof(*des));

	//aes->controlr.F = 128; //default  maybe needs to be updated to a proper value calculated from nbytes
	if(mode > 1)
	{
		aes->IV3R = (*(u32 *) iv_arg);
		aes->IV2R = (*((u32 *) iv_arg + 1));
		aes->IV1R = (*((u32 *) iv_arg + 2));
		aes->IV0R = (*((u32 *) iv_arg + 3));
	};

#ifndef CONFIG_CRYPTO_DEV_DANUBE_DMA
	switch(nbytes)
	{
    	case 16:
    		aes->ID3R = *((u32 *) in_arg + 0);
    		aes->ID2R = *((u32 *) in_arg + 1);
    		aes->ID1R = *((u32 *) in_arg + 2);
    		aes->ID0R = *((u32 *) in_arg + 3);		/* start crypto */
    		break;

    	case 8:
    		aes->ID1R = *((u32 *) in_arg + 0);
    		aes->ID0R = *((u32 *) in_arg + 1);		/* start crypto */
    		break;

    	case 4:
    		aes->ID0R = *((u32 *) in_arg);			/* start crypto */
    		break;

    	case 2:
    		aes->ID0R = (u32) *((u16 *) in_arg);	/* start crypto */
    		break;

    	case 1:
    		aes->ID0R = (u32) *in_arg;				/* start crypto */
    		break;

    	default:
    		printk(KERN_ERR "size = %d\n", size);
    		;
	}

	while(aes->controlr.BUS)
	{
		// this will not take long
	}

	switch(size)
	{
    	case 16:
    		*((u32 *) out_arg + 0) = aes->OD3R;
    		*((u32 *) out_arg + 1) = aes->OD2R;
    		*((u32 *) out_arg + 2) = aes->OD1R;
    		*((u32 *) out_arg + 3) = aes->OD0R;
    		break;

    	case 8:
    		*((u32 *) out_arg + 0) = aes->OD1R;
    		*((u32 *) out_arg + 1) = aes->OD0R;
    		break;

    	case 4:
    		*((u32 *) out_arg + 0) = aes->OD0R;
    		break;

    	case 2:
    		*((u16 *) out_arg) = (u16) aes->OD0R;
    		break;

    	case 1:
    		*out_arg = (u8) aes->OD0R;
    		break;

    	default:
    		;
	}

	local_irq_restore(flag);

#else // dma

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	struct dma_device_info		*dma_device = ifx_deu[0].dma_device;
	volatile struct deu_dma_t	*dma = (struct deu_dma_t *) DMA_CON;
	u32							*out_dma = NULL;
	_ifx_deu_device				*pDev = ifx_deu;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	pDev->len = nbytes;
	pDev->packet_size = nbytes;
	pDev->src = in_arg;

	pDev->dst = out_arg;
	pDev->dst_count = 0;

	dma_device->num_rx_chan = 1;
	dma_device->num_tx_chan = 1;

	for(i = 0; i < dma_device->num_rx_chan; i++)
	{
		dma_device->rx_chan[i]->packet_size = pDev->packet_size;
		dma_device->rx_chan[i]->desc_len = 1;
		dma_device->rx_chan[i]->control = DANUBE_DMA_CH_ON;
		dma_device->rx_chan[i]->byte_offset = 0;
	}

	for(i = 0; i < dma_device->num_tx_chan; i++)
	{
		dma_device->tx_chan[i]->control = DANUBE_DMA_CH_ON;
	}

	dma_device->buffer_alloc = &aes_deu_dma_buffer_alloc;
	dma_device->buffer_free = &deu_dma_buffer_free;
	dma_device->intr_handler = &deu_dma_intr_handler;

	dma_device_register(dma_device);

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	int x = 0;
	u32 *out_dmaarr = out_arg;
	u32 *in_dmaarr = in_arg;
	u32 *dword_aligned_mem = NULL;
	u32 y = 0;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	for(i = 0; i < dma_device->max_rx_chan_num; i++)
	{
		if((dma_device->rx_chan[i])->control == DANUBE_DMA_CH_ON)
			(dma_device->rx_chan[i])->open(dma_device->rx_chan[i]);
	}

#if 0	// no need to execute this if ARS is set to 1
	if((nbytes % 16) == 0)
	{
		for(i = 0; i < (nbytes / 16); i++)
		{
			swapu32(in_dmaarr + (i * 4), in_dmaarr + ((i * 4) + 3));
			swapu32(in_dmaarr + ((i * 4) + 1), in_dmaarr + ((i * 4) + 2));
		}
	}
#endif // no need to execute this if ARS is set to 1

	u8	*incopy = kmalloc(nbytes, GFP_KERNEL);	// incopy is dword-aligned
	if(incopy == NULL)
		return;
	else
	{
		dword_aligned_mem = (u32 *) incopy;		// need to do u32-based copy
		memcpy(incopy, in_arg, nbytes);
	}

	//----------------------------------------------------------------------------------------------
	// somehow we need to use dword-aligned incopy instead of in_arg
	//----------------------------------------------------------------------------------------------
	int rlen = 0;
	rlen = dma_device_write(dma_device, (u8 *) incopy, nbytes, incopy);

	//	rlen=	dma_device_write(dma_device,(u8 *)in_arg,nbytes,in_arg);
	udelay(10);

	while(dma->controlr.BSY)
	{
		// this will not take long
	}

	while(aes->controlr.BUS);
	{
		// this will not take long
	}

	pDev->recv_count = dma_device_read(dma_device, &out_dma, NULL);
	udelay(10);

	while(dma->controlr.BSY)
	{
		// this will not take long
	}

	aes->controlr.SM = 0;						//needed for other modules
	local_irq_restore(flag);

	for(i = 0; i < (nbytes / 4); i++)
	{
		x = i ^ 0x3;

		dword_aligned_mem[i] = out_dma[x];
	}

	memcpy((u8 *) out_dmaarr, incopy, nbytes);	// note that incopy points to dword_aligned_mem
	if(incopy)
	{
		kfree(incopy);
	}

	if(out_dma)
	{
		free_page((u32) out_dma);
	}

	//on the end after read unregister again
	//dma_device=pDev->dma_device;
	if(dma_device) dma_device_release(dma_device);

	if(pDev->recv_count == pDev->len)
	{
		dma_device_unregister(pDev->dma_device);
		dma_device_release(pDev->dma_device);
	}
#endif // else dma
}


static void
aes_ifxdeu_ecb(void *ctx, uint8_t *dst, const uint8_t *src,
		uint8_t *iv, size_t nbytes, int encdec, int inplace)
{
	aes_ifxdeu(ctx, dst, src, NULL, nbytes, encdec,
		    CRYPTO_TFM_MODE_ECB);
}

static void
aes_ifxdeu_cbc(void *ctx, uint8_t *dst, const uint8_t *src, uint8_t *iv,
		size_t nbytes, int encdec, int inplace)
{
	aes_ifxdeu(ctx, dst, src, iv, nbytes, encdec,
		    CRYPTO_TFM_MODE_CBC);
}

static void
aes_ifxdeu_cfb(void *ctx, uint8_t *dst, const uint8_t *src, uint8_t *iv,
		size_t nbytes, int encdec, int inplace)
{
	aes_ifxdeu(ctx, dst, src, iv, nbytes, encdec,
		    CRYPTO_TFM_MODE_CFB);
}

static void
aes_ifxdeu_ofb(void *ctx, uint8_t *dst, const uint8_t *src, uint8_t *iv,
		size_t nbytes, int encdec, int inplace)
{
	aes_ifxdeu(ctx, dst, src, iv, nbytes, encdec,
		    CRYPTO_TFM_MODE_OFB);
}

static void
aes_encrypt(void *ctx_arg, uint8_t *out, const uint8_t *in)
{
	//printk("aes_encrypt\n");
	aes_ifxdeu(ctx_arg, out, in, NULL, AES_BLOCK_SIZE,
		    CRYPTO_DIR_ENCRYPT, CRYPTO_TFM_MODE_ECB);

}

static void
aes_decrypt(void *ctx_arg, uint8_t *out, const uint8_t *in)
{
	//printk("aes_decrypt\n");
	aes_ifxdeu (ctx_arg, out, in, NULL, AES_BLOCK_SIZE,
		     CRYPTO_DIR_DECRYPT, CRYPTO_TFM_MODE_ECB);
}

static struct crypto_alg aes_alg = {
	.cra_name		=	"aes",
	.cra_preference		=	CRYPTO_PREF_HARDWARE,
	.cra_flags		=	CRYPTO_ALG_TYPE_CIPHER,
	.cra_blocksize		=	AES_BLOCK_SIZE,
	.cra_ctxsize		=	sizeof(struct aes_ctx),
	.cra_module		=	THIS_MODULE,
	.cra_list		=	LIST_HEAD_INIT(aes_alg.cra_list),
	.cra_u			=	{
		.cipher = {
			.cia_min_keysize	=	AES_MIN_KEY_SIZE,
			.cia_max_keysize	=	AES_MAX_KEY_SIZE,
			.cia_setkey	   	= 	aes_set_key,
			.cia_encrypt	 	=	aes_encrypt,
			.cia_decrypt	  	=	aes_decrypt
		}
	}
};


int __init ifxdeu_init_aes(void)
{
   printk("ifxdeu initialize!\n"); 
	if (!disable_multiblock) {
		aes_alg.cra_u.cipher.cia_max_nbytes = (size_t)-1;
		aes_alg.cra_u.cipher.cia_req_align  = 16;
		aes_alg.cra_u.cipher.cia_ecb        = aes_ifxdeu_ecb;
		aes_alg.cra_u.cipher.cia_cbc        = aes_ifxdeu_cbc;
		aes_alg.cra_u.cipher.cia_cfb        = aes_ifxdeu_cfb;
		aes_alg.cra_u.cipher.cia_ofb        = aes_ifxdeu_ofb;
	}

	printk(KERN_NOTICE
		"Using Infineon DEU for AES algorithm%s.\n",
		disable_multiblock ? "" : " (multiblock)");

	//gen_tabs();
	return crypto_register_alg(&aes_alg);
}

void __exit ifxdeu_fini_aes(void)
{
	crypto_unregister_alg(&aes_alg);
}

