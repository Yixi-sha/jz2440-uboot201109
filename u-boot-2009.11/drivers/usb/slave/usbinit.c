/*
 * drivers/usb/slave/usbinit.c
 * usb receives, usb device init.
 */

#include <common.h>
#if defined(CONFIG_S3C2400)
#include <s3c2400.h>
#elif defined(CONFIG_S3C2410) || defined(CONFIG_S3C2440)
#include <s3c2410.h>
#endif

#include <asm/io.h>
#include <s3c24x0.h>

#include "usbmain.h"
#include "usbout.h"
#include "usblib.h"
#include "2440usb.h"

#define BIT_DMA2		(0x1<<19)

void Isr_Init(void);
void HaltUndef(void);
void HaltSwi(void);
void HaltPabort(void);
void HaltDabort(void);
void Lcd_Off(void);
__u32 usb_receive(char *buf, size_t len, U32 wait);
void Menu(void);

extern void Timer_InitEx(void);
extern void Timer_StartEx(void);
extern unsigned int Timer_StopEx(void);

extern void (*isr_handle_array[])(void);

/*
 * Reads and returns a character from the serial port
 *   - Times out after delay iterations checking for presence of character
 *   - Sets *error_p to UART error bits or - on timeout
 *   - On timeout, sets *error_p to -1 and returns 0
 */
char awaitkey(unsigned long delay, int* error_p)
{
    int i;

    if (delay == -1) {
        while (1) {
            if (tstc()) /* we got a key press	*/
                return getc();
        }
    } else {        
        for (i = 0; i < delay; i++) {
    		if (tstc()) /* we got a key press	*/
    			return getc();
            udelay (10*1000);
        }
    }
    
    if (error_p)
        *error_p = -1;
    return 0;
}

#define CTRL(x)   (x & 0x1f)
#define INTR      CTRL('C')

void Clk0_Enable(int clock_sel);	
void Clk1_Enable(int clock_sel);
void Clk0_Disable(void);
void Clk1_Disable(void);

volatile U32 downloadAddress;

void (*restart)(void) = (void (*)(void))0x0;
void (*run)(void);


volatile unsigned char *downPt;
volatile U32 downloadFileSize;
volatile U16 checkSum;
volatile unsigned int err=0;
volatile U32 totalDmaCount;

volatile int isUsbdSetConfiguration;

int download_run = 0;
volatile U32 tempDownloadAddress;
int menuUsed = 0;

volatile U32 dwUSBBufReadPtr;
volatile U32 dwUSBBufWritePtr;
volatile U32 dwWillDMACnt;
volatile U32 dwUSBBufBase;
volatile U32 dwUSBBufSize;

int consoleNum;

void usb_init_slave(void)
{
	struct s3c24x0_gpio * const gpioregs = s3c24x0_get_base_gpio();
	char *mode;

	udelay(100000);

    /* 
     * 设置中断处理程序入口，主要针对USBD与DMA2，
     * 前者用于处理USB事务，后者用于处理DMA传输.
     */
	usb_isr_init();

    /* USBD is selected instead of USBH1 */
	gpioregs->MISCCR = gpioregs->MISCCR & ~(1<<3);  
    /* USB port 1 is enabled. */
	gpioregs->MISCCR = gpioregs->MISCCR & ~(1<<13);
    /* USBD should be initialized first of all. */
	isUsbdSetConfiguration = 0;

	UsbdMain(); /* 主要配置函数 */ 
	udelay(100000);

    /*
     * enable USB Device
     * mini2440的USB设备信号线D+通过一个上拉电阻接在
     * GPC5引脚上，只有GPC5输出高电平主机集线器才能
     * 检测到设备，从而给复位信号.
     */
	gpioregs->GPCDAT |= (1<<5);  

#if USBDMA
	mode = "DMA";
#else
	mode = "Int";
#endif
}

__u32 usb_receive(char *buf, size_t len, U32 wait)
{
	int first = 1;
	U8 tempMem[16];
	U32 j;
	unsigned int dwRecvTimeSec = 0;
	char c;
	struct s3c24x0_interrupt *intregs = s3c24x0_get_base_interrupt();
	struct s3c24x0_gpio * const gpioregs = s3c24x0_get_base_gpio();

	gpioregs->MISCCR = gpioregs->MISCCR & ~(1<<3); 

	dwUSBBufReadPtr = dwUSBBufBase;  
	dwUSBBufWritePtr = dwUSBBufBase; 

	tempDownloadAddress = dwUSBBufBase; 

	downloadAddress = (U32)tempMem; 
	downPt = (unsigned char *)downloadAddress;
	downloadFileSize = 0;
    
    /* isUsbdSetConfiguration在USB设备枚举成功后被置1 */
    if (isUsbdSetConfiguration == 0)
	    printf("USB host is not connected yet.\n");

    /* downloadFileSize在USB设备接收到下载数据后被赋值(见EP3中断处理) */
    while (downloadFileSize == 0) {
        if (first == 1 && isUsbdSetConfiguration != 0) {
            printf("USB host is connected. Waiting a download.\n");
            first=0;
        }
		c = awaitkey(1, 0);
		if ((c & 0x7f) == INTR) {
			printf("Cancelled by user\n");
			return 0;
		}
    }

    if (downloadFileSize - 10 > len) {
        printf("Length of file is too big : %d > %d\n", 
               downloadFileSize - 10, len);
        return 0;
    }
    
    Timer_InitEx();
    Timer_StartEx();
        
#if USBDMA    
    intregs->INTMSK &= ~BIT_DMA2;  

    ClearEp3OutPktReady(); 

    if (downloadFileSize > EP3_PKT_SIZE) {
        if (downloadFileSize - EP3_PKT_SIZE <= 0x80000) {
            /* 设置DMA传输的起始地址和大小(头一个Pkg已经在EP3中断中接收) */
            dwUSBBufWritePtr = downloadAddress + EP3_PKT_SIZE - 8;
            dwWillDMACnt = downloadFileSize - EP3_PKT_SIZE;
	    } else {
            dwUSBBufWritePtr = downloadAddress + EP3_PKT_SIZE - 8;
            /* 只是为了使得以后每次DMA传输时dwUSBBufWritePtr能够512KB对齐 */
            dwWillDMACnt = 0x80000 + 8 - EP3_PKT_SIZE;
    	}
     	totalDmaCount = 0;
  	    ConfigEp3DmaMode(dwUSBBufWritePtr, dwWillDMACnt);
    } else {
        dwUSBBufWritePtr = downloadAddress + downloadFileSize - 8;
	    totalDmaCount = downloadFileSize;
    }
#endif

    printf("\nNow, Downloading [ADDRESS:%xh, TOTAL:%d]\n",
    		downloadAddress, downloadFileSize);

    if (wait) {
        printf("RECEIVED DATA SIZE:%8d", 0);

        j = totalDmaCount + 0x10000;
        while (totalDmaCount != downloadFileSize) {
            if (totalDmaCount > j) {
        	    printf("\b\b\b\b\b\b\b\b%8d", j);
                j = totalDmaCount + 0x10000;
            }
        }
	    printf("\b\b\b\b\b\b\b\b%8d ", totalDmaCount);
        dwRecvTimeSec = Timer_StopEx();
        if (dwRecvTimeSec == 0) {
            dwRecvTimeSec = 1;
        }
        printf("(%dKB/S, %dS)\n", (downloadFileSize/dwRecvTimeSec/1024), 
                                  dwRecvTimeSec);
    }

    return downloadFileSize - 10;
}

void udc_disconnect (void)
{
	struct s3c24x0_gpio * gpioregs = s3c24x0_get_base_gpio();
	gpioregs->GPCDAT = gpioregs->GPCDAT & ~(1<<5);	
}
