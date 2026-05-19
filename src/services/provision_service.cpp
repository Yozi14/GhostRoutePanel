#include "ghostroute/services.hpp"

#include <spdlog/spdlog.h>

#ifndef _WIN32
#include <cstdlib>
#endif

namespace ghostroute {

ProvisionService::ProvisionService(const PanelConfig& cfg) : cfg_(cfg) {}

int ProvisionService::run_script(const std::string& script,
                                 const std::vector<std::string>& args) const {
#ifndef _WIN32
    std::string cmd = "bash " + script;
    for (const auto& a : args) cmd += " \"" + a + "\"";
    return std::system(cmd.c_str());
#else
    (void)script;
    (void)args;
    spdlog::warn("Provisioning scripts require Linux server");
    return -1;
#endif
}

bool ProvisionService::install_dependencies() {
    return run_script("scripts/install.sh", {}) == 0;
}

bool ProvisionService::setup_xray(const std::string& domain) {
    return run_script("scripts/provision-xray.sh", {domain}) == 0;
}

bool ProvisionService::setup_hysteria(const std::string& domain) {
    return run_script("scripts/provision-hysteria.sh", {domain}) == 0;
}

bool ProvisionService::setup_nginx_tls(const std::string& domain, const std::string& email) {
    return run_script("scripts/provision-nginx.sh", {domain, email}) == 0;
}

nlohmann::json ProvisionService::status() const {
    return {{"xray_config", cfg_.xray_config},
            {"hysteria_config", cfg_.hysteria_config},
            {"ready", true}};
}

} // namespace ghostroute
