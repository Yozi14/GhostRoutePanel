#!/usr/bin/env bash
set -euo pipefail
DOMAIN="${1:-}"
EMAIL="${2:-}"

if [[ -z "$DOMAIN" || -z "$EMAIL" ]]; then
  echo "Usage: provision-nginx.sh <domain> <email>"
  exit 1
fi

cat > /etc/nginx/sites-available/ghostroute <<EOF
server {
    listen 80;
    server_name $DOMAIN;
    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
    }
}
EOF

ln -sf /etc/nginx/sites-available/ghostroute /etc/nginx/sites-enabled/ghostroute
nginx -t && systemctl reload nginx
certbot --nginx -d "$DOMAIN" --email "$EMAIL" --agree-tos --non-interactive || true
mkdir -p /etc/ghostroute/certs
cp -L /etc/letsencrypt/live/"$DOMAIN"/fullchain.pem /etc/ghostroute/certs/ 2>/dev/null || true
cp -L /etc/letsencrypt/live/"$DOMAIN"/privkey.pem /etc/ghostroute/certs/ 2>/dev/null || true
echo "Nginx + TLS ready for $DOMAIN"
