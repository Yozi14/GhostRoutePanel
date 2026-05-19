#pragma once

#include <nlohmann/json.hpp>

#include "ghostroute/database.hpp"
#include "ghostroute/types.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace ghostroute {

class GeoIpService {
public:
    explicit GeoIpService(const std::string& mmdb_path);
    std::string country_code(const std::string& ip) const;
    bool available() const { return available_; }

private:
    bool available_{false};
    std::string db_path_;
};

class AnalyticsService {
public:
    AnalyticsService(Database& db, GeoIpService& geo);

    void ingest_connection(int64_t tenant_id, int64_t user_id, const std::string& dest_ip, int64_t bytes);
    nlohmann::json dashboard(int64_t tenant_id);

private:
    Database& db_;
    GeoIpService& geo_;
};

class AlertService {
public:
    AlertService(Database& db, const PanelConfig& cfg);
    void check_server_load(double cpu_percent);
    void check_user_limits(int64_t tenant_id);
    void check_certificates(int64_t tenant_id);

private:
    Database& db_;
    PanelConfig cfg_;
};

class HealthService {
public:
    using RestartFn = std::function<bool(const std::string& service)>;

    HealthService(Database& db, const PanelConfig& cfg, RestartFn restart_fn);
    void start();
    void stop();

private:
    Database& db_;
    PanelConfig cfg_;
    RestartFn restart_;
    std::atomic<bool> running_{false};
    std::thread worker_;

    bool is_alive(const std::string& binary) const;
    void tick();
};

class ProvisionService {
public:
    explicit ProvisionService(const PanelConfig& cfg);

    bool install_dependencies();
    bool setup_xray(const std::string& domain);
    bool setup_hysteria(const std::string& domain);
    bool setup_nginx_tls(const std::string& domain, const std::string& email);
    nlohmann::json status() const;

private:
    PanelConfig cfg_;
    int run_script(const std::string& script, const std::vector<std::string>& args) const;
};

class XrayManager {
public:
    explicit XrayManager(const PanelConfig& cfg);
    bool sync_users(const std::vector<User>& users);
    bool reload();
    nlohmann::json generate_config(const std::vector<User>& users) const;

private:
    PanelConfig cfg_;
};

class HysteriaManager {
public:
    explicit HysteriaManager(const PanelConfig& cfg);
    bool sync_users(const std::vector<User>& users);
    bool reload();

private:
    PanelConfig cfg_;
};

class AutoUpdater {
public:
    struct Result {
        bool ok{};
        std::string message;
        std::string new_version;
    };

    static Result check_and_update(const std::string& repo, const std::string& channel);
};

class SystemStats {
public:
    static SystemMetrics sample();
};

} // namespace ghostroute
