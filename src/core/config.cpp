#include "ghostroute/config.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

namespace ghostroute {

PanelConfig Config::defaults() {
    return PanelConfig{};
}

PanelConfig Config::load(const std::string& path) {
    auto cfg = defaults();
    std::ifstream f(path);
    if (!f) return cfg;

    nlohmann::json j;
    f >> j;

    if (j.contains("panel")) {
        const auto& p = j["panel"];
        if (p.contains("host")) cfg.host = p["host"].get<std::string>();
        if (p.contains("port")) cfg.port = p["port"].get<int>();
        if (p.contains("web_root")) cfg.web_root = p["web_root"].get<std::string>();
        if (p.contains("data_dir")) cfg.data_dir = p["data_dir"].get<std::string>();
        if (p.contains("database")) cfg.database = p["database"].get<std::string>();
        if (p.contains("domain")) cfg.domain = p["domain"].get<std::string>();
    }
    if (j.contains("multitenancy")) {
        const auto& m = j["multitenancy"];
        if (m.contains("enabled")) cfg.multitenancy = m["enabled"].get<bool>();
        if (m.contains("default_tenant")) cfg.default_tenant = m["default_tenant"].get<std::string>();
    }
    if (j.contains("geoip") && j["geoip"].contains("database_path"))
        cfg.geoip_db = j["geoip"]["database_path"].get<std::string>();
    if (j.contains("health")) {
        const auto& h = j["health"];
        if (h.contains("check_interval_sec")) cfg.health_interval_sec = h["check_interval_sec"].get<int>();
        if (h.contains("auto_restart")) cfg.health_auto_restart = h["auto_restart"].get<bool>();
        if (h.contains("services")) {
            if (h["services"].contains("xray")) {
                const auto& x = h["services"]["xray"];
                if (x.contains("binary")) cfg.xray_binary = x["binary"].get<std::string>();
                if (x.contains("config")) cfg.xray_config = x["config"].get<std::string>();
            }
            if (h["services"].contains("hysteria")) {
                const auto& hy = h["services"]["hysteria"];
                if (hy.contains("binary")) cfg.hysteria_binary = hy["binary"].get<std::string>();
                if (hy.contains("config")) cfg.hysteria_config = hy["config"].get<std::string>();
            }
        }
    }
    if (j.contains("alerts")) {
        const auto& a = j["alerts"];
        if (a.contains("cpu_threshold_percent")) cfg.cpu_threshold = a["cpu_threshold_percent"].get<int>();
        if (a.contains("cert_warn_days")) cfg.cert_warn_days = a["cert_warn_days"].get<int>();
    }
    return cfg;
}

} // namespace ghostroute
