#pragma once
#include <string>
#include "bridge/core/workspace.hpp"

namespace bridge::core {

struct ResolveResult {
  bool ok = false;
  std::string normalized_root;
  std::string normalized_relative_path;
  std::string absolute_path;
  PathPolicyKind policy = PathPolicyKind::Normal;
  std::string error;
};

std::string normalize_root_path(const std::string& root);
ResolveResult resolve_under_workspace(const WorkspaceConfig& config, const std::string& requested_path);
const char* to_string(PathPolicyKind kind);

} // namespace bridge::core
