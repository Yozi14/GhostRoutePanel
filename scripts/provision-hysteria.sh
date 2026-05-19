#!/usr/bin/env bash
set -euo pipefail
DOMAIN="${1:-}"
CFG="/etc/ghostroute/hysteria/config.yaml"
mkdir -p /etc/ghostroute/hysteria

cat > "$CFG" <<EOF
listen: :8443
tls:
  cert: /etc/ghostroute/certs/fullchain.pem
  key: /etc/ghostroute/certs/privkey.pem
auth:
  type: userpass
  userpass: {}
EOF

systemctl enable hysteria 2>/dev/null || true
systemctl restart hysteria 2>/dev/null || true
echo "Hysteria configured for domain: $DOMAIN"
