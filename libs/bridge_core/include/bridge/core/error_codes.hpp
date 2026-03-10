#pragma once
#include <string>

namespace bridge::core {

struct ErrorInfo {
  std::string code;
  std::string message;
};

ErrorInfo classify_common_error(const std::string& message,
                                const std::string& fallback_code = "INTERNAL_ERROR");
ErrorInfo classify_search_error(const std::string& message, bool timed_out, bool cancelled);
ErrorInfo classify_patch_error(const std::string& message);

} // namespace bridge::core
