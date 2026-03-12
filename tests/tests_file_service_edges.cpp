#include "bridge/core/file_service.hpp"
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
  const fs::path root = fs::temp_directory_path() / "ai_bridge_file_service_edges_test";
  fs::remove_all(root);
  fs::create_directories(root / "docs");
  fs::create_directories(root / "docs" / "nested");
  std::ofstream(root / "docs" / "a.txt", std::ios::binary) << "1234567890\nline2\n";
  std::ofstream(root / "docs" / "b.txt", std::ios::binary) << "B\n";
  std::ofstream(root / "docs" / "empty.txt", std::ios::binary);
  std::ofstream(root / "docs" / "nested" / "c.txt", std::ios::binary) << "C\n";

  auto cfg = bridge::core::make_default_workspace_config(root.string());

  bridge::core::FsListOptions list_opts;
  list_opts.recursive = false;
  list_opts.include_excluded = false;
  list_opts.max_results = 2;
  auto list_truncated = bridge::core::fs_list(cfg, "docs", list_opts);
  assert(list_truncated.ok);
  assert(list_truncated.entries.size() == 2);
  assert(list_truncated.truncated);

  auto list_file_error = bridge::core::fs_list(cfg, "docs/a.txt");
  assert(!list_file_error.ok);
  assert(list_file_error.error == "path is not a directory");

  auto stat_dir = bridge::core::fs_stat(cfg, "docs");
  assert(stat_dir.ok);
  assert(stat_dir.kind == "directory");
  assert(stat_dir.size == 0);
  assert(stat_dir.encoding.empty());
  assert(!stat_dir.binary);

  auto read_truncated = bridge::core::fs_read(cfg, "docs/a.txt", 5);
  assert(read_truncated.ok);
  assert(read_truncated.truncated);
  assert(read_truncated.content == "12345");
  assert(read_truncated.line_count == 1);

  auto read_empty = bridge::core::fs_read(cfg, "docs/empty.txt");
  assert(read_empty.ok);
  assert(read_empty.content.empty());
  assert(read_empty.line_count == 0);
  assert(!read_empty.truncated);

  auto range_invalid = bridge::core::fs_read_range(cfg, "docs/a.txt", 0, 1);
  assert(!range_invalid.ok);
  assert(range_invalid.error == "invalid line range");

  auto range_partial = bridge::core::fs_read_range(cfg, "docs/a.txt", 2, 20);
  assert(range_partial.ok);
  assert(range_partial.content == "line2");
  assert(range_partial.line_count == 1);

  auto range_empty = bridge::core::fs_read_range(cfg, "docs/a.txt", 20, 21);
  assert(range_empty.ok);
  assert(range_empty.content.empty());
  assert(range_empty.line_count == 0);

  auto missing_read = bridge::core::fs_read(cfg, "docs/missing.txt");
  assert(!missing_read.ok);
  assert(missing_read.error == "path not found");

  auto mkdir_created = bridge::core::fs_mkdir(cfg, "docs/generated/nested");
  assert(mkdir_created.ok);
  assert(mkdir_created.created);
  assert(fs::is_directory(root / "docs" / "generated" / "nested"));

  auto mkdir_existing = bridge::core::fs_mkdir(cfg, "docs/generated");
  assert(mkdir_existing.ok);
  assert(!mkdir_existing.created);

  auto write_created = bridge::core::fs_write(cfg, "docs/generated/nested/new.txt", "hello\nworld\n");
  assert(write_created.ok);
  assert(write_created.created);
  assert(write_created.parent_created == false);
  assert(slurp(root / "docs" / "generated" / "nested" / "new.txt") == "hello\nworld\n");

  auto write_overwrite = bridge::core::fs_write(cfg, "docs/generated/nested/new.txt", "updated\n");
  assert(write_overwrite.ok);
  assert(!write_overwrite.created);
  assert(slurp(root / "docs" / "generated" / "nested" / "new.txt") == "updated\n");

  bridge::core::FsWriteOptions no_overwrite;
  no_overwrite.overwrite = false;
  auto write_exists_error = bridge::core::fs_write(cfg, "docs/generated/nested/new.txt", "again\n", no_overwrite);
  assert(!write_exists_error.ok);
  assert(write_exists_error.error == "path already exists");

  fs::remove_all(root);
  return 0;
}
