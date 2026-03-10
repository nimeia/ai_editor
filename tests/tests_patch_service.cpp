#include "bridge/core/patch_service.hpp"
#include "bridge/core/workspace.hpp"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

static void set_env_var(const char* name, const char* value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  ::setenv(name, value, 1);
#endif
}

static void unset_env_var(const char* name) {
#ifdef _WIN32
  _putenv_s(name, "");
#else
  ::unsetenv(name);
#endif
}

static std::string slurp(const fs::path& path) {
  std::ifstream ifs(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

int main() {
  const fs::path root = fs::temp_directory_path() / "ai_bridge_patch_service_test";
  fs::remove_all(root);
  fs::create_directories(root / "docs");
  std::ofstream(root / "docs" / "readme.md") << "hello\nworld\n";

  auto cfg = bridge::core::make_default_workspace_config(root.string());

  set_env_var("AI_BRIDGE_HISTORY_ROTATE_BYTES", "60");
  set_env_var("AI_BRIDGE_HISTORY_ROTATE_KEEP", "2");
  set_env_var("AI_BRIDGE_BACKUP_KEEP", "2");
  set_env_var("AI_BRIDGE_PREVIEW_KEEP", "2");
  set_env_var("AI_BRIDGE_PREVIEW_STATUS_KEEP", "2");

  auto preview = bridge::core::patch_preview(cfg, "docs/readme.md", "hello\nbridge\n");
  assert(preview.ok);
  assert(preview.applicable);
  assert(preview.current_hash != "");
  assert(preview.new_content_hash != "");
  assert(!preview.preview_id.empty());
  assert(!preview.preview_created_at.empty());
  assert(preview.preview_expires_at.empty());
  assert(preview.diff.find("+bridge") != std::string::npos);

  std::string streamed_diff;
  bridge::core::PatchPreviewStreamOptions stream_opts;
  stream_opts.chunk_bytes = 8;
  stream_opts.on_chunk = [&](const std::string& chunk) { streamed_diff += chunk; return true; };
  auto preview_stream = bridge::core::patch_preview_stream(cfg, "docs/readme.md", "hello\nstreamed\n", {}, stream_opts);
  assert(preview_stream.ok);
  assert(!preview_stream.preview_id.empty());
  assert(!preview_stream.preview_created_at.empty());
  assert(preview_stream.chunk_count >= 1);
  assert(preview_stream.total_bytes == streamed_diff.size());
  assert(streamed_diff.find("+streamed") != std::string::npos);

  auto apply_by_preview = bridge::core::patch_apply(cfg,
                                                    "docs/readme.md",
                                                    "",
                                                    {},
                                                    "cli-001",
                                                    "sess-001",
                                                    "req-001",
                                                    preview.preview_id);
  assert(apply_by_preview.ok);
  assert(apply_by_preview.applied);
  assert(apply_by_preview.preview_id == preview.preview_id);
  assert(apply_by_preview.preview_status == "applied");
  assert(!apply_by_preview.backup_id.empty());

  assert(slurp(root / "docs" / "readme.md") == "hello\nbridge\n");

  auto consumed_preview = bridge::core::patch_apply(cfg,
                                                    "docs/readme.md",
                                                    "",
                                                    {},
                                                    "cli-001",
                                                    "sess-001",
                                                    "req-001b",
                                                    preview.preview_id);
  assert(!consumed_preview.ok);
  assert(consumed_preview.error == "preview already applied");

  auto hist = bridge::core::history_list(cfg, "docs/readme.md", 10);
  assert(hist.ok);
  assert(!hist.items.empty());
  assert(hist.items.back().method == "patch.apply.preview");

  auto rollback = bridge::core::patch_rollback(cfg, "docs/readme.md", apply_by_preview.backup_id, "cli-001", "sess-001", "req-002");
  assert(rollback.ok);
  assert(rollback.rolled_back);
  assert(!rollback.current_hash.empty());
  assert(!rollback.current_mtime.empty());
  assert(slurp(root / "docs" / "readme.md") == "hello\nworld\n");

  set_env_var("AI_BRIDGE_PATCH_STREAM_DELAY_MS", "2");
  bridge::core::PatchPreviewStreamOptions timeout_opts;
  timeout_opts.chunk_bytes = 4;
  timeout_opts.timeout_ms = 1;
  auto preview_timeout = bridge::core::patch_preview_stream(cfg, "docs/readme.md", "hello\nstreamed again\n", {}, timeout_opts);
  assert(!preview_timeout.ok);
  assert(preview_timeout.timed_out);
  assert(preview_timeout.error == "request timeout");
  unset_env_var("AI_BRIDGE_PATCH_STREAM_DELAY_MS");

  set_env_var("AI_BRIDGE_PREVIEW_TTL_MS", "1");
  auto ttl_preview = bridge::core::patch_preview(cfg, "docs/readme.md", "hello\nexpired\n");
  assert(ttl_preview.ok);
  assert(!ttl_preview.preview_expires_at.empty());
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  auto expired_preview = bridge::core::patch_apply(cfg,
                                                   "docs/readme.md",
                                                   "",
                                                   {},
                                                   "cli-001",
                                                   "sess-001",
                                                   "req-003",
                                                   ttl_preview.preview_id);
  assert(!expired_preview.ok);
  assert(expired_preview.error == "preview expired");
  unset_env_var("AI_BRIDGE_PREVIEW_TTL_MS");

  set_env_var("AI_BRIDGE_PREVIEW_KEEP", "1");
  auto evicted_first = bridge::core::patch_preview(cfg, "docs/readme.md", "hello\nevicted-1\n");
  auto evicted_second = bridge::core::patch_preview(cfg, "docs/readme.md", "hello\nevicted-2\n");
  assert(evicted_first.ok && evicted_second.ok);
  auto evicted_apply = bridge::core::patch_apply(cfg,
                                                 "docs/readme.md",
                                                 "",
                                                 {},
                                                 "cli-001",
                                                 "sess-001",
                                                 "req-003b",
                                                 evicted_first.preview_id);
  assert(!evicted_apply.ok);
  assert(evicted_apply.error == "preview evicted");
  set_env_var("AI_BRIDGE_PREVIEW_KEEP", "2");

  std::ofstream(root / "docs" / "readme.md") << "changed externally\n";
  auto conflict_preview = bridge::core::patch_preview(cfg, "docs/readme.md", "other\n");
  assert(conflict_preview.ok);
  std::ofstream(root / "docs" / "readme.md") << "changed twice externally\n";
  auto conflict = bridge::core::patch_apply(cfg,
                                            "docs/readme.md",
                                            "",
                                            {},
                                            "cli-001",
                                            "sess-001",
                                            "req-004",
                                            conflict_preview.preview_id);
  assert(!conflict.ok);
  assert(conflict.error.find("patch conflict:") == 0);
  assert(conflict.conflict_reason == "mtime_and_hash_changed" || conflict.conflict_reason == "hash_changed" || conflict.conflict_reason == "mtime_changed");
  assert(!conflict.expected_hash.empty());
  assert(!conflict.actual_hash.empty());

  auto missing_preview = bridge::core::patch_apply(cfg,
                                                   "docs/readme.md",
                                                   "",
                                                   {},
                                                   "cli-001",
                                                   "sess-001",
                                                   "req-005",
                                                   "missing-preview");
  assert(!missing_preview.ok);
  assert(missing_preview.error == "preview not found");

  for (int i = 0; i < 6; ++i) {
    auto prev = bridge::core::patch_preview(cfg, "docs/readme.md", std::string("iter-") + std::to_string(i) + "\n");
    assert(prev.ok);
    auto iter_apply = bridge::core::patch_apply(cfg,
                                                "docs/readme.md",
                                                "",
                                                {},
                                                "cli-001",
                                                "sess-001",
                                                std::string("req-iter-") + std::to_string(i),
                                                prev.preview_id);
    assert(iter_apply.ok);
  }

  assert(fs::exists(root / ".bridge" / "history.log"));
  assert(fs::exists(root / ".bridge" / "history.log.1"));

  std::size_t backup_count = 0;
  for (const auto& entry : fs::directory_iterator(root / ".bridge" / "backups")) {
    if (entry.is_regular_file()) ++backup_count;
  }
  assert(backup_count <= 2);

  std::size_t preview_count = 0;
  std::size_t preview_status_count = 0;
  for (const auto& entry : fs::directory_iterator(root / ".bridge" / "previews")) {
    if (entry.path().extension() == ".meta") ++preview_count;
    if (entry.path().extension() == ".status") ++preview_status_count;
  }
  assert(preview_count <= 2);
  assert(preview_status_count <= 2);

  auto hist2 = bridge::core::history_list(cfg, "docs/readme.md", 20);
  assert(hist2.ok);
  assert(!hist2.items.empty());
  assert(hist2.items.front().method == "patch.apply.preview");

  unset_env_var("AI_BRIDGE_HISTORY_ROTATE_BYTES");
  unset_env_var("AI_BRIDGE_HISTORY_ROTATE_KEEP");
  unset_env_var("AI_BRIDGE_BACKUP_KEEP");
  unset_env_var("AI_BRIDGE_PREVIEW_KEEP");
  unset_env_var("AI_BRIDGE_PREVIEW_STATUS_KEEP");
  unset_env_var("AI_BRIDGE_PREVIEW_TTL_MS");
  fs::remove_all(root);
  return 0;
}
