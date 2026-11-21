/* Camera capture with V4L2 memory-mapped buffers. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include "camera.h"

static int xioctl(int fd, unsigned long req, void *arg)
{
	int r;
	do {
		r = ioctl(fd, req, arg);
	} while (r == -1 && errno == EINTR);
	return r;
}

/* The camera is the V4L2 capture device whose driver is the MMAL one;
 * the codec and ISP devices of the same chip are skipped. */
static int find_camera(char *name, size_t len)
{
	char path[32];
	int i;

	for (i = 0; i < 32; i++) {
		struct v4l2_capability cap;
		int fd;

		snprintf(path, sizeof(path), "/dev/video%d", i);
		fd = open(path, O_RDWR | O_CLOEXEC);
		if (fd < 0)
			continue;
		memset(&cap, 0, sizeof(cap));
		if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == 0 &&
		    (cap.device_caps & V4L2_CAP_VIDEO_CAPTURE) &&
		    (cap.device_caps & V4L2_CAP_STREAMING) &&
		    strstr((const char *)cap.driver, "mmal")) {
			snprintf(name, len, "%s", (const char *)cap.card);
			return fd;
		}
		close(fd);
	}
	return -1;
}

int camera_open(struct camera *c, int w, int h, int fps, int hflip, int vflip)
{
	struct v4l2_format fmt;
	struct v4l2_streamparm parm;
	struct v4l2_requestbuffers rb;
	struct v4l2_control ctl;
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int i;

	memset(c, 0, sizeof(*c));
	c->cur = -1;
	c->fd = find_camera(c->name, sizeof(c->name));
	if (c->fd < 0)
		return -1;

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = (uint32_t)w;
	fmt.fmt.pix.height = (uint32_t)h;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;
	if (xioctl(c->fd, VIDIOC_S_FMT, &fmt) < 0)
		goto fail;
	if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUV420)
		goto fail;
	c->w = (int)fmt.fmt.pix.width;
	c->h = (int)fmt.fmt.pix.height;
	c->stride = (int)fmt.fmt.pix.bytesperline;
	c->frame_size = fmt.fmt.pix.sizeimage;

	memset(&parm, 0, sizeof(parm));
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = (uint32_t)fps;
	xioctl(c->fd, VIDIOC_S_PARM, &parm);

	/* Let the camera do the flips, so video recordings get them too. */
	c->hw_flip = 1;
	ctl.id = V4L2_CID_HFLIP;
	ctl.value = hflip;
	if (xioctl(c->fd, VIDIOC_S_CTRL, &ctl) < 0)
		c->hw_flip = 0;
	ctl.id = V4L2_CID_VFLIP;
	ctl.value = vflip;
	if (xioctl(c->fd, VIDIOC_S_CTRL, &ctl) < 0)
		c->hw_flip = 0;

	memset(&rb, 0, sizeof(rb));
	rb.count = 4;
	rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	rb.memory = V4L2_MEMORY_MMAP;
	if (xioctl(c->fd, VIDIOC_REQBUFS, &rb) < 0 || rb.count < 2)
		goto fail;
	c->nbuf = rb.count > 6 ? 6 : (int)rb.count;
	for (i = 0; i < c->nbuf; i++) {
		struct v4l2_buffer b;

		memset(&b, 0, sizeof(b));
		b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		b.memory = V4L2_MEMORY_MMAP;
		b.index = (uint32_t)i;
		if (xioctl(c->fd, VIDIOC_QUERYBUF, &b) < 0)
			goto fail;
		c->len[i] = b.length;
		c->map[i] = mmap(NULL, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, c->fd, b.m.offset);
		if (c->map[i] == MAP_FAILED) {
			c->map[i] = NULL;
			goto fail;
		}
		if (xioctl(c->fd, VIDIOC_QBUF, &b) < 0)
			goto fail;
	}
	if (xioctl(c->fd, VIDIOC_STREAMON, &type) < 0)
		goto fail;
	return 0;
fail:
	camera_close(c);
	return -1;
}

int camera_get(struct camera *c, const uint8_t **frame, int timeout_ms)
{
	struct pollfd pfd = { c->fd, POLLIN, 0 };
	struct v4l2_buffer b;

	if (c->cur >= 0)
		camera_release(c);
	if (poll(&pfd, 1, timeout_ms) <= 0)
		return -1;
	memset(&b, 0, sizeof(b));
	b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	b.memory = V4L2_MEMORY_MMAP;
	if (xioctl(c->fd, VIDIOC_DQBUF, &b) < 0)
		return -1;
	c->cur = (int)b.index;
	*frame = c->map[b.index];
	return 0;
}

void camera_release(struct camera *c)
{
	struct v4l2_buffer b;

	if (c->cur < 0)
		return;
	memset(&b, 0, sizeof(b));
	b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	b.memory = V4L2_MEMORY_MMAP;
	b.index = (uint32_t)c->cur;
	xioctl(c->fd, VIDIOC_QBUF, &b);
	c->cur = -1;
}

void camera_close(struct camera *c)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int i;

	if (c->fd < 0)
		return;
	xioctl(c->fd, VIDIOC_STREAMOFF, &type);
	for (i = 0; i < c->nbuf; i++)
		if (c->map[i])
			munmap(c->map[i], c->len[i]);
	close(c->fd);
	c->fd = -1;
	c->nbuf = 0;
}
