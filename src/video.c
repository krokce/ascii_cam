/* H.264 video recording through the hardware encoder. Each camera frame
 * is copied into an encoder input buffer; finished H.264 data is
 * appended to a .h264 file on REC that VLC and most players open.
 * Files are named and split every 30 minutes like the text recordings.
 * Each new file starts at a key frame with its stream headers. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include "asciicam.h"
#include "video.h"

#define NOUT 3
#define NCAP 4
#define PART_MS (30u * 60u * 1000u)

struct video {
	int fd, file;
	char dir[200], path[300];
	uint32_t boot, part;
	int w, h;
	void *out_map[NOUT], *cap_map[NCAP];
	uint32_t out_len[NOUT], cap_len[NCAP];
	int out_busy[NOUT];
	uint64_t start, last_flush;
	int want_key, waiting_key;
	int failed;
};

static int xioctl(int fd, unsigned long req, void *arg)
{
	int r;
	do {
		r = ioctl(fd, req, arg);
	} while (r == -1 && errno == EINTR);
	return r;
}

static int find_encoder(void)
{
	char path[32];
	int i;

	for (i = 0; i < 32; i++) {
		struct v4l2_capability cap;
		int fd;

		snprintf(path, sizeof(path), "/dev/video%d", i);
		fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;
		memset(&cap, 0, sizeof(cap));
		if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == 0 &&
		    strstr((const char *)cap.card, "encode") &&
		    (cap.device_caps & V4L2_CAP_VIDEO_M2M_MPLANE))
			return fd;
		close(fd);
	}
	return -1;
}

static void set_ctrl(int fd, uint32_t id, int32_t value)
{
	struct v4l2_control c = { id, value };
	xioctl(fd, VIDIOC_S_CTRL, &c);
}

static int open_file(struct video *v, uint64_t now)
{
	struct stat st;

	for (;;) {
		snprintf(v->path, sizeof(v->path), "%s/boot%05u-part%04u.h264", v->dir, v->boot, v->part);
		if (stat(v->path, &st) != 0)
			break;
		v->part++;
	}
	v->file = open(v->path, O_WRONLY | O_CREAT | O_EXCL, 0644);
	v->start = now;
	return v->file < 0 ? -1 : 0;
}

static int queue_capture(struct video *v, int i)
{
	struct v4l2_buffer b;
	struct v4l2_plane pl[1];

	memset(&b, 0, sizeof(b));
	memset(pl, 0, sizeof(pl));
	b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	b.memory = V4L2_MEMORY_MMAP;
	b.index = (uint32_t)i;
	b.length = 1;
	b.m.planes = pl;
	return xioctl(v->fd, VIDIOC_QBUF, &b);
}

static int map_buffers(struct video *v, uint32_t type, int count, void **maps, uint32_t *lens)
{
	struct v4l2_requestbuffers rb;
	int i;

	memset(&rb, 0, sizeof(rb));
	rb.count = (uint32_t)count;
	rb.type = type;
	rb.memory = V4L2_MEMORY_MMAP;
	if (xioctl(v->fd, VIDIOC_REQBUFS, &rb) < 0 || (int)rb.count < count)
		return -1;
	for (i = 0; i < count; i++) {
		struct v4l2_buffer b;
		struct v4l2_plane pl[1];

		memset(&b, 0, sizeof(b));
		memset(pl, 0, sizeof(pl));
		b.type = type;
		b.memory = V4L2_MEMORY_MMAP;
		b.index = (uint32_t)i;
		b.length = 1;
		b.m.planes = pl;
		if (xioctl(v->fd, VIDIOC_QUERYBUF, &b) < 0)
			return -1;
		lens[i] = pl[0].length;
		maps[i] = mmap(NULL, pl[0].length, PROT_READ | PROT_WRITE, MAP_SHARED, v->fd, pl[0].m.mem_offset);
		if (maps[i] == MAP_FAILED) {
			maps[i] = NULL;
			return -1;
		}
	}
	return 0;
}

struct video *video_open(const char *dir, uint32_t boot, int w, int h, int stride, int fps)
{
	struct video *v = calloc(1, sizeof(*v));
	struct v4l2_format f;
	struct v4l2_streamparm parm;
	enum v4l2_buf_type t;
	int i;

	if (!v)
		return NULL;
	v->file = -1;
	v->fd = find_encoder();
	if (v->fd < 0) {
		log_msg("The hardware video encoder was not found, so video recording is off.");
		free(v);
		return NULL;
	}
	snprintf(v->dir, sizeof(v->dir), "%s", dir);
	v->boot = boot;
	v->part = 1;
	v->w = w;
	v->h = h;

	memset(&f, 0, sizeof(f));
	f.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	f.fmt.pix_mp.width = (uint32_t)w;
	f.fmt.pix_mp.height = (uint32_t)h;
	f.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
	f.fmt.pix_mp.num_planes = 1;
	f.fmt.pix_mp.plane_fmt[0].bytesperline = (uint32_t)stride;
	if (xioctl(v->fd, VIDIOC_S_FMT, &f) < 0)
		goto fail;
	memset(&f, 0, sizeof(f));
	f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	f.fmt.pix_mp.width = (uint32_t)w;
	f.fmt.pix_mp.height = (uint32_t)h;
	f.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
	f.fmt.pix_mp.num_planes = 1;
	f.fmt.pix_mp.plane_fmt[0].sizeimage = 512 * 1024;
	if (xioctl(v->fd, VIDIOC_S_FMT, &f) < 0)
		goto fail;
	memset(&parm, 0, sizeof(parm));
	parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	parm.parm.output.timeperframe.numerator = 1;
	parm.parm.output.timeperframe.denominator = (uint32_t)fps;
	xioctl(v->fd, VIDIOC_S_PARM, &parm);

	/* About 2 megabits per second: roughly 1 gigabyte an hour. Stream
	 * headers are repeated before every key frame, one per 2 seconds,
	 * so any part of a file can be played on its own. */
	set_ctrl(v->fd, V4L2_CID_MPEG_VIDEO_BITRATE, 2000000);
	set_ctrl(v->fd, V4L2_CID_MPEG_VIDEO_H264_PROFILE, V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
	set_ctrl(v->fd, V4L2_CID_MPEG_VIDEO_H264_LEVEL, V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
	set_ctrl(v->fd, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, fps * 2);
	set_ctrl(v->fd, V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER, 1);

	if (map_buffers(v, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, NOUT, v->out_map, v->out_len) < 0 ||
	    map_buffers(v, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, NCAP, v->cap_map, v->cap_len) < 0)
		goto fail;
	for (i = 0; i < NCAP; i++)
		if (queue_capture(v, i) < 0)
			goto fail;
	t = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	if (xioctl(v->fd, VIDIOC_STREAMON, &t) < 0)
		goto fail;
	t = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	if (xioctl(v->fd, VIDIOC_STREAMON, &t) < 0)
		goto fail;
	return v;
fail:
	log_msg("The hardware video encoder could not be set up (%s), so video recording is off.", strerror(errno));
	video_close(v);
	return NULL;
}

/* Collect finished H.264 data and give spent input buffers back. */
static void drain(struct video *v, uint64_t now)
{
	for (;;) {
		struct v4l2_buffer b;
		struct v4l2_plane pl[1];

		memset(&b, 0, sizeof(b));
		memset(pl, 0, sizeof(pl));
		b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		b.memory = V4L2_MEMORY_MMAP;
		b.length = 1;
		b.m.planes = pl;
		if (xioctl(v->fd, VIDIOC_DQBUF, &b) < 0)
			break;
		if (b.index < NOUT)
			v->out_busy[b.index] = 0;
	}
	for (;;) {
		struct v4l2_buffer b;
		struct v4l2_plane pl[1];
		uint32_t used;

		memset(&b, 0, sizeof(b));
		memset(pl, 0, sizeof(pl));
		b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		b.memory = V4L2_MEMORY_MMAP;
		b.length = 1;
		b.m.planes = pl;
		if (xioctl(v->fd, VIDIOC_DQBUF, &b) < 0)
			break;
		used = pl[0].bytesused - pl[0].data_offset;
		/* Start a new file at the first key frame after 30 minutes. */
		if (v->waiting_key && (b.flags & V4L2_BUF_FLAG_KEYFRAME)) {
			if (v->file >= 0) {
				fdatasync(v->file);
				close(v->file);
				v->file = -1;
			}
			v->part++;
			v->waiting_key = 0;
			if (open_file(v, now) < 0)
				log_msg("A video file could not be created (%s).", strerror(errno));
		}
		if (v->file >= 0 && used > 0 && b.index < NCAP &&
		    write(v->file, (uint8_t *)v->cap_map[b.index] + pl[0].data_offset, used) != (ssize_t)used) {
			log_msg("Writing the video file %s failed (%s). Video recording stops until the next start.", v->path, strerror(errno));
			close(v->file);
			v->file = -1;
			v->failed = 1;
		}
		queue_capture(v, (int)b.index);
	}
}

void video_frame(struct video *v, const uint8_t *yuv, uint32_t size, uint64_t now)
{
	struct v4l2_buffer b;
	struct v4l2_plane pl[1];
	int i;

	if (!v || v->failed)
		return;
	if (v->file < 0 && !v->waiting_key && open_file(v, now) < 0) {
		log_msg("A video file could not be created (%s). Video recording stops until the next start.", strerror(errno));
		v->failed = 1;
		return;
	}
	if (now - v->start >= PART_MS && !v->waiting_key) {
		set_ctrl(v->fd, V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);
		v->waiting_key = 1;
		v->start = now;
	}
	drain(v, now);
	for (i = 0; i < NOUT; i++)
		if (!v->out_busy[i])
			break;
	if (i == NOUT)
		return;    /* the encoder is behind: drop this frame */
	if (size > v->out_len[i])
		size = v->out_len[i];
	memcpy(v->out_map[i], yuv, size);
	memset(&b, 0, sizeof(b));
	memset(pl, 0, sizeof(pl));
	b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	b.memory = V4L2_MEMORY_MMAP;
	b.index = (uint32_t)i;
	b.length = 1;
	b.m.planes = pl;
	pl[0].bytesused = size;
	b.timestamp.tv_sec = (long)(now / 1000);
	b.timestamp.tv_usec = (long)(now % 1000) * 1000;
	if (xioctl(v->fd, VIDIOC_QBUF, &b) == 0)
		v->out_busy[i] = 1;
	if (v->file >= 0 && now - v->last_flush >= 5000) {
		fdatasync(v->file);
		v->last_flush = now;
	}
}

void video_close(struct video *v)
{
	enum v4l2_buf_type t;
	int i;

	if (!v)
		return;
	if (v->fd >= 0) {
		t = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		xioctl(v->fd, VIDIOC_STREAMOFF, &t);
		t = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		xioctl(v->fd, VIDIOC_STREAMOFF, &t);
		for (i = 0; i < NOUT; i++)
			if (v->out_map[i])
				munmap(v->out_map[i], v->out_len[i]);
		for (i = 0; i < NCAP; i++)
			if (v->cap_map[i])
				munmap(v->cap_map[i], v->cap_len[i]);
		close(v->fd);
	}
	if (v->file >= 0) {
		fdatasync(v->file);
		close(v->file);
	}
	free(v);
}
