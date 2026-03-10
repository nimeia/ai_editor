#include "bridge/core/logging.hpp"
#include "bridge/core/workspace.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static void set_env_var(const char* name, const char* value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  ::setenv(name, value, 1);
#endif
}

int main() {
  const fs::path tmp = fs::temp_directory_path() / "ai_bridge_test_logging";
  std::error_code ec;
  fs::remove_all(tmp, ec);
  fs::create_directories(tmp, ec);

  set_env_var("AI_BRIDGE_LOG_ROTATE_BYTES", "80");
  set_env_var("AI_BRIDGE_LOG_ROTATE_KEEP", "2");

  for (int i = 0; i < 8; ++i) {
    bridge::core::append_runtime_log(tmp.string(), "INFO", std::string(40, 'a' + (i % 5)));
  }
  if (!fs::exists(tmp / "runtime.log")) {
    std::cerr << "runtime.log missing\n";
    return 1;
  }
  if (!fs::exists(tmp / "runtime.log.1")) {
    std::cerr << "runtime.log.1 missing\n";
    return 1;
  }

  auto ws = bridge::core::make_default_workspace_config(tmp.string());
  bridge::core::AuditRecord rec;
  rec.timestamp = bridge::core::current_timestamp_utc();
  rec.request_id = "req-1";
  rec.client_id = "cli-1";
  rec.session_id = "sess-1";
  rec.method = "fs.read";
  rec.path = "docs/readme.md";
  rec.endpoint = "local://test";
  rec.duration_ms = 12;
  rec.request_bytes = 123;
  rec.response_bytes = 456;
  rec.ok = false;
  rec.truncated = true;
  rec.error_code = "FILE_NOT_FOUND";
  rec.error_message = "path not found";
  for (int i = 0; i < 8; ++i) {
    bridge::core::append_audit_log(ws, rec);
  }
  const fs::path audit = tmp / ".bridge" / "audit.log";
  if (!fs::exists(audit)) {
    std::cerr << "audit.log missing\n";
    return 1;
  }
  if (!fs::exists(tmp / ".bridge" / "audit.log.1")) {
    std::cerr << "audit.log.1 missing\n";
    return 1;
  }
  std::ifstream ifs(audit);
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  if (content.find("FILE_NOT_FOUND") == std::string::npos) {
    std::cerr << "audit code missing\n";
    return 1;
  }
  fs::remove_all(tmp, ec);
  return 0;
}
