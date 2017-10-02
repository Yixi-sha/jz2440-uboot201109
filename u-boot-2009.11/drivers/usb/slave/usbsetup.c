/*
 * drivers/usb/slave/usbsetup.c
 * process the USB setup stage operations.
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

/*
 * End point information
 * EP0: control
 * EP1: not used
 * EP2: not used
 * EP3: bulk out end point
 * EP4: not used
 *
 * VERY IMPORTANT NOTE:
 * Every descriptor size of EP0 should be 8n+m(m=1~7).
 * Otherwise, USB will not operate normally because the program
 * doesn't prepare the case that the descriptor size is 8n+0.
 * If the size of a descriptor is 8n, the 0 length packit should be sent. 
 *
 * All following commands will operate only in case 
 * - ep0_csr is valid.
 */
#define CLR_EP0_OUT_PKT_RDY() 		usbdevregs->EP0_CSR_IN_CSR1_REG = \
    ((ep0_csr & (~EP0_WR_BITS)) | EP0_SERVICED_OUT_PKT_RDY )

#define CLR_EP0_OUTPKTRDY_DATAEND() usbdevregs->EP0_CSR_IN_CSR1_REG = \
    ((ep0_csr & (~EP0_WR_BITS)) | (EP0_SERVICED_OUT_PKT_RDY | EP0_DATA_END))

#define SET_EP0_IN_PKT_RDY() 		usbdevregs->EP0_CSR_IN_CSR1_REG = \
    ((ep0_csr & (~EP0_WR_BITS)) | (EP0_IN_PKT_READY))	 

#define SET_EP0_INPKTRDY_DATAEND() 	usbdevregs->EP0_CSR_IN_CSR1_REG = \
    ((ep0_csr & (~EP0_WR_BITS)) | (EP0_IN_PKT_READY | EP0_DATA_END))			

#define CLR_EP0_SETUP_END() 		usbdevregs->EP0_CSR_IN_CSR1_REG = \
    ((ep0_csr & (~EP0_WR_BITS)) | (EP0_SERVICED_SETUP_END))

#define CLR_EP0_SENT_STALL() 		usbdevregs->EP0_CSR_IN_CSR1_REG = \
    ((ep0_csr & (~EP0_WR_BITS)) & (~EP0_SENT_STALL))

U32 ep0State;
U32 ep0SubState;

extern volatile int isUsbdSetConfiguration;
volatile U8 Rwuen;
volatile U8 Configuration=1;
volatile U8 AlterSetting;
volatile U8 Selfpwr = TRUE;   
volatile U8 device_status;
volatile U8 interface_status;
volatile U8 endpoint0_status;
volatile U8 endpoint3_status;

struct USB_SETUP_DATA descSetup;
struct USB_DEVICE_DESCRIPTOR descDev;
struct USB_CONFIGURATION_DESCRIPTOR descConf;
struct USB_INTERFACE_DESCRIPTOR descIf;
struct USB_ENDPOINT_DESCRIPTOR descEndpt1;  /* EP1 desc */
struct USB_ENDPOINT_DESCRIPTOR descEndpt3;  /* EP3 desc */
struct USB_CONFIGURATION_SET ConfigSet;     /* 记录当前的配置 */
struct USB_INTERFACE_GET InterfaceGet;
struct USB_GET_STATUS StatusGet;  

static const U8 descStr0[] = {
    4, STRING_TYPE, LANGID_US_L, LANGID_US_H,  //codes representing languages
};

static const U8 descStr1[] = {  //Manufacturer  
    (0x30+2), STRING_TYPE, 
    'F',0x0, 'r',0x0, 'i',0x0, 'e',0x0, 'n',0x0, 'd',0x0, 'l',0x0, 'y',0x0,
    'A',0x0, 'R',0x0, 'M',0x0, '/',0x0, 'G',0x0, 'u',0x0, 'o',0x0, '_',0x0, 
    'Q',0x0, 'i',0x0, 'a',0x0, 'n',0x0, '-',0x0, 'C',0x0, 'D',0x0, '.',0x0,
};

static const U8 descStr2[] = {  //Product  
    (0x30+2), STRING_TYPE, 
    'M',0x0, 'i',0x0, 'n',0x0, 'i',0x0, '2',0x0, '4',0x0, '4',0x0, '0',0x0, 
    '/',0x0, 'G',0x0, 'Q',0x0, '2',0x0, '4',0x0, '4',0x0, '0',0x0, ' ',0x0, 
    'U',0x0, 'S',0x0, 'B',0x0, ' ',0x0, 'd',0x0, 'n',0x0, 'w',0x0, '.',0x0, 
};

static void HandleGetDescReq(struct USB_SETUP_DATA *desc)
{
    switch (desc->bValueH) {   
    case DEVICE_TYPE:
        DbgPrintf("[GDD]");
        ep0State = EP0_STATE_GD_DEV_0;	        
        break;	
    case CONFIGURATION_TYPE:
        DbgPrintf("[GDC]");
        if((desc->bLengthL + (desc->bLengthH<<8)) > 0x9)
            ep0State = EP0_STATE_GD_CFG_0; 
        else
            ep0State = EP0_STATE_GD_CFG_ONLY_0; 
        break;
    case STRING_TYPE:
        DbgPrintf("[GDS]");
        switch (desc->bValueL) {
        case 0:
            ep0State = EP0_STATE_GD_STR_I0;
            break;
        case 1:
            ep0State = EP0_STATE_GD_STR_I1;
            break;
        case 2:	
            ep0State = EP0_STATE_GD_STR_I2;
            break;
        default:
            DbgPrintf("[UE:STRI?]");
            break;
        }
        ep0SubState = 0;
        break;
    case INTERFACE_TYPE:
        DbgPrintf("[GDI]");
        ep0State = EP0_STATE_GD_IF_ONLY_0; 
        break;
    case ENDPOINT_TYPE:	    	
        DbgPrintf("[GDE]");
        switch (desc->bValueL & 0xf) {
        case 3:
            ep0State = EP0_STATE_GD_EP3_ONLY_0;
            break;
        default:
            DbgPrintf("[UE:GDE?]");
            break;
        }
        break;
    default:
        DbgPrintf("[UE:GD?]");
        break;
    }	
}

static inline void HandleSetAddrReq(U8 addr)
{
    struct s3c24x0_usb_device * const usbdevregs = s3c24x0_get_base_usb_device();
    
    DbgPrintf("[SA:%d]", addr);
    usbdevregs->FUNC_ADDR_REG = addr | 0x80;
    ep0State = EP0_STATE_INIT; 
}

static inline void HandleSetCfgReq(U8 cfg)
{
    DbgPrintf("[SC]");
    ConfigSet.ConfigurationValue = cfg;
    ep0State = EP0_STATE_INIT;
    isUsbdSetConfiguration = 1; 
}

static inline void HandleClrFeatureReq(struct USB_SETUP_DATA *desc)
{
    DbgPrintf("[CF]");
    switch (desc->bmRequestType) {
    case DEVICE_RECIPIENT:
        if (desc->bValueL == DEVICE_REMOTE_WAKEUP)
            Rwuen = FALSE;   
        break;
    case ENDPOINT_RECIPIENT:
        if (desc->bValueL == ENDPOINT_HALT) {
            switch ((desc->bIndexL & 0xff)) {
            case 0x0:
                StatusGet.Endpoint0 = 0;
                break;
            case 0x03:       // OUT Endpoint 3
                StatusGet.Endpoint3 = 0;      
                break;
            default:
                break;
            }
        }
        break;
    default:
        break;
    }
    ep0State = EP0_STATE_INIT;
}

static inline void HandleGetStatusReq(struct USB_SETUP_DATA *desc)
{
    DbgPrintf("[GS]");
    switch (desc->bmRequestType) {
    case 0x80:
        StatusGet.Device = ((U8)Rwuen<<1) | (U8)Selfpwr;
        ep0State = EP0_GET_DEV_STATUS;
        break;
    case 0x81:
        StatusGet.Interface = 0;
        ep0State = EP0_GET_INF_STATUS;
        break;
    case 0x82:
        switch ((desc->bIndexL & 0xff)) {
        case 0x0:
            ep0State = EP0_GET_EP0_STATUS;
            break;
        case 0x03:       // OUT Endpoint 3
            ep0State = EP0_GET_EP3_STATUS;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

static inline void HandleSetFeatureReq(struct USB_SETUP_DATA *desc)
{
    DbgPrintf("[SF]");
    switch (desc->bmRequestType) {
    case DEVICE_RECIPIENT:
        if (desc->bValueL == DEVICE_REMOTE_WAKEUP) 
            Rwuen = TRUE;
        break;
    case ENDPOINT_RECIPIENT:
        if (desc->bValueL == ENDPOINT_HALT) {
            switch ((desc->bIndexL & 0xff)) {
            case 0x0:
                StatusGet.Endpoint0 = 1;
                break;
            case 0x03:       // OUT Endpoint 3
                StatusGet.Endpoint3 = 1;      
                break;
            default:
                break;
            }
        }
        break;
    default:
        break;
    }
    ep0State=EP0_STATE_INIT;
}

static inline void HandleSetInfReq(U8 inf)
{
    DbgPrintf("[SI]");
    InterfaceGet.AlternateSetting = inf;
    ep0State = EP0_STATE_INIT;
}

static void HandleDevReqF(struct USB_SETUP_DATA *desc)
{
    U8 ep0_csr;
    struct s3c24x0_usb_device * const usbdevregs = s3c24x0_get_base_usb_device();
    ep0_csr = usbdevregs->EP0_CSR_IN_CSR1_REG;

    PrintEp0Pkt((U8 *)(desc)); //DEBUG

    switch (desc->bRequest) {
    case GET_DESCRIPTOR:
        HandleGetDescReq(desc);
        CLR_EP0_OUT_PKT_RDY();
        break;
    case SET_ADDRESS:
        HandleSetAddrReq(desc->bValueL);
        CLR_EP0_OUTPKTRDY_DATAEND(); //Because of no data control transfers.
        break;
    case SET_CONFIGURATION:
        HandleSetCfgReq(desc->bValueL);
        CLR_EP0_OUTPKTRDY_DATAEND(); 
        break;
    case CLEAR_FEATURE:
        HandleClrFeatureReq(desc);
        CLR_EP0_OUTPKTRDY_DATAEND();
        break;
    case GET_CONFIGURATION:
        ep0State = EP0_CONFIG_SET;
        CLR_EP0_OUT_PKT_RDY();
        break;
    case GET_INTERFACE:
        ep0State = EP0_INTERFACE_GET;
        CLR_EP0_OUT_PKT_RDY();
        break;
    case GET_STATUS:
        HandleGetStatusReq(desc);
        CLR_EP0_OUT_PKT_RDY();
        break;
    case SET_DESCRIPTOR:  /* nothing to do */
        ep0State = EP0_STATE_INIT; 
        CLR_EP0_OUTPKTRDY_DATAEND();
        break;
    case SET_FEATURE:
        HandleSetFeatureReq(desc);
        CLR_EP0_OUTPKTRDY_DATAEND();
        break;
    case SET_INTERFACE:
        HandleSetInfReq(desc->bValueL);
        CLR_EP0_OUTPKTRDY_DATAEND(); 
        break;
    case SYNCH_FRAME: /* nothing to do */
        ep0State = EP0_STATE_INIT;
        break;
    default:
        DbgPrintf("[UE:SETUP=%x]", descSetup.bRequest);
        ep0State = EP0_STATE_INIT;
        CLR_EP0_OUTPKTRDY_DATAEND(); 
        break;
    }
}

static void HandleDevReqR(struct USB_SETUP_DATA *desc)
{
    static int ep0SubState;

    U8 ep0_csr;
    struct s3c24x0_usb_device * const usbdevregs = s3c24x0_get_base_usb_device();
    ep0_csr = usbdevregs->EP0_CSR_IN_CSR1_REG;

    switch (ep0State) {	
    case EP0_STATE_INIT:      /* nothing to do */
        break; 
    case EP0_STATE_GD_DEV_0:  /* GET_DESCRIPTOR: DEVICE */
        DbgPrintf("[GDD0]");
        WrPktEp0((U8 *)&descDev, 8); //EP0_PKT_SIZE = 8
        SET_EP0_IN_PKT_RDY();
        ep0State = EP0_STATE_GD_DEV_1;
        break;
    case EP0_STATE_GD_DEV_1:
        DbgPrintf("[GDD1]");
        WrPktEp0((U8 *)&descDev+0x8, 8); 
        SET_EP0_IN_PKT_RDY();
        ep0State = EP0_STATE_GD_DEV_2;
        break;
    case EP0_STATE_GD_DEV_2:
        DbgPrintf("[GDD2]");
        WrPktEp0((U8 *)&descDev+0x10, 2);   //8+8+2=0x12
        SET_EP0_INPKTRDY_DATAEND();
        ep0State = EP0_STATE_INIT;
        break;
    case EP0_STATE_GD_CFG_0: /* CONFIGURATION + INTERFACE + ENDPOINT0 + ENDPOINT1 */
        DbgPrintf("[GDC0]");
        WrPktEp0((U8 *)&descConf, 8); //EP0_PKT_SIZE
        SET_EP0_IN_PKT_RDY();
        ep0State = EP0_STATE_GD_CFG_1;
        break;
    case EP0_STATE_GD_CFG_1:
        DbgPrintf("[GDC1]");
        WrPktEp0((U8 *)&descConf+0x8, 1); 
        WrPktEp0((U8 *)&descIf, 7); 
        SET_EP0_IN_PKT_RDY();
        ep0State = EP0_STATE_GD_CFG_2;
        break;
    case EP0_STATE_GD_CFG_2:
        DbgPrintf("[GDC2]");
        WrPktEp0((U8 *)&descIf+0x7, 2); 
        WrPktEp0((U8 *)&descEndpt1, 6); 
        SET_EP0_IN_PKT_RDY();
        ep0State = EP0_STATE_GD_CFG_3;
        break;
    case EP0_STATE_GD_CFG_3:
        DbgPrintf("[GDC3]");
        WrPktEp0((U8 *)&descEndpt1+6, 1); 
        WrPktEp0((U8 *)&descEndpt3, 7); 
        SET_EP0_IN_PKT_RDY();
        ep0State = EP0_STATE_GD_CFG_4;            
        break;
    case EP0_STATE_GD_CFG_4:
        DbgPrintf("[GDC4]");
        SET_EP0_INPKTRDY_DATAEND();
        ep0State = EP0_STATE_INIT;            
        break;
    case EP0_STATE_GD_CFG_ONLY_0: /* GET_DESCRIPTOR: CONFIGURATION ONLY */
        DbgPrintf("[GDCO0]");
        WrPktEp0((U8 *)&descConf, 8); //EP0_PKT_SIZE
        SET_EP0_IN_PKT_RDY();
        ep0State = EP0_STATE_GD_CFG_ONLY_1;
        break;
    case EP0_STATE_GD_CFG_ONLY_1:
        DbgPrintf("[GDCO1]");
        WrPktEp0((U8 *)&descConf+8, 1); 
        SET_EP0_INPKTRDY_DATAEND();
        ep0State = EP0_STATE_INIT;            
        break;
    case EP0_STATE_GD_IF_ONLY_0:  /* GET_DESCRIPTOR: INTERFACE ONLY */
        DbgPrintf("[GDI0]");
        WrPktEp0((U8 *)&descIf, 8); 
        SET_EP0_IN_PKT_RDY();
        ep0State = EP0_STATE_GD_IF_ONLY_1;
        break;
    case EP0_STATE_GD_IF_ONLY_1:
        DbgPrintf("[GDI1]");
        WrPktEp0((U8 *)&descIf+8, 1); 
        SET_EP0_INPKTRDY_DATAEND();
        ep0State = EP0_STATE_INIT;            
        break;
    case EP0_STATE_GD_EP3_ONLY_0: /* GET_DESCRIPTOR: ENDPOINT 3 ONLY */
        DbgPrintf("[GDEP30]");
        WrPktEp0((U8 *)&descEndpt3, 7); 
        SET_EP0_INPKTRDY_DATAEND();
        ep0State = EP0_STATE_INIT;            
        break;
    case EP0_INTERFACE_GET:
        WrPktEp0((U8 *)&InterfaceGet, 1);
        SET_EP0_INPKTRDY_DATAEND();
        ep0State = EP0_STATE_INIT;      
        break;
    case EP0_STATE_GD_STR_I0:     /* GET_DESCRIPTOR: STRING */
        DbgPrintf("[GDS0_0]");
        WrPktEp0((U8 *)descStr0, 4);  
        SET_EP0_INPKTRDY_DATAEND();
        ep0State = EP0_STATE_INIT;     
        ep0SubState = 0;
        break;
    case EP0_STATE_GD_STR_I1:
        DbgPrintf("[GDS1_%d]",ep0SubState);
        if((ep0SubState*EP0_PKT_SIZE + EP0_PKT_SIZE) < sizeof(descStr1)) {
            WrPktEp0((U8 *)descStr1+(ep0SubState*EP0_PKT_SIZE), EP0_PKT_SIZE); 
            SET_EP0_IN_PKT_RDY();
            ep0State = EP0_STATE_GD_STR_I1;
            ep0SubState++;
        } else {
            WrPktEp0((U8 *)descStr1 + ep0SubState*EP0_PKT_SIZE,
                    sizeof(descStr1) - ep0SubState*EP0_PKT_SIZE); 
            SET_EP0_INPKTRDY_DATAEND();
            ep0State = EP0_STATE_INIT;     
            ep0SubState = 0;
        }
        break;
    case EP0_STATE_GD_STR_I2:
        DbgPrintf("[GDS2_%d]",ep0SubState);
        if((ep0SubState*EP0_PKT_SIZE + EP0_PKT_SIZE) < sizeof(descStr2)) {
            WrPktEp0((U8 *)descStr2+(ep0SubState*EP0_PKT_SIZE), EP0_PKT_SIZE); 
            SET_EP0_IN_PKT_RDY();
            ep0State = EP0_STATE_GD_STR_I2;
            ep0SubState++;
        } else {
            DbgPrintf("[E]");
            WrPktEp0((U8 *)descStr2 + ep0SubState*EP0_PKT_SIZE,
                    sizeof(descStr2) - ep0SubState*EP0_PKT_SIZE); 
            SET_EP0_INPKTRDY_DATAEND();
            ep0State = EP0_STATE_INIT;     
            ep0SubState = 0;
        }
        break;
    case EP0_CONFIG_SET:
        WrPktEp0((U8 *)&ConfigSet, 1); 
        SET_EP0_INPKTRDY_DATAEND();
        ep0State = EP0_STATE_INIT;      
        break;
    case EP0_GET_DEV_STATUS:
        WrPktEp0((U8 *)&StatusGet.Device, 1);
        SET_EP0_INPKTRDY_DATAEND();
        ep0State = EP0_STATE_INIT;      
        break;
    case EP0_GET_INF_STATUS:
        WrPktEp0((U8 *)&StatusGet.Interface, 1);
        SET_EP0_INPKTRDY_DATAEND();
        ep0State = EP0_STATE_INIT;      
        break;
    case EP0_GET_EP0_STATUS:
        WrPktEp0((U8 *)&StatusGet.Endpoint0, 1);
        SET_EP0_INPKTRDY_DATAEND();
        ep0State = EP0_STATE_INIT;      
        break;
    case EP0_GET_EP3_STATUS:
        WrPktEp0((U8 *)&StatusGet.Endpoint3, 1);
        SET_EP0_INPKTRDY_DATAEND();
        ep0State=EP0_STATE_INIT;      
        break;
    default:
        DbgPrintf("UE:G?D");
        break;
    }
}

void Ep0Handler(void)
{
    U8 ep0_csr;
    struct s3c24x0_usb_device *usbdevregs = s3c24x0_get_base_usb_device();

    usbdevregs->INDEX_REG = 0;
    ep0_csr = usbdevregs->EP0_CSR_IN_CSR1_REG;

    DbgPrintf("<0:%x]\n",ep0_csr);

    if (ep0_csr & EP0_SETUP_END) {   
        /*
         * 主机在获取描述符时可能会在IN数据阶段
         * 未完成前就中止，这时SETUP_END位会被设置.
         */
        DbgPrintf("[SETUPEND]");
        CLR_EP0_SETUP_END();

        if(ep0_csr & EP0_OUT_PKT_READY) 
            CLR_EP0_OUT_PKT_RDY();

        ep0State = EP0_STATE_INIT;
        return;
    }	

    if(ep0_csr & EP0_SENT_STALL) {   
        DbgPrintf("[STALL]");
        CLR_EP0_SENT_STALL();

        if(ep0_csr & EP0_OUT_PKT_READY) 
            CLR_EP0_OUT_PKT_RDY();

        ep0State = EP0_STATE_INIT;
        return;
    }	

    if((ep0_csr & EP0_OUT_PKT_READY)) {	
        RdPktEp0((U8 *)&descSetup, EP0_PKT_SIZE);
        HandleDevReqF(&descSetup);
    }

    HandleDevReqR(&descSetup);
}

void PrintEp0Pkt(U8 *pt)
{
    int i;
    DbgPrintf("[RCV:");
    for(i=0;i<EP0_PKT_SIZE;i++)
        DbgPrintf("%x,",pt[i]);
    DbgPrintf("]");
}

void InitDescriptorTable(void)
{	
    /* Standard device descriptor */
    descDev.bLength = 0x12; 
    descDev.bDescriptorType = DEVICE_TYPE;         
    descDev.bcdUSBL = 0x10;
    descDev.bcdUSBH = 0x01;      /* USB Ver 1.10 */
    descDev.bDeviceClass = 0xFF;        
    descDev.bDeviceSubClass = 0x0;          
    descDev.bDeviceProtocol = 0x0;          
    descDev.bMaxPacketSize0 = 0x8;         
    descDev.idVendorL = 0x45;    /* vendor ID: 0x5345. */
    descDev.idVendorH = 0x53;
    descDev.idProductL = 0x34;   /* product ID: 0x1234 */
    descDev.idProductH = 0x12;   /* PC驱动利用这两个ID来匹配设备 */
    descDev.bcdDeviceL = 0x00;   /* 设备版本信息1.00 */
    descDev.bcdDeviceH = 0x01;
    descDev.iManufacturer = 0x1; /* 描述设备厂商的串描述符索引 */
    descDev.iProduct = 0x2;      /* 描述产品的字符串描述符索引 */
    descDev.iSerialNumber = 0x0; /* 没有对应的序列号字符串描述符 */
    descDev.bNumConfigurations = 0x1;   /* 只有一个配置 */

    /* Standard configuration descriptor */
    descConf.bLength = 0x9;    
    descConf.bDescriptorType = CONFIGURATION_TYPE;         
    descConf.wTotalLengthL = 0x20;    /* <cfg desc> + <if desc> + <endp3 desc> */
    descConf.wTotalLengthH = 0;
    descConf.bNumInterfaces = 1;      /* 该配置包含的接口数 */
    descConf.bConfigurationValue = 1; /* 配置编号 */ 
    descConf.iConfiguration = 0;      /* 描述该配置的串描述符索引 */
    descConf.bmAttributes = CONF_ATTR_DEFAULT | CONF_ATTR_SELFPOWERED; /* 设备自供电 */
    descConf.maxPower = 25; /* draws 50mA current from the USB bus. */         

    /* Standard interface descriptor */
    descIf.bLength = 0x9;    
    descIf.bDescriptorType = INTERFACE_TYPE;         
    descIf.bInterfaceNumber = 0x0;    /* 接口编号 */
    descIf.bAlternateSetting = 0x0;
    descIf.bNumEndpoints = 2;         /* 该接口拥有的端点数(除去EP0) */
    descIf.bInterfaceClass = 0xff; 
    descIf.bInterfaceSubClass = 0x0;  
    descIf.bInterfaceProtocol = 0x0;
    descIf.iInterface = 0x0;          /* 描述该接口的串描述符索引 */

    /* Standard endpoint descriptor */
    descEndpt1.bLength = 0x7;    
    descEndpt1.bDescriptorType = ENDPOINT_TYPE;         
    descEndpt1.bEndpointAddress = 1 | EP_ADDR_IN;  /* EP1 IN */  
    descEndpt1.bmAttributes = EP_ATTR_BULK;        /* bulk  */
    descEndpt1.wMaxPacketSizeL = EP1_PKT_SIZE;     /* 64 */
    descEndpt1.wMaxPacketSizeH = 0x0;
    descEndpt1.bInterval = 0x0;

    descEndpt3.bLength = 0x7;    
    descEndpt3.bDescriptorType = ENDPOINT_TYPE;         
    descEndpt3.bEndpointAddress = 3 | EP_ADDR_OUT; /* EP3 OUT */  
    descEndpt3.bmAttributes = EP_ATTR_BULK;        /* bulk  */
    descEndpt3.wMaxPacketSizeL = EP3_PKT_SIZE;     /* 64 */
    descEndpt3.wMaxPacketSizeH = 0x0;
    descEndpt3.bInterval = 0x0;
}

