/************************************************************************
 * Cryptographic API.
 *
 * Support for Infineon hardware crypto engine (DEU).
 *
 * Copyright (c) 2005  Johannes Doering <info@com-style.de>, INFINEON
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 ************************************************************************/

/* Group definitions for Doxygen */
/** \addtogroup API API-Functions */
/** \addtogroup Internal Internally used functions */
 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/crypto.h>
#include <asm/byteorder.h>

#ifdef CONFIG_CRYPTO_DEV_INCAIP1
#include <asm/incaip/inca-ip.h>
#include <asm/incaip/inca-ip-deu-structs.h>
//#define CLC_START  INCA_IP_AES_AES
#endif
#ifdef CONFIG_CRYPTO_DEV_INCAIP2
//#include <asm/incaip2/incaip2_dma.h>
#include <asm/incaip2/incaip2.h>
#include <asm/incaip2/incaip2-deu.h>
#include <asm/incaip2/incaip2-deu-structs.h>
#define CLC_START DEU_CON
#endif
#ifdef CONFIG_CRYPTO_DEV_DANUBE
#include <asm/danube/danube.h>
#include <asm/danube/danube_pmu.h>
#include <asm/danube/danube_deu_structs.h>
#define CLC_START DANUBE_DEU_CLK
#endif

#ifdef CONFIG_CRYPTO_DEV_DANUBE_DMA
#include <asm/danube/danube_dma.h>
#include "ifxdeu-dma.h"
int disable_deudma = 0;
#else
int disable_deudma = 1;
#endif



/**
 * Initialization of all build in Infineon Hardware modules is done here
 * This module is linked together with all modules that hae been selected 
 * in the kernel config for ifx crytpo hw support
 * The initialization functions of all modules should be called from here
 *          
 * \return  0        	OK, all compiled in hardware algorithms have been initialized.
 * \return  -ENOSYS   If Infineon DEU was compiled without any algorithm.
 * \return  OTHER			The return vale of any initialization function of any hardware algorithm failed. 
 * \ingroup Internal  
 */

static int __init deu_init(void)
{
	int ret = -ENOSYS;
	
printk( "Infineon DEU initialization.\n");


#if defined(CONFIG_CRYPTO_DEV_INCAIP2)  | defined(CONFIG_CRYPTO_DEV_DANUBE)
volatile struct clc_controlr_t *clc = (struct clc_controlr_t*) CLC_START;
#ifdef CONFIG_CRYPTO_DEV_INCAIP2
	*INCA_IP2_PMS_PMS_GEN |=0x00040000;
#endif

#ifdef CONFIG_CRYPTO_DEV_DANUBE

//pmu_set((DANUBE_PMU_IOC_SET_DEU & 0xff), 1);

*DANUBE_PMU_PWDCR &= ((~(1 << 20)) );  //& 0x3fffff


#endif
	clc->FSOE= 0;
	clc->SBWE = 0;
	clc->SPEN = 0;
	clc->SBWE = 0;
	clc->DISS = 0;
	clc->DISR = 0;

#endif 


#ifdef CONFIG_CRYPTO_DEV_INCAIP1_DES

	if ((ret = ifxdeu_init_des())) {
		printk(KERN_ERR "Infineon IncaIP1 DES initialization failed.\n");
		return ret;
	}
#endif
#ifdef CONFIG_CRYPTO_DEV_INCAIP1_AES
	if ((ret = ifxdeu_init_aes())) {
		printk(KERN_ERR "Infineon IncaIP1 DES initialization failed.\n");
		return ret;
	}
	
#endif


#ifdef CONFIG_CRYPTO_DEV_INCAIP2_DES

	if ((ret = ifxdeu_init_des())) {
		printk(KERN_ERR "Infineon IncaIP1 DES initialization failed.\n");
		return ret;
	}
#endif
#ifdef CONFIG_CRYPTO_DEV_INCAIP2_AES
	if ((ret = ifxdeu_init_aes())) {
		printk(KERN_ERR "Infineon IncaIP1 DES initialization failed.\n");
		return ret;
	}
#endif

#ifdef CONFIG_CRYPTO_DEV_INCAIP2_SHA1

	if ((ret = ifxdeu_init_sha1())) {
		printk(KERN_ERR "Infineon IncaIP2 DES initialization failed.\n");
		return ret;
	}
#endif
#ifdef CONFIG_CRYPTO_DEV_INCAIP2_MD5
	if ((ret = ifxdeu_init_md5())) {
		printk(KERN_ERR "Infineon IncaIP2 DES initialization failed.\n");
		return ret;
	}
	
#endif


#ifdef CONFIG_CRYPTO_DEV_DANUBE_DES

	if ((ret = ifxdeu_init_des())) {
		printk(KERN_ERR "Infineon IncaIP1 DES initialization failed.\n");
		return ret;
	}
#endif
#ifdef CONFIG_CRYPTO_DEV_DANUBE_AES
	if ((ret = ifxdeu_init_aes())) {
		printk(KERN_ERR "Infineon IncaIP1 DES initialization failed.\n");
		return ret;
	}
#endif

#ifdef CONFIG_CRYPTO_DEV_DANUBE_SHA1

	if ((ret = ifxdeu_init_sha1())) {
		printk(KERN_ERR "Infineon IncaIP2 DES initialization failed.\n");
		return ret;
	}
#endif
#ifdef CONFIG_CRYPTO_DEV_DANUBE_MD5
	if ((ret = ifxdeu_init_md5())) {
		printk(KERN_ERR "Infineon IncaIP2 DES initialization failed.\n");
		return ret;
	}
	
#endif


	if (ret == -ENOSYS)
		printk(KERN_ERR "Infineon DEU was compiled without any algorithm.\n");

	return ret;
}

/**
 * Cleanup the DEU module.
 * Clean up all loaded algorithms that have been loaded for unloading.
 *
 * \ingroup Internal  
 */
static void __exit deu_fini(void)
{
#ifdef CONFIG_CRYPTO_DEV_INCAIP1_DES
	ifxdeu_fini_des();
#endif
#ifdef CONFIG_CRYPTO_DEV_INCAIP1_AES
	ifxdeu_fini_aes();
#endif
#ifdef CONFIG_CRYPTO_DEV_INCAIP2_DES
	ifxdeu_fini_des();
#endif
#ifdef CONFIG_CRYPTO_DEV_INCAIP2_AES
	ifxdeu_fini_aes();
#endif
#ifdef CONFIG_CRYPTO_DEV_INCAIP2_SHA1
	ifxdeu_fini_sha1();
#endif
#ifdef CONFIG_CRYPTO_DEV_INCAIP2_MD5
	ifxdeu_fini_md5();
#endif
}

int disable_multiblock = 0;
MODULE_PARM(disable_multiblock, "i");
MODULE_PARM_DESC(disable_multiblock,
		 "Disable encryption of whole multiblock buffers.");



void
hexdump(unsigned char *buf, unsigned int len)
{
	while (len--)
		printk("%02x", *buf++);

	printk("\n");
}

module_init(deu_init);
module_exit(deu_fini);

MODULE_DESCRIPTION("Infineon DEU crypto engine support.");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes Doering");
