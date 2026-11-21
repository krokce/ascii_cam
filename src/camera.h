#ifndef CAMERA_H
#define CAMERA_H
#include <stdint.h>

/* Camera capture through the bcm2835 V4L2 driver (the legacy firmware
 * camera stack). Frames are YUV 4:2:0; only the brightness plane is
 * used for the text picture, the whole frame for video recording. */
struct camera {
	int fd;
	int w, h, stride;
	uint32_t frame_size;      /* bytes in one whole YUV 4:2:0 frame */
	int hw_flip;              /* flips done by the camera itself */
	int nbuf;
	void *map[6];
	uint32_t len[6];
	int cur;                  /* buffer held by the caller, or -1 */
	char name[64];
};

int camera_open(struct camera *c, int w, int h, int fps, int hflip, int vflip);
/* Waits up to timeout_ms for a frame. Returns 0 and the frame, -1 on error. */
int camera_get(struct camera *c, const uint8_t **frame, int timeout_ms);
void camera_release(struct camera *c);
void camera_close(struct camera *c);

#endif
