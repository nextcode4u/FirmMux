#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PREFIX="retroarch_src/RetroArch-master"
UPSTREAM_URL="https://github.com/nextcode4u/RetroArch-Pathfile-mod.git"
UPSTREAM_REF="${1:-master}"

usage() {
  cat <<EOF
Usage: $0 [branch-or-tag]

Initial import:
  git subtree add --prefix=${PREFIX} ${UPSTREAM_URL} <ref> --squash

Update existing source:
  $0 [branch-or-tag]
EOF
}

if [ "${UPSTREAM_REF}" = "-h" ] || [ "${UPSTREAM_REF}" = "--help" ]; then
  usage
  exit 0
fi

if [ ! -d "${PREFIX}" ] || [ -z "$(find "${PREFIX}" -mindepth 1 -maxdepth 1 2>/dev/null)" ]; then
  echo "RetroArch source tree missing at ${PREFIX}."
  echo "Run this once instead:"
  echo "  git subtree add --prefix=${PREFIX} ${UPSTREAM_URL} ${UPSTREAM_REF} --squash"
  exit 1
fi

git subtree pull --prefix="${PREFIX}" "${UPSTREAM_URL}" "${UPSTREAM_REF}" --squash

echo "Updated ${PREFIX} from ${UPSTREAM_URL} (${UPSTREAM_REF})."
echo "Next:"
echo "  tools/build_retroarch_with_firmux.sh"
