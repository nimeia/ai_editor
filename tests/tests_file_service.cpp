#include "bridge/core/file_service.hpp"
#include "bridge/core/workspace.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
  const fs::path root = fs::temp_directory_path() / "ai_bridge_file_service_test";
  fs::remove_all(root);
  fs::create_directories(root / "docs");
  fs::create_directories(root / "node_modules" / "pkg");
  std::ofstream(root / "docs" / "readme.md") << "line1\nline2\nline3\n";
  std::ofstream(root / "node_modules" / "pkg" / "index.js") << "module.exports = 1;\n";

  auto cfg = bridge::core::make_default_workspace_config(root.string());

  auto stat = bridge::core::fs_stat(cfg, "docs/readme.md");
  assert(stat.ok);
  assert(stat.kind == "file");
  assert(!stat.binary);

  auto read = bridge::core::fs_read(cfg, "docs/readme.md");
  assert(read.ok);
  assert(read.line_count == 3);
  assert(read.content.find("line2") != std::string::npos);

  auto range = bridge::core::fs_read_range(cfg, "docs/readme.md", 2, 3);
  assert(range.ok);
  assert(range.content == "line2\nline3");

  auto list_root = bridge::core::fs_list(cfg, ".");
  assert(list_root.ok);
  bool saw_docs = false;
  bool saw_modules = false;
  for (const auto& e : list_root.entries) {
    if (e.path == "docs") saw_docs = true;
    if (e.path == "node_modules") saw_modules = true;
  }
  assert(saw_docs);
  assert(saw_modules);

  auto list_recursive = bridge::core::fs_list(cfg, ".", {.recursive = true, .include_excluded = false, .max_results = 200});
  assert(list_recursive.ok);
  bool saw_excluded_child = false;
  for (const auto& e : list_recursive.entries) {
    if (e.path.find("node_modules/pkg/index.js") != std::string::npos) saw_excluded_child = true;
  }
  assert(!saw_excluded_child);

  auto read_excluded = bridge::core::fs_read(cfg, "node_modules/pkg/index.js");
  assert(read_excluded.ok);
  assert(read_excluded.policy == bridge::core::PathPolicyKind::SkipByDefault);

  fs::remove_all(root);
  return 0;
}
