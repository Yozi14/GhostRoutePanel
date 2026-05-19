#include "ghostroute/app.hpp"
#include "ghostroute/config.hpp"

#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);
    std::string config_path = "config/panel.example.json";
    if (argc > 1) config_path = argv[1];

    auto cfg = ghostroute::Config::load(config_path);
    ghostroute::App app(cfg);
    app.run();
    return 0;
}
