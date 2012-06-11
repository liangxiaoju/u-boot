/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002, 2010
 * David Mueller, ELSOFT AG, <d.mueller@elsoft.ch>
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
#include <netdev.h>
#include <mmc.h>
#include <asm/io.h>
#include <asm/arch/s3c24x0_cpu.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Miscellaneous platform dependent initialisations
 */

int board_early_init_f(void)
{
	return 0;
}

int board_init(void)
{
	/* arch number of SMDK2416-Board */
	gd->bd->bi_arch_number = MACH_TYPE_SMDK2416;

	/* adress of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;

	return 0;
}

int dram_init(void)
{
	/* dram_init must store complete ramsize in gd->ram_size */
	gd->ram_size = PHYS_SDRAM_1_SIZE;
	return 0;
}

#ifdef CONFIG_CMD_NET
int board_eth_init(bd_t *bis)
{
	return 0;
}
#endif

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	printf("Board:   BD2416\n");
	return 0;
}
#endif

#ifdef CONFIG_GENERIC_MMC
extern int s3c2416_mmc_init(int dev_index, int bus_width);
int board_mmc_init(bd_t *bis)
{
	unsigned int value;

	/* MMC0 GPIO */
	value = readl(GPEPU);
	value &= ~(0xfff<<10);
	writel(value, GPEPU);

	value = readl(GPECON);
	value &= ~(0xfff<<10);
	value |= 0xaaa<<10;
	writel(value, GPECON);

	/* enable MMC0 power */
	value = readl(GPBCON);
	value &= ~(0x2<<18);
	value |= 0x1<<18;
	writel(value, GPBCON);

	value = readl(GPBDAT);
	value &= ~(0x1<<9);
	writel(value, GPBDAT);

	/* MMC1 GPIO */
	value = readl(GPLPU);
	value &= ~((0xf<<16) | 0xff);
	writel(value, GPLPU);

	value = readl(GPLCON);
	value &= ~((0xf<<16) | 0xff);
	value |= (0xa<<16) | 0xaa;
	writel(value, GPLCON);

	/* MMC CLK */
	writel(readl(CLKSRCCON) & (~(0x3<<16)), CLKSRCCON);
	writel(readl(CLKDIV1CON) & (~(0x3<<6)), CLKDIV1CON);
	writel(readl(CLKDIV2CON) & (~(0x3<<6)), CLKDIV2CON);
	writel(readl(HCLKCON) | 0x3<<15, HCLKCON);
	writel(readl(SCLKCON) | 1<<6 | 1<<12, SCLKCON);

	s3c2416_mmc_init(0, 4);
	s3c2416_mmc_init(1, 4);
	/* we make a virtual mmc here for convenience */
	s3c2416_mmc_init(2, 4);

	return 0;
}

int board_mmc_getcd(struct mmc *mmc)
{
	unsigned int value;
	int dev_index = mmc->block_dev.dev;

	if (dev_index == 1) {
		/* switch to mmc 1 */
		value = readl(GPBCON);
		value &= ~(0x2<<20);
		value |= 0x1<<20;
		writel(value, GPBCON);

		value = readl(GPBDAT);
		value &= ~(0x1<<10);
		writel(value, GPBDAT);
	} else if (dev_index == 2) {
		/* switch to mmc 2 */
		value = readl(GPBCON);
		value &= ~(0x2<<20);
		value |= 0x1<<20;
		writel(value, GPBCON);

		value = readl(GPBDAT);
		value |= 0x1<<10;
		writel(value, GPBDAT);
	}

	return 1;
}
#endif
