/* The ASCII camera appliance program: camera in, text picture out to the
 * MDA monitor and HDMI, recording to the REC partition. BusyBox init
 * starts it at boot and restarts it if it ever stops. */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "asciicam.h"
#include "camera.h"
#include "kms.h"
#include "video.h"
#include "button.h"

#define REC_DIR "/rec"
#define SETTINGS_PATH REC_DIR "/settings.txt"
#define TEST_PATH REC_DIR "/test.txt"
#define LOG_PATH REC_DIR "/log.txt"
#define RECORDINGS_DIR REC_DIR "/recordings"

static volatile sig_atomic_t stop;

static void on_signal(int sig)
{
	(void)sig;
	stop = 1;
}

static void warn_setting(const char *msg)
{
	log_msg("%s", msg);
}

static uint32_t read_boot_number(void)
{
	FILE *f = fopen("/run/asciicam-boot", "r");
	unsigned b = 0;

	if (f) {
		if (fscanf(f, "%u", &b) != 1)
			b = 0;
		fclose(f);
	}
	return b;
}

static void sleep_ms(uint64_t ms)
{
	struct timespec ts = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
	while (nanosleep(&ts, &ts) == -1 && errno == EINTR && !stop)
		;
}

static const char *output_name(enum output_mode o)
{
	switch (o) {
	case OUT_BOTH: return "on both the MDA monitor and HDMI";
	case OUT_MDA: return "on the MDA monitor only";
	case OUT_HDMI: return "on HDMI only";
	case OUT_EMU: return "on HDMI in MDA emulation";
	}
	return "";
}

/* Draws a finished grid into every buffer that is in use. */
static void show(struct kms *k, const struct settings *s, const struct grid *g, int force)
{
	static struct grid prev_text, prev_gray, prev_dpi2;

	render_grid(&k->text_cv, g, &prev_text, force);
	if (k->dpi_doubled)
		render_grid(&k->dpi2_cv, g, &prev_dpi2, force);
	if (s->hdmi_gray && s->output != OUT_EMU && s->output != OUT_MDA)
		render_gray(&k->gray_cv, g, &prev_gray, force, s->mode);
	if (s->output == OUT_EMU && s->glow)
		kms_update_glow(k);
}

int main(void)
{
	struct settings s;
	struct kms k;
	struct camera cam;
	struct conv_state cs = { 0 };
	static struct grid g;
	struct recorder *rec = NULL;
	struct video *vid = NULL;
	uint32_t boot = read_boot_number();
	int have_rec = access(REC_DIR "/recordings", F_OK) == 0;
	int have_kms = 0, have_cam = 0, test_mode = 0, cam_fail = 0, force = 1;
	unsigned frame = 0;
	uint64_t t, last_frame = 0, last_poll = 0, last_cam_try = 0, last_test_check = 0, banner_until = 0;
	int button = -1;
	char status[80] = "";

	memset(&k, 0, sizeof(k));
	k.fd = -1;
	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGPIPE, SIG_IGN);

	if (have_rec)
		log_init(LOG_PATH, boot);
	if (have_rec && access(SETTINGS_PATH, F_OK) != 0) {
		if (settings_write_default(SETTINGS_PATH) == 0)
			log_msg("No settings.txt was found on REC, so one with the default values was written.");
	}
	if (settings_load(&s, SETTINGS_PATH, warn_setting) != 0)
		settings_default(&s);
	/* Bring a settings.txt from an older version up to date: the
	 * comments are renewed and every value is kept. */
	if (have_rec && !settings_is_current(SETTINGS_PATH)) {
		if (settings_write(SETTINGS_PATH, &s) == 0)
			log_msg("settings.txt was written by an older version, so its comments were updated. All values were kept.");
	}
	if (!have_rec)
		log_msg("The REC partition is not available, so the default settings are used and nothing is recorded.");

	/* The display driver may still be loading: try for up to 10 seconds. */
	for (t = now_ms(); !stop; sleep_ms(200)) {
		if (kms_open(&k) == 0) {
			have_kms = 1;
			break;
		}
		if (now_ms() - t > 10000) {
			log_msg("The display driver could not be opened, so nothing can be shown. Recording continues if a camera is present.");
			k.fd = -1;
			break;
		}
	}
	if (have_kms && !k.dpi.found && s.output != OUT_HDMI && s.output != OUT_EMU)
		log_msg("The DPI output for the MDA monitor is switched off in config.txt, so only HDMI can be used.");
	kms_apply(&k, &s);

	button = button_open();
	if (button < 0)
		log_msg("The mode button on GPIO 26 could not be set up, so the mode can only be changed in settings.txt.");
	if (s.mode == MODE_COLOUR && (s.output == OUT_MDA || !k.hdmi.found))
		log_msg("Colour mode shows the colour picture on HDMI only. With the current output setting, only the MDA text picture is shown.");

	log_msg("The appliance started in %s mode with a %d by 25 grid, showing the picture %s. Text recording is %s and video recording is %s.",
		mode_name(s.mode), s.cols, output_name(s.output), s.recording ? "on" : "off", s.video ? "on" : "off");

	while (!stop) {
		uint64_t now = now_ms();
		uint64_t interval = 1000u / (unsigned)s.frame_rate;
		int presses = button_presses(button);

		/* Each press steps to the next operating mode: 1 shade blocks,
		 * 2 characters, 3 colour. The choice is written into
		 * settings.txt, so it is kept after a restart. */
		if (presses > 0) {
			enum draw_mode old = s.mode;
			int was_colour = old == MODE_COLOUR;

			while (presses-- > 0)
				s.mode = mode_next(s.mode);
			if (have_rec && settings_set_mode(SETTINGS_PATH, s.mode) != 0)
				log_msg("The new mode could not be written to settings.txt, so it applies only until the next restart.");
			log_msg("The mode button was pressed. The appliance is now in mode %d, %s.", mode_number(s.mode), mode_name(s.mode));
			banner_until = now + 2000;
			/* Colour mode uses a larger camera picture. */
			if (have_cam && !s.video && was_colour != (s.mode == MODE_COLOUR)) {
				camera_close(&cam);
				have_cam = 0;
				last_cam_try = 0;
			}
			/* A new recording file, so its header names the new mode. */
			recorder_close(rec);
			rec = NULL;
			kms_apply(&k, &s);
			force = 1;
		}

		if (now - last_test_check >= 2000 || last_test_check == 0) {
			int was = test_mode;
			test_mode = access(TEST_PATH, F_OK) == 0;
			if (test_mode != was)
				log_msg(test_mode ? "The file test.txt was found, so the test pattern is shown instead of the camera picture."
					: "The file test.txt is gone, so the camera picture is shown again.");
			last_test_check = now;
		}

		/* Open the camera, or try again every 10 seconds. */
		if (!have_cam && !test_mode && (now - last_cam_try >= 10000 || last_cam_try == 0)) {
			int w = s.video ? 1280 : (s.mode == MODE_COLOUR ? 640 : 320);
			int h = s.video ? 720 : (s.mode == MODE_COLOUR ? 480 : 240);
			last_cam_try = now;
			if (camera_open(&cam, w, h, s.frame_rate, s.upside_down ^ s.mirror, s.upside_down) == 0) {
				have_cam = 1;
				cam_fail = 0;
				status[0] = 0;
				log_msg("The camera, %s, is running at %d by %d.", cam.name, cam.w, cam.h);
				if (s.video && have_rec && !vid)
					vid = video_open(RECORDINGS_DIR, boot, cam.w, cam.h, cam.stride, s.frame_rate);
			} else if (!status[0]) {
				snprintf(status, sizeof(status), "No camera found. Trying again every 10 seconds.");
				log_msg("No camera was found. Check the ribbon cable. The test pattern is shown and the camera is tried again every 10 seconds.");
			}
		}

		if (test_mode || !have_cam) {
			if (now - last_frame < interval) {
				sleep_ms(interval - (now - last_frame));
				continue;
			}
			last_frame = now;
			testpattern_grid(&g, s.cols, frame++, test_mode ? "Test mode: delete test.txt from REC to see the camera." : status);
			if (now < banner_until)
				banner_overlay(&g, mode_number(s.mode));
			show(&k, &s, &g, force);
			force = 0;
		} else {
			const uint8_t *yuv;
			struct settings eff = s;

			if (camera_get(&cam, &yuv, 1000) != 0) {
				if (++cam_fail >= 3) {
					log_msg("The camera stopped delivering pictures. It is closed and tried again in 10 seconds.");
					camera_close(&cam);
					have_cam = 0;
					video_close(vid);
					vid = NULL;
				}
				continue;
			}
			cam_fail = 0;
			now = now_ms();
			if (vid)
				video_frame(vid, yuv, cam.frame_size, now);
			if (last_frame && now - last_frame + 5 < interval) {
				camera_release(&cam);
				continue;
			}
			last_frame = now;
			if (cam.hw_flip) {
				eff.upside_down = 0;
				eff.mirror = 0;
			}
			conv_image_to_grid(&cs, &eff, yuv, cam.w, cam.h, cam.stride, &g);
			if (s.mode == MODE_COLOUR && have_kms)
				kms_colour_frame(&k, &s, yuv, cam.w, cam.h, cam.stride);
			camera_release(&cam);
			/* The banner is shown but never recorded. */
			if (now < banner_until) {
				static struct grid shown;
				shown = g;
				banner_overlay(&shown, mode_number(s.mode));
				show(&k, &s, &shown, force);
			} else {
				show(&k, &s, &g, force);
			}
			force = 0;
			if (have_rec && s.recording) {
				if (!rec)
					rec = recorder_open(RECORDINGS_DIR, boot, s.mode == MODE_COLOUR ? MODE_SHADE : s.mode, g.cols);
				recorder_frame(rec, &g, now);
			}
		}

		if (have_kms && now - last_poll >= 2000) {
			kms_poll_hotplug(&k, &s);
			last_poll = now;
		}
	}

	button_close(button);
	recorder_close(rec);
	video_close(vid);
	if (have_cam)
		camera_close(&cam);
	kms_close(&k);
	return 0;
}
