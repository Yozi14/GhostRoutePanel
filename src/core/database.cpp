#include "ghostroute/database.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <random>
#include <sstream>
#include <sqlite3.h>
#include <spdlog/spdlog.h>

namespace ghostroute {

namespace {

std::string uuid_v4() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    auto a = dis(gen), b = dis(gen);
    std::ostringstream oss;
    oss << std::hex;
    oss.width(16);
    oss.fill('0');
    oss << a << b;
    return oss.str().substr(0, 32);
}

} // namespace

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK)
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db_)));
    exec("PRAGMA foreign_keys = ON;");
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw std::runtime_error("SQL error: " + msg);
    }
}

void Database::migrate(const std::string& schema_path) {
    std::ifstream f(schema_path);
    if (!f) throw std::runtime_error("Schema not found: " + schema_path);
    std::ostringstream ss;
    ss << f.rdbuf();
    exec(ss.str());
}

int64_t Database::ensure_tenant(const std::string& slug, const std::string& name) {
    if (auto id = tenant_id_by_slug(slug)) return *id;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO tenants (slug, name) VALUES (?, ?);";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, slug.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return sqlite3_last_insert_rowid(db_);
}

std::optional<int64_t> Database::tenant_id_by_slug(const std::string& slug) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT id FROM tenants WHERE slug = ?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, slug.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto id = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
        return id;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

int64_t Database::create_user(int64_t tenant_id, const std::string& name, int64_t limit_bytes) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO users (tenant_id, uuid, name, traffic_limit_bytes) VALUES (?, ?, ?, ?);";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    auto uid = uuid_v4();
    sqlite3_bind_int64(stmt, 1, tenant_id);
    sqlite3_bind_text(stmt, 2, uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, limit_bytes);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        auto err = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        throw std::runtime_error(err);
    }
    sqlite3_finalize(stmt);
    return sqlite3_last_insert_rowid(db_);
}

std::vector<User> Database::list_users(int64_t tenant_id, int limit) {
    std::vector<User> out;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
                       "SELECT id, tenant_id, uuid, name, traffic_limit_bytes, traffic_used_bytes, "
                       "enabled FROM users WHERE tenant_id = ? LIMIT ?;",
                       -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, tenant_id);
    sqlite3_bind_int(stmt, 2, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        User u;
        u.id = sqlite3_column_int64(stmt, 0);
        u.tenant_id = sqlite3_column_int64(stmt, 1);
        u.uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        u.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        u.traffic_limit_bytes = sqlite3_column_int64(stmt, 4);
        u.traffic_used_bytes = sqlite3_column_int64(stmt, 5);
        u.enabled = sqlite3_column_int(stmt, 6) != 0;
        out.push_back(std::move(u));
    }
    sqlite3_finalize(stmt);
    return out;
}

std::optional<User> Database::user_by_name(int64_t tenant_id, const std::string& name) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
                       "SELECT id, tenant_id, uuid, name, traffic_limit_bytes, traffic_used_bytes, "
                       "enabled FROM users WHERE tenant_id = ? AND name = ?;",
                       -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, tenant_id);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    User u;
    u.id = sqlite3_column_int64(stmt, 0);
    u.tenant_id = sqlite3_column_int64(stmt, 1);
    u.uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    u.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    u.traffic_limit_bytes = sqlite3_column_int64(stmt, 4);
    u.traffic_used_bytes = sqlite3_column_int64(stmt, 5);
    u.enabled = sqlite3_column_int(stmt, 6) != 0;
    sqlite3_finalize(stmt);
    return u;
}

bool Database::delete_user(int64_t tenant_id, const std::string& name) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "DELETE FROM users WHERE tenant_id = ? AND name = ?;", -1, &stmt,
                       nullptr);
    sqlite3_bind_int64(stmt, 1, tenant_id);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    auto changed = sqlite3_changes(db_) > 0;
    sqlite3_finalize(stmt);
    return changed;
}

void Database::record_traffic_hourly(int64_t tenant_id, const std::string& hour, int64_t up,
                                     int64_t down) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO traffic_hourly (tenant_id, hour_bucket, bytes_up, bytes_down) VALUES (?, ?, "
        "?, ?) ON CONFLICT(tenant_id, hour_bucket) DO UPDATE SET bytes_up = bytes_up + ?, "
        "bytes_down = bytes_down + ?;";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, tenant_id);
    sqlite3_bind_text(stmt, 2, hour.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, up);
    sqlite3_bind_int64(stmt, 4, down);
    sqlite3_bind_int64(stmt, 5, up);
    sqlite3_bind_int64(stmt, 6, down);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<HourlyTraffic> Database::hourly_traffic(int64_t tenant_id, int hours) {
    std::vector<HourlyTraffic> out;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
                       "SELECT hour_bucket, bytes_up, bytes_down FROM traffic_hourly WHERE "
                       "tenant_id = ? ORDER BY hour_bucket DESC LIMIT ?;",
                       -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, tenant_id);
    sqlite3_bind_int(stmt, 2, hours);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HourlyTraffic h;
        h.hour_bucket = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        h.bytes_up = sqlite3_column_int64(stmt, 1);
        h.bytes_down = sqlite3_column_int64(stmt, 2);
        out.push_back(std::move(h));
    }
    sqlite3_finalize(stmt);
    std::reverse(out.begin(), out.end());
    return out;
}

void Database::record_country_traffic(int64_t tenant_id, const std::string& cc, int64_t bytes) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO traffic_by_country (tenant_id, country_code, bytes) VALUES (?, ?, ?) "
        "ON CONFLICT(tenant_id, country_code) DO UPDATE SET bytes = bytes + ?, updated_at = "
        "datetime('now');";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, tenant_id);
    sqlite3_bind_text(stmt, 2, cc.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, bytes);
    sqlite3_bind_int64(stmt, 4, bytes);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<CountryTraffic> Database::country_map(int64_t tenant_id) {
    std::vector<CountryTraffic> out;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
                       "SELECT country_code, bytes FROM traffic_by_country WHERE tenant_id = ? "
                       "ORDER BY bytes DESC;",
                       -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, tenant_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CountryTraffic c;
        c.country_code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        c.bytes = sqlite3_column_int64(stmt, 1);
        out.push_back(std::move(c));
    }
    sqlite3_finalize(stmt);
    return out;
}

std::vector<User> Database::top_users_by_traffic(int64_t tenant_id, int limit) {
    std::vector<User> out;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
                       "SELECT id, tenant_id, uuid, name, traffic_limit_bytes, traffic_used_bytes, "
                       "enabled FROM users WHERE tenant_id = ? ORDER BY traffic_used_bytes DESC "
                       "LIMIT ?;",
                       -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, tenant_id);
    sqlite3_bind_int(stmt, 2, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        User u;
        u.id = sqlite3_column_int64(stmt, 0);
        u.tenant_id = sqlite3_column_int64(stmt, 1);
        u.uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        u.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        u.traffic_limit_bytes = sqlite3_column_int64(stmt, 4);
        u.traffic_used_bytes = sqlite3_column_int64(stmt, 5);
        u.enabled = sqlite3_column_int(stmt, 6) != 0;
        out.push_back(std::move(u));
    }
    sqlite3_finalize(stmt);
    return out;
}

int64_t Database::insert_alert(int64_t tenant_id, const std::string& type, const std::string& severity,
                             const std::string& message) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
                       "INSERT INTO alerts (tenant_id, type, severity, message) VALUES (?, ?, ?, ?);",
                       -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, tenant_id);
    sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, severity.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return sqlite3_last_insert_rowid(db_);
}

std::vector<Alert> Database::list_alerts(int64_t tenant_id, bool unacked_only) {
    std::vector<Alert> out;
    std::string sql =
        "SELECT id, tenant_id, type, severity, message, acknowledged, created_at FROM alerts "
        "WHERE tenant_id = ?";
    if (unacked_only) sql += " AND acknowledged = 0";
    sql += " ORDER BY created_at DESC LIMIT 50;";
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, tenant_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Alert a;
        a.id = sqlite3_column_int64(stmt, 0);
        a.tenant_id = sqlite3_column_int64(stmt, 1);
        a.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        a.severity = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        a.message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        a.acknowledged = sqlite3_column_int(stmt, 5) != 0;
        a.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        out.push_back(std::move(a));
    }
    sqlite3_finalize(stmt);
    return out;
}

void Database::update_service_health(const std::string& service, const std::string& status) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO server_health (service, status, last_check) VALUES (?, ?, datetime('now')) "
        "ON CONFLICT(service) DO UPDATE SET status = ?, last_check = datetime('now');";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, service.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<ServiceHealth> Database::all_health() {
    std::vector<ServiceHealth> out;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
                       "SELECT service, status, last_check, restart_count FROM server_health;",
                       -1, &stmt, nullptr);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ServiceHealth h;
        h.service = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        h.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (sqlite3_column_text(stmt, 2))
            h.last_check = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        h.restart_count = sqlite3_column_int(stmt, 3);
        out.push_back(std::move(h));
    }
    sqlite3_finalize(stmt);
    return out;
}

std::vector<std::pair<std::string, std::string>> Database::certs_expiring_within_days(
    int64_t tenant_id, int days) {
    std::vector<std::pair<std::string, std::string>> out;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_,
                       "SELECT domain, expires_at FROM certificates WHERE tenant_id = ? AND "
                       "date(expires_at) <= date('now', '+' || ? || ' days');",
                       -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, tenant_id);
    sqlite3_bind_int(stmt, 2, days);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        out.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                         reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
    }
    sqlite3_finalize(stmt);
    return out;
}

} // namespace ghostroute
