#!/usr/bin/env bash
set -euo pipefail

# Builds a FirmMux CIA using bannertool + makerom.
# This is optional and only runs when both tools are available.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/SD/cias"
TMP_DIR="$ROOT_DIR/build/cia"
TOOL_DIR="$ROOT_DIR/tools/bin"

APP_TITLE="${APP_TITLE:-FirmMux}"
APP_AUTHOR="${APP_AUTHOR:-FirmMux Team}"
APP_PRODUCT_CODE="${APP_PRODUCT_CODE:-CTR-H-FMUX}"
# Keep this customizable to avoid collisions with other homebrew CIAs.
APP_UNIQUE_ID_HEX="${APP_UNIQUE_ID_HEX:-0xF4D58}"
APP_TITLE_ID_HEX="${APP_TITLE_ID_HEX:-0x000400000FF40A00}"

ELF_PATH="${ELF_PATH:-$ROOT_DIR/firmmux_cia_forwarder.elf}"
ICON_PATH="${ICON_PATH:-$ROOT_DIR/assets/icon.png}"
BANNER_IMAGE_PATH="${BANNER_IMAGE_PATH:-$ROOT_DIR/assets/banner.png}"
# Banner audio is required by some bannertool builds.
# FirmMux requires using SD-layout BGM for CIA banner audio.
BANNER_AUDIO_PATH="${BANNER_AUDIO_PATH:-$ROOT_DIR/SD/3ds/FirmMux/bgm/bgm.wav}"
OUT_CIA_PATH="${OUT_CIA_PATH:-$OUT_DIR/FirmMux.cia}"

resolve_tool() {
  local name="$1"
  local local_path="$TOOL_DIR/$name"
  local local_path_exe="$TOOL_DIR/$name.exe"
  if [ -x "$local_path_exe" ]; then
    printf '%s\n' "$local_path_exe"
    return 0
  fi
  if [ -x "$local_path" ]; then
    printf '%s\n' "$local_path"
    return 0
  fi
  if command -v "$name.exe" >/dev/null 2>&1; then
    command -v "$name.exe"
    return 0
  fi
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return 0
  fi
  return 1
}

BANNERTOOL_BIN="$(resolve_tool bannertool || true)"
MAKEROM_BIN="$(resolve_tool makerom || true)"

if [ -z "$BANNERTOOL_BIN" ]; then
  echo "Skipping CIA build: bannertool not found in PATH."
  exit 0
fi
if [ -z "$MAKEROM_BIN" ]; then
  echo "Skipping CIA build: makerom not found in PATH."
  exit 0
fi

if [ ! -f "$ELF_PATH" ]; then
  echo "CIA build failed: missing ELF: $ELF_PATH" >&2
  exit 1
fi
if [ ! -f "$ICON_PATH" ]; then
  echo "CIA build failed: missing icon PNG: $ICON_PATH" >&2
  exit 1
fi
if [ ! -f "$BANNER_IMAGE_PATH" ]; then
  echo "CIA build failed: missing banner PNG: $BANNER_IMAGE_PATH" >&2
  exit 1
fi
if [ ! -f "$BANNER_AUDIO_PATH" ]; then
  echo "CIA build failed: missing banner audio: $BANNER_AUDIO_PATH" >&2
  exit 1
fi

mkdir -p "$TMP_DIR" "$OUT_DIR"

ICON_ICN="$TMP_DIR/icon.icn"
BANNER_BNR="$TMP_DIR/banner.bnr"
RSF_PATH="$TMP_DIR/FirmMux.rsf"
RESIZE_BIN="$TMP_DIR/resize_png"
RESIZED_BANNER_IMAGE_PATH="$TMP_DIR/banner_256x128.png"
PREPARED_BANNER_AUDIO_PATH="$TMP_DIR/banner_audio_2s.wav"

echo "Creating CIA icon..."
"$BANNERTOOL_BIN" makesmdh \
  -s "$APP_TITLE" \
  -l "$APP_TITLE" \
  -p "$APP_AUTHOR" \
  -i "$ICON_PATH" \
  -o "$ICON_ICN"

echo "Creating CIA banner..."
if [ ! -x "$RESIZE_BIN" ]; then
  gcc "$ROOT_DIR/tools/resize_logo.c" -O2 \
    -I"$ROOT_DIR/source" \
    -I"$ROOT_DIR/tools" \
    -lm \
    -o "$RESIZE_BIN"
fi
"$RESIZE_BIN" "$BANNER_IMAGE_PATH" "$RESIZED_BANNER_IMAGE_PATH" 256 128
python3 "$ROOT_DIR/tools/prepare_banner_audio.py" \
  "$BANNER_AUDIO_PATH" \
  "$PREPARED_BANNER_AUDIO_PATH" \
  --seconds 3.0
"$BANNERTOOL_BIN" makebanner \
  -i "$RESIZED_BANNER_IMAGE_PATH" \
  -a "$PREPARED_BANNER_AUDIO_PATH" \
  -o "$BANNER_BNR"

cat > "$RSF_PATH" <<EOF
BasicInfo:
  Title                   : "$APP_TITLE"
  CompanyCode             : "00"
  ProductCode             : "$APP_PRODUCT_CODE"
  ContentType             : Application
  Logo                    : Nintendo

TitleInfo:
  UniqueId                : $APP_UNIQUE_ID_HEX
  Category                : Application

Option:
  UseOnSD                 : true
  EnableCompress          : true
  FreeProductCode         : true

SystemControlInfo:
  SaveDataSize            : 0KB
  RemasterVersion         : 0
  JumpId                  : $APP_TITLE_ID_HEX

AccessControlInfo:
  ServiceAccessControl:
    - "APT:U"
    - "hb:ldr"
EOF

echo "Building CIA..."
"$MAKEROM_BIN" -f cia \
  -o "$OUT_CIA_PATH" \
  -elf "$ELF_PATH" \
  -rsf "$RSF_PATH" \
  -desc app:4 \
  -icon "$ICON_ICN" \
  -banner "$BANNER_BNR" \
  -target t \
  -exefslogo \
  -DAPP_ENCRYPTED=false

echo "Built CIA: $OUT_CIA_PATH"
