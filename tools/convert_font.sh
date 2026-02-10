#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -lt 2 ]; then
  echo "usage: $0 <input.ttf> <output.bcfnt> [size]"
  exit 1
fi
IN="$1"
OUT="$2"
SIZE="${3:-18}"
if [ ! -f "$IN" ]; then
  echo "input not found: $IN"
  exit 1
fi
MKBCFNT="/opt/devkitpro/tools/bin/mkbcfnt"
if [ ! -x "$MKBCFNT" ]; then
  MKBCFNT="mkbcfnt"
fi
$MKBCFNT -o "$OUT" -s "$SIZE" "$IN"
