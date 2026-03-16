#include "bridge/core/session_service.hpp"
#include "bridge/core/patch_service.hpp"
#include "bridge/core/path_policy.hpp"
#include "bridge/core/protocol.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace bridge::core {
namespace {

struct SessionMeta {
  std::string session_id;
  std::string state;
  std::string created_at;
  std::string updated_at;
};

struct PlannedFile {
  std::string path;
  std::string current_content;
  std::string final_content;
  std::vector<SessionInspectItem> items;
  RiskHint risk;
};

struct TextDocument {
  bool exists = false;
  std::string content;
  std::vector<std::string> lines;
  bool trailing_newline = false;
};

struct CandidateRange {
  std::size_t start = 0;
  std::size_t end = 0;
  std::size_t ordinal = 0;
};

struct DiffStats {
  std::size_t hunk_count = 0;
  std::size_t added_line_count = 0;
  std::size_t removed_line_count = 0;
};

TextDocument load_document(const WorkspaceConfig& workspace, const std::string& path, std::string* error);

struct ChangeEntry {
  fs::path meta_path;
  StagedChange change;
};

struct FileFingerprint {
  bool exists = false;
  std::string mtime;
  std::string hash;
  std::string content;
};

std::size_t find_json_value_start_local(const std::string& text, const std::string& key) {
  const std::string needle = std::string("\"") + key + "\"";
  std::size_t pos = text.find(needle);
  while (pos != std::string::npos) {
    std::size_t i = pos + needle.size();
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    if (i >= text.size() || text[i] != ':') {
      pos = text.find(needle, pos + needle.size());
      continue;
    }
    ++i;
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    return i;
  }
  return std::string::npos;
}

std::size_t json_get_size_t_local(const std::string& text, const std::string& key, std::size_t fallback = 0) {
  const auto start = find_json_value_start_local(text, key);
  if (start == std::string::npos || start >= text.size() || !std::isdigit(static_cast<unsigned char>(text[start]))) return fallback;
  std::size_t end = start;
  while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) ++end;
  try {
    return static_cast<std::size_t>(std::stoull(text.substr(start, end - start)));
  } catch (...) {
    return fallback;
  }
}

bool json_get_bool_local(const std::string& text, const std::string& key, bool fallback = false) {
  const auto start = find_json_value_start_local(text, key);
  if (start == std::string::npos) return fallback;
  if (text.compare(start, 4, "true") == 0) return true;
  if (text.compare(start, 5, "false") == 0) return false;
  return fallback;
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

std::uint64_t current_timestamp_ms() {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string iso_time(std::chrono::system_clock::time_point time) {
  const auto tt = std::chrono::system_clock::to_time_t(time);
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

std::string current_timestamp() {
  return iso_time(std::chrono::system_clock::time_point(std::chrono::milliseconds(current_timestamp_ms())));
}

std::string make_change_id(const std::string& session_id, const std::string& operation, const std::string& path) {
  return std::to_string(current_timestamp_ms()) + "-" + hex64(fnv1a64(session_id + "|" + operation + "|" + path));
}

std::string make_commit_id(const std::string& session_id) {
  return std::to_string(current_timestamp_ms()) + "-" + hex64(fnv1a64(session_id + "|commit"));
}

fs::path bridge_dir(const WorkspaceConfig& workspace) {
  return fs::path(normalize_root_path(workspace.root)) / ".bridge";
}

fs::path sessions_dir(const WorkspaceConfig& workspace) {
  return bridge_dir(workspace) / "sessions";
}

fs::path session_dir(const WorkspaceConfig& workspace, const std::string& session_id) {
  return sessions_dir(workspace) / session_id;
}

fs::path session_meta_path(const WorkspaceConfig& workspace, const std::string& session_id) {
  return session_dir(workspace, session_id) / "session.json";
}

fs::path session_changes_dir(const WorkspaceConfig& workspace, const std::string& session_id) {
  return session_dir(workspace, session_id) / "changes";
}

fs::path session_snapshot_path(const WorkspaceConfig& workspace, const std::string& session_id) {
  return session_dir(workspace, session_id) / "snapshot.json";
}

fs::path session_commit_path(const WorkspaceConfig& workspace, const std::string& session_id) {
  return session_dir(workspace, session_id) / "commit.json";
}

bool ensure_session_dirs(const WorkspaceConfig& workspace, const std::string& session_id, std::string* error) {
  std::error_code ec;
  fs::create_directories(session_changes_dir(workspace, session_id), ec);
  if (ec) {
    if (error) *error = ec.message();
    return false;
  }
  return true;
}

bool write_text(const fs::path& path, const std::string& text, std::string* error) {
  std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    if (error) *error = "failed to open file";
    return false;
  }
  ofs << text;
  if (!ofs) {
    if (error) *error = "failed to write file";
    return false;
  }
  return true;
}

std::string slurp_text(const fs::path& path, std::string* error) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    if (error) *error = "path not found";
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

std::string serialize_meta(const SessionMeta& meta) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"session_id\":\"" << json_escape(meta.session_id) << "\",";
  oss << "\"state\":\"" << json_escape(meta.state) << "\",";
  oss << "\"created_at\":\"" << json_escape(meta.created_at) << "\",";
  oss << "\"updated_at\":\"" << json_escape(meta.updated_at) << "\"";
  oss << "}";
  return oss.str();
}

SessionMeta deserialize_meta(const std::string& text) {
  SessionMeta meta;
  meta.session_id = json_get_string(text, "session_id");
  meta.state = json_get_string(text, "state");
  meta.created_at = json_get_string(text, "created_at");
  meta.updated_at = json_get_string(text, "updated_at");
  return meta;
}

std::string serialize_risk(const RiskHint& risk) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"level\":\"" << json_escape(risk.level) << "\",";
  oss << "\"reasons_csv\":\"";
  for (std::size_t i = 0; i < risk.reasons.size(); ++i) {
    if (i) oss << ",";
    oss << json_escape(risk.reasons[i]);
  }
  oss << "\"}";
  return oss.str();
}

RiskHint deserialize_risk(const std::string& text) {
  RiskHint risk;
  risk.level = json_get_string(text, "level");
  if (risk.level.empty()) risk.level = "low";
  std::stringstream ss(json_get_string(text, "reasons_csv"));
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) risk.reasons.push_back(item);
  }
  return risk;
}

std::string serialize_change(const StagedChange& change) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"change_id\":\"" << json_escape(change.change_id) << "\",";
  oss << "\"session_id\":\"" << json_escape(change.session_id) << "\",";
  oss << "\"operation\":\"" << json_escape(change.operation) << "\",";
  oss << "\"path\":\"" << json_escape(change.path) << "\",";
  oss << "\"line_start\":" << change.line_start << ",";
  oss << "\"line_end\":" << change.line_end << ",";
  oss << "\"expected_content\":\"" << json_escape(change.expected_content) << "\",";
  oss << "\"base_mtime\":\"" << json_escape(change.base_mtime) << "\",";
  oss << "\"base_hash\":\"" << json_escape(change.base_hash) << "\",";
  oss << "\"new_content\":\"" << json_escape(change.new_content) << "\",";
  oss << "\"selector_query\":\"" << json_escape(change.selector.query) << "\",";
  oss << "\"selector_exact_path\":\"" << json_escape(change.selector.exact_path) << "\",";
  oss << "\"selector_directory_prefix\":\"" << json_escape(change.selector.directory_prefix) << "\",";
  oss << "\"selector_extension\":\"" << json_escape(change.selector.extension) << "\",";
  oss << "\"selector_anchor_before\":\"" << json_escape(change.selector.anchor_before) << "\",";
  oss << "\"selector_anchor_after\":\"" << json_escape(change.selector.anchor_after) << "\",";
  oss << "\"selector_occurrence\":" << change.selector.occurrence << ",";
  oss << "\"selector_from_end\":" << (change.selector.from_end ? "true" : "false") << ",";
  oss << "\"selector_reason\":\"" << json_escape(change.selector_reason) << "\",";
  oss << "\"anchor\":\"" << json_escape(change.anchor) << "\",";
  oss << "\"created_at\":\"" << json_escape(change.created_at) << "\",";
  oss << "\"risk\":" << serialize_risk(change.risk);
  oss << "}";
  return oss.str();
}

StagedChange deserialize_change(const std::string& text) {
  StagedChange change;
  change.change_id = json_get_string(text, "change_id");
  change.session_id = json_get_string(text, "session_id");
  change.operation = json_get_string(text, "operation");
  change.path = json_get_string(text, "path");
  change.line_start = json_get_size_t_local(text, "line_start", 0);
  change.line_end = json_get_size_t_local(text, "line_end", 0);
  change.expected_content = json_get_string(text, "expected_content");
  change.base_mtime = json_get_string(text, "base_mtime");
  change.base_hash = json_get_string(text, "base_hash");
  change.new_content = json_get_string(text, "new_content");
  change.selector.query = json_get_string(text, "selector_query");
  change.selector.exact_path = json_get_string(text, "selector_exact_path");
  change.selector.directory_prefix = json_get_string(text, "selector_directory_prefix");
  change.selector.extension = json_get_string(text, "selector_extension");
  change.selector.anchor_before = json_get_string(text, "selector_anchor_before");
  change.selector.anchor_after = json_get_string(text, "selector_anchor_after");
  change.selector.occurrence = json_get_size_t_local(text, "selector_occurrence", 1);
  change.selector.from_end = json_get_bool_local(text, "selector_from_end", false);
  change.selector_reason = json_get_string(text, "selector_reason");
  change.anchor = json_get_string(text, "anchor");
  change.created_at = json_get_string(text, "created_at");
  const auto risk_pos = text.find("\"risk\":");
  if (risk_pos != std::string::npos) {
    const auto brace_start = text.find('{', risk_pos);
    if (brace_start != std::string::npos) {
      int depth = 0;
      std::size_t i = brace_start;
      for (; i < text.size(); ++i) {
        if (text[i] == '{') ++depth;
        else if (text[i] == '}') {
          --depth;
          if (depth == 0) break;
        }
      }
      if (i < text.size()) change.risk = deserialize_risk(text.substr(brace_start, i - brace_start + 1));
    }
  }
  return change;
}

bool load_meta(const WorkspaceConfig& workspace, const std::string& session_id, SessionMeta* out, std::string* error) {
  std::string read_error;
  const auto raw = slurp_text(session_meta_path(workspace, session_id), &read_error);
  if (!read_error.empty()) {
    if (error) *error = read_error;
    return false;
  }
  *out = deserialize_meta(raw);
  if (out->session_id.empty()) out->session_id = session_id;
  return true;
}

bool save_meta(const WorkspaceConfig& workspace, const SessionMeta& meta, std::string* error) {
  return write_text(session_meta_path(workspace, meta.session_id), serialize_meta(meta), error);
}

std::vector<fs::path> change_files(const WorkspaceConfig& workspace, const std::string& session_id) {
  std::vector<fs::path> files;
  std::error_code ec;
  const auto dir = session_changes_dir(workspace, session_id);
  if (!fs::exists(dir, ec) || ec) return files;
  for (const auto& entry : fs::directory_iterator(dir, ec)) {
    if (ec) break;
    if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());
  return files;
}

std::vector<StagedChange> load_changes(const WorkspaceConfig& workspace, const std::string& session_id, std::string* error) {
  std::vector<StagedChange> out;
  for (const auto& path : change_files(workspace, session_id)) {
    std::string read_error;
    const auto raw = slurp_text(path, &read_error);
    if (!read_error.empty()) {
      if (error) *error = read_error;
      return {};
    }
    out.push_back(deserialize_change(raw));
  }
  return out;
}

std::vector<ChangeEntry> load_change_entries(const WorkspaceConfig& workspace, const std::string& session_id, std::string* error) {
  std::vector<ChangeEntry> out;
  for (const auto& path : change_files(workspace, session_id)) {
    std::string read_error;
    const auto raw = slurp_text(path, &read_error);
    if (!read_error.empty()) {
      if (error) *error = read_error;
      return {};
    }
    out.push_back({path, deserialize_change(raw)});
  }
  return out;
}

TextDocument make_document_from_content(const std::string& content, bool exists) {
  TextDocument doc;
  doc.exists = exists;
  doc.content = content;
  doc.trailing_newline = !content.empty() && content.back() == '\n';
  std::string current;
  for (char c : content) {
    if (c == '\r') continue;
    if (c == '\n') {
      doc.lines.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty() || (!content.empty() && content.back() != '\n')) doc.lines.push_back(current);
  return doc;
}

TextDocument load_document(const WorkspaceConfig& workspace, const std::string& path, std::string* error) {
  const auto resolved = resolve_under_workspace(workspace, path);
  if (!resolved.ok) {
    if (error) *error = resolved.error;
    return {};
  }
  std::error_code ec;
  if (!fs::exists(resolved.absolute_path, ec) || ec) {
    TextDocument missing;
    missing.exists = false;
    return missing;
  }
  std::ifstream ifs(resolved.absolute_path, std::ios::binary);
  if (!ifs) {
    if (error) *error = "failed to open file";
    return {};
  }
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  return make_document_from_content(content, true);
}

std::string join_lines(const std::vector<std::string>& lines, bool trailing_newline) {
  std::ostringstream oss;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (i) oss << '\n';
    oss << lines[i];
  }
  if (trailing_newline && (!lines.empty() || !oss.str().empty())) oss << '\n';
  return oss.str();
}

std::string join_non_empty(const std::string& a, const std::string& b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  return a + " | " + b;
}

std::string content_hash(const std::string& content) {
  return hex64(fnv1a64(content));
}

std::string slice_range_text(const TextDocument& doc, std::size_t line_start, std::size_t line_end) {
  if (line_start == 0 || line_end == 0 || line_start > line_end || line_end > doc.lines.size()) return {};
  std::ostringstream oss;
  for (std::size_t i = line_start - 1; i < line_end; ++i) {
    if (i > line_start - 1) oss << '\n';
    oss << doc.lines[i];
  }
  return oss.str();
}

FileFingerprint fingerprint_file(const WorkspaceConfig& workspace, const std::string& path, std::string* error) {
  FileFingerprint out;
  std::string doc_error;
  auto doc = load_document(workspace, path, &doc_error);
  if (!doc_error.empty()) {
    if (error) *error = doc_error;
    return out;
  }
  out.exists = doc.exists;
  out.content = doc.content;
  out.hash = content_hash(doc.content);
  const auto resolved = resolve_under_workspace(workspace, path);
  if (!resolved.ok) {
    if (error) *error = resolved.error;
    return out;
  }
  std::error_code ec;
  if (!fs::exists(resolved.absolute_path, ec) || ec) {
    out.mtime = "missing";
    return out;
  }
  const auto ft = fs::last_write_time(resolved.absolute_path, ec);
  if (ec) {
    if (error) *error = ec.message();
    return out;
  }
  out.mtime = std::to_string(ft.time_since_epoch().count());
  return out;
}

RiskHint evaluate_risk(const StagedChange& change, std::size_t candidate_count = 1) {
  RiskHint risk;
  if (candidate_count > 1) risk.reasons.push_back("multiple-candidates");
  if (change.operation == "delete_block") risk.reasons.push_back("destructive-delete");
  const auto replaced_line_count = (change.line_end >= change.line_start && change.line_start != 0)
                                       ? (change.line_end - change.line_start + 1)
                                       : 0;
  if (change.operation == "replace_range" && replaced_line_count >= 8) {
    risk.reasons.push_back("large-range-replace");
  }
  if (!change.expected_content.empty() && change.expected_content.size() >= 160) {
    risk.reasons.push_back("large-target-block");
  }
  const std::size_t new_lines = std::count(change.new_content.begin(), change.new_content.end(), '\n') + (!change.new_content.empty() ? 1 : 0);
  if (new_lines >= 10) risk.reasons.push_back("large-insert");
  if (new_lines >= 20) risk.reasons.push_back("very-large-insert");
  if ((change.operation == "replace_block" || change.operation == "insert_before" || change.operation == "insert_after" || change.operation == "delete_block") &&
      change.selector.anchor_before.empty() && change.selector.anchor_after.empty()) {
    risk.reasons.push_back("no-anchor");
  }
  if (!change.selector.query.empty() && change.selector.query.size() <= 4) {
    risk.reasons.push_back("short-selector-query");
  }
  if (candidate_count > 3) {
    risk.reasons.push_back("low-selector-specificity");
  }
  if (candidate_count > 1 ||
      std::find(risk.reasons.begin(), risk.reasons.end(), "destructive-delete") != risk.reasons.end() ||
      std::find(risk.reasons.begin(), risk.reasons.end(), "very-large-insert") != risk.reasons.end() ||
      std::find(risk.reasons.begin(), risk.reasons.end(), "low-selector-specificity") != risk.reasons.end()) {
    risk.level = "high";
  } else if (!risk.reasons.empty()) {
    risk.level = "medium";
  }
  return risk;
}

bool save_change(const WorkspaceConfig& workspace, const std::string& session_id, const StagedChange& change, std::string* error) {
  const auto dir = session_changes_dir(workspace, session_id);
  const auto ordinal = change_files(workspace, session_id).size() + 1;
  std::ostringstream name;
  name << std::setw(4) << std::setfill('0') << ordinal << "-" << change.change_id << ".json";
  return write_text(dir / name.str(), serialize_change(change), error);
}

std::string infer_anchor(const Selector& selector) {
  return join_non_empty(selector.anchor_before, selector.anchor_after);
}

std::vector<CandidateRange> locate_candidates(const std::string& content, const Selector& selector) {
  std::vector<CandidateRange> candidates;
  if (selector.query.empty()) return candidates;
  std::size_t pos = 0;
  std::size_t ordinal = 0;
  while (true) {
    pos = content.find(selector.query, pos);
    if (pos == std::string::npos) break;
    const auto end = pos + selector.query.size();
    bool allowed = true;
    if (!selector.anchor_before.empty()) {
      const auto before_pos = content.rfind(selector.anchor_before, pos);
      allowed = before_pos != std::string::npos;
    }
    if (allowed && !selector.anchor_after.empty()) {
      const auto after_pos = content.find(selector.anchor_after, end);
      allowed = after_pos != std::string::npos;
    }
    if (allowed) {
      ++ordinal;
      candidates.push_back({pos, end, ordinal});
    }
    pos = end;
  }
  return candidates;
}

bool resolve_candidate(const std::string& content,
                       const Selector& selector,
                       CandidateRange* chosen,
                       std::size_t* candidate_count,
                       std::string* error) {
  const auto candidates = locate_candidates(content, selector);
  if (candidate_count) *candidate_count = candidates.size();
  if (candidates.empty()) {
    if (error) *error = "selector target not found";
    return false;
  }
  if (selector.occurrence == 0) {
    if (error) *error = "invalid selector occurrence";
    return false;
  }
  if (selector.from_end) {
    if (selector.occurrence > candidates.size()) {
      if (error) *error = "selector target not found";
      return false;
    }
    *chosen = candidates[candidates.size() - selector.occurrence];
    return true;
  }
  if (selector.occurrence > candidates.size()) {
    if (error) *error = "selector target not found";
    return false;
  }
  *chosen = candidates[selector.occurrence - 1];
  return true;
}

std::size_t count_line_of_offset(const std::string& content, std::size_t offset) {
  std::size_t line = 1;
  for (std::size_t i = 0; i < offset && i < content.size(); ++i) {
    if (content[i] == '\n') ++line;
  }
  return line;
}

bool apply_change_to_content(const StagedChange& change,
                             const std::string& original,
                             std::string* updated,
                             SessionInspectItem* item,
                             std::string* error) {
  if (change.operation == "replace_content") {
    if (updated) *updated = change.new_content;
    if (item) { item->line_start = 1; item->line_end = count_line_of_offset(original, original.size()); }
    return true;
  }
  if (change.operation == "replace_range") {
    auto doc = make_document_from_content(original, true);
    if (change.line_start == 0 || change.line_end == 0 || change.line_start > change.line_end || change.line_end > doc.lines.size()) {
      if (error) *error = "invalid line range";
      return false;
    }
    std::vector<std::string> replacement = make_document_from_content(change.new_content, true).lines;
    std::vector<std::string> lines;
    for (std::size_t i = 0; i < change.line_start - 1; ++i) lines.push_back(doc.lines[i]);
    lines.insert(lines.end(), replacement.begin(), replacement.end());
    for (std::size_t i = change.line_end; i < doc.lines.size(); ++i) lines.push_back(doc.lines[i]);
    *updated = join_lines(lines, doc.trailing_newline || (!change.new_content.empty() && change.new_content.back() == '\n'));
    if (item) {
      item->line_start = change.line_start;
      item->line_end = change.line_end;
    }
    return true;
  }

  CandidateRange chosen;
  if (!resolve_candidate(original, change.selector, &chosen, nullptr, error)) {
    return false;
  }

  std::string next = original;
  if (change.operation == "replace_block") {
    next.replace(chosen.start, chosen.end - chosen.start, change.new_content);
  } else if (change.operation == "insert_before") {
    next.insert(chosen.start, change.new_content);
  } else if (change.operation == "insert_after") {
    next.insert(chosen.end, change.new_content);
  } else if (change.operation == "delete_block") {
    next.erase(chosen.start, chosen.end - chosen.start);
  } else {
    if (error) *error = "unsupported operation";
    return false;
  }
  *updated = std::move(next);
  if (item) {
    item->line_start = count_line_of_offset(original, chosen.start);
    item->line_end = count_line_of_offset(original, chosen.end);
  }
  return true;
}

bool change_can_rebase_on_content(const StagedChange& change,
                                  const std::string& content,
                                  std::string* reason) {
  if (change.operation == "replace_content") {
    if (!change.expected_content.empty() && content != change.expected_content) {
      if (reason) *reason = "file content changed";
      return false;
    }
    return true;
  }
  if (change.operation == "replace_range") {
    auto doc = make_document_from_content(content, true);
    if (change.line_start == 0 || change.line_end == 0 || change.line_start > change.line_end || change.line_end > doc.lines.size()) {
      if (reason) *reason = "range drifted";
      return false;
    }
    if (slice_range_text(doc, change.line_start, change.line_end) != change.expected_content) {
      if (reason) *reason = "range content changed";
      return false;
    }
    return true;
  }
  CandidateRange chosen;
  std::string resolve_error;
  if (!resolve_candidate(content, change.selector, &chosen, nullptr, &resolve_error)) {
    if (reason) *reason = resolve_error;
    return false;
  }
  const auto current_target = content.substr(chosen.start, chosen.end - chosen.start);
  if (!change.expected_content.empty() && current_target != change.expected_content) {
    if (reason) *reason = "selector target changed";
    return false;
  }
  return true;
}

std::string summarize_recovery_conflict(const std::vector<std::string>& issues, const std::string& prefix) {
  std::ostringstream oss;
  oss << prefix;
  for (std::size_t i = 0; i < issues.size(); ++i) {
    if (i) oss << "; ";
    oss << issues[i];
  }
  return oss.str();
}

bool build_planned_files(const WorkspaceConfig& workspace,
                         const std::string& session_id,
                         std::vector<PlannedFile>* planned,
                         std::string* error);

std::string session_materialize_file_internal(const WorkspaceConfig& workspace,
                                             const std::string& session_id,
                                             const std::string& path,
                                             std::string* error) {
  std::vector<PlannedFile> planned;
  if (!build_planned_files(workspace, session_id, &planned, error)) return {};
  for (const auto& file : planned) {
    if (file.path == path) return file.final_content;
  }
  auto doc = load_document(workspace, path, error);
  return doc.content;
}

bool build_planned_files(const WorkspaceConfig& workspace,
                         const std::string& session_id,
                         std::vector<PlannedFile>* planned,
                         std::string* error) {
  std::string load_error;
  auto changes = load_changes(workspace, session_id, &load_error);
  if (!load_error.empty()) {
    if (error) *error = load_error;
    return false;
  }
  std::map<std::string, PlannedFile> by_path;
  for (const auto& change : changes) {
    auto it = by_path.find(change.path);
    if (it == by_path.end()) {
      std::string doc_error;
      auto doc = load_document(workspace, change.path, &doc_error);
      if (!doc_error.empty()) {
        if (error) *error = doc_error;
        return false;
      }
      PlannedFile file;
      file.path = change.path;
      file.current_content = doc.content;
      file.final_content = doc.content;
      it = by_path.emplace(change.path, std::move(file)).first;
    }
    SessionInspectItem item;
    item.change_id = change.change_id;
    item.operation = change.operation;
    item.path = change.path;
    item.selector_reason = change.selector_reason;
    item.anchor = change.anchor;
    item.risk_level = change.risk.level;
    std::string apply_error;
    std::string updated;
    if (!apply_change_to_content(change, it->second.final_content, &updated, &item, &apply_error)) {
      if (error) *error = apply_error;
      return false;
    }
    it->second.final_content = std::move(updated);
    it->second.items.push_back(item);
    if (change.risk.level == "high") it->second.risk.level = "high";
    else if (change.risk.level == "medium" && it->second.risk.level != "high") it->second.risk.level = "medium";
    it->second.risk.reasons.insert(it->second.risk.reasons.end(), change.risk.reasons.begin(), change.risk.reasons.end());
  }
  planned->clear();
  for (auto& [_, file] : by_path) planned->push_back(std::move(file));
  std::sort(planned->begin(), planned->end(), [](const PlannedFile& a, const PlannedFile& b) { return a.path < b.path; });
  return true;
}

bool session_exists(const WorkspaceConfig& workspace, const std::string& session_id) {
  std::error_code ec;
  return fs::exists(session_meta_path(workspace, session_id), ec) && !ec;
}

bool ensure_active_session(const WorkspaceConfig& workspace, const std::string& session_id, SessionMeta* meta, std::string* error) {
  if (!load_meta(workspace, session_id, meta, error)) return false;
  if (meta->state == "committed" || meta->state == "aborted") {
    if (error) *error = "session is not active";
    return false;
  }
  return true;
}

std::string summarize_selector(const StagedChange& change) {
  if (change.operation == "replace_range") {
    std::ostringstream oss;
    oss << "line-range " << change.line_start << "-" << change.line_end;
    return oss.str();
  }
  std::ostringstream oss;
  oss << change.operation << " occurrence=" << change.selector.occurrence;
  if (change.selector.from_end) oss << " from_end=true";
  if (!change.selector.query.empty()) oss << " query=" << change.selector.query;
  return oss.str();
}

int risk_rank(const std::string& level) {
  if (level == "high") return 3;
  if (level == "medium") return 2;
  return 1;
}

std::string max_risk_level(const std::string& lhs, const std::string& rhs) {
  return risk_rank(lhs) >= risk_rank(rhs) ? lhs : rhs;
}

void append_unique(std::vector<std::string>* dest, const std::vector<std::string>& src) {
  for (const auto& value : src) {
    if (value.empty()) continue;
    if (std::find(dest->begin(), dest->end(), value) == dest->end()) dest->push_back(value);
  }
}

void append_unique(std::vector<std::string>* dest, const std::string& value) {
  if (value.empty()) return;
  if (std::find(dest->begin(), dest->end(), value) == dest->end()) dest->push_back(value);
}

std::string join_csv(const std::vector<std::string>& values, std::size_t limit = 3) {
  std::ostringstream oss;
  for (std::size_t i = 0; i < values.size() && i < limit; ++i) {
    if (i) oss << ", ";
    oss << values[i];
  }
  if (values.size() > limit) oss << " ...";
  return oss.str();
}

DiffStats analyze_diff(const std::string& diff) {
  DiffStats stats;
  std::istringstream iss(diff);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.rfind("@@", 0) == 0) {
      ++stats.hunk_count;
    } else if (!line.empty() && line[0] == '+' && line.rfind("+++", 0) != 0) {
      ++stats.added_line_count;
    } else if (!line.empty() && line[0] == '-' && line.rfind("---", 0) != 0) {
      ++stats.removed_line_count;
    }
  }
  return stats;
}

SessionInspectFile summarize_inspect_file(const std::string& path,
                                         const std::vector<SessionInspectItem>& items,
                                         const RiskHint& risk) {
  SessionInspectFile file;
  file.path = path;
  file.change_count = items.size();
  file.risk_level = risk.level;
  file.risk_reasons = risk.reasons;
  bool have_line = false;
  for (const auto& item : items) {
    if (item.line_start != 0) {
      if (!have_line || item.line_start < file.first_line) file.first_line = item.line_start;
      if (!have_line || item.line_end > file.last_line) file.last_line = item.line_end;
      have_line = true;
    }
    append_unique(&file.anchors, item.anchor);
    append_unique(&file.selector_reasons, item.selector_reason);
  }
  std::ostringstream summary;
  summary << path << ": " << file.change_count << " staged change" << (file.change_count == 1 ? "" : "s");
  if (have_line) summary << " touching lines " << file.first_line << "-" << file.last_line;
  if (!file.selector_reasons.empty()) summary << "; selector " << join_csv(file.selector_reasons, 2);
  if (!file.risk_reasons.empty()) summary << "; risk " << file.risk_level << " (" << join_csv(file.risk_reasons, 3) << ")";
  file.summary = summary.str();
  return file;
}

std::string summarize_preview_file(const SessionPreviewFile& file) {
  std::ostringstream oss;
  oss << file.path << ": " << file.change_count << " change" << (file.change_count == 1 ? "" : "s")
      << ", " << file.hunk_count << " hunk" << (file.hunk_count == 1 ? "" : "s")
      << ", +" << file.added_line_count << "/-" << file.removed_line_count;
  if (file.first_line != 0) oss << ", lines " << file.first_line << "-" << file.last_line;
  if (!file.risk_reasons.empty()) oss << ", risk " << file.risk_level << " (" << join_csv(file.risk_reasons, 3) << ")";
  return oss.str();
}

std::string summarize_session_overview(const std::string& noun,
                                       std::size_t file_count,
                                       std::size_t change_count,
                                       const std::string& risk_level,
                                       const std::vector<std::string>& reasons) {
  std::ostringstream oss;
  oss << noun << ": " << change_count << " staged change" << (change_count == 1 ? "" : "s")
      << " across " << file_count << " file" << (file_count == 1 ? "" : "s")
      << "; overall risk " << risk_level;
  if (!reasons.empty()) oss << " (" << join_csv(reasons, 4) << ")";
  return oss.str();
}

RiskHint aggregate_session_risk(const std::vector<SessionInspectFile>& files,
                                std::size_t staged_change_count) {
  RiskHint out;
  for (const auto& file : files) {
    out.level = max_risk_level(out.level, file.risk_level.empty() ? std::string("low") : file.risk_level);
    append_unique(&out.reasons, file.risk_reasons);
  }
  if (files.size() >= 2) append_unique(&out.reasons, "cross-file-change");
  if (staged_change_count >= 4) append_unique(&out.reasons, "many-staged-changes");
  if (files.size() >= 2 && out.level == "low") out.level = "medium";
  if (staged_change_count >= 4 && out.level != "high") out.level = "medium";
  return out;
}

} // namespace

SessionBeginResult session_begin(const WorkspaceConfig& workspace,
                                 const std::string& session_id) {
  SessionBeginResult out;
  if (session_id.empty()) {
    out.error = "session_id is empty";
    return out;
  }
  std::string dir_error;
  if (!ensure_session_dirs(workspace, session_id, &dir_error)) {
    out.error = dir_error;
    return out;
  }
  SessionMeta meta;
  if (session_exists(workspace, session_id)) {
    std::string load_error;
    if (!load_meta(workspace, session_id, &meta, &load_error)) {
      out.error = load_error;
      return out;
    }
  } else {
    meta.session_id = session_id;
    meta.state = "created";
    meta.created_at = current_timestamp();
    meta.updated_at = meta.created_at;
    std::string save_error;
    if (!save_meta(workspace, meta, &save_error)) {
      out.error = save_error;
      return out;
    }
  }
  out.ok = true;
  out.session_id = meta.session_id;
  out.state = meta.state;
  out.created_at = meta.created_at;
  return out;
}

SessionMutationResult session_add(const WorkspaceConfig& workspace,
                                  const std::string& session_id,
                                  const StagedChange& input) {
  SessionMutationResult out;
  SessionMeta meta;
  std::string meta_error;
  if (!ensure_active_session(workspace, session_id, &meta, &meta_error)) {
    out.error = meta_error;
    return out;
  }
  StagedChange change = input;
  change.session_id = session_id;
  if (change.change_id.empty()) change.change_id = make_change_id(session_id, change.operation, change.path);
  if (change.created_at.empty()) change.created_at = current_timestamp();
  if (change.selector_reason.empty()) change.selector_reason = summarize_selector(change);
  if (change.anchor.empty()) change.anchor = infer_anchor(change.selector);
  std::string save_error;
  if (!save_change(workspace, session_id, change, &save_error)) {
    out.error = save_error;
    return out;
  }
  meta.state = "staged";
  meta.updated_at = current_timestamp();
  if (!save_meta(workspace, meta, &save_error)) {
    out.error = save_error;
    return out;
  }
  const auto changes = load_changes(workspace, session_id, &save_error);
  if (!save_error.empty()) {
    out.error = save_error;
    return out;
  }
  out.ok = true;
  out.session_id = session_id;
  out.state = meta.state;
  out.change_id = change.change_id;
  out.path = change.path;
  out.staged_change_count = changes.size();
  out.selector_reason = change.selector_reason;
  out.anchor = change.anchor;
  out.risk = change.risk;
  return out;
}

SessionInspectResult session_inspect(const WorkspaceConfig& workspace,
                                     const std::string& session_id) {
  SessionInspectResult out;
  SessionMeta meta;
  std::string meta_error;
  if (!load_meta(workspace, session_id, &meta, &meta_error)) {
    out.error = meta_error;
    return out;
  }
  std::string load_error;
  const auto changes = load_changes(workspace, session_id, &load_error);
  if (!load_error.empty()) {
    out.error = load_error;
    return out;
  }

  std::vector<PlannedFile> planned;
  std::string plan_error;
  const bool have_planned = build_planned_files(workspace, session_id, &planned, &plan_error);

  out.ok = true;
  out.session_id = session_id;
  out.state = meta.state;
  out.staged_change_count = changes.size();
  out.staged_block_count = changes.size();

  std::map<std::string, std::vector<SessionInspectItem>> items_by_path;
  std::map<std::string, RiskHint> risk_by_path;

  if (have_planned) {
    for (const auto& file : planned) {
      items_by_path[file.path] = file.items;
      risk_by_path[file.path] = file.risk;
      for (const auto& item : file.items) {
        out.items.push_back(item);
        if (item.risk_level == "high") ++out.high_risk_change_count;
        else if (item.risk_level == "medium") ++out.medium_risk_change_count;
      }
    }
  } else {
    for (const auto& change : changes) {
      SessionInspectItem item;
      item.change_id = change.change_id;
      item.operation = change.operation;
      item.path = change.path;
      item.line_start = change.line_start;
      item.line_end = change.line_end;
      item.selector_reason = change.selector_reason;
      item.anchor = change.anchor;
      item.risk_level = change.risk.level;
      out.items.push_back(item);
      items_by_path[change.path].push_back(item);
      auto& risk = risk_by_path[change.path];
      risk.level = max_risk_level(risk.level, change.risk.level);
      append_unique(&risk.reasons, change.risk.reasons);
      if (item.risk_level == "high") ++out.high_risk_change_count;
      else if (item.risk_level == "medium") ++out.medium_risk_change_count;
    }
  }

  for (const auto& [path, items] : items_by_path) {
    auto file = summarize_inspect_file(path, items, risk_by_path[path]);
    if (file.risk_level == "high") ++out.high_risk_file_count;
    else if (file.risk_level == "medium") ++out.medium_risk_file_count;
    out.files.push_back(std::move(file));
  }
  std::sort(out.files.begin(), out.files.end(), [](const SessionInspectFile& a, const SessionInspectFile& b) { return a.path < b.path; });
  out.staged_file_count = out.files.size();

  const auto aggregate = aggregate_session_risk(out.files, out.staged_change_count);
  out.risk_level = aggregate.level;
  out.risk_reasons = aggregate.reasons;
  out.summary = summarize_session_overview("inspect", out.staged_file_count, out.staged_change_count, out.risk_level, out.risk_reasons);

  for (const auto& file : out.files) {
    if (file.risk_level == "high") append_unique(&out.highlights, file.summary);
  }
  if (out.highlights.empty()) {
    for (const auto& file : out.files) {
      append_unique(&out.highlights, file.summary);
      if (out.highlights.size() >= 3) break;
    }
  }
  return out;
}

SessionPreviewResult session_preview(const WorkspaceConfig& workspace,
                                     const std::string& session_id) {
  SessionPreviewResult out;
  SessionMeta meta;
  std::string meta_error;
  if (!ensure_active_session(workspace, session_id, &meta, &meta_error)) {
    out.error = meta_error;
    return out;
  }
  const auto check = recovery_check(workspace, session_id);
  if (!check.ok) {
    out.error = check.error;
    return out;
  }
  if (check.conflict_file_count > 0) {
    std::vector<std::string> conflicts;
    for (const auto& file : check.files) {
      if (file.status == "conflict") conflicts.push_back(file.path + " (" + file.conflict_reason + ")");
    }
    out.error = summarize_recovery_conflict(conflicts, "session conflict: ");
    return out;
  }
  if (check.rebase_file_count > 0) {
    std::vector<std::string> pending;
    for (const auto& file : check.files) {
      if (file.status == "rebase_required") pending.push_back(file.path);
    }
    out.error = summarize_recovery_conflict(pending, "session rebase required: ");
    return out;
  }
  std::vector<PlannedFile> planned;
  std::string plan_error;
  if (!build_planned_files(workspace, session_id, &planned, &plan_error)) {
    out.error = plan_error;
    return out;
  }
  out.ok = true;
  out.session_id = session_id;
  out.state = planned.empty() ? meta.state : "previewed";
  out.staged_change_count = 0;

  std::vector<SessionInspectFile> inspect_files;
  for (const auto& file : planned) {
    out.staged_change_count += file.items.size();
    inspect_files.push_back(summarize_inspect_file(file.path, file.items, file.risk));

    auto preview = patch_preview(workspace, file.path, file.final_content);
    if (!preview.ok) {
      out.ok = false;
      out.error = preview.error;
      return out;
    }
    const auto diff_stats = analyze_diff(preview.diff);
    SessionPreviewFile p;
    p.path = file.path;
    p.preview_id = preview.preview_id;
    p.diff = preview.diff;
    p.applicable = preview.applicable;
    p.risk_level = file.risk.level;
    p.change_count = file.items.size();
    p.hunk_count = diff_stats.hunk_count;
    p.added_line_count = diff_stats.added_line_count;
    p.removed_line_count = diff_stats.removed_line_count;
    p.risk_reasons = file.risk.reasons;
    if (!file.items.empty()) {
      p.selector_summary = join_csv(inspect_files.back().selector_reasons, 2);
      p.first_line = inspect_files.back().first_line;
      p.last_line = inspect_files.back().last_line;
    }
    if (!preview.applicable) append_unique(&p.risk_reasons, "preview-not-applicable");
    if (diff_stats.hunk_count >= 3) append_unique(&p.risk_reasons, "multi-hunk-change");
    if ((diff_stats.added_line_count + diff_stats.removed_line_count) >= 20) append_unique(&p.risk_reasons, "large-diff");
    if (p.risk_reasons.size() > file.risk.reasons.size() && p.risk_level == "low") p.risk_level = "medium";
    p.summary = summarize_preview_file(p);
    out.total_hunk_count += p.hunk_count;
    out.total_added_line_count += p.added_line_count;
    out.total_removed_line_count += p.removed_line_count;
    if (p.risk_level == "high") ++out.high_risk_file_count;
    else if (p.risk_level == "medium") ++out.medium_risk_file_count;
    append_unique(&out.risk_reasons, p.risk_reasons);
    out.files.push_back(std::move(p));
  }
  out.previewed_file_count = out.files.size();
  auto aggregate = aggregate_session_risk(inspect_files, out.staged_change_count);
  append_unique(&aggregate.reasons, out.risk_reasons);
  if (out.total_hunk_count >= 4) append_unique(&aggregate.reasons, "multi-hunk-session");
  if ((out.total_added_line_count + out.total_removed_line_count) >= 30) append_unique(&aggregate.reasons, "large-session-diff");
  if (out.total_hunk_count >= 4 && aggregate.level == "low") aggregate.level = "medium";
  if ((out.total_added_line_count + out.total_removed_line_count) >= 30 && aggregate.level != "high") aggregate.level = "medium";
  out.risk_level = aggregate.level;
  out.risk_reasons = aggregate.reasons;
  out.summary = summarize_session_overview("preview", out.previewed_file_count, out.staged_change_count, out.risk_level, out.risk_reasons);
  for (const auto& file : out.files) {
    if (file.risk_level == "high") append_unique(&out.highlights, file.summary);
  }
  if (out.highlights.empty()) {
    for (const auto& file : out.files) {
      append_unique(&out.highlights, file.summary);
      if (out.highlights.size() >= 3) break;
    }
  }

  meta.state = out.state;
  meta.updated_at = current_timestamp();
  std::string save_error;
  if (!save_meta(workspace, meta, &save_error)) {
    out.ok = false;
    out.error = save_error;
    return out;
  }
  return out;
}

SessionCommitResult session_commit(const WorkspaceConfig& workspace,
                                   const std::string& session_id,
                                   const std::string& client_id,
                                   const std::string& request_id) {
  SessionCommitResult out;
  SessionMeta meta;
  std::string meta_error;
  if (!ensure_active_session(workspace, session_id, &meta, &meta_error)) {
    out.error = meta_error;
    return out;
  }
  const auto check = recovery_check(workspace, session_id);
  if (!check.ok) {
    out.error = check.error;
    return out;
  }
  if (check.conflict_file_count > 0) {
    std::vector<std::string> conflicts;
    for (const auto& file : check.files) {
      if (file.status == "conflict") conflicts.push_back(file.path + " (" + file.conflict_reason + ")");
    }
    out.error = summarize_recovery_conflict(conflicts, "session conflict: ");
    return out;
  }
  if (check.rebase_file_count > 0) {
    std::vector<std::string> pending;
    for (const auto& file : check.files) {
      if (file.status == "rebase_required") pending.push_back(file.path);
    }
    out.error = summarize_recovery_conflict(pending, "session rebase required: ");
    return out;
  }
  std::vector<PlannedFile> planned;
  std::string plan_error;
  if (!build_planned_files(workspace, session_id, &planned, &plan_error)) {
    out.error = plan_error;
    return out;
  }
  CommitRecord record;
  record.commit_id = make_commit_id(session_id);
  record.session_id = session_id;
  record.state = "recorded";
  record.created_at = current_timestamp();

  meta.state = "validating";
  meta.updated_at = current_timestamp();
  std::string save_error;
  if (!save_meta(workspace, meta, &save_error)) {
    out.error = save_error;
    return out;
  }

  for (const auto& file : planned) {
    meta.state = "writing";
    meta.updated_at = current_timestamp();
    if (!save_meta(workspace, meta, &save_error)) {
      out.error = save_error;
      return out;
    }
    auto preview = patch_preview(workspace, file.path, file.final_content);
    if (!preview.ok) {
      out.error = preview.error;
      return out;
    }
    PatchBase base{preview.current_mtime, preview.current_hash};
    auto apply = patch_apply(workspace,
                             file.path,
                             file.final_content,
                             base,
                             client_id,
                             session_id,
                             request_id,
                             preview.preview_id);
    if (!apply.ok) {
      out.error = apply.error;
      return out;
    }
    meta.state = "verifying";
    meta.updated_at = current_timestamp();
    if (!save_meta(workspace, meta, &save_error)) {
      out.error = save_error;
      return out;
    }
    record.files.push_back({file.path, preview.preview_id, apply.backup_id, apply.current_hash});
    out.files.push_back({file.path, preview.preview_id, apply.backup_id, apply.current_hash});
  }

  std::ostringstream commit_json;
  commit_json << "{";
  commit_json << "\"commit_id\":\"" << json_escape(record.commit_id) << "\",";
  commit_json << "\"session_id\":\"" << json_escape(record.session_id) << "\",";
  commit_json << "\"state\":\"" << json_escape(record.state) << "\",";
  commit_json << "\"created_at\":\"" << json_escape(record.created_at) << "\",";
  commit_json << "\"files\":[";
  for (std::size_t i = 0; i < record.files.size(); ++i) {
    if (i) commit_json << ",";
    commit_json << "{";
    commit_json << "\"path\":\"" << json_escape(record.files[i].path) << "\",";
    commit_json << "\"preview_id\":\"" << json_escape(record.files[i].preview_id) << "\",";
    commit_json << "\"backup_id\":\"" << json_escape(record.files[i].backup_id) << "\",";
    commit_json << "\"current_hash\":\"" << json_escape(record.files[i].current_hash) << "\"";
    commit_json << "}";
  }
  commit_json << "]}";
  if (!write_text(session_commit_path(workspace, session_id), commit_json.str(), &save_error)) {
    out.error = save_error;
    return out;
  }

  meta.state = "committed";
  meta.updated_at = current_timestamp();
  if (!save_meta(workspace, meta, &save_error)) {
    out.error = save_error;
    return out;
  }

  out.ok = true;
  out.session_id = session_id;
  out.state = meta.state;
  out.commit_id = record.commit_id;
  out.committed_file_count = out.files.size();
  return out;
}

SessionAbortResult session_abort(const WorkspaceConfig& workspace,
                                 const std::string& session_id) {
  SessionAbortResult out;
  SessionMeta meta;
  std::string meta_error;
  if (!load_meta(workspace, session_id, &meta, &meta_error)) {
    out.error = meta_error;
    return out;
  }
  meta.state = "aborted";
  meta.updated_at = current_timestamp();
  std::string save_error;
  if (!save_meta(workspace, meta, &save_error)) {
    out.error = save_error;
    return out;
  }
  out.ok = true;
  out.session_id = session_id;
  out.state = meta.state;
  out.aborted = true;
  return out;
}


SessionMutationResult session_drop_change(const WorkspaceConfig& workspace,
                                          const std::string& session_id,
                                          const std::string& change_id) {
  SessionMutationResult out;
  if (change_id.empty()) {
    out.error = "change_id required";
    return out;
  }
  SessionMeta meta;
  std::string meta_error;
  if (!ensure_active_session(workspace, session_id, &meta, &meta_error)) {
    out.error = meta_error;
    return out;
  }
  std::string load_error;
  const auto entries = load_change_entries(workspace, session_id, &load_error);
  if (!load_error.empty()) { out.error = load_error; return out; }
  bool removed = false;
  std::string dropped_path;
  for (const auto& entry : entries) {
    const auto file_name = entry.meta_path.filename().string();
    const bool id_match = entry.change.change_id == change_id || (!change_id.empty() && file_name.find(change_id) != std::string::npos);
    if (!id_match) continue;
    std::error_code ec;
    fs::remove(entry.meta_path, ec);
    if (ec) { out.error = ec.message(); return out; }
    removed = true;
    dropped_path = entry.change.path;
  }
  if (!removed) { out.error = "change not found"; return out; }
  const auto remaining = load_changes(workspace, session_id, &load_error);
  if (!load_error.empty()) { out.error = load_error; return out; }
  meta.state = remaining.empty() ? "created" : "staged";
  meta.updated_at = current_timestamp();
  std::string save_error;
  if (!save_meta(workspace, meta, &save_error)) { out.error = save_error; return out; }
  out.ok = true;
  out.session_id = session_id;
  out.state = meta.state;
  out.change_id = change_id;
  out.path = dropped_path;
  out.staged_change_count = remaining.size();
  out.selector_reason = "rejected staged change";
  out.risk.level = "low";
  return out;
}

SessionMutationResult session_drop_path(const WorkspaceConfig& workspace,
                                        const std::string& session_id,
                                        const std::string& path) {
  SessionMutationResult out;
  SessionMeta meta;
  std::string meta_error;
  if (!ensure_active_session(workspace, session_id, &meta, &meta_error)) {
    out.error = meta_error;
    return out;
  }
  std::string load_error;
  const auto entries = load_change_entries(workspace, session_id, &load_error);
  if (!load_error.empty()) { out.error = load_error; return out; }
  std::size_t removed_count = 0;
  for (const auto& entry : entries) {
    if (entry.change.path != path) continue;
    std::error_code ec;
    fs::remove(entry.meta_path, ec);
    if (ec) { out.error = ec.message(); return out; }
    ++removed_count;
  }
  if (removed_count == 0) { out.error = "path not found in session"; return out; }
  const auto remaining = load_changes(workspace, session_id, &load_error);
  if (!load_error.empty()) { out.error = load_error; return out; }
  meta.state = remaining.empty() ? "created" : "staged";
  meta.updated_at = current_timestamp();
  std::string save_error;
  if (!save_meta(workspace, meta, &save_error)) { out.error = save_error; return out; }
  out.ok = true;
  out.session_id = session_id;
  out.state = meta.state;
  out.path = path;
  out.staged_change_count = remaining.size();
  out.selector_reason = "rejected staged file";
  out.risk.level = removed_count > 1 ? "medium" : "low";
  if (removed_count > 1) out.risk.reasons.push_back("multi-change-file-drop");
  return out;
}

SessionRecoverResult session_recover(const WorkspaceConfig& workspace,
                                     const std::string& session_id) {
  SessionRecoverResult out;
  SessionMeta meta;
  std::string meta_error;
  if (!load_meta(workspace, session_id, &meta, &meta_error)) {
    out.error = meta_error;
    return out;
  }
  const auto check = recovery_check(workspace, session_id);
  if (!check.ok) {
    out.error = check.error;
    return out;
  }
  out.ok = true;
  out.session_id = session_id;
  out.state = meta.state;
  out.staged_change_count = check.staged_change_count;
  out.conflict_file_count = check.conflict_file_count;
  out.rebase_file_count = check.rebase_file_count;
  out.recoverable = check.recoverable && meta.state != "committed" && meta.state != "aborted";
  return out;
}

RecoveryCheckResult recovery_check(const WorkspaceConfig& workspace,
                                   const std::string& session_id) {
  RecoveryCheckResult out;
  SessionMeta meta;
  std::string meta_error;
  if (!load_meta(workspace, session_id, &meta, &meta_error)) {
    out.error = meta_error;
    return out;
  }
  std::string load_error;
  const auto changes = load_changes(workspace, session_id, &load_error);
  if (!load_error.empty()) {
    out.error = load_error;
    return out;
  }
  std::map<std::string, std::vector<StagedChange>> changes_by_path;
  for (const auto& change : changes) changes_by_path[change.path].push_back(change);
  out.ok = true;
  out.session_id = session_id;
  out.state = meta.state;
  out.staged_change_count = changes.size();
  out.file_count = changes_by_path.size();
  out.recoverable = meta.state != "committed" && meta.state != "aborted";
  for (const auto& [path, file_changes] : changes_by_path) {
    RecoveryCheckFile item;
    item.path = path;
    item.recoverable = true;
    item.status = "clean";
    item.base_hash = file_changes.front().base_hash;
    std::string fp_error;
    const auto fp = fingerprint_file(workspace, path, &fp_error);
    if (!fp_error.empty()) { out.ok = false; out.error = fp_error; return out; }
    item.current_hash = fp.hash;
    const bool hash_changed = !item.base_hash.empty() && item.base_hash != fp.hash;
    const bool mtime_changed = !file_changes.front().base_mtime.empty() && file_changes.front().base_mtime != fp.mtime;
    if (!hash_changed && !mtime_changed) { out.files.push_back(item); continue; }
    bool can_rebase = true;
    std::vector<std::string> reasons;
    std::string staged_base = fp.content;
    for (const auto& change : file_changes) {
      std::string reason;
      if (!change_can_rebase_on_content(change, staged_base, &reason)) {
        can_rebase = false;
        reasons.push_back(reason);
        continue;
      }
      std::string next;
      SessionInspectItem ignored;
      std::string apply_error;
      if (!apply_change_to_content(change, staged_base, &next, &ignored, &apply_error)) {
        can_rebase = false;
        reasons.push_back(apply_error);
        continue;
      }
      staged_base = std::move(next);
    }
    if (can_rebase) {
      item.status = "rebase_required";
      item.rebase_required = true;
      ++out.rebase_file_count;
    } else {
      item.status = "conflict";
      item.recoverable = false;
      item.conflict_reason = summarize_recovery_conflict(reasons, "external change detected: ");
      ++out.conflict_file_count;
      out.recoverable = false;
    }
    out.files.push_back(item);
  }
  return out;
}

RecoveryRebaseResult recovery_rebase(const WorkspaceConfig& workspace,
                                     const std::string& session_id) {
  RecoveryRebaseResult out;
  SessionMeta meta;
  std::string meta_error;
  if (!ensure_active_session(workspace, session_id, &meta, &meta_error)) { out.error = meta_error; return out; }
  const auto check = recovery_check(workspace, session_id);
  if (!check.ok) { out.error = check.error; return out; }
  if (!check.recoverable) {
    std::vector<std::string> conflicts;
    for (const auto& file : check.files) if (file.status == "conflict") conflicts.push_back(file.path + " (" + file.conflict_reason + ")");
    out.error = summarize_recovery_conflict(conflicts, "session conflict: ");
    return out;
  }
  std::string load_error;
  auto entries = load_change_entries(workspace, session_id, &load_error);
  if (!load_error.empty()) { out.error = load_error; return out; }
  std::map<std::string, RecoveryCheckFile> file_status;
  for (const auto& file : check.files) file_status[file.path] = file;
  std::set<std::string> rebased_files;
  for (auto& entry : entries) {
    auto it = file_status.find(entry.change.path);
    if (it == file_status.end() || !it->second.rebase_required) continue;
    std::string fp_error;
    const auto fp = fingerprint_file(workspace, entry.change.path, &fp_error);
    if (!fp_error.empty()) { out.error = fp_error; return out; }
    entry.change.base_hash = fp.hash;
    entry.change.base_mtime = fp.mtime;
    std::string save_error;
    if (!write_text(entry.meta_path, serialize_change(entry.change), &save_error)) { out.error = save_error; return out; }
    rebased_files.insert(entry.change.path);
  }
  meta.state = rebased_files.empty() ? meta.state : "staged";
  meta.updated_at = current_timestamp();
  std::string save_error;
  if (!save_meta(workspace, meta, &save_error)) { out.error = save_error; return out; }
  out.ok = true;
  out.session_id = session_id;
  out.state = meta.state;
  out.recoverable = true;
  out.rebased_file_count = rebased_files.size();
  out.files = check.files;
  for (auto& file : out.files) {
    if (rebased_files.find(file.path) != rebased_files.end()) { file.status = "rebased"; file.rebase_required = false; file.recoverable = true; file.conflict_reason.clear(); }
  }
  return out;
}

SessionSnapshotResult session_snapshot(const WorkspaceConfig& workspace,
                                       const std::string& session_id) {
  SessionSnapshotResult out;
  SessionMeta meta;
  std::string meta_error;
  if (!load_meta(workspace, session_id, &meta, &meta_error)) {
    out.error = meta_error;
    return out;
  }
  std::ostringstream snapshot;
  snapshot << serialize_meta(meta);
  std::string load_error;
  const auto changes = load_changes(workspace, session_id, &load_error);
  if (!load_error.empty()) {
    out.error = load_error;
    return out;
  }
  snapshot << "\n";
  for (const auto& change : changes) snapshot << serialize_change(change) << "\n";
  std::string save_error;
  if (!write_text(session_snapshot_path(workspace, session_id), snapshot.str(), &save_error)) {
    out.error = save_error;
    return out;
  }
  out.ok = true;
  out.session_id = session_id;
  out.state = meta.state;
  out.snapshot_path = session_snapshot_path(workspace, session_id).generic_string();
  return out;
}

SessionMutationResult edit_replace_range(const WorkspaceConfig& workspace,
                                         const std::string& session_id,
                                         const std::string& path,
                                         std::size_t line_start,
                                         std::size_t line_end,
                                         const std::string& new_content) {
  std::string fp_error;
  const auto fp = fingerprint_file(workspace, path, &fp_error);
  if (!fp_error.empty()) { SessionMutationResult out; out.error = fp_error; return out; }
  auto doc = make_document_from_content(fp.content, fp.exists);
  if (line_start == 0 || line_end == 0 || line_start > line_end || line_end > doc.lines.size()) { SessionMutationResult out; out.error = "invalid line range"; return out; }
  StagedChange change;
  change.operation = "replace_range";
  change.path = path;
  change.line_start = line_start;
  change.line_end = line_end;
  change.expected_content = slice_range_text(doc, line_start, line_end);
  change.base_hash = fp.hash;
  change.base_mtime = fp.mtime;
  change.new_content = new_content;
  change.selector_reason = summarize_selector(change);
  change.risk = evaluate_risk(change);
  return session_add(workspace, session_id, change);
}

SessionMutationResult edit_replace_block(const WorkspaceConfig& workspace,
                                         const std::string& session_id,
                                         const std::string& path,
                                         const Selector& selector,
                                         const std::string& new_content) {
  std::string fp_error;
  const auto fp = fingerprint_file(workspace, path, &fp_error);
  if (!fp_error.empty()) { SessionMutationResult out; out.error = fp_error; return out; }
  CandidateRange chosen;
  std::size_t candidate_count = 0;
  std::string resolve_error;
  if (!resolve_candidate(fp.content, selector, &chosen, &candidate_count, &resolve_error)) { SessionMutationResult out; out.error = resolve_error; return out; }
  StagedChange change;
  change.operation = "replace_block";
  change.path = path;
  change.selector = selector;
  change.expected_content = fp.content.substr(chosen.start, chosen.end - chosen.start);
  change.base_hash = fp.hash;
  change.base_mtime = fp.mtime;
  change.new_content = new_content;
  change.anchor = infer_anchor(selector);
  change.selector_reason = summarize_selector(change);
  change.risk = evaluate_risk(change, candidate_count);
  return session_add(workspace, session_id, change);
}

SessionMutationResult edit_insert_before(const WorkspaceConfig& workspace,
                                         const std::string& session_id,
                                         const std::string& path,
                                         const Selector& selector,
                                         const std::string& new_content) {
  std::string fp_error;
  const auto fp = fingerprint_file(workspace, path, &fp_error);
  if (!fp_error.empty()) { SessionMutationResult out; out.error = fp_error; return out; }
  CandidateRange chosen;
  std::size_t candidate_count = 0;
  std::string resolve_error;
  if (!resolve_candidate(fp.content, selector, &chosen, &candidate_count, &resolve_error)) { SessionMutationResult out; out.error = resolve_error; return out; }
  StagedChange change;
  change.operation = "insert_before";
  change.path = path;
  change.selector = selector;
  change.expected_content = fp.content.substr(chosen.start, chosen.end - chosen.start);
  change.base_hash = fp.hash;
  change.base_mtime = fp.mtime;
  change.new_content = new_content;
  change.anchor = infer_anchor(selector);
  change.selector_reason = summarize_selector(change);
  change.risk = evaluate_risk(change, candidate_count);
  return session_add(workspace, session_id, change);
}

SessionMutationResult edit_insert_after(const WorkspaceConfig& workspace,
                                        const std::string& session_id,
                                        const std::string& path,
                                        const Selector& selector,
                                        const std::string& new_content) {
  std::string fp_error;
  const auto fp = fingerprint_file(workspace, path, &fp_error);
  if (!fp_error.empty()) { SessionMutationResult out; out.error = fp_error; return out; }
  CandidateRange chosen;
  std::size_t candidate_count = 0;
  std::string resolve_error;
  if (!resolve_candidate(fp.content, selector, &chosen, &candidate_count, &resolve_error)) { SessionMutationResult out; out.error = resolve_error; return out; }
  StagedChange change;
  change.operation = "insert_after";
  change.path = path;
  change.selector = selector;
  change.expected_content = fp.content.substr(chosen.start, chosen.end - chosen.start);
  change.base_hash = fp.hash;
  change.base_mtime = fp.mtime;
  change.new_content = new_content;
  change.anchor = infer_anchor(selector);
  change.selector_reason = summarize_selector(change);
  change.risk = evaluate_risk(change, candidate_count);
  return session_add(workspace, session_id, change);
}

SessionMutationResult edit_delete_block(const WorkspaceConfig& workspace,
                                        const std::string& session_id,
                                        const std::string& path,
                                        const Selector& selector) {
  std::string fp_error;
  const auto fp = fingerprint_file(workspace, path, &fp_error);
  if (!fp_error.empty()) { SessionMutationResult out; out.error = fp_error; return out; }
  CandidateRange chosen;
  std::size_t candidate_count = 0;
  std::string resolve_error;
  if (!resolve_candidate(fp.content, selector, &chosen, &candidate_count, &resolve_error)) { SessionMutationResult out; out.error = resolve_error; return out; }
  StagedChange change;
  change.operation = "delete_block";
  change.path = path;
  change.selector = selector;
  change.expected_content = fp.content.substr(chosen.start, chosen.end - chosen.start);
  change.base_hash = fp.hash;
  change.base_mtime = fp.mtime;
  change.anchor = infer_anchor(selector);
  change.selector_reason = summarize_selector(change);
  change.risk = evaluate_risk(change, candidate_count);
  return session_add(workspace, session_id, change);
}

SessionMutationResult edit_replace_content(const WorkspaceConfig& workspace,
                                           const std::string& session_id,
                                           const std::string& path,
                                           const std::string& expected_content,
                                           const std::string& new_content,
                                           const std::string& operation,
                                           const std::string& selector_reason,
                                           const std::string& anchor) {
  std::string fp_error;
  const auto fp = fingerprint_file(workspace, path, &fp_error);
  if (!fp_error.empty()) { SessionMutationResult out; out.error = fp_error; return out; }
  StagedChange change;
  change.operation = operation.empty() ? "replace_content" : operation;
  change.path = path;
  change.expected_content = expected_content.empty() ? fp.content : expected_content;
  change.base_hash = fp.hash;
  change.base_mtime = fp.mtime;
  change.new_content = new_content;
  change.anchor = anchor;
  change.selector_reason = selector_reason.empty() ? "structure-adapter whole-file rewrite" : selector_reason;
  change.risk = evaluate_risk(change);
  if (change.expected_content.size() >= 512) {
    change.risk.reasons.push_back("whole-file-rewrite");
    if (change.risk.level == "low") change.risk.level = "medium";
  }
  return session_add(workspace, session_id, change);
}

std::string session_materialize_file(const WorkspaceConfig& workspace,
                                     const std::string& session_id,
                                     const std::string& path,
                                     std::string* error) {
  return session_materialize_file_internal(workspace, session_id, path, error);
}

} // namespace bridge::core
