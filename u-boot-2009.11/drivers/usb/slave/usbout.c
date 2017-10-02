/*
 * drivers/usb/slave/usbout.c
 * USB bulk-OUT operation related functions.
 * DMA is enabled.
 */
#include <common.h>
#if defined(CONFIG_S3C2400)
#include <s3c2400.h>
#elif defined(CONFIG_S3C2410) || defined(CONFIG_S3C2440)
#include <s3c2410.h>
#endif

#include <s3c24x0.h>

#include "def.h"
 
#include "2440usb.h"
#include "usbmain.h"
#include "usb.h"
#include "usblib.h"
#include "usbsetup.h"
#include "usbout.h"

#include "usbinit.h"

#define BIT_USBD		(0x1<<25)
#define BIT_DMA2		(0x1<<19)

extern volatile U32 dwUSBBufReadPtr;
extern volatile U32 dwUSBBufWritePtr;
extern volatile U32 dwWillDMACnt;
extern volatile U32 bDMAPending;
extern volatile U32 dwUSBBufBase;
extern volatile U32 dwUSBBufSize;
extern void ClearPending_my(int bit); 
static void RdPktEp3_CheckSum(U8 *buf,int num);

/*
 * All following commands will operate in case 
 * - out_csr3 is valid.
 */
#define CLR_EP3_OUT_PKT_READY() usbdevregs->OUT_CSR1_REG= ((out_csr3 & (~ EPO_WR_BITS)) & (~EPO_OUT_PKT_READY) ) 
#define SET_EP3_SEND_STALL()	usbdevregs->OUT_CSR1_REG= ((out_csr3 & (~EPO_WR_BITS)) | EPO_SEND_STALL) )
#define CLR_EP3_SENT_STALL()	usbdevregs->OUT_CSR1_REG= ((out_csr3 & (~EPO_WR_BITS)) &(~EPO_SENT_STALL) )
#define FLUSH_EP3_FIFO() 	usbdevregs->OUT_CSR1_REG= ((out_csr3 & (~EPO_WR_BITS)) |EPO_FIFO_FLUSH) )

U8 ep3Buf[EP3_PKT_SIZE];

void Ep3Handler(void)
{
	struct s3c24x0_interrupt *intregs = s3c24x0_get_base_interrupt();
	struct s3c24x0_usb_device *const usbdevregs	= s3c24x0_get_base_usb_device();
    U8 out_csr3;
    int fifoCnt;

    usbdevregs->INDEX_REG = 3;
    out_csr3 = usbdevregs->OUT_CSR1_REG;
    
    DbgPrintf("<3:%x]",out_csr3);

    if(out_csr3 & EPO_OUT_PKT_READY) {   
        fifoCnt = usbdevregs->OUT_FIFO_CNT1_REG; 

        if (downloadFileSize == 0) {
            RdPktEp3((U8 *)downPt, 8); 	

            if(download_run == 0) {
                downloadAddress = tempDownloadAddress;
            } else {
                downloadAddress = *((U32 *)downPt);
                dwUSBBufReadPtr = downloadAddress;
                dwUSBBufWritePtr = downloadAddress;
            }
            downloadFileSize = *((U32 *)(downPt + 4));
            DbgPrintf("[dwaddr = %x, dwsize = %x]\n", 
                    downloadAddress, downloadFileSize);

            checkSum = 0;
            downPt = (U8 *)downloadAddress;
            /* The first 8-bytes are deleted. */
            RdPktEp3_CheckSum((U8 *)downPt, fifoCnt-8); 	    
            downPt += fifoCnt - 8;  
  	    
#if USBDMA
            /*
             * 传输由中断发起(只接收头一个Pkg)，由DMA完成.
             * 因此发生中断后就禁止USBD中断，在DMA传输完成
             * 后再次开启.
             */
     	    intregs->INTMSK |= BIT_USBD;   
      	    return;	
#endif	
        } else {
            RdPktEp3_CheckSum((U8 *)downPt, fifoCnt); 	    
            downPt += fifoCnt;     //fifoCnt = 64
        }
        CLR_EP3_OUT_PKT_READY();
        return;
    }
    
    if (out_csr3 & EPO_SENT_STALL) {   
        DbgPrintf("[STALL]");
        CLR_EP3_SENT_STALL();
        return;
    }	
}

void RdPktEp3_CheckSum(U8 *buf,int num)
{
    int i;
	struct s3c24x0_usb_device *const usbdevregs = s3c24x0_get_base_usb_device();    
	
    for(i = 0; i < num; i++) {
        buf[i] = (U8)usbdevregs->fifo[3].EP_FIFO_REG;
        checkSum += buf[i];
    }
}

void IsrDma2()
{
	struct s3c24x0_interrupt *intregs = s3c24x0_get_base_interrupt();
	struct s3c24x0_usb_device *usbdevregs = s3c24x0_get_base_usb_device();
    U8 out_csr3;
    U32 dwEmptyCnt;
    U8 saveIndexReg = usbdevregs->INDEX_REG;
    usbdevregs->INDEX_REG = 3;
    out_csr3 = usbdevregs->OUT_CSR1_REG;

    ClearPending_my((int)BIT_DMA2);	    

    if (!totalDmaCount) 
        totalDmaCount = dwWillDMACnt + EP3_PKT_SIZE;
    else
        totalDmaCount += dwWillDMACnt;

    dwUSBBufWritePtr = ((dwUSBBufWritePtr + dwWillDMACnt - dwUSBBufBase) % 
                        dwUSBBufSize) + dwUSBBufBase;

    if (totalDmaCount >= downloadFileSize) {    /* last */
    	totalDmaCount = downloadFileSize;
	
    	ConfigEp3IntMode();	

    	if (out_csr3 & EPO_OUT_PKT_READY) 
       	    CLR_EP3_OUT_PKT_READY();
	    /* 关闭DMA2中断并重新开启USBD中断 */ 
        intregs->INTMSK |= BIT_DMA2;    
        intregs->INTMSK &= ~(BIT_USBD);  
    } else {
    	if ((totalDmaCount + 0x80000) < downloadFileSize) 
    	    dwWillDMACnt = 0x80000;
    	else
    	    dwWillDMACnt = downloadFileSize - totalDmaCount;

        dwEmptyCnt = (dwUSBBufReadPtr - dwUSBBufWritePtr - 1 + dwUSBBufSize) % 
                     dwUSBBufSize;

        if (dwEmptyCnt >= dwWillDMACnt)
    	    ConfigEp3DmaMode(dwUSBBufWritePtr, dwWillDMACnt);
    }
    usbdevregs->INDEX_REG = saveIndexReg;
}

void ClearEp3OutPktReady(void)
{
	struct s3c24x0_usb_device * const usbdevregs = s3c24x0_get_base_usb_device();
    U8 out_csr3;
    usbdevregs->INDEX_REG = 3;
    out_csr3 = usbdevregs->OUT_CSR1_REG;
    CLR_EP3_OUT_PKT_READY();
}
