#include <config.h>

#define S3C24X0_UART_BASE		0x50000000
#define S3C24X0_UART_OFFSET     (0x4000)

#if defined(CONFIG_SERIAL1)
    #define UART_CONSOLE_BASE S3C24X0_UART_BASE
#elif defined(CONFIG_SERIAL2)
    #define UART_CONSOLE_BASE (S3C24X0_UART_BASE + S3C24X0_UART_OFFSET)
#elif defined(CONFIG_SERIAL3)
    #define UART_CONSOLE_BASE (S3C24X0_UART_BASE + 2*S3C24X0_UART_OFFSET)
#else
    #define UART_CONSOLE_BASE S3C24X0_UART_BASE
#endif

#define S3C2410_UTXH	  (0x20)
#define S3C2410_ULCON	  (0x00)
#define S3C2410_UCON	  (0x04)
#define S3C2410_UFCON	  (0x08)
#define S3C2410_UMCON	  (0x0C)
#define S3C2410_UBRDIV	  (0x28)
#define S3C2410_UTRSTAT	  (0x10)

#define S3C2410_UTRSTAT_TXFE	  (1<<1)

	.macro addruart, rx
		ldr	\rx, = UART_CONSOLE_BASE
	.endm

	.macro	senduart,rd,rx
		strb 	\rd, [\rx, # S3C2410_UTXH ]
	.endm

	.macro	busyuart, rd, rx
1001:
		@ busy waiting for non fifo
		ldr	\rd, [ \rx, # S3C2410_UTRSTAT ]
		tst	\rd, #S3C2410_UTRSTAT_TXFE
		beq	1001b
	.endm

	.macro	waituart,rd,rx
1001:
		@ idle waiting for non fifo
		ldr	\rd, [ \rx, # S3C2410_UTRSTAT ]
		tst	\rd, #S3C2410_UTRSTAT_TXFE
		beq	1001b
	.endm
