#pragma once

#include "ghostroute/config.hpp"
#include "ghostroute/database.hpp"
#include "ghostroute/services.hpp"
#include "ghostroute/tenant_context.hpp"

#include <httplib.h>
#include <memory>

namespace ghostroute {

class App {
public:
    explicit App(PanelConfig cfg);
    void run();

private:
    PanelConfig cfg_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<GeoIpService> geo_;
    std::unique_ptr<AnalyticsService> analytics_;
    std::unique_ptr<AlertService> alerts_;
    std::unique_ptr<HealthService> health_;
    std::unique_ptr<ProvisionService> provision_;
    std::unique_ptr<XrayManager> xray_;
    std::unique_ptr<HysteriaManager> hysteria_;
    TenantResolver tenant_resolver_;

    void register_routes(httplib::Server& svr);
    int64_t resolve_tenant_id(const httplib::Request& req) const;
};

} // namespace ghostroute
