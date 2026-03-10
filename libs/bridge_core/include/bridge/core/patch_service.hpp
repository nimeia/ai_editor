#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include "bridge/core/workspace.hpp"

namespace bridge::core {

struct PatchBase {
  std::string mtime;
  std::string hash;
};

struct PatchPreviewResult {
  bool ok = false;
  std::string path;
  std::string preview_id;
  bool applicable = false;
  std::string encoding;
  bool bom = false;
  std::string eol;
  std::string current_mtime;
  std::string current_hash;
  std::string new_content_hash;
  std::string preview_created_at;
  std::string preview_expires_at;
  std::string diff;
  std::string error;
};

struct PatchPreviewStreamOptions {
  std::size_t chunk_bytes = 16 * 1024;
  std::size_t timeout_ms = 0;
  std::function<bool()> cancel_requested;
  std::function<bool(const std::string&)> on_chunk;
};

struct PatchPreviewStreamResult {
  bool ok = false;
  std::string path;
  std::string preview_id;
  bool applicable = false;
  std::string encoding;
  bool bom = false;
  std::string eol;
  std::string current_mtime;
  std::string current_hash;
  std::string new_content_hash;
  std::string preview_created_at;
  std::string preview_expires_at;
  bool cancelled = false;
  bool timed_out = false;
  std::size_t chunk_count = 0;
  std::size_t total_bytes = 0;
  std::string error;
};

struct PatchApplyResult {
  bool ok = false;
  std::string path;
  bool applied = false;
  std::string preview_id;
  std::string backup_id;
  std::string current_mtime;
  std::string current_hash;
  std::string expected_mtime;
  std::string expected_hash;
  std::string actual_mtime;
  std::string actual_hash;
  std::string conflict_reason;
  std::string preview_status;
  std::string error;
};

struct PatchRollbackResult {
  bool ok = false;
  std::string path;
  bool rolled_back = false;
  std::string backup_id;
  std::string current_mtime;
  std::string current_hash;
  std::string error;
};

struct HistoryItem {
  std::string timestamp;
  std::string method;
  std::string path;
  std::string backup_id;
  std::string client_id;
  std::string session_id;
  std::string request_id;
};

struct HistoryListResult {
  bool ok = false;
  std::vector<HistoryItem> items;
  std::string error;
};

PatchPreviewResult patch_preview(const WorkspaceConfig& workspace,
                                 const std::string& requested_path,
                                 const std::string& new_content,
                                 const PatchBase& base = {});

PatchApplyResult patch_apply(const WorkspaceConfig& workspace,
                             const std::string& requested_path,
                             const std::string& new_content,
                             const PatchBase& base,
                             const std::string& client_id,
                             const std::string& session_id,
                             const std::string& request_id,
                             const std::string& preview_id = {});

PatchRollbackResult patch_rollback(const WorkspaceConfig& workspace,
                                   const std::string& requested_path,
                                   const std::string& backup_id,
                                   const std::string& client_id,
                                   const std::string& session_id,
                                   const std::string& request_id);

PatchPreviewStreamResult patch_preview_stream(const WorkspaceConfig& workspace,
                                              const std::string& requested_path,
                                              const std::string& new_content,
                                              const PatchBase& base = {},
                                              const PatchPreviewStreamOptions& options = {});

HistoryListResult history_list(const WorkspaceConfig& workspace,
                               const std::string& requested_path,
                               std::size_t limit = 20);

} // namespace bridge::core
