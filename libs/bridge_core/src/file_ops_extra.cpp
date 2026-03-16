#include "bridge/core/file_service.hpp"
#include "bridge/core/workspace.hpp"
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace bridge::core {
namespace {

FsMoveResult make_error_move(PathPolicyKind policy, const std::string& error) {
  FsMoveResult out;
  out.policy = policy;
  out.error = error;
  return out;
}

FsCopyResult make_error_copy(PathPolicyKind policy, const std::string& error) {
  FsCopyResult out;
  out.policy = policy;
  out.error = error;
  return out;
}

FsRenameResult make_error_rename(PathPolicyKind policy, const std::string& error) {
  FsRenameResult out;
  out.policy = policy;
  out.error = error;
  return out;
}

bool policy_denies(PathPolicyKind policy) {
  return policy == PathPolicyKind::Deny;
}

bool normalized_relative_is_root(const ResolveResult& resolved) {
  return resolved.normalized_relative_path.empty();
}

bool path_has_prefix(const fs::path& path, const fs::path& prefix) {
  auto p = path.lexically_normal();
  auto q = prefix.lexically_normal();
  auto pit = p.begin();
  auto qit = q.begin();
  for (; qit != q.end(); ++qit, ++pit) {
    if (pit == p.end() || *pit != *qit) return false;
  }
  return true;
}

bool same_or_descendant_path(const fs::path& candidate, const fs::path& base) {
  return path_has_prefix(candidate, base);
}

std::string common_path_validation_error(const ResolveResult& source,
                                         const ResolveResult& target,
                                         const std::string& requested_target_path) {
  if (normalized_relative_is_root(source)) return "cannot move/copy/rename workspace root";
  if (requested_target_path.empty() || normalized_relative_is_root(target)) return "target path required";
  if (source.absolute_path == target.absolute_path) return "source and target paths are the same";
  return {};
}

bool remove_existing_path(const fs::path& path, std::error_code& ec) {
  if (!fs::exists(path, ec)) return true;
  if (ec) return false;
  if (fs::is_directory(path, ec)) fs::remove_all(path, ec);
  else fs::remove(path, ec);
  return !ec;
}

bool ensure_parent_directory(const fs::path& parent,
                             bool create_parents,
                             bool* parent_created,
                             std::string* error) {
  std::error_code ec;
  if (parent.empty()) return true;
  if (!fs::exists(parent, ec)) {
    if (ec) {
      *error = ec.message();
      return false;
    }
    if (!create_parents) {
      *error = "parent directory not found";
      return false;
    }
    const bool created = fs::create_directories(parent, ec);
    if (ec) {
      *error = ec.message();
      return false;
    }
    if (parent_created) *parent_created = created;
    return true;
  }
  if (!fs::is_directory(parent, ec)) {
    *error = "parent path is not a directory";
    return false;
  }
  if (ec) {
    *error = ec.message();
    return false;
  }
  return true;
}

} // namespace

FsMoveResult fs_move(const WorkspaceConfig& workspace,
                     const std::string& requested_path,
                     const std::string& requested_target_path,
                     const FsMoveOptions& options) {
  const auto source = resolve_under_workspace(workspace, requested_path);
  if (!source.ok) return make_error_move(PathPolicyKind::Normal, source.error);
  const auto target = resolve_under_workspace(workspace, requested_target_path);
  if (!target.ok) return make_error_move(source.policy, target.error);
  if (policy_denies(source.policy) || policy_denies(target.policy)) {
    return make_error_move(policy_denies(source.policy) ? source.policy : target.policy, "path denied by policy");
  }
  const auto validation = common_path_validation_error(source, target, requested_target_path);
  if (!validation.empty()) return make_error_move(source.policy, validation);

  const fs::path source_path(source.absolute_path);
  const fs::path target_path(target.absolute_path);
  std::error_code ec;
  if (!fs::exists(source_path, ec)) return make_error_move(source.policy, "path not found");
  if (ec) return make_error_move(source.policy, ec.message());
  const bool source_is_dir = fs::is_directory(source_path, ec);
  if (ec) return make_error_move(source.policy, ec.message());
  if (source_is_dir && same_or_descendant_path(target_path, source_path)) {
    return make_error_move(source.policy, "cannot move/copy directory into its own subtree");
  }

  bool parent_created = false;
  std::string parent_error;
  if (!ensure_parent_directory(target_path.parent_path(), options.create_parents, &parent_created, &parent_error)) {
    return make_error_move(target.policy, parent_error);
  }

  bool overwritten = false;
  if (fs::exists(target_path, ec)) {
    if (ec) return make_error_move(target.policy, ec.message());
    if (!options.overwrite) return make_error_move(target.policy, "path already exists");
    if (!remove_existing_path(target_path, ec)) return make_error_move(target.policy, ec.message());
    overwritten = true;
  }

  fs::rename(source_path, target_path, ec);
  if (ec) return make_error_move(source.policy, ec.message());

  FsMoveResult out;
  out.ok = true;
  out.path = source.normalized_relative_path;
  out.target_path = target.normalized_relative_path;
  out.moved = true;
  out.parent_created = parent_created;
  out.overwritten = overwritten;
  out.policy = source.policy;
  return out;
}

FsCopyResult fs_copy(const WorkspaceConfig& workspace,
                     const std::string& requested_path,
                     const std::string& requested_target_path,
                     const FsCopyOptions& options) {
  const auto source = resolve_under_workspace(workspace, requested_path);
  if (!source.ok) return make_error_copy(PathPolicyKind::Normal, source.error);
  const auto target = resolve_under_workspace(workspace, requested_target_path);
  if (!target.ok) return make_error_copy(source.policy, target.error);
  if (policy_denies(source.policy) || policy_denies(target.policy)) {
    return make_error_copy(policy_denies(source.policy) ? source.policy : target.policy, "path denied by policy");
  }
  const auto validation = common_path_validation_error(source, target, requested_target_path);
  if (!validation.empty()) return make_error_copy(source.policy, validation);

  const fs::path source_path(source.absolute_path);
  const fs::path target_path(target.absolute_path);
  std::error_code ec;
  if (!fs::exists(source_path, ec)) return make_error_copy(source.policy, "path not found");
  if (ec) return make_error_copy(source.policy, ec.message());
  const bool source_is_dir = fs::is_directory(source_path, ec);
  if (ec) return make_error_copy(source.policy, ec.message());
  if (source_is_dir && !options.recursive) return make_error_copy(source.policy, "recursive required for directory copy");
  if (source_is_dir && same_or_descendant_path(target_path, source_path)) {
    return make_error_copy(source.policy, "cannot move/copy directory into its own subtree");
  }

  bool parent_created = false;
  std::string parent_error;
  if (!ensure_parent_directory(target_path.parent_path(), options.create_parents, &parent_created, &parent_error)) {
    return make_error_copy(target.policy, parent_error);
  }

  bool overwritten = false;
  if (fs::exists(target_path, ec)) {
    if (ec) return make_error_copy(target.policy, ec.message());
    if (!options.overwrite) return make_error_copy(target.policy, "path already exists");
    if (!remove_existing_path(target_path, ec)) return make_error_copy(target.policy, ec.message());
    overwritten = true;
  }

  if (source_is_dir) fs::copy(source_path, target_path, fs::copy_options::recursive, ec);
  else fs::copy_file(source_path, target_path, fs::copy_options::none, ec);
  if (ec) return make_error_copy(source.policy, ec.message());

  FsCopyResult out;
  out.ok = true;
  out.path = source.normalized_relative_path;
  out.target_path = target.normalized_relative_path;
  out.copied = true;
  out.parent_created = parent_created;
  out.overwritten = overwritten;
  out.policy = source.policy;
  return out;
}

FsRenameResult fs_rename(const WorkspaceConfig& workspace,
                         const std::string& requested_path,
                         const std::string& requested_target_path,
                         const FsRenameOptions& options) {
  const auto source = resolve_under_workspace(workspace, requested_path);
  if (!source.ok) return make_error_rename(PathPolicyKind::Normal, source.error);
  const auto target = resolve_under_workspace(workspace, requested_target_path);
  if (!target.ok) return make_error_rename(source.policy, target.error);
  if (policy_denies(source.policy) || policy_denies(target.policy)) {
    return make_error_rename(policy_denies(source.policy) ? source.policy : target.policy, "path denied by policy");
  }
  const auto validation = common_path_validation_error(source, target, requested_target_path);
  if (!validation.empty()) return make_error_rename(source.policy, validation);

  const fs::path source_path(source.absolute_path);
  const fs::path target_path(target.absolute_path);
  std::error_code ec;
  if (!fs::exists(source_path, ec)) return make_error_rename(source.policy, "path not found");
  if (ec) return make_error_rename(source.policy, ec.message());
  if (source_path.parent_path().lexically_normal() != target_path.parent_path().lexically_normal()) {
    return make_error_rename(source.policy, "rename target must stay in same directory");
  }

  bool overwritten = false;
  if (fs::exists(target_path, ec)) {
    if (ec) return make_error_rename(target.policy, ec.message());
    if (!options.overwrite) return make_error_rename(target.policy, "path already exists");
    if (!remove_existing_path(target_path, ec)) return make_error_rename(target.policy, ec.message());
    overwritten = true;
  }

  fs::rename(source_path, target_path, ec);
  if (ec) return make_error_rename(source.policy, ec.message());

  FsRenameResult out;
  out.ok = true;
  out.path = source.normalized_relative_path;
  out.target_path = target.normalized_relative_path;
  out.renamed = true;
  out.overwritten = overwritten;
  out.policy = source.policy;
  return out;
}

} // namespace bridge::core
