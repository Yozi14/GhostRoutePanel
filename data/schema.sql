-- GhostRoutePanel — SQLite schema (multitenant)

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS tenants (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    slug        TEXT NOT NULL UNIQUE,
    name        TEXT NOT NULL,
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS admins (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id   INTEGER NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    username    TEXT NOT NULL,
    password_hash TEXT NOT NULL,
    role        TEXT NOT NULL DEFAULT 'admin', -- superadmin | admin
    created_at  TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(tenant_id, username)
);

CREATE TABLE IF NOT EXISTS users (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id   INTEGER NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    uuid        TEXT NOT NULL UNIQUE,
    name        TEXT NOT NULL,
    traffic_limit_bytes INTEGER NOT NULL DEFAULT 0, -- 0 = unlimited
    traffic_used_bytes  INTEGER NOT NULL DEFAULT 0,
    enabled     INTEGER NOT NULL DEFAULT 1,
    device_limit INTEGER NOT NULL DEFAULT 0,
    device_count INTEGER NOT NULL DEFAULT 0,
    expires_at  TEXT,
    created_at  TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(tenant_id, name)
);

CREATE TABLE IF NOT EXISTS traffic_hourly (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id   INTEGER NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    hour_bucket TEXT NOT NULL, -- YYYY-MM-DD HH:00
    bytes_up    INTEGER NOT NULL DEFAULT 0,
    bytes_down  INTEGER NOT NULL DEFAULT 0,
    UNIQUE(tenant_id, hour_bucket)
);

CREATE TABLE IF NOT EXISTS traffic_by_country (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id   INTEGER NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    country_code TEXT NOT NULL,
    bytes       INTEGER NOT NULL DEFAULT 0,
    updated_at  TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(tenant_id, country_code)
);

CREATE TABLE IF NOT EXISTS connection_log (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id   INTEGER NOT NULL,
    user_id     INTEGER,
    dest_ip     TEXT,
    country_code TEXT,
    bytes       INTEGER NOT NULL DEFAULT 0,
    logged_at   TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS alerts (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id   INTEGER NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    type        TEXT NOT NULL, -- server_overload | user_limit | cert_expiry
    severity    TEXT NOT NULL DEFAULT 'warning',
    message     TEXT NOT NULL,
    acknowledged INTEGER NOT NULL DEFAULT 0,
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS server_health (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    service     TEXT NOT NULL UNIQUE, -- xray | hysteria
    status      TEXT NOT NULL DEFAULT 'unknown',
    last_check  TEXT,
    restart_count INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS certificates (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id   INTEGER NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
    domain      TEXT NOT NULL,
    expires_at  TEXT NOT NULL,
    UNIQUE(tenant_id, domain)
);

CREATE TABLE IF NOT EXISTS panel_settings (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_users_tenant ON users(tenant_id);
CREATE INDEX IF NOT EXISTS idx_traffic_hourly_tenant ON traffic_hourly(tenant_id, hour_bucket);
CREATE INDEX IF NOT EXISTS idx_alerts_tenant ON alerts(tenant_id, acknowledged);
