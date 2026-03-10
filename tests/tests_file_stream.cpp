#include "bridge/core/file_service.hpp"
#include "bridge/core/workspace.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
void set_env_var(const char* name, const char* value) {
#if defined(_WIN32)
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

void unset_env_var(const char* name) {
#if defined(_WIN32)
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}
} // namespace


namespace fs = std::filesystem;

int main() {
  const fs::path root = fs::temp_directory_path() / "ai_bridge_file_stream_test";
  fs::remove_all(root);
  fs::create_directories(root / "docs");
  std::ofstream(root / "docs" / "big.txt", std::ios::binary) << "line1\nline2\nline3\nline4\n";

  auto cfg = bridge::core::make_default_workspace_config(root.string());

  std::string collected;
  bridge::core::FsReadStreamOptions opts;
  opts.chunk_bytes = 5;
  opts.on_chunk = [&](const std::string& chunk) {
    collected += chunk;
    return true;
  };
  auto streamed = bridge::core::fs_read_stream(cfg, "docs/big.txt", opts);
  assert(streamed.ok);
  assert(streamed.chunk_count >= 2);
  assert(streamed.line_count == 4);
  assert(collected == "line1\nline2\nline3\nline4\n");

  std::string range_collected;
  bridge::core::FsReadStreamOptions range_opts;
  range_opts.chunk_bytes = 4;
  range_opts.on_chunk = [&](const std::string& chunk) {
    range_collected += chunk;
    return true;
  };
  auto range_streamed = bridge::core::fs_read_range_stream(cfg, "docs/big.txt", 2, 3, range_opts);
  assert(range_streamed.ok);
  assert(range_streamed.line_count == 2);
  assert(range_collected == "line2\nline3");

  set_env_var("AI_BRIDGE_READ_STREAM_DELAY_MS", "2");
  bridge::core::FsReadStreamOptions timeout_opts;
  timeout_opts.chunk_bytes = 4;
  timeout_opts.timeout_ms = 1;
  auto timeout_streamed = bridge::core::fs_read_stream(cfg, "docs/big.txt", timeout_opts);
  assert(!timeout_streamed.ok);
  assert(timeout_streamed.timed_out);
  assert(timeout_streamed.error == "request timeout");
  unset_env_var("AI_BRIDGE_READ_STREAM_DELAY_MS");

  fs::remove_all(root);
  return 0;
}
