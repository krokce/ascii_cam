# Top-level Makefile for the ASCII camera appliance.
#   make image   fetch the pinned Buildroot, configure it, build the SD image
#   make test    run the tests on the build computer
#   make clean   remove the test programs (the Buildroot output is kept)

BUILDROOT_VERSION = 2026.02.3
BUILDROOT_URL = https://gitlab.com/buildroot.org/buildroot.git
TOP := $(CURDIR)
BR = env -u LD_LIBRARY_PATH $(MAKE) -C $(TOP)/buildroot BR2_EXTERNAL=$(TOP)/br2-external O=$(TOP)/output

image: buildroot/Makefile output/.config
	env -u LD_LIBRARY_PATH $(MAKE) -C output
	@echo "The SD card image is output/images/sdcard.img."

buildroot/Makefile:
	git clone --depth 1 --branch $(BUILDROOT_VERSION) $(BUILDROOT_URL) buildroot

output/.config: br2-external/configs/asciicam_defconfig | buildroot/Makefile
	$(BR) asciicam_defconfig

# Rebuild only the program and the image, after changing the C code.
program: output/.config
	env -u LD_LIBRARY_PATH $(MAKE) -C output asciicam-rebuild all

test:
	$(MAKE) -C src test

clean:
	$(MAKE) -C src clean
	rm -rf tests/out

.PHONY: image program test clean
