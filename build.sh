#!/usr/bin/env bash
set -euo pipefail

log_file="build.log"
: > "$log_file"

echo "Building FirmMux..."
if make "$@" 2>&1 | tee -a "$log_file"; then
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
  if make -B "$@" 2>&1 | tee -a "$log_file"; then
    echo "Build succeeded. Log: $log_file"
  else
    echo "Build failed. Log: $log_file" >&2
    exit 1
  fi
fi
