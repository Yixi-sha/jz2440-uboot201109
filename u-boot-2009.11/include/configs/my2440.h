/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 * Gary Jennejohn <garyj@denx.de>
 * David Mueller <d.mueller@elsoft.ch>
 *
 * Configuation settings for the SAMSUNG SMDK2410 board.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __CONFIG_H
#define __CONFIG_H

/*
 * High Level Configuration Options
 * (easy to change)
 */
#define CONFIG_ARM920T		1	/* This is an ARM920T Core	*/
#define	CONFIG_S3C2440		1	/* in a SAMSUNG S3C2410 SoC     */
#define CONFIG_MY2440		1	/* on a SAMSUNG SMDK2410 Board  */

/*
 * For debug
 */
#define CONFIG_MY2440_LED	1
#define CONFIG_ASM_DEBUG	1


/* input clock of PLL */
#define CONFIG_SYS_CLK_FREQ	12000000/* the SMDK2410 has 12MHz input clock */


#define USE_920T_MMU		1
#undef CONFIG_USE_IRQ			/* we don't need IRQ/FIQ stuff */

/*
 * Size of malloc() pool
 */
#define CONFIG_SYS_MALLOC_LEN		(CONFIG_ENV_SIZE + 1024*1024)
#define CONFIG_SYS_GBL_DATA_SIZE	128	/* size in bytes reserved for initial data */

/*
 * Hardware drivers
 */
#define CONFIG_NET_MULTI
#define CONFIG_NET_RETRY_COUNT		20
#define CONFIG_DRIVER_DM9000		1
#define CONFIG_DM9000_BASE		    0x20000000
#define DM9000_IO 			        CONFIG_DM9000_BASE
#define DM9000_DATA 			    (CONFIG_DM9000_BASE+4)
#define CONFIG_DM9000_USE_16BIT		1
#define CONFIG_DM9000_NO_SROM		1

/*
 * select serial console configuration
 */
#define CONFIG_S3C24X0_SERIAL
#define CONFIG_SERIAL1          1	/* we use SERIAL 1 on SMDK2410 */

/************************************************************
 * RTC
 ************************************************************/
#define	CONFIG_RTC_S3C24X0	1

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE

#define CONFIG_BAUDRATE		115200


/*
 * BOOTP options
 */
#define CONFIG_BOOTP_BOOTFILESIZE
#define CONFIG_BOOTP_BOOTPATH
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME


/*
 * Command line configuration.
 */
#include <config_cmd_default.h>

#define CONFIG_CMD_CACHE
#define CONFIG_CMD_DATE
#define CONFIG_CMD_ELF
#define CONFIG_CMD_PING
#define CONFIG_CMD_NAND
#define CONFIG_CMD_YAFFS
#define CONFIG_YAFFS_SKIPFB

#define CONFIG_CMD_MTDPARTS    /* MTD分区命令支持 */  
#define CONFIG_CMD_UBI         /* UBI命令支持 */
//#define CONFIG_CMD_UBIFS       /* UBIFS命令支持 */

#define CONFIG_MTD_DEVICE      /* MTD设备支持 */
#define CONFIG_MTD_PARTITIONS  /* MTD分区支持 */
#define CONFIG_LZO             /* UBIFS要求支持LZO压缩格式 */
#define CONFIG_RBTREE          /* UBIFS需要支持红黑树相关操作 */

#define MTDIDS_DEFAULT   "nand0=NAND"  /* u-boot设备id同内核MTD设备id的映射关系 */
#define MTDPARTS_DEFAULT "mtdparts=NAND:512k(u-boot),"\
                         "512k(param),"\
                         "5m(kernel),"\
                         "-(file-system)"








/* support SD card */
#define CONFIG_CMD_MMC
#define CONFIG_MMC 
#define CONFIG_MMC_S3C
#define CONFIG_MMC_WIDE   /* 4bit的数据线宽度 */
#define CONFIG_CMD_FAT
#define CONFIG_DOS_PARTITION
/* tag list */
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG
#define CONFIG_CMDLINE_TAG

#define CONFIG_BOOTARGS	"root=/dev/mtdblock3  console=ttySAC0,115200"
#define CONFIG_ETHADDR	    08:90:00:A0:90:90
#define CONFIG_NETMASK      255.255.255.0
#define CONFIG_IPADDR		192.168.0.111
#define CONFIG_SERVERIP		192.168.0.103
#define CONFIG_GATEWAYIP	192.168.1.1

#define CONFIG_BOOTDELAY	4
#define CONFIG_BOOTFILE	    "uImage" 
#define CONFIG_BOOTCOMMAND	"nand read 30008000 kernel \;bootm" 

#if defined(CONFIG_CMD_KGDB)
#define CONFIG_KGDB_BAUDRATE	115200		/* speed to run kgdb serial port */
/* what's this ? it's not used anywhere */
#define CONFIG_KGDB_SER_INDEX	1		/* which serial port to use */
#endif

/*
 * Miscellaneous configurable options
 */
#define	CONFIG_SYS_LONGHELP				/* undef to save memory		*/
#define	CONFIG_SYS_PROMPT		"yixi-sha# "	/* Monitor Command Prompt	*/
#define	CONFIG_SYS_CBSIZE		256		/* Console I/O Buffer Size	*/
#define	CONFIG_SYS_PBSIZE (CONFIG_SYS_CBSIZE+sizeof(CONFIG_SYS_PROMPT)+16) /* Print Buffer Size */
#define	CONFIG_SYS_MAXARGS		16		/* max number of command args	*/
#define CONFIG_SYS_BARGSIZE		CONFIG_SYS_CBSIZE	/* Boot Argument Buffer Size	*/
#define CONFIG_CMDLINE_EDITING     /* 使命令行支持通过上下键翻阅命令历史 */
#define CONFIG_AUTO_COMPLETE       /* 使命令行支持自动补齐 */

#define CONFIG_SYS_MEMTEST_START	0x30000000	/* memtest works on	*/
#define CONFIG_SYS_MEMTEST_END		0x33F00000	/* 63 MB in DRAM	*/

#define	CONFIG_SYS_LOAD_ADDR		0x30008000	/* default load address	*/

#define	CONFIG_SYS_HZ			1000

/* valid baudrates */
#define CONFIG_SYS_BAUDRATE_TABLE	{ 9600, 19200, 38400, 57600, 115200 }

/*-----------------------------------------------------------------------
 * Stack sizes
 *
 * The stack sizes are set up in start.S using the settings below
 */
#define CONFIG_STACKSIZE	(128*1024)	/* regular stack */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(4*1024)	/* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(4*1024)	/* FIQ stack */
#endif

/*-----------------------------------------------------------------------
 * Physical Memory Map
 */
#define CONFIG_NR_DRAM_BANKS	1	   /* we have 1 bank of DRAM */
#define PHYS_SDRAM_1		0x30000000 /* SDRAM Bank #1 */
#define PHYS_SDRAM_1_SIZE	0x04000000 /* 64 MB */

#define PHYS_FLASH_1		0x00000000 /* Flash Bank #1 */

#define CONFIG_SYS_FLASH_BASE		PHYS_FLASH_1

/*-----------------------------------------------------------------------
 * FLASH and environment organization
 */

#if 0
#define CONFIG_AMD_LV400	1	/* uncomment this if you have a LV400 flash */
#endif
#define CONFIG_AMD_LV800	1	/* uncomment this if you have a LV800 flash */



#define CONFIG_SYS_MAX_FLASH_BANKS	1	/* max number of memory banks */
#ifdef CONFIG_AMD_LV800
#define PHYS_FLASH_SIZE		0x00200000 /* 1MB */
#define CONFIG_SYS_MAX_FLASH_SECT	(35)	/* max number of sectors on one chip */
#define CONFIG_ENV_ADDR		(CONFIG_SYS_FLASH_BASE + 0x0F0000) /* addr of environment */
#endif
#ifdef CONFIG_AMD_LV400
#define PHYS_FLASH_SIZE		0x00080000 /* 512KB */
#define CONFIG_SYS_MAX_FLASH_SECT	(11)	/* max number of sectors on one chip */
#define CONFIG_ENV_ADDR		(CONFIG_SYS_FLASH_BASE + 0x070000) /* addr of environment */
#endif

/* timeout values are in ticks */
#define CONFIG_SYS_FLASH_ERASE_TOUT	(5*CONFIG_SYS_HZ) /* Timeout for Flash Erase */
#define CONFIG_SYS_FLASH_WRITE_TOUT	(5*CONFIG_SYS_HZ) /* Timeout for Flash Write */

#define	CONFIG_ENV_IS_IN_NAND	1
#define CONFIG_ENV_OFFSET		0x80000	
#define CONFIG_ENV_SIZE		    0x80000	 /* Total Size of Environment Sector */

#if defined(CONFIG_CMD_NAND)
#define CONFIG_NAND_S3C2440                 /* 板级相关的NAND FLASH驱动支持 */
#define CONFIG_SYS_NAND_BASE    0x4E000000  /* NAND FLASH 控制器寄存器基地址 */ 
#define CONFIG_SYS_MAX_NAND_DEVICE	1       /* NAND FLASH 设备数 */
#define CONFIG_SYS_64BIT_VSPRINTF           /* 使vsprintf支持%L格式，nand_util.c要求定义此宏 */
//#define CONFIG_MTD_NAND_VERIFY_WRITE      /* 写后再读出验证 */
//#define CONFIG_NAND_HWECC                 /* 使用S3C2440硬件ECC */
#endif	/* CONFIG_CMD_NAND */

#define CONFIG_UBOOT_SIZE       0x80000

#endif	/* __CONFIG_H */
