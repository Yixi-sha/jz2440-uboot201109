/*
 * drivers/usb/slave/interrupt.c
 * usb device/dma/watchdog interrupt init.
 */
#include <common.h>
#if defined(CONFIG_S3C2400)
#include <s3c2400.h>
#elif defined(CONFIG_S3C2410) || defined (CONFIG_S3C2440) 
#include <s3c2410.h>
#endif
#include <asm/proc-armv/ptrace.h>
#include "common_usb.h"

#define BIT_USBD		(0x1<<25)
#define BIT_DMA2		(0x1<<19)
#define BIT_WDT_AC97	(0x1<<9)

extern void (*isr_handle_array[50])(void);

extern void IsrUsbd(void);
extern void IsrDma2(void);

/*
 * 利用watchdog计时.
 */
static int intCount;

void IsrWatchdog(void);

void ClearPending_my(int bit) 
{
	struct s3c24x0_interrupt *intregs = s3c24x0_get_base_interrupt();
	intregs->SRCPND = bit;
	intregs->INTPND = bit;
}

void Timer_InitEx(void)
{
	struct s3c24x0_interrupt *intregs = s3c24x0_get_base_interrupt();
	intCount=0;	
	intregs->SUBSRCPND	= (1<<13);
	ClearPending_my(BIT_WDT_AC97);
	intregs->INTMSK &= ~(BIT_WDT_AC97);
	intregs->INTSUBMSK &= ~(1<<13);
}

void Timer_StartEx(void)
{
	struct s3c24x0_watchdog *wdtregs = s3c24x0_get_base_watchdog();
    
	wdtregs->WTCON = ((get_PCLK()/1000000-1)<<8)|(0<<3)|(1<<2);	// 16us
	wdtregs->WTDAT = 0xffff;
	wdtregs->WTCNT = 0xffff;   

	// 1/16/(65+1),interrupt enable,reset disable,watchdog enable
	wdtregs->WTCON = ((get_PCLK()/1000000-1)<<8) | 
                      (0<<3)|(1<<2)|(0<<0)|(1<<5);   
}

unsigned int Timer_StopEx(void)
{
	int count;
	struct s3c24x0_watchdog *wdtregs = s3c24x0_get_base_watchdog();
	struct s3c24x0_interrupt *intregs = s3c24x0_get_base_interrupt();
	wdtregs->WTCON = ((get_PCLK()/1000000-1)<<8);
	intregs->INTMSK |= BIT_WDT_AC97; //BIT_WDT;
	intregs->INTSUBMSK |= (1<<13);
	
	count = (0xffff-wdtregs->WTCNT)+(intCount*0xffff);
	return ((unsigned int)count*16/1000000);
}

void IsrWatchdog(void)
{
	struct s3c24x0_interrupt *intregs = s3c24x0_get_base_interrupt();
	intregs->SUBSRCPND = (1<<13);
	ClearPending_my(BIT_WDT_AC97);
	intCount++;   	
}

void usb_isr_init(void)
{
	isr_handle_array[ISR_WDT_OFT]  = IsrWatchdog;
	isr_handle_array[ISR_USBD_OFT] = IsrUsbd;
	isr_handle_array[ISR_DMA2_OFT] = IsrDma2;

	ClearPending_my(BIT_DMA2);
	ClearPending_my(BIT_USBD);
}

