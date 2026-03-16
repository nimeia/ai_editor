#include "bridge/core/structure_adapters.hpp"
#include "bridge/core/protocol.hpp"
#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace bridge::core {
namespace {

std::vector<std::string> split_path(const std::string& text, char delim = '.') {
  std::vector<std::string> out;
  std::string cur;
  for (char c : text) {
    if (c == delim) {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

std::string trim_copy(const std::string& s) {
  std::size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
  std::size_t end = s.size();
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
  return s.substr(start, end - start);
}

bool starts_with(const std::string& text, const std::string& prefix) {
  return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

std::string slurp_effective_file(const WorkspaceConfig& workspace,
                                 const std::string& session_id,
                                 const std::string& path,
                                 std::string* error) {
  return session_materialize_file(workspace, session_id, path, error);
}

SessionMutationResult stage_whole_file_change(const WorkspaceConfig& workspace,
                                              const std::string& session_id,
                                              const std::string& path,
                                              const std::string& expected,
                                              const std::string& next,
                                              const std::string& operation,
                                              const std::string& selector_reason,
                                              const std::string& anchor = {}) {
  if (expected == next) {
    SessionMutationResult out;
    out.error = "no structural change";
    return out;
  }
  (void)operation;
  return edit_replace_content(workspace, session_id, path, expected, next, "replace_content", selector_reason, anchor);
}

struct MarkdownLine {
  std::string text;
  std::size_t start = 0;
  std::size_t end = 0;
};

std::vector<MarkdownLine> markdown_lines(const std::string& text) {
  std::vector<MarkdownLine> out;
  std::size_t start = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\n') {
      out.push_back(MarkdownLine{text.substr(start, i - start), start, i + 1});
      start = i + 1;
    }
  }
  if (start < text.size()) out.push_back(MarkdownLine{text.substr(start), start, text.size()});
  else if (!text.empty()) out.push_back(MarkdownLine{"", text.size(), text.size()});
  return out;
}

std::optional<std::pair<std::size_t, std::size_t>> find_markdown_section_range(const std::string& content,
                                                                                const std::string& heading,
                                                                                std::size_t heading_level,
                                                                                std::size_t* heading_line_end = nullptr) {
  const auto lines = markdown_lines(content);
  const std::string needle = std::string(heading_level, '#') + " " + heading;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (trim_copy(lines[i].text) != needle) continue;
    std::size_t section_start = lines[i].start;
    std::size_t body_start = lines[i].end;
    std::size_t section_end = content.size();
    for (std::size_t j = i + 1; j < lines.size(); ++j) {
      const auto trimmed = trim_copy(lines[j].text);
      if (!trimmed.empty() && trimmed[0] == '#') {
        std::size_t hashes = 0;
        while (hashes < trimmed.size() && trimmed[hashes] == '#') ++hashes;
        if (hashes <= heading_level && hashes > 0 && hashes < trimmed.size() && trimmed[hashes] == ' ') {
          section_end = lines[j].start;
          break;
        }
      }
    }
    if (heading_line_end) *heading_line_end = body_start;
    return std::make_pair(section_start, section_end);
  }
  return std::nullopt;
}

std::string markdown_normalize_body(std::string body) {
  if (!body.empty() && body.back() != '\n') body.push_back('\n');
  return body;
}

struct JsonNode {
  enum class Kind { Null, Bool, Number, String, Object, Array } kind = Kind::Null;
  std::string scalar;
  std::vector<std::pair<std::string, JsonNode>> object_items;
  std::vector<JsonNode> array_items;
};

struct JsonParser {
  const std::string& text;
  std::size_t pos = 0;

  void ws() {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
  }

  [[noreturn]] void fail(const std::string& message) const {
    throw std::runtime_error(message);
  }

  bool consume(char c) {
    ws();
    if (pos < text.size() && text[pos] == c) {
      ++pos;
      return true;
    }
    return false;
  }

  char peek() {
    ws();
    if (pos >= text.size()) fail("unexpected end of json");
    return text[pos];
  }

  std::string parse_string_raw() {
    ws();
    if (pos >= text.size() || text[pos] != '"') fail("expected string");
    ++pos;
    std::string out;
    while (pos < text.size()) {
      char c = text[pos++];
      if (c == '"') return out;
      if (c == '\\') {
        if (pos >= text.size()) fail("bad escape");
        char e = text[pos++];
        switch (e) {
          case '"': out.push_back('"'); break;
          case '\\': out.push_back('\\'); break;
          case '/': out.push_back('/'); break;
          case 'b': out.push_back('\b'); break;
          case 'f': out.push_back('\f'); break;
          case 'n': out.push_back('\n'); break;
          case 'r': out.push_back('\r'); break;
          case 't': out.push_back('\t'); break;
          default: out.push_back(e); break;
        }
      } else {
        out.push_back(c);
      }
    }
    fail("unterminated string");
  }

  JsonNode parse_value() {
    ws();
    const char c = peek();
    if (c == '{') return parse_object();
    if (c == '[') return parse_array();
    if (c == '"') {
      JsonNode node;
      node.kind = JsonNode::Kind::String;
      node.scalar = parse_string_raw();
      return node;
    }
    if (starts_with(text.substr(pos), "true")) {
      pos += 4;
      JsonNode node; node.kind = JsonNode::Kind::Bool; node.scalar = "true"; return node;
    }
    if (starts_with(text.substr(pos), "false")) {
      pos += 5;
      JsonNode node; node.kind = JsonNode::Kind::Bool; node.scalar = "false"; return node;
    }
    if (starts_with(text.substr(pos), "null")) {
      pos += 4;
      JsonNode node; node.kind = JsonNode::Kind::Null; return node;
    }
    JsonNode node;
    node.kind = JsonNode::Kind::Number;
    std::size_t start = pos;
    while (pos < text.size() && (std::isdigit(static_cast<unsigned char>(text[pos])) || text[pos] == '-' || text[pos] == '+' || text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E')) ++pos;
    if (start == pos) fail("expected json value");
    node.scalar = text.substr(start, pos - start);
    return node;
  }

  JsonNode parse_object() {
    JsonNode node;
    node.kind = JsonNode::Kind::Object;
    consume('{');
    ws();
    if (consume('}')) return node;
    while (true) {
      auto key = parse_string_raw();
      if (!consume(':')) fail("expected colon");
      node.object_items.push_back({key, parse_value()});
      ws();
      if (consume('}')) break;
      if (!consume(',')) fail("expected comma");
    }
    return node;
  }

  JsonNode parse_array() {
    JsonNode node;
    node.kind = JsonNode::Kind::Array;
    consume('[');
    ws();
    if (consume(']')) return node;
    while (true) {
      node.array_items.push_back(parse_value());
      ws();
      if (consume(']')) break;
      if (!consume(',')) fail("expected comma");
    }
    return node;
  }
};

std::string json_indent(int depth) { return std::string(static_cast<std::size_t>(depth * 2), ' '); }

std::string json_serialize(const JsonNode& node, int depth = 0) {
  switch (node.kind) {
    case JsonNode::Kind::Null: return "null";
    case JsonNode::Kind::Bool:
    case JsonNode::Kind::Number: return node.scalar;
    case JsonNode::Kind::String: return "\"" + json_escape(node.scalar) + "\"";
    case JsonNode::Kind::Array: {
      if (node.array_items.empty()) return "[]";
      std::ostringstream oss;
      oss << "[\n";
      for (std::size_t i = 0; i < node.array_items.size(); ++i) {
        if (i) oss << ",\n";
        oss << json_indent(depth + 1) << json_serialize(node.array_items[i], depth + 1);
      }
      oss << "\n" << json_indent(depth) << "]";
      return oss.str();
    }
    case JsonNode::Kind::Object: {
      if (node.object_items.empty()) return "{}";
      std::ostringstream oss;
      oss << "{\n";
      for (std::size_t i = 0; i < node.object_items.size(); ++i) {
        if (i) oss << ",\n";
        oss << json_indent(depth + 1) << "\"" << json_escape(node.object_items[i].first) << "\": "
            << json_serialize(node.object_items[i].second, depth + 1);
      }
      oss << "\n" << json_indent(depth) << "}";
      return oss.str();
    }
  }
  return "null";
}

JsonNode parse_json(const std::string& text) {
  JsonParser parser{text};
  auto node = parser.parse_value();
  parser.ws();
  if (parser.pos != text.size()) throw std::runtime_error("trailing json content");
  return node;
}

JsonNode* json_object_get(JsonNode& node, const std::string& key) {
  if (node.kind != JsonNode::Kind::Object) return nullptr;
  for (auto& [name, value] : node.object_items) if (name == key) return &value;
  return nullptr;
}

JsonNode* json_ensure_object_path(JsonNode& root, const std::vector<std::string>& parts, bool include_last_container = true) {
  if (root.kind != JsonNode::Kind::Object) root = JsonNode{JsonNode::Kind::Object};
  JsonNode* cur = &root;
  std::size_t limit = include_last_container ? parts.size() : (parts.empty() ? 0 : parts.size() - 1);
  for (std::size_t i = 0; i < limit; ++i) {
    auto* next = json_object_get(*cur, parts[i]);
    if (!next) {
      cur->object_items.push_back({parts[i], JsonNode{JsonNode::Kind::Object}});
      next = &cur->object_items.back().second;
    }
    if (next->kind != JsonNode::Kind::Object) next->kind = JsonNode::Kind::Object, next->object_items.clear(), next->array_items.clear(), next->scalar.clear();
    cur = next;
  }
  return cur;
}

JsonNode* json_find_path(JsonNode& root, const std::vector<std::string>& parts) {
  JsonNode* cur = &root;
  for (const auto& part : parts) {
    if (cur->kind == JsonNode::Kind::Object) {
      cur = json_object_get(*cur, part);
      if (!cur) return nullptr;
    } else {
      return nullptr;
    }
  }
  return cur;
}

struct YamlNode {
  enum class Kind { Scalar, Map, Array } kind = Kind::Map;
  std::string scalar;
  std::vector<std::pair<std::string, YamlNode>> map_items;
  std::vector<YamlNode> array_items;
};

std::size_t count_indent(const std::string& line) {
  std::size_t i = 0;
  while (i < line.size() && line[i] == ' ') ++i;
  return i;
}

struct YamlLineCursor {
  std::vector<std::string> lines;
  std::size_t idx = 0;
};

YamlNode yaml_parse_block(YamlLineCursor& cur, int indent);

YamlNode yaml_parse_scalar_or_container(const std::string& value) {
  YamlNode node;
  node.kind = YamlNode::Kind::Scalar;
  node.scalar = trim_copy(value);
  return node;
}

YamlNode yaml_parse_array(YamlLineCursor& cur, int indent) {
  YamlNode node;
  node.kind = YamlNode::Kind::Array;
  while (cur.idx < cur.lines.size()) {
    const auto& line = cur.lines[cur.idx];
    if (trim_copy(line).empty()) { ++cur.idx; continue; }
    const auto line_indent = static_cast<int>(count_indent(line));
    if (line_indent < indent || !starts_with(line.substr(static_cast<std::size_t>(line_indent)), "- ")) break;
    std::string rest = line.substr(static_cast<std::size_t>(line_indent + 2));
    ++cur.idx;
    if (trim_copy(rest).empty()) {
      node.array_items.push_back(yaml_parse_block(cur, indent + 2));
    } else if (rest.find(':') != std::string::npos && rest.back() == ':') {
      YamlNode map_node; map_node.kind = YamlNode::Kind::Map;
      map_node.map_items.push_back({trim_copy(rest.substr(0, rest.size() - 1)), yaml_parse_block(cur, indent + 2)});
      node.array_items.push_back(std::move(map_node));
    } else {
      node.array_items.push_back(yaml_parse_scalar_or_container(rest));
    }
  }
  return node;
}

YamlNode yaml_parse_map(YamlLineCursor& cur, int indent) {
  YamlNode node;
  node.kind = YamlNode::Kind::Map;
  while (cur.idx < cur.lines.size()) {
    const auto& line = cur.lines[cur.idx];
    if (trim_copy(line).empty()) { ++cur.idx; continue; }
    const auto line_indent = static_cast<int>(count_indent(line));
    if (line_indent < indent || starts_with(line.substr(static_cast<std::size_t>(line_indent)), "- ")) break;
    const auto trimmed = line.substr(static_cast<std::size_t>(line_indent));
    const auto colon = trimmed.find(':');
    if (colon == std::string::npos) break;
    const auto key = trim_copy(trimmed.substr(0, colon));
    const auto rest = trim_copy(trimmed.substr(colon + 1));
    ++cur.idx;
    if (rest.empty()) node.map_items.push_back({key, yaml_parse_block(cur, indent + 2)});
    else node.map_items.push_back({key, yaml_parse_scalar_or_container(rest)});
  }
  return node;
}

YamlNode yaml_parse_block(YamlLineCursor& cur, int indent) {
  while (cur.idx < cur.lines.size() && trim_copy(cur.lines[cur.idx]).empty()) ++cur.idx;
  if (cur.idx >= cur.lines.size()) {
    YamlNode node; node.kind = YamlNode::Kind::Map; return node;
  }
  const auto line = cur.lines[cur.idx];
  const auto line_indent = static_cast<int>(count_indent(line));
  if (line_indent < indent) {
    YamlNode node; node.kind = YamlNode::Kind::Map; return node;
  }
  const auto trimmed = line.substr(static_cast<std::size_t>(line_indent));
  if (starts_with(trimmed, "- ")) return yaml_parse_array(cur, line_indent);
  return yaml_parse_map(cur, line_indent);
}

YamlNode parse_yaml(const std::string& text) {
  std::vector<std::string> lines;
  std::stringstream ss(text);
  std::string line;
  while (std::getline(ss, line)) lines.push_back(line);
  YamlLineCursor cursor{lines, 0};
  return yaml_parse_block(cursor, 0);
}

std::string yaml_serialize(const YamlNode& node, int indent = 0) {
  std::ostringstream oss;
  const auto ind = std::string(static_cast<std::size_t>(indent), ' ');
  if (node.kind == YamlNode::Kind::Scalar) {
    oss << node.scalar;
  } else if (node.kind == YamlNode::Kind::Map) {
    for (std::size_t i = 0; i < node.map_items.size(); ++i) {
      const auto& [key, value] = node.map_items[i];
      if (value.kind == YamlNode::Kind::Scalar) {
        oss << ind << key << ": " << value.scalar;
      } else {
        oss << ind << key << ":\n" << yaml_serialize(value, indent + 2);
      }
      if (i + 1 < node.map_items.size()) oss << "\n";
    }
  } else {
    for (std::size_t i = 0; i < node.array_items.size(); ++i) {
      const auto& value = node.array_items[i];
      if (value.kind == YamlNode::Kind::Scalar) {
        oss << ind << "- " << value.scalar;
      } else {
        oss << ind << "-";
        if (value.kind == YamlNode::Kind::Map && !value.map_items.empty() && value.map_items[0].second.kind == YamlNode::Kind::Scalar && value.map_items.size() == 1) {
          oss << " " << value.map_items[0].first << ": " << value.map_items[0].second.scalar;
        } else {
          oss << "\n" << yaml_serialize(value, indent + 2);
        }
      }
      if (i + 1 < node.array_items.size()) oss << "\n";
    }
  }
  return oss.str();
}

YamlNode* yaml_map_get(YamlNode& node, const std::string& key) {
  if (node.kind != YamlNode::Kind::Map) return nullptr;
  for (auto& [name, value] : node.map_items) if (name == key) return &value;
  return nullptr;
}

YamlNode* yaml_ensure_map_path(YamlNode& root, const std::vector<std::string>& parts, bool include_last_container = true) {
  if (root.kind != YamlNode::Kind::Map) root.kind = YamlNode::Kind::Map, root.map_items.clear(), root.array_items.clear(), root.scalar.clear();
  YamlNode* cur = &root;
  std::size_t limit = include_last_container ? parts.size() : (parts.empty() ? 0 : parts.size() - 1);
  for (std::size_t i = 0; i < limit; ++i) {
    auto* next = yaml_map_get(*cur, parts[i]);
    if (!next) {
      cur->map_items.push_back({parts[i], YamlNode{YamlNode::Kind::Map}});
      next = &cur->map_items.back().second;
    }
    if (next->kind != YamlNode::Kind::Map) next->kind = YamlNode::Kind::Map, next->map_items.clear(), next->array_items.clear(), next->scalar.clear();
    cur = next;
  }
  return cur;
}

YamlNode* yaml_find_path(YamlNode& root, const std::vector<std::string>& parts) {
  YamlNode* cur = &root;
  for (const auto& part : parts) {
    cur = yaml_map_get(*cur, part);
    if (!cur) return nullptr;
  }
  return cur;
}

std::string first_tag_name(const std::string& open_tag) {
  auto start = open_tag.find('<');
  if (start == std::string::npos) return {};
  ++start;
  while (start < open_tag.size() && (open_tag[start] == '/' || std::isspace(static_cast<unsigned char>(open_tag[start])))) ++start;
  std::size_t end = start;
  while (end < open_tag.size() && (std::isalnum(static_cast<unsigned char>(open_tag[end])) || open_tag[end] == '-' || open_tag[end] == '_')) ++end;
  return open_tag.substr(start, end - start);
}

std::optional<std::pair<std::size_t, std::size_t>> find_html_node_range(const std::string& html,
                                                                        const std::string& selector,
                                                                        std::size_t* open_tag_end = nullptr) {
  std::size_t open = std::string::npos;
  if (!selector.empty() && selector[0] == '<') open = html.find(selector);
  if (open == std::string::npos) open = html.find("<" + selector);
  if (open == std::string::npos) return std::nullopt;
  auto gt = html.find('>', open);
  if (gt == std::string::npos) return std::nullopt;
  if (open_tag_end) *open_tag_end = gt + 1;
  const auto open_tag = html.substr(open, gt - open + 1);
  if (open_tag.size() >= 2 && open_tag[open_tag.size() - 2] == '/') return std::make_pair(open, gt + 1);
  const auto tag = first_tag_name(open_tag);
  if (tag.empty()) return std::nullopt;
  std::size_t pos = open;
  int depth = 0;
  while (pos < html.size()) {
    auto next_open = html.find("<" + tag, pos);
    auto next_close = html.find("</" + tag, pos);
    if (next_open == std::string::npos && next_close == std::string::npos) break;
    if (next_open != std::string::npos && (next_open < next_close || next_close == std::string::npos)) {
      auto end = html.find('>', next_open);
      if (end == std::string::npos) break;
      ++depth;
      pos = end + 1;
      continue;
    }
    auto end = html.find('>', next_close);
    if (end == std::string::npos) break;
    --depth;
    pos = end + 1;
    if (depth == 0) return std::make_pair(open, pos);
  }
  return std::nullopt;
}

} // namespace

SessionMutationResult markdown_replace_section(const WorkspaceConfig& workspace,
                                              const std::string& session_id,
                                              const std::string& path,
                                              const std::string& heading,
                                              std::size_t heading_level,
                                              const std::string& new_content) {
  std::string error;
  const auto current = slurp_effective_file(workspace, session_id, path, &error);
  if (!error.empty()) { SessionMutationResult out; out.error = error; return out; }
  std::size_t body_start = 0;
  const auto range = find_markdown_section_range(current, heading, heading_level, &body_start);
  if (!range) { SessionMutationResult out; out.error = "markdown heading not found"; return out; }
  auto next = current;
  next.replace(body_start, range->second - body_start, markdown_normalize_body(new_content));
  return stage_whole_file_change(workspace, session_id, path, current, next, "markdown_replace_section",
                                 "markdown section replace heading=" + heading, heading);
}

SessionMutationResult markdown_insert_after_heading(const WorkspaceConfig& workspace,
                                                    const std::string& session_id,
                                                    const std::string& path,
                                                    const std::string& heading,
                                                    std::size_t heading_level,
                                                    const std::string& new_content) {
  std::string error;
  const auto current = slurp_effective_file(workspace, session_id, path, &error);
  if (!error.empty()) { SessionMutationResult out; out.error = error; return out; }
  std::size_t body_start = 0;
  const auto range = find_markdown_section_range(current, heading, heading_level, &body_start);
  if (!range) { SessionMutationResult out; out.error = "markdown heading not found"; return out; }
  auto insert = markdown_normalize_body(new_content);
  auto next = current;
  next.insert(body_start, insert);
  return stage_whole_file_change(workspace, session_id, path, current, next, "markdown_insert_after_heading",
                                 "markdown insert after heading=" + heading, heading);
}

SessionMutationResult markdown_upsert_section(const WorkspaceConfig& workspace,
                                              const std::string& session_id,
                                              const std::string& path,
                                              const std::string& heading,
                                              std::size_t heading_level,
                                              const std::string& new_content) {
  std::string error;
  const auto current = slurp_effective_file(workspace, session_id, path, &error);
  if (!error.empty()) { SessionMutationResult out; out.error = error; return out; }
  std::size_t body_start = 0;
  const auto range = find_markdown_section_range(current, heading, heading_level, &body_start);
  if (range) {
    auto next = current;
    next.replace(body_start, range->second - body_start, markdown_normalize_body(new_content));
    return stage_whole_file_change(workspace, session_id, path, current, next, "markdown_upsert_section",
                                   "markdown upsert existing heading=" + heading, heading);
  }
  std::ostringstream append;
  append << current;
  if (!current.empty() && current.back() != '\n') append << '\n';
  append << std::string(heading_level, '#') << " " << heading << "\n" << markdown_normalize_body(new_content);
  return stage_whole_file_change(workspace, session_id, path, current, append.str(), "markdown_upsert_section",
                                 "markdown upsert appended heading=" + heading, heading);
}

SessionMutationResult json_replace_value(const WorkspaceConfig& workspace,
                                         const std::string& session_id,
                                         const std::string& path,
                                         const std::string& key_path,
                                         const std::string& new_json_value) {
  try {
    std::string error;
    const auto current = slurp_effective_file(workspace, session_id, path, &error);
    if (!error.empty()) { SessionMutationResult out; out.error = error; return out; }
    auto root = parse_json(current);
    auto value = parse_json(new_json_value);
    auto parts = split_path(key_path);
    auto* target = json_find_path(root, parts);
    if (!target) { SessionMutationResult out; out.error = "json key path not found"; return out; }
    *target = std::move(value);
    return stage_whole_file_change(workspace, session_id, path, current, json_serialize(root) + "\n",
                                   "json_replace_value", "json replace key_path=" + key_path, key_path);
  } catch (const std::exception& ex) {
    SessionMutationResult out; out.error = ex.what(); return out;
  }
}

SessionMutationResult json_upsert_key(const WorkspaceConfig& workspace,
                                      const std::string& session_id,
                                      const std::string& path,
                                      const std::string& key_path,
                                      const std::string& new_json_value) {
  try {
    std::string error;
    const auto current = slurp_effective_file(workspace, session_id, path, &error);
    if (!error.empty()) { SessionMutationResult out; out.error = error; return out; }
    auto root = parse_json(current);
    auto value = parse_json(new_json_value);
    auto parts = split_path(key_path);
    if (parts.empty()) { SessionMutationResult out; out.error = "json key path is empty"; return out; }
    auto* parent = json_ensure_object_path(root, parts, false);
    auto* existing = json_object_get(*parent, parts.back());
    if (existing) *existing = std::move(value);
    else parent->object_items.push_back({parts.back(), std::move(value)});
    return stage_whole_file_change(workspace, session_id, path, current, json_serialize(root) + "\n",
                                   "json_upsert_key", "json upsert key_path=" + key_path, key_path);
  } catch (const std::exception& ex) {
    SessionMutationResult out; out.error = ex.what(); return out;
  }
}

SessionMutationResult json_append_array_item(const WorkspaceConfig& workspace,
                                             const std::string& session_id,
                                             const std::string& path,
                                             const std::string& key_path,
                                             const std::string& new_json_value) {
  try {
    std::string error;
    const auto current = slurp_effective_file(workspace, session_id, path, &error);
    if (!error.empty()) { SessionMutationResult out; out.error = error; return out; }
    auto root = parse_json(current);
    auto value = parse_json(new_json_value);
    auto parts = split_path(key_path);
    auto* target = json_find_path(root, parts);
    if (!target) {
      auto* parent = json_ensure_object_path(root, parts, false);
      parent->object_items.push_back({parts.back(), JsonNode{JsonNode::Kind::Array}});
      target = &parent->object_items.back().second;
    }
    if (target->kind != JsonNode::Kind::Array) { SessionMutationResult out; out.error = "json target is not an array"; return out; }
    target->array_items.push_back(std::move(value));
    return stage_whole_file_change(workspace, session_id, path, current, json_serialize(root) + "\n",
                                   "json_append_array_item", "json append array key_path=" + key_path, key_path);
  } catch (const std::exception& ex) {
    SessionMutationResult out; out.error = ex.what(); return out;
  }
}

SessionMutationResult yaml_replace_value(const WorkspaceConfig& workspace,
                                         const std::string& session_id,
                                         const std::string& path,
                                         const std::string& key_path,
                                         const std::string& new_yaml_value) {
  try {
    std::string error;
    const auto current = slurp_effective_file(workspace, session_id, path, &error);
    if (!error.empty()) { SessionMutationResult out; out.error = error; return out; }
    auto root = parse_yaml(current);
    auto parts = split_path(key_path);
    auto* target = yaml_find_path(root, parts);
    if (!target) { SessionMutationResult out; out.error = "yaml key path not found"; return out; }
    target->kind = YamlNode::Kind::Scalar;
    target->scalar = trim_copy(new_yaml_value);
    target->map_items.clear();
    target->array_items.clear();
    return stage_whole_file_change(workspace, session_id, path, current, yaml_serialize(root) + "\n",
                                   "yaml_replace_value", "yaml replace key_path=" + key_path, key_path);
  } catch (const std::exception& ex) {
    SessionMutationResult out; out.error = ex.what(); return out;
  }
}

SessionMutationResult yaml_upsert_key(const WorkspaceConfig& workspace,
                                      const std::string& session_id,
                                      const std::string& path,
                                      const std::string& key_path,
                                      const std::string& new_yaml_value) {
  try {
    std::string error;
    const auto current = slurp_effective_file(workspace, session_id, path, &error);
    if (!error.empty()) { SessionMutationResult out; out.error = error; return out; }
    auto root = parse_yaml(current);
    auto parts = split_path(key_path);
    if (parts.empty()) { SessionMutationResult out; out.error = "yaml key path is empty"; return out; }
    auto* parent = yaml_ensure_map_path(root, parts, false);
    auto* existing = yaml_map_get(*parent, parts.back());
    if (!existing) {
      parent->map_items.push_back({parts.back(), YamlNode{YamlNode::Kind::Scalar}});
      existing = &parent->map_items.back().second;
    }
    existing->kind = YamlNode::Kind::Scalar;
    existing->scalar = trim_copy(new_yaml_value);
    existing->map_items.clear();
    existing->array_items.clear();
    return stage_whole_file_change(workspace, session_id, path, current, yaml_serialize(root) + "\n",
                                   "yaml_upsert_key", "yaml upsert key_path=" + key_path, key_path);
  } catch (const std::exception& ex) {
    SessionMutationResult out; out.error = ex.what(); return out;
  }
}

SessionMutationResult yaml_append_item(const WorkspaceConfig& workspace,
                                       const std::string& session_id,
                                       const std::string& path,
                                       const std::string& key_path,
                                       const std::string& new_yaml_value) {
  try {
    std::string error;
    const auto current = slurp_effective_file(workspace, session_id, path, &error);
    if (!error.empty()) { SessionMutationResult out; out.error = error; return out; }
    auto root = parse_yaml(current);
    auto parts = split_path(key_path);
    auto* target = yaml_find_path(root, parts);
    if (!target) {
      auto* parent = yaml_ensure_map_path(root, parts, false);
      parent->map_items.push_back({parts.back(), YamlNode{YamlNode::Kind::Array}});
      target = &parent->map_items.back().second;
    }
    if (target->kind != YamlNode::Kind::Array) { SessionMutationResult out; out.error = "yaml target is not a list"; return out; }
    YamlNode item; item.kind = YamlNode::Kind::Scalar; item.scalar = trim_copy(new_yaml_value);
    target->array_items.push_back(std::move(item));
    return stage_whole_file_change(workspace, session_id, path, current, yaml_serialize(root) + "\n",
                                   "yaml_append_item", "yaml append key_path=" + key_path, key_path);
  } catch (const std::exception& ex) {
    SessionMutationResult out; out.error = ex.what(); return out;
  }
}

SessionMutationResult html_replace_node(const WorkspaceConfig& workspace,
                                        const std::string& session_id,
                                        const std::string& path,
                                        const std::string& selector,
                                        const std::string& new_html) {
  std::string error;
  const auto current = slurp_effective_file(workspace, session_id, path, &error);
  if (!error.empty()) { SessionMutationResult out; out.error = error; return out; }
  const auto range = find_html_node_range(current, selector);
  if (!range) { SessionMutationResult out; out.error = "html selector not found"; return out; }
  auto next = current;
  next.replace(range->first, range->second - range->first, new_html);
  return stage_whole_file_change(workspace, session_id, path, current, next,
                                 "html_replace_node", "html replace selector=" + selector, selector);
}

SessionMutationResult html_insert_after_node(const WorkspaceConfig& workspace,
                                             const std::string& session_id,
                                             const std::string& path,
                                             const std::string& selector,
                                             const std::string& new_html) {
  std::string error;
  const auto current = slurp_effective_file(workspace, session_id, path, &error);
  if (!error.empty()) { SessionMutationResult out; out.error = error; return out; }
  const auto range = find_html_node_range(current, selector);
  if (!range) { SessionMutationResult out; out.error = "html selector not found"; return out; }
  auto next = current;
  next.insert(range->second, new_html);
  return stage_whole_file_change(workspace, session_id, path, current, next,
                                 "html_insert_after_node", "html insert after selector=" + selector, selector);
}

SessionMutationResult html_set_attribute(const WorkspaceConfig& workspace,
                                         const std::string& session_id,
                                         const std::string& path,
                                         const std::string& selector,
                                         const std::string& attribute_name,
                                         const std::string& attribute_value) {
  std::string error;
  const auto current = slurp_effective_file(workspace, session_id, path, &error);
  if (!error.empty()) { SessionMutationResult out; out.error = error; return out; }
  std::size_t open_tag_end = 0;
  const auto range = find_html_node_range(current, selector, &open_tag_end);
  if (!range) { SessionMutationResult out; out.error = "html selector not found"; return out; }
  auto next = current;
  auto open_tag = next.substr(range->first, open_tag_end - range->first);
  const std::string needle = attribute_name + "=";
  const auto attr_pos = open_tag.find(needle);
  if (attr_pos != std::string::npos) {
    auto quote = open_tag.find('"', attr_pos + needle.size());
    if (quote == std::string::npos) { SessionMutationResult out; out.error = "html attribute is malformed"; return out; }
    auto quote_end = open_tag.find('"', quote + 1);
    if (quote_end == std::string::npos) { SessionMutationResult out; out.error = "html attribute is malformed"; return out; }
    open_tag.replace(quote + 1, quote_end - quote - 1, attribute_value);
  } else {
    auto insert_at = open_tag.rfind('>');
    if (insert_at == std::string::npos) { SessionMutationResult out; out.error = "html opening tag is malformed"; return out; }
    const bool self_closing = insert_at > 0 && open_tag[insert_at - 1] == '/';
    if (self_closing) --insert_at;
    open_tag.insert(insert_at, " " + attribute_name + "=\"" + attribute_value + "\"");
  }
  next.replace(range->first, open_tag_end - range->first, open_tag);
  return stage_whole_file_change(workspace, session_id, path, current, next,
                                 "html_set_attribute", "html set attribute selector=" + selector + " attr=" + attribute_name, selector);
}

} // namespace bridge::core
