#ifndef KMS_H
#define KMS_H
#include "asciicam.h"

/* Display output through the kernel's KMS interface, without libdrm.
 *
 * One 720 by 350 ARGB buffer holds the picture with three levels per
 * dot. The DPI CRTC shows it scaled to 1440 by 350 (each dot doubled,
 * nearest neighbour), and the HDMI CRTC shows the same buffer scaled to
 * a centred 4 by 3 area, coloured by that CRTC's gamma table. The
 * processor draws each frame once. Optional extra buffers: 16-level
 * gray for HDMI, a glow layer and a scan-line layer for emulation. */

struct kms_buf {
	uint32_t fb_id, handle, pitch;
	uint64_t size;
	uint32_t *map;
	int w, h;
};

struct kms_out {
	int found;          /* connector exists */
	int connected;      /* a screen is attached */
	int active;         /* our CRTC is running on it */
	uint32_t conn_id, crtc_id, primary;
	int crtc_index;
	uint8_t mode[68];   /* struct drm_mode_modeinfo */
	int mode_w, mode_h;
	uint32_t mode_blob, gamma_blob;
};

struct kms {
	int fd;
	struct kms_out dpi, hdmi;
	uint32_t overlays[16];
	int n_overlays;
	struct kms_buf text, gray, glow, scan, blank, dpi2;
	struct kms_buf colour;   /* YUV 4:2:0 camera picture for colour mode */
	int dpi_doubled;    /* the DPI scaler failed: use the dpi2 buffer */
	int scan_w, scan_h;
	struct canvas text_cv, gray_cv, dpi2_cv;
	uint8_t *mem;       /* in-memory fallback when no display exists */
};

int kms_open(struct kms *k);
int kms_apply(struct kms *k, const struct settings *s);
int kms_poll_hotplug(struct kms *k, const struct settings *s);
void kms_update_glow(struct kms *k);
/* Colour mode: copy a YUV 4:2:0 camera frame for HDMI to show. */
void kms_colour_frame(struct kms *k, const struct settings *s, const uint8_t *yuv, int w, int h, int stride);
void kms_close(struct kms *k);

#endif
