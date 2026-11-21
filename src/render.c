/* Drawing character cells into a 32-bit pixel buffer. Only cells that
 * differ from the previous frame are drawn. The buffer is normally
 * 720 by 350; a buffer twice as wide gets every dot drawn twice. */
#include <string.h>
#include "asciicam.h"

static void draw_cell(struct canvas *cv, int cols, int col, int row,
		      uint8_t ch, uint32_t on, uint32_t off, int solid)
{
	uint16_t rows[CELL_H];
	int xs = cv->w / DOTS_W;          /* 1, or 2 for a doubled buffer */
	int dw = DOTS_W / cols;           /* 9, or 18 for 40 columns */
	int rep = dw / CELL_W * xs;       /* pixels per font dot */
	int y, x, k;

	if (xs < 1)
		xs = 1;
	if (solid) {
		for (y = 0; y < CELL_H; y++)
			rows[y] = 0x1FF;
	} else {
		glyph_rows(ch, rows);
	}
	for (y = 0; y < CELL_H; y++) {
		uint32_t *p = cv->px + (size_t)(row * CELL_H + y) * cv->stride + (size_t)col * dw * xs;
		for (x = 0; x < CELL_W; x++) {
			uint32_t v = (rows[y] & (1u << (CELL_W - 1 - x))) ? on : off;
			for (k = 0; k < rep; k++)
				*p++ = v;
		}
	}
}

int render_grid(struct canvas *cv, const struct grid *g, struct grid *prev, int force)
{
	int i, n = 0;

	if (prev->cols != g->cols || prev->rows != g->rows) {
		force = 1;
		render_clear(cv);
	}
	for (i = 0; i < g->cols * g->rows; i++) {
		const struct cell *c = &g->c[i];

		if (!force && prev->c[i].ch == c->ch && prev->c[i].bright == c->bright)
			continue;
		draw_cell(cv, g->cols, i % g->cols, i / g->cols, c->ch,
			  c->bright ? PIX_BRIGHT : PIX_NORMAL, PIX_OFF, 0);
		prev->c[i] = *c;
		n++;
	}
	prev->cols = g->cols;
	prev->rows = g->rows;
	return n;
}

/* The optional HDMI gray view: 16 real gray levels. In shade blocks
 * mode each cell becomes a solid patch of its own gray; in the other
 * modes the character is drawn in its cell's gray. */
int render_gray(struct canvas *cv, const struct grid *g, struct grid *prev, int force, int mode)
{
	int i, n = 0;

	if (prev->cols != g->cols || prev->rows != g->rows) {
		force = 1;
		render_clear(cv);
	}
	for (i = 0; i < g->cols * g->rows; i++) {
		const struct cell *c = &g->c[i];
		uint8_t lv = g->gray[i] > 15 ? 15 : g->gray[i];
		uint32_t v = lv ? 0xFF000000u | (uint32_t)(lv * 17) * 0x010101u : PIX_OFF;

		if (!force && prev->c[i].ch == c->ch && prev->gray[i] == lv)
			continue;
		if (mode == MODE_SHADE)
			draw_cell(cv, g->cols, i % g->cols, i / g->cols, 0, v, PIX_OFF, 1);
		else
			draw_cell(cv, g->cols, i % g->cols, i / g->cols, c->ch,
				  lv ? v : PIX_NORMAL, PIX_OFF, 0);
		prev->c[i] = *c;
		prev->gray[i] = lv;
		n++;
	}
	prev->cols = g->cols;
	prev->rows = g->rows;
	return n;
}

void render_clear(struct canvas *cv)
{
	int y;

	for (y = 0; y < cv->h; y++)
		memset(cv->px + (size_t)y * cv->stride, 0, sizeof(uint32_t) * cv->w);
}
