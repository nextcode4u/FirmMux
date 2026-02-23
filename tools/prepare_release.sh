#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [ $# -lt 1 ]; then
  echo "Usage: $0 <tag> [--with-sd-zip] [--publish-canonical]" >&2
  exit 1
fi

TAG="$1"
WITH_SD_ZIP=0
PUBLISH_CANONICAL=0
CANONICAL_SD_ZIP="FirmMux-SD.zip"

for arg in "${@:2}"; do
  case "$arg" in
    --with-sd-zip)
      WITH_SD_ZIP=1
      ;;
    --publish-canonical)
      PUBLISH_CANONICAL=1
      ;;
    *)
      echo "Unknown option: $arg" >&2
      echo "Usage: $0 <tag> [--with-sd-zip] [--publish-canonical]" >&2
      exit 1
      ;;
  esac
done

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
    ZIP_PATH="releases/${CANONICAL_SD_ZIP}"
    rm -f "$ZIP_PATH"
    (
      cd SD
      zip -rq "../${CANONICAL_SD_ZIP}" .
    )
    mv -f "${CANONICAL_SD_ZIP}" "$ZIP_PATH"
    echo "- \`${CANONICAL_SD_ZIP}\` (contents of \`SD/\` for end-user copy)"
  else
    echo "- Add your release assets (for example SD zip) when publishing."
  fi
} > "$NOTES_PATH"

rm -f "$COMMITS_FILE"
echo "Generated: $NOTES_PATH"
if [ "$WITH_SD_ZIP" -eq 1 ] && [ -f "releases/${CANONICAL_SD_ZIP}" ]; then
  echo "Generated: releases/${CANONICAL_SD_ZIP}"
fi

if [ "$PUBLISH_CANONICAL" -eq 1 ]; then
  if ! command -v gh >/dev/null 2>&1; then
    echo "Error: gh CLI is required for --publish-canonical." >&2
    exit 1
  fi

  if ! gh release view "${TAG}" >/dev/null 2>&1; then
    echo "Error: release '${TAG}' not found or not accessible." >&2
    exit 1
  fi

  SRC_ZIP=""
  if [ -f "releases/${CANONICAL_SD_ZIP}" ]; then
    SRC_ZIP="releases/${CANONICAL_SD_ZIP}"
  elif [ -f "${CANONICAL_SD_ZIP}" ]; then
    SRC_ZIP="${CANONICAL_SD_ZIP}"
  else
    SRC_ZIP="$(ls -1t releases/FirmMux-*-SD.zip FirmMux-*-SD.zip 2>/dev/null | head -n 1 || true)"
  fi

  if [ -z "${SRC_ZIP}" ] || [ ! -f "${SRC_ZIP}" ]; then
    echo "Error: no source SD zip found to publish." >&2
    exit 1
  fi

  echo "Publishing canonical SD asset from: ${SRC_ZIP}"
  ASSETS="$(gh release view "${TAG}" --json assets --jq '.assets[].name' || true)"
  while IFS= read -r name; do
    [ -z "${name}" ] && continue
    if [[ "${name}" == "${CANONICAL_SD_ZIP}" || "${name}" =~ ^FirmMux-.*-SD\.zip$ ]]; then
      echo "Removing old SD asset: ${name}"
      gh release delete-asset "${TAG}" "${name}" -y
    fi
  done <<< "${ASSETS}"

  TMP_DIR="$(mktemp -d)"
  cleanup() { rm -rf "${TMP_DIR}"; }
  trap cleanup EXIT
  cp -f "${SRC_ZIP}" "${TMP_DIR}/${CANONICAL_SD_ZIP}"

  echo "Uploading ${CANONICAL_SD_ZIP}..."
  gh release upload "${TAG}" "${TMP_DIR}/${CANONICAL_SD_ZIP}"
  echo "Published canonical SD asset to release ${TAG}."
fi
