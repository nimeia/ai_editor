#include "bridge/core/session_service.hpp"
#include "bridge/core/workspace.hpp"
#include <algorithm>
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
  const fs::path root = fs::temp_directory_path() / "ai_bridge_session_service_test";
  fs::remove_all(root);
  fs::create_directories(root / "docs");
  std::ofstream(root / "docs" / "note.txt", std::ios::binary) << "one\ntwo\nthree\n";
  std::ofstream(root / "docs" / "other.txt", std::ios::binary) << "left\ncenter\nright\n";
  std::ofstream(root / "docs" / "block.txt", std::ios::binary) << "alpha\nbeta token\ngamma\n";

  auto cfg = bridge::core::make_default_workspace_config(root.string());

  auto begin = bridge::core::session_begin(cfg, "sess-range");
  assert(begin.ok);
  assert(begin.state == "created");

  auto replace_range = bridge::core::edit_replace_range(cfg, "sess-range", "docs/note.txt", 2, 2, "TWO");
  assert(replace_range.ok);
  assert(replace_range.risk.level == "low" || replace_range.risk.level == "medium");

  auto inspect = bridge::core::session_inspect(cfg, "sess-range");
  assert(inspect.ok);
  assert(inspect.staged_change_count == 1);
  assert(inspect.staged_file_count == 1);
  assert(inspect.staged_block_count == 1);
  assert(inspect.items[0].operation == "replace_range");
  assert(inspect.risk_level == "low");
  assert(!inspect.summary.empty());
  assert(inspect.files.size() == 1);
  assert(inspect.files[0].summary.find("docs/note.txt") != std::string::npos);
  assert(!inspect.highlights.empty());

  auto snapshot = bridge::core::session_snapshot(cfg, "sess-range");
  assert(snapshot.ok);
  assert(fs::exists(snapshot.snapshot_path));

  auto preview = bridge::core::session_preview(cfg, "sess-range");
  assert(preview.ok);
  assert(preview.previewed_file_count == 1);
  assert(preview.risk_level == "low");
  assert(preview.total_hunk_count >= 1);
  assert(!preview.summary.empty());
  assert(preview.files[0].path == "docs/note.txt");
  assert(preview.files[0].diff.find("TWO") != std::string::npos);
  assert(preview.files[0].summary.find("docs/note.txt") != std::string::npos);
  assert(preview.files[0].hunk_count >= 1);
  assert(preview.files[0].change_count == 1);

  auto commit = bridge::core::session_commit(cfg, "sess-range", "cli-001", "req-001");
  assert(commit.ok);
  assert(commit.committed_file_count == 1);
  assert(slurp(root / "docs" / "note.txt") == "one\nTWO\nthree\n");

  auto recover_after_commit = bridge::core::session_recover(cfg, "sess-range");
  assert(recover_after_commit.ok);
  assert(!recover_after_commit.recoverable);


  auto begin_multi = bridge::core::session_begin(cfg, "sess-multi");
  assert(begin_multi.ok);
  auto multi_note = bridge::core::edit_replace_range(cfg, "sess-multi", "docs/note.txt", 1, 1, "ONE");
  assert(multi_note.ok);
  auto multi_other = bridge::core::edit_replace_range(cfg, "sess-multi", "docs/other.txt", 2, 2, "CENTER");
  assert(multi_other.ok);

  auto multi_inspect = bridge::core::session_inspect(cfg, "sess-multi");
  assert(multi_inspect.ok);
  assert(multi_inspect.staged_file_count == 2);
  assert(multi_inspect.risk_level == "medium");
  assert(multi_inspect.risk_reasons.end() != std::find(multi_inspect.risk_reasons.begin(), multi_inspect.risk_reasons.end(), "cross-file-change"));
  assert(multi_inspect.files.size() == 2);

  auto multi_preview = bridge::core::session_preview(cfg, "sess-multi");
  assert(multi_preview.ok);
  assert(multi_preview.previewed_file_count == 2);
  assert(multi_preview.risk_level == "medium");
  assert(multi_preview.total_hunk_count >= 2);
  assert(multi_preview.risk_reasons.end() != std::find(multi_preview.risk_reasons.begin(), multi_preview.risk_reasons.end(), "cross-file-change"));
  assert(multi_preview.files.size() == 2);
  assert(!multi_preview.highlights.empty());

  auto drop_change_missing = bridge::core::session_drop_change(cfg, "sess-multi", "");
  assert(!drop_change_missing.ok);
  assert(drop_change_missing.error == "change_id required");

  auto drop_change = bridge::core::session_drop_change(cfg, "sess-multi", multi_note.change_id);
  assert(drop_change.ok);
  assert(drop_change.staged_change_count == 1);
  auto dropped_inspect = bridge::core::session_inspect(cfg, "sess-multi");
  assert(dropped_inspect.ok);
  assert(dropped_inspect.staged_change_count == 1);
  assert(dropped_inspect.files.size() == 1);
  assert(dropped_inspect.files[0].path == "docs/other.txt");

  auto multi_other_again = bridge::core::edit_replace_range(cfg, "sess-multi", "docs/other.txt", 3, 3, "RIGHT");
  assert(multi_other_again.ok);
  auto drop_path = bridge::core::session_drop_path(cfg, "sess-multi", "docs/other.txt");
  assert(drop_path.ok);
  assert(drop_path.staged_change_count == 0);
  auto emptied_inspect = bridge::core::session_inspect(cfg, "sess-multi");
  assert(emptied_inspect.ok);
  assert(emptied_inspect.staged_change_count == 0);
  assert(emptied_inspect.state == "created");

  auto multi_abort = bridge::core::session_abort(cfg, "sess-multi");
  assert(multi_abort.ok);
  assert(multi_abort.aborted);

  auto begin_block = bridge::core::session_begin(cfg, "sess-block");
  assert(begin_block.ok);

  bridge::core::Selector selector;
  selector.query = "beta token";
  auto insert_after = bridge::core::edit_insert_after(cfg, "sess-block", "docs/block.txt", selector, "\ninserted");
  assert(insert_after.ok);

  auto block_preview = bridge::core::session_preview(cfg, "sess-block");
  assert(block_preview.ok);
  assert(block_preview.files.size() == 1);
  assert(block_preview.files[0].diff.find("inserted") != std::string::npos);

  auto block_commit = bridge::core::session_commit(cfg, "sess-block", "cli-001", "req-002");
  assert(block_commit.ok);
  assert(slurp(root / "docs" / "block.txt") == "alpha\nbeta token\ninserted\ngamma\n");

  auto begin_rebase = bridge::core::session_begin(cfg, "sess-rebase");
  assert(begin_rebase.ok);
  auto rebase_change = bridge::core::edit_insert_after(cfg, "sess-rebase", "docs/block.txt", selector, "\nrebased");
  assert(rebase_change.ok);
  std::ofstream(root / "docs" / "block.txt", std::ios::binary) << "HEADER\nalpha\nbeta token\ninserted\ngamma\n";

  auto rebase_check = bridge::core::recovery_check(cfg, "sess-rebase");
  assert(rebase_check.ok);
  assert(rebase_check.recoverable);
  assert(rebase_check.rebase_file_count == 1);
  assert(rebase_check.files[0].status == "rebase_required");

  auto rebase_commit_fail = bridge::core::session_commit(cfg, "sess-rebase", "cli-001", "req-003");
  assert(!rebase_commit_fail.ok);
  assert(rebase_commit_fail.error.find("session rebase required:") == 0);

  auto rebased = bridge::core::recovery_rebase(cfg, "sess-rebase");
  assert(rebased.ok);
  assert(rebased.rebased_file_count == 1);

  auto rebase_commit = bridge::core::session_commit(cfg, "sess-rebase", "cli-001", "req-004");
  assert(rebase_commit.ok);
  assert(slurp(root / "docs" / "block.txt") == "HEADER\nalpha\nbeta token\nrebased\ninserted\ngamma\n");

  auto begin_conflict = bridge::core::session_begin(cfg, "sess-conflict");
  assert(begin_conflict.ok);
  auto conflict_change = bridge::core::edit_replace_range(cfg, "sess-conflict", "docs/note.txt", 2, 2, "SECOND");
  assert(conflict_change.ok);
  std::ofstream(root / "docs" / "note.txt", std::ios::binary) << "one\nmutated\nthree\n";

  auto conflict_check = bridge::core::recovery_check(cfg, "sess-conflict");
  assert(conflict_check.ok);
  assert(!conflict_check.recoverable);
  assert(conflict_check.conflict_file_count == 1);
  assert(conflict_check.files[0].status == "conflict");

  auto conflict_commit = bridge::core::session_commit(cfg, "sess-conflict", "cli-001", "req-005");
  assert(!conflict_commit.ok);
  assert(conflict_commit.error.find("session conflict:") == 0);

  auto conflict_recover = bridge::core::session_recover(cfg, "sess-conflict");
  assert(conflict_recover.ok);
  assert(!conflict_recover.recoverable);
  assert(conflict_recover.conflict_file_count == 1);

  auto begin_delete = bridge::core::session_begin(cfg, "sess-delete");
  assert(begin_delete.ok);
  auto delete_change = bridge::core::edit_delete_block(cfg, "sess-delete", "docs/block.txt", selector);
  assert(delete_change.ok);
  assert(delete_change.risk.level == "high");

  auto abort_delete = bridge::core::session_abort(cfg, "sess-delete");
  assert(abort_delete.ok);
  assert(abort_delete.aborted);

  fs::remove_all(root);
  return 0;
}
