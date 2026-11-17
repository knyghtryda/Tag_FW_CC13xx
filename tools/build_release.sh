#!/bin/bash
# Chroma 74H release images in one go: clean builds of bootloader_CC1310 and
# oepl_CC1310 CONFIG=Chroma74H, then tools/out2bin.sh and
# tools/make_74h_images.py. Builds in place; runs on macOS (the x86 TI
# compiler runs under Rosetta on Apple Silicon) and Linux/WSL:
#
#   tools/build_release.sh                 # FW_VERSION from oepl_CC1310/main.h
#   FW_VERSION=8 tools/build_release.sh    # override, e.g. for OTA tests
#
# Writes oepl_CC1310/build/release/chroma74h_{full,ota}_<ver>.bin.
# SDK and CGT default to ~/ti as in the makefiles; set them in the
# environment to override.
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$REPO/oepl_CC1310/build/release"

if [ -n "$FW_VERSION" ]; then
   MAKE_ARGS=("EXTRA_DEFINES=-DFW_VERSION=$FW_VERSION")
else
   FW_VERSION=$(sed -n 's/^#define FW_VERSION[[:space:]]*\(0x[0-9A-Fa-f]*\).*/\1/p' \
                "$REPO/oepl_CC1310/main.h")
   [ -n "$FW_VERSION" ] || { echo "FW_VERSION not found in oepl_CC1310/main.h"; exit 1; }
   MAKE_ARGS=()
fi

make -C "$REPO/bootloader_CC1310" clean all
make -C "$REPO/oepl_CC1310" CONFIG=Chroma74H "${MAKE_ARGS[@]}" clean all

mkdir -p "$OUT"
bash "$REPO/tools/out2bin.sh" "$REPO/bootloader_CC1310/build/boot.out" \
     "$OUT/boot.bin" 0x0 0x10000
bash "$REPO/tools/out2bin.sh" "$REPO/oepl_CC1310/build/Chroma74H/oepl_CC1310.out" \
     "$OUT/app.bin" 0x1000 0xE000
python3 "$REPO/tools/make_74h_images.py" "$OUT/boot.bin" "$OUT/app.bin" \
        "$FW_VERSION" --out "$OUT"
