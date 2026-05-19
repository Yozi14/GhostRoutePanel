#!/usr/bin/env bash
set -euo pipefail

echo "[GhostRoutePanel] Installing dependencies…"

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq curl wget nginx certbot python3-certbot-nginx sqlite3

mkdir -p /etc/ghostroute/{xray,hysteria,certs}
mkdir -p /var/lib/ghostroute

# Xray
if ! command -v xray &>/dev/null; then
  bash -c "$(curl -L https://github.com/XTLS/Xray-install/raw/main/install-release.sh)" @ install
fi

# Hysteria2
if ! command -v hysteria &>/dev/null; then
  bash <(curl -fsSL https://get.hy2.sh/)
fi

echo "[GhostRoutePanel] Dependencies installed."
