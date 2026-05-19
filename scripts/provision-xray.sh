#!/usr/bin/env bash
set -euo pipefail
DOMAIN="${1:-}"
CFG_DIR="/etc/ghostroute/xray"
mkdir -p "$CFG_DIR"

cat > "$CFG_DIR/config.json" <<EOF
{
  "log": { "loglevel": "warning" },
  "inbounds": [{
    "port": 443,
    "protocol": "vless",
    "settings": { "clients": [], "decryption": "none" },
    "streamSettings": { "network": "tcp", "security": "reality" }
  }],
  "outbounds": [{ "protocol": "freedom", "tag": "direct" }]
}
EOF

systemctl enable xray 2>/dev/null || true
systemctl restart xray 2>/dev/null || true
echo "Xray configured for domain: $DOMAIN"
