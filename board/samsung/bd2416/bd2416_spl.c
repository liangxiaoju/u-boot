#include <common.h>
#include <nand.h>
#include <asm/io.h>
#include <asm/arch/s3c24x0_cpu.h>

typedef int32_t (*CopyMoviToMem_t)(uint32_t, uint16_t, uint32_t *, uint32_t, int32_t);
typedef int32_t (*CopyNandToMem_t)(uint32_t, uint32_t, uint8_t *);

#define	TCM_BASE		0x40004000

#define CopyMoviToMem(startblkaddr, blocksize, buf, extclk, reinit)		\
({																		\
	CopyMoviToMem_t ptr = (void *)(*((uint32_t *)(TCM_BASE + 0x08)));	\
	ptr(startblkaddr, blocksize, buf, extclk, reinit);					\
})

#define CopyNandToMem(block, page, buf)									\
({																		\
 	CopyNandToMem_t ptr = (void *)(*((uint32_t *)(TCM_BASE + 0x00)));	\
 	ptr(block, page, buf);												\
})

#define CopyNandToMemAdv(block, page, buf)								\
({																		\
 	CopyNandToMem_t ptr = (void *)(*((uint32_t *)(TCM_BASE + 0x04)));	\
 	ptr(block, page, buf);												\
})

#define CONFIG_SYS_MMC_U_BOOT_START CONFIG_SYS_PHY_UBOOT_BASE

#define MOVI_TOTAL_BLKCNT	*((volatile unsigned int*)(TCM_BASE - 0x4))
#define MOVI_HIGH_CAPACITY	*((volatile unsigned int*)(TCM_BASE - 0x8))

#define SS_SIZE			(8 * 1024)
#define eFUSE_SIZE		(1 * 1024)	// 0.5k eFuse, 0.5k reserved
#define MOVI_BLKSIZE	512

#define UBOOT_BLKCNT		(CONFIG_SYS_UBOOT_SIZE / 512)
#define MOVI_LAST_BLKPOS	(MOVI_TOTAL_BLKCNT - (eFUSE_SIZE / MOVI_BLKSIZE))
#define MOVI_UBOOT_POS		(MOVI_LAST_BLKPOS - UBOOT_BLKCNT - (SS_SIZE / MOVI_BLKSIZE))
#define MOVI_INIT_REQUIRED	1

void boot_from_movi(void)
{
	__attribute__((noreturn)) void (*uboot)(void);

	CopyMoviToMem(MOVI_UBOOT_POS,
			UBOOT_BLKCNT, (uint32_t *)CONFIG_SYS_MMC_U_BOOT_START,
			0, MOVI_INIT_REQUIRED);

	uboot = (void *)CONFIG_SYS_MMC_U_BOOT_START;

	(*uboot)();
}

void reserved(void)
{
	while (1) ;
}

void spl_boot(void)
{
	boot_from_movi();
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

