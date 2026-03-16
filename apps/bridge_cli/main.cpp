#include "bridge/core/instance.hpp"
#include "bridge/core/path_policy.hpp"
#include "bridge/core/protocol.hpp"
#include "bridge/core/workspace.hpp"
#include "bridge/platform/runtime.hpp"
#include "bridge/transport/transport.hpp"
#include "bridge/core/version.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

namespace {
std::string escape_json(const std::string& value) {
  return bridge::core::json_escape(value);
}

std::string slurp_file(const std::string& path, std::string* error) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    if (error) *error = "failed to open file: " + path;
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

bool response_ok(const std::string& response) {
  return response.find("\"ok\":true") != std::string::npos;
}

std::string json_get_scalar(const std::string& text, const std::string& key) {
  const std::regex re(std::string("\"") + key + R"(":(true|false|-?[0-9]+(?:\.[0-9]+)?))");
  std::smatch match;
  if (std::regex_search(text, match, re)) return match[1].str();
  return {};
}

std::string format_human(const std::string& command, const std::string& response) {
  if (!response_ok(response)) {
    const auto code = bridge::core::json_get_string(response, "code");
    const auto message = bridge::core::json_get_string(response, "message");
    std::ostringstream oss;
    oss << "error";
    if (!code.empty()) oss << " [" << code << "]";
    if (!message.empty()) oss << ": " << message;
    return oss.str();
  }

  if (command == "ping") {
    return bridge::core::json_get_string(response, "message");
  }
  if (command == "info" || command == "open") {
    std::ostringstream oss;
    oss << "workspace_root: " << bridge::core::json_get_string(response, "workspace_root") << "\n"
        << "profile: " << bridge::core::json_get_string(response, "profile") << "\n"
        << "policy: " << bridge::core::json_get_string(response, "policy") << "\n"
        << "instance_key: " << bridge::core::json_get_string(response, "instance_key") << "\n"
        << "endpoint: " << bridge::core::json_get_string(response, "endpoint") << "\n"
        << "runtime_dir: " << bridge::core::json_get_string(response, "runtime_dir") << "\n"
        << "platform: " << bridge::core::json_get_string(response, "platform") << "\n"
        << "transport: " << bridge::core::json_get_string(response, "transport");
    return oss.str();
  }
  if (command == "resolve") {
    std::ostringstream oss;
    oss << "workspace_root: " << bridge::core::json_get_string(response, "workspace_root") << "\n"
        << "relative_path: " << bridge::core::json_get_string(response, "relative_path") << "\n"
        << "absolute_path: " << bridge::core::json_get_string(response, "absolute_path") << "\n"
        << "policy: " << bridge::core::json_get_string(response, "policy");
    return oss.str();
  }
  if (command == "stat") {
    std::ostringstream oss;
    oss << "path: " << bridge::core::json_get_string(response, "path") << "\n"
        << "kind: " << bridge::core::json_get_string(response, "kind") << "\n"
        << "size: " << bridge::core::json_get_string(response, "size") << "\n"
        << "mtime: " << bridge::core::json_get_string(response, "mtime") << "\n"
        << "encoding: " << bridge::core::json_get_string(response, "encoding") << "\n"
        << "eol: " << bridge::core::json_get_string(response, "eol") << "\n"
        << "policy: " << bridge::core::json_get_string(response, "policy");
    return oss.str();
  }
  if (command == "read" || command == "read-range") {
    return bridge::core::json_get_string(response, "content");
  }
  if (command == "write") {
    std::ostringstream oss;
    oss << "path: " << bridge::core::json_get_string(response, "path") << "\n"
        << "bytes_written: " << json_get_scalar(response, "bytes_written") << "\n"
        << "created: " << json_get_scalar(response, "created") << "\n"
        << "parent_created: " << json_get_scalar(response, "parent_created");
    return oss.str();
  }
  if (command == "mkdir") {
    std::ostringstream oss;
    oss << "path: " << bridge::core::json_get_string(response, "path") << "\n"
        << "created: " << json_get_scalar(response, "created");
    return oss.str();
  }
  if (command == "move") {
    std::ostringstream oss;
    oss << "path: " << bridge::core::json_get_string(response, "path") << "\n"
        << "target_path: " << bridge::core::json_get_string(response, "target_path") << "\n"
        << "moved: " << json_get_scalar(response, "moved") << "\n"
        << "parent_created: " << json_get_scalar(response, "parent_created") << "\n"
        << "overwritten: " << json_get_scalar(response, "overwritten");
    return oss.str();
  }
  if (command == "copy") {
    std::ostringstream oss;
    oss << "path: " << bridge::core::json_get_string(response, "path") << "\n"
        << "target_path: " << bridge::core::json_get_string(response, "target_path") << "\n"
        << "copied: " << json_get_scalar(response, "copied") << "\n"
        << "parent_created: " << json_get_scalar(response, "parent_created") << "\n"
        << "overwritten: " << json_get_scalar(response, "overwritten");
    return oss.str();
  }
  if (command == "rename") {
    std::ostringstream oss;
    oss << "path: " << bridge::core::json_get_string(response, "path") << "\n"
        << "target_path: " << bridge::core::json_get_string(response, "target_path") << "\n"
        << "renamed: " << json_get_scalar(response, "renamed") << "\n"
        << "overwritten: " << json_get_scalar(response, "overwritten");
    return oss.str();
  }
  if (command == "patch-preview") {
    std::ostringstream oss;
    const auto preview_id = bridge::core::json_get_string(response, "preview_id");
    if (!preview_id.empty()) oss << "preview_id: " << preview_id << "\n";
    oss << bridge::core::json_get_string(response, "diff");
    return oss.str();
  }
  if (command == "patch-apply") {
    std::ostringstream oss;
    const auto preview_id = bridge::core::json_get_string(response, "preview_id");
    oss << "applied: " << bridge::core::json_get_string(response, "applied") << "\n";
    if (!preview_id.empty()) oss << "preview_id: " << preview_id << "\n";
    oss << "backup_id: " << bridge::core::json_get_string(response, "backup_id") << "\n"
        << "current_mtime: " << bridge::core::json_get_string(response, "current_mtime") << "\n"
        << "current_hash: " << bridge::core::json_get_string(response, "current_hash");
    return oss.str();
  }
  if (command == "recovery-check") {
    std::ostringstream oss;
    oss << "recoverable: " << json_get_scalar(response, "recoverable") << "\n"
        << "conflict_file_count: " << json_get_scalar(response, "conflict_file_count") << "\n"
        << "rebase_file_count: " << json_get_scalar(response, "rebase_file_count");
    return oss.str();
  }
  if (command == "recovery-rebase") {
    std::ostringstream oss;
    oss << "recoverable: " << json_get_scalar(response, "recoverable") << "\n"
        << "rebased_file_count: " << json_get_scalar(response, "rebased_file_count");
    return oss.str();
  }
  if (command == "cancel") {
    std::ostringstream oss;
    oss << "cancelled: " << bridge::core::json_get_string(response, "cancelled") << "\n"
        << "target_request_id: " << bridge::core::json_get_string(response, "target_request_id");
    return oss.str();
  }
  if (command == "patch-rollback") {
    std::ostringstream oss;
    oss << "rolled_back: " << bridge::core::json_get_string(response, "rolled_back") << "\n"
        << "backup_id: " << bridge::core::json_get_string(response, "backup_id");
    return oss.str();
  }
  if (command == "history") {
    std::ostringstream oss;
    std::regex item_re("\\{\\\"timestamp\\\":\\\"([^\\\"]*)\\\",\\\"method\\\":\\\"([^\\\"]*)\\\",\\\"path\\\":\\\"([^\\\"]*)\\\",\\\"backup_id\\\":\\\"([^\\\"]*)\\\",\\\"client_id\\\":\\\"([^\\\"]*)\\\",\\\"session_id\\\":\\\"([^\\\"]*)\\\",\\\"request_id\\\":\\\"([^\\\"]*)\\\"\\}");
    auto begin = std::sregex_iterator(response.begin(), response.end(), item_re);
    auto end = std::sregex_iterator();
    bool first = true;
    for (auto it = begin; it != end; ++it) {
      if (!first) oss << "\n";
      first = false;
      oss << (*it)[1].str() << " " << (*it)[2].str() << " " << (*it)[3].str() << " backup=" << (*it)[4].str();
    }
    return first ? std::string("(no history)") : oss.str();
  }
  if (command == "list") {
    std::ostringstream oss;
    std::regex entry_re("\\{\\\"name\\\":\\\"([^\\\"]*)\\\",\\\"path\\\":\\\"([^\\\"]*)\\\",\\\"kind\\\":\\\"([^\\\"]*)\\\",\\\"policy\\\":\\\"([^\\\"]*)\\\"\\}");
    auto begin = std::sregex_iterator(response.begin(), response.end(), entry_re);
    auto end = std::sregex_iterator();
    bool first = true;
    for (auto it = begin; it != end; ++it) {
      if (!first) oss << "\n";
      first = false;
      auto p = (*it)[2].str();
      if (p.empty()) p = (*it)[1].str();
      oss << p << " [" << (*it)[3].str() << "] (" << (*it)[4].str() << ")";
    }
    return first ? std::string("(no entries)") : oss.str();
  }
  if (command == "search-text" || command == "search-regex") {
    const auto path = bridge::core::json_get_string(response, "path");
    const auto line_start = json_get_scalar(response, "line_start");
    const auto line_end = json_get_scalar(response, "line_end");
    const auto snippet = bridge::core::json_get_string(response, "snippet");
    if (path.empty()) return std::string("(no matches)");
    std::ostringstream oss;
    oss << path;
    if (!line_start.empty()) {
      oss << ":" << line_start;
      if (!line_end.empty() && line_end != line_start) oss << "-" << line_end;
    }
    if (!snippet.empty()) oss << "\n" << snippet;
    return oss.str();
  }
  return response;
}


std::string format_stream_frame_human(const std::string& command, const std::string& frame) {
  const auto type = bridge::core::json_get_string(frame, "type");
  if (type == "chunk") {
    const auto event = bridge::core::json_get_string(frame, "event");
    if ((command == "search-text" || command == "search-regex") && event == "search.match") {
      const auto path = bridge::core::json_get_string(frame, "path");
      const auto snippet = bridge::core::json_get_string(frame, "snippet");
      std::ostringstream oss;
      if (!path.empty()) oss << path << "\n";
      oss << snippet;
      return oss.str();
    }
    if ((command == "read" || command == "read-range") && (event == "fs.read.chunk" || event == "fs.read_range.chunk")) {
      return bridge::core::json_get_string(frame, "content");
    }
    if (command == "patch-preview" && event == "patch.preview.chunk") {
      return bridge::core::json_get_string(frame, "content");
    }
  }
  if (type == "final") {
    if (frame.find("\"ok\":true") != std::string::npos) {
      const auto chunks = bridge::core::json_get_string(frame, "chunk_count");
      return chunks.empty() ? std::string("stream complete") : (std::string("stream complete (") + chunks + " chunks)");
    }
    return format_human(command, frame);
  }
  return frame;
}

struct StreamState {
  bool saw_final = false;
  bool final_ok = false;
};

} // namespace

int main(int argc, char** argv) {
  if (argc >= 2) {
    const std::string first = argv[1];
    if (first == "--version" || first == "version") {
      std::cout << "ai_bridge_cli " << AI_BRIDGE_VERSION
                << " platform=" << bridge::platform::platform_family()
                << " transport=" << bridge::transport::transport_family() << "\n";
      return 0;
    }
  }
  if (argc < 2) {
    std::cerr << "usage: bridge_cli <ping|info|open|resolve|list|stat|read|read-range|write|mkdir|move|copy|rename|search-text|search-regex|session-begin|session-inspect|session-preview|session-commit|session-abort|session-drop-change|session-drop-path|session-recover|session-snapshot|edit-replace-range|edit-replace-block|edit-insert-before|edit-insert-after|edit-delete-block|markdown-replace-section|markdown-insert-after-heading|markdown-upsert-section|json-replace-value|json-upsert-key|json-append-array-item|yaml-replace-value|yaml-upsert-key|yaml-append-item|html-replace-node|html-insert-after-node|html-set-attribute|cancel|patch-preview|patch-apply|patch-rollback|history> --workspace <path> [options]\n";
    return 1;
  }

  std::string command = argv[1];
  std::string workspace = ".";
  std::string path;
  std::string target_path;
  std::string query;
  std::string pattern;
  std::string exts_csv;
  std::string new_content_file;
  std::string target_request_id;
  std::string request_id = "req-001";
  std::string client_id = "cli-001";
  std::string session_id = "sess-001";
  std::string base_mtime;
  std::string base_hash;
  std::string backup_id;
  std::string preview_id;
  std::string change_id;
  std::string anchor_before;
  std::string anchor_after;
  std::string heading;
  std::string key_path;
  std::string attribute_name;
  std::string attribute_value;
  std::size_t occurrence = 1;
  bool from_end = false;
  std::size_t heading_level = 2;
  std::size_t start_line = 1;
  std::size_t end_line = 1;
  std::size_t max_results = 200;
  std::size_t max_bytes = 65536;
  std::size_t limit = 20;
  std::size_t before = 2;
  std::size_t chunk_bytes = 16384;
  std::size_t after = 2;
  std::size_t timeout_ms = 10000;
  bool recursive = false;
  bool include_excluded = false;
  bool create_parents = true;
  bool overwrite = false;
  bool json_output = false;
  bool stream_enabled = false;

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--workspace" && i + 1 < argc) workspace = argv[++i];
    else if (arg == "--path" && i + 1 < argc) path = argv[++i];
    else if (arg == "--target-path" && i + 1 < argc) target_path = argv[++i];
    else if (arg == "--query" && i + 1 < argc) query = argv[++i];
    else if (arg == "--pattern" && i + 1 < argc) pattern = argv[++i];
    else if (arg == "--exts" && i + 1 < argc) exts_csv = argv[++i];
    else if ((arg == "--new-content-file" || arg == "--content-file") && i + 1 < argc) new_content_file = argv[++i];
    else if (arg == "--target-request-id" && i + 1 < argc) target_request_id = argv[++i];
    else if (arg == "--request-id" && i + 1 < argc) request_id = argv[++i];
    else if (arg == "--client-id" && i + 1 < argc) client_id = argv[++i];
    else if (arg == "--session-id" && i + 1 < argc) session_id = argv[++i];
    else if (arg == "--base-mtime" && i + 1 < argc) base_mtime = argv[++i];
    else if (arg == "--base-hash" && i + 1 < argc) base_hash = argv[++i];
    else if (arg == "--backup-id" && i + 1 < argc) backup_id = argv[++i];
    else if (arg == "--preview-id" && i + 1 < argc) preview_id = argv[++i];
    else if (arg == "--change-id" && i + 1 < argc) change_id = argv[++i];
    else if (arg == "--anchor-before" && i + 1 < argc) anchor_before = argv[++i];
    else if (arg == "--anchor-after" && i + 1 < argc) anchor_after = argv[++i];
    else if (arg == "--heading" && i + 1 < argc) heading = argv[++i];
    else if (arg == "--heading-level" && i + 1 < argc) heading_level = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (arg == "--key-path" && i + 1 < argc) key_path = argv[++i];
    else if (arg == "--attr-name" && i + 1 < argc) attribute_name = argv[++i];
    else if (arg == "--attr-value" && i + 1 < argc) attribute_value = argv[++i];
    else if (arg == "--occurrence" && i + 1 < argc) occurrence = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (arg == "--from-end") from_end = true;
    else if (arg == "--start" && i + 1 < argc) start_line = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (arg == "--end" && i + 1 < argc) end_line = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (arg == "--max-results" && i + 1 < argc) max_results = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (arg == "--max-bytes" && i + 1 < argc) max_bytes = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (arg == "--limit" && i + 1 < argc) limit = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (arg == "--chunk-bytes" && i + 1 < argc) chunk_bytes = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (arg == "--before" && i + 1 < argc) before = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (arg == "--after" && i + 1 < argc) after = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (arg == "--timeout-ms" && i + 1 < argc) timeout_ms = static_cast<std::size_t>(std::stoull(argv[++i]));
    else if (arg == "--recursive") recursive = true;
    else if (arg == "--create-parents") create_parents = true;
    else if (arg == "--no-create-parents") create_parents = false;
    else if (arg == "--overwrite") overwrite = true;
    else if (arg == "--no-overwrite") overwrite = false;
    else if (arg == "--include-excluded") include_excluded = true;
    else if (arg == "--json") json_output = true;
    else if (arg == "--stream") stream_enabled = true;
  }

  auto cfg = bridge::core::make_default_workspace_config(workspace);
  const auto normalized_root = bridge::core::normalize_root_path(cfg.root);
  bridge::core::InstanceScope scope{bridge::platform::current_user_id(), normalized_root, cfg.profile_name, cfg.policy_name};
  const auto instance_key = bridge::core::make_instance_key(scope);
  const auto runtime = bridge::platform::make_runtime_paths(instance_key);

  const auto hello = bridge::core::make_hello_request(instance_key, normalized_root, cfg.profile_name, client_id);
  const std::string method =
      command == "ping" ? "daemon.ping" :
      command == "info" ? "workspace.info" :
      command == "open" ? "workspace.open" :
      command == "resolve" ? "workspace.resolve_path" :
      command == "list" ? "fs.list" :
      command == "stat" ? "fs.stat" :
      command == "read" ? "fs.read" :
      command == "read-range" ? "fs.read_range" :
      command == "write" ? "fs.write" :
      command == "mkdir" ? "fs.mkdir" :
      command == "move" ? "fs.move" :
      command == "copy" ? "fs.copy" :
      command == "rename" ? "fs.rename" :
      command == "search-text" ? "search.text" :
      command == "search-regex" ? "search.regex" :
      command == "session-begin" ? "session.begin" :
      command == "session-inspect" ? "session.inspect" :
      command == "session-preview" ? "session.preview" :
      command == "session-commit" ? "session.commit" :
      command == "session-abort" ? "session.abort" :
      command == "session-drop-change" ? "session.drop_change" :
      command == "session-drop-path" ? "session.drop_path" :
      command == "session-recover" ? "session.recover" :
      command == "recovery-check" ? "recovery.check" :
      command == "recovery-rebase" ? "recovery.rebase" :
      command == "session-snapshot" ? "session.snapshot" :
      command == "edit-replace-range" ? "edit.replace_range" :
      command == "edit-replace-block" ? "edit.replace_block" :
      command == "edit-insert-before" ? "edit.insert_before" :
      command == "edit-insert-after" ? "edit.insert_after" :
      command == "edit-delete-block" ? "edit.delete_block" :
      command == "markdown-replace-section" ? "markdown.replace_section" :
      command == "markdown-insert-after-heading" ? "markdown.insert_after_heading" :
      command == "markdown-upsert-section" ? "markdown.upsert_section" :
      command == "json-replace-value" ? "json.replace_value" :
      command == "json-upsert-key" ? "json.upsert_key" :
      command == "json-append-array-item" ? "json.append_array_item" :
      command == "yaml-replace-value" ? "yaml.replace_value" :
      command == "yaml-upsert-key" ? "yaml.upsert_key" :
      command == "yaml-append-item" ? "yaml.append_item" :
      command == "html-replace-node" ? "html.replace_node" :
      command == "html-insert-after-node" ? "html.insert_after_node" :
      command == "html-set-attribute" ? "html.set_attribute" :
      command == "cancel" ? "request.cancel" :
      command == "patch-preview" ? "patch.preview" :
      command == "patch-apply" ? "patch.apply" :
      command == "patch-rollback" ? "patch.rollback" :
      command == "history" ? "history.list" : "";
  if (method.empty()) {
    std::cerr << "unsupported command\n";
    return 1;
  }

  std::string new_content;
  if (method == "fs.write" || method == "patch.preview" || (method == "patch.apply" && preview_id.empty()) ||
      method == "edit.replace_range" || method == "edit.replace_block" || method == "edit.insert_before" || method == "edit.insert_after" ||
      method == "markdown.replace_section" || method == "markdown.insert_after_heading" || method == "markdown.upsert_section" ||
      method == "json.replace_value" || method == "json.upsert_key" || method == "json.append_array_item" ||
      method == "yaml.replace_value" || method == "yaml.upsert_key" || method == "yaml.append_item" ||
      method == "html.replace_node" || method == "html.insert_after_node") {
    std::string read_error;
    new_content = slurp_file(new_content_file, &read_error);
    if (!read_error.empty()) {
      std::cerr << read_error << "\n";
      return 1;
    }
  }

  std::ostringstream req;
  req << "{" << "\"protocol_version\":1," << "\"instance_key\":\"" << escape_json(instance_key)
      << "\"," << "\"client_id\":\"" << escape_json(client_id) << "\","
      << "\"session_id\":\"" << escape_json(session_id) << "\","
      << "\"request_id\":\"" << escape_json(request_id) << "\"," << "\"method\":\""
      << escape_json(method) << "\"," << "\"params\":{";
  req << "\"workspace_root\":\"" << escape_json(normalized_root) << "\"";
  if (!path.empty()) req << ",\"path\":\"" << escape_json(path) << "\"";

  if (method == "fs.list") {
    req << ",\"recursive\":" << (recursive ? "true" : "false")
        << ",\"include_excluded\":" << (include_excluded ? "true" : "false")
        << ",\"max_results\":" << max_results;
  }
  if (method == "fs.read") {
    req << ",\"max_bytes\":" << max_bytes << ",\"stream\":" << (stream_enabled ? "true" : "false") << ",\"chunk_bytes\":" << chunk_bytes << ",\"timeout_ms\":" << timeout_ms;
  }
  if (method == "fs.read_range") {
    req << ",\"start_line\":" << start_line << ",\"end_line\":" << end_line << ",\"max_bytes\":" << max_bytes << ",\"stream\":" << (stream_enabled ? "true" : "false") << ",\"chunk_bytes\":" << chunk_bytes << ",\"timeout_ms\":" << timeout_ms;
  }
  if (method == "fs.write") {
    req << ",\"content\":\"" << escape_json(new_content) << "\"";
  }
  if (method == "fs.mkdir") {
    req << ",\"create_parents\":" << (create_parents ? "true" : "false");
  }
  if (method == "fs.move") {
    req << ",\"target_path\":\"" << escape_json(target_path) << "\""
        << ",\"create_parents\":" << (create_parents ? "true" : "false")
        << ",\"overwrite\":" << (overwrite ? "true" : "false");
  }
  if (method == "fs.copy") {
    req << ",\"target_path\":\"" << escape_json(target_path) << "\""
        << ",\"create_parents\":" << (create_parents ? "true" : "false")
        << ",\"overwrite\":" << (overwrite ? "true" : "false")
        << ",\"recursive\":" << (recursive ? "true" : "false");
  }
  if (method == "fs.rename") {
    req << ",\"target_path\":\"" << escape_json(target_path) << "\""
        << ",\"overwrite\":" << (overwrite ? "true" : "false");
  }
  if (method == "search.text") {
    req << ",\"query\":\"" << escape_json(query) << "\""
        << ",\"extensions_csv\":\"" << escape_json(exts_csv) << "\""
        << ",\"include_excluded\":" << (include_excluded ? "true" : "false")
        << ",\"context_before\":" << before
        << ",\"context_after\":" << after
        << ",\"max_results\":" << max_results
        << ",\"timeout_ms\":" << timeout_ms
        << ",\"stream\":" << (stream_enabled ? "true" : "false");
  }
  if (method == "search.regex") {
    req << ",\"pattern\":\"" << escape_json(pattern) << "\""
        << ",\"extensions_csv\":\"" << escape_json(exts_csv) << "\""
        << ",\"include_excluded\":" << (include_excluded ? "true" : "false")
        << ",\"context_before\":" << before
        << ",\"context_after\":" << after
        << ",\"max_results\":" << max_results
        << ",\"timeout_ms\":" << timeout_ms
        << ",\"stream\":" << (stream_enabled ? "true" : "false");
  }
  if (method == "edit.replace_range") {
    req << ",\"new_content\":\"" << escape_json(new_content) << "\""
        << ",\"start_line\":" << start_line
        << ",\"end_line\":" << end_line;
  }
  if (method == "edit.replace_block" || method == "edit.insert_before" || method == "edit.insert_after" ||
      method == "markdown.replace_section" || method == "markdown.insert_after_heading" || method == "markdown.upsert_section" ||
      method == "json.replace_value" || method == "json.upsert_key" || method == "json.append_array_item" ||
      method == "yaml.replace_value" || method == "yaml.upsert_key" || method == "yaml.append_item" ||
      method == "html.replace_node" || method == "html.insert_after_node") {
    req << ",\"new_content\":\"" << escape_json(new_content) << "\"";
  }
  if (method == "edit.replace_block" || method == "edit.insert_before" || method == "edit.insert_after" || method == "edit.delete_block") {
    req << ",\"selector_query\":\"" << escape_json(query) << "\""
        << ",\"anchor_before\":\"" << escape_json(anchor_before) << "\""
        << ",\"anchor_after\":\"" << escape_json(anchor_after) << "\""
        << ",\"occurrence\":" << occurrence
        << ",\"from_end\":" << (from_end ? "true" : "false");
  }
  if (method == "markdown.replace_section" || method == "markdown.insert_after_heading" || method == "markdown.upsert_section") {
    req << ",\"heading\":\"" << escape_json(heading) << "\""
        << ",\"heading_level\":" << heading_level;
  }
  if (method == "json.replace_value" || method == "json.upsert_key" || method == "json.append_array_item" ||
      method == "yaml.replace_value" || method == "yaml.upsert_key" || method == "yaml.append_item") {
    req << ",\"key_path\":\"" << escape_json(key_path) << "\"";
  }
  if (method == "html.replace_node" || method == "html.insert_after_node" || method == "html.set_attribute") {
    req << ",\"selector_query\":\"" << escape_json(query) << "\"";
  }
  if (method == "html.set_attribute") {
    req << ",\"attribute_name\":\"" << escape_json(attribute_name) << "\""
        << ",\"attribute_value\":\"" << escape_json(attribute_value) << "\"";
  }
  if (method == "request.cancel") {
    req << ",\"target_request_id\":\"" << escape_json(target_request_id) << "\"";
  }
  if (method == "session.drop_change") {
    req << ",\"change_id\":\"" << escape_json(change_id) << "\"";
  }
  if (method == "patch.preview" || method == "patch.apply") {
    if (!new_content.empty()) req << ",\"new_content\":\"" << escape_json(new_content) << "\"";
    if (!preview_id.empty()) req << ",\"preview_id\":\"" << escape_json(preview_id) << "\"";
    if (!base_mtime.empty()) req << ",\"base_mtime\":\"" << escape_json(base_mtime) << "\"";
    if (!base_hash.empty()) req << ",\"base_hash\":\"" << escape_json(base_hash) << "\"";
    if (method == "patch.preview") {
      req << ",\"stream\":" << (stream_enabled ? "true" : "false")
          << ",\"chunk_bytes\":" << chunk_bytes
          << ",\"timeout_ms\":" << timeout_ms;
    }
  }
  if (method == "patch.rollback") {
    req << ",\"backup_id\":\"" << escape_json(backup_id) << "\"";
  }
  if (method == "history.list") {
    req << ",\"limit\":" << limit;
  }
  req << "}}";

  std::size_t transport_timeout_ms = timeout_ms;
  if (method == "search.text" || method == "search.regex" || method == "fs.read" || method == "fs.read_range" || method == "patch.preview") {
    transport_timeout_ms = timeout_ms == 0 ? 0 : (timeout_ms + 1000);
  }

  std::string response;
  std::string error;
  if (stream_enabled && (method == "search.text" || method == "search.regex" || method == "fs.read" || method == "fs.read_range" || method == "patch.preview")) {
    StreamState state{};
    auto on_frame = [](const std::string& frame, void* ctx_void) -> bool {
      auto* pair = static_cast<std::pair<StreamState*, std::pair<bool, std::string>*>*>(ctx_void);
      auto* state = pair->first;
      auto* opts = pair->second;
      const bool json_mode = opts->first;
      const std::string& cmd = opts->second;
      if (json_mode) std::cout << frame << std::endl;
      else std::cout << format_stream_frame_human(cmd, frame) << std::endl;
      if (bridge::core::json_get_string(frame, "type") == "final") {
        state->saw_final = true;
        state->final_ok = frame.find("\"ok\":true") != std::string::npos;
      }
      return true;
    };
    std::pair<bool, std::string> opts{json_output, command};
    std::pair<StreamState*, std::pair<bool, std::string>*> ctx{&state, &opts};
    if (!bridge::transport::send_request_stream(runtime.endpoint, hello, req.str(), on_frame, &ctx, &error, transport_timeout_ms)) {
      std::cerr << "request failed: " << error << "\n";
      return 1;
    }
    return (state.saw_final && state.final_ok) ? 0 : 1;
  }
  if (!bridge::transport::send_request(runtime.endpoint, hello, req.str(), &response, &error, transport_timeout_ms)) {
    std::cerr << "request failed: " << error << "\n";
    return 1;
  }
  if (json_output) std::cout << response << std::endl;
  else std::cout << format_human(command, response) << std::endl;
  return response_ok(response) ? 0 : 1;
}
