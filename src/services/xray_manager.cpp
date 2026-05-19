#include "ghostroute/services.hpp"

#include <fstream>
#include <spdlog/spdlog.h>

#ifndef _WIN32
#include <cstdlib>
#endif

namespace ghostroute {

XrayManager::XrayManager(const PanelConfig& cfg) : cfg_(cfg) {}

nlohmann::json XrayManager::generate_config(const std::vector<User>& users) const {
    nlohmann::json cfg;
    cfg["log"] = {{"loglevel", "warning"}};
    cfg["inbounds"] = nlohmann::json::array({
        {{"port", 443},
         {"protocol", "vless"},
         {"settings", {{"clients", nlohmann::json::array()}}},
         {"streamSettings", {{"network", "tcp"}, {"security", "reality"}}}},
    });
    auto& clients = cfg["inbounds"][0]["settings"]["clients"];
    for (const auto& u : users) {
        if (!u.enabled) continue;
        clients.push_back({{"id", u.uuid}, {"email", u.name}});
    }
    cfg["outbounds"] = nlohmann::json::array({{{"protocol", "freedom"}, {"tag", "direct"}}});
    return cfg;
}

bool XrayManager::sync_users(const std::vector<User>& users) {
    auto cfg = generate_config(users);
    std::ofstream out(cfg_.xray_config);
    if (!out) {
        spdlog::error("Cannot write xray config to {}", cfg_.xray_config);
        return false;
    }
    out << cfg.dump(2);
    return reload();
}

bool XrayManager::reload() {
#ifndef _WIN32
    std::string cmd = cfg_.xray_binary + " run -test -config " + cfg_.xray_config;
    if (std::system(cmd.c_str()) != 0) return false;
    return std::system("systemctl reload xray 2>/dev/null || systemctl restart xray") == 0;
#else
    spdlog::info("Xray config written (reload on Linux target)");
    return true;
#endif
}

} // namespace ghostroute
