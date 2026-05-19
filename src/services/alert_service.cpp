#include "ghostroute/services.hpp"

#include <sstream>

namespace ghostroute {

AlertService::AlertService(Database& db, const PanelConfig& cfg) : db_(db), cfg_(cfg) {}

void AlertService::check_server_load(double cpu_percent) {
    if (cpu_percent < cfg_.cpu_threshold) return;
    db_.insert_alert(1, "server_overload", "critical",
                     "CPU load " + std::to_string(static_cast<int>(cpu_percent)) + "% exceeds threshold");
}

void AlertService::check_user_limits(int64_t tenant_id) {
    for (const auto& u : db_.list_users(tenant_id, 1000)) {
        if (u.traffic_limit_bytes <= 0) continue;
        if (u.traffic_used_bytes >= u.traffic_limit_bytes) {
            std::ostringstream msg;
            msg << "User '" << u.name << "' exhausted traffic limit";
            db_.insert_alert(tenant_id, "user_limit", "warning", msg.str());
        }
    }
}

void AlertService::check_certificates(int64_t tenant_id) {
    for (const auto& [domain, expires] : db_.certs_expiring_within_days(tenant_id, cfg_.cert_warn_days)) {
        std::ostringstream msg;
        msg << "Certificate for " << domain << " expires at " << expires;
        db_.insert_alert(tenant_id, "cert_expiry", "warning", msg.str());
    }
}

} // namespace ghostroute
