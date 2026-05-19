#!/usr/bin/env bash
set -euo pipefail
INSTALL_DIR="${GR_INSTALL_DIR:-/opt/ghostroute}"
RELEASE_URL="${GR_RELEASE_URL:-}"

if [[ -z "$RELEASE_URL" ]]; then
  echo "Set GR_RELEASE_URL to latest release tarball URL"
  exit 1
fi

systemctl stop ghostroute-panel 2>/dev/null || true
curl -fsSL "$RELEASE_URL" | tar -xz -C "$INSTALL_DIR" --strip-components=1
systemctl start ghostroute-panel 2>/dev/null || true
echo "Updated GhostRoutePanel"
