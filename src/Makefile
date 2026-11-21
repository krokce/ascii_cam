# Makefile for the ASCII camera program.
#   make asciicam   builds the program (Buildroot sets CC to the Pi compiler)
#   make test       builds and runs the tests on the build computer
#   make check      compiles the hardware parts on the build computer too,
#                   to catch errors early; the result is not run

CC ?= cc
CFLAGS ?= -O2
CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-parameter
LDLIBS = -lm

CORE = font.c font_data.c convert.c render.c testpattern.c settings.c recorder.c log.c
HW = main.c kms.c camera.c video.c button.c
HDRS = asciicam.h font.h kms.h camera.h video.h button.h gpio_v2_compat.h

asciicam: $(CORE) $(HW) $(HDRS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(CORE) $(HW) $(LDLIBS)

TESTDIR = ../tests
test: $(TESTDIR)/test_core
	cd $(TESTDIR) && ./test_core
	cd $(TESTDIR) && python3 -I test_replay.py

$(TESTDIR)/test_core: $(TESTDIR)/test_core.c $(CORE) asciicam.h font.h
	cc -O1 -g -std=gnu11 -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined \
		-I. -o $@ $(TESTDIR)/test_core.c $(CORE) -lm

check: $(CORE) $(HW) $(HDRS)
	cc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -fsyntax-only $(CORE) $(HW)

clean:
	rm -f asciicam $(TESTDIR)/test_core

.PHONY: test check clean
