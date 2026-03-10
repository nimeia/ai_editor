#include "bridge/core/path_policy.hpp"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace bridge::core {

static bool starts_with_path_segment(const std::string& value, const std::string& prefix) {
  if (value == prefix) return true;
  if (value.size() <= prefix.size()) return false;
  if (value.compare(0, prefix.size(), prefix) != 0) return false;
  return value[prefix.size()] == '/';
}

std::string normalize_root_path(const std::string& root) {
  fs::path p = fs::weakly_canonical(fs::absolute(fs::path(root)));
  return p.generic_string();
}

const char* to_string(PathPolicyKind kind) {
  switch (kind) {
    case PathPolicyKind::Deny: return "deny";
    case PathPolicyKind::SkipByDefault: return "skip_by_default";
    case PathPolicyKind::Normal: return "normal";
  }
  return "normal";
}

ResolveResult resolve_under_workspace(const WorkspaceConfig& config, const std::string& requested_path) {
  ResolveResult out;
  try {
    const auto normalized_root = normalize_root_path(config.root);
    fs::path root_path(normalized_root);
    fs::path rel = requested_path.empty() ? fs::path(".") : fs::path(requested_path);
    fs::path abs = fs::weakly_canonical(root_path / rel);
    const auto abs_str = abs.generic_string();
    if (!(abs_str == normalized_root || starts_with_path_segment(abs_str, normalized_root))) {
      out.error = "path is outside workspace";
      return out;
    }
    auto relative = fs::relative(abs, root_path).generic_string();
    if (relative == ".") relative = "";

    PathPolicyKind policy = PathPolicyKind::Normal;
    for (const auto& deny : config.deny_paths) {
      if (relative == deny || starts_with_path_segment(relative, deny)) {
        policy = PathPolicyKind::Deny;
        break;
      }
    }
    if (policy != PathPolicyKind::Deny) {
      for (const auto& skip : config.skip_by_default_paths) {
        if (relative == skip || starts_with_path_segment(relative, skip)) {
          policy = PathPolicyKind::SkipByDefault;
          break;
        }
      }
    }

    out.ok = true;
    out.normalized_root = normalized_root;
    out.normalized_relative_path = relative;
    out.absolute_path = abs_str;
    out.policy = policy;
    return out;
  } catch (const std::exception& ex) {
    out.error = ex.what();
    return out;
  }
}

} // namespace bridge::core
