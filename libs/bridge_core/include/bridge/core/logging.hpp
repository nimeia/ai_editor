#pragma once
#include <string>
#include "bridge/core/workspace.hpp"

namespace bridge::core {

struct AuditRecord {
  std::string timestamp;
  std::string request_id;
  std::string client_id;
  std::string session_id;
  std::string method;
  std::string path;
  std::string endpoint;
  long long duration_ms = 0;
  std::size_t request_bytes = 0;
  std::size_t response_bytes = 0;
  bool ok = false;
  bool truncated = false;
  std::string error_code;
  std::string error_message;
};

std::string current_timestamp_utc();
void append_runtime_log(const std::string& runtime_dir,
                        const std::string& level,
                        const std::string& message);
void append_audit_log(const WorkspaceConfig& workspace, const AuditRecord& record);

// For tests and tooling.
void rotate_log_if_needed(const std::string& file_path);

} // namespace bridge::core
