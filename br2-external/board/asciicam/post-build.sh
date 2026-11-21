#!/bin/sh
# Post-build step: trims the root filesystem to the appliance's needs.
set -eu
TARGET="$1"
. "$TARGET/etc/asciicam-build.conf"

# Only our own rcS runs at boot. Remove every other start-up script
# that packages install (system log daemons, the default Dropbear and
# network scripts, random seed saving), since the root is read-only and
# nothing else is wanted.
find "$TARGET/etc/init.d" -name 'S*' -exec rm -f {} +
rm -f "$TARGET/etc/init.d/rcK"

# SSH looks for keys in /root/.ssh; point it at the copy made at boot.
rm -rf "$TARGET/root/.ssh"
ln -s /run/ssh "$TARGET/root/.ssh"

# Debug serial console, only when enabled at build time.
if [ "$SERIAL" = 1 ]; then
	sed -i 's|^# SERIAL-CONSOLE-PLACEHOLDER$|ttyAMA0::respawn:/sbin/getty -L ttyAMA0 115200 vt100|' "$TARGET/etc/inittab"
else
	sed -i '/^# SERIAL-CONSOLE-PLACEHOLDER$/d' "$TARGET/etc/inittab"
fi
echo asciicam > "$TARGET/etc/hostname"

# Safety check: if the kernel modules are xz-compressed, modprobe must be
# able to read them, or no driver loads at boot and the screen stays black.
if find "$TARGET/lib/modules" -name '*.ko.xz' | grep -q . ; then
	if ! grep -q liblzma "$TARGET/usr/bin/kmod" 2>/dev/null; then
		echo "ERROR: kernel modules are xz-compressed but kmod has no xz support. Set BR2_PACKAGE_XZ=y." >&2
		exit 1
	fi
fi

# Safety check: BusyBox sleep here accepts whole seconds only. A
# fractional sleep in a start-up script would return at once without
# waiting, which once made Wi-Fi start before its chip was ready.
if grep -n -E 'sleep +[0-9]*\.[0-9]' "$TARGET/etc/init.d/rcS" "$TARGET/usr/bin/asciicam-run" 2>/dev/null; then
	echo "ERROR: fractional sleep found in a start-up script; use whole seconds." >&2
	exit 1
fi
