#include "bridge/platform/runtime.hpp"
#ifdef _WIN32
#include <windows.h>
#include <filesystem>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace bridge::platform {
namespace {

static unsigned long long fnv1a64(const std::string& s) {
  unsigned long long h = 1469598103934665603ull;
  for (unsigned char c : s) {
    h ^= c;
    h *= 1099511628211ull;
  }
  return h;
}

static std::string to_hex(unsigned long long value) {
  std::ostringstream oss;
  oss << std::hex << value;
  return oss.str();
}

static std::string wide_to_utf8(const std::wstring& value) {
  if (value.empty()) return {};
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (bytes <= 0) return {};
  std::string out(static_cast<std::size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), bytes, nullptr, nullptr);
  return out;
}

static std::string narrow_to_utf8(const char* value) {
  if (!value || !*value) return {};
  const int wide_len = MultiByteToWideChar(CP_ACP, 0, value, -1, nullptr, 0);
  if (wide_len <= 1) return std::string(value);
  std::wstring wide(static_cast<std::size_t>(wide_len - 1), L'\0');
  MultiByteToWideChar(CP_ACP, 0, value, -1, wide.data(), wide_len);
  return wide_to_utf8(wide);
}

static std::string current_user_name_utf8() {
  DWORD size = 0;
  GetUserNameW(nullptr, &size);
  if (size == 0) return {};
  std::vector<wchar_t> buffer(size, L'\0');
  if (!GetUserNameW(buffer.data(), &size) || size == 0) return {};
  std::wstring ws(buffer.data(), buffer.data() + (size > 0 ? size - 1 : 0));
  return wide_to_utf8(ws);
}

} // namespace

std::string platform_family() { return "windows"; }

std::string current_user_id() {
  const auto user = current_user_name_utf8();
  return user.empty() ? std::string("windows-user") : user;
}

RuntimePaths make_runtime_paths(const std::string& instance_key) {
  wchar_t temp_path[MAX_PATH] = {0};
  DWORD len = GetTempPathW(MAX_PATH, temp_path);
  fs::path runtime_dir = len > 0 ? fs::path(temp_path) : fs::path(L"C:\\Temp");
  runtime_dir /= L"ai_bridge_runtime";
  const auto user_utf8 = current_user_id();
  std::wstring user_wide;
  if (!user_utf8.empty()) {
    const int wide_len = MultiByteToWideChar(CP_UTF8, 0, user_utf8.c_str(), -1, nullptr, 0);
    if (wide_len > 1) {
      user_wide.assign(static_cast<std::size_t>(wide_len - 1), L'\0');
      MultiByteToWideChar(CP_UTF8, 0, user_utf8.c_str(), -1, user_wide.data(), wide_len);
    }
  }
  runtime_dir /= user_wide.empty() ? L"windows-user" : fs::path(user_wide);
  std::error_code ec;
  fs::create_directories(runtime_dir, ec);

  RuntimePaths out;
  out.runtime_dir = wide_to_utf8(runtime_dir.native());
  if (out.runtime_dir.empty()) out.runtime_dir = narrow_to_utf8(runtime_dir.string().c_str());
  out.endpoint = std::string(R"(\\.\pipe\aibridge-)" ) + instance_key;
  out.lock_file = out.runtime_dir + "/aibridge-" + to_hex(fnv1a64(instance_key)) + ".lock";
  return out;
}

InstanceLock::InstanceLock() = default;
InstanceLock::~InstanceLock() { release(); }

bool InstanceLock::acquire(const std::string& lock_file, std::string* error_message) {
  release();
  const std::string mutex_name = "Local\\AIBridge-" + to_hex(fnv1a64(lock_file));
  handle_ = CreateMutexA(nullptr, FALSE, mutex_name.c_str());
  if (!handle_) {
    if (error_message) *error_message = "failed to create mutex";
    return false;
  }
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    if (error_message) *error_message = "instance already locked";
    CloseHandle(reinterpret_cast<HANDLE>(handle_));
    handle_ = nullptr;
    return false;
  }
  return true;
}

void InstanceLock::release() {
  if (handle_) {
    CloseHandle(reinterpret_cast<HANDLE>(handle_));
    handle_ = nullptr;
  }
}

} // namespace bridge::platform
#else
namespace bridge::platform {
std::string platform_family() { return "windows"; }
std::string current_user_id() { return {}; }
RuntimePaths make_runtime_paths(const std::string&) { return {}; }
InstanceLock::InstanceLock() = default;
InstanceLock::~InstanceLock() = default;
bool InstanceLock::acquire(const std::string&, std::string*) { return false; }
void InstanceLock::release() {}
} // namespace bridge::platform
#endif
