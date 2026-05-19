#include "ghostroute/app.hpp"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace ghostroute {

App::App(PanelConfig cfg)
    : cfg_(std::move(cfg)),
      tenant_resolver_(cfg_.multitenancy, cfg_.default_tenant) {}

nlohmann::json App::user_to_json(const User& u) {
    const auto pct = u.traffic_limit_bytes > 0
                         ? (100.0 * u.traffic_used_bytes) / u.traffic_limit_bytes
                         : 0.0;
    return {{"id", u.id},
            {"name", u.name},
            {"uuid", u.uuid},
            {"used", u.traffic_used_bytes},
            {"limit", u.traffic_limit_bytes},
            {"enabled", u.enabled},
            {"device_limit", u.device_limit},
            {"device_count", u.device_count},
            {"usage_percent", pct}};
}

void App::sync_vpn_users(int64_t tenant_id) {
    auto users = db_->list_users(tenant_id);
    xray_->sync_users(users);
    hysteria_->sync_users(users);
}

int64_t App::resolve_tenant_id(const httplib::Request& req) const {
    std::optional<std::string> header;
    if (req.has_header("X-Tenant-ID")) header = req.get_header_value("X-Tenant-ID");
    auto ctx = tenant_resolver_.resolve(req.get_header_value("Host"), header, 0);
    auto slug = ctx ? ctx->slug : cfg_.default_tenant;
    auto id = db_->tenant_id_by_slug(slug);
    return id.value_or(1);
}

void App::register_routes(httplib::Server& svr) {
    svr.set_mount_point("/", cfg_.web_root.c_str());

    svr.Get("/api/health", [this](const httplib::Request&, httplib::Response& res) {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& h : db_->all_health())
            j.push_back({{"service", h.service}, {"status", h.status}, {"last_check", h.last_check}});
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/analytics/dashboard", [this](const httplib::Request& req, httplib::Response& res) {
        auto tid = resolve_tenant_id(req);
        res.set_content(analytics_->dashboard(tid).dump(), "application/json");
    });

    svr.Get("/api/alerts", [this](const httplib::Request& req, httplib::Response& res) {
        auto tid = resolve_tenant_id(req);
        nlohmann::json j = nlohmann::json::array();
        for (const auto& a : db_->list_alerts(tid, true))
            j.push_back({{"id", a.id},
                         {"type", a.type},
                         {"severity", a.severity},
                         {"message", a.message},
                         {"created_at", a.created_at}});
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/system/stats", [this](const httplib::Request& req, httplib::Response& res) {
        (void)req;
        auto m = SystemStats::sample();
        alerts_->check_server_load(m.cpu_percent);
        nlohmann::json j = {{"cpu_percent", m.cpu_percent},
                            {"ram_percent", m.ram_percent},
                            {"ram_used_mb", m.ram_used_mb},
                            {"ram_total_mb", m.ram_total_mb},
                            {"disk_percent", m.disk_percent},
                            {"uptime_sec", m.uptime_sec},
                            {"load_avg_1", m.load_avg_1}};
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/api/users", [this](const httplib::Request& req, httplib::Response& res) {
        auto tid = resolve_tenant_id(req);
        nlohmann::json j = nlohmann::json::array();
        for (const auto& u : db_->list_users(tid)) j.push_back(user_to_json(u));
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/users", [this](const httplib::Request& req, httplib::Response& res) {
        auto tid = resolve_tenant_id(req);
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("name")) {
            res.status = 400;
            res.set_content(R"({"error":"name required"})", "application/json");
            return;
        }
        int64_t limit = 0;
        int device_limit = 0;
        if (body.contains("limit_bytes")) limit = body["limit_bytes"].get<int64_t>();
        if (body.contains("device_limit")) device_limit = body["device_limit"].get<int>();
        db_->create_user(tid, body["name"].get<std::string>(), limit, device_limit);
        sync_vpn_users(tid);
        res.set_content(R"({"ok":true})", "application/json");
    });

    svr.Patch(R"(/api/users/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        auto tid = resolve_tenant_id(req);
        auto name = req.matches[1].str();
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
            return;
        }
        std::optional<bool> enabled;
        std::optional<int64_t> limit;
        std::optional<int> devices;
        if (body.contains("enabled")) enabled = body["enabled"].get<bool>();
        if (body.contains("limit_bytes")) limit = body["limit_bytes"].get<int64_t>();
        if (body.contains("device_limit")) devices = body["device_limit"].get<int>();
        if (!db_->update_user(tid, name, enabled, limit, devices)) {
            res.status = 404;
            res.set_content(R"({"error":"user not found"})", "application/json");
            return;
        }
        sync_vpn_users(tid);
        res.set_content(R"({"ok":true})", "application/json");
    });

    svr.Post(R"(/api/users/([^/]+)/reset-traffic)",
             [this](const httplib::Request& req, httplib::Response& res) {
                 auto tid = resolve_tenant_id(req);
                 auto name = req.matches[1].str();
                 if (!db_->reset_user_traffic(tid, name)) {
                     res.status = 404;
                     res.set_content(R"({"error":"user not found"})", "application/json");
                     return;
                 }
                 res.set_content(R"({"ok":true})", "application/json");
             });

    svr.Get(R"(/api/users/([^/]+)/link)", [this](const httplib::Request& req, httplib::Response& res) {
        auto tid = resolve_tenant_id(req);
        auto name = req.matches[1].str();
        auto u = db_->user_by_name(tid, name);
        if (!u) {
            res.status = 404;
            res.set_content(R"({"error":"user not found"})", "application/json");
            return;
        }
        const std::string sub =
            "vless://" + u->uuid + "@YOUR_DOMAIN:443?encryption=none&security=reality&type=tcp#"
            + u->name;
        res.set_content(nlohmann::json{{"subscription", sub}, {"uuid", u->uuid}}.dump(),
                        "application/json");
    });

    svr.Delete(R"(/api/users/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        auto tid = resolve_tenant_id(req);
        auto name = req.matches[1].str();
        db_->delete_user(tid, name);
        sync_vpn_users(tid);
        res.set_content(R"({"ok":true})", "application/json");
    });

    svr.Post("/api/provision/xray", [this](const httplib::Request& req, httplib::Response& res) {
        auto body = nlohmann::json::parse(req.body);
        bool ok = provision_->setup_xray(body.value("domain", ""));
        res.set_content(nlohmann::json{{"ok", ok}}.dump(), "application/json");
    });

    svr.Post("/api/provision/full", [this](const httplib::Request& req, httplib::Response& res) {
        auto body = nlohmann::json::parse(req.body);
        std::string domain = body.value("domain", "");
        std::string email = body.value("email", "");
        bool ok = provision_->install_dependencies() && provision_->setup_nginx_tls(domain, email) &&
                  provision_->setup_xray(domain) && provision_->setup_hysteria(domain);
        res.set_content(nlohmann::json{{"ok", ok}}.dump(), "application/json");
    });

    svr.Get("/api/provision/status", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(provision_->status().dump(), "application/json");
    });
}

void App::run() {
    fs::create_directories(cfg_.data_dir);

    db_ = std::make_unique<Database>(cfg_.database);
    db_->migrate("data/schema.sql");
    db_->ensure_tenant(cfg_.default_tenant, "Default");

    geo_ = std::make_unique<GeoIpService>(cfg_.geoip_db);
    analytics_ = std::make_unique<AnalyticsService>(*db_, *geo_);
    alerts_ = std::make_unique<AlertService>(*db_, cfg_);
    provision_ = std::make_unique<ProvisionService>(cfg_);
    xray_ = std::make_unique<XrayManager>(cfg_);
    hysteria_ = std::make_unique<HysteriaManager>(cfg_);

    health_ = std::make_unique<HealthService>(
        *db_, cfg_, [](const std::string& svc) -> bool {
#ifndef _WIN32
            std::string cmd = "systemctl restart " + svc;
            return std::system(cmd.c_str()) == 0;
#else
            (void)svc;
            return true;
#endif
        });
    health_->start();

    httplib::Server svr;
    register_routes(svr);

    spdlog::info("GhostRoutePanel listening on {}:{}", cfg_.host, cfg_.port);
    svr.listen(cfg_.host.c_str(), cfg_.port);
    health_->stop();
}

} // namespace ghostroute
