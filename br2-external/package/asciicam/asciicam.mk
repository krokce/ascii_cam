################################################################################
#
# asciicam
#
################################################################################

ASCIICAM_VERSION = 1.0
ASCIICAM_SITE = $(BR2_EXTERNAL_ASCIICAM_PATH)/../src
ASCIICAM_SITE_METHOD = local
ASCIICAM_LICENSE = MIT
ASCIICAM_BOARD = $(BR2_EXTERNAL_ASCIICAM_PATH)/board/asciicam

define ASCIICAM_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) -O2" LDFLAGS="$(TARGET_LDFLAGS)" asciicam
endef

ASCIICAM_FLAGS = \
	SERIAL=$(if $(BR2_PACKAGE_ASCIICAM_SERIAL_CONSOLE),1,0)

define ASCIICAM_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/asciicam $(TARGET_DIR)/usr/bin/asciicam
	mkdir -p $(TARGET_DIR)/rec $(TARGET_DIR)/boot
	printf '%s\n' $(ASCIICAM_FLAGS) > $(TARGET_DIR)/etc/asciicam-build.conf
endef

$(eval $(generic-package))
