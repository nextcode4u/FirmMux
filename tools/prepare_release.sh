#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [ $# -lt 1 ]; then
  echo "Usage: $0 <tag> [--with-sd-zip]" >&2
  exit 1
fi

TAG="$1"
WITH_SD_ZIP=0
if [ "${2:-}" = "--with-sd-zip" ]; then
  WITH_SD_ZIP=1
fi

mkdir -p releases

PREV_TAG="$(git tag --sort=-creatordate | grep -Fxv "$TAG" | head -n 1 || true)"
if [ -n "$PREV_TAG" ]; then
  RANGE="$PREV_TAG..HEAD"
else
  RANGE="HEAD"
fi

COMMITS_FILE="$(mktemp)"
git log --oneline "$RANGE" > "$COMMITS_FILE"

mapfile -t TLDR_LINES < <(head -n 6 "$COMMITS_FILE" | sed -E 's/^[0-9a-f]+ //')
if [ "${#TLDR_LINES[@]}" -eq 0 ]; then
  TLDR_LINES=("No code changes since previous release tag.")
fi

NOTES_PATH="releases/${TAG}-notes.md"
{
  echo "## TL;DR"
  for line in "${TLDR_LINES[@]:0:4}"; do
    echo "- ${line}"
  done
  echo
  if [ -n "$PREV_TAG" ]; then
    echo "## Since ${PREV_TAG}"
  else
    echo "## Since Previous Release"
  fi
  echo "- Commits included in this release:"
  awk '{ $1=""; sub(/^ /,""); print "  - " $0 }' "$COMMITS_FILE"
  echo
  echo "## Included Assets"
  if [ "$WITH_SD_ZIP" -eq 1 ] && [ -d "SD" ]; then
    ZIP_PATH="releases/FirmMux-${TAG}-SD.zip"
    rm -f "$ZIP_PATH"
    (
      cd SD
      zip -rq "../FirmMux-${TAG}-SD.zip" .
    )
    mv -f "FirmMux-${TAG}-SD.zip" "$ZIP_PATH"
    echo "- \`FirmMux-${TAG}-SD.zip\` (contents of \`SD/\` for end-user copy)"
  else
    echo "- Add your release assets (for example SD zip) when publishing."
  fi
} > "$NOTES_PATH"

rm -f "$COMMITS_FILE"
echo "Generated: $NOTES_PATH"
if [ "$WITH_SD_ZIP" -eq 1 ] && [ -f "releases/FirmMux-${TAG}-SD.zip" ]; then
  echo "Generated: releases/FirmMux-${TAG}-SD.zip"
fi
