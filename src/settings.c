/* Reading settings.txt from the REC partition. Each setting is one line
 * of the form "name = value". Lines starting with a hash sign are
 * comments. Unknown names and bad values are reported in the log and
 * the default is used instead. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "asciicam.h"

void settings_default(struct settings *s)
{
	memset(s, 0, sizeof(*s));
	s->mode = MODE_SHADE;
	s->output = OUT_BOTH;
	s->hdmi_colour = COL_GREEN;
	s->hdmi_gray = 0;
	s->phosphor = COL_GREEN;
	s->scanlines = 0;
	s->glow = 0;
	s->cols = 80;
	strcpy(s->ramp, " .:-=+*#%@");
	s->dithering = 0;
	s->contrast = 100;
	s->frame_rate = 15;
	s->recording = 1;
	s->video = 0;
	s->upside_down = 0;
	s->mirror = 0;
}

const char *mode_name(enum draw_mode m)
{
	switch (m) {
	case MODE_CHARS: return "characters";
	case MODE_SHADE: return "shade blocks";
	case MODE_HALF: return "half blocks";
	case MODE_SHAPE: return "shape matching";
	case MODE_COLOUR: return "colour";
	}
	return "unknown";
}

/* The word used for each mode in settings.txt. */
const char *mode_keyword(enum draw_mode m)
{
	switch (m) {
	case MODE_CHARS: return "characters";
	case MODE_SHADE: return "shade";
	case MODE_HALF: return "halfblocks";
	case MODE_SHAPE: return "shapes";
	case MODE_COLOUR: return "colour";
	}
	return "shade";
}

/* The three operating modes the button steps through:
 * 1 shade blocks, 2 characters, 3 colour. */
enum draw_mode mode_next(enum draw_mode m)
{
	switch (m) {
	case MODE_SHADE: return MODE_CHARS;
	case MODE_CHARS: return MODE_COLOUR;
	default: return MODE_SHADE;
	}
}

int mode_number(enum draw_mode m)
{
	switch (m) {
	case MODE_SHADE: return 1;
	case MODE_CHARS: return 2;
	case MODE_COLOUR: return 3;
	default: return 0;
	}
}

/* The settings file template. Each setting has a comment in plain words,
 * then a line listing every possible value, then its default. The
 * words in braces are replaced with the current values when the file
 * is written. The first line marks the version of this template, so
 * the program can bring older files up to date. */
#define SETTINGS_VERSION_LINE "# Settings file version 2"

static const char template_text[] =
SETTINGS_VERSION_LINE "\n"
"# Settings for the ASCII camera appliance.\n"
"# Each setting is one line: its name, an equals sign, and its value.\n"
"# Lines starting with a hash sign are comments and are ignored.\n"
"# The settings are read when the appliance starts, so after a change,\n"
"# switch the appliance off and on again. If this file is deleted, a\n"
"# fresh copy with the default values is written at the next start.\n"
"\n"
"# The operating mode. The push button on header pins 37 and 39 steps\n"
"# through the three main modes while the appliance runs, and rewrites\n"
"# this line:\n"
"#   shade, or 1, is mode 1: nine tones from shade block characters.\n"
"#   characters, or 2, is mode 2: the ramp of characters given below.\n"
"#   colour, or 3, is mode 3: the real colour camera video on HDMI,\n"
"#   while the MDA monitor shows shade blocks. color is also accepted.\n"
"# Two more drawing modes can be set here, but are not on the button:\n"
"#   halfblocks splits each cell into a top and a bottom half.\n"
"#   shapes picks the character whose shape best fits each cell, like aalib.\n"
"# Possible values: shade, characters, colour, color, halfblocks, shapes, 1, 2, 3.\n"
"# Default: shade.\n"
"mode = {mode}\n"
"\n"
"# Where the picture goes.\n"
"#   both: the MDA monitor and HDMI at the same time.\n"
"#   mda: the MDA monitor only; HDMI stays off.\n"
"#   hdmi: HDMI only; the MDA sync signals keep running, with a black picture.\n"
"#   emulation: HDMI only, made to look exactly like the MDA monitor. Use it\n"
"#   on a Pi that has no MDA monitor.\n"
"# Possible values: both, mda, hdmi, emulation.\n"
"# Default: both.\n"
"output = {output}\n"
"\n"
"# The text colour on HDMI, always on black, in the both and hdmi outputs.\n"
"# hdmi_color is also accepted as the name.\n"
"# Possible values: green, amber, white, red.\n"
"# Default: green.\n"
"hdmi_colour = {hdmi_colour}\n"
"\n"
"# Real gray shades on HDMI. When on, HDMI shows 16 levels of brightness\n"
"# while the MDA monitor keeps its three. hdmi_grey is also accepted.\n"
"# Possible values: on, off, yes, no, 1, 0.\n"
"# Default: off.\n"
"hdmi_gray = {hdmi_gray}\n"
"\n"
"# The phosphor colour in the emulation output, always on black.\n"
"# Possible values: green, amber, white.\n"
"# Default: green.\n"
"phosphor = {phosphor}\n"
"\n"
"# Faint scan lines in the emulation output.\n"
"# Possible values: on, off, yes, no, 1, 0.\n"
"# Default: off.\n"
"scanlines = {scanlines}\n"
"\n"
"# A soft glow round bright characters in the emulation output.\n"
"# Possible values: on, off, yes, no, 1, 0.\n"
"# Default: off.\n"
"glow = {glow}\n"
"\n"
"# The grid size. 40x25 draws every character twice as wide.\n"
"# Possible values: 80x25, 40x25.\n"
"# Default: 80x25.\n"
"grid = {grid}\n"
"\n"
"# The characters of the ramp for the characters mode, from darkest to\n"
"# brightest, between double quotes. The first one is normally a space.\n"
"# Each character except the first also appears in bright form.\n"
"# Possible values: 2 to 60 printable characters, letters, digits, signs\n"
"# or spaces, between double quotes.\n"
"# Default: \" .:-=+*#%@\".\n"
"ramp = {ramp}\n"
"\n"
"# Dithering spreads rounding errors over the neighbouring cells. It shows\n"
"# more tones but looks grainier. It has no effect in the shapes mode.\n"
"# Possible values: on, off, yes, no, 1, 0.\n"
"# Default: off.\n"
"dithering = {dithering}\n"
"\n"
"# The contrast in percent. The picture is always stretched to use the\n"
"# full range of tones, slowly following the scene so it does not\n"
"# flicker. This value makes it softer below 100 and harsher above 100.\n"
"# Possible values: any whole number from 50 to 200.\n"
"# Default: 100.\n"
"contrast = {contrast}\n"
"\n"
"# The highest number of frames per second.\n"
"# Possible values: any whole number from 1 to 30.\n"
"# Default: 15.\n"
"frame_rate = {frame_rate}\n"
"\n"
"# Recording of the text picture to this partition.\n"
"# Possible values: on, off, yes, no, 1, 0.\n"
"# Default: on.\n"
"recording = {recording}\n"
"\n"
"# Recording of real video, 1280 by 720, to this partition. It fills the\n"
"# card about eight times faster than the text recording and may slow\n"
"# the text picture.\n"
"# Possible values: on, off, yes, no, 1, 0.\n"
"# Default: off.\n"
"video = {video}\n"
"\n"
"# Set to on if the camera is mounted upside down.\n"
"# Possible values: on, off, yes, no, 1, 0.\n"
"# Default: off.\n"
"upside_down = {upside_down}\n"
"\n"
"# Set to on to show the picture mirrored left to right, as in a looking\n"
"# glass.\n"
"# Possible values: on, off, yes, no, 1, 0.\n"
"# Default: off.\n"
"mirror = {mirror}\n"
"\n"
"# Wi-Fi. When on, the appliance joins the network named in wifi.txt on\n"
"# this partition, and writes its network address to log.txt.\n"
"# Possible values: on, off, yes, no, 1, 0.\n"
"# Default: off.\n"
"wifi = {wifi}\n"
"\n"
"# SSH. When on, you can log in as root over the network with a key\n"
"# listed in authorized_keys.txt on this partition. Passwords are always\n"
"# refused. SSH also switches Wi-Fi on.\n"
"# Possible values: on, off, yes, no, 1, 0.\n"
"# Default: off.\n"
"ssh = {ssh}\n";

static const char *onoff(int v) { return v ? "on" : "off"; }

static const char *colour_word(enum colour c)
{
	switch (c) {
	case COL_AMBER: return "amber";
	case COL_WHITE: return "white";
	case COL_RED: return "red";
	default: return "green";
	}
}

static const char *output_word(enum output_mode o)
{
	switch (o) {
	case OUT_MDA: return "mda";
	case OUT_HDMI: return "hdmi";
	case OUT_EMU: return "emulation";
	default: return "both";
	}
}

/* Writes settings.txt with the full comments and the given values. The
 * file is written beside the old one and renamed over it, so a power
 * cut leaves one complete file. Windows line endings make it read well
 * in any PC editor. */
int settings_write(const char *path, const struct settings *s)
{
	char tmp[300], val[96];
	const char *p;
	FILE *f;

	snprintf(tmp, sizeof(tmp), "%s.new", path);
	f = fopen(tmp, "w");
	if (!f)
		return -1;
	for (p = template_text; *p; p++) {
		if (*p == '{') {
			const char *e = strchr(p, '}');
			char key[32];
			size_t n = e ? (size_t)(e - p - 1) : 0;

			if (!e || n >= sizeof(key)) {
				fputc(*p, f);
				continue;
			}
			memcpy(key, p + 1, n);
			key[n] = 0;
			val[0] = 0;
			if (!strcmp(key, "mode")) snprintf(val, sizeof(val), "%s", mode_keyword(s->mode));
			else if (!strcmp(key, "output")) snprintf(val, sizeof(val), "%s", output_word(s->output));
			else if (!strcmp(key, "hdmi_colour")) snprintf(val, sizeof(val), "%s", colour_word(s->hdmi_colour));
			else if (!strcmp(key, "hdmi_gray")) snprintf(val, sizeof(val), "%s", onoff(s->hdmi_gray));
			else if (!strcmp(key, "phosphor")) snprintf(val, sizeof(val), "%s", colour_word(s->phosphor));
			else if (!strcmp(key, "scanlines")) snprintf(val, sizeof(val), "%s", onoff(s->scanlines));
			else if (!strcmp(key, "glow")) snprintf(val, sizeof(val), "%s", onoff(s->glow));
			else if (!strcmp(key, "grid")) snprintf(val, sizeof(val), "%s", s->cols == 40 ? "40x25" : "80x25");
			else if (!strcmp(key, "ramp")) snprintf(val, sizeof(val), "\"%s\"", s->ramp);
			else if (!strcmp(key, "dithering")) snprintf(val, sizeof(val), "%s", onoff(s->dithering));
			else if (!strcmp(key, "contrast")) snprintf(val, sizeof(val), "%d", s->contrast);
			else if (!strcmp(key, "frame_rate")) snprintf(val, sizeof(val), "%d", s->frame_rate);
			else if (!strcmp(key, "recording")) snprintf(val, sizeof(val), "%s", onoff(s->recording));
			else if (!strcmp(key, "video")) snprintf(val, sizeof(val), "%s", onoff(s->video));
			else if (!strcmp(key, "upside_down")) snprintf(val, sizeof(val), "%s", onoff(s->upside_down));
			else if (!strcmp(key, "mirror")) snprintf(val, sizeof(val), "%s", onoff(s->mirror));
			else if (!strcmp(key, "wifi")) snprintf(val, sizeof(val), "%s", onoff(s->wifi));
			else if (!strcmp(key, "ssh")) snprintf(val, sizeof(val), "%s", onoff(s->ssh));
			fputs(val, f);
			p = e;
			continue;
		}
		if (*p == '\n')
			fputc('\r', f);
		fputc(*p, f);
	}
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	if (rename(tmp, path) != 0) {
		unlink(tmp);
		return -1;
	}
	return 0;
}

int settings_write_default(const char *path)
{
	struct settings s;

	settings_default(&s);
	return settings_write(path, &s);
}

/* Returns 1 if settings.txt was written by this version of the template. */
int settings_is_current(const char *path)
{
	char line[256];
	FILE *f = fopen(path, "r");
	int found = 0;

	if (!f)
		return 1;
	while (!found && fgets(line, sizeof(line), f))
		found = !strncmp(line, SETTINGS_VERSION_LINE, strlen(SETTINGS_VERSION_LINE));
	fclose(f);
	return found;
}

static char *trim(char *s)
{
	char *e;

	while (*s && isspace((unsigned char)*s))
		s++;
	e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1]))
		*--e = 0;
	return s;
}

static int parse_onoff(const char *v, int *out)
{
	if (!strcasecmp(v, "on") || !strcasecmp(v, "yes") || !strcmp(v, "1")) { *out = 1; return 0; }
	if (!strcasecmp(v, "off") || !strcasecmp(v, "no") || !strcmp(v, "0")) { *out = 0; return 0; }
	return -1;
}

static int parse_colour(const char *v, enum colour *out, int allow_red)
{
	if (!strcasecmp(v, "green")) { *out = COL_GREEN; return 0; }
	if (!strcasecmp(v, "amber")) { *out = COL_AMBER; return 0; }
	if (!strcasecmp(v, "white")) { *out = COL_WHITE; return 0; }
	if (allow_red && !strcasecmp(v, "red")) { *out = COL_RED; return 0; }
	return -1;
}

static int parse_int(const char *v, int lo, int hi, int *out)
{
	char *end;
	long n = strtol(v, &end, 10);

	if (end == v || *trim(end) || n < lo || n > hi)
		return -1;
	*out = (int)n;
	return 0;
}

int settings_load(struct settings *s, const char *path, void (*warn)(const char *msg))
{
	char line[256], msg[400];
	FILE *f;
	int lineno = 0;

	settings_default(s);
	f = fopen(path, "r");
	if (!f)
		return -1;
	while (fgets(line, sizeof(line), f)) {
		char *name, *val, *eq;
		int bad = 0;

		lineno++;
		name = trim(line);
		if (!*name || *name == '#')
			continue;
		eq = strchr(name, '=');
		if (!eq) {
			snprintf(msg, sizeof(msg), "Line %d of settings.txt has no equals sign, so it was ignored.", lineno);
			if (warn) warn(msg);
			continue;
		}
		*eq = 0;
		name = trim(name);
		val = trim(eq + 1);

		if (!strcasecmp(name, "mode")) {
			if (!strcasecmp(val, "shade")) s->mode = MODE_SHADE;
			else if (!strcasecmp(val, "characters")) s->mode = MODE_CHARS;
			else if (!strcasecmp(val, "halfblocks")) s->mode = MODE_HALF;
			else if (!strcasecmp(val, "shapes")) s->mode = MODE_SHAPE;
			else if (!strcasecmp(val, "colour") || !strcasecmp(val, "color")) s->mode = MODE_COLOUR;
			else if (!strcmp(val, "1")) s->mode = MODE_SHADE;
			else if (!strcmp(val, "2")) s->mode = MODE_CHARS;
			else if (!strcmp(val, "3")) s->mode = MODE_COLOUR;
			else bad = 1;
		} else if (!strcasecmp(name, "output")) {
			if (!strcasecmp(val, "both")) s->output = OUT_BOTH;
			else if (!strcasecmp(val, "mda")) s->output = OUT_MDA;
			else if (!strcasecmp(val, "hdmi")) s->output = OUT_HDMI;
			else if (!strcasecmp(val, "emulation")) s->output = OUT_EMU;
			else bad = 1;
		} else if (!strcasecmp(name, "hdmi_colour") || !strcasecmp(name, "hdmi_color")) {
			bad = parse_colour(val, &s->hdmi_colour, 1);
		} else if (!strcasecmp(name, "hdmi_gray") || !strcasecmp(name, "hdmi_grey")) {
			bad = parse_onoff(val, &s->hdmi_gray);
		} else if (!strcasecmp(name, "phosphor")) {
			bad = parse_colour(val, &s->phosphor, 0);
		} else if (!strcasecmp(name, "scanlines")) {
			bad = parse_onoff(val, &s->scanlines);
		} else if (!strcasecmp(name, "glow")) {
			bad = parse_onoff(val, &s->glow);
		} else if (!strcasecmp(name, "grid")) {
			if (!strcasecmp(val, "80x25")) s->cols = 80;
			else if (!strcasecmp(val, "40x25")) s->cols = 40;
			else bad = 1;
		} else if (!strcasecmp(name, "ramp")) {
			size_t n = strlen(val);
			if (n >= 4 && val[0] == '"' && val[n - 1] == '"' && n - 2 < sizeof(s->ramp)) {
				size_t i;
				for (i = 1; i < n - 1; i++)
					if ((unsigned char)val[i] < 32 || (unsigned char)val[i] > 126)
						bad = 1;
				if (!bad) {
					memcpy(s->ramp, val + 1, n - 2);
					s->ramp[n - 2] = 0;
				}
			} else {
				bad = 1;
			}
		} else if (!strcasecmp(name, "dithering")) {
			bad = parse_onoff(val, &s->dithering);
		} else if (!strcasecmp(name, "contrast")) {
			bad = parse_int(val, 50, 200, &s->contrast);
		} else if (!strcasecmp(name, "frame_rate")) {
			bad = parse_int(val, 1, 30, &s->frame_rate);
		} else if (!strcasecmp(name, "recording")) {
			bad = parse_onoff(val, &s->recording);
		} else if (!strcasecmp(name, "video")) {
			bad = parse_onoff(val, &s->video);
		} else if (!strcasecmp(name, "upside_down")) {
			bad = parse_onoff(val, &s->upside_down);
		} else if (!strcasecmp(name, "mirror")) {
			bad = parse_onoff(val, &s->mirror);
		} else if (!strcasecmp(name, "wifi")) {
			bad = parse_onoff(val, &s->wifi);
		} else if (!strcasecmp(name, "ssh")) {
			bad = parse_onoff(val, &s->ssh);
		} else {
			snprintf(msg, sizeof(msg), "Line %d of settings.txt names an unknown setting, %s, so it was ignored.", lineno, name);
			if (warn) warn(msg);
			continue;
		}
		if (bad) {
			snprintf(msg, sizeof(msg), "Line %d of settings.txt gives %s the value %s, which is not allowed, so the default is used.", lineno, name, val);
			if (warn) warn(msg);
		}
	}
	fclose(f);
	return 0;
}

/* Rewrites only the mode line of settings.txt, keeping every comment
 * and every other line as it is. If there is no mode line, one is
 * added at the end. The new file is written beside the old one and
 * then renamed over it, so a power cut leaves one complete file. */
int settings_set_mode(const char *path, enum draw_mode m)
{
	char tmp[300], line[512];
	FILE *in, *out;
	int done = 0;

	snprintf(tmp, sizeof(tmp), "%s.new", path);
	out = fopen(tmp, "w");
	if (!out)
		return -1;
	in = fopen(path, "r");
	if (in) {
		while (fgets(line, sizeof(line), in)) {
			char copy[512], *name, *eq;

			snprintf(copy, sizeof(copy), "%s", line);
			name = trim(copy);
			eq = strchr(name, '=');
			if (!done && *name != '#' && eq) {
				*eq = 0;
				if (!strcasecmp(trim(name), "mode")) {
					fprintf(out, "mode = %s\r\n", mode_keyword(m));
					done = 1;
					continue;
				}
			}
			fputs(line, out);
		}
		fclose(in);
	}
	if (!done)
		fprintf(out, "mode = %s\r\n", mode_keyword(m));
	fflush(out);
	fsync(fileno(out));
	fclose(out);
	if (rename(tmp, path) != 0) {
		unlink(tmp);
		return -1;
	}
	return 0;
}
