#include "ghostroute/services.hpp"

#include <fstream>
#include <sstream>
#include <string>

#ifndef _WIN32
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

namespace ghostroute {

namespace {

#ifndef _WIN32
struct CpuTimes {
    unsigned long long user{}, nice{}, system{}, idle{}, iowait{}, irq{}, softirq{};
};

bool read_cpu_times(CpuTimes& t) {
    std::ifstream f("/proc/stat");
    if (!f) return false;
    std::string line;
    if (!std::getline(f, line) || line.rfind("cpu ", 0) != 0) return false;
    std::istringstream iss(line.substr(4));
    iss >> t.user >> t.nice >> t.system >> t.idle >> t.iowait >> t.irq >> t.softirq;
    return true;
}

double cpu_usage_percent() {
    static CpuTimes prev{};
    static bool has_prev = false;
    CpuTimes cur{};
    if (!read_cpu_times(cur)) return 0;

    auto total = [](const CpuTimes& c) {
        return c.user + c.nice + c.system + c.idle + c.iowait + c.irq + c.softirq;
    };
    auto idle = [](const CpuTimes& c) { return c.idle + c.iowait; };

    if (!has_prev) {
        prev = cur;
        has_prev = true;
        return 0;
    }

    const auto dt = total(cur) - total(prev);
    const auto didle = idle(cur) - idle(prev);
    prev = cur;
    if (dt == 0) return 0;
    const auto used = dt - didle;
    return (100.0 * static_cast<double>(used)) / static_cast<double>(dt);
}
#endif

} // namespace

SystemMetrics SystemStats::sample() {
    SystemMetrics m;

#ifndef _WIN32
    m.cpu_percent = cpu_usage_percent();

    std::ifstream mem("/proc/meminfo");
    if (mem) {
        std::string key;
        long long val = 0;
        std::string unit;
        long long mem_total_kb = 0, mem_avail_kb = 0;
        while (mem >> key >> val >> unit) {
            if (key == "MemTotal:") mem_total_kb = val;
            if (key == "MemAvailable:") mem_avail_kb = val;
        }
        if (mem_total_kb > 0) {
            m.ram_total_mb = mem_total_kb / 1024;
            const auto used_kb = mem_total_kb - mem_avail_kb;
            m.ram_used_mb = used_kb / 1024;
            m.ram_percent = (100.0 * used_kb) / mem_total_kb;
        }
    }

    struct statvfs st {};
    if (statvfs("/", &st) == 0 && st.f_blocks > 0) {
        const auto total = static_cast<double>(st.f_blocks) * st.f_frsize;
        const auto free = static_cast<double>(st.f_bfree) * st.f_frsize;
        const auto used = total - free;
        m.disk_percent = total > 0 ? (100.0 * used) / total : 0;
    }

    struct sysinfo info {};
    if (sysinfo(&info) == 0) m.uptime_sec = info.uptime;

    double load[3]{};
    if (getloadavg(load, 3) != -1) m.load_avg_1 = load[0];
#else
    m.cpu_percent = 0;
    m.ram_percent = 0;
#endif

    return m;
}

} // namespace ghostroute
