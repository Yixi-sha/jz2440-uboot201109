/*
 * drivers/usb/slave/usblib.c
 * S3C2440X USB library functions.
 */
#include <common.h>
#if defined(CONFIG_S3C2400)
#include <s3c2400.h>
#elif defined(CONFIG_S3C2410) || defined(CONFIG_S3C2440)
#include <s3c2410.h>
#endif

#include <s3c24x0.h>

#include <asm/io.h>

#include "2440usb.h"
#include "usbmain.h"
#include "usblib.h"
#include "usbsetup.h"
#include "usbmain.h"

#define BIT_USBD		(0x1<<25)

extern volatile U32 dwUSBBufReadPtr;
extern volatile U32 dwUSBBufWritePtr;
extern volatile U32 dwPreDMACnt;
extern volatile U32 dwNextDMACnt;

void ConfigUsbd(void)
{
	struct s3c24x0_interrupt *intregs = s3c24x0_get_base_interrupt();
    ReconfigUsbd();
    intregs->INTMSK &= ~(BIT_USBD);  
}

/*   
 * EP1: bulk in end point
 * EP3: bulk out end point
 * other: not used.
 */
void ReconfigUsbd(void)
{
	struct s3c24x0_usb_device * const usbdevregs = s3c24x0_get_base_usb_device();   
 
    usbdevregs->PWR_REG = PWR_REG_DEFAULT_VALUE;	//disable suspend mode

    /* EP0 cfg */
    usbdevregs->INDEX_REG = 0;	
    usbdevregs->MAXP_REG = FIFO_SIZE_8;   	
    usbdevregs->EP0_CSR_IN_CSR1_REG = EP0_SERVICED_OUT_PKT_RDY | EP0_SERVICED_SETUP_END;	

    /* EP1 cfg */
    usbdevregs->INDEX_REG = 1;
	usbdevregs->MAXP_REG = FIFO_SIZE_64;	
    usbdevregs->EP0_CSR_IN_CSR1_REG = EPI_FIFO_FLUSH | EPI_CDT | EPI_BULK;
    usbdevregs->IN_CSR2_REG = EPI_MODE_IN | EPI_IN_DMA_INT_MASK; 
    usbdevregs->OUT_CSR1_REG = EPO_CDT;   	
    usbdevregs->OUT_CSR2_REG = EPO_BULK | EPO_OUT_DMA_INT_MASK;  

    /* EP3 cfg */
    usbdevregs->INDEX_REG = 3;
	usbdevregs->MAXP_REG = FIFO_SIZE_64;	
    usbdevregs->EP0_CSR_IN_CSR1_REG = EPI_FIFO_FLUSH | EPI_CDT | EPI_BULK;
    usbdevregs->IN_CSR2_REG = EPI_MODE_OUT | EPI_IN_DMA_INT_MASK; 
    usbdevregs->OUT_CSR1_REG = EPO_CDT;   	
    usbdevregs->OUT_CSR2_REG = EPO_BULK | EPO_OUT_DMA_INT_MASK;   	

    /* 清中断 */
    usbdevregs->EP_INT_REG = EP0_INT | EP1_INT | EP2_INT | EP3_INT | EP4_INT;
    usbdevregs->USB_INT_REG = RESET_INT | SUSPEND_INT | RESUME_INT; 
    	
    /* EP0,3 & reset interrupt are enabled */
    usbdevregs->EP_INT_EN_REG = EP0_INT | EP3_INT;
    usbdevregs->USB_INT_EN_REG = RESET_INT;
    ep0State = EP0_STATE_INIT;
}

void RdPktEp0(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device * const usbdevregs = s3c24x0_get_base_usb_device();   
    for(i=0;i<num;i++)
        buf[i] = (U8)usbdevregs->fifo[0].EP_FIFO_REG;
}
    
void WrPktEp0(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device *const usbdevregs = s3c24x0_get_base_usb_device();      	
    for(i=0;i<num;i++)
        usbdevregs->fifo[0].EP_FIFO_REG = buf[i];	
}

void WrPktEp1(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device *const usbdevregs = s3c24x0_get_base_usb_device();      	
    for(i=0;i<num;i++)
        usbdevregs->fifo[1].EP_FIFO_REG = buf[i];	
}

void WrPktEp2(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device *const usbdevregs = s3c24x0_get_base_usb_device();      	
    for(i=0;i<num;i++)
        usbdevregs->fifo[2].EP_FIFO_REG = buf[i];	
}

void RdPktEp3(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device *const usbdevregs = s3c24x0_get_base_usb_device();      	
    for(i=0;i<num;i++)
        buf[i]=(U8)usbdevregs->fifo[3].EP_FIFO_REG;	
}

void RdPktEp4(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device * const usbdevregs = s3c24x0_get_base_usb_device();      	
    for(i=0;i<num;i++)
        buf[i] = (U8)usbdevregs->fifo[4].EP_FIFO_REG;	
}

void ConfigEp3DmaMode(U32 bufAddr, U32 count)
{
    int i;
	struct s3c24x0_usb_device *const usbdevregs	= s3c24x0_get_base_usb_device();  
	struct s3c24x0_dmas *const dmaregs	= s3c24x0_get_base_dmas();

    usbdevregs->INDEX_REG = 3;
    count = count & 0xfffff; //transfer size should be <1MB
    
    dmaregs->dma[2].DISRCC = (1<<1) | (1<<0);
    dmaregs->dma[2].DISRC = ADDR_EP3_FIFO; //src=APB,fixed,src=EP3_FIFO
    dmaregs->dma[2].DIDSTC = (0<<1) | (0<<0);  
    dmaregs->dma[2].DIDST = bufAddr;       //dst=AHB,increase,dst=bufAddr
#if USBDMA_DEMAND
    //demand,requestor=APB,CURR_TC int enable,unit transfer,
    //single service,src=USBD,H/W request,autoreload,byte,CURR_TC
    dmaregs->dma[2].DCON = (count)|(0<<31)|(0<<30)|(1<<29)|(0<<28)|\
                           (0<<27)|(4<<24)|(1<<23)|(0<<22)|(0<<20); 
#else
    //handshake,requestor=APB,CURR_TC int enable,unit transfer,
    //single service,src=USBD,H/W request,autoreload,byte,CURR_TC
    dmaregs->dma[2].DCON = (count)|(1<<31)|(0<<30)|(1<<29)|(0<<28)|\
                           (0<<27)|(4<<24)|(1<<23)|(1<<22)|(0<<20); 
#endif        
    dmaregs->dma[2].DMASKTRIG = (1<<1);  //DMA 2 on

    //Total Transfer Counter
    usbdevregs->ep3.EP_DMA_TTC_L = 0xff;
    usbdevregs->ep3.EP_DMA_TTC_M = 0xff;
    usbdevregs->ep3.EP_DMA_TTC_H = 0x0f;

    //AUTO_CLR(OUT_PKT_READY is cleared automatically), interrupt_masking.
    usbdevregs->OUT_CSR2_REG = usbdevregs->OUT_CSR2_REG | \
                               EPO_AUTO_CLR | EPO_OUT_DMA_INT_MASK; 

#if USBDMA_DEMAND
    usbdevregs->ep3.EP_DMA_UNIT = EP3_PKT_SIZE; //DMA transfer unit=64 bytes
    // deamnd enable,out_dma_run=run,in_dma_run=stop,DMA mode enable
    usbdevregs->ep3.EP_DMA_CON = UDMA_DEMAND_MODE | UDMA_OUT_DMA_RUN | \
                                 UDMA_DMA_MODE_EN; 
#else        
    usbdevregs->ep3.EP_DMA_UNIT = 0x01; //DMA transfer unit=1byte
    // deamnd disable,out_dma_run=run,in_dma_run=stop,DMA mode enable
    usbdevregs->ep3.EP_DMA_CON = UDMA_OUT_DMA_RUN | UDMA_DMA_MODE_EN;
#endif  
    //wait until DMA_CON is effective.
    while (!(readb(&usbdevregs->ep3.EP_DMA_CON) & UDMA_OUT_RUN_OB));
}

void ConfigEp3IntMode(void)
{
	struct s3c24x0_usb_device *const usbdevregs	= s3c24x0_get_base_usb_device(); 
	struct s3c24x0_dmas *const dmaregs = s3c24x0_get_base_dmas(); 

    usbdevregs->INDEX_REG = 3;
    
    dmaregs->dma[2].DMASKTRIG = (0<<1);  //DMA channel off
    usbdevregs->OUT_CSR2_REG = usbdevregs->OUT_CSR2_REG & ~(EPO_AUTO_CLR); 
    usbdevregs->ep3.EP_DMA_UNIT = 1;	
    usbdevregs->ep3.EP_DMA_CON = 0; 
    //wait until DMA_CON is effective. 
    while (readb(&usbdevregs->ep3.EP_DMA_CON) & UDMA_OUT_RUN_OB);
}
