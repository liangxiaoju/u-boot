#include <common.h>
#include <asm/io.h>
#include <asm/arch/s3c6400.h>

typedef uint8_t (*CopyMoviToMem_t)(int, uint32_t, uint16_t, uint32_t *, int);
typedef int32_t (*CopyNandToMem_t)(uint32_t, uint32_t, uint8_t *);

#define	TCM_BASE		0x0C004000

#define CopyMoviToMem(channel, startblkaddr, blocksize, buf, reinit)	\
({																		\
	CopyMoviToMem_t ptr = (void *)(TCM_BASE + 0x08);					\
	ptr(channel, startblkaddr, blocksize, buf, reinit);					\
})

#define CopyNandToMem(block, page, buf)									\
({																		\
 	CopyNandToMem_t ptr = (void *)(TCM_BASE + 0x04);					\
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
#define MOVI_INIT_REQUIRED	1


void board_init_f(unsigned long bootflag)
{
	relocate_code(CONFIG_SYS_INIT_SP_ADDR, NULL, CONFIG_SPL_TEXT_BASE);
}

void boot_from_irom(void)
{
	volatile u32 *mmc_control4;
	__attribute__((noreturn)) void (*uboot)(void);

	mmc_control4 = (volatile u32 *)0x7C20008C;
	writel(readl(mmc_control4) | (0x3 << 16), mmc_control4);

	CopyMoviToMem(HSMMC_CHANNEL, MOVI_UBOOT_POS,
			UBOOT_BLKCNT, (uint32_t *)CONFIG_SYS_MMC_U_BOOT_START,
			MOVI_INIT_REQUIRED);

	uboot = (void *)CONFIG_SYS_MMC_U_BOOT_START;
	(*uboot)();
}

void boot_from_nand(void)
{
	int i;
	__attribute__((noreturn)) void (*uboot)(void);

	for (i = 0; i < (UBOOT_BLKCNT * 512 / CONFIG_SYS_NAND_PAGE_SIZE); i++) {
		CopyNandToMem(0, (8 * 1024 / CONFIG_SYS_NAND_PAGE_SIZE) + 1 + i,
				(uint8_t *)(CONFIG_SYS_MMC_U_BOOT_START + i * CONFIG_SYS_NAND_PAGE_SIZE));
	}

	uboot = (void *)CONFIG_SYS_MMC_U_BOOT_START;
	(*uboot)();
}

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
