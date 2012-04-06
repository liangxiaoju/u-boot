#include <common.h>
#include <command.h>

extern int fastboot_init(void);
extern int fastboot_run(void);
extern void fastboot_exit(void);

static int do_fastboot(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret;

	ret = fastboot_init();
	if (ret < 0)
		return ret;

	fastboot_run();

	fastboot_exit();

	return 0;
}

U_BOOT_CMD(
	fastboot, 3, 0, do_fastboot,
	"",
	"\n"
);
