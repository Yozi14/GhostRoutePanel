#include "ghostroute/services.hpp"

#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>

#ifndef _WIN32
#include <cstdlib>
#endif

namespace ghostroute {

HysteriaManager::HysteriaManager(const PanelConfig& cfg) : cfg_(cfg) {}

bool HysteriaManager::sync_users(const std::vector<User>& users) {
    std::ostringstream yaml;
    yaml << "listen: :443\n";
    yaml << "tls:\n  cert: /etc/ghostroute/certs/fullchain.pem\n";
    yaml << "  key: /etc/ghostroute/certs/privkey.pem\n";
    yaml << "auth:\n  type: userpass\n  userpass:\n";
    for (const auto& u : users) {
        if (!u.enabled) continue;
        yaml << "    " << u.name << ": " << u.uuid.substr(0, 16) << "\n";
    }
    std::ofstream out(cfg_.hysteria_config);
    if (!out) return false;
    out << yaml.str();
    return reload();
}

bool HysteriaManager::reload() {
#ifndef _WIN32
    return std::system("systemctl restart hysteria") == 0;
#else
    return true;
#endif
}

} // namespace ghostroute
