#pragma once
#include <string>
#include <vector>

namespace bridge::core {

enum class PathPolicyKind {
  Deny,
  SkipByDefault,
  Normal,
};

struct WorkspaceConfig {
  std::string root;
  std::string profile_name = "default";
  std::string policy_name = "default";
  std::vector<std::string> deny_paths;
  std::vector<std::string> skip_by_default_paths;
};

WorkspaceConfig make_default_workspace_config(const std::string& root,
                                              const std::string& profile_name = "default",
                                              const std::string& policy_name = "default");

} // namespace bridge::core
