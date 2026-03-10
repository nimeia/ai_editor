#include "bridge/core/logging.hpp"
#include "bridge/core/path_policy.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace bridge::core {
namespace {

std::string escape_field(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    if (c == '\t') out += ' ';
    else if (c == '\n' || c == '\r') out += ' ';
    else out += c;
  }
  return out;
}

std::size_t env_to_size(const char* name, std::size_t fallback) {
  if (const char* raw = std::getenv(name)) {
    try { return static_cast<std::size_t>(std::stoull(raw)); } catch (...) {}
  }
  return fallback;
}

std::size_t log_rotate_bytes() {
  return env_to_size("AI_BRIDGE_LOG_ROTATE_BYTES", 256 * 1024);
}

std::size_t log_rotate_keep() {
  return env_to_size("AI_BRIDGE_LOG_ROTATE_KEEP", 3);
}

fs::path ensure_bridge_dir(const WorkspaceConfig& workspace) {
  const fs::path dir = fs::path(normalize_root_path(workspace.root)) / ".bridge";
  std::error_code ec;
  fs::create_directories(dir, ec);
  return dir;
}

void rotate_log_path_if_needed(const fs::path& path) {
  const auto max_bytes = log_rotate_bytes();
  const auto keep = log_rotate_keep();
  if (max_bytes == 0 || keep == 0) return;
  std::error_code ec;
  if (!fs::exists(path, ec) || ec) return;
  const auto size = fs::file_size(path, ec);
  if (ec || size < max_bytes) return;

  for (std::size_t i = keep; i >= 1; --i) {
    const auto older = path.string() + "." + std::to_string(i);
    const auto newer = path.string() + "." + std::to_string(i + 1);
    if (i == keep) {
      fs::remove(older, ec);
    } else if (fs::exists(older, ec)) {
      fs::rename(older, newer, ec);
      ec.clear();
    }
    if (i == 1) break;
  }
  fs::rename(path, path.string() + ".1", ec);
}

void append_line(const fs::path& path, const std::string& line) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  rotate_log_path_if_needed(path);
  std::ofstream ofs(path, std::ios::app);
  if (!ofs) return;
  ofs << line << "\n";
}

} // namespace

std::string current_timestamp_utc() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto tt = system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &tt);
#else
  gmtime_r(&tt, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

void rotate_log_if_needed(const std::string& file_path) {
  rotate_log_path_if_needed(fs::path(file_path));
}

void append_runtime_log(const std::string& runtime_dir,
                        const std::string& level,
                        const std::string& message) {
  std::ostringstream line;
  line << current_timestamp_utc() << "\t" << escape_field(level) << "\t" << escape_field(message);
  append_line(fs::path(runtime_dir) / "runtime.log", line.str());
}

void append_audit_log(const WorkspaceConfig& workspace, const AuditRecord& record) {
  const fs::path dir = ensure_bridge_dir(workspace);
  std::ostringstream line;
  line << escape_field(record.timestamp) << "	"
       << escape_field(record.request_id) << "	"
       << escape_field(record.client_id) << "	"
       << escape_field(record.session_id) << "	"
       << escape_field(record.method) << "	"
       << escape_field(record.path) << "	"
       << escape_field(record.endpoint) << "	"
       << record.duration_ms << "	"
       << record.request_bytes << "	"
       << record.response_bytes << "	"
       << (record.ok ? "ok" : "error") << "	"
       << (record.truncated ? "true" : "false") << "	"
       << escape_field(record.error_code) << "	"
       << escape_field(record.error_message);
  append_line(dir / "audit.log", line.str());
}

} // namespace bridge::core
