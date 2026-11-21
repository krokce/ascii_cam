#ifndef VIDEO_H
#define VIDEO_H
#include <stdint.h>

/* Optional H.264 video recording with the GPU's hardware encoder
 * (the bcm2835-codec V4L2 memory-to-memory device). Off by default. */
struct video;

struct video *video_open(const char *dir, uint32_t boot, int w, int h, int stride, int fps);
void video_frame(struct video *v, const uint8_t *yuv, uint32_t size, uint64_t now_ms);
void video_close(struct video *v);

#endif
