/* Turning a camera brightness image into a grid of character cells.
 *
 * 1. conv_downsample averages the brightness image down to the analysis
 *    grid: one value per cell, two per cell in half blocks mode, four
 *    per cell (quarters) in shape matching mode.
 * 2. conv_stretch spreads those values over the full range, following
 *    the scene's darkest and brightest parts slowly so the picture does
 *    not flicker, then applies the contrast setting.
 * 3. conv_levels_to_grid picks a character and intensity per cell for
 *    the chosen drawing mode, with optional error-diffusion dithering.
 */
#include <math.h>
#include <string.h>
#include "asciicam.h"

#define MAX_GW (MAX_COLS * 2)
#define MAX_GH (ROWS * 2)

/* Brightness of the two MDA levels, used when matching shapes. */
#define LEVEL_NORMAL 0.6f
#define LEVEL_BRIGHT 1.0f

void conv_downsample(const uint8_t *y, int w, int h, int stride,
		     int gw, int gh, int flip, int mirror, float *out)
{
	static int xbin[4096];
	uint32_t acc[MAX_GW];
	int x, row, gy, gx;

	if (w > 4096)
		w = 4096;
	for (x = 0; x < w; x++)
		xbin[x] = (int)((long)x * gw / w);

	row = 0;
	for (gy = 0; gy < gh; gy++) {
		int y1 = (int)((long)(gy + 1) * h / gh);
		int nrows = y1 - row;
		int oy = flip ? gh - 1 - gy : gy;

		memset(acc, 0, sizeof(acc[0]) * gw);
		for (; row < y1; row++) {
			const uint8_t *p = y + (size_t)row * stride;
			for (x = 0; x < w; x++)
				acc[xbin[x]] += p[x];
		}
		for (gx = 0; gx < gw; gx++) {
			int x0 = (int)(((long)gx * w + gw - 1) / gw);
			int x1 = (int)(((long)(gx + 1) * w + gw - 1) / gw);
			int ncols = x1 - x0;
			int ox = mirror ? gw - 1 - gx : gx;
			int count = ncols * nrows;

			out[oy * gw + ox] = count > 0 ? (float)acc[gx] / (float)count : 0.0f;
		}
	}
}

void conv_stretch(struct conv_state *st, float *v, int n, int contrast)
{
	int hist[256] = { 0 };
	int i, sum, plo = 0, phi = 255;
	float lo, hi, range, k;

	for (i = 0; i < n; i++) {
		int b = (int)v[i];
		hist[b < 0 ? 0 : (b > 255 ? 255 : b)]++;
	}
	/* The 2nd and 98th percentiles ignore a few extreme cells. */
	for (sum = 0, i = 0; i < 256; i++) {
		sum += hist[i];
		if (sum > n / 50) { plo = i; break; }
	}
	for (sum = 0, i = 255; i >= 0; i--) {
		sum += hist[i];
		if (sum > n / 50) { phi = i; break; }
	}
	if (!st->init) {
		st->lo = (float)plo;
		st->hi = (float)phi;
		st->init = 1;
	} else {
		/* Slow adaptation: about 5 percent of the way per frame,
		 * so a change of scene settles within one or two seconds. */
		st->lo += ((float)plo - st->lo) * 0.05f;
		st->hi += ((float)phi - st->hi) * 0.05f;
	}
	lo = st->lo;
	hi = st->hi;
	if (hi - lo < 24.0f) {
		float mid = (hi + lo) * 0.5f;
		lo = mid - 12.0f;
		hi = mid + 12.0f;
	}
	range = hi - lo;
	k = (float)contrast / 100.0f;
	for (i = 0; i < n; i++) {
		float t = (v[i] - lo) / range;
		t = 0.5f + (t - 0.5f) * k;
		v[i] = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	}
}

/* In colour mode the MDA monitor shows shade blocks. */
static enum draw_mode text_mode(const struct settings *s)
{
	return s->mode == MODE_COLOUR ? MODE_SHADE : s->mode;
}

/* Tone tables for characters and shade blocks modes, darkest first. */
static int build_tones(const struct settings *s, struct cell *tones)
{
	int n = 0, k, len;

	if (text_mode(s) == MODE_SHADE) {
		static const uint8_t sh[4] = { CH_LIGHT, CH_MEDIUM, CH_DARK, CH_FULL };
		tones[n].ch = ' '; tones[n++].bright = 0;
		for (k = 0; k < 4; k++) {
			tones[n].ch = sh[k]; tones[n++].bright = 0;
			tones[n].ch = sh[k]; tones[n++].bright = 1;
		}
		return n;
	}
	len = (int)strlen(s->ramp);
	if (len < 2)
		len = 0;
	if (len == 0) {
		static const char def[] = " .:-=+*#%@";
		len = (int)strlen(def);
		tones[n].ch = (uint8_t)def[0]; tones[n++].bright = 0;
		for (k = 1; k < len; k++) {
			tones[n].ch = (uint8_t)def[k]; tones[n++].bright = 0;
			tones[n].ch = (uint8_t)def[k]; tones[n++].bright = 1;
		}
		return n;
	}
	tones[n].ch = (uint8_t)s->ramp[0]; tones[n++].bright = 0;
	for (k = 1; k < len; k++) {
		tones[n].ch = (uint8_t)s->ramp[k]; tones[n++].bright = 0;
		tones[n].ch = (uint8_t)s->ramp[k]; tones[n++].bright = 1;
	}
	return n;
}

/* Quantise values 0..1 in place to 'levels' steps, returning the step
 * index per value; with dithering the rounding error is spread to the
 * neighbours (Floyd and Steinberg). */
static void quantise(float *v, int w, int h, int levels, int dither, int *idx)
{
	int x, y;
	float scale = (float)(levels - 1);

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			float t = v[y * w + x];
			int q;
			float err;

			t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
			q = (int)lrintf(t * scale);
			idx[y * w + x] = q;
			if (!dither)
				continue;
			err = t - (float)q / scale;
			if (x + 1 < w)
				v[y * w + x + 1] += err * 7.0f / 16.0f;
			if (y + 1 < h) {
				if (x > 0)
					v[(y + 1) * w + x - 1] += err * 3.0f / 16.0f;
				v[(y + 1) * w + x] += err * 5.0f / 16.0f;
				if (x + 1 < w)
					v[(y + 1) * w + x + 1] += err * 1.0f / 16.0f;
			}
		}
	}
}

/* Shape matching. Every candidate glyph is measured once: how much of
 * each quarter of its cell is lit. A cell is then given the glyph and
 * intensity whose quarters best match the image's four quarter values.
 * Results are cached per pattern of quarter values, quantised to 16
 * steps, so each pattern is worked out only once. */
struct candidate {
	uint8_t ch;
	float q[4];   /* top left, top right, bottom left, bottom right */
};

static struct candidate cands[95 + 8];
static int ncands;
static uint16_t shape_cache[65536];   /* (ch << 1 | bright) + 1, 0 = unknown */

static void shape_init(void)
{
	static const uint8_t extra[8] = { CH_LIGHT, CH_MEDIUM, CH_DARK, CH_FULL,
					  CH_UPPER, CH_LOWER, CH_LEFT, CH_RIGHT };
	uint16_t rows[CELL_H];
	int i, y, x;

	if (ncands)
		return;
	for (i = 0; i < 95 + 8; i++) {
		uint8_t ch = i < 95 ? (uint8_t)(32 + i) : extra[i - 95];
		float q[4] = { 0, 0, 0, 0 };

		glyph_rows(ch, rows);
		for (y = 0; y < CELL_H; y++) {
			int top = y < CELL_H / 2 ? 0 : 2;
			for (x = 0; x < CELL_W; x++) {
				if (!(rows[y] & (1u << (CELL_W - 1 - x))))
					continue;
				if (x < 4)
					q[top] += 1.0f;
				else if (x == 4) {
					q[top] += 0.5f;
					q[top + 1] += 0.5f;
				} else
					q[top + 1] += 1.0f;
			}
		}
		cands[ncands].ch = ch;
		for (x = 0; x < 4; x++)
			cands[ncands].q[x] = q[x] / (4.5f * 7.0f);
		ncands++;
	}
}

static uint16_t shape_match(unsigned key)
{
	float t[4];
	float best = 1e9f;
	uint16_t res = 0;
	int i, k, b;

	for (k = 0; k < 4; k++)
		t[k] = (float)((key >> (k * 4)) & 15) / 15.0f;
	for (i = 0; i < ncands; i++) {
		for (b = 0; b < 2; b++) {
			float lv = b ? LEVEL_BRIGHT : LEVEL_NORMAL;
			float e = 0.0f;
			for (k = 0; k < 4; k++) {
				float d = t[k] - cands[i].q[k] * lv;
				e += d * d;
			}
			/* Prefer the plain normal space for empty cells. */
			if (e < best - 1e-6f) {
				best = e;
				res = (uint16_t)((cands[i].ch << 1) | b);
			}
		}
	}
	return res;
}

void conv_levels_to_grid(const struct settings *s, const float *vin, struct grid *g)
{
	static float v[MAX_GW * MAX_GH];
	static int idx[MAX_GW * MAX_GH];
	int cols = s->cols == 40 ? 40 : 80;
	int rows = ROWS;
	int i, x, y;

	g->cols = cols;
	g->rows = rows;

	if (text_mode(s) == MODE_CHARS || text_mode(s) == MODE_SHADE) {
		struct cell tones[2 * 64];
		int nt = build_tones(s, tones);

		memcpy(v, vin, sizeof(float) * cols * rows);
		quantise(v, cols, rows, nt, s->dithering, idx);
		for (i = 0; i < cols * rows; i++) {
			g->c[i] = tones[idx[i]];
			g->gray[i] = (uint8_t)lrintf(vin[i] * 15.0f);
		}
		return;
	}

	if (text_mode(s) == MODE_HALF) {
		/* Each half is off, normal or bright; the intensity bit is
		 * shared by the whole cell, so it follows the lit halves. */
		memcpy(v, vin, sizeof(float) * cols * rows * 2);
		quantise(v, cols, rows * 2, 3, s->dithering, idx);
		for (y = 0; y < rows; y++) {
			for (x = 0; x < cols; x++) {
				int qt = idx[(2 * y) * cols + x];
				int qb = idx[(2 * y + 1) * cols + x];
				struct cell *c = &g->c[y * cols + x];
				float mean = (vin[(2 * y) * cols + x] + vin[(2 * y + 1) * cols + x]) * 0.5f;

				if (qt && qb)
					c->ch = CH_FULL;
				else if (qt)
					c->ch = CH_UPPER;
				else if (qb)
					c->ch = CH_LOWER;
				else
					c->ch = ' ';
				c->bright = (qt == 2 || qb == 2);
				g->gray[y * cols + x] = (uint8_t)lrintf(mean * 15.0f);
			}
		}
		return;
	}

	/* MODE_SHAPE: four quarter values per cell. */
	shape_init();
	for (y = 0; y < rows; y++) {
		for (x = 0; x < cols; x++) {
			const float *r0 = vin + (2 * y) * (2 * cols) + 2 * x;
			const float *r1 = r0 + 2 * cols;
			float q[4] = { r0[0], r0[1], r1[0], r1[1] };
			unsigned key = 0;
			uint16_t m;
			int k;

			for (k = 0; k < 4; k++) {
				int qq = (int)lrintf(q[k] * 15.0f);
				qq = qq < 0 ? 0 : (qq > 15 ? 15 : qq);
				key |= (unsigned)qq << (k * 4);
			}
			m = shape_cache[key];
			if (!m) {
				m = (uint16_t)(shape_match(key) + 1);
				shape_cache[key] = m;
			}
			m--;
			g->c[y * cols + x].ch = (uint8_t)(m >> 1);
			g->c[y * cols + x].bright = (uint8_t)(m & 1);
			g->gray[y * cols + x] = (uint8_t)lrintf((q[0] + q[1] + q[2] + q[3]) * 0.25f * 15.0f);
		}
	}
}

void conv_image_to_grid(struct conv_state *st, const struct settings *s,
			const uint8_t *y, int w, int h, int stride, struct grid *g)
{
	static float v[MAX_GW * MAX_GH];
	int cols = s->cols == 40 ? 40 : 80;
	int gw = cols, gh = ROWS;
	int hflip = s->upside_down ^ s->mirror;

	if (text_mode(s) == MODE_HALF)
		gh = ROWS * 2;
	else if (text_mode(s) == MODE_SHAPE) {
		gw = cols * 2;
		gh = ROWS * 2;
	}
	conv_downsample(y, w, h, stride, gw, gh, s->upside_down, hflip, v);
	conv_stretch(st, v, gw * gh, s->contrast);
	conv_levels_to_grid(s, v, g);
}
