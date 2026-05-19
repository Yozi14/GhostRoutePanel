#include "ghostroute/config.hpp"
#include "ghostroute/database.hpp"
#include "ghostroute/services.hpp"

#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int64_t parse_size(const std::string& s) {
    if (s.empty()) return 0;
    std::string num;
    std::string unit;
    for (char c : s) {
        if (std::isalpha(static_cast<unsigned char>(c)))
            unit += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        else if (std::isdigit(static_cast<unsigned char>(c)) || c == '.')
            num += c;
    }
    double v = num.empty() ? 0 : std::stod(num);
    if (unit.find("tb") != std::string::npos) return static_cast<int64_t>(v * 1024LL * 1024 * 1024 * 1024);
    if (unit.find("gb") != std::string::npos) return static_cast<int64_t>(v * 1024LL * 1024 * 1024);
    if (unit.find("mb") != std::string::npos) return static_cast<int64_t>(v * 1024LL * 1024);
    if (unit.find("kb") != std::string::npos) return static_cast<int64_t>(v * 1024);
    return static_cast<int64_t>(v);
}

void usage() {
    std::cout << "GhostRoutePanel CLI\n\n"
              << "  ghostroute user add --name <name> [--limit 100gb] [--tenant slug]\n"
              << "  ghostroute user list [--tenant slug]\n"
              << "  ghostroute user del --name <name> [--tenant slug]\n"
              << "  ghostroute tenant add --slug <slug> --name <name>\n"
              << "  ghostroute update [--channel stable|beta]\n"
              << "  ghostroute health\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        return 1;
    }

    auto cfg = ghostroute::Config::load("config/panel.example.json");
    std::filesystem::create_directories(cfg.data_dir);
    ghostroute::Database db(cfg.database);
    db.migrate("data/schema.sql");

    std::string cmd = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) args.push_back(argv[i]);

    auto get_arg = [&](const std::string& key) -> std::string {
        for (size_t i = 0; i + 1 < args.size(); ++i)
            if (args[i] == key) return args[i + 1];
        return "";
    };

    std::string tenant_slug = get_arg("--tenant");
    if (tenant_slug.empty()) tenant_slug = cfg.default_tenant;
    auto tenant_id = db.tenant_id_by_slug(tenant_slug);
    if (!tenant_id) tenant_id = db.ensure_tenant(tenant_slug, tenant_slug);

    if (cmd == "update") {
        std::string channel = get_arg("--channel");
        if (channel.empty()) channel = "stable";
        auto r = ghostroute::AutoUpdater::check_and_update("ghostroute/GhostRoutePanel", channel);
        std::cout << r.message << "\n";
        return r.ok ? 0 : 1;
    }

    if (cmd == "health") {
        for (const auto& h : db.all_health())
            std::cout << h.service << ": " << h.status << " (restarts: " << h.restart_count << ")\n";
        return 0;
    }

    if (cmd == "tenant" && argc >= 3 && std::string(argv[2]) == "add") {
        auto slug = get_arg("--slug");
        auto name = get_arg("--name");
        if (slug.empty() || name.empty()) {
            std::cerr << "--slug and --name required\n";
            return 1;
        }
        auto id = db.ensure_tenant(slug, name);
        std::cout << "Tenant created id=" << id << " slug=" << slug << "\n";
        return 0;
    }

    if (cmd == "user") {
        if (argc < 3) {
            usage();
            return 1;
        }
        std::string sub = argv[2];
        ghostroute::XrayManager xray(cfg);
        ghostroute::HysteriaManager hysteria(cfg);

        if (sub == "add") {
            auto name = get_arg("--name");
            auto limit_s = get_arg("--limit");
            if (name.empty()) {
                std::cerr << "--name required\n";
                return 1;
            }
            int64_t limit = limit_s.empty() ? 0 : parse_size(limit_s);
            db.create_user(*tenant_id, name, limit);
            auto users = db.list_users(*tenant_id);
            xray.sync_users(users);
            hysteria.sync_users(users);
            std::cout << "User '" << name << "' added (limit=" << limit << " bytes)\n";
            return 0;
        }
        if (sub == "list") {
            for (const auto& u : db.list_users(*tenant_id))
                std::cout << u.name << "  used=" << u.traffic_used_bytes
                          << "  limit=" << u.traffic_limit_bytes << "\n";
            return 0;
        }
        if (sub == "del") {
            auto name = get_arg("--name");
            if (name.empty()) {
                std::cerr << "--name required\n";
                return 1;
            }
            db.delete_user(*tenant_id, name);
            auto users = db.list_users(*tenant_id);
            xray.sync_users(users);
            hysteria.sync_users(users);
            std::cout << "User deleted\n";
            return 0;
        }
    }

    usage();
    return 1;
}
