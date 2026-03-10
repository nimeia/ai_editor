#include "bridge/core/file_service.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <thread>
#include <cstdlib>
#include <sstream>

namespace fs = std::filesystem;

namespace bridge::core {
namespace {

struct TextProbe {
  std::string encoding = "utf-8";
  bool bom = false;
  std::string eol = "lf";
  bool binary = false;
  std::string text;
};

std::string kind_of(const fs::path& path) {
  if (fs::is_directory(path)) return "directory";
  if (fs::is_regular_file(path)) return "file";
  if (fs::is_symlink(path)) return "symlink";
  return "other";
}

std::string iso_time(const fs::file_time_type& ft) {
  try {
    const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ft - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    const std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
  } catch (...) {
    return {};
  }
}

TextProbe probe_bytes(const std::string& raw) {
  TextProbe out;
  std::size_t offset = 0;
  if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF &&
      static_cast<unsigned char>(raw[1]) == 0xBB &&
      static_cast<unsigned char>(raw[2]) == 0xBF) {
    out.encoding = "utf-8";
    out.bom = true;
    offset = 3;
  } else if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF &&
             static_cast<unsigned char>(raw[1]) == 0xFE) {
    out.encoding = "utf-16le";
    out.bom = true;
    offset = 2;
  }

  if (raw.find('\0', offset) != std::string::npos) {
    out.binary = true;
    return out;
  }

  if (raw.find("\r\n", offset) != std::string::npos) {
    out.eol = "crlf";
  } else {
    out.eol = "lf";
  }

  out.text = raw.substr(offset);
  return out;
}

std::size_t count_lines(const std::string& content) {
  if (content.empty()) return 0;
  std::size_t lines = static_cast<std::size_t>(std::count(content.begin(), content.end(), '\n'));
  if (content.back() != '\n') ++lines;
  return lines;
}

std::size_t env_delay_ms(const char* name) {
  if (const char* raw = std::getenv(name)) {
    try { return static_cast<std::size_t>(std::stoull(raw)); } catch (...) {}
  }
  return 0;
}

void maybe_sleep_chunk_delay() {
  const auto delay = env_delay_ms("AI_BRIDGE_READ_STREAM_DELAY_MS");
  if (delay > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}

bool check_stream_timeout(const std::chrono::steady_clock::time_point& started, std::size_t timeout_ms) {
  if (timeout_ms == 0) return false;
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started).count();
  return static_cast<std::size_t>(elapsed) > timeout_ms;
}

bool policy_denies(PathPolicyKind policy) {
  return policy == PathPolicyKind::Deny;
}

bool should_skip_in_batch(PathPolicyKind policy, bool include_excluded) {
  return policy == PathPolicyKind::SkipByDefault && !include_excluded;
}

FsReadResult make_error_read(PathPolicyKind policy, const std::string& error) {
  FsReadResult out;
  out.policy = policy;
  out.error = error;
  return out;
}

FsReadStreamResult make_error_read_stream(PathPolicyKind policy, const std::string& error) {
  FsReadStreamResult out;
  out.policy = policy;
  out.error = error;
  return out;
}

FsStatResult make_error_stat(PathPolicyKind policy, const std::string& error) {
  FsStatResult out;
  out.policy = policy;
  out.error = error;
  return out;
}

struct ResolvedFileContext {
  bool ok = false;
  ResolveResult resolved;
  fs::path path;
  std::uintmax_t file_size = 0;
  std::string error;
};

ResolvedFileContext resolve_regular_file(const WorkspaceConfig& workspace, const std::string& requested_path) {
  ResolvedFileContext ctx;
  ctx.resolved = resolve_under_workspace(workspace, requested_path);
  if (!ctx.resolved.ok) {
    ctx.error = ctx.resolved.error;
    return ctx;
  }
  if (policy_denies(ctx.resolved.policy)) {
    ctx.error = "path denied by policy";
    return ctx;
  }
  ctx.path = fs::path(ctx.resolved.absolute_path);
  if (!fs::exists(ctx.path)) {
    ctx.error = "path not found";
    return ctx;
  }
  if (!fs::is_regular_file(ctx.path)) {
    ctx.error = "path is not a regular file";
    return ctx;
  }
  ctx.file_size = fs::file_size(ctx.path);
  ctx.ok = true;
  return ctx;
}

} // namespace

FsListResult fs_list(const WorkspaceConfig& workspace, const std::string& requested_path, const FsListOptions& options) {
  FsListResult out;
  const auto resolved = resolve_under_workspace(workspace, requested_path);
  if (!resolved.ok) {
    out.error = resolved.error;
    return out;
  }
  if (policy_denies(resolved.policy)) {
    out.error = "path denied by policy";
    return out;
  }
  const fs::path target(resolved.absolute_path);
  if (!fs::exists(target)) {
    out.error = "path not found";
    return out;
  }
  if (!fs::is_directory(target)) {
    out.error = "path is not a directory";
    return out;
  }

  const auto emit_entry = [&](const fs::directory_entry& entry) {
    FsEntry item;
    item.name = entry.path().filename().string();
    const auto rel = fs::relative(entry.path(), fs::path(resolved.normalized_root)).generic_string();
    item.path = rel == "." ? "" : rel;
    item.kind = kind_of(entry.path());
    const auto child = resolve_under_workspace(workspace, item.path);
    if (child.ok) item.policy = child.policy;
    out.entries.push_back(std::move(item));
  };

  if (!options.recursive) {
    for (const auto& entry : fs::directory_iterator(target)) {
      emit_entry(entry);
      if (out.entries.size() >= options.max_results) {
        out.truncated = true;
        break;
      }
    }
    out.ok = true;
    return out;
  }

  for (fs::recursive_directory_iterator it(target), end; it != end; ++it) {
    const auto rel = fs::relative(it->path(), fs::path(resolved.normalized_root)).generic_string();
    const auto child = resolve_under_workspace(workspace, rel == "." ? "" : rel);
    if (!child.ok) continue;
    if (policy_denies(child.policy)) {
      if (it->is_directory()) it.disable_recursion_pending();
      continue;
    }
    if (it->is_directory() && should_skip_in_batch(child.policy, options.include_excluded)) {
      out.skipped_directories.push_back(rel);
      it.disable_recursion_pending();
      continue;
    }
    if (it->is_regular_file()) ++out.scanned_files;
    emit_entry(*it);
    if (out.entries.size() >= options.max_results) {
      out.truncated = true;
      break;
    }
  }

  out.ok = true;
  return out;
}

FsStatResult fs_stat(const WorkspaceConfig& workspace, const std::string& requested_path) {
  const auto resolved = resolve_under_workspace(workspace, requested_path);
  if (!resolved.ok) return make_error_stat(PathPolicyKind::Normal, resolved.error);
  if (policy_denies(resolved.policy)) return make_error_stat(resolved.policy, "path denied by policy");

  const fs::path path(resolved.absolute_path);
  if (!fs::exists(path)) return make_error_stat(resolved.policy, "path not found");

  FsStatResult out;
  out.ok = true;
  out.path = resolved.normalized_relative_path;
  out.kind = kind_of(path);
  out.policy = resolved.policy;
  out.mtime = iso_time(fs::last_write_time(path));
  if (fs::is_regular_file(path)) {
    out.size = fs::file_size(path);
    std::ifstream ifs(path, std::ios::binary);
    std::string sample((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    const auto probe = probe_bytes(sample);
    out.encoding = probe.encoding;
    out.bom = probe.bom;
    out.eol = probe.eol;
    out.binary = probe.binary;
  }
  return out;
}

FsReadResult fs_read(const WorkspaceConfig& workspace, const std::string& requested_path, std::size_t max_bytes) {
  const auto ctx = resolve_regular_file(workspace, requested_path);
  if (!ctx.ok) return make_error_read(ctx.resolved.policy, ctx.error);

  std::ifstream ifs(ctx.path, std::ios::binary);
  if (!ifs) return make_error_read(ctx.resolved.policy, "failed to open file");
  std::string raw;
  raw.resize(max_bytes);
  ifs.read(raw.data(), static_cast<std::streamsize>(max_bytes));
  raw.resize(static_cast<std::size_t>(ifs.gcount()));
  const bool truncated = !ifs.eof();
  const auto probe = probe_bytes(raw);
  if (probe.binary) return make_error_read(ctx.resolved.policy, "binary file");

  FsReadResult out;
  out.ok = true;
  out.path = ctx.resolved.normalized_relative_path;
  out.encoding = probe.encoding;
  out.bom = probe.bom;
  out.eol = probe.eol;
  out.binary = false;
  out.content = probe.text;
  out.line_count = count_lines(probe.text);
  out.truncated = truncated;
  out.policy = ctx.resolved.policy;
  return out;
}

FsReadResult fs_read_range(const WorkspaceConfig& workspace,
                           const std::string& requested_path,
                           std::size_t start_line,
                           std::size_t end_line,
                           std::size_t max_bytes) {
  if (start_line == 0 || end_line == 0 || start_line > end_line) {
    return make_error_read(PathPolicyKind::Normal, "invalid line range");
  }
  auto full = fs_read(workspace, requested_path, max_bytes);
  if (!full.ok) return full;

  std::istringstream iss(full.content);
  std::ostringstream selected;
  std::string line;
  std::size_t line_no = 0;
  while (std::getline(iss, line)) {
    ++line_no;
    if (line_no < start_line) continue;
    if (line_no > end_line) break;
    if (line_no > start_line) selected << '\n';
    selected << line;
  }
  full.content = selected.str();
  full.line_count = count_lines(full.content);
  return full;
}

FsReadStreamResult fs_read_stream(const WorkspaceConfig& workspace,
                                  const std::string& requested_path,
                                  const FsReadStreamOptions& options) {
  const auto started = std::chrono::steady_clock::now();
  const auto ctx = resolve_regular_file(workspace, requested_path);
  if (!ctx.ok) return make_error_read_stream(ctx.resolved.policy, ctx.error);

  const std::size_t max_bytes = options.max_bytes == 0 ? static_cast<std::size_t>(ctx.file_size) : options.max_bytes;
  const std::size_t chunk_bytes = options.chunk_bytes == 0 ? (16 * 1024) : options.chunk_bytes;
  const std::size_t to_read = static_cast<std::size_t>(std::min<std::uintmax_t>(ctx.file_size, max_bytes));

  std::ifstream ifs(ctx.path, std::ios::binary);
  if (!ifs) return make_error_read_stream(ctx.resolved.policy, "failed to open file");

  std::string raw;
  raw.resize(to_read);
  ifs.read(raw.data(), static_cast<std::streamsize>(to_read));
  raw.resize(static_cast<std::size_t>(ifs.gcount()));
  const auto probe = probe_bytes(raw);
  if (probe.binary) return make_error_read_stream(ctx.resolved.policy, "binary file");

  FsReadStreamResult out;
  out.ok = true;
  out.path = ctx.resolved.normalized_relative_path;
  out.encoding = probe.encoding;
  out.bom = probe.bom;
  out.eol = probe.eol;
  out.binary = false;
  out.truncated = ctx.file_size > raw.size();
  out.policy = ctx.resolved.policy;

  out.line_count = count_lines(probe.text);
  out.total_bytes = probe.text.size();

  for (std::size_t pos = 0; pos < probe.text.size(); pos += chunk_bytes) {
    if (options.cancel_requested && options.cancel_requested()) {
      out.ok = false;
      out.cancelled = true;
      out.error = "request cancelled";
      return out;
    }
    if (check_stream_timeout(started, options.timeout_ms)) {
      out.ok = false;
      out.timed_out = true;
      out.truncated = true;
      out.error = "request timeout";
      return out;
    }
    const auto part = probe.text.substr(pos, std::min(chunk_bytes, probe.text.size() - pos));
    maybe_sleep_chunk_delay();
    if (check_stream_timeout(started, options.timeout_ms)) {
      out.ok = false;
      out.timed_out = true;
      out.truncated = true;
      out.error = "request timeout";
      return out;
    }
    if (options.on_chunk && !options.on_chunk(part)) {
      out.ok = false;
      out.cancelled = true;
      out.error = "stream consumer cancelled";
      return out;
    }
    ++out.chunk_count;
  }
  return out;
}

FsReadStreamResult fs_read_range_stream(const WorkspaceConfig& workspace,
                                        const std::string& requested_path,
                                        std::size_t start_line,
                                        std::size_t end_line,
                                        const FsReadStreamOptions& options) {
  const auto started = std::chrono::steady_clock::now();
  if (start_line == 0 || end_line == 0 || start_line > end_line) {
    return make_error_read_stream(PathPolicyKind::Normal, "invalid line range");
  }
  const auto full = fs_read(workspace, requested_path, options.max_bytes == 0 ? 1024 * 1024 : options.max_bytes);
  if (!full.ok) return make_error_read_stream(full.policy, full.error);

  std::istringstream iss(full.content);
  std::ostringstream selected;
  std::string line;
  std::size_t line_no = 0;
  while (std::getline(iss, line)) {
    ++line_no;
    if (line_no < start_line) continue;
    if (line_no > end_line) break;
    if (line_no > start_line) selected << '\n';
    selected << line;
  }
  const std::string text = selected.str();

  FsReadStreamResult out;
  out.ok = true;
  out.path = full.path;
  out.encoding = full.encoding;
  out.bom = full.bom;
  out.eol = full.eol;
  out.binary = false;
  out.truncated = full.truncated;
  out.policy = full.policy;
  out.line_count = count_lines(text);
  out.total_bytes = text.size();

  const std::size_t chunk_bytes = options.chunk_bytes == 0 ? (16 * 1024) : options.chunk_bytes;
  for (std::size_t pos = 0; pos < text.size(); pos += chunk_bytes) {
    if (options.cancel_requested && options.cancel_requested()) {
      out.ok = false;
      out.cancelled = true;
      out.error = "request cancelled";
      return out;
    }
    if (check_stream_timeout(started, options.timeout_ms)) {
      out.ok = false;
      out.timed_out = true;
      out.truncated = true;
      out.error = "request timeout";
      return out;
    }
    const auto part = text.substr(pos, std::min(chunk_bytes, text.size() - pos));
    maybe_sleep_chunk_delay();
    if (check_stream_timeout(started, options.timeout_ms)) {
      out.ok = false;
      out.timed_out = true;
      out.truncated = true;
      out.error = "request timeout";
      return out;
    }
    if (options.on_chunk && !options.on_chunk(part)) {
      out.ok = false;
      out.cancelled = true;
      out.error = "stream consumer cancelled";
      return out;
    }
    ++out.chunk_count;
  }
  return out;
}

} // namespace bridge::core
