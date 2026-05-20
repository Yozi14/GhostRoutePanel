#pragma once

#include "ghostroute/types.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <sqlite3.h>
#include <sqlite3.h>

struct sqlite3;
struct sqlite3_stmt;

namespace ghostroute {

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void migrate(const std::string& schema_path);

    int64_t ensure_tenant(const std::string& slug, const std::string& name);
    std::optional<int64_t> tenant_id_by_slug(const std::string& slug);

    int64_t create_user(int64_t tenant_id, const std::string& name, int64_t limit_bytes,
                        int device_limit = 0);
    std::vector<User> list_users(int64_t tenant_id, int limit = 100);
    std::optional<User> user_by_name(int64_t tenant_id, const std::string& name);
    bool delete_user(int64_t tenant_id, const std::string& name);
    bool update_user(int64_t tenant_id, const std::string& name, std::optional<bool> enabled,
                     std::optional<int64_t> limit_bytes, std::optional<int> device_limit);
    bool reset_user_traffic(int64_t tenant_id, const std::string& name);

    void record_traffic_hourly(int64_t tenant_id, const std::string& hour, int64_t up, int64_t down);
    std::vector<HourlyTraffic> hourly_traffic(int64_t tenant_id, int hours = 24);

    void record_country_traffic(int64_t tenant_id, const std::string& cc, int64_t bytes);
    std::vector<CountryTraffic> country_map(int64_t tenant_id);

    std::vector<User> top_users_by_traffic(int64_t tenant_id, int limit = 10);

    int64_t insert_alert(int64_t tenant_id, const std::string& type, const std::string& severity,
                         const std::string& message);
    std::vector<Alert> list_alerts(int64_t tenant_id, bool unacked_only = true);

    void update_service_health(const std::string& service, const std::string& status);
    std::vector<ServiceHealth> all_health();

    std::vector<std::pair<std::string, std::string>> certs_expiring_within_days(int64_t tenant_id, int days);

private:
    sqlite3* db_{nullptr};
    void exec(const std::string& sql);
    void apply_migrations();
    static User row_to_user(sqlite3_stmt* stmt);
};

} // namespace ghostroute
