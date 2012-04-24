#include <common.h>
#include <command.h>

extern int fastboot_init(void);
extern int fastboot_send(const void *buf, int len);
extern int fastboot_sends(const void *buf, int len);
extern int fastboot_recvs(void *buf, int len);
extern int fastboot_run(void);
extern void fastboot_exit(void);

static int do_fastboot(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret;
	char buf[64];
	int read = 0, send = 0, actual = 0;

	if (!strncmp(argv[1], "recv", 4))
		read = 1;
	else if (!strncmp(argv[1], "send", 4)) {
		send = 1;
		strcpy(buf, argv[2]);
	}

	ret = fastboot_init();
	if (ret < 0)
		return ret;

	memset(buf, 0, sizeof(buf));

	if (read) {
		actual = fastboot_recvs(buf, sizeof(buf));
		printf("# recv: [%s](%d)\n", buf, actual);
	} else if (send) {
		printf("# start send...\n");
		actual = fastboot_send(buf, sizeof(buf));
		printf("# sends: [%s](%d)\n", buf, actual);
	} else {
		fastboot_run();
	}

	fastboot_exit();

	return 0;
}

U_BOOT_CMD(
	fastboot, 3, 0, do_fastboot,
	"Fastboot is protocol used to update the flash filesystem in Android devices from a host over USB.",
	"[recv, send]"
);
