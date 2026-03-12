#!/usr/bin/env bash
set -euo pipefail

log_file="build.log"
: > "$log_file"

build_main() {
  make -j4 2>&1 | tee -a "$log_file"
}

build_boot() {
  make -j4 TARGET=boot \
    APP_TITLE='FirmMux Boot Forwarder' \
    APP_DESCRIPTION='Autoboot forwarder for FirmMux' \
    APP_AUTHOR='FirmMux Team' 2>&1 | tee -a "$log_file"
}

build_cia_forwarder() {
  make -B -j4 TARGET=firmmux_cia_forwarder \
    APP_TITLE='FirmMux CIA Forwarder' \
    APP_DESCRIPTION='Launches sd:/3ds/FirmMux.3dsx' \
    APP_AUTHOR='FirmMux Team' 2>&1 | tee -a "$log_file"
}

build_downloader() {
  local dl_icon="assets/logodl.png"
  if [ -f "$dl_icon" ]; then
    local dims
    dims="$(python3 - <<'PY'
import struct
try:
    with open("assets/logodl.png", "rb") as f:
        sig = f.read(8)
        if sig != b"\x89PNG\r\n\x1a\n":
            print("invalid")
        else:
            _len = f.read(4)
            ctype = f.read(4)
            if ctype != b'IHDR':
                print("invalid")
            else:
                data = f.read(13)
                w, h = struct.unpack(">II", data[:8])
                print(f"{w}x{h}")
except Exception:
    print("invalid")
PY
)"
    if [ "$dims" != "48x48" ]; then
      echo "Downloader icon assets/logodl.png is $dims (expected 48x48). Falling back to assets/icon.png." | tee -a "$log_file"
      dl_icon="assets/icon.png"
    fi
  else
    dl_icon="assets/icon.png"
  fi

  make -B -j4 TARGET=firmmux_downloader \
    APP_TITLE='FirmMux Updater' \
    APP_DESCRIPTION='Update FirmMux on-device' \
    APP_AUTHOR='FirmMux Team' \
    APP_ICON="$dl_icon" 2>&1 | tee -a "$log_file"
}

stage_sd() {
  cp -f FirmMux.3dsx SD/3ds/FirmMux.3dsx
  cp -f FirmMux.smdh SD/3ds/FirmMux.smdh
  mkdir -p SD/3ds
  mkdir -p SD/3ds/FirmMux
  mkdir -p SD/cias
  if [ -f boot.3dsx ]; then
    cp -f boot.3dsx SD/3ds/FirmMux/boot.3dsx
  fi
  if [ -f boot.smdh ]; then
    cp -f boot.smdh SD/3ds/FirmMux/boot.smdh
  fi
  if [ -f firmmux_downloader.3dsx ]; then
    cp -f firmmux_downloader.3dsx SD/3ds/firmmux-updater.3dsx
    rm -f SD/3ds/FirmMux/firmmux-downloader.3dsx
    rm -f SD/3ds/firmmux-downloader.3dsx
  fi
  if [ -f firmmux_downloader.smdh ]; then
    cp -f firmmux_downloader.smdh SD/3ds/firmmux-updater.smdh
    rm -f SD/3ds/FirmMux/firmmux-downloader.smdh
    rm -f SD/3ds/firmmux-downloader.smdh
  fi
  echo "Staged SD binaries." | tee -a "$log_file"
}

echo "Building FirmMux..."
if build_main; then
  echo "Build succeeded. Log: $log_file"
else
  echo "Build failed. Log: $log_file" >&2
  exit 1
fi

if [ -f FirmMux.3dsx ]; then
  if [ -f build_number.txt ]; then
    n="$(cat build_number.txt 2>/dev/null || true)"
  else
    n=""
  fi
  if [ -z "$n" ]; then
    n=100
  else
    n=$((n + 1))
  fi
  printf '%s\n' "$n" > build_number.txt
  printf '#ifndef FIRMUX_BUILD_ID\n#define FIRMUX_BUILD_ID \"Build:%s\"\n#endif\n' "$n" > include/build_id.h
  echo "Rebuilding with build number $n..." | tee -a "$log_file"
  if make -B -j4 2>&1 | tee -a "$log_file"; then
    echo "Build succeeded. Log: $log_file"
  else
    echo "Build failed. Log: $log_file" >&2
    exit 1
  fi
fi

echo "Building boot forwarder..."
if build_boot; then
  echo "Boot build succeeded. Log: $log_file"
else
  echo "Boot build failed. Log: $log_file" >&2
  exit 1
fi

echo "Building FirmMux downloader..."
if build_downloader; then
  echo "Downloader build succeeded. Log: $log_file"
else
  echo "Downloader build failed. Log: $log_file" >&2
  exit 1
fi

echo "Building optional FirmMux CIA..."
echo "Building FirmMux CIA forwarder ELF..."
if build_cia_forwarder; then
  echo "CIA forwarder ELF build succeeded. Log: $log_file"
else
  echo "CIA forwarder ELF build failed. Log: $log_file" >&2
  exit 1
fi

if tools/build_firmmux_cia.sh 2>&1 | tee -a "$log_file"; then
  echo "CIA step complete. Log: $log_file"
else
  echo "CIA build failed. Log: $log_file" >&2
  exit 1
fi

stage_sd
