/* Recording the displayed text frames to the REC partition.
 * The file format is described in docs/RECORDING-FORMAT.md. In short:
 * a 64-byte header, then fixed-size frame records, each with its own
 * millisecond timestamp and checksum, all numbers little-endian. */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>
#include "asciicam.h"

#define PART_MS (30u * 60u * 1000u)   /* a new file every 30 minutes */
#define FLUSH_MS 5000u                /* push data to the card every 5 s */
#define KEEPALIVE_MS 5000u            /* write a frame at least every 5 s */
#define CLEANUP_MS 60000u             /* check free space every minute */

static uint32_t crc_table[256];

uint32_t crc32_calc(const void *data, size_t len)
{
	const uint8_t *p = data;
	uint32_t c = 0xFFFFFFFFu;
	size_t i;

	if (!crc_table[1]) {
		uint32_t n, k;
		for (n = 0; n < 256; n++) {
			uint32_t v = n;
			for (k = 0; k < 8; k++)
				v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
			crc_table[n] = v;
		}
	}
	for (i = 0; i < len; i++)
		c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFu;
}

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, (uint16_t)v); put16(p + 2, (uint16_t)(v >> 16)); }
static uint32_t get32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

size_t rec_record_size(int cols, int rows)
{
	size_t cells = (size_t)cols * rows;
	return 4 + 4 + cells + (cells + 7) / 8 + 4;
}

size_t rec_encode_header(uint8_t *b, int cols, int rows, uint32_t boot,
			 uint32_t part, int64_t real_start_ms, int mode)
{
	memset(b, 0, REC_HEADER_SIZE);
	memcpy(b, REC_MAGIC, 8);
	put16(b + 8, REC_VERSION);
	put16(b + 10, REC_HEADER_SIZE);
	put16(b + 12, (uint16_t)cols);
	put16(b + 14, (uint16_t)rows);
	put32(b + 16, boot);
	put32(b + 20, part);
	put32(b + 24, (uint32_t)(uint64_t)real_start_ms);
	put32(b + 28, (uint32_t)((uint64_t)real_start_ms >> 32));
	b[32] = (uint8_t)mode;
	b[33] = 0;   /* character set: 0 means code page 437 */
	put16(b + 34, (uint16_t)rec_record_size(cols, rows));
	put32(b + 60, crc32_calc(b, 60));
	return REC_HEADER_SIZE;
}

size_t rec_encode_frame(uint8_t *b, const struct grid *g, uint32_t ms)
{
	size_t cells = (size_t)g->cols * g->rows;
	size_t size = rec_record_size(g->cols, g->rows);
	uint8_t *ch = b + 8, *bits = b + 8 + cells;
	size_t i;

	put32(b, REC_FRAME_MAGIC);
	put32(b + 4, ms);
	memset(bits, 0, (cells + 7) / 8);
	for (i = 0; i < cells; i++) {
		ch[i] = g->c[i].ch;
		if (g->c[i].bright)
			bits[i / 8] |= (uint8_t)(1u << (i % 8));
	}
	put32(b + size - 4, crc32_calc(b, size - 4));
	return size;
}

int rec_decode_frame(const uint8_t *b, size_t len, int cols, int rows,
		     struct grid *g, uint32_t *ms)
{
	size_t cells = (size_t)cols * rows;
	size_t size = rec_record_size(cols, rows);
	size_t i;

	if (len < size || get32(b) != REC_FRAME_MAGIC)
		return -1;
	if (crc32_calc(b, size - 4) != get32(b + size - 4))
		return -1;
	*ms = get32(b + 4);
	g->cols = cols;
	g->rows = rows;
	for (i = 0; i < cells; i++) {
		g->c[i].ch = b[8 + i];
		g->c[i].bright = (b[8 + cells + i / 8] >> (i % 8)) & 1;
	}
	return 0;
}

struct recorder {
	char dir[200];
	char path[300];
	int fd;
	uint32_t boot, part;
	int mode, cols;
	uint64_t start, last_write, last_flush, last_cleanup, error_until;
	struct grid last;
	int have_last;
	uint8_t buf[4 + 4 + MAX_CELLS + MAX_CELLS / 8 + 4 + 8];
};

static int open_part(struct recorder *r, uint64_t now)
{
	uint8_t hdr[REC_HEADER_SIZE];
	struct stat st;
	int64_t real = 0;

	for (;;) {
		snprintf(r->path, sizeof(r->path), "%s/boot%05u-part%04u.acr", r->dir, r->boot, r->part);
		if (stat(r->path, &st) != 0)
			break;
		r->part++;    /* the program restarted during this boot */
	}
	if (access("/run/asciicam-clock-ok", F_OK) == 0) {
		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		real = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	}
	r->fd = open(r->path, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (r->fd < 0)
		return -1;
	rec_encode_header(hdr, r->cols, ROWS, r->boot, r->part, real, r->mode);
	if (write(r->fd, hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) {
		close(r->fd);
		r->fd = -1;
		return -1;
	}
	r->start = now;
	r->have_last = 0;
	return 0;
}

struct recorder *recorder_open(const char *dir, uint32_t boot, int mode, int cols)
{
	struct recorder *r = calloc(1, sizeof(*r));

	if (!r)
		return NULL;
	snprintf(r->dir, sizeof(r->dir), "%s", dir);
	mkdir(dir, 0777);
	r->boot = boot;
	r->part = 1;
	r->mode = mode;
	r->cols = cols;
	r->fd = -1;
	return r;
}

static void close_part(struct recorder *r)
{
	if (r->fd >= 0) {
		fdatasync(r->fd);
		close(r->fd);
		r->fd = -1;
	}
}

void recorder_frame(struct recorder *r, const struct grid *g, uint64_t now)
{
	size_t size;

	if (!r || now < r->error_until)
		return;
	if (now - r->last_cleanup >= CLEANUP_MS || r->last_cleanup == 0) {
		rec_cleanup(r->dir, 90);
		r->last_cleanup = now;
	}
	if (r->fd >= 0 && (now - r->start >= PART_MS || g->cols != r->cols)) {
		close_part(r);
		r->part++;
		rec_cleanup(r->dir, 90);
	}
	r->cols = g->cols;
	if (r->fd < 0 && open_part(r, now) < 0) {
		log_msg("A recording file could not be created on the REC partition (%s). Recording pauses for one minute.", strerror(errno));
		r->error_until = now + 60000;
		return;
	}
	/* Skip frames identical to the last one written, but write one at
	 * least every 5 seconds so a reader can tell the recorder was alive. */
	if (r->have_last && now - r->last_write < KEEPALIVE_MS &&
	    !memcmp(r->last.c, g->c, sizeof(struct cell) * g->cols * g->rows))
		return;
	size = rec_encode_frame(r->buf, g, (uint32_t)(now - r->start));
	if (write(r->fd, r->buf, size) != (ssize_t)size) {
		log_msg("Writing to the recording file %s failed (%s). Recording pauses for one minute.", r->path, strerror(errno));
		close_part(r);
		r->part++;
		r->error_until = now + 60000;
		return;
	}
	memcpy(r->last.c, g->c, sizeof(struct cell) * g->cols * g->rows);
	r->have_last = 1;
	r->last_write = now;
	if (now - r->last_flush >= FLUSH_MS) {
		fdatasync(r->fd);
		r->last_flush = now;
	}
}

void recorder_close(struct recorder *r)
{
	if (!r)
		return;
	close_part(r);
	free(r);
}

static int cmp_names(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Delete the oldest recordings while the partition is over max_percent
 * full. Names sort by boot and part number, so the first is the oldest.
 * The newest file, the one being written, is never deleted. */
int rec_cleanup(const char *dir, int max_percent)
{
	int deleted = 0;

	for (;;) {
		struct statvfs vfs;
		DIR *d;
		struct dirent *e;
		char **names = NULL;
		size_t n = 0, cap = 0;
		char path[512];

		if (statvfs(dir, &vfs) != 0 || vfs.f_blocks == 0)
			return deleted;
		if ((vfs.f_blocks - vfs.f_bfree) * 100 <= (unsigned long long)vfs.f_blocks * max_percent)
			return deleted;
		d = opendir(dir);
		if (!d)
			return deleted;
		while ((e = readdir(d))) {
			size_t len = strlen(e->d_name);
			if (strncmp(e->d_name, "boot", 4))
				continue;
			if (!(len > 4 && (!strcmp(e->d_name + len - 4, ".acr") ||
					  (len > 5 && !strcmp(e->d_name + len - 5, ".h264")))))
				continue;
			if (n == cap) {
				cap = cap ? cap * 2 : 64;
				names = realloc(names, cap * sizeof(*names));
			}
			names[n++] = strdup(e->d_name);
		}
		closedir(d);
		if (n < 2) {
			while (n)
				free(names[--n]);
			free(names);
			return deleted;
		}
		qsort(names, n, sizeof(*names), cmp_names);
		{
			/* Files of the newest part (text and video) are kept. */
			const char *dot = strchr(names[n - 1], '.');
			size_t plen = dot ? (size_t)(dot - names[n - 1]) : strlen(names[n - 1]);
			int ok = 0;

			if (strncmp(names[0], names[n - 1], plen) != 0) {
				snprintf(path, sizeof(path), "%s/%s", dir, names[0]);
				if (unlink(path) == 0) {
					ok = 1;
					deleted++;
					log_msg("The REC partition was over %d percent full, so the oldest recording, %s, was deleted.", max_percent, names[0]);
				}
			}
			while (n)
				free(names[--n]);
			free(names);
			if (!ok || deleted > 1000)
				return deleted;
		}
	}
}
