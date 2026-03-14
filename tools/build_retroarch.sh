#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="/tmp/retroarch_build"
SRC_TREE="$ROOT/retroarch_src/RetroArch-master"
SRC_DIR="$BUILD_DIR/RetroArch-master"
OUT_DIR="$ROOT/SD/3ds/firmmux/emulators"
if [ ! -d "$SRC_TREE" ]; then
  echo "Missing source tree: $SRC_TREE" >&2
  exit 1
fi
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR" "$OUT_DIR"
cp -a "$SRC_TREE" "$SRC_DIR"
make -C "$SRC_DIR" -f Makefile.ctr -j"$(nproc)"
cp -f "$SRC_DIR/retroarch_3ds.3dsx" "$OUT_DIR/retroarch.3dsx"
if [ -f "$SRC_DIR/retroarch_3ds.smdh" ]; then
  cp -f "$SRC_DIR/retroarch_3ds.smdh" "$OUT_DIR/retroarch.smdh"
fi
echo "Built: $OUT_DIR/retroarch.3dsx"
