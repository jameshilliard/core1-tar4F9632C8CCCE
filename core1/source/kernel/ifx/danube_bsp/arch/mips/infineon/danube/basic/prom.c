/*
 * PROM interface routines.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/ioport.h>
#include <asm/bootinfo.h>
#include <asm/danube/danube.h>
#include <asm/danube/irq.h>
#include <asm/danube/emulation.h>
#include <asm/danube/model.h>
#include <asm/cpu.h>
#include <linux/version.h>

char arcs_cmdline[CL_SIZE];
//prochao+
//prochao+
#if 1
//pliu: for ramdisk
#ifndef CONFIG_TWINPASS_64_MEG
static char fake2cpu1_cmdline[73] = "root=/dev/ram rw console=ttyS1,115200 mem=12M@20M";
#else
static char fake2cpu1_cmdline[73] = "root=/dev/ram rw console=ttyS1,115200 mem=12M@52M";
#endif
#else
static char fake2cpu1_cmdline[73] = "rootfstype=squashfs root=/dev/mtdblock2 console=ttyS1,115200 mem=16M@16M";
#endif
//prochao-
#if	defined(CONFIG_DANUBE_CORE1)||defined(CONFIG_TWINPASS_E_CORE1)		//for the dual-core
static char fake_cmdline[73] = "rootfstype=squashfs root=/dev/mtdblock2 console=ttyS1,115200 mem=16M@16M";
#else	//original one
static char fake_cmdline[21] = "console=ttyS0,115200";
#endif
//prochao-
/* flag to indicate whether the user put mem= in the command line */
//prochao, adds one for the CPU1 entry address
static int got_mem = 0, got_cpu1_entry = 0;
#ifdef CONFIG_BLK_DEV_INITRD
extern unsigned long initrd_start, initrd_end;
#endif
#if	defined(CONFIG_MTD_DANUBE) || defined(CONFIG_MTD_TWINPASS)
extern unsigned long flash_start, flash_size;
#endif
u32* danube_cp1_base = 0;

#if defined(CONFIG_ATM_BONDING_BM) || defined(CONFIG_USE_EMULATOR)
#define  USE_BUILTIN_PARAMETER
#else
#undef  USE_BUILTIN_PARAMETER
#endif

#ifdef USE_BUILTIN_PARAMETER
static char * danube_arg[3] =
{
"<ignored>",
#ifdef CONFIG_USE_EMULATOR
#ifdef CONFIG_IFX_ASC_CONSOLE_ASC1
"root=/dev/ram rw ip=172.20.80.222:172.17.69.210::255.255.252.0::eth0:off ether=0,0,eth0 ethaddr=00:01:02:03:04:05 console=ttyS1,9600 panic=1 mem=8M",
#else /*CONFIG_IFX_ASC_CONSOLE_ASC1*/
"root=/dev/ram rw ip=172.20.80.222:172.17.69.210::255.255.252.0::eth0:off ether=0,0,eth0 ethaddr=00:01:02:03:04:05 console=ttyS0,9600 panic=1 mem=8M",
#endif /*CONFIG_IFX_ASC_CONSOLE_ASC1*/
#else
#error "define your root argument!"
#endif //CONFIG_USE_EMULATOR
NULL
};

static char * danube_env[] =
{
"flash_start=0x13000000",
"flash_size=0x1000000",
NULL
};
#endif //USE_BUILTIN_PARAMETER

//prochao+, using the beginning 8KB area reserved for CPU1 to pass the necessary info
// because following macro will be referenced by both CPU0/CPU1, it must be located at the starting of CPU1
#ifndef CONFIG_TWINPASS_64_MEG
#define	CPU1_BASE_ADDR	(KSEG1 | OFFSET_ABOVE_16MB | 0x01400000)	//OFFSET_ABOVE_16MB=0x01000000
#else
#define	CPU1_BASE_ADDR	(KSEG1 | OFFSET_ABOVE_16MB | 0x03400000)	//24 meg offset
#endif
typedef struct {
	//environment variables
	unsigned long	memsize;
	u32				*cpu1_entry;
	unsigned long	flash_start, flash_size;
	//arguments
	int		argc;
	char	args_cmdline[CL_SIZE];
} CPU1_Argument_T;

#if	defined(CONFIG_DANUBE) || defined(CONFIG_BOARD_TWINPASS_E)
CPU1_Argument_T	*pCPU1_arguments = CPU1_BASE_ADDR;
#endif
//prochao-,=======================================================

void __init prom_init(int argc, char **argv, char **envp, int *prom_vec)
{
	int i, len=0, left;
	unsigned long memsz;
#ifdef CONFIG_BLK_DEV_INITRD
	unsigned long rdstart, rdsize;
	int have_initrd = 0;
#endif
	char *dest, *scr;

	mips_machgroup = MACH_GROUP_DANUBE;

#ifdef USE_BUILTIN_PARAMETER
	argv = (char **)KSEG1ADDR((unsigned long)danube_arg);
	envp = (char **)KSEG1ADDR((unsigned long)danube_env);
	argc = 2;
#else
	argv = KSEG1ADDR((unsigned long)argv);
	envp = KSEG1ADDR((unsigned long)envp);
#endif

	/* Get the command line */
	if (argc>0) {
		left = CL_SIZE-1;
		dest = arcs_cmdline;
		/* BUG in u-boot, start at 1 */
		for (i = 1; i < argc; i++)
		{
			/* strlen does NOT include the terminating NUL */
			if (argv[i] == NULL) break;
			len = strlen(argv[i]);
			/*
			 * strncpy only copies up to and including the
			 * first NUL. end points to the next character
			 * after the NUL!
			 */
			strncpy(dest, argv[i], left);
			/* the user set mem= in the command line */
			if (strncmp(dest, "mem=", 4) == 0)
				got_mem = 1;
			//prochao, for the new cpu1_entry argument
			if (strncmp(dest, "cpu1_entry=", 11) == 0)
				got_cpu1_entry = 1;
			//
			left -= len + 1;
			/* NOTE: if the if hits then the last char is a NUL */
			if (left <= 0)
				break;
			/* no NULs in the cmdline please */
			dest[len] = ' ';
			dest += len + 1;
		}
		/* make sure the last char is a NUL */
		if (left > 0)
			dest[len] = '\0';
		if (i != argc)
			printk("promt_init: %s", "unable to copy the entire command line!\n");
	}
	else
	{
		/* provide a fake command line */
		strncpy(arcs_cmdline, fake_cmdline, 21);
		left = 0;
	}
	//prochao+, passing the assumed cmd_line to the CPU1
#if	defined(CONFIG_DANUBE) || defined(CONFIG_BOARD_TWINPASS_E)
	pCPU1_arguments->argc = strlen(fake2cpu1_cmdline);	//73;
	strncpy(pCPU1_arguments->args_cmdline, fake2cpu1_cmdline, pCPU1_arguments->argc);
	pCPU1_arguments->args_cmdline[ pCPU1_arguments->argc] = '\0';
#endif
	//prochao-,==========================
	memsz = 0;
#ifdef CONFIG_BLK_DEV_INITRD
	rdstart = rdsize = 0;
#endif
	/* now handle envp */
	if (envp != (char **)KSEG1ADDR(0))
	{
		/* assume for now that exactly 3 values get passed */
		i = 0;
		while (envp[i] != NULL)
		{
			/* check for memsize */
			if (strncmp(envp[i], "memsize=", 8) == 0)
			{
				scr = envp[i] + 8;
				memsz = (int)simple_strtoul(scr, NULL, 0);
#define DEBUG_PROM
#ifdef DEBUG_PROM
				printk("memsize=%ul\n", memsz);
#endif
				//prochao+, passing memsize to the CPU1
#if	defined(CONFIG_DANUBE) || defined(CONFIG_BOARD_TWINPASS_E)
				pCPU1_arguments->memsize = memsz;
#endif
				//prochao-,==========================
			}
			//prochao, ---------------------------
			/* check for cpu1_entry */
			if (strncmp(envp[i], "cpu1_entry=", 11) == 0)
			{	// and make sure it is in the uncache area (by xor'ing with 0xA0000000)
				scr = envp[i] + 11;
				danube_cp1_base = (u32 *) simple_strtoul(scr, NULL, 0);
#ifdef DEBUG_PROM
				printk("danube_cp1_base=%ul\n", danube_cp1_base);
#endif
				//prochao+, passing cpu1_entry to the CPU1
#if	defined(CONFIG_DANUBE) || defined(CONFIG_BOARD_TWINPASS_E)
				pCPU1_arguments->cpu1_entry = danube_cp1_base;
#endif
				//prochao-,==========================
			}
			//----------------
#ifdef CONFIG_BLK_DEV_INITRD
			/* check for initrd_start */
			if (strncmp(envp[i], "initrd_start=", 13) == 0)
			{
				scr = envp[i] + 13;
				rdstart = (int)simple_strtoul(scr, NULL, 0);
				rdstart = KSEG1ADDR(rdstart);
#ifdef DEBUG_PROM
printk("initrd_start=%#x\n", rdstart);
#endif
			}
			/* check for initrd_size */
			if (strncmp(envp[i], "initrd_size=", 12) == 0)
			{
				scr = envp[i] + 12;
				rdsize = (int)simple_strtoul(scr, NULL, 0);
#ifdef DEBUG_PROM
printk("initrd_size=%ul\n", rdsize);
#endif
			}
#endif /* CONFIG_BLK_DEV_INITRD */

//prochao+-
#if	defined(CONFIG_MTD_DANUBE) || defined(CONFIG_MTD_TWINPASS)
			/* check for flash address and size */
			if (strncmp(envp[i], "flash_start=", 12) == 0) {
				scr = envp[i] + 12;
				flash_start = simple_strtoul(scr, NULL, 0);
#ifdef DEBUG_PROM
printk("flash_start=%#x\n", flash_start);
#endif
				//prochao+, passing flash_start to the CPU1
#if	defined(CONFIG_DANUBE) || defined(CONFIG_BOARD_TWINPASS_E)
				pCPU1_arguments->flash_start = flash_start;
#endif
				//prochao-,==========================
			}
			if (strncmp(envp[i], "flash_size=", 11) == 0) {
				scr = envp[i] + 11;
				flash_size = simple_strtoul(scr, NULL, 0);
#ifdef DEBUG_PROM
printk("flash_size=%ul\n", flash_size);
#endif
				//prochao+, passing flash_size to the CPU1
#if	defined(CONFIG_DANUBE) || defined(CONFIG_BOARD_TWINPASS_E)
				pCPU1_arguments->flash_size = flash_size;
#endif
				//prochao-,==========================
			}
#endif	/* CONFIG_MTD_DANUBE || CONFIG_TWINPASS */
			i++;
		}
	}

#if (CONFIG_DANUBE_RAM_SIZE != 0)
    memsz=CONFIG_DANUBE_RAM_SIZE;
#else
// prochao, 7-24-2006, for dual-core support, the CPU0 uses only the 1st 16MB
#ifndef CONFIG_TWINPASS_64_MEG
	memsz = 20;		//16MB, the plus 1 is for the substraction at next line
#else
    memsz = 52;
#endif
#endif
    /* Store start of VCPU memory */
//    memsz -= 1;
	//prochao, --------------
	if (danube_cp1_base == 0)
		danube_cp1_base = (u32*)(0xA0000000 + (memsz * 1024 * 1024));
	//
#ifdef DEBUG_PROM
printk("Starting entry for CP1 @0x%08x\n", (u32)danube_cp1_base);
printk("memsize=%u\n", memsz);
#endif

	if (memsz)
	{
		if (got_mem == 0)
		{
			/* is there room for mem=XXXM ? */
			/* dest still points at the last char in cmdline */
			if (left > 8)
			{
				*dest++ = ' ';
				left--;
				strcat(dest, "mem=");
				dest += 4;
				left -= 4;
				i = snprintf(dest, left, "%d", (int)memsz);
				dest += i;
				/* memsize is always in megabytes - XXX */
				/* strcat adds the NUL at the end */
				strcat(dest, "M");
				got_mem = 1;

			}
		}
	}

#ifdef CONFIG_BLK_DEV_INITRD
	/* u-boot always passes a non-zero start, but a 0 size if there */
	/* is no ramdisk */
	if (rdstart != 0 && rdsize != 0)
	{
		initrd_start = rdstart;
		initrd_end = rdstart + rdsize;
	}
#endif
	/* Set the I/O base address */
	set_io_port_base(0);

	/* Set memory regions */
	ioport_resource.start = 0;		/* Should be KSEGx ???	*/
	ioport_resource.end = 0xffffffff;	/* Should be ???	*/

	/* 16MB starting at 0 */
	//prochao, if for the 2nd CPU (CPU1), should be the 16MB starting at 16MB
	if (!got_mem)
#ifndef CONFIG_TWINPASS_64_MEG
		add_memory_region(0x00000000, 0x2000000, BOOT_MEM_RAM);	//prochao+, 10/29/2006, cover all memory size (32MB)
#else
        add_memory_region(0x00000000, 0x4000000, BOOT_MEM_RAM);	//prochao+, 10/29/2006, cover all memory size (32MB)
#endif
//		add_memory_region(0x00000000, 0x1000000, BOOT_MEM_RAM);
}

void prom_free_prom_memory(void)
{
}


const char *get_system_type(void)
{
	return BOARD_SYSTEM_TYPE;
}

//----------------------<< This is ONLY for the CPU1 core >>-----------------------
//prochao+, for CPU1,
#if	defined(CONFIG_DANUBE)||defined(CONFIG_BOARD_TWINPASS_E)		//for dual-core

void __init prom1_init(int argc, char **argv, char **envp, int *prom_vec)
{
	int i;
	unsigned long memsz;

	mips_machgroup = MACH_GROUP_DANUBE;

#ifdef USE_BUILTIN_PARAMETER
	argv = (char **)KSEG1ADDR((unsigned long)danube_arg);
	envp = (char **)KSEG1ADDR((unsigned long)danube_env);
	argc = 2;
#else
	argv = KSEG1ADDR((unsigned long)argv);
	envp = KSEG1ADDR((unsigned long)envp);
#endif

	/* Get the command line */
	//prochao+, such command line is passed by the CPU0
#if	defined(CONFIG_DANUBE) || defined(CONFIG_BOARD_TWINPASS_E)
//	pCPU1_arguments->args_cmdline = fake_cmdline;
//pliu: for ramdisk
#if 1
	pCPU1_arguments->argc = strlen(fake2cpu1_cmdline);	//73;
	pCPU1_arguments->args_cmdline[ pCPU1_arguments->argc] = '\0';
	strncpy( arcs_cmdline, fake2cpu1_cmdline, pCPU1_arguments->argc);
#else
	strncpy( arcs_cmdline, pCPU1_arguments->args_cmdline, pCPU1_arguments->argc);	//this will less one char
#endif
	arcs_cmdline[ pCPU1_arguments->argc] = '\0';
#if 0
#if	defined(CONFIG_DANUBE_CORE1)||defined(CONFIG_TWINPASS_E_CORE1)		//for the dual-core
	strncpy(arcs_cmdline, fake_cmdline, 73);
#else
	strncpy(arcs_cmdline, fake_cmdline, 21);
#endif
#endif

#endif
	/* now handle envp */
// prochao, 7-24-2006, for dual-core support, the CPU0 uses only the 1st 16MB
#ifndef CONFIG_TWINPASS_64_MEG
	memsz = 20;		//16MB, the plus 1 is for the substraction at next line
#else
    memsz = 52;
#endif
#if	defined(CONFIG_DANUBE) || defined(CONFIG_BOARD_TWINPASS_E)
//	memsz = pCPU1_arguments->memsize;	//should be 16 (MB)
	danube_cp1_base = pCPU1_arguments->cpu1_entry;
#endif
//prochao+-
#if	defined(CONFIG_MTD_DANUBE) || defined(CONFIG_MTD_TWINPASS)
	flash_start = pCPU1_arguments->flash_start;
	flash_size = pCPU1_arguments->flash_size;
#endif
	//prochao-,======================================
	//
#ifdef DEBUG_PROM
printk("Starting entry for CPU1 @0x%08x\n", (u32) danube_cp1_base);
printk("memsize=%u\n", memsz);
#endif
	/* Set the I/O base address */
	set_io_port_base(0);		//keep as it is

	/* Set memory regions */
	ioport_resource.start = 0;		/* Should be KSEGx ???	*/
	ioport_resource.end = 0xffffffff;	/* Should be ???	*/

	/* 16MB starting at the 2nd 16MB */
	//prochao, if for the 2nd CPU (CPU1), should be the 16MB starting at 16MB
//	add_memory_region(OFFSET_ABOVE_16MB, 0x01000000, BOOT_MEM_RAM);	//prochao, revised
#ifndef CONFIG_TWINPASS_64_MEG
	add_memory_region(0x00000000, 0x2000000, BOOT_MEM_RAM);	//prochao+, 10/29/2006, cover all memory size (32MB)
#else
    add_memory_region(0x00000000, 0x4000000, BOOT_MEM_RAM);	//prochao+, 10/29/2006, cover all memory size (32MB)
#endif
}
#endif
//prochao-, -------------------------------------------------------------------------

