#include "ghostroute/services.hpp"

#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>

#ifndef _WIN32
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace ghostroute {

HealthService::HealthService(Database& db, const PanelConfig& cfg, RestartFn restart_fn)
    : db_(db), cfg_(cfg), restart_(std::move(restart_fn)) {}

void HealthService::start() {
    running_ = true;
    worker_ = std::thread([this] {
        while (running_) {
            tick();
            std::this_thread::sleep_for(std::chrono::seconds(cfg_.health_interval_sec));
        }
    });
}

void HealthService::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

bool HealthService::is_alive(const std::string& binary) const {
#ifndef _WIN32
    std::string cmd = "pgrep -f " + binary + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
#else
    (void)binary;
    return true; // Windows: use service checks
#endif
}

void HealthService::tick() {
    struct Svc {
        const char* name;
        const std::string& bin;
    } services[] = {{"xray", cfg_.xray_binary}, {"hysteria", cfg_.hysteria_binary}};

    for (const auto& s : services) {
        bool alive = is_alive(s.bin);
        db_.update_service_health(s.name, alive ? "alive" : "dead");
        if (!alive && cfg_.health_auto_restart) {
            spdlog::warn("{} is down — attempting restart", s.name);
            if (restart_(s.name)) {
                spdlog::info("{} restarted successfully", s.name);
                db_.update_service_health(s.name, "alive");
            }
        }
    }
}

} // namespace ghostroute
