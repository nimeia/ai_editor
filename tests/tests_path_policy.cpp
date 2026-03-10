#include "bridge/core/path_policy.hpp"
#include "bridge/core/workspace.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
  const fs::path root = fs::temp_directory_path() / "ai_bridge_path_policy_test";
  fs::create_directories(root / "src");
  fs::create_directories(root / "node_modules" / "pkg");
  fs::create_directories(root / ".bridge");
  std::ofstream(root / "src" / "main.txt") << "ok";

  auto cfg = bridge::core::make_default_workspace_config(root.string());

  auto inside = bridge::core::resolve_under_workspace(cfg, "src/main.txt");
  assert(inside.ok);
  assert(std::string(bridge::core::to_string(inside.policy)) == "normal");

  auto excluded = bridge::core::resolve_under_workspace(cfg, "node_modules/pkg");
  assert(excluded.ok);
  assert(std::string(bridge::core::to_string(excluded.policy)) == "skip_by_default");

  auto denied = bridge::core::resolve_under_workspace(cfg, ".bridge/data");
  assert(denied.ok);
  assert(std::string(bridge::core::to_string(denied.policy)) == "deny");

  auto outside = bridge::core::resolve_under_workspace(cfg, "../etc/passwd");
  assert(!outside.ok);

  fs::remove_all(root);
  return 0;
}
