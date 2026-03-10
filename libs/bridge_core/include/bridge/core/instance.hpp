#pragma once
#include <string>

namespace bridge::core {

struct InstanceScope {
  std::string user_id;
  std::string workspace_root;
  std::string profile_name;
  std::string policy_name;
};

std::string make_instance_key(const InstanceScope& scope);

} // namespace bridge::core
