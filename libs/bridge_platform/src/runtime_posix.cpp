#include "bridge/platform/runtime.hpp"
#include <filesystem>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>

namespace fs = std::filesystem;

namespace bridge::platform {

std::string platform_family() { return "posix"; }

std::string current_user_id() {
  return std::to_string(::getuid());
}

RuntimePaths make_runtime_paths(const std::string& instance_key) {
  const char* xdg = std::getenv("XDG_RUNTIME_DIR");
  fs::path runtime_dir = xdg ? fs::path(xdg) : fs::path("/tmp/ai_bridge_runtime");
  runtime_dir /= current_user_id();
  fs::create_directories(runtime_dir);
  ::chmod(runtime_dir.c_str(), 0700);
  RuntimePaths out;
  out.runtime_dir = runtime_dir.string();
  out.endpoint = (runtime_dir / ("aibridge-" + instance_key + ".sock")).string();
  out.lock_file = (runtime_dir / ("aibridge-" + instance_key + ".lock")).string();
  return out;
}

InstanceLock::InstanceLock() = default;
InstanceLock::~InstanceLock() { release(); }

bool InstanceLock::acquire(const std::string& lock_file, std::string* error_message) {
  release();
  fd_ = ::open(lock_file.c_str(), O_CREAT | O_RDWR, 0600);
  if (fd_ < 0) {
    if (error_message) *error_message = "failed to open lock file";
    return false;
  }
  if (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
    if (error_message) *error_message = "instance already locked";
    ::close(fd_);
    fd_ = -1;
    return false;
  }
  return true;
}

void InstanceLock::release() {
  if (fd_ >= 0) {
    ::flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = -1;
  }
}

} // namespace bridge::platform
