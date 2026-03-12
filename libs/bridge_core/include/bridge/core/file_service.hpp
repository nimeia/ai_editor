#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "bridge/core/path_policy.hpp"

namespace bridge::core {

struct FsEntry {
  std::string name;
  std::string path;
  std::string kind;
  PathPolicyKind policy = PathPolicyKind::Normal;
};

struct FsListOptions {
  bool recursive = false;
  bool include_excluded = false;
  std::size_t max_results = 200;
};

struct FsListResult {
  bool ok = false;
  std::vector<FsEntry> entries;
  std::vector<std::string> skipped_directories;
  std::size_t scanned_files = 0;
  std::size_t skipped_files = 0;
  bool truncated = false;
  std::string error;
};

struct FsStatResult {
  bool ok = false;
  std::string path;
  std::string kind;
  std::uintmax_t size = 0;
  std::string mtime;
  std::string encoding;
  bool bom = false;
  std::string eol;
  bool binary = false;
  PathPolicyKind policy = PathPolicyKind::Normal;
  std::string error;
};

struct FsReadResult {
  bool ok = false;
  std::string path;
  std::string encoding;
  bool bom = false;
  std::string eol;
  bool binary = false;
  std::string content;
  std::size_t line_count = 0;
  bool truncated = false;
  PathPolicyKind policy = PathPolicyKind::Normal;
  std::string error;
};


struct FsWriteOptions {
  std::string encoding = "utf-8";
  bool bom = false;
  std::string eol = "lf";
  bool create_parents = true;
  bool overwrite = true;
};

struct FsWriteResult {
  bool ok = false;
  std::string path;
  std::size_t bytes_written = 0;
  bool created = false;
  bool parent_created = false;
  std::string encoding = "utf-8";
  bool bom = false;
  std::string eol = "lf";
  PathPolicyKind policy = PathPolicyKind::Normal;
  std::string error;
};

struct FsMkdirOptions {
  bool create_parents = true;
};

struct FsMkdirResult {
  bool ok = false;
  std::string path;
  bool created = false;
  PathPolicyKind policy = PathPolicyKind::Normal;
  std::string error;
};

struct FsReadStreamOptions {
  std::size_t max_bytes = 1024 * 1024;
  std::size_t chunk_bytes = 16 * 1024;
  std::size_t timeout_ms = 0;
  std::function<bool()> cancel_requested;
  std::function<bool(const std::string&)> on_chunk;
};

struct FsReadStreamResult {
  bool ok = false;
  std::string path;
  std::string encoding;
  bool bom = false;
  std::string eol;
  bool binary = false;
  std::size_t line_count = 0;
  bool truncated = false;
  bool cancelled = false;
  bool timed_out = false;
  std::size_t chunk_count = 0;
  std::size_t total_bytes = 0;
  PathPolicyKind policy = PathPolicyKind::Normal;
  std::string error;
};

FsListResult fs_list(const WorkspaceConfig& workspace, const std::string& requested_path, const FsListOptions& options = {});
FsStatResult fs_stat(const WorkspaceConfig& workspace, const std::string& requested_path);
FsReadResult fs_read(const WorkspaceConfig& workspace, const std::string& requested_path, std::size_t max_bytes = 64 * 1024);
FsReadResult fs_read_range(const WorkspaceConfig& workspace,
                           const std::string& requested_path,
                           std::size_t start_line,
                           std::size_t end_line,
                           std::size_t max_bytes = 64 * 1024);
FsReadStreamResult fs_read_stream(const WorkspaceConfig& workspace,
                                  const std::string& requested_path,
                                  const FsReadStreamOptions& options = {});
FsReadStreamResult fs_read_range_stream(const WorkspaceConfig& workspace,
                                        const std::string& requested_path,
                                        std::size_t start_line,
                                        std::size_t end_line,
                                        const FsReadStreamOptions& options = {});
FsWriteResult fs_write(const WorkspaceConfig& workspace,
                       const std::string& requested_path,
                       const std::string& content,
                       const FsWriteOptions& options = {});
FsMkdirResult fs_mkdir(const WorkspaceConfig& workspace,
                       const std::string& requested_path,
                       const FsMkdirOptions& options = {});

} // namespace bridge::core
