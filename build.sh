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

stage_sd() {
  cp -f FirmMux.3dsx SD/3ds/FirmMux.3dsx
  cp -f FirmMux.smdh SD/3ds/FirmMux.smdh
  mkdir -p SD/3ds/FirmMux
  mkdir -p SD/cias
  if [ -f boot.3dsx ]; then
    cp -f boot.3dsx SD/3ds/FirmMux/boot.3dsx
  fi
  if [ -f boot.smdh ]; then
    cp -f boot.smdh SD/3ds/FirmMux/boot.smdh
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

echo "Building optional FirmMux CIA..."
if tools/build_firmmux_cia.sh 2>&1 | tee -a "$log_file"; then
  echo "CIA step complete. Log: $log_file"
else
  echo "CIA build failed. Log: $log_file" >&2
  exit 1
fi

stage_sd
