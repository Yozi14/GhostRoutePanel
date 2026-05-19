#include "ghostroute/services.hpp"

#include <spdlog/spdlog.h>

#ifndef _WIN32
#include <cstdlib>
#endif

namespace ghostroute {

AutoUpdater::Result AutoUpdater::check_and_update(const std::string& repo,
                                                  const std::string& channel) {
    Result r;
    (void)channel;
    spdlog::info("Checking updates for {}", repo);

#ifndef _WIN32
    std::string cmd =
        "curl -fsSL https://api.github.com/repos/" + repo + "/releases/latest | "
        "grep tag_name | head -1";
    if (std::system(cmd.c_str()) != 0) {
        r.message = "Failed to check GitHub releases";
        return r;
    }
    if (std::system("bash scripts/self-update.sh") == 0) {
        r.ok = true;
        r.message = "GhostRoutePanel updated successfully";
        r.new_version = "latest";
    } else {
        r.message = "Self-update script failed";
    }
#else
    r.message = "Auto-update runs on Linux deployment host";
#endif
    return r;
}

} // namespace ghostroute
