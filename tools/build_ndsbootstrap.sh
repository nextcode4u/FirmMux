#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repo="$root/ndsbs_src/nds-bootstrap-master"
if [ ! -d "$repo" ]; then
  echo "nds-bootstrap source not found at $repo" >&2
  exit 1
fi
if [ ! -d "/opt/devkitpro/libnds/include" ]; then
  echo "libnds headers missing. Install libnds." >&2
  exit 1
fi
lzss_bin="$root/tools/lzss"
if ! command -v lzss >/dev/null 2>&1; then
  if [ ! -f "$lzss_bin" ]; then
    cc "$repo/lzss.c" -O2 -o "$lzss_bin"
  fi
  export PATH="$root/tools:$PATH"
fi
make -C "$repo/retail" cardenginei_arm7_cheat
out="$repo/retail/nitrofiles/cardenginei_arm7_cheat.bin"
sd_out="$root/SD/_nds/nds-bootstrap"
if [ -f "$out" ]; then
  echo "built ... $(basename "$out")"
  echo "output: $out"
  mkdir -p "$sd_out"
  cp -f "$out" "$sd_out/"
  echo "copied to: $sd_out/$(basename "$out")"
else
  echo "build completed but output not found" >&2
  exit 1
fi
