#ifndef __ARCH_MMC_H__
#define __ARCH_MMC_H__

#include <asm/arch/regs-sdi.h>

#define MMC_BLOCK_SIZE			512

#define CMD_F_RESP	0x01
#define CMD_F_RESP_LONG	0x02
#define CMD_F_RESP_R7 CMD_F_RESP

struct sd_cid {
	char		pnm_0;	/* product name */
	char		oid_1;	/* OEM/application ID */
	char		oid_0;
	uint8_t		mid;	/* manufacturer ID */
	char		pnm_4;
	char		pnm_3;
	char		pnm_2;
	char		pnm_1;
	uint8_t		psn_2;	/* product serial number */
	uint8_t		psn_1;
	uint8_t		psn_0;	
	uint8_t		prv;	/* product revision */
	uint8_t		crc;	/* CRC7 checksum, b0 is unused and set to 1 */
	uint8_t		mdt_1;	
	uint8_t		mdt_0;	
	uint8_t		psn_3;	
};

#endif /* __ARCH_MMC_H__ */
