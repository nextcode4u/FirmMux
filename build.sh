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
