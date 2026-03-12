#include "bridge/core/error_codes.hpp"
#include <algorithm>
#include <cctype>

namespace bridge::core {
namespace {

bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

std::string lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

} // namespace

ErrorInfo classify_common_error(const std::string& message, const std::string& fallback_code) {
  if (message == "path is outside workspace") return {"PATH_OUTSIDE_WORKSPACE", message};
  if (message == "path denied by policy") return {"DENIED_BY_POLICY", message};
  if (message == "path not found") return {"FILE_NOT_FOUND", message};
  if (message == "binary file") return {"BINARY_FILE", message};
  if (message == "request timeout") return {"REQUEST_TIMEOUT", message};
  if (message == "invalid line range") return {"INVALID_PARAMS", message};
  if (message == "path is not a directory") return {"INVALID_PARAMS", message};
  if (message == "path is not a regular file") return {"INVALID_PARAMS", message};
  if (message == "path exists and is not a directory") return {"INVALID_PARAMS", message};
  if (message == "parent path is not a directory") return {"INVALID_PARAMS", message};
  if (message == "path already exists") return {"ALREADY_EXISTS", message};
  if (message == "parent directory not found") return {"FILE_NOT_FOUND", message};
  if (message == "unsupported encoding") return {"INVALID_PARAMS", message};
  if (message == "unsupported eol") return {"INVALID_PARAMS", message};
  if (message == "binary content not supported") return {"INVALID_PARAMS", message};
  if (contains(message, "failed to open file")) return {"ACCESS_DENIED", message};
  if (contains(message, "Permission denied")) return {"ACCESS_DENIED", message};
  return {fallback_code, message};
}

ErrorInfo classify_search_error(const std::string& message, bool timed_out, bool cancelled) {
  if (cancelled) return {"REQUEST_CANCELLED", message.empty() ? "request cancelled" : message};
  if (timed_out) return {"SEARCH_TIMEOUT", message.empty() ? "search timeout" : message};
  const auto folded = lowercase(message);
  if (contains(message, "regex_error") ||
      contains(message, "Mismatched") ||
      contains(message, "Invalid") ||
      contains(message, "Unexpected") ||
      contains(folded, "mismatch") ||
      contains(folded, "regex") ||
      contains(folded, "regular expression") ||
      contains(folded, "unexpected") ||
      contains(folded, "bracket") ||
      contains(folded, "parenth")) {
    return {"INVALID_PARAMS", message};
  }
  return classify_common_error(message, "SEARCH_FAILED");
}

ErrorInfo classify_patch_error(const std::string& message) {
  if (message == "request timeout") return {"REQUEST_TIMEOUT", message};
  if (message == "patch conflict" || contains(message, "patch conflict:")) return {"PATCH_CONFLICT", message};
  if (message == "backup not found") return {"ROLLBACK_NOT_FOUND", message};
  if (message == "preview not found") return {"PREVIEW_NOT_FOUND", message};
  if (message == "preview expired") return {"PREVIEW_EXPIRED", message};
  if (message == "preview evicted") return {"PREVIEW_EVICTED", message};
  if (message == "preview already applied") return {"PREVIEW_CONSUMED", message};
  if (message == "preview invalid") return {"PREVIEW_INVALID", message};
  if (message == "preview metadata corrupted") return {"PREVIEW_INVALID", message};
  if (message == "preview content missing") return {"PREVIEW_INVALID", message};
  if (message == "preview content hash mismatch") return {"PREVIEW_INVALID", message};
  if (message == "preview path mismatch") return {"PREVIEW_MISMATCH", message};
  if (message == "preview content mismatch") return {"PREVIEW_MISMATCH", message};
  if (message == "preview base mismatch") return {"PREVIEW_MISMATCH", message};
  return classify_common_error(message, "PATCH_APPLY_FAILED");
}

} // namespace bridge::core
