/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/* The GPIO character device version 2 interface, copied from the Linux
 * 6.12 header include/uapi/linux/gpio.h, for toolchains whose kernel
 * headers are older than Linux 5.10. button.c includes it only when the
 * toolchain's own <linux/gpio.h> lacks this interface. */
#ifndef GPIO_V2_COMPAT_H
#define GPIO_V2_COMPAT_H
#include <linux/types.h>
#include <linux/ioctl.h>

#ifndef __aligned_u64
#define __aligned_u64 __u64 __attribute__((aligned(8)))
#endif
#ifndef _BITULL
#define _BITULL(x) (1ULL << (x))
#endif
#ifndef GPIO_MAX_NAME_SIZE
#define GPIO_MAX_NAME_SIZE 32
#endif
#define GPIO_V2_LINES_MAX 64
#define GPIO_V2_LINE_NUM_ATTRS_MAX 10

enum gpio_v2_line_flag {
	GPIO_V2_LINE_FLAG_USED			= _BITULL(0),
	GPIO_V2_LINE_FLAG_ACTIVE_LOW		= _BITULL(1),
	GPIO_V2_LINE_FLAG_INPUT			= _BITULL(2),
	GPIO_V2_LINE_FLAG_OUTPUT		= _BITULL(3),
	GPIO_V2_LINE_FLAG_EDGE_RISING		= _BITULL(4),
	GPIO_V2_LINE_FLAG_EDGE_FALLING		= _BITULL(5),
	GPIO_V2_LINE_FLAG_OPEN_DRAIN		= _BITULL(6),
	GPIO_V2_LINE_FLAG_OPEN_SOURCE		= _BITULL(7),
	GPIO_V2_LINE_FLAG_BIAS_PULL_UP		= _BITULL(8),
	GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN	= _BITULL(9),
	GPIO_V2_LINE_FLAG_BIAS_DISABLED		= _BITULL(10),
	GPIO_V2_LINE_FLAG_EVENT_CLOCK_REALTIME	= _BITULL(11),
	GPIO_V2_LINE_FLAG_EVENT_CLOCK_HTE	= _BITULL(12),
};

enum gpio_v2_line_attr_id {
	GPIO_V2_LINE_ATTR_ID_FLAGS		= 1,
	GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES	= 2,
	GPIO_V2_LINE_ATTR_ID_DEBOUNCE		= 3,
};

struct gpio_v2_line_attribute {
	__u32 id;
	__u32 padding;
	union {
		__aligned_u64 flags;
		__aligned_u64 values;
		__u32 debounce_period_us;
	};
};

struct gpio_v2_line_config_attribute {
	struct gpio_v2_line_attribute attr;
	__aligned_u64 mask;
};

struct gpio_v2_line_config {
	__aligned_u64 flags;
	__u32 num_attrs;
	/* Pad to fill implicit padding and reserve space for future use. */
	__u32 padding[5];
	struct gpio_v2_line_config_attribute attrs[GPIO_V2_LINE_NUM_ATTRS_MAX];
};

struct gpio_v2_line_request {
	__u32 offsets[GPIO_V2_LINES_MAX];
	char consumer[GPIO_MAX_NAME_SIZE];
	struct gpio_v2_line_config config;
	__u32 num_lines;
	__u32 event_buffer_size;
	/* Pad to fill implicit padding and reserve space for future use. */
	__u32 padding[5];
	__s32 fd;
};

enum gpio_v2_line_event_id {
	GPIO_V2_LINE_EVENT_RISING_EDGE	= 1,
	GPIO_V2_LINE_EVENT_FALLING_EDGE	= 2,
};

struct gpio_v2_line_event {
	__aligned_u64 timestamp_ns;
	__u32 id;
	__u32 offset;
	__u32 seqno;
	__u32 line_seqno;
	/* Space reserved for future use. */
	__u32 padding[6];
};

#define GPIO_V2_GET_LINE_IOCTL _IOWR(0xB4, 0x07, struct gpio_v2_line_request)
#endif
