#include <config.h>
#include <common.h>
#include <mmc.h>
#include <asm/arch/mmc.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <s3c2410.h>
#include <part.h>
#include <fat.h>

#if defined(CONFIG_MMC) && defined(CONFIG_MMC_S3C)

static struct s3c2410_sdi *sdi;

static block_dev_desc_t mmc_dev;

block_dev_desc_t * mmc_get_dev(int dev)
{
	return ((block_dev_desc_t *)&mmc_dev);
}

static uchar mmc_buf[MMC_BLOCK_SIZE];
static int mmc_ready = 0;
static int wide = 0;

static u_int32_t *mmc_cmd(ushort cmd, ulong arg, ushort flags)
{
	static u_int32_t resp[5];

	u_int32_t ccon, csta;
	u_int32_t csta_rdy_bit = S3C2410_SDICMDSTAT_CMDSENT;

	memset(resp, 0, sizeof(resp));

	sdi->SDICSTA = 0xffffffff;  /* 清除状态寄存器中的值 */
	sdi->SDIDSTA = 0xffffffff;
	sdi->SDIFSTA = 0xffffffff;

	sdi->SDICARG = arg;

	ccon = cmd & S3C2410_SDICMDCON_INDEX;
	ccon |= S3C2410_SDICMDCON_SENDERHOST|S3C2410_SDICMDCON_CMDSTART;

	if (flags & CMD_F_RESP) {
		ccon |= S3C2410_SDICMDCON_WAITRSP;
		csta_rdy_bit = S3C2410_SDICMDSTAT_RSPFIN; /* 1 << 9 */
	}

	if (flags & CMD_F_RESP_LONG)
		ccon |= S3C2410_SDICMDCON_LONGRSP;

	sdi->SDICCON = ccon;

	while (1) {
		csta = sdi->SDICSTA;
		if (csta & csta_rdy_bit)
			break;
		if (csta & S3C2410_SDICMDSTAT_CMDTIMEOUT) {
			debug("[MMC CMD Timeout]\n");
			sdi->SDICSTA |= S3C2410_SDICMDSTAT_CMDTIMEOUT;
			return resp;
		}
	}

	sdi->SDICSTA |= csta_rdy_bit; /* 清理状态位 */

	if (flags & CMD_F_RESP) {
		resp[0] = sdi->SDIRSP0;
		resp[1] = sdi->SDIRSP1;
		resp[2] = sdi->SDIRSP2;
		resp[3] = sdi->SDIRSP3;
	}

	return resp;
}

#define FIFO_FILL(host) ((host->SDIFSTA & S3C2410_SDIFSTA_COUNTMASK) >> 2)

static int mmc_block_read(uchar *dst, ulong src, ulong len)
{
	u_int32_t dcon, fifo;
	u_int32_t *dst_u32 = (u_int32_t *)dst;
	u_int32_t *resp;

	if (len == 0)
		return 0;

	/* set block len */
	resp = mmc_cmd(MMC_CMD_SET_BLOCKLEN, len, CMD_F_RESP);
	sdi->SDIBSIZE = len;

	/* setup data */
	dcon = (len >> 9) & S3C2410_SDIDCON_BLKNUM;
	dcon |= S3C2410_SDIDCON_BLOCKMODE;
	dcon |= S3C2410_SDIDCON_RXAFTERCMD|S3C2410_SDIDCON_XFER_RXSTART;
	if (wide)
		dcon |= S3C2410_SDIDCON_WIDEBUS;
#if defined(CONFIG_S3C2440) || defined(CONFIG_S3C2442)
	dcon |= S3C2440_SDIDCON_DS_WORD | S3C2440_SDIDCON_DATSTART;
#endif
	sdi->SDIDCON = dcon;

	/* send read command */
	resp = mmc_cmd(MMC_CMD_READ_SINGLE_BLOCK, (mmc_dev.if_type == IF_TYPE_SDHC) ? 
                                               (src >> 9) : src, CMD_F_RESP);
	while (len > 0) {
		u_int32_t sdidsta = sdi->SDIDSTA;
		fifo = FIFO_FILL(sdi);
		if (sdidsta & (S3C2410_SDIDSTA_FIFOFAIL|
				S3C2410_SDIDSTA_CRCFAIL|
				S3C2410_SDIDSTA_RXCRCFAIL|
				S3C2410_SDIDSTA_DATATIMEOUT)) {
			printf("mmc_block_read: err SDIDSTA=0x%08x\n", sdidsta);
			return -EIO;
		}

		while (fifo--) {
			*(dst_u32++) = sdi->SDIDAT;
			if (len >= 4)
				len -= 4;
			else {
				len = 0;
				break;
			}
		}
	}
	while (!(sdi->SDIDSTA & (1 << 4))) {}
	sdi->SDIDCON = 0;

	if (!(sdi->SDIDSTA & S3C2410_SDIDSTA_XFERFINISH))
		debug("mmc_block_read; transfer not finished!\n");

	return 0;
}

int mmc_read(ulong src, uchar *dst, int size)
{
	ulong end, part_start, part_end, part_len, aligned_start, aligned_end;
	ulong mmc_block_size, mmc_block_address;

	if (size == 0)
		return 0;

	if (!mmc_ready) {
		printf("Please initialize the MMC first\n");
		return -1;
	}

	mmc_block_size = MMC_BLOCK_SIZE;
	mmc_block_address = ~(mmc_block_size - 1);

	end = src + size;
	part_start = ~mmc_block_address & src;
	part_end = ~mmc_block_address & end;
	aligned_start = mmc_block_address & src;
	aligned_end = mmc_block_address & end;

	/* all block aligned accesses */
	if (part_start) {
		part_len = mmc_block_size - part_start;
		if ((mmc_block_read(mmc_buf, aligned_start, mmc_block_size)) < 0)
			return -1;

		memcpy(dst, mmc_buf+part_start, part_len);
		dst += part_len;
		src += part_len;
	}
	for (; src < aligned_end; src += mmc_block_size, dst += mmc_block_size) {
		if ((mmc_block_read((uchar *)(dst), src, mmc_block_size)) < 0)
			return -1;
	}
	if (part_end && src < end) {
		if ((mmc_block_read(mmc_buf, aligned_end, mmc_block_size)) < 0)
			return -1;
		memcpy(dst, mmc_buf, part_end);
	}
	return 0;
}

ulong mmc_bread(int dev_num, ulong blknr, ulong blkcnt, void *dst)
{
	int mmc_block_size = MMC_BLOCK_SIZE;
	ulong src = blknr * mmc_block_size;

	mmc_read(src, dst, blkcnt*mmc_block_size);
	return blkcnt;
}

static u_int16_t rca;
static void print_sd_cid(const struct sd_cid *cid)
{
	printf("Manufacturer:       0x%02x, OEM \"%c%c\"\n",
	    cid->mid, cid->oid_0, cid->oid_1);
	printf("Product name:       \"%c%c%c%c%c\", revision %d.%d\n",
	    cid->pnm_0, cid->pnm_1, cid->pnm_2, cid->pnm_3, cid->pnm_4,
	    cid->prv >> 4, cid->prv & 15);
	printf("Serial number:      %u\n",
	    cid->psn_0 << 24 | cid->psn_1 << 16 | cid->psn_2 << 8 |
	    cid->psn_3);
	printf("Manufacturing date: %d/%d\n",
	    cid->mdt_1 & 15,
	    2000+((cid->mdt_0 & 15) << 4)+((cid->mdt_1 & 0xf0) >> 4));
	printf("CRC:                0x%02x, b0 = %d\n",
	    cid->crc >> 1, cid->crc & 1);
}

int mmc_legacy_init(int verbose)
{
 	int retries, rc = -ENODEV;
	int is_sd = 0;
	u_int32_t *resp;
	struct s3c24x0_clock_power * const clk_power = s3c24x0_get_base_clock_power();
	block_dev_desc_t *mmc_blkdev_p = &mmc_dev;

	sdi = s3c2410_get_base_sdi();
	clk_power->CLKCON |= (1 << 9);

	sdi->SDIBSIZE = MMC_BLOCK_SIZE;
#if defined(CONFIG_S3C2410)
	sdi->SDIPRE = 0x02;  /* 2410: SDCLK = PCLK/2 / (SDIPRE+1) = 11MHz */
	sdi->SDIDTIMER = 0xffff;
#elif defined(CONFIG_S3C2440) || defined(CONFIG_S3C2442)
	sdi->SDIPRE = 0x01;  /* 2440: SDCLK = PCLK / (SDIPRE+1) = 25MHz */
	sdi->SDIDTIMER = 0x7fffff;
#endif
	sdi->SDIIMSK = 0x0;   /* 屏蔽所有中断 */
	sdi->SDICON = S3C2410_SDICON_FIFORESET | S3C2410_SDICON_CLOCKTYPE;
	udelay(1000); /* 等待74个SDCLK使初始化完成 */

	/* reset */
	resp = mmc_cmd(MMC_CMD_GO_IDLE_STATE, 0, 0);

	mmc_dev.if_type = IF_TYPE_UNKNOWN;
	/* 发送CMD8，判断SD卡设备能否工作在主机提供的电压范围(2.7~3.6V) */
    resp = mmc_cmd(SD_CMD_SEND_IF_COND, ((1 << 8) | 0xAA), CMD_F_RESP_R7);
    if (!resp[0]) {
        /* 不应答CMD8，说明可能是SD设备(SD1.x) */
        mmc_blkdev_p->if_type = IF_TYPE_SD;
    } else {
        /* 应答CMD8，说明可能是SDHC设备(SD2.0 or later) */
        mmc_blkdev_p->if_type = IF_TYPE_SDHC;
        /* 检查SD卡设备是否支持主机提供的电压范围 */
        if (resp[0] != ((1 << 8) | 0xAA)) 
            return -ENODEV;
    }

	retries = 10;
	while (retries--) {
		udelay(10000);
		resp = mmc_cmd(MMC_CMD_APP_CMD, 0x0, CMD_F_RESP);
        /* 发送ACMD41，判断设备是否支持给定供电范围以及是否是SDHC设备 */
		resp = mmc_cmd(SD_CMD_APP_SEND_OP_COND, (mmc_blkdev_p->if_type == 
                       IF_TYPE_SDHC) ? MMC_VDD_32_33 | MMC_VDD_33_34 | OCR_HCS : 
                                       MMC_VDD_32_33 | MMC_VDD_33_34, 
                                       CMD_F_RESP);
		if (resp[0] & OCR_BUSY) {
			is_sd = 1;
			break;
		}
	}

	/* 通过HCS位，确定是SDHC还是SD设备 */
    if (is_sd && (resp[0] & OCR_HCS) != OCR_HCS) 
        mmc_dev.if_type = IF_TYPE_SD;

	if (retries == 0 && !is_sd) {
		printf("failed to detect SD Card. maybe card with non compatible "
               "voltage range, or maybe it's a MMC device not supported "
               "by this driver.\n");
        return rc;
	}

	/* 读取CID寄存器 */
	resp = mmc_cmd(MMC_CMD_ALL_SEND_CID, 0, CMD_F_RESP|CMD_F_RESP_LONG);
	if (resp) {
        struct sd_cid *cid = (struct sd_cid *) resp;
        if (verbose)
            print_sd_cid(cid);

        sprintf((char *) mmc_dev.vendor, "Man %02x OEM %c%c \"%c%c%c%c%c\"", 
                cid->mid, cid->oid_0, cid->oid_1, 
                cid->pnm_0, cid->pnm_1, cid->pnm_2, 
                cid->pnm_3, cid->pnm_4);
        sprintf((char *) mmc_dev.product, "%d",
                cid->psn_0 << 24 | cid->psn_1 << 16 |
                cid->psn_2 << 8 | cid->psn_3);
        sprintf((char *) mmc_dev.revision, "%d.%d",
                cid->prv >> 4, cid->prv & 15);

		/* 获取设备的RCA */
		resp = mmc_cmd(SD_CMD_SEND_RELATIVE_ADDR, 0x0, CMD_F_RESP);
		if (is_sd)
			rca = resp[0]>>16;

		/* 读取CSD寄存器  */
		resp = mmc_cmd(MMC_CMD_SEND_CSD, rca<<16, CMD_F_RESP|CMD_F_RESP_LONG);
		if (resp) {
			rc = 0;
			mmc_ready = 1;
            /* 接下来可以通过解析CSD寄存器获得SD卡容量等信息，此驱动从略 */
		}
	}

    /* 通过RCA选择当前操作的设备 */
    resp = mmc_cmd(MMC_CMD_SELECT_CARD, rca<<16, CMD_F_RESP);

#ifdef CONFIG_MMC_WIDE
	if (is_sd) {
		resp = mmc_cmd(MMC_CMD_APP_CMD, rca<<16, CMD_F_RESP);
		resp = mmc_cmd(SD_CMD_APP_SET_BUS_WIDTH, 0x02, CMD_F_RESP);
		wide = 1;
	}
#endif
	if (verbose) {
        printf("SD Card detected RCA: 0x%x type: %s\n", rca, 
               (mmc_dev.if_type == IF_TYPE_SDHC) ? "SDHC" : "SD");
    }

    mmc_dev.part_type = PART_TYPE_DOS;
    mmc_dev.blksz = MMC_BLOCK_SIZE;
    mmc_dev.block_read = mmc_bread;
    /* 注册FAT设备 */
	//fat_register_device(&mmc_dev, 1); 

	return rc;
}
#endif	

