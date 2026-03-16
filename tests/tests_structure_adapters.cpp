#include "bridge/core/session_service.hpp"
#include "bridge/core/structure_adapters.hpp"
#include "bridge/core/workspace.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static std::string slurp(const fs::path& path) {
  std::ifstream ifs(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

int main() {
  const fs::path root = fs::temp_directory_path() / "ai_bridge_structure_adapters_test";
  fs::remove_all(root);
  fs::create_directories(root / "docs");
  fs::create_directories(root / "cfg");
  fs::create_directories(root / "web");

  std::ofstream(root / "docs" / "guide.md", std::ios::binary)
      << "# Title\n\n## Intro\nold intro\n\n## Notes\nold notes\n";
  std::ofstream(root / "cfg" / "settings.json", std::ios::binary)
      << "{\n  \"name\": \"demo\",\n  \"features\": {\n    \"enabled\": false\n  },\n  \"items\": [\n    1\n  ]\n}\n";
  std::ofstream(root / "cfg" / "settings.yaml", std::ios::binary)
      << "app:\n  name: demo\n  flags:\n    - alpha\n";
  std::ofstream(root / "web" / "index.html", std::ios::binary)
      << "<html><body><div id=\"app\"><p>Hello</p></div></body></html>";

  auto cfg = bridge::core::make_default_workspace_config(root.string());

  auto md_begin = bridge::core::session_begin(cfg, "sess-md");
  assert(md_begin.ok);
  auto md_replace = bridge::core::markdown_replace_section(cfg, "sess-md", "docs/guide.md", "Intro", 2, "new intro\nline 2\n");
  assert(md_replace.ok);
  auto md_insert = bridge::core::markdown_insert_after_heading(cfg, "sess-md", "docs/guide.md", "Notes", 2, "inserted note\n");
  assert(md_insert.ok);
  auto md_upsert = bridge::core::markdown_upsert_section(cfg, "sess-md", "docs/guide.md", "Appendix", 2, "extra body\n");
  assert(md_upsert.ok);
  auto md_commit = bridge::core::session_commit(cfg, "sess-md", "cli", "req-md");
  assert(md_commit.ok);
  const auto md_text = slurp(root / "docs" / "guide.md");
  assert(md_text.find("new intro") != std::string::npos);
  assert(md_text.find("inserted note") != std::string::npos);
  assert(md_text.find("## Appendix") != std::string::npos);

  auto json_begin = bridge::core::session_begin(cfg, "sess-json");
  assert(json_begin.ok);
  auto json_replace = bridge::core::json_replace_value(cfg, "sess-json", "cfg/settings.json", "features.enabled", "true");
  assert(json_replace.ok);
  auto json_upsert = bridge::core::json_upsert_key(cfg, "sess-json", "cfg/settings.json", "features.mode", "\"fast\"");
  assert(json_upsert.ok);
  auto json_append = bridge::core::json_append_array_item(cfg, "sess-json", "cfg/settings.json", "items", "2");
  assert(json_append.ok);
  auto json_preview = bridge::core::session_preview(cfg, "sess-json");
  assert(json_preview.ok);
  assert(json_preview.files[0].diff.find("fast") != std::string::npos);
  auto json_commit = bridge::core::session_commit(cfg, "sess-json", "cli", "req-json");
  assert(json_commit.ok);
  const auto json_text = slurp(root / "cfg" / "settings.json");
  assert(json_text.find("\"enabled\": true") != std::string::npos);
  assert(json_text.find("\"mode\": \"fast\"") != std::string::npos);
  assert(json_text.find("2") != std::string::npos);

  auto yaml_begin = bridge::core::session_begin(cfg, "sess-yaml");
  assert(yaml_begin.ok);
  auto yaml_replace = bridge::core::yaml_replace_value(cfg, "sess-yaml", "cfg/settings.yaml", "app.name", "demo2");
  assert(yaml_replace.ok);
  auto yaml_upsert = bridge::core::yaml_upsert_key(cfg, "sess-yaml", "cfg/settings.yaml", "app.mode", "safe");
  assert(yaml_upsert.ok);
  auto yaml_append = bridge::core::yaml_append_item(cfg, "sess-yaml", "cfg/settings.yaml", "app.flags", "beta");
  assert(yaml_append.ok);
  auto yaml_commit = bridge::core::session_commit(cfg, "sess-yaml", "cli", "req-yaml");
  assert(yaml_commit.ok);
  const auto yaml_text = slurp(root / "cfg" / "settings.yaml");
  assert(yaml_text.find("name: demo2") != std::string::npos);
  assert(yaml_text.find("mode: safe") != std::string::npos);
  assert(yaml_text.find("- beta") != std::string::npos);

  auto html_begin = bridge::core::session_begin(cfg, "sess-html");
  assert(html_begin.ok);
  auto html_replace = bridge::core::html_replace_node(cfg, "sess-html", "web/index.html", "div id=\"app\"", "<section id=\"app\"><p>World</p></section>");
  assert(html_replace.ok);
  auto html_insert = bridge::core::html_insert_after_node(cfg, "sess-html", "web/index.html", "section id=\"app\"", "<footer>Tail</footer>");
  assert(html_insert.ok);
  auto html_attr = bridge::core::html_set_attribute(cfg, "sess-html", "web/index.html", "section id=\"app\"", "data-mode", "active");
  assert(html_attr.ok);
  auto html_commit = bridge::core::session_commit(cfg, "sess-html", "cli", "req-html");
  assert(html_commit.ok);
  const auto html_text = slurp(root / "web" / "index.html");
  assert(html_text.find("<section id=\"app\" data-mode=\"active\">") != std::string::npos ||
         html_text.find("<section data-mode=\"active\" id=\"app\">") != std::string::npos);
  assert(html_text.find("<footer>Tail</footer>") != std::string::npos);

  fs::remove_all(root);
  return 0;
}
