#include <common.h>
#include <nand.h>
#include <asm/io.h>
#include <asm/arch/s3c6400.h>

typedef uint8_t (*CopyMoviToMem_t)(int, uint32_t, uint16_t, uint32_t *, int);
typedef int32_t (*CopyNandToMem_t)(uint32_t, uint32_t, uint8_t *);

#define	TCM_BASE		0x0C004000

#define CopyMoviToMem(channel, startblkaddr, blocksize, buf, reinit)	\
({																		\
	CopyMoviToMem_t ptr = (void *)(*((uint32_t *)(TCM_BASE + 0x08)));	\
	ptr(channel, startblkaddr, blocksize, buf, reinit);					\
})

#define CopyNandToMem(block, page, buf)									\
({																		\
 	CopyNandToMem_t ptr = (void *)(*((uint32_t *)(TCM_BASE + 0x04)));	\
 	ptr(block, page, buf);												\
})

#define HSMMC_CHANNEL 0

#define CONFIG_SYS_MMC_U_BOOT_START CONFIG_SYS_PHY_UBOOT_BASE

#define MOVI_TOTAL_BLKCNT	*((volatile unsigned int*)(TCM_BASE - 0x4))
#define MOVI_HIGH_CAPACITY	*((volatile unsigned int*)(TCM_BASE - 0x8))

#define SS_SIZE			(8 * 1024)
#define eFUSE_SIZE		(1 * 1024)	// 0.5k eFuse, 0.5k reserved
#define MOVI_BLKSIZE	512

/*
 * XXX Define the u-boot.bin's largest block size,
 * here we assume the u-boot.bin image is less than 512k.
 */
#define UBOOT_BLKCNT		((512)*1024/512)
#define MOVI_LAST_BLKPOS	(MOVI_TOTAL_BLKCNT - (eFUSE_SIZE / MOVI_BLKSIZE))
#define MOVI_UBOOT_POS		(MOVI_LAST_BLKPOS - UBOOT_BLKCNT)
#define MOVI_INIT_REQUIRED	0

void led_on(uint8_t value)
{
	writel((readl(GPKCON0) & 0x0000ffff) | 0x11110000, GPKCON0);
	writel(readl(GPKPUD) & 0xffff00ff, GPKPUD);
	writel((readl(GPKDAT) & 0xffffff0f) | ((~value & 0xf) << 4), GPKDAT);
}

void boot_from_irom(void)
{
	volatile u32 *mmc_control4;
	__attribute__((noreturn)) void (*uboot)(void);

	led_on(1<<0);

	mmc_control4 = (volatile u32 *)(0x7C20008C + HSMMC_CHANNEL * 0x100000);
	writel(readl(mmc_control4) | (0x3 << 16), mmc_control4);

	CopyMoviToMem(HSMMC_CHANNEL, MOVI_UBOOT_POS,
			UBOOT_BLKCNT, (uint32_t *)CONFIG_SYS_MMC_U_BOOT_START,
			MOVI_INIT_REQUIRED);

	uboot = (void *)CONFIG_SYS_MMC_U_BOOT_START;
	(*uboot)();
}

#if 0
void boot_from_nand(void)
{
	int i;
	__attribute__((noreturn)) void (*uboot)(void);

	for (i = 0; i < (UBOOT_BLKCNT * 512 / CONFIG_SYS_NAND_PAGE_SIZE); i++) {
		CopyNandToMem(i / CONFIG_SYS_NAND_PAGE_COUNT,
				((8 * 1024 / CONFIG_SYS_NAND_PAGE_SIZE) + 1 + i) % CONFIG_SYS_NAND_PAGE_COUNT,
				(uint8_t *)(CONFIG_SYS_MMC_U_BOOT_START + i * CONFIG_SYS_NAND_PAGE_SIZE));
	}

	uboot = (void *)CONFIG_SYS_MMC_U_BOOT_START;
	(*uboot)();
}
#else
void boot_from_nand(void)
{
	led_on(1<<1);
	nand_init();
	nand_boot();
}
#endif

void reserved(void)
{
	while (1) ;
}

void spl_boot(void)
{
	u32 status;

	status = MEM_CFG_STAT_REG;

	switch ((status >> 5) & 0x03) {
		case 0x00:
			boot_from_nand();
			break;
		case 0x01:
			reserved();
			break;
		case 0x02:
			reserved();
			break;
		case 0x03:
			boot_from_irom();
			break;
	}

}

void board_init_f(unsigned long bootflag)
{
	spl_boot();
	while (1);
}

void board_init_r(gd_t *id, ulong dest_addr)
{
	while (1);
}

