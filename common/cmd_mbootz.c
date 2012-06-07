#include <common.h>
#include <command.h>
#include <image.h>

typedef int boot_os_fn(int flag, int argc, char * const argv[],
			bootm_headers_t *images);

extern boot_os_fn do_bootm_linux;

int do_mbootz(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char command[64];
	boot_os_fn	*boot_fn;
	bootm_headers_t image;
	image_header_t *kaddr, *raddr;
	u32 rsize = 0;

	kaddr = raddr = 0;
	boot_fn = do_bootm_linux;
	memset((void *)&image, 0, sizeof(bootm_headers_t));

	if (argc > 1)
		kaddr = (image_header_t *)simple_strtoul(argv[1], NULL, 0);
	if (argc > 2)
		raddr = (image_header_t *)simple_strtoul(argv[2], NULL, 0);
	if (argc > 3)
		rsize = simple_strtoul(argv[3], NULL, 0);

	if (image_check_magic(kaddr) && image_check_magic(raddr)) {
		sprintf(command, "bootm 0x%x 0x%x", (u32)kaddr, (u32)raddr);
		run_command(command, 0);
	} else {

		if (image_check_magic(kaddr)) {
			printf("## Get zImage from uImage.\n");
			image.ep = image_get_ep(kaddr);
		} else
			image.ep = (u32)kaddr;

		if (image_check_magic(raddr)) {
			printf("## Get Ramdisk from uImage.\n");
			image.rd_start = image_get_ep(raddr);
			image.rd_end = image.rd_start + image_get_size(raddr);
		} else {
			image.rd_start = (u32)raddr;
			if (rsize)
				image.rd_end = image.rd_start + rsize;
			else
				image.rd_end = image.rd_start + 8*1024*1024;
		}

		printf("## Booting kernel from zImage at %lx ...\n", image.ep);
		printf("## Loading init Ramdisk from Image at %lx - %lx ...\n",
				image.rd_start, image.rd_end);

		boot_fn(flag, argc, argv, &image);
	}

	return 0;
}

U_BOOT_CMD(
	mbootz, 4, 0, do_mbootz,
	"boot zImage",
	"kernel ramdisk [ramdisk_size]"
);
