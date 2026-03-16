#include "bridge/core/protocol.hpp"
#include "bridge/core/error_codes.hpp"
#include "bridge/core/file_service.hpp"
#include "bridge/core/patch_service.hpp"
#include "bridge/core/search_service.hpp"
#include "bridge/core/session_service.hpp"
#include "bridge/core/structure_adapters.hpp"
#include <cctype>
#include <sstream>
#include <vector>

namespace bridge::core {
namespace {

std::string json_unescape(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    const char c = value[i];
    if (c == '\\' && i + 1 < value.size()) {
      const char n = value[++i];
      switch (n) {
        case '\\': out.push_back('\\'); break;
        case '"': out.push_back('"'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        default: out.push_back(n); break;
      }
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::size_t find_json_value_start(const std::string& text, const std::string& key) {
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

std::string json_get_uint_raw(const std::string& text, const std::string& key, const std::string& fallback) {
  const auto start = find_json_value_start(text, key);
  if (start == std::string::npos || start >= text.size() || !std::isdigit(static_cast<unsigned char>(text[start]))) return fallback;
  std::size_t end = start;
  while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) ++end;
  return text.substr(start, end - start);
}

std::size_t json_get_size_t(const std::string& text, const std::string& key, std::size_t fallback) {
  const auto raw = json_get_uint_raw(text, key, "");
  if (raw.empty()) return fallback;
  return static_cast<std::size_t>(std::stoull(raw));
}

bool json_get_bool(const std::string& text, const std::string& key, bool fallback) {
  const auto start = find_json_value_start(text, key);
  if (start == std::string::npos) return fallback;
  if (text.compare(start, 4, "true") == 0) return true;
  if (text.compare(start, 5, "false") == 0) return false;
  return fallback;
}

std::vector<std::string> split_csv(const std::string& csv) {
  std::vector<std::string> parts;
  std::stringstream ss(csv);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) parts.push_back(item);
  }
  return parts;
}

PatchBase extract_patch_base(const std::string& request_json) {
  PatchBase base;
  base.mtime = json_get_string(request_json, "base_mtime");
  base.hash = json_get_string(request_json, "base_hash");
  return base;
}


Selector extract_selector(const std::string& request_json) {
  Selector selector;
  selector.query = json_get_string(request_json, "selector_query");
  if (selector.query.empty()) selector.query = json_get_string(request_json, "query");
  selector.exact_path = json_get_string(request_json, "selector_exact_path");
  selector.directory_prefix = json_get_string(request_json, "selector_directory_prefix");
  selector.extension = json_get_string(request_json, "selector_extension");
  selector.anchor_before = json_get_string(request_json, "anchor_before");
  selector.anchor_after = json_get_string(request_json, "anchor_after");
  selector.occurrence = json_get_size_t(request_json, "occurrence", 1);
  selector.from_end = json_get_bool(request_json, "from_end", false);
  return selector;
}

std::string format_fs_list(const FsListResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"entries\":[";
  for (std::size_t i = 0; i < res.entries.size(); ++i) {
    const auto& e = res.entries[i];
    if (i) oss << ",";
    oss << "{";
    oss << "\"name\":\"" << json_escape(e.name) << "\",";
    oss << "\"path\":\"" << json_escape(e.path) << "\",";
    oss << "\"kind\":\"" << json_escape(e.kind) << "\",";
    oss << "\"policy\":\"" << json_escape(to_string(e.policy)) << "\"";
    oss << "}";
  }
  oss << "],";
  oss << "\"skipped_directories\":[";
  for (std::size_t i = 0; i < res.skipped_directories.size(); ++i) {
    if (i) oss << ",";
    oss << "\"" << json_escape(res.skipped_directories[i]) << "\"";
  }
  oss << "],";
  oss << "\"scanned_files\":" << res.scanned_files << ",";
  oss << "\"skipped_files\":" << res.skipped_files << ",";
  oss << "\"truncated\":" << (res.truncated ? "true" : "false");
  oss << "}";
  return oss.str();
}

std::string format_fs_stat(const FsStatResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"path\":\"" << json_escape(res.path) << "\",";
  oss << "\"kind\":\"" << json_escape(res.kind) << "\",";
  oss << "\"size\":" << res.size << ",";
  oss << "\"mtime\":\"" << json_escape(res.mtime) << "\",";
  oss << "\"encoding\":\"" << json_escape(res.encoding) << "\",";
  oss << "\"bom\":" << (res.bom ? "true" : "false") << ",";
  oss << "\"eol\":\"" << json_escape(res.eol) << "\",";
  oss << "\"binary\":" << (res.binary ? "true" : "false") << ",";
  oss << "\"policy\":\"" << json_escape(to_string(res.policy)) << "\"";
  oss << "}";
  return oss.str();
}

std::string format_fs_read(const FsReadResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"path\":\"" << json_escape(res.path) << "\",";
  oss << "\"encoding\":\"" << json_escape(res.encoding) << "\",";
  oss << "\"bom\":" << (res.bom ? "true" : "false") << ",";
  oss << "\"eol\":\"" << json_escape(res.eol) << "\",";
  oss << "\"binary\":" << (res.binary ? "true" : "false") << ",";
  oss << "\"line_count\":" << res.line_count << ",";
  oss << "\"policy\":\"" << json_escape(to_string(res.policy)) << "\",";
  oss << "\"truncated\":" << (res.truncated ? "true" : "false") << ",";
  oss << "\"content\":\"" << json_escape(res.content) << "\"";
  oss << "}";
  return oss.str();
}

std::string format_fs_write(const FsWriteResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"path\":\"" << json_escape(res.path) << "\",";
  oss << "\"bytes_written\":" << res.bytes_written << ",";
  oss << "\"created\":" << (res.created ? "true" : "false") << ",";
  oss << "\"parent_created\":" << (res.parent_created ? "true" : "false") << ",";
  oss << "\"encoding\":\"" << json_escape(res.encoding) << "\",";
  oss << "\"bom\":" << (res.bom ? "true" : "false") << ",";
  oss << "\"eol\":\"" << json_escape(res.eol) << "\",";
  oss << "\"policy\":\"" << json_escape(to_string(res.policy)) << "\"";
  oss << "}";
  return oss.str();
}

std::string format_fs_mkdir(const FsMkdirResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"path\":\"" << json_escape(res.path) << "\",";
  oss << "\"created\":" << (res.created ? "true" : "false") << ",";
  oss << "\"policy\":\"" << json_escape(to_string(res.policy)) << "\"";
  oss << "}";
  return oss.str();
}

std::string format_fs_move(const FsMoveResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"path\":\"" << json_escape(res.path) << "\",";
  oss << "\"target_path\":\"" << json_escape(res.target_path) << "\",";
  oss << "\"moved\":" << (res.moved ? "true" : "false") << ",";
  oss << "\"parent_created\":" << (res.parent_created ? "true" : "false") << ",";
  oss << "\"overwritten\":" << (res.overwritten ? "true" : "false") << ",";
  oss << "\"policy\":\"" << json_escape(to_string(res.policy)) << "\"";
  oss << "}";
  return oss.str();
}

std::string format_fs_copy(const FsCopyResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"path\":\"" << json_escape(res.path) << "\",";
  oss << "\"target_path\":\"" << json_escape(res.target_path) << "\",";
  oss << "\"copied\":" << (res.copied ? "true" : "false") << ",";
  oss << "\"parent_created\":" << (res.parent_created ? "true" : "false") << ",";
  oss << "\"overwritten\":" << (res.overwritten ? "true" : "false") << ",";
  oss << "\"policy\":\"" << json_escape(to_string(res.policy)) << "\"";
  oss << "}";
  return oss.str();
}

std::string format_fs_rename(const FsRenameResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"path\":\"" << json_escape(res.path) << "\",";
  oss << "\"target_path\":\"" << json_escape(res.target_path) << "\",";
  oss << "\"renamed\":" << (res.renamed ? "true" : "false") << ",";
  oss << "\"overwritten\":" << (res.overwritten ? "true" : "false") << ",";
  oss << "\"policy\":\"" << json_escape(to_string(res.policy)) << "\"";
  oss << "}";
  return oss.str();
}

std::string format_search_match(const SearchMatch& m) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"match_id\":\"" << json_escape(m.match_id) << "\",";
  oss << "\"path\":\"" << json_escape(m.path) << "\",";
  oss << "\"line_start\":" << m.line_start << ",";
  oss << "\"line_end\":" << m.line_end << ",";
  oss << "\"snippet\":\"" << json_escape(m.snippet) << "\",";
  oss << "\"anchor\":\"" << json_escape(m.anchor) << "\",";
  oss << "\"selector_reason\":\"" << json_escape(m.selector_reason) << "\",";
  oss << "\"confidence\":" << m.confidence << ",";
  oss << "\"scope_path\":\"" << json_escape(m.scope_path) << "\",";
  oss << "\"block_type\":\"" << json_escape(m.block_type) << "\"";
  oss << "}";
  return oss.str();
}

std::string make_stream_chunk(const std::string& request_id,
                              const std::string& event,
                              std::size_t index,
                              const std::string& data_json) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"type\":\"chunk\",";
  oss << "\"request_id\":\"" << json_escape(request_id) << "\",";
  oss << "\"event\":\"" << json_escape(event) << "\",";
  oss << "\"index\":" << index << ",";
  oss << "\"data\":" << data_json;
  oss << "}";
  return oss.str();
}


void append_stream_summary_common(std::ostringstream& oss,
                                  const std::string& stream_event,
                                  std::size_t chunk_count,
                                  bool cancelled,
                                  bool timed_out) {
  oss << "\"stream_event\":\"" << json_escape(stream_event) << "\",";
  oss << "\"chunk_count\":" << chunk_count << ",";
  oss << "\"cancelled\":" << (cancelled ? "true" : "false") << ",";
  oss << "\"timed_out\":" << (timed_out ? "true" : "false");
}

std::string make_stream_final(const std::string& request_id, bool ok, const std::string& body_json) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"type\":\"final\",";
  oss << "\"request_id\":\"" << json_escape(request_id) << "\",";
  oss << "\"ok\":" << (ok ? "true" : "false") << ",";
  if (ok) oss << "\"result\":" << body_json;
  else oss << "\"error\":" << body_json;
  oss << "}";
  return oss.str();
}

std::string format_search_result(const SearchResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"matches\":[";
  for (std::size_t i = 0; i < res.matches.size(); ++i) {
    if (i) oss << ",";
    oss << format_search_match(res.matches[i]);
  }
  oss << "],";
  oss << "\"scanned_files\":" << res.scanned_files << ",";
  oss << "\"skipped_files\":" << res.skipped_files << ",";
  oss << "\"skipped_directories\":[";
  for (std::size_t i = 0; i < res.skipped_directories.size(); ++i) {
    if (i) oss << ",";
    oss << "\"" << json_escape(res.skipped_directories[i]) << "\"";
  }
  oss << "],";
  oss << "\"truncated\":" << (res.truncated ? "true" : "false") << ",";
  oss << "\"timed_out\":" << (res.timed_out ? "true" : "false") << ",";
  oss << "\"cancelled\":" << (res.cancelled ? "true" : "false");
  oss << "}";
  return oss.str();
}

std::string format_patch_preview(const PatchPreviewResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"path\":\"" << json_escape(res.path) << "\",";
  oss << "\"preview_id\":\"" << json_escape(res.preview_id) << "\",";
  oss << "\"applicable\":" << (res.applicable ? "true" : "false") << ",";
  oss << "\"encoding\":\"" << json_escape(res.encoding) << "\",";
  oss << "\"bom\":" << (res.bom ? "true" : "false") << ",";
  oss << "\"eol\":\"" << json_escape(res.eol) << "\",";
  oss << "\"current_mtime\":\"" << json_escape(res.current_mtime) << "\",";
  oss << "\"current_hash\":\"" << json_escape(res.current_hash) << "\",";
  oss << "\"new_content_hash\":\"" << json_escape(res.new_content_hash) << "\",";
  oss << "\"preview_created_at\":\"" << json_escape(res.preview_created_at) << "\",";
  oss << "\"preview_expires_at\":\"" << json_escape(res.preview_expires_at) << "\",";
  oss << "\"diff\":\"" << json_escape(res.diff) << "\"";
  oss << "}";
  return oss.str();
}

std::string format_patch_apply(const PatchApplyResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"path\":\"" << json_escape(res.path) << "\",";
  oss << "\"applied\":" << (res.applied ? "true" : "false") << ",";
  oss << "\"preview_id\":\"" << json_escape(res.preview_id) << "\",";
  oss << "\"backup_id\":\"" << json_escape(res.backup_id) << "\",";
  oss << "\"current_mtime\":\"" << json_escape(res.current_mtime) << "\",";
  oss << "\"current_hash\":\"" << json_escape(res.current_hash) << "\",";
  oss << "\"expected_mtime\":\"" << json_escape(res.expected_mtime) << "\",";
  oss << "\"expected_hash\":\"" << json_escape(res.expected_hash) << "\",";
  oss << "\"actual_mtime\":\"" << json_escape(res.actual_mtime) << "\",";
  oss << "\"actual_hash\":\"" << json_escape(res.actual_hash) << "\",";
  oss << "\"conflict_reason\":\"" << json_escape(res.conflict_reason) << "\",";
  oss << "\"preview_status\":\"" << json_escape(res.preview_status) << "\"";
  oss << "}";
  return oss.str();
}

std::string format_patch_rollback(const PatchRollbackResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"path\":\"" << json_escape(res.path) << "\",";
  oss << "\"restored\":" << (res.rolled_back ? "true" : "false") << ",";
  oss << "\"rolled_back\":" << (res.rolled_back ? "true" : "false") << ",";
  oss << "\"backup_id\":\"" << json_escape(res.backup_id) << "\",";
  oss << "\"current_mtime\":\"" << json_escape(res.current_mtime) << "\",";
  oss << "\"current_hash\":\"" << json_escape(res.current_hash) << "\"";
  oss << "}";
  return oss.str();
}


std::string format_string_array(const std::vector<std::string>& values) {
  std::ostringstream oss;
  oss << "[";
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) oss << ",";
    oss << "\"" << json_escape(values[i]) << "\"";
  }
  oss << "]";
  return oss.str();
}

std::string format_risk(const RiskHint& risk) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"level\":\"" << json_escape(risk.level) << "\",";
  oss << "\"reasons\":" << format_string_array(risk.reasons);
  oss << "}";
  return oss.str();
}

std::string format_session_mutation(const SessionMutationResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"session_id\":\"" << json_escape(res.session_id) << "\",";
  oss << "\"state\":\"" << json_escape(res.state) << "\",";
  oss << "\"change_id\":\"" << json_escape(res.change_id) << "\",";
  oss << "\"path\":\"" << json_escape(res.path) << "\",";
  oss << "\"staged_change_count\":" << res.staged_change_count << ",";
  oss << "\"selector_reason\":\"" << json_escape(res.selector_reason) << "\",";
  oss << "\"anchor\":\"" << json_escape(res.anchor) << "\",";
  oss << "\"risk\":" << format_risk(res.risk);
  oss << "}";
  return oss.str();
}

std::string format_session_inspect(const SessionInspectResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"session_id\":\"" << json_escape(res.session_id) << "\",";
  oss << "\"state\":\"" << json_escape(res.state) << "\",";
  oss << "\"staged_change_count\":" << res.staged_change_count << ",";
  oss << "\"staged_file_count\":" << res.staged_file_count << ",";
  oss << "\"staged_block_count\":" << res.staged_block_count << ",";
  oss << "\"high_risk_change_count\":" << res.high_risk_change_count << ",";
  oss << "\"medium_risk_change_count\":" << res.medium_risk_change_count << ",";
  oss << "\"high_risk_file_count\":" << res.high_risk_file_count << ",";
  oss << "\"medium_risk_file_count\":" << res.medium_risk_file_count << ",";
  oss << "\"risk_level\":\"" << json_escape(res.risk_level) << "\",";
  oss << "\"summary\":\"" << json_escape(res.summary) << "\",";
  oss << "\"items\":[";
  for (std::size_t i = 0; i < res.items.size(); ++i) {
    const auto& item = res.items[i];
    if (i) oss << ",";
    oss << "{";
    oss << "\"change_id\":\"" << json_escape(item.change_id) << "\",";
    oss << "\"operation\":\"" << json_escape(item.operation) << "\",";
    oss << "\"path\":\"" << json_escape(item.path) << "\",";
    oss << "\"line_start\":" << item.line_start << ",";
    oss << "\"line_end\":" << item.line_end << ",";
    oss << "\"selector_reason\":\"" << json_escape(item.selector_reason) << "\",";
    oss << "\"anchor\":\"" << json_escape(item.anchor) << "\",";
    oss << "\"risk_level\":\"" << json_escape(item.risk_level) << "\"";
    oss << "}";
  }
  oss << "],";
  oss << "\"files\":[";
  for (std::size_t i = 0; i < res.files.size(); ++i) {
    const auto& file = res.files[i];
    if (i) oss << ",";
    oss << "{";
    oss << "\"path\":\"" << json_escape(file.path) << "\",";
    oss << "\"change_count\":" << file.change_count << ",";
    oss << "\"first_line\":" << file.first_line << ",";
    oss << "\"last_line\":" << file.last_line << ",";
    oss << "\"risk_level\":\"" << json_escape(file.risk_level) << "\",";
    oss << "\"risk_reasons\":" << format_string_array(file.risk_reasons) << ",";
    oss << "\"anchors\":" << format_string_array(file.anchors) << ",";
    oss << "\"selector_reasons\":" << format_string_array(file.selector_reasons) << ",";
    oss << "\"summary\":\"" << json_escape(file.summary) << "\"";
    oss << "}";
  }
  oss << "],";
  oss << "\"highlights\":" << format_string_array(res.highlights) << ",";
  oss << "\"risk_reasons\":" << format_string_array(res.risk_reasons);
  oss << "}";
  return oss.str();
}

std::string format_session_preview(const SessionPreviewResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"session_id\":\"" << json_escape(res.session_id) << "\",";
  oss << "\"state\":\"" << json_escape(res.state) << "\",";
  oss << "\"staged_change_count\":" << res.staged_change_count << ",";
  oss << "\"previewed_file_count\":" << res.previewed_file_count << ",";
  oss << "\"total_hunk_count\":" << res.total_hunk_count << ",";
  oss << "\"total_added_line_count\":" << res.total_added_line_count << ",";
  oss << "\"total_removed_line_count\":" << res.total_removed_line_count << ",";
  oss << "\"high_risk_file_count\":" << res.high_risk_file_count << ",";
  oss << "\"medium_risk_file_count\":" << res.medium_risk_file_count << ",";
  oss << "\"risk_level\":\"" << json_escape(res.risk_level) << "\",";
  oss << "\"summary\":\"" << json_escape(res.summary) << "\",";
  oss << "\"files\":[";
  for (std::size_t i = 0; i < res.files.size(); ++i) {
    const auto& file = res.files[i];
    if (i) oss << ",";
    oss << "{";
    oss << "\"path\":\"" << json_escape(file.path) << "\",";
    oss << "\"preview_id\":\"" << json_escape(file.preview_id) << "\",";
    oss << "\"diff\":\"" << json_escape(file.diff) << "\",";
    oss << "\"selector_summary\":\"" << json_escape(file.selector_summary) << "\",";
    oss << "\"risk_level\":\"" << json_escape(file.risk_level) << "\",";
    oss << "\"change_count\":" << file.change_count << ",";
    oss << "\"hunk_count\":" << file.hunk_count << ",";
    oss << "\"added_line_count\":" << file.added_line_count << ",";
    oss << "\"removed_line_count\":" << file.removed_line_count << ",";
    oss << "\"first_line\":" << file.first_line << ",";
    oss << "\"last_line\":" << file.last_line << ",";
    oss << "\"risk_reasons\":" << format_string_array(file.risk_reasons) << ",";
    oss << "\"summary\":\"" << json_escape(file.summary) << "\",";
    oss << "\"applicable\":" << (file.applicable ? "true" : "false");
    oss << "}";
  }
  oss << "],";
  oss << "\"highlights\":" << format_string_array(res.highlights) << ",";
  oss << "\"risk_reasons\":" << format_string_array(res.risk_reasons);
  oss << "}";
  return oss.str();
}

std::string format_session_commit(const SessionCommitResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"session_id\":\"" << json_escape(res.session_id) << "\",";
  oss << "\"state\":\"" << json_escape(res.state) << "\",";
  oss << "\"commit_id\":\"" << json_escape(res.commit_id) << "\",";
  oss << "\"committed_file_count\":" << res.committed_file_count << ",";
  oss << "\"files\":[";
  for (std::size_t i = 0; i < res.files.size(); ++i) {
    const auto& file = res.files[i];
    if (i) oss << ",";
    oss << "{";
    oss << "\"path\":\"" << json_escape(file.path) << "\",";
    oss << "\"preview_id\":\"" << json_escape(file.preview_id) << "\",";
    oss << "\"backup_id\":\"" << json_escape(file.backup_id) << "\",";
    oss << "\"current_hash\":\"" << json_escape(file.current_hash) << "\"";
    oss << "}";
  }
  oss << "]}";
  return oss.str();
}

std::string format_session_begin(const SessionBeginResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"session_id\":\"" << json_escape(res.session_id) << "\",";
  oss << "\"state\":\"" << json_escape(res.state) << "\",";
  oss << "\"created_at\":\"" << json_escape(res.created_at) << "\"";
  oss << "}";
  return oss.str();
}

std::string format_session_abort(const SessionAbortResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"session_id\":\"" << json_escape(res.session_id) << "\",";
  oss << "\"state\":\"" << json_escape(res.state) << "\",";
  oss << "\"aborted\":" << (res.aborted ? "true" : "false");
  oss << "}";
  return oss.str();
}

std::string format_session_recover(const SessionRecoverResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"session_id\":\"" << json_escape(res.session_id) << "\",";
  oss << "\"state\":\"" << json_escape(res.state) << "\",";
  oss << "\"recoverable\":" << (res.recoverable ? "true" : "false") << ",";
  oss << "\"staged_change_count\":" << res.staged_change_count << ",";
  oss << "\"conflict_file_count\":" << res.conflict_file_count << ",";
  oss << "\"rebase_file_count\":" << res.rebase_file_count;
  oss << "}";
  return oss.str();
}

std::string format_recovery_file(const RecoveryCheckFile& file) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"path\":\"" << json_escape(file.path) << "\",";
  oss << "\"status\":\"" << json_escape(file.status) << "\",";
  oss << "\"recoverable\":" << (file.recoverable ? "true" : "false") << ",";
  oss << "\"rebase_required\":" << (file.rebase_required ? "true" : "false") << ",";
  oss << "\"conflict_reason\":\"" << json_escape(file.conflict_reason) << "\",";
  oss << "\"base_hash\":\"" << json_escape(file.base_hash) << "\",";
  oss << "\"current_hash\":\"" << json_escape(file.current_hash) << "\"";
  oss << "}";
  return oss.str();
}

std::string format_recovery_check(const RecoveryCheckResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"session_id\":\"" << json_escape(res.session_id) << "\",";
  oss << "\"state\":\"" << json_escape(res.state) << "\",";
  oss << "\"recoverable\":" << (res.recoverable ? "true" : "false") << ",";
  oss << "\"staged_change_count\":" << res.staged_change_count << ",";
  oss << "\"file_count\":" << res.file_count << ",";
  oss << "\"conflict_file_count\":" << res.conflict_file_count << ",";
  oss << "\"rebase_file_count\":" << res.rebase_file_count << ",";
  oss << "\"files\":[";
  for (std::size_t i = 0; i < res.files.size(); ++i) { if (i) oss << ","; oss << format_recovery_file(res.files[i]); }
  oss << "]}";
  return oss.str();
}

std::string format_recovery_rebase(const RecoveryRebaseResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"session_id\":\"" << json_escape(res.session_id) << "\",";
  oss << "\"state\":\"" << json_escape(res.state) << "\",";
  oss << "\"recoverable\":" << (res.recoverable ? "true" : "false") << ",";
  oss << "\"rebased_file_count\":" << res.rebased_file_count << ",";
  oss << "\"files\":[";
  for (std::size_t i = 0; i < res.files.size(); ++i) { if (i) oss << ","; oss << format_recovery_file(res.files[i]); }
  oss << "]}";
  return oss.str();
}

std::string format_session_snapshot(const SessionSnapshotResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"session_id\":\"" << json_escape(res.session_id) << "\",";
  oss << "\"state\":\"" << json_escape(res.state) << "\",";
  oss << "\"snapshot_path\":\"" << json_escape(res.snapshot_path) << "\"";
  oss << "}";
  return oss.str();
}

std::string format_history(const HistoryListResult& res) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"items\":[";
  for (std::size_t i = 0; i < res.items.size(); ++i) {
    const auto& item = res.items[i];
    if (i) oss << ",";
    oss << "{";
    oss << "\"timestamp\":\"" << json_escape(item.timestamp) << "\",";
    oss << "\"method\":\"" << json_escape(item.method) << "\",";
    oss << "\"path\":\"" << json_escape(item.path) << "\",";
    oss << "\"backup_id\":\"" << json_escape(item.backup_id) << "\",";
    oss << "\"client_id\":\"" << json_escape(item.client_id) << "\",";
    oss << "\"session_id\":\"" << json_escape(item.session_id) << "\",";
    oss << "\"request_id\":\"" << json_escape(item.request_id) << "\"";
    oss << "}";
  }
  oss << "]}";
  return oss.str();
}

} // namespace

std::string json_escape(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

std::string json_get_string(const std::string& text, const std::string& key) {
  const auto start = find_json_value_start(text, key);
  if (start == std::string::npos || start >= text.size() || text[start] != '"') return {};
  std::string out;
  out.reserve(64);
  bool escaping = false;
  for (std::size_t i = start + 1; i < text.size(); ++i) {
    const char c = text[i];
    if (escaping) {
      switch (c) {
        case '\\': out.push_back('\\'); break;
        case '"': out.push_back('"'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        default: out.push_back(c); break;
      }
      escaping = false;
      continue;
    }
    if (c == '\\') {
      escaping = true;
      continue;
    }
    if (c == '"') return out;
    out.push_back(c);
  }
  return {};
}

std::string make_hello_request(const std::string& instance_key,
                               const std::string& workspace_root,
                               const std::string& profile,
                               const std::string& client_id) {
  std::ostringstream oss;
  oss << "{" << "\"type\":\"hello\"," << "\"protocol_version\":1,"
      << "\"expected_instance_key\":\"" << json_escape(instance_key) << "\"," 
      << "\"expected_workspace\":\"" << json_escape(workspace_root) << "\"," 
      << "\"expected_profile\":\"" << json_escape(profile) << "\"," 
      << "\"client_id\":\"" << json_escape(client_id) << "\""
      << "}";
  return oss.str();
}

std::string make_hello_ack(const std::string& instance_key,
                           const std::string& workspace_root,
                           const std::string& profile,
                           const std::string& endpoint) {
  std::ostringstream oss;
  oss << "{" << "\"type\":\"hello_ack\"," << "\"protocol_version\":1,"
      << "\"instance_key\":\"" << json_escape(instance_key) << "\"," 
      << "\"workspace\":\"" << json_escape(workspace_root) << "\"," 
      << "\"profile\":\"" << json_escape(profile) << "\"," 
      << "\"endpoint\":\"" << json_escape(endpoint) << "\""
      << "}";
  return oss.str();
}

std::string make_request(const std::string& instance_key,
                         const std::string& client_id,
                         const std::string& session_id,
                         const std::string& request_id,
                         const std::string& method,
                         const std::string& workspace_root,
                         const std::string& path) {
  std::ostringstream oss;
  oss << "{" << "\"protocol_version\":1," << "\"instance_key\":\"" << json_escape(instance_key) << "\"," 
      << "\"client_id\":\"" << json_escape(client_id) << "\"," 
      << "\"session_id\":\"" << json_escape(session_id) << "\"," 
      << "\"request_id\":\"" << json_escape(request_id) << "\"," 
      << "\"method\":\"" << json_escape(method) << "\"," 
      << "\"params\":{"
      << "\"workspace_root\":\"" << json_escape(workspace_root) << "\"";
  if (!path.empty()) {
    oss << ",\"path\":\"" << json_escape(path) << "\"";
  }
  oss << "}}";
  return oss.str();
}

std::string make_ok_response(const std::string& request_id, const std::string& result_json) {
  std::ostringstream oss;
  oss << "{" << "\"request_id\":\"" << json_escape(request_id) << "\"," 
      << "\"ok\":true," << "\"result\":" << result_json << ","
      << "\"meta\":{\"truncated\":false}" << "}";
  return oss.str();
}

std::string make_error_response(const std::string& request_id, const std::string& code, const std::string& message) {
  return std::string("{") +
         "\"request_id\":\"" + json_escape(request_id) + "\"," +
         "\"ok\":false," +
         "\"error\":{" +
         "\"code\":\"" + json_escape(code) + "\"," +
         "\"message\":\"" + json_escape(message) + "\"}," +
         "\"meta\":{}" +
         "}";
}

std::string handle_request(const std::string& request_json,
                           const WorkspaceConfig& workspace,
                           const std::string& instance_key,
                           const std::string& endpoint,
                           const std::string& runtime_dir,
                           const std::string& platform,
                           const std::string& transport,
                           const std::function<bool(const std::string&)>& is_cancelled) {
  const std::string request_id = json_get_string(request_json, "request_id");
  const std::string client_id = json_get_string(request_json, "client_id");
  const std::string session_id = json_get_string(request_json, "session_id");
  const std::string method = json_get_string(request_json, "method");

  if (method == "daemon.ping") {
    return make_ok_response(request_id, "{\"message\":\"pong\"}");
  }
  if (method == "workspace.info" || method == "workspace.open") {
    std::ostringstream result;
    result << "{" << "\"workspace_root\":\"" << json_escape(normalize_root_path(workspace.root)) << "\","
           << "\"profile\":\"" << json_escape(workspace.profile_name) << "\","
           << "\"policy\":\"" << json_escape(workspace.policy_name) << "\","
           << "\"instance_key\":\"" << json_escape(instance_key) << "\","
           << "\"endpoint\":\"" << json_escape(endpoint) << "\","
           << "\"runtime_dir\":\"" << json_escape(runtime_dir) << "\","
           << "\"platform\":\"" << json_escape(platform) << "\","
           << "\"transport\":\"" << json_escape(transport) << "\""
           << "}";
    return make_ok_response(request_id, result.str());
  }
  if (method == "workspace.resolve_path") {
    const auto resolved = resolve_under_workspace(workspace, json_get_string(request_json, "path"));
    if (!resolved.ok) {
      return make_error_response(request_id, "PATH_OUTSIDE_WORKSPACE", resolved.error);
    }
    std::ostringstream result;
    result << "{" << "\"workspace_root\":\"" << json_escape(resolved.normalized_root) << "\","
           << "\"relative_path\":\"" << json_escape(resolved.normalized_relative_path) << "\","
           << "\"absolute_path\":\"" << json_escape(resolved.absolute_path) << "\","
           << "\"policy\":\"" << json_escape(to_string(resolved.policy)) << "\""
           << "}";
    return make_ok_response(request_id, result.str());
  }
  if (method == "fs.list") {
    FsListOptions options;
    options.recursive = json_get_bool(request_json, "recursive", false);
    options.include_excluded = json_get_bool(request_json, "include_excluded", false);
    options.max_results = json_get_size_t(request_json, "max_results", 200);
    const auto result = fs_list(workspace, json_get_string(request_json, "path"), options);
    if (!result.ok) { const auto err = classify_common_error(result.error, "FS_LIST_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_fs_list(result));
  }
  if (method == "fs.stat") {
    const auto result = fs_stat(workspace, json_get_string(request_json, "path"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "FS_STAT_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_fs_stat(result));
  }
  if (method == "fs.read") {
    const auto result = fs_read(workspace,
                                json_get_string(request_json, "path"),
                                json_get_size_t(request_json, "max_bytes", 64 * 1024));
    if (!result.ok) { const auto err = classify_common_error(result.error, "FS_READ_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_fs_read(result));
  }
  if (method == "fs.read_range") {
    const auto result = fs_read_range(workspace,
                                      json_get_string(request_json, "path"),
                                      json_get_size_t(request_json, "start_line", 1),
                                      json_get_size_t(request_json, "end_line", 1),
                                      json_get_size_t(request_json, "max_bytes", 64 * 1024));
    if (!result.ok) { const auto err = classify_common_error(result.error, "FS_READ_RANGE_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_fs_read(result));
  }
  if (method == "fs.write") {
    FsWriteOptions options;
    options.encoding = json_get_string(request_json, "encoding");
    if (options.encoding.empty()) options.encoding = "utf-8";
    options.bom = json_get_bool(request_json, "bom", false);
    options.eol = json_get_string(request_json, "eol");
    if (options.eol.empty()) options.eol = "lf";
    options.create_parents = json_get_bool(request_json, "create_parents", true);
    options.overwrite = json_get_bool(request_json, "overwrite", true);
    const auto result = fs_write(workspace,
                                 json_get_string(request_json, "path"),
                                 json_get_string(request_json, "content"),
                                 options);
    if (!result.ok) { const auto err = classify_common_error(result.error, "FS_WRITE_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_fs_write(result));
  }
  if (method == "fs.mkdir") {
    FsMkdirOptions options;
    options.create_parents = json_get_bool(request_json, "create_parents", true);
    const auto result = fs_mkdir(workspace, json_get_string(request_json, "path"), options);
    if (!result.ok) { const auto err = classify_common_error(result.error, "FS_MKDIR_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_fs_mkdir(result));
  }
  if (method == "fs.move") {
    FsMoveOptions options;
    options.create_parents = json_get_bool(request_json, "create_parents", true);
    options.overwrite = json_get_bool(request_json, "overwrite", false);
    const auto result = fs_move(workspace, json_get_string(request_json, "path"), json_get_string(request_json, "target_path"), options);
    if (!result.ok) { const auto err = classify_common_error(result.error, "FS_MOVE_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_fs_move(result));
  }
  if (method == "fs.copy") {
    FsCopyOptions options;
    options.create_parents = json_get_bool(request_json, "create_parents", true);
    options.overwrite = json_get_bool(request_json, "overwrite", false);
    options.recursive = json_get_bool(request_json, "recursive", false);
    const auto result = fs_copy(workspace, json_get_string(request_json, "path"), json_get_string(request_json, "target_path"), options);
    if (!result.ok) { const auto err = classify_common_error(result.error, "FS_COPY_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_fs_copy(result));
  }
  if (method == "fs.rename") {
    FsRenameOptions options;
    options.overwrite = json_get_bool(request_json, "overwrite", false);
    const auto result = fs_rename(workspace, json_get_string(request_json, "path"), json_get_string(request_json, "target_path"), options);
    if (!result.ok) { const auto err = classify_common_error(result.error, "FS_RENAME_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_fs_rename(result));
  }
  if (method == "search.text" || method == "search.regex") {
    SearchOptions options;
    options.root_path = json_get_string(request_json, "path");
    if (options.root_path.empty()) options.root_path = ".";
    options.exact_path = json_get_string(request_json, "exact_path");
    options.directory_prefix = json_get_string(request_json, "directory_prefix");
    options.include_excluded = json_get_bool(request_json, "include_excluded", false);
    options.extensions = split_csv(json_get_string(request_json, "extensions_csv"));
    options.context_before = json_get_size_t(request_json, "context_before", 2);
    options.context_after = json_get_size_t(request_json, "context_after", 2);
    options.min_line = json_get_size_t(request_json, "min_line", 0);
    options.max_line = json_get_size_t(request_json, "max_line", 0);
    options.max_results = json_get_size_t(request_json, "max_results", 100);
    options.max_matches_per_file = json_get_size_t(request_json, "max_matches_per_file", 20);
    options.max_file_bytes = json_get_size_t(request_json, "max_file_bytes", 256 * 1024);
    options.timeout_ms = json_get_size_t(request_json, "timeout_ms", 10000);
    const auto needle = json_get_string(request_json, method == "search.text" ? "query" : "pattern");
    options.cancel_requested = [&]() { return is_cancelled && is_cancelled(request_id); };
    const auto result = method == "search.text" ? search_text(workspace, needle, options) : search_regex(workspace, needle, options);
    if (!result.ok) {
      const auto err = classify_search_error(result.error, result.timed_out, result.cancelled);
      return make_error_response(request_id, err.code, err.message);
    }
    return make_ok_response(request_id, format_search_result(result));
  }
  if (method == "session.begin") {
    const auto result = session_begin(workspace, session_id);
    if (!result.ok) { const auto err = classify_common_error(result.error, "SESSION_BEGIN_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_begin(result));
  }
  if (method == "session.inspect") {
    const auto result = session_inspect(workspace, session_id);
    if (!result.ok) { const auto err = classify_common_error(result.error, "SESSION_INSPECT_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_inspect(result));
  }
  if (method == "session.preview") {
    const auto result = session_preview(workspace, session_id);
    if (!result.ok) { const auto err = classify_common_error(result.error, "SESSION_PREVIEW_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_preview(result));
  }
  if (method == "session.commit") {
    const auto result = session_commit(workspace, session_id, client_id, request_id);
    if (!result.ok) { const auto err = classify_common_error(result.error, "SESSION_COMMIT_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_commit(result));
  }
  if (method == "session.abort") {
    const auto result = session_abort(workspace, session_id);
    if (!result.ok) { const auto err = classify_common_error(result.error, "SESSION_ABORT_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_abort(result));
  }
  if (method == "session.drop_change") {
    const auto result = session_drop_change(workspace, session_id, json_get_string(request_json, "change_id"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "SESSION_DROP_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "session.drop_path") {
    const auto result = session_drop_path(workspace, session_id, json_get_string(request_json, "path"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "SESSION_DROP_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "session.recover") {
    const auto result = session_recover(workspace, session_id);
    if (!result.ok) { const auto err = classify_common_error(result.error, "SESSION_RECOVER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_recover(result));
  }
  if (method == "recovery.check") {
    const auto result = recovery_check(workspace, session_id);
    if (!result.ok) { const auto err = classify_common_error(result.error, "RECOVERY_CHECK_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_recovery_check(result));
  }
  if (method == "recovery.rebase") {
    const auto result = recovery_rebase(workspace, session_id);
    if (!result.ok) { const auto err = classify_common_error(result.error, "RECOVERY_REBASE_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_recovery_rebase(result));
  }
  if (method == "session.snapshot") {
    const auto result = session_snapshot(workspace, session_id);
    if (!result.ok) { const auto err = classify_common_error(result.error, "SESSION_SNAPSHOT_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_snapshot(result));
  }
  if (method == "session.add" || method == "edit.replace_range") {
    const auto result = edit_replace_range(workspace,
                                           session_id,
                                           json_get_string(request_json, "path"),
                                           json_get_size_t(request_json, "start_line", json_get_size_t(request_json, "line_start", 1)),
                                           json_get_size_t(request_json, "end_line", json_get_size_t(request_json, "line_end", 1)),
                                           json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "EDIT_ADD_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "edit.replace_block") {
    const auto result = edit_replace_block(workspace,
                                           session_id,
                                           json_get_string(request_json, "path"),
                                           extract_selector(request_json),
                                           json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "EDIT_ADD_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "edit.insert_before") {
    const auto result = edit_insert_before(workspace,
                                           session_id,
                                           json_get_string(request_json, "path"),
                                           extract_selector(request_json),
                                           json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "EDIT_ADD_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "edit.insert_after") {
    const auto result = edit_insert_after(workspace,
                                          session_id,
                                          json_get_string(request_json, "path"),
                                          extract_selector(request_json),
                                          json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "EDIT_ADD_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "edit.delete_block") {
    const auto result = edit_delete_block(workspace,
                                          session_id,
                                          json_get_string(request_json, "path"),
                                          extract_selector(request_json));
    if (!result.ok) { const auto err = classify_common_error(result.error, "EDIT_ADD_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "markdown.replace_section") {
    const auto result = markdown_replace_section(workspace,
                                                 session_id,
                                                 json_get_string(request_json, "path"),
                                                 json_get_string(request_json, "heading"),
                                                 json_get_size_t(request_json, "heading_level", 2),
                                                 json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "STRUCTURE_ADAPTER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "markdown.insert_after_heading") {
    const auto result = markdown_insert_after_heading(workspace,
                                                      session_id,
                                                      json_get_string(request_json, "path"),
                                                      json_get_string(request_json, "heading"),
                                                      json_get_size_t(request_json, "heading_level", 2),
                                                      json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "STRUCTURE_ADAPTER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "markdown.upsert_section") {
    const auto result = markdown_upsert_section(workspace,
                                                session_id,
                                                json_get_string(request_json, "path"),
                                                json_get_string(request_json, "heading"),
                                                json_get_size_t(request_json, "heading_level", 2),
                                                json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "STRUCTURE_ADAPTER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "json.replace_value") {
    const auto result = json_replace_value(workspace,
                                           session_id,
                                           json_get_string(request_json, "path"),
                                           json_get_string(request_json, "key_path"),
                                           json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "STRUCTURE_ADAPTER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "json.upsert_key") {
    const auto result = json_upsert_key(workspace,
                                        session_id,
                                        json_get_string(request_json, "path"),
                                        json_get_string(request_json, "key_path"),
                                        json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "STRUCTURE_ADAPTER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "json.append_array_item") {
    const auto result = json_append_array_item(workspace,
                                               session_id,
                                               json_get_string(request_json, "path"),
                                               json_get_string(request_json, "key_path"),
                                               json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "STRUCTURE_ADAPTER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "yaml.replace_value") {
    const auto result = yaml_replace_value(workspace,
                                           session_id,
                                           json_get_string(request_json, "path"),
                                           json_get_string(request_json, "key_path"),
                                           json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "STRUCTURE_ADAPTER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "yaml.upsert_key") {
    const auto result = yaml_upsert_key(workspace,
                                        session_id,
                                        json_get_string(request_json, "path"),
                                        json_get_string(request_json, "key_path"),
                                        json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "STRUCTURE_ADAPTER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "yaml.append_item") {
    const auto result = yaml_append_item(workspace,
                                         session_id,
                                         json_get_string(request_json, "path"),
                                         json_get_string(request_json, "key_path"),
                                         json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "STRUCTURE_ADAPTER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "html.replace_node") {
    const auto result = html_replace_node(workspace,
                                          session_id,
                                          json_get_string(request_json, "path"),
                                          json_get_string(request_json, "selector_query"),
                                          json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "STRUCTURE_ADAPTER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "html.insert_after_node") {
    const auto result = html_insert_after_node(workspace,
                                               session_id,
                                               json_get_string(request_json, "path"),
                                               json_get_string(request_json, "selector_query"),
                                               json_get_string(request_json, "new_content"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "STRUCTURE_ADAPTER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "html.set_attribute") {
    const auto result = html_set_attribute(workspace,
                                           session_id,
                                           json_get_string(request_json, "path"),
                                           json_get_string(request_json, "selector_query"),
                                           json_get_string(request_json, "attribute_name"),
                                           json_get_string(request_json, "attribute_value"));
    if (!result.ok) { const auto err = classify_common_error(result.error, "STRUCTURE_ADAPTER_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_session_mutation(result));
  }
  if (method == "patch.preview") {
    const auto result = patch_preview(workspace, json_get_string(request_json, "path"), json_get_string(request_json, "new_content"), extract_patch_base(request_json));
    if (!result.ok) { const auto err = classify_patch_error(result.error); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_patch_preview(result));
  }
  if (method == "patch.apply") {
    const auto result = patch_apply(workspace,
                                    json_get_string(request_json, "path"),
                                    json_get_string(request_json, "new_content"),
                                    extract_patch_base(request_json),
                                    client_id,
                                    session_id,
                                    request_id,
                                    json_get_string(request_json, "preview_id"));
    if (!result.ok) { const auto err = classify_patch_error(result.error); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_patch_apply(result));
  }
  if (method == "patch.rollback") {
    const auto result = patch_rollback(workspace,
                                       json_get_string(request_json, "path"),
                                       json_get_string(request_json, "backup_id"),
                                       client_id,
                                       session_id,
                                       request_id);
    if (!result.ok) { const auto err = classify_patch_error(result.error); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_patch_rollback(result));
  }
  if (method == "history.list") {
    const auto result = history_list(workspace, json_get_string(request_json, "path"), json_get_size_t(request_json, "limit", 20));
    if (!result.ok) { const auto err = classify_common_error(result.error, "HISTORY_LIST_FAILED"); return make_error_response(request_id, err.code, err.message); }
    return make_ok_response(request_id, format_history(result));
  }
  return make_error_response(request_id, "UNSUPPORTED_METHOD", "unsupported method");
}

void handle_request_stream(const std::string& request_json,
                           const WorkspaceConfig& workspace,
                           const std::string& instance_key,
                           const std::string& endpoint,
                           const std::string& runtime_dir,
                           const std::string& platform,
                           const std::string& transport,
                           const StreamEmit& emit,
                           const std::function<bool(const std::string&)>& is_cancelled) {
  const std::string request_id = json_get_string(request_json, "request_id");
  const std::string method = json_get_string(request_json, "method");
  const bool stream = json_get_bool(request_json, "stream", false);
  if (!stream || (method != "search.text" && method != "search.regex" && method != "fs.read" && method != "fs.read_range" && method != "patch.preview")) {
    emit(handle_request(request_json, workspace, instance_key, endpoint, runtime_dir, platform, transport, is_cancelled));
    return;
  }

  if (method == "search.text" || method == "search.regex") {
    SearchOptions options;
    options.root_path = json_get_string(request_json, "path");
    if (options.root_path.empty()) options.root_path = ".";
    options.exact_path = json_get_string(request_json, "exact_path");
    options.directory_prefix = json_get_string(request_json, "directory_prefix");
    options.include_excluded = json_get_bool(request_json, "include_excluded", false);
    options.extensions = split_csv(json_get_string(request_json, "extensions_csv"));
    options.context_before = json_get_size_t(request_json, "context_before", 2);
    options.context_after = json_get_size_t(request_json, "context_after", 2);
    options.min_line = json_get_size_t(request_json, "min_line", 0);
    options.max_line = json_get_size_t(request_json, "max_line", 0);
    options.max_results = json_get_size_t(request_json, "max_results", 100);
    options.max_matches_per_file = json_get_size_t(request_json, "max_matches_per_file", 20);
    options.max_file_bytes = json_get_size_t(request_json, "max_file_bytes", 256 * 1024);
    options.timeout_ms = json_get_size_t(request_json, "timeout_ms", 10000);
    options.cancel_requested = [&]() { return is_cancelled && is_cancelled(request_id); };

    std::size_t chunk_index = 0;
    options.on_match = [&](const SearchMatch& m) {
      return emit(make_stream_chunk(request_id, "search.match", ++chunk_index, format_search_match(m)));
    };

    const auto needle = json_get_string(request_json, method == "search.text" ? "query" : "pattern");
    auto result = method == "search.text" ? search_text(workspace, needle, options) : search_regex(workspace, needle, options);
    if (!result.ok) {
      const auto err = classify_search_error(result.error, result.timed_out, result.cancelled);
      std::ostringstream e;
      e << "{" << "\"code\":\"" << json_escape(err.code) << "\"," << "\"message\":\"" << json_escape(err.message) << "\"}";
      emit(make_stream_final(request_id, false, e.str()));
      return;
    }
    std::ostringstream summary;
    summary << "{";
    summary << "\"match_count\":" << result.matches.size() << ",";
    summary << "\"scanned_files\":" << result.scanned_files << ",";
    summary << "\"skipped_files\":" << result.skipped_files << ",";
    summary << "\"skipped_directories\":[";
    for (std::size_t i = 0; i < result.skipped_directories.size(); ++i) {
      if (i) summary << ",";
      summary << "\"" << json_escape(result.skipped_directories[i]) << "\"";
    }
    summary << "],";
    summary << "\"truncated\":" << (result.truncated ? "true" : "false") << ",";
    append_stream_summary_common(summary, "search.match", chunk_index, result.cancelled, result.timed_out);
    summary << "}";
    emit(make_stream_final(request_id, true, summary.str()));
    return;
  }

  if (method == "patch.preview") {
    PatchPreviewStreamOptions options;
    options.chunk_bytes = json_get_size_t(request_json, "chunk_bytes", 16 * 1024);
    options.timeout_ms = json_get_size_t(request_json, "timeout_ms", 10000);
    options.cancel_requested = [&]() { return is_cancelled && is_cancelled(request_id); };
    std::size_t chunk_index = 0;
    options.on_chunk = [&](const std::string& chunk) {
      std::ostringstream data;
      data << "{" << "\"content\":\"" << json_escape(chunk) << "\"}";
      return emit(make_stream_chunk(request_id, "patch.preview.chunk", ++chunk_index, data.str()));
    };

    const auto result = patch_preview_stream(workspace,
                                             json_get_string(request_json, "path"),
                                             json_get_string(request_json, "new_content"),
                                             extract_patch_base(request_json),
                                             options);
    if (!result.ok) {
      const auto err = classify_patch_error(result.error);
      std::ostringstream e;
      e << "{" << "\"code\":\"" << json_escape(err.code) << "\"," << "\"message\":\"" << json_escape(err.message) << "\"}";
      emit(make_stream_final(request_id, false, e.str()));
      return;
    }

    std::ostringstream summary;
    summary << "{";
    summary << "\"path\":\"" << json_escape(result.path) << "\",";
    summary << "\"preview_id\":\"" << json_escape(result.preview_id) << "\",";
    summary << "\"applicable\":" << (result.applicable ? "true" : "false") << ",";
    summary << "\"encoding\":\"" << json_escape(result.encoding) << "\",";
    summary << "\"bom\":" << (result.bom ? "true" : "false") << ",";
    summary << "\"eol\":\"" << json_escape(result.eol) << "\",";
    summary << "\"current_mtime\":\"" << json_escape(result.current_mtime) << "\",";
    summary << "\"current_hash\":\"" << json_escape(result.current_hash) << "\",";
    summary << "\"new_content_hash\":\"" << json_escape(result.new_content_hash) << "\",";
    append_stream_summary_common(summary, "patch.preview.chunk", result.chunk_count, result.cancelled, result.timed_out);
    summary << ",\"total_bytes\":" << result.total_bytes;
    summary << "}";
    emit(make_stream_final(request_id, true, summary.str()));
    return;
  }

  FsReadStreamOptions options;
  options.max_bytes = json_get_size_t(request_json, "max_bytes", 1024 * 1024);
  options.chunk_bytes = json_get_size_t(request_json, "chunk_bytes", 16 * 1024);
  options.timeout_ms = json_get_size_t(request_json, "timeout_ms", 10000);
  options.cancel_requested = [&]() { return is_cancelled && is_cancelled(request_id); };
  std::size_t chunk_index = 0;
  options.on_chunk = [&](const std::string& chunk) {
    std::ostringstream data;
    data << "{" << "\"content\":\"" << json_escape(chunk) << "\"}";
    return emit(make_stream_chunk(request_id,
                                  method == "fs.read" ? "fs.read.chunk" : "fs.read_range.chunk",
                                  ++chunk_index,
                                  data.str()));
  };

  FsReadStreamResult result;
  if (method == "fs.read") {
    result = fs_read_stream(workspace, json_get_string(request_json, "path"), options);
  } else {
    result = fs_read_range_stream(workspace,
                                  json_get_string(request_json, "path"),
                                  json_get_size_t(request_json, "start_line", 1),
                                  json_get_size_t(request_json, "end_line", 1),
                                  options);
  }
  if (!result.ok) {
    const auto err = classify_common_error(result.error, result.cancelled ? "REQUEST_CANCELLED" : (result.timed_out ? "REQUEST_TIMEOUT" : "FS_READ_FAILED"));
    std::ostringstream e;
    e << "{" << "\"code\":\"" << json_escape(err.code) << "\"," << "\"message\":\"" << json_escape(err.message) << "\"}";
    emit(make_stream_final(request_id, false, e.str()));
    return;
  }
  std::ostringstream summary;
  summary << "{";
  summary << "\"path\":\"" << json_escape(result.path) << "\",";
  summary << "\"encoding\":\"" << json_escape(result.encoding) << "\",";
  summary << "\"bom\":" << (result.bom ? "true" : "false") << ",";
  summary << "\"eol\":\"" << json_escape(result.eol) << "\",";
  summary << "\"binary\":false,";
  summary << "\"line_count\":" << result.line_count << ",";
  summary << "\"truncated\":" << (result.truncated ? "true" : "false") << ",";
  append_stream_summary_common(summary, method == "fs.read" ? "fs.read.chunk" : "fs.read_range.chunk", result.chunk_count, result.cancelled, result.timed_out);
  summary << ",\"total_bytes\":" << result.total_bytes;
  summary << "}";
  emit(make_stream_final(request_id, true, summary.str()));
}


} // namespace bridge::core
