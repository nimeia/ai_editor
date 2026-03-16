#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "bridge/core/workspace.hpp"

namespace bridge::core {

struct Scope {
  std::string path;
  std::string exact_path;
  std::string directory_prefix;
  std::vector<std::string> extensions;
  std::size_t min_line = 0;
  std::size_t max_line = 0;
};

struct Selector {
  std::string query;
  std::string exact_path;
  std::string directory_prefix;
  std::string extension;
  std::string anchor_before;
  std::string anchor_after;
  std::size_t occurrence = 1;
  bool from_end = false;
};

struct RiskHint {
  std::string level = "low";
  std::vector<std::string> reasons;
};

struct StagedChange {
  std::string change_id;
  std::string session_id;
  std::string operation;
  std::string path;
  std::size_t line_start = 0;
  std::size_t line_end = 0;
  Selector selector;
  std::string expected_content;
  std::string base_mtime;
  std::string base_hash;
  std::string new_content;
  std::string selector_reason;
  std::string anchor;
  RiskHint risk;
  std::string created_at;
};

struct CommitFileRecord {
  std::string path;
  std::string preview_id;
  std::string backup_id;
  std::string current_hash;
};

struct CommitRecord {
  std::string commit_id;
  std::string session_id;
  std::string state;
  std::vector<CommitFileRecord> files;
  std::string created_at;
};

struct SessionBeginResult {
  bool ok = false;
  std::string session_id;
  std::string state;
  std::string created_at;
  std::string error;
};

struct SessionMutationResult {
  bool ok = false;
  std::string session_id;
  std::string state;
  std::string change_id;
  std::string path;
  std::size_t staged_change_count = 0;
  std::string selector_reason;
  std::string anchor;
  RiskHint risk;
  std::string error;
};

struct SessionInspectItem {
  std::string change_id;
  std::string operation;
  std::string path;
  std::size_t line_start = 0;
  std::size_t line_end = 0;
  std::string selector_reason;
  std::string anchor;
  std::string risk_level;
};

struct SessionInspectFile {
  std::string path;
  std::size_t change_count = 0;
  std::size_t first_line = 0;
  std::size_t last_line = 0;
  std::string risk_level;
  std::vector<std::string> risk_reasons;
  std::vector<std::string> anchors;
  std::vector<std::string> selector_reasons;
  std::string summary;
};

struct SessionInspectResult {
  bool ok = false;
  std::string session_id;
  std::string state;
  std::size_t staged_change_count = 0;
  std::size_t staged_file_count = 0;
  std::size_t staged_block_count = 0;
  std::size_t high_risk_change_count = 0;
  std::size_t medium_risk_change_count = 0;
  std::size_t high_risk_file_count = 0;
  std::size_t medium_risk_file_count = 0;
  std::string risk_level;
  std::string summary;
  std::vector<SessionInspectItem> items;
  std::vector<SessionInspectFile> files;
  std::vector<std::string> highlights;
  std::vector<std::string> risk_reasons;
  std::string error;
};

struct SessionPreviewFile {
  std::string path;
  std::string preview_id;
  std::string diff;
  std::string selector_summary;
  std::string risk_level;
  std::size_t change_count = 0;
  std::size_t hunk_count = 0;
  std::size_t added_line_count = 0;
  std::size_t removed_line_count = 0;
  std::size_t first_line = 0;
  std::size_t last_line = 0;
  std::vector<std::string> risk_reasons;
  std::string summary;
  bool applicable = false;
};

struct SessionPreviewResult {
  bool ok = false;
  std::string session_id;
  std::string state;
  std::size_t staged_change_count = 0;
  std::size_t previewed_file_count = 0;
  std::size_t total_hunk_count = 0;
  std::size_t total_added_line_count = 0;
  std::size_t total_removed_line_count = 0;
  std::size_t high_risk_file_count = 0;
  std::size_t medium_risk_file_count = 0;
  std::string risk_level;
  std::string summary;
  std::vector<SessionPreviewFile> files;
  std::vector<std::string> highlights;
  std::vector<std::string> risk_reasons;
  std::string error;
};

struct SessionCommitResult {
  bool ok = false;
  std::string session_id;
  std::string state;
  std::string commit_id;
  std::size_t committed_file_count = 0;
  std::vector<CommitFileRecord> files;
  std::string error;
};

struct SessionAbortResult {
  bool ok = false;
  std::string session_id;
  std::string state;
  bool aborted = false;
  std::string error;
};

struct SessionRecoverResult {
  bool ok = false;
  std::string session_id;
  std::string state;
  bool recoverable = false;
  std::size_t staged_change_count = 0;
  std::size_t conflict_file_count = 0;
  std::size_t rebase_file_count = 0;
  std::string error;
};

struct RecoveryCheckFile {
  std::string path;
  std::string status;
  bool recoverable = false;
  bool rebase_required = false;
  std::string conflict_reason;
  std::string base_hash;
  std::string current_hash;
};

struct RecoveryCheckResult {
  bool ok = false;
  std::string session_id;
  std::string state;
  bool recoverable = false;
  std::size_t staged_change_count = 0;
  std::size_t file_count = 0;
  std::size_t conflict_file_count = 0;
  std::size_t rebase_file_count = 0;
  std::vector<RecoveryCheckFile> files;
  std::string error;
};

struct RecoveryRebaseResult {
  bool ok = false;
  std::string session_id;
  std::string state;
  bool recoverable = false;
  std::size_t rebased_file_count = 0;
  std::vector<RecoveryCheckFile> files;
  std::string error;
};

struct SessionSnapshotResult {
  bool ok = false;
  std::string session_id;
  std::string state;
  std::string snapshot_path;
  std::string error;
};

SessionBeginResult session_begin(const WorkspaceConfig& workspace,
                                 const std::string& session_id);

SessionMutationResult session_add(const WorkspaceConfig& workspace,
                                  const std::string& session_id,
                                  const StagedChange& change);

SessionInspectResult session_inspect(const WorkspaceConfig& workspace,
                                     const std::string& session_id);

SessionPreviewResult session_preview(const WorkspaceConfig& workspace,
                                     const std::string& session_id);

SessionCommitResult session_commit(const WorkspaceConfig& workspace,
                                   const std::string& session_id,
                                   const std::string& client_id,
                                   const std::string& request_id);

SessionAbortResult session_abort(const WorkspaceConfig& workspace,
                                 const std::string& session_id);

SessionMutationResult session_drop_change(const WorkspaceConfig& workspace,
                                          const std::string& session_id,
                                          const std::string& change_id);

SessionMutationResult session_drop_path(const WorkspaceConfig& workspace,
                                        const std::string& session_id,
                                        const std::string& path);

SessionRecoverResult session_recover(const WorkspaceConfig& workspace,
                                     const std::string& session_id);

RecoveryCheckResult recovery_check(const WorkspaceConfig& workspace,
                                   const std::string& session_id);

RecoveryRebaseResult recovery_rebase(const WorkspaceConfig& workspace,
                                     const std::string& session_id);

SessionSnapshotResult session_snapshot(const WorkspaceConfig& workspace,
                                       const std::string& session_id);

SessionMutationResult edit_replace_range(const WorkspaceConfig& workspace,
                                         const std::string& session_id,
                                         const std::string& path,
                                         std::size_t line_start,
                                         std::size_t line_end,
                                         const std::string& new_content);

SessionMutationResult edit_replace_block(const WorkspaceConfig& workspace,
                                         const std::string& session_id,
                                         const std::string& path,
                                         const Selector& selector,
                                         const std::string& new_content);

SessionMutationResult edit_insert_before(const WorkspaceConfig& workspace,
                                         const std::string& session_id,
                                         const std::string& path,
                                         const Selector& selector,
                                         const std::string& new_content);

SessionMutationResult edit_insert_after(const WorkspaceConfig& workspace,
                                        const std::string& session_id,
                                        const std::string& path,
                                        const Selector& selector,
                                        const std::string& new_content);

SessionMutationResult edit_delete_block(const WorkspaceConfig& workspace,
                                        const std::string& session_id,
                                        const std::string& path,
                                        const Selector& selector);

SessionMutationResult edit_replace_content(const WorkspaceConfig& workspace,
                                           const std::string& session_id,
                                           const std::string& path,
                                           const std::string& expected_content,
                                           const std::string& new_content,
                                           const std::string& operation,
                                           const std::string& selector_reason,
                                           const std::string& anchor = {});

std::string session_materialize_file(const WorkspaceConfig& workspace,
                                     const std::string& session_id,
                                     const std::string& path,
                                     std::string* error);

} // namespace bridge::core
