/*
 * (C) Copyright 2001-2004
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2002
 * David Mueller, ELSOFT AG, d.mueller@elsoft.ch
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

/* This code should work for both the S3C2400 and the S3C2410
 * as they seem to have the same PLL and clock machinery inside.
 * The different address mapping is handled by the s3c24xx.h files below.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/s3c24x0_cpu.h>

#define MPLL 0
#define EPLL 1

static ulong get_PLLCLK(int pllreg)
{
	ulong r, m, p, s;
	ulong clk;

	if (pllreg == MPLL)
		r = readl(MPLLCON);
	else if (pllreg == EPLL)
		r = readl(EPLLCON);
	else {
		for (;;) ;
	}

	m = (r >> 14) & 0x3ff;
	p = (r >> 5) & 0x3f;
	s = r & 0x7;

	/* XXX avoid to overflow */
	clk = m * (CONFIG_SYS_CLK_FREQ / (p << s));

	return clk;
}

ulong get_MSYSCLK(void)
{
	return get_PLLCLK(MPLL);
}

ulong get_ARMCLK(void)
{
	uint div;
	ulong msysclk;

	msysclk = get_MSYSCLK();

	div = readl(CLKDIV0CON);
	div = (div >> 9) & 0x7;

	return msysclk / (div + 1);
}

ulong get_FCLK(void)
{
	return get_MSYSCLK();
}

/* return HCLK frequency */
ulong get_HCLK(void)
{
	uint hclk_div = readl(CLKDIV0CON) & 0x3;
	uint pre_div = (readl(CLKDIV0CON) >> 4) & 0x3;

	return get_MSYSCLK() / ((hclk_div + 1) * (pre_div + 1));
}

/* return PCLK frequency */
ulong get_PCLK(void)
{
	return (readl(CLKDIV0CON) & (1<<2)) ? get_HCLK() / 2 : get_HCLK();
}

/* return ESYSCLK frequency */
ulong get_ESYSCLK(void)
{
	return get_PLLCLK(EPLL);
}
