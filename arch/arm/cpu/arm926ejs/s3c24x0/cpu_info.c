/*
 * (C) Copyright 2010
 * David Mueller <d.mueller@elsoft.ch>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/s3c24x0_cpu.h>

ulong get_MSYSCLK(void);
ulong get_ESYSCLK(void);
ulong get_ARMCLK(void);
ulong get_HCLK(void);
ulong get_PCLK(void);

typedef ulong (*getfreq)(void);

struct cpu_freq {
	const char *name;
	getfreq get_freq;
};

struct cpu_freq freq[] = {
	{"MSysClk", get_MSYSCLK},
	{"ESysClk", get_ESYSCLK},
	{"ArmClk", get_ARMCLK},
	{"Hclk", get_HCLK},
	{"Pclk", get_PCLK},
};

int print_cpuinfo(void)
{
	int i;
	char buf[32];
	ulong cpuid;

	cpuid = readl(GSTATUS1);
	printf("%-8s: %8lX\n", "CPUID", cpuid);
	for (i = 0; i < ARRAY_SIZE(freq); i++)
		printf("%-8s: %8s MHz\n", freq[i].name, strmhz(buf, freq[i].get_freq()));
	printf("\n");

	return 0;
}
