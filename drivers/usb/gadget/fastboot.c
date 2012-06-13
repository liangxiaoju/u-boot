/*
#define DEBUG
*/
#include <common.h>
#include <malloc.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <linux/usb/ch9.h>
#include <usbdescriptors.h>
#include <linux/usb/gadget.h>
#include <usb/lin_gadget_compat.h>
#include <fastboot.h>
#include "regs-otg.h"

#define STRING_MANUFACTURER		1
#define STRING_PRODUCT			2
#define STRING_SERIALNUMBER		3
#define STRING_CONFIG			0
#define STRING_INTF				0

typedef int (*fastboot_handle_t)(const char *cmd);

typedef struct {
	char *lable;
	fastboot_handle_t handle;
} fastboot_func_t;

typedef struct fastboot_dev {
	struct usb_gadget *gadget;
	struct usb_request *req;
	struct usb_request *recv_req;
	struct usb_request *send_req;
	struct usb_ep *epin_bulk;
	struct usb_ep *epout_bulk;
} fastboot_dev_t;

static char manufacturer[] = "U-Boot";
static char product_desc[] = "Board";
static char config_desc[] = "usb-updater";
static char intf_desc[] = "fastboot";
static char serial_number[] = "fastboot-uboot";

/* define the start address of fastboot buffer */
static void *fastboot_buffer_addr = (void *)CONFIG_FASTBOOT_BUFFER_ADDR;
static unsigned download_size = 0;

static struct fastboot_dev *pfdev;

static struct usb_string fastboot_strings[] = {
	{ STRING_MANUFACTURER,	manufacturer, },
	{ STRING_PRODUCT,		product_desc, },
	{ STRING_CONFIG,		config_desc, },
	{ STRING_INTF,			intf_desc, },
	{ STRING_SERIALNUMBER,	serial_number, },
	{  },		/* end of list */
};

static struct usb_gadget_strings stringtab = {
	.language	= 0x0409,	/* en-us */
	.strings	= fastboot_strings,
};

static struct usb_device_descriptor fastboot_device_desc = {
	.bLength			= USB_DT_DEVICE_SIZE,
	.bDescriptorType	= USB_DT_DEVICE,
	/* change on the fly */
	.bcdUSB				= __constant_cpu_to_le16(0x0200),
	.bDeviceClass		= USB_CLASS_VENDOR_SPEC,
	.bDeviceSubClass	= 0,
	.bDeviceProtocol	= 0,
	/* change on the fly */
	.bMaxPacketSize0	= 64,
	.idVendor			= __constant_cpu_to_le16(0x18d1),
	.idProduct			= __constant_cpu_to_le16(0x0002),
	.bcdDevice			= __constant_cpu_to_le16(0x0100),
	.iManufacturer		= STRING_MANUFACTURER,
	.iProduct			= STRING_PRODUCT,
	.iSerialNumber		= STRING_SERIALNUMBER,
	.bNumConfigurations	= 1,
};

static const struct usb_config_descriptor fastboot_config_desc = {
	.bLength			= USB_DT_CONFIG_SIZE,
	.bDescriptorType	= USB_DT_CONFIG,
	.wTotalLength		= __constant_cpu_to_le16(
			USB_DT_CONFIG_SIZE + USB_DT_INTERFACE_SIZE +
			USB_DT_ENDPOINT_SIZE * 2),
	.bNumInterfaces		= 1,
	.bConfigurationValue= 1,//used by set_configuration
	.iConfiguration		= STRING_CONFIG,
	.bmAttributes		= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower			= 25,
};

static struct usb_interface_descriptor fastboot_intf_desc = {
	.bLength			= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= 0,
	.bAlternateSetting	= 0,
	.bNumEndpoints		= 2,
	.bInterfaceClass	= 0xff,
	.bInterfaceSubClass	= 0x42,
	.bInterfaceProtocol	= 0x03,
	.iInterface			= STRING_INTF,
};

struct usb_endpoint_descriptor fastboot_endpin_desc = {
	.bLength			= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	/* change on the fly */
	.bEndpointAddress	= USB_DIR_IN,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	/* change on the fly */
	.wMaxPacketSize		= __constant_cpu_to_le16(512),
	.bInterval			= 0,
};

struct usb_endpoint_descriptor fastboot_endpout_desc = {
	.bLength			= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	/* change on the fly */
	.bEndpointAddress	= USB_DIR_OUT,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	/* change on the fly */
	.wMaxPacketSize		= __constant_cpu_to_le16(512),
	.bInterval			= 0,
};

static const struct usb_descriptor_header *fastboot_desc_header[] = {
	(struct usb_descriptor_header *) &fastboot_intf_desc,
	(struct usb_descriptor_header *) &fastboot_endpin_desc,
	(struct usb_descriptor_header *) &fastboot_endpout_desc,
	NULL,
};

fastboot_flash_ops_t *default_get_fastboot_flash_ops(void) { return NULL; }

fastboot_flash_ops_t *get_fastboot_flash_ops(void) __attribute__(
		(weak, alias("default_get_fastboot_flash_ops")));

static int recv_complete;
static void fastboot_recv_complete(struct usb_ep *ep, struct usb_request *req)
{
	debug("%s\n", __func__);
	recv_complete = 1;
}

int fastboot_recv(void *buf, int len)
{
	struct fastboot_dev *fdev = pfdev;
	int ret;

	fdev->recv_req->length = len;
	fdev->recv_req->complete = fastboot_recv_complete;
	recv_complete = 0;
	ret = usb_ep_queue(fdev->epout_bulk, fdev->recv_req, 0);
	if (ret)
		return ret;

	while (!recv_complete) {
		usb_gadget_handle_interrupts();

		if (ctrlc())
			return -EINTR;
	}

	memcpy(buf, fdev->recv_req->buf, fdev->recv_req->actual);

	return fdev->recv_req->actual;
}

int fastboot_recvs(void *buf, int len)
{
	struct fastboot_dev *fdev = pfdev;
	int amount_left_to_recv = len;
	int maxpacket = fdev->epout_bulk->maxpacket;
	int amount, actual;

	while (amount_left_to_recv > 0) {
		amount = min(len, maxpacket);
		actual = fastboot_recv(buf, amount);
		if (actual < 0)
			return actual;

		amount_left_to_recv -= actual;
		buf += actual;

		/* XXX finish ? */
		if (actual < amount)
			return len - amount_left_to_recv;
	}

	return len;
}

static int send_complete;
static void fastboot_send_complete(struct usb_ep *ep, struct usb_request *req)
{
	debug("%s\n", __func__);
	send_complete = 1;
}

int fastboot_send(const void *buf, int len)
{
	struct fastboot_dev *fdev = pfdev;
	int ret;

	usb_gadget_handle_interrupts();
	usb_gadget_handle_interrupts();

	fdev->send_req->length = len;
	fdev->send_req->complete = fastboot_send_complete;
	memcpy(fdev->send_req->buf, buf, len);

	send_complete = 0;
	ret = usb_ep_queue(fdev->epin_bulk, fdev->send_req, 0);
	if (ret)
		return ret;

	while (!send_complete) {
		usb_gadget_handle_interrupts();

		if (ctrlc())
			return -EINTR;
	}

	/* XXX: what if actual != len */
	return fdev->send_req->actual;
}

int fastboot_sends(const void *buf, int len)
{
	struct fastboot_dev *fdev = pfdev;
	int amount_left_to_send = len;
	int maxpacket = fdev->epin_bulk->maxpacket;
	int amount, actual;

	while (amount_left_to_send > 0) {
		amount = min(len, maxpacket);
		actual = fastboot_send(buf, amount);
		if (actual < 0)
			return actual;

		amount_left_to_send -= actual;
		buf += actual;
	}

	return len;
}

static int fastboot_unknown(const char *cmd)
{
	char status[64];

	printf("fastboot %s.\n", cmd);

	sprintf(status, "OKAY");
	fastboot_sends(status, sizeof(status));

	return 0;
}

static int fastboot_reboot(const char *cmd)
{
	char status[64];

	printf("fastboot reboot.\n");

	sprintf(status, "OKAY");
	fastboot_sends(status, sizeof(status));

	printf("fastboot reboot finished.\n");
	run_command("reset", 0);

	return 0;
}

static int fastboot_reboot_bootloader(const char *cmd)
{
	char status[64];

	printf("fastboot reboot-bootloader.\n");

	sprintf(status, "OKAY");
	fastboot_sends(status, sizeof(status));

	run_command("reset", 0);

	return 0;
}

static int fastboot_continue(const char *cmd)
{
	char status[64];

	printf("fastboot continue.\n");

	sprintf(status, "OKAY");
	fastboot_sends(status, sizeof(status));

	return 1;
}

static int fastboot_download(const char *cmd)
{
	char status[64];
	unsigned size = simple_strtoul(cmd + 9, NULL, 16);
	int ret;

	printf("fastboot download(0x%x).\n", size);

	sprintf(status, "DATA%08x", size);
	fastboot_sends(status, sizeof(status));

	sprintf(status, "OKAY");
	ret = fastboot_recvs(fastboot_buffer_addr, size);
	if (ret != size) {
		printf("Error: Downloaded 0x%x bytes of 0x%x bytes.\n", ret, size);
		sprintf(status, "FAIL");
	}

	fastboot_sends(status, sizeof(status));

	download_size = size;

	return 0;
}

static int fastboot_flash(const char *cmd)
{
	int ret = 0;
	char status[64];
	const char *part = cmd + 6;
	unsigned long addr;
	fastboot_flash_ops_t *flash_ops;

	printf("fastboot flash %s.\n", part);

	sprintf(status, "FAIL");

	flash_ops = get_fastboot_flash_ops();
	if ((!flash_ops) || (!flash_ops->erase) || (!flash_ops->write))
		goto out;

	if (download_size > 0) {

		addr = simple_strtoul(part, NULL, 16);
		if (part[0] == '0' && part[1] == 'x') {
			memcpy((void *)addr, fastboot_buffer_addr, download_size);
			sprintf(status, "OKAY");
			goto out;
		}

		ret = flash_ops->erase(part);
		if (ret)
			goto out;

		ret = flash_ops->write((void *)fastboot_buffer_addr, part, download_size);
		if (ret)
			goto out;

		sprintf(status, "OKAY");
	}

out:
	download_size = 0;
	fastboot_sends(status, sizeof(status));

	return ret;
}

static int fastboot_boot(const char *cmd)
{
	char status[64], command[64];
	struct boot_img_hdr *hdr = fastboot_buffer_addr;
	unsigned page_size, ksize, rsize, kaddr=0, raddr=0;

	printf("fastboot boot.\n");

	sprintf(status, "FAIL");

	if (download_size > 0) {
		if (strncmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
			download_size = 0;
			goto out;
		}
		page_size = hdr->page_size;
		ksize = roundup(hdr->kernel_size, page_size);
		rsize = roundup(hdr->ramdisk_size, page_size);
		kaddr = hdr->kernel_addr;
		raddr = hdr->ramdisk_addr;
		/* make sure the base address is the same as SDRAM_BASE */
		kaddr = (kaddr & (~(0xf<<28))) | (CONFIG_SYS_SDRAM_BASE & (0xf<<28));
		raddr = (raddr & (~(0xf<<28))) | (CONFIG_SYS_SDRAM_BASE & (0xf<<28));

		memcpy((void *)kaddr, (void *)(hdr->magic + page_size), ksize);
		memcpy((void *)raddr, (void *)hdr->magic + page_size + ksize, rsize);

		if (hdr->cmdline[0]) {
			setenv("bootcmd", hdr->cmdline);
		}

		sprintf(status, "OKAY");
	}
out:
	fastboot_sends(status, sizeof(status));

	if (download_size > 0) {
		download_size = 0;
		sprintf(command, "bootmz 0x%x 0x%x 0x%x", kaddr, raddr, hdr->ramdisk_size);
		run_command(command, 0);
	}

	return 0;
}

static int fastboot_erase(const char *cmd)
{
	int ret;
	char status[64];
	const char *part = cmd + 6;
	fastboot_flash_ops_t *flash_ops;

	printf("fastboot erase %s.\n", part);

	flash_ops = get_fastboot_flash_ops();
	if (!flash_ops || !flash_ops->erase) {
		sprintf(status, "FAIL");
		ret = -1;
		goto out;
	}

	ret = flash_ops->erase(part);
	if (ret)
		sprintf(status, "FAIL");
	else
		sprintf(status, "OKAY");

out:
	fastboot_sends(status, sizeof(status));

	return ret;
}

static int fastboot_oem(const char *cmd)
{
	char status[64];

	printf("fastboot %s.\n", cmd);

	sprintf(status, "OKAY");
	fastboot_sends(status, sizeof(status));

	return 0;
}

fastboot_func_t fastboot_func[] = {
	{ "unknown",			fastboot_unknown },
	{ "reboot",				fastboot_reboot },
	{ "reboot-bootloader",	fastboot_reboot_bootloader },
	{ "continue",			fastboot_continue },
	{ "download",			fastboot_download },
	{ "flash",				fastboot_flash },
	{ "boot",				fastboot_boot },
	{ "erase",				fastboot_erase },
	{ "oem",				fastboot_oem },
};

fastboot_handle_t fastboot_find_item(const void *cmd)
{
	char *lable;
	int i, len;

	for (i = 0; i < ARRAY_SIZE(fastboot_func); i++) {
		lable = fastboot_func[i].lable;
		len = strlen(lable);
		if (!strncmp(lable, cmd, len))
			return fastboot_func[i].handle;
	}

	return NULL;
}

int fastboot_run(void)
{
	char cmd[64];
	int len, ret;
	fastboot_handle_t handle;

	while (!ctrlc()) {
		len = fastboot_recvs(cmd, sizeof(cmd));
		if (len < 0)
			return -1;

		cmd[len] = '\0';
		handle = fastboot_find_item(cmd);
		if (!handle) {
			printf("Failed to find handle [fastboot %s]\n", cmd);
			continue;
		}

		ret = handle(cmd);
		if (ret < 0)
			printf("Failed to handle [fastboot %s]\n", cmd);

		if (ret > 0)
			break;
	}

	return 0;
}

static void fastboot_setup_complete(struct usb_ep *ep, struct usb_request *req)
{
	debug("%s\n", __func__);
}

static int enum_complete = 0;
static void fastboot_enum_complete(struct usb_ep *ep, struct usb_request *req)
{
	debug("%s\n", __func__);
	enum_complete = 1;
}

static int fastboot_bind(struct usb_gadget *gadget)
{
	struct fastboot_dev *fdev;

	fdev = malloc(sizeof(struct fastboot_dev));
	if (!fdev)
		goto err;

	fdev->gadget = gadget;
	set_gadget_data(gadget, fdev);

	if (gadget->speed != USB_SPEED_HIGH)
		fastboot_device_desc.bcdUSB = __constant_cpu_to_le16(0x0110);

	fastboot_device_desc.bMaxPacketSize0 = gadget->ep0->maxpacket;

	fdev->epin_bulk = usb_ep_autoconfig(gadget, &fastboot_endpin_desc);
	fdev->epout_bulk = usb_ep_autoconfig(gadget, &fastboot_endpout_desc);
	if (!fdev->epin_bulk || !fdev->epout_bulk)
		goto err;

	fdev->req = usb_ep_alloc_request(gadget->ep0, GFP_KERNEL);
	fdev->recv_req = usb_ep_alloc_request(fdev->epout_bulk, GFP_KERNEL);
	fdev->send_req = usb_ep_alloc_request(fdev->epin_bulk, GFP_KERNEL);
	if (!fdev->req || !fdev->recv_req || !fdev->send_req)
		goto err;

	fdev->req->buf = malloc(gadget->ep0->maxpacket);
	fdev->recv_req->buf = malloc(fdev->epout_bulk->maxpacket);
	fdev->send_req->buf = malloc(fdev->epin_bulk->maxpacket);
	if (!fdev->req->buf || !fdev->recv_req->buf || !fdev->send_req->buf)
		goto err;

	fdev->req->complete = fastboot_setup_complete;

	pfdev = fdev;

	debug("%s: epin_bulk(%s), epout_bulk(%s)\n", __func__,
			fdev->epin_bulk->name, fdev->epout_bulk->name);
	return 0;

err:
	if (fdev) {
		if (fdev->req) {
			if (fdev->req->buf)
				free(fdev->req->buf);
			free(fdev->req);
		}
		if (fdev->recv_req) {
			if (fdev->recv_req->buf)
				free(fdev->recv_req->buf);
			free(fdev->recv_req);
		}
		if (fdev->send_req) {
			if (fdev->send_req->buf)
				free(fdev->send_req->buf);
			free(fdev->send_req);
		}
		free(fdev);
	}

	return -1;
}

static void fastboot_unbind(struct usb_gadget *gadget)
{
	struct fastboot_dev *fdev = get_gadget_data(gadget);

	debug("%s\n", __func__);
	free(fdev->req->buf);
	free(fdev->recv_req->buf);
	free(fdev->send_req->buf);
	usb_ep_free_request(gadget->ep0, fdev->req);
	usb_ep_free_request(fdev->epout_bulk, fdev->recv_req);
	usb_ep_free_request(fdev->epin_bulk, fdev->send_req);
	free(fdev);
}

static int fastboot_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	struct fastboot_dev *fdev = get_gadget_data(gadget);
	struct usb_request *req = fdev->req;
	void *buf = req->buf;
//	u16 wIndex = le16_to_cpu(ctrl->wIndex);
	u16 wValue = le16_to_cpu(ctrl->wValue);
	u16 wLength = le16_to_cpu(ctrl->wLength);
	int value = -1;
	int type;

	req->complete = fastboot_setup_complete;

	switch (ctrl->bRequest) {
	case USB_REQ_GET_DESCRIPTOR:
		debug("USB_REQ_GET_DESCRIPTOR\n");
		if (ctrl->bRequestType != USB_DIR_IN)
			break;
		type = wValue >> 8;
		switch (type) {
		case USB_DT_DEVICE:
			/*
			 * adjust the maxpacketsize here,
			 * because it may have been changed while enumerating
			 */
			fastboot_endpin_desc.wMaxPacketSize = fdev->epin_bulk->maxpacket;
			fastboot_endpout_desc.wMaxPacketSize = fdev->epout_bulk->maxpacket;

			debug("USB_DT_DEVICE(wLength=%d)\n", wLength);
			value = min(wLength, USB_DT_DEVICE_SIZE);
			memcpy(buf, &fastboot_device_desc, value);
			break;
		case USB_DT_OTHER_SPEED_CONFIG:
			debug("USB_DT_OTHER_SPEED_CONFIG\n");
			break;
		case USB_DT_CONFIG:
			debug("USB_DT_CONFIG(wLength=%d)\n", wLength);
			value = usb_gadget_config_buf(
					&fastboot_config_desc, buf,
					gadget->ep0->maxpacket, fastboot_desc_header);
			((struct usb_config_descriptor *)buf)->bDescriptorType = type;
			if (value > 0)
				value = min(wLength, (u16)value);
			break;
		case USB_DT_STRING:
			debug("USB_DT_STRING(index=%d)\n", wValue & 0xff);
			value = usb_gadget_get_string(&stringtab,
					wValue & 0xff, buf);
			if (value >= 0)
				value = min(wLength, (u16)value);
			break;
		default:
			debug("UNKNOWN DT\n");
			break;
		}
		break;
	case USB_REQ_SET_CONFIGURATION:
		debug("USB_REQ_SET_CONFIGURATION(wValue=%d)\n", wValue);
		value = 0;
		req->complete = fastboot_enum_complete;
		usb_ep_disable(fdev->epin_bulk);
		usb_ep_disable(fdev->epout_bulk);
		usb_ep_enable(fdev->epin_bulk, &fastboot_endpin_desc);
		usb_ep_enable(fdev->epout_bulk, &fastboot_endpout_desc);
		break;
	case USB_REQ_GET_CONFIGURATION:
		debug("USB_REQ_GET_CONFIGURATION\n");
		value = min(wLength, (u16)1);
		memcpy(buf, &fastboot_config_desc.bConfigurationValue, value);
		break;
	default:
		debug("UNKNOWN REQ\n");
		break;
	}

	if (value >= 0) {
		debug("respond with data transfer before status phase\n");
		req->length = value;
		req->zero = value < wLength
				&& (value % gadget->ep0->maxpacket) == 0;
		value = usb_ep_queue(gadget->ep0, req, 0);
		if (value < 0) {
			req->status = 0;
			fastboot_setup_complete(gadget->ep0, req);
		}
	}

	return value;
}

static void fastboot_disconnect(struct usb_gadget *gadget)
{
	debug("%s\n", __func__);
}

static struct usb_gadget_driver fastboot_gadget_driver = {
	.speed		= USB_SPEED_HIGH,
	.bind		= fastboot_bind,
	.unbind		= fastboot_unbind,
	.setup		= fastboot_setup,
	.disconnect	= fastboot_disconnect,
};

int fastboot_init(void)
{
	int ret;

	ret = usb_gadget_register_driver(&fastboot_gadget_driver);
	if (ret < 0)
		return -1;

	enum_complete = 0;
	while (!ctrlc() && !enum_complete) {
		usb_gadget_handle_interrupts();
	}

	return 0;
}

void fastboot_exit(void)
{
	usb_gadget_unregister_driver(&fastboot_gadget_driver);
}
