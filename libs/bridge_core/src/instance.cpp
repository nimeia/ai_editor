#include "bridge/core/instance.hpp"
#include <functional>
#include <sstream>

namespace bridge::core {

std::string make_instance_key(const InstanceScope& scope) {
  const std::string joined = scope.user_id + "|" + scope.workspace_root + "|" + scope.profile_name + "|" + scope.policy_name;
  const auto hash = std::hash<std::string>{}(joined);
  std::ostringstream oss;
  oss << std::hex << hash;
  return oss.str();
}

} // namespace bridge::core
