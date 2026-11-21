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

static const char default_text[] =
"# Settings for the ASCII camera appliance.\n"
"# Each setting is one line: its name, an equals sign, and its value.\n"
"# Lines starting with a hash sign are comments and are ignored.\n"
"# The settings are read when the appliance starts, so after a change,\n"
"# switch the appliance off and on again. If this file is deleted, a\n"
"# fresh copy with the default values is written at the next start.\n"
"\n"
"# The operating mode. There are three main modes, and the push button\n"
"# on header pins 37 and 39 steps through them while the appliance runs:\n"
"# mode 1 is shade, the default, nine tones from shade block characters;\n"
"# mode 2 is characters, the ramp of characters given further down;\n"
"# mode 3 is colour, the real colour camera picture on HDMI, while the\n"
"# MDA monitor shows shade blocks. Two more drawing modes can be set\n"
"# here but are not on the button: halfblocks splits each cell into a\n"
"# top and a bottom half, and shapes picks the character whose shape\n"
"# best matches each cell, like aalib. A button press rewrites this line.\n"
"mode = shade\n"
"\n"
"# Where the picture goes. both means the MDA monitor and HDMI at once,\n"
"# and is the default. mda means the MDA monitor only. hdmi means HDMI\n"
"# only; the MDA sync signals keep running but the picture is black.\n"
"# emulation means HDMI only, made to look exactly like the MDA monitor;\n"
"# use it on a Pi that has no MDA monitor.\n"
"output = both\n"
"\n"
"# The text colour on HDMI in the both and hdmi outputs: green, amber,\n"
"# white or red, always on black. The default is green.\n"
"hdmi_colour = green\n"
"\n"
"# Real gray shades on HDMI, on or off. When on, HDMI shows 16 levels of\n"
"# brightness while the MDA monitor keeps its three. Off by default.\n"
"hdmi_gray = off\n"
"\n"
"# The phosphor colour in the emulation output: green, amber or white.\n"
"# The default is green.\n"
"phosphor = green\n"
"\n"
"# Faint scan lines in the emulation output, on or off. Off by default.\n"
"scanlines = off\n"
"\n"
"# A soft glow round bright characters in the emulation output, on or\n"
"# off. Off by default.\n"
"glow = off\n"
"\n"
"# The grid size: 80x25, the default, or 40x25, where every character\n"
"# is drawn twice as wide.\n"
"grid = 80x25\n"
"\n"
"# The characters of the ramp for the characters mode, from darkest to\n"
"# brightest, between double quotes. The first one is normally a space.\n"
"# Each character also appears in bright form, which doubles the tones.\n"
"ramp = \" .:-=+*#%@\"\n"
"\n"
"# Dithering, on or off. When on, rounding errors are spread over the\n"
"# neighbouring cells, which shows more tones but looks grainier. It has\n"
"# no effect in the shapes mode. Off by default.\n"
"dithering = off\n"
"\n"
"# The contrast in percent, from 50 to 200. 100 is the default. The\n"
"# picture is always stretched to use the full range of tones, slowly\n"
"# following the scene so it does not flicker; this value then makes\n"
"# it softer below 100 or harsher above 100.\n"
"contrast = 100\n"
"\n"
"# The highest number of frames per second, from 1 to 30. The default\n"
"# is 15.\n"
"frame_rate = 15\n"
"\n"
"# Recording of the text picture to this partition, on or off. On by\n"
"# default.\n"
"recording = on\n"
"\n"
"# Recording of real video, 1280 by 720, to this partition, on or off.\n"
"# It fills the card about eight times faster than the text recording\n"
"# and may slow the text picture. Off by default.\n"
"video = off\n"
"\n"
"# Set upside_down to on if the camera is mounted upside down.\n"
"# Off by default.\n"
"upside_down = off\n"
"\n"
"# Set mirror to on to show the picture mirrored left to right, as in a\n"
"# looking glass. Off by default.\n"
"mirror = off\n"
"\n"
"# Wi-Fi, on or off. When on, the appliance joins the network named in\n"
"# wifi.txt on this partition, and writes its network address to\n"
"# log.txt. Off by default.\n"
"wifi = off\n"
"\n"
"# SSH, on or off. When on, you can log in as root over the network with\n"
"# a key listed in authorized_keys.txt on this partition. Passwords are\n"
"# always refused. SSH also switches Wi-Fi on. Off by default.\n"
"ssh = off\n";

int settings_write_default(const char *path)
{
	FILE *f = fopen(path, "w");
	const char *p;

	if (!f)
		return -1;
	/* Windows line endings, so the file reads well in any PC editor. */
	for (p = default_text; *p; p++) {
		if (*p == '\n')
			fputc('\r', f);
		fputc(*p, f);
	}
	fflush(f);
	fclose(f);
	return 0;
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
