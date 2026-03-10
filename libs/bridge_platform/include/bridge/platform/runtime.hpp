#pragma once
#include <string>

namespace bridge::platform {

struct RuntimePaths {
  std::string runtime_dir;
  std::string endpoint;
  std::string lock_file;
};

std::string platform_family();
std::string current_user_id();
RuntimePaths make_runtime_paths(const std::string& instance_key);

class InstanceLock {
 public:
  InstanceLock();
  ~InstanceLock();
  bool acquire(const std::string& lock_file, std::string* error_message);
  void release();
 private:
#ifdef _WIN32
  void* handle_ = nullptr;
#else
  int fd_ = -1;
#endif
};

} // namespace bridge::platform
