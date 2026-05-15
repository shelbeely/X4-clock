#!/usr/bin/env bash
# fetch_mquickjs.sh — Download mquickjs, build the x4_stdlib header, and
# populate firmware/lib/mquickjs/src/ with all source files needed to compile
# the firmware.
#
# Usage (from the firmware/ directory):
#   bash scripts/fetch_mquickjs.sh
#
# Requirements:
#   git, gcc, make (standard POSIX tools)
#
# What it does:
#   1. Clones github.com/bellard/mquickjs (shallow) into /tmp
#   2. Compiles the x4_stdlib build-tool (host binary) from
#      scripts/x4_stdlib.c + mquickjs_build.c
#   3. Runs the tool twice to generate:
#        x4_stdlib.h      — compiled stdlib bytecode + C function table
#        mquickjs_atom.h  — sorted atom table required by mquickjs.c
#      Both files use 32-bit encoding (-m32) for the ESP32-C3 RISC-V target
#   4. Copies all required .c/.h source files into lib/mquickjs/src/
#      (PlatformIO will compile every .c file found there)
#
# Re-running the script is safe — it overwrites existing files in lib/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$(dirname "$SCRIPT_DIR")"
LIB_SRC="$FIRMWARE_DIR/lib/mquickjs/src"
WORK="$(mktemp -d)"   # uses the system's default temp directory (TMPDIR)

cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT

echo "=== Cloning bellard/mquickjs ==="
git clone --depth=1 https://github.com/bellard/mquickjs.git "$WORK/mquickjs"
cd "$WORK/mquickjs"

echo ""
echo "=== Building x4_stdlib host tool ==="
# Copy our custom stdlib definition into the mquickjs source tree so the
# build tool can find mquickjs_build.h via a simple relative include.
cp "$SCRIPT_DIR/x4_stdlib.c" .

gcc -Wall -O2 -D_GNU_SOURCE \
    -o x4_stdlib_tool \
    x4_stdlib.c mquickjs_build.c

echo ""
echo "=== Generating lib/mquickjs/src/x4_stdlib.h (32-bit bytecode) ==="
./x4_stdlib_tool -m32 > x4_stdlib.h

echo "=== Generating lib/mquickjs/src/mquickjs_atom.h ==="
./x4_stdlib_tool -a -m32 > mquickjs_atom.h

echo ""
echo "=== Installing source files into $LIB_SRC ==="
mkdir -p "$LIB_SRC"

# Engine core
cp mquickjs.c       "$LIB_SRC/"
cp mquickjs.h       "$LIB_SRC/"
cp mquickjs_priv.h  "$LIB_SRC/"
cp mquickjs_opcode.h "$LIB_SRC/"

# Utilities
cp cutils.c  "$LIB_SRC/"
cp cutils.h  "$LIB_SRC/"
cp dtoa.c    "$LIB_SRC/"
cp dtoa.h    "$LIB_SRC/"
cp libm.c    "$LIB_SRC/"
cp libm.h    "$LIB_SRC/"
cp list.h    "$LIB_SRC/"

# Soft-float helpers (needed when CONFIG_SOFTFLOAT is set; harmless otherwise)
cp softfp_template.h      "$LIB_SRC/"
cp softfp_template_icvt.h "$LIB_SRC/"

# Generated headers — MUST be regenerated whenever x4_stdlib.c changes
cp mquickjs_atom.h  "$LIB_SRC/"
cp x4_stdlib.h      "$LIB_SRC/"

echo ""
echo "=== Done! ==="
echo ""
echo "Files installed to:  $LIB_SRC"
echo ""
echo "Next steps:"
echo "  cd firmware/"
echo "  pio run           # build the firmware"
echo "  pio run -t upload # flash to device"
echo ""
echo "NOTE: Re-run this script after editing firmware/scripts/x4_stdlib.c"
echo "      to regenerate x4_stdlib.h and mquickjs_atom.h."
