#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include "bridge/core/path_policy.hpp"
#include "bridge/core/workspace.hpp"

namespace bridge::core {

struct SearchMatch {
  std::string path;
  std::size_t line_start = 0;
  std::size_t line_end = 0;
  std::string snippet;
};

struct SearchOptions {
  std::string root_path = ".";
  bool include_excluded = false;
  std::vector<std::string> extensions;
  std::size_t context_before = 2;
  std::size_t context_after = 2;
  std::size_t max_results = 100;
  std::size_t max_matches_per_file = 20;
  std::size_t max_file_bytes = 256 * 1024;
  std::size_t timeout_ms = 10000;
  std::function<bool()> cancel_requested;
  std::function<bool(const SearchMatch&)> on_match;
};

struct SearchResult {
  bool ok = false;
  std::vector<SearchMatch> matches;
  std::size_t scanned_files = 0;
  std::size_t skipped_files = 0;
  std::vector<std::string> skipped_directories;
  bool truncated = false;
  bool timed_out = false;
  bool cancelled = false;
  std::string error;
};

SearchResult search_text(const WorkspaceConfig& workspace,
                         const std::string& query,
                         const SearchOptions& options = {});

SearchResult search_regex(const WorkspaceConfig& workspace,
                          const std::string& pattern,
                          const SearchOptions& options = {});

} // namespace bridge::core
