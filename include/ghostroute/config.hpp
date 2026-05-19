#pragma once

#include "ghostroute/types.hpp"
#include <string>

namespace ghostroute {

class Config {
public:
    static PanelConfig load(const std::string& path);
    static PanelConfig defaults();
};

} // namespace ghostroute
