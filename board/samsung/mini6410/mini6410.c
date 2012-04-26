/*
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * David Mueller, ELSOFT AG, <d.mueller@elsoft.ch>
 *
 * (C) Copyright 2008
 * Guennadi Liakhovetki, DENX Software Engineering, <lg@denx.de>
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
#include <asm/arch/s3c6400.h>
#include <asm/io.h>
#include <usb/s3c_udc.h>

DECLARE_GLOBAL_DATA_PTR;

/* ------------------------------------------------------------------------- */
#define CS8900_Tacs	0x0	/* 0clk		address set-up		*/
#define CS8900_Tcos	0x4	/* 4clk		chip selection set-up	*/
#define CS8900_Tacc	0xE	/* 14clk	access cycle		*/
#define CS8900_Tcoh	0x1	/* 1clk		chip selection hold	*/
#define CS8900_Tah	0x4	/* 4clk		address holding time	*/
#define CS8900_Tacp	0x6	/* 6clk		page mode access cycle	*/
#define CS8900_PMC	0x0	/* normal(1data)page mode configuration	*/

static inline void delay(unsigned long loops)
{
	__asm__ volatile ("1:\n" "subs %0, %1, #1\n"
			  "bne 1b"
			  : "=r" (loops) : "0" (loops));
}

/*
 * Miscellaneous platform dependent initialisations
 */

static void cs8900_pre_init(void)
{
	SROM_BW_REG &= ~(0xf << 4);
	SROM_BW_REG |= (1 << 7) | (1 << 6) | (1 << 4);
	SROM_BC1_REG = ((CS8900_Tacs << 28) + (CS8900_Tcos << 24) +
			(CS8900_Tacc << 16) + (CS8900_Tcoh << 12) +
			(CS8900_Tah << 8) + (CS8900_Tacp << 4) + CS8900_PMC);
}

static int s3c_otg_phy_control(int on)
{
	if (on)
		writel(readl(OTHERS) | (1<<16), OTHERS);
	else
		writel(readl(OTHERS) & (~(1<<16)), OTHERS);

	return 0;
}

static struct s3c_plat_otg_data s3c_otg_pdata = {
	.phy_control	= s3c_otg_phy_control,
	.regs_phy		= 0x7c100000,
	.regs_otg		= 0x7c000000,
	.usb_phy_ctrl	= 0x7c000e00,
	.usb_flags		= 0,
};

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
	return s3c_udc_probe(&s3c_otg_pdata);
}
#endif

int board_init(void)
{
	cs8900_pre_init();

	/* NOR-flash in SROM0 */

	/* Enable WAIT */
	SROM_BW_REG |= 4 | 8 | 1;

	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;

	return 0;
}

void dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;
}

int dram_init(void)
{
	gd->ram_size = get_ram_size((long *)CONFIG_SYS_SDRAM_BASE,
				PHYS_SDRAM_1_SIZE);

	return 0;
}

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	printf("Board:   MINI6410\n");
	return 0;
}
#endif

#ifdef CONFIG_ENABLE_MMU
ulong virt_to_phy_smdk6400(ulong addr)
{
	if ((0xc0000000 <= addr) && (addr < 0xc8000000))
		return addr - 0xc0000000 + 0x50000000;
	else
		printf("do not support this address : %08lx\n", addr);

	return addr;
}
#endif

#ifdef CONFIG_CMD_NET
int board_eth_init(bd_t *bis)
{
	int rc = 0;
#ifdef CONFIG_CS8900
	rc = cs8900_initialize(0, CONFIG_CS8900_BASE);
#endif
	return rc;
}
#endif

#ifdef CONFIG_GENERIC_MMC
extern int s3c6410_mmc_init(int, int);
int board_mmc_init(bd_t *bis)
{
	int ret = 0;

	writel(0x0, GPGPUD);
	writel(0x02222222, GPGCON);

	ret = s3c6410_mmc_init(0, 4);

	return ret;
}
#endif
