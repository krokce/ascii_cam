/* Shared definitions for the ASCII camera appliance. */
#ifndef ASCIICAM_H
#define ASCIICAM_H

#include <stdint.h>
#include <stddef.h>

/* The MDA picture: 80 columns of 9 dots and 25 rows of 14 lines. */
#define DOTS_W 720
#define DOTS_H 350
#define CELL_W 9
#define CELL_H 14
#define MAX_COLS 80
#define ROWS 25
#define MAX_CELLS (MAX_COLS * ROWS)

/* Pixel values in the shared framebuffer (ARGB8888). Off is fully
 * transparent black, so overlay planes can show through it. On the DPI
 * port only red bit 7 (video, GPIO 19) and red bit 6 (intensity,
 * GPIO 18) reach the monitor: 0x80 gives video only, 0xFF gives video
 * and intensity. HDMI colours them through the CRTC gamma table. */
#define PIX_OFF    0x00000000u
#define PIX_NORMAL 0xFF808080u
#define PIX_BRIGHT 0xFFFFFFFFu

enum draw_mode { MODE_CHARS, MODE_SHADE, MODE_HALF, MODE_SHAPE, MODE_COLOUR };
enum output_mode { OUT_BOTH, OUT_MDA, OUT_HDMI, OUT_EMU };
enum colour { COL_GREEN, COL_AMBER, COL_WHITE, COL_RED };

struct settings {
	enum draw_mode mode;
	enum output_mode output;
	enum colour hdmi_colour;
	int hdmi_gray;          /* 16 real gray levels on HDMI */
	enum colour phosphor;   /* emulation: green, amber or white */
	int scanlines;
	int glow;
	int cols;               /* 80 or 40 */
	char ramp[64];          /* characters mode ramp, darkest first */
	int dithering;
	int contrast;           /* percent, 50 to 200 */
	int frame_rate;         /* frames per second limit, 1 to 30 */
	int recording;
	int video;
	int upside_down;
	int mirror;
	int wifi;               /* read by the start-up script */
	int ssh;                /* read by the start-up script */
};

/* One character cell: a code page 437 character and the intensity bit. */
struct cell {
	uint8_t ch;
	uint8_t bright;
};

struct grid {
	int cols, rows;
	struct cell c[MAX_CELLS];
	uint8_t gray[MAX_CELLS];   /* 0 to 15, for the optional HDMI gray view */
};

/* A 32-bit pixel buffer, either the real framebuffer or one in memory. */
struct canvas {
	uint32_t *px;
	int w, h;
	int stride;   /* in pixels */
};

/* Code page 437 characters the program draws itself. */
#define CH_LIGHT  0xB0
#define CH_MEDIUM 0xB1
#define CH_DARK   0xB2
#define CH_FULL   0xDB
#define CH_LOWER  0xDC
#define CH_LEFT   0xDD
#define CH_RIGHT  0xDE
#define CH_UPPER  0xDF
#define CH_VLINE  0xB3
#define CH_HLINE  0xC4
#define CH_TL     0xDA
#define CH_TR     0xBF
#define CH_BL     0xC0
#define CH_BR     0xD9

/* settings.c */
void settings_default(struct settings *s);
int settings_load(struct settings *s, const char *path, void (*warn)(const char *msg));
int settings_write_default(const char *path);
int settings_write(const char *path, const struct settings *s);
int settings_is_current(const char *path);
const char *mode_name(enum draw_mode m);
const char *mode_keyword(enum draw_mode m);
enum draw_mode mode_next(enum draw_mode m);
int mode_number(enum draw_mode m);
int settings_set_mode(const char *path, enum draw_mode m);

/* font.c */
void glyph_rows(uint8_t ch, uint16_t rows[CELL_H]);

/* convert.c */
struct conv_state {
	int init;
	float lo, hi;
};
void conv_downsample(const uint8_t *y, int w, int h, int stride,
		     int gw, int gh, int flip, int mirror, float *out);
void conv_stretch(struct conv_state *st, float *v, int n, int contrast);
void conv_image_to_grid(struct conv_state *st, const struct settings *s,
			const uint8_t *y, int w, int h, int stride,
			struct grid *g);
void conv_levels_to_grid(const struct settings *s, const float *v, struct grid *g);

/* render.c */
int render_grid(struct canvas *cv, const struct grid *g, struct grid *prev, int force);
int render_gray(struct canvas *cv, const struct grid *g, struct grid *prev, int force, int mode);
void render_clear(struct canvas *cv);

/* testpattern.c */
void testpattern_grid(struct grid *g, int cols, unsigned frame, const char *status);
void banner_overlay(struct grid *g, int number);

/* recorder.c */
#define REC_MAGIC "ASCIIREC"
#define REC_VERSION 1
#define REC_HEADER_SIZE 64
#define REC_FRAME_MAGIC 0x4D415246u   /* "FRAM" read as little-endian */
uint32_t crc32_calc(const void *data, size_t len);
size_t rec_record_size(int cols, int rows);
size_t rec_encode_header(uint8_t *buf, int cols, int rows, uint32_t boot,
			 uint32_t part, int64_t real_start_ms, int mode);
size_t rec_encode_frame(uint8_t *buf, const struct grid *g, uint32_t ms);
int rec_decode_frame(const uint8_t *buf, size_t len, int cols, int rows,
		     struct grid *g, uint32_t *ms);

struct recorder;
struct recorder *recorder_open(const char *dir, uint32_t boot, int mode, int cols);
void recorder_frame(struct recorder *r, const struct grid *g, uint64_t now_ms);
void recorder_close(struct recorder *r);
int rec_cleanup(const char *dir, int max_percent);

/* log.c */
void log_init(const char *path, uint32_t boot);
void log_msg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* util */
uint64_t now_ms(void);

#endif
