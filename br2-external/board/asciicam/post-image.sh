#!/bin/bash
# Post-image step: compiles the pin overlay, builds the boot, REC and
# SD card images. Run by Buildroot with BINARIES_DIR set.
set -euo pipefail

BOARD_DIR="$(dirname "$0")"
FW="${BINARIES_DIR}/rpi-firmware"
. "${TARGET_DIR}/etc/asciicam-build.conf"

# Compile our overlay that limits DPI to four pins.
DTC="$(ls -d "${BUILD_DIR}"/linux-*/scripts/dtc/dtc | head -n 1)"
mkdir -p "${FW}/overlays"
"${DTC}" -@ -q -I dts -O dtb -o "${FW}/overlays/mda-dpi-pins.dtbo" \
	"${BOARD_DIR}/overlays/mda-dpi-pins-overlay.dts"

# Keep only the overlays config.txt actually uses: fewer files, faster boot.
for f in "${FW}"/overlays/*.dtbo; do
	case "$(basename "$f")" in
		vc4-kms-v3d.dtbo|vc4-kms-dpi-generic.dtbo|disable-bt.dtbo|mda-dpi-pins.dtbo) ;;
		*) rm -f "$f" ;;
	esac
done
rm -f "${FW}/overlays/README"

# Debug serial console: add it to the kernel command line.
if [ "${SERIAL}" = 1 ]; then
	grep -q 'console=ttyAMA0' "${FW}/cmdline.txt" || \
		sed -i 's|^|console=ttyAMA0,115200 |' "${FW}/cmdline.txt"
fi

# The small REC partition shipped in the image. It holds GROWME.TXT,
# which tells the first boot to enlarge REC to fill the card.
REC_IMG="${BINARIES_DIR}/rec.vfat"
rm -f "${REC_IMG}"
mkfs.fat -C -F 32 -n REC "${REC_IMG}" $((64 * 1024)) > /dev/null
printf '%s\r\n' \
	"This file tells the camera appliance that this is its first boot." \
	"On that boot the REC partition is enlarged to fill the whole card" \
	"and formatted, and this file disappears. If you delete it before the" \
	"first boot, REC keeps its small size of 64 megabytes." \
	> "${BINARIES_DIR}/GROWME.TXT"
mcopy -i "${REC_IMG}" "${BINARIES_DIR}/GROWME.TXT" ::GROWME.TXT

GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"
ROOTPATH_TMP="$(mktemp -d)"
trap 'rm -rf "${ROOTPATH_TMP}"' EXIT
rm -rf "${GENIMAGE_TMP}"
genimage --rootpath "${ROOTPATH_TMP}" --tmppath "${GENIMAGE_TMP}" \
	--inputpath "${BINARIES_DIR}" --outputpath "${BINARIES_DIR}" \
	--config "${BOARD_DIR}/genimage.cfg"
