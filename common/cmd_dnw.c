/*
 * common/cmd_usbd.c
 *
 *  $Id: cmd_usbd.c,v 1.7 2008/07/10 08:17:36 boyko Exp $
 *
 * (C) Copyright 2007
 * Byungjae Lee, Samsung Erectronics, bjlee@samsung.com.
 *	- support for S3C2412, S3C2443 and S3C6400
 *
 * (C) Copyright SAMSUNG Electronics
 *      SW.LEE  <hitchcar@samsung.com>
 *      - add USB device fo S3C2440A, S3C24A0A
 *
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

/*
 * Memory Functions
 *
 * Copied from FADS ROM, Dan Malek (dmalek@jlc.net)
 */

#include <common.h>
#include <command.h>

#define CONFIG_S3C_USBD
#ifdef CONFIG_S3C_USBD

#include "../arch/arm/cpu/arm1176/s3c64xx/usb/s3c6410.h"

#if defined(CONFIG_S3C6400) || defined(CONFIG_S3C6410) || defined(CONFIG_S3C6430)
#include "../arch/arm/cpu/arm1176/s3c64xx/usb/usbd-otg-hs.h"
#else
#error "* CFG_ERROR : you have to setup right Samsung CPU configuration"
#endif

#undef	CMD_USBD_DEBUG
#ifdef	CMD_USBD_DEBUG
#define	PRINTF(fmt,args...)	printf (fmt ,##args)
#else
#define PRINTF(fmt,args...)
#endif

#define USBD_DOWN_ADDR		0xc0000000

extern unsigned UsbDownloadChecksumOK, UsbDownloadFileSize;

static const char pszMe[] = "usbd: ";

int FriendlyARMGetDataFromUSB (unsigned max_len, unsigned char **data_ptr, unsigned *received_len)
{
	s3c_usbd_dn_addr = USBD_DOWN_ADDR;	/* Default Address */
	s3c_receive_done = 0;

	//memset( (unsigned char *)USBD_DOWN_ADDR, 0xFF, max_len);

	s3c_usbctl_init();
	s3c_usbc_activate();

	printf("Download address 0x%08x\n", s3c_usbd_dn_addr);

	while (1) {
		if (S3C_USBD_DETECT_IRQ()) {
			s3c_udc_int_hndlr();
			S3C_USBD_CLEAR_IRQ();
		}

		if (s3c_receive_done)
			break;
	}
	s3c_usb_stop();

	*received_len = UsbDownloadFileSize;
	*data_ptr = (unsigned char *)USBD_DOWN_ADDR;

	if (UsbDownloadFileSize > max_len) {
		printf("DNW download Data size is too big\n");
		return -1;
	}
	if (!UsbDownloadChecksumOK) {
		printf("DNW download Data Error\n");
		return -2;
	}
	{
		int PadLen = 128 * 1024 - UsbDownloadFileSize % (128 * 1024);
		PadLen %= 128 * 1024;
		if (PadLen != 0 && PadLen + UsbDownloadFileSize <= max_len) {
			memset((unsigned char *)USBD_DOWN_ADDR + UsbDownloadFileSize, 0xFF, PadLen);
		}
	}

	return 0;
}

int do_usbd_dnw ( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{

	if (argv[0][0] == 'u') {
		DNW = 0;
	}
	else {
		DNW = 1;
		s3c_got_header = 0;
	}

	switch (argc) {
	case 1 :
		s3c_usbd_dn_addr = USBD_DOWN_ADDR;	/* Default Address */
		break;
	case 2 :
		s3c_usbd_dn_addr = simple_strtoul(argv[1], NULL, 16);
		break;
	default:
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	s3c_receive_done = 0;

	s3c_usbctl_init();
	s3c_usbc_activate();

	PRINTF("Download address 0x%08x\n", s3c_usbd_dn_addr);
	printf("Now, Waiting for DNW to transmit data\n");

	while (1) {
		if (S3C_USBD_DETECT_IRQ()) {
			s3c_udc_int_hndlr();
			S3C_USBD_CLEAR_IRQ();
		}

		if (s3c_receive_done)
			break;

		if (serial_tstc()) {
			serial_getc();
			break;
		}
	}

	/* when operation is done, usbd must be stopped */
	s3c_usb_stop();

	return 0;
}

#if 0 /* ud command not support yet */
U_BOOT_CMD(
	ud, 3, 0, do_usbd_dnw,
	"ud      - initialize USB device and ready to receive for LINUX server (specific)\n",
	"[download address]\n"
);
#endif

U_BOOT_CMD(
	dnw, 3, 0, do_usbd_dnw,
	"dnw     - initialize USB device and ready to receive for Windows server (specific)\n",
	"[download address]\n"
);

#endif	/* CONFIG_S3C_USBD */

