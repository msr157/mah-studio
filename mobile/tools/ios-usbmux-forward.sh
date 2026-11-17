#!/usr/bin/env bash
# Forward MAH Studio listen port to a USB-connected iPhone via usbmuxd / iproxy.
# Requires libimobiledevice (iproxy). Same MAH1 TCP protocol as Wi‑Fi.
#
#   ./ios-usbmux-forward.sh 28887
#
set -euo pipefail
PORT="${1:-28887}"
if ! command -v iproxy >/dev/null 2>&1; then
  echo "Install libimobiledevice (iproxy) first." >&2
  exit 1
fi
echo "Forwarding localhost:${PORT} ↔ device:${PORT} (Ctrl+C to stop)"
exec iproxy "${PORT}" "${PORT}"
