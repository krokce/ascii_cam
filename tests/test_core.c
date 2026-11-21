/* Tests for the conversion, drawing, recording and timing code, run on
 * the build computer against test images and a framebuffer held in
 * memory. Run with "make test" in the src directory. Each test prints
 * one plain sentence saying what it checked. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "asciicam.h"

static int failures, checks;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; \
	printf("FAILED at line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define IMG_W 320
#define IMG_H 240
static uint8_t img[IMG_W * IMG_H];
static uint32_t fbmem[DOTS_W * 2 * DOTS_H];

static struct canvas make_canvas(int w)
{
	struct canvas cv = { fbmem, w, DOTS_H, w };
	memset(fbmem, 0, sizeof(fbmem));
	return cv;
}

static void img_gradient(void)
{
	for (int y = 0; y < IMG_H; y++)
		for (int x = 0; x < IMG_W; x++)
			img[y * IMG_W + x] = (uint8_t)(x * 255 / (IMG_W - 1));
}

/* Saves the in-memory framebuffer as a gray picture file, so the result
 * can be looked at on a PC if wanted. */
static void save_pgm(const char *name, struct canvas *cv)
{
	char path[256];
	FILE *f;
	mkdir("out", 0777);
	snprintf(path, sizeof(path), "out/%s.pgm", name);
	f = fopen(path, "wb");
	if (!f)
		return;
	fprintf(f, "P5\n%d %d\n255\n", cv->w, cv->h);
	for (int y = 0; y < cv->h; y++)
		for (int x = 0; x < cv->w; x++)
			fputc((int)(cv->px[y * cv->stride + x] & 0xFF), f);
	fclose(f);
}

static int lit_in_cell(struct canvas *cv, int cols, int col, int row)
{
	int dw = DOTS_W / cols, n = 0;
	for (int y = 0; y < CELL_H; y++)
		for (int x = 0; x < dw; x++)
			n += cv->px[(row * CELL_H + y) * cv->stride + col * dw + x] != PIX_OFF;
	return n;
}

static void run(struct settings *s, struct grid *g)
{
	struct conv_state st = { 0 };
	conv_image_to_grid(&st, s, img, IMG_W, IMG_H, IMG_W, g);
}

static int tone_of(const struct settings *s, struct cell c)
{
	if (s->mode == MODE_SHADE) {
		static const uint8_t sh[4] = { CH_LIGHT, CH_MEDIUM, CH_DARK, CH_FULL };
		if (c.ch == ' ')
			return 0;
		for (int k = 0; k < 4; k++)
			if (sh[k] == c.ch)
				return 1 + 2 * k + c.bright;
		return -1;
	}
	const char *p = strchr(s->ramp, c.ch);
	if (!p)
		return -1;
	int k = (int)(p - s->ramp);
	return k == 0 ? 0 : 2 * k - 1 + c.bright;
}

static void test_characters_mode(void)
{
	struct settings s;
	static struct grid g, prev;
	struct canvas cv = make_canvas(DOTS_W);
	int mono = 1, seen[64] = { 0 }, distinct = 0;

	settings_default(&s);
	s.mode = MODE_CHARS;
	img_gradient();
	run(&s, &g);
	for (int x = 1; x < 80; x++)
		if (tone_of(&s, g.c[12 * 80 + x]) < tone_of(&s, g.c[12 * 80 + x - 1]))
			mono = 0;
	for (int x = 0; x < 80; x++) {
		int t = tone_of(&s, g.c[12 * 80 + x]);
		if (t >= 0 && !seen[t]++)
			distinct++;
	}
	CHECK(g.c[12 * 80].ch == ' ', "characters mode: the darkest cell should be a space");
	CHECK(g.c[12 * 80 + 79].ch == '@' && g.c[12 * 80 + 79].bright, "characters mode: the brightest cell should be a bright at sign");
	CHECK(mono, "characters mode: tones should rise from left to right");
	CHECK(distinct >= 17, "characters mode: expected about 19 tones, saw %d", distinct);
	int n = render_grid(&cv, &g, &prev, 1);
	CHECK(n == 2000, "characters mode: all 2000 cells should be drawn the first time");
	CHECK(lit_in_cell(&cv, 80, 79, 12) > lit_in_cell(&cv, 80, 0, 12), "characters mode: right cells should have more lit dots");
	CHECK(cv.px[0] == PIX_OFF, "characters mode: the top left dot should be off");
	save_pgm("mode-characters", &cv);
	printf("Characters mode: a left-to-right gradient gave %d distinct tones, from a space to a bright at sign, rising steadily.\n", distinct);
}

static void test_shade_mode(void)
{
	struct settings s;
	static struct grid g, prev;
	struct canvas cv = make_canvas(DOTS_W);
	int mono = 1, seen[16] = { 0 }, distinct = 0;

	settings_default(&s);
	img_gradient();
	run(&s, &g);
	for (int x = 1; x < 80; x++)
		if (tone_of(&s, g.c[5 * 80 + x]) < tone_of(&s, g.c[5 * 80 + x - 1]))
			mono = 0;
	for (int x = 0; x < 80; x++) {
		int t = tone_of(&s, g.c[5 * 80 + x]);
		if (t >= 0 && !seen[t]++)
			distinct++;
	}
	CHECK(s.mode == MODE_SHADE, "shade blocks should be the default mode");
	CHECK(distinct == 9, "shade mode: expected 9 tones, saw %d", distinct);
	CHECK(mono, "shade mode: tones should rise from left to right");
	CHECK(g.c[5 * 80 + 79].ch == CH_FULL && g.c[5 * 80 + 79].bright, "shade mode: the brightest cell should be a bright full block");
	render_grid(&cv, &g, &prev, 1);
	CHECK(lit_in_cell(&cv, 80, 79, 5) == CELL_W * CELL_H, "shade mode: a full block should light all 126 dots");
	CHECK(cv.px[5 * CELL_H * DOTS_W + 79 * CELL_W] == PIX_BRIGHT, "shade mode: the bright full block should use the bright level");
	save_pgm("mode-shade", &cv);

	/* Redraw only what changed. */
	CHECK(render_grid(&cv, &g, &prev, 0) == 0, "shade mode: an unchanged frame should redraw no cells");
	g.c[100].ch = CH_FULL;
	g.c[100].bright = !prev.c[100].bright || prev.c[100].ch != CH_FULL;
	CHECK(render_grid(&cv, &g, &prev, 0) == 1, "shade mode: one changed cell should redraw exactly one cell");
	printf("Shade blocks mode: a gradient gave all 9 tones in order, and a repeated frame redrew no cells while one changed cell redrew one.\n");
}

static void test_half_mode(void)
{
	struct settings s;
	static struct grid g, prev;
	struct canvas cv = make_canvas(DOTS_W);
	int upper = 0;

	settings_default(&s);
	s.mode = MODE_HALF;
	/* 50 horizontal bands, alternately white and black: the top half of
	 * every cell row is white and the bottom half black. */
	for (int y = 0; y < IMG_H; y++)
		for (int x = 0; x < IMG_W; x++)
			img[y * IMG_W + x] = ((y * 50 / IMG_H) % 2 == 0) ? 255 : 0;
	run(&s, &g);
	for (int i = 0; i < 2000; i++)
		upper += g.c[i].ch == CH_UPPER;
	CHECK(upper >= 1900, "half blocks: expected upper half blocks nearly everywhere, got %d", upper);
	render_grid(&cv, &g, &prev, 1);
	CHECK(cv.px[(3 * CELL_H + 0) * DOTS_W + 40 * CELL_W] != PIX_OFF, "half blocks: the top line of a cell should be lit");
	CHECK(cv.px[(3 * CELL_H + 13) * DOTS_W + 40 * CELL_W] == PIX_OFF, "half blocks: the bottom line of a cell should be dark");
	save_pgm("mode-halfblocks", &cv);

	/* A vertical gradient checks the 80 by 50 resolution. */
	for (int y = 0; y < IMG_H; y++)
		for (int x = 0; x < IMG_W; x++)
			img[y * IMG_W + x] = (uint8_t)(y * 255 / (IMG_H - 1));
	run(&s, &g);
	CHECK(g.c[0].ch == ' ' && g.c[24 * 80].ch == CH_FULL, "half blocks: a vertical gradient should go from empty to full");
	printf("Half blocks mode: alternating bands half a character high gave upper half blocks in %d of 2000 cells, showing the 80 by 50 grid.\n", upper);
}

static void test_shape_mode(void)
{
	struct settings s;
	static struct grid g, prev;
	struct canvas cv = make_canvas(DOTS_W);
	int left = 0, upper = 0;

	settings_default(&s);
	s.mode = MODE_SHAPE;
	/* Left half of every cell bright, right half dark. */
	for (int y = 0; y < IMG_H; y++)
		for (int x = 0; x < IMG_W; x++)
			img[y * IMG_W + x] = ((x * 160 / IMG_W) % 2 == 0) ? 255 : 0;
	run(&s, &g);
	for (int i = 0; i < 2000; i++)
		left += g.c[i].ch == CH_LEFT;
	CHECK(left >= 1900, "shape matching: expected left half blocks, got %d", left);

	/* Top half of every cell bright. */
	for (int y = 0; y < IMG_H; y++)
		for (int x = 0; x < IMG_W; x++)
			img[y * IMG_W + x] = ((y * 50 / IMG_H) % 2 == 0) ? 255 : 0;
	run(&s, &g);
	for (int i = 0; i < 2000; i++)
		upper += g.c[i].ch == CH_UPPER;
	CHECK(upper >= 1900, "shape matching: expected upper half blocks, got %d", upper);

	/* A diagonal pattern: top left and bottom right quarters lit. The
	 * chosen glyph must light more of those quarters than the others. */
	{
		float v[160 * 50];
		uint16_t rows[CELL_H];
		int on = 0, off = 0;
		for (int y = 0; y < 50; y++)
			for (int x = 0; x < 160; x++)
				v[y * 160 + x] = ((x & 1) == (y & 1)) ? 1.0f : 0.0f;
		conv_levels_to_grid(&s, v, &g);
		glyph_rows(g.c[0].ch, rows);
		for (int y = 0; y < CELL_H; y++)
			for (int x = 0; x < CELL_W; x++) {
				int lit = (rows[y] >> (CELL_W - 1 - x)) & 1;
				int diag = (x < 4.5) == (y < 7);
				if (diag) on += lit; else off += lit;
			}
		CHECK(on > off, "shape matching: the diagonal pattern chose %c, which is not diagonal", g.c[0].ch);
		printf("Shape matching mode: left-half and top-half patterns gave left and upper half blocks in %d and %d of 2000 cells, and a diagonal pattern chose character code %d.\n", left, upper, g.c[0].ch);
	}
	img_gradient();
	run(&s, &g);
	render_grid(&cv, &g, &prev, 1);
	save_pgm("mode-shapes", &cv);
}

static void test_dither_and_grid40(void)
{
	struct settings s;
	static struct grid g, prev;
	struct canvas cv = make_canvas(DOTS_W);
	float v[80 * 25];
	int a = 0, b = 0;

	settings_default(&s);
	s.dithering = 1;
	for (int i = 0; i < 2000; i++)
		v[i] = 0.5f / 8.0f;     /* halfway between the first two tones */
	conv_levels_to_grid(&s, v, &g);
	for (int i = 0; i < 2000; i++) {
		a += g.c[i].ch == ' ';
		b += g.c[i].ch == CH_LIGHT && !g.c[i].bright;
	}
	CHECK(a > 600 && b > 600 && a + b == 2000, "dithering: expected a mix of two tones, got %d and %d", a, b);

	settings_default(&s);
	s.mode = MODE_CHARS;
	s.cols = 40;
	img_gradient();
	run(&s, &g);
	CHECK(g.cols == 40, "40 by 25 grid: expected 40 columns");
	render_grid(&cv, &g, &prev, 1);
	{
		int same = 1;
		for (int y = 0; y < DOTS_H; y++)
			for (int x = 0; x < DOTS_W; x += 2)
				if (cv.px[y * DOTS_W + x] != cv.px[y * DOTS_W + x + 1])
					same = 0;
		CHECK(same, "40 by 25 grid: every dot should be drawn twice as wide");
	}
	save_pgm("grid-40x25", &cv);

	/* A doubled buffer, 1440 wide, as the DPI port could use. */
	struct canvas cv2 = make_canvas(DOTS_W * 2);
	static struct grid prev2;
	settings_default(&s);
	img_gradient();
	run(&s, &g);
	render_grid(&cv2, &g, &prev2, 1);
	CHECK(cv2.px[5 * CELL_H * 1440 + 1439] == PIX_BRIGHT, "doubled buffer: the last dot should be bright");
	printf("Dithering mixed two neighbouring tones in %d and %d cells; the 40 by 25 grid drew every dot twice as wide.\n", a, b);
}

static void test_pattern_and_gray(void)
{
	static struct grid g, prev, gprev;
	struct canvas cv = make_canvas(DOTS_W);
	testpattern_grid(&g, 80, 1234, "No camera found.");
	CHECK(g.c[0].ch == CH_TL && g.c[79].ch == CH_TR, "test pattern: corners missing");
	render_grid(&cv, &g, &prev, 1);
	save_pgm("testpattern", &cv);
	testpattern_grid(&g, 80, 1235, "No camera found.");
	int n = render_grid(&cv, &g, &prev, 0);
	CHECK(n > 0 && n < 40, "test pattern: counting up should change only a few cells, changed %d", n);

	struct settings s;
	settings_default(&s);
	img_gradient();
	run(&s, &g);
	struct canvas gc = make_canvas(DOTS_W);
	render_gray(&gc, &g, &gprev, 1, MODE_SHADE);
	int levels[16] = { 0 }, nl = 0;
	for (int x = 0; x < 80; x++) {
		uint32_t p = gc.px[5 * CELL_H * DOTS_W + x * CELL_W];
		int lv = (int)(p & 0xFF) / 17;
		if (!levels[lv]++)
			nl++;
	}
	CHECK(nl >= 15, "gray view: expected 16 gray levels, saw %d", nl);
	save_pgm("hdmi-gray", &gc);
	printf("The test pattern showed its border and counter, and counting changed only %d cells. The HDMI gray view showed %d gray levels.\n", n, nl);
}


static int count_lines_starting(const char *path, char c)
{
	char line[512];
	int n = 0;
	FILE *f = fopen(path, "r");
	if (!f)
		return -1;
	while (fgets(line, sizeof(line), f))
		n += line[0] == c;
	fclose(f);
	return n;
}

static void test_operating_modes(void)
{
	struct settings s;
	static struct grid g;
	char path[] = "out/settings-modes.txt";
	int comments, seen[16] = { 0 }, distinct = 0, full = 0;

	/* The button order: 1 shade, 2 characters, 3 colour, then 1 again. */
	CHECK(mode_next(MODE_SHADE) == MODE_CHARS && mode_next(MODE_CHARS) == MODE_COLOUR &&
	      mode_next(MODE_COLOUR) == MODE_SHADE && mode_next(MODE_SHAPE) == MODE_SHADE,
	      "modes: the button order is wrong");
	CHECK(mode_number(MODE_SHADE) == 1 && mode_number(MODE_CHARS) == 2 && mode_number(MODE_COLOUR) == 3,
	      "modes: the mode numbers are wrong");

	/* Rewriting the mode line keeps every comment. */
	mkdir("out", 0777);
	settings_write_default(path);
	comments = count_lines_starting(path, '#');
	CHECK(settings_set_mode(path, MODE_COLOUR) == 0, "modes: the mode line could not be rewritten");
	CHECK(count_lines_starting(path, '#') == comments, "modes: rewriting the mode line lost comments");
	settings_load(&s, path, NULL);
	CHECK(s.mode == MODE_COLOUR, "modes: settings.txt should now say colour");
	settings_set_mode(path, MODE_CHARS);
	settings_load(&s, path, NULL);
	CHECK(s.mode == MODE_CHARS && s.wifi == 0 && s.ssh == 0, "modes: settings.txt should now say characters, with Wi-Fi and SSH off");

	/* Wi-Fi, SSH and mode numbers in settings.txt. */
	FILE *f = fopen(path, "a");
	fputs("wifi = on\r\nssh = on\r\nmode = 3\r\n", f);
	fclose(f);
	settings_load(&s, path, NULL);
	CHECK(s.wifi && s.ssh && s.mode == MODE_COLOUR, "modes: wifi, ssh and mode = 3 were not read");

	/* In colour mode the MDA text uses the nine shade block tones. */
	settings_default(&s);
	s.mode = MODE_COLOUR;
	img_gradient();
	run(&s, &g);
	for (int x = 0; x < 80; x++) {
		struct settings sh = s;
		sh.mode = MODE_SHADE;
		int t = tone_of(&sh, g.c[5 * 80 + x]);
		if (t >= 0 && !seen[t]++)
			distinct++;
	}
	CHECK(distinct == 9, "modes: colour mode should give 9 shade tones on the MDA, saw %d", distinct);

	/* The banner: large letters of full blocks in the middle rows. */
	testpattern_grid(&g, 80, 0, "");
	banner_overlay(&g, 3);
	for (int i = 0; i < 2000; i++)
		full += g.c[i].ch == CH_FULL && i / 80 >= 10 && i / 80 <= 14;
	CHECK(full > 60, "modes: the banner should draw large letters, drew %d blocks", full);
	CHECK(g.c[12 * 80].ch == ' ', "modes: the banner band should be cleared");
	testpattern_grid(&g, 40, 0, "");
	banner_overlay(&g, 1);
	CHECK(g.cols == 40, "modes: the banner should fit a 40 column grid");
	printf("Operating modes: the button steps from mode 1 shade to mode 2 characters to mode 3 colour and back, the mode line of settings.txt is rewritten without losing comments, wifi and ssh are read, colour mode gives the MDA nine shade tones, and the mode banner draws %d blocks.\n", full);
}


static void test_settings_template(void)
{
	struct settings a, b;
	char path[] = "out/settings-template.txt";
	char line[512];
	int settings_lines = 0, documented = 0, possible_seen = 0;
	FILE *f;

	mkdir("out", 0777);
	settings_default(&a);
	a.mode = MODE_COLOUR;
	a.output = OUT_EMU;
	a.hdmi_colour = COL_RED;
	a.phosphor = COL_AMBER;
	a.cols = 40;
	strcpy(a.ramp, " .oO@");
	a.contrast = 140;
	a.frame_rate = 10;
	a.upside_down = 1;
	a.wifi = 1;
	a.ssh = 1;
	CHECK(settings_write(path, &a) == 0, "template: could not write settings");
	CHECK(settings_is_current(path), "template: the version marker is missing");
	settings_load(&b, path, NULL);
	CHECK(!memcmp(&a, &b, sizeof(a)), "template: values did not survive writing and reading");

	/* Every setting line must follow a "Possible values" comment. */
	f = fopen(path, "r");
	while (f && fgets(line, sizeof(line), f)) {
		if (!strncmp(line, "# Possible values:", 18))
			possible_seen = 1;
		else if (line[0] != '#' && strchr(line, '=')) {
			settings_lines++;
			documented += possible_seen;
			possible_seen = 0;
		}
	}
	if (f)
		fclose(f);
	CHECK(settings_lines == 18 && documented == 18, "template: %d of %d settings list their possible values", documented, settings_lines);

	/* An old file without the marker is recognised as old. */
	f = fopen(path, "w");
	fputs("mode = shade\r\n", f);
	fclose(f);
	CHECK(!settings_is_current(path), "template: an old file should be recognised");
	printf("The settings template lists the possible values and the default for all %d settings, and non-default values survive writing and reading.\n", settings_lines);
}

static void warn_collect(const char *m) { (void)m; checks++; }

static void test_settings(void)
{
	struct settings a, b;
	char path[] = "out/settings-test.txt";
	mkdir("out", 0777);
	CHECK(settings_write_default(path) == 0, "settings: could not write the default file");
	settings_default(&a);
	CHECK(settings_load(&b, path, NULL) == 0, "settings: could not read the default file");
	CHECK(!memcmp(&a, &b, sizeof(a)), "settings: the default file should read back as the defaults");
	FILE *f = fopen(path, "a");
	fputs("mode = shapes\r\nframe_rate = 99\r\nbogus = 1\r\nramp = \" .oO@\"\r\n", f);
	fclose(f);
	settings_load(&b, path, warn_collect);
	CHECK(b.mode == MODE_SHAPE && b.frame_rate == 15 && !strcmp(b.ramp, " .oO@"), "settings: changed values not read correctly");
	printf("The settings file was written with defaults, read back unchanged, and a bad frame rate fell back to the default.\n");
}

static int count_files(const char *dir, const char *suffix)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	int n = 0;
	if (!d)
		return 0;
	while ((e = readdir(d)))
		if (strstr(e->d_name, suffix))
			n++;
	closedir(d);
	return n;
}

static void test_recording(void)
{
	char dir[] = "out/rec-XXXXXX";
	static struct grid g, back;
	struct settings s;
	char path[256];
	uint8_t *buf;
	FILE *f;
	long size;
	int frames = 0;
	uint32_t ms, times[16];

	mkdir("out", 0777);
	CHECK(mkdtemp(dir) != NULL, "recording: no temporary folder");
	settings_default(&s);
	img_gradient();
	run(&s, &g);

	struct recorder *r = recorder_open(dir, 42, MODE_SHADE, 80);
	uint64_t t0 = 1000000;
	recorder_frame(r, &g, t0);              /* written */
	recorder_frame(r, &g, t0 + 66);         /* identical: skipped */
	g.c[0].ch = 'X';
	recorder_frame(r, &g, t0 + 133);        /* changed: written */
	recorder_frame(r, &g, t0 + 5200);       /* identical but 5 s later: written */
	recorder_frame(r, &g, t0 + 31 * 60000); /* 31 minutes later: new file */
	recorder_close(r);

	snprintf(path, sizeof(path), "%s/boot00042-part0001.acr", dir);
	f = fopen(path, "rb");
	CHECK(f != NULL, "recording: the first file is missing");
	if (!f)
		return;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = malloc((size_t)size + 100);
	CHECK(fread(buf, 1, (size_t)size, f) == (size_t)size, "recording: read failed");
	fclose(f);
	/* Simulate a power cut in the middle of the next record. */
	memset(buf + size, 0x46, 100);
	size += 100;

	CHECK(!memcmp(buf, REC_MAGIC, 8), "recording: wrong magic");
	CHECK(buf[12] == 80 && buf[14] == 25 && buf[16] == 42 && buf[20] == 1, "recording: header fields wrong");
	CHECK(crc32_calc(buf, 60) == (uint32_t)(buf[60] | buf[61] << 8 | buf[62] << 16 | (uint32_t)buf[63] << 24), "recording: header checksum wrong");
	size_t rs = rec_record_size(80, 25);
	CHECK(rs == 2262, "recording: a record should be 2262 bytes, is %zu", rs);
	for (long off = REC_HEADER_SIZE; off + (long)rs <= size; off += (long)rs) {
		if (rec_decode_frame(buf + off, rs, 80, 25, &back, &ms) != 0)
			break;
		times[frames++] = ms;
	}
	CHECK(frames == 3, "recording: expected 3 frames, decoded %d", frames);
	CHECK(frames == 3 && times[0] == 0 && times[1] == 133 && times[2] == 5200, "recording: wrong timestamps");
	CHECK(back.c[0].ch == 'X' && back.c[79 + 5 * 80].bright, "recording: frame content wrong");
	CHECK(count_files(dir, ".acr") == 2, "recording: a second file should start after 30 minutes");
	free(buf);

	/* Cleanup with a limit of 0 percent deletes all but the newest file. */
	rec_cleanup(dir, 0);
	CHECK(count_files(dir, ".acr") == 1, "recording: cleanup should keep exactly the newest file");

	/* Keep a copy for the Python replay test. */
	snprintf(path, sizeof(path), "cp %s/boot00042-part0002.acr out/sample.acr", dir);
	CHECK(system(path) == 0, "recording: copy failed");
	{
		struct recorder *r2;
		char d2[] = "out/sample";
		mkdir(d2, 0777);
		if (system("rm -f out/sample/*.acr") != 0)
			printf("Could not empty out/sample.\n");
		r2 = recorder_open(d2, 1, MODE_SHADE, 80);
		for (int i = 0; i < 30; i++) {
			testpattern_grid(&g, 80, (unsigned)i, "Replay test");
			recorder_frame(r2, &g, 5000000 + (uint64_t)i * 100);
		}
		recorder_close(r2);
		f = fopen("out/sample/boot00001-part0001.acr", "ab");
		fwrite("FRAMpartial", 1, 11, f);   /* a torn last record */
		fclose(f);
	}
	printf("A recording was written and decoded: 3 of 5 frames were stored, the identical frame within 5 seconds was skipped, timestamps were 0, 133 and 5200 milliseconds, a torn record at the end was ignored, a new file began after 30 minutes, and cleanup kept the newest file.\n");
}

/* The timing test reads the DPI line from config.txt, works out the
 * pixel clock the Pi's clock driver will really produce, and checks the
 * resulting horizontal and vertical frequencies against MDA. */
static long param(const char *line, const char *key)
{
	char pat[64];
	const char *p;
	snprintf(pat, sizeof(pat), ",%s=", key);
	p = strstr(line, pat);
	return p ? strtol(p + strlen(pat), NULL, 10) : -1;
}

static double bcm2835_dpi_rate(double want, int *integer_div)
{
	/* The DPI clock divider has 4 integer and 8 fractional bits and no
	 * MASH noise shaping; its usable parents are the 19.2 MHz crystal
	 * and PLLD at 500 MHz (see clk-bcm2835.c in the pinned kernel). */
	static const double parents[2] = { 19.2e6, 500e6 };
	double best = 0;
	for (int i = 0; i < 2; i++) {
		unsigned long long div = (unsigned long long)((parents[i] * 4096.0) / want);
		div &= ~0xFULL;
		if (div < 4096) div = 4096;
		if (div > 0xFFF0) div = 0xFFF0;
		double rate = parents[i] * 256.0 / (double)(div >> 4);
		if (fabs(rate - want) < fabs(best - want)) {
			best = rate;
			*integer_div = (div & 0xFFF) == 0;
		}
	}
	return best;
}

static void test_timing(void)
{
	char line[1024] = "";
	FILE *f = fopen("../br2-external/board/asciicam/config.txt", "r");
	CHECK(f != NULL, "timing: config.txt not found");
	if (!f)
		return;
	while (fgets(line, sizeof(line), f))
		if (!strncmp(line, "dtoverlay=vc4-kms-dpi-generic,", 30))
			break;
	fclose(f);
	long clk = param(line, "clock-frequency");
	long ha = param(line, "hactive"), hfp = param(line, "hfp"), hs = param(line, "hsync"), hbp = param(line, "hbp");
	long va = param(line, "vactive"), vfp = param(line, "vfp"), vs = param(line, "vsync"), vbp = param(line, "vbp");
	long htotal = ha + hfp + hs + hbp, vtotal = va + vfp + vs + vbp;
	int intdiv = 0;
	/* The kernel stores the mode clock in kilohertz. */
	double real = bcm2835_dpi_rate((double)(clk / 1000) * 1000.0, &intdiv);
	double hz = real / (double)htotal, vz = hz / (double)vtotal;
	double dot = real / 2.0;   /* every MDA dot is two DPI pixels */

	CHECK(ha == 1440 && va == 350, "timing: the DPI mode should be 1440 by 350 (720 dots doubled)");
	CHECK(vtotal == 370, "timing: MDA has 370 lines in total, config has %ld", vtotal);
	CHECK(intdiv, "timing: the pixel clock should come from a whole-number divider");
	CHECK(fabs(hz - 18430.0) / 18430.0 < 0.005, "timing: horizontal %.1f Hz is more than half a percent from 18,430", hz);
	CHECK(fabs(vz - 49.8) / 49.8 < 0.005, "timing: vertical %.2f Hz is more than half a percent from 49.8", vz);
	CHECK(strstr(line, ",vsync-invert") && !strstr(line, "hsync-invert"), "timing: vertical sync must be active low and horizontal sync active high");
	CHECK(strstr(line, ",rgb565"), "timing: the 16-bit format is required for GPIO 18 and 19");
	double hsync_us = (double)hs / real * 1e6;
	CHECK(hsync_us > 7.0 && hsync_us < 9.5, "timing: horizontal sync pulse of %.2f microseconds is far from MDA", hsync_us);
	printf("Timing: the pixel clock will be %.6f megahertz, from a whole-number divider, giving an effective MDA dot clock of %.4f megahertz. "
	       "The horizontal frequency is %.1f hertz, %.3f percent from 18,430, and the vertical frequency is %.2f hertz. "
	       "The horizontal sync pulse lasts %.2f microseconds.\n",
	       real / 1e6, dot / 1e6, hz, (hz - 18430.0) / 184.30, vz, hsync_us);
}

int main(void)
{
	test_characters_mode();
	test_shade_mode();
	test_half_mode();
	test_shape_mode();
	test_dither_and_grid40();
	test_pattern_and_gray();
	test_settings();
	test_operating_modes();
	test_settings_template();
	test_recording();
	test_timing();
	if (failures) {
		printf("%d of %d checks failed.\n", failures, checks);
		return 1;
	}
	printf("All %d checks passed.\n", checks);
	return 0;
}
