/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Alex Zuepke <azu@sysgo.de>
 *
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <gj@denx.de>
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

#include <common.h>

#if defined(CONFIG_S3C2400)
#include <s3c2400.h>
#elif defined(CONFIG_S3C2410) || defined(CONFIG_S3C2440)
#include <s3c2410.h>
#endif
#include <asm/proc-armv/ptrace.h>
#include <asm/io.h>

#define BIT_ALLMSK		(0xFFFFFFFF)

void (*isr_handle_array[50])(void);


void do_irq (struct pt_regs *pt_regs)
{
	struct s3c24x0_interrupt *irq = s3c24x0_get_base_interrupt();
	u_int32_t intpnd = readl(&irq->INTPND);

	struct s3c24x0_gpio *gpio = s3c24x0_get_base_gpio();
	unsigned long oft = readl(&irq->INTOFFSET);
    
	/* clean int */
	if (oft == 4) 
        gpio->EINTPEND = 1<<7;
	irq->SRCPND = 1<<oft;	
	irq->INTPND	= intpnd;	 

	/* run the isr */
	isr_handle_array[oft]();
}

void dummy_isr(void)
{
	struct s3c24x0_interrupt *intregs = s3c24x0_get_base_interrupt();
	printf("dummy_isr, INTOFFSET: %d, INTMSK = 0x%x\n", 
            intregs->INTOFFSET, intregs->INTMSK);
	while(1);
}

int arch_interrupt_init(void)
{
	int i = 0;
	struct s3c24x0_interrupt *intregs = s3c24x0_get_base_interrupt();
    
	intregs->INTMOD = 0x0;	          // All=IRQ mode
	intregs->INTMSK = BIT_ALLMSK;	  // All interrupt is masked.
	intregs->INTPND = intregs->INTPND;	  // clear all.
 
	for (i = 0; i < ARRAY_SIZE(isr_handle_array); i++)
		isr_handle_array[i] = dummy_isr;

    return 0;
}
