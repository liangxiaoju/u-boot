#ifndef __TEST_H__
#define __TEST_H__

#include <asm/arch/s3c24x0_cpu.h>

static inline void ___delay(void)
{
	int i;

	for (i = 0x1000000; i > 0; i--)
		asm("nop");
}

static inline void led_on(uint8_t value)
{
	writel((readl(GPHCON) & (~3)) | (1), GPHCON);
	writel(readl(GPHPU) & (~3), GPHPU);
	writel((readl(GPHDAT) & (~1)) | !!value, GPHDAT);
}

static void led_flash(uint8_t value)
{
	int i;

	do {
		led_on(0);
		___delay();

		led_on(1);
		___delay();

	} while (--value);

	led_on(0);
	___delay();
}
#endif
