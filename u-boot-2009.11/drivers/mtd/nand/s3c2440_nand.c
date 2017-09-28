/*
 *  drivers/mtd/nand/s3c2440_nand.c
 *  Copyright (C) 2009-2012 Richard Fan (Guo Qian)
 *  
 *  s3c2410/s3c2440 nand flash driver, available for 
 *  SLC 512/1024/2048 bytes page size.
 */
#include <common.h>

#include <nand.h>
#include <s3c2410.h>
#include <asm/io.h>

#include <asm/errno.h>

#define NFCONT_EN          (1<<0)
#define NFCONT_INITECC     (1<<4)
#define NFCONT_nFCE        (1<<1)
#define NFCONT_MAINECCLOCK (1<<5)
#define NFCONF_TACLS(x)    ((x)<<12)
#define NFCONF_TWRPH0(x)   ((x)<<8)
#define NFCONF_TWRPH1(x)   ((x)<<4)

/* 页大小为512B的SLC-NandFlash oob区布局情况 */
static struct nand_ecclayout s3c_nand_oob_16 = {
	.eccbytes = 4,
	.eccpos = {1, 2, 3, 4},
	.oobfree = { {.offset = 6, .length = 10} }
};

/* 页大小为2KB的SLC-NandFlash oob区布局情况 */
static struct nand_ecclayout s3c_nand_oob_64 = {
	.eccbytes = 16,
	.eccpos = {40, 41, 42, 43, 44, 45, 46, 47,
		   48, 49, 50, 51, 52, 53, 54, 55},
	.oobfree = { {.offset = 2,  .length = 38},
                 {.offset = 56, .length = 8} }
};

static void s3c_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct s3c2410_nand *nand = s3c2410_get_base_nand();

	if (ctrl & NAND_CTRL_CHANGE) {
		if (ctrl & NAND_NCE)
			writel(readl(&nand->NFCONT) & ~NFCONT_nFCE,
			       &nand->NFCONT);
		else
			writel(readl(&nand->NFCONT) | NFCONT_nFCE,
			       &nand->NFCONT);
	}

	if (cmd != NAND_CMD_NONE) {
       	if (ctrl & NAND_CLE)
			writeb(cmd, &nand->NFCMD);
		else if (ctrl & NAND_ALE)
			writeb(cmd, &nand->NFADDR);
    }
}

static int s3c_dev_ready(struct mtd_info *mtd)
{
	struct s3c2410_nand *nand = s3c2410_get_base_nand();
	return readl(&nand->NFSTAT) & 0x01;
}

#ifdef CONFIG_NAND_HWECC
void s3c_nand_enable_hwecc(struct mtd_info *mtd, int mode)
{
	struct s3c2410_nand *nand = s3c2410_get_base_nand();

	writel(readl(&nand->NFCONT) | NFCONT_INITECC, &nand->NFCONT);
	writel(readl(&nand->NFCONT) & ~NFCONT_MAINECCLOCK, &nand->NFCONT);
}

static int s3c_nand_calculate_ecc(struct mtd_info *mtd, const u_char *dat,
				      u_char *ecc_code)
{
    ulong nfecc;
  	struct s3c2410_nand *nand = s3c2410_get_base_nand();

    writel(readl(&nand->NFCONT)|(1<<5), &nand->NFCONT); /* MECC Lock */
    
    nfecc = readl(&nand->NFMECC0);
    ecc_code[0] = nfecc & 0xff;
    ecc_code[1] = (nfecc >> 8) & 0xff;
    ecc_code[2] = (nfecc >> 16) & 0xff;
    ecc_code[3] = (nfecc >> 24) & 0xff;

	return 0;
}

static int s3c_nand_correct_data(struct mtd_info *mtd, u_char *dat,
				     u_char *read_ecc, u_char *calc_ecc)
{
    int ret = -1;
    u_long nfestat0, nfmeccdata0, nfmeccdata1;
	u_char err_type;
  	struct s3c2410_nand *nand = s3c2410_get_base_nand();

    /* Write ecc to compare */
    nfmeccdata0 = (read_ecc[1] << 16) | read_ecc[0];
    nfmeccdata1 = (read_ecc[3] << 16) | read_ecc[2];
    writel(nfmeccdata0, &nand->NFMECCD0);
    writel(nfmeccdata1, &nand->NFMECCD1);

    /* Read ecc status */
    nfestat0 = readl(&nand->NFESTAT0);
    err_type = nfestat0 & 0x3;

    switch (err_type) {
        case 0: /* No error */
            ret = 0;
			break;
		case 1: 
            /* 
             * 1 bit error (Correctable)
             * (nfestat0 >> 7) & 0x7ff	:error byte number
             * (nfestat0 >> 4) & 0x7	:error bit number
             */
			printk("s3c-nand: 1 bit error detected at byte %ld, correcting from "
					"0x%02x ", (nfestat0 >> 7) & 0x7ff, dat[(nfestat0 >> 7) & 0x7ff]);
			dat[(nfestat0 >> 7) & 0x7ff] ^= (1 << ((nfestat0 >> 4) & 0x7));
			printk("to 0x%02x...OK\n", dat[(nfestat0 >> 7) & 0x7ff]);
			ret = 1;
			break;
		case 2: /* Multiple error */
		case 3: /* ECC area error */
			printk("s3c-nand: ECC uncorrectable error detected\n");
			ret = -1;
			break;
		}
    return ret;
}

static int s3c_nand_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
			      int page)
{
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	int status = 0;
	int eccbytes = chip->ecc.bytes;
	int secc_start = mtd->oobsize - eccbytes;
	int i;

	chip->cmdfunc(mtd, NAND_CMD_SEQIN, mtd->writesize, page);

	/* spare area */
	chip->ecc.hwctl(mtd, NAND_ECC_WRITE);
	chip->write_buf(mtd, chip->oob_poi, secc_start);
	chip->ecc.calculate(mtd, 0, &ecc_calc[chip->ecc.total]);

	for (i = 0; i < eccbytes; i++)
		chip->oob_poi[secc_start + i] = ecc_calc[chip->ecc.total + i];

	chip->write_buf(mtd, chip->oob_poi + secc_start, eccbytes);

	/* Send command to program the OOB data */
	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);

	status = chip->waitfunc(mtd, chip);

	return status & NAND_STATUS_FAIL ? -EIO : 0;
}

static int s3c_nand_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
			     int page, int sndcmd)
{
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	int eccbytes = chip->ecc.bytes;
	int secc_start = mtd->oobsize - eccbytes;
	
	if (sndcmd) {
		chip->cmdfunc(mtd, NAND_CMD_READOOB, 0, page);
		sndcmd = 0;
	}

	chip->ecc.hwctl(mtd, NAND_ECC_READ);
	chip->read_buf(mtd, chip->oob_poi, secc_start);
	chip->ecc.calculate(mtd, 0, &ecc_calc[chip->ecc.total]);
	chip->read_buf(mtd, chip->oob_poi + secc_start, eccbytes);
    chip->ecc.correct(mtd, chip->oob_poi, chip->oob_poi + secc_start, 0);

	return sndcmd;
}

static void s3c_nand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
				  const uint8_t *buf)
{
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	int secc_start = mtd->oobsize - eccbytes;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	const uint8_t *p = buf;
	
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	/* main area */
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->ecc.hwctl(mtd, NAND_ECC_WRITE);
		chip->write_buf(mtd, p, eccsize);
		chip->ecc.calculate(mtd, p, &ecc_calc[i]);
	}

	for (i = 0; i < chip->ecc.total; i++)
		chip->oob_poi[eccpos[i]] = ecc_calc[i];

	/* spare area */
	chip->ecc.hwctl(mtd, NAND_ECC_WRITE);
	chip->write_buf(mtd, chip->oob_poi, secc_start);
	chip->ecc.calculate(mtd, p, &ecc_calc[chip->ecc.total]);

	for (i = 0; i < eccbytes; i++)
		chip->oob_poi[secc_start + i] = ecc_calc[chip->ecc.total + i];

	chip->write_buf(mtd, chip->oob_poi + secc_start, eccbytes);
}

static int s3c_nand_read_page(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, int page)
{
	int i, stat, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	int secc_start = mtd->oobsize - eccbytes;  /* spare area ecc放在oob区的末尾 */
	int col = 0;
	uint8_t *p = buf;	
	uint32_t *mecc_pos = chip->ecc.layout->eccpos;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint8_t *ecc_code = chip->buffers->ecccode;

	col = mtd->writesize;
	chip->cmdfunc(mtd, NAND_CMD_RNDOUT, col, -1);

	/* spare area */
	chip->ecc.hwctl(mtd, NAND_ECC_READ);
	chip->read_buf(mtd, chip->oob_poi, secc_start);
	chip->ecc.calculate(mtd, p, &ecc_calc[chip->ecc.total]); /* secc放在mecc之后*/
	chip->read_buf(mtd, chip->oob_poi + secc_start, eccbytes);
    chip->ecc.correct(mtd, chip->oob_poi, chip->oob_poi + secc_start, 0);

	for (i = 0; i < chip->ecc.total; i++)
		ecc_code[i] = chip->oob_poi[mecc_pos[i]];

	col = 0;
	/* main area */
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, col, -1);
		chip->ecc.hwctl(mtd, NAND_ECC_READ);
		chip->read_buf(mtd, p, eccsize);
		chip->ecc.calculate(mtd, p, &ecc_calc[i]);
        
		stat = chip->ecc.correct(mtd, p, &ecc_code[i], 0);
		if (stat == -1)
			mtd->ecc_stats.failed++;
		else
			mtd->ecc_stats.corrected += stat;

		col = eccsize * (chip->ecc.steps + 1 - eccsteps);
	}
	return 0;
}
#endif /* #ifdef CONFIG_NAND_HWECC */

int board_nand_init(struct nand_chip *nand)
{
	u_int32_t cfg;
	u_int8_t tacls, twrph0, twrph1;
	struct s3c24x0_clock_power *clk_power = s3c24x0_get_base_clock_power();
	struct s3c2410_nand *nand_reg = s3c2410_get_base_nand();

	writel(readl(&clk_power->CLKCON) | (1 << 4), &clk_power->CLKCON);
	
	/* initialize hardware */
	twrph0 = 2;
	twrph1 = 1;
	tacls = 1;
	
	cfg = 0;	
	cfg |= NFCONF_TACLS(tacls - 1);
	cfg |= NFCONF_TWRPH0(twrph0 - 1);
	cfg |= NFCONF_TWRPH1(twrph1 - 1);
	writel(cfg, &nand_reg->NFCONF);

	cfg = (1<<4) | (0<<1) | (1<<0);
	writel(cfg, &nand_reg->NFCONT);

	/* initialize nand_chip data structure */
	nand->IO_ADDR_R = nand->IO_ADDR_W = (void *)&nand_reg->NFDATA;
	nand->cmd_ctrl = s3c_hwcontrol;
	nand->dev_ready = s3c_dev_ready;

#ifdef CONFIG_NAND_HWECC
	nand->ecc.mode = NAND_ECC_HW;
	
    nand->ecc.hwctl = s3c_nand_enable_hwecc;
	nand->ecc.calculate = s3c_nand_calculate_ecc;
	nand->ecc.correct = s3c_nand_correct_data;

    nand->ecc.size = 512;
    nand->ecc.bytes = 4;

    nand->ecc.read_page = s3c_nand_read_page;
    nand->ecc.write_page = s3c_nand_write_page;
    nand->ecc.read_oob = s3c_nand_read_oob;
    nand->ecc.write_oob = s3c_nand_write_oob;  

	nand->cmd_ctrl(0, NAND_CMD_READID, NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
	nand->cmd_ctrl(0, 0x00, NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE);
	nand->dev_ready(0);

    readb(nand->IO_ADDR_R);
    if (readb(nand->IO_ADDR_R) > 0x80)
        nand->ecc.layout = &s3c_nand_oob_64;
    else 
        nand->ecc.layout = &s3c_nand_oob_16;
#else
	nand->ecc.mode = NAND_ECC_SOFT;
#endif

#ifdef CONFIG_NAND_BBT
	nand->options = NAND_USE_FLASH_BBT;
#else
	nand->options = NAND_SKIP_BBTSCAN;
#endif
	return 0;
}

