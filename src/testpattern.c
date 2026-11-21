/* The test pattern, shown instead of the camera picture when test.txt
 * exists on REC, or when no camera can be found. It has a border round
 * the whole screen, every tone of every drawing mode, and a frame
 * counter that counts up, so a frozen picture is easy to spot. */
#include <stdio.h>
#include <string.h>
#include "asciicam.h"

static void put(struct grid *g, int x, int y, uint8_t ch, int bright)
{
	if (x < 0 || y < 0 || x >= g->cols || y >= g->rows)
		return;
	g->c[y * g->cols + x].ch = ch;
	g->c[y * g->cols + x].bright = (uint8_t)bright;
	g->gray[y * g->cols + x] = (uint8_t)(ch == ' ' ? 0 : (bright ? 15 : 9));
}

static void text(struct grid *g, int x, int y, const char *s, int bright)
{
	for (; *s; s++, x++)
		put(g, x, y, (uint8_t)*s, bright);
}

void testpattern_grid(struct grid *g, int cols, unsigned frame, const char *status)
{
	static const char ramp[] = " .:-=+*#%@";
	static const uint8_t shades[4] = { CH_LIGHT, CH_MEDIUM, CH_DARK, CH_FULL };
	char buf[96];
	int x, y, k, n;

	g->cols = cols == 40 ? 40 : 80;
	g->rows = ROWS;
	memset(g->c, 0, sizeof(g->c));
	for (k = 0; k < MAX_CELLS; k++)
		g->c[k].ch = ' ';
	memset(g->gray, 0, sizeof(g->gray));

	/* Border. */
	for (x = 1; x < g->cols - 1; x++) {
		put(g, x, 0, CH_HLINE, 1);
		put(g, x, ROWS - 1, CH_HLINE, 1);
	}
	for (y = 1; y < ROWS - 1; y++) {
		put(g, 0, y, CH_VLINE, 1);
		put(g, g->cols - 1, y, CH_VLINE, 1);
	}
	put(g, 0, 0, CH_TL, 1);
	put(g, g->cols - 1, 0, CH_TR, 1);
	put(g, 0, ROWS - 1, CH_BL, 1);
	put(g, g->cols - 1, ROWS - 1, CH_BR, 1);

	text(g, 2, 2, "TEST PATTERN", 1);
	snprintf(buf, sizeof(buf), "Frame %08u", frame);
	text(g, g->cols - 2 - (int)strlen(buf), 2, buf, 1);

	/* Characters ramp, each character normal then bright. */
	text(g, 2, 4, "Characters:", 0);
	for (k = 0, x = 2; ramp[k]; k++) {
		put(g, x++, 6, (uint8_t)ramp[k], 0);
		put(g, x++, 6, (uint8_t)ramp[k], 1);
		x++;
	}

	/* Shade blocks ramp: nine tones, each three cells wide. */
	text(g, 2, 8, "Shade blocks:", 0);
	for (n = 0, x = 2; n < 9; n++) {
		uint8_t ch = n == 0 ? ' ' : shades[(n - 1) / 2];
		int br = n == 0 ? 0 : ((n - 1) & 1);
		for (k = 0; k < 3; k++)
			put(g, x++, 10, ch, br);
		x++;
	}

	/* Half blocks and intensity. */
	text(g, 2, 12, "Half blocks:", 0);
	x = 2;
	put(g, x++, 14, CH_UPPER, 0); put(g, x++, 14, CH_LOWER, 0); x++;
	put(g, x++, 14, CH_UPPER, 1); put(g, x++, 14, CH_LOWER, 1); x++;
	put(g, x++, 14, CH_FULL, 0); put(g, x++, 14, CH_FULL, 1); x++;
	put(g, x++, 14, CH_LEFT, 0); put(g, x++, 14, CH_RIGHT, 0);

	/* A large counter in the middle: the frame count in big digits made
	 * of full blocks, five cells high, so it can be read from afar. */
	{
		static const char *digits[10][5] = {
			{ "###", "# #", "# #", "# #", "###" },
			{ "  #", "  #", "  #", "  #", "  #" },
			{ "###", "  #", "###", "#  ", "###" },
			{ "###", "  #", "###", "  #", "###" },
			{ "# #", "# #", "###", "  #", "  #" },
			{ "###", "#  ", "###", "  #", "###" },
			{ "###", "#  ", "###", "# #", "###" },
			{ "###", "  #", "  #", "  #", "  #" },
			{ "###", "# #", "###", "# #", "###" },
			{ "###", "# #", "###", "  #", "###" },
		};
		unsigned v = frame % 10000;
		int d, ox = g->cols / 2 - 8;
		for (d = 0; d < 4; d++) {
			int dig = (int)((v / (d == 0 ? 1000 : d == 1 ? 100 : d == 2 ? 10 : 1)) % 10);
			for (y = 0; y < 5; y++)
				for (k = 0; k < 3; k++)
					if (digits[dig][y][k] == '#')
						put(g, ox + d * 4 + k, 16 + y, CH_FULL, 1);
		}
	}

	if (status && *status)
		text(g, 2, ROWS - 2, status, 1);
}

/* The mode banner shown for 2 seconds after a button press: "MODE 1",
 * "MODE 2" or "MODE 3" in large letters made of full blocks, on a
 * cleared band across the middle of the screen. */
void banner_overlay(struct grid *g, int number)
{
	static const char *letters[5][5] = {
		{ "# #", "###", "# #", "# #", "# #" },   /* M */
		{ "###", "# #", "# #", "# #", "###" },   /* O */
		{ "## ", "# #", "# #", "# #", "## " },   /* D */
		{ "###", "#  ", "###", "#  ", "###" },   /* E */
		{ "   ", "   ", "   ", "   ", "   " },   /* space */
	};
	static const char *digits[4][5] = {
		{ "###", "# #", "# #", "# #", "###" },
		{ " # ", "## ", " # ", " # ", "###" },
		{ "###", "  #", "###", "#  ", "###" },
		{ "###", "  #", "###", "  #", "###" },
	};
	const char *(*glyph[6])[5];
	int sx = g->cols >= 80 ? 2 : 1;          /* cells per dot, across */
	int cw = 3 * sx + sx;                    /* letter width plus gap */
	int width = 6 * cw - sx;
	int x0 = (g->cols - width) / 2, y0 = (ROWS - 5) / 2;
	int i, x, y, k;

	if (number < 0 || number > 3)
		number = 0;
	glyph[0] = &letters[0]; glyph[1] = &letters[1]; glyph[2] = &letters[2];
	glyph[3] = &letters[3]; glyph[4] = &letters[4]; glyph[5] = &digits[number];

	/* Clear a band two rows above and below the letters. */
	for (y = y0 - 2; y < y0 + 7; y++)
		for (x = 0; x < g->cols; x++) {
			g->c[y * g->cols + x].ch = ' ';
			g->c[y * g->cols + x].bright = 0;
			g->gray[y * g->cols + x] = 0;
		}
	for (i = 0; i < 6; i++)
		for (y = 0; y < 5; y++)
			for (x = 0; x < 3; x++)
				if ((*glyph[i])[y][x] == '#')
					for (k = 0; k < sx; k++)
						put(g, x0 + i * cw + x * sx + k, y0 + y, CH_FULL, 1);
}
