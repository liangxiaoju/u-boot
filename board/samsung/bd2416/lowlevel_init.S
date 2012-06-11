#include <config.h>
#include <version.h>
#include <asm/arch/s3c24x0_cpu.h>

#if defined(CONFIG_CLK_400_133_66)
#define	Startup_MDIV		400
#define	Startup_PDIV		3
#define	Startup_SDIV		1
#define Startup_EPLLSTOP	0
#define	Startup_ARMCLKdiv	1
#define	Startup_PREdiv		0x2
#define	Startup_HCLKdiv		0x1
#define	Startup_PCLKdiv		1
#elif defined (CONFIG_CLK_534_133_66)
#define	Startup_MDIV		267
#define	Startup_PDIV		3
#define	Startup_SDIV		1
#define Startup_EPLLSTOP	0
#define	Startup_ARMCLKdiv	0
#define	Startup_PREdiv		0x1
#define	Startup_HCLKdiv		0x1
#define	Startup_PCLKdiv		1
#elif defined (CONFIG_CLK_267_133_66)
#define	Startup_MDIV		267
#define	Startup_PDIV		3
#define	Startup_SDIV		1
#define Startup_EPLLSTOP	0
#define	Startup_ARMCLKdiv	1
#define	Startup_PREdiv		0x1
#define	Startup_HCLKdiv		0x1
#define	Startup_PCLKdiv		1
#else
# error Must define CONFIG_CLK_*
#endif

#define CLK_DIV_VAL	((Startup_ARMCLKdiv<<9)|(Startup_PREdiv<<4)|(Startup_PCLKdiv<<2)|(Startup_HCLKdiv)|(1<<3))
#define MPLL_VAL	((Startup_EPLLSTOP<<24)|(Startup_MDIV<<14)|(Startup_PDIV<<5)|(Startup_SDIV))
#define EPLL_VAL	(32<<16)|(1<<8)|(3<<0)

#define	CFG_BANK_CFG_VAL		0x0006920d
#define	CFG_BANK_CON1_VAL		0x04000040
#define	CFG_BANK_CFG_VAL_ALT	0x00048904
#define	CFG_BANK_CON1_VAL_ALT	0x04000040
#define CFG_BANK_CFG_VAL_DDR2	0x00049253
#define CFG_BANK_CON1_VAL_DDR2	0x44000040

#define	CFG_BANK_CON2_VAL		0x005b0035
#define CFG_BANK_CON2_VAL_DDR2	0x005D0035
#define	CFG_BANK_CON3_VAL		0x80000030
#define	CFG_BANK_REFRESH_VAL	0x00000410


.globl lowlevel_init
lowlevel_init:
	/*
	 * Don't use r12 here !
	 * Register r12 (IP) may be used by a linker as a scratch register
	 * between a routine and any subroutine it calls
	 * (for details, see §5.3.1.1, Use of IP by the linker)
	 */
	mov r11, lr

#ifdef CONFIG_SPL_BUILD
	/* Disable Watchdog */
	ldr	r0, =ELFIN_WATCHDOG_BASE
	mov	r1, #0
	str	r1, [r0]

	/* Disable main and sub interrupts */
	ldr	r0, =ELFIN_INTERRUPT_BASE
	mvn	r1, #0x0
	str	r1, [r0, #INTMSK_OFFSET]
	str	r1, [r0, #INTSUBMSK_OFFSET]

	/* Set all interrupts as IRQ */
	mov	r1, #0x0
	str	r1, [r0, #INTMOD_OFFSET]

	/* Init System Clock */
	bl system_clock_init

	/* Init Uart */
	bl uart_asm_init

	/* Init Memory */
	bl mem_asm_init
#endif

	/* everything is fine now */
	mov lr, r11
	mov	pc, lr


system_clock_init:

	ldr	r0, =ELFIN_CLOCK_POWER_BASE

	ldr	r1, [r0, #CLKDIV0CON_OFFSET]
	bic	r1, r1, #0x37	/* clear HCLKDIV, PREDIV, PCLKDIV */
	bic	r1, r1, #(0xf<<9) /* clear ARMCLKDIV */
	ldr	r2, =CLK_DIV_VAL
	orr	r1, r1, r2
	str	r1, [r0, #CLKDIV0CON_OFFSET]

	/*Set MPLL Locktime*/
	ldr	r1, =0xf10
	str	r1, [r0, #LOCKCON0_OFFSET]

	ldr	r1, =MPLL_VAL
	str	r1, [r0, #MPLLCON_OFFSET]

#if 1
	ldr	r1, =0x1780
	str	r1, [r0, #LOCKCON1_OFFSET]

	ldr	r1, =EPLL_VAL		/*Set EPLL*/
	str	r1, [r0, #EPLLCON_OFFSET]

	ldr	r1, [r0, #CLKSRCCON_OFFSET]
	bic r1, r1, #(0x3<<7)
	orr	r1, r1, #(0x2<<7)
	str	r1, [r0, #CLKSRCCON_OFFSET]

	ldr	r1, [r0, #CLKSRCCON_OFFSET]
	orr	r1, r1, #(1<<6)
	str	r1, [r0, #CLKSRCCON_OFFSET]

#endif

	/* set CLKSRC to fit MPLL and USB */
	ldr	r1, [r0, #CLKSRCCON_OFFSET]
	orr	r1, r1, #0x10
	str	r1, [r0, #CLKSRCCON_OFFSET]

	/* wait at least 200us to stablize all clock */
	mov	r1, #0x10000
0:	subs	r1, r1, #1
	bne	0b

	mov pc, lr


uart_asm_init:
	/* setup UART GPIO */
	ldr	r0, =ELFIN_GPIO_BASE
	ldr r1, [r0, #GPHCON_OFFSET]
	ldr r2, =0xffff
	bic r1, r1, r2
	ldr r2, =0xaaaa
	orr r1, r1, r2
	str	r1, [r0, #GPHCON_OFFSET]

	mov	pc, lr


mem_asm_init:
	mov	r13, #2

	/* Set GPK port when using x32 bus width. */
	ldr	r2,=GPKCON
	ldr	r1,=0xaaaaaaaa	@ set Sdata[31:16]
	str	r1, [r2]

	/* read setting value from structure */
	adr r0, sdram_bank_set_val
	ldr	r4, =ELFIN_MEMCTL_BASE
	ldmia	r0!, {r5-r9}
	stmia	r4!, {r5-r9}

1000:
	ldr	r4, =ELFIN_MEMCTL_BASE

	ldr	r1, [r4, #BANKCON1_OFFSET]
	ldr	r2, =0x40000000			/* DDR DSQInDLL Value Setting */
	orr	r1, r1, r2
	bic	r1, r1, #INIT_MASK

	/* Step 1: issue PALL */
	orr	r2, r1, #INIT_PALL
	str	r2, [r4, #BANKCON1_OFFSET]

	/* Step 2: write 0xff into the refresh timer */
	mov	r3, #0xff
	str	r3, [r4, #REFRESH_OFFSET]

	/* Step 3: wait more than 120 clk */
	mov	r3, #0x100
1:	subs	r3, r3, #1
	bne	1b

	/* Step 4: issue MRS */
	orr	r2, r1, #INIT_MRS
	str	r2, [r4, #BANKCON1_OFFSET]

	/* Step 5: nornal operation value into the refresh timer */
	str	r9, [r4, #REFRESH_OFFSET]

	/* Step 6: issue EMRS */
	orr	r2, r1, #INIT_EMRS
	str	r2, [r4, #BANKCON1_OFFSET]

	/* Step 7: Normal Mode */
	orr	r2, r1, #INIT_NORMAL
	str	r2, [r4, #BANKCON1_OFFSET]

	ldr	r6, =(PHYS_SDRAM_1+0x4)
	ldr	r7, =0x24564236
	swp	r8, r7, [r6]
	swp	r5, r8, [r6]
	cmp	r7, r5
	beq	1001f

	sub	r13, r13, #1
	cmp	r13, #0
	beq	1004f

1002:
	adr r0, sdram_bank_sel_val_alt
	ldmia	r0!, {r5-r6}
	stmia	r4!, {r5-r6}
	b	1000b

/* boyko : Memory Type DDR2 */
1004:	ldr	r4, =ELFIN_MEMCTL_BASE

	/* Step 1: BANKCFG Setting */
	ldr	r2, =CFG_BANK_CFG_VAL_DDR2
	str	r2, [r4, #BANKCFG_OFFSET]

	ldr	r1, =0x44000040
	str	r1, [r4, #BANKCON1_OFFSET]

	/* Step 2: BANKCON2 Setting */
	ldr	r2, =CFG_BANK_CON2_VAL_DDR2
	str	r2, [r4, #BANKCON2_OFFSET]

	/* Step 3: issue PALL */
	orr	r2, r1, #INIT_PALL
	str	r2, [r4, #BANKCON1_OFFSET]

	/* Step 4: Issue a EMRS2 command */
	ldr	r2, =0x80000000
	str	r2, [r4, #BANKCON3_OFFSET]

	orr	r2, r1, #INIT_EMRS
	str	r2, [r4, #BANKCON1_OFFSET]

	/* Step 5: Issue a EMRS3 command */
	ldr	r2, =0xc0000000
	str	r2, [r4, #BANKCON3_OFFSET]

	orr	r2, r1, #INIT_EMRS
	str	r2, [r4, #BANKCON1_OFFSET]

	/* Step 6: Issue a EMRS1 command */
	ldr	r2, =0x44000000
	str	r2, [r4, #BANKCON3_OFFSET]

	orr	r2, r1, #INIT_EMRS
	str	r2, [r4, #BANKCON1_OFFSET]

	/* Step 7: issue MRS */
	ldr	r2, =0x44000130
	str	r2, [r4, #BANKCON3_OFFSET]

	orr	r2, r1, #INIT_MRS
	str	r2, [r4, #BANKCON1_OFFSET]

	/* Step 8: issue PALL */
	orr	r2, r1, #INIT_PALL
	str	r2, [r4, #BANKCON1_OFFSET]

	/* Step 9: write 0xff into the refresh timer */
	mov	r3, #0xff
	str	r3, [r4, #REFRESH_OFFSET]

	/* Step 10: wait more than 120 clk */
	mov	r3, #0x100
10:	subs	r3, r3, #1
	bne	10b

	/* Step 11: issue MRS */
	ldr	r2, =0x44000030
	str	r2, [r4, #BANKCON3_OFFSET]

	orr	r2, r1, #INIT_MRS
	str	r2, [r4, #BANKCON1_OFFSET]

	/* Step 12: Issue a EMRS1 command */
	ldr	r2, =0x47800030
	str	r2, [r4, #BANKCON3_OFFSET]

	orr	r2, r1, #INIT_EMRS
	str	r2, [r4, #BANKCON1_OFFSET]

	ldr	r2, =0x44000030
	str	r2, [r4, #BANKCON3_OFFSET]

	orr	r2, r1, #INIT_EMRS
	str	r2, [r4, #BANKCON1_OFFSET]

	/* Step 13: write 0x87 into the refresh timer */
	mov	r3, #0x87
	str	r3, [r4, #REFRESH_OFFSET]

	/* Step 14: Normal Mode */
	orr	r2, r1, #INIT_NORMAL
	str	r2, [r4, #BANKCON1_OFFSET]
/* boyko : END - Memory Type DDR2 */

1001:
	mov	pc, lr


	.ltorg
sdram_bank_set_val:
	.word	CFG_BANK_CFG_VAL
	.word	CFG_BANK_CON1_VAL
	.word	CFG_BANK_CON2_VAL
	.word	CFG_BANK_CON3_VAL
	.word	CFG_BANK_REFRESH_VAL

sdram_bank_sel_val_alt:
	.word	CFG_BANK_CFG_VAL_ALT
	.word	CFG_BANK_CON1_VAL_ALT
