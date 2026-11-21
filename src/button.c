/* Reading the mode button through the kernel's GPIO character device,
 * with only the C library. The kernel reports each falling edge with a
 * timestamp; presses closer together than 300 milliseconds are treated
 * as contact bounce and ignored. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/gpio.h>
#ifndef GPIO_V2_GET_LINE_IOCTL
#include "gpio_v2_compat.h"
#endif
#include "button.h"

#define BOUNCE_NS 300000000ull

static uint64_t last_press_ns;

int button_open(void)
{
	struct gpio_v2_line_request req;
	int chip, flags;

	chip = open("/dev/gpiochip0", O_RDONLY | O_CLOEXEC);
	if (chip < 0)
		return -1;
	memset(&req, 0, sizeof(req));
	req.offsets[0] = BUTTON_GPIO;
	req.num_lines = 1;
	strncpy(req.consumer, "asciicam mode button", sizeof(req.consumer) - 1);
	req.config.flags = GPIO_V2_LINE_FLAG_INPUT | GPIO_V2_LINE_FLAG_EDGE_FALLING |
			   GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
	req.config.num_attrs = 1;
	req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE;
	req.config.attrs[0].attr.debounce_period_us = 50000;
	req.config.attrs[0].mask = 1;
	if (ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
		/* Try again without debounce, in case the kernel lacks it. */
		req.config.num_attrs = 0;
		if (ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
			close(chip);
			return -1;
		}
	}
	close(chip);
	flags = fcntl(req.fd, F_GETFL);
	fcntl(req.fd, F_SETFL, flags | O_NONBLOCK);
	return req.fd;
}

int button_presses(int fd)
{
	struct gpio_v2_line_event ev[8];
	int n = 0;

	if (fd < 0)
		return 0;
	for (;;) {
		ssize_t r = read(fd, ev, sizeof(ev));
		size_t i;

		if (r <= 0)
			break;
		for (i = 0; i < (size_t)r / sizeof(ev[0]); i++) {
			if (ev[i].id != GPIO_V2_LINE_EVENT_FALLING_EDGE)
				continue;
			if (last_press_ns && ev[i].timestamp_ns - last_press_ns < BOUNCE_NS)
				continue;
			last_press_ns = ev[i].timestamp_ns;
			n++;
		}
	}
	return n;
}

void button_close(int fd)
{
	if (fd >= 0)
		close(fd);
}
