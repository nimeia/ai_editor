#include "bridge/core/patch_service.hpp"
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
  const fs::path root = fs::temp_directory_path() / "ai_bridge_patch_service_edges_test";
  fs::remove_all(root);
  fs::create_directories(root / "docs");
  std::ofstream(root / "docs" / "note.txt", std::ios::binary) << "one\ntwo\nthree\n";

  auto cfg = bridge::core::make_default_workspace_config(root.string());

  auto state = bridge::core::patch_preview(cfg, "docs/note.txt", "one\nTWO\nthree\n");
  assert(state.ok);

  auto wrong_path = bridge::core::patch_apply(cfg,
                                              "docs/other.txt",
                                              "",
                                              {},
                                              "cli-001",
                                              "sess-001",
                                              "req-preview-wrong-path",
                                              state.preview_id);
  assert(!wrong_path.ok);
  assert(wrong_path.error == "preview path mismatch");

  auto wrong_content = bridge::core::patch_apply(cfg,
                                                 "docs/note.txt",
                                                 "different\n",
                                                 {},
                                                 "cli-001",
                                                 "sess-001",
                                                 "req-preview-wrong-content",
                                                 state.preview_id);
  assert(!wrong_content.ok);
  assert(wrong_content.error == "preview content mismatch");

  auto wrong_base = bridge::core::patch_apply(cfg,
                                              "docs/note.txt",
                                              "",
                                              {.mtime = "mismatch", .hash = state.current_hash},
                                              "cli-001",
                                              "sess-001",
                                              "req-preview-wrong-base",
                                              state.preview_id);
  assert(!wrong_base.ok);
  assert(wrong_base.error == "preview base mismatch");

  auto direct_base = bridge::core::patch_preview(cfg, "docs/note.txt", "one\nTWO\nthree\n");
  assert(direct_base.ok);
  bridge::core::PatchBase apply_base{direct_base.current_mtime, direct_base.current_hash};

  auto direct_apply = bridge::core::patch_apply(cfg,
                                                "docs/note.txt",
                                                "one\nTWO\nthree\n",
                                                apply_base,
                                                "cli-001",
                                                "sess-001",
                                                "req-direct-apply");
  assert(direct_apply.ok);
  assert(direct_apply.applied);
  assert(!direct_apply.backup_id.empty());
  assert(slurp(root / "docs" / "note.txt") == "one\nTWO\nthree\n");

  auto missing_backup = bridge::core::patch_rollback(cfg,
                                                     "docs/note.txt",
                                                     "missing-backup",
                                                     "cli-001",
                                                     "sess-001",
                                                     "req-missing-backup");
  assert(!missing_backup.ok);
  assert(missing_backup.error == "backup not found");

  auto rollback = bridge::core::patch_rollback(cfg,
                                               "docs/note.txt",
                                               direct_apply.backup_id,
                                               "cli-001",
                                               "sess-001",
                                               "req-rollback");
  assert(rollback.ok);
  assert(rollback.rolled_back);
  assert(slurp(root / "docs" / "note.txt") == "one\ntwo\nthree\n");

  auto rollback_again = bridge::core::patch_rollback(cfg,
                                                     "docs/note.txt",
                                                     direct_apply.backup_id,
                                                     "cli-001",
                                                     "sess-001",
                                                     "req-rollback-again");
  assert(rollback_again.ok);
  assert(rollback_again.rolled_back);
  assert(slurp(root / "docs" / "note.txt") == "one\ntwo\nthree\n");

  auto history = bridge::core::history_list(cfg, "docs/note.txt", 10);
  assert(history.ok);
  assert(history.items.size() >= 3);
  bool saw_apply = false;
  bool saw_rollback = false;
  for (const auto& item : history.items) {
    if (item.method == "patch.apply") saw_apply = true;
    if (item.method == "patch.rollback") saw_rollback = true;
  }
  assert(saw_apply);
  assert(saw_rollback);

  fs::remove_all(root);
  return 0;
}
