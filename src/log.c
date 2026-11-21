/* The error log on the REC partition: one plain sentence per line,
 * each starting with the boot number and the seconds since start-up.
 * When the log passes one megabyte it is renamed to log-old.txt and a
 * new one is started, so it can never fill the card. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "asciicam.h"

#define LOG_MAX (1024 * 1024)

static char log_path[256];
static uint32_t log_boot;

uint64_t now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

void log_init(const char *path, uint32_t boot)
{
	snprintf(log_path, sizeof(log_path), "%s", path);
	log_boot = boot;
}

void log_msg(const char *fmt, ...)
{
	struct stat st;
	FILE *f;
	va_list ap;
	char old[300];
	struct timespec ts;

	clock_gettime(CLOCK_BOOTTIME, &ts);
	va_start(ap, fmt);
	fprintf(stderr, "asciicam: ");
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	if (!log_path[0])
		return;
	if (stat(log_path, &st) == 0 && st.st_size > LOG_MAX) {
		char *dot;
		snprintf(old, sizeof(old), "%s", log_path);
		dot = strrchr(old, '.');
		if (dot)
			*dot = 0;
		strncat(old, "-old.txt", sizeof(old) - strlen(old) - 1);
		rename(log_path, old);
	}
	f = fopen(log_path, "a");
	if (!f)
		return;
	fprintf(f, "Boot %u, %ld seconds after power-on: ", log_boot, (long)ts.tv_sec);
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fputs("\r\n", f);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
}
