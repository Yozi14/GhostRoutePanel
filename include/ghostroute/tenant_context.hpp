#pragma once

#include <optional>
#include <string>

namespace ghostroute {

struct TenantContext {
    int64_t tenant_id{};
    std::string slug;
    std::string role; // superadmin | admin
};

class TenantResolver {
public:
  explicit TenantResolver(bool multitenancy, const std::string& default_slug);

  // From Host header (tenant1.panel.example.com) or X-Tenant-ID
  std::optional<TenantContext> resolve(const std::string& host,
                                       const std::optional<std::string>& header_tenant,
                                       int64_t session_tenant_id) const;

private:
    bool multitenancy_;
    std::string default_slug_;
};

} // namespace ghostroute
