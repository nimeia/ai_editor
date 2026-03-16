#include "bridge/core/search_service.hpp"
#include "bridge/core/workspace.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
  const fs::path root = fs::temp_directory_path() / "ai_bridge_search_service_test";
  fs::remove_all(root);
  fs::create_directories(root / "docs");
  fs::create_directories(root / "node_modules" / "pkg");
  std::ofstream(root / "docs" / "readme.md") << "alpha\nbeta token\ngamma\n";
  std::ofstream(root / "docs" / "code.cpp") << "int main() {}\n// token here\n";
  std::ofstream(root / "node_modules" / "pkg" / "index.js") << "console.log('token');\n";

  auto cfg = bridge::core::make_default_workspace_config(root.string());

  bridge::core::SearchOptions opts;
  opts.root_path = ".";
  opts.extensions = {".md", ".cpp"};
  opts.max_results = 10;
  auto text = bridge::core::search_text(cfg, "token", opts);
  assert(text.ok);
  assert(text.matches.size() == 2);
  assert(text.scanned_files >= 2);
  assert(!text.matches[0].match_id.empty());
  assert(!text.matches[0].anchor.empty());
  assert(text.matches[0].selector_reason == "literal-query");
  assert(text.matches[0].confidence > 0.0);

  auto regex = bridge::core::search_regex(cfg, "main\\s*\\(", opts);
  assert(regex.ok);
  assert(regex.matches.size() == 1);
  assert(regex.matches[0].path == "docs/code.cpp");

  bridge::core::SearchOptions excluded_opts;
  excluded_opts.root_path = ".";
  excluded_opts.include_excluded = true;
  excluded_opts.max_results = 10;
  auto excluded = bridge::core::search_text(cfg, "token", excluded_opts);
  assert(excluded.ok);
  bool saw_excluded = false;
  for (const auto& m : excluded.matches) {
    if (m.path == "node_modules/pkg/index.js") saw_excluded = true;
  }
  assert(saw_excluded);


  bridge::core::SearchOptions filtered_opts;
  filtered_opts.root_path = ".";
  filtered_opts.exact_path = "docs/readme.md";
  filtered_opts.min_line = 2;
  filtered_opts.max_line = 2;
  auto filtered = bridge::core::search_text(cfg, "token", filtered_opts);
  assert(filtered.ok);
  assert(filtered.matches.size() == 1);
  assert(filtered.matches[0].path == "docs/readme.md");
  assert(filtered.matches[0].line_start == 2);

  bool cancelled = false;
  bridge::core::SearchOptions cancel_opts;
  cancel_opts.root_path = ".";
  cancel_opts.cancel_requested = [&]() { return cancelled; };
  cancelled = true;
  auto cancelled_result = bridge::core::search_text(cfg, "token", cancel_opts);
  assert(!cancelled_result.ok);
  assert(cancelled_result.cancelled);
  assert(cancelled_result.error == "request cancelled");

  fs::remove_all(root);
  return 0;
}
