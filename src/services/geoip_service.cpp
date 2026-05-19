#include "ghostroute/services.hpp"

#include <fstream>
#include <spdlog/spdlog.h>

namespace ghostroute {

GeoIpService::GeoIpService(const std::string& mmdb_path) : db_path_(mmdb_path) {
    std::ifstream f(mmdb_path, std::ios::binary);
    available_ = f.good();
    if (!available_)
        spdlog::warn("GeoIP database not found at {} — country map disabled", mmdb_path);
    // Production: integrate maxminddb library (libmaxminddb)
}

std::string GeoIpService::country_code(const std::string& ip) const {
    if (!available_) return "XX";
    (void)ip;
    // Stub: replace with MMDB_lookup_string when libmaxminddb linked
    return "XX";
}

} // namespace ghostroute
