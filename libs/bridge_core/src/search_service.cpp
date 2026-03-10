#include "bridge/core/search_service.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace bridge::core {
namespace {

struct TextProbe {
  bool binary = false;
  std::string text;
};

TextProbe probe_text(const std::string& raw) {
  TextProbe out;
  std::size_t offset = 0;
  if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF &&
      static_cast<unsigned char>(raw[1]) == 0xBB && static_cast<unsigned char>(raw[2]) == 0xBF) {
    offset = 3;
  } else if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF &&
             static_cast<unsigned char>(raw[1]) == 0xFE) {
    offset = 2;
  }
  if (raw.find('\0', offset) != std::string::npos) {
    out.binary = true;
    return out;
  }
  out.text = raw.substr(offset);
  return out;
}

std::size_t env_delay_ms() {
  if (const char* raw = std::getenv("AI_BRIDGE_SEARCH_DELAY_MS")) {
    try { return static_cast<std::size_t>(std::stoull(raw)); } catch (...) {}
  }
  return 0;
}

std::vector<std::string> split_lines_preserve_empty(const std::string& text) {
  std::vector<std::string> lines;
  std::string current;
  for (char c : text) {
    if (c == '\r') continue;
    if (c == '\n') {
      lines.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty() || (!text.empty() && text.back() != '\n')) {
    lines.push_back(current);
  }
  return lines;
}

bool extension_allowed(const fs::path& path, const std::vector<std::string>& exts) {
  if (exts.empty()) return true;
  const auto ext = path.extension().string();
  return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

std::string make_snippet(const std::vector<std::string>& lines,
                         std::size_t hit_start,
                         std::size_t hit_end,
                         std::size_t before,
                         std::size_t after) {
  if (lines.empty()) return {};
  const std::size_t start = hit_start > before ? hit_start - before : 1;
  const std::size_t end = std::min(lines.size(), hit_end + after);
  std::ostringstream oss;
  for (std::size_t i = start; i <= end; ++i) {
    oss << lines[i - 1];
    if (i != end) oss << '\n';
  }
  return oss.str();
}

template <typename Matcher>
SearchResult run_search(const WorkspaceConfig& workspace,
                        const SearchOptions& options,
                        Matcher matcher) {
  SearchResult out;
  const auto started = std::chrono::steady_clock::now();
  const auto delay_ms = env_delay_ms();

  auto check_interrupt = [&]() -> bool {
    if (options.cancel_requested && options.cancel_requested()) {
      out.cancelled = true;
      out.truncated = true;
      out.error = "request cancelled";
      return true;
    }
    if (options.timeout_ms > 0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - started).count();
      if (static_cast<std::size_t>(elapsed) > options.timeout_ms) {
        out.timed_out = true;
        out.truncated = true;
        out.error = "search timeout";
        return true;
      }
    }
    return false;
  };

  const auto resolved = resolve_under_workspace(workspace, options.root_path.empty() ? "." : options.root_path);
  if (!resolved.ok) {
    out.error = resolved.error;
    return out;
  }
  if (resolved.policy == PathPolicyKind::Deny) {
    out.error = "path denied by policy";
    return out;
  }

  const fs::path root(resolved.absolute_path);
  if (!fs::exists(root)) {
    out.error = "path not found";
    return out;
  }

  auto process_file = [&](const fs::path& path, const std::string& rel) {
    if (check_interrupt()) return false;
    if (!extension_allowed(path, options.extensions)) {
      ++out.skipped_files;
      return true;
    }
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
      ++out.skipped_files;
      return true;
    }
    std::string raw;
    raw.resize(options.max_file_bytes);
    ifs.read(raw.data(), static_cast<std::streamsize>(options.max_file_bytes));
    raw.resize(static_cast<std::size_t>(ifs.gcount()));
    const auto probe = probe_text(raw);
    if (probe.binary) {
      ++out.skipped_files;
      return true;
    }
    ++out.scanned_files;
    const auto lines = split_lines_preserve_empty(probe.text);
    std::size_t matches_in_file = 0;
    for (std::size_t i = 0; i < lines.size(); ++i) {
      if (delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
      if (check_interrupt()) return false;
      if (!matcher(lines[i])) continue;
      ++matches_in_file;
      SearchMatch m;
      m.path = rel;
      m.line_start = i + 1;
      m.line_end = i + 1;
      m.snippet = make_snippet(lines, i + 1, i + 1, options.context_before, options.context_after);
      if (options.on_match && !options.on_match(m)) {
        out.cancelled = true;
        out.truncated = true;
        out.error = "stream consumer cancelled";
        return false;
      }
      out.matches.push_back(m);
      if (matches_in_file >= options.max_matches_per_file) break;
      if (out.matches.size() >= options.max_results) {
        out.truncated = true;
        return false;
      }
    }
    return true;
  };

  if (fs::is_regular_file(root)) {
    const auto rel = resolved.normalized_relative_path;
    process_file(root, rel);
    out.ok = !out.timed_out && !out.cancelled;
    return out;
  }

  for (fs::recursive_directory_iterator it(root), end; it != end; ++it) {
    if (check_interrupt()) break;
    const auto rel = fs::relative(it->path(), fs::path(resolved.normalized_root)).generic_string();
    const auto child = resolve_under_workspace(workspace, rel == "." ? "" : rel);
    if (!child.ok) continue;
    if (child.policy == PathPolicyKind::Deny) {
      if (it->is_directory()) it.disable_recursion_pending();
      continue;
    }
    if (it->is_directory() && child.policy == PathPolicyKind::SkipByDefault && !options.include_excluded) {
      out.skipped_directories.push_back(rel);
      it.disable_recursion_pending();
      continue;
    }
    if (!it->is_regular_file()) continue;
    if (!process_file(it->path(), rel)) break;
  }

  out.ok = !out.timed_out && !out.cancelled;
  return out;
}

} // namespace

SearchResult search_text(const WorkspaceConfig& workspace,
                         const std::string& query,
                         const SearchOptions& options) {
  if (query.empty()) {
    SearchResult out;
    out.error = "query is empty";
    return out;
  }
  return run_search(workspace, options, [&](const std::string& line) {
    return line.find(query) != std::string::npos;
  });
}

SearchResult search_regex(const WorkspaceConfig& workspace,
                          const std::string& pattern,
                          const SearchOptions& options) {
  if (pattern.empty()) {
    SearchResult out;
    out.error = "pattern is empty";
    return out;
  }
  try {
    const std::regex re(pattern);
    return run_search(workspace, options, [&](const std::string& line) {
      return std::regex_search(line, re);
    });
  } catch (const std::exception& e) {
    SearchResult out;
    out.error = e.what();
    return out;
  }
}

} // namespace bridge::core
