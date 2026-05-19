#include "ghostroute/tenant_context.hpp"

#include <algorithm>

namespace ghostroute {

TenantResolver::TenantResolver(bool multitenancy, const std::string& default_slug)
    : multitenancy_(multitenancy), default_slug_(default_slug) {}

std::optional<TenantContext> TenantResolver::resolve(
    const std::string& host, const std::optional<std::string>& header_tenant,
    int64_t session_tenant_id) const {

    TenantContext ctx;
    ctx.role = "admin";

    if (header_tenant && multitenancy_) {
        ctx.slug = *header_tenant;
        ctx.tenant_id = session_tenant_id;
        return ctx;
    }

    if (multitenancy_) {
        // tenant-slug.example.com → slug
        auto dot = host.find('.');
        if (dot != std::string::npos && dot > 0) {
            auto sub = host.substr(0, dot);
            if (sub != "www" && sub != "panel") {
                ctx.slug = sub;
                ctx.tenant_id = session_tenant_id;
                return ctx;
            }
        }
    }

    ctx.slug = default_slug_;
    ctx.tenant_id = session_tenant_id;
    return ctx;
}

} // namespace ghostroute
