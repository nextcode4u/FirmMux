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

Sync submodule to latest upstream:
  $0 [branch-or-tag]
EOF
}

if [ "${UPSTREAM_REF}" = "-h" ] || [ "${UPSTREAM_REF}" = "--help" ]; then
  usage
  exit 0
fi

if [ ! -d "${PREFIX}" ]; then
  echo "RetroArch source tree missing at ${PREFIX}."
  echo "Run this once instead:"
  echo "  git submodule update --init --remote ${PREFIX}"
  exit 1
fi

git submodule sync -- "${PREFIX}"
git -C "${PREFIX}" fetch origin "${UPSTREAM_REF}"
git -C "${PREFIX}" checkout FETCH_HEAD

echo "Updated ${PREFIX} from ${UPSTREAM_URL} (${UPSTREAM_REF})."
echo "Next:"
echo "  tools/build_retroarch_with_firmux.sh"
