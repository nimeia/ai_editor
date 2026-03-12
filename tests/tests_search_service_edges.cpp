#include "bridge/core/search_service.hpp"
#include "bridge/core/workspace.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main() {
  const fs::path root = fs::temp_directory_path() / "ai_bridge_search_service_edges_test";
  fs::remove_all(root);
  fs::create_directories(root / "docs");
  fs::create_directories(root / "bin");

  std::ofstream(root / "docs" / "story.txt", std::ios::binary)
      << "before-1\n"
      << "before-2\n"
      << "needle here\n"
      << "after-1\n"
      << "after-2\n"
      << "after-3\n";
  std::ofstream(root / "docs" / "many.txt", std::ios::binary)
      << "needle one\n"
      << "needle two\n"
      << "needle three\n";
  std::ofstream(root / "docs" / "other.log", std::ios::binary) << "needle in log\n";
  std::ofstream(root / "bin" / "blob.bin", std::ios::binary) << std::string("abc\0needle\0xyz", 14);

  auto cfg = bridge::core::make_default_workspace_config(root.string());

  bridge::core::SearchOptions context_opts;
  context_opts.root_path = "docs/story.txt";
  context_opts.context_before = 1;
  context_opts.context_after = 1;
  auto context = bridge::core::search_text(cfg, "needle", context_opts);
  assert(context.ok);
  assert(context.matches.size() == 1);
  assert(context.matches[0].path == "docs/story.txt");
  assert(context.matches[0].line_start == 3);
  assert(context.matches[0].snippet == "before-2\nneedle here\nafter-1");

  bridge::core::SearchOptions truncated_opts;
  truncated_opts.root_path = "docs";
  truncated_opts.max_results = 2;
  auto truncated = bridge::core::search_text(cfg, "needle", truncated_opts);
  assert(truncated.ok);
  assert(truncated.matches.size() == 2);
  assert(truncated.truncated);

  bridge::core::SearchOptions per_file_opts;
  per_file_opts.root_path = "docs/many.txt";
  per_file_opts.max_results = 10;
  per_file_opts.max_matches_per_file = 1;
  auto per_file = bridge::core::search_text(cfg, "needle", per_file_opts);
  assert(per_file.ok);
  assert(per_file.matches.size() == 1);
  assert(per_file.matches[0].line_start == 1);

  bridge::core::SearchOptions ext_opts;
  ext_opts.root_path = "docs";
  ext_opts.extensions = {".txt"};
  auto ext_filtered = bridge::core::search_text(cfg, "needle", ext_opts);
  assert(ext_filtered.ok);
  for (const auto& match : ext_filtered.matches) {
    assert(match.path.find(".log") == std::string::npos);
  }

  bridge::core::SearchOptions binary_opts;
  binary_opts.root_path = ".";
  auto binary_skipped = bridge::core::search_text(cfg, "needle", binary_opts);
  assert(binary_skipped.ok);
  for (const auto& match : binary_skipped.matches) {
    assert(match.path != "bin/blob.bin");
  }

  auto empty_query = bridge::core::search_text(cfg, "", {});
  assert(!empty_query.ok);
  assert(empty_query.error == "query is empty");

  auto empty_pattern = bridge::core::search_regex(cfg, "", {});
  assert(!empty_pattern.ok);
  assert(empty_pattern.error == "pattern is empty");

  auto bad_regex = bridge::core::search_regex(cfg, "(", {});
  assert(!bad_regex.ok);
  assert(!bad_regex.error.empty());

  fs::remove_all(root);
  return 0;
}
