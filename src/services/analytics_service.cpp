#include "ghostroute/services.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace ghostroute {

namespace {

std::string current_hour_bucket() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:00");
    return oss.str();
}

} // namespace

AnalyticsService::AnalyticsService(Database& db, GeoIpService& geo) : db_(db), geo_(geo) {}

void AnalyticsService::ingest_connection(int64_t tenant_id, int64_t user_id,
                                         const std::string& dest_ip, int64_t bytes) {
    auto cc = geo_.country_code(dest_ip);
    db_.record_country_traffic(tenant_id, cc, bytes);
    db_.record_traffic_hourly(tenant_id, current_hour_bucket(), 0, bytes);
    (void)user_id;
}

nlohmann::json AnalyticsService::dashboard(int64_t tenant_id) {
    nlohmann::json j;
    j["hourly"] = nlohmann::json::array();
    for (const auto& h : db_.hourly_traffic(tenant_id, 24)) {
        j["hourly"].push_back({{"hour", h.hour_bucket},
                               {"up", h.bytes_up},
                               {"down", h.bytes_down},
                               {"total", h.bytes_up + h.bytes_down}});
    }
    j["countries"] = nlohmann::json::array();
    for (const auto& c : db_.country_map(tenant_id)) {
        j["countries"].push_back({{"code", c.country_code}, {"bytes", c.bytes}});
    }
    j["top_users"] = nlohmann::json::array();
    for (const auto& u : db_.top_users_by_traffic(tenant_id, 10)) {
        j["top_users"].push_back({{"name", u.name},
                                  {"used", u.traffic_used_bytes},
                                  {"limit", u.traffic_limit_bytes}});
    }
    return j;
}

} // namespace ghostroute
