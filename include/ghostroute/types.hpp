#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ghostroute {

struct Tenant {
    int64_t id{};
    std::string slug;
    std::string name;
};

struct User {
    int64_t id{};
    int64_t tenant_id{};
    std::string uuid;
    std::string name;
    int64_t traffic_limit_bytes{};
    int64_t traffic_used_bytes{};
    bool enabled{true};
    std::optional<std::string> expires_at;
};

struct Alert {
    int64_t id{};
    int64_t tenant_id{};
    std::string type;
    std::string severity;
    std::string message;
    bool acknowledged{};
    std::string created_at;
};

struct HourlyTraffic {
    std::string hour_bucket;
    int64_t bytes_up{};
    int64_t bytes_down{};
};

struct CountryTraffic {
    std::string country_code;
    int64_t bytes{};
};

struct ServiceHealth {
    std::string service;
    std::string status; // alive | dead | unknown
    std::string last_check;
    int restart_count{};
};

struct PanelConfig {
    std::string host{"0.0.0.0"};
    int port{8080};
    std::string web_root{"./web"};
    std::string data_dir{"./data/runtime"};
    std::string database{"./data/runtime/panel.db"};
    bool multitenancy{true};
    std::string default_tenant{"default"};
    std::string geoip_db{"./data/GeoLite2-Country.mmdb"};
    int health_interval_sec{30};
    bool health_auto_restart{true};
    std::string xray_binary{"/usr/local/bin/xray"};
    std::string xray_config{"/etc/ghostroute/xray/config.json"};
    std::string hysteria_binary{"/usr/local/bin/hysteria"};
    std::string hysteria_config{"/etc/ghostroute/hysteria/config.yaml"};
    int cpu_threshold{85};
    int cert_warn_days{7};
};

} // namespace ghostroute
