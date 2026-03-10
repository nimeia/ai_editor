#include "bridge/core/workspace.hpp"

namespace bridge::core {

WorkspaceConfig make_default_workspace_config(const std::string& root,
                                              const std::string& profile_name,
                                              const std::string& policy_name) {
  WorkspaceConfig cfg;
  cfg.root = root;
  cfg.profile_name = profile_name;
  cfg.policy_name = policy_name;
  cfg.deny_paths = {".bridge"};
  cfg.skip_by_default_paths = {
      ".git", "node_modules", "dist", "build", "out", "bin", "obj",
      "target", ".vs", ".next", "coverage", "__pycache__", ".venv"};
  return cfg;
}

} // namespace bridge::core
