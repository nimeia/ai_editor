#include "bridge/core/patch_service.hpp"
#include "bridge/core/path_policy.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace bridge::core {
namespace {

std::size_t env_delay_ms(const char* name) {
  if (const char* raw = std::getenv(name)) {
    try { return static_cast<std::size_t>(std::stoull(raw)); } catch (...) {}
  }
  return 0;
}

void maybe_sleep_patch_stream_delay() {
  const auto delay = env_delay_ms("AI_BRIDGE_PATCH_STREAM_DELAY_MS");
  if (delay > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}

bool check_stream_timeout(const std::chrono::steady_clock::time_point& started, std::size_t timeout_ms) {
  if (timeout_ms == 0) return false;
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started).count();
  return static_cast<std::size_t>(elapsed) > timeout_ms;
}

struct FileState {
  bool ok = false;
  bool binary = false;
  bool bom = false;
  std::string encoding = "utf-8";
  std::string eol = "lf";
  std::string mtime;
  std::string hash;
  std::string text;
  std::string error;
};

struct PreviewRecord {
  std::string preview_id;
  std::string path;
  std::string current_mtime;
  std::string current_hash;
  std::string new_content_hash;
  std::string encoding;
  bool bom = false;
  std::string eol = "lf";
  std::uint64_t created_at_ms = 0;
};

struct PreviewStatusRecord {
  std::string state;
  std::uint64_t recorded_at_ms = 0;
};

std::string iso_time(std::chrono::system_clock::time_point time) {
  using namespace std::chrono;
  const auto tt = system_clock::to_time_t(time);
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

std::string iso_time(fs::file_time_type time) {
  using namespace std::chrono;
  const auto sctp = time_point_cast<system_clock::duration>(time - fs::file_time_type::clock::now() + system_clock::now());
  return iso_time(sctp);
}

std::uint64_t current_timestamp_ms() {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string format_timestamp_ms(std::uint64_t value) {
  if (value == 0) return {};
  return iso_time(std::chrono::system_clock::time_point(std::chrono::milliseconds(value)));
}

std::string current_timestamp() {
  return format_timestamp_ms(current_timestamp_ms());
}

std::uint64_t fnv1a64(std::string_view value) {
  std::uint64_t hash = 1469598103934665603ull;
  for (unsigned char c : value) {
    hash ^= static_cast<std::uint64_t>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

std::string hex64(std::uint64_t value) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(16) << value;
  return oss.str();
}

std::string content_hash(const std::string& text) {
  return hex64(fnv1a64(text));
}

bool has_nul_after_bom(const std::string& raw, std::size_t offset) {
  return raw.find('\0', offset) != std::string::npos;
}

FileState read_text_file(const fs::path& path) {
  FileState out;
  if (!fs::exists(path)) {
    out.error = "path not found";
    return out;
  }
  if (!fs::is_regular_file(path)) {
    out.error = "path is not a regular file";
    return out;
  }
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    out.error = "failed to open file";
    return out;
  }
  std::string raw((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  std::size_t offset = 0;
  if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF &&
      static_cast<unsigned char>(raw[1]) == 0xBB && static_cast<unsigned char>(raw[2]) == 0xBF) {
    out.encoding = "utf-8";
    out.bom = true;
    offset = 3;
  } else if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF &&
             static_cast<unsigned char>(raw[1]) == 0xFE) {
    out.encoding = "utf-16le";
    out.bom = true;
    offset = 2;
  }
  if (has_nul_after_bom(raw, offset)) {
    out.binary = true;
    out.error = "binary file";
    return out;
  }
  out.eol = raw.find("\r\n", offset) != std::string::npos ? "crlf" : "lf";
  out.text = raw.substr(offset);
  out.hash = content_hash(out.text);
  out.mtime = iso_time(fs::last_write_time(path));
  out.ok = true;
  return out;
}

std::string encode_text(const std::string& text, const std::string& eol, bool bom, const std::string& encoding) {
  std::string normalized;
  normalized.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\r') continue;
    normalized.push_back(text[i]);
  }
  std::string converted;
  if (eol == "crlf") {
    converted.reserve(normalized.size() * 2);
    for (char c : normalized) {
      if (c == '\n') converted += "\r\n";
      else converted.push_back(c);
    }
  } else {
    converted = std::move(normalized);
  }
  std::string out;
  if (bom && encoding == "utf-8") out = "\xEF\xBB\xBF";
  else if (bom && encoding == "utf-16le") out = "\xFF\xFE";
  out += converted;
  return out;
}

fs::path bridge_dir(const WorkspaceConfig& workspace) {
  return fs::path(normalize_root_path(workspace.root)) / ".bridge";
}

fs::path backups_dir(const WorkspaceConfig& workspace) {
  return bridge_dir(workspace) / "backups";
}

fs::path previews_dir(const WorkspaceConfig& workspace) {
  return bridge_dir(workspace) / "previews";
}

fs::path history_file(const WorkspaceConfig& workspace) {
  return bridge_dir(workspace) / "history.log";
}

std::size_t env_to_size(const char* name, std::size_t fallback) {
  if (const char* raw = std::getenv(name)) {
    try { return static_cast<std::size_t>(std::stoull(raw)); } catch (...) {}
  }
  return fallback;
}

std::size_t history_rotate_bytes() { return env_to_size("AI_BRIDGE_HISTORY_ROTATE_BYTES", 256 * 1024); }
std::size_t history_rotate_keep() { return env_to_size("AI_BRIDGE_HISTORY_ROTATE_KEEP", 3); }
std::size_t backup_keep_count() { return env_to_size("AI_BRIDGE_BACKUP_KEEP", 20); }
std::size_t preview_keep_count() { return env_to_size("AI_BRIDGE_PREVIEW_KEEP", 20); }
std::size_t preview_status_keep_count() { return env_to_size("AI_BRIDGE_PREVIEW_STATUS_KEEP", preview_keep_count()); }
std::size_t preview_ttl_ms() { return env_to_size("AI_BRIDGE_PREVIEW_TTL_MS", 0); }

void rotate_file_if_needed(const fs::path& path, std::size_t max_bytes, std::size_t keep) {
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

std::vector<fs::path> history_files(const WorkspaceConfig& workspace) {
  std::vector<fs::path> files;
  const auto base = history_file(workspace);
  std::error_code ec;
  if (fs::exists(base, ec) && !ec) files.push_back(base);
  for (std::size_t i = 1; i <= history_rotate_keep(); ++i) {
    fs::path rotated = base.string() + "." + std::to_string(i);
    ec.clear();
    if (fs::exists(rotated, ec) && !ec) files.push_back(rotated);
  }
  return files;
}

void cleanup_old_backups(const WorkspaceConfig& workspace) {
  const auto keep = backup_keep_count();
  if (keep == 0) return;
  std::error_code ec;
  std::vector<std::pair<fs::path, fs::file_time_type>> items;
  if (!fs::exists(backups_dir(workspace), ec) || ec) return;
  for (const auto& entry : fs::directory_iterator(backups_dir(workspace), ec)) {
    if (ec) break;
    if (!entry.is_regular_file()) continue;
    items.push_back({entry.path(), entry.last_write_time(ec)});
    ec.clear();
  }
  if (items.size() <= keep) return;
  std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
  for (std::size_t i = keep; i < items.size(); ++i) {
    fs::remove(items[i].first, ec);
    ec.clear();
  }
}

bool ensure_bridge_dirs(const WorkspaceConfig& workspace, std::string* error) {
  std::error_code ec;
  fs::create_directories(backups_dir(workspace), ec);
  if (ec) {
    if (error) *error = ec.message();
    return false;
  }
  fs::create_directories(previews_dir(workspace), ec);
  if (ec) {
    if (error) *error = ec.message();
    return false;
  }
  return true;
}

std::string make_backup_id(const std::string& rel_path) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  return std::to_string(micros) + "-" + hex64(fnv1a64(rel_path));
}

std::string make_preview_id(const std::string& rel_path, const std::string& current_hash, const std::string& new_hash) {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
  return std::to_string(micros) + "-" + hex64(fnv1a64(rel_path + "|" + current_hash + "|" + new_hash));
}

std::string sanitize_rel_path(const std::string& rel) {
  std::string out;
  out.reserve(rel.size());
  for (char c : rel) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_') out.push_back(c);
    else out.push_back('_');
  }
  if (out.empty()) out = "root";
  return out;
}

fs::path backup_file_path(const WorkspaceConfig& workspace, const std::string& backup_id, const std::string& rel_path) {
  return backups_dir(workspace) / (backup_id + "-" + sanitize_rel_path(rel_path) + ".bak");
}

fs::path preview_meta_path(const WorkspaceConfig& workspace, const std::string& preview_id) {
  return previews_dir(workspace) / (preview_id + ".meta");
}

fs::path preview_content_path(const WorkspaceConfig& workspace, const std::string& preview_id) {
  return previews_dir(workspace) / (preview_id + ".content");
}

fs::path preview_status_path(const WorkspaceConfig& workspace, const std::string& preview_id) {
  return previews_dir(workspace) / (preview_id + ".status");
}

void remove_preview_content_and_meta(const WorkspaceConfig& workspace, const std::string& preview_id) {
  std::error_code ec;
  fs::remove(preview_meta_path(workspace, preview_id), ec);
  ec.clear();
  fs::remove(preview_content_path(workspace, preview_id), ec);
}

bool save_preview_status(const WorkspaceConfig& workspace,
                         const std::string& preview_id,
                         const std::string& state,
                         std::string* error = nullptr) {
  std::ofstream status(preview_status_path(workspace, preview_id), std::ios::binary | std::ios::trunc);
  if (!status) {
    if (error) *error = "failed to write preview status";
    return false;
  }
  status << state << '\n' << current_timestamp_ms() << '\n';
  return true;
}

bool load_preview_status(const WorkspaceConfig& workspace,
                         const std::string& preview_id,
                         PreviewStatusRecord* out) {
  std::ifstream status(preview_status_path(workspace, preview_id), std::ios::binary);
  if (!status) return false;
  PreviewStatusRecord rec;
  std::getline(status, rec.state);
  std::string raw_ms;
  std::getline(status, raw_ms);
  try {
    if (!raw_ms.empty()) rec.recorded_at_ms = static_cast<std::uint64_t>(std::stoull(raw_ms));
  } catch (...) {}
  if (out) *out = rec;
  return !rec.state.empty();
}

std::string preview_status_error_message(const std::string& state) {
  if (state == "applied") return "preview already applied";
  if (state == "expired") return "preview expired";
  if (state == "evicted") return "preview evicted";
  if (state == "invalid") return "preview invalid";
  return "preview not found";
}

void cleanup_old_preview_statuses(const WorkspaceConfig& workspace) {
  const auto keep = preview_status_keep_count();
  if (keep == 0) return;
  std::error_code ec;
  if (!fs::exists(previews_dir(workspace), ec) || ec) return;
  std::vector<std::pair<fs::path, fs::file_time_type>> items;
  for (const auto& entry : fs::directory_iterator(previews_dir(workspace), ec)) {
    if (ec) break;
    if (!entry.is_regular_file()) continue;
    if (entry.path().extension() != ".status") continue;
    items.push_back({entry.path(), entry.last_write_time(ec)});
    ec.clear();
  }
  if (items.size() <= keep) return;
  std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
  for (std::size_t i = keep; i < items.size(); ++i) {
    fs::remove(items[i].first, ec);
    ec.clear();
  }
}

bool save_preview_record(const WorkspaceConfig& workspace, const PreviewRecord& rec, const std::string& new_content, std::string* error) {
  std::error_code ec;
  fs::remove(preview_status_path(workspace, rec.preview_id), ec);
  std::ofstream meta(preview_meta_path(workspace, rec.preview_id), std::ios::binary | std::ios::trunc);
  if (!meta) {
    if (error) *error = "failed to write preview metadata";
    return false;
  }
  meta << rec.preview_id << '\n'
       << rec.path << '\n'
       << rec.current_mtime << '\n'
       << rec.current_hash << '\n'
       << rec.new_content_hash << '\n'
       << rec.encoding << '\n'
       << (rec.bom ? "1" : "0") << '\n'
       << rec.eol << '\n'
       << rec.created_at_ms << '\n';
  std::ofstream content(preview_content_path(workspace, rec.preview_id), std::ios::binary | std::ios::trunc);
  if (!content) {
    remove_preview_content_and_meta(workspace, rec.preview_id);
    if (error) *error = "failed to write preview content";
    return false;
  }
  content << new_content;
  return true;
}

bool load_preview_record(const WorkspaceConfig& workspace, const std::string& preview_id, PreviewRecord* out, std::string* new_content, std::string* error) {
  std::ifstream meta(preview_meta_path(workspace, preview_id), std::ios::binary);
  if (!meta) {
    PreviewStatusRecord status;
    if (load_preview_status(workspace, preview_id, &status)) {
      if (error) *error = preview_status_error_message(status.state);
      return false;
    }
    if (error) *error = "preview not found";
    return false;
  }
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(meta, line)) lines.push_back(line);
  if (lines.size() < 8) {
    save_preview_status(workspace, preview_id, "invalid");
    remove_preview_content_and_meta(workspace, preview_id);
    if (error) *error = "preview metadata corrupted";
    return false;
  }
  PreviewRecord rec;
  rec.preview_id = lines[0];
  rec.path = lines[1];
  rec.current_mtime = lines[2];
  rec.current_hash = lines[3];
  rec.new_content_hash = lines[4];
  rec.encoding = lines[5];
  rec.bom = lines[6] == "1";
  rec.eol = lines[7];
  if (lines.size() >= 9) {
    try {
      rec.created_at_ms = static_cast<std::uint64_t>(std::stoull(lines[8]));
    } catch (...) {
      rec.created_at_ms = 0;
    }
  }
  const auto ttl = preview_ttl_ms();
  if (ttl > 0 && rec.created_at_ms > 0 && current_timestamp_ms() > rec.created_at_ms + ttl) {
    save_preview_status(workspace, preview_id, "expired");
    remove_preview_content_and_meta(workspace, preview_id);
    cleanup_old_preview_statuses(workspace);
    if (error) *error = "preview expired";
    return false;
  }
  std::ifstream content(preview_content_path(workspace, preview_id), std::ios::binary);
  if (!content) {
    save_preview_status(workspace, preview_id, "invalid");
    remove_preview_content_and_meta(workspace, preview_id);
    if (error) *error = "preview content missing";
    return false;
  }
  std::string text((std::istreambuf_iterator<char>(content)), std::istreambuf_iterator<char>());
  if (content_hash(text) != rec.new_content_hash) {
    save_preview_status(workspace, preview_id, "invalid");
    remove_preview_content_and_meta(workspace, preview_id);
    if (error) *error = "preview content hash mismatch";
    return false;
  }
  if (out) *out = rec;
  if (new_content) *new_content = std::move(text);
  return true;
}

void cleanup_old_previews(const WorkspaceConfig& workspace) {
  const auto keep = preview_keep_count();
  if (keep == 0) return;
  std::error_code ec;
  if (!fs::exists(previews_dir(workspace), ec) || ec) return;
  std::vector<std::pair<fs::path, fs::file_time_type>> metas;
  for (const auto& entry : fs::directory_iterator(previews_dir(workspace), ec)) {
    if (ec) break;
    if (!entry.is_regular_file()) continue;
    if (entry.path().extension() != ".meta") continue;
    metas.push_back({entry.path(), entry.last_write_time(ec)});
    ec.clear();
  }
  if (metas.size() <= keep) return;
  std::sort(metas.begin(), metas.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
  for (std::size_t i = keep; i < metas.size(); ++i) {
    const auto preview_id = metas[i].first.stem().string();
    save_preview_status(workspace, preview_id, "evicted");
    remove_preview_content_and_meta(workspace, preview_id);
  }
  cleanup_old_preview_statuses(workspace);
}

void append_history(const WorkspaceConfig& workspace,
                    const std::string& method,
                    const std::string& rel_path,
                    const std::string& backup_id,
                    const std::string& client_id,
                    const std::string& session_id,
                    const std::string& request_id) {
  const auto file = history_file(workspace);
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  rotate_file_if_needed(file, history_rotate_bytes(), history_rotate_keep());
  std::ofstream ofs(file, std::ios::app);
  ofs << current_timestamp() << "\t" << method << "\t" << rel_path << "\t" << backup_id << "\t"
      << client_id << "\t" << session_id << "\t" << request_id << "\n";
}

std::vector<std::string> split_lines(const std::string& text) {
  std::vector<std::string> lines;
  std::string cur;
  for (char c : text) {
    if (c == '\r') continue;
    if (c == '\n') {
      lines.push_back(cur);
      cur.clear();
    } else cur.push_back(c);
  }
  if (!cur.empty() || (!text.empty() && text.back() != '\n')) lines.push_back(cur);
  return lines;
}

std::string make_simple_diff(const std::string& rel_path, const std::string& old_text, const std::string& new_text) {
  std::ostringstream oss;
  oss << "--- a/" << rel_path << "\n";
  oss << "+++ b/" << rel_path << "\n";
  if (old_text == new_text) {
    oss << "@@ no changes @@\n";
    return oss.str();
  }
  oss << "@@\n";
  for (const auto& line : split_lines(old_text)) oss << "-" << line << "\n";
  for (const auto& line : split_lines(new_text)) oss << "+" << line << "\n";
  return oss.str();
}

bool base_matches(const PatchBase& base, const FileState& state) {
  if (!base.mtime.empty() && base.mtime != state.mtime) return false;
  if (!base.hash.empty() && base.hash != state.hash) return false;
  return true;
}

std::string preview_expires_at(const PreviewRecord& rec) {
  const auto ttl = preview_ttl_ms();
  if (ttl == 0 || rec.created_at_ms == 0) return {};
  return format_timestamp_ms(rec.created_at_ms + ttl);
}

std::string patch_conflict_reason(const PatchBase& expected, const FileState& actual) {
  const bool mtime_conflict = !expected.mtime.empty() && expected.mtime != actual.mtime;
  const bool hash_conflict = !expected.hash.empty() && expected.hash != actual.hash;
  if (mtime_conflict && hash_conflict) return "mtime_and_hash_changed";
  if (mtime_conflict) return "mtime_changed";
  if (hash_conflict) return "hash_changed";
  return "file_changed";
}

std::string patch_conflict_message(const PatchBase& expected, const FileState& actual) {
  const auto reason = patch_conflict_reason(expected, actual);
  std::ostringstream oss;
  oss << "patch conflict: " << reason;
  if (!expected.mtime.empty()) {
    oss << " (expected mtime=" << expected.mtime << ", actual mtime=" << actual.mtime << ")";
  }
  if (!expected.hash.empty()) {
    oss << " (expected hash=" << expected.hash << ", actual hash=" << actual.hash << ")";
  }
  return oss.str();
}

HistoryItem parse_history_line(const std::string& line) {
  HistoryItem item;
  std::vector<std::string> fields;
  std::size_t start = 0;
  while (true) {
    const auto pos = line.find('\t', start);
    if (pos == std::string::npos) {
      fields.push_back(line.substr(start));
      break;
    }
    fields.push_back(line.substr(start, pos - start));
    start = pos + 1;
  }
  if (fields.size() >= 7) {
    item.timestamp = fields[0];
    item.method = fields[1];
    item.path = fields[2];
    item.backup_id = fields[3];
    item.client_id = fields[4];
    item.session_id = fields[5];
    item.request_id = fields[6];
  }
  return item;
}

} // namespace

PatchPreviewResult patch_preview(const WorkspaceConfig& workspace,
                                 const std::string& requested_path,
                                 const std::string& new_content,
                                 const PatchBase& base) {
  PatchPreviewResult out;
  const auto resolved = resolve_under_workspace(workspace, requested_path);
  if (!resolved.ok) {
    out.error = resolved.error;
    return out;
  }
  if (resolved.policy == PathPolicyKind::Deny) {
    out.error = "path denied by policy";
    return out;
  }
  const auto state = read_text_file(resolved.absolute_path);
  if (!state.ok) {
    out.error = state.error;
    return out;
  }
  std::string mkdir_error;
  if (!ensure_bridge_dirs(workspace, &mkdir_error)) {
    out.error = mkdir_error;
    return out;
  }
  out.ok = true;
  out.path = resolved.normalized_relative_path;
  out.applicable = base_matches(base, state);
  out.encoding = state.encoding;
  out.bom = state.bom;
  out.eol = state.eol;
  out.current_mtime = state.mtime;
  out.current_hash = state.hash;
  out.new_content_hash = content_hash(new_content);
  out.preview_id = make_preview_id(out.path, out.current_hash, out.new_content_hash);
  out.diff = make_simple_diff(out.path, state.text, new_content);

  PreviewRecord rec;
  rec.preview_id = out.preview_id;
  rec.path = out.path;
  rec.current_mtime = out.current_mtime;
  rec.current_hash = out.current_hash;
  rec.new_content_hash = out.new_content_hash;
  rec.encoding = out.encoding;
  rec.bom = out.bom;
  rec.eol = out.eol;
  rec.created_at_ms = current_timestamp_ms();
  out.preview_created_at = format_timestamp_ms(rec.created_at_ms);
  out.preview_expires_at = preview_expires_at(rec);
  if (!save_preview_record(workspace, rec, new_content, &out.error)) {
    out.ok = false;
    out.preview_id.clear();
    out.diff.clear();
    return out;
  }
  cleanup_old_previews(workspace);
  return out;
}

PatchPreviewStreamResult patch_preview_stream(const WorkspaceConfig& workspace,
                                              const std::string& requested_path,
                                              const std::string& new_content,
                                              const PatchBase& base,
                                              const PatchPreviewStreamOptions& options) {
  const auto started = std::chrono::steady_clock::now();
  PatchPreviewStreamResult out;
  const auto preview = patch_preview(workspace, requested_path, new_content, base);
  if (!preview.ok) {
    out.error = preview.error;
    return out;
  }
  out.ok = true;
  out.path = preview.path;
  out.preview_id = preview.preview_id;
  out.applicable = preview.applicable;
  out.encoding = preview.encoding;
  out.bom = preview.bom;
  out.eol = preview.eol;
  out.current_mtime = preview.current_mtime;
  out.current_hash = preview.current_hash;
  out.new_content_hash = preview.new_content_hash;
  out.preview_created_at = preview.preview_created_at;
  out.preview_expires_at = preview.preview_expires_at;
  out.total_bytes = preview.diff.size();

  const std::size_t chunk_bytes = options.chunk_bytes == 0 ? (16 * 1024) : options.chunk_bytes;
  for (std::size_t pos = 0; pos < preview.diff.size(); pos += chunk_bytes) {
    if (options.cancel_requested && options.cancel_requested()) {
      out.ok = false;
      out.cancelled = true;
      out.error = "request cancelled";
      return out;
    }
    if (check_stream_timeout(started, options.timeout_ms)) {
      out.ok = false;
      out.timed_out = true;
      out.error = "request timeout";
      return out;
    }
    const auto part = preview.diff.substr(pos, std::min(chunk_bytes, preview.diff.size() - pos));
    maybe_sleep_patch_stream_delay();
    if (check_stream_timeout(started, options.timeout_ms)) {
      out.ok = false;
      out.timed_out = true;
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

PatchApplyResult patch_apply(const WorkspaceConfig& workspace,
                             const std::string& requested_path,
                             const std::string& new_content,
                             const PatchBase& base,
                             const std::string& client_id,
                             const std::string& session_id,
                             const std::string& request_id,
                             const std::string& preview_id) {
  PatchApplyResult out;
  std::string effective_path = requested_path;
  std::string effective_new_content = new_content;
  PatchBase effective_base = base;
  PreviewRecord preview;
  bool using_preview = !preview_id.empty();
  if (using_preview) {
    std::string preview_error;
    if (!load_preview_record(workspace, preview_id, &preview, &effective_new_content, &preview_error)) {
      out.preview_status = preview_error.rfind("preview ", 0) == 0 ? preview_error.substr(8) : std::string();
      out.error = preview_error;
      return out;
    }
    if (!effective_path.empty() && effective_path != preview.path) {
      out.error = "preview path mismatch";
      return out;
    }
    effective_path = preview.path;
    if (!new_content.empty() && content_hash(new_content) != preview.new_content_hash) {
      out.error = "preview content mismatch";
      return out;
    }
    if (effective_base.mtime.empty()) effective_base.mtime = preview.current_mtime;
    else if (effective_base.mtime != preview.current_mtime) {
      out.error = "preview base mismatch";
      return out;
    }
    if (effective_base.hash.empty()) effective_base.hash = preview.current_hash;
    else if (effective_base.hash != preview.current_hash) {
      out.error = "preview base mismatch";
      return out;
    }
  }

  const auto resolved = resolve_under_workspace(workspace, effective_path);
  if (!resolved.ok) {
    out.error = resolved.error;
    return out;
  }
  if (resolved.policy == PathPolicyKind::Deny) {
    out.error = "path denied by policy";
    return out;
  }
  const auto state = read_text_file(resolved.absolute_path);
  if (!state.ok) {
    out.error = state.error;
    return out;
  }
  if (!base_matches(effective_base, state)) {
    out.expected_mtime = effective_base.mtime;
    out.expected_hash = effective_base.hash;
    out.actual_mtime = state.mtime;
    out.actual_hash = state.hash;
    out.conflict_reason = patch_conflict_reason(effective_base, state);
    out.error = patch_conflict_message(effective_base, state);
    return out;
  }
  if (using_preview) {
    if (state.mtime != preview.current_mtime || state.hash != preview.current_hash) {
      PatchBase preview_base{preview.current_mtime, preview.current_hash};
      out.expected_mtime = preview.current_mtime;
      out.expected_hash = preview.current_hash;
      out.actual_mtime = state.mtime;
      out.actual_hash = state.hash;
      out.conflict_reason = patch_conflict_reason(preview_base, state);
      out.error = patch_conflict_message(preview_base, state);
      return out;
    }
  }

  std::string mkdir_error;
  if (!ensure_bridge_dirs(workspace, &mkdir_error)) {
    out.error = mkdir_error;
    return out;
  }
  const auto backup_id = make_backup_id(resolved.normalized_relative_path);
  const auto backup_path = backup_file_path(workspace, backup_id, resolved.normalized_relative_path);
  {
    std::ofstream backup(backup_path, std::ios::binary);
    if (!backup) {
      out.error = "failed to create backup";
      return out;
    }
    backup << encode_text(state.text, state.eol, state.bom, state.encoding);
  }
  const fs::path target(resolved.absolute_path);
  const fs::path temp = target.string() + ".bridge.tmp";
  {
    std::ofstream ofs(temp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      out.error = "failed to open temp file";
      return out;
    }
    ofs << encode_text(effective_new_content, state.eol, state.bom, state.encoding);
  }
  std::error_code ec;
  fs::rename(temp, target, ec);
  if (ec) {
    fs::remove(temp);
    out.error = ec.message();
    return out;
  }
  append_history(workspace, using_preview ? "patch.apply.preview" : "patch.apply", resolved.normalized_relative_path, backup_id, client_id, session_id, request_id);
  cleanup_old_backups(workspace);
  if (using_preview) {
    save_preview_status(workspace, preview_id, "applied");
    remove_preview_content_and_meta(workspace, preview_id);
    cleanup_old_preview_statuses(workspace);
  }
  const auto new_state = read_text_file(target);
  out.ok = new_state.ok;
  out.path = resolved.normalized_relative_path;
  out.applied = new_state.ok;
  out.preview_id = preview_id;
  out.backup_id = backup_id;
  out.current_mtime = new_state.mtime;
  out.current_hash = new_state.hash;
  out.preview_status = using_preview ? "applied" : "";
  if (!new_state.ok) out.error = new_state.error;
  return out;
}

PatchRollbackResult patch_rollback(const WorkspaceConfig& workspace,
                                   const std::string& requested_path,
                                   const std::string& backup_id,
                                   const std::string& client_id,
                                   const std::string& session_id,
                                   const std::string& request_id) {
  PatchRollbackResult out;
  const auto resolved = resolve_under_workspace(workspace, requested_path);
  if (!resolved.ok) {
    out.error = resolved.error;
    return out;
  }
  if (resolved.policy == PathPolicyKind::Deny) {
    out.error = "path denied by policy";
    return out;
  }
  const auto backup_path = backup_file_path(workspace, backup_id, resolved.normalized_relative_path);
  if (!fs::exists(backup_path)) {
    out.error = "backup not found";
    return out;
  }
  std::ifstream ifs(backup_path, std::ios::binary);
  if (!ifs) {
    out.error = "failed to open backup";
    return out;
  }
  const std::string backup_bytes((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  const fs::path target(resolved.absolute_path);
  const fs::path temp = target.string() + ".bridge.rollback.tmp";
  {
    std::ofstream ofs(temp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      out.error = "failed to open temp file";
      return out;
    }
    ofs << backup_bytes;
  }
  std::error_code ec;
  fs::rename(temp, target, ec);
  if (ec) {
    fs::remove(temp);
    out.error = ec.message();
    return out;
  }
  append_history(workspace, "patch.rollback", resolved.normalized_relative_path, backup_id, client_id, session_id, request_id);
  cleanup_old_backups(workspace);
  const auto restored_state = read_text_file(target);
  out.ok = restored_state.ok;
  out.path = resolved.normalized_relative_path;
  out.rolled_back = restored_state.ok;
  out.backup_id = backup_id;
  out.current_mtime = restored_state.mtime;
  out.current_hash = restored_state.hash;
  if (!restored_state.ok) out.error = restored_state.error;
  return out;
}

HistoryListResult history_list(const WorkspaceConfig& workspace,
                               const std::string& requested_path,
                               std::size_t limit) {
  HistoryListResult out;
  std::string normalized;
  if (!requested_path.empty()) {
    const auto resolved = resolve_under_workspace(workspace, requested_path);
    if (!resolved.ok) {
      out.error = resolved.error;
      return out;
    }
    normalized = resolved.normalized_relative_path;
  }
  auto files = history_files(workspace);
  std::vector<HistoryItem> all;
  for (const auto& file : files) {
    std::ifstream ifs(file);
    std::string line;
    while (std::getline(ifs, line)) {
      auto item = parse_history_line(line);
      if (item.timestamp.empty()) continue;
      if (!normalized.empty() && item.path != normalized) continue;
      all.push_back(std::move(item));
    }
  }
  std::sort(all.begin(), all.end(), [](const HistoryItem& a, const HistoryItem& b) { return a.timestamp > b.timestamp; });
  if (all.size() > limit) all.resize(limit);
  out.ok = true;
  out.items = std::move(all);
  return out;
}

} // namespace bridge::core
