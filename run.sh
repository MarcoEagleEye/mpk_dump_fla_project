#!/usr/bin/env bash
set -euo pipefail
ROM="${1:-mpk_dump_fla.z64}"
if command -v simple64 >/dev/null 2>&1; then
  exec simple64 "$ROM"
elif command -v mupen64plus >/dev/null 2>&1; then
  exec mupen64plus "$ROM"
else
  echo "Bitte simple64 oder mupen64plus installieren und ins PATH legen." >&2
  exit 1
fi
